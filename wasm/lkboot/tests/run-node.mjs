// run-node.mjs ... lkboot under Node: threaded boot with the store worker,
// the single-thread fallback, the real LithoKraft module, and negatives.
// usage: node run-node.mjs <thread.wasm> <single.wasm> [LithoKraft.wasm]
import fs from 'node:fs';
import { Worker } from 'node:worker_threads';
import { lkBoot, detectMode, clampBudgetMiB, LkBootError } from '../lkboot.mjs';
import { makeChannel, StoreClient, MemoryBackend, E } from '../lkstore.mjs';
import { startWorker } from '../lkthread.mjs';

const results = []; let fail = 0;
const check = (name, ok, extra = '') => { results.push((ok ? 'PASS ' : 'FAIL ') + name + (extra ? ' ' + extra : '')); if (!ok) fail++; };
const [threadWasm, singleWasm, krafWasm] = process.argv.slice(2);
const log = []; globalThis.__lkThreadLog = m => log.push('T: ' + (m.error || m.console));
const onConsole = (s, e) => log.push((e ? 'E: ' : '') + s);

// 1. threaded
{
  const module = new WebAssembly.Module(fs.readFileSync(threadWasm));
  const memory = new WebAssembly.Memory({ initial: 4096, maximum: 9600, shared: true });
  const channel = makeChannel();
  const store = new Worker(new URL('../lkthread.mjs', import.meta.url), { workerData: { lkKind: 'store', backend: 'memory', memory, channel } });
  const workers = []; globalThis.__lkWorkers = workers;
  const t0 = performance.now();
  const r = await lkBoot({ module, mode: 'threaded', memory, channel, mainCanBlock: true, onConsole, cpuCount: 4,
    startWorker: (k, d) => startWorker(k, d) });
  check('threaded boot: 4 threads, per-thread stack+TLS, cross-thread free, lk_store from a worker, join', r.code === 0, `rc=${r.code} ${(performance.now() - t0).toFixed(0)} ms`);
  // page-thread rule: a client that may not block gets EAGAIN without touching the channel
  check('negative: lk_store from a thread that may not block -> EAGAIN', new StoreClient(channel, false).call('lk_store_flush', 1) === -E.EAGAIN);
  // a bad handle through the real channel comes back as EBADF
  check('negative: lk_store_flush(bad handle) over the channel -> EBADF', new StoreClient(channel, true).call('lk_store_flush', 999) === -E.EBADF);
  await store.terminate();
}

// 2. single-thread fallback
{
  const moduleSingle = new WebAssembly.Module(fs.readFileSync(singleWasm));
  let hit500 = 0, hit450 = 0;
  const r = await lkBoot({ moduleSingle, mode: 'single', onConsole, budgetMiB: 500, onBudget: () => hit500++ });
  check('single fallback: unshared memory, spawn EAGAIN, heap+stdio+store work, 300 MiB fits a 500 MiB budget', r.code === 0 && !(r.memory.buffer instanceof SharedArrayBuffer) && hit500 === 0, `rc=${r.code} pages=${r.memory.buffer.byteLength / 65536}`);
  const r2 = await lkBoot({ moduleSingle, mode: 'single', onConsole, budgetMiB: 450, onBudget: () => hit450++ });
  check('negative: the same 300 MiB under a 450 MiB budget is refused (ENOMEM, onBudget)', r2.code === 4 && hit450 > 0 && r2.memory.buffer.byteLength <= 450 * 1048576, `rc=${r2.code} pages=${r2.memory.buffer.byteLength / 65536}`);
  let threw = null;
  try { await lkBoot({ mode: 'single', module: moduleSingle }); } catch (e) { threw = e; }
  check('negative: single mode without the unshared build stops with a clear error', threw instanceof LkBootError);
}

// 3. mode detection and budget clamp
check('detect: SAB + crossOriginIsolated -> threaded', detectMode({ SharedArrayBuffer, crossOriginIsolated: true }) === 'threaded');
check('detect: not crossOriginIsolated -> single', detectMode({ SharedArrayBuffer, crossOriginIsolated: false }) === 'single');
check('detect: no SharedArrayBuffer -> single', detectMode({ crossOriginIsolated: true }) === 'single');
check('budget clamp 450..500', clampBudgetMiB(300) === 450 && clampBudgetMiB(700) === 500 && clampBudgetMiB() === 480);

// 4. the real module: start sequence up to the first host call lkboot does not own
if (krafWasm) {
  const module = new WebAssembly.Module(fs.readFileSync(krafWasm));
  let err = null, r = null;
  try { r = await lkBoot({ module, mode: 'threaded', mainCanBlock: true, onConsole, cpuCount: 4, argv: ['LithoKraft', '--no-save'] }); } catch (e) { err = e; }
  const first = err && /host does not provide (\S+)/.exec(err.message);
  check('LithoKraft: ctors + crt status 0, main runs until the first provider call', !!first || (r && r.code === 0), first ? 'first unprovided import: ' + first[1] : (err ? String(err).slice(0, 120) : 'rc=' + r.code));
}
console.log(results.join('\n'));
console.log('console:', JSON.stringify(log.slice(0, 12)));
console.log(fail ? `LKBOOT NODE: FAIL (${fail})` : 'LKBOOT NODE: PASS');
process.exit(fail ? 1 : 0);
