// lkpage.mjs: the page side of an engine main that runs in a worker. Transfers the
// canvas as an OffscreenCanvas, forwards DOM input to the worker, and carries out
// title, size and pointer-lock requests coming back. No engine code runs here.
export function lkPageAttach(canvas, worker, opts = {}) {
  const off = canvas.transferControlToOffscreen();
  const st = { captureWanted: 0, lockExitMs: -1e9, installed: false };
  const css = () => { const r = canvas.getBoundingClientRect(); return [r.width, r.height]; };
  const send = ev => worker.postMessage({ lkInput: true, css: css(), ev });
  const lock = () => { if (window.__lkTouchPrimary || !canvas.requestPointerLock) return; try { const p = canvas.requestPointerLock(); if (p && p.catch) p.catch(() => {}); } catch {} };
  function install() {
    if (st.installed) return; st.installed = true;
    const NAV = new Set(['Space', 'Tab', 'ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', 'PageUp', 'PageDown']);
    document.addEventListener('keydown', e => { send([1, (e.repeat ? 1 : 0) | (e.ctrlKey ? 2 : 0) | (e.metaKey ? 4 : 0), 0, 0, 0, 0, 0, e.code, e.key || '']); if (document.pointerLockElement || NAV.has(e.code)) e.preventDefault(); });
    document.addEventListener('keyup', e => send([2, 0, 0, 0, 0, 0, 0, e.code, e.key || '']));
    canvas.addEventListener('mousedown', e => {
      if (st.captureWanted && !document.pointerLockElement && performance.now() - st.lockExitMs > 1250) lock();
      send([3, 0, e.button, e.offsetX, e.offsetY, 0, 0, null, null]); e.preventDefault();
    });
    canvas.addEventListener('mouseup', e => send([4, 0, e.button, e.offsetX, e.offsetY, 0, 0, null, null]));
    canvas.addEventListener('mousemove', e => send([5, 0, 0, e.offsetX, e.offsetY, e.movementX, e.movementY, null, null]));
    canvas.addEventListener('wheel', e => { send([6, 0, e.deltaMode, e.deltaY, 0, 0, 0, null, null]); e.preventDefault(); }, { passive: false });
    document.addEventListener('pointerlockchange', () => { if (!document.pointerLockElement) st.lockExitMs = performance.now(); send([7, document.pointerLockElement ? 1 : 0, 0, 0, 0, 0, 0, null, null]); });
    document.addEventListener('pointerlockerror', () => send([8, 0, 0, 0, 0, 0, 0, null, null]));
    window.addEventListener('focus', () => send([9, 0, 0, 0, 0, 0, 0, null, null]));
    window.addEventListener('blur', () => send([10, 0, 0, 0, 0, 0, 0, null, null]));
    window.addEventListener('resize', () => send([0, 0, 0, 0, 0, 0, 0, null, null]));
  }
  function onMessage(d) {
    switch (d.lkPage) {
      case 'title': document.title = d.s; return true;
      case 'lock': lock(); return true;
      case 'unlock': if (document.exitPointerLock) document.exitPointerLock(); return true;
      case 'capture': st.captureWanted = d.on; return true;
      case 'input': install(); send([0, 0, 0, 0, 0, 0, 0, null, null]); return true;
      case 'size': return true;
    }
    return false;
  }
  return { offscreen: off, onMessage, css };
}
