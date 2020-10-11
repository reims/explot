cmake_minimum_required(VERSION 3.11)

project(explot)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS Off)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(MSDFGEN_CORE_ONLY ON)
set(BUILD_SHARED_LIBS ON)
add_subdirectory(msdfgen)
add_subdirectory(src)
