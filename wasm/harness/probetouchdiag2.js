// Diagnostic 2: ground truth from the page log + ring indices around
// a joystick-W gesture in PLAY state.
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
    userAgent: 'diag2',
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
  const ring = () => page.evaluate(() => {
    try {
      var v = window.__lkRingView;
      if (!v) return { ring: 'noview', on: window.__lkRingOn, q: (window.__lkQ || []).length };
      return { on: window.__lkRingOn, w: Atomics.load(v, 1), r: Atomics.load(v, 2), dropped: Atomics.load(v, 3), q: (window.__lkQ || []).length };
    } catch (e) { return { err: String(e) }; }
  });
  console.log('boot:', await waitForLog('backend live', 300));
  console.log('menu:', await waitForLog('menu: launch menu', 240));
  console.log('ring at menu:', JSON.stringify(await ring()));

  // W gesture in MENU state
  const joyEl = await page.$('#lk-joy');
  const jb = await joyEl.boundingBox();
  const jx = jb.x + jb.width / 2, jy = jb.y + jb.height / 2;
  let before = await logText();
  await page.touchscreen.touchStart(jx, jy);
  await page.touchscreen.touchMove(jx, jy - 55);
  await sleep(800);
  await page.touchscreen.touchEnd();
  await sleep(1500);
  let after = await logText();
  console.log('MENU gesture: KeyDown W echo present:', after.slice(before.length).includes('KeyDown W'));
  console.log('ring after menu gesture:', JSON.stringify(await ring()));

  // ENTER via overlay -> play
  const enEl = await page.$('#lk-enter');
  const eb = await enEl.boundingBox();
  await page.touchscreen.touchStart(eb.x + eb.width / 2, eb.y + eb.height / 2);
  await sleep(150);
  await page.touchscreen.touchEnd();
  console.log('play:', await waitForLog('menu: play', 90));
  await sleep(1500);
  console.log('ring in play:', JSON.stringify(await ring()));
  console.log('pointerLockElement:', await page.evaluate(() => document.pointerLockElement ? document.pointerLockElement.id : null));

  // W gesture in PLAY state
  before = await logText();
  await page.touchscreen.touchStart(jx, jy);
  await page.touchscreen.touchMove(jx, jy - 55);
  await sleep(800);
  const midRing = await ring();
  await page.touchscreen.touchEnd();
  await sleep(2000);
  after = await logText();
  const slice = after.slice(before.length);
  console.log('PLAY gesture: KeyDown W echo present:', slice.includes('KeyDown W'));
  console.log('PLAY gesture: any KeyDown lines:', (slice.match(/KeyDown[^\n]*/g) || []).slice(0, 5));
  console.log('PLAY gesture: any input> lines:', (slice.match(/input>[^\n]*/g) || []).slice(0, 8));
  console.log('ring during/after play gesture:', JSON.stringify(midRing), JSON.stringify(await ring()));

  // Direct synthetic KeyW via evaluate (no overlay)
  before = await logText();
  await page.evaluate(() => {
    document.dispatchEvent(new KeyboardEvent('keydown', { code: 'KeyW', key: 'w', bubbles: true }));
    setTimeout(() => document.dispatchEvent(new KeyboardEvent('keyup', { code: 'KeyW', key: 'w', bubbles: true })), 300);
  });
  await sleep(2000);
  after = await logText();
  console.log('PLAY direct synthetic: KeyDown W echo:', after.slice(before.length).includes('KeyDown W'));

  // Real CDP keyboard KeyW
  before = await logText();
  await page.keyboard.down('KeyW');
  await sleep(600);
  await page.keyboard.up('KeyW');
  await sleep(1500);
  after = await logText();
  console.log('PLAY real keyboard: KeyDown W echo:', after.slice(before.length).includes('KeyDown W'));
  await browser.close();
})().catch((e) => { console.error('DIAG2 ERROR', e); process.exit(2); });
