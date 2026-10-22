// Pacing bench driver: loads a bench page top-level or iframed (itch-style),
// waits for the game's own bench summary line, and reports frame stats +
// page rAF cadence stats + isolation state of the game frame.
// Usage: node bench_driver.js <top|iframe> <url> <tag> [timeoutS]
const fs = require('fs');
const puppeteer = require('puppeteer-core');

const mode = process.argv[2];
const url = process.argv[3];
const tag = process.argv[4] || 'run';
const timeoutS = parseInt(process.argv[5] || '480', 10);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

function stats(a) {
  if (!a || !a.length) return null;
  const s = [...a].sort((x, y) => x - y);
  const q = (p) => s[Math.min(s.length - 1, Math.floor(p * s.length))];
  return { n: s.length, median: +q(0.5).toFixed(2), p95: +q(0.95).toFixed(2), p99: +q(0.99).toFixed(2),
           max: +s[s.length - 1].toFixed(2), mean: +(s.reduce((x, y) => x + y, 0) / s.length).toFixed(2),
           fracOver25: +(s.filter((x) => x > 25).length / s.length).toFixed(3),
           fracOver40: +(s.filter((x) => x > 40).length / s.length).toFixed(3) };
}

async function main() {
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
    const f = page.frames().find((fr) => fr.url().includes(':8902'));
    return f || null;
  }
  let benchLine = null, logText = '';
  while (Date.now() - t0 < timeoutS * 1000) {
    const f = await gameFrame();
    if (f) {
      logText = await f.evaluate(() => (document.getElementById('log') || {}).textContent || '').catch(() => '');
      const m = logText.match(/bench renderer=[^\n]*/);
      if (m) { benchLine = m[0]; break; }
    }
    await sleep(2000);
  }
  const f = await gameFrame();
  let raf = null, iso = null;
  if (f) {
    raf = await f.evaluate(() => window.__rafD || []).catch(() => null);
    iso = await f.evaluate(() => ({ crossOriginIsolated: self.crossOriginIsolated, sab: typeof SharedArrayBuffer })).catch(() => null);
  }
  const out = { tag, mode, url, wallS: +((Date.now() - t0) / 1000).toFixed(1), benchLine, raf: stats(raf), iso,
                consoleIssues: consoleLines.slice(0, 12) };
  // Per-second bucket lines (bench mode): tail medians = steady-state split.
  const secRe = /bench-sec frame: input=([\d.]+) update=([\d.]+) upload=([\d.]+) render=([\d.]+) present=([\d.]+) ms, upload=(\d+) B\/frame, tick=([\d.]+) ms/g;
  const secs = [...logText.matchAll(secRe)].map((m) => ({ input: +m[1], update: +m[2], upload: +m[3], render: +m[4], present: +m[5], upBytes: +m[6], tick: +m[7] }));
  const tail = secs.slice(-15);
  const medOf = (a) => { if (!a.length) return null; const x = [...a].sort((p, q) => p - q); return +x[Math.floor(x.length / 2)].toFixed(3); };
  out.benchSecs = { count: secs.length, tailN: tail.length,
    input: medOf(tail.map((x) => x.input)), update: medOf(tail.map((x) => x.update)), upload: medOf(tail.map((x) => x.upload)),
    render: medOf(tail.map((x) => x.render)), present: medOf(tail.map((x) => x.present)), tick: medOf(tail.map((x) => x.tick)),
    sum: medOf(tail.map((x) => x.input + x.update + x.upload + x.render + x.present)) };
  const bp = benchLine && benchLine.match(/frames=(\d+) .*?avg=([\d.]+)ms p50=([\d.]+)ms p99=([\d.]+)ms max=([\d.]+)ms input=([\d.]+) update=([\d.]+) upload=([\d.]+) render=([\d.]+) present=([\d.]+)/);
  if (bp) out.parsed = { frames: +bp[1], avg: +bp[2], p50: +bp[3], p99: +bp[4], max: +bp[5], input: +bp[6], update: +bp[7], upload: +bp[8], render: +bp[9], present: +bp[10] };
  fs.writeFileSync(`results/${tag}.json`, JSON.stringify(out, null, 1));
  fs.writeFileSync(`results/${tag}-log.txt`, logText);
  console.log(tag, benchLine ? 'BENCH OK' : 'BENCH TIMEOUT', out.parsed ? JSON.stringify(out.parsed) : '', 'tailSec', JSON.stringify(out.benchSecs), 'rafMed', raf ? stats(raf).median : '-');
  await browser.close();
}
main().catch((e) => { console.error('DRIVER FAIL', e); process.exit(1); });
