
# add the executable
add_executable(explot
  explot.cpp
  plot2d.cpp
  graph2d.cpp
  coordinate_system_2d.cpp
  data.cpp
  minmax.cpp
  rect.cpp
  line_drawing.cpp
  point_drawing.cpp
  font_atlas.cpp
  parse_commands.cpp
  parse_ast.cpp
  csv.cpp
  events.cpp
  drag_renderer.cpp
  settings.cpp
  range_setting.cpp
  graph3d.cpp
  plot3d.cpp
  coordinate_system_3d.cpp
  program.cpp
  legend.cpp
  colors.cpp
  rx-renderers.cpp
  impulse.cpp
  user_definitions.cpp
  prefix_sum.cpp
  surfaces.cpp
  layout.cpp
)

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

target_include_directories(explot PRIVATE ../RxCpp/Rx/v2/src)

set(OpenGL_GL_PREFERENCE GLVND)

find_package(fmt REQUIRED)
target_link_libraries(explot PRIVATE fmt::fmt-header-only)

find_package(Freetype REQUIRED)
target_link_libraries(explot PRIVATE Freetype::Freetype)

find_package(OpenGL REQUIRED)
target_link_libraries(explot PRIVATE OpenGL::GL)

find_package(GLEW REQUIRED)
target_link_libraries(explot PRIVATE GLEW::GLEW)

find_package(glfw3 3.3 REQUIRED)
target_link_libraries(explot PRIVATE glfw)

if (WIN32)
    find_package(unofficial-readline-win32 CONFIG REQUIRED)
    target_link_libraries(explot PRIVATE unofficial::readline-win32::readline)
else()
    target_link_libraries(explot PRIVATE linenoise)
endif()

find_package(glm REQUIRED)
#target_link_libraries(explot PRIVATE glm)

find_package(Fontconfig REQUIRED)
target_link_libraries(explot PRIVATE Fontconfig::Fontconfig)

if (WIN32)
target_compile_options(explot PRIVATE -Og -Wpedantic -Wextra -Wconversion -Wno-deprecated-declarations -Werror -O1)
else()
target_compile_options(explot PRIVATE -Og -Wpedantic -Werror -Wextra -Wall -Wconversion -Wno-deprecated-declarations -glldb -O1)
endif()
