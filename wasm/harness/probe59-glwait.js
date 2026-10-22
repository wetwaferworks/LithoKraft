// GL follow-up: after Enter->play, press W and poll ring indices for up
// to 30 s to distinguish drain latency (slow SwiftShader frames) from a
// stall. Usage: node probe59b.js <url> <tag>
const fs = require('fs');
const puppeteer = require('puppeteer-core');
const url = process.argv[2];
const tag = process.argv[3] || 'probe59b';
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
  console.log('boot:', await waitForLog('backend live', 300) ? 'OK' : 'TIMEOUT');
  console.log('menu:', await waitForLog('menu: launch menu', 120) ? 'OK' : 'TIMEOUT');
  await page.bringToFront().catch(() => {});
  await page.evaluate(() => document.getElementById('canvas').focus());
  await page.keyboard.press('Enter');
  console.log('play:', await waitForLog('menu: play', 60) ? 'OK' : 'TIMEOUT');
  await sleep(3000);
  console.log('pre-W ring:', JSON.stringify(await ringIdx()));
  await page.keyboard.down('KeyW');
  const t0 = Date.now();
  let caught = null;
  while (Date.now() - t0 < 30000) {
    await sleep(1500);
    const idx = await ringIdx();
    const t = await logText().catch(() => '');
    const echo = t.includes('KeyDown W (code KeyW)');
    console.log(`+${((Date.now() - t0) / 1000).toFixed(1)}s ring=${JSON.stringify(idx)} echoW=${echo}`);
    if (echo) { caught = idx; break; }
  }
  await page.keyboard.up('KeyW');
  await sleep(3000);
  console.log('final ring:', JSON.stringify(await ringIdx()));
  fs.writeFileSync(`${tag}-log.txt`, await logText().catch(() => ''));
  await browser.close();
}
main().catch((e) => { console.error(e); process.exit(1); });
