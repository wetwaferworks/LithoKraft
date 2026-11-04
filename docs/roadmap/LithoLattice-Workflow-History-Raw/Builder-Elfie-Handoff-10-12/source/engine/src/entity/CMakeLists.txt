# entity: entity storage, ticking, spatial queries, save/load hooks.
add_library(litholattice_entity STATIC entity.cpp physics.cpp)
target_include_directories(litholattice_entity PUBLIC include)
target_link_libraries(litholattice_entity PUBLIC litholattice_core)
litholattice_module_options(litholattice_entity)
