#include "PdfPostProcessor.h"
#include "PageLayout.h"

#include "utils/FileUtils.h"

#include <QFile>
#include <QStringList>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QPageRanges>
#include <QPainter>
#include <QColor>
#include <QtGlobal>
#include <QPdfWriter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryFile>
#include <QTextDocument>

QString replaceHeaderTokens(QString value, const PlaceholderContext& context)
{
    const QDateTime now = QDateTime::currentDateTime();
    const int page = context.page + context.pageOffset;
    const int pages = context.pages + context.pageOffset;
    const int from = 1 + context.pageOffset;
    value.replace(QStringLiteral("[page]"), QString::number(page), Qt::CaseInsensitive);
    value.replace(QStringLiteral("[topage]"), QString::number(pages), Qt::CaseInsensitive);
    value.replace(QStringLiteral("[frompage]"), QString::number(from), Qt::CaseInsensitive);
    value.replace(QStringLiteral("[title]"), context.title, Qt::CaseInsensitive);
    value.replace(QStringLiteral("[doctitle]"), context.documentTitle.isEmpty()
                      ? context.title : context.documentTitle, Qt::CaseInsensitive);
    value.replace(QStringLiteral("[webpage]"), context.webpage, Qt::CaseInsensitive);
    value.replace(QStringLiteral("[url]"), context.webpage, Qt::CaseInsensitive);
    value.replace(QStringLiteral("[section]"), context.section, Qt::CaseInsensitive);
    value.replace(QStringLiteral("[subsection]"), context.subsection, Qt::CaseInsensitive);
    value.replace(QStringLiteral("[sitepage]"), QString::number(context.sitePage), Qt::CaseInsensitive);
    value.replace(QStringLiteral("[sitepages]"), QString::number(context.sitePages), Qt::CaseInsensitive);
    value.replace(QStringLiteral("[isodate]"), now.date().toString(Qt::ISODate), Qt::CaseInsensitive);
    value.replace(QStringLiteral("[date]"), now.date().toString(Qt::ISODate), Qt::CaseInsensitive);
    value.replace(QStringLiteral("[time]"), now.time().toString(Qt::ISODate), Qt::CaseInsensitive);
    return value;
}

namespace {

bool containsPagePlaceholder(const QString& value)
{
    return value.contains(QStringLiteral("[page]"), Qt::CaseInsensitive) ||
           value.contains(QStringLiteral("[topage]"), Qt::CaseInsensitive) ||
           value.contains(QStringLiteral("[frompage]"), Qt::CaseInsensitive) ||
           value.contains(QStringLiteral("[sitepage]"), Qt::CaseInsensitive) ||
           value.contains(QStringLiteral("[sitepages]"), Qt::CaseInsensitive);
}

int pageCount(const QByteArray& pdf)
{
    // Chromium page trees put /Count before or after /Type /Pages, and intermediate
    // nodes have smaller counts. The document page count is the largest Pages /Count.
    const QString text = QString::fromLatin1(pdf);
    const QRegularExpression typed(
        QStringLiteral("/Type\\s*/Pages\\b.{0,800}?/Count\\s+(\\d+)|/Count\\s+(\\d+).{0,800}?/Type\\s*/Pages\\b"),
        QRegularExpression::DotMatchesEverythingOption);
    int best = 0;
    QRegularExpressionMatchIterator typedMatches = typed.globalMatch(text);
    while (typedMatches.hasNext()) {
        const QRegularExpressionMatch match = typedMatches.next();
        const int value = match.captured(1).isEmpty()
            ? match.captured(2).toInt()
            : match.captured(1).toInt();
        if (value > best) best = value;
    }
    if (best > 0) return best;

    const QRegularExpression pageExpression(QStringLiteral("/Type\\s*/Page\\b"));
    QRegularExpressionMatchIterator iterator = pageExpression.globalMatch(text);
    int count = 0;
    while (iterator.hasNext()) {
        iterator.next();
        ++count;
    }
    return count;
}

bool writeOverlay(const QString& path, int pages, const GlobalSettings& global,
                  const ObjectSettings& settings, const QString& title, QString* error)
{
    QPdfWriter writer(path);
    writer.setPageLayout(global.pageLayout);
    writer.setResolution(96);
    writer.setTitle(title);
    QPainter painter(&writer);
    if (!painter.isActive()) {
        if (error) *error = QStringLiteral("cannot create PDF page-number overlay");
        return false;
    }

    const int width = writer.width();
    const int height = writer.height();
    const QMarginsF margins = global.pageLayout.margins(QPageLayout::Millimeter);
    const qreal topMargin = margins.top() * 96.0 / 25.4;
    const qreal bottomMargin = margins.bottom() * 96.0 / 25.4;
    const QString titleText = title;

    auto contextFor = [&](int pageNumber) {
        PlaceholderContext context;
        context.page = pageNumber;
        context.pages = pages;
        context.pageOffset = global.pageOffset;
        context.sitePage = pageNumber;
        context.sitePages = pages;
        context.title = titleText;
        context.documentTitle = global.documentTitle.isEmpty() ? titleText : global.documentTitle;
        context.webpage = settings.page;
        return context;
    };

    auto withReplacements = [&](QString value, int pageNumber) {
        value = replaceHeaderTokens(value, contextFor(pageNumber));
        for (auto it = settings.replacements.cbegin(); it != settings.replacements.cend(); ++it) {
            value.replace(QLatin1Char('[') + it.key() + QLatin1Char(']'), it.value(), Qt::CaseInsensitive);
        }
        return value;
    };

    auto drawHtml = [&](const QString& value, int fontSize, const QString& fontName,
                        qreal y, int pageNumber) {
        if (value.isEmpty()) return;
        QTextDocument document;
        QFont font(fontName, fontSize);
        document.setDefaultFont(font);
        document.setHtml(withReplacements(value, pageNumber));
        document.setTextWidth(width);
        painter.save();
        painter.translate(0, y);
        document.drawContents(&painter, QRectF(0, 0, width, document.size().height()));
        painter.restore();
    };

    auto pageMatches = [](const QString& mode, int pageNumber, bool skipFirst) {
        if (skipFirst && pageNumber == 1) return false;
        if (mode == QStringLiteral("odd")) return (pageNumber % 2) == 1;
        if (mode == QStringLiteral("even")) return (pageNumber % 2) == 0;
        return true;
    };

    if (!global.documentTitle.isEmpty()) writer.setTitle(global.documentTitle);
    if (!global.author.isEmpty()) writer.setCreator(global.author);

    for (int currentPage = 1; currentPage <= pages; ++currentPage) {
        if (currentPage > 1) writer.newPage();
        auto drawAligned = [&](const QString& value, int fontSize, const QString& fontName,
                               qreal y, Qt::Alignment alignment) {
            if (value.isEmpty()) return;
            QFont font(fontName, fontSize);
            painter.setFont(font);
            const QFontMetricsF metrics(font);
            const qreal lineHeight = metrics.height();
            const QRectF bounds(0, y, width, lineHeight + 2);
            painter.drawText(bounds, alignment | Qt::AlignVCenter,
                             withReplacements(value, currentPage));
        };

        const bool drawHeader = pageMatches(global.headerOn, currentPage, global.skipHeaderOnFirst);
        const bool drawFooter = pageMatches(global.footerOn, currentPage, global.skipHeaderOnFirst);
        if (drawHeader) {
        drawHtml(settings.headerHtml, settings.headerFontSize, settings.headerFontName,
                 qMax<qreal>(0, topMargin - 2 * settings.headerFontSize), currentPage);
        drawAligned(settings.headerLeft, settings.headerFontSize, settings.headerFontName,
                    qMax<qreal>(0, topMargin - 2 * settings.headerFontSize), Qt::AlignLeft);
        drawAligned(settings.headerRight, settings.headerFontSize, settings.headerFontName,
                    qMax<qreal>(0, topMargin - 2 * settings.headerFontSize), Qt::AlignRight);
        if (!settings.headerCenter.isEmpty()) {
            drawAligned(settings.headerCenter, settings.headerFontSize, settings.headerFontName,
                        qMax<qreal>(0, topMargin - 2 * settings.headerFontSize), Qt::AlignCenter);
        }
        }

        if (drawFooter) {
        drawHtml(settings.footerHtml, settings.footerFontSize, settings.footerFontName,
                 height - bottomMargin, currentPage);
        drawAligned(settings.footerLeft, settings.footerFontSize, settings.footerFontName,
                    height - bottomMargin, Qt::AlignLeft);
        drawAligned(settings.footerRight, settings.footerFontSize, settings.footerFontName,
                    height - bottomMargin, Qt::AlignRight);
        if (!settings.footerCenter.isEmpty()) {
            drawAligned(settings.footerCenter, settings.footerFontSize, settings.footerFontName,
                        height - bottomMargin, Qt::AlignCenter);
        }
        }
        if (!global.watermark.trimmed().isEmpty()) {
            painter.save();
            QFont markFont(QStringLiteral("sans-serif"), 48);
            painter.setFont(markFont);
            QColor ink(160, 160, 160, 60);
            painter.setPen(ink);
            painter.translate(width / 2.0, height / 2.0);
            painter.rotate(-35.0);
            painter.drawText(QRectF(-width, -40, width * 2, 80), Qt::AlignCenter,
                             withReplacements(global.watermark, currentPage));
            painter.restore();
        }
    }
    painter.end();

    return true;
}

bool runQpdf(const QStringList& arguments, QByteArray* standardOutput, QString* error)
{
    QProcess process;
    process.start(QStringLiteral("qpdf"), arguments);
    if (!process.waitForFinished(120000) || process.exitStatus() != QProcess::NormalExit ||
        process.exitCode() != 0) {
        if (error) {
            const QString output = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
            *error = output.isEmpty() ? QStringLiteral("qpdf is required for PDF post-processing") : output;
        }
        return false;
    }
    if (standardOutput) *standardOutput = process.readAllStandardOutput();
    return true;
}

QByteArray pdfString(const QString& value)
{
    QByteArray bytes("<FEFF");
    for (const QChar character : value) {
        const ushort code = character.unicode();
        bytes += QByteArray::number(code, 16).rightJustified(4, '0').toUpper();
    }
    bytes += '>';
    return bytes;
}

struct OutlineNode {
    OutlineEntry entry;
    int objectId = 0;
    int parentId = 0;
    int previousId = 0;
    int nextId = 0;
    int firstChildId = 0;
    int lastChildId = 0;
    int descendantCount = 0;
};

int descendantCount(QList<OutlineNode>* nodes, int parentId)
{
    int count = 0;
    for (OutlineNode& node : *nodes) {
        if (node.parentId == parentId) {
            count += 1 + descendantCount(nodes, node.objectId);
        }
    }
    return count;
}

}

bool hasPageNumberPlaceholders(const ObjectSettings& settings)
{
    return containsPagePlaceholder(settings.headerLeft) ||
           containsPagePlaceholder(settings.headerCenter) ||
           containsPagePlaceholder(settings.headerRight) ||
           containsPagePlaceholder(settings.footerLeft) ||
           containsPagePlaceholder(settings.footerCenter) ||
           containsPagePlaceholder(settings.footerRight) ||
           containsPagePlaceholder(settings.headerHtml) ||
           containsPagePlaceholder(settings.footerHtml);
}

bool applyPageNumberOverlay(const QString& inputPath, const QString& outputPath,
                            const GlobalSettings& global, const ObjectSettings& settings,
                            QString* error)
{
    QFile input(inputPath);
    if (!input.open(QIODevice::ReadOnly)) {
        if (error) *error = input.errorString();
        return false;
    }
    input.close();
    const int pages = countPdfPages(inputPath);
    if (pages <= 0) {
        if (error) *error = QStringLiteral("could not determine PDF page count");
        return false;
    }

    QTemporaryFile overlay(QDir::tempPath() + QStringLiteral("/wkhtmltopdf-ng-overlay-XXXXXX.pdf"));
    overlay.setAutoRemove(false);
    if (!overlay.open()) {
        if (error) *error = overlay.errorString();
        return false;
    }
    const QString overlayPath = overlay.fileName();
    overlay.close();

    if (!writeOverlay(overlayPath, pages, global, settings, global.documentTitle, error)) {
        QFile::remove(overlayPath);
        return false;
    }

    QTemporaryFile merged(QDir::tempPath() + QStringLiteral("/wkhtmltopdf-ng-merged-XXXXXX.pdf"));
    merged.setAutoRemove(false);
    if (!merged.open()) {
        QFile::remove(overlayPath);
        if (error) *error = merged.errorString();
        return false;
    }
    const QString mergedPath = merged.fileName();
    merged.close();

    QProcess qpdf;
    qpdf.start(QStringLiteral("qpdf"), {QStringLiteral("--overlay"), overlayPath,
                                         QStringLiteral("--"), inputPath, mergedPath});
    if (!qpdf.waitForFinished(120000) || qpdf.exitStatus() != QProcess::NormalExit ||
        qpdf.exitCode() != 0) {
        if (error) {
            const QString output = QString::fromLocal8Bit(qpdf.readAllStandardError()).trimmed();
            *error = output.isEmpty() ? QStringLiteral("qpdf is required for page-number overlays") : output;
        }
        QFile::remove(overlayPath);
        QFile::remove(mergedPath);
        return false;
    }

    QFile::remove(overlayPath);
    if (outputPath != mergedPath) {
        QFile::remove(outputPath);
        if (!QFile::rename(mergedPath, outputPath)) {
            QFile source(mergedPath);
            if (!source.open(QIODevice::ReadOnly)) {
                if (error) *error = source.errorString();
                QFile::remove(mergedPath);
                return false;
            }
            const QByteArray result = source.readAll();
            QString writeError;
            const bool written = writeAllFile(outputPath, result, &writeError);
            QFile::remove(mergedPath);
            if (!written && error) *error = writeError;
            return written;
        }
    }
    return true;
}

bool embedPdfOutlines(const QString& inputPath, const QString& outputPath,
                      const QList<OutlineEntry>& entries, QString* error)
{
    if (entries.isEmpty()) {
        QFile::remove(outputPath);
        return QFile::copy(inputPath, outputPath);
    }

    QTemporaryFile normalized(QDir::tempPath() + QStringLiteral("/wkhtmltopdf-ng-qdf-XXXXXX.pdf"));
    normalized.setAutoRemove(true);
    if (!normalized.open()) {
        if (error) *error = normalized.errorString();
        return false;
    }
    const QString normalizedPath = normalized.fileName();
    normalized.close();

    if (!runQpdf({QStringLiteral("--qdf"), QStringLiteral("--object-streams=disable"),
                  inputPath, normalizedPath}, nullptr, error)) {
        return false;
    }

    QByteArray pdf = readAllFile(normalizedPath);
    QByteArray pagesJson;
    if (!runQpdf({QStringLiteral("--json"), QStringLiteral("--json-key=pages"), normalizedPath},
                 &pagesJson, error)) return false;
    const QJsonDocument json = QJsonDocument::fromJson(pagesJson);
    const QJsonArray pages = json.object().value(QStringLiteral("pages")).toArray();
    if (pages.isEmpty()) {
        if (error) *error = QStringLiteral("qpdf returned no page objects");
        return false;
    }

    QByteArray trailer;
    if (!runQpdf({QStringLiteral("--show-object=trailer"), normalizedPath}, &trailer, error)) return false;
    const QString trailerText = QString::fromLatin1(trailer);
    const QRegularExpression rootExpression(QStringLiteral("/Root\\s+(\\d+)\\s+0\\s+R"));
    const QRegularExpression sizeExpression(QStringLiteral("/Size\\s+(\\d+)"));
    const QRegularExpression startExpression(QStringLiteral("startxref\\s+(\\d+)"));
    const auto rootMatch = rootExpression.match(trailerText);
    const auto sizeMatch = sizeExpression.match(trailerText);
    const auto startMatch = startExpression.match(QString::fromLatin1(pdf));
    if (!rootMatch.hasMatch() || !sizeMatch.hasMatch() || !startMatch.hasMatch()) {
        if (error) *error = QStringLiteral("could not read qpdf trailer metadata");
        return false;
    }
    const int rootId = rootMatch.captured(1).toInt();
    const int oldSize = sizeMatch.captured(1).toInt();
    const int outlineId = oldSize;
    const int firstItemId = outlineId + 1;

    QList<OutlineNode> nodes;
    QList<int> stack;
    for (int index = 0; index < entries.size(); ++index) {
        OutlineNode node;
        node.entry = entries.at(index);
        node.entry.level = qMax(1, node.entry.level);
        node.objectId = firstItemId + index;
        while (!stack.isEmpty() && nodes.at(stack.last()).entry.level >= node.entry.level) stack.removeLast();
        node.parentId = stack.isEmpty() ? outlineId : nodes.at(stack.last()).objectId;
        int previous = 0;
        for (int reverse = nodes.size() - 1; reverse >= 0; --reverse) {
            if (nodes.at(reverse).parentId == node.parentId) {
                previous = nodes.at(reverse).objectId;
                break;
            }
        }
        node.previousId = previous;
        if (previous) nodes[previous - firstItemId].nextId = node.objectId;
        nodes.append(node);
        stack.append(nodes.size() - 1);
    }

    for (OutlineNode& node : nodes) {
        node.descendantCount = descendantCount(&nodes, node.objectId);
        for (const OutlineNode& child : nodes) {
            if (child.parentId != node.objectId) continue;
            if (!node.firstChildId) node.firstChildId = child.objectId;
            node.lastChildId = child.objectId;
        }
    }

    const QJsonArray pageArray = pages;
    QByteArray additions;
    QList<QPair<int, qint64>> offsets;
    auto appendObject = [&](int id, const QByteArray& body) {
        offsets.append({id, pdf.size() + 1 + additions.size()});
        additions += QByteArray::number(id) + " 0 obj\n" + body + "\nendobj\n";
    };

    QByteArray rootBody;
    QByteArray rootObject;
    if (!runQpdf({QStringLiteral("--show-object=%1").arg(rootId), normalizedPath}, &rootObject, error)) return false;
    rootBody = rootObject.trimmed();
    const int rootEnd = rootBody.lastIndexOf(" >>");
    const int rootEndCompact = rootBody.lastIndexOf(">>");
    const int insertAt = rootEnd >= 0 ? rootEnd : rootEndCompact;
    if (insertAt < 0) {
        if (error) *error = QStringLiteral("could not locate PDF catalog dictionary");
        return false;
    }
    rootBody.insert(insertAt, QByteArray(" /Outlines ") + QByteArray::number(outlineId) + " 0 R");
    appendObject(rootId, rootBody);

    QByteArray rootOutline = "<< /Type /Outlines /First ";
    rootOutline += QByteArray::number(firstItemId);
    rootOutline += " 0 R /Last ";
    rootOutline += QByteArray::number(firstItemId + nodes.size() - 1);
    rootOutline += " 0 R /Count ";
    rootOutline += QByteArray::number(nodes.size());
    rootOutline += " >>";
    appendObject(outlineId, rootOutline);

    for (const OutlineNode& node : nodes) {
        const int pageIndex = qBound(1, node.entry.page, pageArray.size()) - 1;
        const QString pageReference = pageArray.at(pageIndex).toObject().value(QStringLiteral("object")).toString();
        QByteArray body = "<< /Title ";
        body += pdfString(node.entry.title);
        body += " /Parent ";
        body += QByteArray::number(node.parentId) + " 0 R /Dest [" + pageReference.toLatin1() + " /Fit]";
        if (node.previousId) body += " /Prev " + QByteArray::number(node.previousId) + " 0 R";
        if (node.nextId) body += " /Next " + QByteArray::number(node.nextId) + " 0 R";
        if (node.firstChildId) {
            body += " /First " + QByteArray::number(node.firstChildId) + " 0 R /Last " +
                    QByteArray::number(node.lastChildId) + " 0 R /Count " +
                    QByteArray::number(node.descendantCount);
        }
        body += " >>";
        appendObject(node.objectId, body);
    }

    pdf += '\n';
    pdf += additions;
    const qint64 xrefOffset = pdf.size();
    pdf += "xref\n";
    pdf += QByteArray::number(rootId) + " 1\n";
    pdf += QByteArray::number(offsets.first().second).rightJustified(10, '0') + " 00000 n \n";
    pdf += QByteArray::number(outlineId) + " " + QByteArray::number(nodes.size() + 1) + "\n";
    for (int index = 1; index < offsets.size(); ++index) {
        pdf += QByteArray::number(offsets.at(index).second).rightJustified(10, '0') + " 00000 n \n";
    }
    pdf += "trailer\n<< /Size " + QByteArray::number(outlineId + nodes.size() + 1) +
           " /Root " + QByteArray::number(rootId) + " 0 R /Prev " + startMatch.captured(1).toLatin1() +
           " >>\nstartxref\n" + QByteArray::number(xrefOffset) + "\n%%EOF\n";

    QString writeError;
    const bool written = writeAllFile(outputPath, pdf, &writeError);
    if (!written && error) *error = writeError;
    return written;
}

bool pdfNeedsRecompress(const GlobalSettings& global)
{
    return global.recompressPdf || global.compressionLevel > 0 || global.optimizePdfImages;
}

bool applyPdfCopiesAndCompression(const QString& inputPath, const QString& outputPath,
                                  const GlobalSettings& global, QString* error)
{
    const int copies = qMax(1, global.copies);
    const bool recompress = global.useCompression && pdfNeedsRecompress(global);
    if (copies == 1 && global.useCompression && !recompress) {
        if (inputPath == outputPath) return true;
        QFile::remove(outputPath);
        if (QFile::copy(inputPath, outputPath)) return true;
        bool ok = false;
        const QByteArray data = readAllFile(inputPath, &ok);
        if (!ok) {
            if (error) *error = QStringLiteral("cannot read PDF for output");
            return false;
        }
        QString writeError;
        const bool written = writeAllFile(outputPath, data, &writeError);
        if (!written && error) *error = writeError;
        return written;
    }

    QStringList arguments;
    if (!global.useCompression) {
        arguments.append(QStringLiteral("--stream-data=uncompress"));
    } else if (recompress) {
        arguments << QStringLiteral("--compress-streams=y")
                  << QStringLiteral("--object-streams=generate")
                  << QStringLiteral("--recompress-flate");
        if (global.compressionLevel > 0) {
            arguments << QStringLiteral("--compression-level=") +
                QString::number(qBound(1, global.compressionLevel, 9));
        }
        if (global.optimizePdfImages) arguments << QStringLiteral("--optimize-images");
    }
    if (copies == 1) {
        arguments << inputPath << outputPath;
        return runQpdf(arguments, nullptr, error);
    }

    arguments << QStringLiteral("--empty") << QStringLiteral("--pages");
    if (global.collate) {
        for (int copy = 0; copy < copies; ++copy) {
            arguments << inputPath << QStringLiteral("1-z");
        }
    } else {
        bool ok = false;
        const QByteArray pdf = readAllFile(inputPath, &ok);
        const int pages = ok ? pageCount(pdf) : 0;
        if (pages <= 0) {
            if (error) *error = QStringLiteral("could not determine PDF page count for --copies");
            return false;
        }
        QStringList repeats;
        repeats.reserve(pages * copies);
        for (int page = 1; page <= pages; ++page) {
            for (int copy = 0; copy < copies; ++copy) repeats.append(QString::number(page));
        }
        arguments << inputPath << repeats.join(QLatin1Char(','));
    }
    arguments << QStringLiteral("--") << outputPath;
    return runQpdf(arguments, nullptr, error);
}

int countPdfPages(const QString& path)
{
    QByteArray output;
    if (runQpdf({QStringLiteral("--show-npages"), path}, &output, nullptr)) {
        bool parsed = false;
        const int pages = QString::fromLatin1(output).trimmed().toInt(&parsed);
        if (parsed && pages > 0) return pages;
    }
    bool ok = false;
    const QByteArray pdf = readAllFile(path, &ok);
    if (!ok) return 0;
    return pageCount(pdf);
}

bool mergePdfFiles(const QStringList& inputPaths, const QString& outputPath, QString* error)
{
    QStringList existing;
    for (const QString& path : inputPaths) {
        if (!path.isEmpty() && QFileInfo::exists(path)) existing.append(path);
    }
    if (existing.isEmpty()) {
        if (error) *error = QStringLiteral("no PDF objects to merge");
        return false;
    }
    if (existing.size() == 1) {
        QFile::remove(outputPath);
        if (QFile::copy(existing.first(), outputPath)) return true;
        bool ok = false;
        const QByteArray data = readAllFile(existing.first(), &ok);
        if (!ok) {
            if (error) *error = QStringLiteral("cannot read PDF object");
            return false;
        }
        QString writeError;
        const bool written = writeAllFile(outputPath, data, &writeError);
        if (!written && error) *error = writeError;
        return written;
    }

    QStringList arguments{QStringLiteral("--empty"), QStringLiteral("--pages")};
    for (const QString& path : existing) arguments << path << QStringLiteral("1-z");
    arguments << QStringLiteral("--") << outputPath;
    return runQpdf(arguments, nullptr, error);
}

bool applyPdfPageRanges(const QString& inputPath, const QString& outputPath,
                        const QString& ranges, QString* error)
{
    QPageRanges parsed;
    if (!parsePageRanges(ranges, &parsed) || parsed.isEmpty()) {
        if (error) *error = QStringLiteral("invalid page ranges %1").arg(ranges);
        return false;
    }
    return runQpdf({QStringLiteral("--empty"), QStringLiteral("--pages"), inputPath,
                    parsed.toString(), QStringLiteral("--"), outputPath}, nullptr, error);
}

bool splitPdfPages(const QString& inputPath, const QString& outputPath, QString* error)
{
    if (!QFileInfo::exists(inputPath)) {
        if (error) *error = QStringLiteral("PDF not found: %1").arg(inputPath);
        return false;
    }
    const QFileInfo info(outputPath);
    QDir().mkpath(info.path().isEmpty() ? QStringLiteral(".") : info.path());
    return runQpdf({QStringLiteral("--split-pages"), inputPath, outputPath}, nullptr, error);
}

bool insertPdfAfterPage(const QString& originalPath, const QString& insertPath,
                        int afterPage, const QString& outputPath, QString* error)
{
    if (!QFileInfo::exists(originalPath)) {
        if (error) *error = QStringLiteral("PDF not found: %1").arg(originalPath);
        return false;
    }
    if (!QFileInfo::exists(insertPath)) {
        if (error) *error = QStringLiteral("PDF not found: %1").arg(insertPath);
        return false;
    }
    const int pages = countPdfPages(originalPath);
    if (pages < 1) {
        if (error) *error = QStringLiteral("could not read page count from %1").arg(originalPath);
        return false;
    }
    if (afterPage < 0 || afterPage > pages) {
        if (error) {
            *error = QStringLiteral("after-page %1 is out of range (0-%2)")
                         .arg(afterPage).arg(pages);
        }
        return false;
    }

    QStringList arguments{QStringLiteral("--empty"), QStringLiteral("--pages")};
    if (afterPage >= 1)
        arguments << originalPath << QStringLiteral("1-%1").arg(afterPage);
    arguments << insertPath << QStringLiteral("1-z");
    if (afterPage < pages)
        arguments << originalPath << QStringLiteral("%1-z").arg(afterPage + 1);
    arguments << QStringLiteral("--") << outputPath;
    return runQpdf(arguments, nullptr, error);
}

bool runPdfEdit(const GlobalSettings& global, const QStringList& inputs,
                const QString& output, QString* error)
{
    switch (global.pdfEdit) {
    case PdfEditMode::Merge:
        if (inputs.size() < 2) {
            if (error) *error = QStringLiteral("merge needs two or more input PDFs and an output");
            return false;
        }
        return mergePdfFiles(inputs, output, error);
    case PdfEditMode::Split:
        if (inputs.size() != 1) {
            if (error) *error = QStringLiteral("split needs one input PDF and an output");
            return false;
        }
        if (!global.pageRanges.isEmpty())
            return applyPdfPageRanges(inputs.first(), output, global.pageRanges, error);
        return splitPdfPages(inputs.first(), output, error);
    case PdfEditMode::Insert:
        if (inputs.size() != 1) {
            if (error) *error = QStringLiteral("insert needs ORIGINAL.pdf OUTPUT.pdf");
            return false;
        }
        if (global.insertPdf.isEmpty()) {
            if (error) *error = QStringLiteral("--insert-pdf requires a PDF path");
            return false;
        }
        if (global.afterPage < 0) {
            if (error) *error = QStringLiteral("insert requires --after-page N or --before-page N");
            return false;
        }
        return insertPdfAfterPage(inputs.first(), global.insertPdf, global.afterPage, output, error);
    case PdfEditMode::None:
        break;
    }
    if (error) *error = QStringLiteral("not a PDF edit command");
    return false;
}

bool pdfNeedsImageOptimization(const GlobalSettings& global)
{
    return global.lowquality || global.imageDpi != 600 || global.imageQuality != 94;
}

void effectiveImageOptimization(const GlobalSettings& global, int* dpi, int* quality)
{
    int effectiveDpi = global.imageDpi > 0 ? global.imageDpi : 600;
    int effectiveQuality = qBound(0, global.imageQuality, 100);
    if (global.lowquality) {
        if (global.imageDpi == 600) effectiveDpi = 150;
        if (global.imageQuality == 94) effectiveQuality = 40;
    }
    if (dpi) *dpi = effectiveDpi;
    if (quality) *quality = effectiveQuality;
}

bool pdfNeedsDocumentExtras(const GlobalSettings& global)
{
    return !global.userPassword.isEmpty() || !global.ownerPassword.isEmpty() ||
        global.linearize || !global.author.isEmpty() || !global.subject.isEmpty() ||
        !global.keywords.isEmpty();
}

void remapOutlinePages(QList<OutlineEntry>* entries, int actualPages)
{
    if (!entries || entries->isEmpty() || actualPages <= 0) return;
    int estimated = 1;
    for (const OutlineEntry& entry : *entries)
        estimated = qMax(estimated, entry.page);
    if (estimated == actualPages) return;
    for (OutlineEntry& entry : *entries) {
        const int mapped = qMax(1, int(qRound(double(entry.page) * actualPages / double(estimated))));
        entry.page = qMin(actualPages, mapped);
    }
}

bool applyPdfDocumentExtras(const QString& inputPath, const QString& outputPath,
                            const GlobalSettings& global, QString* error)
{
    QStringList arguments;
    if (global.linearize) arguments << QStringLiteral("--linearize");
    if (!global.userPassword.isEmpty() || !global.ownerPassword.isEmpty()) {
        const QString user = global.userPassword;
        const QString owner = global.ownerPassword.isEmpty() ? user : global.ownerPassword;
        arguments << QStringLiteral("--encrypt") << user << owner << QStringLiteral("256")
                  << QStringLiteral("--");
    }
    if (arguments.isEmpty()) {
        if (inputPath == outputPath) return true;
        QFile::remove(outputPath);
        if (QFile::copy(inputPath, outputPath)) return true;
        bool ok = false;
        const QByteArray data = readAllFile(inputPath, &ok);
        return ok && writeAllFile(outputPath, data, error);
    }
    arguments << inputPath << outputPath;
    return runQpdf(arguments, nullptr, error);
}
