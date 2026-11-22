-- Thin wrapper exposing this repository's CMake build (see CMakeLists.txt)
-- as an xmake package, so xmake-based projects can consume it via:
--
--   includes("path/to/glint")
--   add_requires("glint")
--
--   target("myapp")
--       add_packages("glint")
--
-- The actual build is still owned by CMake; this only drives it with
-- `import("package.tools.cmake")` and installs its outputs into the package
-- dir. All of glint's dependencies (lua, raylib) are vendored under
-- ./vendor/ and built as part of the same C-only CMake tree, so nothing is
-- fetched from xmake-repo, and consumers don't need any add_deps() beyond
-- "cmake" itself.
--
-- CMake installs the engine, dependencies, and public headers.

package("glint")
    set_sourcedir(os.scriptdir())
    set_kind("library")

    add_deps("cmake")

    -- Flat static archives: the engine first, then its vendored deps as a
    -- group so their order doesn't matter.
    add_syslinks(
        "glint",
        "raylib",
        "lua",
        {group = true}
    )

    -- raylib links X11 PRIVATEly against its own CMake target, which does
    -- not propagate to consumers of a flat prebuilt libraylib.a -- so
    -- redeclare it here for platforms where raylib actually needs it.
    -- libm is linked PUBLICly by the engine's CMake target.
    if is_plat("linux") then
        add_syslinks("X11", "GL", "pthread", "dl", "m")
    elseif is_plat("macosx") then
        add_frameworks("Cocoa", "IOKit", "CoreFoundation", "OpenGL")
    elseif is_plat("windows", "mingw") then
        add_syslinks("opengl32", "gdi32", "winmm")
    end

    on_install(function (package)
        import("package.tools.cmake")
        local configs = {}
        table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:debug() and "Debug" or "Release"))

        cmake.build(package, configs, {target = "glint"})
        cmake.install(package, configs)
    end)
