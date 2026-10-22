// Diagnostic: are pointermove events reaching the touch.js handlers
// on the real game page? Instruments the live overlay elements.
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
    userAgent: 'diag',
  });
  page.on('pageerror', (e) => console.log('PAGEERROR:', e.message));
  await page.goto('http://127.0.0.1:8931/gl.html?touch=1&debuginput=1', { waitUntil: 'load', timeout: 120000 });
  const logText = () => page.evaluate(() => document.getElementById('log').textContent);
  const t0 = Date.now();
  while (Date.now() - t0 < 300000) {
    const t = await logText().catch(() => '');
    if (t.includes('backend live')) break;
    await sleep(1000);
  }
  console.log('booted');
  const diag = await page.evaluate(() => {
    window.__dc = { joyDown: 0, joyMove: 0, joyCancel: 0, lookDown: 0, lookMove: 0, lookCancel: 0, docKeys: [] };
    const joy = document.getElementById('lk-joy');
    const look = document.getElementById('lk-look');
    joy.addEventListener('pointerdown', () => window.__dc.joyDown++);
    joy.addEventListener('pointermove', () => window.__dc.joyMove++);
    joy.addEventListener('pointercancel', () => window.__dc.joyCancel++);
    look.addEventListener('pointerdown', () => window.__dc.lookDown++);
    look.addEventListener('pointermove', () => window.__dc.lookMove++);
    look.addEventListener('pointercancel', () => window.__dc.lookCancel++);
    document.addEventListener('keydown', (e) => window.__dc.docKeys.push(e.code));
    return {
      joyTouchAction: getComputedStyle(joy).touchAction,
      lookTouchAction: getComputedStyle(look).touchAction,
      joyRect: JSON.stringify(joy.getBoundingClientRect()),
      scrollY: window.scrollY, docH: document.documentElement.scrollHeight, innerH: window.innerHeight,
    };
  });
  console.log('diag setup:', JSON.stringify(diag));
  const joyBox = await (await page.$('#lk-joy')).boundingBox();
  const jx = joyBox.x + joyBox.width / 2, jy = joyBox.y + joyBox.height / 2;
  console.log('joy gesture at', jx, jy);
  await page.touchscreen.touchStart(jx, jy);
  await sleep(200);
  await page.touchscreen.touchMove(jx, jy - 55);
  await sleep(500);
  await page.touchscreen.touchEnd();
  await sleep(400);
  console.log('after joy gesture:', JSON.stringify(await page.evaluate(() => window.__dc)));
  console.log('scrollY now:', await page.evaluate(() => window.scrollY));
  await browser.close();
})().catch((e) => { console.error('DIAG ERROR', e); process.exit(2); });
