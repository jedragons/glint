target("glint-addon", function()
	set_kind("headeronly")
	set_languages("c++23")
	set_warnings("allextra")

	add_packages({
		"entt",
		"nlohmann_json",
		"raylib-cpp",
		"sol2",
	}, {
		public = true,
	})

	add_includedirs("include", { public = true })
end)
