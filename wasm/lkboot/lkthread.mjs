// lkthread.mjs ... worker entry for one LithoWeave thread (and the store
// worker). Message / workerData: { kind: 'thread' | 'store', ... }.
// Thread sequence: instantiate the module over the shared memory, set
// __stack_pointer to the block lkboot allocated, __wasm_init_tls(tls),
// lithosys_thread_start(start_arg), free the block, then the frozen
// tid-cell protocol: Atomics.store(cell, 0) and Atomics.notify(cell).
import { makeImports, storeImports, spawnThread, LkExit } from './lkboot.mjs';
import { StoreServer, MemoryBackend, OpfsBackend } from './lkstore.mjs';

export async function runThread(d, post = () => {}) {
  if (globalThis.process?.env?.LKB_DEBUG) (await import('node:fs')).writeSync(2, 'thread start ' + d.tid + '\n');
  const t = { canBlock: true, onConsole: (s, e) => post({ console: s, err: e }) };
  t.store = storeImports(d.shared, true);
  const env = makeImports(d.shared, t);
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

export async function runStore(d) {
  const backend = d.backend === 'opfs' ? new OpfsBackend(await navigator.storage.getDirectory(), d.memory) : new MemoryBackend(d.memory);
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
  const w = pool.shift() ?? new Worker(url, { type: 'module' }); w.postMessage(msg); return w;
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
  self.onmessage = e => { const d = e.data; const post = m => self.postMessage(m); if (d.lkKind === 'ping') post({ lkReady: true }); else if (d.lkKind === 'thread') runThread(d, post); else if (d.lkKind === 'main') runMain(d, post); else runStore(d); };
}
