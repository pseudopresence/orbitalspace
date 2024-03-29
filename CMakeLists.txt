cmake_minimum_required (VERSION 2.6)

project (OrbitalSpace)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "./cmake/")

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g3 -Wall -Wno-deprecated -Wno-comment -fno-operator-names -std=c++11 -D_DEBUG")

#set(CMAKE_BUILD_TYPE Release)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

find_package(Threads REQUIRED)

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME ON)
set(Boost_MIN_VERSION "1.46.0")
find_package(Boost COMPONENTS thread program_options system date_time REQUIRED)

find_package(OpenGL REQUIRED) # includes GLU

# No include for Threads
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${OpenGL_INCLUDE_DIRS})

# TODO what?? Fix plz
# TODO? find_package(Eigen3 REQUIRED)
include_directories("/usr/include/eigen3")
include_directories("/usr/include/eigen3/unsupported")

include_directories("./3rdparty/glm")

add_subdirectory("./3rdparty/SDL")
include_directories("./3rdparty/SDL/include")

include_directories("./3rdparty/glad/include")

include_directories("./3rdparty/stb/include")

include_directories("./3rdparty/imgui")

include_directories("./include")
include_directories("./src")

add_definitions(-D__STDC_LIMIT_MACROS)


# Old version
#add_executable(OrbitalSpace
#  src/main.cpp
#  src/orApp.cpp
#  src/orCamera.cpp
#  src/orEntity.cpp
#  src/orPhysics.cpp
#  src/orRender.cpp
#  src/util.cpp
#  src/orProfile/perftimer.cpp
#  src/timer.cpp
#)

add_executable(OrbitalSpace
  src/main.cpp
  src/util/timer.cpp
  3rdparty/glad/src/glad.c # TODO build and link separately
  3rdparty/imgui/imgui.cpp
  3rdparty/imgui/imgui_draw.cpp
  3rdparty/imgui/imgui_demo.cpp
  3rdparty/imgui/imgui_impl_sdl_gl3.cpp
)

target_link_libraries(OrbitalSpace
  ${CMAKE_THREAD_LIBS_INIT} # special case because pthread isn't always called pthread
  ${Boost_LIBRARIES}
  ${OpenGL_LIBRARIES}
  SDL2
  GLU # for gluErrorString
)
