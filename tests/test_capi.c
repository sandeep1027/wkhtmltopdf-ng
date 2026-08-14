#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond, msg)                                                     \
    do {                                                                     \
        if (!(cond)) {                                                       \
            fprintf(stderr, "FAIL: %s\n", msg);                              \
            failures++;                                                      \
        }                                                                    \
    } while (0)

typedef struct wkhtmltopdf_global_settings wkhtmltopdf_global_settings;
typedef struct wkhtmltopdf_object_settings wkhtmltopdf_object_settings;
typedef struct wkhtmltopdf_converter wkhtmltopdf_converter;
typedef struct wkhtmltoimage_global_settings wkhtmltoimage_global_settings;
typedef struct wkhtmltoimage_converter wkhtmltoimage_converter;

typedef void (*wkhtmltopdf_str_callback)(wkhtmltopdf_converter*, const char*);
typedef void (*wkhtmltopdf_int_callback)(wkhtmltopdf_converter*, int);
typedef void (*wkhtmltopdf_void_callback)(wkhtmltopdf_converter*);

static int phaseChanges = 0;
static int progressValues = 0;
static int lastProgress = -1;
static int finishedValue = -1;
static int errorCount = 0;

static void phase_cb(wkhtmltopdf_converter* c)
{
    (void)c;
    phaseChanges++;
}

static void progress_cb(wkhtmltopdf_converter* c, int value)
{
    (void)c;
    lastProgress = value;
    progressValues++;
}

static void finished_cb(wkhtmltopdf_converter* c, int value)
{
    (void)c;
    finishedValue = value;
}

static void error_cb(wkhtmltopdf_converter* c, const char* message)
{
    (void)c;
    (void)message;
    errorCount++;
}

static int has_pdf_magic(const unsigned char* data, long size)
{
    return size > 4 && data[0] == '%' && data[1] == 'P' && data[2] == 'D' && data[3] == 'F';
}

static int has_png_magic(const unsigned char* data, long size)
{
    static const unsigned char png[] = { 0x89, 'P', 'N', 'G' };
    return size > 4 && memcmp(data, png, 4) == 0;
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <libwkhtmltox> [test-data-dir [out-dir]]\n", argv[0]);
        return 1;
    }

    void* handle = dlopen(argv[1], RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        fprintf(stderr, "FAIL: dlopen(%s): %s\n", argv[1], dlerror());
        return 1;
    }

    const char* required[] = {
        "wkhtmltopdf_init",
        "wkhtmltopdf_deinit",
        "wkhtmltopdf_extended_qt",
        "wkhtmltopdf_version",
        "wkhtmltopdf_create_global_settings",
        "wkhtmltopdf_destroy_global_settings",
        "wkhtmltopdf_create_object_settings",
        "wkhtmltopdf_destroy_object_settings",
        "wkhtmltopdf_set_global_setting",
        "wkhtmltopdf_get_global_setting",
        "wkhtmltopdf_set_object_setting",
        "wkhtmltopdf_get_object_setting",
        "wkhtmltopdf_create_converter",
        "wkhtmltopdf_destroy_converter",
        "wkhtmltopdf_set_debug_callback",
        "wkhtmltopdf_set_info_callback",
        "wkhtmltopdf_set_warning_callback",
        "wkhtmltopdf_set_error_callback",
        "wkhtmltopdf_set_phase_changed_callback",
        "wkhtmltopdf_set_progress_changed_callback",
        "wkhtmltopdf_set_finished_callback",
        "wkhtmltopdf_convert",
        "wkhtmltopdf_add_object",
        "wkhtmltopdf_current_phase",
        "wkhtmltopdf_phase_count",
        "wkhtmltopdf_phase_description",
        "wkhtmltopdf_progress_string",
        "wkhtmltopdf_http_error_code",
        "wkhtmltopdf_get_output",
        "wkhtmltoimage_create_global_settings",
        "wkhtmltoimage_destroy_global_settings",
        "wkhtmltoimage_set_global_setting",
        "wkhtmltoimage_get_global_setting",
        "wkhtmltoimage_create_converter",
        "wkhtmltoimage_destroy_converter",
        "wkhtmltoimage_set_warning_callback",
        "wkhtmltoimage_set_error_callback",
        "wkhtmltoimage_set_phase_changed_callback",
        "wkhtmltoimage_set_progress_changed_callback",
        "wkhtmltoimage_set_finished_callback",
        "wkhtmltoimage_convert",
        "wkhtmltoimage_current_phase",
        "wkhtmltoimage_phase_count",
        "wkhtmltoimage_phase_description",
        "wkhtmltoimage_progress_string",
        "wkhtmltoimage_http_error_code",
        "wkhtmltoimage_get_output",
    };
    const size_t requiredCount = sizeof(required) / sizeof(required[0]);
    for (size_t i = 0; i < requiredCount; ++i) {
        if (!dlsym(handle, required[i])) {
            fprintf(stderr, "FAIL: missing symbol %s\n", required[i]);
            failures++;
        }
    }

    const char* (*wkhtmltopdf_version)(void) =
        (const char* (*)(void))dlsym(handle, "wkhtmltopdf_version");
    CHECK(wkhtmltopdf_version != NULL && strstr(wkhtmltopdf_version(), "0.13") != NULL,
          "version string");
    CHECK(strstr(wkhtmltopdf_version(), "0.13") != NULL, "version 0.13");

    int (*wkhtmltopdf_init)(int) = (int (*)(int))dlsym(handle, "wkhtmltopdf_init");
    CHECK(wkhtmltopdf_init != NULL && wkhtmltopdf_init(0) == 1, "wkhtmltopdf_init");

    wkhtmltopdf_global_settings* (*create_global)(void) =
        (wkhtmltopdf_global_settings * (*)(void))dlsym(handle, "wkhtmltopdf_create_global_settings");
    void (*destroy_global)(wkhtmltopdf_global_settings*) =
        (void (*)(wkhtmltopdf_global_settings*))dlsym(handle, "wkhtmltopdf_destroy_global_settings");
    int (*set_global)(wkhtmltopdf_global_settings*, const char*, const char*) =
        (int (*)(wkhtmltopdf_global_settings*, const char*, const char*))dlsym(handle, "wkhtmltopdf_set_global_setting");
    int (*get_global)(wkhtmltopdf_global_settings*, const char*, char*, int) =
        (int (*)(wkhtmltopdf_global_settings*, const char*, char*, int))dlsym(handle, "wkhtmltopdf_get_global_setting");

    wkhtmltopdf_global_settings* gs = create_global();
    CHECK(gs != NULL, "create_global_settings");
    CHECK(set_global(gs, "out", "/tmp/wkhtmltox-test.pdf") == 1, "set out");
    char buffer[256];
    CHECK(get_global(gs, "out", buffer, sizeof(buffer)) == 1 &&
              strcmp(buffer, "/tmp/wkhtmltox-test.pdf") == 0,
          "get out round-trip");
    CHECK(set_global(gs, "dpi", "150") == 1, "set dpi");
    CHECK(get_global(gs, "dpi", buffer, sizeof(buffer)) == 1 && strcmp(buffer, "150") == 0,
          "get dpi round-trip");
    CHECK(set_global(gs, "no.such.key", "x") == 0, "unknown global key rejected");
    destroy_global(gs);

    wkhtmltopdf_object_settings* (*create_object)(void) =
        (wkhtmltopdf_object_settings * (*)(void))dlsym(handle, "wkhtmltopdf_create_object_settings");
    void (*destroy_object)(wkhtmltopdf_object_settings*) =
        (void (*)(wkhtmltopdf_object_settings*))dlsym(handle, "wkhtmltopdf_destroy_object_settings");
    int (*set_object)(wkhtmltopdf_object_settings*, const char*, const char*) =
        (int (*)(wkhtmltopdf_object_settings*, const char*, const char*))dlsym(handle, "wkhtmltopdf_set_object_setting");
    int (*get_object)(wkhtmltopdf_object_settings*, const char*, char*, int) =
        (int (*)(wkhtmltopdf_object_settings*, const char*, char*, int))dlsym(handle, "wkhtmltopdf_get_object_setting");

    wkhtmltopdf_object_settings* os = create_object();
    CHECK(os != NULL, "create_object_settings");
    CHECK(set_object(os, "page", "http://example.com/") == 1, "set page");
    CHECK(get_object(os, "page", buffer, sizeof(buffer)) == 1 &&
              strcmp(buffer, "http://example.com/") == 0,
          "get page round-trip");
    CHECK(set_object(os, "footer.left", "Confidential") == 1, "set footer.left");
    CHECK(get_object(os, "footer.left", buffer, sizeof(buffer)) == 1 &&
              strcmp(buffer, "Confidential") == 0,
          "get footer.left round-trip");
    CHECK(set_object(os, "no.such.key", "x") == 0, "unknown object key rejected");
    destroy_object(os);

    wkhtmltoimage_global_settings* (*image_create)(void) =
        (wkhtmltoimage_global_settings * (*)(void))dlsym(handle, "wkhtmltoimage_create_global_settings");
    void (*image_destroy)(wkhtmltoimage_global_settings*) =
        (void (*)(wkhtmltoimage_global_settings*))dlsym(handle, "wkhtmltoimage_destroy_global_settings");
    int (*image_set)(wkhtmltoimage_global_settings*, const char*, const char*) =
        (int (*)(wkhtmltoimage_global_settings*, const char*, const char*))dlsym(handle, "wkhtmltoimage_set_global_setting");
    int (*image_get)(wkhtmltoimage_global_settings*, const char*, char*, int) =
        (int (*)(wkhtmltoimage_global_settings*, const char*, char*, int))dlsym(handle, "wkhtmltoimage_get_global_setting");

    wkhtmltoimage_global_settings* igs = image_create();
    CHECK(igs != NULL, "image create_global_settings");
    CHECK(image_set(igs, "screenWidth", "800") == 1, "image set screenWidth");
    CHECK(image_get(igs, "screenWidth", buffer, sizeof(buffer)) == 1 &&
              strcmp(buffer, "800") == 0,
          "image get screenWidth round-trip");
    CHECK(image_set(igs, "fmt", "jpg") == 1, "image set fmt");
    CHECK(image_get(igs, "fmt", buffer, sizeof(buffer)) == 1 && strcmp(buffer, "jpg") == 0,
          "image get fmt round-trip");
    CHECK(image_set(igs, "crop.width", "320") == 1, "image set crop.width");
    CHECK(image_get(igs, "crop.width", buffer, sizeof(buffer)) == 1 &&
              strcmp(buffer, "320") == 0,
          "image get crop.width round-trip");
    CHECK(image_set(igs, "no.such.key", "x") == 0, "image unknown key rejected");
    image_destroy(igs);

    wkhtmltopdf_converter* (*create_converter)(wkhtmltopdf_global_settings*) =
        (wkhtmltopdf_converter * (*)(wkhtmltopdf_global_settings*))dlsym(handle, "wkhtmltopdf_create_converter");
    void (*destroy_converter)(wkhtmltopdf_converter*) =
        (void (*)(wkhtmltopdf_converter*))dlsym(handle, "wkhtmltopdf_destroy_converter");
    int (*phase_count)(wkhtmltopdf_converter*) = (int (*)(wkhtmltopdf_converter*))dlsym(handle, "wkhtmltopdf_phase_count");
    const char* (*phase_description)(wkhtmltopdf_converter*, int) =
        (const char* (*)(wkhtmltopdf_converter*, int))dlsym(handle, "wkhtmltopdf_phase_description");

    wkhtmltopdf_converter* conv = create_converter(NULL);
    CHECK(conv != NULL, "create_converter");
    CHECK(phase_count(conv) == 6, "pdf phase count is 6");
    CHECK(phase_description(conv, 0) != NULL && strcmp(phase_description(conv, 0), "Loading pages") == 0,
          "phase 0 description");
    CHECK(phase_description(conv, 5) != NULL && strcmp(phase_description(conv, 5), "Done") == 0,
          "phase 5 description");
    CHECK(phase_description(conv, 99) == NULL, "out-of-range phase description");
    destroy_converter(conv);

    wkhtmltoimage_converter* (*image_create_converter)(wkhtmltoimage_global_settings*) =
        (wkhtmltoimage_converter * (*)(wkhtmltoimage_global_settings*))dlsym(handle, "wkhtmltoimage_create_converter");
    void (*image_destroy_converter)(wkhtmltoimage_converter*) =
        (void (*)(wkhtmltoimage_converter*))dlsym(handle, "wkhtmltoimage_destroy_converter");
    int (*image_phase_count)(wkhtmltoimage_converter*) = (int (*)(wkhtmltoimage_converter*))dlsym(handle, "wkhtmltoimage_phase_count");
    const char* (*image_phase_description)(wkhtmltoimage_converter*, int) =
        (const char* (*)(wkhtmltoimage_converter*, int))dlsym(handle, "wkhtmltoimage_phase_description");

    wkhtmltoimage_converter* iconv = image_create_converter(NULL);
    CHECK(iconv != NULL, "image create_converter");
    CHECK(image_phase_count(iconv) == 3, "image phase count is 3");
    CHECK(image_phase_description(iconv, 1) != NULL &&
              strcmp(image_phase_description(iconv, 1), "Rendering page") == 0,
          "image phase 1 description");
    image_destroy_converter(iconv);

    if (argc >= 3) {
        const char* dataDir = argv[2];
        const char* outDir = argc >= 4 ? argv[3] : argv[2];
        char htmlPath[1024];
        snprintf(htmlPath, sizeof(htmlPath), "%s/toc.html", dataDir);
        char outPath[1024];
        snprintf(outPath, sizeof(outPath), "%s/capi.pdf", outDir);

        wkhtmltopdf_global_settings* g = create_global();
        set_global(g, "out", outPath);
        wkhtmltopdf_converter* c = create_converter(g);
        destroy_global(g);
        wkhtmltopdf_object_settings* o = create_object();
        char value[2048];
        snprintf(value, sizeof(value), "file://%s", htmlPath);
        set_object(o, "page", value);
        set_object(o, "enableLocalFileAccess", "true");
        set_object(o, "footer.left", "C API");

        void (*add_object)(wkhtmltopdf_converter*, wkhtmltopdf_object_settings*, const char*) =
            (void (*)(wkhtmltopdf_converter*, wkhtmltopdf_object_settings*, const char*))dlsym(handle, "wkhtmltopdf_add_object");
        void (*set_phase_cb)(wkhtmltopdf_converter*, wkhtmltopdf_void_callback) =
            (void (*)(wkhtmltopdf_converter*, wkhtmltopdf_void_callback))dlsym(handle, "wkhtmltopdf_set_phase_changed_callback");
        void (*set_progress_cb)(wkhtmltopdf_converter*, wkhtmltopdf_int_callback) =
            (void (*)(wkhtmltopdf_converter*, wkhtmltopdf_int_callback))dlsym(handle, "wkhtmltopdf_set_progress_changed_callback");
        void (*set_finished_cb)(wkhtmltopdf_converter*, wkhtmltopdf_int_callback) =
            (void (*)(wkhtmltopdf_converter*, wkhtmltopdf_int_callback))dlsym(handle, "wkhtmltopdf_set_finished_callback");
        void (*set_error_cb)(wkhtmltopdf_converter*, wkhtmltopdf_str_callback) =
            (void (*)(wkhtmltopdf_converter*, wkhtmltopdf_str_callback))dlsym(handle, "wkhtmltopdf_set_error_callback");
        int (*do_convert)(wkhtmltopdf_converter*) = (int (*)(wkhtmltopdf_converter*))dlsym(handle, "wkhtmltopdf_convert");
        int (*current_phase)(wkhtmltopdf_converter*) = (int (*)(wkhtmltopdf_converter*))dlsym(handle, "wkhtmltopdf_current_phase");
        const char* (*progress_string)(wkhtmltopdf_converter*) = (const char* (*)(wkhtmltopdf_converter*))dlsym(handle, "wkhtmltopdf_progress_string");
        int (*http_error)(wkhtmltopdf_converter*) = (int (*)(wkhtmltopdf_converter*))dlsym(handle, "wkhtmltopdf_http_error_code");
        long (*get_output)(wkhtmltopdf_converter*, const unsigned char**) = (long (*)(wkhtmltopdf_converter*, const unsigned char**))dlsym(handle, "wkhtmltopdf_get_output");

        set_phase_cb(c, phase_cb);
        set_progress_cb(c, progress_cb);
        set_finished_cb(c, finished_cb);
        set_error_cb(c, error_cb);
        add_object(c, o, NULL);

        phaseChanges = 0;
        progressValues = 0;
        lastProgress = -1;
        finishedValue = -1;
        errorCount = 0;

        const int ok = do_convert(c);
        CHECK(ok == 1, "pdf conversion succeeded");
        CHECK(errorCount == 0, "pdf conversion produced no errors");
        CHECK(finishedValue == 1, "pdf finished callback value 1");
        CHECK(phaseChanges >= 6, "pdf phase callback fired for all phases");
        CHECK(progressValues >= 2, "pdf progress callback fired");
        CHECK(lastProgress == 100, "pdf progress reached 100");
        CHECK(current_phase(c) == 5, "pdf current phase is Done");
        CHECK(progress_string(c) != NULL && strcmp(progress_string(c), "Done") == 0,
              "pdf progress string is Done");
        CHECK(http_error(c) == 0, "pdf http error code 0");

        const unsigned char* data = NULL;
        const long size = get_output(c, &data);
        CHECK(size > 0 && data != NULL, "pdf output available");
        CHECK(has_pdf_magic(data, size), "pdf output has %PDF magic");
        destroy_object(o);
        destroy_converter(c);

        int (*image_convert)(wkhtmltoimage_converter*) =
            (int (*)(wkhtmltoimage_converter*))dlsym(handle, "wkhtmltoimage_convert");
        long (*image_get_output)(wkhtmltoimage_converter*, const unsigned char**) =
            (long (*)(wkhtmltoimage_converter*, const unsigned char**))dlsym(handle, "wkhtmltoimage_get_output");

        wkhtmltoimage_global_settings* ig = image_create();
        snprintf(value, sizeof(value), "%s/modern.html", dataDir);
        image_set(ig, "in", value);
        image_set(ig, "out", "");
        image_set(ig, "fmt", "png");
        image_set(ig, "enableLocalFileAccess", "true");
        image_set(ig, "screenWidth", "800");
        image_set(ig, "screenHeight", "600");
        wkhtmltoimage_converter* ic = image_create_converter(ig);
        const int iok = image_convert(ic);
        CHECK(iok == 1, "image conversion succeeded");
        const unsigned char* idata = NULL;
        const long isize = image_get_output(ic, &idata);
        CHECK(isize > 0 && idata != NULL, "image output available");
        CHECK(has_png_magic(idata, isize), "image output has PNG magic");
        image_destroy_converter(ic);
        image_destroy(ig);
    }

    if (failures == 0) {
        printf("All C API checks passed\n");
        return 0;
    }
    fprintf(stderr, "%d C API check(s) failed\n", failures);
    return 1;
}
