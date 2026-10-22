// probeh2.js — H2 regression probe (bug-fix run, 2026-10-20).
//
// The GL ring's async write path claimed space by loading writeIdx,
// computing a slot, writing the record, and storing writeIdx back — no
// CAS. Two producer threads running that protocol concurrently claim
// the SAME slot and overwrite each other's records (silent stream
// corruption). This probe stresses the REAL lkGLStreamCall/lkGLDrain
// from the lkboot.js under test: two producer workers blast patterned
// async records into a small ring while a third worker drains and
// validates every record's pattern and the total count.
//
// Usage: node probeh2.js [lkboot.js] [recordsPerProducer]
// Exit 0 = PASS (all records intact), 1 = FAIL.
const { Worker } = require('worker_threads');
const path = require('path');

const lkbootPath = path.resolve(process.argv[2] || '../full/lkboot.js');
const perProducer = parseInt(process.argv[3] || '3000', 10);
const PRODUCERS = 2;

const memory = new WebAssembly.Memory({ initial: 1024, maximum: 1024, shared: true });
const RING_BASE = 32 * 1024 * 1024;   // 32 MiB into the shared memory
const RING_SIZE = 256 * 1024;         // small on purpose: wraps + contention
new Uint8Array(memory.buffer).fill(0, RING_BASE, RING_BASE + 64 + RING_SIZE);
// go-flag word lives one page BELOW the ring base (scratch space the
// stream never touches — the ring's data region starts at base+64).
const GO_INDEX = (RING_BASE >> 2) - 1024;
const goView = new Int32Array(memory.buffer);

function spawn(role, extra) {
  const w = new Worker(path.join(__dirname, 'probeh2-worker.js'));
  w.postMessage({ cmd: 'init', lkbootPath, memory, ringBase: RING_BASE, ringSize: RING_SIZE, role, ...extra });
  return w;
}

async function main() {
  const drain = spawn('drain', { expected: perProducer * PRODUCERS });
  const producers = [];
  const results = { produced: 0, drained: null };
  const done = new Promise((resolve) => {
    let readyCount = 0;
    const onMsg = (m) => {
      if (m.cmd === 'ready') {
        readyCount++;
        if (readyCount === PRODUCERS) {
          // release the producers simultaneously
          Atomics.store(goView, GO_INDEX, 1);
          Atomics.notify(goView, GO_INDEX);
        }
      } else if (m.cmd === 'produced') {
        results.produced += m.sent;
      } else if (m.cmd === 'drained' || m.cmd === 'drained-timeout') {
        results.drained = m;
        resolve();
      }
    };
    drain.on('message', onMsg);
    for (let i = 0; i < PRODUCERS; i++) {
      const p = spawn('producer', { pid: i, records: perProducer });
      p.on('message', onMsg);
      producers.push(p);
    }
  });
  const timeout = new Promise((resolve) => setTimeout(() => resolve('timeout'), 90000));
  const which = await Promise.race([done.then(() => 'done'), timeout]);
  drain.terminate(); producers.forEach((p) => p.terminate());

  const d = results.drained || { executed: 0, corrupt: 0, samples: [] };
  const expected = perProducer * PRODUCERS;
  console.log('produced:', results.produced, '/', expected);
  console.log('executed:', d.executed, ' corrupt:', d.corrupt,
    d.samples && d.samples.length ? ' samples: ' + d.samples.join(' | ') : '');
  const pass = which === 'done' && d.executed === expected && d.corrupt === 0;
  console.log(pass ? 'PROBEH2: PASS' : 'PROBEH2: FAIL');
  process.exit(pass ? 0 : 1);
}
main().catch((e) => { console.error('PROBEH2: ERROR', e); process.exit(1); });
