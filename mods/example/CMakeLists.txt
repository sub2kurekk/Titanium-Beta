if(MINGW)
    add_library(example_mod SHARED src/main.cpp)
    # Self-contained: statically link the MinGW runtime so the mod doesn't
    # depend on libgcc_s_seh-1.dll / libstdc++-6.dll being present in the
    # Proton prefix (which causes LoadLibraryA error 126).
    target_link_options(example_mod PRIVATE
        -static
        -static-libgcc
        -static-libstdc++
    )
else()
    add_library(example_mod MODULE src/main.cpp)
endif()

target_include_directories(example_mod PRIVATE ${CMAKE_SOURCE_DIR}/sdk/include)
add_dependencies(example_mod titaniumloader)

install(TARGETS example_mod LIBRARY DESTINATION share/titaniumloader/mods/example)
