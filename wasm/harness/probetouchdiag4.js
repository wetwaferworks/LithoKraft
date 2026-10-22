// Diagnostic 4: in PLAY state (pointer lock engaged), count what the
// overlay elements actually receive during a joystick gesture.
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
    userAgent: 'diag4',
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
  console.log('boot:', await waitForLog('backend live', 300));
  console.log('menu:', await waitForLog('menu: launch menu', 240));
  const enEl = await page.$('#lk-enter');
  const eb = await enEl.boundingBox();
  await page.touchscreen.touchStart(eb.x + eb.width / 2, eb.y + eb.height / 2);
  await sleep(150);
  await page.touchscreen.touchEnd();
  console.log('play:', await waitForLog('menu: play', 90));
  await sleep(1000);

  await page.evaluate(() => {
    window.__dc = { joyDown: 0, joyMove: 0, joyUp: 0, joyCancel: 0, docKeys: [], canvasMoves: 0, docMoves: 0 };
    const joy = document.getElementById('lk-joy');
    joy.addEventListener('pointerdown', () => window.__dc.joyDown++);
    joy.addEventListener('pointermove', () => window.__dc.joyMove++);
    joy.addEventListener('pointerup', () => window.__dc.joyUp++);
    joy.addEventListener('pointercancel', () => window.__dc.joyCancel++);
    document.addEventListener('keydown', (e) => window.__dc.docKeys.push(e.code));
    document.getElementById('canvas').addEventListener('mousemove', () => window.__dc.canvasMoves++);
    document.addEventListener('mousemove', () => window.__dc.docMoves++);
  });
  const jb = await (await page.$('#lk-joy')).boundingBox();
  const jx = jb.x + jb.width / 2, jy = jb.y + jb.height / 2;
  await page.touchscreen.touchStart(jx, jy);
  await sleep(200);
  console.log('after touchStart:', JSON.stringify(await page.evaluate(() => window.__dc)));
  await page.touchscreen.touchMove(jx, jy - 55);
  await sleep(600);
  console.log('after touchMove:', JSON.stringify(await page.evaluate(() => window.__dc)));
  await page.touchscreen.touchEnd();
  await sleep(400);
  console.log('after touchEnd:', JSON.stringify(await page.evaluate(() => window.__dc)));
  await browser.close();
})().catch((e) => { console.error('DIAG4 ERROR', e); process.exit(2); });
