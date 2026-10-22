// Black-screen diagnosis probe (2026-10-20). Boots ONE full-test page from
// the shell APK's exact asset set under a WebView-class configuration
// variant and MEASURES presentation: canvas-region screenshot luminance
// (decoded in a helper page — never eyeballed), plus the engine log
// marker timeline and console capture.
//
// Usage: node probeblack.js <page> <variant> <tag> [port]
//   page:    index.html | gl.html
//   variant: base        — desktop-like (OffscreenCanvas transfer intact)
//            notransfer  — HTMLCanvasElement.transferControlToOffscreen removed
//            nooffscreen — transferControlToOffscreen AND window.OffscreenCanvas removed
const fs = require('fs');
const puppeteer = require('puppeteer-core');

const page0 = process.argv[2] || 'gl.html';
const variant = process.argv[3] || 'base';
const tag = process.argv[4] || ('black-' + variant + '-' + page0.replace('.html', ''));
const PORT = process.argv[5] || '8933';
const BASE = 'http://127.0.0.1:' + PORT + '/';
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

async function main() {
  const browser = await puppeteer.launch({
    executablePath: '/home/hatch/.cache/puppeteer/chrome/linux-155.0.8059.79/chrome-linux64/chrome',
    headless: 'new',
    args: ['--no-sandbox', '--disable-dev-shm-usage', '--use-gl=angle', '--use-angle=swiftshader',
           '--enable-unsafe-swiftshader', '--window-size=1100,1000'],
  });
  const page = await browser.newPage();
  // Phone emulation (same as probetouch): no Pointer Lock API in WebView.
  await page.evaluateOnNewDocument(() => {
    try { delete Element.prototype.requestPointerLock; } catch (e) {}
    Element.prototype.requestPointerLock = undefined;
  });
  if (variant === 'notransfer' || variant === 'nooffscreen') {
    await page.evaluateOnNewDocument(() => {
      try { delete HTMLCanvasElement.prototype.transferControlToOffscreen; } catch (e) {}
      HTMLCanvasElement.prototype.transferControlToOffscreen = undefined;
      window.__lkVariant = 'transfer-removed';
    });
  }
  if (variant === 'nooffscreen') {
    await page.evaluateOnNewDocument(() => {
      try { delete window.OffscreenCanvas; } catch (e) {}
      window.OffscreenCanvas = undefined;
      window.__lkVariant = 'offscreen-removed';
    });
  }
  if (variant === 'orphan') {
    // Transfer "succeeds" but the worker receives an OffscreenCanvas that
    // is NOT the canvas's placeholder — a stand-in for a compositor that
    // never commits transferred frames (the reported WebView behavior).
    await page.evaluateOnNewDocument(() => {
      HTMLCanvasElement.prototype.transferControlToOffscreen = function () {
        return new OffscreenCanvas(this.width || 1280, this.height || 720);
      };
      window.__lkVariant = 'orphan-transfer';
    });
  }
  await page.emulate({
    viewport: { width: 1100, height: 1000, hasTouch: true, isMobile: false, deviceScaleFactor: 1 },
    userAgent: 'Mozilla/5.0 (Linux; Android 14) AppleWebKit/537.36 Chrome/155 Mobile Safari/537.36 LithoBlackProbe',
  });
  const consoleLines = [];
  page.on('console', (m) => consoleLines.push(m.type() + ': ' + m.text()));
  page.on('pageerror', (e) => consoleLines.push('PAGEERROR: ' + e.message));

  const url = BASE + page0;
  const t0 = Date.now();
  const marks = {};
  console.log('opening', url, 'variant', variant);
  await page.goto(url, { waitUntil: 'load', timeout: 120000 });

  const logText = () => page.evaluate(() => document.getElementById('log').textContent).catch(() => '');
  async function waitMark(needle, timeoutS) {
    const start = Date.now();
    while (Date.now() - start < timeoutS * 1000) {
      const t = await logText();
      if (t.includes(needle)) { marks[needle] = ((Date.now() - t0) / 1000).toFixed(1) + 's'; return true; }
      await sleep(1000);
    }
    return false;
  }
  const FAST = process.argv[6] === 'fast';
  const live = await waitMark('backend live', FAST ? 60 : 180);
  console.log('backend live:', live, marks['backend live'] || '');
  const wp = await waitMark('web present:', FAST ? 20 : 60);
  console.log('web present line:', wp);
  const menu = await waitMark('menu: launch menu', FAST ? 60 : 240);
  console.log('menu:', menu, marks['menu: launch menu'] || '');
  const perf = await waitMark('perf ', FAST ? 30 : 90);
  console.log('perf lines:', perf);
  await sleep(6000);

  // Capability probe (main thread): can THIS browser render WebGL2 into
  // an OffscreenCanvas at all? (Worker-transferred compositing is a
  // separate question; this bisects context/render from transfer.)
  const cap = await page.evaluate(() => {
    const out = { ua: navigator.userAgent, offscreenType: typeof OffscreenCanvas,
                  transferType: typeof HTMLCanvasElement.prototype.transferControlToOffscreen };
    try {
      const oc = new OffscreenCanvas(64, 64);
      const gl = oc.getContext('webgl2');
      if (!gl) { out.ocWebgl2 = 'no-context'; }
      else {
        gl.clearColor(1, 0, 1, 1); gl.clear(gl.COLOR_BUFFER_BIT);
        const px = new Uint8Array(4); gl.readPixels(32, 32, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, px);
        out.ocWebgl2 = 'px=' + Array.from(px).join(',');
      }
    } catch (e) { out.ocWebgl2 = 'throw: ' + e.message; }
    try {
      const c = document.createElement('canvas');
      out.mainWebgl2 = c.getContext('webgl2') ? 'ok' : 'no-context';
    } catch (e) { out.mainWebgl2 = 'throw: ' + e.message; }
    return out;
  }).catch((e) => ({ evalError: e.message }));
  console.log('capability:', JSON.stringify(cap));

  // MEASURED presentation: screenshot clipped to the canvas element,
  // decoded in a helper page; report mean luminance + % bright pixels.
  let lum = { error: 'no canvas box' };
  const box = await page.evaluate(() => {
    const r = document.getElementById('canvas').getBoundingClientRect();
    return { x: r.x, y: r.y, width: r.width, height: r.height };
  }).catch(() => null);
  if (box && box.width > 0) {
    const shot = await page.screenshot({ clip: box });
    fs.writeFileSync(tag + '.png', shot);
    const b64 = shot.toString('base64');
    const helper = await browser.newPage();
    lum = await helper.evaluate(async (data) => {
      const img = new Image();
      await new Promise((res, rej) => { img.onload = res; img.onerror = rej; img.src = 'data:image/png;base64,' + data; });
      const c = document.createElement('canvas'); c.width = img.width; c.height = img.height;
      const g = c.getContext('2d'); g.drawImage(img, 0, 0);
      const d = g.getImageData(0, 0, c.width, c.height).data;
      let sum = 0, bright = 0, n = 0;
      for (let i = 0; i < d.length; i += 16) { const l = 0.2126 * d[i] + 0.7152 * d[i + 1] + 0.0722 * d[i + 2]; sum += l; if (l > 24) bright++; n++; }
      return { meanLum: +(sum / n).toFixed(2), pctBright: +(100 * bright / n).toFixed(2), w: c.width, h: c.height };
    }, b64).catch((e) => ({ error: e.message }));
    await helper.close();
  }
  console.log('LUMINANCE:', JSON.stringify(lum));

  const t = await logText();
  fs.writeFileSync(tag + '-log.txt', t);
  fs.writeFileSync(tag + '-console.txt', consoleLines.join('\n'));
  const verdict = { page: page0, variant, backendLive: live, webPresentLine: wp, menu, perfLines: perf, marks, cap, lum };
  fs.writeFileSync(tag + '-result.json', JSON.stringify(verdict, null, 2));
  console.log('VERDICT ' + JSON.stringify(verdict));
  await browser.close();
}
main().catch((e) => { console.error('PROBE FAIL', e); process.exit(1); });
