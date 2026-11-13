# procgen: noise (Perlin, OpenSimplex2 F/S, value, cellular, white), modifiers (fractals, domain
# warp), the JSON node graph, the generator pipeline (stages, seeding), the procedural tree generator
# (trees.h), the procedural grass/shrub generator (shrubs.h) and the Litho Formula sampler
# (litho_formula.h). Depends on core and subdiv
# (SparseOctree for tree/shrub output):
# no world, assets or GL types; worlds plug in through material ids and stage objects.
add_library(litholattice_procgen STATIC noise.cpp perlin.cpp open_simplex2.cpp value_noise.cpp cellular_noise.cpp
            modifiers.cpp graph.cpp graph_nodes.cpp
            litho_formula_gallery.cpp
            pipeline.cpp stages.cpp diversity.cpp generator_loader.cpp procgen_selftest.cpp diversity_selftest.cpp
            struct_registry.cpp struct_registry_selftest.cpp
            struct_block.cpp
            water.cpp water_selftest.cpp
            trees.cpp trees_selftest.cpp shrubs.cpp shrubs_selftest.cpp
            litho_formula.cpp litho_formula_selftest.cpp
            litho_formula_node.cpp litho_formula_node_selftest.cpp
            litho_stamp_feature_selftest.cpp
            litho_formula_voxelize.cpp litho_formula_voxelize_selftest.cpp)
target_include_directories(litholattice_procgen PUBLIC include)
target_link_libraries(litholattice_procgen PUBLIC litholattice_core litholattice_subdiv litholattice_crypto)
# water_selftest.cpp reads litholattice/world/fluid.h (fluid-level emission convention);
# private include dir only -- procgen still does not link the world module.
target_include_directories(litholattice_procgen PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../world/include)
litholattice_module_options(litholattice_procgen)
