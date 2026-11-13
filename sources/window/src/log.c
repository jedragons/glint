#include <glint/log.h>

#include <raylib.h>

void glog_set_level(glog_Level level) {
    SetTraceLogLevel((int)level);
}
