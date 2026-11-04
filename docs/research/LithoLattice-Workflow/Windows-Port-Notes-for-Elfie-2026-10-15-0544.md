# Windows port notes for Elfie (2026-10-15)

Tree: `R:\repos\LithoLattice-Workflow\LithoLattice-LithoKraft-Full-Source-2026-10-15` (unpacked from LithoLattice-LithoKraft-Full-Source-Package-2026-10-15.zip). Not a git repo; nothing committed or pushed.
Toolchain: Windows, Visual Studio 18 2026 Build Tools, MSVC 19.51, bundled CMake, x64.
Every edited file has a `<file>.orig-YYYY-MM-DD-HHMM` backup beside it. The diff below is against the pristine files extracted from the package zip. All build/test logs are kept in the tree root (`build-*.log`, `build-debug-*.log`, `ctest-*.log`, `selftest-*.log`, `run-*.log`, `scenario-hash-debug-2026-10-15.log`, `smoke-biome-capture-*.log`).

## Results (Debug, per your Windows brief; Release exe refuses to boot on the test key by design, not bypassed)

Commands: `cmake -S engine -B build-debug -DCMAKE_BUILD_TYPE=Debug -A x64`; `cmake --build build-debug --config Debug --parallel`; `ctest --test-dir build-debug -C Debug --output-on-failure` (serial).

- Build: Debug compiles and links with no errors. Release compiled too.
- ctest (Debug, serial): **41/48 pass, 7 fail** (manifest: 48/48 serial on Linux).
- `--selftest` (inside ctest #40, Debug): **1236 PASS / 2 FAIL** (manifest: 1239/0). The two failures are listed below. 1238 checks ran instead of 1239; I didn't find which check is missing.
- Scenario hash: `--handshake-scenario 2048` legacy, handshake 2w/4p, handshake 4w/4p all give **aa82e7afb1eff9d3**, which matches. (256-tick: ba9c8fd2d17d9020 on all four arms, consistent with each other.)

### ctest failures on Windows (Debug)
1. `engine_systerian` (39): `masters: missing file .../engine/src/systerian/../../../../user/files/cistercian/...svg`. The masters live outside the package (`user/files`). Environment issue, not a code issue.
2. `engine_litho_formula`: `lithoform_stamp: wafer.json not found for integration check`. Looks like a hardcoded/out-of-tree path. Same class as biome_capture's `/home/hatch/...` paths (see Smoke).
3. `engine_assets` (`sig: symlink fixture created`) and 4. `engine_storage` (6 sandbox/symlink checks): the tests could not create symlinks. Windows needs Developer Mode or admin for that. Environment issue; Developer Mode was not changed.
5. `engine_input`: `input pump: queue overflow drops the newest events and counts them`. Also fails in --selftest. The test sleeps 2.5 s expecting the pump thread to overrun the queue with 3000 events, but the pump does not deliver all 3000 in that window on Windows. My guess is Windows' ~15.6 ms sleep granularity. Not changed (it's timing/test design, your call).
6. `engine_console` (**Debug only**; passes in Release): 6 checks fail: cvars get/set/coercion, toggle, incrementvar, ++/--, incrementvar nan/inf, executeLine unterminated /*. Not diagnosed. Needs a look, possibly something that behaves differently under the MSVC debug CRT.
7. `lithokraft_selftest`: the 2 fails above (input pump, symlink fixture).

Fixed during the port: `engine_crypto` (selftest left a file open before remove_all), `engine_audio` + `lithokraft_selftest` (stack overflow 0xC00000FD), `lithokraft_screenshot_selftest` (8.3 short TEMP path).

Other Windows findings:
- In the Release build, the ctest `engine_sim` run hung for over 15 min after the /STACK change (it had passed in 3.4 s before), so I killed it. In Debug, `engine_sim` passes in 1.7 s. Not investigated further.
- A stale Debug `litholattice_tests.exe crypto` from the Oct 14 Alpha build (started 2026-10-14 3:28 PM) was still running and holding `%TEMP%\litho_sign_selftest\signature.json`. I killed it.
- `tools/build.py` is still Release-only. Debug was run with cmake/ctest directly.

## Software renderer
- Selection: `--renderer software|gl33|vulkan` (main.cpp:614). `software` is the default (main.cpp:465).
- Windowless smoke: `build-debug\Debug\biome_capture.exe <outdir>` (real worldgen + mesher + SoftwareRenderer, no window). Result: 7/7 PNGs written, 0 failures (the out dir must already exist; with a missing dir it reports "write failed").
  Caveat: biome_capture.cpp hardcodes `/home/hatch/workspace/litholattice/...` asset paths (lines 64, 100, 102). On Windows the shots come out with only ~5 distinct colours, so they are most likely untextured.
- No game window was opened. Not run: `--bench` (opens a window) and `--perf-headless` (windowless but long).

## Files changed, why, Linux/Mac impact

| File | Why | Linux/macOS impact |
|---|---|---|
| tools/build.py | Was hardcoded to "Visual Studio 17 2022". Now uses CMake's newest VS by default, overridable with `LL_CMAKE_GENERATOR` | None (Windows branch only) |
| engine/CMakeLists.txt | (a) engine/LithoKraft symlink unzips on Windows as an 18-byte file, so it falls back to ../game/LithoKraft. (b) MSVC-only `/STACK:8388608` (8 MB, same as the Linux default) | (a) none, the symlink still resolves as a directory. (b) inside `if(MSVC)` |
| engine/src/crypto/include/litholattice/crypto/u128.h (new) | MSVC has no `__uint128_t`. `ll_u128` = `unsigned __int128` on GCC/Clang, and a hi/lo struct on MSVC with only + - * >> << and the casts the code uses, wrapping mod 2^128 | None (same native type) |
| sha512.h, sha512.cpp, ed25519.cpp | `__uint128_t` -> `ll_u128` (token rename only) | None. Hash/signature output is bit-identical: MSVC passes the SHA-512 NIST and Ed25519 RFC 8032 vectors |
| engine/src/crypto/signing.cpp | POSIX headers wrapped in `#if defined(__unix__) \|\| defined(__APPLE__)`, matching the existing guard around their only use | None |
| engine/src/crypto/crypto_selftest.cpp | `f2.close()` before the later remove_all/rewrite. Windows can't delete an open file, so remove_all threw and aborted the test | None |
| engine/src/mcp/http_server.cpp | `#include <windows.h>` before `<bcrypt.h>` in the existing _WIN32 branch (bcrypt.h needs NTSTATUS etc.) | None |
| engine/src/procgen/struct_registry_selftest.cpp | `MaterialId{10 + i}` -> `static_cast<MaterialId>(10 + i)` (MSVC C2397 narrowing int->uint16_t) | None, same value |
| engine/src/render/binary_greedy_mesher.cpp | `__builtin_ctz/ctzll` -> `llCtz32/64`: _BitScanForward(64) on MSVC, original builtins on GCC/Clang. Inputs are always non-zero | None, same instructions |
| engine/src/render/include/.../software_shaders.h | `__builtin_sqrtf` -> `LL_SQRTF`: std::sqrt(float) on MSVC, original builtin otherwise | None |
| engine/src/storage/anvil_region.cpp | `fopen(path.c_str())` -> `fopen(path.string().c_str())`. On Windows, fs::path::c_str() is wchar_t. Same style as file_contract.cpp | None (string() is the native narrow path on POSIX) |
| engine/tests/engine_tests.cpp | `const uint8_t empty[] = {}` (zero-size array, a GNU extension) -> `empty[1] = {0}`. Only ever used as `span(empty, 0)` | None |
| game/.../app/selftest.cpp, game/.../game/game_ui_selftest.cpp, game/.../game/input_selftest.cpp | unistd.h for getpid: on _WIN32, `<process.h>` + `#define getpid _getpid` | None (#else keeps unistd.h) |
| game/.../game/screenshot_selftest.cpp | Test root now `fs::weakly_canonical(...)`, because resolveScreenshotPath canonicalizes the root. Windows TEMP is an 8.3 short path (JDWETW~1), which made every compare fail. Production code untouched | Linux: no-op. macOS: likely fixes the same mismatch (/var -> /private/var) |

Not changed: sim code, determinism paths, any pinned hash, the signing-key gate.
Not compiled here: GCC/Clang. Please confirm on Debian, since that is the real check of "no Linux impact".

## Exact diff (pristine package -> current)

```diff
diff --git a/win-port-scratch/pristine/tools/build.py b/tools/build.py
index 0a87a26..a225704 100644
--- a/win-port-scratch/pristine/tools/build.py
+++ b/tools/build.py
@@ -69,7 +69,7 @@ def main():
         os.makedirs(BUILD_DIR, exist_ok=True)
         cfg = ["cmake", "-S", ENGINE_SRC, "-B", BUILD_DIR]
         if is_win:
-            cfg += ["-G", "Visual Studio 17 2022", "-A", "x64"]
+            cfg += (["-G", os.environ["LL_CMAKE_GENERATOR"]] if os.environ.get("LL_CMAKE_GENERATOR") else []) + ["-A", "x64"]  # newest VS by default (VS 2022 or 2026)
         cfg += ["-DCMAKE_BUILD_TYPE=Release"]
         run(cfg)
 
diff --git a/win-port-scratch/pristine/engine/CMakeLists.txt b/engine/CMakeLists.txt
index 9588acf..cdbc268 100644
--- a/win-port-scratch/pristine/engine/CMakeLists.txt
+++ b/engine/CMakeLists.txt
@@ -117,6 +117,12 @@ if(LITHOLATTICE_SANITIZE AND NOT MSVC)
     message(FATAL_ERROR "LITHOLATTICE_SANITIZE must be address or thread")
   endif()
 endif()
+# Windows: the default stack is 1 MB (main thread and std::thread) vs 8 MB on Linux;
+# deep recursion in the audio tests and --selftest overflowed it. Match Linux's 8 MB.
+if(MSVC)
+  add_link_options(/STACK:8388608)
+endif()
+
 function(litholattice_module_options target)
   if(MSVC)
     target_compile_options(${target} PRIVATE /W4 /permissive- $<$<BOOL:${LITHOLATTICE_WERROR}>:/WX>)
@@ -166,4 +172,11 @@ option(LITHOLATTICE_BUILD_FUZZERS "Build the litholattice_fuzz harnesses (tools/
 if(LITHOLATTICE_BUILD_FUZZERS)
   add_subdirectory(tools/fuzz)
 endif()
-add_subdirectory(LithoKraft game)  # binary dir "game": the exe LithoKraft sits in the build root
+# Game source lives in ../game/LithoKraft. engine/LithoKraft is a symlink on Linux/macOS,
+# but Windows unzips it as a plain file, so fall back to the real path when it is not a dir.
+if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/LithoKraft")
+  set(LITHOKRAFT_GAME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/LithoKraft")
+else()
+  get_filename_component(LITHOKRAFT_GAME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../game/LithoKraft" ABSOLUTE)
+endif()
+add_subdirectory("${LITHOKRAFT_GAME_DIR}" game)  # binary dir "game": the exe LithoKraft sits in the build root
diff --git a/win-port-scratch/pristine/engine/src/crypto/include/litholattice/crypto/sha512.h b/engine/src/crypto/include/litholattice/crypto/sha512.h
index f061631..72af1e3 100644
--- a/win-port-scratch/pristine/engine/src/crypto/include/litholattice/crypto/sha512.h
+++ b/engine/src/crypto/include/litholattice/crypto/sha512.h
@@ -7,6 +7,8 @@
 #include <cstddef>
 #include <cstdint>
 
+#include "litholattice/crypto/u128.h"
+
 namespace litholattice::crypto {
 
 class Sha512 {
@@ -17,7 +19,7 @@ class Sha512 {
 
  private:
   uint64_t h_[8];
-  __uint128_t total_ = 0;  // bytes fed so far
+  ll_u128 total_ = 0;  // bytes fed so far
   uint8_t buf_[128];
   size_t buflen_ = 0;
   void compress(const uint8_t block[128]);
diff --git a/win-port-scratch/pristine/engine/src/crypto/sha512.cpp b/engine/src/crypto/sha512.cpp
index 57de4c3..b6e1ac5 100644
--- a/win-port-scratch/pristine/engine/src/crypto/sha512.cpp
+++ b/engine/src/crypto/sha512.cpp
@@ -100,7 +100,7 @@ void Sha512::update(const uint8_t* data, size_t len) {
 }
 
 std::array<uint8_t, 64> Sha512::final() {
-  const __uint128_t bitlen = total_ * 8;
+  const ll_u128 bitlen = total_ * 8;
   uint8_t pad = 0x80;
   update(&pad, 1);
   uint8_t zero = 0x00;
diff --git a/win-port-scratch/pristine/engine/src/crypto/ed25519.cpp b/engine/src/crypto/ed25519.cpp
index 7d3be7a..8a13f43 100644
--- a/win-port-scratch/pristine/engine/src/crypto/ed25519.cpp
+++ b/engine/src/crypto/ed25519.cpp
@@ -22,19 +22,19 @@ constexpr uint64_t kP[4] = {0xffffffffffffffedULL, 0xffffffffffffffffULL,
 // Reduce 8-limb (512-bit) t mod p into r. Uses 2^256 = 38 (mod p).
 void fe_reduce(Fe& r, const uint64_t t[8]) {
   uint64_t w[4];
-  __uint128_t carry = 0;
+  ll_u128 carry = 0;
   for (int i = 0; i < 4; ++i) {
-    __uint128_t s = (__uint128_t)t[i] + (__uint128_t)t[i + 4] * 38 + (uint64_t)carry;
+    ll_u128 s = (ll_u128)t[i] + (ll_u128)t[i + 4] * 38 + (uint64_t)carry;
     w[i] = (uint64_t)s;
     carry = s >> 64;
   }
   // carry * 2^256 = carry * 38
-  __uint128_t c = carry * 38;
-  __uint128_t s = (__uint128_t)w[0] + (uint64_t)c;
+  ll_u128 c = carry * 38;
+  ll_u128 s = (ll_u128)w[0] + (uint64_t)c;
   w[0] = (uint64_t)s;
   uint64_t cc = (uint64_t)(s >> 64) + (uint64_t)(c >> 64);
   for (int i = 1; i < 4 && cc; ++i) {
-    __uint128_t s2 = (__uint128_t)w[i] + cc;
+    ll_u128 s2 = (ll_u128)w[i] + cc;
     w[i] = (uint64_t)s2;
     cc = (uint64_t)(s2 >> 64);
   }
@@ -44,7 +44,7 @@ void fe_reduce(Fe& r, const uint64_t t[8]) {
     if (!ge) break;
     uint64_t borrow = 0;
     for (int i = 0; i < 4; ++i) {
-      __uint128_t d = (__uint128_t)w[i] - kP[i] - borrow;
+      ll_u128 d = (ll_u128)w[i] - kP[i] - borrow;
       w[i] = (uint64_t)d;
       borrow = (uint64_t)(d >> 64) & 1u;
     }
@@ -56,7 +56,7 @@ void fe_add(Fe& r, const Fe& a, const Fe& b) {
   uint64_t t[8] = {0, 0, 0, 0, 0, 0, 0, 0};
   uint64_t carry = 0;
   for (int i = 0; i < 4; ++i) {
-    __uint128_t s = (__uint128_t)a.w[i] + b.w[i] + carry;
+    ll_u128 s = (ll_u128)a.w[i] + b.w[i] + carry;
     t[i] = (uint64_t)s;
     carry = (uint64_t)(s >> 64);
   }
@@ -72,7 +72,7 @@ void fe_sub(Fe& r, const Fe& a, const Fe& b) {
   {
     uint64_t borrow = 0;
     for (int i = 0; i < 4; ++i) {
-      __uint128_t d = (__uint128_t)pb.w[i] - b.w[i] - borrow;
+      ll_u128 d = (ll_u128)pb.w[i] - b.w[i] - borrow;
       negb.w[i] = (uint64_t)d;
       borrow = (uint64_t)(d >> 64) & 1u;
     }
@@ -85,7 +85,7 @@ void fe_mul(Fe& r, const Fe& a, const Fe& b) {
   for (int i = 0; i < 4; ++i) {
     uint64_t carry = 0;
     for (int j = 0; j < 4; ++j) {
-      __uint128_t s = (__uint128_t)a.w[i] * b.w[j] + t[i + j] + carry;
+      ll_u128 s = (ll_u128)a.w[i] * b.w[j] + t[i + j] + carry;
       t[i + j] = (uint64_t)s;
       carry = (uint64_t)(s >> 64);
     }
@@ -342,7 +342,7 @@ int cmp512(const uint64_t a[8], const uint64_t b[8]) {
 void sub512(uint64_t r[8], const uint64_t a[8], const uint64_t b[8]) {
   uint64_t borrow = 0;
   for (int i = 0; i < 8; ++i) {
-    __uint128_t d = (__uint128_t)a[i] - b[i] - borrow;
+    ll_u128 d = (ll_u128)a[i] - b[i] - borrow;
     r[i] = (uint64_t)d;
     borrow = (uint64_t)(d >> 64) & 1u;
   }
@@ -376,7 +376,7 @@ void scalar_add(Scalar& r, const Scalar& a, const Scalar& b) {
   uint8_t buf[64] = {0};
   uint64_t carry = 0;
   for (int i = 0; i < 4; ++i) {
-    __uint128_t s = (__uint128_t)a.w[i] + b.w[i] + carry;
+    ll_u128 s = (ll_u128)a.w[i] + b.w[i] + carry;
     uint64_t lo = (uint64_t)s;
     carry = (uint64_t)(s >> 64);
     for (int j = 0; j < 8; ++j) buf[i * 8 + j] = (uint8_t)(lo >> (8 * j));
@@ -390,7 +390,7 @@ void scalar_mul(Scalar& r, const Scalar& a, const Scalar& b) {
   for (int i = 0; i < 4; ++i) {
     uint64_t carry = 0;
     for (int j = 0; j < 4; ++j) {
-      __uint128_t s = (__uint128_t)a.w[i] * b.w[j] + t[i + j] + carry;
+      ll_u128 s = (ll_u128)a.w[i] * b.w[j] + t[i + j] + carry;
       t[i + j] = (uint64_t)s;
       carry = (uint64_t)(s >> 64);
     }
diff --git a/win-port-scratch/pristine/engine/src/crypto/signing.cpp b/engine/src/crypto/signing.cpp
index 65f9c1e..c43fac9 100644
--- a/win-port-scratch/pristine/engine/src/crypto/signing.cpp
+++ b/engine/src/crypto/signing.cpp
@@ -3,9 +3,11 @@
 
 #include <algorithm>
 #include <array>
+#if defined(__unix__) || defined(__APPLE__)
 #include <fcntl.h>
 #include <sys/stat.h>
 #include <unistd.h>
+#endif
 #include <cstdio>
 #include <cstring>
 #include <fstream>
diff --git a/win-port-scratch/pristine/engine/src/crypto/crypto_selftest.cpp b/engine/src/crypto/crypto_selftest.cpp
index dd8d083..f8dd211 100644
--- a/win-port-scratch/pristine/engine/src/crypto/crypto_selftest.cpp
+++ b/engine/src/crypto/crypto_selftest.cpp
@@ -167,6 +167,7 @@ void testSigning(Checker& check) {
   check(signMod(dir, seed.data(), &err), "signing: re-sign works: " + err);
   std::ifstream f2(dir / "signature.json", std::ios::binary);
   const std::string sig2((std::istreambuf_iterator<char>(f2)), std::istreambuf_iterator<char>());
+  f2.close();  // Windows cannot delete a file that is still open (remove_all at the end)
   check(sig1 == sig2 && !sig1.empty(), "signing: deterministic (identical signature.json)");
 
   check(verifyMod(dir, pub.data(), &err), "signing: verify with correct key: " + err);
diff --git a/win-port-scratch/pristine/engine/src/mcp/http_server.cpp b/engine/src/mcp/http_server.cpp
index 3c4e3eb..e468d22 100644
--- a/win-port-scratch/pristine/engine/src/mcp/http_server.cpp
+++ b/engine/src/mcp/http_server.cpp
@@ -14,6 +14,7 @@
 #ifndef WIN32_LEAN_AND_MEAN
 #define WIN32_LEAN_AND_MEAN
 #endif
+#include <windows.h>  // bcrypt.h needs NTSTATUS/ULONG etc.
 #include <bcrypt.h>
 #pragma comment(lib, "bcrypt.lib")
 #else
diff --git a/win-port-scratch/pristine/engine/src/procgen/struct_registry_selftest.cpp b/engine/src/procgen/struct_registry_selftest.cpp
index 4a61552..5f9cdae 100644
--- a/win-port-scratch/pristine/engine/src/procgen/struct_registry_selftest.cpp
+++ b/engine/src/procgen/struct_registry_selftest.cpp
@@ -114,7 +114,7 @@ void variantChecks(Check& check) {
     for (int i = 0; i < 3; ++i) {
       StructureFeature::Variant v;
       v.cells = testCells();
-      v.cells.front().material = MaterialId{10 + i};  // tag each variant
+      v.cells.front().material = static_cast<MaterialId>(10 + i);  // tag each variant
       v.weight = float(i + 1);                        // weights 1, 2, 3
       variants.push_back(std::move(v));
     }
diff --git a/win-port-scratch/pristine/engine/src/render/binary_greedy_mesher.cpp b/engine/src/render/binary_greedy_mesher.cpp
index b145bb7..acaac59 100644
--- a/win-port-scratch/pristine/engine/src/render/binary_greedy_mesher.cpp
+++ b/engine/src/render/binary_greedy_mesher.cpp
@@ -13,11 +13,26 @@
 #include "litholattice/render/binary_greedy_mesher.h"
 #include "litholattice/world/block.h"
 
+#if defined(_MSC_VER) && !defined(__clang__)
+#include <intrin.h>
+#endif
+
 namespace litholattice {
 namespace {
 
 constexpr int kN = 16;
 
+// Count-trailing-zeros helpers. GCC/Clang keep the original builtins; MSVC
+// uses the equivalent _BitScanForward intrinsics. Callers only pass non-zero
+// values, so results are identical.
+#if defined(_MSC_VER) && !defined(__clang__)
+inline int llCtz32(uint32_t v) { unsigned long i; _BitScanForward(&i, v); return int(i); }
+inline int llCtz64(uint64_t v) { unsigned long i; _BitScanForward64(&i, v); return int(i); }
+#else
+inline int llCtz32(uint32_t v) { return __builtin_ctz(v); }
+inline int llCtz64(uint64_t v) { return __builtin_ctzll(v); }
+#endif
+
 // Plane coords (p, q) -> voxel for axis a (0=X, 1=Y, 2=Z), slice i.
 inline void planeToVoxel(int a, int i, int p, int q, int& x, int& y, int& z) {
   if (a == 0) { x = i; y = p; z = q; }
@@ -87,7 +102,7 @@ void mergeSlice(int a, int d, int i, const uint64_t in[4], const uint8_t* materi
   for (int q = 0; q < kN; ++q) {
     uint32_t row = rowOf(q);
     while (row) {
-      const int p0 = __builtin_ctz(row);
+      const int p0 = llCtz32(row);
       const uint8_t m0 = matOf(material, a, i, p0, q);
       int p1 = p0;  // expand along the row while bits are set and material matches
       while (p1 + 1 < kN && (row & (1u << (p1 + 1))) && matOf(material, a, i, p1 + 1, q) == m0) ++p1;
@@ -133,7 +148,7 @@ void cullSameTransparentFaces(const MeshInput& in, const BlockRegistry& reg, Fac
         for (int w = 0; w < 4; ++w) {
           uint64_t word = m.face[a][d][i][w];
           while (word) {
-            const int b = __builtin_ctzll(word);
+            const int b = llCtz64(word);
             word &= word - 1;
             const int bit = (w << 6) + b;
             int x, y, z;
diff --git a/win-port-scratch/pristine/engine/src/render/include/litholattice/render/software_shaders.h b/engine/src/render/include/litholattice/render/software_shaders.h
index 3579d62..8f7663e 100644
--- a/win-port-scratch/pristine/engine/src/render/include/litholattice/render/software_shaders.h
+++ b/engine/src/render/include/litholattice/render/software_shaders.h
@@ -4,6 +4,12 @@
 // Enough for Chromebooks and GPU-less machines to run the game.
 #pragma once
 #include <cstdint>
+#if defined(_MSC_VER) && !defined(__clang__)
+#include <cmath>
+#define LL_SQRTF(x) std::sqrt(static_cast<float>(x))  // MSVC: no __builtin_sqrtf; IEEE sqrt is correctly rounded either way
+#else
+#define LL_SQRTF(x) __builtin_sqrtf(x)
+#endif
 
 namespace litholattice {
 
@@ -69,7 +75,7 @@ inline void applyBumpMap(const FragmentInput& in, float u, float v, uint32_t lay
   *nx -= dx; *ny -= dy;
   // Renormalize
   float len = *nx**nx + *ny**ny + *nz**nz;
-  if (len > 1e-6f) { len = 1.0f / __builtin_sqrtf(len); *nx*=len; *ny*=len; *nz*=len; }
+  if (len > 1e-6f) { len = 1.0f / LL_SQRTF(len); *nx*=len; *ny*=len; *nz*=len; }
 }
 
 /// Basic shader: diffuse + ambient + texture.
diff --git a/win-port-scratch/pristine/engine/src/storage/anvil_region.cpp b/engine/src/storage/anvil_region.cpp
index 8cc8581..90060e5 100644
--- a/win-port-scratch/pristine/engine/src/storage/anvil_region.cpp
+++ b/engine/src/storage/anvil_region.cpp
@@ -246,12 +246,12 @@ bool AnvilRegion::open(std::string* err) {
     opened_ = false;
     return false;
   };
-  f_ = std::fopen(path_.c_str(), "r+b");
+  f_ = std::fopen(path_.string().c_str(), "r+b");
   if (!f_) {
     // Never truncate a file we could not read: only create when it does not exist.
     std::error_code ec;
     if (std::filesystem::exists(path_, ec)) return fail("cannot open region file for update");
-    f_ = std::fopen(path_.c_str(), "w+b");
+    f_ = std::fopen(path_.string().c_str(), "w+b");
     if (!f_) return fail("cannot create region file '" + path_.string() + "'");
   }
   uint64_t size = fileSize(f_);
@@ -346,7 +346,7 @@ AnvilRegion::Staged AnvilRegion::stage(int lx, int lz, uint8_t codec,
     // so the same payload-first ordering protects the external path.
     const auto rec = buildRecord(codec, payload);
     const auto sp = sidecarPath(path_, rx_ * kSpan + lx, rz_ * kSpan + lz);
-    std::FILE* sf = std::fopen(sp.c_str(), "wb");
+    std::FILE* sf = std::fopen(sp.string().c_str(), "wb");
     if (!sf) {
       if (err) *err = "cannot write sidecar '" + sp.string() + "'";
       return none;
@@ -403,7 +403,7 @@ bool AnvilRegion::commit(int lx, int lz, uint8_t codec, const Staged& s, uint32_
     bool ok = false;
     if (s.external) {
       const auto sp = sidecarPath(path_, rx_ * kSpan + lx, rz_ * kSpan + lz);
-      std::FILE* sf = std::fopen(sp.c_str(), "rb");
+      std::FILE* sf = std::fopen(sp.string().c_str(), "rb");
       if (sf) {
         const uint64_t n = fileSize(sf);
         if (n <= 64 * 1024 * 1024 && seekAbs(sf, 0)) {
@@ -526,7 +526,7 @@ std::optional<AnvilRegion::Record> AnvilRegion::readInline(uint32_t sector, uint
 std::optional<AnvilRegion::Record> AnvilRegion::readExternal(int lx, int lz, uint32_t gen,
                                                             std::string* err) const {
   const auto sp = sidecarPath(path_, rx_ * kSpan + lx, rz_ * kSpan + lz);
-  std::FILE* sf = std::fopen(sp.c_str(), "rb");
+  std::FILE* sf = std::fopen(sp.string().c_str(), "rb");
   if (!sf) {
     if (err) *err = "external sidecar missing: '" + sp.string() + "'";
     return std::nullopt;
diff --git a/win-port-scratch/pristine/engine/tests/engine_tests.cpp b/engine/tests/engine_tests.cpp
index f52292e..632b71b 100644
--- a/win-port-scratch/pristine/engine/tests/engine_tests.cpp
+++ b/engine/tests/engine_tests.cpp
@@ -7381,7 +7381,7 @@ void testNetHandshake() {
   {
     bool ok = true;
     const uint8_t v1[] = {1}, v2[] = {2}, v12[] = {1, 2}, v212[] = {2, 1, 2}, v21[] = {2, 1};
-    const uint8_t empty[] = {};
+    const uint8_t empty[1] = {0};  // zero-length arrays are a GNU extension; only used as span(empty, 0)
     auto same = negotiate(v2, v2);
     ok = ok && same.ok && same.version == 2;  // same version
     auto hi = negotiate(v12, v12);
diff --git a/win-port-scratch/pristine/game/LithoKraft/src/app/selftest.cpp b/game/LithoKraft/src/app/selftest.cpp
index 6376e74..bdd80d7 100644
--- a/win-port-scratch/pristine/game/LithoKraft/src/app/selftest.cpp
+++ b/game/LithoKraft/src/app/selftest.cpp
@@ -5,7 +5,12 @@
 #include <chrono>
 #include <stdexcept>
 #include <cstdio>
+#if defined(_WIN32)
+#include <process.h>  // _getpid (Windows has no unistd.h)
+#define getpid _getpid
+#else
 #include <unistd.h>  // getpid: per-run unique temp dirs (G-M7)
+#endif
 #include <cstring>
 #include <filesystem>
 #include <fstream>
diff --git a/win-port-scratch/pristine/game/LithoKraft/src/game/game_ui_selftest.cpp b/game/LithoKraft/src/game/game_ui_selftest.cpp
index 7420a30..6f8ef42 100644
--- a/win-port-scratch/pristine/game/LithoKraft/src/game/game_ui_selftest.cpp
+++ b/game/LithoKraft/src/game/game_ui_selftest.cpp
@@ -5,7 +5,12 @@
 #include <fstream>
 #include <sstream>
 #include <string>
+#if defined(_WIN32)
+#include <process.h>  // _getpid (Windows has no unistd.h)
+#define getpid _getpid
+#else
 #include <unistd.h>  // getpid: per-run unique temp dirs (G-M7, P2b-2)
+#endif
 #include "lithokraft/game/game_ui.h"
 #include "lithokraft/content/items.h"  // kItemFragment / kItemBashor / itemIconRef (P2b-1 HUD binding)
 #include "lithokraft/game/script_editor.h"
diff --git a/win-port-scratch/pristine/game/LithoKraft/src/game/input_selftest.cpp b/game/LithoKraft/src/game/input_selftest.cpp
index 3683692..730d9b3 100644
--- a/win-port-scratch/pristine/game/LithoKraft/src/game/input_selftest.cpp
+++ b/game/LithoKraft/src/game/input_selftest.cpp
@@ -6,7 +6,12 @@
 #include <cstdio>
 #include <filesystem>
 #include <string>
+#if defined(_WIN32)
+#include <process.h>  // _getpid (Windows has no unistd.h)
+#define getpid _getpid
+#else
 #include <unistd.h>  // getpid: per-run unique temp dirs (G-M7, P2b-2)
+#endif
 #include "litholattice/input/input_system.h"
 #include "lithokraft/game/actions.h"
 
diff --git a/win-port-scratch/pristine/game/LithoKraft/src/game/screenshot_selftest.cpp b/game/LithoKraft/src/game/screenshot_selftest.cpp
index 1bf0869..46984bd 100644
--- a/win-port-scratch/pristine/game/LithoKraft/src/game/screenshot_selftest.cpp
+++ b/game/LithoKraft/src/game/screenshot_selftest.cpp
@@ -362,8 +362,11 @@ void testSessionTier() {
 // -- Screenshot path confinement (P1b, App. F M6) -------------------------------
 void testPathConfinement() {
   namespace fs = std::filesystem;
-  const fs::path root =
-      fs::temp_directory_path() / "lithokraft-shot-selftest-root" / "screenshots";
+  // weakly_canonical: resolveScreenshotPath canonicalizes the root, so compare against the
+  // same form. Windows' TEMP is often an 8.3 short path (JDWETW~1) and macOS's /var is a
+  // symlink to /private/var; on Linux /tmp this is a no-op.
+  const fs::path root = fs::weakly_canonical(
+      fs::temp_directory_path() / "lithokraft-shot-selftest-root" / "screenshots");
   auto insideRoot = [&](const fs::path& p) {
     auto ri = root.begin();
     auto pi = p.begin();
diff --git a/engine/src/crypto/include/litholattice/crypto/u128.h b/engine/src/crypto/include/litholattice/crypto/u128.h
new file mode 100644
index 0000000..3765649
--- /dev/null
+++ b/engine/src/crypto/include/litholattice/crypto/u128.h
@@ -0,0 +1,61 @@
+// Portable unsigned 128-bit integer for the crypto code.
+// GCC/Clang: the native unsigned __int128 (unchanged behaviour).
+// MSVC (no native 128-bit type): a minimal hi/lo struct with exactly the
+// operations sha512.cpp / ed25519.cpp use; results are bit-identical
+// (mod 2^128 wraparound semantics, same as the native type).
+#pragma once
+
+#include <cstdint>
+#include <type_traits>
+
+namespace litholattice::crypto {
+
+#if defined(_MSC_VER) && !defined(__clang__)
+struct ll_u128 {
+  uint64_t lo = 0, hi = 0;
+  constexpr ll_u128() = default;
+  constexpr ll_u128(uint64_t v) : lo(v), hi(0) {}  // NOLINT(implicit)
+  constexpr ll_u128(uint64_t h, uint64_t l) : lo(l), hi(h) {}
+  template <class T, class = std::enable_if_t<std::is_integral_v<T>>>
+  constexpr explicit operator T() const { return static_cast<T>(lo); }
+
+  friend constexpr ll_u128 operator+(ll_u128 a, ll_u128 b) {
+    const uint64_t l = a.lo + b.lo;
+    return ll_u128(a.hi + b.hi + (l < a.lo ? 1u : 0u), l);
+  }
+  friend constexpr ll_u128 operator-(ll_u128 a, ll_u128 b) {
+    const uint64_t l = a.lo - b.lo;
+    return ll_u128(a.hi - b.hi - (a.lo < b.lo ? 1u : 0u), l);
+  }
+  static constexpr ll_u128 mul64(uint64_t a, uint64_t b) {
+    const uint64_t a0 = a & 0xffffffffu, a1 = a >> 32;
+    const uint64_t b0 = b & 0xffffffffu, b1 = b >> 32;
+    const uint64_t p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
+    const uint64_t mid = (p00 >> 32) + (p01 & 0xffffffffu) + (p10 & 0xffffffffu);
+    return ll_u128(p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32),
+                   (mid << 32) | (p00 & 0xffffffffu));
+  }
+  friend constexpr ll_u128 operator*(ll_u128 a, ll_u128 b) {
+    ll_u128 r = mul64(a.lo, b.lo);
+    r.hi += a.lo * b.hi + a.hi * b.lo;  // mod 2^128
+    return r;
+  }
+  friend constexpr ll_u128 operator>>(ll_u128 a, int s) {
+    if (s <= 0) return a;
+    if (s >= 128) return ll_u128();
+    if (s >= 64) return ll_u128(0, a.hi >> (s - 64));
+    return ll_u128(a.hi >> s, (a.lo >> s) | (a.hi << (64 - s)));
+  }
+  friend constexpr ll_u128 operator<<(ll_u128 a, int s) {
+    if (s <= 0) return a;
+    if (s >= 128) return ll_u128();
+    if (s >= 64) return ll_u128(a.lo << (s - 64), 0);
+    return ll_u128((a.hi << s) | (a.lo >> (64 - s)), a.lo << s);
+  }
+  ll_u128& operator+=(ll_u128 b) { return *this = *this + b; }
+};
+#else
+using ll_u128 = unsigned __int128;
+#endif
+
+}  // namespace litholattice::crypto
\ No newline at end of file

```

## Update 2026-10-15 0807 EDT: signing key retired, Release boots, hitching
- The retired test key (3bb44b55...) is no longer baked in. kBakedPublicKeyHex is now JD's local dev key 04c1766e...011c (mod_signature.h). Elfie: anything you sign with the old test key will now verify as Invalid. Either ask JD to sign with the dev key (the private key stays on TheWorkstation, outside the tree), or generate your own with `litho_sign keygen --out <prefix>` and bake its .pub hex in (as hex, the .pub file is 32 raw bytes) for your local builds. The release FATAL check is unchanged.
- biome_capture.cpp: the source root is resolved portably (hatch path first, then LITHO_SOURCE_ROOT, then walk up from the cwd/exe).
- Hitching: the stream pump does sim_->stop()/start() on every insert/evict batch, so achieved TPS collapses while streaming (Release 5.5/512 on the stream bench, 469.5/512 on orbit). Software render 134 ms/frame at 720p in Release. Details: Hitching-Diagnosis-2026-10-15-0807.md.


## Update 2026-10-15 0841 EDT: streaming no longer stops the sim
- Game::pumpStreaming and evictFarColumns now use the new SimThread::holdBetweenTicks() (the existing blockMutex_) instead of sim_->stop()/start() (a sim thread join plus a handshake worker join/respawn on every batch). stop() remains the fallback when the handshake is frozen or poisoned. The post-tick flushDirty/stats section of the sim loop is now also under blockMutex_. The change is the same on every compiler.
- Release stream bench: 5.5 -> 512.0 TPS at 512 and 0.2 -> 32.0 at 32. Hash aa82e7afb1eff9d3 unchanged. ctest 41/48 (same as baseline). See ChunkLoad-Sim-Fix-2026-10-15-0841.md. JD wants to discuss handshake-coupled streaming: see Handshake-Chunk-Streaming-2026-10-15-0841.md.


## Update 2026-10-15 09:43 EDT: alt-tab recapture + console dangling pointer
- **Console** (engine/src/console/concmd.cpp, ConsoleRegistry::coerce Int/Float): strtol/strtod parsed `std::string(raw).c_str()`, so `end` pointed into a destroyed temporary (ASan stack-use-after-scope; failed only in Debug). Now `const std::string tmp(raw);` and parse `tmp.c_str()`.
- **Alt-tab** (JD keeps auto-pause on focus loss):
  - glfw_input_backend.h/.cpp: the focus callback counts lost/gained edges. Window::takeFocusEdges() reads them, so the frame loop sees every alt-tab even if away-and-back falls between two frames. The old attribute poll remains as a fallback.
  - window.h/.cpp: Window::cursorReallyCaptured() = glfwGetInputMode(GLFW_CURSOR)==DISABLED. main.cpp compares the game's wanted capture against this real mode (not the cached flag) and re-applies on focus gain.
  - game.cpp/game.h: onFocusChanged sets pausedByFocus_. While it is set, a left click in the world that no UI widget consumed resumes and recaptures; that click does not break a block. handlePause: Esc toggles (Esc while MenuPaused resumes). The Resume button and menus are unchanged.
- Backups: .orig-2026-10-15-0928 (concmd.cpp), .orig-2026-10-15-0935 (the rest). Portable: GLFW + std only.

## Update 2026-10-15 12:22 EDT: review fixes + MT software raster + playtest fixes
See Elfie-Handoff-2026-10-15-1222.md and CHANGES.md in LithoLattice-LithoKraft-Elfie-Handoff-2026-10-15-1222. Summary: High review findings fixed except SIM-H1 (reverted, engine_sim timeout) and SIM-H2/PG-H1 (deferred). The software renderer runs multithreaded (default 4 threads, --render-threads), pixel-identical, at 40.7 FPS at 720p / 29.4 FPS at 1080p borderless with 8 threads at 512 TPS. Hash aa82e7afb1eff9d3 unchanged. Also fixed: alt-tab click bug, near dither off by default, F7 goes to 512 TPS, borderless is the default display mode.
