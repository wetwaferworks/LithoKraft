// probeh2-worker.js — worker side of the H2 ring-claim stress probe.
// Loaded by probeh2.js via worker_threads. Each worker loads the REAL
// lkboot.js (path in the init message) over a shared WebAssembly.Memory
// provided by the parent, then either produces async GL stream records
// with a self-checking argument pattern (role 'producer') or drains and
// validates them (role 'drain').
const fs = require('fs');
const vm = require('vm');
const { parentPort } = require('worker_threads');

let boot = null; // { sandbox }
parentPort.on('message', (msg) => {
  if (msg.cmd === 'init') {
    const sandbox = {
      console, performance, TextDecoder, TextEncoder,
      setTimeout, clearTimeout, setInterval, clearInterval,
      crypto: require('node:crypto').webcrypto,
      importScripts: () => { throw new Error('probe: no importScripts'); },
    };
    const selfObj = {
      location: { href: 'http://probe/lkboot.js' },
      postMessage: () => {},
      _onmessage: null,
    };
    Object.defineProperty(selfObj, 'onmessage', {
      set(fn) { this._onmessage = fn; },
      get() { return this._onmessage; },
    });
    sandbox.self = selfObj;
    vm.createContext(sandbox);
    vm.runInContext(fs.readFileSync(msg.lkbootPath, 'utf8'), sandbox,
      { filename: msg.lkbootPath });
    sandbox.__mem = msg.memory;
    sandbox.__ringBase = msg.ringBase;
    sandbox.__ringSize = msg.ringSize;
    vm.runInContext(
      'LK.memory = __mem; LK.glRing = { base: __ringBase, size: __ringSize };',
      sandbox);
    boot = { sandbox };
    parentPort.postMessage({ cmd: 'ready', role: msg.role });
    if (msg.role === 'drain') startDrain(msg);
    else startProducer(msg);
  }
});

function startProducer(msg) {
  const { sandbox } = boot;
  const pid = msg.pid;
  const total = msg.records;
  const funcIdx = vm.runInContext(
    "LK_GL_NAMES.indexOf('emscripten_glUniform4iv')", sandbox);
  sandbox.__go = msg.goView;
  // Record pattern: [magic, seq, magic^0x5A5A, seq] — a torn record
  // (interleaved bytes from two producers' claims) breaks the pattern.
  const magic = 0xA500 + pid;
  vm.runInContext(`
    (() => {
      const i32 = new Int32Array(__mem.buffer);
      const GO = (__ringBase >> 2) - 1024; // scratch word below the ring
      while (Atomics.load(i32, GO) === 0) {
        Atomics.wait(i32, GO, 0, 1);
      }
    })();
  `, sandbox);
  const code = `
    (() => {
      let sent = 0;
      for (let seq = 1; seq <= ${total}; seq++) {
        lkGLStreamCall(${funcIdx}, false, [${magic}, seq, ${magic} ^ 0x5A5A, seq]);
        sent++;
      }
      return sent;
    })();
  `;
  const sent = vm.runInContext(code, sandbox);
  parentPort.postMessage({ cmd: 'produced', pid, sent });
}

function startDrain(msg) {
  const { sandbox } = boot;
  const expected = msg.expected;
  // Fake out the executor: validate each decoded record's pattern and
  // count. lkGLExecute is a top-level function declaration in lkboot.js;
  // replacing it here swaps what lkGLDrain dispatches to.
  sandbox.__stats = { executed: 0, corrupt: 0, samples: [] };
  vm.runInContext(`
    LK.isWorker = false;   // lkGLDrain refuses to run in a "worker"
    LK.exports = {};       // drain gate: LK.exports must exist
    lkGLExecute = (funcIdx, args) => {
      const st = __stats;
      st.executed++;
      const ok = args.length === 4 &&
        (args[0] === 0xA500 || args[0] === 0xA501 || args[0] === 0xA502) &&
        args[1] === args[3] &&
        (args[2] === (args[0] ^ 0x5A5A));
      if (!ok) {
        st.corrupt++;
        if (st.samples.length < 5) st.samples.push(JSON.stringify(args));
      }
      return 0;
    };
  `, sandbox);
  const timer = setInterval(() => {
    vm.runInContext('lkGLDrain()', sandbox);
    const st = sandbox.__stats;
    if (st.executed + st.corrupt >= expected) {
      clearInterval(timer);
      parentPort.postMessage({ cmd: 'drained', ...st });
    }
  }, 1);
  setTimeout(() => {
    clearInterval(timer);
    const st = sandbox.__stats;
    parentPort.postMessage({ cmd: 'drained-timeout', ...st });
  }, 60000);
}
