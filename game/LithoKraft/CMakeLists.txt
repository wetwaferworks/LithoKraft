# LithoKraft: the game (game module, app = main + selftest runner, game pack assets/, scripts/),
# built on the LithoLattice engine modules from the root. Dependencies point game -> engine only.
project(LithoKraft VERSION 0.1.0 LANGUAGES C CXX)
# Runtime .lthm model directory baked into the binary as LITHOKRAFT_MODELS_DIR.
# Native default is the source-tree gallery (unchanged). A WASM/web build
# overrides it with the preloaded VFS location, because the host source path
# does not exist inside the browser filesystem:
#   -DLITHOKRAFT_MODELS_DIR_PATH=/LithoKraft/assets/models
if(NOT DEFINED LITHOKRAFT_MODELS_DIR_PATH)
  set(LITHOKRAFT_MODELS_DIR_PATH "${LithoKraft_SOURCE_DIR}/assets/models")
endif()
# 2026-10-16 (JD): build counter "Alpha a.b.c.d", bumped on EVERY build (Debug or Release) by an
# always-out-of-date target; persisted in the tracked <tree>/build_counter.txt; the generated header
# lands in the build dir. Display/log only (never in the determinism hash).
set(LK_BUILD_COUNTER_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../build_counter.txt")
set(LK_BUILD_COUNTER_HEADER_DIR "${CMAKE_BINARY_DIR}/generated/lithokraft")
file(MAKE_DIRECTORY "${LK_BUILD_COUNTER_HEADER_DIR}")
add_custom_target(lk_build_counter ALL
  COMMAND ${CMAKE_COMMAND} -DCOUNTER=${LK_BUILD_COUNTER_FILE} -DHEADER=${LK_BUILD_COUNTER_HEADER_DIR}/lithokraft_build_counter.h
          -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/bump_build_counter.cmake
  BYPRODUCTS ${LK_BUILD_COUNTER_HEADER_DIR}/lithokraft_build_counter.h
  COMMENT "LithoKraft build counter bump"
  VERBATIM)
add_subdirectory(src/content)
add_subdirectory(src/game)
add_subdirectory(src/app)
add_subdirectory(src/headless)
