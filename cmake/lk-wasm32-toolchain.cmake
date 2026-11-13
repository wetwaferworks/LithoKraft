# lk-wasm32-toolchain.cmake
#
# Stock-clang wasm32 toolchain for the LithoLattice / LithoKraft web
# build, replacing the retired vendor toolchain end to end. The
# compiler is an unmodified LLVM release clang; the C library, C++
# runtime, thread library and allocator it links are LithoRoot
# (lithoroot/), built from this tree. The host surface the linked
# module may import is frozen in lithoroot/include/lithosys.h
# (14 calls, module "env") plus the lk_* platform surface that was
# always ours.
#
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/lk-wasm32-toolchain.cmake ...
#
# The LLVM location is parameterized: set LK_LLVM_HOME (cache var or
# environment) to the root of an official LLVM release tree (the one
# containing bin/clang). Default below is where the toolchain piece
# installed LLVM 20.1.8 on Elfie's bench VM; on JD's workstation it
# will point wherever that tree's LLVM lives. Nothing else in this
# file is machine-specific.
#
# Verified by the toolchain piece only against a trivial throwaway
# project (the engine relink is frozen until JD lifts the test-build
# suspension); see lithoroot/docs/TOOLCHAIN.md.

# --- target identity ---------------------------------------------------
set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR wasm32)

# --- compiler ----------------------------------------------------------
if(NOT DEFINED LK_LLVM_HOME)
  if(DEFINED ENV{LK_LLVM_HOME})
    set(LK_LLVM_HOME "$ENV{LK_LLVM_HOME}")
  else()
    set(LK_LLVM_HOME "$ENV{HOME}/workspace/toolchain/LLVM-20.1.8-Linux-X64")
  endif()
endif()
set(LK_LLVM_HOME "${LK_LLVM_HOME}" CACHE PATH "Root of the official LLVM release tree")

set(CMAKE_C_COMPILER   "${LK_LLVM_HOME}/bin/clang")
set(CMAKE_CXX_COMPILER "${LK_LLVM_HOME}/bin/clang++")
set(CMAKE_AR           "${LK_LLVM_HOME}/bin/llvm-ar"   CACHE FILEPATH "")
set(CMAKE_RANLIB       "${LK_LLVM_HOME}/bin/llvm-ranlib" CACHE FILEPATH "")
set(CMAKE_LINKER       "${LK_LLVM_HOME}/bin/wasm-ld"   CACHE FILEPATH "")

set(CMAKE_C_COMPILER_TARGET   wasm32-unknown-unknown)
set(CMAKE_CXX_COMPILER_TARGET wasm32-unknown-unknown)

# Do not let CMake try to run target binaries during its checks;
# there is no wasm interpreter in the configure step.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# --- sysroot -------------------------------------------------------------
# LithoRoot IS the sysroot: its include/ is the whole header world
# (LithoLibC + LithoThread + LithoAbi headers, frozen by the pieces),
# and the library objects are built from lithoroot/ sources by the
# engine's own build once the relink lands. --sysroot makes clang
# stop looking at the host's /usr entirely.
get_filename_component(LK_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(CMAKE_SYSROOT "${LK_REPO_ROOT}/engine/lithoroot")  # 2026-10-29: LithoRoot ships inside the LithoLattice submodule
set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# --- target features -----------------------------------------------------
# atomics + bulk-memory: LithoThread/LithoWeave wait and notify are
# the memory.atomic instructions, and LithoLibC's word copies use
# bulk-memory forms. sign-ext: the form every current engine emits.
# posix thread model: __thread lowers to real wasm TLS (per-thread
# __tls_base), matching the shared-memory build.
set(LK_WASM_FEATURES "-matomics -mbulk-memory -msign-ext -mthread-model posix")
set(CMAKE_C_FLAGS_INIT   "${LK_WASM_FEATURES}")
set(CMAKE_CXX_FLAGS_INIT "${LK_WASM_FEATURES}")

# WebAssembly exceptions are the standing EH contract (caught-inside
# works, uncaught is fatal); see lithoroot/docs/LITHOABI.md.
set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} -fwasm-exceptions")

# --- linker posture --------------------------------------------------------
# Imported, shared linear memory: the host (lkboot) creates the
# WebAssembly.Memory and owns growth through lithosys_memory_grow.
# Sizes mirror lkboot's current memory object (initial 8192 pages =
# 512 MiB, maximum 32768 pages = 2 GiB); a relink may retune them
# here, in one place, with the host object changed to match.
set(LK_WASM_LINK_FLAGS
  "-Wl,--import-memory -Wl,--shared-memory -Wl,--initial-memory=536870912 -Wl,--max-memory=2147483648")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${LK_WASM_LINK_FLAGS}")

# The module's entry exports beyond _start/main are declared at the
# consuming target (the engine exports its lk_* entry points and
# lithosys_thread_start); the toolchain file does not guess them.
