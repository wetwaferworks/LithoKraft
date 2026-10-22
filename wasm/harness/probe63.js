// Build 66 probe (drain-cadence fix verification). Per page load:
//  - "input: mapped ring drain at STEP cadence" line present in page log.
//  - Ring LIVE + thin-import lines captured.
//  - Drain latency: poll the ring header (write/read indices) at ~8 ms
//    from the driver side; dispatch ONE keydown; measure time from just
//    before dispatch until the read index first reaches the new write
//    index (event consumed). Build 62's frame-bound drain showed ~1.5 s
//    under SwiftShader GL; step-cadence drain should land in tens of ms
//    (poll quantization dominates).
//  - Presented-frame rate: in-page rAF counter over a fixed window.
//  - Pointer-lock cycle: click canvas (backend requests lock), then
//    document.exitPointerLock(), then an immediate re-click (inside
//    Chrome's ~1.25 s cooldown): assert no PAGEERROR and no console
//    line containing "SecurityError" / "Uncaught".
// Usage: node probe63.js <url-with-query> <tag> [bootTimeoutS]
const fs = require('fs');
const puppeteer = require('puppeteer-core');

const url = process.argv[2];
const tag = process.argv[3] || 'probe63';
const bootTimeoutS = parseInt(process.argv[4] || '300', 10);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

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
  page.on('console', (m) => consoleLines.push(m.type() + ': ' + m.text()));
  page.on('pageerror', (e) => consoleLines.push('PAGEERROR: ' + e.message));
  await page.goto(url, { waitUntil: 'load', timeout: 120000 });

  const logText = () => page.evaluate(() => document.getElementById('log').textContent);
  async function waitForLog(needle, timeoutS) {
    const t0 = Date.now();
    while (Date.now() - t0 < timeoutS * 1000) {
      const t = await logText().catch(() => '');
      if (t.includes(needle)) return t;
      await sleep(1000);
    }
    return null;
  }
  const ringIdx = () => page.evaluate(() => {
    try {
      var v = window.__lkRingView;
      if (!v) return null;
      return { on: window.__lkRingOn, w: Atomics.load(v, 1), r: Atomics.load(v, 2), dropped: Atomics.load(v, 3) };
    } catch (e) { return { err: String(e) }; }
  });

  const booted = await waitForLog('backend live', bootTimeoutS);
  console.log('boot wait:', booted ? 'OK' : 'TIMEOUT');
  const log1 = await logText().catch(() => '');
  console.log('ring line:', (log1.match(/input: memory-mapped ring[^\n]*/) || ['(none)'])[0]);
  console.log('cadence line:', (log1.match(/input: mapped ring drain[^\n]*/) || ['(none)'])[0]);
  console.log('thin line:', (log1.match(/thin-import present[^\n]*/) || ['(none)'])[0]);

  const menu = await waitForLog('menu: launch menu', 180);
  console.log('menu wait:', menu ? 'OK' : 'TIMEOUT');
  await sleep(2000);
  await page.screenshot({ path: `${tag}-menu.png` });
  await page.bringToFront().catch(() => {});
  await page.evaluate(() => document.getElementById('canvas').focus());
  await page.keyboard.press('Enter');
  const play = await waitForLog('menu: play', 90);
  console.log('play wait:', play ? 'OK' : 'TIMEOUT');
  await sleep(1500);
  await page.screenshot({ path: `${tag}-play.png` });

  // Presented-frame rate via rAF over 15 s.
  const fps = await page.evaluate(() => new Promise((resolve) => {
    let n = 0; const t0 = performance.now();
    function tick() { n++; if (performance.now() - t0 < 15000) requestAnimationFrame(tick); else resolve(n / ((performance.now() - t0) / 1000)); }
    requestAnimationFrame(tick);
  }));
  console.log('presented fps (rAF, 15 s):', fps.toFixed(2));

  // Drain latency: high-frequency ring polling around one keydown.
  async function measureDrain(key) {
    const base = await ringIdx();
    if (!base || base.err) return { err: 'no ring', base };
    const samples = [];
    let stop = false;
    const poller = (async () => {
      while (!stop) {
        const s = await ringIdx().catch(() => null);
        if (s) samples.push({ t: Date.now(), w: s.w, r: s.r });
        await sleep(8);
      }
    })();
    await sleep(150); // baseline samples
    const tDispatch = Date.now();
    await page.keyboard.down(key);
    await sleep(600);
    await page.keyboard.up(key);
    await sleep(400);
    stop = true;
    await poller;
    const target = base.w + 1;
    let tWrite = null, tRead = null;
    for (const s of samples) {
      if (tWrite === null && s.w >= target) tWrite = s.t;
      if (tWrite !== null && s.r >= target) { tRead = s.t; break; }
    }
    return { base, target, writeLagMs: tWrite === null ? null : tWrite - tDispatch, drainLagMs: tRead === null ? null : tRead - tDispatch, samples: samples.length };
  }
  const drain = await measureDrain('KeyW');
  console.log('drain measure:', JSON.stringify(drain));
  const log2 = await logText().catch(() => '');
  console.log('echo W:', log2.includes('KeyDown W (code KeyW)') ? 'SEEN' : 'NOT SEEN');

  // Pointer-lock exit / immediate re-lock cycle.
  const lockState = () => page.evaluate(() => ({
    locked: !!document.pointerLockElement,
    lastExitMs: (typeof window.__lkLockExitMs === 'number') ? window.__lkLockExitMs : null,
  }));
  const consoleMark = consoleLines.length;
  await page.click('#canvas').catch((e) => console.log('click1 err:', String(e)));
  await sleep(1200);
  console.log('lock after click:', JSON.stringify(await lockState()));
  await page.evaluate(() => { if (document.pointerLockElement) document.exitPointerLock(); }).catch(() => {});
  await sleep(250);
  console.log('lock after exit:', JSON.stringify(await lockState()));
  await page.click('#canvas').catch((e) => console.log('click2 err:', String(e)));
  await sleep(2000);
  console.log('lock after re-click:', JSON.stringify(await lockState()));
  const cycleLines = consoleLines.slice(consoleMark);
  const bad = cycleLines.filter((l) => l.includes('SecurityError') || l.includes('Uncaught') || l.startsWith('PAGEERROR'));
  console.log('pointer-lock cycle console (new lines):', JSON.stringify(cycleLines));
  console.log('pointer-lock uncaught errors:', bad.length === 0 ? 'NONE' : JSON.stringify(bad));
  const allBad = consoleLines.filter((l) => l.includes('SecurityError') || l.startsWith('PAGEERROR'));
  console.log('page-total SecurityError/PAGEERROR lines:', allBad.length === 0 ? 'NONE' : JSON.stringify(allBad));

  fs.writeFileSync(`${tag}-log.txt`, await logText().catch(() => ''));
  fs.writeFileSync(`${tag}-console.txt`, consoleLines.join('\n'));
  await browser.close();
}
main().catch((e) => { console.error(e); process.exit(1); });
