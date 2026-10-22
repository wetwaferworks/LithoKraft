# lithocef_cef — the minimal CEF-backed LithoCEF host (Phase 2).
#
# STANDALONE project: deliberately not part of the engine build. The
# LithoCEF layer sits OUTSIDE the engine (JD, 2026-10-20); its build
# must not grow hooks into the engine's CMake. Point CEF_ROOT at an
# extracted official CEF binary distribution:
#
#   cmake -S host/lithocef/cef -B <build-dir> \
#         -DCEF_ROOT=<path-to-cef_binary_..._linux64> -DCMAKE_BUILD_TYPE=Release
#   cmake --build <build-dir> -j2
#
# Follows the CEF project's documented consumer pattern (FindCEF +
# libcef_dll_wrapper + the distribution's target-property macros).
cmake_minimum_required(VERSION 3.19)
# C must be enabled alongside CXX: the distribution's cef_variables.cmake
# probes C compiler flags during find_package(CEF).
project(lithocef_cef C CXX)

if(NOT CEF_ROOT)
  message(FATAL_ERROR "Set -DCEF_ROOT=<extracted CEF binary distribution dir>")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CEF_ROOT}/cmake")
find_package(CEF REQUIRED)

# The wrapper static library, built from the distribution's sources —
# the supported way to consume libcef from C++.
add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)

# Logical imported target for libcef itself (CEF 152's cmake pattern:
# ADD_LOGICAL_TARGET over CEF_LIB_DEBUG / CEF_LIB_RELEASE; there is no
# CEF_LIBRARIES variable in current distributions).
ADD_LOGICAL_TARGET("libcef_lib" "${CEF_LIB_DEBUG}" "${CEF_LIB_RELEASE}")

add_executable(lithocef_cef lithocef_cef_main.cpp)
SET_EXECUTABLE_TARGET_PROPERTIES(lithocef_cef)
add_dependencies(lithocef_cef libcef_dll_wrapper)
target_link_libraries(lithocef_cef libcef_lib libcef_dll_wrapper ${CEF_STANDARD_LIBRARIES})

# Runtime layout: the host executable sits beside libcef.so and the
# CEF resource files, exactly as the distribution's samples arrange it.
COPY_FILES(lithocef_cef "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR}"
           "${CMAKE_CURRENT_BINARY_DIR}")
COPY_FILES(lithocef_cef "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}"
           "${CMAKE_CURRENT_BINARY_DIR}")

PRINT_CEF_CONFIG()
