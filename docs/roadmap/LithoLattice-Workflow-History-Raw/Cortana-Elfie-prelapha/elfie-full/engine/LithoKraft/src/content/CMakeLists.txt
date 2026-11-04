# content: LithoKraft's game content as the first "mod" on the LithoLattice SDK - the block
# registry, the signal sim (IWorldSimulation) and its component behaviours, the block appearance
# (IBlockAppearance) and the sound-effect recipes. Depends on engine modules only.
add_library(lithokraft_content STATIC
  blocks.cpp circuit_scene.cpp signal.cpp signal_world.cpp signal_selftest.cpp appearance.cpp sfx.cpp content_selftest.cpp
  components/catalog.cpp components/passive_blocks.cpp components/logic_components.cpp
  components/input_components.cpp components/sound_components.cpp)
target_include_directories(lithokraft_content PUBLIC include)
target_link_libraries(lithokraft_content PUBLIC litholattice_storage litholattice_sim litholattice_render litholattice_audio litholattice_world litholattice_core)
litholattice_module_options(lithokraft_content)
