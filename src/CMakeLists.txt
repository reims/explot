
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
  line_type.cpp
  rx-renderers.cpp
  impulse.cpp
  user_definitions.cpp
  prefix_sum.cpp
)

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
target_link_libraries(explot PRIVATE fmt)

find_package(Freetype REQUIRED)
target_link_libraries(explot PRIVATE Freetype::Freetype)

find_package(OpenGL REQUIRED)
target_link_libraries(explot PRIVATE OpenGL::GL)

find_package(GLEW REQUIRED)
target_link_libraries(explot PRIVATE GLEW::GLEW)

find_package(glfw3 3.3 REQUIRED)
target_link_libraries(explot PRIVATE glfw)

target_link_libraries(explot PRIVATE linenoise)

find_package(glm REQUIRED)
#target_link_libraries(explot PRIVATE glm)

find_package(Fontconfig REQUIRED)
target_link_libraries(explot PRIVATE Fontconfig::Fontconfig)

target_compile_options(explot  PUBLIC)

target_compile_options(explot PRIVATE -Og -Wpedantic -Werror -Wextra -Wall -Wconversion -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable -Wno-deprecated-declarations -Wno-unused-command-line-argument -stdlib=libc++ -fvisibility=hidden)
