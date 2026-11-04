# procgen: noise (Perlin, OpenSimplex2 F/S, value, cellular, white), modifiers (fractals, domain
# warp), the JSON node graph and the generator pipeline (stages, seeding). Depends on core only:
# no world, assets or GL types; worlds plug in through material ids and stage objects.
add_library(litholattice_procgen STATIC noise.cpp perlin.cpp open_simplex2.cpp value_noise.cpp cellular_noise.cpp
            modifiers.cpp graph.cpp graph_nodes.cpp
            pipeline.cpp stages.cpp generator_loader.cpp procgen_selftest.cpp)
target_include_directories(litholattice_procgen PUBLIC include)
target_link_libraries(litholattice_procgen PUBLIC litholattice_core)
litholattice_module_options(litholattice_procgen)
