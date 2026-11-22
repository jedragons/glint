#include "./logging.h"
#include "./raylib_internal.h"

static void graylib__log(int level, const char* format, va_list args);

void graylib_init_logging(void) {
    SetTraceLogCallback(graylib__log);
    SetTraceLogLevel(LOG_ALL);
}

static void graylib__log(int level, const char* format, va_list args) {
    glog_Level mapped;
    switch (level) {
        case LOG_TRACE:
            mapped = GLOG_TRACE;
            break;
        case LOG_DEBUG:
            mapped = GLOG_DEBUG;
            break;
        case LOG_INFO:
            mapped = GLOG_INFO;
            break;
        case LOG_WARNING:
            mapped = GLOG_WARN;
            break;
        case LOG_ERROR:
            mapped = GLOG_ERROR;
            break;
        case LOG_FATAL:
            mapped = GLOG_FATAL;
            break;
        default:
            return;
    }

    glog_vlog(mapped, format, args);
}
