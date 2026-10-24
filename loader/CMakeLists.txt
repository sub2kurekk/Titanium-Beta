if(WIN32)
    add_library(titaniumloader SHARED
        src/dllmain.cpp
        src/hookengine.cpp
        src/patternscan.cpp
        src/modloader.cpp
        src/gdinit.cpp
        src/loader.cpp
        src/ui/modmenu.cpp
        src/log.cpp
    )
    target_link_libraries(titaniumloader PRIVATE psapi)
    target_link_options(titaniumloader PRIVATE -static -static-libgcc -static-libstdc++)
else()
    # Dummy / Linux native support if needed
    add_library(titaniumloader SHARED
        src/entry.cpp
        src/loader.cpp
        src/log.cpp
        src/hookengine.cpp
        src/patternscan.cpp
        src/modloader.cpp
        src/gdinit.cpp
        src/ui/modmenu.cpp
    )
    target_link_libraries(titaniumloader PRIVATE dl)
endif()

target_include_directories(titaniumloader
    PUBLIC  include
    PRIVATE src
    PRIVATE ${CMAKE_SOURCE_DIR}/sdk/include
)

target_compile_definitions(titaniumloader PRIVATE TITANIUM_BUILD_LOADER)

install(TARGETS titaniumloader LIBRARY DESTINATION lib/titaniumloader)
