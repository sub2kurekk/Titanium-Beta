add_executable(titanium src/main.cpp)

target_link_libraries(titanium PRIVATE dl)

install(TARGETS titanium RUNTIME DESTINATION bin)
