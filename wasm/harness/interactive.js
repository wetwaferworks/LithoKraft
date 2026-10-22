// Interactive-input harness for the full-test pages (2026-10-19 fix run):
// boots a page in its default INTERACTIVE mode, waits for the launch menu,
// then drives real input through CDP — click (focus), Enter (menu Play),
// F3 (debug overlay), click (grab), mouse moves (look), held W (move) —
// and proves state change three ways: the game's own log lines, the stats
// panel contents, and canvas pixel differences between stages.
// Usage: node interactive.js <url> <out-prefix> [bootTimeoutSec]
const puppeteer = require('puppeteer-core');
const fs = require('fs');

const url = process.argv[2];
const prefix = process.argv[3] || 'inter';
const bootTimeoutSec = parseInt(process.argv[4] || '300', 10);
const CHROME = '/home/hatch/.cache/puppeteer/chrome/linux-155.0.8059.79/chrome-linux64/chrome';

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

async function waitLog(page, needle, timeoutMs, what) {
  try {
    await page.waitForFunction(
      (n) => document.getElementById('log').textContent.includes(n),
      { timeout: timeoutMs, polling: 500 },
      needle
    );
    return true;
  } catch (e) {
    console.error(`TIMEOUT waiting for ${what} ("${needle}")`);
    return false;
  }
}

async function canvasShot(page, browser, name) {
  const rect = await page.evaluate(() => {
    const r = document.getElementById('canvas').getBoundingClientRect();
    return { x: r.x, y: r.y, w: r.width, h: r.height };
  });
  const buf = await page.screenshot({ type: 'png' });
  fs.writeFileSync(`${prefix}-${name}.png`, buf);
  const auditPage = await browser.newPage();
  await auditPage.goto('about:blank');
  const audit = await auditPage.evaluate(async (dataUrl, rect) => {
    const img = new Image();
    await new Promise((res, rej) => { img.onload = res; img.onerror = rej; img.src = dataUrl; });
    const c = document.createElement('canvas');
    c.width = Math.round(rect.w); c.height = Math.round(rect.h);
    const ctx = c.getContext('2d');
    ctx.drawImage(img, rect.x, rect.y, rect.w, rect.h, 0, 0, c.width, c.height);
    const data = ctx.getImageData(0, 0, c.width, c.height).data;
    const colors = new Set();
    let nonBlack = 0, colorful = 0, sampled = 0, sum = 0;
    for (let i = 0; i < data.length; i += 4 * 7) {
      const r = data[i], g = data[i + 1], b = data[i + 2];
      colors.add((r << 16) | (g << 8) | b);
      if (r + g + b > 24) nonBlack++;
      const mx = Math.max(r, g, b), mn = Math.min(r, g, b);
      if (mx - mn > 24 && mx > 40) colorful++;
      sum += r + g + b;
      sampled++;
    }
    return { distinctSampledColors: colors.size, nonBlackFrac: +(nonBlack / sampled).toFixed(4),
             colorfulFrac: +(colorful / sampled).toFixed(4), meanSum: +(sum / sampled).toFixed(2) };
  }, 'data:image/png;base64,' + buf.toString('base64'), rect);
  await auditPage.close();
  return { rect, audit, png: `${prefix}-${name}.png` };
}

async function pixelDiff(browser, pngA, pngB, rect) {
  const a = fs.readFileSync(pngA).toString('base64');
  const b = fs.readFileSync(pngB).toString('base64');
  const p = await browser.newPage();
  await p.goto('about:blank');
  const frac = await p.evaluate(async (da, db, rect) => {
    const load = (u) => new Promise((res, rej) => { const i = new Image(); i.onload = () => res(i); i.onerror = rej; i.src = u; });
    const [ia, ib] = [await load(da), await load(db)];
    const mk = (img) => {
      const c = document.createElement('canvas');
      c.width = Math.round(rect.w); c.height = Math.round(rect.h);
      c.getContext('2d').drawImage(img, rect.x, rect.y, rect.w, rect.h, 0, 0, c.width, c.height);
      return c.getContext('2d').getImageData(0, 0, c.width, c.height).data;
    };
    const A = mk(ia), B = mk(ib);
    let changed = 0, sampled = 0;
    for (let i = 0; i < A.length; i += 4 * 3) {
      const d = Math.abs(A[i] - B[i]) + Math.abs(A[i + 1] - B[i + 1]) + Math.abs(A[i + 2] - B[i + 2]);
      if (d > 24) changed++;
      sampled++;
    }
    return +(changed / sampled).toFixed(4);
  }, 'data:image/png;base64,' + a, 'data:image/png;base64,' + b, rect);
  await p.close();
  return frac;
}

(async () => {
  const browser = await puppeteer.launch({
    executablePath: CHROME,
    headless: 'new',
    args: [
      '--no-sandbox', '--disable-dev-shm-usage',
      '--enable-unsafe-swiftshader', '--use-gl=angle', '--use-angle=swiftshader',
      '--window-size=1100,1000',
    ],
  });
  const page = await browser.newPage();
  const consoleLines = [];
  page.on('console', (m) => consoleLines.push(`[${m.type()}] ${m.text()}`));
  page.on('pageerror', (e) => consoleLines.push(`[pageerror] ${e.message}`));
  const result = { url, stages: {} };

  await page.goto(url, { waitUntil: 'load', timeout: 60000 });

  result.stages.menuReached = await waitLog(page, 'menu: launch menu', bootTimeoutSec * 1000, 'launch menu');
  result.stages.backendLine = await page.evaluate(() =>
    document.getElementById('log').textContent.split('\n').find((l) => l.includes('input:')) || null);
  await sleep(2500);  // let the menu dolly + first frames settle
  const menuShot = await canvasShot(page, browser, 'menu');
  result.stages.menuShot = menuShot.audit;

  const rect = menuShot.rect;
  const cx = rect.x + rect.w / 2, cy = rect.y + rect.h / 2;

  // Stage 1: click the canvas (focus) then Enter -> menu Play.
  await page.mouse.click(cx, cy);
  await sleep(400);
  await page.keyboard.press('Enter');
  result.stages.playReached = await waitLog(page, 'menu: play', 90000, 'menu play after Enter');
  await sleep(1500);
  const playShot = await canvasShot(page, browser, 'play');
  result.stages.playShot = playShot.audit;

  // Stage 2: F3 debug overlay on (visible state), click to grab, look, move.
  await page.keyboard.press('F3');
  await sleep(600);
  await page.mouse.click(cx, cy);
  await sleep(600);
  result.stages.pointerLockElement = await page.evaluate(() =>
    document.pointerLockElement ? document.pointerLockElement.id : null);
  await page.mouse.move(cx, cy);
  await page.mouse.move(cx + 240, cy + 40, { steps: 12 });
  await sleep(600);
  await page.keyboard.down('KeyW');
  await sleep(1500);
  await page.keyboard.up('KeyW');
  await sleep(800);
  const moveShot = await canvasShot(page, browser, 'moved');
  result.stages.moveShot = moveShot.audit;

  // Evidence FIRST (log/console/stats), pixel diffs second — a diff-stage
  // failure must never cost us the run's evidence.
  const logText = await page.evaluate(() => document.getElementById('log').textContent);
  fs.writeFileSync(`${prefix}-log.txt`, logText);
  fs.writeFileSync(`${prefix}-console.txt`, consoleLines.join('\n'));
  try {
    result.stages.diffMenuVsPlay = await pixelDiff(browser, menuShot.png, playShot.png, rect);
    result.stages.diffPlayVsMoved = await pixelDiff(browser, playShot.png, moveShot.png, rect);
  } catch (e) {
    result.stages.diffError = String(e && e.message || e);
  }
  result.stats = await page.evaluate(() => {
    const g = (id) => document.getElementById(id).textContent;
    return { release: g('st-release'), counter: g('st-counter'), renderer: g('st-renderer'),
             mode: g('st-mode'), fps: g('st-fps'), input: g('st-input'), lock: g('st-lock'),
             bench: g('st-bench'), tick: g('st-tick') };
  });
  result.logEvidence = {
    backendLive: logText.includes('emscripten-html5 backend live'),
    debugEchoOn: logText.includes('debug echo ON'),
    keyDownEnter: /input> KeyDown Enter/.test(logText),
    keyDownW: /input> KeyDown W/.test(logText),
    mouseDown: /input> MouseDown/.test(logText),
    mouseMove: /input> MouseMove/.test(logText),
    lockLines: logText.split('\n').filter((l) => l.includes('pointer lock')),
  };
  result.consoleErrors = consoleLines.filter((l) => /error|GL_INVALID|glReadPixels/i.test(l));
  result.glErrors = consoleLines.filter((l) => /GL_INVALID_OPERATION|glReadPixelsRobustANGLE/.test(l));

  console.log(JSON.stringify(result, null, 2));
  await browser.close();
})().catch((e) => { console.error('HARNESS FAIL', e); process.exit(1); });
