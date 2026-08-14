#ifndef SETTINGS_H
#define SETTINGS_H

#include <QString>
#include <QStringList>
#include <QMap>
#include <QList>
#include <QPair>
#include <QPageLayout>
#include <QPageSize>
#include <QUrl>
#include <QMarginsF>
#include <QVariant>
#include <QSizeF>

enum class ObjectKind {
    Page,
    Cover,
    Toc
};

class GlobalSettings {
public:
    QString out;                    // --output
    QString documentTitle;          // --title
    QPageLayout pageLayout{QPageSize(QPageSize::A4), QPageLayout::Portrait,
                            QMarginsF(10, 10, 10, 10), QPageLayout::Millimeter};
    double pageWidth = -1.0;        // --page-width
    double pageHeight = -1.0;       // --page-height
    int dpi = 96;                   // --dpi
    int imageDpi = 600;             // --image-dpi
    int imageQuality = 94;          // --image-quality
    bool grayscale = false;         // --grayscale
    bool lowquality = false;        // --lowquality
    bool printMediaType = true;     // --print-media-type
    QString logLevel = "info";      // --log-level
    bool quiet = false;             // --quiet
    bool useCompression = true;     // --no-pdf-compression
    bool recompressPdf = false;     // --compress (extra qpdf shrink pass)
    int compressionLevel = 0;       // --compress-level 1-9 (0 = qpdf default)
    bool optimizePdfImages = false; // --optimize-images
    QString outline;                // --dump-outline
    bool outlineEnabled = true;     // --outline/--no-outline
    int outlineDepth = 4;           // --outline-depth
    int pageOffset = 0;             // --page-offset
    int copies = 1;                 // --copies
    bool collate = true;            // --collate/--no-collate
    QString useXServer;             // --use-xserver
    bool readArgsFromStdin = false; // --read-args-from-stdin
    QString cacheDir;               // --cache-dir
    bool dumpDefaultTocXsl = false; // --dump-default-toc-xsl
    QString pageRanges;             // --page-ranges (Qt 6.8 printToPdf)
    QString author;                 // --author
    QString subject;                // --subject
    QString keywords;               // --keywords
    QString userPassword;           // --user-password
    QString ownerPassword;          // --owner-password
    bool linearize = false;         // --linearize
    QString watermark;              // --watermark TEXT
    bool skipHeaderOnFirst = false; // --no-header-on-first / --skip-first-header
    QString headerOn = "all";       // --header-on all|odd|even
    QString footerOn = "all";       // --footer-on all|odd|even
    int retry = 0;                  // --retry N
    int timeoutMs = 0;              // --timeout MS (0 = default)
    QString sslCrtPath;             // --ssl-crt-path
    QString sslKeyPath;             // --ssl-key-path
    QString sslKeyPassword;         // --ssl-key-password
    QString caCertificate;          // --ca-certificate

    // For C API reflection
    QString get(const char* name) const;
    bool set(const char* name, const QString& value);
};

class ObjectSettings {
public:
    ObjectKind kind = ObjectKind::Page;
    bool localOptions = false;      // page options applied after this object started
    QString page;                   // URL or HTML file
    QString html;                   // Raw HTML (--html)
    QString userStyleSheet;         // --user-style-sheet
    QString headerHtml;             // --header-html
    QString footerHtml;             // --footer-html
    QString headerLeft, headerCenter, headerRight;  // --header-*
    QString footerLeft, footerCenter, footerRight;  // --footer-*
    int headerFontSize = 12;        // --header-font-size
    int footerFontSize = 12;        // --footer-font-size
    QString headerFontName = "Arial";  // --header-font-name
    QString footerFontName = "Arial";  // --footer-font-name
    double headerSpacing = 0;       // --header-spacing
    double footerSpacing = 0;       // --footer-spacing
    bool headerLine = false;        // --header-line
    bool footerLine = false;        // --footer-line
    bool disableSmartShrinking = false;  // --disable-smart-shrinking
    bool enableLocalFileAccess = false;  // --enable-local-file-access
    bool produceForms = false;      // --produce-forms
    bool includeInOutline = true;   // --include-in-outline
    bool pagesCount = true;         // --pages-count
    int javascriptDelay = 200;      // --javascript-delay
    QString windowStatus;           // --window-status
    QUrl proxy;                     // --proxy
    QString proxyAuth;              // --proxy-auth
    QString username;               // --username
    QString password;               // --password
    QString cookieJar;              // --cookie-jar
    QList<QPair<QString, QString>> extraCookies; // --cookie NAME VALUE
    QStringList allowedPaths;       // --allow
    QStringList bypassProxyFor;     // --bypass-proxy-for
    bool customHeaderPropagation = true; // --custom-header-propagation
    bool debugJavascript = false;   // --debug-javascript
    QMap<QString, QString> customHeaders;  // --custom-header
    QString runScript;              // --run-script
    QStringList runScripts;         // repeatable --run-script
    QString loadErrorHandling = "abort";  // --load-error-handling
    QString loadMediaErrorHandling = "ignore";  // --load-media-error-handling
    bool enableJavascript = true;   // --enable-javascript/--disable-javascript
    bool enablePlugins = false;     // --enable-plugins
    bool disableForms = false;      // --disable-forms
    bool stopSlowScripts = true;    // --stop-slow-scripts
    bool loadImages = true;         // --images/--no-images
    bool disableExternalLinks = false; // --disable-external-links
    bool disableInternalLinks = false; // --disable-internal-links
    bool printBackground = true;    // --background/--no-background
    bool keepRelativeLinks = false; // --keep-relative-links
    bool resolveRelativeLinks = true; // --resolve-relative-links
    QString encoding = "utf-8";     // --encoding
    double zoom = 1.0;              // --zoom
    QString viewportSize;           // --viewport-size
    int minimumFontSize = 0;        // --minimum-font-size (0 = engine default)
    QString background = "";        // --background
    QMap<QString, QString> replacements; // --replace NAME VALUE
    QString postData;               // --post
    QString postFile;               // --post-file
    QList<QPair<QString, QString>> postFields; // --post NAME VALUE
    QList<QPair<QString, QString>> postFiles;  // --post-file NAME PATH
    bool enableSmartShrinking = true; // --enable-smart-shrinking
    bool useLocalLinks = true;      // --enable-internal-links/--disable-internal-links
    bool useExternalLinks = true;   // --enable-external-links/--disable-external-links

    // TOC settings
    bool toc = false;               // --toc
    QString tocHeaderText = "Table of Contents";  // --toc-header-text
    QString tocLevelIndentation = "1em";  // --toc-level-indentation
    double tocTextSizeShrink = 0.8;  // --toc-text-size-shrink
    bool tocUseDottedLines = true;  // --toc-use-dotted-lines
    QString tocCaptionText = "Table of Contents";  // --toc-caption-text
    bool tocForwardLinks = true;    // --toc-forward-links
    bool tocBackLinks = false;      // --toc-back-links
    QString tocXsl;                 // --toc-xsl

    // For C API reflection
    QString get(const char* name) const;
    bool set(const char* name, const QString& value);
};

class ImageSettings {
public:
    QString in;                     // input URL or file path
    QString out;                    // output file
    QString fmt = "png";            // --format: png, jpg, webp
    int screenWidth = 1024;         // --width
    int screenHeight = 0;           // --height (0 = auto to content)
    int quality = 90;               // --quality
    double zoom = 1.0;              // --zoom
    bool smartWidth = true;         // --smart-width
    int cropLeft = 0;               // --crop-x
    int cropTop = 0;                // --crop-y
    int cropWidth = 0;              // --crop-width (0 = not set)
    int cropHeight = 0;             // --crop-height (0 = not set)
    bool transparent = false;       // --transparent
    bool enableJavascript = true;   // --enable-javascript
    int javascriptDelay = 0;        // --javascript-delay
    bool enableLocalFileAccess = false; // --enable-local-file-access
    int dpi = 96;                   // --dpi (image output scale)

    // For C API reflection
    QString get(const char* name) const;
    bool set(const char* name, const QString& value);
};

#endif // SETTINGS_H
