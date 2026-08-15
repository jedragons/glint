target("glint-addon-nuklear", function()
	set_kind("static")
	set_languages("c++23")
	set_warnings("allextra")

	add_deps({
		"glint-addon",
		"glint-core",
		"glint-addon-dim2",
		"raylib-nuklear",
	}, {
		public = true,
	})

	add_files("./src/**.cpp")
	add_includedirs("include", { public = true })
end)
