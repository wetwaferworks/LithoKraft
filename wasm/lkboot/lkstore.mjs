// lkstore.mjs ... the host side of lk_store (engine/lithoroot/include/lk_store.h).
//
// Backends implement the 11 calls as async functions over a
// WebAssembly.Memory (pointers and lengths are wasm addresses):
//   OpfsBackend   Origin Private File System (browser, inside a worker)
//   MemoryBackend in-memory map, same behaviour as tests/lithovfs_mock.mjs
//
// Threaded mode (shared memory): a dedicated store worker runs
// StoreServer over one backend. Each engine thread owns one request
// slot in a SharedArrayBuffer, claimed once by CAS (no lock); a call
// writes its op and args into the slot, flips the state word and
// blocks in Atomics.wait until the server answers. The server reads
// and writes the wasm memory directly (it holds the shared memory),
// so file bytes are never copied through the channel. OPFS's async
// calls (getFileHandle, removeEntry, move) are awaited by the server
// only; the engine sees a plain synchronous call. Requests are served
// one at a time in arrival order. The page thread may not block, so a
// call from it returns -EAGAIN (LithoVFS is never driven from the
// page frame loop).
//
// OPFS sync access handles take an exclusive lock on their file while
// open: that is the OS lock on save files, the one lock JD's doctrine
// allows.
export const E = { EAGAIN: 6, EBADF: 8, EBUSY: 10, EEXIST: 20, EINVAL: 28, EIO: 29, EISDIR: 31, ENOENT: 44, ENOSPC: 51, ENOSYS: 52, ENOTDIR: 54, ENOTEMPTY: 55, EROFS: 69 };
export const OPS = ['lk_store_open', 'lk_store_read', 'lk_store_write', 'lk_store_truncate', 'lk_store_flush',
  'lk_store_close', 'lk_store_stat', 'lk_store_mkdir', 'lk_store_list', 'lk_store_remove', 'lk_store_rename'];
const td = new TextDecoder();
const parentOf = p => p.slice(0, p.lastIndexOf('/')) || '/';
const baseOf = p => p.slice(p.lastIndexOf('/') + 1);
const split = p => p.split('/').filter(Boolean);
const str = (mem, p, n) => td.decode(new Uint8Array(mem.buffer, p, n).slice());
const now = () => BigInt(Date.now()) * 1000000n;
function writeStat(mem, out, type, size, mtime) {
  const dv = new DataView(mem.buffer, out, 24);
  dv.setUint32(0, type, true); dv.setUint32(4, 0, true); dv.setBigUint64(8, BigInt(size), true); dv.setBigInt64(16, BigInt(mtime), true);
}
function writeList(mem, buf, cap, names) {
  const b = new TextEncoder().encode(names.map(s => s + '\0').join(''));
  if (b.length <= cap) new Uint8Array(mem.buffer, buf, b.length).set(b);
  return b.length;
}

export class OpfsBackend {
  constructor(root, mem) { this.root = root; this.mem = mem; this.handles = new Map(); this.nextH = 1; }
  async _dir(path, create = false) { let d = this.root; for (const p of split(path)) d = await d.getDirectoryHandle(p, { create }); return d; }
  async _entry(path) { // -> {kind, h} | null
    if (path === '/') return { kind: 'directory', h: this.root };
    let d; try { d = await this._dir(parentOf(path)); } catch { return null; }
    try { return { kind: 'file', h: await d.getFileHandle(baseOf(path)) }; } catch {}
    try { return { kind: 'directory', h: await d.getDirectoryHandle(baseOf(path)) }; } catch {}
    return null;
  }
  async lk_store_open(p, n, flags) {
    const path = str(this.mem, p, n); const e = await this._entry(path);
    if (e && e.kind === 'directory') return -E.EISDIR;
    if (e && (flags & 4) && (flags & 16)) return -E.EEXIST;
    let fh = e && e.h;
    if (!fh) {
      if (!(flags & 4)) return -E.ENOENT;
      let d; try { d = await this._dir(parentOf(path)); } catch { return -E.ENOENT; }
      fh = await d.getFileHandle(baseOf(path), { create: true });
    }
    let sah; try { sah = await fh.createSyncAccessHandle(); } catch { return -E.EBUSY; } // the file lock is held elsewhere
    if (flags & 8) sah.truncate(0);
    const h = this.nextH++; this.handles.set(h, { path, sah }); return h;
  }
  async lk_store_read(h, buf, len, off) { const o = this.handles.get(h); if (!o) return -E.EBADF;
    const tmp = new Uint8Array(len); const n = o.sah.read(tmp, { at: Number(off) }); new Uint8Array(this.mem.buffer, buf, n).set(tmp.subarray(0, n)); return n; }
  async lk_store_write(h, buf, len, off) { const o = this.handles.get(h); if (!o) return -E.EBADF;
    try { return o.sah.write(new Uint8Array(this.mem.buffer, buf, len).slice(), { at: Number(off) }); } catch (e) { return e && e.name === 'QuotaExceededError' ? -E.ENOSPC : -E.EIO; } }
  async lk_store_truncate(h, size) { const o = this.handles.get(h); if (!o) return -E.EBADF; o.sah.truncate(Number(size)); return 0; }
  async lk_store_flush(h) { const o = this.handles.get(h); if (!o) return -E.EBADF; o.sah.flush(); return 0; }
  async lk_store_close(h) { const o = this.handles.get(h); if (!o) return -E.EBADF; o.sah.close(); this.handles.delete(h); return 0; }
  async lk_store_stat(p, n, out) {
    const path = str(this.mem, p, n);
    for (const o of this.handles.values()) if (o.path === path) { writeStat(this.mem, out, 1, o.sah.getSize(), now()); return 0; }
    const e = await this._entry(path); if (!e) return -E.ENOENT;
    if (e.kind === 'directory') { writeStat(this.mem, out, 2, 0, 0); return 0; }
    const f = await e.h.getFile(); writeStat(this.mem, out, 1, f.size, BigInt(f.lastModified) * 1000000n); return 0;
  }
  async lk_store_mkdir(p, n) { const path = str(this.mem, p, n);
    if (await this._entry(path)) return -E.EEXIST;
    let d; try { d = await this._dir(parentOf(path)); } catch { return -E.ENOENT; }
    await d.getDirectoryHandle(baseOf(path), { create: true }); return 0; }
  async lk_store_list(p, n, buf, cap) { const path = str(this.mem, p, n);
    const e = await this._entry(path); if (!e || e.kind !== 'directory') return -E.ENOENT;
    const names = []; for await (const [name, h] of e.h.entries()) names.push(h.kind === 'directory' ? name + '/' : name);
    names.sort(); return writeList(this.mem, buf, cap, names); }
  async lk_store_remove(p, n) { const path = str(this.mem, p, n);
    for (const o of this.handles.values()) if (o.path === path) return -E.EBUSY;
    const e = await this._entry(path); if (!e || path === '/') return -E.ENOENT;
    const d = await this._dir(parentOf(path));
    try { await d.removeEntry(baseOf(path)); } catch (x) { return x && x.name === 'InvalidModificationError' ? -E.ENOTEMPTY : -E.EIO; }
    return 0; }
  async lk_store_rename(a, an, b, bn) { const from = str(this.mem, a, an), to = str(this.mem, b, bn);
    const e = await this._entry(from); if (!e) return -E.ENOENT;
    let dd; try { dd = await this._dir(parentOf(to)); } catch { return -E.ENOENT; }
    if (e.kind === 'file') { await e.h.move(dd, baseOf(to)); return 0; }
    // OPFS cannot move a directory: copy the subtree, then remove it.
    const copy = async (src, dstParent, name) => {
      const dst = await dstParent.getDirectoryHandle(name, { create: true });
      for await (const [nm, h] of src.entries()) {
        if (h.kind === 'directory') await copy(h, dst, nm);
        else await h.move(dst, nm);
      }
    };
    await copy(e.h, dd, baseOf(to));
    await (await this._dir(parentOf(from))).removeEntry(baseOf(from), { recursive: true });
    return 0; }
}

export class MemoryBackend {
  constructor(mem) { this.mem = mem; this.files = new Map(); this.dirs = new Set(['/']); this.handles = new Map(); this.nextH = 1; }
  // Seed read-only files from a parsed LKP2 pack (lkpack.mjs). The bytes stay in
  // the pack buffer (views, no copy) and outside wasm memory; writes, truncation,
  // removal and renames of pack files answer EROFS.
  seedPack(pk) {
    for (const d of pk.dirs) { let a = ''; for (const part of d.split('/').filter(Boolean)) { a += '/' + part; this.dirs.add(a); } }
    for (const f of pk.files) this.files.set(f.path, { data: pk.bytes.subarray(f.start, f.start + f.len), mtime: 0, ro: true });
    return pk.files.length;
  }
  lk_store_open(p, n, fl) { const k = str(this.mem, p, n);
    if (this.dirs.has(k)) return -E.EISDIR;
    if (this.files.get(k)?.ro && (fl & (2 | 8))) return -E.EROFS;
    if (!this.files.has(k)) { if (!(fl & 4) || !this.dirs.has(parentOf(k))) return -E.ENOENT; this.files.set(k, { data: new Uint8Array(0), mtime: now() }); }
    else if ((fl & 4) && (fl & 16)) return -E.EEXIST;
    if (fl & 8) { const f = this.files.get(k); f.data = new Uint8Array(0); f.buf = null; }
    this.handles.set(this.nextH, k); return this.nextH++; }
  lk_store_read(h, buf, len, off) { const k = this.handles.get(h); if (!k) return -E.EBADF;
    const d = this.files.get(k).data, o = Number(off); if (o >= d.length) return 0;
    const m = Math.min(len, d.length - o); new Uint8Array(this.mem.buffer, buf, m).set(d.subarray(o, o + m)); return m; }
  lk_store_write(h, buf, len, off) { const k = this.handles.get(h); if (!k) return -E.EBADF;
    const f = this.files.get(k), o = Number(off);
    if (f.ro) return -E.EROFS;
    if (o + len > f.data.length) { // geometric capacity: appends were O(n^2) (whole-file copy per write)
      const cap = f.buf ? f.buf.length : f.data.length;
      if (o + len > cap) { const nb = new Uint8Array(Math.max(o + len, cap * 2, 4096)); nb.set(f.data); f.buf = nb; }
      f.data = f.buf.subarray(0, o + len);
    }
    f.data.set(new Uint8Array(this.mem.buffer, buf, len).slice(), o); f.mtime = now(); return len; }
  lk_store_truncate(h, size) { const k = this.handles.get(h); if (!k) return -E.EBADF;
    const f = this.files.get(k); if (f.ro) return -E.EROFS;
    const nd = new Uint8Array(Number(size)); nd.set(f.data.subarray(0, Number(size))); f.data = nd; f.buf = null; return 0; }
  lk_store_flush(h) { return this.handles.has(h) ? 0 : -E.EBADF; }
  lk_store_close(h) { return this.handles.delete(h) ? 0 : -E.EBADF; }
  lk_store_stat(p, n, out) { const k = str(this.mem, p, n);
    if (this.dirs.has(k)) { writeStat(this.mem, out, 2, 0, 0); return 0; }
    const f = this.files.get(k); if (!f) return -E.ENOENT; writeStat(this.mem, out, 1, f.data.length, f.mtime); return 0; }
  lk_store_mkdir(p, n) { const k = str(this.mem, p, n);
    if (this.dirs.has(k) || this.files.has(k)) return -E.EEXIST; if (!this.dirs.has(parentOf(k))) return -E.ENOENT; this.dirs.add(k); return 0; }
  lk_store_list(p, n, buf, cap) { const k = str(this.mem, p, n); if (!this.dirs.has(k)) return -E.ENOENT;
    const pre = k === '/' ? '/' : k + '/', out = [];
    for (const d of this.dirs) if (d !== k && d.startsWith(pre) && !d.slice(pre.length).includes('/')) out.push(d.slice(pre.length) + '/');
    for (const f of this.files.keys()) if (f.startsWith(pre) && !f.slice(pre.length).includes('/')) out.push(f.slice(pre.length));
    return writeList(this.mem, buf, cap, out); }
  lk_store_remove(p, n) { const k = str(this.mem, p, n);
    if (this.files.get(k)?.ro) return -E.EROFS;
    if ([...this.handles.values()].includes(k)) return -E.EBUSY;
    if (this.files.delete(k)) return 0;
    if (this.dirs.has(k) && k !== '/') { for (const x of [...this.dirs, ...this.files.keys()]) if (x.startsWith(k + '/')) return -E.ENOTEMPTY; this.dirs.delete(k); return 0; }
    return -E.ENOENT; }
  lk_store_rename(a, an, b, bn) { const s = str(this.mem, a, an), d = str(this.mem, b, bn);
    if (!this.dirs.has(parentOf(d))) return -E.ENOENT;
    if (this.files.get(s)?.ro || this.files.get(d)?.ro) return -E.EROFS;
    if (this.files.has(s)) { this.files.set(d, this.files.get(s)); this.files.delete(s); return 0; }
    if (!this.dirs.has(s)) return -E.ENOENT;
    for (const x of [...this.dirs]) if (x === s || x.startsWith(s + '/')) { this.dirs.delete(x); this.dirs.add(d + x.slice(s.length)); }
    for (const [x, v] of [...this.files]) if (x.startsWith(s + '/')) { this.files.delete(x); this.files.set(d + x.slice(s.length), v); }
    return 0; }
}

// ---- the cross-thread channel -------------------------------------------------
// Slot layout (Int32, 16 words): [0] state 0 idle / 1 request / 2 answered,
// [1] op index, [2..9] args (i64 args as lo/hi pairs), [10] return value.
// Header word 0 of the SAB is the slot-claim counter base.
export const SLOT_WORDS = 16, SLOTS = 64, WAKE = 0;
export function makeChannel() { return new SharedArrayBuffer(4 * (SLOT_WORDS * (SLOTS + 1))); }
const ARGSPEC = { // which args are 64-bit
  lk_store_open: [0, 0, 0], lk_store_read: [0, 0, 0, 1], lk_store_write: [0, 0, 0, 1], lk_store_truncate: [0, 1],
  lk_store_flush: [0], lk_store_close: [0], lk_store_stat: [0, 0, 0], lk_store_mkdir: [0, 0], lk_store_list: [0, 0, 0, 0],
  lk_store_remove: [0, 0], lk_store_rename: [0, 0, 0, 0] };

export class StoreServer {
  constructor(backend, sab) { this.b = backend; this.i32 = new Int32Array(sab); this.running = true; }
  async run() { // serve until stop(); one request at a time, arrival order
    const i = this.i32;
    while (this.running) {
      const seen = Atomics.load(i, WAKE);
      let served = 0;
      for (let s = 1; s <= SLOTS; ++s) {
        const base = s * SLOT_WORDS;
        if (Atomics.load(i, base) !== 1) continue;
        const op = OPS[i[base + 1]], spec = ARGSPEC[op], args = [];
        for (let a = 0, w = base + 2; a < spec.length; ++a) {
          if (spec[a]) { args.push((BigInt(i[w + 1] >>> 0) << 32n) | BigInt(i[w] >>> 0)); w += 2; } else args.push(i[w++] >>> 0);
        }
        if (op === 'lk_store_open' || op === 'lk_store_read' || op === 'lk_store_write' || op === 'lk_store_truncate' || op === 'lk_store_flush' || op === 'lk_store_close') args[0] = args[0] | 0;
        let r; try { r = await this.b[op](...args); } catch { r = -E.EIO; }
        i[base + 10] = r | 0; Atomics.store(i, base, 2); Atomics.notify(i, base); ++served;
      }
      // Idle: block this worker until a client bumps WAKE. A plain wait,
      // not waitAsync: V8 does not deliver a waitAsync wake-up while the
      // notifying thread sits blocked in Atomics.wait (seen in Node 20).
      // The store worker has nothing else to do while idle; OPFS's own
      // promises run during the awaits above.
      if (!served) Atomics.wait(i, WAKE, seen);
    }
  }
  stop() { this.running = false; Atomics.add(this.i32, WAKE, 1); Atomics.notify(this.i32, WAKE); }
}

// One per engine thread. Synchronous: the call returns once answered.
export class StoreClient {
  constructor(sab, canBlock) {
    this.i32 = new Int32Array(sab); this.canBlock = canBlock; this.slot = 0;
  }
  _claim() { // CAS-free claim: each thread takes the next slot number (fetch-add)
    const n = Atomics.add(this.i32, 1, 1) + 1; // word 1 of the header: slots handed out
    if (n > SLOTS) return 0;
    this.slot = n; return n;
  }
  call(op, ...args) {
    if (!this.canBlock) return -E.EAGAIN;
    if (!this.slot && !this._claim()) return -E.EAGAIN;
    const i = this.i32, base = this.slot * SLOT_WORDS, spec = ARGSPEC[op];
    i[base + 1] = OPS.indexOf(op);
    for (let a = 0, w = base + 2; a < spec.length; ++a) {
      if (spec[a]) { const v = BigInt.asUintN(64, BigInt(args[a])); i[w] = Number(v & 0xffffffffn) | 0; i[w + 1] = Number(v >> 32n) | 0; w += 2; }
      else i[w++] = args[a] | 0;
    }
    Atomics.store(i, base, 1);
    Atomics.add(i, WAKE, 1); Atomics.notify(i, WAKE);
    while (Atomics.load(i, base) !== 2) Atomics.wait(i, base, 1);
    const r = i[base + 10]; Atomics.store(i, base, 0); return r;
  }
  imports() { const o = {}; for (const op of OPS) o[op] = (...a) => this.call(op, ...a); return o; }
}

// ---- read-only pack in front of a writable backend ----------------------------
// Paths the pack owns (its files and directories) are answered from the pack
// (MemoryBackend.seedPack, read-only: EROFS on any change); every other path goes
// to the writable backend (OPFS for saves). Handles are told apart by a high bit.
const PACK_H = 1 << 24;
export class PackOverlay {
  constructor(mem, pack, rw) { this.mem = mem; this.ro = new MemoryBackend(mem); this.ro.seedPack(pack); this.rw = rw; }
  owns(p, n) { const k = str(this.mem, p, n); return this.ro.files.has(k) || (k !== '/' && this.ro.dirs.has(k)); }
  lk_store_open(p, n, fl) {
    if (!this.owns(p, n)) return this.rw.lk_store_open(p, n, fl);
    const h = this.ro.lk_store_open(p, n, fl); return h < 0 ? h : h + PACK_H;
  }
  pick(h) { return h >= PACK_H ? [this.ro, h - PACK_H] : [this.rw, h]; }
  lk_store_read(h, b, l, o) { const [x, y] = this.pick(h); return x.lk_store_read(y, b, l, o); }
  lk_store_write(h, b, l, o) { const [x, y] = this.pick(h); return x.lk_store_write(y, b, l, o); }
  lk_store_truncate(h, s) { const [x, y] = this.pick(h); return x.lk_store_truncate(y, s); }
  lk_store_flush(h) { const [x, y] = this.pick(h); return x.lk_store_flush(y); }
  lk_store_close(h) { const [x, y] = this.pick(h); return x.lk_store_close(y); }
  lk_store_stat(p, n, out) { return this.owns(p, n) ? this.ro.lk_store_stat(p, n, out) : this.rw.lk_store_stat(p, n, out); }
  lk_store_mkdir(p, n) { return this.owns(p, n) ? -E.EEXIST : this.rw.lk_store_mkdir(p, n); }
  lk_store_list(p, n, b, c) { return this.owns(p, n) ? this.ro.lk_store_list(p, n, b, c) : this.rw.lk_store_list(p, n, b, c); }
  lk_store_remove(p, n) { return this.owns(p, n) ? -E.EROFS : this.rw.lk_store_remove(p, n); }
  lk_store_rename(a, an, b, bn) { return (this.owns(a, an) || this.owns(b, bn)) ? -E.EROFS : this.rw.lk_store_rename(a, an, b, bn); }
}

// ---- single-thread persistent memory (no SAB store worker) -----------------
// OPFS methods are async, but wasm imports must return synchronously. Hydrate
// a MemoryBackend at boot, serve the engine from it, and flush dirty files
// back to OPFS after each host frame (and on close/flush). The pack stays a
// read-only MemoryBackend in front via PackOverlay.
async function walkOpfs(dir, prefix, files, dirs) {
  dirs.add(prefix || '/');
  for await (const [name, h] of dir.entries()) {
    const path = (prefix === '/' ? '' : prefix) + '/' + name;
    if (h.kind === 'directory') await walkOpfs(h, path, files, dirs);
    else {
      const f = await h.getFile();
      files.set(path, { data: new Uint8Array(await f.arrayBuffer()), mtime: BigInt(f.lastModified) * 1000000n });
    }
  }
}

export class PersistentMemory extends MemoryBackend {
  constructor(mem, root) {
    super(mem);
    this.root = root;
    this.dirty = new Set();
    this.removed = new Set();
  }
  static async open(mem) {
    if (!(globalThis.navigator && navigator.storage && navigator.storage.getDirectory)) {
      return new MemoryBackend(mem);
    }
    const root = await navigator.storage.getDirectory();
    const p = new PersistentMemory(mem, root);
    const files = new Map(), dirs = new Set();
    await walkOpfs(root, '/', files, dirs);
    for (const d of dirs) p.dirs.add(d);
    for (const [k, v] of files) p.files.set(k, v);
    return p;
  }
  _mark(k) { if (k) { this.dirty.add(k); this.removed.delete(k); } }
  lk_store_open(p, n, fl) {
    const k = str(this.mem, p, n);
    const r = super.lk_store_open(p, n, fl);
    if (r >= 0 && (fl & (4 | 8))) this._mark(k);
    return r;
  }
  lk_store_write(h, buf, len, off) {
    const r = super.lk_store_write(h, buf, len, off);
    if (r >= 0) this._mark(this.handles.get(h));
    return r;
  }
  lk_store_truncate(h, size) {
    const r = super.lk_store_truncate(h, size);
    if (r >= 0) this._mark(this.handles.get(h));
    return r;
  }
  lk_store_mkdir(p, n) {
    const r = super.lk_store_mkdir(p, n);
    if (r >= 0) this._mark(str(this.mem, p, n));
    return r;
  }
  lk_store_remove(p, n) {
    const k = str(this.mem, p, n);
    const r = super.lk_store_remove(p, n);
    if (r >= 0) { this.dirty.delete(k); this.removed.add(k); }
    return r;
  }
  lk_store_rename(a, an, b, bn) {
    const src = str(this.mem, a, an), dst = str(this.mem, b, bn);
    const r = super.lk_store_rename(a, an, b, bn);
    if (r >= 0) {
      this.removed.add(src);
      this.dirty.delete(src);
      this._mark(dst);
      for (const k of [...this.dirty]) if (k.startsWith(src + '/')) { this.dirty.delete(k); this.dirty.add(dst + k.slice(src.length)); }
    }
    return r;
  }
  // Snapshot the dirty set synchronously (bytes copied now), then write to OPFS
  // asynchronously, so the engine may keep mutating files while the flush runs.
  // A failed write puts its path back in the dirty set for the next flush.
  async persist() {
    if (!this.root || (this.dirty.size === 0 && this.removed.size === 0)) return; // cheap no-op on clean frames
    const removed = [...this.removed]; this.removed.clear();
    const dirs = [], files = [];
    for (const k of this.dirty) {
      if (this.dirs.has(k)) { if (k !== '/') dirs.push(k); continue; }
      const f = this.files.get(k);
      if (f && !f.ro) files.push([k, f.data.slice()]);
    }
    this.dirty.clear();
    const mkdir = async path => { let d = this.root; for (const p of split(path)) d = await d.getDirectoryHandle(p, { create: true }); return d; };
    for (const k of removed) {
      if (this.files.has(k) || this.dirs.has(k)) continue; // re-created since: the write below wins
      try {
        let x = this.root; for (const p of split(parentOf(k))) x = await x.getDirectoryHandle(p);
        await x.removeEntry(baseOf(k), { recursive: true });
      } catch {}
    }
    for (const k of dirs) { try { await mkdir(k); } catch {} }
    for (const [k, bytes] of files) {
      try {
        const d = await mkdir(parentOf(k));
        const fh = await d.getFileHandle(baseOf(k), { create: true });
        const w = await fh.createWritable();
        await w.write(bytes);
        await w.close();
      } catch (e) { this.dirty.add(k); console.log('lkstore persist failed ' + k + ': ' + e); }
    }
  }
}
