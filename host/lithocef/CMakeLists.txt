# LithoCEF host shell — outside the engine by design (JD, 2026-10-20:
# "The CEF layer will sit OUTSIDE of the engine and just boot the engine.
# And the CEF layer plugs into the hardware layer.")
#
# This directory is added from engine/CMakeLists.txt only when
# LL_WITH_LITHOCEF=ON (default OFF: the default build is untouched).
# Nothing here links into the engine or the game; the stub backend
# proves the ILithoCEF contract standalone, which is the point: the
# shell is a host, not a module of the thing it hosts.

add_library(lithocef_stub STATIC lithocef_stub.cpp)
target_include_directories(lithocef_stub PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(lithocef_stub PUBLIC cxx_std_17)

add_executable(lithocef_smoke lithocef_smoke.cpp)
target_link_libraries(lithocef_smoke PRIVATE lithocef_stub)

# Registered unconditionally: this directory is only ever added after the
# engine root has called enable_testing(), the same convention the
# engine's own module tests rely on.
add_test(NAME lithocef_stub_smoke COMMAND lithocef_smoke)
