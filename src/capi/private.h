#ifndef WKHTMLTOX_PRIVATE_H
#define WKHTMLTOX_PRIVATE_H

#include "wkhtmltox.h"
#include "converter/HtmlToImageConverter.h"
#include "converter/HtmlToPdfConverter.h"
#include "converter/Settings.h"

#include <QApplication>
#include <QByteArray>
#include <QList>
#include <QString>

#include <cstring>

namespace wkhtmltox {

extern const char* const pdfPhases[];
extern const int pdfPhaseCount;
extern const char* const imagePhases[];
extern const int imagePhaseCount;

QApplication* ensureApplication();

}

struct wkhtmltopdf_global_settings {
    GlobalSettings value;
};

struct wkhtmltopdf_object_settings {
    ObjectSettings value;
};

// Shared state used by the wkhtmltopdf_* and wkhtmltoimage_* converter
// accessors. It is the FIRST member of both converter structs so that the
// pointer handed to wkhtmltopdf_* callbacks (which the 0.12 API types as
// wkhtmltopdf_converter* even for image converters) can be safely passed back
// to wkhtmltoimage_current_phase / wkhtmltoimage_get_output / etc.: those
// functions only touch this region, which lives at the same offsets in both
// structs.
struct wkhtmltox_converter_common {
    wkhtmltopdf_str_callback debugCallback = nullptr;
    wkhtmltopdf_str_callback infoCallback = nullptr;
    wkhtmltopdf_str_callback warningCallback = nullptr;
    wkhtmltopdf_str_callback errorCallback = nullptr;
    wkhtmltopdf_void_callback phaseCallback = nullptr;
    wkhtmltopdf_int_callback progressCallback = nullptr;
    wkhtmltopdf_int_callback finishedCallback = nullptr;
    QByteArray output;
    QByteArray progress;
    int phase = 0;
    int httpError = 0;
};

struct wkhtmltopdf_converter {
    wkhtmltox_converter_common common;
    GlobalSettings global;
    QList<ObjectSettings> objects;
};

struct wkhtmltoimage_global_settings {
    ImageSettings global;
};

struct wkhtmltoimage_converter {
    wkhtmltox_converter_common common;
    ImageSettings global;
};

inline void copyString(const QString& value, char* output, int size)
{
    if (!output || size <= 0) return;
    const QByteArray utf8 = value.toUtf8();
    const int length = qMin(size - 1, utf8.size());
    std::memcpy(output, utf8.constData(), static_cast<size_t>(length));
    output[length] = '\0';
}

#endif
