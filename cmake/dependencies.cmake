include(FetchContent)

# Some third-party projects still declare a minimum CMake version below 3.5,
# which CMake 4 refuses by default.  Opt into the compatibility behaviour when
# the variable is available.
if(CMAKE_VERSION VERSION_GREATER_EQUAL 4.0)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

# yaml-cpp 0.9.0 is the first release that builds with recent GCC versions
# (0.8.0 and older miss <cstdint> in emitterutils.cpp).
FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG yaml-cpp-0.9.0
)
FetchContent_MakeAvailable(yaml-cpp)

FetchContent_Declare(
    magic_enum
    GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
    GIT_TAG 369642f2b0ab2288bd2df1a977e1ee053b54eba2
)
FetchContent_MakeAvailable(magic_enum)

FetchContent_Declare(
    visit_struct
    GIT_REPOSITORY https://github.com/garbageslam/visit_struct.git
    GIT_TAG 8c91d2283c7050593df5b692a13cb0ea99ba81d5
)
FetchContent_MakeAvailable(visit_struct)
