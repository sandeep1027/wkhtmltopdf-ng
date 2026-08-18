#include "HtmlToPdfConverter.h"

#include "HeaderFooter.h"
#include "PageLayout.h"
#include "PdfPostProcessor.h"
#include "TocGenerator.h"
#include "utils/FileUtils.h"
#include "utils/JsBridge.h"

#include <QElapsedTimer>
#include <QPageLayout>
#include <QPageRanges>
#include <QSize>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QTimeZone>
#include <QAuthenticator>
#include <QNetworkCookie>
#include <QXmlStreamWriter>
#include <QTextStream>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QTemporaryFile>
#include <QProcess>
#include <QUrl>
#include <QVariant>
#include <QWebEnginePage>
#include <QWebEngineCookieStore>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineHttpRequest>
#include <QWebEngineView>
#include <QWebEngineClientCertificateStore>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslKey>

#include <functional>

namespace {

int logRank(const QString& level)
{
    if (level == QStringLiteral("none")) return 0;
    if (level == QStringLiteral("error")) return 1;
    if (level == QStringLiteral("warn")) return 2;
    if (level == QStringLiteral("info")) return 3;
    if (level == QStringLiteral("verbose")) return 4;
    return 3;
}

void logAt(const GlobalSettings& global, int minRank, const QString& message)
{
    if (global.quiet || logRank(global.logLevel) < minRank) return;
    QTextStream(stderr) << message << '\n';
}

class HeaderInterceptor final : public QWebEngineUrlRequestInterceptor {
public:
    HeaderInterceptor(const QMap<QString, QString>& headers, bool propagate,
                      bool allowAllLocal, const QStringList& allowedPaths)
        : m_headers(headers)
        , m_propagate(propagate)
        , m_allowAllLocal(allowAllLocal)
        , m_allowedPaths(allowedPaths)
    {
        for (QString& path : m_allowedPaths) path = QFileInfo(path).absoluteFilePath();
    }

    void interceptRequest(QWebEngineUrlRequestInfo& info) override
    {
        const bool mainFrame =
            info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeMainFrame;
        if (m_propagate || mainFrame) {
            for (auto it = m_headers.cbegin(); it != m_headers.cend(); ++it) {
                info.setHttpHeader(it.key().toUtf8(), it.value().toUtf8());
            }
        }
        const QUrl url = info.requestUrl();
        if (url.isLocalFile() && !m_allowAllLocal) {
            const QString path = QFileInfo(url.toLocalFile()).absoluteFilePath();
            bool allowed = false;
            for (const QString& root : m_allowedPaths) {
                if (!root.isEmpty() && (path == root || path.startsWith(root + QLatin1Char('/')))) {
                    allowed = true;
                    break;
                }
            }
            if (!allowed) info.block(true);
        }
    }

private:
    QMap<QString, QString> m_headers;
    bool m_propagate = true;
    bool m_allowAllLocal = false;
    QStringList m_allowedPaths;
};

double printableWidthPx(const QPageLayout& layout)
{
    const QSizeF pageMillimeters = layout.pageSize().size(QPageSize::Millimeter);
    const QMarginsF margins = layout.margins(QPageLayout::Millimeter);
    return qMax(1.0, (pageMillimeters.width() - margins.left() - margins.right()) * 96.0 / 25.4);
}

double layoutScale(int dpi, double zoom)
{
    const double safeDpi = dpi > 0 ? double(dpi) : 96.0;
    const double safeZoom = zoom > 0.0 ? zoom : 1.0;
    return (96.0 / safeDpi) * safeZoom;
}

bool parseViewportSize(const QString& value, int* width, int* height)
{
    if (value.isEmpty()) return true;
    const QStringList parts = value.split(QLatin1Char('x'));
    if (parts.size() != 2) return false;
    bool widthOk = false;
    bool heightOk = false;
    *width = parts.at(0).toInt(&widthOk);
    *height = parts.at(1).toInt(&heightOk);
    return widthOk && heightOk && *width > 0 && *height > 0;
}

// A bare QWebEnginePage (no shown view) reports innerWidth/innerHeight = 0 on
// Qt 6.2 offscreen. That makes screen max-width breakpoints (e.g. 700px) match
// and collapse table-cell / multi-column layouts. Size the view to the paper
// in CSS pixels, but never below a desktop 1024x768 so those queries stay off.
QSize defaultPdfViewport(const QPageLayout& layout)
{
    const QSizeF millimeters = layout.pageSize().size(QPageSize::Millimeter);
    const int pageWidth = qMax(1, qRound(millimeters.width() * 96.0 / 25.4));
    const int pageHeight = qMax(1, qRound(millimeters.height() * 96.0 / 25.4));
    return QSize(qMax(1024, pageWidth), qMax(768, pageHeight));
}

QString rendererDiagnosticsJavascript()
{
    return QStringLiteral(
        "(function(){function dump(sel){const el=document.querySelector(sel);"
        "if(!el)return{selector:sel,missing:true};"
        "const s=getComputedStyle(el);const r=el.getBoundingClientRect();"
        "return{selector:sel,display:s.display,width:s.width,"
        "backgroundImage:s.backgroundImage,backgroundColor:s.backgroundColor,"
        "printColorAdjust:s.webkitPrintColorAdjust||s.printColorAdjust,"
        "rect:{x:r.x,y:r.y,w:r.width,h:r.height}};}"
        "return{innerWidth:window.innerWidth,innerHeight:window.innerHeight,"
        "dpr:window.devicePixelRatio,"
        "bodyWidth:document.body?document.body.getBoundingClientRect().width:0,"
        "documentWidth:document.documentElement.getBoundingClientRect().width,"
        "screenMedia:matchMedia('screen').matches,"
        "printMedia:matchMedia('print').matches,"
        "responsive700:matchMedia('screen and (max-width: 700px)').matches,"
        "header:dump('.header'),kpis:dump('.kpis'),kpi:dump('.kpi'),"
        "columns:dump('.columns'),column:dump('.column'),"
        "progressTrack:dump('.progress-track'),progressBar:dump('.progress-bar'),"
        "features:dump('.features'),feature:dump('.feature')};})()");
}

QString printBackgroundJavascript()
{
    // Chromium print defaults to print-color-adjust: economy, which drops
    // CSS backgrounds and gradients even when PrintElementBackgrounds is on.
    return QStringLiteral(
        "(function(){const s=document.createElement('style');"
        "s.setAttribute('data-wkhtmltopdf-ng-bg','1');"
        "s.textContent='html{-webkit-print-color-adjust:exact!important;"
        "print-color-adjust:exact!important}';"
        "document.head.appendChild(s);})();");
}

QString waitForPaintJavascript()
{
    return QStringLiteral(
        "(function(){"
        "if(window.__wkhtmltopdfNgPaintReady===undefined){"
        "window.__wkhtmltopdfNgPaintReady=false;"
        "const ready=(document.fonts&&document.fonts.ready)?document.fonts.ready:Promise.resolve();"
        "ready.then(function(){"
        "requestAnimationFrame(function(){requestAnimationFrame(function(){"
        "window.__wkhtmltopdfNgPaintReady=true;});});"
        "}).catch(function(){window.__wkhtmltopdfNgPaintReady=true;});"
        "return false;}"
        "const images=Array.from(document.images).every(function(img){return img.complete;});"
        "const fonts=!document.fonts||document.fonts.status!=='loading';"
        "return window.__wkhtmltopdfNgPaintReady&&images&&fonts;})()");
}

QString loadErrorPolicy(const ObjectSettings& object)
{
    const QString policy = object.loadErrorHandling.trimmed().toLower();
    if (policy == QStringLiteral("ignore") || policy == QStringLiteral("skip")) return policy;
    return QStringLiteral("abort");
}

void ensureHeaderFooterMargins(GlobalSettings* global, const ObjectSettings& object)
{
    if (!global) return;
    const bool hasHeader = !object.headerHtml.isEmpty() || !object.headerLeft.isEmpty() ||
        !object.headerCenter.isEmpty() || !object.headerRight.isEmpty() || object.headerLine;
    const bool hasFooter = !object.footerHtml.isEmpty() || !object.footerLeft.isEmpty() ||
        !object.footerCenter.isEmpty() || !object.footerRight.isEmpty() || object.footerLine;
    QMarginsF margins = global->pageLayout.margins(QPageLayout::Millimeter);
    bool changed = false;
    if (hasHeader && margins.top() <= 10.01) {
        margins.setTop(20.0);
        changed = true;
    }
    if (hasFooter && margins.bottom() <= 10.01) {
        margins.setBottom(18.0);
        changed = true;
    }
    if (changed) global->pageLayout.setMargins(margins);
}

QString keepRelativeLinksJavascript()
{
    return QStringLiteral(
        "(function(){const pageUrl=document.baseURI||location.href;"
        "const dir=pageUrl.replace(/[#?].*$/,'').replace(/[^/]*$/,'');"
        "document.querySelectorAll('a[href]').forEach(function(a){"
        "const raw=a.getAttribute('href')||'';if(!raw||raw.charAt(0)==='#')return;"
        "try{const abs=new URL(raw,pageUrl);if(abs.protocol!=='file:')return;"
        "if(abs.href.indexOf(dir)===0){"
        "a.setAttribute('href',decodeURIComponent(abs.href.slice(dir.length)));}"
        "}catch(e){}});})();");
}

QUrl inputUrl(const QString& input);

QString noPrintMediaJavascript()
{
    return QStringLiteral(
        "(function(){"
        "document.querySelectorAll('link[media=\"print\"]').forEach(function(el){el.disabled=true;});"
        "document.querySelectorAll('style').forEach(function(el){"
        "el.textContent=el.textContent.replace(/@media\\s+print\\s*\\{([^{}]|\\{[^{}]*\\})*\\}/gi,'');});"
        "})();");
}

QByteArray uncompressPdfStreams(const QByteArray& pdf)
{
    QTemporaryFile input(QDir::tempPath() + QStringLiteral("/wkhtmltopdf-ng-in-XXXXXX.pdf"));
    QTemporaryFile output(QDir::tempPath() + QStringLiteral("/wkhtmltopdf-ng-out-XXXXXX.pdf"));
    input.setAutoRemove(true);
    output.setAutoRemove(true);
    if (!input.open() || input.write(pdf) != pdf.size()) return pdf;
    input.close();
    if (!output.open()) return pdf;
    const QString outPath = output.fileName();
    output.close();
    QProcess process;
    process.start(QStringLiteral("qpdf"),
                  {QStringLiteral("--stream-data=uncompress"), input.fileName(), outPath});
    if (!process.waitForFinished(60000) || process.exitStatus() != QProcess::NormalExit ||
        process.exitCode() != 0) {
        return pdf;
    }
    bool ok = false;
    const QByteArray data = readAllFile(outPath, &ok);
    return ok && !data.isEmpty() ? data : pdf;
}

QByteArray relativizeLocalFileUris(QByteArray pdf, const QString& pagePath)
{
    if (pagePath.isEmpty()) return pdf;
    const QUrl pageUrl = inputUrl(pagePath);
    if (!pageUrl.isLocalFile()) return pdf;
    QString dir = QFileInfo(pageUrl.toLocalFile()).absolutePath();
    if (!dir.endsWith(QLatin1Char('/'))) dir += QLatin1Char('/');
    QByteArray prefix = QUrl::fromLocalFile(dir).toEncoded();
    if (!prefix.endsWith('/')) prefix += '/';
    pdf = uncompressPdfStreams(pdf);
    int index = 0;
    while ((index = pdf.indexOf(prefix, index)) >= 0) {
        const int start = index + prefix.size();
        int end = start;
        while (end < pdf.size()) {
            const char ch = pdf.at(end);
            if (ch == ')' || ch == ' ' || ch == '>' || ch == '\n' || ch == '\r') break;
            ++end;
        }
        const QByteArray relative = pdf.mid(start, end - start);
        pdf.replace(index, end - index, relative);
        index += relative.size();
    }
    QTemporaryFile rewritten(QDir::tempPath() + QStringLiteral("/wkhtmltopdf-ng-rel-XXXXXX.pdf"));
    QTemporaryFile rebuilt(QDir::tempPath() + QStringLiteral("/wkhtmltopdf-ng-fix-XXXXXX.pdf"));
    rewritten.setAutoRemove(true);
    rebuilt.setAutoRemove(true);
    if (!rewritten.open() || rewritten.write(pdf) != pdf.size()) return pdf;
    rewritten.close();
    if (!rebuilt.open()) return pdf;
    const QString rebuiltPath = rebuilt.fileName();
    rebuilt.close();
    QProcess process;
    process.start(QStringLiteral("qpdf"), {rewritten.fileName(), rebuiltPath});
    if (!process.waitForFinished(60000) || process.exitStatus() != QProcess::NormalExit ||
        (process.exitCode() != 0 && process.exitCode() != 3)) {
        return pdf;
    }
    bool ok = false;
    const QByteArray fixed = readAllFile(rebuiltPath, &ok);
    return ok && !fixed.isEmpty() ? fixed : pdf;
}

QUrl inputUrl(const QString& input)
{
    if (input.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive) ||
        input.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive) ||
        input.startsWith(QStringLiteral("file://"), Qt::CaseInsensitive)) {
        return QUrl(input);
    }
    if (!QFileInfo(input).exists()) {
        const QUrl userInput = QUrl::fromUserInput(input);
        if (userInput.isValid() && !userInput.scheme().isEmpty() &&
            userInput.scheme() != QStringLiteral("file")) {
            return userInput;
        }
    }
    return QUrl::fromLocalFile(QFileInfo(input).absoluteFilePath());
}

QString contentOrFile(const QString& value, QString* error)
{
    if (value.isEmpty()) return {};
    const QFileInfo info(value);
    if (!info.exists() || !info.isFile()) return value;
    bool ok = false;
    const QByteArray data = readAllFile(info.absoluteFilePath(), &ok);
    if (!ok) {
        if (error) *error = QStringLiteral("cannot read %1").arg(info.absoluteFilePath());
        return {};
    }
    return QString::fromUtf8(data);
}

bool loadCookieJar(QWebEngineProfile* profile, const QString& path, QString* error)
{
    if (path.isEmpty()) return true;
    bool ok = false;
    const QByteArray data = readAllFile(path, &ok);
    if (!ok) {
        if (error) *error = QStringLiteral("cannot read cookie jar %1").arg(path);
        return false;
    }

    const QList<QByteArray> lines = data.split('\n');
    for (const QByteArray& rawLine : lines) {
        const QByteArray line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        const QList<QByteArray> fields = line.split('\t');
        if (fields.size() < 7) continue; // tolerate malformed entries

        QNetworkCookie cookie(fields.at(5), fields.at(6));
        cookie.setDomain(QString::fromUtf8(fields.at(0)));
        cookie.setPath(QString::fromUtf8(fields.at(2)));
        cookie.setSecure(fields.at(3) == QByteArrayLiteral("TRUE"));
        bool expiryOk = false;
        const qint64 expiry = fields.at(4).toLongLong(&expiryOk);
        if (expiryOk && expiry > 0) {
            cookie.setExpirationDate(
                QDateTime::fromSecsSinceEpoch(expiry, QTimeZone(QByteArrayLiteral("UTC"))));
        }

        QString domain = QString::fromUtf8(fields.at(0));
        if (domain.startsWith(QLatin1Char('.'))) domain.remove(0, 1);
        const QUrl origin(QStringLiteral("http%1://%2/")
            .arg(cookie.isSecure() ? QStringLiteral("s") : QString(), domain));
        profile->cookieStore()->setCookie(cookie, origin);
    }
    return true;
}

}

HtmlToPdfConverter::HtmlToPdfConverter(const GlobalSettings& global)
    : m_global(global)
{
}

void HtmlToPdfConverter::setPhaseCallback(std::function<void(int, int)> callback)
{
    m_phaseCallback = std::move(callback);
}

void HtmlToPdfConverter::reportPhase(int phase, int percent)
{
    if (m_phaseCallback) m_phaseCallback(phase, percent);
    if (m_global.quiet || logRank(m_global.logLevel) < 3) return;
    static const char* names[] = {
        "Loading pages", "Counting pages", "Resolving links",
        "Loading headers and footers", "Printing pages", "Done"
    };
    if (phase < 0 || phase > 5) return;
    QTextStream(stderr) << names[phase] << " (" << (phase + 1) << "/6)";
    if (percent >= 0) QTextStream(stderr) << " [" << percent << "%]";
    QTextStream(stderr) << '\n';
}

namespace {

bool usesObjectPipeline(const QList<ObjectSettings>& objects)
{
    if (objects.isEmpty()) return false;
    for (const ObjectSettings& object : objects) {
        if (object.kind == ObjectKind::Cover || object.kind == ObjectKind::Toc) return true;
        if (object.localOptions) return true;
        if (!object.tocXsl.isEmpty()) return true;
    }
    return false;
}

void applyCoverRules(ObjectSettings* object)
{
    if (object->kind != ObjectKind::Cover) return;
    object->includeInOutline = false;
    object->headerLeft.clear();
    object->headerCenter.clear();
    object->headerRight.clear();
    object->headerHtml.clear();
    object->footerLeft.clear();
    object->footerCenter.clear();
    object->footerRight.clear();
    object->footerHtml.clear();
    object->headerLine = false;
    object->footerLine = false;
    object->toc = false;
}

bool writeInfoJson(const QString& outputPath, const QList<OutlineEntry>& outline,
                   const GlobalSettings& global, QString* error)
{
    QJsonObject root;
    const int pages = outputPath == QStringLiteral("-")
        ? 0 : countPdfPages(outputPath);
    root.insert(QStringLiteral("pages"), pages);
    root.insert(QStringLiteral("title"), global.documentTitle);
    root.insert(QStringLiteral("author"), global.author);
    root.insert(QStringLiteral("subject"), global.subject);
    root.insert(QStringLiteral("keywords"), global.keywords);
    root.insert(QStringLiteral("pageSize"), global.pageLayout.pageSize().name());
    root.insert(QStringLiteral("orientation"),
                global.pageLayout.orientation() == QPageLayout::Landscape
                    ? QStringLiteral("Landscape") : QStringLiteral("Portrait"));
    QJsonArray outlineArray;
    for (const OutlineEntry& entry : outline) {
        QJsonObject item;
        item.insert(QStringLiteral("title"), entry.title);
        item.insert(QStringLiteral("level"), entry.level);
        item.insert(QStringLiteral("page"), entry.page);
        outlineArray.append(item);
    }
    root.insert(QStringLiteral("outline"), outlineArray);
    return writeAllFile(global.dumpInfo,
                        QJsonDocument(root).toJson(QJsonDocument::Indented), error);
}

}

bool HtmlToPdfConverter::convert(const ObjectSettings& input, const QString& outputPath, QString* error)
{
    return convert(QList<ObjectSettings>{input}, outputPath, error);
}

bool HtmlToPdfConverter::convert(const QList<ObjectSettings>& inputs, const QString& outputPath, QString* error)
{
    logAt(m_global, 4, QStringLiteral("Output: %1").arg(outputPath));
    logAt(m_global, 4, QStringLiteral("Page: %1 %2")
         .arg(m_global.pageLayout.pageSize().name(),
              m_global.pageLayout.orientation() == QPageLayout::Landscape
                  ? QStringLiteral("Landscape") : QStringLiteral("Portrait")));
    logAt(m_global, 4, QStringLiteral("Objects: %1").arg(inputs.size()));
    const bool success = convertDocuments(inputs, outputPath, error, true);
    if (success && !m_global.dumpInfo.isEmpty()) {
        QString infoError;
        if (!writeInfoJson(outputPath, m_lastOutline, m_global, &infoError)) {
            if (error) *error = infoError;
            return false;
        }
    }
    return success;
}

bool HtmlToPdfConverter::convertObjectPipeline(const QList<ObjectSettings>& objects,
                                               const QString& outputPath, QString* error)
{
    QList<ObjectSettings> work = objects;
    bool hasTocKind = false;
    int tocTemplate = -1;
    for (int i = 0; i < work.size(); ++i) {
        if (work.at(i).kind == ObjectKind::Toc) hasTocKind = true;
        if (work.at(i).toc && tocTemplate < 0) tocTemplate = i;
    }
    if (!hasTocKind && tocTemplate >= 0) {
        ObjectSettings toc = work.at(tocTemplate);
        toc.kind = ObjectKind::Toc;
        toc.page.clear();
        toc.toc = false;
        toc.localOptions = true;
        applyCoverRules(&toc);
        work.insert(0, toc);
    }
    for (ObjectSettings& object : work) {
        applyCoverRules(&object);
        if (object.kind == ObjectKind::Toc) object.toc = false;
        else if (hasTocKind || tocTemplate >= 0) object.toc = false;
    }

    QStringList pdfs(work.size());
    QList<QList<OutlineEntry>> outlines(work.size());
    QList<int> pageCounts(work.size(), 0);
    QList<QTemporaryFile*> temps;
    auto cleanup = [&]() {
        for (QTemporaryFile* file : temps) delete file;
    };

    for (int i = 0; i < work.size(); ++i) {
        if (work.at(i).kind == ObjectKind::Toc) continue;
        const char* kindName = work.at(i).kind == ObjectKind::Cover ? "cover" : "page";
        logAt(m_global, 4, QStringLiteral("Object %1/%2 (%3): %4")
             .arg(i + 1).arg(work.size()).arg(QLatin1String(kindName), work.at(i).page));
        auto* temp = new QTemporaryFile(QDir::tempPath() + QStringLiteral("/wkhtmltopdf-ng-obj-XXXXXX.pdf"));
        temp->setAutoRemove(true);
        if (!temp->open()) {
            cleanup();
            if (error) *error = temp->errorString();
            delete temp;
            return false;
        }
        const QString path = temp->fileName();
        temp->close();
        temps.append(temp);
        m_lastOutline.clear();
        if (!convertDocuments(QList<ObjectSettings>{work.at(i)}, path, error, false)) {
            cleanup();
            return false;
        }
        pdfs[i] = path;
        outlines[i] = m_lastOutline;
        pageCounts[i] = countPdfPages(path);
    }

    for (int i = 0; i < work.size(); ++i) {
        if (work.at(i).kind != ObjectKind::Toc) continue;
        logAt(m_global, 4, QStringLiteral("Object %1/%2 (toc)").arg(i + 1).arg(work.size()));

        // Heading pages shown in the TOC must be absolute (they include the
        // pages of everything before each heading AND the TOC's own pages).
        // The TOC page count is only known after rendering, so render once,
        // correct the numbers, and re-render if the TOC spans more pages
        // than guessed. Page numbers never change the row count, so this
        // converges on the second attempt in practice.
        auto tocHeadings = [&](int tocPagesGuess) {
            QList<OutlineEntry> result;
            int offset = 0;
            for (int k = 0; k < work.size(); ++k) {
                if (k == i) {
                    offset += tocPagesGuess;
                    continue;
                }
                if (work.at(k).kind != ObjectKind::Cover &&
                    work.at(k).kind != ObjectKind::Toc &&
                    work.at(k).includeInOutline) {
                    for (const OutlineEntry& entry : outlines.at(k)) {
                        OutlineEntry corrected = entry;
                        corrected.page += offset;
                        result.append(corrected);
                    }
                }
                offset += pageCounts.at(k);
            }
            return result;
        };

        QString tocPdfPath;
        int tocPages = 1; // first guess; corrected after the first render
        for (int attempt = 0; attempt < 3; ++attempt) {
            const QList<OutlineEntry> correctedHeadings = tocHeadings(tocPages);
            QString tocHtml;
            if (!work.at(i).tocXsl.isEmpty()) {
                QString xsltError;
                if (!applyTocXsl(work.at(i).tocXsl, outlineXmlDocument(correctedHeadings),
                                 &tocHtml, &xsltError)) {
                    cleanup();
                    if (error) *error = xsltError;
                    return false;
                }
            } else {
                tocHtml = tocHtmlDocument(work.at(i), correctedHeadings);
            }
            auto* htmlFile = new QTemporaryFile(QDir::tempPath() + QStringLiteral("/wkhtmltopdf-ng-toc-XXXXXX.html"));
            htmlFile->setAutoRemove(true);
            if (!htmlFile->open()) {
                cleanup();
                if (error) *error = htmlFile->errorString();
                delete htmlFile;
                return false;
            }
            const QByteArray html = tocHtml.toUtf8();
            if (htmlFile->write(html) != html.size()) {
                cleanup();
                if (error) *error = QStringLiteral("cannot write TOC HTML");
                delete htmlFile;
                return false;
            }
            htmlFile->close();
            temps.append(htmlFile);

            ObjectSettings tocObject = work.at(i);
            tocObject.kind = ObjectKind::Page;
            tocObject.page = htmlFile->fileName();
            tocObject.enableLocalFileAccess = true;
            tocObject.includeInOutline = false;
            tocObject.toc = false;

            auto* pdfFile = new QTemporaryFile(QDir::tempPath() + QStringLiteral("/wkhtmltopdf-ng-toc-XXXXXX.pdf"));
            pdfFile->setAutoRemove(true);
            if (!pdfFile->open()) {
                cleanup();
                if (error) *error = pdfFile->errorString();
                delete pdfFile;
                return false;
            }
            const QString pdfPath = pdfFile->fileName();
            pdfFile->close();
            temps.append(pdfFile);
            m_lastOutline.clear();
            if (!convertDocuments(QList<ObjectSettings>{tocObject}, pdfPath, error, false)) {
                cleanup();
                return false;
            }
            tocPdfPath = pdfPath;
            pageCounts[i] = countPdfPages(pdfPath);
            if (pageCounts.at(i) == tocPages) break;
            tocPages = pageCounts.at(i);
        }
        pdfs[i] = tocPdfPath;
        outlines[i] = m_lastOutline;
    }

    QTemporaryFile merged(QDir::tempPath() + QStringLiteral("/wkhtmltopdf-ng-merged-XXXXXX.pdf"));
    merged.setAutoRemove(true);
    if (!merged.open()) {
        cleanup();
        if (error) *error = merged.errorString();
        return false;
    }
    const QString mergedPath = merged.fileName();
    merged.close();
    logAt(m_global, 4, QStringLiteral("Merging %1 objects").arg(work.size()));
    if (!mergePdfFiles(pdfs, mergedPath, error)) {
        cleanup();
        return false;
    }

    QString currentPath = mergedPath;

    // Per-object page-number overlay. Applied BEFORE --page-ranges (matching
    // the single-object path) so [page]/[topage] refer to the full document,
    // and each page is overlaid with the header/footer of the object it came
    // from. Cover and TOC pages draw no header/footer.
    QList<ObjectSettings> pageObjects;
    bool needOverlay = false;
    const bool globalForce = !m_global.watermark.trimmed().isEmpty() ||
        m_global.skipHeaderOnFirst ||
        m_global.headerOn != QStringLiteral("all") ||
        m_global.footerOn != QStringLiteral("all");
    for (int i = 0; i < work.size(); ++i) {
        const ObjectSettings& object = work.at(i);
        const bool bodyObject = object.kind != ObjectKind::Cover && object.kind != ObjectKind::Toc;
        if (bodyObject && (globalForce || hasPageNumberPlaceholders(object))) needOverlay = true;
        for (int page = 0; page < pageCounts.at(i); ++page) pageObjects.append(object);
    }
    QTemporaryFile numbered(QDir::tempPath() + QStringLiteral("/wkhtmltopdf-ng-numbered-XXXXXX.pdf"));
    if (needOverlay) {
        if (!numbered.open()) {
            cleanup();
            if (error) *error = numbered.errorString();
            return false;
        }
        const QString numberedPath = numbered.fileName();
        numbered.close();
        QString overlayError;
        if (!applyPageNumberOverlay(currentPath, numberedPath, m_global, pageObjects, &overlayError)) {
            cleanup();
            if (error) *error = overlayError;
            return false;
        }
        currentPath = numberedPath;
    }

    QTemporaryFile ranged(QDir::tempPath() + QStringLiteral("/wkhtmltopdf-ng-ranged-XXXXXX.pdf"));
    if (!m_global.pageRanges.trimmed().isEmpty()) {
        if (!ranged.open()) {
            cleanup();
            if (error) *error = ranged.errorString();
            return false;
        }
        const QString rangedPath = ranged.fileName();
        ranged.close();
        QString rangeError;
        if (!applyPdfPageRanges(currentPath, rangedPath, m_global.pageRanges, &rangeError)) {
            cleanup();
            if (error) *error = rangeError;
            return false;
        }
        currentPath = rangedPath;
    }

    QList<OutlineEntry> combinedOutline;
    int pageOffset = 0;
    for (int i = 0; i < work.size(); ++i) {
        for (OutlineEntry entry : outlines.at(i)) {
            if (work.at(i).kind == ObjectKind::Cover || !work.at(i).includeInOutline) continue;
            entry.page += pageOffset;
            combinedOutline.append(entry);
        }
        pageOffset += pageCounts.at(i);
    }
    m_lastOutline = combinedOutline;

    if (!m_global.outline.isEmpty()) {
        const QString xml = outlineXmlDocument(combinedOutline);
        QString writeError;
        if (!writeAllFile(m_global.outline, xml.toUtf8(), &writeError)) {
            cleanup();
            if (error) *error = writeError;
            return false;
        }
    }

    QTemporaryFile outlined(QDir::tempPath() + QStringLiteral("/wkhtmltopdf-ng-outlined-XXXXXX.pdf"));
    QString writeError;
    if (m_global.outlineEnabled && !combinedOutline.isEmpty()) {
        if (!outlined.open()) {
            cleanup();
            if (error) *error = outlined.errorString();
            return false;
        }
        const QString outlinedPath = outlined.fileName();
        outlined.close();
        remapOutlinePages(&combinedOutline, countPdfPages(currentPath));
        if (!embedPdfOutlines(currentPath, outlinedPath, combinedOutline, &writeError)) {
            cleanup();
            if (error) *error = writeError;
            return false;
        }
        currentPath = outlinedPath;
    }

    QTemporaryFile finalized(QDir::tempPath() + QStringLiteral("/wkhtmltopdf-ng-final-XXXXXX.pdf"));
    const bool needsFinalize = m_global.copies > 1 || !m_global.useCompression ||
        m_global.recompressPdf || m_global.compressionLevel > 0 || m_global.optimizePdfImages;
    const bool needsExtras = pdfNeedsDocumentExtras(m_global);
    if (needsFinalize) {
        if (!finalized.open()) {
            cleanup();
            if (error) *error = finalized.errorString();
            return false;
        }
        const QString finalizedPath = finalized.fileName();
        finalized.close();
        if (!applyPdfCopiesAndCompression(currentPath, finalizedPath, m_global, &writeError)) {
            cleanup();
            if (error) *error = writeError;
            return false;
        }
        currentPath = finalizedPath;
    }

    QTemporaryFile extras(QDir::tempPath() + QStringLiteral("/wkhtmltopdf-ng-extras-XXXXXX.pdf"));
    if (needsExtras) {
        if (!extras.open()) {
            cleanup();
            if (error) *error = extras.errorString();
            return false;
        }
        const QString extrasPath = extras.fileName();
        extras.close();
        if (!applyPdfDocumentExtras(currentPath, extrasPath, m_global, &writeError)) {
            cleanup();
            if (error) *error = writeError;
            return false;
        }
        currentPath = extrasPath;
    }

    QFile result(currentPath);
    if (!result.open(QIODevice::ReadOnly)) {
        cleanup();
        if (error) *error = result.errorString();
        return false;
    }
    const QByteArray processed = result.readAll();
    result.close();
    cleanup();
    const bool written = outputPath == QStringLiteral("-")
        ? writeAllToStdout(processed, error)
        : writeAllFile(outputPath, processed, error);
    return written;
}

bool HtmlToPdfConverter::convertDocuments(const QList<ObjectSettings>& inputs, const QString& outputPath,
                                          QString* error, bool finalize)
{
    if (inputs.isEmpty()) {
        if (error) *error = QStringLiteral("no input documents supplied");
        return false;
    }
    if (finalize && usesObjectPipeline(inputs)) {
        reportPhase(0, 0);
        const bool ok = convertObjectPipeline(inputs, outputPath, error);
        if (ok) reportPhase(5, 100);
        return ok;
    }
    if (finalize) reportPhase(0, 0);

    ObjectSettings object = inputs.first();
    applyCoverRules(&object);
    m_lastOutline.clear();
    if (!object.proxy.isEmpty()) {
        const QString scheme = object.proxy.scheme().isEmpty()
            ? QStringLiteral("http") : object.proxy.scheme();
        const int port = object.proxy.port() > 0
            ? object.proxy.port()
            : (scheme == QStringLiteral("https") ? 443 : 80);
        const QString proxyServer = QStringLiteral("%1://%2:%3")
            .arg(scheme, object.proxy.host()).arg(port);
        QByteArray flags = qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");
        if (!flags.isEmpty()) flags += ' ';
        flags += QByteArrayLiteral("--proxy-server=") + proxyServer.toUtf8();
        if (!object.bypassProxyFor.isEmpty()) {
            flags += QByteArrayLiteral(" --proxy-bypass-list=") +
                object.bypassProxyFor.join(QLatin1Char(',')).toUtf8();
        }
        qputenv("QTWEBENGINE_CHROMIUM_FLAGS", flags);
    }
    object.headerHtml = contentOrFile(object.headerHtml, error);
    if (error && !error->isEmpty()) return false;
    object.footerHtml = contentOrFile(object.footerHtml, error);
    if (error && !error->isEmpty()) return false;
    ensureHeaderFooterMargins(&m_global, object);
    const bool forceOverlay = !m_global.watermark.isEmpty() || m_global.skipHeaderOnFirst ||
        m_global.headerOn != QStringLiteral("all") || m_global.footerOn != QStringLiteral("all");
    const bool suppressCssHeaders = hasPageNumberPlaceholders(object) || forceOverlay;
    const bool pageNumberOverlay = finalize && suppressCssHeaders;

    QWebEngineProfile profile;
    if (!m_global.cacheDir.isEmpty()) profile.setCachePath(m_global.cacheDir);
#if QT_CONFIG(ssl)
    if (!m_global.sslCrtPath.isEmpty() && !m_global.sslKeyPath.isEmpty() &&
        profile.clientCertificateStore()) {
        QFile certFile(m_global.sslCrtPath);
        QFile keyFile(m_global.sslKeyPath);
        if (certFile.open(QIODevice::ReadOnly) && keyFile.open(QIODevice::ReadOnly)) {
            const QSslCertificate certificate(certFile.readAll());
            const QSslKey key(keyFile.readAll(), QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey,
                              m_global.sslKeyPassword.toUtf8());
            if (!certificate.isNull() && !key.isNull())
                profile.clientCertificateStore()->add(certificate, key);
        }
    }
    if (!m_global.caCertificate.isEmpty()) {
        QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();
        ssl.addCaCertificates(m_global.caCertificate);
        QSslConfiguration::setDefaultConfiguration(ssl);
    }
#endif
    if (!loadCookieJar(&profile, object.cookieJar, error)) return false;
    for (const auto& cookie : object.extraCookies) {
        QNetworkCookie extra(cookie.first.toUtf8(), cookie.second.toUtf8());
        const QUrl origin = inputUrl(object.page);
        profile.cookieStore()->setCookie(extra, origin);
    }
    HeaderInterceptor interceptor(object.customHeaders, object.customHeaderPropagation,
                                  object.enableLocalFileAccess, object.allowedPaths);
    if (!object.customHeaders.isEmpty() ||
        (!object.enableLocalFileAccess && !object.allowedPaths.isEmpty())) {
        profile.setUrlRequestInterceptor(&interceptor);
    }

    class LoggingPage final : public QWebEnginePage {
    public:
        using QWebEnginePage::QWebEnginePage;
        bool debug = false;
    protected:
        void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel, const QString& message,
                                      int line, const QString& source) override
        {
            if (!debug) return;
            QTextStream(stderr) << "js: " << source << ':' << line << ": " << message << '\n';
        }
    };
    LoggingPage page(&profile);
    page.debug = object.debugJavascript;
    if (!object.proxyAuth.isEmpty() || !object.proxy.userName().isEmpty()) {
        QObject::connect(&page, &QWebEnginePage::proxyAuthenticationRequired, &page,
                         [&](const QUrl&, QAuthenticator* authenticator, const QString&) {
            QString user = object.proxy.userName();
            QString password = object.proxy.password();
            if (!object.proxyAuth.isEmpty()) {
                const int separator = object.proxyAuth.indexOf(QLatin1Char(':'));
                if (separator >= 0) {
                    user = object.proxyAuth.left(separator);
                    password = object.proxyAuth.mid(separator + 1);
                }
            }
            authenticator->setUser(user);
            authenticator->setPassword(password);
        });
    }
    if (!object.username.isEmpty() || !object.password.isEmpty()) {
        QObject::connect(&page, &QWebEnginePage::authenticationRequired, &page,
                         [&](const QUrl&, QAuthenticator* authenticator) {
            authenticator->setUser(object.username);
            authenticator->setPassword(object.password);
        });
    }
    QWebEngineSettings* webSettings = page.settings();
    webSettings->setAttribute(QWebEngineSettings::JavascriptEnabled, object.enableJavascript);
    webSettings->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls,
                              object.enableLocalFileAccess || !object.allowedPaths.isEmpty());
    // Remote http(s) from a file:// page is allowed by default, matching
    // wkhtmltopdf 0.12. Local files stay gated on --enable-local-file-access.
    webSettings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    webSettings->setAttribute(QWebEngineSettings::AutoLoadImages, object.loadImages);
    webSettings->setAttribute(QWebEngineSettings::PluginsEnabled, object.enablePlugins);
    webSettings->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, false);
    if (object.minimumFontSize > 0) {
        webSettings->setFontSize(QWebEngineSettings::MinimumFontSize, object.minimumFontSize);
    }
    webSettings->setAttribute(QWebEngineSettings::PrintElementBackgrounds, object.printBackground);

    int viewportWidth = 0;
    int viewportHeight = 0;
    if (!parseViewportSize(object.viewportSize, &viewportWidth, &viewportHeight)) {
        if (error) *error = QStringLiteral("invalid viewport size %1").arg(object.viewportSize);
        return false;
    }
    if (viewportWidth <= 0 || viewportHeight <= 0) {
        const QSize fallback = defaultPdfViewport(m_global.pageLayout);
        viewportWidth = fallback.width();
        viewportHeight = fallback.height();
    }
    // Must show() the view: resize() alone leaves an offscreen widget at 0x0,
    // so Chromium keeps innerWidth=0 and screen max-width queries match.
    std::unique_ptr<QWebEngineView> view = std::make_unique<QWebEngineView>();
    view->setPage(&page);
    view->resize(viewportWidth, viewportHeight);
    view->show();
    logAt(m_global, 4, QStringLiteral("Viewport: %1x%2").arg(viewportWidth).arg(viewportHeight));

    QEventLoop loop;
    QElapsedTimer timeout;
    QElapsedTimer paintWait;
    timeout.start();
    int loadAttempts = 0;
    bool success = false;
    QString localError;
    bool printStarted = false;
    int inputIndex = 0;
    bool finalDocumentLoaded = inputs.size() == 1;
    QStringList documentHeads;
    QStringList documentBodies;
    QList<OutlineEntry> outlineEntries;
    double printScale = 1.0;

    auto finish = [&](bool ok, const QString& message = QString()) {
        if (success) return;
        success = ok;
        if (!message.isEmpty()) localError = message;
        loop.quit();
    };
    if (m_global.timeoutMs > 0) {
        QTimer::singleShot(m_global.timeoutMs, &loop, [&]() {
            finish(false, QStringLiteral("timed out after %1 ms").arg(m_global.timeoutMs));
        });
    }

    std::function<void()> printPage;
    std::function<void()> waitForWindowStatus;
    std::function<void()> dumpOutlineThenWait;
    std::function<void()> applyPrintScale;
    std::function<void(double)> finishScale;
    std::function<void()> optimizeImages;
    std::function<void()> waitImageOptimization;
    std::function<void()> runNext;
    std::function<void()> waitForFonts;
    std::function<void()> checkMediaThenInject;
    auto startPrint = [&]() {
        logAt(m_global, 4, QStringLiteral("Printing %1").arg(outputPath));
        if (finalize) reportPhase(4, 80);
        const QPageLayout layout = m_global.pageLayout;
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        QPageRanges ranges;
        if (finalize) parsePageRanges(m_global.pageRanges, &ranges);
#endif
        page.printToPdf([&](const QByteArray& rawPdf) {
            if (rawPdf.isEmpty()) {
                finish(false, QStringLiteral("Qt WebEngine returned an empty PDF"));
                return;
            }
            QByteArray pdf = object.keepRelativeLinks
                ? relativizeLocalFileUris(rawPdf, object.page)
                : rawPdf;
            QString writeError;
            const bool nativeOutlines = finalize && m_global.outlineEnabled && !outlineEntries.isEmpty();
            const bool needsFinalize = finalize && (m_global.copies > 1 || !m_global.useCompression ||
                m_global.recompressPdf || m_global.compressionLevel > 0 || m_global.optimizePdfImages);
            const bool needsExtras = finalize && pdfNeedsDocumentExtras(m_global);
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
            const bool needsRanges = false;
#else
            const bool needsRanges = finalize && !m_global.pageRanges.trimmed().isEmpty();
#endif
            const bool needsPost = pageNumberOverlay || nativeOutlines || needsFinalize || needsRanges ||
                needsExtras;
            bool written = false;
            if (!needsPost) {
                written = outputPath == QStringLiteral("-")
                    ? writeAllToStdout(pdf, &writeError)
                    : writeAllFile(outputPath, pdf, &writeError);
                finish(written, written ? QString() : writeError);
                return;
            }

            QTemporaryFile raw(QDir::tempPath() + QStringLiteral("/wkhtmltopdf-ng-raw-XXXXXX.pdf"));
            raw.setAutoRemove(true);
            if (!raw.open() || raw.write(pdf) != pdf.size()) {
                finish(false, QStringLiteral("cannot create temporary PDF"));
                return;
            }
            raw.close();
            QString currentPath = raw.fileName();
            written = true;

            QTemporaryFile numbered(QDir::tempPath() + QStringLiteral("/wkhtmltopdf-ng-numbered-XXXXXX.pdf"));
            if (pageNumberOverlay) {
                if (!numbered.open()) {
                    finish(false, numbered.errorString());
                    return;
                }
                const QString numberedPath = numbered.fileName();
                numbered.close();
                written = applyPageNumberOverlay(currentPath, numberedPath, m_global,
                                                 QList<ObjectSettings>{object}, &writeError);
                if (written) currentPath = numberedPath;
            }

            QTemporaryFile outlined(QDir::tempPath() + QStringLiteral("/wkhtmltopdf-ng-outlined-XXXXXX.pdf"));
            if (written && nativeOutlines) {
                if (!outlined.open()) {
                    written = false;
                    writeError = outlined.errorString();
                } else {
                    const QString outlinedPath = outlined.fileName();
                    outlined.close();
                    remapOutlinePages(&outlineEntries, countPdfPages(currentPath));
                    written = embedPdfOutlines(currentPath, outlinedPath, outlineEntries, &writeError);
                    if (written) currentPath = outlinedPath;
                }
            }

            QTemporaryFile ranged(QDir::tempPath() + QStringLiteral("/wkhtmltopdf-ng-ranged-XXXXXX.pdf"));
            if (written && needsRanges) {
                if (!ranged.open()) {
                    written = false;
                    writeError = ranged.errorString();
                } else {
                    const QString rangedPath = ranged.fileName();
                    ranged.close();
                    written = applyPdfPageRanges(currentPath, rangedPath, m_global.pageRanges, &writeError);
                    if (written) currentPath = rangedPath;
                }
            }

            QTemporaryFile finalized(QDir::tempPath() + QStringLiteral("/wkhtmltopdf-ng-final-XXXXXX.pdf"));
            if (written && needsFinalize) {
                if (!finalized.open()) {
                    written = false;
                    writeError = finalized.errorString();
                } else {
                    const QString finalizedPath = finalized.fileName();
                    finalized.close();
                    written = applyPdfCopiesAndCompression(currentPath, finalizedPath, m_global, &writeError);
                    if (written) currentPath = finalizedPath;
                }
            }

            QTemporaryFile extras(QDir::tempPath() + QStringLiteral("/wkhtmltopdf-ng-extras-XXXXXX.pdf"));
            if (written && needsExtras) {
                if (!extras.open()) {
                    written = false;
                    writeError = extras.errorString();
                } else {
                    const QString extrasPath = extras.fileName();
                    extras.close();
                    written = applyPdfDocumentExtras(currentPath, extrasPath, m_global, &writeError);
                    if (written) currentPath = extrasPath;
                }
            }

            if (written) {
                QFile result(currentPath);
                if (!result.open(QIODevice::ReadOnly)) {
                    written = false;
                    writeError = result.errorString();
                } else {
                    const QByteArray processed = result.readAll();
                    written = outputPath == QStringLiteral("-")
                        ? writeAllToStdout(processed, &writeError)
                        : writeAllFile(outputPath, processed, &writeError);
                }
            }
            finish(written, written ? QString() : writeError);
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        }, layout, ranges);
#else
        }, layout);
#endif
    };

    printPage = [&]() {
        if (printStarted) return;
        printStarted = true;
        if (m_global.logLevel != QStringLiteral("verbose") || !object.enableJavascript) {
            startPrint();
            return;
        }
        page.runJavaScript(rendererDiagnosticsJavascript(), [&](const QVariant& value) {
            const QJsonDocument json = QJsonDocument::fromVariant(value);
            logAt(m_global, 4, QStringLiteral("Renderer diagnostics: %1")
                 .arg(QString::fromUtf8(json.toJson(QJsonDocument::Compact))));
            startPrint();
        });
    };

    waitForWindowStatus = [&]() {
        const int statusBudget = object.stopSlowScripts ? 2000 : 120000;
        if (timeout.elapsed() > statusBudget) {
            finish(false, QStringLiteral("timed out waiting for window.status"));
            return;
        }
        if (object.windowStatus.isEmpty()) {
            printPage();
            return;
        }
        page.runJavaScript(QStringLiteral("String(window.status)"), [&](const QVariant& value) {
            if (value.toString() == object.windowStatus) QTimer::singleShot(0, &loop, printPage);
            else QTimer::singleShot(100, &loop, waitForWindowStatus);
        });
    };

    dumpOutlineThenWait = [&]() {
        if (finalize) reportPhase(2, 40);
        if (!object.includeInOutline ||
            (finalize && !m_global.outlineEnabled && m_global.outline.isEmpty())) {
            if (finalize) reportPhase(3, 60);
            QTimer::singleShot(0, &loop, waitForWindowStatus);
            return;
        }

        const QSizeF pageMillimeters = m_global.pageLayout.pageSize().size(QPageSize::Millimeter);
        const QMarginsF margins = m_global.pageLayout.margins(QPageLayout::Millimeter);
        const qreal contentHeightPixels = qMax<qreal>(1.0,
            (pageMillimeters.height() - margins.top() - margins.bottom()) * 96.0 / 25.4
            / qMax(0.01, printScale));
        const int tocOffset = object.toc ? 1 : 0;
        page.runJavaScript(QStringLiteral(
            "(function(){const pageHeight=%1;const tocOffset=%2;"
            "const sections=Array.from(document.querySelectorAll('.wkhtmltopdf-ng-document'));"
            "const offsets=new Map();let offset=tocOffset;"
            "sections.forEach(function(section){offsets.set(section,offset);"
            "offset+=Math.max(1,Math.ceil(section.scrollHeight/pageHeight));});"
            "return Array.from(document.querySelectorAll('h1,h2,h3,h4,h5,h6')).map(function(el,index){"
            "const id=el.id||('wkhtmltopdf-ng-heading-'+index);el.id=id;"
            "const section=el.closest('.wkhtmltopdf-ng-document');"
            "const base=section?offsets.get(section):tocOffset;"
            "const relative=section?el.offsetTop-section.offsetTop:el.offsetTop;"
            "return {title:el.textContent.trim(),level:Number(el.tagName.substring(1)),"
            "page:base+Math.floor(relative/pageHeight)+1,id:id};});})()")
                .arg(contentHeightPixels).arg(tocOffset),
            [&](const QVariant& value) {
                const QVariantList entries = value.toList();
                for (const QVariant& entry : entries) {
                    const QVariantMap item = entry.toMap();
                    if (item.value(QStringLiteral("level")).toInt() > m_global.outlineDepth) continue;
                    OutlineEntry outline;
                    outline.title = item.value(QStringLiteral("title")).toString();
                    outline.level = item.value(QStringLiteral("level")).toInt();
                    outline.page = item.value(QStringLiteral("page")).toInt();
                    const QString id = item.value(QStringLiteral("id")).toString();
                    if (!id.isEmpty()) outline.link = QLatin1Char('#') + id;
                    if (object.tocBackLinks) outline.backLink = QStringLiteral("#wkhtmltopdf-ng-toc");
                    outlineEntries.append(outline);
                }
                const QString xml = outlineXmlDocument(outlineEntries);

                if (finalize && !m_global.outline.isEmpty()) {
                    QString writeError;
                    if (!writeAllFile(m_global.outline, xml.toUtf8(), &writeError)) {
                        finish(false, writeError);
                        return;
                    }
                }
                if (finalize) reportPhase(3, 60);
                QTimer::singleShot(0, &loop, waitForWindowStatus);
            });
    };

    finishScale = [&](double scale) {
        printScale = scale > 0.0 ? scale : 1.0;
        if (!object.enableJavascript) {
            page.setZoomFactor(printScale);
            optimizeImages();
            return;
        }
        if (qFuzzyCompare(printScale, 1.0)) {
            optimizeImages();
            return;
        }
        page.runJavaScript(
            QStringLiteral(
                "(function(){document.querySelectorAll('style[data-wkhtmltopdf-ng-scale]').forEach("
                "function(el){el.remove();});const s=document.createElement('style');"
                "s.setAttribute('data-wkhtmltopdf-ng-scale','1');"
                "s.textContent='html{zoom:%1}';document.head.appendChild(s);})()")
                .arg(QString::number(printScale, 'g', 12)),
            [&](const QVariant&) { QTimer::singleShot(0, &loop, optimizeImages); });
    };

    applyPrintScale = [&]() {
        const double baseScale = layoutScale(m_global.dpi, object.zoom);
        if (!object.enableSmartShrinking || !object.enableJavascript) {
            finishScale(baseScale);
            return;
        }

        const double pageWidth = printableWidthPx(m_global.pageLayout);
        page.runJavaScript(
            QStringLiteral(
                "(function(){const root=document.documentElement;"
                "const body=document.body;"
                "return Math.max(root?root.scrollWidth:0,body?body.scrollWidth:0);})()"),
            [&, baseScale, pageWidth](const QVariant& value) {
                const double contentWidth = value.toDouble();
                double scale = baseScale;
                if (contentWidth > 0.0) {
                    const double visualWidth = contentWidth * baseScale;
                    if (visualWidth > pageWidth) scale = pageWidth / contentWidth;
                }
                finishScale(scale);
            });
    };

    optimizeImages = [&]() {
        if (!pdfNeedsImageOptimization(m_global) || !object.enableJavascript) {
            dumpOutlineThenWait();
            return;
        }
        int dpi = 600;
        int quality = 94;
        effectiveImageOptimization(m_global, &dpi, &quality);
        page.runJavaScript(imageDownsampleJavascript(dpi, quality), [&](const QVariant&) {
            const qint64 started = timeout.elapsed();
            waitImageOptimization = [&, started]() {
                if (timeout.elapsed() - started > 30000) {
                    dumpOutlineThenWait();
                    return;
                }
                page.runJavaScript(QStringLiteral("window.__wkhtmltopdfNgImages === true"),
                    [&](const QVariant& value) {
                        if (value.toBool()) dumpOutlineThenWait();
                        else QTimer::singleShot(50, &loop, waitImageOptimization);
                    });
            };
            QTimer::singleShot(0, &loop, waitImageOptimization);
        });
    };

    auto injectAndPrint = [&]() {
        const QString headerFooter = suppressCssHeaders ? QString() : headerFooterJavascript(object);
        const QString toc = object.toc ? tocJavascript(object) : QString();
        runNext = [&, headerFooter]() {
            QStringList scripts = object.runScripts;
            if (scripts.isEmpty() && !object.runScript.isEmpty()) scripts.append(object.runScript);
            if (scripts.isEmpty()) {
                applyPrintScale();
                return;
            }
            page.runJavaScript(scripts.join(QStringLiteral(";\n")),
                               [&](const QVariant&) { applyPrintScale(); });
        };

        const bool hasTitle = !m_global.documentTitle.isEmpty();
        const bool hasGrayscale = m_global.grayscale;
        const bool hideBackground = !object.printBackground;
        const bool printBackgrounds = object.printBackground;

        QString css;
        if (!object.userStyleSheet.isEmpty()) {
            bool ok = false;
            css = contentOrFile(object.userStyleSheet, &localError);
            if (!localError.isEmpty()) {
                finish(false, localError);
                return;
            }
            Q_UNUSED(ok);
        }
        QString script;
        if (!css.isEmpty()) {
            script += QStringLiteral("(function(){const s=document.createElement('style');s.textContent=%1;document.head.appendChild(s);})();\n")
                .arg(javascriptString(css));
        }
        if (hasTitle) {
            script += QStringLiteral("document.title=%1;\n").arg(javascriptString(m_global.documentTitle));
        }
        if (!headerFooter.isEmpty()) script += headerFooter;
        if (!toc.isEmpty()) script += toc;
        if (hasGrayscale) {
            script += QStringLiteral(
                "(function(){const s=document.createElement('style');s.textContent='@media print{html{filter:grayscale(1)}}';document.head.appendChild(s);})();\n");
        }
        if (hideBackground) {
            script += QStringLiteral(
                "(function(){const s=document.createElement('style');s.textContent="
                "'@media print{*{background-image:none!important;box-shadow:none!important;}"
                "html,body,*{background-color:transparent!important}html,body{background:#fff!important}}';"
                "document.head.appendChild(s);})();\n");
        } else if (printBackgrounds) {
            script += printBackgroundJavascript();
        }
        if (object.disableExternalLinks || object.disableInternalLinks || object.disableForms) {
            script += QStringLiteral("(function(){");
            if (object.disableExternalLinks) {
                script += QStringLiteral(
                    "document.querySelectorAll('a[href]').forEach(function(a){"
                    "const href=a.getAttribute('href')||'';"
                    "if(href.charAt(0)!=='#')a.removeAttribute('href');});");
            }
            if (object.disableInternalLinks) {
                script += QStringLiteral(
                    "document.querySelectorAll('a[href^=\"#\"]').forEach(function(a){a.removeAttribute('href');});");
            }
            if (object.disableForms) {
                script += QStringLiteral("document.querySelectorAll('input,button,select,textarea').forEach(function(el){el.disabled=true;});");
            }
            script += QStringLiteral("})();\n");
        }
        if (object.keepRelativeLinks) script += keepRelativeLinksJavascript();
        if (!m_global.printMediaType) script += noPrintMediaJavascript();
        if (script.isEmpty()) {
            runNext();
            return;
        }
        page.runJavaScript(script, [&](const QVariant&) { QTimer::singleShot(0, &loop, runNext); });
    };

    waitForFonts = [&]() {
        if (!object.enableJavascript) {
            checkMediaThenInject();
            return;
        }
        // Budget starts when we begin waiting, not at convert() start — page
        // load already used most of the old 400ms window, so remote images
        // never finished unless the user padded --javascript-delay.
        if (!paintWait.isValid()) paintWait.start();
        const int paintBudget = object.stopSlowScripts ? 3000 : 20000;
        if (paintWait.elapsed() > paintBudget) {
            checkMediaThenInject();
            return;
        }
        page.runJavaScript(waitForPaintJavascript(), [&](const QVariant& value) {
            if (value.toBool()) checkMediaThenInject();
            else QTimer::singleShot(50, &loop, waitForFonts);
        });
    };

    checkMediaThenInject = [&]() {
        const QString policy = object.loadMediaErrorHandling.trimmed().toLower();
        if (!object.enableJavascript || policy == QStringLiteral("ignore") ||
            policy == QStringLiteral("skip")) {
            if (finalize) reportPhase(1, 20);
            injectAndPrint();
            return;
        }
        page.runJavaScript(
            QStringLiteral(
                "(function(){return Array.from(document.images).filter(function(img){"
                "return img.complete&&img.naturalWidth===0&&img.getAttribute('src');})"
                ".map(function(img){return img.src;});})()"),
            [&](const QVariant& value) {
                const QVariantList broken = value.toList();
                if (!broken.isEmpty() && policy == QStringLiteral("abort")) {
                    finish(false, QStringLiteral("failed to load media %1")
                                      .arg(broken.first().toString()));
                    return;
                }
                if (finalize) reportPhase(1, 20);
                injectAndPrint();
            });
    };

    auto combineLoadedDocuments = [&]() {
        if (documentBodies.isEmpty()) {
            finish(false, QStringLiteral("no input documents could be loaded"));
            return;
        }
        QString combined = QStringLiteral("<!doctype html><html><head><meta charset=\"utf-8\">");
        combined += documentHeads.join(QStringLiteral("\n"));
        combined += QStringLiteral(
            "<style>.wkhtmltopdf-ng-document{break-before:page;page-break-before:always;}"
            ".wkhtmltopdf-ng-document:first-child{break-before:auto;page-break-before:auto;}</style>"
            "</head><body>");
        for (const QString& body : documentBodies) {
            combined += QStringLiteral("<section class=\"wkhtmltopdf-ng-document\">");
            combined += body;
            combined += QStringLiteral("</section>");
        }
        combined += QStringLiteral("</body></html>");
        finalDocumentLoaded = true;
        page.setHtml(combined, QUrl::fromLocalFile(QDir::currentPath() + QLatin1Char('/')));
    };

    auto loadNextOrCombine = [&]() {
        if (inputIndex + 1 < inputs.size()) {
            ++inputIndex;
            page.load(inputUrl(inputs.at(inputIndex).page));
            return;
        }
        combineLoadedDocuments();
    };

    QObject::connect(&page, &QWebEnginePage::loadFinished, &loop, [&](bool ok) {
        if (!ok) {
            const QString policy = loadErrorPolicy(object);
            if (policy == QStringLiteral("skip")) {
                if (!finalDocumentLoaded) {
                    if (inputs.size() == 1) {
                        finish(false, QStringLiteral("failed to load %1").arg(inputs.at(inputIndex).page));
                        return;
                    }
                    loadNextOrCombine();
                    return;
                }
            } else if (policy != QStringLiteral("ignore")) {
                if (loadAttempts < m_global.retry) {
                    ++loadAttempts;
                    logAt(m_global, 4, QStringLiteral("Retry %1/%2: %3")
                         .arg(loadAttempts).arg(m_global.retry).arg(inputs.at(inputIndex).page));
                    page.triggerAction(QWebEnginePage::Reload);
                    return;
                }
                finish(false, QStringLiteral("failed to load %1").arg(inputs.at(inputIndex).page));
                return;
            }
        }

        if (!finalDocumentLoaded) {
            page.runJavaScript(QStringLiteral(
                "(function(){"
                "const absolute=function(value){try{return new URL(value,document.baseURI).href;}catch(e){return value;}};"
                "document.querySelectorAll('[src],[href],[action],[poster],[data],[cite],[background],[src]').forEach(function(el){"
                "['src','href','action','poster','data','cite','background'].forEach(function(name){if(el.hasAttribute(name)){"
                "const value=el.getAttribute(name);if(value&&!value.startsWith('#'))el.setAttribute(name,absolute(value));}});});"
                "document.querySelectorAll('video source, audio source').forEach(function(el){"
                "if(el.hasAttribute('src'))el.setAttribute('src',absolute(el.getAttribute('src')));});"
                "document.querySelectorAll('track').forEach(function(el){"
                "if(el.hasAttribute('src'))el.setAttribute('src',absolute(el.getAttribute('src')));});"
                "document.querySelectorAll('[srcset]').forEach(function(el){"
                "const values=el.getAttribute('srcset').split(',').map(function(item){"
                "const parts=item.trim().split(/\\s+/);parts[0]=absolute(parts[0]);return parts.join(' ');});"
                "el.setAttribute('srcset',values.join(', '));});"
                "document.querySelectorAll('param[value]').forEach(function(el){"
                "const value=el.getAttribute('value');if(value&&!value.startsWith('#'))el.setAttribute('value',absolute(value));});"
                "const rewriteCss=function(css){return css.replace(/url\\(\\s*(['\"]?)([^'\")]+)\\1\\s*\\)/g,function(_,quote,value){"
                "return 'url(\\\"'+absolute(value)+'\\\")';});};"
                "const rewriteImports=function(css){return css.replace(/@import\\s+url\\(\\s*(['\"]?)([^'\")]+)\\1\\s*\\)/g,function(_,quote,value){"
                "return '@import url(\\\"'+absolute(value)+'\\\")';});};"
                "document.querySelectorAll('[style]').forEach(function(el){el.setAttribute('style',rewriteImports(rewriteCss(el.getAttribute('style'))));});"
                "document.querySelectorAll('style').forEach(function(el){el.textContent=rewriteImports(rewriteCss(el.textContent));});"
                "document.querySelectorAll('[srcdoc]').forEach(function(el){"
                "const source=el.getAttribute('srcdoc');const inner=new DOMParser().parseFromString(source,'text/html');"
                "inner.querySelectorAll('[src],[href],[action],[poster],[data],[cite],[background]').forEach(function(node){"
                "['src','href','action','poster','data','cite','background'].forEach(function(name){if(node.hasAttribute(name)){"
                "const value=node.getAttribute(name);if(value&&!value.startsWith('#'))node.setAttribute(name,absolute(value));}});});"
                "inner.querySelectorAll('video source, audio source, track').forEach(function(node){"
                "if(node.hasAttribute('src'))node.setAttribute('src',absolute(node.getAttribute('src')));});"
                "inner.querySelectorAll('style').forEach(function(node){node.textContent=rewriteImports(rewriteCss(node.textContent));});"
                "el.setAttribute('srcdoc',inner.documentElement.outerHTML);});"
                "document.querySelectorAll('base').forEach(function(el){el.remove();});"
                "return {head: document.head ? document.head.innerHTML : '', body: document.body ? document.body.innerHTML : ''};"
                "})()"),
                [&](const QVariant& value) {
                    const QVariantMap document = value.toMap();
                    documentHeads.append(document.value(QStringLiteral("head")).toString());
                    documentBodies.append(document.value(QStringLiteral("body")).toString());
                    loadNextOrCombine();
                });
            return;
        }

        int delay = qMax(0, object.javascriptDelay);
        if (object.stopSlowScripts) delay = qMin(delay, 500);
        QTimer::singleShot(delay, &loop, waitForFonts);
    });

    const QString inputPage = object.page;
    logAt(m_global, 4, QStringLiteral("Loading %1").arg(inputPage));
    if (inputPage == QStringLiteral("-")) {
        if (inputs.size() > 1) {
            finish(false, QStringLiteral("stdin can only be used as a single input document"));
        } else {
            const QByteArray html = readAllFromStdin();
            if (html.isEmpty()) {
                finish(false, QStringLiteral("stdin did not contain HTML"));
            } else {
                page.setHtml(decodeText(html, object.encoding),
                             QUrl::fromLocalFile(QDir::currentPath() + QLatin1Char('/')));
            }
        }
    } else {
        if (!object.postData.isEmpty() || !object.postFile.isEmpty() ||
            !object.postFields.isEmpty() || !object.postFiles.isEmpty()) {
            QWebEngineHttpRequest request;
            request.setMethod(QWebEngineHttpRequest::Post);
            request.setUrl(inputUrl(inputPage));
            QByteArray postData;
            auto appendField = [&](const QString& name, const QByteArray& value) {
                if (!postData.isEmpty()) postData += '&';
                postData += QUrl::toPercentEncoding(name) + '=' + QUrl::toPercentEncoding(QString::fromUtf8(value));
            };
            for (const auto& field : object.postFields) appendField(field.first, field.second.toUtf8());
            for (const auto& file : object.postFiles) {
                bool ok = false;
                const QByteArray contents = readAllFile(file.second, &ok);
                if (!ok) {
                    finish(false, QStringLiteral("cannot read POST file %1").arg(file.second));
                    return success;
                }
                appendField(file.first, contents);
            }
            if (!object.postData.isEmpty()) {
                if (!postData.isEmpty()) postData += '&';
                postData += object.postData.toUtf8();
            }
            if (!object.postFile.isEmpty()) {
                bool ok = false;
                const QByteArray contents = readAllFile(object.postFile, &ok);
                if (!ok) {
                    finish(false, QStringLiteral("cannot read POST file %1").arg(object.postFile));
                    return success;
                }
                if (!postData.isEmpty()) postData += '&';
                postData += contents;
            }
            request.setPostData(postData);
            request.setHeader(QByteArrayLiteral("Content-Type"), QByteArrayLiteral("application/x-www-form-urlencoded"));
            page.load(request);
        } else {
            const QUrl url = inputUrl(inputPage);
            const bool customEncoding = !object.encoding.isEmpty() &&
                object.encoding.compare(QStringLiteral("utf-8"), Qt::CaseInsensitive) != 0;
            if (url.isLocalFile() && customEncoding) {
                bool ok = false;
                const QByteArray bytes = readAllFile(url.toLocalFile(), &ok);
                if (!ok) {
                    finish(false, QStringLiteral("cannot read %1").arg(inputPage));
                } else {
                    page.setHtml(decodeText(bytes, object.encoding), url);
                }
            } else {
                page.load(url);
            }
        }
    }

    QList<QNetworkCookie> collectedCookies;
    if (!object.cookieJar.isEmpty()) {
        QObject::connect(profile.cookieStore(), &QWebEngineCookieStore::cookieAdded, &loop,
                         [&](const QNetworkCookie& cookie) { collectedCookies.append(cookie); });
        profile.cookieStore()->loadAllCookies();
    }

    if (!success && localError.isEmpty()) loop.exec();
    if (!object.cookieJar.isEmpty()) {
        // Wait until no new cookie has arrived for a short quiet period (or a
        // hard cap) so cookies set near the end of the load are not lost.
        QEventLoop drain;
        QTimer quietTimer;
        quietTimer.setInterval(100);
        quietTimer.setSingleShot(true);
        QObject::connect(&quietTimer, &QTimer::timeout, &drain, &QEventLoop::quit);
        QObject::connect(profile.cookieStore(), &QWebEngineCookieStore::cookieAdded, &drain,
                         [&](const QNetworkCookie&) { quietTimer.start(); });
        QTimer::singleShot(3000, &drain, &QEventLoop::quit);
        quietTimer.start();
        drain.exec();
        QByteArray jar("# Netscape HTTP Cookie File\n");
        for (const QNetworkCookie& cookie : collectedCookies) {
            jar += cookie.domain().toUtf8() + '\t'
                + QByteArrayLiteral("TRUE\t")
                + cookie.path().toUtf8() + '\t'
                + (cookie.isSecure() ? "TRUE" : "FALSE") + '\t'
                + QByteArray::number(cookie.expirationDate().isValid()
                    ? cookie.expirationDate().toSecsSinceEpoch() : 0) + '\t'
                + cookie.name() + '\t' + cookie.value() + '\n';
        }
        writeAllFile(object.cookieJar, jar, nullptr);
    }
    if (!success && localError.isEmpty()) localError = QStringLiteral("conversion failed");
    if (success) m_lastOutline = outlineEntries;
    if (success && finalize) reportPhase(5, 100);
    if (error) *error = localError;
    return success;
}
