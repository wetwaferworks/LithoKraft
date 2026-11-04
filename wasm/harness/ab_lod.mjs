// LOD A/B bench: boot gl.html, enter play, sample the engine perf
// lines + lkboot perf lines every 20 s. Run before and after a
// change; compare the tail samples (same rig, same seed, same
// radius 16 page default). SwiftShader numbers do not transfer to
// the phone; the DELTA on this rig is the signal.
import puppeteer from 'puppeteer-core';
const browser = await puppeteer.launch({ executablePath: process.env.CHROME, headless: 'new',
  args: ['--no-sandbox','--enable-unsafe-swiftshader','--use-gl=angle','--use-angle=swiftshader','--enable-features=SharedArrayBuffer'] });
const page = await browser.newPage();
await page.setViewport({ width: 900, height: 760 });
await page.goto('http://127.0.0.1:8080/gl.html', { waitUntil: 'load', timeout: 30000 });
await new Promise(r => setTimeout(r, 25000));
await page.keyboard.press('Enter');
for (let s = 0; s < 5; s++) {
  await new Promise(r => setTimeout(r, 20000));
  const out = await page.evaluate(() => {
    const txt = document.body.innerText;
    const perf = [...txt.matchAll(/perf fps=\d+ frame=[^\n]*/g)].map(m => m[0]);
    const lk = [...txt.matchAll(/lkboot: perf 1s:[^\n]*/g)].map(m => m[0]);
    const lod = [...txt.matchAll(/software far LOD:[^\n]*/g)].map(m => m[0]);
    return { perf: perf.slice(-2), lk: lk.slice(-1), lod: lod.slice(-1) };
  });
  console.log(`t+${(s + 1) * 20}s`, JSON.stringify(out, null, 1));
}
await browser.close();
