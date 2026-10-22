// probevao.js — H3 regression probe (bug-fix run, 2026-10-20).
//
// The page-side GL wrapper caches buffer bindings keyed by TARGET
// only. But ELEMENT_ARRAY_BUFFER (target 0x8893) binding is per-VAO
// state in WebGL2: bind element buffer X under VAO A, switch to VAO B,
// bind X again — the old cache saw "0x8893 already = X" and suppressed
// the second bind, leaving VAO B with NO element buffer (drawElements
// then reads garbage/fails, silently).
//
// This probe boots the real gl.html, waits for the launch menu, then
// drives the REAL wrapper (LK_GL) in one synchronous evaluate (atomic
// vs the engine's streamed frames on the single page thread):
//   gen VAO A, VAO B, buffer X
//   bind VAO A; bindBuffer(ELEMENT_ARRAY_BUFFER, X)
//   bind VAO B; bindBuffer(ELEMENT_ARRAY_BUFFER, X)
//   read back gl.getParameter(ELEMENT_ARRAY_BUFFER_BINDING=0x8895) under B
// PASS iff the readback names X.
//
// Constant note: an earlier draft of this probe used 0x8897 as the
// target (the code review's parenthetical) — that is not the target
// (0x8893) nor the binding pname (0x8895), the binds were invalid-enum
// no-ops, and its "fail" proved nothing. The values below are verified
// against the tree's gl_loader.h and the Khronos GLES3 header.
//
// Usage: node probevao.js <origin> [tag]      (origin e.g. http://localhost:8123)
const path = require('path');
const puppeteer = require('puppeteer-core');
const CHROME = '/home/hatch/.cache/puppeteer/chrome/linux-155.0.8059.79/chrome-linux64/chrome';

async function main() {
  const origin = process.argv[2];
  if (!origin) { console.error('usage: probevao.js <origin> [tag]'); process.exit(2); }
  const url = origin + '/gl.html';
  const browser = await puppeteer.launch({
    executablePath: CHROME, headless: 'new',
    args: ['--no-sandbox', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--mute-audio'],
  });
  const page = await browser.newPage();
  await page.evaluateOnNewDocument(() => {
    window.__certAccept = setInterval(() => {
      const b = document.querySelector('button');
      if (b && /proceed/i.test(b.textContent || '')) b.click();
    }, 500);
  });
  const cdp = await page.createCDPSession();
  await cdp.send('Security.setIgnoreCertificateErrors', { ignore: true }).catch(() => {});
  page.on('console', (m) => console.log('  [page]', m.text().slice(0, 300)));
  await page.goto(url, { waitUntil: 'load', timeout: 60000 });

  let booted = false;
  for (let i = 0; i < 360 && !booted; i++) {
    await new Promise((r) => setTimeout(r, 500));
    const log = await page.evaluate(() => document.getElementById('log').textContent).catch(() => '');
    if (/menu: launch menu/.test(log)) booted = true;
    if (/BOOT FAILED|not cross-origin isolated/.test(log) && i > 20) break;
  }
  if (!booted) { console.log('PROBEVAO: FAIL (page did not reach the launch menu)'); await browser.close(); process.exit(1); }

  const r = await page.evaluate(() => {
    const GL = LK_GL;
    const rec = LK.gl && LK.gl.current;
    if (!rec) return { err: 'no current GL context on the page' };
    const u32 = new Uint32Array(LK.memory.buffer);
    const ptr = LK.exports.malloc(64);
    const nameAt = () => u32[ptr >> 2];
    GL.emscripten_glGenVertexArrays(1, ptr); const vaoA = nameAt();
    GL.emscripten_glGenVertexArrays(1, ptr); const vaoB = nameAt();
    GL.emscripten_glGenBuffers(1, ptr); const bufX = nameAt();
    GL.emscripten_glBindVertexArray(vaoA);
    GL.emscripten_glBindBuffer(0x8893, bufX);       // ELEMENT_ARRAY_BUFFER under A
    const underA = rec.gl.getParameter(0x8895);     // ELEMENT_ARRAY_BUFFER_BINDING
    GL.emscripten_glBindVertexArray(vaoB);
    GL.emscripten_glBindBuffer(0x8893, bufX);       // same bind under B — H3 case
    const boundObj = rec.gl.getParameter(0x8895);   // ELEMENT_ARRAY_BUFFER_BINDING
    const boundName = boundObj ? (rec.reg.buffer.rev.get(boundObj) || 0) : 0;
    const underAName = underA ? (rec.reg.buffer.rev.get(underA) || 0) : 0;
    // restore an inert state for the still-running menu
    GL.emscripten_glBindVertexArray(0);
    LK.exports.free(ptr);
    return { vaoA, vaoB, bufX, underAName, boundName, ok: boundName === bufX && bufX !== 0 && underAName === bufX };
  });
  console.log('scenario:', JSON.stringify(r));
  await browser.close();
  console.log(r.ok ? 'PROBEVAO: PASS' : 'PROBEVAO: FAIL');
  process.exit(r.ok ? 0 : 1);
}
main().catch((e) => { console.error('PROBEVAO: ERROR', e && e.message); process.exit(1); });
