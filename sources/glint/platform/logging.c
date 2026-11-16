#include "./logging.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static glog_Level glog__level = GLOG_INFO;
static glog_Callback glog__callback;
static void* glog__user;

static const char* glog__names[] = {
    [GLOG_TRACE] = "TRACE",
    [GLOG_DEBUG] = "DEBUG",
    [GLOG_INFO] = "INFO",
    [GLOG_WARN] = "WARN",
    [GLOG_ERROR] = "ERROR",
    [GLOG_FATAL] = "FATAL",
};

void glog_set_level(glog_Level level) {
    if (level > GLOG_DEFAULT && level <= GLOG_FATAL) {
        glog__level = level;
    }
}

glog_Level glog_get_level(void) {
    return glog__level;
}

void glog_set_callback(glog_Callback callback, void* user) {
    glog__callback = callback;
    glog__user = user;
}

void glog_log(glog_Level level, GLINT_PRINTF_STRING const char* format, ...) {
    va_list args;
    va_start(args, format);
    glog_vlog(level, format, args);
    va_end(args);
}

void glog_vlog(glog_Level level, GLINT_PRINTF_STRING const char* format, va_list args) {
    bool valid_level = level >= GLOG_TRACE && level <= GLOG_FATAL;
    bool enabled = glog__level != GLOG_NONE && level >= glog__level;
    if (!valid_level || !enabled || format == NULL) {
        return;
    }

    if (glog__callback != NULL) {
        glog__callback(level, format, args, glog__user);
        return;
    }

    fprintf(stderr, "%s: ", glog__names[level]);
    vfprintf(stderr, format, args);
    fputc('\n', stderr);
}

void glog_trace(GLINT_PRINTF_STRING const char* format, ...) {
    va_list args;
    va_start(args, format);
    glog_vlog(GLOG_TRACE, format, args);
    va_end(args);
}

void glog_debug(GLINT_PRINTF_STRING const char* format, ...) {
    va_list args;
    va_start(args, format);
    glog_vlog(GLOG_DEBUG, format, args);
    va_end(args);
}

void glog_info(GLINT_PRINTF_STRING const char* format, ...) {
    va_list args;
    va_start(args, format);
    glog_vlog(GLOG_INFO, format, args);
    va_end(args);
}

void glog_warn(GLINT_PRINTF_STRING const char* format, ...) {
    va_list args;
    va_start(args, format);
    glog_vlog(GLOG_WARN, format, args);
    va_end(args);
}

void glog_error(GLINT_PRINTF_STRING const char* format, ...) {
    va_list args;
    va_start(args, format);
    glog_vlog(GLOG_ERROR, format, args);
    va_end(args);
}

void glog_fatal(GLINT_PRINTF_STRING const char* format, ...) {
    va_list args;
    va_start(args, format);
    glog_vlog(GLOG_ERROR, format, args);
    va_end(args);
    abort();
}
