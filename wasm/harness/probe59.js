// Build 59 probe: verifies the two web-seam landings on a served page.
//  - "input: memory-mapped ring LIVE" appears in the page log.
//  - window.__lkRingOn === 1 and the ring write index ADVANCES when a key
//    is dispatched, then the read index catches up (C++ drain) — direct
//    evidence the event travelled the shared-memory ring, not the queue.
//  - With ?debuginput=1 the conversion echo "input> KeyDown W (code
//    KeyW)" appears after pressing W.
//  - Software page only: "thin-import present engaged" (or, with
//    ?lkthin=off, "disabled by ?lkthin=off").
//  - Enter reaches "menu: play"; screenshots at menu and play.
// Usage: node probe59.js <url-with-query> <tag> [bootTimeoutS]
const fs = require('fs');
const puppeteer = require('puppeteer-core');

const url = process.argv[2];
const tag = process.argv[3] || 'probe59';
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
  page.on('console', (m) => consoleLines.push(m.text()));
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
  console.log('thin line:', (log1.match(/thin-import present[^\n]*/) || ['(none)'])[0]);
  console.log('ring state at boot:', JSON.stringify(await ringIdx()));

  const menu = await waitForLog('menu: launch menu', 120);
  console.log('menu wait:', menu ? 'OK' : 'TIMEOUT');
  await sleep(3000);
  await page.screenshot({ path: `${tag}-menu.png` });
  console.log('shot: menu');

  await page.bringToFront().catch(() => {});
  await page.evaluate(() => document.getElementById('canvas').focus());
  const before = await ringIdx();
  await page.keyboard.press('Enter');
  const play = await waitForLog('menu: play', 60);
  console.log('play wait:', play ? 'OK' : 'TIMEOUT');
  await sleep(2000);
  await page.screenshot({ path: `${tag}-play.png` });
  console.log('shot: play');

  // W keypress: ring indices are the direct evidence.
  const preW = await ringIdx();
  await page.keyboard.down('KeyW');
  await sleep(700);
  const midW = await ringIdx();
  await page.keyboard.up('KeyW');
  await sleep(700);
  const postW = await ringIdx();
  console.log('ring before Enter:', JSON.stringify(before));
  console.log('ring pre-W:', JSON.stringify(preW), 'mid-W:', JSON.stringify(midW), 'post-W:', JSON.stringify(postW));
  const log2 = await logText().catch(() => '');
  console.log('echo W:', log2.includes('KeyDown W (code KeyW)') ? 'SEEN' : 'NOT SEEN');
  fs.writeFileSync(`${tag}-log.txt`, log2);
  fs.writeFileSync(`${tag}-console.txt`, consoleLines.join('\n'));
  await browser.close();
}
main().catch((e) => { console.error(e); process.exit(1); });
