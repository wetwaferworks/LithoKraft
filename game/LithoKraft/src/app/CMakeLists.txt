# Dev builds (default) load scripts from the source tree so F10 edits stay in the repo; shipping
# builds use the scripts/ copied next to the exe. --scripts source|exe|<dir> overrides at run time.
option(LITHOKRAFT_DEV_BUILD "Scripts (and the F10 editor) use the source tree's scripts/" ON)
add_executable(lithokraft main.cpp BenchSampleGate.cpp scripts_dir.cpp selftest.cpp perf_headless.cpp noise_preview.cpp handshake_test.cpp control_latency.cpp control_stream_test.cpp demo_record_test.cpp)
# Exe icon (Windows): lithokraft.rc embeds LithoKraft/assets/icons/lithokraft.ico.
set(LITHOKRAFT_ICO ${LithoKraft_SOURCE_DIR}/assets/icons/lithokraft.ico)
if(WIN32 AND EXISTS ${LITHOKRAFT_ICO})
  configure_file(${LITHOKRAFT_ICO} ${CMAKE_CURRENT_BINARY_DIR}/lithokraft.ico COPYONLY)
  configure_file(lithokraft.rc ${CMAKE_CURRENT_BINARY_DIR}/lithokraft.rc COPYONLY)
  target_sources(lithokraft PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/lithokraft.rc)
elseif(WIN32)
  message(WARNING "lithokraft.ico not found: the exe has no icon")
endif()
target_link_libraries(lithokraft PRIVATE lithokraft_game litholattice_audio_device)
# WASM checkpoint (2026-10-18): no Vulkan backend in the browser build; the
# stub answers createVulkanRenderer/probeVulkan with the same "unavailable"
# result a Vulkan-less machine gives, so the app's fallback path is unchanged.
if(NOT LK_WEB)  # LK_WEB: the LithoRoot wasm32 toolchain (engine/CMakeLists.txt), never EMSCRIPTEN
  target_link_libraries(lithokraft PRIVATE litholattice_render_vk)
else()
  target_sources(lithokraft PRIVATE vulkan_stub_web.cpp)
  # Purge stage 1 (2026-10-28): the page-layer imports (lk_web.h, the
  # lk_gl* table, the lk_webio verbs) are declared in C++ with import
  # annotations and provided at instantiation by lkboot.js â€” no JS
  # library registers them, so the driver's glue symbol checker must
  # not error on them. A genuinely missing import still fails loudly:
  # lkboot refuses instantiation, and the build's import census gate
  # verifies the exact set. Stage 2 (2026-11-16): the vendor driver flag
  # that silenced its glue checker is gone with the vendor toolchain; stock wasm-ld resolves annotated imports itself and fails on any other undefined symbol.
  # malloc stays an export: lkboot allocates its static strings and
  # boot structures through it. Nothing in the (retired) JS libraries
  # references it any more, so the driver no longer exports it by
  # default (gate (d) evidence, 2026-10-28). The export goes straight
  # to wasm-ld on purpose: in this configuration the driver's own
  # EXPORTED_FUNCTIONS route collapses the module's import surface
  # (measured: 142 imports -> 14, the program DCE'd around the listed
  # names), while --export=malloc is surgical (142 imports, malloc
  # exported).
  target_link_options(lithokraft PRIVATE "-Wl,--export=malloc")
  # wasm-ld reads the allow list line by line and keeps a trailing CR, so the two checked-in lists
  # (CRLF on a Windows checkout) are joined into one LF file in the build tree at configure time.
  set(_lk_web_allowed "${CMAKE_CURRENT_BINARY_DIR}/web-link-allowed.txt")
  file(STRINGS "${LithoLattice_SOURCE_DIR}/lithoroot/toolchain/lithosys-allowed.txt" _lk_allow_sys)
  file(STRINGS "${LithoLattice_SOURCE_DIR}/lithoroot/toolchain/web-link-pending.txt" _lk_allow_pending REGEX "^[^#]")
  list(TRANSFORM _lk_allow_sys STRIP)
  list(TRANSFORM _lk_allow_pending STRIP)
  list(JOIN _lk_allow_sys "\n" _lk_a1)
  list(JOIN _lk_allow_pending "\n" _lk_a2)
  file(WRITE "${_lk_web_allowed}" "${_lk_a1}\n${_lk_a2}\n")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${LithoLattice_SOURCE_DIR}/lithoroot/toolchain/lithosys-allowed.txt"
    "${LithoLattice_SOURCE_DIR}/lithoroot/toolchain/web-link-pending.txt")
  # Link wiring (2026-11-16): the host calls __wasm_call_ctors, then __main_argc_argv (clang's
  # wasm name for main(argc, argv)); there is no _start. lithosys_thread_start is the weave's
  # thread entry, __wasm_init_tls the per-thread TLS set-up. Undefined symbols: only the 14 frozen
  # lithosys calls (lithosys-allowed.txt) and the named list of stages still pending
  # (web-link-pending.txt) may become imports; lk_* imports are annotated in their headers. Any
  # other undefined symbol is a link error.
  target_link_options(lithokraft PRIVATE "-Wl,--no-entry" "-Wl,--export=__main_argc_argv"
    "-Wl,--export=lithosys_thread_start" "-Wl,--export-if-defined=__wasm_call_ctors"
    "-Wl,--export-if-defined=__wasm_init_tls"
    # lkboot thread start (wasm/lkboot/lkthread.mjs): per-thread stack and TLS
    "-Wl,--export=__stack_pointer"
    "-Wl,--export-if-defined=__tls_size"
    "-Wl,--export-if-defined=__tls_align"
    "-Wl,--export=free"
    "-Wl,--allow-undefined-file=${_lk_web_allowed}")
  # S1 (2026-11-16): an 8 MiB main stack, placed first so an overflow traps at address 0
  # instead of running into static data (wasm-ld's default stack is 64 KiB).
  target_link_options(lithokraft PRIVATE "-Wl,-z,stack-size=8388608" "-Wl,--stack-first")
  # The render_vk target (and with it its PUBLIC include dir) is not built
  # here, but main.cpp/selftest.cpp still #include its header for the factory
  # declarations the stub implements. Lend the target the include path only.
  target_include_directories(lithokraft PRIVATE ${LithoLattice_SOURCE_DIR}/src/render_vk/include)
endif()
target_compile_definitions(lithokraft PRIVATE LITHOKRAFT_VERSION="${PROJECT_VERSION}"
                          LITHOKRAFT_SOURCE_DIR="${LithoKraft_SOURCE_DIR}"
                          LITHOKRAFT_ENGINE_ASSETS_DIR="${LithoLattice_SOURCE_DIR}/assets")
option(LK_RECORDER_BUILD "Name the game exe LithoKraft-Recorder (separate recorder builds)" OFF)
if(LK_RECORDER_BUILD)
  set_target_properties(lithokraft PROPERTIES OUTPUT_NAME LithoKraft-Recorder
                        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
else()
  set_target_properties(lithokraft PROPERTIES OUTPUT_NAME LithoKraft
                        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
endif()
if(LITHOKRAFT_DEV_BUILD)
  target_compile_definitions(lithokraft PRIVATE LITHOKRAFT_DEV_SCRIPTS_DIR="${LithoKraft_SOURCE_DIR}/scripts")
endif()
litholattice_module_options(lithokraft)
# 2026-10-24 (JD): playtest builds spawn in the test area on a no-flag launch (own save dir).
option(LK_PLAYTEST_SPAWN "No-flag launch opens the --test-world area (saves/playtest)" OFF)
if(LK_PLAYTEST_SPAWN)
  target_compile_definitions(lithokraft PRIVATE LK_PLAYTEST_SPAWN=1)
endif()
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
# The --selftest run includes litholattice's assets suite, which writes the fixed path
# %TEMP%\litholattice-selftest-assets. engine_assets writes the same path, so under ctest -j the two
# raced (Debug 2026-10-29: both failed together; each passes alone). Share the engine's lock.
set_tests_properties(lithokraft_selftest PROPERTIES RESOURCE_LOCK litho_tempdir)
# Screenshot self-test: headless (no window / GL context), its own ctest entry.
add_executable(lithokraft_screenshot_selftest screenshot_selftest_main.cpp)
target_link_libraries(lithokraft_screenshot_selftest PRIVATE lithokraft_game)
set_target_properties(lithokraft_screenshot_selftest PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
litholattice_module_options(lithokraft_screenshot_selftest)
add_test(NAME lithokraft_screenshot_selftest COMMAND lithokraft_screenshot_selftest WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
# Scout self-test (locatebiome/genmap/structures): headless, its own ctest entry.
add_executable(lithokraft_scout_selftest scout_selftest_main.cpp)
target_link_libraries(lithokraft_scout_selftest PRIVATE lithokraft_game)
set_target_properties(lithokraft_scout_selftest PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
litholattice_module_options(lithokraft_scout_selftest)
add_test(NAME lithokraft_scout_selftest COMMAND lithokraft_scout_selftest WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
# Chipor viewport self-test (workstream 4): headless, its own ctest entry.
add_executable(lithokraft_chipor_selftest chipor_selftest_main.cpp)
target_link_libraries(lithokraft_chipor_selftest PRIVATE lithokraft_game)
set_target_properties(lithokraft_chipor_selftest PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
litholattice_module_options(lithokraft_chipor_selftest)
add_test(NAME lithokraft_chipor_selftest COMMAND lithokraft_chipor_selftest WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
# Fragment state self-test (workstream 3): headless, its own ctest entry.
add_executable(lithokraft_fragment_selftest fragment_selftest_main.cpp)
target_link_libraries(lithokraft_fragment_selftest PRIVATE lithokraft_game)
set_target_properties(lithokraft_fragment_selftest PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
litholattice_module_options(lithokraft_fragment_selftest)
add_test(NAME lithokraft_fragment_selftest COMMAND lithokraft_fragment_selftest WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
# Fragment markdown renderer self-test (workstream 2): headless, its own ctest entry.
add_executable(lithokraft_markdown_selftest fragment_markdown_selftest_main.cpp)
target_link_libraries(lithokraft_markdown_selftest PRIVATE lithokraft_game)
set_target_properties(lithokraft_markdown_selftest PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
litholattice_module_options(lithokraft_markdown_selftest)
add_test(NAME lithokraft_markdown_selftest COMMAND lithokraft_markdown_selftest WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
# Silica self-test (workstream 5): headless, its own ctest entry.
add_executable(lithokraft_silica_selftest silica_selftest_main.cpp)
target_link_libraries(lithokraft_silica_selftest PRIVATE lithokraft_game)
set_target_properties(lithokraft_silica_selftest PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
litholattice_module_options(lithokraft_silica_selftest)
add_test(NAME lithokraft_silica_selftest COMMAND lithokraft_silica_selftest WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
# Items self-test (2026-10-12): headless item registry / models / fresh-spawn
# inventory acceptance tests, its own ctest entry.
add_executable(lithokraft_items_selftest items_selftest_main.cpp)
target_link_libraries(lithokraft_items_selftest PRIVATE lithokraft_game)
target_compile_definitions(lithokraft_items_selftest PRIVATE LITHOKRAFT_MODELS_DIR="${LITHOKRAFT_MODELS_DIR_PATH}")
set_target_properties(lithokraft_items_selftest PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
litholattice_module_options(lithokraft_items_selftest)
add_test(NAME lithokraft_items_selftest COMMAND lithokraft_items_selftest WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
# Biome capture: headless 3D screenshots of the 5 Wafer biomes (not a ctest entry;
# run manually: ./biome_capture <outdir>).
add_executable(biome_capture biome_capture.cpp)
target_link_libraries(biome_capture PRIVATE lithokraft_game)
set_target_properties(biome_capture PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
litholattice_module_options(biome_capture)
# Foliage meshing counter (Crew Canopy, F-B): headless quad/vert/byte counts over
# the test world's crystal-leaf grove (not a ctest entry; run manually:
# ./foliage_count [farOffsetChunks]).
add_executable(foliage_count foliage_count_main.cpp)
target_link_libraries(foliage_count PRIVATE lithokraft_game)
set_target_properties(foliage_count PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
litholattice_module_options(foliage_count)
# Interchange capture: headless screenshots of a generated Wafer
# interchange + its population manifest (not a ctest entry;
# run manually: ./interchange_capture <outdir>).
add_executable(interchange_capture interchange_capture.cpp)
target_link_libraries(interchange_capture PRIVATE lithokraft_game)
set_target_properties(interchange_capture PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
litholattice_module_options(interchange_capture)
# Silicon Cities stills + flyover frames (headless; run manually: ./city_capture <outdir> [frames]).
add_executable(city_capture city_capture.cpp)
target_link_libraries(city_capture PRIVATE lithokraft_game)
set_target_properties(city_capture PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
litholattice_module_options(city_capture)

# FrameHashRecorder contract self-test (tick-paced hashes.txt; Data 2026-11-09).
# Headless; expected FAIL until the recorder stub is replaced by the real sink.
add_executable(lithokraft_frame_hash_selftest frame_hash_recorder_selftest_main.cpp)
target_link_libraries(lithokraft_frame_hash_selftest PRIVATE lithokraft_game)
set_target_properties(lithokraft_frame_hash_selftest PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
litholattice_module_options(lithokraft_frame_hash_selftest)
add_test(NAME lithokraft_frame_hash_selftest COMMAND lithokraft_frame_hash_selftest WORKING_DIRECTORY ${CMAKE_BINARY_DIR})


# FrameHashRecorder demo capture (headless software renderer; does not replace LithoKraft.exe).
add_executable(frame_hash_demo_capture frame_hash_demo_capture.cpp)
target_link_libraries(frame_hash_demo_capture PRIVATE lithokraft_game)
set_target_properties(frame_hash_demo_capture PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
litholattice_module_options(frame_hash_demo_capture)

add_executable(frame_hash_verify frame_hash_verify.cpp)
target_link_libraries(frame_hash_verify PRIVATE lithokraft_game)
set_target_properties(frame_hash_verify PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
litholattice_module_options(frame_hash_verify)
# Texture generator: builds block textures from assets/texture_recipes/*.ltxr
# into a pack (not a ctest entry; run manually per pack size, e.g.
# ./texture_gen --recipes ../game/LithoKraft/assets/texture_recipes --pack ../game/LithoKraft/assets/packs/pack_64 --size 64).
add_executable(texture_gen texture_gen.cpp)
target_link_libraries(texture_gen PRIVATE lithokraft_game litholattice_texture)
set_target_properties(texture_gen PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
litholattice_module_options(texture_gen)
