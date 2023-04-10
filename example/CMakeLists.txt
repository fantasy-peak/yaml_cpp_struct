cmake_minimum_required(VERSION 3.14)

project(example)

include("FetchContent")

set(CMAKE_CXX_STANDARD 11)

add_compile_options(
    -Wall
    -Wunused-parameter
    -gdwarf-2 -O2
    -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free
    -Wextra -Wno-missing-field-initializers -Wno-ignored-qualifiers -pedantic-errors
)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.x
)
FetchContent_GetProperties(spdlog)
if (NOT spdlog_POPULATED)
    FetchContent_Populate(spdlog)
    add_subdirectory(${spdlog_SOURCE_DIR} ${spdlog_BINARY_DIR})
endif()

FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG yaml-cpp-0.7.0
)
FetchContent_MakeAvailable(yaml-cpp)

FetchContent_Declare(
    visit_struct
    GIT_REPOSITORY https://github.com/garbageslam/visit_struct.git
    GIT_TAG 8c91d2283c7050593df5b692a13cb0ea99ba81d5
)
FetchContent_MakeAvailable(visit_struct)

FetchContent_Declare(
    optional
    GIT_REPOSITORY https://github.com/TartanLlama/optional.git
    GIT_TAG 3a1209de8370bf5fe16362934956144b49591565
)
FetchContent_MakeAvailable(optional)

FetchContent_Declare(
    yaml_cpp_struct
    GIT_REPOSITORY https://github.com/fantasy-peak/yaml_cpp_struct.git
    GIT_TAG cpp11
)
FetchContent_MakeAvailable(yaml_cpp_struct)

add_executable(example main.cpp)
target_include_directories(example PUBLIC ${CMAKE_BINARY_DIR}/_deps/visit_struct-src/include/;)
target_link_libraries(example
    yaml-cpp
    yaml_cpp_struct
    pthread
    spdlog::spdlog
    optional
)
