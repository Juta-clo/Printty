includes("deps/Linc/linc-targets.lua")

set_toolchains("clang")
set_toolset("cc", "clang")
set_toolset("ld", "clang")
set_policy("build.ccache", false)
add_rules("plugin.compile_commands.autoupdate", { outputdir = "." })

add_cflags("-g", "-O0", "-std=gnu23", "-Wall", "-Wextra")
add_includedirs("src", "test", "deps/Unity", "include")
add_defines("_GNU_SOURCE", "UNITY_OUTPUT_COLOR", "UNITY_FIXTURE_NO_EXTRAS")
add_requires("unibilium", "vterm", "pthread", { system = true })

target("test_all")
    set_kind("binary")
    set_targetdir(".")

    add_packages("vterm", "unibilium", "pthread");
    add_deps(linc_targets)
    add_packages("unibilium")
    add_files("src/*.c", "deps/Unity/*.c", "test/*.c")
