// Unit checks for lkhost.mjs (page services for a worker main). node tests/host_check.mjs
import { makeHost, isUnwindTrap } from '../lkhost.mjs';
let fails = 0; const check = (n, ok, d = '') => { console.log((ok ? 'PASS ' : 'FAIL ') + n + (d ? ' ' + d : '')); if (!ok) fails++; };
const canvas = { width: 640, height: 360 };
const posted = [];
const mk = shared => { const memory = new WebAssembly.Memory({ initial: 2, maximum: 4, shared }); const ctx = { memory, exports: null }; return { ctx, h: makeHost(ctx, { canvas, post: m => posted.push(m), search: '?aa=4&lkthin=off&debuginput' }) }; };
const rd = (mem, p) => { const m = new Uint8Array(mem.buffer); let e = p; while (m[e]) e++; return Buffer.from(m.slice(p, e)).toString(); };
{
  const { ctx, h } = mk(true);
  check('flags from the page query', h.imports.lk_page_aa_mode() === 4 && h.imports.lk_present_thin_ok() === 0 && h.imports.lk_input_debug_flag() === 1);
  // legacy queue: events before the ring is live go to the queue
  h.onPageMessage({ css: [320, 180], ev: [1, 2, 0, 0, 0, 0, 0, 'KeyW', 'w'] });
  h.onPageMessage({ ev: [3, 0, 0, 10, 20, 0, 0, null, null] });
  const n = h.imports.lk_input_drain_queue(1024, 256);
  check('drain: sz header + queued records', n > 0 && rd(ctx.memory, 1024) === 'sz|320|180|640|360\nkd|KeyW|w|0|1|0\nmd|0|10|20', JSON.stringify(rd(ctx.memory, 1024)));
  // negative: a record that does not fit stays queued (B3) and the drain says -1 or header only
  h.onPageMessage({ ev: [1, 0, 0, 0, 0, 0, 0, 'K'.repeat(60), ''] });
  const small = h.imports.lk_input_drain_queue(2048, 24);
  check('negative: too-small buffer keeps nothing it cannot fit', small === 'sz|320|180|640|360'.length && h.state.dropped === 1, `n=${small} dropped=${h.state.dropped}`);
  // ring: publish, then an event lands as a record with type, flags, i1
  check('ring publish on shared memory', h.imports.lk_input_ring_publish(4096, 8, 8 + 8 * 20) === 1);
  h.onPageMessage({ ev: [4, 0, 2, 5, 6, 0, 0, null, null] });
  const v = new Int32Array(ctx.memory.buffer, 4096, 8 + 8 * 20);
  check('ring record written and published', Atomics.load(v, 1) === 1 && v[8] === 4 && v[8 + 2] === 2 && v[6] === 640 && v[7] === 360);
  // negative: a full ring counts drops instead of overwriting
  for (let i = 0; i < 12; i++) h.onPageMessage({ ev: [5, 0, 0, 1, 1, 1, 1, null, null] });
  check('negative: full ring counts drops', Atomics.load(v, 1) === 8 && Atomics.load(v, 3) === 5, `w=${v[1]} drops=${v[3]}`);
  // proxies
  new Uint8Array(ctx.memory.buffer).set(Buffer.from('LithoKraft\0'), 8000);
  h.imports.lk_window_set_title(8000); h.imports.lk_pointer_lock_request(); h.imports.lk_input_set_capture_wanted(1);
  check('title / pointer lock / capture go to the page', posted.some(m => m.lkPage === 'title' && m.s === 'LithoKraft') && posted.some(m => m.lkPage === 'lock') && posted.some(m => m.lkPage === 'capture' && m.on === 1));
  let threw = null; try { h.imports.lk_frame_loop_set(7, 8, 0, 1); } catch (e) { threw = e; }
  check('frame loop registration unwinds main with a trap', threw instanceof WebAssembly.RuntimeError && isUnwindTrap(threw, h) && h.state.loop.cb === 7);
  check('negative: an ordinary trap is not taken for the hand-over', !isUnwindTrap(new WebAssembly.RuntimeError('x'), { state: {} }));
  ctx.exports = { __indirect_function_table: { get: i => a => { h.state.lastCall = [i, a]; } } };
  check('frame() calls the registered callback', h.frame() && h.state.lastCall[0] === 7 && h.state.lastCall[1] === 8 && h.state.frames === 1);
  h.imports.lk_frame_loop_cancel();
  check('negative: cancelled loop runs no frame', h.frame() === false);
}
{
  const { h } = mk(false);
  check('negative: ring refused on unshared memory (single mode uses the queue)', h.imports.lk_input_ring_publish(4096, 8, 168) === 0);
}
console.log(fails ? 'HOST CHECK: FAIL' : 'HOST CHECK: PASS'); process.exit(fails ? 1 : 0);
