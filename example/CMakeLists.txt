cmake_minimum_required(VERSION 3.14)

project(example LANGUAGES CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/dependencies.cmake)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.15.3
)
FetchContent_MakeAvailable(spdlog)

FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

find_package(Threads REQUIRED)

add_executable(example main.cpp)
target_include_directories(example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${visit_struct_SOURCE_DIR}/include)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(example PRIVATE
        -Wall
        -Wextra
        -pedantic-errors
        -Wno-missing-field-initializers
        -Wno-ignored-qualifiers)
endif()

target_link_libraries(example PRIVATE
    yaml-cpp
    magic_enum
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    Threads::Threads
)

enable_testing()
add_test(NAME example COMMAND example ${CMAKE_CURRENT_SOURCE_DIR}/config.yaml)
