#ifndef WKHTMLTOPDF_NG_HTML_TO_IMAGE_CONVERTER_H
#define WKHTMLTOPDF_NG_HTML_TO_IMAGE_CONVERTER_H

#include "Settings.h"

#include <QByteArray>
#include <QString>

class HtmlToImageConverter {
public:
    explicit HtmlToImageConverter(const ImageSettings& settings);

    QByteArray convertToBuffer(const QString& input, QString* error = nullptr);
    bool convert(const QString& input, const QString& outputPath, QString* error = nullptr);

private:
    ImageSettings m_settings;
};

#endif
