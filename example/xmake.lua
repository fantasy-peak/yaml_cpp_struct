set_project("example")
set_version("0.0.1", {build = "%Y%m%d%H%M"})
set_xmakever("2.7.8")

add_repositories("my_repo https://github.com/fantasy-peak/xmake-repo.git")
add_requires("spdlog", "yaml_cpp_struct main")

set_languages("c++23")
set_policy("check.auto_ignore_flags", false)
add_cxflags("-O2 -Wall -Wextra -Werror -pedantic-errors -Wno-missing-field-initializers -Wno-ignored-qualifiers")

target("example")
    set_kind("binary")
    add_files("main.cpp")
    add_packages("spdlog", "yaml_cpp_struct")
target_end()
