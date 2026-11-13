#ifndef GLOG_LOG_H_
#define GLOG_LOG_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Severity-ordered log levels shared by the engine, its Lua bindings, and
 * raylib. GLOG_DEFAULT keeps raylib's own level; the remaining values
 * match raylib's TraceLogLevel, which performs the filtering and output. */
typedef enum glog_Level {
    GLOG_DEFAULT = 0,
    GLOG_TRACE = 1,
    GLOG_DEBUG = 2,
    GLOG_INFO = 3,
    GLOG_WARNING = 4,
    GLOG_ERROR = 5,
    GLOG_FATAL = 6,
    GLOG_NONE = 7,
} glog_Level;

/* raylib owns logging and allocates nothing here. */
void glog_set_level(glog_Level level);

#ifdef __cplusplus
}
#endif
#endif // GLOG_LOG_H_
