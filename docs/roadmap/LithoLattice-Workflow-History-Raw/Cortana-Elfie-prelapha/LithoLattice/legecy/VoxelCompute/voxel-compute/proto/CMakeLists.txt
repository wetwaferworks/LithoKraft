cmake_minimum_required(VERSION 3.16)
project(VoxelCompute LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

include(FetchContent)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
if(UNIX AND NOT APPLE)
  # X11 only keeps the Linux dependency list short; Wayland can be enabled later.
  set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
endif()
FetchContent_Declare(glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(glfw)

find_package(Threads REQUIRED)

add_executable(voxel_compute
  src/main.cpp
  src/gl_loader.cpp
  src/world.cpp
  src/mesher.cpp
  src/sim.cpp)
target_link_libraries(voxel_compute PRIVATE glfw Threads::Threads)
if(WIN32)
  target_link_libraries(voxel_compute PRIVATE winmm)  # timeBeginPeriod for high-rate sim
  target_compile_definitions(voxel_compute PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
target_compile_definitions(voxel_compute PRIVATE GLFW_INCLUDE_NONE)

if(MSVC)
  target_compile_options(voxel_compute PRIVATE /W4 /permissive- /O2)
  set_property(TARGET voxel_compute PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
  target_compile_options(voxel_compute PRIVATE -Wall -Wextra -O2)
endif()
