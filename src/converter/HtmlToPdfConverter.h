#ifndef WKHTMLTOPDF_NG_HTML_TO_PDF_CONVERTER_H
#define WKHTMLTOPDF_NG_HTML_TO_PDF_CONVERTER_H

#include "PageLayout.h"
#include "PdfPostProcessor.h"
#include "Settings.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <functional>

class HtmlToPdfConverter {
public:
    explicit HtmlToPdfConverter(const GlobalSettings& global);

    bool convert(const ObjectSettings& object, const QString& outputPath, QString* error = nullptr);
    bool convert(const QList<ObjectSettings>& objects, const QString& outputPath, QString* error = nullptr);
    void setPhaseCallback(std::function<void(int phase, int percent)> callback);

private:
    bool convertDocuments(const QList<ObjectSettings>& objects, const QString& outputPath,
                          QString* error, bool finalize);
    bool convertObjectPipeline(const QList<ObjectSettings>& objects, const QString& outputPath,
                               QString* error);

    void reportPhase(int phase, int percent);

    GlobalSettings m_global;
    QList<OutlineEntry> m_lastOutline;
    std::function<void(int, int)> m_phaseCallback;
};

#endif
