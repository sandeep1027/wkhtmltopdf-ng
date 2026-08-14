#include "private.h"

extern "C" {

void wkhtmltopdf_set_debug_callback(wkhtmltopdf_converter* converter, wkhtmltopdf_str_callback callback)
{ if (converter) converter->debugCallback = callback; }

void wkhtmltopdf_set_info_callback(wkhtmltopdf_converter* converter, wkhtmltopdf_str_callback callback)
{ if (converter) converter->infoCallback = callback; }

void wkhtmltopdf_set_warning_callback(wkhtmltopdf_converter* converter, wkhtmltopdf_str_callback callback)
{ if (converter) converter->warningCallback = callback; }

void wkhtmltopdf_set_error_callback(wkhtmltopdf_converter* converter, wkhtmltopdf_str_callback callback)
{ if (converter) converter->errorCallback = callback; }

void wkhtmltopdf_set_phase_changed_callback(wkhtmltopdf_converter* converter, wkhtmltopdf_void_callback callback)
{ if (converter) converter->phaseCallback = callback; }

void wkhtmltopdf_set_progress_changed_callback(wkhtmltopdf_converter* converter, wkhtmltopdf_int_callback callback)
{ if (converter) converter->progressCallback = callback; }

void wkhtmltopdf_set_finished_callback(wkhtmltopdf_converter* converter, wkhtmltopdf_int_callback callback)
{ if (converter) converter->finishedCallback = callback; }

}
