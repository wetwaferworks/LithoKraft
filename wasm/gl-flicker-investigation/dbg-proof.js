// dbg-proof.js <url> <outprefix> — pcproof-shaped, plus window.__dbg dump
'use strict';
const puppeteer = require('/home/hatch/workspace/torch-receive-2026-10-17/tree/wasm/harness/node_modules/puppeteer-core');
const CHROME = '/home/hatch/.cache/puppeteer/chrome/linux-155.0.8059.79/chrome-linux64/chrome';
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
(async () => {
  const url = process.argv[2], out = process.argv[3];
  const browser = await puppeteer.launch({ executablePath: CHROME, headless: 'new',
    args: ['--no-sandbox', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--mute-audio',
           '--window-size=1280,800'] });
  try {
    const page = await browser.newPage();
    await page.setViewport({ width: 1280, height: 800 });
    const errors = [];
    page.on('pageerror', (e) => errors.push(String(e).slice(0, 160)));
    await page.goto(url, { waitUntil: 'load', timeout: 60000 });
    const log = () => page.evaluate(() => document.getElementById('log').textContent).catch(() => '');
    let t0 = Date.now(), L = '';
    while (Date.now() - t0 < 240000) { L = await log(); if (/menu: launch menu/.test(L)) break; await sleep(2000); }
    console.log('menu reached:', /menu: launch menu/.test(L));
    await page.screenshot({ path: out + '-menu.png' });
    await page.keyboard.press('Enter');
    await sleep(45000);
    await page.screenshot({ path: out + '-settled.png' });
    const dbg = await page.evaluate(() => window.__dbg || null).catch(() => null);
    console.log('__dbg:', JSON.stringify(dbg));
    L = await log();
    const perf = L.match(/perf fps=[^\n]+/g) || [];
    console.log('final perf:', perf.length ? perf[perf.length - 1] : '(none)');
    console.log('pageerrors:', errors.length, JSON.stringify(errors.slice(0, 4)));
  } finally { await browser.close(); }
  console.log('DBGPROOF DONE');
})().catch((e) => { console.error('DBGPROOF FAIL', e && e.message); process.exit(1); });
