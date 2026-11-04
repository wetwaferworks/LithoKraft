# Assets: resource packs, pack stack, texture registry, block models, shader sources (CPU only;
# depends on core; never includes GL). stb_image is third-party: its own tiny library, built
# without our warning flags.
add_library(litholattice_stb STATIC stb_image_impl.c)
target_include_directories(litholattice_stb PUBLIC ${LITHOLATTICE_STB_DIR})

add_library(litholattice_assets STATIC image.cpp resource_pack.cpp texture_registry.cpp block_model.cpp box_model.cpp mod_manifest.cpp
            resource_loader.cpp shader_library.cpp assets_selftest.cpp)
target_include_directories(litholattice_assets PUBLIC include)
target_link_libraries(litholattice_assets PUBLIC litholattice_core PRIVATE litholattice_stb)
litholattice_module_options(litholattice_assets)
