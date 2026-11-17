// LKP2 reader + read-only seeding checks. node tests/pack_check.mjs <pack.lkp2>
import fs from 'node:fs';
import { parseLKP2, LkPackError } from '../lkpack.mjs';
import { MemoryBackend, E } from '../lkstore.mjs';
let fails = 0; const check = (n, ok, d = '') => { console.log((ok ? 'PASS ' : 'FAIL ') + n + (d ? ' ' + d : '')); if (!ok) fails++; };
const buf = new Uint8Array(fs.readFileSync(process.argv[2]));
const pk = parseLKP2(buf);
check('real pack parses', pk.files.length > 5000 && pk.dirs.includes('/LithoKraft/assets'), `${pk.files.length} files, ${pk.dirs.length} dirs`);
// Every file view is inside the buffer and pack.json exists where the engine looks.
check('file ranges inside the payload', pk.files.every(f => f.start + f.len <= buf.length));
check('engine and game pack.json present', pk.files.some(f => f.path === '/assets/pack.json') && pk.files.some(f => f.path === '/LithoKraft/assets/pack.json'));
// Malformed packs are rejected whole.
const rej = (name, b) => { let e = null; try { parseLKP2(b); } catch (x) { e = x; } check('negative: ' + name, e instanceof LkPackError, e ? e.message : 'accepted'); };
rej('empty', new Uint8Array(0));
rej('bad magic', Uint8Array.from([0x4C, 0x4B, 0x50, 0x31, 0, 0, 0, 0]));
{ const b = buf.slice(0, 64); rej('header truncated', b); }
{ const b = buf.slice(); new DataView(b.buffer).setUint32(4, 0xFFFFFFF0, true); rej('header length past end', b); }
{ const b = buf.slice(0, buf.length - 1000); rej('payload truncated (last file runs past the end)', b); }
{ const b = buf.slice(); b[10] = 0x2E; b[11] = 0x2E; rej('corrupt dir table', b); }
// Seeded files are readable and read-only.
const mem = new WebAssembly.Memory({ initial: 4 });
const mb = new MemoryBackend(mem); mb.seedPack(pk);
const put = s => { const b = new TextEncoder().encode(s); new Uint8Array(mem.buffer).set(b, 1024); return [1024, b.length]; };
const h = mb.lk_store_open(...put('/assets/pack.json'), 1);
check('seeded file opens for read', h > 0);
check('negative: write to a pack file -> EROFS', mb.lk_store_write(h, 2048, 4, 0n) === -E.EROFS);
check('negative: open pack file for write -> EROFS', mb.lk_store_open(...put('/assets/pack.json'), 2) === -E.EROFS);
check('negative: remove pack file -> EROFS', mb.lk_store_remove(...put('/assets/pack.json')) === -E.EROFS);
check('saves still writable next to the pack', mb.lk_store_open(...put('/save.bin'), 2 | 4) > 0);
console.log(fails ? 'PACK CHECK: FAIL' : 'PACK CHECK: PASS'); process.exit(fails ? 1 : 0);
