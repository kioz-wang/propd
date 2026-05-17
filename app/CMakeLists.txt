add_executable(prop prop/main.c)
target_link_libraries(prop PRIVATE libprop)

add_executable(propd propd/main.c)
target_link_libraries(propd PRIVATE libpropd libprop)
