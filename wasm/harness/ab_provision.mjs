// Build86l provisioning bench test: override the PAGE's
// hardwareConcurrency to 8 (phone-class) before lkboot evaluates,
// then boot gl.html, enter play, and dump every pool/dispatch log
// line. Pass criteria: host-dispatch line names provision 16; a
// pre-warm line appears; NO "exceeded"/"pool cap" ERR line; grants
// climb with high-water labelled (provision 16); play starts.
import puppeteer from 'puppeteer-core';
const browser = await puppeteer.launch({ executablePath: process.env.CHROME, headless: 'new',
  args: ['--no-sandbox','--enable-unsafe-swiftshader','--use-gl=angle','--use-angle=swiftshader','--enable-features=SharedArrayBuffer'] });
const page = await browser.newPage();
await page.setViewport({ width: 900, height: 760 });
await page.evaluateOnNewDocument(() => {
  try { Object.defineProperty(navigator, 'hardwareConcurrency', { get: () => 8 }); } catch (e) {}
});
const consoleLines = [];
page.on('console', (m) => consoleLines.push(m.text()));
await page.goto('http://127.0.0.1:8080/gl.html', { waitUntil: 'load', timeout: 30000 });
await new Promise(r => setTimeout(r, 30000));
await page.keyboard.press('Enter');
await new Promise(r => setTimeout(r, 25000));
const txt = await page.evaluate(() => document.body.innerText);
const interesting = txt.split('\n').filter(l =>
  /thread pool|host dispatch|pool|mega-draw|menu:|dimension|jobs:/.test(l));
console.log('--- page log lines ---');
for (const l of interesting) console.log(l);
const joined = txt + '\n' + consoleLines.join('\n');
console.log('--- checks ---');
console.log('provision named in dispatch:', /host dispatch:.*provision 16/.test(joined));
console.log('pre-warm line:', /pre-warmed \d+ idle workers/.test(joined));
console.log('ERR banner present (want false):', /pool (cap|provision) \(.*(reached|exceeded)/.test(joined));
console.log('high-water lines:', (txt.match(/high-water \d+ concurrent workers \(provision 16\)/g) || []).length);
await page.screenshot({ path: '/tmp/provision_end.png' });
await browser.close();
