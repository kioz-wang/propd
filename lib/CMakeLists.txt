add_compile_options(
    -fPIC
)

add_link_options(
    -shared
    -pthread
)

# ── shared internal headers ──────────────────────────────────────────
include_directories(
    PUBLIC  ../include
    PRIVATE include
    PRIVATE ../vendor
)

# ── vendor sources ────────────────────────────────────────────────────
file(GLOB_RECURSE VENDOR_MLOGGER_SOURCES CONFIGURE_DEPENDS
    ../vendor/mlogger/*.c
)
file(GLOB_RECURSE VENDOR_MEMIO_SOURCES CONFIGURE_DEPENDS
    ../vendor/memio/*.c
)

# ── libprop (thin client library) ─────────────────────────────────────
file(GLOB_RECURSE LIBPROP_SOURCES CONFIGURE_DEPENDS
    prop/*.c prop/**/*.c
)

message(STATUS "libprop source list => ${LIBPROP_SOURCES}")

add_library(libprop SHARED ${LIBPROP_SOURCES} ${VENDOR_MLOGGER_SOURCES} ${VENDOR_MEMIO_SOURCES})
target_compile_definitions(libprop PRIVATE _GNU_SOURCE)
target_link_libraries(libprop cjson)

# ── libpropd (full server library) ────────────────────────────────────
file(GLOB_RECURSE LIBPROPD_SOURCES CONFIGURE_DEPENDS
    propd/*.c propd/**/*.c
)

message(STATUS "libpropd source list => ${LIBPROPD_SOURCES}")

add_library(libpropd SHARED ${LIBPROPD_SOURCES})
target_compile_definitions(libpropd PRIVATE _GNU_SOURCE)
target_link_libraries(libpropd PRIVATE libprop cjson)
