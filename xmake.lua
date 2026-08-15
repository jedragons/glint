set_project("Alienwave")

add_rules("mode.debug", "mode.release", "mode.minsizerel", "mode.check", "mode.profile")
add_rules("plugin.compile_commands.autoupdate", { outputdir = "build" })

includes("./xmake/format.lua")

add_requires("entt")
add_requires("fmt")
add_requires("nlohmann_json")
add_requires("raylib")
add_requires("raylib-cpp")
add_requires("sol2")

if is_mode("debug") then
	add_requires("watcher")
end

includes("./sources/addon-audio/")
includes("./sources/addon-dim2/")
includes("./sources/addon-dim3/")
includes("./sources/addon-nuklear/")
includes("./sources/addon/")
includes("./sources/core/")
includes("./sources/glint/")
includes("./sources/vfs/")

includes("./vendor/raylib-nuklear/")
