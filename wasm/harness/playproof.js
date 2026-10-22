// Full play proof: menu -> (bare Enter) -> play -> click (grab) -> look -> move.
// Evidence: game log lines (menu: play, pointer lock engaged), perf tps>0,
// debug echoes, and screenshots at menu/play/moved stages.
const puppeteer = require('puppeteer-core');
const fs = require('fs');
const CHROME = '/home/hatch/.cache/puppeteer/chrome/linux-155.0.8059.79/chrome-linux64/chrome';
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const url = process.argv[2];
const prefix = process.argv[3] || 'playproof';
(async () => {
  const browser = await puppeteer.launch({ executablePath: CHROME, headless: 'new',
    args: ['--no-sandbox', '--disable-dev-shm-usage', '--enable-unsafe-swiftshader', '--use-gl=angle', '--use-angle=swiftshader', '--window-size=1100,1000'] });
  const page = await browser.newPage();
  const consoleLines = [];
  page.on('console', (m) => consoleLines.push(`[${m.type()}] ${m.text()}`));
  page.on('pageerror', (e) => consoleLines.push(`[pageerror] ${e.message}`));
  const logHas = async (s) => page.evaluate((n) => document.getElementById('log').textContent.includes(n), s).catch(() => false);
  const result = {};
  await page.goto(url, { waitUntil: 'load', timeout: 60000 });
  await page.waitForFunction(() => document.getElementById('log').textContent.includes('menu: launch menu'), { timeout: 300000, polling: 500 });
  result.menu = true;
  await sleep(2000);
  await page.screenshot({ path: `${prefix}-menu.png` });
  await page.keyboard.press('Enter');
  result.play = await page.waitForFunction(() => document.getElementById('log').textContent.includes('menu: play'), { timeout: 60000, polling: 500 }).then(() => true).catch(() => false);
  await sleep(3000);
  await page.screenshot({ path: `${prefix}-play.png` });
  const rect = await page.evaluate(() => { const r = document.getElementById('canvas').getBoundingClientRect(); return { x: r.x, y: r.y, w: r.width, h: r.height }; });
  const cx = rect.x + rect.w / 2, cy = rect.y + rect.h / 2;
  await page.mouse.click(cx, cy);
  await sleep(2500);
  result.lockEngaged = await logHas('input: pointer lock engaged');
  result.lockElement = await page.evaluate(() => document.pointerLockElement ? document.pointerLockElement.id : null);
  await page.mouse.move(cx, cy);
  await page.mouse.move(cx + 260, cy + 50, { steps: 12 });
  await sleep(800);
  await page.keyboard.down('KeyW');
  await sleep(2000);
  await page.keyboard.up('KeyW');
  await sleep(1200);
  await page.screenshot({ path: `${prefix}-moved.png` });
  const log = await page.evaluate(() => document.getElementById('log').textContent);
  fs.writeFileSync(`${prefix}-log.txt`, log);
  fs.writeFileSync(`${prefix}-console.txt`, consoleLines.join('\n'));
  const perf = log.split('\n').filter((l) => l.includes('perf fps')).slice(-3);
  result.lastPerf = perf;
  result.tpsLive = perf.some((l) => /tps=[1-9]/.test(l));
  result.echoes = {
    enter: /input> KeyDown Enter/.test(log), w: /input> KeyDown W/.test(log),
    mouseDown: /input> MouseDown/.test(log), mouseMove: /input> MouseMove/.test(log),
  };
  result.glErrors = consoleLines.filter((l) => /GL_INVALID_OPERATION|glReadPixelsRobustANGLE/.test(l));
  result.stats = await page.evaluate(() => {
    const g = (id) => document.getElementById(id).textContent;
    return { release: g('st-release'), counter: g('st-counter'), renderer: g('st-renderer'), input: g('st-input'), lock: g('st-lock') };
  });
  console.log(JSON.stringify(result, null, 2));
  await browser.close();
})().catch((e) => { console.error('FAIL', e); process.exit(1); });
