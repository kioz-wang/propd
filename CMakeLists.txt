cmake_minimum_required(VERSION 4.2.1) 
project(propd LANGUAGES C)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'RelWithDebInfo' as none was specified.")
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the type of build" FORCE)

    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(WITH_SANITIZERS "Enable sanitizers in Debug" ON)

set(CompileOptions-Debug -O0 -g3 -DDEBUG -D_DEBUG -fno-omit-frame-pointer)
set(CompileOptions-Release -O3 -DNDEBUG)
set(CompileOptions-RelWithDebInfo -O2 -g -DNDEBUG)
set(CompileOptions-MinSizeRel -Os -DNDEBUG -fdata-sections -ffunction-sections)
set(CompileOptions-Debug-Sanitizers -fsanitize=address,undefined -fsanitize-recover=address)

set(LinkOptions-MinSizeRel -Wl,--gc-sections)
set(LinkOptions-Debug-Sanitizers -fsanitize=address -fsanitize=address,undefined -fsanitize-address-use-after-scope)

add_compile_options(
    -Wall
    -Wextra

    "$<$<CONFIG:Debug>:${CompileOptions-Debug}>"
    "$<$<CONFIG:Release>:${CompileOptions-Release}>"
    "$<$<CONFIG:RelWithDebInfo>:${CompileOptions-RelWithDebInfo}>"
    "$<$<CONFIG:MinSizeRel>:${CompileOptions-MinSizeRel}>"
    "$<$<AND:$<CONFIG:Debug>,$<BOOL:${WITH_SANITIZERS}>>:${CompileOptions-Debug-Sanitizers}>"
)

add_link_options(
    -Wl,--as-needed

    "$<$<CONFIG:MinSizeRel>:${LinkOptions-MinSizeRel}>"
    "$<$<AND:$<CONFIG:Debug>,$<BOOL:${WITH_SANITIZERS}>>:${LinkOptions-Debug-Sanitizers}>"
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_subdirectory(lib)
add_subdirectory(app)
