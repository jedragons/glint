target("glint", function()
	set_kind("static")
	set_languages("c++23")
	set_warnings("allextra")

	add_deps({
		"glint-addon-audio",
		"glint-addon-dim2",
		"glint-addon-dim3",
		"glint-addon-nuklear",
		"glint-core",
		"glint-vfs",
	}, {
		public = true,
	})

	add_packages({
		"entt",
		"nlohmann_json",
		"raylib-cpp",
		"sol2",
	}, {
		public = true,
	})

	add_files("src/**.cpp")
	add_includedirs("include", { public = true })

	-- add_cxflags("-fPIC", { force = true, tools = "gcc" })
	-- add_cxflags("-fPIC", { force = true, tools = "clang" })
end)
