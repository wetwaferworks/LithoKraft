// lkthread.mjs ... worker entry for one LithoWeave thread (and the store
// worker). Message / workerData: { kind: 'thread' | 'store', ... }.
// Thread sequence: instantiate the module over the shared memory, set
// __stack_pointer to the block lkboot allocated, __wasm_init_tls(tls),
// lithosys_thread_start(start_arg), free the block, then the frozen
// tid-cell protocol: Atomics.store(cell, 0) and Atomics.notify(cell).
import { makeImports, storeImports, spawnThread, threadHost, LkExit } from './lkboot.mjs';
import { StoreServer, MemoryBackend, OpfsBackend, PackOverlay } from './lkstore.mjs';

export async function runThread(d, post = () => {}) {
  if (globalThis.process?.env?.LKB_DEBUG) (await import('node:fs')).writeSync(2, 'thread start ' + d.tid + '\n');
  const t = { canBlock: true, onConsole: (s, e) => post({ console: s, err: e }) };
  t.store = storeImports(d.shared, true);
  const env = makeImports(d.shared, t);
  Object.assign(env, threadHost(d.shared));
  for (const i of WebAssembly.Module.imports(d.module)) if (i.kind === 'function' && !(i.name in env)) env[i.name] = () => { throw new Error('host does not provide ' + i.name); };
  t.spawn = null; // nested spawns go through the same path:
  const inst = new WebAssembly.Instance(d.module, { env });
  const ex = inst.exports;
  // A thread may spawn threads: same path as the main thread, from here.
  t.spawn = startArg => spawnThread(ex, d.shared, { module: d.module, startWorker }, startArg);
  ex.__stack_pointer.value = d.stackTop;
  ex.__wasm_init_tls(d.tlsBase);
  if (globalThis.process?.env?.LKB_DEBUG) (await import('node:fs')).writeSync(2, 'thread run ' + d.tid + ' sp=' + d.stackTop + ' tls=' + d.tlsBase + '\n');
  try { ex.lithosys_thread_start(d.startArg); }
  catch (e) { if (!(e instanceof LkExit)) post({ error: String(e && e.stack || e) }); else post({ exit: e.code }); }
  ex.free(d.block);
  const cell = new Int32Array(d.memory.buffer, d.startArg, 1);
  Atomics.store(cell, 0, 0);
  Atomics.notify(cell, 0);
  if (globalThis.process?.env?.LKB_DEBUG) (await import('node:fs')).writeSync(2, 'thread done ' + d.tid + '\n');
  post({ done: d.tid });
}

// The engine's main thread in a worker (the page thread may not block).
export async function runMain(d, post) {
  const { lkBoot } = await import('./lkboot.mjs');
  const module = d.module ?? new WebAssembly.Module(await (await fetch(d.url)).arrayBuffer());
  const log = [];
  await prewarm(d.pool ?? 0);
  try {
    const r = await lkBoot({ module, mode: 'threaded', memory: d.memory, channel: d.channel, mainCanBlock: true,
      cpuCount: d.cpuCount, startWorker, onConsole: s => log.push(s), argv: d.argv, volume: d.volume });
    post({ mainDone: r.code, log });
  } catch (e) { post({ mainError: String(e && e.message || e), log }); }
}

// LithoKraft in a worker with an OffscreenCanvas: boot, then drive the frame
// loop that main() registered. Posts { kraft: stats } about once a second.
export async function runKraft(d, post) {
  const { lkBoot } = await import('./lkboot.mjs');
  const { makeGL } = await import('./lkgl.mjs');
  const { makeHost, isUnwindTrap } = await import('./lkhost.mjs');
  const errors = [], log = [];
  const ilog = [];
  globalThis.__lkThreadLog = m => { if (m.error) errors.push('thread: ' + m.error); else if (m.console) onConsole(m.console, m.err); };
  const onConsole = (s, e) => { (e ? errors : log).push(s); if (log.length > 400) log.shift(); if (/input|screen|world|menu/i.test(s) && ilog.length < 200) ilog.push(s); };
  const module = new WebAssembly.Module(await (await fetch(d.url)).arrayBuffer());
  let gl, host;
  const providers = ctx => {
    gl = makeGL(ctx, d.canvas, { onError: s => errors.push(s) });
    host = makeHost(ctx, { canvas: d.canvas, search: d.search, post: m => post(m) });
    return { ...gl.imports, ...host.imports };
  };
  self.addEventListener('message', e => { if (e.data && e.data.lkInput) host && host.onPageMessage(e.data); });
  await prewarm(d.pool ?? 0);
  const t0 = performance.now();
  let r;
  try {
    r = await lkBoot({ module, mode: 'threaded', memory: d.memory, channel: d.channel, mainCanBlock: true, cpuCount: d.cpuCount,
      startWorker, onConsole, providers, argv: d.argv, isUnwind: e => isUnwindTrap(e, host) });
  } catch (e) { post({ kraft: { fatal: String(e && e.stack || e), errors, log: log.slice(-40) } }); return; }
  const report = extra => post({ kraft: { unwound: r.unwound, code: r.code, frames: host.state.frames, draws: gl.stats.draws,
    glCalls: gl.stats.calls, glErrors: gl.stats.glErrors, gaps: gl.stats.gaps, contexts: gl.stats.contexts, title: host.state.title, events: host.state.events, ringWrites: host.state.ringWrites, queued: host.state.queue.length, ringOn: host.state.ringOn, ringHdr: host.state.ringOn ? Array.from(new Int32Array(d.memory.buffer, host.state.ringBase, 4)) : null, threadsLeft: pool.length, memMiB: Math.round(d.memory.buffer.byteLength / 1048576),
    ms: Math.round(performance.now() - t0), errors: errors.slice(0, 20), errorCount: errors.length, log: log.slice(-25), ilog, ...extra } });
  if (!r.unwound) { report({ exited: true }); return; }
  const raf = self.requestAnimationFrame ? self.requestAnimationFrame.bind(self) : f => setTimeout(f, 16);
  let last = 0;
  const tick = () => {
    try { if (!host.frame()) { report({ loopCancelled: true }); return; } }
    catch (e) { errors.push('frame threw: ' + (e && e.stack || e)); report({ frameThrew: true }); return; }
    const now = performance.now(); if (now - last > 1000) { last = now; report({}); }
    raf(tick);
  };
  raf(tick);
}

export async function runStore(d, post = () => {}) {
  let backend = d.backend === 'opfs' ? new OpfsBackend(await navigator.storage.getDirectory(), d.memory) : new MemoryBackend(d.memory);
  let packFiles = 0;
  if (d.packUrl) { // the read-only asset pack (LKP2), fetched once, kept outside wasm memory
    const { parseLKP2 } = await import('./lkpack.mjs');
    try {
      const pk = parseLKP2(await (await fetch(d.packUrl)).arrayBuffer());
      if (backend instanceof MemoryBackend) packFiles = backend.seedPack(pk);
      else { backend = new PackOverlay(d.memory, pk, backend); packFiles = pk.files.length; }
    } catch (e) { post({ storeReady: false, error: String(e && e.message || e) }); return; }
  }
  post({ storeReady: true, packFiles });
  const srv = new StoreServer(backend, d.channel);
  globalThis.__lkStoreServer = srv;
  await srv.run();
}

const nodeWT = (typeof process !== 'undefined' && process.versions?.node) ? await import('node:worker_threads') : null;

// Start a worker of either kind (browser or Node).
export function startWorker(kind, data) {
  const url = new URL('./lkthread.mjs', import.meta.url);
  const msg = { ...data, lkKind: kind };
  if (nodeWT) { // synchronous: the spawner may block in lw_join right after
    const w = new nodeWT.Worker(url, { workerData: msg }); w.unref?.(); globalThis.__lkWorkers?.push(w);
    w.on('message', m => { if (m.error || m.console) globalThis.__lkThreadLog?.(m); });
    return w;
  }
  // Browser: a Worker created while its creator is blocked never starts, so take a
  // pre-started one from the pool (see prewarm) and fall back to a fresh one.
  const w = pool.shift() ?? new Worker(url, { type: 'module' });
  w.onmessage = e => { if (e.data && (e.data.error || e.data.console)) globalThis.__lkThreadLog?.(e.data); };
  w.postMessage(msg); return w;
}

// Browser worker pool: start n workers and wait until each one is listening, before
// the engine can block. Each worker serves one thread. No-op under Node.
const pool = [];
export async function prewarm(n) {
  if (nodeWT || typeof Worker === 'undefined' || !(n > 0)) return;
  const url = new URL('./lkthread.mjs', import.meta.url);
  const ready = [];
  for (let i = 0; i < n; i++) {
    const w = new Worker(url, { type: 'module' });
    ready.push(new Promise(res => {
      const f = e => { if (e.data && e.data.lkReady) { w.removeEventListener('message', f); res(w); } };
      w.addEventListener('message', f); w.postMessage({ lkKind: 'ping' });
    }));
  }
  pool.push(...await Promise.all(ready));
}

// Worker bootstrap: browser (postMessage) or Node (worker_threads).
if (nodeWT) {
  const wt = nodeWT;
  if (!wt.isMainThread && wt.workerData?.lkKind) {
    const d = wt.workerData; const fsx = await import('node:fs');
    const post = m => { if (m.error || (m.console && process.env.LKB_DEBUG)) fsx.writeSync(2, 'lkthread: ' + (m.error || m.console) + '\n'); wt.parentPort.postMessage(m); };
    if (d.lkKind === 'ping') post({ lkReady: true }); else if (d.lkKind === 'thread') runThread(d, post); else runStore(d);
  }
} else if (typeof WorkerGlobalScope !== 'undefined') {
  self.onmessage = e => { const d = e.data; const post = m => self.postMessage(m); if (d.lkKind === 'ping') post({ lkReady: true }); else if (d.lkKind === 'thread') runThread(d, post); else if (d.lkKind === 'main') runMain(d, post); else if (d.lkKind === 'kraft') runKraft(d, post); else if (d.lkKind === 'store') runStore(d, post); };
}
