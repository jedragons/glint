target("glint-core", function()
	set_kind("static")
	set_languages("c++23")
	set_warnings("allextra")

	add_deps({
		"glint-addon",
		"glint-vfs",
	}, {
		public = true,
	})

	add_packages({
		"entt",
		"raylib-cpp",
		"sol2",
		"fmt",
	}, {
		public = true,
	})

	add_files("./src/**.cpp")
	add_includedirs("include", { public = true })

	if is_mode("debug") then
		add_defines("GLINT_HOT_RELOAD_ASSETS=1", { public = true })
		add_packages({ "watcher" }, { public = true })
	else
		add_defines("GLINT_HOT_RELOAD_ASSETS=0", { public = true })
	end
end)
