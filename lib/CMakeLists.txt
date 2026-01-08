add_compile_options(
    -fPIC
)

add_link_options(
    -shared
    -pthread
)

file(GLOB_RECURSE PROPD_SOURCES CONFIGURE_DEPENDS
    *.c **/*.c
)

message(STATUS "source list => ${PROPD_SOURCES}")

add_library(libpropd SHARED ${PROPD_SOURCES})
target_include_directories(libpropd PRIVATE .)
target_include_directories(libpropd PUBLIC include)
target_include_directories(libpropd PUBLIC mlogger)
target_link_libraries(libpropd cjson)
