#include "private.h"

#include "utils/FileUtils.h"

#include <QTemporaryFile>

namespace {

wkhtmltopdf_converter* asPdfConverter(wkhtmltoimage_converter* converter)
{
    return reinterpret_cast<wkhtmltopdf_converter*>(converter);
}

void notify(wkhtmltoimage_converter* converter, wkhtmltopdf_str_callback callback,
            const QString& message)
{
    if (!callback) return;
    const QByteArray utf8 = message.toUtf8();
    callback(asPdfConverter(converter), utf8.constData());
}

void setPhase(wkhtmltoimage_converter* converter, int phase)
{
    if (!converter || phase == converter->common.phase) return;
    converter->common.phase = phase;
    if (phase >= 0 && phase < wkhtmltox::imagePhaseCount) {
        converter->common.progress = QByteArray(wkhtmltox::imagePhases[phase]) + "...";
    }
    if (converter->common.phaseCallback)
        converter->common.phaseCallback(asPdfConverter(converter));
}

void setProgress(wkhtmltoimage_converter* converter, int value)
{
    if (!converter) return;
    if (converter->common.progressCallback)
        converter->common.progressCallback(asPdfConverter(converter), value);
}

}

extern "C" {

wkhtmltoimage_global_settings* wkhtmltoimage_create_global_settings(void)
{
    return new wkhtmltoimage_global_settings;
}

void wkhtmltoimage_destroy_global_settings(wkhtmltoimage_global_settings* settings)
{
    delete settings;
}

int wkhtmltoimage_set_global_setting(wkhtmltoimage_global_settings* settings,
                                     const char* name, const char* value)
{
    return settings && name && value && settings->global.set(name, QString::fromUtf8(value));
}

int wkhtmltoimage_get_global_setting(wkhtmltoimage_global_settings* settings,
                                     const char* name, char* value, int valueSize)
{
    if (!settings || !name || !value || valueSize <= 0) return 0;
    const QString result = settings->global.get(name);
    if (result.isNull()) return 0;
    copyString(result, value, valueSize);
    return 1;
}

wkhtmltoimage_converter* wkhtmltoimage_create_converter(wkhtmltoimage_global_settings* settings)
{
    auto* converter = new wkhtmltoimage_converter;
    if (settings) converter->global = settings->global;
    return converter;
}

void wkhtmltoimage_destroy_converter(wkhtmltoimage_converter* converter)
{
    delete converter;
}

void wkhtmltoimage_set_warning_callback(wkhtmltoimage_converter* converter, wkhtmltopdf_str_callback callback)
{ if (converter) converter->common.warningCallback = callback; }

void wkhtmltoimage_set_error_callback(wkhtmltoimage_converter* converter, wkhtmltopdf_str_callback callback)
{ if (converter) converter->common.errorCallback = callback; }

void wkhtmltoimage_set_phase_changed_callback(wkhtmltoimage_converter* converter, wkhtmltopdf_void_callback callback)
{ if (converter) converter->common.phaseCallback = callback; }

void wkhtmltoimage_set_progress_changed_callback(wkhtmltoimage_converter* converter, wkhtmltopdf_int_callback callback)
{ if (converter) converter->common.progressCallback = callback; }

void wkhtmltoimage_set_finished_callback(wkhtmltoimage_converter* converter, wkhtmltopdf_int_callback callback)
{ if (converter) converter->common.finishedCallback = callback; }

int wkhtmltoimage_convert(wkhtmltoimage_converter* converter)
{
    using namespace wkhtmltox;
    if (!converter) return 0;
    if (converter->global.in.isEmpty()) {
        notify(converter, converter->common.errorCallback, QStringLiteral("no input"));
        if (converter->common.finishedCallback)
            converter->common.finishedCallback(asPdfConverter(converter), 0);
        return 0;
    }
    ensureApplication();

    converter->common.phase = -1;
    converter->common.progress.clear();
    setPhase(converter, 0);
    setProgress(converter, 0);
    notify(converter, converter->common.infoCallback, QStringLiteral("Loading page"));

    const QString outputPath = converter->global.out;
    HtmlToImageConverter renderer(converter->global);
    setPhase(converter, 1);
    setProgress(converter, 50);
    QString error;
    bool success = false;
    if (outputPath.isEmpty() || outputPath == QStringLiteral("-")) {
        converter->common.output = renderer.convertToBuffer(converter->global.in, &error);
        success = !converter->common.output.isEmpty();
    } else {
        success = renderer.convert(converter->global.in, outputPath, &error);
        if (success) {
            bool ok = false;
            converter->common.output = readAllFile(outputPath, &ok);
        }
    }

    if (!success) {
        converter->common.httpError = 1;
        notify(converter, converter->common.errorCallback, error);
        converter->common.phase = imagePhaseCount - 1;
        converter->common.progress = QByteArrayLiteral("Done");
        if (converter->common.phaseCallback)
            converter->common.phaseCallback(asPdfConverter(converter));
        setProgress(converter, 100);
    } else {
        setPhase(converter, imagePhaseCount - 2);
        setProgress(converter, 90);
        setPhase(converter, imagePhaseCount - 1);
        converter->common.progress = QByteArrayLiteral("Done");
        if (converter->common.progressCallback)
            converter->common.progressCallback(asPdfConverter(converter), 100);
        notify(converter, converter->common.infoCallback, QStringLiteral("Image generated"));
    }
    if (converter->common.finishedCallback)
        converter->common.finishedCallback(asPdfConverter(converter), success ? 1 : 0);
    return success ? 1 : 0;
}

int wkhtmltoimage_current_phase(wkhtmltoimage_converter* converter)
{ return converter ? converter->common.phase : 0; }

int wkhtmltoimage_phase_count(wkhtmltoimage_converter*)
{ return wkhtmltox::imagePhaseCount; }

const char* wkhtmltoimage_phase_description(wkhtmltoimage_converter* converter, int phase)
{
    if (!converter) return nullptr;
    if (phase == -1) phase = converter->common.phase;
    if (phase < 0 || phase >= wkhtmltox::imagePhaseCount) return nullptr;
    return wkhtmltox::imagePhases[phase];
}

const char* wkhtmltoimage_progress_string(wkhtmltoimage_converter* converter)
{ return converter ? converter->common.progress.constData() : nullptr; }

int wkhtmltoimage_http_error_code(wkhtmltoimage_converter* converter)
{ return converter ? converter->common.httpError : 0; }

long wkhtmltoimage_get_output(wkhtmltoimage_converter* converter, const unsigned char** data)
{
    if (!converter || !data) return 0;
    *data = reinterpret_cast<const unsigned char*>(converter->common.output.constData());
    return converter->common.output.size();
}

}
