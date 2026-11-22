#pragma once

#include <glint/config.h>

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum glog_Level {
    GLOG_DEFAULT,
    GLOG_NONE,
    GLOG_TRACE,
    GLOG_DEBUG,
    GLOG_INFO,
    GLOG_WARN,
    GLOG_ERROR,
    GLOG_FATAL,
} glog_Level;

/// The callback receives a printf format and arguments valid only during the call.
typedef void (*glog_Callback)(
    glog_Level level,
    GLINT_PRINTF_STRING const char* format,
    va_list args,
    void* user
) GLINT_PRINTF_FORMAT(2, 0);

/// DEFAULT keeps the current threshold; NONE disables output. Initially INFO.
void glog_set_level(glog_Level level);

glog_Level glog_get_level(void);

/// NULL restores stderr output. Callback and user data are borrowed until replaced.
void glog_set_callback(glog_Callback callback, void* user);

/// Log using printf formatting. Only glog_fatal terminates the process.
void glog_log(glog_Level level, GLINT_PRINTF_STRING const char* format, ...) GLINT_PRINTF_FORMAT(2, 3);

void glog_vlog(glog_Level level, GLINT_PRINTF_STRING const char* format, va_list args) GLINT_PRINTF_FORMAT(2, 0);

void glog_trace(GLINT_PRINTF_STRING const char* format, ...) GLINT_PRINTF_FORMAT(1, 2);

void glog_debug(GLINT_PRINTF_STRING const char* format, ...) GLINT_PRINTF_FORMAT(1, 2);

void glog_info(GLINT_PRINTF_STRING const char* format, ...) GLINT_PRINTF_FORMAT(1, 2);

void glog_warn(GLINT_PRINTF_STRING const char* format, ...) GLINT_PRINTF_FORMAT(1, 2);

void glog_error(GLINT_PRINTF_STRING const char* format, ...) GLINT_PRINTF_FORMAT(1, 2);

/// Log at ERROR level, then abort, as in the C++ API.
void glog_fatal(GLINT_PRINTF_STRING const char* format, ...) GLINT_PRINTF_FORMAT(1, 2);

#ifdef __cplusplus
}
#endif
