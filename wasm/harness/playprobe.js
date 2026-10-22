// Play-trigger probe: boots the SW page interactively, then tries, in
// order, (1) a real mouse click on the Play button's computed position,
// (2) Space, (3) Tab then Enter — watching the game log after each.
const puppeteer = require('puppeteer-core');
const fs = require('fs');
const CHROME = '/home/hatch/.cache/puppeteer/chrome/linux-155.0.8059.79/chrome-linux64/chrome';
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const url = process.argv[2];
(async () => {
  const browser = await puppeteer.launch({ executablePath: CHROME, headless: 'new',
    args: ['--no-sandbox', '--disable-dev-shm-usage', '--enable-unsafe-swiftshader', '--use-gl=angle', '--use-angle=swiftshader', '--window-size=1100,1000'] });
  const page = await browser.newPage();
  page.on('pageerror', (e) => console.log('[pageerror]', e.message));
  await page.goto(url, { waitUntil: 'load', timeout: 60000 });
  await page.waitForFunction(() => document.getElementById('log').textContent.includes('menu: launch menu'), { timeout: 300000, polling: 500 });
  console.log('menu reached');
  await sleep(3000);
  const logHas = async (s) => page.evaluate((n) => document.getElementById('log').textContent.includes(n), s);
  const rect = await page.evaluate(() => { const r = document.getElementById('canvas').getBoundingClientRect(); return { x: r.x, y: r.y, w: r.width, h: r.height }; });
  console.log('canvas rect', JSON.stringify(rect));
  // (1) click Play: fb center-stack anchor (0.5, 0.44), Play is the 4th row;
  // try a small vertical sweep of candidate positions around 0.50-0.62 height.
  for (const fy of [0.52, 0.56, 0.60, 0.48]) {
    await page.mouse.click(rect.x + rect.w * 0.5, rect.y + rect.h * fy);
    await sleep(2500);
    if (await logHas('menu: play')) { console.log('PLAY via click at fy=' + fy); break; }
    console.log('click fy=' + fy + ' -> no play');
  }
  if (!(await logHas('menu: play'))) {
    await page.keyboard.press('Space');
    await sleep(2500);
    console.log('after Space: play=' + (await logHas('menu: play')));
  }
  if (!(await logHas('menu: play'))) {
    await page.keyboard.press('Tab');
    await sleep(800);
    await page.keyboard.press('Enter');
    await sleep(2500);
    console.log('after Tab+Enter: play=' + (await logHas('menu: play')));
  }
  const log = await page.evaluate(() => document.getElementById('log').textContent);
  fs.writeFileSync('playprobe-log.txt', log);
  console.log('final play state:', await logHas('menu: play'));
  await browser.close();
})().catch((e) => { console.error('FAIL', e); process.exit(1); });
