// lkhost.mjs: the engine's non-GL page services (frame loop, canvas size, title,
// input ring and queue, pointer lock, clock, flags), for an engine main that runs
// in a worker. The page side (lkpage.mjs) owns the DOM: it forwards input events
// here and carries out title and pointer-lock requests posted from here.
// Wire formats match the old wasm/full/lkboot.js (ring records, queue lines).

// main() hands over to the frame loop by unwinding (its stack is abandoned, as with
// the old runtime). The unwind must not run C++ cleanups: main owns the frame state
// through a unique_ptr. A thrown JS value would be caught by wasm catch_all cleanup
// pads (wasm EH), so the host unwinds with a real trap instead (an `unreachable` in
// a 34-byte helper module), which wasm code cannot catch. state.unwinding tells the
// boot code the trap was this hand-over, not a crash.
const TRAP = new WebAssembly.Instance(new WebAssembly.Module(new Uint8Array([0, 97, 115, 109, 1, 0, 0, 0,
  1, 4, 1, 96, 0, 0, 3, 2, 1, 0, 7, 5, 1, 1, 116, 0, 0, 10, 5, 1, 3, 0, 0, 11]))).exports.t;
export const isUnwindTrap = (e, host) => !!(host && host.state.unwinding && e instanceof WebAssembly.RuntimeError);

export class LkUnwind extends Error { constructor() { super('lkboot: main unwound into the frame loop'); this.name = 'LkUnwind'; } }

// makeHost(ctx, { canvas, post, search, onError }) -> { imports, onPageMessage, frame }
export function makeHost(ctx, o) {
  const q = new URLSearchParams(o.search ?? '');
  const post = o.post ?? (() => {});
  const S = { loop: null, ringOn: false, ringBase: 0, ringCap: 0, ringWords: 0, css: [o.canvas?.width ?? 0, o.canvas?.height ?? 0],
    queue: [], dropped: 0, frames: 0, title: '', captureWanted: 0 };
  const enc = new TextEncoder(), dec = new TextDecoder();
  const cstr = p => { const m = new Uint8Array(ctx.memory.buffer); let e = p; while (m[e]) e++; return dec.decode(m.slice(p, e)); };
  const tsF = new Float64Array(1), tsU = new Uint32Array(tsF.buffer);

  // Ring writer (same record layout as the page writer it replaces).
  function emit(type, flags, i1, f1, f2, f3, f4, s1, s2) {
    if (!S.ringOn) return false;
    const b = ctx.memory.buffer, v = new Int32Array(b, S.ringBase, S.ringWords);
    const hv = new Float32Array(b, S.ringBase + 16, 2); hv[0] = S.css[0]; hv[1] = S.css[1];
    v[6] = o.canvas.width | 0; v[7] = o.canvas.height | 0;
    if (type === 0) return true;
    const w = Atomics.load(v, 1), r = Atomics.load(v, 2);
    if (w - r >= S.ringCap) { Atomics.add(v, 3, 1); return true; }
    const base = 8 + (w % S.ringCap) * 20;
    v[base + 1] = flags | 0; v[base + 2] = i1 | 0; v[base + 3] = 0;
    const fv = new Float32Array(b, S.ringBase + (base + 4) * 4, 4); fv[0] = f1; fv[1] = f2; fv[2] = f3; fv[3] = f4;
    const u8 = new Uint8Array(b, S.ringBase + (base + 8) * 4, 32); u8.fill(0);
    if (s1) u8.set(enc.encode(s1).subarray(0, 15), 0);
    if (s2) u8.set(enc.encode(s2).subarray(0, 15), 16);
    tsF[0] = performance.now(); v[base + 16] = tsU[0] | 0; v[base + 17] = tsU[1] | 0; v[base + 18] = 0; v[base + 19] = 0;
    Atomics.store(v, base, type); Atomics.store(v, 1, w + 1);
    return true;
  }
  const E = encodeURIComponent;
  const legacy = (t, f, i1, f1, f2, f3, f4, s1, s2) => ({
    1: () => `kd|${E(s1)}|${E(s2 || '')}|${f & 1 ? 1 : 0}|${f & 2 ? 1 : 0}|${f & 4 ? 1 : 0}`, 2: () => `ku|${E(s1)}|${E(s2 || '')}`,
    3: () => `md|${i1}|${f1}|${f2}`, 4: () => `mu|${i1}|${f1}|${f2}`, 5: () => `mm|${f1}|${f2}|${f3}|${f4}`, 6: () => `wh|${f1}|${i1}`,
    7: () => `plc|${f}`, 8: () => 'ple', 9: () => 'fo', 10: () => 'bl' }[t]?.());
  function onPageMessage(d) {
    if (d.css) S.css = d.css;
    if (d.ev) { const e = d.ev; if (!emit(...e) && e[0] !== 0) { const l = legacy(...e); if (l) S.queue.push(l); } }
  }
  const imports = {
    lk_host_is_node: () => 0,
    lk_now_ms: () => performance.now(),
    lk_frame_loop_set(cb, arg, fps, simulateInfinite) { S.loop = { cb, arg }; if (simulateInfinite) { S.unwinding = true; TRAP(); } },
    lk_frame_loop_cancel() { S.loop = null; },
    lk_canvas_set_size(w, h) { o.canvas.width = w | 0; o.canvas.height = h | 0; post({ lkPage: 'size', w, h }); },
    lk_canvas_get_size(wp, hp) { const m = new Int32Array(ctx.memory.buffer); m[wp >> 2] = o.canvas.width | 0; m[hp >> 2] = o.canvas.height | 0; return 0; },
    lk_window_set_title(p) { S.title = cstr(p); post({ lkPage: 'title', s: S.title }); },
    lk_pointer_lock_request() { post({ lkPage: 'lock' }); },
    lk_pointer_lock_exit() { post({ lkPage: 'unlock' }); },
    lk_input_set_capture_wanted(on) { S.captureWanted = on; post({ lkPage: 'capture', on }); },
    lk_input_install() { post({ lkPage: 'input' }); },
    lk_input_ring_publish(base, cap, words) {
      if (!(ctx.memory.buffer instanceof SharedArrayBuffer)) return 0; // single mode: the queue carries input
      S.ringBase = base; S.ringCap = cap; S.ringWords = words; S.ringOn = true; return 1;
    },
    lk_input_drain_queue(bufPtr, cap) {
      const header = `sz|${S.css[0]}|${S.css[1]}|${o.canvas.width}|${o.canvas.height}`;
      const parts = [header]; let size = header.length, take = 0;
      while (take < S.queue.length) {
        const n = S.queue[take].length + 1;
        if (size + n + 1 > cap) { if (take > 0) break; S.dropped++; take++; continue; }
        parts.push(S.queue[take]); size += n; take++;
      }
      const bytes = enc.encode(parts.join('\n'));
      if (bytes.length + 1 > cap) return -1;
      S.queue.splice(0, take);
      const m = new Uint8Array(ctx.memory.buffer); m.set(bytes, bufPtr); m[bufPtr + bytes.length] = 0;
      return bytes.length;
    },
    lk_input_debug_flag: () => (q.has('debuginput') ? 1 : 0),
    lk_page_aa_mode: () => { const v = q.get('aa'); return v === 'on' ? 1 : v === '4' ? 4 : 0; },
    lk_present_thin_ok: () => (q.get('lkthin') === 'off' ? 0 : 1),
  };
  // One frame: call the engine's registered callback through the function table.
  function frame() {
    if (!S.loop) return false;
    const t = ctx.exports.__indirect_function_table;
    t.get(S.loop.cb)(S.loop.arg); S.frames++;
    return true;
  }
  return { imports, onPageMessage, frame, state: S };
}
