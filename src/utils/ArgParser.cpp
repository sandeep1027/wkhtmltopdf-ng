#include "ArgParser.h"

#include "converter/PageLayout.h"

#include <QFileInfo>
#include <QUrl>

namespace {

bool needsValue(const QString& option)
{
    return option == QStringLiteral("--page-size") || option == QStringLiteral("--orientation") ||
           option.startsWith(QStringLiteral("--margin-")) || option == QStringLiteral("--dpi") ||
           option == QStringLiteral("--image-dpi") || option == QStringLiteral("--image-quality") ||
           option == QStringLiteral("--page-width") || option == QStringLiteral("--page-height") ||
           option == QStringLiteral("--paperwidth") || option == QStringLiteral("--paperheight") ||
           option == QStringLiteral("--title") || option == QStringLiteral("--header-html") ||
           option == QStringLiteral("--footer-html") || option.startsWith(QStringLiteral("--header-")) ||
           option.startsWith(QStringLiteral("--footer-")) || option == QStringLiteral("--javascript-delay") ||
           option == QStringLiteral("--window-status") || option == QStringLiteral("--proxy") ||
           option == QStringLiteral("--proxy-auth") || option == QStringLiteral("--cookie-jar") ||
           option == QStringLiteral("--username") || option == QStringLiteral("--password") ||
           option == QStringLiteral("--custom-header") || option == QStringLiteral("--run-script") ||
           option == QStringLiteral("--user-style-sheet") || option == QStringLiteral("--encoding") ||
           option == QStringLiteral("--zoom") || option == QStringLiteral("--viewport-size") ||
           option == QStringLiteral("--load-error-handling") || option == QStringLiteral("--load-media-error-handling") ||
           option == QStringLiteral("--outline-depth") || option == QStringLiteral("--dump-outline") ||
           option == QStringLiteral("--toc-header-text") || option == QStringLiteral("--toc-caption-text") ||
           option == QStringLiteral("--toc-level-indentation") || option == QStringLiteral("--toc-text-size-shrink") ||
            option == QStringLiteral("--toc-xsl") || option == QStringLiteral("--xsl-style-sheet") ||
            option == QStringLiteral("--page-offset") || option == QStringLiteral("--page-ranges") ||
            option == QStringLiteral("--copies") ||
            option == QStringLiteral("--minimum-font-size") ||
            option == QStringLiteral("--post") || option == QStringLiteral("--post-file") ||
            option == QStringLiteral("--cookie") || option == QStringLiteral("--allow") ||
            option == QStringLiteral("--bypass-proxy-for") || option == QStringLiteral("--cache-dir") ||
            option == QStringLiteral("--log-level") ||
            option == QStringLiteral("--compress-level") ||
            option == QStringLiteral("--author") || option == QStringLiteral("--subject") ||
            option == QStringLiteral("--keywords") ||
            option == QStringLiteral("--user-password") || option == QStringLiteral("--owner-password") ||
            option == QStringLiteral("--watermark") ||
            option == QStringLiteral("--header-on") || option == QStringLiteral("--footer-on") ||
            option == QStringLiteral("--retry") || option == QStringLiteral("--timeout") ||
            option == QStringLiteral("--ssl-crt-path") || option == QStringLiteral("--ssl-key-path") ||
            option == QStringLiteral("--ssl-key-password") || option == QStringLiteral("--ca-certificate") ||
            option == QStringLiteral("--replace") ||
            option == QStringLiteral("--insert-pdf") ||
            option == QStringLiteral("--after-page") ||
            option == QStringLiteral("--before-page");
}

bool isGlobalOnly(const QString& option)
{
    return option == QStringLiteral("--page-size") || option == QStringLiteral("--orientation") ||
           option.startsWith(QStringLiteral("--margin-")) || option == QStringLiteral("--dpi") ||
           option == QStringLiteral("--image-dpi") || option == QStringLiteral("--image-quality") ||
           option == QStringLiteral("--page-width") || option == QStringLiteral("--page-height") ||
           option == QStringLiteral("--paperwidth") || option == QStringLiteral("--paperheight") ||
           option == QStringLiteral("--title") || option == QStringLiteral("--copies") ||
           option == QStringLiteral("--collate") || option == QStringLiteral("--no-collate") ||
           option == QStringLiteral("--grayscale") || option == QStringLiteral("--lowquality") ||
           option == QStringLiteral("--quiet") || option == QStringLiteral("--outline") ||
           option == QStringLiteral("--no-outline") || option == QStringLiteral("--outline-depth") ||
           option == QStringLiteral("--dump-outline") || option == QStringLiteral("--page-offset") ||
           option == QStringLiteral("--page-ranges") ||
           option == QStringLiteral("--no-pdf-compression") ||
           option == QStringLiteral("--compress") || option == QStringLiteral("--pdf-compression") ||
           option == QStringLiteral("--compress-level") || option == QStringLiteral("--optimize-images") ||
           option == QStringLiteral("--use-xserver") ||
           option == QStringLiteral("--read-args-from-stdin") ||
           option == QStringLiteral("--print-media-type") ||
           option == QStringLiteral("--no-print-media-type") ||
           option == QStringLiteral("--cache-dir") || option == QStringLiteral("--log-level") ||
           option == QStringLiteral("--dump-default-toc-xsl") ||
           option == QStringLiteral("--author") || option == QStringLiteral("--subject") ||
           option == QStringLiteral("--keywords") ||
           option == QStringLiteral("--user-password") || option == QStringLiteral("--owner-password") ||
           option == QStringLiteral("--linearize") || option == QStringLiteral("--watermark") ||
           option == QStringLiteral("--no-header-on-first") || option == QStringLiteral("--skip-first-header") ||
           option == QStringLiteral("--header-on") || option == QStringLiteral("--footer-on") ||
           option == QStringLiteral("--retry") || option == QStringLiteral("--timeout") ||
           option == QStringLiteral("--ssl-crt-path") || option == QStringLiteral("--ssl-key-path") ||
           option == QStringLiteral("--ssl-key-password") || option == QStringLiteral("--ca-certificate") ||
           option == QStringLiteral("--merge-pdf") || option == QStringLiteral("--split-pdf") ||
           option == QStringLiteral("--split-pages") || option == QStringLiteral("--insert-pdf") ||
           option == QStringLiteral("--after-page") || option == QStringLiteral("--before-page");
}

bool takeValue(const QStringList& args, int* index, QString* value, QString* error)
{
    if (*index + 1 >= args.size()) {
        *error = QStringLiteral("missing value for %1").arg(args.at(*index));
        return false;
    }
    ++(*index);
    *value = args.at(*index);
    return true;
}

bool setMargin(GlobalSettings* global, const QString& option, const QString& value)
{
    const QString side = option.mid(QStringLiteral("--margin-").size());
    return global->set((QStringLiteral("margin.") + side).toUtf8().constData(), value);
}

bool setHeaderFooter(ObjectSettings* object, const QString& option, const QString& value)
{
    const QString name = option.mid(2);
    const int separator = name.indexOf(QLatin1Char('-'));
    if (separator < 0) return false;
    const QString section = name.left(separator);
    const QString field = name.mid(separator + 1);
    if (section != QStringLiteral("header") && section != QStringLiteral("footer")) return false;
    const QString key = section + QLatin1Char('.') +
        (field == QStringLiteral("html") ? QStringLiteral("htmlUrl") : field);
    return object->set(key.toUtf8().constData(), value);
}

void applyCoverDefaults(ObjectSettings* object)
{
    object->kind = ObjectKind::Cover;
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
}

bool applyFlag(const QString& argument, GlobalSettings* global, ObjectSettings* object)
{
    if (argument == QStringLiteral("--quiet")) { global->quiet = true; return true; }
    if (argument == QStringLiteral("--no-outline")) { global->outlineEnabled = false; return true; }
    if (argument == QStringLiteral("--outline")) { global->outlineEnabled = true; return true; }
    if (argument == QStringLiteral("--grayscale")) { global->grayscale = true; return true; }
    if (argument == QStringLiteral("--lowquality") || argument == QStringLiteral("-l")) {
        global->lowquality = true;
        return true;
    }
    if (argument == QStringLiteral("--print-media-type")) { global->printMediaType = true; return true; }
    if (argument == QStringLiteral("--no-print-media-type")) { global->printMediaType = false; return true; }
    if (argument == QStringLiteral("--no-images")) { object->loadImages = false; return true; }
    if (argument == QStringLiteral("--images")) { object->loadImages = true; return true; }
    if (argument == QStringLiteral("--enable-plugins")) { object->enablePlugins = true; return true; }
    if (argument == QStringLiteral("--disable-plugins")) { object->enablePlugins = false; return true; }
    if (argument == QStringLiteral("--disable-forms")) { object->disableForms = true; return true; }
    if (argument == QStringLiteral("--enable-forms")) { object->disableForms = false; return true; }
    if (argument == QStringLiteral("--disable-external-links")) { object->disableExternalLinks = true; return true; }
    if (argument == QStringLiteral("--enable-external-links")) { object->disableExternalLinks = false; return true; }
    if (argument == QStringLiteral("--disable-internal-links")) { object->disableInternalLinks = true; return true; }
    if (argument == QStringLiteral("--enable-internal-links")) { object->disableInternalLinks = false; return true; }
    if (argument == QStringLiteral("--background")) { object->printBackground = true; return true; }
    if (argument == QStringLiteral("--no-background")) { object->printBackground = false; return true; }
    if (argument == QStringLiteral("--default-header")) {
        object->headerLeft = QStringLiteral("[webpage]");
        object->headerRight = QStringLiteral("[page]/[topage]");
        object->headerLine = true;
        global->set("margin.top", QStringLiteral("20mm"));
        return true;
    }
    if (argument == QStringLiteral("--keep-relative-links")) {
        object->keepRelativeLinks = true;
        object->resolveRelativeLinks = false;
        return true;
    }
    if (argument == QStringLiteral("--resolve-relative-links")) {
        object->resolveRelativeLinks = true;
        object->keepRelativeLinks = false;
        return true;
    }
    if (argument == QStringLiteral("--disable-javascript")) { object->enableJavascript = false; return true; }
    if (argument == QStringLiteral("--enable-javascript")) { object->enableJavascript = true; return true; }
    if (argument == QStringLiteral("--enable-local-file-access")) { object->enableLocalFileAccess = true; return true; }
    if (argument == QStringLiteral("--disable-local-file-access")) { object->enableLocalFileAccess = false; return true; }
    if (argument == QStringLiteral("--toc")) { object->toc = true; return true; }
    if (argument == QStringLiteral("--toc-use-dotted-lines")) { object->tocUseDottedLines = true; return true; }
    if (argument == QStringLiteral("--no-toc-use-dotted-lines")) { object->tocUseDottedLines = false; return true; }
    if (argument == QStringLiteral("--toc-forward-links")) { object->tocForwardLinks = true; return true; }
    if (argument == QStringLiteral("--no-toc-forward-links")) { object->tocForwardLinks = false; return true; }
    if (argument == QStringLiteral("--disable-toc-links")) { object->tocForwardLinks = false; return true; }
    if (argument == QStringLiteral("--toc-back-links")) { object->tocBackLinks = true; return true; }
    if (argument == QStringLiteral("--no-toc-back-links")) { object->tocBackLinks = false; return true; }
    if (argument == QStringLiteral("--disable-dotted-lines")) { object->tocUseDottedLines = false; return true; }
    if (argument == QStringLiteral("--no-collate")) { global->collate = false; return true; }
    if (argument == QStringLiteral("--collate")) { global->collate = true; return true; }
    if (argument == QStringLiteral("--read-args-from-stdin")) { global->readArgsFromStdin = true; return true; }
    if (argument == QStringLiteral("--disable-smart-shrinking")) { object->enableSmartShrinking = false; return true; }
    if (argument == QStringLiteral("--enable-smart-shrinking")) { object->enableSmartShrinking = true; return true; }
    if (argument == QStringLiteral("--use-xserver")) { global->useXServer = QStringLiteral("1"); return true; }
    if (argument == QStringLiteral("--no-pdf-compression")) {
        global->useCompression = false;
        global->recompressPdf = false;
        return true;
    }
    if (argument == QStringLiteral("--compress") || argument == QStringLiteral("--pdf-compression")) {
        global->useCompression = true;
        global->recompressPdf = true;
        return true;
    }
    if (argument == QStringLiteral("--optimize-images")) {
        global->optimizePdfImages = true;
        global->useCompression = true;
        global->recompressPdf = true;
        return true;
    }
    if (argument == QStringLiteral("--header-line")) { object->headerLine = true; return true; }
    if (argument == QStringLiteral("--footer-line")) { object->footerLine = true; return true; }
    if (argument == QStringLiteral("--no-header-line")) { object->headerLine = false; return true; }
    if (argument == QStringLiteral("--no-footer-line")) { object->footerLine = false; return true; }
    if (argument == QStringLiteral("--exclude-from-outline")) { object->includeInOutline = false; return true; }
    if (argument == QStringLiteral("--include-in-outline")) { object->includeInOutline = true; return true; }
    if (argument == QStringLiteral("--custom-header-propagation")) {
        object->customHeaderPropagation = true;
        return true;
    }
    if (argument == QStringLiteral("--no-custom-header-propagation")) {
        object->customHeaderPropagation = false;
        return true;
    }
    if (argument == QStringLiteral("--debug-javascript")) { object->debugJavascript = true; return true; }
    if (argument == QStringLiteral("--no-debug-javascript")) { object->debugJavascript = false; return true; }
    if (argument == QStringLiteral("--dump-default-toc-xsl")) { global->dumpDefaultTocXsl = true; return true; }
    if (argument == QStringLiteral("--stop-slow-scripts")) { object->stopSlowScripts = true; return true; }
    if (argument == QStringLiteral("--no-stop-slow-scripts")) { object->stopSlowScripts = false; return true; }
    if (argument == QStringLiteral("--linearize")) { global->linearize = true; return true; }
    if (argument == QStringLiteral("--no-header-on-first") ||
        argument == QStringLiteral("--skip-first-header")) {
        global->skipHeaderOnFirst = true;
        return true;
    }
    if (argument == QStringLiteral("--merge-pdf")) {
        global->pdfEdit = PdfEditMode::Merge;
        return true;
    }
    if (argument == QStringLiteral("--split-pdf")) {
        global->pdfEdit = PdfEditMode::Split;
        return true;
    }
    if (argument == QStringLiteral("--split-pages")) {
        global->splitPages = true;
        if (global->pdfEdit == PdfEditMode::None) global->pdfEdit = PdfEditMode::Split;
        return true;
    }
    if (argument == QStringLiteral("--produce-forms")) { object->produceForms = true; return true; }
    if (argument == QStringLiteral("--no-produce-forms")) { object->produceForms = false; return true; }
    return false;
}

bool applyValue(const QString& argument, const QString& value, GlobalSettings* global,
                ObjectSettings* object, bool* ok)
{
    *ok = true;
    if (argument == QStringLiteral("--page-size")) *ok = global->set("size.pageSize", value);
    else if (argument == QStringLiteral("--orientation")) *ok = global->set("orientation", value);
    else if (argument.startsWith(QStringLiteral("--margin-"))) *ok = setMargin(global, argument, value);
    else if (argument == QStringLiteral("--dpi")) *ok = global->set("dpi", value);
    else if (argument == QStringLiteral("--image-dpi")) *ok = global->set("imageDPI", value);
    else if (argument == QStringLiteral("--image-quality")) *ok = global->set("imageQuality", value);
    else if (argument == QStringLiteral("--page-width") || argument == QStringLiteral("--paperwidth"))
        *ok = global->set("size.width", value);
    else if (argument == QStringLiteral("--page-height") || argument == QStringLiteral("--paperheight"))
        *ok = global->set("size.height", value);
    else if (argument == QStringLiteral("--title")) global->documentTitle = value;
    else if (argument == QStringLiteral("--header-spacing")) { object->headerSpacing = value.toDouble(ok); }
    else if (argument == QStringLiteral("--footer-spacing")) { object->footerSpacing = value.toDouble(ok); }
    else if (argument == QStringLiteral("--header-font-size")) { object->headerFontSize = value.toInt(ok); }
    else if (argument == QStringLiteral("--footer-font-size")) { object->footerFontSize = value.toInt(ok); }
    else if (argument == QStringLiteral("--header-font-name")) object->headerFontName = value;
    else if (argument == QStringLiteral("--footer-font-name")) object->footerFontName = value;
    else if (argument == QStringLiteral("--header-on")) *ok = global->set("headerOn", value);
    else if (argument == QStringLiteral("--footer-on")) *ok = global->set("footerOn", value);
    else if (argument.startsWith(QStringLiteral("--header-")) || argument.startsWith(QStringLiteral("--footer-")))
        *ok = setHeaderFooter(object, argument, value);
    else if (argument == QStringLiteral("--javascript-delay"))
        *ok = object->set("javascriptDelay", value);
    else if (argument == QStringLiteral("--window-status")) object->windowStatus = value;
    else if (argument == QStringLiteral("--proxy")) object->proxy = QUrl::fromUserInput(value);
    else if (argument == QStringLiteral("--proxy-auth")) object->proxyAuth = value;
    else if (argument == QStringLiteral("--username")) object->username = value;
    else if (argument == QStringLiteral("--password")) object->password = value;
    else if (argument == QStringLiteral("--cookie-jar")) object->cookieJar = value;
    else if (argument == QStringLiteral("--custom-header")) {
        const int separator = value.indexOf(QLatin1Char(':'));
        if (separator <= 0) *ok = false;
        else object->customHeaders.insert(value.left(separator).trimmed(), value.mid(separator + 1).trimmed());
    } else if (argument == QStringLiteral("--run-script")) {
        object->runScript = value;
        object->runScripts.append(value);
    }
    else if (argument == QStringLiteral("--user-style-sheet")) object->userStyleSheet = value;
    else if (argument == QStringLiteral("--encoding")) object->encoding = value;
    else if (argument == QStringLiteral("--zoom")) object->zoom = value.toDouble(ok);
    else if (argument == QStringLiteral("--viewport-size"))
        *ok = object->set("viewportSize", value);
    else if (argument == QStringLiteral("--load-error-handling"))
        *ok = object->set("loadErrorHandling", value);
    else if (argument == QStringLiteral("--minimum-font-size"))
        *ok = object->set("web.minimumFontSize", value);
    else if (argument == QStringLiteral("--load-media-error-handling")) object->loadMediaErrorHandling = value;
    else if (argument == QStringLiteral("--outline-depth")) global->outlineDepth = value.toInt(ok);
    else if (argument == QStringLiteral("--page-offset")) global->pageOffset = value.toInt(ok);
    else if (argument == QStringLiteral("--page-ranges")) *ok = global->set("pageRanges", value);
    else if (argument == QStringLiteral("--dump-outline")) global->outline = value;
    else if (argument == QStringLiteral("--toc-header-text")) object->tocHeaderText = value;
    else if (argument == QStringLiteral("--toc-caption-text")) object->tocCaptionText = value;
    else if (argument == QStringLiteral("--toc-level-indentation")) object->tocLevelIndentation = value;
    else if (argument == QStringLiteral("--toc-text-size-shrink")) object->tocTextSizeShrink = value.toDouble(ok);
    else if (argument == QStringLiteral("--toc-xsl") || argument == QStringLiteral("--xsl-style-sheet"))
        object->tocXsl = value;
    else if (argument == QStringLiteral("--copies")) *ok = global->set("copies", value);
    else if (argument == QStringLiteral("--compress-level")) *ok = global->set("compressionLevel", value);
    else if (argument == QStringLiteral("--author")) global->author = value;
    else if (argument == QStringLiteral("--subject")) global->subject = value;
    else if (argument == QStringLiteral("--keywords")) global->keywords = value;
    else if (argument == QStringLiteral("--user-password")) global->userPassword = value;
    else if (argument == QStringLiteral("--owner-password")) global->ownerPassword = value;
    else if (argument == QStringLiteral("--watermark")) global->watermark = value;
    else if (argument == QStringLiteral("--header-on")) *ok = global->set("headerOn", value);
    else if (argument == QStringLiteral("--footer-on")) *ok = global->set("footerOn", value);
    else if (argument == QStringLiteral("--retry")) *ok = global->set("retry", value);
    else if (argument == QStringLiteral("--timeout")) *ok = global->set("timeout", value);
    else if (argument == QStringLiteral("--ssl-crt-path")) global->sslCrtPath = value;
    else if (argument == QStringLiteral("--ssl-key-path")) global->sslKeyPath = value;
    else if (argument == QStringLiteral("--ssl-key-password")) global->sslKeyPassword = value;
    else if (argument == QStringLiteral("--ca-certificate")) global->caCertificate = value;
    else if (argument == QStringLiteral("--insert-pdf")) {
        global->insertPdf = value;
        global->pdfEdit = PdfEditMode::Insert;
    } else if (argument == QStringLiteral("--after-page")) {
        bool parsed = false;
        global->afterPage = value.toInt(&parsed);
        *ok = parsed && global->afterPage >= 0;
    } else if (argument == QStringLiteral("--before-page")) {
        bool parsed = false;
        const int page = value.toInt(&parsed);
        *ok = parsed && page >= 1;
        if (*ok) global->afterPage = page - 1;
    }
    else if (argument == QStringLiteral("--post")) object->postData = value;
    else if (argument == QStringLiteral("--post-file")) object->postFile = value;
    else if (argument == QStringLiteral("--allow")) {
        if (!value.isEmpty()) object->allowedPaths.append(value);
    } else if (argument == QStringLiteral("--bypass-proxy-for")) {
        if (!value.isEmpty()) object->bypassProxyFor.append(value);
    } else if (argument == QStringLiteral("--cache-dir")) {
        *ok = global->set("cacheDir", value);
    } else if (argument == QStringLiteral("--log-level")) {
        *ok = global->set("logLevel", value);
    } else return false;
    return true;
}

bool isKeyword(const QString& argument)
{
    return argument == QStringLiteral("cover") || argument == QStringLiteral("toc") ||
           argument == QStringLiteral("page");
}

}

ParsedArguments ArgParser::parse(const QStringList& arguments)
{
    ParsedArguments result;
    bool endOptions = false;

    int outputIndex = -1;
    bool expectBoundInput = false;
    for (int i = 0; i < arguments.size(); ++i) {
        const QString argument = arguments.at(i);
        if (!endOptions && argument == QStringLiteral("--")) {
            endOptions = true;
            continue;
        }
        if (!endOptions && argument != QStringLiteral("-") && argument.startsWith(QLatin1Char('-'))) {
            if (argument == QStringLiteral("--replace") || argument == QStringLiteral("--cookie") ||
                argument == QStringLiteral("--post") || argument == QStringLiteral("--post-file")) {
                i += 2;
                expectBoundInput = false;
                continue;
            }
            if (argument == QStringLiteral("--custom-header")) {
                if (i + 1 < arguments.size() && arguments.at(i + 1).contains(QLatin1Char(':')) &&
                    !arguments.at(i + 1).startsWith(QLatin1Char('-'))) {
                    ++i;
                } else {
                    i += 2;
                }
                expectBoundInput = false;
                continue;
            }
            if (needsValue(argument)) {
                ++i;
                expectBoundInput = false;
            }
            continue;
        }
        if (!endOptions && isKeyword(argument)) {
            expectBoundInput = argument != QStringLiteral("toc");
            continue;
        }
        if (expectBoundInput) {
            expectBoundInput = false;
            continue;
        }
        outputIndex = i;
    }

    endOptions = false;
    ObjectSettings defaults;
    ObjectSettings* current = nullptr;
    enum class Expect { None, Cover, Page } expect = Expect::None;

    auto startObject = [&](ObjectKind kind, const QString& page) {
        ObjectSettings object = defaults;
        object.kind = kind;
        object.page = page;
        object.localOptions = false;
        if (kind != ObjectKind::Page) object.toc = false;
        if (kind == ObjectKind::Cover) applyCoverDefaults(&object);
        if (kind == ObjectKind::Toc) object.kind = ObjectKind::Toc;
        result.objects.append(object);
        current = &result.objects.last();
        expect = Expect::None;
    };

    for (int i = 0; i < arguments.size(); ++i) {
        if (i == outputIndex) continue;
        const QString argument = arguments.at(i);
        if (!endOptions && argument == QStringLiteral("--")) {
            endOptions = true;
            continue;
        }
        if (!endOptions && argument != QStringLiteral("-") && argument.startsWith(QLatin1Char('-'))) {
            if (argument == QStringLiteral("--help") || argument == QStringLiteral("-h")) {
                result.help = true;
                continue;
            }
            if (argument == QStringLiteral("--version") || argument == QStringLiteral("-V")) {
                result.version = true;
                continue;
            }
            if (current && isGlobalOnly(argument)) {
                result.error = QStringLiteral("global option %1 must appear before document objects")
                    .arg(argument);
                return result;
            }
            ObjectSettings* target = current ? current : &defaults;
            if (current) current->localOptions = true;

            if (argument == QStringLiteral("--replace")) {
                QString name;
                if (!takeValue(arguments, &i, &name, &result.error)) return result;
                QString replacement;
                if (!takeValue(arguments, &i, &replacement, &result.error)) return result;
                target->replacements.insert(name, replacement);
                continue;
            }
            if (argument == QStringLiteral("--cookie")) {
                QString name;
                if (!takeValue(arguments, &i, &name, &result.error)) return result;
                QString value;
                if (!takeValue(arguments, &i, &value, &result.error)) return result;
                target->extraCookies.append({name, value});
                continue;
            }
            if (argument == QStringLiteral("--post")) {
                QString name;
                if (!takeValue(arguments, &i, &name, &result.error)) return result;
                QString value;
                if (!takeValue(arguments, &i, &value, &result.error)) return result;
                target->postFields.append({name, value});
                continue;
            }
            if (argument == QStringLiteral("--post-file")) {
                QString name;
                if (!takeValue(arguments, &i, &name, &result.error)) return result;
                QString path;
                if (!takeValue(arguments, &i, &path, &result.error)) return result;
                target->postFiles.append({name, path});
                continue;
            }
            if (argument == QStringLiteral("--custom-header")) {
                QString first;
                if (!takeValue(arguments, &i, &first, &result.error)) return result;
                if (first.contains(QLatin1Char(':'))) {
                    const int separator = first.indexOf(QLatin1Char(':'));
                    target->customHeaders.insert(first.left(separator).trimmed(),
                                                 first.mid(separator + 1).trimmed());
                } else {
                    QString headerValue;
                    if (!takeValue(arguments, &i, &headerValue, &result.error)) return result;
                    target->customHeaders.insert(first.trimmed(), headerValue);
                }
                continue;
            }
            if (applyFlag(argument, &result.global, target)) continue;
            if (!needsValue(argument)) {
                result.error = QStringLiteral("unknown option: %1").arg(argument);
                return result;
            }
            QString value;
            if (!takeValue(arguments, &i, &value, &result.error)) return result;
            bool ok = true;
            if (!applyValue(argument, value, &result.global, target, &ok)) {
                result.error = QStringLiteral("unknown option: %1").arg(argument);
                return result;
            }
            if (!ok) {
                result.error = QStringLiteral("invalid value for %1: %2").arg(argument, value);
                return result;
            }
            continue;
        }

        if (!endOptions && argument == QStringLiteral("cover")) {
            expect = Expect::Cover;
            current = nullptr;
            continue;
        }
        if (!endOptions && argument == QStringLiteral("page")) {
            expect = Expect::Page;
            current = nullptr;
            continue;
        }
        if (!endOptions && argument == QStringLiteral("toc")) {
            startObject(ObjectKind::Toc, QString());
            current->localOptions = true;
            continue;
        }

        if (expect == Expect::Cover) {
            startObject(ObjectKind::Cover, argument);
            continue;
        }
        if (expect == Expect::Page) {
            startObject(ObjectKind::Page, argument);
            continue;
        }
        startObject(ObjectKind::Page, argument);
    }

    if (expect != Expect::None) {
        result.error = expect == Expect::Cover
            ? QStringLiteral("missing input after cover")
            : QStringLiteral("missing input after page");
        return result;
    }

    result.object = defaults;
    if (outputIndex >= 0) {
        result.output = arguments.at(outputIndex);
        result.global.out = result.output;
    }
    for (const ObjectSettings& object : result.objects) {
        if (object.kind == ObjectKind::Toc) continue;
        if (result.input.isEmpty()) result.input = object.page;
        result.inputs.append(object.page);
    }
    if (!result.objects.isEmpty()) result.object.page = result.input;

    if (!result.help && !result.version && !result.global.dumpDefaultTocXsl &&
        !result.global.readArgsFromStdin) {
        if (result.output.isEmpty()) {
            result.error = QStringLiteral("expected one or more INPUT arguments followed by OUTPUT");
            return result;
        }
        if (result.global.pdfEdit != PdfEditMode::None) {
            if (result.inputs.isEmpty()) {
                result.error = QStringLiteral("expected one or more INPUT PDFs followed by OUTPUT");
                return result;
            }
            return result;
        }
        bool hasRenderable = false;
        for (const ObjectSettings& object : result.objects) {
            if (object.kind != ObjectKind::Toc || object.toc) hasRenderable = true;
            if (object.kind == ObjectKind::Page || object.kind == ObjectKind::Cover) hasRenderable = true;
        }
        if (result.objects.isEmpty() || !hasRenderable) {
            result.error = QStringLiteral("expected one or more INPUT arguments followed by OUTPUT");
            return result;
        }
    }
    return result;
}

QString ArgParser::usage()
{
    return QStringLiteral(
        "Usage: wkhtmltopdf-ng [GLOBAL OPTIONS] [OBJECT]... <output pdf/->\n\n"
        "Modern Qt WebEngine HTML to PDF converter.\n\n"
        "Document objects:\n"
        "  [page] <url|file|-> [PAGE OPTIONS]\n"
        "  cover <url|file|-> [PAGE OPTIONS]\n"
        "  toc [TOC OPTIONS]\n"
        "  Options before the first object are defaults. Page options after an\n"
        "  object apply only to that object. cover has no headers/footers and is\n"
        "  omitted from the outline.\n\n"
        "Page options:\n"
        "  --page-size SIZE              A4, Letter, Legal, ...\n"
        "  --orientation ORIENTATION     Portrait or Landscape\n"
        "  --margin-top/--margin-right/--margin-bottom/--margin-left LENGTH\n"
        "  --dpi DPI                     Layout DPI (default 96; scales like zoom 96/DPI)\n"
        "  --title TITLE                 PDF document title\n"
        "  --enable-smart-shrinking      Shrink wide content to the page width (default)\n"
        "  --disable-smart-shrinking     Do not shrink wide content to the page width\n\n"
        "Rendering options:\n"
        "  --javascript-delay MS         Wait after page load (default 200)\n"
        "  --viewport-size WxH           Emulate a browser viewport in pixels\n"
        "  --minimum-font-size SIZE      Smallest font size in pixels\n"
        "  --background                  Print CSS backgrounds (default)\n"
        "  --no-background               Do not print CSS backgrounds\n"
        "  --load-error-handling MODE    abort, ignore, or skip (default abort)\n"
        "  --page-ranges RANGES          Print only these pages (e.g. 1-3,5)\n"
        "  --image-dpi DPI               Cap in-page image DPI before print (default 600)\n"
        "  --image-quality QUALITY       JPEG quality when resampling images (default 94)\n"
        "  --lowquality, -l              Smaller PDF: 150 DPI images at quality 40\n"
        "  --copies N                    Repeat the document N times (default 1)\n"
        "  --compress                    Recompress with qpdf (object streams + flate)\n"
        "  --compress-level N            Flate level 1-9 (implies --compress)\n"
        "  --optimize-images             Recompress images in the PDF (implies --compress)\n"
        "  --no-pdf-compression          Leave PDF streams uncompressed\n"
        "  --author TEXT                 PDF Author metadata\n"
        "  --subject TEXT                PDF Subject metadata\n"
        "  --keywords TEXT               PDF Keywords metadata\n"
        "  --user-password PASS          Encrypt; required to open\n"
        "  --owner-password PASS         Encrypt; owner/permissions password\n"
        "  --linearize                   Fast web view (linearized PDF)\n"
        "  --watermark TEXT              Diagonal watermark on every page\n"
        "  --no-header-on-first          Do not draw header/footer on page 1\n"
        "  --header-on all|odd|even      Which pages get the header\n"
        "  --footer-on all|odd|even      Which pages get the footer\n"
        "  --retry N                     Retry failed loads N times\n"
        "  --timeout MS                  Abort convert after MS milliseconds\n"
        "  --ssl-crt-path FILE           Client TLS certificate (PEM)\n"
        "  --ssl-key-path FILE           Client TLS private key (PEM)\n"
        "  --ca-certificate FILE         Extra CA certificate (PEM)\n"
        "  --window-status VALUE         Wait until window.status matches\n"
        "  --run-script SCRIPT           Run JavaScript after load\n"
        "  --disable-javascript          Disable JavaScript\n"
        "  --stop-slow-scripts           Cap long script/font waits (default)\n"
        "  --no-stop-slow-scripts        Wait the full javascript-delay / font ready\n"
        "  --keep-relative-links         Leave local hrefs relative in the PDF\n"
        "  --encoding ENC                Decode local HTML as ENC (default utf-8)\n"
        "  --print-media-type            Use @media print (default)\n"
        "  --no-print-media-type         Prefer screen styles; drop print stylesheets\n"
        "  --enable-local-file-access    Allow file URLs to access local files\n"
        "  --custom-header NAME:VALUE    Add an HTTP request header\n"
        "  --proxy URL                  Use an HTTP/HTTPS proxy\n"
        "  --proxy-auth USER:PASSWORD  Authenticate with the proxy\n"
        "  --username USER             HTTP authentication username\n"
        "  --password PASSWORD         HTTP authentication password\n"
        "  --replace NAME VALUE        Replace a header/footer token\n"
        "  --no-images                 Disable image loading\n"
        "  --disable-forms             Disable form controls\n"
        "  --disable-external-links    Remove hyperlinks\n"
        "  --disable-internal-links    Remove local hyperlinks\n"
        "  --default-header            Add a default header\n"
        "  --header-center TEXT         Fixed header text\n"
        "  --footer-center TEXT         Fixed footer text\n"
        "  --exclude-from-outline       Do not include this object in the outline\n"
        "  --include-in-outline         Include this object in the outline (default)\n\n"
        "  --toc                         Insert a heading TOC before the first page\n"
        "  toc                           Insert a TOC object at this position\n"
        "  --toc-header-text TEXT       TOC empty-state heading\n"
        "  --toc-caption-text TEXT      TOC title\n"
        "  --toc-level-indentation LEN  Heading indentation unit\n"
        "  --no-toc-use-dotted-lines    Disable TOC dotted leaders\n\n"
        "  --dump-outline FILE          Export heading outline XML\n"
        "  --dump-default-toc-xsl       Print the default TOC XSLT and exit\n"
        "  --toc-xsl FILE               Build the TOC with an XSLT stylesheet\n"
        "  --cookie NAME VALUE          Set an extra cookie\n"
        "  --allow PATH                 Allow local files under PATH\n"
        "  --cache-dir PATH             WebEngine cache directory\n"
        "  --log-level LEVEL            none, error, warn, or info\n"
        "  --debug-javascript           Print page console messages\n"
        "  --bypass-proxy-for HOST      Do not use the proxy for HOST\n"
        "  --custom-header-propagation  Send custom headers on every request (default)\n\n"
        "PDF split / merge / insert (qpdf; no HTML render):\n"
        "  --merge-pdf A.pdf B.pdf [C.pdf ...] out.pdf\n"
        "                                Concatenate PDFs in order\n"
        "  --split-pdf in.pdf --page-ranges 1-2 part.pdf\n"
        "                                Extract those pages to one PDF\n"
        "  --split-pdf --split-pages in.pdf out.pdf\n"
        "                                Write out-1.pdf, out-2.pdf, ...\n"
        "  --insert-pdf extra.pdf --after-page 3 original.pdf out.pdf\n"
        "                                Pages 1-3 of original, then extra, then the rest\n"
        "  --insert-pdf extra.pdf --before-page 4 original.pdf out.pdf\n"
        "                                Same as --after-page 3\n"
        "  --after-page 0                Insert extra before the first page\n\n"
        "Other options:\n"
        "  --quiet                       Suppress informational output\n"
        "  --version                     Print version\n"
        "  --help                        Print this help\n\n"
        "Note: Qt WebEngine does not provide wkhtmltopdf's native PDF outline and\n"
        "HTML header/footer APIs. Page numbers use a qpdf overlay. Default top/bottom\n"
        "margins grow when a header or footer is set so body text is not covered.\n");
}
