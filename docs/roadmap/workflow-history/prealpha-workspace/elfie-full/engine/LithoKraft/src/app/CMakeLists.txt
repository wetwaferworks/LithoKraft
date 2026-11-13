# Dev builds (default) load scripts from the source tree so F10 edits stay in the repo; shipping
# builds use the scripts/ copied next to the exe. --scripts source|exe|<dir> overrides at run time.
option(LITHOKRAFT_DEV_BUILD "Scripts (and the F10 editor) use the source tree's scripts/" ON)
add_executable(lithokraft main.cpp scripts_dir.cpp selftest.cpp perf_headless.cpp noise_preview.cpp)
# Exe icon (Windows): lithokraft.rc embeds LithoKraft/assets/icons/lithokraft.ico.
set(LITHOKRAFT_ICO ${LithoKraft_SOURCE_DIR}/assets/icons/lithokraft.ico)
if(WIN32 AND EXISTS ${LITHOKRAFT_ICO})
  configure_file(${LITHOKRAFT_ICO} ${CMAKE_CURRENT_BINARY_DIR}/lithokraft.ico COPYONLY)
  configure_file(lithokraft.rc ${CMAKE_CURRENT_BINARY_DIR}/lithokraft.rc COPYONLY)
  target_sources(lithokraft PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/lithokraft.rc)
elseif(WIN32)
  message(WARNING "lithokraft.ico not found: the exe has no icon")
endif()
target_link_libraries(lithokraft PRIVATE lithokraft_game litholattice_audio_device litholattice_render_vk)
target_compile_definitions(lithokraft PRIVATE LITHOKRAFT_VERSION="${PROJECT_VERSION}"
                          LITHOKRAFT_SOURCE_DIR="${LithoKraft_SOURCE_DIR}"
                          LITHOKRAFT_ENGINE_ASSETS_DIR="${LithoLattice_SOURCE_DIR}/assets")
set_target_properties(lithokraft PROPERTIES OUTPUT_NAME LithoKraft
                      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
if(LITHOKRAFT_DEV_BUILD)
  target_compile_definitions(lithokraft PRIVATE LITHOKRAFT_DEV_SCRIPTS_DIR="${LithoKraft_SOURCE_DIR}/scripts")
endif()
litholattice_module_options(lithokraft)
# Ship scripts/ next to the exe (used by shipping builds and --scripts exe).
# Third-party license texts ship next to the exe.
add_custom_command(TARGET lithokraft POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/THIRD_PARTY_LICENSES.md $<TARGET_FILE_DIR:lithokraft>/THIRD_PARTY_LICENSES.md)
add_custom_command(TARGET lithokraft POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LithoKraft_SOURCE_DIR}/mod.json $<TARGET_FILE_DIR:lithokraft>/mod.json)
add_custom_command(TARGET lithokraft POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${LithoKraft_SOURCE_DIR}/scripts $<TARGET_FILE_DIR:lithokraft>/scripts)
# ctest -C <config>: the full headless self-test suite (engine modules + game), exit code = failures.
add_test(NAME lithokraft_selftest COMMAND lithokraft --selftest WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
