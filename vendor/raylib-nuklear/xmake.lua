target("raylib-nuklear", function()
	set_kind("static")
	set_languages("c++23")

	add_packages({
		"raylib",
	}, {
		public = true,
	})
	add_files("src/**.c")
	add_sysincludedirs("include", { public = true })
	if is_plat("iphoneos") then
		set_plat("wasm")
		set_arch("wasm32")
		set_toolchains("emcc")
	end
end)
