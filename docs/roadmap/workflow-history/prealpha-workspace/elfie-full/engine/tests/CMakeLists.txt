# litholattice_tests: engine-only tests (links no game code). ctest runs one test per module.
add_executable(litholattice_tests engine_tests.cpp)
target_link_libraries(litholattice_tests PRIVATE litholattice_storage litholattice_ai litholattice_sim litholattice_render litholattice_world litholattice_procgen
                      litholattice_ui litholattice_assets litholattice_audio litholattice_input litholattice_core)
litholattice_module_options(litholattice_tests)
foreach(module core world render sim ai storage procgen ui assets audio input)
  add_test(NAME engine_${module} COMMAND litholattice_tests ${module})
endforeach()
