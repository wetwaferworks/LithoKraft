# game: the game's generator files (assets/worldgen/*.json) are embedded as built-in fallbacks
# (generated header, rebuilt when a generator file changes).
set(LITHOKRAFT_WORLDGEN_DIR ${LithoKraft_SOURCE_DIR}/assets/worldgen)
file(GLOB LITHOLATTICE_WORLDGEN_FILES CONFIGURE_DEPENDS ${LITHOKRAFT_WORLDGEN_DIR}/*.json)
set(LITHOKRAFT_BUILTIN_WORLDGEN ${CMAKE_CURRENT_BINARY_DIR}/generated/builtin_worldgen.h)
add_custom_command(OUTPUT ${LITHOKRAFT_BUILTIN_WORLDGEN}
  COMMAND ${CMAKE_COMMAND} -DDIR=${LITHOKRAFT_WORLDGEN_DIR} "-DPATTERNS=*.json" -DOUT=${LITHOKRAFT_BUILTIN_WORLDGEN}
          -DSTRUCT=BuiltinWorldgenFile -DARRAY=kBuiltinWorldgenFiles -P ${LithoLattice_SOURCE_DIR}/cmake/embed_text_files.cmake
  DEPENDS ${LITHOLATTICE_WORLDGEN_FILES} ${LithoLattice_SOURCE_DIR}/cmake/embed_text_files.cmake
  COMMENT "Embedding built-in world generators")

add_library(lithokraft_game STATIC game.cpp world_save.cpp bench_stream.cpp actions.cpp input_selftest.cpp dimensions.cpp structures.cpp controls_panel.cpp player.cpp resource_manager.cpp sound_director.cpp
            game_ui.cpp game_ui_selftest.cpp script_editor.cpp ui_input_bridge.cpp ui_script_bindings.cpp
            ${LITHOKRAFT_BUILTIN_WORLDGEN})
target_include_directories(lithokraft_game PUBLIC include PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(lithokraft_game PUBLIC lithokraft_content litholattice_ai litholattice_sim litholattice_world litholattice_render litholattice_platform litholattice_script litholattice_audio litholattice_input litholattice_ui litholattice_core)
litholattice_module_options(lithokraft_game)
