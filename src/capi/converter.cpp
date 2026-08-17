#include "private.h"

#include "utils/FileUtils.h"

#include <QFile>
#include <QTemporaryFile>

namespace wkhtmltox {

void notify(wkhtmltopdf_converter* converter, wkhtmltopdf_str_callback callback,
            const QString& message)
{
    if (!callback) return;
    const QByteArray utf8 = message.toUtf8();
    callback(converter, utf8.constData());
}

void setPhase(wkhtmltopdf_converter* converter, int phase)
{
    if (!converter || phase == converter->common.phase) return;
    converter->common.phase = phase;
    if (phase >= 0 && phase < pdfPhaseCount) {
        converter->common.progress = QByteArray(pdfPhases[phase]) + "...";
    }
    if (converter->common.phaseCallback) converter->common.phaseCallback(converter);
}

void setProgress(wkhtmltopdf_converter* converter, int value)
{
    if (!converter) return;
    if (converter->common.progressCallback) converter->common.progressCallback(converter, value);
}

}

extern "C" {

wkhtmltopdf_converter* wkhtmltopdf_create_converter(wkhtmltopdf_global_settings* settings)
{
    auto* converter = new wkhtmltopdf_converter;
    if (settings) converter->global = settings->value;
    return converter;
}

void wkhtmltopdf_destroy_converter(wkhtmltopdf_converter* converter)
{
    delete converter;
}

int wkhtmltopdf_convert(wkhtmltopdf_converter* converter)
{
    using namespace wkhtmltox;
    if (!converter || converter->objects.isEmpty()) return 0;
    ensureApplication();

    const QString requestedOutput = converter->global.out;
    QTemporaryFile temporary;
    QString outputPath = requestedOutput;
    if (outputPath.isEmpty() || outputPath == QStringLiteral("-")) {
        temporary.setAutoRemove(true);
        if (!temporary.open()) {
            notify(converter, converter->common.errorCallback, QStringLiteral("cannot create temporary PDF"));
            if (converter->common.finishedCallback) converter->common.finishedCallback(converter, 0);
            return 0;
        }
        outputPath = temporary.fileName();
        temporary.close();
    }

    converter->common.phase = -1;
    converter->common.progress.clear();
    notify(converter, converter->common.infoCallback, QStringLiteral("Loading page"));

    HtmlToPdfConverter renderer(converter->global);
    renderer.setPhaseCallback([&](int phase, int percent) {
        setPhase(converter, phase);
        setProgress(converter, percent);
    });
    QString error;
    const bool success = renderer.convert(converter->objects, outputPath, &error);

    if (!success) {
        converter->common.httpError = 1;
        notify(converter, converter->common.errorCallback, error);
        setPhase(converter, pdfPhaseCount - 1);
        converter->common.progress = QByteArrayLiteral("Done");
        setProgress(converter, 100);
    } else {
        bool ok = false;
        converter->common.output = readAllFile(outputPath, &ok);
        if (!ok) {
            converter->common.output.clear();
            notify(converter, converter->common.errorCallback, QStringLiteral("cannot read generated PDF"));
        }
        setPhase(converter, pdfPhaseCount - 1);
        converter->common.progress = QByteArrayLiteral("Done");
        setProgress(converter, 100);
        notify(converter, converter->common.infoCallback, QStringLiteral("PDF generated"));
    }
    if (converter->common.finishedCallback)
        converter->common.finishedCallback(converter, success ? 1 : 0);
    return success ? 1 : 0;
}

void wkhtmltopdf_add_object(wkhtmltopdf_converter* converter,
                            wkhtmltopdf_object_settings* settings, const char* data)
{
    if (!converter) return;
    ObjectSettings object;
    if (settings) object = settings->value;
    if (data) object.page = QString::fromUtf8(data);
    converter->objects.append(object);
}

int wkhtmltopdf_current_phase(wkhtmltopdf_converter* converter)
{ return converter ? converter->common.phase : 0; }

int wkhtmltopdf_phase_count(wkhtmltopdf_converter*)
{ return wkhtmltox::pdfPhaseCount; }

const char* wkhtmltopdf_phase_description(wkhtmltopdf_converter* converter, int phase)
{
    if (!converter) return nullptr;
    if (phase == -1) phase = converter->common.phase;
    if (phase < 0 || phase >= wkhtmltox::pdfPhaseCount) return nullptr;
    return wkhtmltox::pdfPhases[phase];
}

const char* wkhtmltopdf_progress_string(wkhtmltopdf_converter* converter)
{ return converter ? converter->common.progress.constData() : nullptr; }

int wkhtmltopdf_http_error_code(wkhtmltopdf_converter* converter)
{ return converter ? converter->common.httpError : 0; }

long wkhtmltopdf_get_output(wkhtmltopdf_converter* converter, const unsigned char** data)
{
    if (!converter || !data) return 0;
    *data = reinterpret_cast<const unsigned char*>(converter->common.output.constData());
    return converter->common.output.size();
}

}
