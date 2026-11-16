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
# 2026-10-23 (JD, fix run 2): LithoKraft has its OWN rolling decimal counter, bumped on EVERY
# build: <tree>/LITHOKRAFT_BUILD_COUNTER.txt. The public label lives in <tree>/LITHOKRAFT_VERSION.txt.
# (Was <tree>/build_counter.txt, which on Windows is the same file as the engine BUILD_COUNTER.txt.)
# Display/log only (never in the determinism hash). See docs/versioning/COUNTERS.md.
set(LK_BUILD_COUNTER_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../LITHOKRAFT_BUILD_COUNTER.txt")
set(LK_VERSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../LITHOKRAFT_VERSION.txt")
set(LK_BUILD_COUNTER_HEADER_DIR "${CMAKE_BINARY_DIR}/generated/lithokraft")
file(MAKE_DIRECTORY "${LK_BUILD_COUNTER_HEADER_DIR}")
add_custom_target(lk_build_counter ALL
  COMMAND ${CMAKE_COMMAND} -DCOUNTER=${LK_BUILD_COUNTER_FILE} -DHEADER=${LK_BUILD_COUNTER_HEADER_DIR}/lithokraft_build_counter.h -DVERSION_FILE=${LK_VERSION_FILE}
          -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/bump_build_counter.cmake
  BYPRODUCTS ${LK_BUILD_COUNTER_HEADER_DIR}/lithokraft_build_counter.h
  COMMENT "LithoKraft build counter bump"
  VERBATIM)
# 2026-11-16 (S5 web): the web configure writes lithokraft_build_counter.h up front with the same script,
# run on a held copy of the counter (a .hold file next to it), so configuring never bumps the real
# counter; the build target above still bumps it per build. Native configure: unchanged.
if(LK_WEB)
  set(_lk_cfg_counter "${LK_BUILD_COUNTER_HEADER_DIR}/configure_counter.txt")
  if(EXISTS "${LK_BUILD_COUNTER_FILE}")
    configure_file("${LK_BUILD_COUNTER_FILE}" "${_lk_cfg_counter}" COPYONLY)
  else()
    file(WRITE "${_lk_cfg_counter}" "0\n")
  endif()
  file(WRITE "${_lk_cfg_counter}.hold" "")
  execute_process(COMMAND ${CMAKE_COMMAND} -DCOUNTER=${_lk_cfg_counter} -DHEADER=${LK_BUILD_COUNTER_HEADER_DIR}/lithokraft_build_counter.h -DVERSION_FILE=${LK_VERSION_FILE}
                          -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/bump_build_counter.cmake
                  RESULT_VARIABLE _lk_gen_rc)
  if(NOT _lk_gen_rc EQUAL 0)
    message(FATAL_ERROR "web configure: writing lithokraft_build_counter.h failed (${_lk_gen_rc})")
  endif()
endif()
add_subdirectory(src/content)
add_subdirectory(src/game)
add_subdirectory(src/app)
add_subdirectory(src/headless)
