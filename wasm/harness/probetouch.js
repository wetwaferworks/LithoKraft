// Touch-controls probe (2026-10-20). Boots ONE full-test page in
// headless Chrome with touch capability, forces the overlay with
// ?touch=1 (and ?debuginput=1 for the backend's input> echoes), then
// drives the overlay with CDP touch and asserts, from the page log:
//   overlay present (window.LK_TOUCH_UI + #lktouch in the DOM)
//   ENTER button -> input> KeyDown Enter, then "menu: play"
//   joystick up  -> input> KeyDown W (code KeyW); release -> KeyUp W
//   look drag    -> input> MouseMove +dx lines (position-delta look)
//   BREAK hold   -> input> MouseDown Left ... MouseUp Left
//   PLACE hold   -> input> MouseDown Right ... MouseUp Right
//   JUMP hold    -> input> KeyDown Space (code Space)
//   ESC tap      -> input> KeyDown Escape (code Escape)
//   zero PAGEERROR lines throughout
// Usage: node probetouch.js <page> [tag]    e.g. node probetouch.js gl.html
const fs = require('fs');
const puppeteer = require('puppeteer-core');

const page0 = process.argv[2] || 'gl.html';
const tag = process.argv[3] || ('touch-' + page0.replace('.html', ''));
const BASE = 'http://127.0.0.1:8931/';
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
let failures = 0;
function check(name, ok, extra) {
  console.log((ok ? 'PASS ' : 'FAIL ') + name + (extra ? ' — ' + extra : ''));
  if (!ok) failures++;
}

async function main() {
  const browser = await puppeteer.launch({
    executablePath: '/home/hatch/.cache/puppeteer/chrome/linux-155.0.8059.79/chrome-linux64/chrome',
    headless: 'new',
    args: ['--no-sandbox', '--disable-dev-shm-usage', '--use-gl=angle', '--use-angle=swiftshader',
           '--enable-unsafe-swiftshader', '--window-size=1100,1000'],
  });
  const page = await browser.newPage();
  // WEBVIEW EMULATION: the Android WebView has no Pointer Lock API.
  // Remove it before any page script runs so the engine's capture
  // requests no-op exactly as they do on the phone. (With the API
  // present, desktop Chrome locks on play, the engine re-locks every
  // 0.5 s per window_web_legacy.cpp's frame loop, and Chrome then
  // reports clientX/Y = 0,0 for pointer events on overlay elements —
  // probetouchdiag5/6 — a desktop-only artifact the phone never sees.)
  await page.evaluateOnNewDocument(() => {
    try { delete Element.prototype.requestPointerLock; } catch (e) { /* noop */ }
    Element.prototype.requestPointerLock = undefined;
  });
  await page.emulate({
    viewport: { width: 1100, height: 1000, hasTouch: true, isMobile: false, deviceScaleFactor: 1 },
    userAgent: 'Mozilla/5.0 (Linux; Android 14) AppleWebKit/537.36 Chrome/155 Mobile Safari/537.36 LithoTouchProbe',
  });
  const consoleLines = [];
  page.on('console', (m) => consoleLines.push(m.type() + ': ' + m.text()));
  page.on('pageerror', (e) => consoleLines.push('PAGEERROR: ' + e.message));

  const url = BASE + page0 + '?touch=1&debuginput=1';
  console.log('opening', url);
  await page.goto(url, { waitUntil: 'load', timeout: 120000 });
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
  async function logSince(needle, from) {
    const t = await logText().catch(() => '');
    return t.slice(from).includes(needle);
  }
  // Poll-wait variant: under SwiftShader the page log can lag the
  // gesture by seconds, so single-shot checks false-negative.
  async function waitSince(needle, from, timeoutS) {
    const t0 = Date.now();
    while (Date.now() - t0 < (timeoutS || 25) * 1000) {
      if (await logSince(needle, from)) return true;
      await sleep(400);
    }
    return false;
  }
  const logLen = async () => (await logText().catch(() => '')).length;

  check('boot: backend live', await waitForLog('backend live', 300));
  const overlay = await page.evaluate(() => ({
    flag: window.LK_TOUCH_UI, dom: !!document.getElementById('lktouch'),
    joy: !!document.getElementById('lk-joy'), enter: !!document.getElementById('lk-enter'),
  }));
  check('overlay present (?touch=1)', overlay.flag === true && overlay.dom && overlay.joy && overlay.enter,
        JSON.stringify(overlay));
  check('menu reached', await waitForLog('menu: launch menu', 240));
  await page.screenshot({ path: `${tag}-menu.png` });

  const box = async (sel) => {
    const el = await page.$(sel);
    if (!el) throw new Error('missing element ' + sel);
    const b = await el.boundingBox();
    if (!b) throw new Error('no bounding box for ' + sel);
    return { x: b.x + b.width / 2, y: b.y + b.height / 2, w: b.width, h: b.height };
  };
  async function tap(sel, holdMs) {
    const c = await box(sel);
    await page.touchscreen.touchStart(c.x, c.y);
    await sleep(holdMs || 120);
    await page.touchscreen.touchEnd();
    await sleep(250);
  }

  // ENTER -> play
  let mark = await logLen();
  await tap('#lk-enter', 150);
  check('ENTER button: KeyDown Enter echo', await (async () => {
    const t0 = Date.now();
    while (Date.now() - t0 < 20000) { if (await logSince('KeyDown Enter', mark)) return true; await sleep(400); }
    return false;
  })());
  check('ENTER button: menu -> play', await waitForLog('menu: play', 90));
  await sleep(1200);
  await page.screenshot({ path: `${tag}-play.png` });
  // The lock must never have engaged (API removed above, as on the
  // phone): play state runs on the position-delta look fallback.
  const lockAfter = await page.evaluate(() => document.pointerLockElement ? 'locked' : 'free');
  check('no pointer lock in play (WebView condition)', lockAfter === 'free', lockAfter);

  // Joystick up (forward)
  mark = await logLen();
  const joy = await box('#lk-joy');
  await page.touchscreen.touchStart(joy.x, joy.y);
  await page.touchscreen.touchMove(joy.x, joy.y - 55);
  await sleep(700);
  check('joystick up: KeyDown W echo', await waitSince('KeyDown W (code KeyW)', mark, 25));
  await page.touchscreen.touchEnd();
  await sleep(400);
  check('joystick release: KeyUp W echo', await waitSince('KeyUp W', mark, 25));

  // Look drag (right zone): start mid-right, drag +140 x
  mark = await logLen();
  const cv = await box('#canvas');
  const lx = cv.x + cv.w * 0.32, ly = cv.y;
  await page.touchscreen.touchStart(lx, ly);
  for (let i = 1; i <= 7; i++) { await page.touchscreen.touchMove(lx + i * 20, ly); await sleep(60); }
  await sleep(400);
  const moveOk = await (async () => {
    const t0 = Date.now();
    while (Date.now() - t0 < 25000) {
      const t = (await logText()).slice(mark);
      const m = t.match(/MouseMove \+\d+ [+-]?\d+/g);
      if (m && m.length >= 2) return true;
      await sleep(400);
    }
    return false;
  })();
  check('look drag: MouseMove +dx echoes', moveOk);
  await page.touchscreen.touchEnd();
  await sleep(300);

  // BREAK hold / release
  mark = await logLen();
  await tap('#lk-break', 600);
  check('BREAK hold: MouseDown Left echo', await logSince('MouseDown Left', mark));
  check('BREAK release: MouseUp Left echo', await logSince('MouseUp Left', mark));

  // PLACE hold / release
  mark = await logLen();
  await tap('#lk-place', 600);
  check('PLACE hold: MouseDown Right echo', await logSince('MouseDown Right', mark));
  check('PLACE release: MouseUp Right echo', await logSince('MouseUp Right', mark));

  // JUMP hold
  mark = await logLen();
  await tap('#lk-jump', 500);
  check('JUMP hold: KeyDown Space echo', await logSince('KeyDown Space (code Space)', mark));

  // ESC tap
  mark = await logLen();
  await tap('#lk-esc', 150);
  check('ESC tap: KeyDown Escape echo', await logSince('KeyDown Escape (code Escape)', mark));

  const pageErrors = consoleLines.filter((l) => l.startsWith('PAGEERROR'));
  check('no page errors', pageErrors.length === 0, pageErrors.slice(0, 3).join(' | '));
  fs.writeFileSync(`${tag}-console.txt`, consoleLines.join('\n'));
  await browser.close();
  console.log(failures === 0 ? 'PROBE RESULT: ALL PASS' : `PROBE RESULT: ${failures} FAILURES`);
  process.exit(failures === 0 ? 0 : 1);
}
main().catch((e) => { console.error('PROBE ERROR', e); process.exit(2); });
