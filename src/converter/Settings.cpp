#include "Settings.h"

#include "PageLayout.h"

#include <QByteArray>
#include <QPageRanges>
#include <QStringList>

namespace {

bool parseBool(const QString& value, bool* result)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("1") || normalized == QStringLiteral("true") ||
        normalized == QStringLiteral("yes")) {
        *result = true;
        return true;
    }
    if (normalized == QStringLiteral("0") || normalized == QStringLiteral("false") ||
        normalized == QStringLiteral("no")) {
        *result = false;
        return true;
    }
    return false;
}

QString boolString(bool value)
{
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

QString lengthString(double value)
{
    return QString::number(value, 'g', 12) + QStringLiteral("mm");
}

bool updateMargin(QPageLayout::Unit unit, const QString& value, QPageLayout* layout,
                  int side)
{
    double millimeters = 0.0;
    if (!parseLengthMm(value, &millimeters)) {
        return false;
    }
    QMarginsF margins = layout->margins(QPageLayout::Millimeter);
    switch (side) {
    case 0: margins.setTop(millimeters); break;
    case 1: margins.setRight(millimeters); break;
    case 2: margins.setBottom(millimeters); break;
    case 3: margins.setLeft(millimeters); break;
    default: return false;
    }
    Q_UNUSED(unit);
    layout->setMargins(margins);
    return true;
}

}

QString GlobalSettings::get(const char* name) const
{
    const QString key = QString::fromUtf8(name ? name : "");
    if (key == QStringLiteral("out")) return out;
    if (key == QStringLiteral("documentTitle")) return documentTitle;
    if (key == QStringLiteral("size.width")) return QString::number(pageWidth) + QStringLiteral("mm");
    if (key == QStringLiteral("size.height")) return QString::number(pageHeight) + QStringLiteral("mm");
    if (key == QStringLiteral("dpi")) return QString::number(dpi);
    if (key == QStringLiteral("imageDPI")) return QString::number(imageDpi);
    if (key == QStringLiteral("imageQuality")) return QString::number(imageQuality);
    if (key == QStringLiteral("grayscale")) return boolString(grayscale);
    if (key == QStringLiteral("lowquality")) return boolString(lowquality);
    if (key == QStringLiteral("quiet")) return boolString(quiet);
    if (key == QStringLiteral("logLevel")) return logLevel;
    if (key == QStringLiteral("cacheDir") || key == QStringLiteral("load.cacheDir")) return cacheDir;
    if (key == QStringLiteral("outline")) return boolString(outlineEnabled);
    if (key == QStringLiteral("outlineDepth")) return QString::number(outlineDepth);
    if (key == QStringLiteral("pageOffset")) return QString::number(pageOffset);
    if (key == QStringLiteral("copies")) return QString::number(copies);
    if (key == QStringLiteral("collate")) return boolString(collate);
    if (key == QStringLiteral("useCompression")) return boolString(useCompression);
    if (key == QStringLiteral("recompressPdf") || key == QStringLiteral("compress"))
        return boolString(recompressPdf);
    if (key == QStringLiteral("compressionLevel")) return QString::number(compressionLevel);
    if (key == QStringLiteral("optimizePdfImages")) return boolString(optimizePdfImages);
    if (key == QStringLiteral("readArgsFromStdin")) return boolString(readArgsFromStdin);
    if (key == QStringLiteral("pageRanges")) return pageRanges;
    if (key == QStringLiteral("author")) return author;
    if (key == QStringLiteral("subject")) return subject;
    if (key == QStringLiteral("keywords")) return keywords;
    if (key == QStringLiteral("userPassword")) return userPassword;
    if (key == QStringLiteral("ownerPassword")) return ownerPassword;
    if (key == QStringLiteral("linearize")) return boolString(linearize);
    if (key == QStringLiteral("watermark")) return watermark;
    if (key == QStringLiteral("skipHeaderOnFirst")) return boolString(skipHeaderOnFirst);
    if (key == QStringLiteral("headerOn")) return headerOn;
    if (key == QStringLiteral("footerOn")) return footerOn;
    if (key == QStringLiteral("retry")) return QString::number(retry);
    if (key == QStringLiteral("timeoutMs") || key == QStringLiteral("timeout"))
        return QString::number(timeoutMs);
    if (key == QStringLiteral("sslCrtPath")) return sslCrtPath;
    if (key == QStringLiteral("sslKeyPath")) return sslKeyPath;
    if (key == QStringLiteral("caCertificate")) return caCertificate;
    if (key == QStringLiteral("margin.top")) return lengthString(pageLayout.margins(QPageLayout::Millimeter).top());
    if (key == QStringLiteral("margin.right")) return lengthString(pageLayout.margins(QPageLayout::Millimeter).right());
    if (key == QStringLiteral("margin.bottom")) return lengthString(pageLayout.margins(QPageLayout::Millimeter).bottom());
    if (key == QStringLiteral("margin.left")) return lengthString(pageLayout.margins(QPageLayout::Millimeter).left());
    if (key == QStringLiteral("orientation")) {
        return pageLayout.orientation() == QPageLayout::Landscape
            ? QStringLiteral("Landscape") : QStringLiteral("Portrait");
    }
    if (key == QStringLiteral("size.pageSize")) return pageLayout.pageSize().name();
    return {};
}

bool GlobalSettings::set(const char* name, const QString& value)
{
    const QString key = QString::fromUtf8(name ? name : "");
    bool ok = false;
    if (key == QStringLiteral("out")) { out = value; return true; }
    if (key == QStringLiteral("documentTitle")) { documentTitle = value; return true; }
    if (key == QStringLiteral("size.width") || key == QStringLiteral("size.height")) {
        double millimeters = 0.0;
        if (!parseLengthMm(value, &millimeters)) return false;
        if (key == QStringLiteral("size.width")) pageWidth = millimeters;
        else pageHeight = millimeters;
        const QSizeF current = pageLayout.pageSize().size(QPageSize::Millimeter);
        const double width = pageWidth > 0 ? pageWidth : current.width();
        const double height = pageHeight > 0 ? pageHeight : current.height();
        pageLayout.setPageSize(QPageSize(QSizeF(width, height), QPageSize::Millimeter,
                                         QStringLiteral("Custom")));
        return true;
    }
    if (key == QStringLiteral("dpi")) {
        const int v = value.toInt(&ok);
        if (ok && v > 0) dpi = v;
        return ok && v > 0;
    }
    if (key == QStringLiteral("imageDPI")) {
        const int v = value.toInt(&ok);
        if (ok && v > 0) imageDpi = v;
        return ok && v > 0;
    }
    if (key == QStringLiteral("imageQuality")) {
        const int v = value.toInt(&ok);
        if (ok && v >= 0 && v <= 100) imageQuality = v;
        return ok && v >= 0 && v <= 100;
    }
    if (key == QStringLiteral("grayscale")) return parseBool(value, &grayscale);
    if (key == QStringLiteral("lowquality")) return parseBool(value, &lowquality);
    if (key == QStringLiteral("quiet")) return parseBool(value, &quiet);
    if (key == QStringLiteral("logLevel")) {
        const QString normalized = value.trimmed().toLower();
        if (normalized != QStringLiteral("none") && normalized != QStringLiteral("error") &&
            normalized != QStringLiteral("warn") && normalized != QStringLiteral("info")) {
            return false;
        }
        logLevel = normalized;
        if (normalized == QStringLiteral("none")) quiet = true;
        return true;
    }
    if (key == QStringLiteral("cacheDir") || key == QStringLiteral("load.cacheDir")) {
        cacheDir = value;
        return true;
    }
    if (key == QStringLiteral("outline")) return parseBool(value, &outlineEnabled);
    if (key == QStringLiteral("outlineDepth")) { const int v = value.toInt(&ok); if (ok) outlineDepth = v; return ok; }
    if (key == QStringLiteral("pageOffset")) { const int v = value.toInt(&ok); if (ok) pageOffset = v; return ok; }
    if (key == QStringLiteral("copies")) {
        const int v = value.toInt(&ok);
        if (ok && v >= 1) copies = v;
        return ok && v >= 1;
    }
    if (key == QStringLiteral("collate")) return parseBool(value, &collate);
    if (key == QStringLiteral("useCompression")) {
        if (!parseBool(value, &useCompression)) return false;
        if (!useCompression) recompressPdf = false;
        return true;
    }
    if (key == QStringLiteral("recompressPdf") || key == QStringLiteral("compress")) {
        if (!parseBool(value, &recompressPdf)) return false;
        if (recompressPdf) useCompression = true;
        return true;
    }
    if (key == QStringLiteral("compressionLevel")) {
        const int v = value.toInt(&ok);
        if (!ok || v < 0 || v > 9) return false;
        compressionLevel = v;
        if (v > 0) {
            useCompression = true;
            recompressPdf = true;
        }
        return true;
    }
    if (key == QStringLiteral("optimizePdfImages")) {
        if (!parseBool(value, &optimizePdfImages)) return false;
        if (optimizePdfImages) {
            useCompression = true;
            recompressPdf = true;
        }
        return true;
    }
    if (key == QStringLiteral("readArgsFromStdin")) return parseBool(value, &readArgsFromStdin);
    if (key == QStringLiteral("pageRanges")) {
        QPageRanges ranges;
        if (!parsePageRanges(value, &ranges)) return false;
        pageRanges = value.trimmed();
        return true;
    }
    if (key == QStringLiteral("author")) { author = value; return true; }
    if (key == QStringLiteral("subject")) { subject = value; return true; }
    if (key == QStringLiteral("keywords")) { keywords = value; return true; }
    if (key == QStringLiteral("userPassword")) { userPassword = value; return true; }
    if (key == QStringLiteral("ownerPassword")) { ownerPassword = value; return true; }
    if (key == QStringLiteral("linearize")) return parseBool(value, &linearize);
    if (key == QStringLiteral("watermark")) { watermark = value; return true; }
    if (key == QStringLiteral("skipHeaderOnFirst")) return parseBool(value, &skipHeaderOnFirst);
    if (key == QStringLiteral("headerOn")) {
        const QString n = value.trimmed().toLower();
        if (n != QStringLiteral("all") && n != QStringLiteral("odd") && n != QStringLiteral("even"))
            return false;
        headerOn = n;
        return true;
    }
    if (key == QStringLiteral("footerOn")) {
        const QString n = value.trimmed().toLower();
        if (n != QStringLiteral("all") && n != QStringLiteral("odd") && n != QStringLiteral("even"))
            return false;
        footerOn = n;
        return true;
    }
    if (key == QStringLiteral("retry")) {
        const int v = value.toInt(&ok);
        if (!ok || v < 0) return false;
        retry = v;
        return true;
    }
    if (key == QStringLiteral("timeoutMs") || key == QStringLiteral("timeout")) {
        const int v = value.toInt(&ok);
        if (!ok || v < 0) return false;
        timeoutMs = v;
        return true;
    }
    if (key == QStringLiteral("sslCrtPath")) { sslCrtPath = value; return true; }
    if (key == QStringLiteral("sslKeyPath")) { sslKeyPath = value; return true; }
    if (key == QStringLiteral("sslKeyPassword")) { sslKeyPassword = value; return true; }
    if (key == QStringLiteral("caCertificate")) { caCertificate = value; return true; }
    if (key == QStringLiteral("orientation")) {
        const QString normalized = value.trimmed().toLower();
        if (normalized == QStringLiteral("landscape")) pageLayout.setOrientation(QPageLayout::Landscape);
        else if (normalized == QStringLiteral("portrait")) pageLayout.setOrientation(QPageLayout::Portrait);
        else return false;
        return true;
    }
    if (key == QStringLiteral("size.pageSize")) {
        QPageSize size;
        if (!parsePageSize(value, &size)) return false;
        pageLayout.setPageSize(size);
        return true;
    }
    if (key == QStringLiteral("margin.top")) return updateMargin(QPageLayout::Millimeter, value, &pageLayout, 0);
    if (key == QStringLiteral("margin.right")) return updateMargin(QPageLayout::Millimeter, value, &pageLayout, 1);
    if (key == QStringLiteral("margin.bottom")) return updateMargin(QPageLayout::Millimeter, value, &pageLayout, 2);
    if (key == QStringLiteral("margin.left")) return updateMargin(QPageLayout::Millimeter, value, &pageLayout, 3);
    return false;
}

QString ObjectSettings::get(const char* name) const
{
    const QString key = QString::fromUtf8(name ? name : "");
    if (key == QStringLiteral("page")) return page;
    if (key == QStringLiteral("header.htmlUrl")) return headerHtml;
    if (key == QStringLiteral("footer.htmlUrl")) return footerHtml;
    if (key == QStringLiteral("header.left")) return headerLeft;
    if (key == QStringLiteral("header.center")) return headerCenter;
    if (key == QStringLiteral("header.right")) return headerRight;
    if (key == QStringLiteral("footer.left")) return footerLeft;
    if (key == QStringLiteral("footer.center")) return footerCenter;
    if (key == QStringLiteral("footer.right")) return footerRight;
    if (key == QStringLiteral("header.fontSize")) return QString::number(headerFontSize);
    if (key == QStringLiteral("footer.fontSize")) return QString::number(footerFontSize);
    if (key == QStringLiteral("header.spacing")) return QString::number(headerSpacing);
    if (key == QStringLiteral("footer.spacing")) return QString::number(footerSpacing);
    if (key == QStringLiteral("javascriptDelay") || key == QStringLiteral("load.jsdelay"))
        return QString::number(javascriptDelay);
    if (key == QStringLiteral("loadErrorHandling") || key == QStringLiteral("load.loadErrorHandling"))
        return loadErrorHandling;
    if (key == QStringLiteral("web.background")) return boolString(printBackground);
    if (key == QStringLiteral("web.debugJavascript")) return boolString(debugJavascript);
    if (key == QStringLiteral("load.customHeadersPropagation")) return boolString(customHeaderPropagation);
    if (key == QStringLiteral("web.minimumFontSize") || key == QStringLiteral("minimumFontSize"))
        return QString::number(minimumFontSize);
    if (key == QStringLiteral("enableJavascript")) return boolString(enableJavascript);
    if (key == QStringLiteral("enableLocalFileAccess")) return boolString(enableLocalFileAccess);
    if (key == QStringLiteral("cookieJar")) return cookieJar;
    if (key == QStringLiteral("includeInOutline")) return boolString(includeInOutline);
    if (key == QStringLiteral("pagesCount")) return boolString(pagesCount);
    if (key == QStringLiteral("stopSlowScripts")) return boolString(stopSlowScripts);
    if (key == QStringLiteral("keepRelativeLinks")) return boolString(keepRelativeLinks);
    if (key == QStringLiteral("web.defaultEncoding") || key == QStringLiteral("encoding"))
        return encoding;
    if (key == QStringLiteral("loadMediaErrorHandling")) return loadMediaErrorHandling;
    if (key == QStringLiteral("zoom")) return QString::number(zoom);
    if (key == QStringLiteral("toc")) return boolString(toc);
    if (key == QStringLiteral("toc.headerText")) return tocHeaderText;
    if (key == QStringLiteral("toc.captionText")) return tocCaptionText;
    if (key == QStringLiteral("toc.levelIndentation")) return tocLevelIndentation;
    if (key == QStringLiteral("toc.textSizeShrink")) return QString::number(tocTextSizeShrink);
    if (key == QStringLiteral("toc.useDottedLines")) return boolString(tocUseDottedLines);
    if (key == QStringLiteral("postData")) return postData;
    if (key == QStringLiteral("postFile")) return postFile;
    if (key == QStringLiteral("viewportSize")) return viewportSize;
    if (key == QStringLiteral("enableSmartShrinking")) return boolString(enableSmartShrinking);
    if (key == QStringLiteral("useLocalLinks")) return boolString(useLocalLinks);
    if (key == QStringLiteral("useExternalLinks")) return boolString(useExternalLinks);
    return {};
}

bool ObjectSettings::set(const char* name, const QString& value)
{
    const QString key = QString::fromUtf8(name ? name : "");
    bool ok = false;
    if (key == QStringLiteral("page")) { page = value; return true; }
    if (key == QStringLiteral("header.htmlUrl")) { headerHtml = value; return true; }
    if (key == QStringLiteral("footer.htmlUrl")) { footerHtml = value; return true; }
    if (key == QStringLiteral("header.left")) { headerLeft = value; return true; }
    if (key == QStringLiteral("header.center")) { headerCenter = value; return true; }
    if (key == QStringLiteral("header.right")) { headerRight = value; return true; }
    if (key == QStringLiteral("footer.left")) { footerLeft = value; return true; }
    if (key == QStringLiteral("footer.center")) { footerCenter = value; return true; }
    if (key == QStringLiteral("footer.right")) { footerRight = value; return true; }
    if (key == QStringLiteral("header.fontSize")) { const int v = value.toInt(&ok); if (ok) headerFontSize = v; return ok; }
    if (key == QStringLiteral("footer.fontSize")) { const int v = value.toInt(&ok); if (ok) footerFontSize = v; return ok; }
    if (key == QStringLiteral("header.spacing")) { const double v = value.toDouble(&ok); if (ok) headerSpacing = v; return ok; }
    if (key == QStringLiteral("footer.spacing")) { const double v = value.toDouble(&ok); if (ok) footerSpacing = v; return ok; }
    if (key == QStringLiteral("javascriptDelay") || key == QStringLiteral("load.jsdelay")) {
        const int v = value.toInt(&ok);
        if (ok && v >= 0) javascriptDelay = v;
        return ok && v >= 0;
    }
    if (key == QStringLiteral("loadErrorHandling") || key == QStringLiteral("load.loadErrorHandling")) {
        const QString normalized = value.trimmed().toLower();
        if (normalized != QStringLiteral("abort") && normalized != QStringLiteral("ignore") &&
            normalized != QStringLiteral("skip")) {
            return false;
        }
        loadErrorHandling = normalized;
        return true;
    }
    if (key == QStringLiteral("web.background")) return parseBool(value, &printBackground);
    if (key == QStringLiteral("web.debugJavascript")) return parseBool(value, &debugJavascript);
    if (key == QStringLiteral("load.customHeadersPropagation"))
        return parseBool(value, &customHeaderPropagation);
    if (key == QStringLiteral("load.allow")) {
        if (!value.isEmpty()) allowedPaths.append(value);
        return true;
    }
    if (key == QStringLiteral("load.cookie")) {
        const int separator = value.indexOf(QLatin1Char('='));
        if (separator <= 0) return false;
        extraCookies.append({value.left(separator), value.mid(separator + 1)});
        return true;
    }
    if (key == QStringLiteral("web.minimumFontSize") || key == QStringLiteral("minimumFontSize")) {
        const int v = value.toInt(&ok);
        if (ok && v >= 0) minimumFontSize = v;
        return ok && v >= 0;
    }
    if (key == QStringLiteral("enableJavascript")) return parseBool(value, &enableJavascript);
    if (key == QStringLiteral("enableLocalFileAccess")) return parseBool(value, &enableLocalFileAccess);
    if (key == QStringLiteral("cookieJar")) { cookieJar = value; return true; }
    if (key == QStringLiteral("includeInOutline")) return parseBool(value, &includeInOutline);
    if (key == QStringLiteral("pagesCount")) return parseBool(value, &pagesCount);
    if (key == QStringLiteral("stopSlowScripts")) return parseBool(value, &stopSlowScripts);
    if (key == QStringLiteral("keepRelativeLinks")) {
        if (!parseBool(value, &keepRelativeLinks)) return false;
        resolveRelativeLinks = !keepRelativeLinks;
        return true;
    }
    if (key == QStringLiteral("web.defaultEncoding") || key == QStringLiteral("encoding")) {
        encoding = value;
        return true;
    }
    if (key == QStringLiteral("loadMediaErrorHandling")) {
        const QString normalized = value.trimmed().toLower();
        if (normalized != QStringLiteral("abort") && normalized != QStringLiteral("ignore") &&
            normalized != QStringLiteral("skip")) {
            return false;
        }
        loadMediaErrorHandling = normalized;
        return true;
    }
    if (key == QStringLiteral("zoom")) { const double v = value.toDouble(&ok); if (ok) zoom = v; return ok; }
    if (key == QStringLiteral("windowStatus")) { windowStatus = value; return true; }
    if (key == QStringLiteral("runScript")) { runScript = value; return true; }
    if (key == QStringLiteral("userStyleSheet")) { userStyleSheet = value; return true; }
    if (key == QStringLiteral("toc")) return parseBool(value, &toc);
    if (key == QStringLiteral("toc.headerText")) { tocHeaderText = value; return true; }
    if (key == QStringLiteral("toc.captionText")) { tocCaptionText = value; return true; }
    if (key == QStringLiteral("toc.levelIndentation")) { tocLevelIndentation = value; return true; }
    if (key == QStringLiteral("toc.textSizeShrink")) { const double v = value.toDouble(&ok); if (ok) tocTextSizeShrink = v; return ok; }
    if (key == QStringLiteral("toc.useDottedLines")) return parseBool(value, &tocUseDottedLines);
    if (key == QStringLiteral("postData")) { postData = value; return true; }
    if (key == QStringLiteral("postFile")) { postFile = value; return true; }
    if (key == QStringLiteral("viewportSize")) {
        const QStringList parts = value.split(QLatin1Char('x'));
        if (parts.size() != 2) return false;
        bool widthOk = false;
        bool heightOk = false;
        const int width = parts.at(0).toInt(&widthOk);
        const int height = parts.at(1).toInt(&heightOk);
        if (!widthOk || !heightOk || width <= 0 || height <= 0) return false;
        viewportSize = value;
        return true;
    }
    if (key == QStringLiteral("enableSmartShrinking")) return parseBool(value, &enableSmartShrinking);
    if (key == QStringLiteral("useLocalLinks")) return parseBool(value, &useLocalLinks);
    if (key == QStringLiteral("useExternalLinks")) return parseBool(value, &useExternalLinks);
    return false;
}

QString ImageSettings::get(const char* name) const
{
    const QString key = QString::fromUtf8(name ? name : "");
    if (key == QStringLiteral("in")) return in;
    if (key == QStringLiteral("out")) return out;
    if (key == QStringLiteral("fmt")) return fmt;
    if (key == QStringLiteral("screenWidth")) return QString::number(screenWidth);
    if (key == QStringLiteral("screenHeight")) return QString::number(screenHeight);
    if (key == QStringLiteral("quality")) return QString::number(quality);
    if (key == QStringLiteral("zoom")) return QString::number(zoom);
    if (key == QStringLiteral("smartWidth")) return boolString(smartWidth);
    if (key == QStringLiteral("crop.left")) return QString::number(cropLeft);
    if (key == QStringLiteral("crop.top")) return QString::number(cropTop);
    if (key == QStringLiteral("crop.width")) return QString::number(cropWidth);
    if (key == QStringLiteral("crop.height")) return QString::number(cropHeight);
    if (key == QStringLiteral("transparent")) return boolString(transparent);
    if (key == QStringLiteral("enableJavascript")) return boolString(enableJavascript);
    if (key == QStringLiteral("javascriptDelay")) return QString::number(javascriptDelay);
    if (key == QStringLiteral("enableLocalFileAccess")) return boolString(enableLocalFileAccess);
    if (key == QStringLiteral("dpi")) return QString::number(dpi);
    return {};
}

bool ImageSettings::set(const char* name, const QString& value)
{
    const QString key = QString::fromUtf8(name ? name : "");
    bool ok = false;
    if (key == QStringLiteral("in")) { in = value; return true; }
    if (key == QStringLiteral("out")) { out = value; return true; }
    if (key == QStringLiteral("fmt")) { fmt = value; return true; }
    if (key == QStringLiteral("screenWidth")) { const int v = value.toInt(&ok); if (ok) screenWidth = v; return ok; }
    if (key == QStringLiteral("screenHeight")) { const int v = value.toInt(&ok); if (ok) screenHeight = v; return ok; }
    if (key == QStringLiteral("quality")) { const int v = value.toInt(&ok); if (ok) quality = v; return ok; }
    if (key == QStringLiteral("zoom")) { const double v = value.toDouble(&ok); if (ok) zoom = v; return ok; }
    if (key == QStringLiteral("smartWidth")) return parseBool(value, &smartWidth);
    if (key == QStringLiteral("crop.left")) { const int v = value.toInt(&ok); if (ok) cropLeft = v; return ok; }
    if (key == QStringLiteral("crop.top")) { const int v = value.toInt(&ok); if (ok) cropTop = v; return ok; }
    if (key == QStringLiteral("crop.width")) { const int v = value.toInt(&ok); if (ok) cropWidth = v; return ok; }
    if (key == QStringLiteral("crop.height")) { const int v = value.toInt(&ok); if (ok) cropHeight = v; return ok; }
    if (key == QStringLiteral("transparent")) return parseBool(value, &transparent);
    if (key == QStringLiteral("enableJavascript")) return parseBool(value, &enableJavascript);
    if (key == QStringLiteral("javascriptDelay")) { const int v = value.toInt(&ok); if (ok) javascriptDelay = v; return ok; }
    if (key == QStringLiteral("enableLocalFileAccess")) return parseBool(value, &enableLocalFileAccess);
    if (key == QStringLiteral("dpi")) {
        const int v = value.toInt(&ok);
        if (!ok || v <= 0) return false;
        dpi = v;
        return true;
    }
    return false;
}
