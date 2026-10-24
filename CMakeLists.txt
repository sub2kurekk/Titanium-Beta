cmake_minimum_required(VERSION 3.20)
project(TitaniumLoader VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(loader)
if(NOT WIN32)
    add_subdirectory(titanium)
endif()
add_subdirectory(mods/example)
