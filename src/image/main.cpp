#include "converter/HtmlToImageConverter.h"
#include "converter/Settings.h"
#include "utils/HeadlessQt.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QTextStream>

int main(int argc, char* argv[])
{
    prepareHeadlessQt(argc, argv);
    QStringList cliArgs;
    cliArgs.append(QString::fromLocal8Bit(argv[0] ? argv[0] : "wkhtmltoimage-ng"));
    for (int i = 1; i < argc; ++i)
        cliArgs.append(QString::fromLocal8Bit(argv[i]));
    int qtArgc = 1;
    char* qtArgv[] = { argv[0], nullptr };
    QApplication application(qtArgc, qtArgv);
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Qt WebEngine HTML to image converter"));
    parser.addHelpOption();
    parser.addOption({QStringLiteral("width"), QStringLiteral("Viewport width in pixels"), QStringLiteral("pixels"), QStringLiteral("1024")});
    parser.addOption({QStringLiteral("height"), QStringLiteral("Viewport height in pixels"), QStringLiteral("pixels"), QStringLiteral("0")});
    parser.addOption({QStringLiteral("javascript-delay"), QStringLiteral("Wait after page load"), QStringLiteral("milliseconds"), QStringLiteral("200")});
    parser.addOption({QStringLiteral("quality"), QStringLiteral("Image quality from 0 to 100"), QStringLiteral("quality"), QStringLiteral("90")});
    parser.addOption({QStringLiteral("format"), QStringLiteral("png, jpg, or webp"), QStringLiteral("fmt")});
    parser.addOption({QStringLiteral("crop-x"), QStringLiteral("Crop origin X"), QStringLiteral("pixels"), QStringLiteral("0")});
    parser.addOption({QStringLiteral("crop-y"), QStringLiteral("Crop origin Y"), QStringLiteral("pixels"), QStringLiteral("0")});
    parser.addOption({QStringLiteral("crop-width"), QStringLiteral("Crop width"), QStringLiteral("pixels")});
    parser.addOption({QStringLiteral("crop-height"), QStringLiteral("Crop height"), QStringLiteral("pixels")});
    parser.addOption({QStringLiteral("transparent"), QStringLiteral("Use a transparent page background")});
    parser.addOption({QStringLiteral("enable-local-file-access"), QStringLiteral("Allow local file resources")});
    parser.addOption({QStringLiteral("disable-javascript"), QStringLiteral("Disable JavaScript")});
    parser.addOption({QStringLiteral("disable-smart-width"), QStringLiteral("Do not expand width to content")});
    parser.addOption({QStringLiteral("zoom"), QStringLiteral("Zoom factor"), QStringLiteral("factor"), QStringLiteral("1")});
    parser.addOption({QStringLiteral("dpi"), QStringLiteral("Output DPI scale (default 96)"), QStringLiteral("dpi"), QStringLiteral("96")});
    parser.addOption({QStringLiteral("version"), QStringLiteral("Print version")});
    parser.process(cliArgs);

    if (parser.isSet(QStringLiteral("version"))) {
        QTextStream(stdout) << "wkhtmltoimage-ng 0.13.0 (Qt WebEngine / Chromium)\n";
        return 0;
    }

    const QStringList positional = parser.positionalArguments();
    if (positional.size() != 2) {
        parser.showHelp(2);
    }

    ImageSettings settings;
    bool ok = true;
    settings.screenWidth = parser.value(QStringLiteral("width")).toInt(&ok);
    if (!ok || settings.screenWidth <= 0) {
        QTextStream(stderr) << "invalid width\n";
        return 2;
    }
    settings.screenHeight = parser.value(QStringLiteral("height")).toInt(&ok);
    if (!ok || settings.screenHeight < 0) {
        QTextStream(stderr) << "invalid height\n";
        return 2;
    }
    settings.javascriptDelay = parser.value(QStringLiteral("javascript-delay")).toInt(&ok);
    if (!ok || settings.javascriptDelay < 0) {
        QTextStream(stderr) << "invalid javascript delay\n";
        return 2;
    }
    settings.quality = parser.value(QStringLiteral("quality")).toInt(&ok);
    if (!ok || settings.quality < 0 || settings.quality > 100) {
        QTextStream(stderr) << "invalid quality\n";
        return 2;
    }
    settings.cropLeft = parser.value(QStringLiteral("crop-x")).toInt(&ok);
    if (!ok || settings.cropLeft < 0) {
        QTextStream(stderr) << "invalid crop-x\n";
        return 2;
    }
    settings.cropTop = parser.value(QStringLiteral("crop-y")).toInt(&ok);
    if (!ok || settings.cropTop < 0) {
        QTextStream(stderr) << "invalid crop-y\n";
        return 2;
    }
    if (parser.isSet(QStringLiteral("crop-width"))) {
        settings.cropWidth = parser.value(QStringLiteral("crop-width")).toInt(&ok);
        if (!ok || settings.cropWidth <= 0) {
            QTextStream(stderr) << "invalid crop-width\n";
            return 2;
        }
    }
    if (parser.isSet(QStringLiteral("crop-height"))) {
        settings.cropHeight = parser.value(QStringLiteral("crop-height")).toInt(&ok);
        if (!ok || settings.cropHeight <= 0) {
            QTextStream(stderr) << "invalid crop-height\n";
            return 2;
        }
    }
    settings.zoom = parser.value(QStringLiteral("zoom")).toDouble(&ok);
    if (!ok || settings.zoom <= 0.0) {
        QTextStream(stderr) << "invalid zoom\n";
        return 2;
    }
    settings.dpi = parser.value(QStringLiteral("dpi")).toInt(&ok);
    if (!ok || settings.dpi <= 0) {
        QTextStream(stderr) << "invalid dpi\n";
        return 2;
    }
    if (parser.isSet(QStringLiteral("format"))) settings.fmt = parser.value(QStringLiteral("format"));
    settings.transparent = parser.isSet(QStringLiteral("transparent"));
    settings.enableLocalFileAccess = parser.isSet(QStringLiteral("enable-local-file-access"));
    settings.enableJavascript = !parser.isSet(QStringLiteral("disable-javascript"));
    settings.smartWidth = !parser.isSet(QStringLiteral("disable-smart-width"));
    settings.in = positional.at(0);
    settings.out = positional.at(1);

    HtmlToImageConverter converter(settings);
    QString error;
    if (!converter.convert(settings.in, settings.out, &error)) {
        QTextStream(stderr) << "wkhtmltoimage-ng: " << error << '\n';
        return 1;
    }
    return 0;
}
