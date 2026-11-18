// lkgl.mjs: the engine's GL imports (lk_gl*, lkio_present, lk_mega_*) over one
// WebGL2 context on an OffscreenCanvas owned by the engine's main worker.
// Hand-written; the old wasm/full/lkboot.js was the behaviour reference only.
// The engine speaks integer GL names; this file keeps the name <-> object maps.
//
// makeGL(ctx, canvas, opts) -> { imports, stats }
//   ctx: { memory, exports } (exports filled in after instantiation)
//   canvas: an OffscreenCanvas (or HTMLCanvasElement) with getContext('webgl2')

const POWER = ['default', 'low-power', 'high-performance'];
const TYPE_SIZE = { 0x1400: 1, 0x1401: 1, 0x1402: 2, 0x1403: 2, 0x1404: 4, 0x1405: 4, 0x1406: 4, 0x140B: 2,
  0x8033: 2, 0x8034: 2, 0x8363: 2, 0x8368: 4, 0x84FA: 4, 0x8C3B: 4, 0x8C3E: 4, 0x8DAD: 8 };
const PACKED = new Set([0x8033, 0x8034, 0x8363, 0x8368, 0x84FA, 0x8C3B, 0x8C3E, 0x8DAD]);
const COMPONENTS = { 0x1903: 1, 0x8227: 2, 0x1907: 3, 0x1908: 4, 0x8D94: 1, 0x8228: 2, 0x8D98: 3, 0x8D99: 4,
  0x1902: 1, 0x84F9: 1, 0x1909: 1, 0x190A: 2, 0x1906: 1 };
const KINDS = ['buffer', 'texture', 'program', 'shader', 'vao'];

export function makeGL(ctx, canvas, opts = {}) {
  const log = opts.onError ?? (s => console.error(s));
  const stats = { calls: 0, draws: 0, contexts: 0, glErrors: 0, gaps: {} };
  const reg = {}; for (const k of KINDS) reg[k] = { next: 1, map: new Map(), rev: new Map() };
  const alloc = (k, o) => { const r = reg[k], n = r.next++; r.map.set(n, o); r.rev.set(o, n); return n; };
  const get = (k, n) => (n ? reg[k].map.get(n) ?? null : null);
  const drop = (k, n) => { const r = reg[k], o = r.map.get(n); if (o !== undefined) r.rev.delete(o); r.map.delete(n); return o; };
  const nameOf = o => { for (const k of KINDS) { const n = reg[k].rev.get(o); if (n) return n; } return 0; };
  const locs = new Map(), locById = new Map(); let nextLoc = 1;
  const strs = new Map();
  const contexts = new Map(); let nextHandle = 1;
  let gl = null;
  let unpackAlign = 4, packAlign = 4, unpackBuf = false, packBuf = false;
  // Software present frame boundary (2026-11-18). While main() runs without returning to the event
  // loop (the web bench's direct frame loop), the OffscreenCanvas never commits a frame, and the GPU
  // process (ANGLE/SwiftShader) kept every lkio_present texture upload alive: about 1 GB/s at the
  // software bench's frame rate, until the browser ran out of memory and the run stalled (single and
  // threaded alike; glFlush/glFinish do not release it). 'auto' (default) ends the frame with
  // transferToImageBitmap() after each present only while opts.inMain() says main() is still running;
  // the unwound rAF loop commits normally and is untouched. Test pages: ?swsync=auto|bitmap|flush|finish|none.
  const presentSync = opts.presentSync || 'auto';
  const inMain = typeof opts.inMain === 'function' ? opts.inMain : () => false;
  const frameBoundary = () => { if (canvas && typeof canvas.transferToImageBitmap === 'function') { try { const b = canvas.transferToImageBitmap(); if (b && b.close) b.close(); stats.boundaries = (stats.boundaries | 0) + 1; } catch (e) { stats.boundaryErr = String(e && e.message || e); } } };
  const buf = () => ctx.memory.buffer;
  const u8 = () => new Uint8Array(buf()), i32 = () => new Int32Array(buf()), u32 = () => new Uint32Array(buf()), f32 = () => new Float32Array(buf());
  const dec = new TextDecoder();
  const cstr = (p, len = -1) => { const m = u8(); let e = p; if (len >= 0) e = p + len; else while (m[e]) e++; return dec.decode(m.slice(p, e)); };
  const enc = new TextEncoder();
  const writeStr = (p, s, cap) => { if (!p || cap <= 0) return 0; const b = enc.encode(s).subarray(0, cap - 1); u8().set(b, p); u8()[p + b.length] = 0; return b.length; };
  const staticStr = s => { let p = strs.get(s); if (p === undefined) { const b = enc.encode(s); p = ctx.exports.malloc(b.length + 1); u8().set(b, p); u8()[p + b.length] = 0; strs.set(s, p); } return p; };
  const gap = name => () => { if (!stats.gaps[name]) log('lkgl: lk_gl' + name + ' has no WebGL2 equivalent; ignored'); stats.gaps[name] = (stats.gaps[name] | 0) + 1; return 0; };
  // Pixel view for (w, h, d) of format/type at ptr, honouring the row alignment.
  const pixView = (ptr, w, h, d, format, type, align) => {
    const ts = TYPE_SIZE[type] ?? 1;
    const px = PACKED.has(type) ? ts : ts * (COMPONENTS[format] ?? 4);
    const row = Math.ceil((w * px) / align) * align;
    const bytes = Math.max(0, row * (h * d - 1) + w * px);
    const n = Math.max(0, Math.min(bytes, buf().byteLength - ptr));
    if (ts === 1) return new Uint8Array(buf(), ptr, n);
    if (type === 0x1406) return new Float32Array(buf(), ptr, n >> 2);
    if (ts === 2) return (type === 0x1402 ? new Int16Array(buf(), ptr, n >> 1) : new Uint16Array(buf(), ptr, n >> 1));
    return (type === 0x1404 ? new Int32Array(buf(), ptr, n >> 2) : new Uint32Array(buf(), ptr, n >> 2));
  };
  const gen = (k, make) => (n, p) => { const m = u32(); for (let i = 0; i < n; i++) m[(p >> 2) + i] = alloc(k, make()); };
  const del = (k, kill) => (n, p) => { const m = u32(); for (let i = 0; i < n; i++) { const o = drop(k, m[(p >> 2) + i]); if (o) kill(o); } };
  const loc = id => (id > 0 ? locById.get(id)?.loc ?? null : null);
  const writeParam = (pname, p, kind) => {
    let v = gl.getParameter(pname);
    if (v && typeof v === 'object' && !ArrayBuffer.isView(v) && !Array.isArray(v)) v = nameOf(v);
    if (v === null || v === undefined) v = 0;
    const one = (at, x) => { if (kind === 'f') f32()[at >> 2] = +x; else i32()[at >> 2] = typeof x === 'boolean' ? +x : x | 0; };
    if (typeof v === 'number' || typeof v === 'boolean') one(p, v); else for (let i = 0; i < v.length; i++) one(p + 4 * i, v[i]);
  };

  const G = {
    gl_create_context(attrsPtr) {
      const m = u8(), w = i32();
      if (m[attrsPtr + 25]) return 0; // explicit swap control: never requested
      let g = null;
      try {
        g = canvas.getContext('webgl2', { alpha: !!m[attrsPtr], depth: !!m[attrsPtr + 1], stencil: !!m[attrsPtr + 2],
          antialias: !!m[attrsPtr + 3], premultipliedAlpha: !!m[attrsPtr + 4], preserveDrawingBuffer: !!m[attrsPtr + 5],
          powerPreference: POWER[w[(attrsPtr + 8) >> 2]] ?? 'default', failIfMajorPerformanceCaveat: !!m[attrsPtr + 12],
          desynchronized: !!m[attrsPtr + 33] });
      } catch (e) { log('lkgl: getContext threw ' + e); }
      if (!g) return 0;
      const h = nextHandle++; contexts.set(h, g); stats.contexts++;
      return h;
    },
    gl_make_current(h) { const g = contexts.get(h); if (!g) return -4; gl = g; return 0; },
    glClear: m => gl.clear(m),
    glClearColor: (r, g, b, a) => gl.clearColor(r, g, b, a),
    glEnable: c => gl.enable(c),
    glDisable: c => gl.disable(c),
    glGetError: () => { const e = gl ? gl.getError() : 0; if (e) stats.glErrors++; return e; },
    glViewport: (x, y, w, h) => gl.viewport(x, y, w, h),
    glBlendFunc: (s, d) => gl.blendFunc(s, d),
    glCullFace: m => gl.cullFace(m),
    glFrontFace: m => gl.frontFace(m),
    glDepthFunc: f => gl.depthFunc(f),
    glDepthMask: f => gl.depthMask(!!f),
    glPixelStorei: (p, v) => { if (p === 0x0CF5) unpackAlign = v; if (p === 0x0D05) packAlign = v; gl.pixelStorei(p, v); },
    // textures
    glGenTextures: gen('texture', () => gl.createTexture()),
    glDeleteTextures: del('texture', o => gl.deleteTexture(o)),
    glBindTexture: (t, n) => gl.bindTexture(t, get('texture', n)),
    glActiveTexture: u => gl.activeTexture(u),
    glTexParameteri: (t, p, v) => gl.texParameteri(t, p, v),
    glTexParameterf: (t, p, v) => gl.texParameterf(t, p, v),
    glGenerateMipmap: t => gl.generateMipmap(t),
    glTexImage2D: (t, l, fi, w, h, b, f, ty, p) => unpackBuf ? gl.texImage2D(t, l, fi, w, h, b, f, ty, p)
      : gl.texImage2D(t, l, fi, w, h, b, f, ty, p ? pixView(p, w, h, 1, f, ty, unpackAlign) : null),
    glTexSubImage2D: (t, l, x, y, w, h, f, ty, p) => unpackBuf ? gl.texSubImage2D(t, l, x, y, w, h, f, ty, p)
      : gl.texSubImage2D(t, l, x, y, w, h, f, ty, pixView(p, w, h, 1, f, ty, unpackAlign)),
    glTexImage3D: (t, l, fi, w, h, d, b, f, ty, p) => unpackBuf ? gl.texImage3D(t, l, fi, w, h, d, b, f, ty, p)
      : gl.texImage3D(t, l, fi, w, h, d, b, f, ty, p ? pixView(p, w, h, d, f, ty, unpackAlign) : null),
    glTexSubImage3D: (t, l, x, y, z, w, h, d, f, ty, p) => unpackBuf ? gl.texSubImage3D(t, l, x, y, z, w, h, d, f, ty, p)
      : gl.texSubImage3D(t, l, x, y, z, w, h, d, f, ty, pixView(p, w, h, d, f, ty, unpackAlign)),
    glCopyTexSubImage2D: (t, l, xo, yo, x, y, w, h) => gl.copyTexSubImage2D(t, l, xo, yo, x, y, w, h),
    glTexBuffer: gap('TexBuffer'),
    glReadPixels: (x, y, w, h, f, ty, p) => packBuf ? gl.readPixels(x, y, w, h, f, ty, p) : gl.readPixels(x, y, w, h, f, ty, pixView(p, w, h, 1, f, ty, packAlign)),
    // buffers
    glGenBuffers: gen('buffer', () => gl.createBuffer()),
    glDeleteBuffers: del('buffer', o => gl.deleteBuffer(o)),
    glBindBuffer: (t, n) => { if (t === 0x88EC) unpackBuf = !!n; if (t === 0x88EB) packBuf = !!n; gl.bindBuffer(t, get('buffer', n)); },
    glBufferData: (t, size, p, usage) => p ? gl.bufferData(t, new Uint8Array(buf(), p, size), usage) : gl.bufferData(t, size, usage),
    glBufferSubData: (t, off, size, p) => gl.bufferSubData(t, off, new Uint8Array(buf(), p, size)),
    glCopyBufferSubData: (r, w, ro, wo, size) => gl.copyBufferSubData(r, w, ro, wo, size),
    // vertex arrays
    glGenVertexArrays: gen('vao', () => gl.createVertexArray()),
    glDeleteVertexArrays: del('vao', o => gl.deleteVertexArray(o)),
    glBindVertexArray: n => gl.bindVertexArray(get('vao', n)),
    glEnableVertexAttribArray: i => gl.enableVertexAttribArray(i),
    glVertexAttribPointer: (i, s, t, n, st, off) => gl.vertexAttribPointer(i, s, t, !!n, st, off),
    glVertexAttribIPointer: (i, s, t, st, off) => gl.vertexAttribIPointer(i, s, t, st, off),
    glVertexAttribDivisor: (i, d) => gl.vertexAttribDivisor(i, d),
    // draws
    glDrawArrays: (m, f, c) => { stats.draws++; if (bigDraw && c >= bigDraw) bigNote('glDrawArrays', [m, f, c]); gl.drawArrays(m, f, c); },
    glDrawElements: (m, c, t, off) => { stats.draws++; if (bigDraw && c >= bigDraw) bigNote('glDrawElements', [m, c, t, off]); gl.drawElements(m, c, t, off); },
    glDrawElementsBaseVertex: gap('DrawElementsBaseVertex'),
    // shaders and programs
    glCreateShader: t => { const o = gl.createShader(t); return o ? alloc('shader', o) : 0; },
    glShaderSource: (n, count, strPtr, lenPtr) => {
      const o = get('shader', n); if (!o) return;
      const m = u32(), l = i32(); let s = '';
      for (let i = 0; i < count; i++) s += cstr(m[(strPtr >> 2) + i], lenPtr ? l[(lenPtr >> 2) + i] : -1);
      gl.shaderSource(o, s);
    },
    glCompileShader: n => { const o = get('shader', n); if (o) gl.compileShader(o); },
    glGetShaderiv: (n, pn, p) => {
      const o = get('shader', n); const w = i32();
      if (!o) { w[p >> 2] = 0; return; }
      if (pn === 0x8B84) { w[p >> 2] = (gl.getShaderInfoLog(o) || '').length + 1; return; }
      const v = gl.getShaderParameter(o, pn); w[p >> 2] = typeof v === 'boolean' ? +v : v | 0;
    },
    glGetShaderInfoLog: (n, cap, lenPtr, p) => { const o = get('shader', n); const k = writeStr(p, o ? gl.getShaderInfoLog(o) || '' : '', cap); if (lenPtr) i32()[lenPtr >> 2] = k; },
    glDeleteShader: n => { const o = drop('shader', n); if (o) gl.deleteShader(o); },
    glCreateProgram: () => { const o = gl.createProgram(); return o ? alloc('program', o) : 0; },
    glAttachShader: (pr, sh) => { const a = get('program', pr), b = get('shader', sh); if (a && b) gl.attachShader(a, b); },
    glLinkProgram: n => { const o = get('program', n); if (o) { gl.linkProgram(o); const m = locs.get(n); if (m) { for (const e of m.values()) locById.delete(e.id); locs.delete(n); } } },
    glGetProgramiv: (n, pn, p) => {
      const o = get('program', n); const w = i32();
      if (!o) { w[p >> 2] = 0; return; }
      if (pn === 0x8B84) { w[p >> 2] = (gl.getProgramInfoLog(o) || '').length + 1; return; }
      const v = gl.getProgramParameter(o, pn); w[p >> 2] = typeof v === 'boolean' ? +v : v | 0;
    },
    glGetProgramInfoLog: (n, cap, lenPtr, p) => { const o = get('program', n); const k = writeStr(p, o ? gl.getProgramInfoLog(o) || '' : '', cap); if (lenPtr) i32()[lenPtr >> 2] = k; },
    glUseProgram: n => gl.useProgram(get('program', n)),
    glDeleteProgram: n => { const o = drop('program', n); if (o) gl.deleteProgram(o); const m = locs.get(n); if (m) { for (const e of m.values()) locById.delete(e.id); locs.delete(n); } },
    glGetUniformLocation: (n, namePtr) => {
      const o = get('program', n); if (!o) return -1;
      const name = cstr(namePtr);
      let m = locs.get(n); if (!m) locs.set(n, m = new Map());
      const hit = m.get(name); if (hit) return hit.id;
      const l = gl.getUniformLocation(o, name); if (!l) return -1;
      const e = { id: nextLoc++, loc: l }; m.set(name, e); locById.set(e.id, e); return e.id;
    },
    glUniform1i: (l, a) => gl.uniform1i(loc(l), a),
    glUniform1f: (l, a) => gl.uniform1f(loc(l), a),
    glUniform2f: (l, a, b) => gl.uniform2f(loc(l), a, b),
    glUniform3f: (l, a, b, c) => gl.uniform3f(loc(l), a, b, c),
    glUniform3fv: (l, n, p) => gl.uniform3fv(loc(l), new Float32Array(buf(), p, 3 * n)),
    glUniform4fv: (l, n, p) => gl.uniform4fv(loc(l), new Float32Array(buf(), p, 4 * n)),
    glUniformMatrix4fv: (l, n, tr, p) => gl.uniformMatrix4fv(loc(l), !!tr, new Float32Array(buf(), p, 16 * n)),
    // queries
    glGetIntegerv: (pn, p) => writeParam(pn, p, 'i'),
    glGetFloatv: (pn, p) => writeParam(pn, p, 'f'),
    glGetString: name => {
      if (!gl) return 0;
      const P = { 0x1F00: 'WebGL2', 0x1F01: 'WebGL2', 0x1F02: 'WebGL 2.0', 0x8B8C: 'WebGL GLSL ES 3.00' };
      if (name === 0x1F03) return staticStr((gl.getSupportedExtensions() || []).join(' '));
      if (!(name in P)) return 0;
      return staticStr(String(gl.getParameter(name) || P[name]));
    },
    glGetStringi: (name, i) => { if (!gl || name !== 0x1F03) return 0; const e = gl.getSupportedExtensions() || []; return i >= 0 && i < e.length ? staticStr(e[i]) : 0; },
  };

  // Present a CPU-rendered frame: upload texels and draw a fullscreen triangle.
  function lkio_present(target, tex, prog, vao, namePtr, ptr, w, h, fbW, fbH, internalFormat, format, fresh) {
    if (target !== 0 || !gl) return -1;
    const t = get('texture', tex), p = get('program', prog), v = get('vao', vao);
    if (!t || !p || !v) return -1;
    gl.viewport(0, 0, fbW, fbH); gl.disable(0x0B71); gl.disable(0x0BE2);
    gl.activeTexture(0x84C0); gl.bindTexture(0x0DE1, t);
    const want = w * h * 4, data = new Uint8Array(buf(), ptr, Math.max(0, Math.min(want, buf().byteLength - ptr)));
    if (fresh) gl.texImage2D(0x0DE1, 0, internalFormat, w, h, 0, format, 0x1401, data);
    else gl.texSubImage2D(0x0DE1, 0, 0, 0, w, h, format, 0x1401, data);
    const err = gl.getError(); if (err) return err;
    gl.useProgram(p);
    const l = gl.getUniformLocation(p, cstr(namePtr)); if (l) gl.uniform1i(l, 0);
    gl.bindVertexArray(v); gl.drawArrays(0x0004, 0, 3); stats.draws++;
    gl.bindVertexArray(null); gl.useProgram(null); gl.bindTexture(0x0DE1, null);
    if (presentSync === 'bitmap' || (presentSync === 'auto' && inMain())) frameBoundary();
    else if (presentSync === 'flush') gl.flush(); else if (presentSync === 'finish') gl.finish();
    return 0;
  }
  // Mega-draw (gl33_renderer.cpp): one instanced draw per record, re-pointing
  // attributes 4..8 at the record's slot of the instance buffer.
  const LAYOUT_OFF = [0, 12, 28, 44, 60];
  function lk_mega_draw(mode, countsPtr, offsetsPtr, n) {
    if (!gl) return -1; if (!n) return 0;
    if (slowMs > 0 && glSync) gl.getError();  // ?glsync: drain earlier work so it is not billed to this call
    const tA = slowMs > 0 ? performance.now() : 0; let drawMs = 0;
    const m = i32(), counts = m.slice(countsPtr >> 2, (countsPtr >> 2) + n), offs = m.slice(offsetsPtr >> 2, (offsetsPtr >> 2) + n);
    const s = [];
    for (let a = 4; a <= 8; a++) s.push({ b: gl.getVertexAttrib(a, 0x889F), size: gl.getVertexAttrib(a, 0x8623), type: gl.getVertexAttrib(a, 0x8625),
      norm: gl.getVertexAttrib(a, 0x886A), stride: gl.getVertexAttrib(a, 0x8624), off: LAYOUT_OFF[a - 4] });
    const stride = s[0].stride; if (!stride) return -1;
    const prev = gl.getParameter(0x8894);
    const tB = slowMs > 0 ? performance.now() : 0;
    gl.bindBuffer(0x8892, s[0].b);
    for (let i = 0; i < n; i++) {
      for (let a = 4; a <= 8; a++) { const q = s[a - 4]; gl.vertexAttribPointer(a, q.size, q.type, q.norm, q.stride, q.off + i * stride); }
      const t0 = slowMs > 0 ? performance.now() : 0;
      if (bigDraw && counts[i] >= bigDraw) bigNote('lk_mega_draw[record ' + i + '/' + n + ']', [counts[i], offs[i]]);
      gl.drawElementsInstanced(mode, counts[i], 0x1405, offs[i], 1); stats.draws++;
      if (slowMs > 0) {
        if (glSync) gl.getError();  // ?glsync: force a round trip so each record's GPU cost lands on that record
        const ms = performance.now() - t0; drawMs += ms;
        if (ms >= slowMs) {  // diagnosis: which record, its index range and its instance data
          const inst = new Float32Array(stride >> 2);
          try { gl.getBufferSubData(0x8892, i * stride, inst); } catch (e) {}
          const eab = gl.getParameter(0x8895);
          const ebSize = eab ? (gl.bindBuffer(0x8893, eab), gl.getBufferParameter(0x8893, 0x8764)) : -1;
          const vbSize = (() => { const vb = gl.getVertexAttrib(0, 0x889F); if (!vb) return -1; gl.bindBuffer(0x8892, vb); const z = gl.getBufferParameter(0x8892, 0x8764); gl.bindBuffer(0x8892, s[0].b); return z; })();
          onSlow('lk_mega_draw[record ' + i + '/' + n + ']', ms, [counts[i], offs[i], 'err=' + gl.getError(), 'ebo=' + ebSize, 'vbo=' + vbSize,
            'counts=' + Array.from(counts).join('/'), 'offs=' + Array.from(offs).join('/'), 'inst=' + Array.from(inst).map(v => +v.toFixed(3)).join('/')]);
        }
      }
    }
    for (let a = 4; a <= 8; a++) { const q = s[a - 4]; gl.vertexAttribPointer(a, q.size, q.type, q.norm, q.stride, q.off); }
    gl.bindBuffer(0x8892, prev);
    if (slowMs > 0) {  // diagnosis: split a slow call into the state queries, the draws and the rest
      const tE = performance.now();
      if (tE - tA >= slowMs) onSlow('lk_mega_draw[sections]', tE - tA, ['query=' + (tB - tA).toFixed(1), 'draws=' + drawMs.toFixed(1),
        'other=' + (tE - tA - (tB - tA) - drawMs).toFixed(1), 'sync=' + glSync, 'counts=' + Array.from(counts).join('/'), 'offs=' + Array.from(offs).join('/')]);
    }
    return 0;
  }

  // Diagnosis (opts.slowMs > 0, test pages ?glslow=N): time every GL import; a call that takes
  // slowMs or more is reported through opts.onSlow(name, ms, args). Off by default (no wrapper cost).
  const slowMs = +opts.slowMs || 0, glSync = !!opts.glSync;
  // Diagnosis (opts.bigDraw > 0, test pages ?glbig=N): report draws of N or more vertices/indices, with the
  // bound program and buffer sizes (at most 40 reports).
  const bigDraw = +opts.bigDraw || 0; let bigLeft = 40;
  function bigNote(name, a) {
    if (bigLeft-- <= 0) return;
    const p = gl.getParameter(0x8B8D), e = gl.getParameter(0x8895);
    const ebo = e ? (gl.bindBuffer(0x8893, e), gl.getBufferParameter(0x8893, 0x8764)) : -1;
    (opts.onSlow ?? (() => {}))('big ' + name, 0, [...a, 'prog=' + nameOf(p), 'ebo=' + ebo, 'vao=' + nameOf(gl.getParameter(0x85B5))]);
  }
  const onSlow = opts.onSlow ?? ((n, ms, a) => log('lkgl slow: ' + n + ' ' + ms.toFixed(1) + ' ms'));
  stats.glMs = 0; stats.slow = 0;
  const RING = 4000, ring = new Array(RING); let ringAt = 0;
  const HEAVY = /BufferData|SubData|CopyBuffer|TexImage|TexSubImage|TexStorage|Delete|ReadPixels|CompileShader|LinkProgram|GenerateMipmap|Map|Flush|Finish/, heavy = new Array(60); let heavyAt = 0;
  const timed = (name, fn) => (slowMs > 0 ? (...a) => {
    ring[ringAt] = name + '(' + a.slice(0, 5).join(',') + ')'; ringAt = (ringAt + 1) % RING;
    if (HEAVY.test(name) && !(/SubData$/.test(name) && a[2] < 65536)) { heavy[heavyAt] = performance.now().toFixed(0) + ' ' + ring[(ringAt + RING - 1) % RING]; heavyAt = (heavyAt + 1) % 60; }
    const t0 = performance.now(); const r = fn(...a); if (glSync && gl) gl.getError(); const ms = performance.now() - t0;
    stats.glMs += ms;
    if (ms >= slowMs) {
      stats.slow++; onSlow(name, ms, a);
      // and the calls that led up to it: a histogram of the last RING calls plus the last 60 verbatim
      const last = [], hist = new Map();
      for (let k = 0; k < RING; k++) { const s = ring[(ringAt + k) % RING]; if (!s) continue; last.push(s); const nm = s.slice(0, s.indexOf('(')); hist.set(nm, (hist.get(nm) || 0) + 1); }
      onSlow('  before (histogram of last ' + last.length + ')', ms, [[...hist].sort((x, y) => y[1] - x[1]).map(([k, v]) => k + 'x' + v).join(' ')]);
      for (let k = Math.max(0, last.length - 60); k < last.length; k++) onSlow('  before[' + (k - last.length) + '] ' + last[k], 0, []);
      onSlow('  heavy calls (t ms, now ' + t0.toFixed(0) + ')', 0, []);
      for (let k = 0; k < 60; k++) { const s = heavy[(heavyAt + k) % 60]; if (s) onSlow('  heavy ' + s, 0, []); }
    }
    return r;
  } : fn);
  const imports = {};
  for (const [k, fn] of Object.entries(G)) {
    const name = 'lk_' + k;
    imports[name] = timed(name, k.startsWith('gl_') ? fn : (...a) => { stats.calls++; return gl ? fn(...a) : 0; });
  }
  imports.lkio_present = timed('lkio_present', lkio_present);
  // Walk-freeze fix (2026-11-17): the mega-draw is OFF on the web unless asked for (opts.mega, test pages ?mega).
  // WebGL2 has no multi-draw here, so this shim re-points instance attributes 4..8 and issues one
  // drawElementsInstanced per record. In headless Chromium (ANGLE/SwiftShader), that froze the GPU process for 48-84 s
  // about 53 s into --bench-path walk, every run, in both coop and single modes. With the shim off (arena still on), the same walk
  // has no freeze (worst frame 46 ms) and runs faster (avg 1274 vs ~600 fps). The engine falls back to its
  // per-mesh arena draws when this returns 0.
  imports.lk_mega_probe = () => (gl && opts.mega && typeof gl.drawElementsInstanced === 'function' ? 1 : 0);
  imports.lk_mega_draw = timed('lk_mega_draw', lk_mega_draw);
  return { imports, stats, get gl() { return gl; } };
}
