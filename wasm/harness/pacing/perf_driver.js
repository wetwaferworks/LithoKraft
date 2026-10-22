// Pacing perf driver: boots a page interactively (top-level or itch-style
// iframe), enters play (Enter), stands at the spawn vista, and collects the
// game's own per-second "perf" lines once mesh streaming plateaus — the
// same posture as JD's itch playtest. Reports medians of the frame-average
// values + rAF cadence stats of the game frame.
// Usage: node perf_driver.js <top|iframe> <url> <tag> [timeoutS] [collectN]
const fs = require('fs');
const path = require('path');
const puppeteer = require('puppeteer-core');

const mode = process.argv[2];
const url = process.argv[3];
const tag = process.argv[4] || 'run';
const timeoutS = parseInt(process.argv[5] || '600', 10);
const collectN = parseInt(process.argv[6] || '15', 10);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const RESULTS = path.join(__dirname, 'results');

function med(a) { if (!a.length) return null; const s = [...a].sort((x, y) => x - y); return +s[Math.floor(s.length / 2)].toFixed(2); }
function stats(a) {
  if (!a || !a.length) return null;
  const s = [...a].sort((x, y) => x - y);
  const q = (p) => s[Math.min(s.length - 1, Math.floor(p * s.length))];
  return { n: s.length, median: +q(0.5).toFixed(2), p95: +q(0.95).toFixed(2), max: +s[s.length - 1].toFixed(2),
           fracOver25: +(s.filter((x) => x > 25).length / s.length).toFixed(3),
           fracOver40: +(s.filter((x) => x > 40).length / s.length).toFixed(3) };
}
const PERF = /perf fps=([\d.]+) frame=([\d.]+)\/([\d.]+)ms tps=([\d.]+)\/\d+ tick=([\d.]+)\/([\d.]+)ms budget=([\d.]+)% dropped=(\d+) chunks=(\d+) meshes=(\d+)\/(\d+)/g;

async function main() {
  const out = { tag, mode, url, forcedCollect: false };
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
  await page.evaluateOnNewDocument(() => {
    window.__rafD = [];
    let last = performance.now();
    const loop = (ts) => { window.__rafD.push(ts - last); last = ts; if (window.__rafD.length < 200000) requestAnimationFrame(loop); };
    requestAnimationFrame(loop);
  });
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

  // Wait for the launch menu, then Enter into play (Enter-first is the
  // documented reliable order: a stray click can clear menu focus).
  let menuSeen = false;
  while (Date.now() - t0 < 240 * 1000) {
    const t = await logOf();
    if (t.includes('menu: launch menu')) { menuSeen = true; break; }
    await sleep(2000);
  }
  out.menuSeen = menuSeen;
  await page.keyboard.press('Enter');
  let playSeen = false;
  for (let i = 0; i < 30 && !playSeen; i++) {
    const t = await logOf();
    if (t.includes('menu: play')) playSeen = true; else await sleep(2000);
  }
  out.playSeen = playSeen;
  // Click for pointer lock (the play posture), then stand still at the vista.
  {
    const f = await gameFrame();
    if (f) {
      const el = await f.$('#canvas').catch(() => null);
      if (el) await el.click().catch(() => {});
    }
  }

  // Collect perf lines; start after the mesh stream plateaus (received
  // unchanged across 3 lines, or >=98% of total sub-chunks) with the sim
  // at speed; force-start after 240 s of play so a churny world still
  // yields a (flagged) sample.
  const lines = [];
  let lastMesh = -1, stable = 0, startedCollect = false, seen = 0;
  const playT0 = Date.now();
  while (Date.now() - t0 < timeoutS * 1000) {
    const t = await logOf();
    const all = [...t.matchAll(PERF)];
    for (let i = seen; i < all.length; i++) {
      seen++;
      const m = all[i];
      const rec = { fps: +m[1], frameAvg: +m[2], frameMax: +m[3], tps: +m[4], tickAvg: +m[5], dropped: +m[8], meshes: +m[10], totalSubs: +m[11] };
      if (rec.meshes === lastMesh) stable++; else { stable = 0; lastMesh = rec.meshes; }
      const saturated = rec.totalSubs > 0 && rec.meshes >= 0.98 * rec.totalSubs;
      if (!startedCollect && ((stable >= 3 || saturated) && rec.tps > 100)) startedCollect = true;
      if (!startedCollect && rec.tps > 200 && Date.now() - playT0 > 240 * 1000) { startedCollect = true; out.forcedCollect = true; }
      if (startedCollect) lines.push(rec);
    }
    if (lines.length >= collectN) break;
    await sleep(2000);
  }
  const f = await gameFrame();
  let raf = null, iso = null;
  if (f) {
    raf = await f.evaluate(() => window.__rafD || []).catch(() => null);
    iso = await f.evaluate(() => ({ crossOriginIsolated: self.crossOriginIsolated, sab: typeof SharedArrayBuffer })).catch(() => null);
  }
  try { await page.screenshot({ path: path.join(RESULTS, tag + '.png') }); }
  catch (e) { out.shotErr = String(e).slice(0, 200); }
  Object.assign(out, {
    wallS: +((Date.now() - t0) / 1000).toFixed(1),
    collected: lines.length,
    frameAvgMedian: med(lines.map((l) => l.frameAvg)),
    frameMaxMedian: med(lines.map((l) => l.frameMax)),
    fpsMedian: med(lines.map((l) => l.fps)),
    tpsMedian: med(lines.map((l) => l.tps)),
    tickAvgMedian: med(lines.map((l) => l.tickAvg)),
    meshesAtCollect: lines.length ? lines[lines.length - 1].meshes : null,
    totalSubsAtCollect: lines.length ? lines[lines.length - 1].totalSubs : null,
    droppedMax: lines.length ? Math.max(...lines.map((l) => l.dropped)) : null,
    raf: stats(raf), iso, consoleIssues: consoleLines.slice(0, 12), lines,
  });
  fs.writeFileSync(path.join(RESULTS, tag + '.json'), JSON.stringify(out, null, 1));
  console.log(tag, 'collected', out.collected, 'frameAvgMed', out.frameAvgMedian, 'fpsMed', out.fpsMedian,
              'meshes', out.meshesAtCollect, '/', out.totalSubsAtCollect, 'rafMed', out.raf ? out.raf.median : '-');
  await browser.close();
}
main().catch((e) => {
  try { fs.writeFileSync(path.join(RESULTS, tag + '.error.txt'), String((e && e.stack) || e)); } catch (_) {}
  console.error('DRIVER FAIL', e);
  process.exit(1);
});
