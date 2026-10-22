// Build 50 diagnostic: does interactive boot PRESENT on the same page (no
// re-navigation)? interactive.js's canvasShot re-navigates before every
// screenshot, so its frames may be fresh instances caught mid-boot. This
// probe navigates ONCE, waits for the menu, and screenshots the SAME page
// via CDP at menu and after Enter->play.
const fs = require('fs');
const puppeteer = require('puppeteer-core');

const url = process.argv[2];
const tag = process.argv[3] || 'probe';
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

  const booted = await waitForLog('backend live', bootTimeoutS);
  console.log('boot wait:', booted ? 'OK' : 'TIMEOUT');
  const menu = await waitForLog('menu: launch menu', 120);
  console.log('menu wait:', menu ? 'OK' : 'TIMEOUT');
  const presentLine = await page.evaluate(() => document.getElementById('log').textContent).catch(() => '');
  console.log('present path:', (presentLine.match(/web present: [^\n]*/) || ['(none logged)'])[0]);
  const perf = await waitForLog('perf fps', 240);
  console.log('perf wait:', perf ? 'OK' : 'TIMEOUT');
  await sleep(6000);
  await page.screenshot({ path: `${tag}-menu.png` });
  console.log('shot: menu (same page)');

  await page.bringToFront().catch(() => {});
  await page.evaluate(() => document.getElementById('canvas').focus());
  await page.keyboard.press('Enter');
  const play = await waitForLog('menu: play', 60);
  console.log('play wait:', play ? 'OK' : 'TIMEOUT');
  await sleep(4000);
  await page.screenshot({ path: `${tag}-play.png` });
  console.log('shot: play (same page)');

  const t = await logText().catch(() => '');
  fs.writeFileSync(`${tag}-log.txt`, t);
  fs.writeFileSync(`${tag}-console.txt`, consoleLines.join('\n'));
  await browser.close();
}
main().catch((e) => { console.error('PROBE FAIL', e); process.exit(1); });
