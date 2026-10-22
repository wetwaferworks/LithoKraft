// probegates.js — full-test gate runner (bug-fix run, 2026-10-20).
// Boots pages from a served origin and reports, per arm:
//   - the lkboot environment line (crossOriginIsolated / SharedArrayBuffer)
//   - the launch-menu milestone (interactive pages)
//   - the in-browser selftest census (?selftest): SELFTEST PASS/FAIL lines
//   - the demo scenario hash (?demotest=on|off)
// Usage: node probegates.js <origin> <mode>
//   mode: menu <page> | selftest <page> | demo <on|off> <page>
const path = require('path');
const puppeteer = require('puppeteer-core');
const CHROME = '/home/hatch/.cache/puppeteer/chrome/linux-155.0.8059.79/chrome-linux64/chrome';

async function boot(origin, pageName, query, waitMs) {
  const browser = await puppeteer.launch({
    executablePath: CHROME, headless: 'new',
    args: ['--no-sandbox', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--mute-audio'],
  });
  const page = await browser.newPage();
  const errors = [];
  page.on('pageerror', (e) => errors.push(String(e).slice(0, 200)));
  await page.goto(origin + '/' + pageName + (query ? '?' + query : ''), { waitUntil: 'load', timeout: 60000 });
  const t0 = Date.now();
  let log = '';
  while (Date.now() - t0 < waitMs) {
    await new Promise((r) => setTimeout(r, 2000));
    log = await page.evaluate(() => document.getElementById('log').textContent).catch(() => '');
    if (/ALL SELFTESTS|SELFTEST SUMMARY|scenario hash|BOOT FAILED|menu: launch menu/.test(log)) break;
  }
  const iso = await page.evaluate(() => ({
    coi: window.crossOriginIsolated, sab: typeof SharedArrayBuffer,
    counter: (document.getElementById('st-counter') || {}).textContent,
    release: (document.getElementById('st-release') || {}).textContent,
  })).catch(() => ({}));
  await browser.close();
  return { log, iso, errors };
}

async function main() {
  const origin = process.argv[2];
  const mode = process.argv[3];
  if (!origin || !mode) { console.error('usage: probegates.js <origin> <mode> ...'); process.exit(2); }
  if (mode === 'menu') {
    const pageName = process.argv[4] || 'gl.html';
    const { log, iso } = await boot(origin, pageName, '', 240000);
    console.log('env line:', (log.match(/lkboot: env:.*/) || ['(none)'])[0]);
    console.log('isolation:', JSON.stringify(iso));
    console.log('menu reached:', /menu: launch menu/.test(log));
    console.log('BOOT FAILED present:', /BOOT FAILED/.test(log));
    console.log(/menu: launch menu/.test(log) && iso.coi === true ? 'GATE MENU: PASS' : 'GATE MENU: FAIL');
  } else if (mode === 'selftest') {
    const pageName = process.argv[4] || 'index.html';
    const { log, iso } = await boot(origin, pageName, 'selftest', 600000);
    require('fs').writeFileSync('/home/hatch/workspace/torch-receive-2026-10-17/scratch/selftest-log.txt', log);
    const passLines = (log.match(/PASS/g) || []).length;
    console.log('raw PASS-token lines:', (log.match(/^[^\n]*PASS[^\n]*$/gm) || []).length, 'FAIL-token lines:', (log.match(/^[^\n]*\bFAIL\b[^\n]*$/gm) || []).length);
    const fails = [...log.matchAll(/SELFTEST FAIL:? ?([^\n]*)/g)].map((m) => m[1].trim());
    const segOk = [...log.matchAll(/([A-Z][A-Z ]*SELFTEST): OK/g)].map((m) => m[1]);
    const passCount = (log.match(/SELFTEST OK/g) || []).length;
    console.log('isolation:', JSON.stringify(iso));
    console.log('segments OK:', segOk.join(' | '));
    console.log('SELFTEST OK lines:', passCount);
    console.log('FAIL lines:', fails.length);
    for (const f of fails) console.log('  FAIL:', f);
    const allOk = /ALL SELFTESTS: OK/.test(log);
    console.log('ALL SELFTESTS OK:', allOk);
    console.log('log tail:', log.slice(-800).replace(/\n/g, ' ').slice(0, 800));
  } else if (mode === 'demo') {
    const arm = process.argv[4] || 'on';
    const pageName = process.argv[5] || 'index.html';
    const { log, iso } = await boot(origin, pageName, 'demotest=' + arm, 600000);
    const hashes = [...log.matchAll(/hash[:= ]+([0-9a-f]{16})/g)].map((m) => m[1]);
    console.log('isolation:', JSON.stringify(iso));
    console.log('hashes seen:', [...new Set(hashes)].join(', '));
    console.log('log tail:', log.slice(-600));
  }
}
main().catch((e) => { console.error('PROBEGATES: ERROR', e && e.message); process.exit(1); });
