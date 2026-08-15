target("glint-addon-dim3", function()
	set_kind("static")
	set_languages("c++23")
	set_warnings("allextra")

	add_deps("glint-addon", { public = true })

	add_files("./src/**.cpp")
	add_includedirs("include", { public = true })
end)
