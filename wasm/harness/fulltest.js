// Full-test harness: load a page, wait for the bench run to finish,
// screenshot the page (the canvas is an OffscreenCanvas owned by the
// proxied pthread, so in-page readback is impossible — audit the CDP
// screenshot instead, clipped to the canvas element's rect).
// Usage: node fulltest.js <url> <out-prefix> [timeoutSec]
const puppeteer = require('puppeteer-core');
const fs = require('fs');

const url = process.argv[2];
const prefix = process.argv[3] || 'shot';
const timeoutSec = parseInt(process.argv[4] || '240', 10);
const CHROME = '/home/hatch/.cache/puppeteer/chrome/linux-155.0.8059.79/chrome-linux64/chrome';

(async () => {
  const browser = await puppeteer.launch({
    executablePath: CHROME,
    headless: 'new',
    args: [
      '--no-sandbox', '--disable-dev-shm-usage',
      '--enable-unsafe-swiftshader', '--use-gl=angle', '--use-angle=swiftshader',
      '--window-size=1100,900',
    ],
  });
  const page = await browser.newPage();
  const consoleLines = [];
  page.on('console', (m) => consoleLines.push(`[${m.type()}] ${m.text()}`));
  page.on('pageerror', (e) => consoleLines.push(`[pageerror] ${e.message}`));
  await page.goto(url, { waitUntil: 'load', timeout: 60000 });

  let finished = false;
  try {
    await page.waitForFunction(
      () => {
        const s = document.getElementById('status');
        const l = document.getElementById('log');
        return (s && (s.textContent.includes('run finished') || s.textContent.startsWith('error:'))) ||
               (l && /\] exit\s*$/.test(l.textContent));
      },
      { timeout: timeoutSec * 1000, polling: 1000 }
    );
    finished = true;
  } catch (e) {
    consoleLines.push('[harness] TIMEOUT waiting for run finish');
  }
  await new Promise((r) => setTimeout(r, 1500));

  const status = await page.evaluate(() => document.getElementById('status').textContent);
  const logText = await page.evaluate(() => document.getElementById('log').textContent);
  const rect = await page.evaluate(() => {
    const r = document.getElementById('canvas').getBoundingClientRect();
    return { x: r.x, y: r.y, w: r.width, h: r.height };
  });
  const shotBuf = await page.screenshot({ type: 'png' });
  fs.writeFileSync(`${prefix}.png`, shotBuf);
  fs.writeFileSync(`${prefix}-log.txt`, logText);
  fs.writeFileSync(`${prefix}-console.txt`, consoleLines.join('\n'));

  // Audit the canvas region of the screenshot in a scratch page.
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
    let nonBlack = 0, colorful = 0, sampled = 0;
    for (let i = 0; i < data.length; i += 4 * 7) {
      const r = data[i], g = data[i + 1], b = data[i + 2];
      colors.add((r << 16) | (g << 8) | b);
      if (r + g + b > 24) nonBlack++;
      const mx = Math.max(r, g, b), mn = Math.min(r, g, b);
      if (mx - mn > 24 && mx > 40) colorful++;
      sampled++;
    }
    return {
      via: 'cdp-screenshot-clip', rect,
      distinctSampledColors: colors.size,
      nonBlackFrac: +(nonBlack / sampled).toFixed(4),
      colorfulFrac: +(colorful / sampled).toFixed(4),
    };
  }, 'data:image/png;base64,' + shotBuf.toString('base64'), rect);

  console.log(JSON.stringify({ url, finished, status, audit }, null, 2));
  await browser.close();
})().catch((e) => { console.error('HARNESS FAIL', e); process.exit(1); });
