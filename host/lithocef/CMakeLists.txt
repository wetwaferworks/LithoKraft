# IHwLayer hardware layer (formerly the LithoCEF host shell) — outside the engine by design (JD, 2026-10-20:
# "The CEF layer will sit OUTSIDE of the engine and just boot the engine.
# And the CEF layer plugs into the hardware layer.")
#
# This directory is added from engine/CMakeLists.txt only when
# LL_WITH_LITHOCEF=ON (default OFF: the default build is untouched).
# Nothing here links into the engine or the game; the stub backend
# proves the IHwLayer contract standalone, which is the point: the
# shell is a host, not a module of the thing it hosts.

add_library(lithohw_stub STATIC hw_stub.cpp)
target_include_directories(lithohw_stub PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(lithohw_stub PUBLIC cxx_std_17)

add_executable(hwlayer_smoke hwlayer_smoke.cpp)
target_link_libraries(hwlayer_smoke PRIVATE lithohw_stub)

# The Thread Handshake (JD, 2026-10-22): the CEF layer's thread
# architecture — pool at the top, one listener/dispatch thread at the
# bottom, "thread needed" calls from the engine and other systems.
# Lives in this layer alone; the engine never links it.
find_package(Threads REQUIRED)
add_library(lithohw_thread_handshake STATIC thread_handshake.cpp)
target_include_directories(lithohw_thread_handshake PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(lithohw_thread_handshake PUBLIC cxx_std_17)
target_link_libraries(lithohw_thread_handshake PUBLIC Threads::Threads)

add_executable(thread_handshake_smoke thread_handshake_smoke.cpp)
target_link_libraries(thread_handshake_smoke PRIVATE lithohw_thread_handshake)

# The lifecycle smoke: re-boot after shutdown, a finite engine-side
# ring (drops at capacity), and the Thread Handshake serving the
# engine's "thread needed" call while the shell is booted.
add_executable(hwlayer_boot_smoke hwlayer_boot_smoke.cpp)
target_link_libraries(hwlayer_boot_smoke PRIVATE lithohw_stub lithohw_thread_handshake)

# Registered unconditionally: this directory is only ever added after the
# engine root has called enable_testing(), the same convention the
# engine's own module tests rely on.
add_test(NAME hwlayer_stub_smoke COMMAND hwlayer_smoke)
add_test(NAME hwlayer_boot_smoke COMMAND hwlayer_boot_smoke)
add_test(NAME hwlayer_thread_handshake_smoke COMMAND thread_handshake_smoke)
