# Cortana 2026-10-15 1105: RND-MT AVX2 for software_renderer.cpp
# render: IRenderer / ITextureArray interfaces, BlockMaterials, mesher (public); backends are
# private sources. The gl33 backend embeds assets/shaders as its built-in fallback (generated
# header, rebuilt when a shader file changes).
set(LITHOLATTICE_SHADER_DIR ${LithoLattice_SOURCE_DIR}/assets/shaders)
file(GLOB LITHOLATTICE_SHADER_FILES CONFIGURE_DEPENDS ${LITHOLATTICE_SHADER_DIR}/*.vert ${LITHOLATTICE_SHADER_DIR}/*.frag ${LITHOLATTICE_SHADER_DIR}/*.glsl)
set(LITHOLATTICE_BUILTIN_SHADERS ${CMAKE_CURRENT_BINARY_DIR}/generated/builtin_shaders.h)
add_custom_command(OUTPUT ${LITHOLATTICE_BUILTIN_SHADERS}
  COMMAND ${CMAKE_COMMAND} -DDIR=${LITHOLATTICE_SHADER_DIR} "-DPATTERNS=*.vert,*.frag,*.glsl" -DOUT=${LITHOLATTICE_BUILTIN_SHADERS}
          -DSTRUCT=BuiltinShaderFile -DARRAY=kBuiltinShaderFiles -P ${LithoLattice_SOURCE_DIR}/cmake/embed_text_files.cmake
  DEPENDS ${LITHOLATTICE_SHADER_FILES} ${LithoLattice_SOURCE_DIR}/cmake/embed_text_files.cmake
  COMMENT "Embedding base shaders")

add_library(litholattice_render STATIC mesher.cpp binary_greedy_mesher.cpp micro_mesher.cpp block_materials.cpp pbr_materials.cpp pbr_brdf.cpp pbr_quad_mesh.cpp overlay_geometry.cpp hud_ui_renderer.cpp software_renderer.cpp software_aa.cpp software_ascii.cpp ground_traces.cpp
            gl33/gl_loader.cpp gl33/gl_program.cpp gl33/gl_texture_array.cpp gl33/gl33_renderer.cpp gl33/software_presenter.cpp
            ${LITHOLATTICE_BUILTIN_SHADERS})
target_include_directories(litholattice_render PUBLIC include PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(litholattice_render PUBLIC litholattice_assets litholattice_world litholattice_ui litholattice_core litholattice_subdiv PRIVATE litholattice_systerian)  # world: read-only block data; ui: IUiRenderer; subdiv: micro-face meshing; systerian: ASCII pipeline glyph codec (software_ascii.cpp)
litholattice_module_options(litholattice_render)

# RND perf (2026-10-15, Cortana): AVX2 for the software rasterizer only.
# Never applied to sim/circuit code (hash stays put). No FMA contraction is
# requested (GCC/Clang: -mavx2 without -mfma; MSVC keeps its default of no
# /fp:contract), so float results match the SSE2 build. Opt out with
# -DLL_SW_AVX2=OFF; non-x86 targets (Apple Silicon, ARM Linux) are untouched.
option(LL_SW_AVX2 "Build software_renderer.cpp with AVX2" ON)
if(LL_SW_AVX2 AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|x86_64|amd64)$" AND NOT CMAKE_OSX_ARCHITECTURES MATCHES "arm64")
  if(MSVC)
    set_source_files_properties(software_renderer.cpp PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
  else()
    set_source_files_properties(software_renderer.cpp PROPERTIES COMPILE_OPTIONS "-mavx2")
  endif()
endif()
