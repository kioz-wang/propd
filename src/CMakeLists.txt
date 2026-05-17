add_compile_options(
    -fPIC
)

add_link_options(
    -shared
    -pthread
)

# ── shared internal headers ──────────────────────────────────────────

# ── vendor sources ────────────────────────────────────────────────────
file(GLOB_RECURSE VENDOR_MLOGGER_SOURCES CONFIGURE_DEPENDS
    ../vendor/mlogger/*.c
)
file(GLOB_RECURSE VENDOR_MEMIO_SOURCES CONFIGURE_DEPENDS
    ../vendor/memio/*.c
)

# ── libprop (thin client library) ─────────────────────────────────────
file(GLOB_RECURSE LIBPROP_SOURCES CONFIGURE_DEPENDS
    client/*.c client/**/*.c
)

message(STATUS "libprop source list => ${LIBPROP_SOURCES}")

add_library(libprop SHARED ${LIBPROP_SOURCES} ${VENDOR_MLOGGER_SOURCES} ${VENDOR_MEMIO_SOURCES})
target_include_directories(libprop
    PUBLIC  ../include                      # users: #include <propd/prop.h>
    PRIVATE ../include/propd                # internal: #include "prop.h" (legacy)
    PRIVATE .                               # shared/
    PRIVATE client                          # global.h, unix_stream.h
    PRIVATE ../vendor/mlogger               # logger.h
    PRIVATE ../vendor                       # memio/layout.h
)
target_compile_definitions(libprop PRIVATE _GNU_SOURCE)
target_link_libraries(libprop cjson)

# ── libpropd (full server library) ────────────────────────────────────
file(GLOB_RECURSE LIBPROPD_SOURCES CONFIGURE_DEPENDS
    server/*.c server/**/*.c
)

message(STATUS "libpropd source list => ${LIBPROPD_SOURCES}")

add_library(libpropd SHARED ${LIBPROPD_SOURCES})
target_include_directories(libpropd
    PUBLIC  ../include                      # users: #include <propd/propd.h>
    PRIVATE ../include/propd                # internal: #include "propd.h" (legacy)
    PRIVATE .                               # shared/
    PRIVATE server                          # ctrl.h, io.h, cache.h, route.h, io_context.h
    PRIVATE server/infra                    # thread_pool.h, named_mutex.h
    PRIVATE client                          # global.h, misc.h, unix_stream.h (reuse from libprop)
    PRIVATE ../vendor/mlogger               # logger.h
    PRIVATE ../vendor                       # memio/layout.h
)
target_compile_definitions(libpropd PRIVATE _GNU_SOURCE)
target_link_libraries(libpropd PRIVATE libprop cjson)
