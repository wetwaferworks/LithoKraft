// Diagnostic 3: replicate probetouch's exact play-state joystick flow,
// logging element rects + scrollY at every step and dumping which key
// echoes the gesture actually produced.
const puppeteer = require('puppeteer-core');
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
(async () => {
  const browser = await puppeteer.launch({
    executablePath: '/home/hatch/.cache/puppeteer/chrome/linux-155.0.8059.79/chrome-linux64/chrome',
    headless: 'new',
    args: ['--no-sandbox', '--disable-dev-shm-usage', '--use-gl=angle', '--use-angle=swiftshader',
           '--enable-unsafe-swiftshader', '--window-size=1100,1000'],
  });
  const page = await browser.newPage();
  await page.emulate({
    viewport: { width: 1100, height: 1000, hasTouch: true, isMobile: false, deviceScaleFactor: 1 },
    userAgent: 'diag3',
  });
  page.on('pageerror', (e) => console.log('PAGEERROR:', e.message));
  await page.goto('http://127.0.0.1:8931/gl.html?touch=1&debuginput=1', { waitUntil: 'load', timeout: 120000 });
  const logText = () => page.evaluate(() => document.getElementById('log').textContent);
  async function waitForLog(needle, timeoutS) {
    const t0 = Date.now();
    while (Date.now() - t0 < timeoutS * 1000) {
      const t = await logText().catch(() => '');
      if (t.includes(needle)) return true;
      await sleep(1000);
    }
    return false;
  }
  const state = (label) => page.evaluate((l) => {
    const j = document.getElementById('lk-joy').getBoundingClientRect();
    const c = document.getElementById('canvas').getBoundingClientRect();
    return { label: l, scrollY: window.scrollY,
             joy: [j.x, j.y, j.width, j.height].map(Math.round).join(','),
             canvas: [c.x, c.y].map(Math.round).join(','),
             lock: document.pointerLockElement ? 'canvas' : 'none' };
  }, label).then((s) => console.log(JSON.stringify(s)));

  console.log('boot:', await waitForLog('backend live', 300));
  console.log('menu:', await waitForLog('menu: launch menu', 240));
  await state('at menu');
  // ENTER via overlay tap (same as probetouch)
  const enEl = await page.$('#lk-enter');
  const eb = await enEl.boundingBox();
  await page.touchscreen.touchStart(eb.x + eb.width / 2, eb.y + eb.height / 2);
  await sleep(150);
  await page.touchscreen.touchEnd();
  await sleep(250);
  console.log('play:', await waitForLog('menu: play', 90));
  await sleep(1200);
  await page.screenshot({ path: 'diag3-play.png' });
  await state('after play + screenshot');

  const joyEl = await page.$('#lk-joy');
  const jb = await joyEl.boundingBox();
  console.log('probe-style box center:', Math.round(jb.x + jb.width / 2), Math.round(jb.y + jb.height / 2));
  const before = await logText();
  await page.touchscreen.touchStart(jb.x + jb.width / 2, jb.y + jb.height / 2);
  await sleep(120);
  await state('after touchStart');
  await page.touchscreen.touchMove(jb.x + jb.width / 2, jb.y + jb.height / 2 - 55);
  await sleep(800);
  await state('after touchMove');
  await page.touchscreen.touchEnd();
  await sleep(1500);
  const slice = (await logText()).slice(before.length);
  console.log('echoes:', JSON.stringify(slice.match(/KeyDown [A-Z]( |$| \(| )/g) || []),
              'KeyUps:', JSON.stringify(slice.match(/KeyUp [A-Z]/g) || []));
  console.log('any S?', slice.includes('KeyDown S'), '| any W?', slice.includes('KeyDown W'));
  await browser.close();
})().catch((e) => { console.error('DIAG3 ERROR', e); process.exit(2); });
