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
-- dir. All of glint's dependencies (raylib, sol2, fmt, lua, entt, nlohmann
-- json, wtr) are vendored under ./vendor/ and built as part
-- of the same CMake tree, so `cmake --install` already produces every
-- lib/*.a and include/ header glint needs -- nothing is fetched from
-- xmake-repo, and consumers don't need any add_deps() beyond "cmake" itself.

package("glint")
    set_sourcedir(os.scriptdir())
    set_kind("library")

    add_deps("cmake")

    -- The whole engine builds as a single library (libglint.a), so there are
    -- no cross-library ordering concerns anymore. Keep the third-party libs
    -- in a group so their order doesn't matter either.
    add_syslinks(
        "glint",
        "raylib",
        "fmt",
        "fmt-c",
        "lua",
        {group = true}
    )

    -- raylib links X11 PRIVATEly against its own CMake target, which does
    -- not propagate to consumers of a flat prebuilt libraylib.a -- so
    -- redeclare it here for platforms where raylib actually needs it.
    if is_plat("linux") then
        add_syslinks("X11")
    end

    on_install(function (package)
        local configs = {}
        table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:debug() and "Debug" or "Release"))
        import("package.tools.cmake").install(package, configs)
    end)
