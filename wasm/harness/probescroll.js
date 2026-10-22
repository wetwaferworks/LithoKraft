// probescroll.js — console scroll-behavior probe (bug-fix run,
// 2026-10-20; JD's field report 16:39 EDT: the diag/log pane snapped to
// the bottom on every new line, so an exception could not be read).
//
// Rule under test (both pages): the log pane auto-follows the tail ONLY
// while the view is at (or within a small threshold of) the bottom;
// once the user scrolls up, new lines must NOT move scrollTop; going
// back to the bottom re-engages follow.
//
// Usage: node probescroll.js <origin> <page>   (page = index.html|gl.html)
const path = require('path');
const puppeteer = require('puppeteer-core');
const CHROME = '/home/hatch/.cache/puppeteer/chrome/linux-155.0.8059.79/chrome-linux64/chrome';

async function main() {
  const origin = process.argv[2];
  const pageName = process.argv[3] || 'gl.html';
  if (!origin) { console.error('usage: probescroll.js <origin> <page>'); process.exit(2); }
  const browser = await puppeteer.launch({
    executablePath: CHROME, headless: 'new',
    args: ['--no-sandbox', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--mute-audio'],
  });
  const page = await browser.newPage();
  await page.goto(origin + '/' + pageName, { waitUntil: 'load', timeout: 60000 });
  await new Promise((r) => setTimeout(r, 3000)); // log pane + toLog exist from parse

  const r = await page.evaluate(() => new Promise((resolve) => {
    const el = document.getElementById('log');
    if (!el || typeof toLog !== 'function') { resolve({ err: 'no #log/toLog' }); return; }
    for (let i = 0; i < 300; i++) toLog('probe fill ' + i);
    requestAnimationFrame(() => {
      el.scrollTop = 0;                      // user scrolls to the top
      // a real user's scroll fires the scroll event async; give it a tick
      setTimeout(() => {
        const before = el.scrollTop;
        for (let i = 0; i < 30; i++) toLog('probe tail ' + i);
        const afterUp = el.scrollTop;
        el.scrollTop = el.scrollHeight;      // back to the bottom
        setTimeout(() => {
          for (let i = 0; i < 30; i++) toLog('probe follow ' + i);
          const gap = el.scrollHeight - el.scrollTop - el.clientHeight;
          resolve({ before, afterUp, gap, scrollHeight: el.scrollHeight });
        }, 60);
      }, 60);
    });
  }));
  console.log('scroll scenario:', JSON.stringify(r));
  await browser.close();
  const held = typeof r.afterUp === 'number' && Math.abs(r.afterUp - r.before) <= 1;
  const follows = typeof r.gap === 'number' && r.gap < 24;
  console.log('held-under-new-lines:', held, ' follow-at-bottom:', follows);
  const pass = held && follows;
  console.log(pass ? 'PROBESCROLL: PASS' : 'PROBESCROLL: FAIL');
  process.exit(pass ? 0 : 1);
}
main().catch((e) => { console.error('PROBESCROLL: ERROR', e && e.message); process.exit(1); });
