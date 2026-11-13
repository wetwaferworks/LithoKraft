// lkboot.js — the LithoKraft thin layer (hand-written, Wet Wafer Works).
//
// This file REPLACES the generated Emscripten glue (the old LithoKraft.js)
// in everything the pages and the Android shell serve. JD's ruling
// (2026-10-20): Emscripten is removed from the CEF layer completely — no
// generated glue, no PROXY_TO_PTHREAD, no OffscreenCanvas transfer. What
// remains of the toolchain is only the compiler: the WASM binary is still
// produced by emcc (see wasm/build-thin.sh for the exact link line), and
// this loader implements the binary's declared import surface itself:
//
//   - boot/instantiate + argv + the rAF main loop, on the browser MAIN
//     thread (present = the page compositor, no canvas hand-off hop);
//   - an in-memory filesystem fed by our own pack format (lkpack.py);
//   - the Linux-flavored syscalls + WASI fd layer over that FS;
//   - a pthread pool speaking a small postMessage protocol (this same
//     file is the worker bootstrap — it detects its context);
//   - the mailbox proxy protocol the binary's own code expects for
//     main-thread services (syscalls, DOM, EM_ASM) from worker threads;
//   - EM_ASM bodies re-built at runtime from the strings in the binary
//     (they are OUR source, embedded by the compiler verbatim);
//   - a state-caching WebGL2 wrapper implementing the binary's imported
//     GL functions with redundant state traffic removed (clause two);
//   - lkio_present, our thin present verb, finally on the real path.
//
// Design notes live with the code. Everything here is original; the
// interface contracts (import names, struct layouts, message shapes the
// binary's compiled code depends on) were studied from the artifacts the
// old build produced, then implemented clean-room.
'use strict';

/* ============================== context ============================== */

const LK_IS_WORKER = typeof window === 'undefined' && typeof self !== 'undefined' &&
                     typeof self.postMessage === 'function';
const LK_BOOT_URL = LK_IS_WORKER ? self.location.href
                                 : (document.currentScript && document.currentScript.src) || location.href;

// Errno values are the WASI numbering the binary's libc expects
// (ENOENT=44, EBADF=8, ...). Syscalls return -errno; WASI returns +errno.
const LK_E = { PERM: 63, NOENT: 44, INTR: 27, IO: 29, BADF: 8, AGAIN: 6, NOMEM: 48,
               ACCES: 2, FAULT: 21, EXIST: 20, NOTDIR: 54, ISDIR: 31, INVAL: 28,
               NFILE: 41, MFILE: 33, NAMETOOLONG: 37, NOSPC: 51, NOSYS: 52,
               NOTEMPTY: 55, ROFS: 68, NOTTY: 59, PIPE: 64, RANGE: 67, LOOP: 32,
               XDEV: 76, FBIG: 22, SPIPE: 69, DOM: 18, DEADLK: 16, NOLCK: 46,
               NOTSUP: 58, OVERFLOW: 61, TIMEDOUT: 73, BUSY: 10, MLINK: 34 };

const LK_UNWIND = { lkUnwind: true };   // thrown by set_main_loop to unwind main()
const LK_EXIT = { lkExit: true };       // thrown by exit paths

function lkIsUnwind(e) { return e === LK_UNWIND || e === 'unwind'; }

/* ========================= per-context runtime ======================= */
// One of these exists in the page (main) and one in each pthread worker.
// The main context additionally owns the filesystem, canvas and GL.

function lkNewContext(isWorker) {
  return {
    isWorker,
    module: null,          // WebAssembly.Module
    memory: null,          // WebAssembly.Memory (shared)
    instance: null,
    exports: null,
    exited: false,
    exitCode: 0,
    mainLoop: null,        // {cb, arg} once the binary registers it
    asmCache: new Map(),   // EM_ASM address -> compiled JS function
    strCache: new Map(),   // static C strings handed to the binary (GL etc.)
    currentProxiedCaller: 0,
    printBuffer: '',       // stdout line buffer (main ctx sink)
    printErrBuffer: '',
    // topology (v2): the PAGE context is the service thread — it never
    // runs engine main(). Engine main runs on a dedicated main worker;
    // pool workers run the remaining pthreads. GL calls from any worker
    // stream to the page through a shared-memory command ring and are
    // executed here, where the real WebGL2 context lives.
    role: isWorker ? 'pool' : 'page', // worker role refined at lk-load
    singleAgent: false,    // page only: the host could not obtain a
                           // cross-origin-isolated context, so it cannot
                           // provision ANY worker (the shared memory
                           // cannot be posted to one). The engine is
                           // multithreaded (std::thread pools from
                           // startup) and threads cannot be fabricated
                           // on the host thread, so the boot stops at
                           // the spawn stage with a standing blocked
                           // notice instead of dying on a DataCloneError
                           // or an engine abort (2026-10-21; evidence in
                           // the isolation-fix run summary)
    mainThreadPtr: 0,      // the binary's main-thread struct (from tb)
    glRing: null,          // {base, size} of the GL command ring
    rpcOff: 0,             // byte offset of the service-RPC slot in wasm memory
    mainWorker: null,      // page only: the main worker record
    mainLoopRemote: false, // page only: main worker registered its loop
    frameOutstanding: false,
    // main-context only:
    fs: null,              // filesystem root + fd table
    canvas: null,
    gl: null,              // GL wrapper state (created on first context)
    pool: null,            // pthread pool {byPtr: Map, idle: []}
  };
}

const LK = lkNewContext(LK_IS_WORKER);

// Boot progress marker: every stage of lkBootMain records itself here
// so a failure can NAME where it stopped (the loud-error rule — see the
// entry catch at the bottom of this file).
let LK_BOOT_STAGE = 'entry';
function lkBootStage(s) {
  LK_BOOT_STAGE = s;
  lkOut('lkboot: boot stage: ' + s);
}


/* ========================= memory + strings ========================== */
// Heap views are re-created on every access: the memory can grow, which
// detaches/replaces buffers, so caching a view is a bug waiting to bite.

// The synchronous service-RPC slot lives INSIDE the shared wasm
// memory (bug-fix run, 2026-10-20): a standalone `new SharedArrayBuffer`
// requires crossOriginIsolated, which the Android shell's intercepted
// origin did not provide on a real WebView (JD's device, shell v1.3:
// "ReferenceError: SharedArrayBuffer is not defined" at boot) — while
// the shared wasm memory itself, Atomics included, worked fine there.
// Layout matches the old SAB slot: state i32 @+0, tag i32 @+8,
// value (f64/u64) @+16. Views are built fresh per call so memory
// growth can never stale them.
function lkRpcViews() {
  const buf = LK.memory.buffer, off = LK.rpcOff;
  return {
    i32: new Int32Array(buf, off, 16),
    f64: new Float64Array(buf, off, 8),
    u64: new BigUint64Array(buf, off, 8),
  };
}

function lkU8()  { return new Uint8Array(LK.memory.buffer); }
function lkI8()  { return new Int8Array(LK.memory.buffer); }
function lkU16() { return new Uint16Array(LK.memory.buffer); }
function lkI32() { return new Int32Array(LK.memory.buffer); }
function lkU32() { return new Uint32Array(LK.memory.buffer); }
function lkF32() { return new Float32Array(LK.memory.buffer); }
function lkF64() { return new Float64Array(LK.memory.buffer); }
function lkI64() { return new BigInt64Array(LK.memory.buffer); }
function lkU64() { return new BigUint64Array(LK.memory.buffer); }

const lkTextDecoder = new TextDecoder('utf-8');
const lkTextEncoder = new TextEncoder();

function lkReadCStr(ptr, maxLen) {
  if (!ptr) return '';
  const u8 = lkU8();
  let end = ptr;
  const limit = maxLen ? ptr + maxLen : u8.length;
  while (end < limit && u8[end] !== 0) end++;
  // TextDecoder refuses views backed by a SharedArrayBuffer (the wasm
  // memory is shared) — copy into a private buffer first. (Node's
  // decoder accepts shared views, which is why this only bites in the
  // browser; it was the first-browser-boot failure of the thin layer.)
  return lkTextDecoder.decode(new Uint8Array(u8.subarray(ptr, end)));
}
function lkLengthBytesUTF8(str) { return lkTextEncoder.encode(str).length; }
function lkWriteCStr(ptr, str, maxBytes) {
  const bytes = lkTextEncoder.encode(str);
  const n = maxBytes ? Math.min(bytes.length, maxBytes - 1) : bytes.length;
  lkU8().set(bytes.subarray(0, n), ptr);
  if (maxBytes) lkU8()[ptr + n] = 0;
  return n;
}
function lkUTF8ArrayToString(u8, ptr, len) {
  // Same shared-buffer rule as lkReadCStr: decode a private copy.
  return lkTextDecoder.decode(new Uint8Array(u8.subarray(ptr, ptr + len)));
}
// Emscripten-helper-shaped aliases used by EM_ASM bodies (our own code):
function UTF8ToString(ptr) { return lkReadCStr(ptr); }
function UTF8ArrayToString(u8, ptr, len) { return lkUTF8ArrayToString(u8, ptr, len); }
function stringToUTF8(str, ptr, maxBytes) { return lkWriteCStr(ptr, str, maxBytes) + 1; }
function stringToUTF8Array(str, u8, ptr, maxBytes) {
  const bytes = lkTextEncoder.encode(str);
  const n = Math.min(bytes.length, maxBytes - 1);
  u8.set(bytes.subarray(0, n), ptr);
  u8[ptr + n] = 0;
  return n + 1;
}
function lengthBytesUTF8(str) { return lkLengthBytesUTF8(str); }

// Static strings handed to the binary by pointer (GL strings and the
// like): allocated once in wasm memory and cached by content.
function lkStaticStr(str) {
  let ptr = LK.strCache.get(str);
  if (ptr === undefined) {
    const bytes = lkTextEncoder.encode(str);
    ptr = LK.exports.malloc(bytes.length + 1);
    lkU8().set(bytes, ptr);
    lkU8()[ptr + bytes.length] = 0;
    LK.strCache.set(str, ptr);
  }
  return ptr;
}

/* ============================ printing =============================== */
// fd 1/2 land here (on the main thread — worker writes are proxied).
// Line-buffered, mirroring the old runtime's behavior toward the page log.

function lkEmit(line, isErr) {
  if (LK.isWorker) return; // worker stdio is proxied to main; never direct
  const m = (typeof Module !== 'undefined') ? Module : null;
  if (isErr) { if (m && m.printErr) m.printErr(line); else console.error(line); }
  else { if (m && m.print) m.print(line); else console.log(line); }
}
function lkWriteSink(text, isErr) {
  const key = isErr ? 'printErrBuffer' : 'printBuffer';
  let buf = LK[key] + text;
  let idx;
  while ((idx = buf.indexOf('\n')) >= 0) {
    lkEmit(buf.slice(0, idx), isErr);
    buf = buf.slice(idx + 1);
  }
  LK[key] = buf;
}
function lkFlushSink(isErr) {
  const key = isErr ? 'printErrBuffer' : 'printBuffer';
  if (LK[key]) { lkEmit(LK[key], isErr); LK[key] = ''; }
}
function lkOut(str) { lkWriteSink(String(str) + '\n', false); }
function lkErr(str) { lkWriteSink(String(str) + '\n', true); }

/* ============================ filesystem ============================= */
// In-memory FS. Package files arrive as views into the fetched blob;
// the first write copies (copy-on-write), so the pack is never mutated.
// Directories are real nodes so getdents/mkdir/rename behave.

let lkInoSeq = 1;
function lkNodeFile(data) {
  return { type: 'file', data, owned: false, mode: 0o100644, ino: lkInoSeq++, mtime: Date.now() };
}
function lkNodeDir() {
  return { type: 'dir', children: new Map(), mode: 0o40755, ino: lkInoSeq++, mtime: Date.now() };
}
function lkNodeLink(target) {
  return { type: 'link', target, mode: 0o120777, ino: lkInoSeq++, mtime: Date.now() };
}
function lkNodeTty(kind) { // kind: 'in' | 'out' | 'err'
  return { type: 'tty', kind, mode: 0o20666, ino: lkInoSeq++, mtime: Date.now() };
}
function lkNodeNull() {
  return { type: 'null', mode: 0o20666, ino: lkInoSeq++, mtime: Date.now() };
}

function lkBuildFS(pack) {
  const root = lkNodeDir();
  const fs = { root, fds: new Map(), nextFd: 3, blob: pack.blob };
  const ensureDir = (path) => {
    let node = root;
    for (const part of path.split('/').filter(Boolean)) {
      let next = node.children.get(part);
      if (!next) { next = lkNodeDir(); node.children.set(part, next); }
      node = next;
    }
    return node;
  };
  for (const d of pack.dirs) ensureDir(d);
  for (const f of pack.files) {
    const parentPath = f.path.slice(0, f.path.lastIndexOf('/')) || '/';
    const parent = ensureDir(parentPath);
    const name = f.path.slice(f.path.lastIndexOf('/') + 1);
    const data = pack.blob.subarray(pack.payloadStart + f.start, pack.payloadStart + f.start + f.len);
    parent.children.set(name, lkNodeFile(data));
  }
  // Standard furniture the old MEMFS world also had.
  const dev = ensureDir('/dev');
  dev.children.set('null', lkNodeNull());
  dev.children.set('stdin', lkNodeLink('/dev/stdin-fd'));
  ensureDir('/tmp');
  ensureDir('/home/web_user');
  fs.fds.set(0, { node: lkNodeTty('in'), pos: 0, append: false, rd: true, wr: false, flags: 0 });
  fs.fds.set(1, { node: lkNodeTty('out'), pos: 0, append: false, rd: false, wr: true, flags: 1 });
  fs.fds.set(2, { node: lkNodeTty('err'), pos: 0, append: false, rd: false, wr: true, flags: 1 });
  return fs;
}

function lkNormalize(path) {
  const out = [];
  for (const part of path.split('/')) {
    if (part === '' || part === '.') continue;
    if (part === '..') { out.pop(); continue; }
    out.push(part);
  }
  return '/' + out.join('/');
}

// Resolve an absolute path to its node; follows links except optionally
// the final component. Returns {node, parent, name} or {err}.
function lkResolve(path, followFinal = true, depth = 0) {
  if (depth > 8) return { err: LK_E.LOOP };
  path = lkNormalize(path);
  if (path === '/') return { node: LK.fs.root, parent: null, name: '' };
  const parts = path.split('/').filter(Boolean);
  let node = LK.fs.root, parent = null, name = '';
  for (let i = 0; i < parts.length; i++) {
    if (node.type !== 'dir') return { err: LK_E.NOTDIR };
    parent = node; name = parts[i];
    const child = node.children.get(name);
    if (!child) return { err: LK_E.NOENT };
    node = child;
    const isFinal = i === parts.length - 1;
    if (node.type === 'link' && (!isFinal || followFinal)) {
      const rest = parts.slice(i + 1).join('/');
      const target = node.target.startsWith('/')
        ? node.target + (rest ? '/' + rest : '')
        : lkNormalize((path.split('/').slice(0, i + 1).join('/')) + '/' + node.target) + (rest ? '/' + rest : '');
      return lkResolve(target, followFinal, depth + 1);
    }
  }
  return { node, parent, name };
}

function lkResolveForOpen(dirfd, path) {
  if (path.startsWith('/')) return lkNormalize(path);
  if (dirfd === -100) return lkNormalize('/' + path); // AT_FDCWD; cwd is /
  const open = LK.fs.fds.get(dirfd);
  if (!open || open.node.type !== 'dir') return null;
  // We don't track an fd's path; relative-to-dirfd opens other than cwd
  // are not used by the engine's asset paths (all absolute). Fall back
  // to cwd semantics rather than failing.
  return lkNormalize('/' + path);
}

function lkFileBytes(node) { return node.data; }
function lkFileEnsureOwned(node) {
  if (!node.owned) { node.data = node.data.slice(); node.owned = true; }
}
function lkFileResize(node, size) {
  lkFileEnsureOwned(node);
  if (node.data.length === size) return;
  const next = new Uint8Array(size);
  next.set(node.data.subarray(0, Math.min(size, node.data.length)));
  node.data = next;
  node.mtime = Date.now();
}
function lkFileWrite(node, pos, src) {
  lkFileEnsureOwned(node);
  const end = pos + src.length;
  if (end > node.data.length) lkFileResize(node, end);
  node.data.set(src, pos);
  node.mtime = Date.now();
}

const LK_S_IFMT = 0o170000;
function lkStatSize(node) {
  if (node.type === 'file') return node.data.length;
  if (node.type === 'link') return node.target.length;
  return 0;
}
// Writes the stat64 layout the binary's libc reads (offsets verified
// against the old runtime's writer): dev, mode, nlink, uid, gid, rdev,
// size(i64), blksize, blocks, a/m/ctime (sec i64 + nsec u32), ino(i64).
function lkWriteStat(buf, node) {
  const u32 = lkU32(), i64 = lkI64();
  const B = buf >> 2, B8 = buf >> 3;
  u32[B + 0] = 1;                                        // st_dev
  u32[B + 1] = node.mode;                                // st_mode
  u32[B + 2] = node.type === 'dir' ? 2 : 1;              // st_nlink
  u32[B + 3] = 0;                                        // st_uid
  u32[B + 4] = 0;                                        // st_gid
  u32[B + 5] = 0;                                        // st_rdev
  i64[B8 + 3] = BigInt(lkStatSize(node));                // st_size @24
  u32[B + 8] = 4096;                                     // st_blksize @32
  u32[B + 9] = Math.ceil(lkStatSize(node) / 512);        // st_blocks @36 (512-byte units)
  const sec = BigInt(Math.floor(node.mtime / 1000));
  const nsec = (node.mtime % 1000) * 1000000;
  i64[B8 + 5] = sec; u32[B + 12] = nsec;                 // atime @40/@48
  i64[B8 + 7] = sec; u32[B + 16] = nsec;                 // mtime @56/@64
  i64[B8 + 9] = sec; u32[B + 20] = nsec;                 // ctime @72/@80
  i64[B8 + 11] = BigInt(node.ino);                       // ino @88
  return 0;
}

/* ====================== syscalls + WASI layer ======================== */
// These run on the MAIN thread. In a worker, the same-named imports
// proxy here through the mailbox (see the proxy section).

const LK_O = { CREAT: 0x40, EXCL: 0x80, TRUNC: 0x200, APPEND: 0x400,
               DIRECTORY: 0x10000, CLOEXEC: 0x80000, RDONLY: 0, WRONLY: 1, RDWR: 2 };

function lkSysOpenAt(dirfd, pathPtr, flags, mode) {
  const raw = lkReadCStr(pathPtr);
  const path = lkResolveForOpen(dirfd, raw);
  if (path === null) return -LK_E.BADF;
  const acc = flags & 3;
  let r = lkResolve(path, true);
  if (r.err) {
    if (r.err === LK_E.NOENT && (flags & LK_O.CREAT)) {
      const parentPath = path.slice(0, path.lastIndexOf('/')) || '/';
      const pr = lkResolve(parentPath, true);
      if (pr.err) return -pr.err;
      if (pr.node.type !== 'dir') return -LK_E.NOTDIR;
      const node = lkNodeFile(new Uint8Array(0));
      node.owned = true;
      pr.node.children.set(path.slice(path.lastIndexOf('/') + 1), node);
      r = { node };
    } else return -r.err;
  } else if ((flags & LK_O.CREAT) && (flags & LK_O.EXCL)) return -LK_E.EXIST;
  const node = r.node;
  if (node.type === 'dir' && acc !== LK_O.RDONLY) return -LK_E.ISDIR;
  if ((flags & LK_O.DIRECTORY) && node.type !== 'dir') return -LK_E.NOTDIR;
  if ((flags & LK_O.TRUNC) && node.type === 'file' && acc !== LK_O.RDONLY) lkFileResize(node, 0);
  const fd = LK.fs.nextFd++;
  LK.fs.fds.set(fd, { node, pos: 0, append: !!(flags & LK_O.APPEND),
                      rd: acc === LK_O.RDONLY || acc === LK_O.RDWR,
                      wr: acc === LK_O.WRONLY || acc === LK_O.RDWR,
                      flags, dirIter: null });
  return fd;
}

function lkFdGet(fd) { return LK.fs.fds.get(fd); }

function lkFdRead(fd, iovs, iovsLen, nreadPtr) {
  const open = lkFdGet(fd);
  if (!open) return LK_E.BADF;
  if (!open.rd) return LK_E.BADF;
  const node = open.node;
  let total = 0;
  const u32 = lkU32(), u8 = lkU8();
  if (node.type === 'tty' || node.type === 'null') {
    u32[nreadPtr >> 2] = 0; // stdin/null: immediate EOF
    return 0;
  }
  if (node.type === 'dir') return LK_E.ISDIR;
  const data = lkFileBytes(node);
  for (let i = 0; i < iovsLen; i++) {
    const base = u32[(iovs >> 2) + i * 2], len = u32[(iovs >> 2) + i * 2 + 1];
    const avail = Math.max(0, data.length - open.pos);
    const n = Math.min(len, avail);
    if (n > 0) { u8.set(data.subarray(open.pos, open.pos + n), base); open.pos += n; total += n; }
    if (n < len) break;
  }
  u32[nreadPtr >> 2] = total;
  return 0;
}

function lkFdWrite(fd, iovs, iovsLen, nwrittenPtr) {
  const open = lkFdGet(fd);
  if (!open) return LK_E.BADF;
  if (!open.wr) return LK_E.BADF;
  const node = open.node;
  let total = 0;
  const u32 = lkU32(), u8 = lkU8();
  const gather = () => {
    const chunks = [];
    let size = 0;
    for (let i = 0; i < iovsLen; i++) {
      const base = u32[(iovs >> 2) + i * 2], len = u32[(iovs >> 2) + i * 2 + 1];
      chunks.push(u8.subarray(base, base + len)); size += len;
    }
    const joined = new Uint8Array(size);
    let off = 0;
    for (const c of chunks) { joined.set(c, off); off += c.length; }
    return joined;
  };
  if (node.type === 'tty') {
    const bytes = gather();
    const text = lkTextDecoder.decode(bytes);
    lkWriteSink(text, node.kind === 'err');
    total = bytes.length;
  } else if (node.type === 'null') {
    total = gatherLength(iovs, iovsLen);
  } else if (node.type === 'dir') {
    return LK_E.ISDIR;
  } else {
    const bytes = gather();
    if (open.append) open.pos = node.data.length;
    lkFileWrite(node, open.pos, bytes);
    open.pos += bytes.length;
    total = bytes.length;
  }
  u32[nwrittenPtr >> 2] = total;
  return 0;
}
function gatherLength(iovs, iovsLen) {
  const u32 = lkU32();
  let size = 0;
  for (let i = 0; i < iovsLen; i++) size += u32[(iovs >> 2) + i * 2 + 1];
  return size;
}

function lkFdSeek(fd, offset, whence, newPosPtr) {
  const open = lkFdGet(fd);
  if (!open) return LK_E.BADF;
  if (open.node.type === 'tty' || open.node.type === 'null') return LK_E.SPIPE;
  const off = typeof offset === 'bigint' ? Number(offset) : offset;
  let base;
  if (whence === 0) base = 0;
  else if (whence === 1) base = open.pos;
  else if (whence === 2) base = open.node.type === 'file' ? open.node.data.length : 0;
  else return LK_E.INVAL;
  const next = base + off;
  if (next < 0) return LK_E.INVAL;
  open.pos = next;
  open.dirIter = null; // a seek rewinds any in-progress directory scan
  lkI64()[newPosPtr >> 3] = BigInt(next);
  return 0;
}

function lkSysStatPath(pathPtr, buf, follow) {
  const r = lkResolve(lkReadCStr(pathPtr), follow);
  if (r.err) return -r.err;
  return lkWriteStat(buf, r.node);
}

// Main-thread implementations of the syscall/WASI surface. The proxy
// table (below) routes worker calls to these same functions, in the
// index order the binary's linked code was built against.
const LK_SYS_MAIN = {
  proc_exit: (code) => lkDoExit(code),
  exitOnMainThread: (code) => lkDoExit(code),
  pthreadCreateProxied: (pthread_ptr, attr, startRoutine, arg) =>
    lkSpawnThread(pthread_ptr, startRoutine, arg),
  __syscall_chmod: (pathPtr, mode) => {
    const r = lkResolve(lkReadCStr(pathPtr), true);
    return r.err ? -r.err : 0;
  },
  __syscall_dup: (fd) => {
    const open = lkFdGet(fd);
    if (!open) return -LK_E.BADF;
    const nfd = LK.fs.nextFd++;
    LK.fs.fds.set(nfd, open); // dup shares the open file description
    return nfd;
  },
  __syscall_dup3: (fd, nfd, flags) => {
    const open = lkFdGet(fd);
    if (!open) return -LK_E.BADF;
    if (nfd < 0) return -LK_E.INVAL;
    LK.fs.fds.set(nfd, open);
    if (nfd >= LK.fs.nextFd) LK.fs.nextFd = nfd + 1;
    return nfd;
  },
  __syscall_fchmod: (fd, mode) => (lkFdGet(fd) ? 0 : -LK_E.BADF),
  __syscall_fcntl64: (fd, cmd, arg) => {
    const open = lkFdGet(fd);
    if (!open) return -LK_E.BADF;
    if (cmd === 0 || cmd === 1030) { // F_DUPFD / F_DUPFD_CLOEXEC
      let nfd = Math.max(arg | 0, 3);
      while (LK.fs.fds.has(nfd)) nfd++;
      LK.fs.fds.set(nfd, open);
      if (nfd >= LK.fs.nextFd) LK.fs.nextFd = nfd + 1;
      return nfd;
    }
    if (cmd === 1) return 0;  // F_GETFD
    if (cmd === 2) return 0;  // F_SETFD
    if (cmd === 3) return open.flags | 0;  // F_GETFL: the stored open flags
    if (cmd === 4) return 0;  // F_SETFL
    return -LK_E.INVAL;
  },
  __syscall_fstat64: (fd, buf) => {
    const open = lkFdGet(fd);
    if (!open) return -LK_E.BADF;
    return lkWriteStat(buf, open.node);
  },
  __syscall_ftruncate64: (fd, len) => {
    const open = lkFdGet(fd);
    if (!open) return -LK_E.BADF;
    if (open.node.type !== 'file') return -LK_E.INVAL;
    lkFileResize(open.node, Number(len));
    return 0;
  },
  __syscall_getcwd: (buf, size) => {
    if (size < 2) return -LK_E.RANGE;
    lkU8()[buf] = 0x2f; lkU8()[buf + 1] = 0; // "/"
    return buf;
  },
  __syscall_getdents64: (fd, buf, count) => {
    const open = lkFdGet(fd);
    if (!open) return -LK_E.BADF;
    if (open.node.type !== 'dir') return -LK_E.NOTDIR;
    if (!open.dirIter) {
      const names = ['.', '..', ...open.node.children.keys()];
      open.dirIter = { names, idx: 0 };
    }
    const it = open.dirIter;
    const u8 = lkU8(), i64 = lkI64(), u16 = lkU16();
    let written = 0;
    while (it.idx < it.names.length) {
      const name = it.names[it.idx];
      const child = name === '.' ? open.node : name === '..' ? open.node : open.node.children.get(name);
      const ino = child ? child.ino : 0;
      const dtype = !child ? 0 : child.type === 'dir' ? 4 : child.type === 'link' ? 10 :
                    (child.type === 'tty' || child.type === 'null') ? 2 : 8;
      const nb = lkTextEncoder.encode(name); // reclen is in ENCODED bytes
      const reclen = Math.ceil((19 + nb.length + 1) / 8) * 8;
      if (written + reclen > count) break;
      const at = buf + written;
      i64[at >> 3] = BigInt(ino);
      i64[(at + 8) >> 3] = BigInt(it.idx + 1);
      u16[(at + 16) >> 1] = reclen;
      u8[at + 18] = dtype;
      u8.set(nb, at + 19);
      u8.fill(0, at + 19 + nb.length, at + reclen);
      written += reclen;
      it.idx++;
    }
    return written;
  },
  __syscall_ioctl: (fd, req, argp) => {
    const open = lkFdGet(fd);
    if (!open) return -LK_E.BADF;
    if (req === 0x5413 && open.node.type === 'tty') { // TIOCGWINSZ
      const u16 = lkU16();
      u16[argp >> 1] = 24;       // rows
      u16[(argp + 2) >> 1] = 80; // cols
      u16[(argp + 4) >> 1] = 0;
      u16[(argp + 6) >> 1] = 0;
      return 0;
    }
    return -LK_E.NOTTY;
  },
  __syscall_lstat64: (pathPtr, buf) => lkSysStatPath(pathPtr, buf, false),
  __syscall_mkdirat: (dirfd, pathPtr, mode) => {
    const path = lkResolveForOpen(dirfd, lkReadCStr(pathPtr));
    if (path === null) return -LK_E.BADF;
    const existing = lkResolve(path, false);
    if (!existing.err) return -LK_E.EXIST;
    const parentPath = path.slice(0, path.lastIndexOf('/')) || '/';
    const pr = lkResolve(parentPath, true);
    if (pr.err) return -pr.err;
    if (pr.node.type !== 'dir') return -LK_E.NOTDIR;
    pr.node.children.set(path.slice(path.lastIndexOf('/') + 1), lkNodeDir());
    return 0;
  },
  __syscall_newfstatat: (dirfd, pathPtr, buf, flags) => {
    const raw = lkReadCStr(pathPtr);
    if (raw === '') { // AT_EMPTY_PATH: stat the fd itself
      const open = lkFdGet(dirfd);
      if (!open) return -LK_E.BADF;
      return lkWriteStat(buf, open.node);
    }
    const path = lkResolveForOpen(dirfd, raw);
    if (path === null) return -LK_E.BADF;
    const r = lkResolve(path, !(flags & 0x100)); // AT_SYMLINK_NOFOLLOW
    if (r.err) return -r.err;
    return lkWriteStat(buf, r.node);
  },
  __syscall_openat: (dirfd, pathPtr, flags, mode) => lkSysOpenAt(dirfd, pathPtr, flags, mode),
  __syscall_poll: (fdsPtr, nfds, timeout) => lkPoll(fdsPtr, nfds),
  __syscall_poll_nonblocking: (fdsPtr, nfds) => lkPoll(fdsPtr, nfds),
  __syscall_readlinkat: (dirfd, pathPtr, buf, size) => {
    const path = lkResolveForOpen(dirfd, lkReadCStr(pathPtr));
    if (path === null) return -LK_E.BADF;
    const r = lkResolve(path, false);
    if (r.err) return -r.err;
    if (r.node.type !== 'link') return -LK_E.INVAL;
    const bytes = lkTextEncoder.encode(r.node.target);
    const n = Math.min(bytes.length, size);
    lkU8().set(bytes.subarray(0, n), buf);
    return n;
  },
  __syscall_renameat: (olddirfd, oldPtr, newdirfd, newPtr) => {
    const oldPath = lkResolveForOpen(olddirfd, lkReadCStr(oldPtr));
    const newPath = lkResolveForOpen(newdirfd, lkReadCStr(newPtr));
    if (oldPath === null || newPath === null) return -LK_E.BADF;
    const r = lkResolve(oldPath, false);
    if (r.err) return -r.err;
    if (!r.parent) return -LK_E.INVAL;
    const parentPath = newPath.slice(0, newPath.lastIndexOf('/')) || '/';
    const pr = lkResolve(parentPath, true);
    if (pr.err) return -pr.err;
    if (pr.node.type !== 'dir') return -LK_E.NOTDIR;
    r.parent.children.delete(r.name);
    pr.node.children.set(newPath.slice(newPath.lastIndexOf('/') + 1), r.node);
    return 0;
  },
  __syscall_rmdir: (pathPtr) => LK_SYS_MAIN.__syscall_unlinkat(-100, pathPtr, 0x200),
  __syscall_stat64: (pathPtr, buf) => lkSysStatPath(pathPtr, buf, true),
  __syscall_symlinkat: (targetPtr, dirfd, linkPtr) => {
    const path = lkResolveForOpen(dirfd, lkReadCStr(linkPtr));
    if (path === null) return -LK_E.BADF;
    const existing = lkResolve(path, false);
    if (!existing.err) return -LK_E.EXIST;
    const parentPath = path.slice(0, path.lastIndexOf('/')) || '/';
    const pr = lkResolve(parentPath, true);
    if (pr.err) return -pr.err;
    if (pr.node.type !== 'dir') return -LK_E.NOTDIR;
    pr.node.children.set(path.slice(path.lastIndexOf('/') + 1), lkNodeLink(lkReadCStr(targetPtr)));
    return 0;
  },
  __syscall_truncate64: (pathPtr, len) => {
    const r = lkResolve(lkReadCStr(pathPtr), true);
    if (r.err) return -r.err;
    if (r.node.type !== 'file') return -LK_E.INVAL;
    lkFileResize(r.node, Number(len));
    return 0;
  },
  __syscall_unlinkat: (dirfd, pathPtr, flags) => {
    const path = lkResolveForOpen(dirfd, lkReadCStr(pathPtr));
    if (path === null) return -LK_E.BADF;
    const r = lkResolve(path, false);
    if (r.err) return -r.err;
    if (!r.parent) return -LK_E.PERM;
    const wantDir = !!(flags & 0x200); // AT_REMOVEDIR
    if (r.node.type === 'dir') {
      if (!wantDir) return -LK_E.ISDIR;
      if (r.node.children.size > 0) return -LK_E.NOTEMPTY;
    } else if (wantDir) return -LK_E.NOTDIR;
    r.parent.children.delete(r.name);
    return 0;
  },
  environ_get: (envPtr, envBufPtr) => 0,
  environ_sizes_get: (countPtr, sizePtr) => {
    lkU32()[countPtr >> 2] = 0;
    lkU32()[sizePtr >> 2] = 0;
    return 0;
  },
  fd_close: (fd) => {
    if (fd <= 2) return 0; // keep stdio descriptors alive
    return LK.fs.fds.delete(fd) ? 0 : LK_E.BADF;
  },
  fd_fdstat_get: (fd, buf) => {
    const open = lkFdGet(fd);
    if (!open) return LK_E.BADF;
    const u8 = lkU8(), u64 = lkU64();
    u8[buf] = open.node.type === 'dir' ? 3 : (open.node.type === 'tty' || open.node.type === 'null') ? 2 : 4;
    lkU16()[ (buf + 2) >> 1 ] = 0; // fs_flags
    u64[(buf + 8) >> 3] = 0xffffffffffffffffn;  // rights base
    u64[(buf + 16) >> 3] = 0xffffffffffffffffn; // rights inheriting
    return 0;
  },
  fd_read: (fd, iovs, iovsLen, nreadPtr) => lkFdRead(fd, iovs, iovsLen, nreadPtr),
  fd_seek: (fd, offset, whence, newPosPtr) => lkFdSeek(fd, offset, whence, newPosPtr),
  fd_sync: (fd) => (lkFdGet(fd) ? 0 : LK_E.BADF),
  fd_write: (fd, iovs, iovsLen, nwrittenPtr) => lkFdWrite(fd, iovs, iovsLen, nwrittenPtr),
};

function lkPoll(fdsPtr, nfds) {
  const i32 = lkI32(), i16 = new Int16Array(LK.memory.buffer);
  let ready = 0;
  for (let i = 0; i < nfds; i++) {
    const at = fdsPtr + i * 8;
    const fd = i32[at >> 2];
    const events = i16[(at + 4) >> 1];
    let revents = 0;
    const open = lkFdGet(fd);
    if (!open) revents = 0x10; // POLLNVAL
    else if (open.node.type === 'file' || open.node.type === 'tty' || open.node.type === 'null') revents = events & 0x5; // POLLIN|POLLOUT as requested
    i16[(at + 6) >> 1] = revents;
    if (revents) ready++;
  }
  return ready;
}

/* ========================= exit + main loop ========================== */

function lkDoExit(code) {
  if (!LK.exited) {
    LK.exited = true;
    LK.exitCode = code | 0;
    lkFlushSink(false); lkFlushSink(true);
    const m = (typeof Module !== 'undefined') ? Module : null;
    if (!LK.isWorker && m && m.onExit) { try { m.onExit(LK.exitCode); } catch (e) {} }
  }
  throw LK_EXIT;
}

// Page rAF driver (service thread). Two duties per tick: drain the GL
// command ring, and — once the main worker has registered its frame
// loop — ask it for one frame. Build 80 (frame-boundary atomicity):
// while a frame is outstanding, the drain DECODES its async records
// but executes them only when the frame completes (lk-frame-done), in
// order, in one task — so the compositor can only ever paint whole
// frames, never the half-drawn state a mid-frame drain used to expose
// (the device flicker: clear + first draws painted, rest still in
// flight). Sync records still execute in place (flushing the buffer
// first, so stream order holds at every sync point), and outside a
// frame everything executes on arrival as before. The canvas never
// leaves this thread; only commands cross.
let lkRafId = 0;
function lkPageDriverStart() {
  if (LK.isWorker || lkRafId) return;
  const step = () => {
    lkRafId = requestAnimationFrame(step);
    lkGLDrain();
    if (LK.exited) { cancelAnimationFrame(lkRafId); lkRafId = 0; return; }
    if (LK.mainWorker && LK.mainLoopRemote && !LK.frameOutstanding) {
      LK.frameOutstanding = true;
      LK.mainWorker.worker.postMessage({ cmd: 'lk-frame' });
    }
  };
  lkRafId = requestAnimationFrame(step);
}

/* ============================ EM_ASM ================================= */
// The binary carries our own EM_ASM bodies as strings (they are source
// text from the engine, embedded verbatim by the compiler). The import
// receives the string's address; we rebuild the function from the string
// and call it. Helpers from the old runtime that the bodies legitimately
// use (string codecs, heap views, the audio-object registry that
// miniaudio's WebAudio code expects) are provided in the eval scope —
// they are small utilities of ours, written here.

const lkAudioObjects = [null, undefined];
function emscriptenRegisterAudioObject(obj) {
  for (let i = 1; i < lkAudioObjects.length; i++) if (lkAudioObjects[i] === undefined) { lkAudioObjects[i] = obj; return i; }
  lkAudioObjects.push(obj);
  return lkAudioObjects.length - 1;
}
function emscriptenGetAudioObject(id) { return lkAudioObjects[id]; }

// Argument buffer layout (matches the binary's variadic packing):
// 'i' -> i32 (4 bytes), 'p' -> u32 pointer (4 bytes), 'j' -> i64 (8,
// 8-aligned), everything else -> f64 (8, 8-aligned).
function lkReadAsmArgs(sigPtr, buf) {
  const args = [];
  const u8 = lkU8(), i32 = lkI32(), u32 = lkU32(), i64 = lkI64(), f64 = lkF64();
  let ch;
  while ((ch = u8[sigPtr++]) !== 0) {
    const wide = ch !== 105 && ch !== 112;
    if (wide && (buf % 8)) buf += 4;
    if (ch === 112) args.push(u32[buf >> 2]);
    else if (ch === 106) args.push(i64[buf >> 3]);
    else if (ch === 105) args.push(i32[buf >> 2]);
    else args.push(f64[buf >> 3]);
    buf += wide ? 8 : 4;
  }
  return args;
}

// EM_ASM bodies are the engine's own C++ snippets, extracted by the
// toolchain at link time into a table keyed by address (lkasm.js,
// generated by extract-asm.py — the bodies never exist in wasm memory).
// The table's factory binds the bodies to this context's environment:
// the helper names they reference resolve through lkAsmEnv's getters,
// so heap views stay live across memory growth.
function lkAsmEnv() {
  const env = {
    UTF8ToString, UTF8ArrayToString, stringToUTF8, stringToUTF8Array,
    lengthBytesUTF8, emscriptenGetAudioObject, emscriptenRegisterAudioObject,
    Module: (typeof Module !== 'undefined' ? Module : {}),
    out: lkOut, err: lkErr,
  };
  const views = {
    wasmMemory: () => LK.memory,
    wasmTable: () => (LK.exports ? LK.exports.__indirect_function_table : null),
    HEAP8: lkI8, HEAPU8: lkU8, HEAP16: () => new Int16Array(LK.memory.buffer),
    HEAPU16: lkU16, HEAP32: lkI32, HEAPU32: lkU32, HEAPF32: lkF32,
    HEAPF64: lkF64, HEAP64: lkI64, HEAPU64: lkU64,
  };
  for (const k of Object.keys(views)) {
    Object.defineProperty(env, k, { get: views[k], enumerable: true });
  }
  return env;
}
let LK_ASM_TABLE = null;
function lkAsmTable() {
  if (!LK_ASM_TABLE) {
    if (typeof lkAsmConsts === 'function') LK_ASM_TABLE = lkAsmConsts(lkAsmEnv());
    else { lkErr('lkboot: lkasm.js is not loaded — EM_ASM bodies unavailable'); LK_ASM_TABLE = {}; }
  }
  return LK_ASM_TABLE;
}
function lkAsmFunction(codeAddr) {
  let fn = LK.asmCache.get(codeAddr);
  if (fn) return fn;
  fn = lkAsmTable()[codeAddr];
  if (!fn) {
    lkErr('lkboot: no EM_ASM body for address ' + codeAddr + ' (stale lkasm.js?)');
    fn = function () { return 0; };
  }
  LK.asmCache.set(codeAddr, fn);
  return fn;
}

function lkCoerceAsmReturn(v) {
  if (v === undefined) return 0;
  if (typeof v === 'bigint') return Number(v);
  return v;
}
function lkAsmConstInt(code, sigPtr, argbuf) {
  const args = lkReadAsmArgs(sigPtr, argbuf);
  try {
    return lkCoerceAsmReturn(lkAsmFunction(code)(...args));
  } catch (e) {
    if (lkIsUnwind(e) || e === LK_EXIT) throw e;
    lkErr('lkboot: EM_ASM body at ' + code + ' threw: ' + (e && e.message || e));
    return 0;
  }
}

/* ==================== mailbox proxy protocol ========================= */
// Contract the binary's compiled code depends on (studied from the old
// build's artifacts, re-implemented): a worker thread serializes a call
// into a stack buffer of tagged 8-byte slots (tag 1n = BigInt follows,
// 0n = Number follows), the binary's own export queues it into the main
// thread's mailbox and notifies us; the main thread's binary code then
// calls our _emscripten_receive_on_main_thread_js import, which executes
// the target (an EM_ASM body by address, or a proxied library function
// by index) and returns the value, or completes asynchronously through
// the binary's __emscripten_run_js_on_main_thread_done export.

// Proxied-function indices, in the order the binary was linked against.
const LK_PROXIED = [
  LK_SYS_MAIN.proc_exit,               // 0
  LK_SYS_MAIN.exitOnMainThread,        // 1
  LK_SYS_MAIN.pthreadCreateProxied,    // 2
  LK_SYS_MAIN.__syscall_chmod,         // 3
  LK_SYS_MAIN.__syscall_dup,           // 4
  LK_SYS_MAIN.__syscall_dup3,          // 5
  LK_SYS_MAIN.__syscall_fchmod,        // 6
  LK_SYS_MAIN.__syscall_fcntl64,       // 7
  LK_SYS_MAIN.__syscall_fstat64,       // 8
  LK_SYS_MAIN.__syscall_ftruncate64,   // 9
  LK_SYS_MAIN.__syscall_getcwd,        // 10
  LK_SYS_MAIN.__syscall_getdents64,    // 11
  LK_SYS_MAIN.__syscall_ioctl,         // 12
  LK_SYS_MAIN.__syscall_lstat64,       // 13
  LK_SYS_MAIN.__syscall_mkdirat,       // 14
  LK_SYS_MAIN.__syscall_newfstatat,    // 15
  LK_SYS_MAIN.__syscall_openat,        // 16
  LK_SYS_MAIN.__syscall_poll,          // 17
  LK_SYS_MAIN.__syscall_poll_nonblocking, // 18
  LK_SYS_MAIN.__syscall_readlinkat,    // 19
  LK_SYS_MAIN.__syscall_renameat,      // 20
  LK_SYS_MAIN.__syscall_rmdir,         // 21
  LK_SYS_MAIN.__syscall_stat64,        // 22
  LK_SYS_MAIN.__syscall_symlinkat,     // 23
  LK_SYS_MAIN.__syscall_truncate64,    // 24
  LK_SYS_MAIN.__syscall_unlinkat,      // 25
  null,                                // 26 exit_pointerlock (filled below)
  null,                                // 27 getCanvasSize (filled below)
  null,                                // 28 request_pointerlock (filled below)
  null,                                // 29 setCanvasSize (filled below)
  LK_SYS_MAIN.environ_get,             // 30
  LK_SYS_MAIN.environ_sizes_get,       // 31
  LK_SYS_MAIN.fd_close,                // 32
  LK_SYS_MAIN.fd_fdstat_get,           // 33
  LK_SYS_MAIN.fd_read,                 // 34
  LK_SYS_MAIN.fd_seek,                 // 35
  LK_SYS_MAIN.fd_sync,                 // 36
  LK_SYS_MAIN.fd_write,                // 37
];

function lkStackSave() { return LK.exports.emscripten_stack_get_current(); }
function lkStackAlloc(n) { return LK.exports._emscripten_stack_alloc(n); }
function lkStackRestore(sp) { LK.exports._emscripten_stack_restore(sp); }

function lkProxyToMainThread(funcIndex, emAsmAddr, proxyMode, ...callArgs) {
  const bufSize = 16 * callArgs.length;
  const sp = lkStackSave();
  const argsPtr = lkStackAlloc(bufSize);
  const i64 = lkI64(), f64 = lkF64();
  let b = argsPtr >> 3;
  for (const arg of callArgs) {
    if (typeof arg === 'bigint') { i64[b++] = 1n; i64[b++] = arg; }
    else { i64[b++] = 0n; f64[b++] = arg; }
  }
  const rtn = LK.exports._emscripten_run_js_on_main_thread(funcIndex, emAsmAddr, bufSize, argsPtr, proxyMode);
  lkStackRestore(sp);
  return rtn;
}

function lkReceiveOnMainThread(funcIndex, emAsmAddr, callingThread, bufSize, argsPtr, ctx, ctxArgs) {
  const args = [];
  const i64 = lkI64(), f64 = lkF64();
  let b = argsPtr >> 3;
  const end = (argsPtr + bufSize) >> 3;
  while (b < end) {
    if (i64[b++]) args.push(i64[b++]);
    else args.push(f64[b++]);
  }
  // Proxied services execute ONLY on the page: that is where the FS,
  // the DOM and the GL state live. A worker can still be asked to run
  // one — the binary's queue pump executes queued calls in whatever
  // thread waits on the "main" struct (our main worker adopts it, so
  // its futex waits pump the queue) — so workers forward the call to
  // the page instead of executing it against their empty local state.
  if (LK.isWorker) return lkReceiveForward(funcIndex, emAsmAddr, callingThread, args, ctx, ctxArgs);
  const fn = emAsmAddr ? lkAsmFunction(emAsmAddr) : LK_PROXIED[funcIndex];
  LK.currentProxiedCaller = callingThread;
  let rtn;
  try { rtn = fn(...args); }
  catch (e) {
    // Mirror the forwarded path: a throwing service is logged and
    // answered (undefined/0), never propagated as a bare trap into
    // whichever thread happened to pump the queue. Unwind/exit
    // sentinels are control flow, not errors — they pass through.
    if (lkIsUnwind(e) || e === LK_EXIT) throw e;
    lkErr('lkboot: service on main thread threw (funcIndex ' + funcIndex + '): ' + (e && e.message || e));
    rtn = undefined;
  }
  finally { LK.currentProxiedCaller = 0; }
  if (ctx) {
    Promise.resolve(rtn).then((v) => {
      try { LK.exports._emscripten_run_js_on_main_thread_done(ctx, ctxArgs, v); } catch (e) {}
    });
    return undefined;
  }
  return rtn;
}

// Worker side of a forwarded proxied service. Async calls (ctx set)
// complete through the page's reply message; immediate-value calls
// (ctx == 0) run a synchronous RPC through a small shared slot: the
// page writes the result there and notifies — the worker's own
// message loop is blocked inside the pump, so the reply cannot ride
// postMessage. One call in flight at a time (CAS on the state word).
function lkReceiveForward(funcIndex, emAsmAddr, callingThread, args, ctx, ctxArgs) {
  if (ctx) {
    self.postMessage({ cmd: 'lk-rpc', funcIndex, emAsmAddr, callingThread, args, ctx, ctxArgs, sync: false });
    return undefined;
  }
  if (!LK.rpcOff) return 0;
  const { i32, f64, u64 } = lkRpcViews();
  while (Atomics.compareExchange(i32, 0, 0, 1) !== 0) Atomics.wait(i32, 0, 1);
  self.postMessage({ cmd: 'lk-rpc', funcIndex, emAsmAddr, callingThread, args, ctx: 0, ctxArgs: 0, sync: true });
  while (Atomics.load(i32, 0) === 1) Atomics.wait(i32, 0, 1);
  const tag = i32[2];
  let rtn;
  if (tag === 1) rtn = u64[2];
  else if (tag === 2) rtn = undefined;
  else rtn = f64[2];
  Atomics.store(i32, 0, 0);
  Atomics.notify(i32, 0);
  return rtn;
}

function lkCheckMailbox() {
  // The main worker shares the main-thread struct (and therefore its
  // mailbox) with the page; servicing is the page's job alone.
  if (LK.role === 'main') return;
  if (!LK.exports) return;
  let selfPtr = 0;
  try { selfPtr = LK.exports.pthread_self(); } catch (e) { return; }
  if (!selfPtr) return;
  try {
    lkMailboxAwait(selfPtr);
    LK.exports._emscripten_check_mailbox();
  } catch (e) {
    if (!lkIsUnwind(e) && e !== LK_EXIT) lkErr('lkboot: mailbox processing threw: ' + (e && e.message || e));
  }
}

function lkMailboxAwait(pthread_ptr) {
  if (typeof Atomics !== 'undefined' && typeof Atomics.waitAsync === 'function') {
    try {
      const wait = Atomics.waitAsync(lkI32(), pthread_ptr >> 2, pthread_ptr);
      if (wait && typeof wait === 'object' && 'value' in wait) {
        if (wait.async) wait.value.then(() => lkCheckMailbox());
        lkI32()[(pthread_ptr + 112) >> 2] = 1;
      }
    } catch (e) { /* notifications still arrive via cmd-4 messages */ }
  }
}

function lkNotifyMailbox(targetThread, currThreadId) {
  // The main mailbox is owned and serviced ONLY by the page. A worker
  // NEVER services it locally — not even the main worker, whose
  // pthread_self() equals the main struct: its proxied requests must
  // travel to the page like every other thread's. (A self-service
  // shortcut here races the page and executes services in a context
  // that has no FS/DOM — the exact failure this comment replaces.)
  if (LK.isWorker) {
    self.postMessage({ cmd: 4, targetThread });
    return;
  }
  let selfPtr = 0;
  try { selfPtr = LK.exports ? LK.exports.pthread_self() : 0; } catch (e) {}
  if (targetThread && targetThread === selfPtr) {
    setTimeout(lkCheckMailbox, 0);
  } else if (LK.pool) {
    const rec = LK.pool.byPtr.get(targetThread);
    if (rec) rec.worker.postMessage({ cmd: 4 });
    else if (LK.mainWorker) LK.mainWorker.worker.postMessage({ cmd: 4 });
  }
}

/* ============================ pthreads =============================== */
// Main-thread side: a lazy worker pool. Each worker runs THIS file,
// receives the compiled module + shared memory once ('lk-load'), then
// one 'lk-run' message per thread it hosts. Workers are recycled.

// Thread pool cap (build 78, 2026-10-21): sized from MEASURED engine
// thread demand, not a guess. The workstation interactive probe peaked
// at 4 concurrent pool workers (hardwareConcurrency 2); on device
// (hardwareConcurrency 8) build 77 filled all 8 and was denied a 9th
// during world load. The most demanding per-core model consistent with
// both points wants 16 at 8 cores, so the cap is 16 + 50% headroom.
// Still a hard, bounded cap: an uncapped pool is a fork bomb.
const LK_POOL_MAX_WORKERS = 24;

// HOST DISPATCH (topology reorder, JD's direction, 2026-10-21). The
// host surface — this page — is the ONLY worker authority in the
// layer: it provisions the main worker at boot and it provisions
// every pool worker on request. The engine never conjures a worker
// from below; its pthread_create arrives here as a REQUEST (on the
// page: a direct call into this dispatch; in a worker: the dedicated
// lk-spawn-request channel, see lkSpawnRequest), and the host grants
// or denies it under its own policy — the pool cap, and, in
// single-agent contexts where the shared memory cannot be posted to
// any worker, an outright denial that leaves the engine running on
// the host's thread. lkSpawnThread IS that dispatch.
function lkSpawnThread(pthread_ptr, startRoutine, arg) {
  if (LK.singleAgent) {
    // Unreachable in the blocked boot (main never starts), kept so
    // any create attempted on the page instance in a non-isolated
    // context gets a defined refusal instead of a half-state.
    if (!LK.poolDeniedWarned) {
      LK.poolDeniedWarned = true;
      lkErr('lkboot: SINGLE-AGENT BLOCKED: host dispatch denies pthread_create (EAGAIN) — ' +
            'no worker can be provisioned in a non-isolated context.');
    }
    return 6; // EAGAIN
  }
  if (typeof Worker === 'undefined') return 6; // EAGAIN
  if (!LK.pool) LK.pool = { byPtr: new Map(), idle: [] };
  if (!LK.poolStats) LK.poolStats = { creates: 0, highWater: 0 };
  let rec = LK.pool.idle.pop();
  if (!rec) {
    if (LK.pool.byPtr.size + LK.pool.idle.length >= LK_POOL_MAX_WORKERS) {
      if (!LK.poolCapWarned) {
        LK.poolCapWarned = true;
        lkErr('lkboot: pthread pool cap (' + LK_POOL_MAX_WORKERS +
              ' workers) reached; pthread_create returns EAGAIN');
      }
      return 6; // EAGAIN: retryable by contract, never a hang or a trap
    }
    let worker;
    try {
      worker = new Worker(LK_BOOT_URL);
    } catch (e) {
      lkErr('lkboot: worker construction failed: ' + (e && e.message || e) +
            '; pthread_create returns EAGAIN');
      return 6; // EAGAIN, not a throw across the import boundary
    }
    rec = { worker, pthread_ptr: 0 };
    worker.onmessage = (e) => lkOnWorkerMessage(e, rec);
    worker.onerror = (e) => lkErr('lkboot: pthread worker error: ' + (e.message || e));
    worker.postMessage({ cmd: 'lk-load', module: LK.module, memory: LK.memory, glRing: LK.glRing, rpc: LK.rpcOff, role: 'pool' });
  }
  rec.pthread_ptr = pthread_ptr;
  LK.pool.byPtr.set(pthread_ptr, rec);
  LK.poolStats.creates++;
  {
    const live = LK.pool.byPtr.size + LK.pool.idle.length;
    if (live > LK.poolStats.highWater) {
      LK.poolStats.highWater = live;
      lkOut('lkboot: thread pool: ' + LK.poolStats.creates +
            ' pthread_create grants, high-water ' + live +
            ' concurrent workers (cap ' + LK_POOL_MAX_WORKERS + ')');
    }
  }
  rec.worker.postMessage({ cmd: 'lk-run', start_routine: startRoutine, arg, pthread_ptr });
  return 0;
}

function lkRecycleWorker(pthread_ptr) {
  if (!LK.pool) return;
  const rec = LK.pool.byPtr.get(pthread_ptr);
  if (!rec) return;
  LK.pool.byPtr.delete(pthread_ptr);
  rec.pthread_ptr = 0;
  LK.pool.idle.push(rec);
}

// Worker side of host dispatch (topology reorder): the engine's
// __pthread_create_js import in a worker is a REQUEST to the host,
// carried on a dedicated message — NOT on the mailbox service proxy
// it rode before the reorder (the mailbox is for services; worker
// provisioning is the host's own business, and the request now reads
// as one). The call is synchronous from the engine's side, so the
// verdict travels back through the shared service-RPC slot under the
// same claim discipline lkReceiveForward uses (one claimant at a
// time, CAS on the state word): the host's dispatch executes, writes
// the return code (0 or EAGAIN) into the slot, and notifies.
function lkSpawnRequest(pthread_ptr, attr, startRoutine, arg) {
  if (!LK.rpcOff) return 6; // no slot, no channel: EAGAIN
  const { i32, f64 } = lkRpcViews();
  while (Atomics.compareExchange(i32, 0, 0, 1) !== 0) Atomics.wait(i32, 0, 1);
  self.postMessage({ cmd: 'lk-spawn-request', pthread_ptr, startRoutine, arg });
  while (Atomics.load(i32, 0) === 1) Atomics.wait(i32, 0, 1);
  const rc = f64[2] | 0;
  Atomics.store(i32, 0, 0);
  Atomics.notify(i32, 0);
  return rc;
}

// The engine's main worker dying is TERMINAL (bug-fix run): the page
// must not keep pumping frames at a corpse or report a healthy shell
// around a dead engine. Stop the driver, name the death in the page,
// and drop the record so nothing recycles it.
function lkMainWorkerDied(reason) {
  LK.mainLoopRemote = false;
  LK.frameOutstanding = false;
  lkPendingGL = [];  // build 80: a dead worker's partial frame is never presented
  LK.mainWorker = null;
  lkErr('lkboot: ENGINE MAIN WORKER DIED: ' + reason +
        ' — the engine cannot continue; reload the page.');
  const m = (typeof Module !== 'undefined') ? Module : null;
  if (m && m.setStatus) {
    try { m.setStatus('Engine main worker died: ' + reason); } catch (e) {}
  }
}

function lkOnWorkerMessage(e, rec) {
  const d = e.data || {};
  if (d.cmd === 4) {
    lkCheckMailbox();
  } else if (d.cmd === 'lk-gl') {
    lkGLDrain();
  } else if (d.cmd === 'lk-spawn-request') {
    // Host dispatch (topology reorder): a worker asks the host to
    // provision a pool thread. The host's own dispatch decides; the
    // verdict returns through the shared RPC slot the requester is
    // waiting on (see lkSpawnRequest for the claim discipline).
    const rc = lkSpawnThread(d.pthread_ptr, d.startRoutine, d.arg);
    const { i32, f64 } = lkRpcViews();
    f64[2] = rc;
    i32[2] = 0; // tag: plain number
    Atomics.store(i32, 0, 2);
    Atomics.notify(i32, 0);
  } else if (d.cmd === 'lk-mainloop') {
    LK.mainLoopRemote = d.on !== false;
    if (LK.mainLoopRemote) lkPageDriverStart();
  } else if (d.cmd === 'lk-frame-done') {
    LK.frameOutstanding = false;
    lkGLFlushPending();  // build 80: the completed frame executes whole, in this task
    lkB81PresentFrame(); // build 81: ...and reaches the canvas in ONE blit
    lkGLDrain();
  } else if (d.cmd === 'lk-main-done') {
    LK.mainLoopRemote = false;
    LK.frameOutstanding = false;
    // main() returned a code instead of calling exit(): the program
    // is over — run the page-side exit (flush + Module.onExit).
    if (typeof d.exitCode === 'number' && !LK.exited) {
      LK.exited = true;
      LK.exitCode = d.exitCode | 0;
      lkFlushSink(false); lkFlushSink(true);
      const m = (typeof Module !== 'undefined') ? Module : null;
      if (m && m.onExit) { try { m.onExit(LK.exitCode); } catch (e) {} }
    }
  } else if (d.cmd === 'lk-rpc') {
    // A worker forwarded a proxied service: execute it here, where
    // the state lives. Sync calls answer through the shared RPC slot;
    // async calls reply to the sending worker, which completes the
    // queued call through its own instance's done export.
    const fn = d.emAsmAddr ? lkAsmFunction(d.emAsmAddr) : LK_PROXIED[d.funcIndex];
    LK.currentProxiedCaller = d.callingThread || 0;
    let out;
    try { out = fn(...d.args); } catch (err) { lkErr('lkboot: forwarded service threw: ' + (err && err.message || err)); out = undefined; }
    LK.currentProxiedCaller = 0;
    if (d.sync) {
      const { i32, f64, u64 } = lkRpcViews();
      if (typeof out === 'bigint') { u64[2] = out; i32[2] = 1; }
      else if (out === undefined || (out && typeof out.then === 'function')) { if (out && typeof out.then === 'function') lkErr('lkboot: sync service returned a promise (answered undefined)'); i32[2] = 2; }
      else { f64[2] = Number(out); i32[2] = 0; }
      Atomics.store(i32, 0, 2);
      Atomics.notify(i32, 0);
    } else if (rec && rec.worker) {
      const target = rec.worker;
      Promise.resolve(out).then((v) => {
        try { target.postMessage({ cmd: 'lk-rpc-done', ctx: d.ctx, ctxArgs: d.ctxArgs, value: v }); } catch (e) {}
      });
    }
  } else if (d.cmd === 'lk-done' || d.cmd === 'lk-cleanup') {
    lkRecycleWorker(d.pthread_ptr);
  } else if (d.cmd === 'lk-crashed') {
    if (LK.mainWorker && rec === LK.mainWorker) { lkMainWorkerDied(d.error); return; }
    lkErr('lkboot: pthread ' + d.pthread_ptr + ' crashed: ' + d.error);
    lkRecycleWorker(d.pthread_ptr);
  }
}

// The main worker: runs the binary's main() on its own thread (the
// engine's threads block and rendezvous; only a real worker may do
// that). Spawned once by lkBootMain after the service instance's
// constructors have initialized the main-thread struct. The main
// worker ADOPTS that struct: the binary's locks carry an owner field
// checked against the executing thread's identity, so every "main
// thread" execution — the page's service executions and this
// worker's — must present the same identity, exactly as the old
// runtime arranged. Thread init uses the plain entry flags
// (is_main=0, is_runtime=0): this instance is not the binary's main
// or runtime thread (the page's instance is), it merely runs main()
// on the main struct, so its proxied services route to the page and
// its futex waits are real.
function lkSpawnMainWorker(argc, argv) {
  const worker = new Worker(LK_BOOT_URL);
  const rec = { worker, pthread_ptr: LK.mainThreadPtr };
  worker.onmessage = (e) => lkOnWorkerMessage(e, rec);
  worker.onerror = (e) => {
    lkErr('lkboot: main worker error: ' + (e.message || e));
    if (LK.mainWorker === rec) lkMainWorkerDied(e.message || String(e));
  };
  const stackSize = 8388608;
  const stackBase = LK.exports.malloc(stackSize);
  worker.postMessage({ cmd: 'lk-load', module: LK.module, memory: LK.memory, glRing: LK.glRing, rpc: LK.rpcOff, role: 'main' });
  worker.postMessage({ cmd: 'lk-main', pthread_ptr: LK.mainThreadPtr, argc, argv, stackHigh: stackBase + stackSize, stackSize });
  LK.mainWorker = rec;
  return rec;
}

/* ==================== HTML5: canvas + pointer ======================== */

function lkFindCanvas(target) {
  if (!target) return LK.canvas || document.getElementById('canvas');
  const sel = lkReadCStr(target);
  if (sel === '#canvas' || sel === 'canvas') return document.getElementById('canvas') || LK.canvas;
  try { return document.querySelector(sel); } catch (e) { return null; }
}
function lkSetCanvasSize(target, w, h) {
  const cv = lkFindCanvas(target);
  if (cv) {
    cv.width = w | 0; cv.height = h | 0;
    // Build 81: keep every context surface on this canvas sized to
    // it (attachments are reallocated in place by lkB81Ensure).
    for (const rec of lkGLState().contexts.values())
      if (rec.canvas === cv && rec.b81) lkB81Ensure(rec);
  }
  return 0;
}
function lkGetCanvasSize(target, wPtr, hPtr) {
  const cv = lkFindCanvas(target);
  if (!cv) return -1;
  lkI32()[wPtr >> 2] = cv.width | 0;
  lkI32()[hPtr >> 2] = cv.height | 0;
  return 0;
}
function lkRequestPointerLock(target, useAdjusted) {
  // Build 80: on touch-primary pages (touch.js sets window.__lkTouchPrimary
  // when its overlay is up) pointer lock is refused by the browser BY
  // DESIGN — the engine's drag-look fallback is the intended path there.
  // Refuse silently at the host seam instead of issuing a DOM request
  // whose only effect was a pointerlockerror (and one spam log line per
  // tap on JD's phone, build 79). Desktop behavior is unchanged.
  if (typeof window !== 'undefined' && window.__lkTouchPrimary) return -1;
  const cv = lkFindCanvas(target);
  if (!cv || !cv.requestPointerLock) return -1;
  try {
    const p = useAdjusted ? cv.requestPointerLock({ unadjustedMovement: true }) : cv.requestPointerLock();
    if (p && typeof p.catch === 'function') p.catch(() => {});
  } catch (e) { /* the C++ side owns the cooldown/retry policy */ }
  return 0;
}
function lkExitPointerLock() {
  if (typeof document !== 'undefined' && document.exitPointerLock) document.exitPointerLock();
  return 0;
}
LK_PROXIED[26] = () => lkExitPointerLock();
LK_PROXIED[27] = (target, wPtr, hPtr) => lkGetCanvasSize(target, wPtr, hPtr);
LK_PROXIED[28] = (target, useAdjusted) => lkRequestPointerLock(target, useAdjusted);
LK_PROXIED[29] = (target, w, h) => lkSetCanvasSize(target, w, h);

/* ===================== WebGL2 wrapper (clause two) =================== */
// The binary imports ~270 GL entry points. We implement them ourselves
// on top of ONE WebGL2 context per canvas, with:
//
//   - handle registries (the binary speaks in integer GL names; the
//     wrapper owns the name -> object maps, per object class);
//   - a state cache: enables, binds, blend/depth/stencil state, viewport,
//     clear values, program and the presenter's sampler uniform are only
//     re-issued to the GPU when they actually change. The stock glue
//     re-issues every call (plus per-call bookkeeping); a present in the
//     classic path was ~15 crossings of pure re-assertion. Draws are
//     never cached — they are the payload.
//   - zero logging, zero telemetry, zero timing instrumentation in the
//     call path. What the game asks for is what the GPU gets.
//
// GL calls are expected on the main thread (the frame loop lives there
// now). A GL import invoked from a worker gets a logged-once no-op: WebGL
// contexts cannot leave the main thread, and silently proxying GL would
// be a lie about ordering.

function lkNewGLRegistry() { return { next: 1, map: new Map(), rev: new Map() }; }
function lkRegAlloc(rec, kind, obj) {
  const reg = rec.reg[kind];
  const name = reg.next++;
  reg.map.set(name, obj);
  if (obj) reg.rev.set(obj, name);
  return name;
}
function lkRegGet(rec, kind, name) { return rec.reg[kind].map.get(name); }
function lkRegFree(rec, kind, name) {
  const reg = rec.reg[kind];
  const obj = reg.map.get(name);
  if (obj !== undefined) reg.rev.delete(obj);
  reg.map.delete(name);
}
function lkRegName(rec, kind, obj) { return rec.reg[kind].rev.get(obj) || 0; }

function lkNewGLState() {
  return {
    contexts: new Map(),  // handle -> context record
    nextHandle: 1,
    current: null,        // current context record
    warnedWorkerGL: false,
  };
}
function lkNewContextRecord(handle, gl, canvas) {
  const kinds = ['buffer', 'texture', 'program', 'shader', 'framebuffer',
                 'renderbuffer', 'vao', 'query', 'sampler', 'sync', 'tfb'];
  const reg = {};
  for (const k of kinds) reg[k] = lkNewGLRegistry();
  return {
    handle, gl, canvas,
    reg,
    b81: null,            // build 81: double-buffer surface state (lkB81Ensure)
    locs: new Map(),      // program name -> Map(name -> {id, loc})
    locById: new Map(),   // id -> {prog, loc}
    nextLoc: 1,
    exts: new Map(),      // extension name -> object|null
    bufMeta: new Map(),   // buffer name -> {size, usage}
    attribs: new Map(),   // vao name -> Map(index -> attrib record)
    cache: {
      program: 0, activeUnit: 0, vao: 0, rbo: 0,
      fbo: new Map(),     // target -> name
      bufBind: new Map(), // target -> name
      texBind: new Map(), // unit:target -> name
      samplerBind: new Map(), // unit -> name
      caps: new Map(),    // cap -> bool
      attribEnabled: new Map(), // index -> bool (per current vao semantics; reset on vao switch)
      viewport: '', scissor: '', clearColor: '', clearDepth: -1, clearStencil: -1,
      blendFunc: '', blendFuncSep: '', blendEq: '', blendEqSep: '', blendColor: '',
      colorMask: '', depthFunc: -1, depthMask: null, depthRange: '',
      cullFace: -1, frontFace: -1, pixelStore: new Map(),
      stencilFunc: '', stencilFuncSep: '', stencilMask: -1, stencilMaskSep: '',
      stencilOp: '', stencilOpSep: '', lineWidth: -1, polygonOffset: '',
      sampleCoverage: '', hint: new Map(), uniforms: new Map(),
    },
  };
}
function lkGLState() {
  if (!LK.gl) LK.gl = lkNewGLState();
  return LK.gl;
}
function lkCtx() {
  const st = lkGLState();
  return st.current;
}
function lkGLOrWarn() {
  const rec = lkCtx();
  if (rec) return rec;
  if (LK.isWorker && !lkGLState().warnedWorkerGL) {
    lkGLState().warnedWorkerGL = true;
    lkErr('lkboot: GL import called from a worker thread — ignored (WebGL lives on the main thread)');
  }
  return null;
}
// State-cache gate: returns true when the value changed (caller issues
// the real call), and records it either way on first sight.
function lkCacheChanged(rec, key, value) {
  const c = rec.cache;
  if (c[key] === value) return false;
  c[key] = value;
  return true;
}
function lkCacheMapChanged(map, key, value) {
  if (map.get(key) === value) return false;
  map.set(key, value);
  return true;
}

const LK_GL = {}; // import name -> implementation (filled below)

function lkDefGL(name, fn) { LK_GL['emscripten_gl' + name] = fn; }
function lkNoCtx(defaultValue) { return defaultValue; }

// --- object creation / deletion ---
function lkGen(kind, createFn) {
  return (n, ptr) => {
    const rec = lkGLOrWarn(); if (!rec) return;
    const u32 = lkU32();
    for (let i = 0; i < n; i++) {
      const obj = createFn(rec.gl);
      u32[(ptr >> 2) + i] = lkRegAlloc(rec, kind, obj);
    }
  };
}
function lkCreate(kind, createFn) {
  return () => {
    const rec = lkGLOrWarn(); if (!rec) return 0;
    return lkRegAlloc(rec, kind, createFn(rec.gl));
  };
}
function lkDelete(kind, deleteFn, after) {
  return (n, ptr) => {
    const rec = lkGLOrWarn(); if (!rec) return;
    const u32 = lkU32();
    for (let i = 0; i < n; i++) {
      const name = u32[(ptr >> 2) + i];
      if (!name) continue;
      const obj = lkRegGet(rec, kind, name);
      if (obj !== undefined && deleteFn) deleteFn(rec.gl, obj);
      if (after) after(rec, name);
      lkRegFree(rec, kind, name);
    }
  };
}
function lkIs(kind) {
  return (name) => {
    const rec = lkGLOrWarn(); if (!rec) return 0;
    return rec.reg[kind].map.has(name) ? 1 : 0;
  };
}

lkDefGL('GenBuffers', lkGen('buffer', (gl) => gl.createBuffer()));
lkDefGL('DeleteBuffers', lkDelete('buffer', (gl, o) => gl.deleteBuffer(o), (rec, name) => {
  for (const [k, v] of rec.cache.bufBind) if (v === name) rec.cache.bufBind.delete(k);
  rec.bufMeta.delete(name);
}));
lkDefGL('IsBuffer', lkIs('buffer'));
lkDefGL('GenTextures', lkGen('texture', (gl) => gl.createTexture()));
lkDefGL('DeleteTextures', lkDelete('texture', (gl, o) => gl.deleteTexture(o), (rec, name) => {
  for (const [k, v] of rec.cache.texBind) if (v === name) rec.cache.texBind.delete(k);
}));
lkDefGL('IsTexture', lkIs('texture'));
lkDefGL('GenFramebuffers', lkGen('framebuffer', (gl) => gl.createFramebuffer()));
lkDefGL('DeleteFramebuffers', lkDelete('framebuffer', (gl, o) => gl.deleteFramebuffer(o), (rec, name) => {
  for (const [k, v] of rec.cache.fbo) if (v === name) rec.cache.fbo.delete(k);
}));
lkDefGL('IsFramebuffer', lkIs('framebuffer'));
lkDefGL('GenRenderbuffers', lkGen('renderbuffer', (gl) => gl.createRenderbuffer()));
lkDefGL('DeleteRenderbuffers', lkDelete('renderbuffer', (gl, o) => gl.deleteRenderbuffer(o), (rec, name) => {
  if (rec.cache.rbo === name) rec.cache.rbo = 0;
}));
lkDefGL('IsRenderbuffer', lkIs('renderbuffer'));
lkDefGL('GenVertexArrays', lkGen('vao', (gl) => gl.createVertexArray()));
lkDefGL('GenVertexArraysOES', lkGen('vao', (gl) => gl.createVertexArray()));
lkDefGL('DeleteVertexArrays', lkDelete('vao', (gl, o) => gl.deleteVertexArray(o), (rec, name) => {
  if (rec.cache.vao === name) { rec.cache.vao = 0; rec.cache.attribEnabled.clear(); }
  rec.attribs.delete(name);
}));
lkDefGL('DeleteVertexArraysOES', LK_GL['emscripten_glDeleteVertexArrays']);
lkDefGL('IsVertexArray', lkIs('vao'));
lkDefGL('IsVertexArrayOES', LK_GL['emscripten_glIsVertexArray']);
lkDefGL('CreateProgram', lkCreate('program', (gl) => gl.createProgram()));
lkDefGL('DeleteProgram', lkDelete('program', (gl, o) => gl.deleteProgram(o), (rec, name) => {
  if (rec.cache.program === name) rec.cache.program = 0;
  const lm = rec.locs.get(name);
  if (lm) { for (const ent of lm.values()) rec.locById.delete(ent.id); rec.locs.delete(name); }
}));
lkDefGL('IsProgram', lkIs('program'));
lkDefGL('CreateShader', (type) => {
  const rec = lkGLOrWarn(); if (!rec) return 0;
  const obj = rec.gl.createShader(type);
  return obj ? lkRegAlloc(rec, 'shader', obj) : 0;
});
lkDefGL('DeleteShader', lkDelete('shader', (gl, o) => gl.deleteShader(o)));
lkDefGL('IsShader', lkIs('shader'));
lkDefGL('GenQueries', lkGen('query', (gl) => gl.createQuery()));
lkDefGL('GenQueriesEXT', LK_GL['emscripten_glGenQueries']);
lkDefGL('DeleteQueries', lkDelete('query', (gl, o) => gl.deleteQuery(o)));
lkDefGL('DeleteQueriesEXT', LK_GL['emscripten_glDeleteQueries']);
lkDefGL('IsQuery', lkIs('query'));
lkDefGL('IsQueryEXT', LK_GL['emscripten_glIsQuery']);
lkDefGL('GenSamplers', lkGen('sampler', (gl) => gl.createSampler()));
lkDefGL('DeleteSamplers', lkDelete('sampler', (gl, o) => gl.deleteSampler(o), (rec, name) => {
  for (const [k, v] of rec.cache.samplerBind) if (v === name) rec.cache.samplerBind.delete(k);
}));
lkDefGL('IsSampler', lkIs('sampler'));
lkDefGL('GenTransformFeedbacks', lkGen('tfb', (gl) => gl.createTransformFeedback()));
lkDefGL('DeleteTransformFeedbacks', lkDelete('tfb', (gl, o) => gl.deleteTransformFeedback(o)));
lkDefGL('IsTransformFeedback', lkIs('tfb'));

// --- binds (cached) ---
// H3 (bug-fix run): ELEMENT_ARRAY_BUFFER (target 0x8893) binding is
// per-VAO state in WebGL2; every other buffer target is context state.
// Key the binding cache by (current VAO, target) for 0x8893 only —
// keyed by target alone, the cache suppressed a needed re-bind after
// a VAO switch and left the new VAO with no element buffer, silently.
// (Constant check, verified against the tree's gl_loader.h and the
// Khronos GLES3 header: 0x8893 is the TARGET; 0x8895 is the
// ELEMENT_ARRAY_BUFFER_BINDING pname — the code review's "0x8897"
// was neither, and keying on it would have made this fix inert. The
// H3 probe caught it: its first post-fix run bound an invalid target
// and read back null even for the unswitched VAO.)
function lkBufBindKey(rec, target) {
  return target === 0x8893 ? (rec.cache.vao || 0) + ':8893' : target;
}
lkDefGL('BindBuffer', (target, name) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  if (lkCacheMapChanged(rec.cache.bufBind, lkBufBindKey(rec, target), name))
    rec.gl.bindBuffer(target, name ? lkRegGet(rec, 'buffer', name) : null);
});
lkDefGL('BindTexture', (target, name) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const key = rec.cache.activeUnit + ':' + target;
  if (lkCacheMapChanged(rec.cache.texBind, key, name))
    rec.gl.bindTexture(target, name ? lkRegGet(rec, 'texture', name) : null);
});
lkDefGL('ActiveTexture', (unit) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const idx = unit - 0x84C0; // GL_TEXTURE0
  if (lkCacheChanged(rec, 'activeUnit', idx)) rec.gl.activeTexture(unit);
});
lkDefGL('BindFramebuffer', (target, name) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const targets = target === 0x8D40 ? [0x8D40, 0x8CA9, 0x8CA8] : [target]; // FRAMEBUFFER sets both
  let changed = false;
  for (const t of targets) if (lkCacheMapChanged(rec.cache.fbo, t, name)) changed = true;
  // Build 81: the cache keeps the ENGINE name (0 included), but the
  // physical binding for name 0 is the wrapper's double-buffer
  // surface — the stream never renders to the canvas directly.
  if (changed) rec.gl.bindFramebuffer(target, name ? lkRegGet(rec, 'framebuffer', name) : lkB81Surface(rec));
});
lkDefGL('BindRenderbuffer', (target, name) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  if (lkCacheChanged(rec, 'rbo', name))
    rec.gl.bindRenderbuffer(target, name ? lkRegGet(rec, 'renderbuffer', name) : null);
});
lkDefGL('BindVertexArray', (name) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  if (lkCacheChanged(rec, 'vao', name)) {
    rec.gl.bindVertexArray(name ? lkRegGet(rec, 'vao', name) : null);
    rec.cache.attribEnabled.clear();
  }
});
lkDefGL('BindVertexArrayOES', LK_GL['emscripten_glBindVertexArray']);
lkDefGL('UseProgram', (name) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  if (lkCacheChanged(rec, 'program', name))
    rec.gl.useProgram(name ? lkRegGet(rec, 'program', name) : null);
});
lkDefGL('BindSampler', (unit, name) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  if (lkCacheMapChanged(rec.cache.samplerBind, unit, name))
    rec.gl.bindSampler(unit, name ? lkRegGet(rec, 'sampler', name) : null);
});
lkDefGL('BindTransformFeedback', (target, name) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  rec.gl.bindTransformFeedback(target, name ? lkRegGet(rec, 'tfb', name) : null);
});
lkDefGL('BindBufferBase', (target, index, name) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  rec.gl.bindBufferBase(target, index, name ? lkRegGet(rec, 'buffer', name) : null);
  rec.cache.bufBind.set(lkBufBindKey(rec, target), name);
});
lkDefGL('BindBufferRange', (target, index, name, offset, size) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  rec.gl.bindBufferRange(target, index, name ? lkRegGet(rec, 'buffer', name) : null, offset, size);
  rec.cache.bufBind.set(lkBufBindKey(rec, target), name);
});

// --- enable/disable + simple state (cached) ---
function lkCap(name, isEnable) {
  lkDefGL(name, (cap) => {
    const rec = lkGLOrWarn(); if (!rec) return;
    if (lkCacheMapChanged(rec.cache.caps, cap, isEnable)) {
      if (isEnable) rec.gl.enable(cap); else rec.gl.disable(cap);
    }
  });
}
lkCap('Enable', true);
lkCap('Disable', false);
lkDefGL('IsEnabled', (cap) => {
  const rec = lkGLOrWarn(); if (!rec) return 0;
  const cached = rec.cache.caps.get(cap);
  return (cached !== undefined ? cached : rec.gl.isEnabled(cap)) ? 1 : 0;
});
lkDefGL('Viewport', (x, y, w, h) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  if (lkCacheChanged(rec, 'viewport', x + ',' + y + ',' + w + ',' + h)) rec.gl.viewport(x, y, w, h);
});
lkDefGL('Scissor', (x, y, w, h) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  if (lkCacheChanged(rec, 'scissor', x + ',' + y + ',' + w + ',' + h)) rec.gl.scissor(x, y, w, h);
});
lkDefGL('ClearColor', (r, g, b, a) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  if (lkCacheChanged(rec, 'clearColor', r + ',' + g + ',' + b + ',' + a)) rec.gl.clearColor(r, g, b, a);
});
lkDefGL('ClearDepthf', (d) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  if (lkCacheChanged(rec, 'clearDepth', d)) rec.gl.clearDepth(d);
});
lkDefGL('ClearStencil', (s) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  if (lkCacheChanged(rec, 'clearStencil', s)) rec.gl.clearStencil(s);
});
lkDefGL('Clear', (mask) => { const rec = lkGLOrWarn(); if (rec) rec.gl.clear(mask); });
lkDefGL('ColorMask', (r, g, b, a) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const v = (r ? 1 : 0) + ',' + (g ? 1 : 0) + ',' + (b ? 1 : 0) + ',' + (a ? 1 : 0);
  if (lkCacheChanged(rec, 'colorMask', v)) rec.gl.colorMask(!!r, !!g, !!b, !!a);
});
lkDefGL('DepthFunc', (f) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  if (lkCacheChanged(rec, 'depthFunc', f)) rec.gl.depthFunc(f);
});
lkDefGL('DepthMask', (flag) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  if (lkCacheChanged(rec, 'depthMask', !!flag)) rec.gl.depthMask(!!flag);
});
lkDefGL('DepthRangef', (n, f) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  if (lkCacheChanged(rec, 'depthRange', n + ',' + f)) rec.gl.depthRange(n, f);
});
lkDefGL('CullFace', (mode) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  if (lkCacheChanged(rec, 'cullFace', mode)) rec.gl.cullFace(mode);
});
lkDefGL('FrontFace', (mode) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  if (lkCacheChanged(rec, 'frontFace', mode)) rec.gl.frontFace(mode);
});
lkDefGL('BlendColor', (r, g, b, a) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  if (lkCacheChanged(rec, 'blendColor', r + ',' + g + ',' + b + ',' + a)) rec.gl.blendColor(r, g, b, a);
});
lkDefGL('BlendFunc', (sf, df) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  if (lkCacheChanged(rec, 'blendFunc', sf + ',' + df)) rec.gl.blendFunc(sf, df);
});
lkDefGL('BlendFuncSeparate', (srgb, drgb, salpha, dalpha) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const v = srgb + ',' + drgb + ',' + salpha + ',' + dalpha;
  if (lkCacheChanged(rec, 'blendFuncSep', v)) rec.gl.blendFuncSeparate(srgb, drgb, salpha, dalpha);
});
lkDefGL('BlendEquation', (mode) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  if (lkCacheChanged(rec, 'blendEq', String(mode))) rec.gl.blendEquation(mode);
});
lkDefGL('BlendEquationSeparate', (rgb, a) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  if (lkCacheChanged(rec, 'blendEqSep', rgb + ',' + a)) rec.gl.blendEquationSeparate(rgb, a);
});
lkDefGL('StencilFunc', (func, ref, mask) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const v = func + ',' + ref + ',' + mask;
  if (lkCacheChanged(rec, 'stencilFunc', v)) rec.gl.stencilFunc(func, ref, mask);
});
lkDefGL('StencilFuncSeparate', (face, func, ref, mask) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const v = face + ',' + func + ',' + ref + ',' + mask;
  if (lkCacheChanged(rec, 'stencilFuncSep', v)) rec.gl.stencilFuncSeparate(face, func, ref, mask);
});
lkDefGL('StencilMask', (mask) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  if (lkCacheChanged(rec, 'stencilMask', mask)) rec.gl.stencilMask(mask);
});
lkDefGL('StencilMaskSeparate', (face, mask) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const v = face + ',' + mask;
  if (lkCacheChanged(rec, 'stencilMaskSep', v)) rec.gl.stencilMaskSeparate(face, mask);
});
lkDefGL('StencilOp', (fail, zfail, zpass) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const v = fail + ',' + zfail + ',' + zpass;
  if (lkCacheChanged(rec, 'stencilOp', v)) rec.gl.stencilOp(fail, zfail, zpass);
});
lkDefGL('StencilOpSeparate', (face, fail, zfail, zpass) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const v = face + ',' + fail + ',' + zfail + ',' + zpass;
  if (lkCacheChanged(rec, 'stencilOpSep', v)) rec.gl.stencilOpSeparate(face, fail, zfail, zpass);
});
lkDefGL('LineWidth', (w) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  if (lkCacheChanged(rec, 'lineWidth', w)) rec.gl.lineWidth(w);
});
lkDefGL('PolygonOffset', (factor, units) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  if (lkCacheChanged(rec, 'polygonOffset', factor + ',' + units)) rec.gl.polygonOffset(factor, units);
});
lkDefGL('SampleCoverage', (value, invert) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const v = value + ',' + (invert ? 1 : 0);
  if (lkCacheChanged(rec, 'sampleCoverage', v)) rec.gl.sampleCoverage(value, !!invert);
});
lkDefGL('Hint', (target, mode) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  if (lkCacheMapChanged(rec.cache.hint, target, mode)) rec.gl.hint(target, mode);
});
lkDefGL('PixelStorei', (pname, param) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  if (lkCacheMapChanged(rec.cache.pixelStore, pname, param)) rec.gl.pixelStorei(pname, param);
});
lkDefGL('Flush', () => { const rec = lkGLOrWarn(); if (rec) rec.gl.flush(); });
lkDefGL('Finish', () => { const rec = lkGLOrWarn(); if (rec) rec.gl.finish(); });
lkDefGL('GetError', () => { const rec = lkGLOrWarn(); return rec ? rec.gl.getError() : 0; });
lkDefGL('ReleaseShaderCompiler', () => {});

// --- typed views over wasm memory for GL data movement ---
function lkTypeArray(ptr, type, count) {
  const buf = LK.memory.buffer;
  switch (type) {
    case 0x1401: case 0x1400: // UNSIGNED_BYTE, BYTE(returns u8 here)
      return new Uint8Array(buf, ptr, count);
    case 0x1402: return new Int16Array(buf, ptr, count);  // SHORT
    case 0x1403: return new Uint16Array(buf, ptr, count); // UNSIGNED_SHORT (+ packed 565/4444/5551)
    case 0x1404: return new Int32Array(buf, ptr, count);  // INT
    case 0x1405: return new Uint32Array(buf, ptr, count); // UNSIGNED_INT (+ packed)
    case 0x1406: return new Float32Array(buf, ptr, count); // FLOAT
    case 0x140B: return new Uint16Array(buf, ptr, count); // HALF_FLOAT
    default: return new Uint8Array(buf, ptr, count * 4);
  }
}
function lkTypeSize(type) {
  switch (type) {
    case 0x1400: case 0x1401: return 1;
    case 0x1402: case 0x1403: case 0x140B: case 0x8033: case 0x8034: case 0x8032: return 2;
    case 0x1404: case 0x1405: case 0x1406: case 0x8036: case 0x84FA: case 0x8DAD: return 4;
    case 0x8DAB: return 8; // FLOAT_32_UNSIGNED_INT_24_8_REV
    default: return 1;
  }
}
function lkFormatComponents(format) {
  switch (format) {
    case 0x1903: case 0x8225: case 0x8D94: case 0x1902: case 0x1800: case 0x1906: return 1; // RED, RED_INTEGER, DEPTH, ALPHA
    case 0x8227: case 0x8226: case 0x8D95: case 0x190A: return 2; // RG, RG_INTEGER, LUMINANCE_ALPHA
    case 0x1907: case 0x8D96: case 0x8F92: return 3;               // RGB, RGB_INTEGER, SRGB
    case 0x1908: case 0x8D97: case 0x80E1: case 0x8F93: return 4; // RGBA, RGBA_INTEGER, BGRA, SRGB_ALPHA
    case 0x84F9: return 1; // DEPTH_STENCIL (packed element)
    default: return 4;
  }
}
// A view of the right element type/length for a texImage-style upload.
function lkTexView(ptr, w, h, d, format, type) {
  const packed = [0x8033, 0x8034, 0x8032, 0x8036, 0x84FA, 0x8DAD, 0x8DAB].includes(type);
  const elems = packed ? w * h * d : w * h * d * lkFormatComponents(format);
  const byteLen = elems * lkTypeSize(type);
  const avail = LK.memory.buffer.byteLength - ptr;
  const clamped = Math.max(0, Math.min(byteLen, avail));
  const per = lkTypeSize(type);
  return lkTypeArray(ptr, type, Math.floor(clamped / per));
}

// --- buffers ---
lkDefGL('BufferData', (target, size, ptr, usage) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  if (ptr) rec.gl.bufferData(target, new Uint8Array(LK.memory.buffer, ptr, size), usage);
  else rec.gl.bufferData(target, size, usage);
  const bound = rec.cache.bufBind.get(lkBufBindKey(rec, target));
  if (bound) rec.bufMeta.set(bound, { size, usage });
});
lkDefGL('BufferSubData', (target, offset, size, ptr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  rec.gl.bufferSubData(target, offset, new Uint8Array(LK.memory.buffer, ptr, size));
});
lkDefGL('CopyBufferSubData', (rt, wt, ro, wo, size) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  rec.gl.copyBufferSubData(rt, wt, ro, wo, size);
});
lkDefGL('GetBufferParameteriv', (target, pname, ptr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const v = rec.gl.getBufferParameter(target, pname);
  lkI32()[ptr >> 2] = typeof v === 'boolean' ? (v ? 1 : 0) : v | 0;
});
lkDefGL('GetBufferParameteri64v', (target, pname, ptr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const v = rec.gl.getBufferParameter(target, pname);
  lkI64()[ptr >> 3] = BigInt(typeof v === 'number' ? v : 0);
});

// --- textures ---
lkDefGL('TexImage2D', (target, level, internal, w, h, border, format, type, ptr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const gl = rec.gl;
  if (rec.cache.bufBind.get(0x88EC)) { gl.texImage2D(target, level, internal, w, h, border, format, type, ptr); return; } // PIXEL_UNPACK_BUFFER: ptr is an offset
  if (!ptr) { gl.texImage2D(target, level, internal, w, h, border, format, type, null); return; }
  gl.texImage2D(target, level, internal, w, h, border, format, type, lkTexView(ptr, w, h, 1, format, type));
});
lkDefGL('TexSubImage2D', (target, level, x, y, w, h, format, type, ptr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const gl = rec.gl;
  if (rec.cache.bufBind.get(0x88EC)) { gl.texSubImage2D(target, level, x, y, w, h, format, type, ptr); return; }
  gl.texSubImage2D(target, level, x, y, w, h, format, type, lkTexView(ptr, w, h, 1, format, type));
});
lkDefGL('TexImage3D', (target, level, internal, w, h, d, border, format, type, ptr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const gl = rec.gl;
  if (rec.cache.bufBind.get(0x88EC)) { gl.texImage3D(target, level, internal, w, h, d, border, format, type, ptr); return; }
  if (!ptr) { gl.texImage3D(target, level, internal, w, h, d, border, format, type, null); return; }
  gl.texImage3D(target, level, internal, w, h, d, border, format, type, lkTexView(ptr, w, h, d, format, type));
});
lkDefGL('TexSubImage3D', (target, level, x, y, z, w, h, d, format, type, ptr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const gl = rec.gl;
  if (rec.cache.bufBind.get(0x88EC)) { gl.texSubImage3D(target, level, x, y, z, w, h, d, format, type, ptr); return; }
  gl.texSubImage3D(target, level, x, y, z, w, h, d, format, type, lkTexView(ptr, w, h, d, format, type));
});
lkDefGL('CompressedTexImage2D', (target, level, internal, w, h, border, size, ptr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  if (rec.cache.bufBind.get(0x88EC)) { rec.gl.compressedTexImage2D(target, level, internal, w, h, border, size, ptr); return; }
  rec.gl.compressedTexImage2D(target, level, internal, w, h, border, new Uint8Array(LK.memory.buffer, ptr, size));
});
lkDefGL('CompressedTexSubImage2D', (target, level, x, y, w, h, format, size, ptr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  if (rec.cache.bufBind.get(0x88EC)) { rec.gl.compressedTexSubImage2D(target, level, x, y, w, h, format, size, ptr); return; }
  rec.gl.compressedTexSubImage2D(target, level, x, y, w, h, format, new Uint8Array(LK.memory.buffer, ptr, size));
});
lkDefGL('CompressedTexImage3D', (target, level, internal, w, h, d, border, size, ptr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  if (rec.cache.bufBind.get(0x88EC)) { rec.gl.compressedTexImage3D(target, level, internal, w, h, d, border, size, ptr); return; }
  rec.gl.compressedTexImage3D(target, level, internal, w, h, d, border, new Uint8Array(LK.memory.buffer, ptr, size));
});
lkDefGL('CompressedTexSubImage3D', (target, level, x, y, z, w, h, d, format, size, ptr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  if (rec.cache.bufBind.get(0x88EC)) { rec.gl.compressedTexSubImage3D(target, level, x, y, z, w, h, d, format, size, ptr); return; }
  rec.gl.compressedTexSubImage3D(target, level, x, y, z, w, h, d, format, new Uint8Array(LK.memory.buffer, ptr, size));
});
lkDefGL('CopyTexImage2D', (target, level, internal, x, y, w, h, border) => {
  const rec = lkGLOrWarn(); if (rec) rec.gl.copyTexImage2D(target, level, internal, x, y, w, h, border);
});
lkDefGL('CopyTexSubImage2D', (target, level, xoff, yoff, x, y, w, h) => {
  const rec = lkGLOrWarn(); if (rec) rec.gl.copyTexSubImage2D(target, level, xoff, yoff, x, y, w, h);
});
lkDefGL('CopyTexSubImage3D', (target, level, xoff, yoff, zoff, x, y, w, h) => {
  const rec = lkGLOrWarn(); if (rec) rec.gl.copyTexSubImage3D(target, level, xoff, yoff, zoff, x, y, w, h);
});
lkDefGL('TexStorage2D', (target, levels, internal, w, h) => {
  const rec = lkGLOrWarn(); if (rec) rec.gl.texStorage2D(target, levels, internal, w, h);
});
lkDefGL('TexStorage3D', (target, levels, internal, w, h, d) => {
  const rec = lkGLOrWarn(); if (rec) rec.gl.texStorage3D(target, levels, internal, w, h, d);
});
lkDefGL('GenerateMipmap', (target) => { const rec = lkGLOrWarn(); if (rec) rec.gl.generateMipmap(target); });
lkDefGL('TexParameterf', (target, pname, param) => { const rec = lkGLOrWarn(); if (rec) rec.gl.texParameterf(target, pname, param); });
lkDefGL('TexParameteri', (target, pname, param) => { const rec = lkGLOrWarn(); if (rec) rec.gl.texParameteri(target, pname, param); });
lkDefGL('TexParameterfv', (target, pname, ptr) => {
  const rec = lkGLOrWarn(); if (rec) rec.gl.texParameterf(target, pname, lkF32()[ptr >> 2]);
});
lkDefGL('TexParameteriv', (target, pname, ptr) => {
  const rec = lkGLOrWarn(); if (rec) rec.gl.texParameteri(target, pname, lkI32()[ptr >> 2]);
});
lkDefGL('GetTexParameterfv', (target, pname, ptr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const v = rec.gl.getTexParameter(target, pname);
  lkF32()[ptr >> 2] = typeof v === 'number' ? v : 0;
});
lkDefGL('GetTexParameteriv', (target, pname, ptr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const v = rec.gl.getTexParameter(target, pname);
  lkI32()[ptr >> 2] = typeof v === 'boolean' ? (v ? 1 : 0) : (v | 0);
});
lkDefGL('ReadPixels', (x, y, w, h, format, type, ptr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  // M8: with a PIXEL_PACK_BUFFER (target 0x88EB) bound, the pointer
  // argument is a byte OFFSET into that buffer, not a memory address
  // — mirror the upload side (which passes ptr through for 0x88EC).
  if (rec.cache.bufBind.get(0x88EB)) { rec.gl.readPixels(x, y, w, h, format, type, ptr); return; }
  rec.gl.readPixels(x, y, w, h, format, type, lkTexView(ptr, w, h, 1, format, type));
});
lkDefGL('ReadBuffer', (mode) => { const rec = lkGLOrWarn(); if (rec) rec.gl.readBuffer(mode); });

// --- framebuffers / renderbuffers ---
lkDefGL('CheckFramebufferStatus', (target) => {
  const rec = lkGLOrWarn(); return rec ? rec.gl.checkFramebufferStatus(target) : 0;
});
lkDefGL('FramebufferTexture2D', (target, attachment, textarget, texName, level) => {
  const rec = lkGLOrWarn(); if (rec) rec.gl.framebufferTexture2D(target, attachment, textarget, texName ? lkRegGet(rec, 'texture', texName) : null, level);
});
lkDefGL('FramebufferTextureLayer', (target, attachment, texName, level, layer) => {
  const rec = lkGLOrWarn(); if (rec) rec.gl.framebufferTextureLayer(target, attachment, texName ? lkRegGet(rec, 'texture', texName) : null, level, layer);
});
lkDefGL('FramebufferRenderbuffer', (target, attachment, rbtarget, rbName) => {
  const rec = lkGLOrWarn(); if (rec) rec.gl.framebufferRenderbuffer(target, attachment, rbtarget, rbName ? lkRegGet(rec, 'renderbuffer', rbName) : null);
});
lkDefGL('RenderbufferStorage', (target, internal, w, h) => {
  const rec = lkGLOrWarn(); if (rec) rec.gl.renderbufferStorage(target, internal, w, h);
});
lkDefGL('RenderbufferStorageMultisample', (target, samples, internal, w, h) => {
  const rec = lkGLOrWarn(); if (rec) rec.gl.renderbufferStorageMultisample(target, samples, internal, w, h);
});
lkDefGL('BlitFramebuffer', (sx0, sy0, sx1, sy1, dx0, dy0, dx1, dy1, mask, filter) => {
  const rec = lkGLOrWarn(); if (rec) rec.gl.blitFramebuffer(sx0, sy0, sx1, sy1, dx0, dy0, dx1, dy1, mask, filter);
});
lkDefGL('DrawBuffers', (n, ptr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const arr = [];
  const i32 = lkI32();
  for (let i = 0; i < n; i++) arr.push(i32[(ptr >> 2) + i]);
  rec.gl.drawBuffers(arr);
});
lkDefGL('DrawBuffersEXT', LK_GL['emscripten_glDrawBuffers']);
lkDefGL('DrawBuffersWEBGL', LK_GL['emscripten_glDrawBuffers']);
lkDefGL('InvalidateFramebuffer', (target, n, ptr) => {
  const rec = lkGLOrWarn(); if (!rec || !rec.gl.invalidateFramebuffer) return;
  const arr = []; const i32 = lkI32();
  for (let i = 0; i < n; i++) arr.push(i32[(ptr >> 2) + i]);
  rec.gl.invalidateFramebuffer(target, arr);
});
lkDefGL('InvalidateSubFramebuffer', (target, n, ptr, x, y, w, h) => {
  const rec = lkGLOrWarn(); if (!rec || !rec.gl.invalidateSubFramebuffer) return;
  const arr = []; const i32 = lkI32();
  for (let i = 0; i < n; i++) arr.push(i32[(ptr >> 2) + i]);
  rec.gl.invalidateSubFramebuffer(target, arr, x, y, w, h);
});
lkDefGL('GetFramebufferAttachmentParameteriv', (target, attachment, pname, ptr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const v = rec.gl.getFramebufferAttachmentParameter(target, attachment, pname);
  if (pname === 0x8CD1) { // FRAMEBUFFER_ATTACHMENT_OBJECT_NAME: object -> name
    let name = 0;
    if (v) {
      name = lkRegName(rec, 'texture', v) || lkRegName(rec, 'renderbuffer', v);
    }
    lkI32()[ptr >> 2] = name;
  } else {
    lkI32()[ptr >> 2] = typeof v === 'boolean' ? (v ? 1 : 0) : (v | 0);
  }
});
lkDefGL('GetRenderbufferParameteriv', (target, pname, ptr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const v = rec.gl.getRenderbufferParameter(target, pname);
  lkI32()[ptr >> 2] = typeof v === 'boolean' ? (v ? 1 : 0) : (v | 0);
});
lkDefGL('GetInternalformativ', (target, internal, pname, bufSize, ptr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const v = rec.gl.getInternalformatParameter(target, internal, pname);
  if (v) { const i32 = lkI32(); for (let i = 0; i < Math.min(bufSize, v.length); i++) i32[(ptr >> 2) + i] = v[i]; }
});

// --- shaders / programs ---
lkDefGL('ShaderSource', (shader, count, strPtr, lenPtr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const obj = lkRegGet(rec, 'shader', shader); if (!obj) return;
  const u32 = lkU32(), i32 = lkI32();
  let src = '';
  for (let i = 0; i < count; i++) {
    const p = u32[(strPtr >> 2) + i];
    const len = lenPtr ? i32[(lenPtr >> 2) + i] : -1;
    src += len >= 0 ? lkReadCStr(p, len) : lkReadCStr(p);
  }
  rec.gl.shaderSource(obj, src);
});
lkDefGL('CompileShader', (shader) => {
  const rec = lkGLOrWarn(); if (rec) { const o = lkRegGet(rec, 'shader', shader); if (o) rec.gl.compileShader(o); }
});
lkDefGL('GetShaderSource', (shader, bufSize, lenPtr, srcPtr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const o = lkRegGet(rec, 'shader', shader);
  const src = o ? (rec.gl.getShaderSource(o) || '') : '';
  const written = lkWriteCStr(srcPtr, src, bufSize);
  if (lenPtr) lkI32()[lenPtr >> 2] = written;
});
lkDefGL('GetShaderInfoLog', (shader, bufSize, lenPtr, logPtr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const o = lkRegGet(rec, 'shader', shader);
  const log = o ? (rec.gl.getShaderInfoLog(o) || '') : '';
  const written = lkWriteCStr(logPtr, log, bufSize);
  if (lenPtr) lkI32()[lenPtr >> 2] = written;
});
lkDefGL('GetShaderiv', (shader, pname, ptr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const o = lkRegGet(rec, 'shader', shader); if (!o) { lkI32()[ptr >> 2] = 0; return; }
  if (pname === 0x8B84) { // INFO_LOG_LENGTH
    lkI32()[ptr >> 2] = (rec.gl.getShaderInfoLog(o) || '').length + 1;
    return;
  }
  const v = rec.gl.getShaderParameter(o, pname);
  lkI32()[ptr >> 2] = typeof v === 'boolean' ? (v ? 1 : 0) : (v | 0);
});
lkDefGL('GetShaderPrecisionFormat', (type, precisionType, rangePtr, precisionPtr) => {
  const rec = lkGLOrWarn(); if (!rec) return 0;
  const f = rec.gl.getShaderPrecisionFormat(type, precisionType);
  if (f) {
    lkI32()[rangePtr >> 2] = f.rangeMin;
    lkI32()[(rangePtr >> 2) + 1] = f.rangeMax;
    lkI32()[precisionPtr >> 2] = f.precision;
    return f.precision;
  }
  return 0;
});
lkDefGL('ShaderBinary', () => {});
lkDefGL('AttachShader', (prog, shader) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const p = lkRegGet(rec, 'program', prog), s = lkRegGet(rec, 'shader', shader);
  if (p && s) rec.gl.attachShader(p, s);
});
lkDefGL('DetachShader', (prog, shader) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const p = lkRegGet(rec, 'program', prog), s = lkRegGet(rec, 'shader', shader);
  if (p && s) rec.gl.detachShader(p, s);
});
lkDefGL('BindAttribLocation', (prog, index, namePtr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const p = lkRegGet(rec, 'program', prog); if (p) rec.gl.bindAttribLocation(p, index, lkReadCStr(namePtr));
});
lkDefGL('LinkProgram', (prog) => {
  const rec = lkGLOrWarn(); if (rec) { const p = lkRegGet(rec, 'program', prog); if (p) rec.gl.linkProgram(p); }
});
lkDefGL('ValidateProgram', (prog) => {
  const rec = lkGLOrWarn(); if (rec) { const p = lkRegGet(rec, 'program', prog); if (p) rec.gl.validateProgram(p); }
});
lkDefGL('GetProgramiv', (prog, pname, ptr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const p = lkRegGet(rec, 'program', prog); if (!p) { lkI32()[ptr >> 2] = 0; return; }
  if (pname === 0x8B84) { lkI32()[ptr >> 2] = (rec.gl.getProgramInfoLog(p) || '').length + 1; return; }
  const v = rec.gl.getProgramParameter(p, pname);
  lkI32()[ptr >> 2] = typeof v === 'boolean' ? (v ? 1 : 0) : (v | 0);
});
lkDefGL('GetProgramInfoLog', (prog, bufSize, lenPtr, logPtr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const p = lkRegGet(rec, 'program', prog);
  const log = p ? (rec.gl.getProgramInfoLog(p) || '') : '';
  const written = lkWriteCStr(logPtr, log, bufSize);
  if (lenPtr) lkI32()[lenPtr >> 2] = written;
});
lkDefGL('GetAttachedShaders', (prog, maxCount, countPtr, shadersPtr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const p = lkRegGet(rec, 'program', prog); if (!p) return;
  const list = rec.gl.getAttachedShaders(p) || [];
  const u32 = lkU32();
  let n = 0;
  for (const s of list) {
    if (n >= maxCount) break;
    u32[(shadersPtr >> 2) + n] = lkRegName(rec, 'shader', s);
    n++;
  }
  if (countPtr) u32[countPtr >> 2] = n;
});
lkDefGL('GetActiveAttrib', (prog, index, bufSize, lenPtr, sizePtr, typePtr, namePtr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const p = lkRegGet(rec, 'program', prog); if (!p) return;
  const info = rec.gl.getActiveAttrib(p, index); if (!info) return;
  const written = lkWriteCStr(namePtr, info.name, bufSize);
  if (lenPtr) lkI32()[lenPtr >> 2] = written;
  if (sizePtr) lkI32()[sizePtr >> 2] = info.size;
  if (typePtr) lkI32()[typePtr >> 2] = info.type;
});
lkDefGL('GetActiveUniform', (prog, index, bufSize, lenPtr, sizePtr, typePtr, namePtr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const p = lkRegGet(rec, 'program', prog); if (!p) return;
  const info = rec.gl.getActiveUniform(p, index); if (!info) return;
  const written = lkWriteCStr(namePtr, info.name, bufSize);
  if (lenPtr) lkI32()[lenPtr >> 2] = written;
  if (sizePtr) lkI32()[sizePtr >> 2] = info.size;
  if (typePtr) lkI32()[typePtr >> 2] = info.type;
});
lkDefGL('GetAttribLocation', (prog, namePtr) => {
  const rec = lkGLOrWarn(); if (!rec) return -1;
  const p = lkRegGet(rec, 'program', prog); if (!p) return -1;
  return rec.gl.getAttribLocation(p, lkReadCStr(namePtr));
});
// --- confessed stub registry (M7, bug-fix run) ---------------------
// Every wrapper entry that does NOT faithfully implement its GL call
// is listed here, with its exact behavior, so no stub can hide in the
// table. Anything not listed is a real implementation.
//   GetFragDataLocation — returns -1 (no WebGL2 equivalent; -1 is the
//     legal GL answer for "no such output").
//   GetProgramBinary — writes length 0 (WebGL exposes no program
//     binaries; an engine that caches binaries must recompile).
//   ProgramBinary — no-op (pair of the above).
//   ClientWaitSync — single non-blocking poll of SYNC_STATUS (+ a real
//     gl.flush() when SYNC_FLUSH_COMMANDS_BIT is set, per spec); it
//     does NOT wait out the caller's timeout. Legal return values
//     only (ALREADY_SIGNALED / TIMEOUT_EXPIRED / WAIT_FAILED).
//   QueryCounterEXT — emulated as an immediate begin/end pair
//     (WebGL2 has no queryCounter); the counter value is approximate.
// (WaitSync used to sit on this list as a no-op; it is implemented
//  for real below via the GPU-side wait WebGL2 provides.)
lkDefGL('GetFragDataLocation', () => -1); // no WebGL2 equivalent; -1 is the legal answer
lkDefGL('GetProgramBinary', (prog, bufSize, lenPtr, fmtPtr, binPtr) => {
  if (lenPtr) lkI32()[lenPtr >> 2] = 0; // WebGL exposes no program binaries
});
lkDefGL('ProgramBinary', () => {});
lkDefGL('ProgramParameteri', (prog, pname, value) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const p = lkRegGet(rec, 'program', prog);
  if (p && rec.gl.programParameteri) rec.gl.programParameteri(p, pname, value);
});

// --- uniforms ---
function lkLocEntry(rec, locId) { return rec.locById.get(locId); }
lkDefGL('GetUniformLocation', (prog, namePtr) => {
  const rec = lkGLOrWarn(); if (!rec) return -1;
  const p = lkRegGet(rec, 'program', prog); if (!p) return -1;
  const name = lkReadCStr(namePtr);
  let perProg = rec.locs.get(prog);
  if (!perProg) { perProg = new Map(); rec.locs.set(prog, perProg); }
  const existing = perProg.get(name);
  if (existing) return existing.id;
  const loc = rec.gl.getUniformLocation(p, name);
  if (!loc) return -1;
  const id = rec.nextLoc++;
  const entry = { id, loc, prog };
  perProg.set(name, entry);
  rec.locById.set(id, entry);
  return id;
});
function lkUniformCall(rec, locId, cacheKey, apply) {
  const entry = lkLocEntry(rec, locId);
  if (!entry) return;
  if (cacheKey !== null) {
    if (rec.cache.uniforms.get(locId) === cacheKey) return;
    rec.cache.uniforms.set(locId, cacheKey);
  }
  apply(rec.gl, entry.loc);
}
const LK_U_SCALARS = {
  Uniform1f: ['f', 1], Uniform2f: ['f', 2], Uniform3f: ['f', 3], Uniform4f: ['f', 4],
  Uniform1i: ['i', 1], Uniform2i: ['i', 2], Uniform3i: ['i', 3], Uniform4i: ['i', 4],
  Uniform1ui: ['ui', 1], Uniform2ui: ['ui', 2], Uniform3ui: ['ui', 3], Uniform4ui: ['ui', 4],
};
for (const [uname, [kind, n]] of Object.entries(LK_U_SCALARS)) {
  lkDefGL(uname, (...args) => {
    const rec = lkGLOrWarn(); if (!rec) return;
    const vals = args.slice(1, 1 + n);
    lkUniformCall(rec, args[0], kind + ':' + vals.join(','), (gl, loc) => {
      gl['uniform' + n + kind](loc, ...vals);
    });
  });
}
const LK_U_VECS = {
  Uniform1fv: ['f', 1, 'F32'], Uniform2fv: ['f', 2, 'F32'], Uniform3fv: ['f', 3, 'F32'], Uniform4fv: ['f', 4, 'F32'],
  Uniform1iv: ['i', 1, 'I32'], Uniform2iv: ['i', 2, 'I32'], Uniform3iv: ['i', 3, 'I32'], Uniform4iv: ['i', 4, 'I32'],
  Uniform1uiv: ['ui', 1, 'U32'], Uniform2uiv: ['ui', 2, 'U32'], Uniform3uiv: ['ui', 3, 'U32'], Uniform4uiv: ['ui', 4, 'U32'],
};
for (const [uname, [kind, n, view]] of Object.entries(LK_U_VECS)) {
  lkDefGL(uname, (locId, count, ptr) => {
    const rec = lkGLOrWarn(); if (!rec) return;
    const entry = lkLocEntry(rec, locId); if (!entry) return;
    const Ctor = view === 'F32' ? Float32Array : view === 'I32' ? Int32Array : Uint32Array;
    const arr = new Ctor(LK.memory.buffer, ptr, count * n);
    // M1: the dedup key must cover the WHOLE array — a 16-element
    // prefix key suppressed real updates for arrays differing past
    // element 16 (e.g. light arrays). FNV-1a over the raw u32 words
    // of the full array + its length.
    const words = new Uint32Array(LK.memory.buffer, ptr, count * n);
    let hsh = 0x811c9dc5;
    for (let i = 0; i < words.length; i++) { hsh ^= words[i]; hsh = Math.imul(hsh, 0x01000193); }
    const key = kind + 'v:' + words.length + ':' + (hsh >>> 0).toString(36);
    lkUniformCall(rec, locId, key, (gl, loc) => gl['uniform' + n + kind + 'v'](loc, arr));
  });
}
const LK_U_MATS = {
  UniformMatrix2fv: [2, 2], UniformMatrix3fv: [3, 3], UniformMatrix4fv: [4, 4],
  UniformMatrix2x3fv: [2, 3], UniformMatrix3x2fv: [3, 2], UniformMatrix2x4fv: [2, 4],
  UniformMatrix4x2fv: [4, 2], UniformMatrix3x4fv: [3, 4], UniformMatrix4x3fv: [4, 3],
};
for (const [uname, [cols, rows]] of Object.entries(LK_U_MATS)) {
  const label = cols === rows ? String(cols) : cols + 'x' + rows;
  lkDefGL(uname, (locId, count, transpose, ptr) => {
    const rec = lkGLOrWarn(); if (!rec) return;
    const entry = lkLocEntry(rec, locId); if (!entry) return;
    const arr = new Float32Array(LK.memory.buffer, ptr, count * cols * rows);
    rec.gl['uniformMatrix' + label + 'fv'](entry.loc, !!transpose, arr);
  });
}
function lkWriteUniformValues(rec, progName, locId, ptr, asUnsigned) {
  const p = lkRegGet(rec, 'program', progName); if (!p) return;
  const entry = lkLocEntry(rec, locId); if (!entry) return;
  const v = rec.gl.getUniform(p, entry.loc);
  const write = (arr, Ctor, setter) => {
    const view = new Ctor(LK.memory.buffer, ptr, arr.length);
    for (let i = 0; i < arr.length; i++) setter(view, i, arr[i]);
  };
  if (v === null || v === undefined) return;
  if (typeof v === 'number') { if (asUnsigned) lkU32()[ptr >> 2] = v; else lkF32()[ptr >> 2] = v; return; }
  if (typeof v === 'boolean') { lkI32()[ptr >> 2] = v ? 1 : 0; return; }
  if (v instanceof Float32Array) write(v, Float32Array, (a, i, x) => a[i] = x);
  else if (v instanceof Uint32Array) write(v, Uint32Array, (a, i, x) => a[i] = x);
  else if (v instanceof Int32Array) write(v, Int32Array, (a, i, x) => a[i] = x);
  else if (Array.isArray(v)) { const f = lkF32(); for (let i = 0; i < v.length; i++) f[(ptr >> 2) + i] = v[i]; }
}
lkDefGL('GetUniformfv', (prog, locId, ptr) => { const rec = lkGLOrWarn(); if (rec) lkWriteUniformValues(rec, prog, locId, ptr, false); });
lkDefGL('GetUniformiv', (prog, locId, ptr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const p = lkRegGet(rec, 'program', prog); if (!p) return;
  const entry = lkLocEntry(rec, locId); if (!entry) return;
  const v = rec.gl.getUniform(p, entry.loc);
  if (v === null || v === undefined) return;
  if (typeof v === 'number') lkI32()[ptr >> 2] = v | 0;
  else if (typeof v === 'boolean') lkI32()[ptr >> 2] = v ? 1 : 0;
  else { const i32 = lkI32(); for (let i = 0; i < v.length; i++) i32[(ptr >> 2) + i] = v[i]; }
});
lkDefGL('GetUniformuiv', (prog, locId, ptr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const p = lkRegGet(rec, 'program', prog); if (!p) return;
  const entry = lkLocEntry(rec, locId); if (!entry) return;
  const v = rec.gl.getUniform(p, entry.loc);
  if (v === null || v === undefined) return;
  if (typeof v === 'number') lkU32()[ptr >> 2] = v >>> 0;
  else { const u32 = lkU32(); for (let i = 0; i < v.length; i++) u32[(ptr >> 2) + i] = v[i]; }
});
lkDefGL('GetUniformIndices', (prog, count, namesPtr, indicesPtr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const p = lkRegGet(rec, 'program', prog); if (!p) return;
  const u32 = lkU32();
  const names = [];
  for (let i = 0; i < count; i++) names.push(lkReadCStr(u32[(namesPtr >> 2) + i]));
  const result = rec.gl.getUniformIndices(p, names) || [];
  for (let i = 0; i < count; i++) u32[(indicesPtr >> 2) + i] = result[i] === null || result[i] === undefined ? 0xFFFFFFFF : result[i];
});
lkDefGL('GetActiveUniformsiv', (prog, count, indicesPtr, pname, ptr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const p = lkRegGet(rec, 'program', prog); if (!p) return;
  const u32 = lkU32();
  const idx = [];
  for (let i = 0; i < count; i++) idx.push(u32[(indicesPtr >> 2) + i]);
  const vals = rec.gl.getActiveUniforms(p, idx, pname) || [];
  const i32 = lkI32();
  for (let i = 0; i < vals.length; i++) i32[(ptr >> 2) + i] = typeof vals[i] === 'boolean' ? (vals[i] ? 1 : 0) : vals[i] | 0;
});
lkDefGL('GetUniformBlockIndex', (prog, namePtr) => {
  const rec = lkGLOrWarn(); if (!rec) return 0xFFFFFFFF;
  const p = lkRegGet(rec, 'program', prog); if (!p) return 0xFFFFFFFF;
  const v = rec.gl.getUniformBlockIndex(p, lkReadCStr(namePtr));
  return v === null || v === undefined ? 0xFFFFFFFF : v;
});
lkDefGL('GetActiveUniformBlockiv', (prog, blockIndex, pname, ptr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const p = lkRegGet(rec, 'program', prog); if (!p) return;
  const v = rec.gl.getActiveUniformBlockParameter(p, blockIndex, pname);
  if (v === null || v === undefined) return;
  const i32 = lkI32();
  if (typeof v === 'number' || typeof v === 'boolean') i32[ptr >> 2] = typeof v === 'boolean' ? (v ? 1 : 0) : v | 0;
  else for (let i = 0; i < v.length; i++) i32[(ptr >> 2) + i] = v[i];
});
lkDefGL('GetActiveUniformBlockName', (prog, blockIndex, bufSize, lenPtr, namePtr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const p = lkRegGet(rec, 'program', prog); if (!p) return;
  const name = rec.gl.getActiveUniformBlockName(p, blockIndex) || '';
  const written = lkWriteCStr(namePtr, name, bufSize);
  if (lenPtr) lkI32()[lenPtr >> 2] = written;
});
lkDefGL('UniformBlockBinding', (prog, blockIndex, binding) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const p = lkRegGet(rec, 'program', prog); if (p) rec.gl.uniformBlockBinding(p, blockIndex, binding);
});
lkDefGL('TransformFeedbackVaryings', (prog, count, varyingsPtr, bufferMode) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const p = lkRegGet(rec, 'program', prog); if (!p) return;
  const u32 = lkU32();
  const names = [];
  for (let i = 0; i < count; i++) names.push(lkReadCStr(u32[(varyingsPtr >> 2) + i]));
  rec.gl.transformFeedbackVaryings(p, names, bufferMode);
});
lkDefGL('GetTransformFeedbackVarying', (prog, index, bufSize, lenPtr, sizePtr, typePtr, namePtr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const p = lkRegGet(rec, 'program', prog); if (!p) return;
  const info = rec.gl.getTransformFeedbackVarying(p, index); if (!info) return;
  const written = lkWriteCStr(namePtr, info.name, bufSize);
  if (lenPtr) lkI32()[lenPtr >> 2] = written;
  if (sizePtr) lkI32()[sizePtr >> 2] = info.size;
  if (typePtr) lkI32()[typePtr >> 2] = info.type;
});

// --- vertex attribs ---
function lkAttribRecord(rec, index) {
  const vaoName = rec.cache.vao;
  let perVao = rec.attribs.get(vaoName);
  if (!perVao) { perVao = new Map(); rec.attribs.set(vaoName, perVao); }
  let a = perVao.get(index);
  if (!a) { a = { size: 4, type: 0x1406, normalized: false, stride: 0, offset: 0, buffer: 0, divisor: 0, isInt: false }; perVao.set(index, a); }
  return a;
}
lkDefGL('VertexAttribPointer', (index, size, type, normalized, stride, ptr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  rec.gl.vertexAttribPointer(index, size, type, !!normalized, stride, ptr);
  const a = lkAttribRecord(rec, index);
  a.size = size; a.type = type; a.normalized = !!normalized; a.stride = stride; a.offset = ptr;
  a.buffer = rec.cache.bufBind.get(0x8892) || 0; a.isInt = false;
});
lkDefGL('VertexAttribIPointer', (index, size, type, stride, ptr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  rec.gl.vertexAttribIPointer(index, size, type, stride, ptr);
  const a = lkAttribRecord(rec, index);
  a.size = size; a.type = type; a.stride = stride; a.offset = ptr;
  a.buffer = rec.cache.bufBind.get(0x8892) || 0; a.isInt = true;
});
function lkAttribEnabled(name, isEnable) {
  lkDefGL(name, (index) => {
    const rec = lkGLOrWarn(); if (!rec) return;
    const key = rec.cache.vao + ':' + index;
    if (lkCacheMapChanged(rec.cache.attribEnabled, key, isEnable)) {
      if (isEnable) rec.gl.enableVertexAttribArray(index); else rec.gl.disableVertexAttribArray(index);
    }
  });
}
lkAttribEnabled('EnableVertexAttribArray', true);
lkAttribEnabled('DisableVertexAttribArray', false);
function lkDivisor(name) {
  lkDefGL(name, (index, divisor) => {
    const rec = lkGLOrWarn(); if (!rec) return;
    rec.gl.vertexAttribDivisor(index, divisor);
    lkAttribRecord(rec, index).divisor = divisor;
  });
}
lkDivisor('VertexAttribDivisor');
lkDivisor('VertexAttribDivisorANGLE');
lkDivisor('VertexAttribDivisorEXT');
lkDivisor('VertexAttribDivisorNV');
lkDivisor('VertexAttribDivisorARB');
const LK_VA_SCALARS = {
  VertexAttrib1f: 1, VertexAttrib2f: 2, VertexAttrib3f: 3, VertexAttrib4f: 4,
};
for (const [vname, n] of Object.entries(LK_VA_SCALARS)) {
  lkDefGL(vname, (index, ...vals) => {
    const rec = lkGLOrWarn(); if (rec) rec.gl['vertexAttrib' + n + 'f'](index, ...vals.slice(0, n));
  });
  lkDefGL(vname + 'v', (index, ptr) => {
    const rec = lkGLOrWarn(); if (!rec) return;
    const arr = new Float32Array(LK.memory.buffer, ptr, n);
    rec.gl['vertexAttrib' + n + 'fv'](index, arr);
  });
}
lkDefGL('VertexAttribI4i', (index, x, y, z, w) => { const rec = lkGLOrWarn(); if (rec) rec.gl.vertexAttribI4i(index, x, y, z, w); });
lkDefGL('VertexAttribI4ui', (index, x, y, z, w) => { const rec = lkGLOrWarn(); if (rec) rec.gl.vertexAttribI4ui(index, x, y, z, w); });
lkDefGL('VertexAttribI4iv', (index, ptr) => {
  const rec = lkGLOrWarn(); if (rec) rec.gl.vertexAttribI4iv(index, new Int32Array(LK.memory.buffer, ptr, 4));
});
lkDefGL('VertexAttribI4uiv', (index, ptr) => {
  const rec = lkGLOrWarn(); if (rec) rec.gl.vertexAttribI4uiv(index, new Uint32Array(LK.memory.buffer, ptr, 4));
});
function lkWriteAttribValues(rec, index, pname, ptr, kind) {
  const a = lkAttribRecord(rec, index);
  if (pname === 0x889F) { lkI32()[ptr >> 2] = a.buffer; return; } // ARRAY_BUFFER_BINDING
  if (pname === 0x88A1) { lkI32()[ptr >> 2] = rec.cache.attribEnabled.get(rec.cache.vao + ':' + index) ? 1 : 0; return; }
  if (pname === 0x88A2) { lkI32()[ptr >> 2] = a.size; return; }
  if (pname === 0x88A3) { lkI32()[ptr >> 2] = a.stride; return; }
  if (pname === 0x88A4) { lkI32()[ptr >> 2] = a.type; return; }
  if (pname === 0x88A5) { lkI32()[ptr >> 2] = a.normalized ? 1 : 0; return; }
  if (pname === 0x88FE) { lkI32()[ptr >> 2] = a.divisor; return; }
  if (pname === 0x88FD) { lkI32()[ptr >> 2] = a.isInt ? 1 : 0; return; }
  const v = rec.gl.getVertexAttrib(index, pname);
  if (v === null || v === undefined) return;
  if (kind === 'f') { const f = lkF32(); if (typeof v === 'number') f[ptr >> 2] = v; else for (let i = 0; i < v.length; i++) f[(ptr >> 2) + i] = v[i]; }
  else { const i32 = lkI32(); if (typeof v === 'number') i32[ptr >> 2] = v | 0; else for (let i = 0; i < v.length; i++) i32[(ptr >> 2) + i] = v[i]; }
}
lkDefGL('GetVertexAttribfv', (index, pname, ptr) => { const rec = lkGLOrWarn(); if (rec) lkWriteAttribValues(rec, index, pname, ptr, 'f'); });
lkDefGL('GetVertexAttribiv', (index, pname, ptr) => { const rec = lkGLOrWarn(); if (rec) lkWriteAttribValues(rec, index, pname, ptr, 'i'); });
lkDefGL('GetVertexAttribIiv', (index, pname, ptr) => { const rec = lkGLOrWarn(); if (rec) lkWriteAttribValues(rec, index, pname, ptr, 'i'); });
lkDefGL('GetVertexAttribIuiv', (index, pname, ptr) => { const rec = lkGLOrWarn(); if (rec) lkWriteAttribValues(rec, index, pname, ptr, 'i'); });
lkDefGL('GetVertexAttribPointerv', (index, pname, ptrPtr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  lkU32()[ptrPtr >> 2] = lkAttribRecord(rec, index).offset;
});

// --- draws (never cached: they are the payload) ---
lkDefGL('DrawArrays', (mode, first, count) => { const rec = lkGLOrWarn(); if (rec) rec.gl.drawArrays(mode, first, count); });
lkDefGL('DrawElements', (mode, count, type, offset) => { const rec = lkGLOrWarn(); if (rec) rec.gl.drawElements(mode, count, type, offset); });
lkDefGL('DrawRangeElements', (mode, start, end, count, type, offset) => {
  const rec = lkGLOrWarn(); if (rec) rec.gl.drawRangeElements(mode, start, end, count, type, offset);
});
function lkDrawInstanced(name, method) {
  lkDefGL(name, (mode, first, count, prims) => { const rec = lkGLOrWarn(); if (rec) rec.gl[method](mode, first, count, prims); });
}
function lkDrawElementsInstanced(name) {
  lkDefGL(name, (mode, count, type, offset, prims) => {
    const rec = lkGLOrWarn(); if (rec) rec.gl.drawElementsInstanced(mode, count, type, offset, prims);
  });
}
lkDrawInstanced('DrawArraysInstanced', 'drawArraysInstanced');
lkDrawInstanced('DrawArraysInstancedANGLE', 'drawArraysInstanced');
lkDrawInstanced('DrawArraysInstancedEXT', 'drawArraysInstanced');
lkDrawInstanced('DrawArraysInstancedNV', 'drawArraysInstanced');
lkDrawInstanced('DrawArraysInstancedARB', 'drawArraysInstanced');
lkDrawElementsInstanced('DrawElementsInstanced');
lkDrawElementsInstanced('DrawElementsInstancedANGLE');
lkDrawElementsInstanced('DrawElementsInstancedEXT');
lkDrawElementsInstanced('DrawElementsInstancedNV');
lkDrawElementsInstanced('DrawElementsInstancedARB');

// --- queries / sync / transform feedback ---
function lkQueryBegin(name) {
  lkDefGL(name, (target, q) => { const rec = lkGLOrWarn(); if (rec) { const o = lkRegGet(rec, 'query', q); if (o) rec.gl.beginQuery(target, o); } });
}
function lkQueryEnd(name) {
  lkDefGL(name, (target) => { const rec = lkGLOrWarn(); if (rec) rec.gl.endQuery(target); });
}
lkQueryBegin('BeginQuery'); lkQueryBegin('BeginQueryEXT');
lkQueryEnd('EndQuery'); lkQueryEnd('EndQueryEXT');
lkDefGL('QueryCounterEXT', (q, target) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const o = lkRegGet(rec, 'query', q);
  if (o && rec.gl.beginQuery) { /* WebGL2 has no queryCounter; emulate via begin/end pair */
    try { rec.gl.beginQuery(target, o); rec.gl.endQuery(target); } catch (e) {}
  }
});
lkDefGL('GetQueryiv', (target, pname, ptr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const v = rec.gl.getQuery(target, pname);
  lkI32()[ptr >> 2] = v && typeof v === 'object' ? lkRegName(rec, 'query', v) : (v | 0);
});
lkDefGL('GetQueryivEXT', LK_GL['emscripten_glGetQueryiv']);
function lkQueryObject(name, kind) {
  lkDefGL(name, (q, pname, ptr) => {
    const rec = lkGLOrWarn(); if (!rec) return;
    const o = lkRegGet(rec, 'query', q); if (!o) return;
    const v = rec.gl.getQueryParameter(o, pname);
    if (kind === 'i64') lkI64()[ptr >> 3] = BigInt(v | 0);
    else if (kind === 'u64') lkU64()[ptr >> 3] = BigInt((v | 0) >>> 0);
    else if (kind === 'u') lkU32()[ptr >> 2] = (typeof v === 'boolean' ? (v ? 1 : 0) : v) >>> 0;
    else lkI32()[ptr >> 2] = typeof v === 'boolean' ? (v ? 1 : 0) : v | 0;
  });
}
lkQueryObject('GetQueryObjectiv', 'i');
lkQueryObject('GetQueryObjectuiv', 'u');
lkQueryObject('GetQueryObjectivEXT', 'i');
lkQueryObject('GetQueryObjectuivEXT', 'u');
lkQueryObject('GetQueryObjecti64vEXT', 'i64');
lkQueryObject('GetQueryObjectui64vEXT', 'u64');
lkDefGL('FenceSync', (condition, flags) => {
  const rec = lkGLOrWarn(); if (!rec) return 0;
  const obj = rec.gl.fenceSync(condition, flags);
  return obj ? lkRegAlloc(rec, 'sync', obj) : 0;
});
lkDefGL('IsSync', lkIs('sync'));
lkDefGL('DeleteSync', (name) => {
  const rec = lkGLOrWarn(); if (!rec || !name) return;
  const o = lkRegGet(rec, 'sync', name);
  if (o) rec.gl.deleteSync(o);
  lkRegFree(rec, 'sync', name);
});
lkDefGL('ClientWaitSync', (name, flags, timeout) => {
  const rec = lkGLOrWarn(); if (!rec) return 0x911B; // TIMEOUT_EXPIRED
  const o = lkRegGet(rec, 'sync', name); if (!o) return 0x911C; // WAIT_FAILED
  if (flags & 1) rec.gl.flush(); // SYNC_FLUSH_COMMANDS_BIT: implicit flush per spec
  const status = rec.gl.getSyncParameter(o, 0x9112 /* SYNC_STATUS */);
  return status === 0x9113 /* SIGNALED */ ? 0x911A /* ALREADY_SIGNALED */ : 0x911B;
});
lkDefGL('WaitSync', (name, flags, timeout) => {
  // Real GPU-side wait (WebGL2 has waitSync; its timeout must be 0 —
  // the wait is unbounded on the GPU timeline by design). Was a no-op
  // stub before the bug-fix run.
  const rec = lkGLOrWarn(); if (!rec) return;
  const o = lkRegGet(rec, 'sync', name); if (!o) return;
  if (rec.gl.waitSync) { try { rec.gl.waitSync(o, flags | 0, 0); } catch (e) {} }
});
lkDefGL('GetSynciv', (name, pname, bufSize, lenPtr, ptr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const o = lkRegGet(rec, 'sync', name); if (!o) return;
  const v = rec.gl.getSyncParameter(o, pname);
  lkI32()[ptr >> 2] = typeof v === 'boolean' ? (v ? 1 : 0) : v | 0;
  if (lenPtr) lkI32()[lenPtr >> 2] = 1;
});
lkDefGL('BeginTransformFeedback', (mode) => { const rec = lkGLOrWarn(); if (rec) rec.gl.beginTransformFeedback(mode); });
lkDefGL('EndTransformFeedback', () => { const rec = lkGLOrWarn(); if (rec) rec.gl.endTransformFeedback(); });
lkDefGL('PauseTransformFeedback', () => { const rec = lkGLOrWarn(); if (rec) rec.gl.pauseTransformFeedback(); });
lkDefGL('ResumeTransformFeedback', () => { const rec = lkGLOrWarn(); if (rec) rec.gl.resumeTransformFeedback(); });

// --- samplers ---
lkDefGL('SamplerParameteri', (s, pname, param) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const o = lkRegGet(rec, 'sampler', s); if (o) rec.gl.samplerParameteri(o, pname, param);
});
lkDefGL('SamplerParameterf', (s, pname, param) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const o = lkRegGet(rec, 'sampler', s); if (o) rec.gl.samplerParameterf(o, pname, param);
});
lkDefGL('SamplerParameteriv', (s, pname, ptr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const o = lkRegGet(rec, 'sampler', s); if (o) rec.gl.samplerParameteri(o, pname, lkI32()[ptr >> 2]);
});
lkDefGL('SamplerParameterfv', (s, pname, ptr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const o = lkRegGet(rec, 'sampler', s); if (o) rec.gl.samplerParameterf(o, pname, lkF32()[ptr >> 2]);
});
lkDefGL('GetSamplerParameteriv', (s, pname, ptr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const o = lkRegGet(rec, 'sampler', s); if (!o) return;
  const v = rec.gl.getSamplerParameter(o, pname);
  lkI32()[ptr >> 2] = typeof v === 'boolean' ? (v ? 1 : 0) : v | 0;
});
lkDefGL('GetSamplerParameterfv', (s, pname, ptr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const o = lkRegGet(rec, 'sampler', s); if (!o) return;
  const v = rec.gl.getSamplerParameter(o, pname);
  lkF32()[ptr >> 2] = typeof v === 'number' ? v : 0;
});

// --- generic parameter getters (ctx.getParameter family) ---
function lkParamToName(rec, v) {
  if (v === null || typeof v !== 'object') return v;
  for (const kind of ['buffer', 'texture', 'program', 'framebuffer', 'renderbuffer', 'vao', 'sampler', 'tfb']) {
    const n = lkRegName(rec, kind, v);
    if (n) return n;
  }
  return 0;
}
function lkWriteParam(rec, pname, ptr, kind) {
  const raw = rec.gl.getParameter(pname);
  const v = (raw && typeof raw === 'object' && !Array.isArray(raw) && !ArrayBuffer.isView(raw)) ? lkParamToName(rec, raw) : raw;
  if (v === null || v === undefined) return;
  if (kind === 'i64') {
    if (typeof v === 'number' || typeof v === 'boolean') lkI64()[ptr >> 3] = BigInt(v | 0);
    else if (v.length) lkI64()[ptr >> 3] = BigInt(v[0] | 0);
    return;
  }
  const writeOne = (at, x) => {
    if (kind === 'f') lkF32()[at >> 2] = x;
    else if (kind === 'b') lkU8()[at] = x ? 1 : 0;
    else lkI32()[at >> 2] = typeof x === 'boolean' ? (x ? 1 : 0) : x | 0;
  };
  if (typeof v === 'number' || typeof v === 'boolean') writeOne(ptr, v);
  else if (v.length !== undefined) for (let i = 0; i < v.length; i++) writeOne(ptr + i * (kind === 'b' ? 1 : 4), v[i]);
}
lkDefGL('GetIntegerv', (pname, ptr) => { const rec = lkGLOrWarn(); if (rec) lkWriteParam(rec, pname, ptr, 'i'); });
lkDefGL('GetInteger64v', (pname, ptr) => { const rec = lkGLOrWarn(); if (rec) lkWriteParam(rec, pname, ptr, 'i64'); });
lkDefGL('GetFloatv', (pname, ptr) => { const rec = lkGLOrWarn(); if (rec) lkWriteParam(rec, pname, ptr, 'f'); });
lkDefGL('GetBooleanv', (pname, ptr) => { const rec = lkGLOrWarn(); if (rec) lkWriteParam(rec, pname, ptr, 'b'); });
lkDefGL('GetIntegeri_v', (target, index, ptr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const v = rec.gl.getIndexedParameter(target, index);
  lkI32()[ptr >> 2] = (v && typeof v === 'object') ? lkParamToName(rec, v) : (typeof v === 'boolean' ? (v ? 1 : 0) : v | 0);
});
lkDefGL('GetInteger64i_v', (target, index, ptr) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const v = rec.gl.getIndexedParameter(target, index);
  lkI64()[ptr >> 3] = BigInt(((v && typeof v === 'object') ? lkParamToName(rec, v) : (v | 0)) | 0);
});
lkDefGL('GetString', (name) => {
  const rec = lkGLOrWarn(); if (!rec) return 0;
  const gl = rec.gl;
  let s = '';
  if (name === 0x1F00) s = String(gl.getParameter(0x1F00) || 'WebGL2'); // VENDOR: the real one (L5)
  else if (name === 0x1F01) s = String(gl.getParameter(0x1F01) || 'WebGL2');
  else if (name === 0x1F02) s = String(gl.getParameter(0x1F02) || 'WebGL 2.0');
  else if (name === 0x8B8C) s = String(gl.getParameter(0x8B8C) || 'WebGL GLSL ES 3.00');
  else if (name === 0x1F03) s = (gl.getSupportedExtensions() || []).join(' ');
  else return 0;
  return lkStaticStr(s);
});
lkDefGL('GetStringi', (name, index) => {
  const rec = lkGLOrWarn(); if (!rec) return 0;
  if (name === 0x1F03) {
    const exts = rec.gl.getSupportedExtensions() || [];
    if (index < 0 || index >= exts.length) return 0;
    return lkStaticStr(exts[index]);
  }
  return 0;
});

// --- clear-buffer + extension oddments ---
lkDefGL('ClearBufferfv', (buffer, drawbuffer, ptr) => {
  const rec = lkGLOrWarn(); if (rec) rec.gl.clearBufferfv(buffer, drawbuffer, new Float32Array(LK.memory.buffer, ptr, 4));
});
lkDefGL('ClearBufferiv', (buffer, drawbuffer, ptr) => {
  const rec = lkGLOrWarn(); if (rec) rec.gl.clearBufferiv(buffer, drawbuffer, new Int32Array(LK.memory.buffer, ptr, 4));
});
lkDefGL('ClearBufferuiv', (buffer, drawbuffer, ptr) => {
  const rec = lkGLOrWarn(); if (rec) rec.gl.clearBufferuiv(buffer, drawbuffer, new Uint32Array(LK.memory.buffer, ptr, 4));
});
lkDefGL('ClearBufferfi', (buffer, drawbuffer, depth, stencil) => {
  const rec = lkGLOrWarn(); if (rec) rec.gl.clearBufferfi(buffer, drawbuffer, depth, stencil);
});
function lkExtObject(rec, name) {
  if (rec.exts.has(name)) return rec.exts.get(name);
  const ext = rec.gl.getExtension(name);
  rec.exts.set(name, ext || null);
  return ext;
}
lkDefGL('PolygonOffsetClampEXT', (factor, units, clamp) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const ext = lkExtObject(rec, 'EXT_polygon_offset_clamp');
  if (ext && ext.polygonOffsetClampEXT) ext.polygonOffsetClampEXT(factor, units, clamp);
});
lkDefGL('ClipControlEXT', (origin, depth) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const ext = lkExtObject(rec, 'EXT_clip_control');
  if (ext && ext.clipControlEXT) ext.clipControlEXT(origin, depth);
});
lkDefGL('PolygonModeWEBGL', (face, mode) => {
  const rec = lkGLOrWarn(); if (!rec) return;
  const ext = lkExtObject(rec, 'WEBGL_polygon_mode');
  if (ext && ext.polygonModeWEBGL) ext.polygonModeWEBGL(face, mode);
});

// --- lkio_present: our thin present verb, on the real path at last ---
// Contract (from gl33/lk_webio.js, our own spec): target 0; tex/prog/vao
// are GL names created through the normal path; upload the texels and
// draw the fullscreen triangle in ONE host crossing. Returns 0 on
// success, the GL error code if the upload failed (nothing drawn), or
// -1 when the verb cannot run (caller falls back to the classic path).
function lkioPresent(target, tex, prog, vao, namePtr, ptr, w, h, fbW, fbH, internalFormat, format, fresh) {
  if (target !== 0) return -1;
  const rec = lkCtx();
  if (!rec) return -1;
  try {
    const gl = rec.gl;
    const texObj = lkRegGet(rec, 'texture', tex);
    const progObj = lkRegGet(rec, 'program', prog);
    const vaoObj = lkRegGet(rec, 'vao', vao);
    if (!texObj || !progObj || !vaoObj) return -1;
    LK_GL['emscripten_glViewport'](0, 0, fbW, fbH);
    LK_GL['emscripten_glDisable'](0x0B71); // DEPTH_TEST
    LK_GL['emscripten_glDisable'](0x0BE2); // BLEND
    LK_GL['emscripten_glActiveTexture'](0x84C0); // TEXTURE0
    LK_GL['emscripten_glBindTexture'](0x0DE1, tex); // TEXTURE_2D
    // L6: clamp the upload view to what actually remains in memory —
    // the unclamped constructor throws RangeError on a short buffer,
    // which used to surface here as an unexplained -1.
    const want = w * h * 4;
    const avail = Math.max(0, LK.memory.buffer.byteLength - ptr);
    const data = new Uint8Array(LK.memory.buffer, ptr, Math.min(want, avail));
    if (data.length < want && !lkioPresent.clampWarned) {
      lkioPresent.clampWarned = true;
      lkErr('lkboot: lkio_present: texel buffer short (' + data.length + ' of ' + want + ' bytes); upload clamped');
    }
    if (fresh) gl.texImage2D(0x0DE1, 0, internalFormat, w, h, 0, format, 0x1401, data);
    else gl.texSubImage2D(0x0DE1, 0, 0, 0, w, h, format, 0x1401, data);
    const err = gl.getError();
    if (err !== 0) return err;
    LK_GL['emscripten_glUseProgram'](prog);
    // Cache the sampler location per (program, name): this ran a
    // getUniformLocation on EVERY present (L6).
    if (!rec.presentLocs) rec.presentLocs = new Map();
    const locKey = prog + ':' + namePtr;
    let loc = rec.presentLocs.get(locKey);
    if (loc === undefined) {
      loc = gl.getUniformLocation(progObj, lkReadCStr(namePtr));
      rec.presentLocs.set(locKey, loc);
    }
    if (loc) gl.uniform1i(loc, 0);
    LK_GL['emscripten_glBindVertexArray'](vao);
    gl.drawArrays(0x0004, 0, 3); // TRIANGLES
    LK_GL['emscripten_glBindVertexArray'](0);
    LK_GL['emscripten_glUseProgram'](0);
    LK_GL['emscripten_glBindTexture'](0x0DE1, 0);
    return 0;
  } catch (e) {
    return -1;
  }
}

/* ================= GL command stream (v2 topology) ================== */
// Why this exists: the engine's threads — including the one running
// main() — block and rendezvous freely, so they cannot live on the
// browser main thread (a blocked main thread can neither service the
// mailbox nor present). They live on workers. WebGL lives here, on the
// page. So worker-side GL imports serialize into a shared-memory ring;
// the page executes them in order against the real context with the
// state-caching wrapper above (LK_GL). The canvas is never transferred
// anywhere — the compositor presents it directly.
//
// Ring layout (offsets from LK.glRing.base, all u32 LE):
//   +0  claimIdx  (monotonic byte count; CAS ticket dispenser, H2)
//   +4  readIdx   (monotonic byte count, executor-owned)
//   +8  syncLock  (0/1, Atomics CAS)
//   +12 syncDoneSeq
//   +16 syncTag (0 = number in f64 slot, 1 = BigInt in lo/hi)
//   +20 syncSeq counter (sync callers, under syncLock)
//   +24 syncValue f64
//   +32 syncBigLo u32, +36 syncBigHi u32
//   +40 commitIdx (monotonic byte count; writers publish records in
//       claim order by advancing this word — the executor NEVER reads
//       past it, so a half-written record is never visible, H2)
//   +64 data region (LK.glRing.size bytes, a multiple of 16)
// Record (P1 packed format, bug-fix run): 8-byte header
//   [funcIdx u32][packed u32: flags:8 (bit0 = sync) | slotCount:8 |
//   totalBytes:16]
// then slotCount slots of 8 bytes: [tag u32][value u32]. Tags:
//   0 = the argument itself (a JS integer: pointers, names, enums —
//       the bulk of GL traffic; decoded back with |0, exactly the
//       number the wasm import received)
//   1 = BigInt low half; the NEXT slot is the [3][high half] pair
//   2 = f64 low half;   the NEXT slot is the [3][high half] pair
//   4 = sync sequence number (leading slot of sync records)
// A record with funcIdx = 0xFFFFFFFE is wrap padding: its second word
// is the pad length in bytes and the next record starts at offset 0.
// Typical draw record: 32 B (was 64 B in the 16 B-slot format).
const LK_GL_HDR = 64;
const LK_GL_WRAP = 0xFFFFFFFE;
const LK_GL_NAMES = Object.keys(LK_GL); // identical order in every context (same file)
const lkF64Scratch = new Float64Array(1);
const lkF64ScratchU32 = new Uint32Array(lkF64Scratch.buffer);
// Reserved pseudo-functions after the LK_GL table:
const LK_GLRES_CTX_CREATE = LK_GL_NAMES.length + 0;   // (target, attrs) -> handle (sync)
const LK_GLRES_CTX_MAKE_CURRENT = LK_GL_NAMES.length + 1; // (handle) -> status (sync)
const LK_GLRES_CTX_GET_CURRENT = LK_GL_NAMES.length + 2;  // () -> handle (sync)
const LK_GLRES_CTX_DESTROY = LK_GL_NAMES.length + 3;      // (handle) (async)
const LK_GLRES_CTX_ENABLE_EXT = LK_GL_NAMES.length + 4;   // (handle, namePtr) -> bool (sync)
const LK_GLRES_LKIO_PRESENT = LK_GL_NAMES.length + 5;     // lkio_present args (sync)

// --- H1: the sync/async classification -------------------------------
// A call must execute BEFORE its caller continues if it returns a
// value, writes caller memory the engine reads next, or READS pointer
// data the engine may reuse/free as soon as the call returns. The old
// classifier was one name regex; it missed 11 pointer-reading imports
// (8 Delete* that walk a name array, VertexAttribI4iv/uiv, and
// BindAttribLocation, which reads a C string), and those streamed
// async against possibly-reused memory — silent corruption class.
//
// The fix is structural, not a longer regex: pointer-dereferencing
// imports are named EXPLICITLY in LK_GL_POINTER_READERS (derived from
// the C signatures — a name belongs here iff its implementation reads
// or writes caller memory through a pointer argument). The regex
// remains only for the value-returning families. Harnesses cross-
// check this set against the shipped binary's import census
// (wasm/harness/probeglclass.js), and lkGLClassAudit() re-checks it
// at every boot: a classification gap is a loud boot log line, never
// a silent default.
const LK_GL_POINTER_READERS = new Set([
  // delete-by-array (H1's eight) + VAO deletes
  'DeleteBuffers', 'DeleteTextures', 'DeleteFramebuffers', 'DeleteRenderbuffers',
  'DeleteQueries', 'DeleteQueriesEXT', 'DeleteSamplers', 'DeleteTransformFeedbacks',
  'DeleteVertexArrays', 'DeleteVertexArraysOES',
  // integer attribs + string-reading bind (H1's remaining three)
  'VertexAttribI4iv', 'VertexAttribI4uiv', 'BindAttribLocation',
  // name-array writers (Gen*)
  'GenBuffers', 'GenTextures', 'GenFramebuffers', 'GenRenderbuffers',
  'GenVertexArrays', 'GenVertexArraysOES', 'GenQueries', 'GenQueriesEXT',
  'GenSamplers', 'GenTransformFeedbacks',
  // result writers (Get* family: values land in caller memory)
  'GetBooleanv', 'GetIntegerv', 'GetInteger64v', 'GetFloatv',
  'GetBufferParameteriv', 'GetBufferParameteri64v',
  'GetTexParameterfv', 'GetTexParameteriv', 'ReadPixels',
  'GetFramebufferAttachmentParameteriv', 'GetRenderbufferParameteriv',
  'GetInternalformativ', 'GetShaderiv', 'GetShaderInfoLog', 'GetShaderSource',
  'GetProgramiv', 'GetProgramInfoLog', 'GetAttachedShaders',
  'GetActiveAttrib', 'GetActiveUniform', 'GetActiveUniformsiv',
  'GetUniformfv', 'GetUniformiv', 'GetUniformuiv', 'GetUniformIndices',
  'GetActiveUniformBlockiv', 'GetActiveUniformBlockName',
  'GetQueryiv', 'GetQueryivEXT', 'GetQueryObjectiv', 'GetQueryObjectuiv',
  'GetQueryObjectivEXT', 'GetQueryObjectuivEXT', 'GetQueryObjecti64vEXT',
  'GetQueryObjectui64vEXT', 'GetSynciv', 'GetSamplerParameteriv',
  'GetSamplerParameterfv', 'GetVertexAttribfv', 'GetVertexAttribiv',
  'GetVertexAttribIiv', 'GetVertexAttribIuiv', 'GetVertexAttribPointerv',
  'GetIntegeri_v', 'GetInteger64i_v', 'GetTransformFeedbackVarying',
  'GetProgramBinary', 'GetFragDataLocation',
  // pointer readers (uploads, arrays, strings, varyings)
  'BufferData', 'BufferSubData', 'ShaderSource',
  'TexImage2D', 'TexImage3D', 'TexSubImage2D', 'TexSubImage3D',
  'CompressedTexImage2D', 'CompressedTexImage3D',
  'CompressedTexSubImage2D', 'CompressedTexSubImage3D',
  'TexParameterfv', 'TexParameteriv', 'SamplerParameterfv', 'SamplerParameteriv',
  'Uniform1fv', 'Uniform2fv', 'Uniform3fv', 'Uniform4fv',
  'Uniform1iv', 'Uniform2iv', 'Uniform3iv', 'Uniform4iv',
  'Uniform1uiv', 'Uniform2uiv', 'Uniform3uiv', 'Uniform4uiv',
  'UniformMatrix2fv', 'UniformMatrix3fv', 'UniformMatrix4fv',
  'UniformMatrix2x3fv', 'UniformMatrix3x2fv', 'UniformMatrix2x4fv',
  'UniformMatrix4x2fv', 'UniformMatrix3x4fv', 'UniformMatrix4x3fv',
  'VertexAttrib1fv', 'VertexAttrib2fv', 'VertexAttrib3fv', 'VertexAttrib4fv',
  'ClearBufferfv', 'ClearBufferiv', 'ClearBufferuiv',
  'DrawBuffers', 'DrawBuffersEXT', 'DrawBuffersWEBGL',
  'InvalidateFramebuffer', 'InvalidateSubFramebuffer',
  'TransformFeedbackVaryings', 'ProgramBinary',
].map((s) => 'emscripten_gl' + s));
// Value-returning families (the result travels back through the sync
// reply slot; pointer semantics are covered by the set above).
const LK_GL_SYNC_RE = /^(emscripten_gl)(Create|Gen|Get|Is|Check|Fence|ClientWait|ReadPixels)/;
function lkGLIsSync(name) {
  return LK_GL_POINTER_READERS.has(name) || LK_GL_SYNC_RE.test(name);
}
// Boot-time audit (page): every GL import the binary actually carries
// must have an implementation and an EXPLICIT classification story;
// every pointer-reader entry must name a real implementation. Any gap
// is logged loudly here (and fails the harness probe), rather than
// defaulting silently in production.
function lkGLClassAudit() {
  try {
    const stale = [...LK_GL_POINTER_READERS].filter((n) => !(n in LK_GL));
    if (stale.length) lkErr('lkboot: GL class audit: pointer-reader set names unimplemented calls: ' + stale.join(', '));
    if (!LK.module) return;
    const imported = WebAssembly.Module.imports(LK.module)
      .map((i) => i.name).filter((n) => n.startsWith('emscripten_gl'));
    const unimplemented = imported.filter((n) => !(n in LK_GL));
    if (unimplemented.length) lkErr('lkboot: GL class audit: binary imports GL calls with no implementation: ' + unimplemented.join(', '));
    const syncCount = imported.filter((n) => lkGLIsSync(n)).length;
    lkOut('lkboot: GL class audit: ' + imported.length + ' GL imports, ' +
          syncCount + ' sync-classified, ' + (imported.length - syncCount) +
          ' async, ' + LK_GL_POINTER_READERS.size + ' explicit pointer-readers');
  } catch (e) {
    lkErr('lkboot: GL class audit failed: ' + (e && e.message || e));
  }
}

let lkGLStubCache = null;
function lkGLStubs() {
  if (lkGLStubCache) return lkGLStubCache;
  const stubs = {};
  LK_GL_NAMES.forEach((name, idx) => {
    const sync = lkGLIsSync(name);
    stubs[name] = (...args) => lkGLStreamCall(idx, sync, args);
  });
  lkGLStubCache = stubs;
  return stubs;
}

function lkGLStreamCall(funcIdx, sync, args) {
  const ring = LK.glRing;
  if (!ring) return 0;
  const i32 = lkI32(), u32 = lkU32(), f64 = lkF64();
  const HB = ring.base >> 2;
  if (sync) {
    // Serialize sync calls through one lock: they are rare next to the
    // async stream, and a single reply slot keeps the protocol simple.
    while (Atomics.compareExchange(i32, HB + 2, 0, 1) !== 0) {
      Atomics.wait(i32, HB + 2, 1, 1);
    }
  }
  let mySeq = 0;
  try {
    // Encode first (P1 packed slots, see the layout comment): a flat
    // u32 array, two words per slot. Sync records lead with [4][seq],
    // the seq patched in once claimed (under the sync lock).
    const slots = [];
    if (sync) slots.push(4, 0);
    for (const a of args) {
      if (typeof a === 'bigint') {
        slots.push(1, Number(a & 0xffffffffn), 3, Number((a >> 32n) & 0xffffffffn));
      } else if (Number.isInteger(a) && a >= -2147483648 && a < 4294967296) {
        slots.push(0, a >>> 0);
      } else {
        lkF64Scratch[0] = a;
        slots.push(2, lkF64ScratchU32[0], 3, lkF64ScratchU32[1]);
      }
    }
    const slotCount = slots.length / 2;
    if (slotCount > 255) throw new Error('lkboot: GL record too wide: ' + slotCount + ' slots');
    const recBytes = 8 + slots.length * 4;
    const dataSize = ring.size;
    // H2: claim space with a CAS ticket. The old protocol loaded
    // writeIdx, wrote the record, and stored writeIdx back — two
    // producers could claim the same bytes and overwrite each other
    // mid-record (proven by harness/probeh2.js against the old code:
    // thousands of torn records). The claim is now atomic; the record
    // is then PUBLISHED in claim order through commitIdx (HB+10),
    // which the executor never reads past.
    let ticket = 0, need = 0, wasEmpty = false;
    for (;;) {
      const w = Atomics.load(u32, HB + 0);
      const r = Atomics.load(u32, HB + 1);
      const off = w % dataSize;
      need = recBytes + (off + recBytes > dataSize ? dataSize - off : 0);
      if (w - r + need <= dataSize) {
        if (Atomics.compareExchange(u32, HB + 0, w, w + need) === w) {
          ticket = w; wasEmpty = (r === w); break;
        }
        continue; // lost the claim race; recompute against the new head
      }
      // Ring full: wait for the drain to advance readIdx.
      const rNow = Atomics.load(u32, HB + 1);
      self.postMessage({ cmd: 'lk-gl' });
      Atomics.wait(i32, HB + 1, rNow, 2);
    }
    let at = ticket;
    const off0 = ticket % dataSize;
    if (off0 + recBytes > dataSize) {
      const pad = dataSize - off0;
      const p = (ring.base + LK_GL_HDR + off0) >> 2;
      u32[p + 0] = LK_GL_WRAP; u32[p + 1] = pad;
      at += pad;
    }
    const recOff = at % dataSize;
    const p = (ring.base + LK_GL_HDR + recOff) >> 2;
    if (sync) { mySeq = Atomics.add(u32, HB + 5, 1) + 1; slots[1] = mySeq >>> 0; }
    u32[p + 0] = funcIdx;
    u32[p + 1] = (sync ? 1 : 0) | (slotCount << 8) | (recBytes << 16);
    let s = p + 2;
    for (let i = 0; i < slots.length; i++) u32[s++] = slots[i];
    // Publish in claim order: earlier tickets commit first. Every
    // claimed writer reaches this point without needing ring space or
    // the sync lock, so the wait always terminates.
    while (Atomics.load(u32, HB + 10) !== ticket) {
      Atomics.wait(i32, HB + 10, Atomics.load(u32, HB + 10), 1);
    }
    Atomics.store(u32, HB + 10, ticket + need);
    Atomics.notify(i32, HB + 10);
    // Wake the page: always for sync (it waits on us); for async,
    // when the ring was empty before this write (steady-state frames
    // self-wake via the rAF drain + frame-done messages).
    if (sync || wasEmpty) self.postMessage({ cmd: 'lk-gl' });
    if (!sync) return 0;
    // Wait for the executor's reply.
    for (;;) {
      const done = Atomics.load(u32, HB + 3);
      if (done === mySeq) break;
      Atomics.wait(i32, HB + 3, done, 5);
    }
    const tag = Atomics.load(u32, HB + 4);
    if (tag === 1) {
      const lo = Atomics.load(u32, HB + 8), hi = Atomics.load(u32, HB + 9);
      return (BigInt(hi) << 32n) | BigInt(lo);
    }
    return f64[(ring.base + 24) >> 3];
  } finally {
    if (sync) { Atomics.store(i32, HB + 2, 0); Atomics.notify(i32, HB + 2); }
  }
}

// Executor side (page): perform one decoded record; returns its value
// for sync records.
function lkGLExecute(funcIdx, args) {
  lkB81MarkDirty(funcIdx); // build 81: note pixel work against the surface
  if (funcIdx === LK_GLRES_CTX_CREATE) {
    const canvas = lkFindCanvas(args[0]);
    return canvas ? lkWebGLCreateContext(canvas, args[1]) : 0;
  }
  if (funcIdx === LK_GLRES_CTX_MAKE_CURRENT) return lkGLMakeCurrent(args[0]);
  if (funcIdx === LK_GLRES_CTX_GET_CURRENT) {
    const rec = lkCtx();
    return rec ? rec.handle : 0;
  }
  if (funcIdx === LK_GLRES_CTX_DESTROY) { lkGLDestroyContext(args[0]); return 0; }
  if (funcIdx === LK_GLRES_CTX_ENABLE_EXT) {
    const rec = lkCtx(); if (!rec) return 0;
    let name = lkReadCStr(args[1]);
    if (name.startsWith('GL_')) name = name.slice(3);
    try { return rec.gl.getExtension(name) ? 1 : 0; } catch (e) { return 0; }
  }
  if (funcIdx === LK_GLRES_LKIO_PRESENT) return lkioPresent(...args);
  const fn = LK_GL[LK_GL_NAMES[funcIdx]];
  if (!fn) return 0;
  return fn(...args);
}

function lkGLDrain() {
  if (LK.isWorker) return;
  const ring = LK.glRing;
  if (!ring || !LK.exports) return;
  const lkB81Gen0 = lkB81DirtyGen; // build 81: present out-of-frame work at drain end
  const u32 = lkU32(), i32 = lkI32(), f64 = lkF64();
  const HB = ring.base >> 2;
  const dataSize = ring.size;
  let r = Atomics.load(u32, HB + 1);
  const w = Atomics.load(u32, HB + 10); // commitIdx: never read past it (H2)
  let guard = 0;
  while (r !== w && guard++ < 200000) {
    const off = r % dataSize;
    const p = (ring.base + LK_GL_HDR + off) >> 2;
    const funcIdx = u32[p + 0];
    if (funcIdx === LK_GL_WRAP) {
      const pad = u32[p + 1];
      Atomics.store(u32, HB + 1, r + pad);
      Atomics.notify(i32, HB + 1);
      r += pad;
      continue;
    }
    const packed = u32[p + 1];
    const flags = packed & 0xff;
    const slotCount = (packed >> 8) & 0xff;
    const total = packed >>> 16;
    if (total === 0 || total > dataSize) {
      // Unreachable under the commit protocol (only fully-written
      // records are ever committed); if it ever fires, the failure is
      // LOUD and the drain halts rather than executing garbage.
      lkErr('lkboot: GL ring corrupt record at byte ' + r + ' (totalBytes=' + total + '); drain halted');
      break;
    }
    const args = [];
    let s = p + 2;
    let seq = 0;
    for (let i = 0; i < slotCount; i++) {
      const tag = u32[s];
      if (tag === 4) { seq = u32[s + 1]; s += 2; }
      else if (tag === 1) {
        const v = (BigInt(u32[s + 3]) << 32n) | BigInt(u32[s + 1]);
        args.push(v >= 0x8000000000000000n ? v - 0x10000000000000000n : v); // back to signed i64
        s += 4;
      }
      else if (tag === 2) {
        lkF64ScratchU32[0] = u32[s + 1]; lkF64ScratchU32[1] = u32[s + 3];
        args.push(lkF64Scratch[0]); s += 4;
      } else { args.push(u32[s + 1] | 0); s += 2; }
    }
    // Advance readIdx BEFORE executing publishes space early; sync
    // completion is signaled through syncDoneSeq, not readIdx.
    const next = r + total;
    Atomics.store(u32, HB + 1, next);
    Atomics.notify(i32, HB + 1);
    r = next;
    const isSync = (flags & 1) !== 0;
    if (!isSync && LK.frameOutstanding) {
      // Build 80: frame-boundary atomicity — hold this frame's async
      // records until lk-frame-done (see lkGLFlushPending). readIdx
      // has already advanced, so producers never block on ring space
      // while the frame is in flight; the decoded args are plain
      // values (every pointer-reading call is sync-classified, H1),
      // so buffering them cannot dangle.
      lkPendingGL.push([funcIdx, args]);
      continue;
    }
    let result = 0;
    if (isSync && lkPendingGL.length) lkGLFlushPending();  // stream order at sync points
    try { result = lkGLExecute(funcIdx, args); } catch (e) {
      lkErr('lkboot: GL stream call ' + (LK_GL_NAMES[funcIdx] || funcIdx) + ' threw: ' + (e && e.message || e));
    }
    if (flags & 1) {
      // Reply: value first, then doneSeq (release order via Atomics).
      if (typeof result === 'bigint') {
        Atomics.store(u32, HB + 4, 1);
        Atomics.store(u32, HB + 8, Number(result & 0xffffffffn));
        Atomics.store(u32, HB + 9, Number((result >> 32n) & 0xffffffffn));
      } else {
        Atomics.store(u32, HB + 4, 0);
        f64[(ring.base + 24) >> 3] = typeof result === 'number' ? result : 0;
      }
      // Reply with the writer's own seq (carried in the record's
      // leading slot): value first, then the done word (release).
      Atomics.store(u32, HB + 3, seq);
      Atomics.notify(i32, HB + 3);
    }
  }
  Atomics.store(u32, HB + 1, r);
  // Build 81: work executed while NO frame was outstanding (setup,
  // menus outside the frame protocol, stragglers after frame-done)
  // is presented here, once, at the end of the drain that executed
  // it — the build-80 on-arrival visibility for out-of-frame work,
  // without ever exposing an in-frame prefix: while a frame is
  // outstanding this never fires, and only lk-frame-done presents.
  if (!LK.frameOutstanding && lkB81DirtyGen !== lkB81Gen0) lkB81PresentFrame();
}
let lkGLExecSeq = 0;

// Build 80: async GL records of the in-flight engine frame, decoded
// and held by lkGLDrain until the frame completes (see the page
// driver comment). Executed in stream order by lkGLFlushPending.
let lkPendingGL = [];
function lkGLFlushPending() {
  if (!lkPendingGL.length) return;
  const pend = lkPendingGL;
  lkPendingGL = [];
  for (const [funcIdx, args] of pend) {
    try { lkGLExecute(funcIdx, args); } catch (e) {
      lkErr('lkboot: GL stream call ' + (LK_GL_NAMES[funcIdx] || funcIdx) + ' threw: ' + (e && e.message || e));
    }
  }
}

/* ===================== WebGL context creation ======================== */

const LK_POWER_PREF = ['default', 'low-power', 'high-performance'];
function lkWebGLCreateContext(canvas, attrsPtr) {
  const st = lkGLState();
  const u8 = lkU8(), i32 = lkI32();
  const attrs = {
    alpha: !!u8[attrsPtr + 0],
    depth: !!u8[attrsPtr + 1],
    stencil: !!u8[attrsPtr + 2],
    antialias: !!u8[attrsPtr + 3],
    premultipliedAlpha: !!u8[attrsPtr + 4],
    preserveDrawingBuffer: !!u8[attrsPtr + 5],
    powerPreference: LK_POWER_PREF[i32[(attrsPtr + 8) >> 2]] || 'default',
    failIfMajorPerformanceCaveat: !!u8[attrsPtr + 12],
    majorVersion: i32[(attrsPtr + 16) >> 2],
    minorVersion: i32[(attrsPtr + 20) >> 2],
    enableExtensionsByDefault: !!u8[attrsPtr + 24],
    explicitSwapControl: !!u8[attrsPtr + 25],
    proxyContextToMainThread: i32[(attrsPtr + 28) >> 2],
    renderViaOffscreenBackBuffer: !!u8[attrsPtr + 32],
    desynchronized: !!u8[attrsPtr + 33],
  };
  if (attrs.explicitSwapControl) return 0; // never requested a manual swap mode
  let gl = null;
  try {
    gl = canvas.getContext('webgl2', {
      alpha: attrs.alpha, depth: attrs.depth, stencil: attrs.stencil,
      antialias: attrs.antialias, premultipliedAlpha: attrs.premultipliedAlpha,
      preserveDrawingBuffer: attrs.preserveDrawingBuffer,
      powerPreference: attrs.powerPreference,
      failIfMajorPerformanceCaveat: attrs.failIfMajorPerformanceCaveat,
      desynchronized: attrs.desynchronized,
    });
  } catch (e) { gl = null; }
  if (!gl) return 0;
  const handle = st.nextHandle++;
  const rec = lkNewContextRecord(handle, gl, canvas);
  st.contexts.set(handle, rec);
  // M6: creating a context must NOT make it current (GL semantics;
  // Emscripten's contract too). Callers use make-current explicitly —
  // the old set-on-create hid a whole class of wrong-context bugs.
  return handle;
}

function lkGLMakeCurrent(handle) {
  const st = lkGLState();
  const rec = st.contexts.get(handle);
  if (!rec) return -4; // INVALID_OPERATION-ish
  st.current = rec;
  // Build 81: on first make-current, the double-buffer surface
  // becomes this context's physical default framebuffer (bindings
  // are per-context state, so this is done once per record).
  const b = lkB81Ensure(rec);
  if (b && b.fbo && !b.defaultBound) {
    rec.gl.bindFramebuffer(0x8D40, b.fbo); // FRAMEBUFFER
    b.defaultBound = true;
  }
  return 0;
}
function lkGLDestroyContext(handle) {
  const st = lkGLState();
  const rec = st.contexts.get(handle);
  if (rec) {
    // Build 81: free the double-buffer surface with its context.
    if (rec.b81) {
      const gl = rec.gl, b = rec.b81;
      if (b.fbo) gl.deleteFramebuffer(b.fbo);
      if (b.colorTex) gl.deleteTexture(b.colorTex);
      if (b.depthRb) gl.deleteRenderbuffer(b.depthRb);
      rec.b81 = null;
    }
    const ext = rec.gl.getExtension('WEBGL_lose_context');
    if (ext) ext.loseContext();
    if (st.current === rec) st.current = null;
    st.contexts.delete(handle);
  }
}

/* ================= Build 81: wrapper double-buffer ================== */
// Option C for Finding 1 (the convicted build-80 flicker mechanism).
// Build 80's gate executes a frame's records whole at
// lk-frame-done, but SYNC records execute in place mid-frame — each
// first flushes the pending prefix, after which the compositor is
// free to paint that partial state (measured on the bench: 1,502
// sync flushes vs 14 frame-done in one session; 21.6% of the sync
// flushes left the canvas mostly sky). The gate orders execution;
// it cannot un-paint what execution exposed.
//
// The fix is a surface change, not a scheduling change: the stream
// renders into a wrapper-owned offscreen framebuffer (the
// "surface": colour texture + depth/stencil renderbuffer, sized to
// the canvas), and the completed frame reaches the canvas in ONE
// blit at lk-frame-done — the same single-operation present shape
// lkio_present already proves for the software path. Sync flushes
// now execute against the surface, which the compositor never
// paints, so no execution order can expose a partial frame. Stream
// order, sync semantics and record classification are untouched —
// no record is reclassified, reordered or split (option B's prefix
// tricks stay rejected).
//
// Routing model: an engine bind of framebuffer name 0 (the default
// framebuffer) is translated to the surface; engine-created
// framebuffers bind exactly as before. The wrapper caches keep
// holding ENGINE names, so binding queries stay truthful by
// construction: getParameter(FRAMEBUFFER_BINDING) resolves the
// physically bound object through lkParamToName, and the surface —
// present in no engine registry — reads back as name 0, the name
// the engine bound. Before the engine's first bind, make-current
// binds the surface physically, so the surface IS this context's
// default framebuffer from the wrapper's point of view. Resizes
// reallocate the attachments in place (same objects, new storage),
// via lkSetCanvasSize and a size check on the present path.
//
// The present is a blitFramebuffer, NOT a fullscreen draw: a draw
// would disturb the program/VAO/uniform state the wrapper caches,
// while a blit touches none of it. The physical state a blit DOES
// honour — framebuffer bindings, scissor test, colour mask — is
// restored around the copy (bindings re-applied from the routed
// cache; scissor and colour mask forced open for the copy and put
// back from the cache). lkio_present is byte-untouched: it still
// draws in one host crossing to whatever is bound — the surface,
// when the engine's binding routes there — and the frame's blit
// presents the result in the same frame cycle.
//
// Dirty tracking: pixel-writing records (clears, draws, blits,
// tex-image copies, and the present verb) executed while the DRAW
// binding routes to the surface mark it dirty. lk-frame-done
// presents a dirty surface exactly once; a drain that executed
// pixel work while no frame was outstanding presents at drain end
// (build 80's on-arrival visibility for out-of-frame work). Known
// cost: one extra fullscreen copy per presented frame — a fill
// cost to be measured on device, not on the bench.
const LK_B81_PIXEL_NAMES = [
  'Clear', 'ClearBufferfi', 'ClearBufferfv', 'ClearBufferiv', 'ClearBufferuiv',
  'DrawArrays', 'DrawElements', 'DrawRangeElements',
  'DrawArraysInstanced', 'DrawArraysInstancedANGLE', 'DrawArraysInstancedEXT',
  'DrawArraysInstancedNV', 'DrawArraysInstancedARB',
  'DrawElementsInstanced', 'DrawElementsInstancedANGLE',
  'DrawElementsInstancedEXT', 'DrawElementsInstancedNV',
  'DrawElementsInstancedARB',
  'BlitFramebuffer', 'CopyTexImage2D', 'CopyTexSubImage2D',
  'CopyTexSubImage3D',
];
const LK_B81_PIXEL_FUNCS = new Set(LK_B81_PIXEL_NAMES
  .map((n) => LK_GL_NAMES.indexOf('emscripten_gl' + n)).filter((i) => i >= 0));
let lkB81DirtyGen = 0; // bumped on every surface-dirtying execution

function lkB81Ensure(rec) {
  const cv = rec.canvas;
  if (!cv) return null;
  const w = Math.max(1, cv.width | 0), h = Math.max(1, cv.height | 0);
  let b = rec.b81;
  if (b && b.fbo && b.w === w && b.h === h) return b;
  const gl = rec.gl;
  if (!b) {
    b = rec.b81 = { fbo: null, colorTex: null, depthRb: null, w: 0, h: 0, dirty: false, defaultBound: false };
  }
  if (!b.fbo) {
    b.colorTex = gl.createTexture();
    b.depthRb = gl.createRenderbuffer();
    b.fbo = gl.createFramebuffer();
  }
  // (Re)allocate storage in place: the attachment objects keep
  // their identity, so no re-attach is needed on resize. The
  // temporary binds below are restored from the wrapper caches.
  gl.bindTexture(0x0DE1, b.colorTex); // TEXTURE_2D, active unit
  gl.texParameteri(0x0DE1, 0x2801, 0x2601); // MIN_FILTER, LINEAR
  gl.texParameteri(0x0DE1, 0x2800, 0x2601); // MAG_FILTER, LINEAR
  gl.texParameteri(0x0DE1, 0x2802, 0x812F); // WRAP_S, CLAMP_TO_EDGE
  gl.texParameteri(0x0DE1, 0x2803, 0x812F); // WRAP_T, CLAMP_TO_EDGE
  gl.texImage2D(0x0DE1, 0, 0x8058, w, h, 0, 0x1908, 0x1401, null); // RGBA8 / RGBA / UNSIGNED_BYTE
  const prevTex = rec.cache.texBind.get(rec.cache.activeUnit + ':' + 0x0DE1) || 0;
  gl.bindTexture(0x0DE1, prevTex ? lkRegGet(rec, 'texture', prevTex) : null);
  gl.bindRenderbuffer(0x8D41, b.depthRb); // RENDERBUFFER
  gl.renderbufferStorage(0x8D41, 0x88F0, w, h); // DEPTH24_STENCIL8
  const prevRb = rec.cache.rbo || 0;
  gl.bindRenderbuffer(0x8D41, prevRb ? lkRegGet(rec, 'renderbuffer', prevRb) : null);
  gl.bindFramebuffer(0x8D40, b.fbo); // FRAMEBUFFER
  gl.framebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, b.colorTex, 0); // COLOR_ATTACHMENT0
  gl.framebufferRenderbuffer(0x8D40, 0x8D20, 0x8D41, b.depthRb); // DEPTH_STENCIL_ATTACHMENT
  lkB81ApplyBindings(rec);
  b.w = w; b.h = h;
  return b;
}

// The physical object an engine bind of name 0 must produce.
function lkB81Surface(rec) {
  const b = lkB81Ensure(rec);
  return b ? b.fbo : null;
}

// Put the physical READ/DRAW bindings where the routed engine
// state says they belong: cache name 0 (or no bind yet on a
// surface-owning context) -> the surface; an engine name -> its
// registry object; no surface -> the real default framebuffer.
function lkB81ApplyBindings(rec) {
  const b = rec.b81;
  for (const t of [0x8CA9, 0x8CA8]) { // READ_FRAMEBUFFER, DRAW_FRAMEBUFFER
    let name = rec.cache.fbo.get(t);
    if (name === undefined) name = rec.cache.fbo.get(0x8D40);
    rec.gl.bindFramebuffer(t, (name === undefined || name === 0)
      ? (b ? b.fbo : null)
      : lkRegGet(rec, 'framebuffer', name));
  }
}

// True when the engine's DRAW binding currently routes to the
// surface (name 0 bound, or nothing bound yet on a context whose
// surface was made the physical default at make-current).
function lkB81DrawRouted(rec) {
  const b = rec.b81;
  if (!b || !b.fbo) return false;
  let name = rec.cache.fbo.get(0x8CA8); // DRAW_FRAMEBUFFER_BINDING slot
  if (name === undefined) name = rec.cache.fbo.get(0x8D40);
  return name === undefined ? b.defaultBound : name === 0;
}

function lkB81MarkDirty(funcIdx) {
  if (funcIdx !== LK_GLRES_LKIO_PRESENT && !LK_B81_PIXEL_FUNCS.has(funcIdx)) return;
  const rec = lkCtx();
  if (!rec || !lkB81DrawRouted(rec)) return;
  rec.b81.dirty = true;
  lkB81DirtyGen++;
}

// Present the current context's completed surface to its canvas in
// one blit. No-op unless pixel work actually reached the surface.
function lkB81PresentFrame() {
  const rec = lkCtx();
  if (!rec) return;
  const b = rec.b81;
  if (!b || !b.fbo || !b.dirty) return;
  lkB81Ensure(rec); // a resize that bypassed lkSetCanvasSize lands here
  if (!b.fbo) return;
  b.dirty = false;
  const gl = rec.gl, cv = rec.canvas;
  const scissorOn = rec.cache.caps.get(0x0C11) === true; // SCISSOR_TEST
  if (scissorOn) gl.disable(0x0C11);
  const cm = rec.cache.colorMask; // '' until the engine's first ColorMask
  if (cm && cm !== '1,1,1,1') gl.colorMask(true, true, true, true);
  gl.bindFramebuffer(0x8CA9, b.fbo); // READ_FRAMEBUFFER = surface
  gl.bindFramebuffer(0x8CA8, null);  // DRAW_FRAMEBUFFER = canvas
  gl.blitFramebuffer(0, 0, b.w, b.h, 0, 0, cv.width, cv.height,
                     0x00004000, // COLOR_BUFFER_BIT
                     (b.w === cv.width && b.h === cv.height) ? 0x2600 : 0x2601); // NEAREST : LINEAR
  if (cm && cm !== '1,1,1,1') {
    const p = cm.split(',');
    gl.colorMask(!!+p[0], !!+p[1], !!+p[2], !!+p[3]);
  }
  if (scissorOn) gl.enable(0x0C11);
  lkB81ApplyBindings(rec);
}

/* ============ misc runtime imports (non-GL, non-syscall) ============= */

const LK_CLOCK_MONO = 1;
function lkNowMs() {
  return (typeof performance !== 'undefined' ? performance.now() : Date.now());
}

function lkBuildImports() {
  const imp = {
    // ---- proxied services: direct on main, via mailbox in workers ----
    _proc_exit: LK.isWorker
      ? (code) => lkProxyToMainThread(0, 0, 1, code)
      : (code) => LK_SYS_MAIN.proc_exit(code),
  };
  const proxiedNames = [
    ['exit', 1],
    ['__pthread_create_js', 2],
    ['__syscall_chmod', 3], ['__syscall_dup', 4], ['__syscall_dup3', 5],
    ['__syscall_fchmod', 6], ['__syscall_fcntl64', 7], ['__syscall_fstat64', 8],
    ['__syscall_ftruncate64', 9], ['__syscall_getcwd', 10], ['__syscall_getdents64', 11],
    ['__syscall_ioctl', 12], ['__syscall_lstat64', 13], ['__syscall_mkdirat', 14],
    ['__syscall_newfstatat', 15], ['__syscall_openat', 16], ['__syscall_poll', 17],
    ['__syscall_poll_nonblocking', 18], ['__syscall_readlinkat', 19], ['__syscall_renameat', 20],
    ['__syscall_rmdir', 21], ['__syscall_stat64', 22], ['__syscall_symlinkat', 23],
    ['__syscall_truncate64', 24], ['__syscall_unlinkat', 25],
    ['emscripten_exit_pointerlock', 26], ['emscripten_get_canvas_element_size', 27],
    ['emscripten_request_pointerlock', 28], ['emscripten_set_canvas_element_size', 29],
    ['environ_get', 30], ['environ_sizes_get', 31],
    ['fd_close', 32], ['fd_fdstat_get', 33], ['fd_read', 34],
    ['fd_seek', 35], ['fd_sync', 36], ['fd_write', 37],
  ];
  for (const [name, idx] of proxiedNames) {
    if (LK.isWorker) {
      imp[name] = (...args) => lkProxyToMainThread(idx, 0, 1, ...args);
    } else {
      imp[name] = (...args) => LK_PROXIED[idx](...args);
    }
  }
  // Topology reorder (2026-10-21): in a worker, pthread_create is a
  // REQUEST to the host's dispatch on its dedicated channel — it no
  // longer rides the mailbox service proxy (proxied index 2 remains
  // bound on the page, where the import calls the dispatch directly).
  if (LK.isWorker) {
    imp['__pthread_create_js'] = (pthread_ptr, attr, startRoutine, arg) =>
      lkSpawnRequest(pthread_ptr, attr, startRoutine, arg);
  }
  // BigInt-carrying syscalls: __syscall_ftruncate64 / __syscall_truncate64
  // take i64 — worker proxying must pass BigInt through (the tag handles
  // it natively). On the main thread they already run directly.

  Object.assign(imp, {
    memory: LK.memory,

    abort: () => { throw new Error('lkboot: abort() called'); },
    _abort_js: () => { throw new Error('lkboot: abort() called'); },
    _emscripten_abort_js: () => { throw new Error('lkboot: abort() called'); },
    __cxa_throw: (ptr, type, destructor) => {
      // In this build, exceptions the wasm cannot catch itself are
      // fatal (the old runtime's RaiseException was abort()); caught
      // exceptions never leave the wasm. Mirror that contract.
      throw new Error('lkboot: uncaught C++ exception (object at ' + ptr + ')');
    },
    __assert_fail: (condition, filename, line, func) => {
      throw new Error('Assertion failed: ' + lkReadCStr(condition) + ', at: ' +
        (filename ? lkReadCStr(filename) : 'unknown filename') + ':' + line +
        (func ? ' (' + lkReadCStr(func) + ')' : ''));
    },
    emscripten_check_blocking_allowed: () => {},
    emscripten_num_logical_cores: () =>
      (typeof navigator !== 'undefined' && navigator.hardwareConcurrency) || 4,

    // ---- time ----
    emscripten_date_now: () => Date.now(),
    _emscripten_date_now: () => Date.now(),
    emscripten_get_now: () => lkNowMs(),
    _emscripten_get_now: () => lkNowMs(),
    _emscripten_get_now_is_monotonic: () => 1,
    clock_time_get: (clockId, precision, timePtr) => {
      const ns = clockId === LK_CLOCK_MONO
        ? BigInt(Math.floor(lkNowMs() * 1e6))
        : BigInt(Date.now()) * 1000000n;
      lkI64()[timePtr >> 3] = ns;
      return 0;
    },
    _clock_time_get: (clockId, precision, timePtr) => imp.clock_time_get(clockId, precision, timePtr),
    _localtime_js: (time, tmPtr) => {
      const date = new Date(Number(time) * 1000);
      if (isNaN(date.getTime())) return 1;
      const i32 = lkI32();
      const B = tmPtr >> 2;
      i32[B + 0] = date.getSeconds();
      i32[B + 1] = date.getMinutes();
      i32[B + 2] = date.getHours();
      i32[B + 3] = date.getDate();
      i32[B + 4] = date.getMonth();
      i32[B + 5] = date.getFullYear() - 1900;
      i32[B + 6] = date.getDay();
      const start = new Date(date.getFullYear(), 0, 1);
      i32[B + 7] = Math.floor((date - start) / 86400000);
      i32[B + 8] = (date.getTimezoneOffset() === Math.min(
        new Date(date.getFullYear(), 0, 1).getTimezoneOffset(),
        new Date(date.getFullYear(), 6, 1).getTimezoneOffset())) &&
        new Date(date.getFullYear(), 0, 1).getTimezoneOffset() !==
        new Date(date.getFullYear(), 6, 1).getTimezoneOffset() ? 1 : 0;
      i32[B + 9] = -date.getTimezoneOffset() * 60; // tm_gmtoff
      return 0;
    },
    __tzset_js: (timezonePtr, daylightPtr, stdNamePtr, dstNamePtr) => {
      const yr = new Date().getFullYear();
      const winterOff = new Date(yr, 0, 1).getTimezoneOffset();
      const summerOff = new Date(yr, 6, 1).getTimezoneOffset();
      const stdOffMin = Math.max(winterOff, summerOff);
      lkU32()[timezonePtr >> 2] = stdOffMin * 60;
      lkI32()[daylightPtr >> 2] = winterOff !== summerOff ? 1 : 0;
      const fmt = (offMin) => {
        const sign = offMin >= 0 ? '-' : '+';
        const a = Math.abs(offMin);
        return 'UTC' + sign + String(Math.floor(a / 60)).padStart(2, '0') + ':' + String(a % 60).padStart(2, '0');
      };
      lkWriteCStr(stdNamePtr, fmt(stdOffMin), 16);
      lkWriteCStr(dstNamePtr, fmt(Math.min(winterOff, summerOff)), 16);
    },

    // ---- memory ----
    emscripten_get_heap_max: () => 2147483648,
    emscripten_resize_heap: (requested) => {
      try {
        const delta = requested - LK.memory.buffer.byteLength;
        if (delta <= 0) return 1;
        LK.memory.grow(Math.ceil(delta / 65536));
        return 1;
      } catch (e) { return 0; }
    },

    // ---- randomness ----
    random_get: (buf, len) => {
      // getRandomValues rejects SharedArrayBuffer-backed views in
      // browsers (same lesson as TextDecoder): fill a private scratch
      // buffer, then copy into shared memory.
      const chunk = 65536;
      const u8 = lkU8();
      const scratch = new Uint8Array(Math.min(chunk, len));
      for (let off = 0; off < len; off += chunk) {
        const n = Math.min(len - off, chunk);
        crypto.getRandomValues(n === scratch.length ? scratch : scratch.subarray(0, n));
        u8.set(scratch.subarray(0, n), buf + off);
      }
      return 0;
    },
    _random_get: (buf, len) => imp.random_get(buf, len),
    getentropy: (buf, len) => {
      if (len > 256) return -LK_E.INVAL;
      const r = imp.random_get(buf, len);
      return r === 0 ? 0 : -r;
    },

    // ---- exceptions / setjmp bookkeeping ----
    __cxa_find_pointer_type: () => 0,
    __cxa_can_catch: () => 1,
    _setThrew: (thrownPtr, thrownValue) => {
      LK.exports.setThrew(thrownPtr, thrownValue);
    },
    _Unwind_RaiseException: () => { throw new Error('lkboot: _Unwind_RaiseException (C++ throw)'); },
    __cxa_call_unterminate: () => { throw new Error('lkboot: terminate'); },
    _ZSt18uncaught_exceptionv: () => 0,
    __cxa_uncaught_exceptions: () => 0,
    _emscripten_cxa_new_delete: () => {},
    __resumeException: (ptr) => { throw new Error('lkboot: C++ exception resumed'); },
    _invoke_cleanup: (ptr) => {},
    _emscripten_throw_longjmp: () => { throw new Error('lkboot: longjmp'); },
    _emscripten_longjmp: (env, value) => { throw new Error('lkboot: longjmp (unsupported path)'); },
    __throw_exception_with_stack_trace: (ptr) => { throw new Error('lkboot: C++ throw'); },
    __handle_stack_overflow: () => { throw new Error('lkboot: stack overflow'); },
    _emscripten_check_blocking_allowed: () => {},
    _emscripten_runtime_thread_is_main_thread: () => (LK.isWorker ? 0 : 1),
    _emscripten_thread_is_main_runtime_thread: () => (LK.isWorker ? 0 : 1),
    _emscripten_thread_is_browser_thread: () => 1,
    _emscripten_system: () => -1,

    // ---- pthread glue (binary-internal expectations) ----
    _emscripten_thread_cleanup: (pthread_ptr) => {
      if (LK.isWorker) {
        self.postMessage({ cmd: 'lk-done', pthread_ptr });
      }
    },
    _emscripten_thread_mailbox_await: (pthread_ptr) => lkMailboxAwait(pthread_ptr),
    _emscripten_notify_mailbox_postmessage: (target, current) => lkNotifyMailbox(target, current),
    _emscripten_receive_on_main_thread_js: (funcIndex, emAsmAddr, callingThread, bufSize, args, ctx, ctxArgs) =>
      lkReceiveOnMainThread(funcIndex, emAsmAddr, callingThread, bufSize, args, ctx, ctxArgs),
    _emscripten_init_main_thread_js: (tb) => {
      LK.mainThreadPtr = tb; // the main worker's thread_init reuses this struct
      let canBlock = true;
      try { Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 1, 0); }
      catch (e) { canBlock = false; }
      LK.exports._emscripten_thread_init(tb, LK.isWorker ? 0 : 1, 1,
        canBlock ? 1 : 0, 8388608, 0);
      LK.exports._emscripten_tls_init();
    },
    _emscripten_thread_exit_proxied: (code) => {
      if (LK.isWorker) return lkProxyToMainThread(0, 0, 1, code);
      return lkDoExit(code);
    },
    _emscripten_thread_set_strongref: () => {},
    __emscripten_thread_exit: (code) => { throw LK_UNWIND; },
    _emscripten_yield: () => {},

    // ---- EM_ASM ----
    emscripten_asm_const_int: (code, sigPtr, argbuf) => lkAsmConstInt(code, sigPtr, argbuf),
    emscripten_asm_const_int_sync_on_main_thread: (code, sigPtr, argbuf) => {
      if (LK.isWorker) {
        const args = lkReadAsmArgs(sigPtr, argbuf);
        return lkProxyToMainThread(0, code, 1, ...args);
      }
      return lkAsmConstInt(code, sigPtr, argbuf);
    },
    emscripten_asm_const_ptr: (code, sigPtr, argbuf) => lkAsmConstInt(code, sigPtr, argbuf),
    emscripten_asm_const_async_on_main_thread: (code, sigPtr, argbuf) => {
      if (LK.isWorker) {
        const args = lkReadAsmArgs(sigPtr, argbuf);
        return lkProxyToMainThread(0, code, 3, ...args);
      }
      return lkAsmConstInt(code, sigPtr, argbuf);
    },

    // ---- main loop ----
    // Registered by the MAIN WORKER (engine main lives there): record
    // the callback locally and tell the page to start asking for frames.
    // main() then unwinds exactly as the binary expects.
    emscripten_set_main_loop_arg: (cb, arg, fps, simulateInfiniteLoop) => {
      LK.mainLoop = { cb, arg };
      if (LK.isWorker) self.postMessage({ cmd: 'lk-mainloop', on: true });
      if (simulateInfiniteLoop) throw LK_UNWIND;
    },
    _emscripten_set_main_loop_arg: (cb, arg, fps, simulateInfiniteLoop) =>
      imp.emscripten_set_main_loop_arg(cb, arg, fps, simulateInfiniteLoop),
    emscripten_cancel_main_loop: () => {
      LK.mainLoop = null;
      if (LK.isWorker) self.postMessage({ cmd: 'lk-mainloop', on: false });
    },
    emscripten_exit_with_live_runtime: () => { throw LK_UNWIND; },
    exit_with_live_runtime: () => { throw LK_UNWIND; },
    emscripten_request_animation_frame_loop: () => 0,
    _emscripten_request_animation_frame_loop: () => 0,
    emscripten_vibrate: () => 0,

    // ---- html5 events: the engine uses its own EM_ASM DOM listeners
    // and the LKIO ring; these registrations accept-and-ignore because
    // callback delivery through this channel is provably dead in the
    // binary's configuration (drain-fix run evidence). ----
    emscripten_set_keydown_callback_on_thread: () => -1,
    emscripten_set_keyup_callback_on_thread: () => -1,
    emscripten_set_keypress_callback_on_thread: () => -1,
    emscripten_set_mousedown_callback_on_thread: () => -1,
    emscripten_set_mouseup_callback_on_thread: () => -1,
    emscripten_set_mousemove_callback_on_thread: () => -1,
    emscripten_set_mouseenter_callback_on_thread: () => -1,
    emscripten_set_mouseleave_callback_on_thread: () => -1,
    emscripten_set_wheel_callback_on_thread: () => -1,
    emscripten_set_click_callback_on_thread: () => -1,
    emscripten_set_dblclick_callback_on_thread: () => -1,
    emscripten_set_touchstart_callback_on_thread: () => -1,
    emscripten_set_touchend_callback_on_thread: () => -1,
    emscripten_set_touchmove_callback_on_thread: () => -1,
    emscripten_set_touchcancel_callback_on_thread: () => -1,
    emscripten_set_resize_callback_on_thread: () => -1,
    emscripten_set_visibilitychange_callback_on_thread: () => -1,
    emscripten_set_fullscreenchange_callback_on_thread: () => -1,
    emscripten_set_pointerlockchange_callback_on_thread: () => -1,
    emscripten_set_focus_callback_on_thread: () => -1,
    emscripten_set_blur_callback_on_thread: () => -1,
    emscripten_set_gamepadconnected_callback_on_thread: () => -1,
    emscripten_set_gamepaddisconnected_callback_on_thread: () => -1,
    emscripten_set_scroll_callback_on_thread: () => -1,

    // ---- canvas / window ----
    // In a worker these forward through the GL command stream to the
    // page (the context lives there); on the page they run directly.
    emscripten_webgl_create_context: (targetPtr, attrsPtr) => {
      if (LK.isWorker) return lkGLStreamCall(LK_GLRES_CTX_CREATE, true, [targetPtr, attrsPtr]);
      const canvas = lkFindCanvas(targetPtr);
      return canvas ? lkWebGLCreateContext(canvas, attrsPtr) : 0;
    },
    _emscripten_webgl_create_context: (targetPtr, attrsPtr) => {
      if (LK.isWorker) return lkGLStreamCall(LK_GLRES_CTX_CREATE, true, [targetPtr, attrsPtr]);
      const canvas = lkFindCanvas(targetPtr);
      return canvas ? lkWebGLCreateContext(canvas, attrsPtr) : 0;
    },
    emscripten_webgl_make_context_current: (handle) => {
      if (LK.isWorker) return lkGLStreamCall(LK_GLRES_CTX_MAKE_CURRENT, true, [handle]);
      return lkGLMakeCurrent(handle);
    },
    emscripten_webgl_get_current_context: () => {
      if (LK.isWorker) return lkGLStreamCall(LK_GLRES_CTX_GET_CURRENT, true, []);
      const rec = lkCtx();
      return rec ? rec.handle : 0;
    },
    emscripten_webgl_destroy_context: (handle) => {
      if (LK.isWorker) { lkGLStreamCall(LK_GLRES_CTX_DESTROY, false, [handle]); return 0; }
      lkGLDestroyContext(handle);
      return 0;
    },
    emscripten_webgl_enable_extension: (ctxHandle, namePtr) => {
      if (LK.isWorker) return lkGLStreamCall(LK_GLRES_CTX_ENABLE_EXT, true, [ctxHandle, namePtr]);
      const rec = lkCtx(); if (!rec) return 0;
      let name = lkReadCStr(namePtr);
      if (name.startsWith('GL_')) name = name.slice(3);
      try { return rec.gl.getExtension(name) ? 1 : 0; } catch (e) { return 0; }
    },
    _emscripten_get_device_pixel_ratio: () =>
      (typeof devicePixelRatio !== 'undefined' ? devicePixelRatio : 1),
    emscripten_get_device_pixel_ratio: () =>
      (typeof devicePixelRatio !== 'undefined' ? devicePixelRatio : 1),
    _emscripten_get_window_inner_size: (wPtr, hPtr) => {
      lkI32()[wPtr >> 2] = (typeof innerWidth !== 'undefined' ? innerWidth : 0) | 0;
      lkI32()[hPtr >> 2] = (typeof innerHeight !== 'undefined' ? innerHeight : 0) | 0;
    },
    _get_canvas_size: () => {},
    emscripten_get_screen_size: (wPtr, hPtr) => {
      lkI32()[wPtr >> 2] = (typeof screen !== 'undefined' ? screen.width : 0) | 0;
      lkI32()[hPtr >> 2] = (typeof screen !== 'undefined' ? screen.height : 0) | 0;
    },
    emscripten_get_element_css_size: (targetPtr, wPtr, hPtr) => {
      const el = lkFindCanvas(targetPtr);
      if (!el) return -1;
      const rect = el.getBoundingClientRect();
      lkF64()[wPtr >> 3] = rect.width;
      lkF64()[hPtr >> 3] = rect.height;
      return 0;
    },
    emscripten_set_element_css_size: (targetPtr, w, h) => {
      const el = lkFindCanvas(targetPtr);
      if (el) { el.style.width = w + 'px'; el.style.height = h + 'px'; }
      return 0;
    },
    _emscripten_set_offscreencanvas_size_on_thread: () => -1,
    _emscripten_webgl_do_create_context: (targetPtr, attrsPtr) => {
      if (LK.isWorker) return lkGLStreamCall(LK_GLRES_CTX_CREATE, true, [targetPtr, attrsPtr]);
      const canvas = lkFindCanvas(targetPtr);
      return canvas ? lkWebGLCreateContext(canvas, attrsPtr) : 0;
    },
    emscripten_get_num_gamepads: () => 0,
    emscripten_sample_gamepad_data: () => -1,
    emscripten_get_gamepad_status: () => -1,
    emscripten_get_preloaded_image_data_from_FILE: () => -1,
    emscripten_run_script_string: () => 0,
    emscripten_set_immediate: () => 0,
    emscripten_clear_immediate: () => {},
  });

  // The GL import surface: on the page, the real wrapper (LK_GL) — the
  // executor calls these directly. In a worker, stream stubs that
  // serialize each call into the command ring for the page to execute.
  const glSource = LK.isWorker ? lkGLStubs() : LK_GL;
  for (const [name, fn] of Object.entries(glSource)) imp[name] = fn;
  imp['emscripten_glGetString'] = glSource['emscripten_glGetString'];
  imp['lkio_present'] = LK.isWorker
    ? (...args) => lkGLStreamCall(LK_GLRES_LKIO_PRESENT, true, args)
    : lkioPresent;
  imp['_tzset_js'] = imp['__tzset_js'];
  return imp;
}

/* ============================ boot =================================== */

async function lkFetchPack() {
  const m = (typeof Module !== 'undefined') ? Module : {};
  const url = (m.locateFile ? m.locateFile('LithoKraft.data', '') : 'LithoKraft.data');
  const resp = await fetch(url);
  if (!resp.ok) throw new Error('lkboot: data pack fetch failed: HTTP ' + resp.status);
  const total = Number(resp.headers.get('Content-Length') || 0);
  const reader = resp.body && resp.body.getReader ? resp.body.getReader() : null;
  let blob;
  if (reader) {
    const chunks = [];
    let got = 0;
    for (;;) {
      const { done, value } = await reader.read();
      if (done) break;
      chunks.push(value);
      got += value.length;
      if (m.setStatus) m.setStatus('Downloading data... (' + got + '/' + (total || got) + ')');
    }
    blob = new Uint8Array(got);
    let off = 0;
    for (const c of chunks) { blob.set(c, off); off += c.length; }
  } else {
    blob = new Uint8Array(await resp.arrayBuffer());
  }
  if (blob.length < 8 || lkTextDecoder.decode(blob.subarray(0, 4)) !== 'LKP2') {
    throw new Error('lkboot: LithoKraft.data is not an LKP2 pack (see lkpack.py)');
  }
  // LKP2 binary header (bit-packing ledger adoption, bug-fix run):
  //   [0..3]  'LKP2'
  //   [4..7]  u32 headerLen (bytes after this field, before payload)
  //   dirs:   u16 count; per dir: u16 byteLen + UTF-8 bytes
  //   files:  u32 count; per file: u16 dirIndex, u16 nameLen + name,
  //           u32 startDelta (LEB128, from the previous file's start),
  //           u32 length (LEB128)
  // Every range is validated before use (this also discharges the old
  // L9 finding: a malformed pack is a loud boot failure, never a
  // silent read past the payload).
  const dv = new DataView(blob.buffer, blob.byteOffset, blob.byteLength);
  const hlen = dv.getUint32(4, true);
  const hend = 8 + hlen;
  if (hlen < 6 || hend > blob.length) {
    throw new Error('lkboot: LKP2 header length out of range: ' + hlen);
  }
  let p = 8;
  const bad = (what) => { throw new Error('lkboot: LKP2 pack malformed: ' + what); };
  const dirCount = dv.getUint16(p, true); p += 2;
  const dirs = [];
  for (let i = 0; i < dirCount; i++) {
    if (p + 2 > hend) bad('dir table overruns header');
    const n = dv.getUint16(p, true); p += 2;
    if (p + n > hend) bad('dir name overruns header');
    dirs.push(lkTextDecoder.decode(blob.subarray(p, p + n))); p += n;
  }
  if (p + 4 > hend) bad('file table overruns header');
  const fileCount = dv.getUint32(p, true); p += 4;
  const files = [];
  let start = 0;
  const payloadLen = blob.length - hend;
  const readVarint = () => {
    let v = 0, shift = 0;
    for (;;) {
      if (p >= hend) bad('varint overruns header');
      const byte = blob[p++];
      v += (byte & 0x7f) * (2 ** shift);
      if (!(byte & 0x80)) return v;
      shift += 7;
      if (shift > 28) bad('varint too long');
    }
  };
  for (let i = 0; i < fileCount; i++) {
    if (p + 4 > hend) bad('file entry overruns header');
    const dirIdx = dv.getUint16(p, true); p += 2;
    const nameLen = dv.getUint16(p, true); p += 2;
    if (dirIdx >= dirs.length) bad('file dir index out of range');
    if (p + nameLen > hend) bad('file name overruns header');
    const name = lkTextDecoder.decode(blob.subarray(p, p + nameLen)); p += nameLen;
    if (!name || name.includes('/') || name.includes('..')) bad('bad file name: ' + name);
    start += readVarint();
    const len = readVarint();
    if (start + len > payloadLen) bad('file range overruns payload: ' + name);
    const dir = dirs[dirIdx];
    files.push({ path: dir === '/' ? '/' + name : dir + '/' + name, start, len });
  }
  if (p !== hend) bad('header length mismatch: parsed ' + (p - 8) + ' of ' + hlen);
  lkOut('lkboot: pack LKP2: ' + files.length + ' files, ' + dirs.length +
        ' dirs, header ' + hlen + ' B, payload ' + payloadLen + ' B');
  return { blob, files, dirs, payloadStart: hend };
}

function lkWriteArgs() {
  const m = (typeof Module !== 'undefined') ? Module : {};
  const args = ['LithoKraft', ...(m.arguments || [])];
  const ptrsSize = (args.length + 1) * 4;
  const argv = LK.exports.malloc(ptrsSize);
  const u32 = lkU32();
  args.forEach((a, i) => {
    const bytes = lkTextEncoder.encode(a);
    const p = LK.exports.malloc(bytes.length + 1);
    lkU8().set(bytes, p);
    lkU8()[p + bytes.length] = 0;
    u32[(argv >> 2) + i] = p;
  });
  u32[(argv >> 2) + args.length] = 0;
  return { argc: args.length, argv };
}

async function lkBootMain() {
  const m = (typeof Module !== 'undefined') ? Module : {};
  LK.canvas = document.getElementById('canvas') || null;
  // Stage-tracked, loud boot (bug-fix run, 2026-10-20). The device
  // lesson (shell v1.3, JD's moto g): a boot that dies silently behind
  // "see JavaScript console" on a device with no console is the exact
  // failure species this layer exists to kill. Every stage names
  // itself as it starts; the entry catch prints BOOT FAILED with the
  // stage it died in, the error's name and message, in the page.
  lkBootStage('environment');
  const coi = (typeof crossOriginIsolated !== 'undefined') ? String(crossOriginIsolated) : 'unknown';
  lkOut('lkboot: env: crossOriginIsolated=' + coi +
        ' SharedArrayBuffer=' + (typeof SharedArrayBuffer) +
        ' hardwareConcurrency=' + ((typeof navigator !== 'undefined' && navigator.hardwareConcurrency) || '?'));
  // Topology (host dispatch, 2026-10-21): this page is the host
  // surface and the layer's only worker authority. In an isolated
  // context it provisions the main worker + pool and the engine asks
  // it for threads. Without isolation the shared memory cannot be
  // posted to any worker; the engine is multithreaded from startup
  // (std::thread pools — a failed create throws and aborts it) and a
  // wasm thread cannot be fabricated on the host thread, so this
  // boot will stop at the spawn stage with a standing blocked notice
  // — never the v1.4 bare DataCloneError, never an engine abort.
  LK.singleAgent = (coi !== 'true');
  if (LK.singleAgent) {
    lkErr('lkboot: SINGLE-AGENT BLOCKED: crossOriginIsolated=' + coi + ' — this context cannot run the engine.');
    lkErr('lkboot: SINGLE-AGENT BLOCKED: the engine is multithreaded and its threads share one wasm memory; ' +
          'without cross-origin isolation that memory cannot be posted to a worker, and threads cannot be ' +
          'fabricated on the host thread. Serve this page from a cross-origin-isolated origin to run the game.');
  } else {
    lkOut('lkboot: host dispatch: the page provisions all workers (main + pool, cap ' +
          LK_POOL_MAX_WORKERS + '); engine thread requests arrive on the dispatch channel.');
  }
  try {
    LK.memory = new WebAssembly.Memory({ initial: 8192, maximum: 32768, shared: true });
  } catch (e) {
    throw new Error('shared WebAssembly.Memory unavailable — this build requires a cross-origin-isolated context (COOP/COEP): ' + (e && e.message || e));
  }
  lkOut('lkboot: shared wasm memory ok: ' + (LK.memory.buffer.byteLength / 1048576) + ' MiB initial');
  lkBootStage('pack fetch');
  const pack = await lkFetchPack();
  LK.fs = lkBuildFS(pack);
  lkBootStage('wasm instantiate');
  const wasmUrl = (m.locateFile ? m.locateFile('LithoKraft.wasm', '') : 'LithoKraft.wasm');
  const imports = lkBuildImports();
  let instance;
  try {
    const streaming = await WebAssembly.instantiateStreaming(fetch(wasmUrl), { env: imports, wasi_snapshot_preview1: imports, a: imports });
    instance = streaming.instance;
    LK.module = streaming.module;
  } catch (e) {
    const buf = await (await fetch(wasmUrl)).arrayBuffer();
    const compiled = await WebAssembly.compile(buf);
    LK.module = compiled;
    instance = await WebAssembly.instantiate(compiled, { env: imports, wasi_snapshot_preview1: imports, a: imports });
  }
  LK.instance = instance;
  LK.exports = instance.exports;
  lkGLClassAudit();
  // Worker threads share this compiled module (spawned lazily).
  LK.exports.__wasm_call_ctors();
  lkBootStage('command ring + rpc slot');
  // The GL command ring lives in shared memory (allocated here, after
  // the runtime's own initialization): 8 MiB of command space.
  const ringSize = 8 * 1024 * 1024;
  const ringBase = LK.exports.malloc(ringSize + LK_GL_HDR + 64);
  lkU8().fill(0, ringBase, ringBase + LK_GL_HDR);
  LK.glRing = { base: ringBase, size: ringSize };
  // Shared slot for synchronous service RPCs forwarded by workers
  // (see lkReceiveForward): state i32 @0, tag i32 @8, value @16.
  // Backed by the wasm memory itself, NOT a standalone
  // SharedArrayBuffer — see lkRpcViews for the device evidence.
  LK.rpcOff = LK.exports.malloc(64);
  lkU8().fill(0, LK.rpcOff, LK.rpcOff + 64);
  const { argc, argv } = lkWriteArgs();
  if (LK.singleAgent) {
    // Standing blocked state (see the env-stage notice): everything
    // upstream has been proven on this device (memory, pack,
    // instantiate, ring), and the boot stops HERE — before any
    // thread is requested — with the reason on the page's own status
    // line, where a phone user reads it without opening a console.
    lkBootStage('blocked: cross-origin isolation required');
    const m = (typeof Module !== 'undefined') ? Module : null;
    if (m && m.setStatus) {
      try {
        m.setStatus('Blocked: this page is not cross-origin isolated, so the engine\'s threads cannot start. ' +
                    'Open the game from an isolated origin (the Android shell v1.5 HTTPS build, or any ' +
                    'COOP/COEP-served copy) to play.');
      } catch (e) {}
    }
    return;
  }
  lkBootStage('spawn main worker');
  // Engine main runs on the main worker; this thread only serves.
  lkSpawnMainWorker(argc, argv);
  lkPageDriverStart();
  lkBootStage('running');
}

/* ======================= worker bootstrap ============================ */

function lkWorkerMain() {
  let loaded = false;
  let pendingRuns = [];
  let pendingMain = null;
  const startMain = (d) => {
    // Adopt the main-thread struct (see lkSpawnMainWorker), but run
    // main() on a private stack region: the main stack region doubles
    // as the page instance's service stack, and what the constructors
    // left there poisons main's entry. The struct's stack fields are
    // pointed at the fresh region (allocated by the page, passed in
    // the lk-main message).
    const u32 = lkU32();
    u32[(d.pthread_ptr + 48) >> 2] = d.stackHigh;
    u32[(d.pthread_ptr + 52) >> 2] = d.stackSize;
    LK.exports.emscripten_stack_set_limits(d.stackHigh, d.stackHigh - d.stackSize);
    LK.exports._emscripten_stack_restore(d.stackHigh);
    LK.exports._emscripten_thread_init(d.pthread_ptr, 0, 0, 1, 0, 0);
    LK.exports._emscripten_tls_init();
    lkMailboxAwait(d.pthread_ptr);
    let unwound = false;
    let rc = 0;
    try {
      rc = LK.exports.__main_argc_argv(d.argc, d.argv) | 0;
    } catch (e) {
      if (e === LK_EXIT) { /* proc_exit path already ran on the page */ }
      else if (lkIsUnwind(e)) { unwound = true; /* main loop registered; main unwound as designed */ }
      else {
        self.postMessage({ cmd: 'lk-crashed', pthread_ptr: 0, error: 'main: ' + (e && e.message || e) + ' ' + String(e && e.stack || '').split('\n').slice(0, 7).join(' <- ') });
      }
    }
    // On the designed unwind the program is NOT done — its main loop
    // is live and frames will be driven by the page. Reporting
    // lk-main-done here would make the page shut that loop down.
    if (!unwound) {
      self.postMessage({ cmd: 'lk-main-done', exitCode: rc });
    }
  };
  const runFrame = () => {
    if (!LK.mainLoop) { self.postMessage({ cmd: 'lk-frame-done' }); return; }
    const { cb, arg } = LK.mainLoop;
    try {
      LK.exports.__indirect_function_table.get(cb)(arg);
    } catch (e) {
      if (lkIsUnwind(e) || e === LK_EXIT) {
        LK.mainLoop = null;
        self.postMessage({ cmd: 'lk-mainloop', on: false });
      } else {
        // A dead frame callback ends the loop WORKER-side too (the
        // page marks the death terminal in lkMainWorkerDied).
        LK.mainLoop = null;
        self.postMessage({ cmd: 'lk-mainloop', on: false });
        self.postMessage({ cmd: 'lk-crashed', pthread_ptr: 0, error: 'frame: ' + (e && e.message || e) });
      }
    }
    self.postMessage({ cmd: 'lk-frame-done' });
  };
  const startThread = (d) => {
    const stackHigh = lkU32()[(d.pthread_ptr + 48) >> 2];
    const stackSize = lkU32()[(d.pthread_ptr + 52) >> 2];
    LK.exports.emscripten_stack_set_limits(stackHigh, stackHigh - stackSize);
    LK.exports._emscripten_stack_restore(stackHigh);
    LK.exports._emscripten_thread_init(d.pthread_ptr, 0, 0, 1, 0, 0);
    // TLS init is NOT optional: it binds this instance's __tls_base to
    // the thread's TLS block (pointer at struct+100). Without it every
    // thread_local access in the engine reads through a null base.
    LK.exports._emscripten_tls_init();
    lkMailboxAwait(d.pthread_ptr);
    let result;
    let returned = false;
    try {
      const entry = LK.exports.__indirect_function_table.get(d.start_routine);
      result = entry(d.arg);
      returned = true;
    } catch (e) {
      if (lkIsUnwind(e) || e === LK_EXIT) {
        // Clean thread exit: the thread called exit() itself, so the
        // in-wasm exit path already ran (and threw to get here).
      } else {
        try { LK.exports._emscripten_thread_crashed(); } catch (e2) {}
        self.postMessage({ cmd: 'lk-crashed', pthread_ptr: d.pthread_ptr, error: String(e && e.message || e) + ' ' + String(e && e.stack || '').split('\n').slice(0, 7).join(' <- ') });
      }
    }
    if (returned) {
      // The worker wrapper owes the binary its thread-exit finish:
      // _emscripten_thread_exit runs the exit bookkeeping in-wasm —
      // detach/exit state, futex wake for pthread_join, TLS/dtors, and
      // the _emscripten_thread_cleanup import (which posts lk-done so
      // the page can recycle this worker). Without it a thread that
      // simply returns leaves every joiner waiting forever. (Boot
      // threads never return, which is why only the selftest's
      // short-lived threads exposed this.)
      try { LK.exports._emscripten_thread_exit(result || 0); }
      catch (e) { if (!lkIsUnwind(e) && e !== LK_EXIT) throw e; }
    }
  };
  self.onmessage = (e) => {
    const d = e.data || {};
    if (d.cmd === 'lk-load') {
      LK.memory = d.memory;
      LK.module = d.module;
      LK.glRing = d.glRing || null;
      LK.rpcOff = d.rpc | 0;
      LK.role = d.role || 'pool';
      const imports = lkBuildImports();
      WebAssembly.instantiate(d.module, { env: imports, wasi_snapshot_preview1: imports, a: imports }).then((instance) => {
        LK.instance = instance;
        LK.exports = instance.exports;
        loaded = true;
        const queued = pendingRuns;
        pendingRuns = [];
        for (const run of queued) startThread(run);
        if (pendingMain) { const pm = pendingMain; pendingMain = null; startMain(pm); }
        self.postMessage({ cmd: 'lk-loaded' });
      }).catch((err) => {
        self.postMessage({ cmd: 'lk-crashed', pthread_ptr: 0, error: 'instantiate: ' + (err && err.message || err) });
      });
    } else if (d.cmd === 'lk-run') {
      if (!loaded) pendingRuns.push(d);
      else startThread(d);
    } else if (d.cmd === 'lk-main') {
      if (!loaded) pendingMain = d;
      else startMain(d);
    } else if (d.cmd === 'lk-frame') {
      runFrame();
    } else if (d.cmd === 'lk-rpc-done') {
      // The page executed a forwarded async service; complete the
      // queued call in this instance.
      try { LK.exports._emscripten_run_js_on_main_thread_done(d.ctx, d.ctxArgs, d.value); } catch (e) {}
    } else if (d.cmd === 4) {
      lkCheckMailbox();
    }
  };
}

/* ============================ entry ================================== */

if (LK_IS_WORKER) {
  // Workers run this file alone; the EM_ASM table sits beside it.
  if (typeof lkAsmConsts !== 'function') {
    try { importScripts(new URL('lkasm.js', self.location.href).href); } catch (e) {}
  }
  lkWorkerMain();
} else if (typeof document !== 'undefined') {
  lkBootMain().catch((e) => {
    // The loud-error rule, permanent (diag on or off): a boot failure
    // names itself, its stage, and its cause IN THE PAGE — the status
    // line, the log pane, and the pages' sticky error slot all carry
    // it. "See JavaScript console" is not an acceptable epitaph on a
    // device that has no console.
    const cause = (e && e.name ? e.name + ': ' : '') + (e && e.message || e);
    const frames = e && e.stack ? ' <- ' + String(e.stack).split('\n').slice(1, 5).join(' <- ') : '';
    lkErr('lkboot: BOOT FAILED at stage "' + LK_BOOT_STAGE + '": ' + cause + frames);
    const m = (typeof Module !== 'undefined') ? Module : null;
    if (m && m.setStatus) {
      try { m.setStatus('BOOT FAILED at stage "' + LK_BOOT_STAGE + '": ' + (e && e.message || e)); } catch (e2) {}
    }
  });
}
