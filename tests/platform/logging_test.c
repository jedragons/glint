#include <glint/modules/script/bindings.h>
#include <glint/modules/window/bindings.h>
#include <glint/platform/logging.h>
#include <glint/unit/unit.h>

#include <stdio.h>
#include <string.h>

typedef struct glog__TestSink {
    glog_Level level;
    size_t count;
    int length;
    char message[8192];
} glog__TestSink;

static void glog__test_capture(glog_Level level, const char* format, va_list args, void* user);

typedef struct glog__TestContext {
    glog__TestSink sink;
    glog_Level previous_level;
    galloc_Allocator alloc;
    gscript_lua_Runtime* script;
    gecs_World* world;
} glog__TestContext;

static bool glog__test_create(glog__TestContext* context, bool script);
static void glog__test_free(void* user);
static bool glog__test_lua(glog__TestContext* context, const char* source);

GUNIT_TEST(logging_trace_helper,
    glog__TestContext context = {0};
    gunit_defer(glog__test_free, &context);
    gunit_assert("create logging fixture", glog__test_create(&context, false));
    glog_trace("trace %d", 1);
    gunit_assert("severity forwarded", context.sink.level == GLOG_TRACE && context.sink.count == 1);
    gunit_assert("message formatted", strcmp(context.sink.message, "trace 1") == 0);
)

GUNIT_TEST(logging_debug_helper,
    glog__TestContext context = {0};
    gunit_defer(glog__test_free, &context);
    gunit_assert("create logging fixture", glog__test_create(&context, false));
    glog_debug("debug %d", 2);
    gunit_assert("severity forwarded", context.sink.level == GLOG_DEBUG && context.sink.count == 1);
    gunit_assert("message formatted", strcmp(context.sink.message, "debug 2") == 0);
)

GUNIT_TEST(logging_info_helper,
    glog__TestContext context = {0};
    gunit_defer(glog__test_free, &context);
    gunit_assert("create logging fixture", glog__test_create(&context, false));
    glog_info("info %s", "100%");
    gunit_assert("severity forwarded", context.sink.level == GLOG_INFO && context.sink.count == 1);
    gunit_assert("message formatted", strcmp(context.sink.message, "info 100%") == 0);
)

GUNIT_TEST(logging_warn_helper,
    glog__TestContext context = {0};
    gunit_defer(glog__test_free, &context);
    gunit_assert("create logging fixture", glog__test_create(&context, false));
    glog_warn("warning");
    gunit_assert("severity forwarded", context.sink.level == GLOG_WARN && context.sink.count == 1);
    gunit_assert("message formatted", strcmp(context.sink.message, "warning") == 0);
)

GUNIT_TEST(logging_error_helper,
    glog__TestContext context = {0};
    gunit_defer(glog__test_free, &context);
    gunit_assert("create logging fixture", glog__test_create(&context, false));
    glog_error("error");
    gunit_assert("severity forwarded", context.sink.level == GLOG_ERROR && context.sink.count == 1);
    gunit_assert("message formatted", strcmp(context.sink.message, "error") == 0);
)

GUNIT_TEST(logging_default_preserves_threshold,
    glog__TestContext context = {0};
    gunit_defer(glog__test_free, &context);
    gunit_assert("create logging fixture", glog__test_create(&context, false));
    glog_set_level(GLOG_WARN);
    glog_set_level(GLOG_DEFAULT);
    gunit_assert("default preserves configured level", glog_get_level() == GLOG_WARN);
)

GUNIT_TEST(logging_filters_below_threshold,
    glog__TestContext context = {0};
    gunit_defer(glog__test_free, &context);
    gunit_assert("create logging fixture", glog__test_create(&context, false));
    glog_set_level(GLOG_WARN);
    glog_info("filtered");
    gunit_assert("below threshold message filtered", context.sink.count == 0);
)

GUNIT_TEST(logging_threshold_is_inclusive,
    glog__TestContext context = {0};
    gunit_defer(glog__test_free, &context);
    gunit_assert("create logging fixture", glog__test_create(&context, false));
    glog_set_level(GLOG_WARN);
    glog_warn("accepted");
    gunit_assert(
        "threshold message forwarded",
        context.sink.count == 1 && strcmp(context.sink.message, "accepted") == 0
    );
)

GUNIT_TEST(logging_none_suppresses_fatal_severity,
    glog__TestContext context = {0};
    gunit_defer(glog__test_free, &context);
    gunit_assert("create logging fixture", glog__test_create(&context, false));
    glog_set_level(GLOG_NONE);
    glog_log(GLOG_FATAL, "filtered fatal severity");
    gunit_assert("none suppresses every severity", context.sink.count == 0);
)

GUNIT_TEST(logging_callback_message_is_not_truncated,
    glog__TestContext context = {0};
    gunit_defer(glog__test_free, &context);
    gunit_assert("create logging fixture", glog__test_create(&context, false));
    char message[4096];
    memset(message, 'x', sizeof(message) - 1);
    message[sizeof(message) - 1] = '\0';
    glog_info("%s", message);
    gunit_assert("callback receives full message", strcmp(context.sink.message, message) == 0);
)

GUNIT_TEST(logging_lua_growing_buffer_preserves_full_message,
    glog__TestContext context = {0};
    gunit_defer(glog__test_free, &context);
    gunit_assert("create logging fixture", glog__test_create(&context, true));
    gunit_assert("run logger script", glog__test_lua(&context, "log_info(string.rep('x', 16384), '100%')"));
    gunit_assert("script logger reaches Glint", context.sink.count == 1 && context.sink.level == GLOG_INFO);
    gunit_assert("Lua buffering preserves full message", context.sink.length == 16389);
)

GUNIT_TEST(logging_world_and_window_registration_preserve_loggers,
    glog__TestContext context = {0};
    gunit_defer(glog__test_free, &context);
    gunit_assert("create logging fixture", glog__test_create(&context, true));
    gunit_assert(
        "run logger script",
        glog__test_lua(&context, "log_functions = {log_trace, log_debug, log_info, log_warn, log_error, log_fatal}")
    );
    context.world = gecs_World_new(context.alloc);
    gunit_assert("create world", context.world != NULL);
    gunit_assert("bind world", gscript_bindings_bind_world(context.script, context.world));
    gunit_assert("bind window", gwindow_bindings_install(context.alloc, gscript_lua_state(context.script), NULL, NULL));
    gunit_assert(
        "run logger script",
        glog__test_lua(
            &context,
            "local current = {log_trace, log_debug, log_info, log_warn, log_error, log_fatal}; for i=1,6 do assert(current[i] == log_functions[i]) end"
        )
    );
)

GUNIT_TEST(logging_lua_trace,
    glog__TestContext context = {0};
    gunit_defer(glog__test_free, &context);
    gunit_assert("create logging fixture", glog__test_create(&context, true));
    gunit_assert("run logger script", glog__test_lua(&context, "log_trace('trace')"));
    gunit_assert("Lua severity forwarded", context.sink.level == GLOG_TRACE && context.sink.count == 1);
    gunit_assert("Lua arguments converted", strcmp(context.sink.message, "trace") == 0);
)

GUNIT_TEST(logging_lua_debug,
    glog__TestContext context = {0};
    gunit_defer(glog__test_free, &context);
    gunit_assert("create logging fixture", glog__test_create(&context, true));
    gunit_assert("run logger script", glog__test_lua(&context, "log_debug('debug')"));
    gunit_assert("Lua severity forwarded", context.sink.level == GLOG_DEBUG && context.sink.count == 1);
    gunit_assert("Lua arguments converted", strcmp(context.sink.message, "debug") == 0);
)

GUNIT_TEST(logging_lua_info,
    glog__TestContext context = {0};
    gunit_defer(glog__test_free, &context);
    gunit_assert("create logging fixture", glog__test_create(&context, true));
    gunit_assert(
        "run logger script",
        glog__test_lua(
            &context,
            "log_info('100% safe', 42, true, nil, setmetatable({}, {__tostring = function() return 'custom' end}))"
        )
    );
    gunit_assert("Lua severity forwarded", context.sink.level == GLOG_INFO && context.sink.count == 1);
    gunit_assert("Lua arguments converted", strcmp(context.sink.message, "100% safe 42 true nil custom") == 0);
)

GUNIT_TEST(logging_lua_warn,
    glog__TestContext context = {0};
    gunit_defer(glog__test_free, &context);
    gunit_assert("create logging fixture", glog__test_create(&context, true));
    gunit_assert("run logger script", glog__test_lua(&context, "log_warn('warning')"));
    gunit_assert("Lua severity forwarded", context.sink.level == GLOG_WARN && context.sink.count == 1);
    gunit_assert("Lua arguments converted", strcmp(context.sink.message, "warning") == 0);
)

GUNIT_TEST(logging_lua_error,
    glog__TestContext context = {0};
    gunit_defer(glog__test_free, &context);
    gunit_assert("create logging fixture", glog__test_create(&context, true));
    gunit_assert("run logger script", glog__test_lua(&context, "log_error('error')"));
    gunit_assert("Lua severity forwarded", context.sink.level == GLOG_ERROR && context.sink.count == 1);
    gunit_assert("Lua arguments converted", strcmp(context.sink.message, "error") == 0);
)

GUNIT_TEST(logging_lua_fatal,
    glog__TestContext context = {0};
    gunit_defer(glog__test_free, &context);
    gunit_assert("create logging fixture", glog__test_create(&context, true));
    gunit_assert(
        "run logger script",
        glog__test_lua(
            &context,
            "local ok, err=pcall(log_fatal, 'fatal'); assert(not ok and err:find('log_fatal called'))"
        )
    );
    gunit_assert("Lua severity forwarded", context.sink.level == GLOG_ERROR && context.sink.count == 1);
    gunit_assert("Lua arguments converted", strcmp(context.sink.message, "fatal") == 0);
)

gunit_main();

static bool glog__test_create(glog__TestContext* context, bool script) {
    context->previous_level = glog_get_level();
    context->alloc = galloc_malloc_allocator();
    glog_set_callback(glog__test_capture, &context->sink);
    glog_set_level(GLOG_TRACE);
    if (script) {
        context->script = gscript_lua_new(context->alloc);
        if (!gunit_check("create script runtime", context->script != NULL)) {
            return false;
        }
    }
    return true;
}

static void glog__test_free(void* user) {
    glog__TestContext* context = user;
    if (context->script != NULL) {
        gscript_lua_destroy(context->alloc, context->script);
    }
    if (context->world != NULL) {
        gecs_World_free(context->world);
    }
    glog_set_callback(NULL, NULL);
    glog_set_level(context->previous_level);
}

static bool glog__test_lua(glog__TestContext* context, const char* source) {
    if (!gunit_check(
            "Lua logger script succeeds",
            gscript_lua_run(context->alloc, context->script, source, strlen(source), "log")
        )) {
        return false;
    }
    return true;
}

static void glog__test_capture(glog_Level level, const char* format, va_list args, void* user) {
    glog__TestSink* sink = user;
    sink->level = level;
    sink->count += 1;
    sink->length = vsnprintf(sink->message, sizeof(sink->message), format, args);
}
