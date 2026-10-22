// Pacing cast driver v2 (fixed schedule). Boots a page interactively
// (top-level or itch-style cross-origin iframe), enters play, waits a
// FIXED settle time, then measures PRESENTED-frame cadence externally
// via CDP Page.startScreencast (compositor timestamps) over a fixed
// window. Why fixed-schedule: on this 2-vCPU SwiftShader bench the
// chunk stream never fully saturates in reasonable time (it asymptotes
// with circuit remesh churn), so identical schedules are the only
// apples-to-apples basis; the stream state (meshes received/total) is
// reported per run alongside the cadence.
// Sim health comes from the game's perf lines (their tps/tick fields
// are valid in interactive mode; the fps/frame fields of that line
// read 0 in play — a readout quirk also present in build 55's own
// probe logs — so frame truth comes from the screencast + the F3
// overlay values visible in the end screenshot).
// Usage: node cast_driver.js <top|iframe> <url> <tag> [settleS] [windowS]
const fs = require('fs');
const path = require('path');
const puppeteer = require('puppeteer-core');

const mode = process.argv[2];
const url = process.argv[3];
const tag = process.argv[4] || 'run';
const settleS = parseInt(process.argv[5] || '240', 10);
const windowS = parseInt(process.argv[6] || '90', 10);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const RESULTS = path.join(__dirname, 'results');

function med(a) { if (!a.length) return null; const s = [...a].sort((x, y) => x - y); return +s[Math.floor(s.length / 2)].toFixed(2); }
function istats(iv) {
  if (iv.length < 1) return null;
  const ms = iv.map((x) => x * 1000);
  const s = [...ms].sort((a, b) => a - b);
  const q = (p) => s[Math.min(s.length - 1, Math.floor(p * s.length))];
  return { intervals: ms.length, fps: +(ms.length / (iv.reduce((a, b) => a + b, 0))).toFixed(2),
           medianMs: +q(0.5).toFixed(1), p95Ms: +q(0.95).toFixed(1), maxMs: +s[s.length - 1].toFixed(1),
           fracOver40: +(ms.filter((x) => x > 40).length / ms.length).toFixed(3),
           fracOver100: +(ms.filter((x) => x > 100).length / ms.length).toFixed(3),
           fracOver1000: +(ms.filter((x) => x > 1000).length / ms.length).toFixed(3) };
}
const PERF = /perf fps=([\d.]+) frame=([\d.]+)\/([\d.]+)ms tps=([\d.]+)\/\d+ tick=([\d.]+)\/([\d.]+)ms budget=([\d.]+)% dropped=(\d+) chunks=(\d+) meshes=(\d+)\/(\d+)/g;

async function main() {
  const out = { tag, mode, url, settleS, windowS };
  const browser = await puppeteer.launch({
    executablePath: '/home/hatch/.cache/puppeteer/chrome/linux-155.0.8059.79/chrome-linux64/chrome',
    headless: 'new',
    args: ['--no-sandbox', '--disable-dev-shm-usage', '--use-gl=angle', '--use-angle=swiftshader',
           '--enable-unsafe-swiftshader', '--window-size=1100,1000'],
  });
  const page = await browser.newPage();
  await page.setViewport({ width: 1100, height: 1000 });
  const consoleLines = [];
  page.on('console', (m) => { const t = m.text(); if (/error|Error|GL_INVALID|failed/i.test(t)) consoleLines.push(t); });
  page.on('pageerror', (e) => consoleLines.push('PAGEERROR: ' + e.message));
  const t0 = Date.now();
  await page.goto(url, { waitUntil: 'load', timeout: 120000 });

  async function gameFrame() {
    if (mode === 'top') return page.mainFrame();
    return page.frames().find((fr) => fr.url().includes(':8902')) || null;
  }
  const logOf = async () => {
    const f = await gameFrame();
    if (!f) return '';
    return f.evaluate(() => (document.getElementById('log') || {}).textContent || '').catch(() => '');
  };
  const perfTail = (t) => {
    const all = [...t.matchAll(PERF)];
    return all.length ? all[all.length - 1] : null;
  };

  let menuSeen = false;
  while (Date.now() - t0 < 240 * 1000) {
    const t = await logOf();
    if (t.includes('menu: launch menu')) { menuSeen = true; break; }
    await sleep(2000);
  }
  out.menuSeen = menuSeen;
  await page.keyboard.press('Enter');
  let playSeen = false;
  for (let i = 0; i < 40 && !playSeen; i++) {
    const t = await logOf();
    if (t.includes('menu: play')) playSeen = true; else await sleep(2000);
  }
  out.playSeen = playSeen;
  {
    const f = await gameFrame();
    if (f) { const el = await f.$('#canvas').catch(() => null); if (el) await el.click().catch(() => {}); }
  }
  const tPlay = Date.now();
  out.pointerLock = await (async () => {
    const t = await logOf();
    return t.includes('input: pointer lock engaged') ? 'engaged' : 'unknown';
  })();

  // Fixed settle: poll perf state so the report carries the stream state.
  let lastPerf = null;
  while (Date.now() - tPlay < settleS * 1000) {
    const t = await logOf();
    lastPerf = perfTail(t) || lastPerf;
    await sleep(5000);
  }
  if (lastPerf) {
    out.meshesAtCast = +lastPerf[10]; out.totalSubsAtCast = +lastPerf[11];
    out.tickAvgAtCast = +lastPerf[5]; out.tpsAtCast = +lastPerf[4]; out.droppedAtCast = +lastPerf[8];
  }

  const client = await page.createCDPSession();
  await client.send('Page.enable');
  const stamps = [];
  client.on('Page.screencastFrame', async (ev) => {
    stamps.push(ev.metadata.timestamp);
    try { await client.send('Page.screencastFrameAck', { sessionId: ev.sessionId }); } catch (_) {}
  });
  await client.send('Page.startScreencast', { format: 'jpeg', quality: 20, maxWidth: 1100, maxHeight: 1000, everyNthFrame: 1 });
  await sleep(windowS * 1000);
  await client.send('Page.stopScreencast').catch(() => {});
  const iv = [];
  for (let i = 1; i < stamps.length; i++) iv.push(stamps[i] - stamps[i - 1]);
  out.cast = istats(iv);
  out.intervalsMs = iv.map((x) => +(x * 1000).toFixed(1));
  {
    const t = await logOf();
    const last = perfTail(t);
    if (last) { out.tickAvgAfter = +last[5]; out.tpsAfter = +last[4]; out.droppedAfter = +last[8]; out.meshesAfter = +last[10]; }
    out.aaLine = (t.match(/window: WebGL antialias[^\n]*/) || [null])[0];
    const f = await gameFrame();
    if (f) out.iso = await f.evaluate(() => ({ crossOriginIsolated: self.crossOriginIsolated, sab: typeof SharedArrayBuffer })).catch(() => null);
  }
  try { await page.screenshot({ path: path.join(RESULTS, tag + '.png') }); } catch (e) { out.shotErr = String(e).slice(0, 160); }
  out.wallS = +((Date.now() - t0) / 1000).toFixed(1);
  out.consoleIssues = consoleLines.slice(0, 12);
  fs.writeFileSync(path.join(RESULTS, tag + '.json'), JSON.stringify(out, null, 1));
  console.log(tag, 'cast', JSON.stringify(out.cast), 'meshes', out.meshesAtCast + '/' + out.totalSubsAtCast, 'tick', out.tickAvgAtCast, 'lock', out.pointerLock);
  await browser.close();
}
main().catch((e) => {
  try { fs.writeFileSync(path.join(RESULTS, tag + '.error.txt'), String((e && e.stack) || e)); } catch (_) {}
  console.error('DRIVER FAIL', e);
  process.exit(1);
});
