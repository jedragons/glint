target("glint-vfs", function()
	set_kind("static")
	set_languages("c++23")
	set_warnings("allextra")

	add_files("./src/**.cpp")
	add_includedirs("include", { public = true })

	set_symbols("debug")
end)
