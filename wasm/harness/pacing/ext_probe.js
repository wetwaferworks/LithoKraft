// One-shot probe: WebGL2 extension availability + WebGPU presence in the
// bench's headless Chromium (SwiftShader). Answers JD's question whether
// the GL arena + base-vertex model could be restored via extension
// multi-draw on WebGL2. Probe only — no implementation.
const puppeteer = require('puppeteer-core');
(async () => {
  const browser = await puppeteer.launch({
    executablePath: '/home/hatch/.cache/puppeteer/chrome/linux-155.0.8059.79/chrome-linux64/chrome',
    headless: 'new',
    args: ['--no-sandbox', '--disable-dev-shm-usage', '--use-gl=angle', '--use-angle=swiftshader',
           '--enable-unsafe-swiftshader', '--window-size=800,600'],
  });
  const page = await browser.newPage();
  await page.goto('http://127.0.0.1:8902/gl.html?benchprobe=0', { waitUntil: 'domcontentloaded', timeout: 60000 }).catch(() => {});
  const r = await page.evaluate(() => {
    const out = {};
    const cv = document.createElement('canvas');
    const gl = cv.getContext('webgl2');
    out.webgl2 = !!gl;
    if (gl) {
      out.renderer = gl.getParameter(gl.RENDERER);
      const exts = gl.getSupportedExtensions() || [];
      out.extCount = exts.length;
      out.WEBGL_multi_draw = exts.includes('WEBGL_multi_draw');
      out.WEBGL_multi_draw_instanced_base_vertex_base_instance = exts.includes('WEBGL_multi_draw_instanced_base_vertex_base_instance');
      out.EXT_draw_elements_base_vertex = exts.includes('EXT_draw_elements_base_vertex');
      out.OES_draw_elements_base_vertex = exts.includes('OES_draw_elements_base_vertex');
      // functional checks: fetch the extension objects + test the entry points
      const md = gl.getExtension('WEBGL_multi_draw');
      out.multiDrawArraysFn = !!(md && md.multiDrawArraysWEBGL);
      const mdb = gl.getExtension('WEBGL_multi_draw_instanced_base_vertex_base_instance');
      out.multiDrawInstancedBaseVertexFn = !!(mdb && mdb.multiDrawElementsInstancedBaseVertexBaseInstanceWEBGL);
      const bv = gl.getExtension('EXT_draw_elements_base_vertex');
      out.baseVertexFn = !!(bv && bv.drawElementsBaseVertexEXT);
      out.allExts = exts;
    }
    out.webgpu = !!navigator.gpu;
    return out;
  });
  console.log(JSON.stringify(r, null, 1));
  await browser.close();
})().catch((e) => { console.error('PROBE FAIL', e); process.exit(1); });
