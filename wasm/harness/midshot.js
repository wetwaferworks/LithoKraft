// Mid-run presentation probe: boot bench mode, screenshot DURING the run
// (before the process exits), then again after exit; audit both.
const puppeteer = require('puppeteer-core');
const fs = require('fs');
const CHROME = '/home/hatch/.cache/puppeteer/chrome/linux-155.0.8059.79/chrome-linux64/chrome';
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
(async () => {
  const browser = await puppeteer.launch({ executablePath: CHROME, headless: 'new',
    args: ['--no-sandbox', '--disable-dev-shm-usage', '--enable-unsafe-swiftshader', '--use-gl=angle', '--use-angle=swiftshader', '--window-size=1100,1000'] });
  const page = await browser.newPage();
  await page.goto(process.argv[2], { waitUntil: 'load', timeout: 60000 });
  // Wait until the world is loaded and the bench is running, but NOT finished.
  await page.waitForFunction(() => document.getElementById('log').textContent.includes('bench renderer='), { timeout: 300000, polling: 500 }).catch(() => console.log('no bench line yet'));
  const running = await page.evaluate(() => !document.getElementById('status').textContent.includes('run finished'));
  await page.screenshot({ path: 'midshot-during.png' });
  console.log('during-run screenshot taken; still running:', running);
  await page.waitForFunction(() => document.getElementById('status').textContent.includes('run finished'), { timeout: 300000, polling: 1000 }).catch(() => console.log('no finish'));
  await sleep(1500);
  await page.screenshot({ path: 'midshot-after.png' });
  console.log('after-exit screenshot taken');
  await browser.close();
})().catch((e) => { console.error('FAIL', e); process.exit(1); });
