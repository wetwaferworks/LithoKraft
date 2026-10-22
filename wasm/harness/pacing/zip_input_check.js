// From-the-zip input check: boot a page (default the zip's gl.html with
// ?debuginput=1), Enter -> play, click -> pointer lock, then hold W and
// verify (a) the DOM->game echo of the key and (b) the player position
// actually changes in the game's own state lines.
// Usage: node zip_input_check.js <url> <tag>
const fs = require('fs');
const path = require('path');
const puppeteer = require('puppeteer-core');
const url = process.argv[2];
const tag = process.argv[3] || 'zipinput';
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const RESULTS = path.join(__dirname, 'results');
const STATE = /#1\s+(-?\d+)\s+(-?\d+)\s+(-?\d+)\s+state/g;

async function main() {
  const out = { tag, url };
  const browser = await puppeteer.launch({
    executablePath: '/home/hatch/.cache/puppeteer/chrome/linux-155.0.8059.79/chrome-linux64/chrome',
    headless: 'new',
    args: ['--no-sandbox', '--disable-dev-shm-usage', '--use-gl=angle', '--use-angle=swiftshader',
           '--enable-unsafe-swiftshader', '--window-size=1100,1000'],
  });
  const page = await browser.newPage();
  await page.setViewport({ width: 1100, height: 1000 });
  const t0 = Date.now();
  await page.goto(url, { waitUntil: 'load', timeout: 120000 });
  const logOf = async () => page.evaluate(() => (document.getElementById('log') || {}).textContent || '').catch(() => '');
  let ok = false;
  while (Date.now() - t0 < 300 * 1000) { if ((await logOf()).includes('menu: launch menu')) { ok = true; break; } await sleep(2000); }
  out.menuSeen = ok;
  await page.keyboard.press('Enter');
  ok = false;
  for (let i = 0; i < 45 && !ok; i++) { if ((await logOf()).includes('menu: play')) ok = true; else await sleep(2000); }
  out.playSeen = ok;
  const el = await page.$('#canvas');
  if (el) await el.click().catch(() => {});
  await sleep(1500);
  let t = await logOf();
  out.pointerLock = t.includes('input: pointer lock engaged') ? 'engaged' : 'NOT SEEN';
  const states1 = [...t.matchAll(STATE)].map((m) => [+m[1], +m[2], +m[3]]);
  out.stateBefore = states1.length ? states1[states1.length - 1] : null;
  await page.keyboard.down('KeyW');
  // Poll up to 90 s for a second state line while W is held.
  for (let i = 0; i < 45; i++) {
    await sleep(2000);
    const tt = await logOf();
    const ss = [...tt.matchAll(STATE)];
    if (ss.length >= 2) break;
  }
  await page.keyboard.up('KeyW');
  await sleep(1500);
  t = await logOf();
  const states2 = [...t.matchAll(STATE)].map((m) => [+m[1], +m[2], +m[3]]);
  out.stateAfter = states2.length ? states2[states2.length - 1] : null;
  out.wEcho = /input> KeyDown KeyW|input> KeyDown W\b/.test(t) || t.includes('KeyDown') && t.includes('KeyW');
  out.wEchoRaw = (t.match(/input>[^\n]*KeyW[^\n]*/) || [null])[0];
  if (out.stateBefore && out.stateAfter) {
    out.delta = out.stateAfter.map((v, i) => v - out.stateBefore[i]);
    out.moved = out.delta.some((v) => v !== 0);
  }
  out.perfTail = (t.match(/perf fps=[^\n]*/) || []).slice(-1)[0] || null;
  out.aaLine = (t.match(/window: WebGL antialias[^\n]*/) || [null])[0];
  await page.screenshot({ path: path.join(RESULTS, tag + '.png') }).catch(() => {});
  out.wallS = +((Date.now() - t0) / 1000).toFixed(1);
  fs.writeFileSync(path.join(RESULTS, tag + '.json'), JSON.stringify(out, null, 1));
  console.log(tag, JSON.stringify(out));
  await browser.close();
}
main().catch((e) => { console.error('CHECK FAIL', e); process.exit(1); });
