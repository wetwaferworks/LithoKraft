# script: sandboxed AngelScript engine. Depends on core only; the game registers its API.
set(LITHOLATTICE_AS_STRING ${angelscript_SOURCE_DIR}/add_on/scriptstdstring/scriptstdstring.cpp)
add_library(litholattice_script STATIC script_engine.cpp ${LITHOLATTICE_AS_STRING})
target_include_directories(litholattice_script PUBLIC include ${angelscript_SOURCE_DIR}/add_on)
target_link_libraries(litholattice_script PUBLIC litholattice_core angelscript)
litholattice_module_options(litholattice_script)
# Third-party add-on: build it quietly (our warning level is for our code).
set_source_files_properties(${LITHOLATTICE_AS_STRING} PROPERTIES COMPILE_OPTIONS "$<IF:$<CXX_COMPILER_ID:MSVC>,/W0,-w>")
