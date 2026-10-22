const puppeteer = require('puppeteer-core');
const fs = require('fs');
const CHROME = '/home/hatch/.cache/puppeteer/chrome/linux-155.0.8059.79/chrome-linux64/chrome';
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
(async () => {
  const browser = await puppeteer.launch({ executablePath: CHROME, headless: 'new',
    args: ['--no-sandbox', '--disable-dev-shm-usage', '--enable-unsafe-swiftshader', '--use-gl=angle', '--use-angle=swiftshader', '--window-size=1100,1000'] });
  const page = await browser.newPage();
  page.on('pageerror', (e) => console.log('[pageerror]', e.message));
  await page.goto(process.argv[2], { waitUntil: 'load', timeout: 60000 });
  await page.waitForFunction(() => document.getElementById('log').textContent.includes('menu: launch menu'), { timeout: 300000, polling: 500 });
  console.log('menu reached — pressing Enter with NO prior click');
  await sleep(2000);
  await page.keyboard.press('Enter');
  await sleep(4000);
  let played = await page.evaluate(() => document.getElementById('log').textContent.includes('menu: play'));
  console.log('play after bare Enter:', played);
  if (!played) {
    await page.keyboard.press('Space');
    await sleep(3000);
    played = await page.evaluate(() => document.getElementById('log').textContent.includes('menu: play'));
    console.log('play after bare Space:', played);
  }
  const log = await page.evaluate(() => document.getElementById('log').textContent);
  fs.writeFileSync('enterprobe-log.txt', log);
  await browser.close();
})().catch((e) => { console.error('FAIL', e); process.exit(1); });
