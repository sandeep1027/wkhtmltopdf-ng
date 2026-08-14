#ifndef WKHTMLTOX_H
#define WKHTMLTOX_H

#ifdef _WIN32
#  ifdef WKHTMLTOX_BUILD
#    define WKHTMLTOX_API __declspec(dllexport)
#  else
#    define WKHTMLTOX_API __declspec(dllimport)
#  endif
#else
#  define WKHTMLTOX_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wkhtmltopdf_global_settings wkhtmltopdf_global_settings;
typedef struct wkhtmltopdf_object_settings wkhtmltopdf_object_settings;
typedef struct wkhtmltopdf_converter wkhtmltopdf_converter;

typedef void (*wkhtmltopdf_str_callback)(wkhtmltopdf_converter*, const char*);
typedef void (*wkhtmltopdf_int_callback)(wkhtmltopdf_converter*, int);
typedef void (*wkhtmltopdf_void_callback)(wkhtmltopdf_converter*);

WKHTMLTOX_API int wkhtmltopdf_init(int use_graphics);
WKHTMLTOX_API int wkhtmltopdf_deinit(void);
WKHTMLTOX_API int wkhtmltopdf_extended_qt(void);
WKHTMLTOX_API const char* wkhtmltopdf_version(void);

WKHTMLTOX_API wkhtmltopdf_global_settings* wkhtmltopdf_create_global_settings(void);
WKHTMLTOX_API void wkhtmltopdf_destroy_global_settings(wkhtmltopdf_global_settings*);
WKHTMLTOX_API wkhtmltopdf_object_settings* wkhtmltopdf_create_object_settings(void);
WKHTMLTOX_API void wkhtmltopdf_destroy_object_settings(wkhtmltopdf_object_settings*);

WKHTMLTOX_API int wkhtmltopdf_set_global_setting(wkhtmltopdf_global_settings*, const char*, const char*);
WKHTMLTOX_API int wkhtmltopdf_get_global_setting(wkhtmltopdf_global_settings*, const char*, char*, int);
WKHTMLTOX_API int wkhtmltopdf_set_object_setting(wkhtmltopdf_object_settings*, const char*, const char*);
WKHTMLTOX_API int wkhtmltopdf_get_object_setting(wkhtmltopdf_object_settings*, const char*, char*, int);

WKHTMLTOX_API wkhtmltopdf_converter* wkhtmltopdf_create_converter(wkhtmltopdf_global_settings*);
WKHTMLTOX_API void wkhtmltopdf_destroy_converter(wkhtmltopdf_converter*);
WKHTMLTOX_API void wkhtmltopdf_set_debug_callback(wkhtmltopdf_converter*, wkhtmltopdf_str_callback);
WKHTMLTOX_API void wkhtmltopdf_set_info_callback(wkhtmltopdf_converter*, wkhtmltopdf_str_callback);
WKHTMLTOX_API void wkhtmltopdf_set_warning_callback(wkhtmltopdf_converter*, wkhtmltopdf_str_callback);
WKHTMLTOX_API void wkhtmltopdf_set_error_callback(wkhtmltopdf_converter*, wkhtmltopdf_str_callback);
WKHTMLTOX_API void wkhtmltopdf_set_phase_changed_callback(wkhtmltopdf_converter*, wkhtmltopdf_void_callback);
WKHTMLTOX_API void wkhtmltopdf_set_progress_changed_callback(wkhtmltopdf_converter*, wkhtmltopdf_int_callback);
WKHTMLTOX_API void wkhtmltopdf_set_finished_callback(wkhtmltopdf_converter*, wkhtmltopdf_int_callback);
WKHTMLTOX_API int wkhtmltopdf_convert(wkhtmltopdf_converter*);
WKHTMLTOX_API void wkhtmltopdf_add_object(wkhtmltopdf_converter*, wkhtmltopdf_object_settings*, const char*);
WKHTMLTOX_API int wkhtmltopdf_current_phase(wkhtmltopdf_converter*);
WKHTMLTOX_API int wkhtmltopdf_phase_count(wkhtmltopdf_converter*);
WKHTMLTOX_API const char* wkhtmltopdf_phase_description(wkhtmltopdf_converter*, int);
WKHTMLTOX_API const char* wkhtmltopdf_progress_string(wkhtmltopdf_converter*);
WKHTMLTOX_API int wkhtmltopdf_http_error_code(wkhtmltopdf_converter*);
WKHTMLTOX_API long wkhtmltopdf_get_output(wkhtmltopdf_converter*, const unsigned char**);

typedef struct wkhtmltoimage_global_settings wkhtmltoimage_global_settings;
typedef struct wkhtmltoimage_converter wkhtmltoimage_converter;

WKHTMLTOX_API wkhtmltoimage_global_settings* wkhtmltoimage_create_global_settings(void);
WKHTMLTOX_API void wkhtmltoimage_destroy_global_settings(wkhtmltoimage_global_settings*);
WKHTMLTOX_API int wkhtmltoimage_set_global_setting(wkhtmltoimage_global_settings*, const char*, const char*);
WKHTMLTOX_API int wkhtmltoimage_get_global_setting(wkhtmltoimage_global_settings*, const char*, char*, int);

WKHTMLTOX_API wkhtmltoimage_converter* wkhtmltoimage_create_converter(wkhtmltoimage_global_settings*);
WKHTMLTOX_API void wkhtmltoimage_destroy_converter(wkhtmltoimage_converter*);
WKHTMLTOX_API void wkhtmltoimage_set_warning_callback(wkhtmltoimage_converter*, wkhtmltopdf_str_callback);
WKHTMLTOX_API void wkhtmltoimage_set_error_callback(wkhtmltoimage_converter*, wkhtmltopdf_str_callback);
WKHTMLTOX_API void wkhtmltoimage_set_phase_changed_callback(wkhtmltoimage_converter*, wkhtmltopdf_void_callback);
WKHTMLTOX_API void wkhtmltoimage_set_progress_changed_callback(wkhtmltoimage_converter*, wkhtmltopdf_int_callback);
WKHTMLTOX_API void wkhtmltoimage_set_finished_callback(wkhtmltoimage_converter*, wkhtmltopdf_int_callback);
WKHTMLTOX_API int wkhtmltoimage_convert(wkhtmltoimage_converter*);
WKHTMLTOX_API int wkhtmltoimage_current_phase(wkhtmltoimage_converter*);
WKHTMLTOX_API int wkhtmltoimage_phase_count(wkhtmltoimage_converter*);
WKHTMLTOX_API const char* wkhtmltoimage_phase_description(wkhtmltoimage_converter*, int);
WKHTMLTOX_API const char* wkhtmltoimage_progress_string(wkhtmltoimage_converter*);
WKHTMLTOX_API int wkhtmltoimage_http_error_code(wkhtmltoimage_converter*);
WKHTMLTOX_API long wkhtmltoimage_get_output(wkhtmltoimage_converter*, const unsigned char**);

#ifdef __cplusplus
}
#endif

#endif
