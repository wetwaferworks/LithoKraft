# lithokraft_single (LK_WEB only): the same game linked without shared memory, for
# lkboot's single-thread fallback on pages that are not crossOriginIsolated (no
# SharedArrayBuffer). Atomic instructions stay in the code; on unshared memory they
# are ordinary loads and stores, and spawn answers EAGAIN at run time.
# The toolchain puts --shared-memory into CMAKE_EXE_LINKER_FLAGS; this directory's
# copy of that variable drops it, so only targets defined here are affected.
string(REPLACE "-Wl,--shared-memory" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
get_target_property(_lk_src lithokraft SOURCES)
get_target_property(_lk_src_dir lithokraft SOURCE_DIR)
set(_lk_abs)
foreach(_s IN LISTS _lk_src)
  if(IS_ABSOLUTE "${_s}")
    list(APPEND _lk_abs "${_s}")
  else()
    list(APPEND _lk_abs "${_lk_src_dir}/${_s}")
  endif()
endforeach()
add_executable(lithokraft_single ${_lk_abs})
foreach(_p LINK_LIBRARIES COMPILE_DEFINITIONS INCLUDE_DIRECTORIES COMPILE_OPTIONS COMPILE_FEATURES LINK_OPTIONS CXX_STANDARD)
  get_target_property(_v lithokraft ${_p})
  if(_v)
    set_property(TARGET lithokraft_single PROPERTY ${_p} "${_v}")
  endif()
endforeach()
set_target_properties(lithokraft_single PROPERTIES OUTPUT_NAME LithoKraft-single
                      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
# engine/CMakeLists.txt gives the game target the generated build-counter header after
# this directory is read; mirror it, and build after lithokraft so the counter bumps once.
target_include_directories(lithokraft_single PRIVATE "${CMAKE_BINARY_DIR}/generated")
add_dependencies(lithokraft_single lithokraft)
