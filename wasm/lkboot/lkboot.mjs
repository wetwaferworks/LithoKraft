// lkboot.mjs ... the in-house host for the LithoRoot web module.
// No Emscripten, no libraries. Runs in a browser (page or worker) and
// in Node (tests). Owns: the 14 frozen lithosys calls, the 11 lk_store
// calls (lkstore.mjs), threads (lkthread.mjs), the memory budget, and
// the start sequence. The lk_gl* / input / canvas imports come from
// opts.providers (not part of this file).
//
// Start sequence (main thread):
//   instantiate -> __wasm_call_ctors -> check __lithoroot_crt_status == 0
//   -> __main_argc_argv(argc, argv)   (or __main_void for test modules)
//
// Modes:
//   threaded     shared memory; needs SharedArrayBuffer, i.e.
//                crossOriginIsolated in a browser.
//   single       JD's approved fallback when !crossOriginIsolated: the
//                module linked without --shared-memory (opts.moduleSingle),
//                unshared memory, lithosys_thread_spawn says EAGAIN,
//                cpu_count is 1. lk_store then has no blocking channel
//                and uses opts.singleStore (default: an in-memory store,
//                NOT persistent; see the report).
//
// Memory: the budget (default 480 MiB, clamped to 450..500) refuses
// lithosys_memory_grow past it with ENOMEM; the hard cap (600 MiB, the
// module's declared maximum) can never be passed whatever the budget.
import { MemoryBackend, StoreClient, makeChannel } from './lkstore.mjs';

export const LITHOSYS = { OK: 0, EAGAIN: 6, EBADF: 8, EFAULT: 21, EINVAL: 28, EIO: 29, ENOENT: 44, ENOMEM: 48, ENOSYS: 52, ESPIPE: 70 };
export const PAGE = 65536, HARD_CAP_MIB = 600;
export class LkExit extends Error { constructor(code) { super('exit ' + code); this.code = code; } }
export class LkBootError extends Error {}

const VOL_SLOTS = 61, VOL_FD0 = 3; // volume fds 3..63 (64+ belong to LithoVFS)

export function detectMode(g = globalThis) {
  const sab = typeof g.SharedArrayBuffer === 'function';
  const isolated = g.crossOriginIsolated !== false; // undefined in Node: no isolation concept
  return sab && isolated ? 'threaded' : 'single';
}

export function clampBudgetMiB(b) { return Math.min(500, Math.max(450, b ?? 480)); }

// Shared state every thread's import object closes over.
export function makeShared({ mode, memory, volume, budgetMiB, channel, cpuCount }) {
  // The pack: one buffer + a name -> [offset, length] directory.
  const names = [...(volume ? volume.keys() : [])];
  let total = 0; const dir = new Map();
  for (const n of names) { dir.set(n.replace(/^\/+/, ''), [total, volume.get(n).length]); total += volume.get(n).length; }
  const Buf = mode === 'threaded' ? SharedArrayBuffer : ArrayBuffer;
  const packBuf = new Buf(Math.max(1, total)); const pk = new Uint8Array(packBuf);
  for (const n of names) pk.set(volume.get(n), dir.get(n.replace(/^\/+/, ''))[0]);
  // Volume fd table: [state, fileIndex, posLo, posHi] per slot, claimed by CAS.
  const volTab = new Buf(4 * 4 * VOL_SLOTS);
  return { mode, memory, packBuf, dir: [...dir], volTab, budgetMiB: clampBudgetMiB(budgetMiB), channel, cpuCount };
}

// Build the env import object for one thread.
export function makeImports(sh, t) {
  // t: { canBlock, exports (set after instantiate), spawn(startArg) -> tid, onConsole(text, isErr), store }
  const mem = sh.memory, dir = new Map(sh.dir.map(([k, v]) => [k, v]));
  const fileIdx = [...dir.keys()];
  const pack = new Uint8Array(sh.packBuf), vt = new Int32Array(sh.volTab);
  const dv = () => new DataView(mem.buffer);
  const u8 = () => new Uint8Array(mem.buffer);
  const cstr = p => { const b = u8(); let e = p; while (b[e]) e++; return new TextDecoder().decode(b.slice(p, e)); };
  const lines = { 1: '', 2: '' };
  const emit = (fd, text) => {
    lines[fd] += text; let k;
    while ((k = lines[fd].indexOf('\n')) >= 0) { t.onConsole(lines[fd].slice(0, k), fd === 2); lines[fd] = lines[fd].slice(k + 1); }
  };
  const volGet = fd => { const s = fd - VOL_FD0; if (s < 0 || s >= VOL_SLOTS || Atomics.load(vt, s * 4) !== 1) return -1; return s; };
  const iovs = (p, n) => { const d = dv(), out = []; for (let j = 0; j < n; j++) out.push([d.getUint32(p + 8 * j, true), d.getUint32(p + 8 * j + 4, true)]); return out; };
  const clock0 = (globalThis.performance?.timeOrigin ?? Date.now());
  const store = t.store;
  const env = {
    memory: mem,
    lithosys_memory_grow(delta, outOld) {
      const old = mem.buffer.byteLength / PAGE, want = old + delta;
      if (want * PAGE > sh.budgetMiB * 1048576 || want * PAGE > HARD_CAP_MIB * 1048576) { t.onBudget?.(want * PAGE); return LITHOSYS.ENOMEM; }
      try { mem.grow(delta); } catch { return LITHOSYS.ENOMEM; }
      dv().setUint32(outOld, old, true); return 0;
    },
    lithosys_clock_now_ns(id, out) {
      let ns;
      if (id === 0) ns = BigInt(Math.round((clock0 + performance.now()) * 1e6)); // monotonic, same origin on every thread
      else if (id === 1) ns = BigInt(Date.now()) * 1000000n;
      else return LITHOSYS.EINVAL;
      dv().setBigUint64(out, ns, true); return 0;
    },
    lithosys_thread_spawn(startArg) { return sh.mode === 'threaded' && t.spawn ? t.spawn(startArg) : -LITHOSYS.EAGAIN; },
    lithosys_fd_write(fd, p, n, out) {
      if (fd !== 1 && fd !== 2) return LITHOSYS.EBADF;
      let tot = 0; const b = u8();
      for (const [bp, bl] of iovs(p, n)) { emit(fd, new TextDecoder().decode(b.slice(bp, bp + bl))); tot += bl; }
      if (out) dv().setUint32(out, tot, true); return 0;
    },
    lithosys_fd_read(fd, p, n, out) {
      if (fd === 0) { if (out) dv().setUint32(out, 0, true); return 0; }
      const s = volGet(fd); if (s < 0) return LITHOSYS.EBADF;
      const [off, len] = dir.get(fileIdx[vt[s * 4 + 1]]);
      let pos = vt[s * 4 + 2] >>> 0, got = 0; const b = u8();
      for (const [bp, bl] of iovs(p, n)) { const m = Math.max(0, Math.min(bl, len - pos)); b.set(pack.subarray(off + pos, off + pos + m), bp); pos += m; got += m; }
      vt[s * 4 + 2] = pos; if (out) dv().setUint32(out, got, true); return 0;
    },
    lithosys_fd_close(fd) {
      if (fd <= 2) return 0;
      const s = volGet(fd); if (s < 0) return LITHOSYS.EBADF;
      Atomics.store(vt, s * 4, 0); return 0;
    },
    lithosys_fd_seek(fd, off, whence, out) {
      if (fd <= 2) return LITHOSYS.ESPIPE;
      const s = volGet(fd); if (s < 0) return LITHOSYS.EBADF;
      const len = dir.get(fileIdx[vt[s * 4 + 1]])[1];
      const base = whence === 0 ? 0 : whence === 1 ? vt[s * 4 + 2] >>> 0 : whence === 2 ? len : NaN;
      const np = base + Number(off); if (!(np >= 0)) return LITHOSYS.EINVAL;
      vt[s * 4 + 2] = np; dv().setBigUint64(out, BigInt(np), true); return 0;
    },
    lithosys_fd_sync(fd) { // stdio: the console is unbuffered at this layer; pack fds have nothing to sync
      if (fd === 1 || fd === 2) { if (lines[fd]) { t.onConsole(lines[fd], fd === 2); lines[fd] = ''; } return 0; }
      if (fd === 0 || volGet(fd) >= 0) return 0;
      return LITHOSYS.EBADF;
    },
    lithosys_volume_open(p, out) {
      const k = cstr(p).replace(/^\/+/, ''); const fi = fileIdx.indexOf(k);
      if (fi < 0) return LITHOSYS.ENOENT;
      for (let s = 0; s < VOL_SLOTS; s++) {
        if (Atomics.compareExchange(vt, s * 4, 0, 1) === 0) { vt[s * 4 + 1] = fi; vt[s * 4 + 2] = 0; vt[s * 4 + 3] = 0; dv().setUint32(out, VOL_FD0 + s, true); return 0; }
      }
      return LITHOSYS.EAGAIN;
    },
    lithosys_console_write(p, n) { t.onConsole(new TextDecoder().decode(u8().slice(p, p + n)).replace(/\n$/, ''), true); },
    lithosys_random_fill(p, n) {
      const tmp = new Uint8Array(n);
      for (let o = 0; o < n; o += 65536) globalThis.crypto.getRandomValues(tmp.subarray(o, Math.min(n, o + 65536)));
      u8().set(tmp, p); return 0;
    },
    lithosys_cpu_count() { return sh.mode === 'threaded' ? (sh.cpuCount || 1) : 1; },
    lithosys_exit(code) { throw new LkExit(code); },
    lithosys_abort() { throw new LkExit(134); },
  };
  // lk_store: the blocking channel in threaded mode, the direct store otherwise.
  if (store) for (const [k, f] of Object.entries(store)) env[k] = f;
  return env;
}

export function storeImports(sh, canBlock, singleStore) {
  if (sh.mode === 'threaded') return new StoreClient(sh.channel, canBlock).imports();
  const b = singleStore; const o = {};
  for (const k of Object.keys(Object.getPrototypeOf(b)).concat(Object.getOwnPropertyNames(Object.getPrototypeOf(b))))
    if (k.startsWith('lk_store_')) o[k] = (...a) => { const r = b[k](...a); return typeof r === 'number' ? r : -52; /* async backend: ENOSYS */ };
  return o;
}

function allocArgv(ex, mem, argv) {
  const enc = new TextEncoder(); const ptrs = [];
  for (const a of argv) { const b = enc.encode(a + '\0'); const p = ex.malloc(b.length); if (!p) throw new LkBootError('argv: malloc failed'); new Uint8Array(mem.buffer).set(b, p); ptrs.push(p); }
  const arr = ex.malloc(4 * (ptrs.length + 1)); const dv = new DataView(mem.buffer);
  ptrs.forEach((p, i) => dv.setUint32(arr + 4 * i, p, true)); dv.setUint32(arr + 4 * ptrs.length, 0, true);
  return arr;
}

// Boot. opts: { module, moduleSingle, mode, volume: Map, argv, budgetMiB, initialPages, maxPages,
//   providers(ctx) -> {lk_* imports}, onConsole, store: {backend in the store worker} | singleStore,
//   channel (SAB, threaded), startWorker(kind, data) -> Worker-like, cpuCount }
export async function lkBoot(opts) {
  const mode = opts.mode ?? detectMode();
  const module = mode === 'threaded' ? opts.module : (opts.moduleSingle ?? null);
  if (!module) throw new LkBootError(mode === 'single'
    ? 'single-thread fallback needs the unshared build (moduleSingle): this page is not crossOriginIsolated'
    : 'no module');
  const maxPages = Math.min(opts.maxPages ?? 9600, HARD_CAP_MIB * 16);
  const memory = opts.memory ?? new WebAssembly.Memory({ initial: opts.initialPages ?? 4096, maximum: maxPages, shared: mode === 'threaded' });
  const channel = mode === 'threaded' ? (opts.channel ?? makeChannel()) : null;
  const sh = makeShared({ mode, memory, volume: opts.volume, budgetMiB: opts.budgetMiB, channel, cpuCount: opts.cpuCount });
  const log = opts.onConsole ?? ((s, e) => (e ? console.error : console.log)(s));
  const t = { canBlock: opts.mainCanBlock ?? false, onConsole: log, onBudget: opts.onBudget };
  t.store = storeImports(sh, t.canBlock, opts.singleStore ?? new MemoryBackend(memory));
  const env = makeImports(sh, t);
  const pctx = { memory, exports: null };
  Object.assign(env, opts.providers ? opts.providers(pctx) : {});
  const missing = WebAssembly.Module.imports(module).filter(i => i.kind === 'function' && !(i.name in env)).map(i => i.name);
  for (const n of missing) env[n] = () => { throw new LkBootError('host does not provide ' + n); };
  const inst = new WebAssembly.Instance(module, { env });
  const ex = inst.exports;
  pctx.exports = ex;
  if (mode === 'threaded') {
    const need = ['__stack_pointer', '__tls_size', '__tls_align', '__wasm_init_tls', 'lithosys_thread_start', 'malloc', 'free'];
    const lack = need.filter(n => !(n in ex));
    if (lack.length || !opts.startWorker) t.spawn = null, log('lkboot: threads disabled (missing exports: ' + (lack.join(' ') || 'startWorker') + ')', true);
    else t.spawn = startArg => spawnThread(ex, sh, opts, startArg);
  }
  ex.__wasm_call_ctors();
  const status = new Int32Array(memory.buffer, ex.__lithoroot_crt_status.value, 1)[0];
  if (status !== 0) throw new LkBootError('crt1 status ' + status + (status === 2 ? ' (main thread attach failed)' : status === 1 ? ' (heap ok, weave not attached)' : ' (heap not installed)'));
  let code;
  try {
    if (ex.__main_argc_argv) { const argv = opts.argv ?? ['LithoKraft']; code = ex.__main_argc_argv(argv.length, allocArgv(ex, memory, argv)); }
    else if (ex.__main_void) code = ex.__main_void();
    else throw new LkBootError('module has no __main_argc_argv');
  } catch (e) {
    if (e instanceof LkExit) code = e.code;
    else if ((e && e.name === 'LkUnwind') || (opts.isUnwind && opts.isUnwind(e))) code = undefined; // main handed over to the frame loop
    else throw e;
  }
  return { code, unwound: code === undefined, mode, memory, exports: ex, missing, missingImports: missing };
}

// Spawn: runs on the spawning thread (its TLS is live, so malloc is
// safe here). One block holds the new thread's TLS at the bottom and
// its stack above it; the worker frees it when the thread is done.
const STACK_DEFAULT = 1 << 20;
export function spawnThread(ex, sh, opts, startArg) {
  const dv = new DataView(sh.memory.buffer);
  const tid = dv.getUint32(startArg, true);
  const stackReq = dv.getUint32(startArg + 4, true) || STACK_DEFAULT;
  const tlsSize = ex.__tls_size.value, tlsAlign = Math.max(16, ex.__tls_align.value);
  const total = tlsSize + tlsAlign + stackReq + 16;
  const block = ex.malloc(total);
  if (!block) return -LITHOSYS.EAGAIN;
  const tlsBase = (block + tlsAlign - 1) & ~(tlsAlign - 1);
  const stackTop = (block + total) & ~15;
  try {
    opts.startWorker('thread', { module: opts.module, memory: sh.memory, shared: sh, startArg, block, tlsBase, stackTop, tid });
  } catch { ex.free(block); return -LITHOSYS.EAGAIN; }
  return tid;
}
