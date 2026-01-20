cmake_minimum_required(VERSION 3.11)

project(explot)


set(BUILD_SHARED_LIBS ON)
set(CMAKE_CONFIGURATION_TYPES Debug Release)

add_executable(explot)

set_property(TARGET explot PROPERTY CXX_STANDARD 23)
set_property(TARGET explot PROPERTY CXX_STANDARD_REQUIRED True)
set_property(TARGET explot PROPERTY CXX_EXTENSIONS Off)

set_property(TARGET explot PROPERTY EXPORT_COMPILE_COMMANDS ON)

#set_property(TARGET explot PROPERTY CXX_CLANG_TIDY clang-tidy -checks=-*,readability-*,-readability-uppercase-literal-suffix,-readability-identifier-length,portability-*,performance-*,cppcoreguidelines-*)

if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
  cmake_policy(SET CMP0135 NEW)
endif()
include(FetchContent)
FetchContent_Declare(lexy URL https://lexy.foonathan.net/download/lexy-src.zip)
FetchContent_MakeAvailable(lexy)
target_link_libraries(explot PRIVATE foonathan::lexy)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(explot PRIVATE Threads::Threads)

target_include_directories(explot PRIVATE RxCpp/Rx/v2/src)

set(OpenGL_GL_PREFERENCE GLVND)

find_package(Freetype REQUIRED)
target_link_libraries(explot PRIVATE Freetype::Freetype)

find_package(OpenGL REQUIRED)
target_link_libraries(explot PRIVATE OpenGL::GL)

find_package(GLEW REQUIRED)
target_link_libraries(explot PRIVATE GLEW::GLEW)

find_package(glfw3 3.3 REQUIRED)
target_link_libraries(explot PRIVATE glfw)

find_package(fmt REQUIRED)
target_link_libraries(explot PRIVATE fmt::fmt-header-only)

if (WIN32)
    find_package(unofficial-readline-win32 CONFIG REQUIRED)
    target_link_libraries(explot PRIVATE unofficial::readline-win32::readline)
else()
    target_link_libraries(explot PRIVATE linenoise)
endif()

find_package(glm REQUIRED)
#target_link_libraries(explot PRIVATE glm::glm)

find_package(Fontconfig REQUIRED)
target_link_libraries(explot PRIVATE Fontconfig::Fontconfig)

target_compile_options(explot PRIVATE -Og -Wpedantic -Werror -Wextra $<$<PLATFORM_ID:Linux>:-Wall> -Wconversion -Wno-deprecated-declarations $<$<CONFIG:Debug>:-O0 $<$<PLATFORM_ID:Linux>:-glldb>> $<$<CONFIG:Release>:-O3>)

cmake_policy(SET CMP0076 NEW)
add_subdirectory(src)
