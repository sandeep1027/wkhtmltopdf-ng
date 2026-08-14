#include "private.h"
#include "utils/HeadlessQt.h"

extern "C" {

int wkhtmltopdf_init(int)
{
    return 1;
}

int wkhtmltopdf_deinit(void)
{
    return 1;
}

int wkhtmltopdf_extended_qt(void)
{
    return 1;
}

const char* wkhtmltopdf_version(void)
{
    static const char version[] = "0.13.2 (Qt WebEngine)";
    return version;
}

}

namespace wkhtmltox {

const char* const pdfPhases[] = {
    "Loading pages",
    "Counting pages",
    "Resolving links",
    "Adding headers and footers",
    "Printing pages",
    "Done",
};
const int pdfPhaseCount = int(sizeof(pdfPhases) / sizeof(pdfPhases[0]));

const char* const imagePhases[] = {
    "Loading page",
    "Rendering page",
    "Done",
};
const int imagePhaseCount = int(sizeof(imagePhases) / sizeof(imagePhases[0]));

QApplication* ensureApplication()
{
    if (QCoreApplication::instance()) {
        return qobject_cast<QApplication*>(QCoreApplication::instance());
    }
    static QApplication* application = [] {
        prepareHeadlessQt();
        static int argc = 1;
        static char arg0[] = "wkhtmltox";
        static char* argv[] = { arg0, nullptr };
        return new QApplication(argc, argv);
    }();
    return application;
}

}
