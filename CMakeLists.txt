cmake_minimum_required(VERSION 3.7)

project(gbemu)

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Qt5Widgets REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

set(CMAKE_AUTOMOC ON)
#qt5_wrap_cpp(src/debugwindow.hpp src/debugwindow.cpp)

add_executable(gbemu src/main.cpp src/cpu.cpp src/ppu.cpp external/src/imgui.cpp external/src/imgui_demo.cpp external/src/imgui_draw.cpp external/src/imgui_widgets.cpp external/src/imgui_impl_sdl.cpp external/src/imgui_impl_opengl2.cpp external/src/glad.cpp src/timer.cpp src/disas.cpp src/opcodes.cpp src/disas.cpp src/util.cpp src/debugwindow.cpp
    include/cpu.hpp include/opcodes.hpp include/ppu.hpp include/timer.hpp include/disas.hpp include/util.hpp include/debugwindow.hpp)

target_include_directories(gbemu PRIVATE include external/include ${Qt5Widgets_INCLUDE_DIRS})
target_link_libraries(gbemu ${SDL2_LIBRARIES} ${OPENGL_LIBRARIES} ${CMAKE_DL_LIBS} ${Qt5Widgets_LIBRARIES})
