#include "private.h"

#include <cstring>

extern "C" {

wkhtmltopdf_global_settings* wkhtmltopdf_create_global_settings(void)
{
    return new wkhtmltopdf_global_settings;
}

void wkhtmltopdf_destroy_global_settings(wkhtmltopdf_global_settings* settings)
{
    delete settings;
}

int wkhtmltopdf_set_global_setting(wkhtmltopdf_global_settings* settings,
                                   const char* name, const char* value)
{
    return settings && name && value && settings->value.set(name, QString::fromUtf8(value));
}

int wkhtmltopdf_get_global_setting(wkhtmltopdf_global_settings* settings,
                                   const char* name, char* value, int valueSize)
{
    if (!settings || !name || !value || valueSize <= 0) return 0;
    const QString result = settings->value.get(name);
    if (result.isNull()) return 0;
    copyString(result, value, valueSize);
    return 1;
}

wkhtmltopdf_object_settings* wkhtmltopdf_create_object_settings(void)
{
    return new wkhtmltopdf_object_settings;
}

void wkhtmltopdf_destroy_object_settings(wkhtmltopdf_object_settings* settings)
{
    delete settings;
}

int wkhtmltopdf_set_object_setting(wkhtmltopdf_object_settings* settings,
                                   const char* name, const char* value)
{
    return settings && name && value && settings->value.set(name, QString::fromUtf8(value));
}

int wkhtmltopdf_get_object_setting(wkhtmltopdf_object_settings* settings,
                                   const char* name, char* value, int valueSize)
{
    if (!settings || !name || !value || valueSize <= 0) return 0;
    const QString result = settings->value.get(name);
    if (result.isNull()) return 0;
    copyString(result, value, valueSize);
    return 1;
}

}
