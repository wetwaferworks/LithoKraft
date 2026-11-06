# content: LithoKraft's game content as the first "mod" on the LithoLattice SDK - the block
# registry, the signal sim (IWorldSimulation) and its component behaviours, the block appearance
# (IBlockAppearance) and the sound-effect recipes. Depends on engine modules only.
add_subdirectory(model)  # .lthm model format + save/place pipeline
add_subdirectory(vegetation)  # game-side ProcGen vegetation API (workstream 3)
add_library(lithokraft_content STATIC
  blocks.cpp block_families.cpp items.cpp item_use.cpp chipor_viewport.cpp circuit_scene.cpp litho_scene.cpp pattern_rom.cpp test_world.cpp cBoltLabWorld.cpp signal.cpp signal_world.cpp signal_selftest.cpp appearance.cpp silica.cpp sfx.cpp content_selftest.cpp certificates.cpp
  components/catalog.cpp components/passive_blocks.cpp components/torch.cpp components/logic_components.cpp
  components/input_components.cpp components/sound_components.cpp components/certificate_table.cpp components/etchor.cpp components/post.cpp components/latchor.cpp components/randor.cpp components/fusor_sum.cpp components/dividor.cpp components/routor.cpp
  foliage.cpp component_models.cpp scheduled_output.cpp material_bindings.cpp)  # JD Vulkan push 2026-10-16: .lthm material bindings  # Cortana 2026-10-15 2010 [GAME]: 3D foliage
target_include_directories(lithokraft_content PUBLIC include)
target_link_libraries(lithokraft_content PUBLIC litholattice_storage litholattice_sim litholattice_render litholattice_audio litholattice_world litholattice_core lithokraft_model)
# .lthm model loading for the Chipor viewport (workstream 4 -> workstream 3's lithokraft_model).
# The real SparseOctree implementation arrives transitively via litholattice_render -> litholattice_subdiv.
target_compile_definitions(lithokraft_content PRIVATE LITHOKRAFT_MODELS_DIR="${LITHOKRAFT_MODELS_DIR_PATH}")
litholattice_module_options(lithokraft_content)
