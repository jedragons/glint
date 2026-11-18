#include <glint/modules/script/bindings.h>
#include <glint/modules/window/bindings.h>
#include <glint/platform/logging.h>
#include <glint/unit/unit.h>

#include <stdio.h>
#include <string.h>

typedef struct glog_test__Sink {
    glog_Level level;
    size_t count;
    int length;
    char message[8192];
} glog_test__Sink;

static void glog_test__capture(glog_Level level, const char* format, va_list args, void* user);

typedef struct glog_test__Context {
    glog_test__Sink sink;
    glog_Level previous_level;
    galloc_Allocator alloc;
    gscript_lua_Runtime* script;
    gecs_World* world;
} glog_test__Context;

static bool glog_test__create(glog_test__Context* context, bool script);
static void glog_test__free(void* user);
static bool glog_test__lua(glog_test__Context* context, const char* source);

GUNIT_TEST(logging_native_helpers,
    glog_test__Context context = {0};

    GUNIT_SETUP({
        gunit_defer(glog_test__free, &context);
        gunit_assert("create logging fixture", glog_test__create(&context, false));
    })

    GUNIT_SECTION("trace forwards trace severity", {
        glog_trace("trace %d", 1);
        gunit_assert("severity forwarded", context.sink.level == GLOG_TRACE && context.sink.count == 1);
        gunit_assert("message formatted", strcmp(context.sink.message, "trace 1") == 0);
    })

    GUNIT_SECTION("debug forwards debug severity", {
        glog_debug("debug %d", 2);
        gunit_assert("severity forwarded", context.sink.level == GLOG_DEBUG && context.sink.count == 1);
        gunit_assert("message formatted", strcmp(context.sink.message, "debug 2") == 0);
    })

    GUNIT_SECTION("info preserves percent signs in string arguments", {
        glog_info("info %s", "100%");
        gunit_assert("severity forwarded", context.sink.level == GLOG_INFO && context.sink.count == 1);
        gunit_assert("message formatted", strcmp(context.sink.message, "info 100%") == 0);
    })

    GUNIT_SECTION("warn forwards warning severity", {
        glog_warn("warning");
        gunit_assert("severity forwarded", context.sink.level == GLOG_WARN && context.sink.count == 1);
        gunit_assert("message formatted", strcmp(context.sink.message, "warning") == 0);
    })

    GUNIT_SECTION("error forwards error severity", {
        glog_error("error");
        gunit_assert("severity forwarded", context.sink.level == GLOG_ERROR && context.sink.count == 1);
        gunit_assert("message formatted", strcmp(context.sink.message, "error") == 0);
    })

    GUNIT_SECTION("the callback receives the full length of a long message", {
        char message[4096];
        memset(message, 'x', sizeof(message) - 1);
        message[sizeof(message) - 1] = '\0';
        glog_info("%s", message);
        gunit_assert("callback receives full message", strcmp(context.sink.message, message) == 0);
    })
)

GUNIT_TEST(logging_threshold,
    glog_test__Context context = {0};

    GUNIT_SETUP({
        gunit_defer(glog_test__free, &context);
        gunit_assert("create logging fixture", glog_test__create(&context, false));
        glog_set_level(GLOG_WARN);
    })

    GUNIT_SECTION("the default preserves the configured threshold", {
        glog_set_level(GLOG_DEFAULT);
        gunit_assert("default preserves configured level", glog_get_level() == GLOG_WARN);
    })

    GUNIT_SECTION("messages below the threshold are filtered", {
        glog_info("filtered");
        gunit_assert("below threshold message filtered", context.sink.count == 0);
    })

    GUNIT_SECTION("messages at the threshold reach the sink", {
        glog_warn("accepted");
        gunit_assert(
            "threshold message forwarded",
            context.sink.count == 1 && strcmp(context.sink.message, "accepted") == 0
        );
    })

    GUNIT_SECTION("disabling logging suppresses even fatal severity", {
        glog_set_level(GLOG_NONE);
        glog_log(GLOG_FATAL, "filtered fatal severity");
        gunit_assert("none suppresses every severity", context.sink.count == 0);
    })
)

GUNIT_TEST(logging_lua_helpers,
    glog_test__Context context = {0};

    GUNIT_SETUP({
        gunit_defer(glog_test__free, &context);
        gunit_assert("create logging fixture", glog_test__create(&context, true));
    })

    GUNIT_SECTION("trace forwards trace severity", {
        gunit_assert("call Lua logger", glog_test__lua(&context, "log_trace('trace')"));
        gunit_assert("Lua severity forwarded", context.sink.level == GLOG_TRACE && context.sink.count == 1);
        gunit_assert("Lua arguments converted", strcmp(context.sink.message, "trace") == 0);
    })

    GUNIT_SECTION("debug forwards debug severity", {
        gunit_assert("call Lua logger", glog_test__lua(&context, "log_debug('debug')"));
        gunit_assert("Lua severity forwarded", context.sink.level == GLOG_DEBUG && context.sink.count == 1);
        gunit_assert("Lua arguments converted", strcmp(context.sink.message, "debug") == 0);
    })

    GUNIT_SECTION("info converts values to strings and preserves percent signs", {
        gunit_assert(
            "call Lua logger with mixed value types",
            glog_test__lua(
                &context,
                "local custom = setmetatable({}, {__tostring = function() return 'custom' end})\n"
                "log_info('100% safe', 42, true, nil, custom)"
            )
        );
        gunit_assert("Lua severity forwarded", context.sink.level == GLOG_INFO && context.sink.count == 1);
        gunit_assert("Lua arguments converted", strcmp(context.sink.message, "100% safe 42 true nil custom") == 0);
    })

    GUNIT_SECTION("warn forwards warning severity", {
        gunit_assert("call Lua logger", glog_test__lua(&context, "log_warn('warning')"));
        gunit_assert("Lua severity forwarded", context.sink.level == GLOG_WARN && context.sink.count == 1);
        gunit_assert("Lua arguments converted", strcmp(context.sink.message, "warning") == 0);
    })

    GUNIT_SECTION("error forwards error severity", {
        gunit_assert("call Lua logger", glog_test__lua(&context, "log_error('error')"));
        gunit_assert("Lua severity forwarded", context.sink.level == GLOG_ERROR && context.sink.count == 1);
        gunit_assert("Lua arguments converted", strcmp(context.sink.message, "error") == 0);
    })

    GUNIT_SECTION("fatal reaches the error sink and raises a Lua error", {
        gunit_assert(
            "Lua fatal logger raises the expected error",
            glog_test__lua(
                &context,
                "local ok, err = pcall(log_fatal, 'fatal')\n"
                "assert(not ok and err:find('log_fatal called'))"
            )
        );
        gunit_assert("Lua severity forwarded", context.sink.level == GLOG_ERROR && context.sink.count == 1);
        gunit_assert("Lua arguments converted", strcmp(context.sink.message, "fatal") == 0);
    })

    GUNIT_SECTION("the callback receives the full length of a long Lua message", {
        gunit_assert("call Lua logger", glog_test__lua(&context, "log_info(string.rep('x', 16384), '100%')"));
        gunit_assert("script logger reaches Glint", context.sink.count == 1 && context.sink.level == GLOG_INFO);
        gunit_assert("Lua buffering preserves full message", context.sink.length == 16389);
    })
)

GUNIT_TEST(logging_world_and_window_registration_preserve_loggers,
    glog_test__Context context = {0};
    gunit_defer(glog_test__free, &context);
    gunit_assert("create Lua logging fixture", glog_test__create(&context, true));
    gunit_assert(
        "record original Lua log functions",
        glog_test__lua(&context, "log_functions = {log_trace, log_debug, log_info, log_warn, log_error, log_fatal}")
    );

    context.world = gecs_World_new(context.alloc);
    gunit_assert("create world", context.world != NULL);
    gunit_assert("bind world", gscript_bindings_bind_world(context.script, context.world));
    gunit_assert("bind window", gwindow_bindings_install(context.alloc, gscript_lua_state(context.script), NULL, NULL));

    gunit_assert(
        "world and window bindings preserve the original Lua log functions",
        glog_test__lua(
            &context,
            "local current = {log_trace, log_debug, log_info, log_warn, log_error, log_fatal}\n"
            "for i = 1, 6 do assert(current[i] == log_functions[i]) end"
        )
    );
)

gunit_main();

static bool glog_test__create(glog_test__Context* context, bool script) {
    context->previous_level = glog_get_level();
    context->alloc = galloc_malloc_allocator();
    glog_set_callback(glog_test__capture, &context->sink);
    glog_set_level(GLOG_TRACE);
    if (script) {
        context->script = gscript_lua_new(context->alloc);
        if (!gunit_check("create script runtime", context->script != NULL)) {
            return false;
        }
    }
    return true;
}

static void glog_test__free(void* user) {
    glog_test__Context* context = user;
    if (context->script != NULL) {
        gscript_lua_destroy(context->alloc, context->script);
    }
    if (context->world != NULL) {
        gecs_World_free(context->world);
    }
    glog_set_callback(NULL, NULL);
    glog_set_level(context->previous_level);
}

static bool glog_test__lua(glog_test__Context* context, const char* source) {
    return gscript_lua_run(context->alloc, context->script, source, strlen(source), "log");
}

static void glog_test__capture(glog_Level level, const char* format, va_list args, void* user) {
    glog_test__Sink* sink = user;
    sink->level = level;
    sink->count += 1;
    sink->length = vsnprintf(sink->message, sizeof(sink->message), format, args);
}
