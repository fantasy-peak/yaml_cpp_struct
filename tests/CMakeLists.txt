cmake_minimum_required(VERSION 3.14)

project(yaml_cpp_struct_tests LANGUAGES CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/dependencies.cmake)

FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

find_package(Threads REQUIRED)

function(ycs_add_test target source)
    add_executable(${target} ${source})
    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include)
    target_link_libraries(${target} PRIVATE
        yaml-cpp
        magic_enum
        nlohmann_json::nlohmann_json
        GTest::gtest_main
        Threads::Threads)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -pedantic-errors
            -Wno-missing-field-initializers
            -Wno-ignored-qualifiers)
    endif()
endfunction()

ycs_add_test(unit_tests test_yaml_cpp_struct.cpp)
target_compile_definitions(unit_tests PRIVATE OPEN_YAML_TO_JSON)

enable_testing()
add_test(NAME unit_tests COMMAND unit_tests)
