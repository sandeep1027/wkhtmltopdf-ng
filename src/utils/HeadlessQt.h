#ifndef WKHTMLTOPDF_NG_HEADLESS_QT_H
#define WKHTMLTOPDF_NG_HEADLESS_QT_H

#include <QByteArray>
#include <QCoreApplication>
#include <QString>
#include <cstdio>
#include <cstring>

inline void wkhtmltopdfNgMessageHandler(QtMsgType type, const QMessageLogContext& context,
                                        const QString& message)
{
    const QByteArray category(context.category ? context.category : "");
    if (category == "qt.webenginecontext") return;
    if (message.contains(QLatin1String("GLImplementation")) ||
        message.contains(QLatin1String("Sandboxing disabled by user"))) {
        return;
    }
    const QByteArray line = qFormatLogMessage(type, context, message).toLocal8Bit();
    std::fwrite(line.constData(), 1, static_cast<size_t>(line.size()), stderr);
    std::fwrite("\n", 1, 1, stderr);
}

inline void prepareHeadlessQt(int argc = 0, char** argv = nullptr)
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    QByteArray flags = qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");
    const char* extras[] = {
        "--no-sandbox",
        "--disable-gpu",
        "--disable-gpu-compositing",
        "--disable-dev-shm-usage",
        "--disable-seccomp-filter-sandbox",
    };
    for (const char* extra : extras) {
        if (!flags.contains(extra)) {
            if (!flags.isEmpty()) flags += ' ';
            flags += extra;
        }
    }
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", flags);

    bool verbose = false;
    if (argv) {
        for (int i = 1; i < argc; ++i) {
            if (!argv[i]) continue;
            if (std::strcmp(argv[i], "--debug-javascript") == 0) verbose = true;
            if (std::strcmp(argv[i], "--verbose") == 0 || std::strcmp(argv[i], "-v") == 0)
                verbose = true;
            if (std::strcmp(argv[i], "--log-level") == 0 && i + 1 < argc && argv[i + 1] &&
                (std::strcmp(argv[i + 1], "info") == 0 || std::strcmp(argv[i + 1], "warn") == 0 ||
                 std::strcmp(argv[i + 1], "verbose") == 0)) {
                verbose = true;
            }
        }
    }
    if (!verbose && qEnvironmentVariableIsEmpty("QT_LOGGING_RULES")) {
        qputenv("QT_LOGGING_RULES", "qt.webenginecontext=false;qt.webenginecontext.debug=false");
    }
    if (!verbose) {
        qInstallMessageHandler(wkhtmltopdfNgMessageHandler);
    }
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts, true);
}

#endif
