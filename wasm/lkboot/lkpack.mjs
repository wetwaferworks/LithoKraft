// lkpack.mjs: reader for the LKP2 asset pack (wasm/lkpack.py writes it).
// Untrusted input: every count, length and offset is bounds-checked, and any
// inconsistency rejects the whole pack (LkPackError) rather than loading part of it.
//   "LKP2" | u32le H | dirs: u16 n, (u16 len, utf8)* | files: u32 n,
//   (u16 dir, u16 len, utf8, varint startDelta, varint length)* | payload
export class LkPackError extends Error {}
const MAX_FILES = 1 << 20, MAX_DIRS = 1 << 16;
export function parseLKP2(buf) {
  const u8 = buf instanceof Uint8Array ? buf : new Uint8Array(buf);
  const bad = m => { throw new LkPackError('LKP2: ' + m); };
  if (u8.length < 8) bad('too short');
  if (u8[0] !== 0x4C || u8[1] !== 0x4B || u8[2] !== 0x50 || u8[3] !== 0x32) bad('bad magic');
  const dv = new DataView(u8.buffer, u8.byteOffset, u8.byteLength);
  const H = dv.getUint32(4, true);
  if (H > u8.length - 8) bad('header length past end');
  const end = 8 + H, payload = end, dec = new TextDecoder('utf-8', { fatal: true });
  let p = 8;
  const need = n => { if (p + n > end) bad('header truncated at ' + p); };
  const u16 = () => { need(2); const v = dv.getUint16(p, true); p += 2; return v; };
  const u32 = () => { need(4); const v = dv.getUint32(p, true); p += 4; return v; };
  const str = () => { const n = u16(); need(n); let s; try { s = dec.decode(u8.subarray(p, p + n)); } catch { bad('name is not UTF-8'); } p += n; return s; };
  const varint = () => { let v = 0; for (let i = 0; i < 5; i++) { need(1); const b = u8[p++]; v += (b & 0x7f) * 2 ** (7 * i); if (!(b & 0x80)) { if (v > 0xffffffff) bad('varint overflow'); return v; } } bad('varint too long'); };
  const okName = s => s.length > 0 && !s.includes('/') && s !== '.' && s !== '..' && !s.includes('\0');
  const nd = u16(); if (nd === 0 || nd > MAX_DIRS) bad('dir count ' + nd);
  const dirs = [];
  for (let i = 0; i < nd; i++) {
    const d = str();
    if (!(d === '/' || (d.startsWith('/') && !d.endsWith('/') && d.slice(1).split('/').every(okName)))) bad('bad dir ' + JSON.stringify(d.slice(0, 60)));
    dirs.push(d);
  }
  const nf = u32(); if (nf > MAX_FILES) bad('file count ' + nf);
  const files = []; let start = 0;
  const payloadLen = u8.length - payload;
  for (let i = 0; i < nf; i++) {
    const di = u16(); if (di >= nd) bad('dir index ' + di);
    const name = str(); if (!okName(name)) bad('bad file name ' + JSON.stringify(name.slice(0, 60)));
    start += varint(); const len = varint();
    if (start + len > payloadLen) bad('file ' + name + ' runs past the payload');
    const path = (dirs[di] === '/' ? '' : dirs[di]) + '/' + name;
    files.push({ path, start: payload + start, len });
  }
  if (p !== end) bad('header has ' + (end - p) + ' trailing bytes');
  const dirSet = new Set(dirs);
  for (const f of files) if (dirSet.has(f.path)) bad('path is both file and dir: ' + f.path);
  return { dirs, files, bytes: u8 };
}
