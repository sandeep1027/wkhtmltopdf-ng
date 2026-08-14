#include "converter/HtmlToPdfConverter.h"
#include "converter/TocGenerator.h"
#include "utils/ArgParser.h"
#include "utils/HeadlessQt.h"

#include <QApplication>
#include <QTextStream>
#include <QFile>
#include <QIODevice>

namespace {
constexpr const char* version = "0.13.0";
}

int processArguments(QApplication& application, const QStringList& args)
{
    const ParsedArguments arguments = ArgParser::parse(args);
    if (arguments.help) {
        QTextStream(stdout) << ArgParser::usage();
        return 0;
    }
    if (arguments.version) {
        QTextStream(stdout) << "wkhtmltopdf-ng " << version
                            << " (Qt WebEngine / Chromium)\n";
        return 0;
    }
    if (arguments.global.dumpDefaultTocXsl) {
        QTextStream(stdout) << defaultTocXsl();
        return 0;
    }
    if (!arguments.error.isEmpty()) {
        QTextStream(stderr) << "wkhtmltopdf-ng: " << arguments.error << "\n\n"
                            << ArgParser::usage();
        return 2;
    }

    HtmlToPdfConverter converter(arguments.global);
    QList<ObjectSettings> objects = arguments.objects;
    if (objects.isEmpty()) {
        for (const QString& input : arguments.inputs) {
            ObjectSettings object = arguments.object;
            object.page = input;
            objects.append(object);
        }
    }
    QString error;
    if (!converter.convert(objects, arguments.output, &error)) {
        if (!arguments.global.quiet) {
            QTextStream(stderr) << "wkhtmltopdf-ng: " << error << '\n';
        }
        return 1;
    }
    return 0;
}

int main(int argc, char* argv[])
{
    prepareHeadlessQt(argc, argv);
    // Keep Chromium from treating wkhtmltopdf flags as its own command line.
    QStringList cliArgs;
    for (int i = 1; i < argc; ++i)
        cliArgs.append(QString::fromLocal8Bit(argv[i]));
    int qtArgc = 1;
    char* qtArgv[] = { argv[0], nullptr };
    QApplication application(qtArgc, qtArgv);
    application.setApplicationName(QStringLiteral("wkhtmltopdf-ng"));
    application.setApplicationVersion(QString::fromLatin1(version));

    ParsedArguments initial = ArgParser::parse(cliArgs);
    if (initial.help || initial.version || !initial.error.isEmpty()) {
        if (initial.help) {
            QTextStream(stdout) << ArgParser::usage();
            return 0;
        }
        if (initial.version) {
            QTextStream(stdout) << "wkhtmltopdf-ng " << version
                                << " (Qt WebEngine / Chromium)\n";
            return 0;
        }
        QTextStream(stderr) << "wkhtmltopdf-ng: " << initial.error << "\n\n"
                            << ArgParser::usage();
        return 2;
    }

    if (initial.global.readArgsFromStdin) {
        QTextStream stdinStream(stdin, QIODevice::ReadOnly);
        int exitCode = 0;
        while (!stdinStream.atEnd()) {
            const QString line = stdinStream.readLine().trimmed();
            if (line.isEmpty()) continue;
            const QStringList lineArgs = line.split(' ', Qt::SkipEmptyParts);
            exitCode = processArguments(application, lineArgs);
            if (exitCode != 0) break;
        }
        return exitCode;
    }

    return processArguments(application, cliArgs);
}
