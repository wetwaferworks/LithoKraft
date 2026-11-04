// LithoKraft web shell — BASIC TOUCH CONTROLS (2026-10-20).
//
// Page-shell feature only: no engine/WASM code is involved. The overlay
// translates thumbs into the EXACT DOM event shapes the web
// backend's own listeners already consume (window_web.cpp):
//   document 'keydown'/'keyup'  -> e.code + e.key (+ repeat/ctrl/meta)
//   canvas   'mousedown'/'mouseup' -> e.button + e.offsetX/e.offsetY
//   canvas   'mousemove'         -> e.offsetX/e.offsetY (+ movementX/Y)
// Those listeners feed the memory-mapped input ring (or the legacy
// string queue), so synthesized events ride the same transport as a
// physical keyboard/mouse — no new input path is invented here.
//
// Look without pointer lock: the backend's unlocked fallback computes
// look deltas from successive canvas-relative positions (the GLFW
// method), so the look drag dispatches mousemoves whose clientX/Y
// advance along the finger's path and lets the browser derive
// offsetX/offsetY (pinned explicitly as well, belt and suspenders).
//
// Visibility: auto-on when the device reports touch
// ('ontouchstart' in window || navigator.maxTouchPoints > 0),
// ?touch=1 forces on, ?touch=0 forces off. Desktop is untouched.
(function () {
  'use strict';
  var canvas = document.getElementById('canvas');
  var wrap = document.getElementById('wrap');
  if (!canvas || !wrap) return;

  var params = new URLSearchParams(location.search);
  var force = params.get('touch');
  var hasTouch = ('ontouchstart' in window) || (navigator.maxTouchPoints > 0);
  var show = (force === '1') ? true : (force === '0') ? false : hasTouch;
  window.LK_TOUCH_UI = show;
  if (!show) return;
  // Build 80: touch is the primary input on this page. lkboot's pointer-
  // lock seam and the engine's canvas mousedown listener both consult
  // this flag and skip pointer-lock requests entirely — the browser
  // refuses them on touch by design, and drag-look is the intended path.
  window.__lkTouchPrimary = true;

  // ---------- event synthesis (the shapes the backend consumes) ------
  function keyEvent(type, code, key, repeat) {
    var ev;
    try {
      ev = new KeyboardEvent(type, {
        code: code, key: key, repeat: !!repeat,
        bubbles: true, cancelable: true
      });
    } catch (e) {
      ev = document.createEvent('Event');
      ev.initEvent(type, true, true);
      ev.code = code; ev.key = key; ev.repeat = !!repeat;
    }
    document.dispatchEvent(ev);
  }

  function pinOffsets(ev, x, y) {
    // offsetX/offsetY for a synthetic MouseEvent are browser-derived
    // from clientX/Y; pin them explicitly so the values the backend
    // reads are exact on every WebView.
    try {
      Object.defineProperty(ev, 'offsetX', { value: x });
      Object.defineProperty(ev, 'offsetY', { value: y });
    } catch (e) { /* derived values will have to do */ }
  }

  function mouseEvent(type, button, clientX, clientY, dx, dy) {
    var r = canvas.getBoundingClientRect();
    var ev = new MouseEvent(type, {
      button: button || 0,
      buttons: type === 'mousedown' ? (button === 2 ? 2 : 1) : 0,
      clientX: clientX, clientY: clientY,
      movementX: dx || 0, movementY: dy || 0,
      bubbles: true, cancelable: true
    });
    pinOffsets(ev, clientX - r.left, clientY - r.top);
    canvas.dispatchEvent(ev);
  }

  function mouseButton(button, down) {
    var r = canvas.getBoundingClientRect();
    mouseEvent(down ? 'mousedown' : 'mouseup', button,
               r.left + r.width / 2, r.top + r.height / 2, 0, 0);
  }

  // Held keys mimic a real keyboard: one initial keydown, auto-repeat
  // keydowns (repeat=true) while held, one keyup on release.
  var held = {}; // code -> { timer, key }
  function pressKey(code, key) {
    if (held[code] !== undefined) return;
    keyEvent('keydown', code, key, false);
    held[code] = {
      key: key,
      timer: setInterval(function () { keyEvent('keydown', code, key, true); }, 90)
    };
  }
  function releaseKey(code) {
    var h = held[code];
    if (h === undefined) return;
    clearInterval(h.timer);
    delete held[code];
    keyEvent('keyup', code, h.key, false);
  }
  function tapKey(code, key) {
    keyEvent('keydown', code, key, false);
    setTimeout(function () { keyEvent('keyup', code, key, false); }, 60);
  }

  // ---------- overlay DOM ---------------------------------------------
  var style = document.createElement('style');
  style.textContent = [
    '#lktouch { position:absolute; inset:0; z-index:40; pointer-events:none;',
    '  font-family: ui-monospace, monospace; -webkit-user-select:none; user-select:none;',
    '  -webkit-tap-highlight-color: transparent; }',
    '#lktouch .zone { position:absolute; pointer-events:auto; touch-action:none; }',
    '#lk-look { left:36%; right:0; top:0; bottom:0; }',
    '#lk-joy { left:16px; bottom:16px; width:148px; height:148px; border-radius:50%;',
    '  background:rgba(19,23,38,.38); border:2px solid rgba(185,168,240,.45); }',
    '#lk-nub { position:absolute; left:50%; top:50%; width:58px; height:58px;',
    '  margin:-29px 0 0 -29px; border-radius:50%;',
    '  background:rgba(185,168,240,.35); border:2px solid rgba(185,168,240,.75); }',
    '#lk-btns { right:14px; bottom:14px; display:flex; flex-direction:column;',
    '  align-items:flex-end; gap:10px; pointer-events:none; }',
    '.lk-row { display:flex; gap:10px; align-items:flex-end; }',
    '.lk-btn { pointer-events:auto; touch-action:none; display:flex;',
    '  align-items:center; justify-content:center; border-radius:50%;',
    '  background:rgba(19,23,38,.55); border:2px solid rgba(185,168,240,.55);',
    '  color:#E8E4F8; font-size:12px; letter-spacing:.06em; }',
    '.lk-btn.on { background:rgba(47,180,255,.45); border-color:#2FB4FF; }',
    '.lk-b72 { width:72px; height:72px; } .lk-b64 { width:64px; height:64px; }',
    '.lk-pill { border-radius:10px; width:84px; height:46px; font-size:13px; }',
    '#lk-sys { right:12px; top:12px; display:flex; gap:10px; pointer-events:none; }',
    '#lk-hot { left:50%; top:12px; transform:translateX(-50%); display:flex;',
    '  gap:6px; pointer-events:none; }',
    '.lk-slot { pointer-events:auto; touch-action:none; width:34px; height:34px;',
    '  display:flex; align-items:center; justify-content:center; border-radius:6px;',
    '  background:rgba(19,23,38,.5); border:1px solid rgba(185,168,240,.5);',
    '  color:#b9b3d6; font-size:12px; }',
    '.lk-slot.on { background:rgba(47,180,255,.45); color:#E8E4F8; }'
  ].join('\n');
  document.head.appendChild(style);

  var root = document.createElement('div');
  root.id = 'lktouch';
  root.innerHTML =
    '<div class="zone" id="lk-look"></div>' +
    '<div class="zone" id="lk-joy"><div id="lk-nub"></div></div>' +
    '<div class="zone" id="lk-sys">' +
    '  <div class="lk-btn lk-pill" id="lk-enter">ENTER</div>' +
    '  <div class="lk-btn lk-pill" id="lk-esc">ESC</div>' +
    '</div>' +
    '<div class="zone" id="lk-hot"></div>' +
    '<div class="zone" id="lk-btns">' +
    '  <div class="lk-row">' +
    '    <div class="lk-btn lk-b64" id="lk-place">PLACE</div>' +
    '    <div class="lk-btn lk-b72" id="lk-break">BREAK</div>' +
    '  </div>' +
    '  <div class="lk-row">' +
    '    <div class="lk-btn lk-b72" id="lk-jump">JUMP</div>' +
    '  </div>' +
    '</div>';
  wrap.appendChild(root);

  var hot = document.getElementById('lk-hot');
  for (var n = 1; n <= 8; n++) {
    var slot = document.createElement('div');
    slot.className = 'lk-slot';
    slot.textContent = String(n);
    slot.setAttribute('data-digit', 'Digit' + n);
    hot.appendChild(slot);
  }

  function focusCanvas() {
    try { canvas.focus({ preventScroll: true }); } catch (e) { canvas.focus(); }
  }

  // ---------- generic hold-button wiring (multi-touch safe) -----------
  // Each control captures its own pointerId, so the joystick, the look
  // drag and any button can all be active at once.
  function bindHold(el, down, up) {
    var active = null;
    el.addEventListener('pointerdown', function (e) {
      if (active !== null) return;
      active = e.pointerId;
      try { el.setPointerCapture(active); } catch (err) {}
      el.classList.add('on');
      e.preventDefault();
      focusCanvas();
      down();
    });
    function end(e) {
      if (e.pointerId !== active) return;
      active = null;
      el.classList.remove('on');
      e.preventDefault();
      up();
    }
    el.addEventListener('pointerup', end);
    el.addEventListener('pointercancel', end);
    el.addEventListener('lostpointercapture', function (e) {
      if (active !== null && e.pointerId === active) {
        active = null;
        el.classList.remove('on');
        up();
      }
    });
  }

  // ---------- buttons ---------------------------------------------------
  bindHold(document.getElementById('lk-enter'),
    function () { pressKey('Enter', 'Enter'); },
    function () { releaseKey('Enter'); });
  bindHold(document.getElementById('lk-esc'),
    function () { pressKey('Escape', 'Escape'); },
    function () { releaseKey('Escape'); });
  bindHold(document.getElementById('lk-jump'),
    function () { pressKey('Space', ' '); },
    function () { releaseKey('Space'); });
  bindHold(document.getElementById('lk-break'),
    function () { mouseButton(0, true); },
    function () { mouseButton(0, false); });
  bindHold(document.getElementById('lk-place'),
    function () { mouseButton(2, true); },
    function () { mouseButton(2, false); });

  var slots = hot.children;
  for (var i = 0; i < slots.length; i++) {
    (function (slot) {
      var code = slot.getAttribute('data-digit');
      var label = slot.textContent;
      bindHold(slot,
        function () { keyEvent('keydown', code, label, false); },
        function () { keyEvent('keyup', code, label, false); });
    })(slots[i]);
  }

  // ---------- look drag (right zone; position-delta look) ---------------
  var look = document.getElementById('lk-look');
  var lookId = null, lastX = 0, lastY = 0;
  function clampedToCanvas(x, y) {
    var r = canvas.getBoundingClientRect();
    return [
      Math.min(Math.max(x, r.left), r.right),
      Math.min(Math.max(y, r.top), r.bottom)
    ];
  }
  look.addEventListener('pointerdown', function (e) {
    if (lookId !== null) return;
    lookId = e.pointerId;
    try { look.setPointerCapture(lookId); } catch (err) {}
    var p = clampedToCanvas(e.clientX, e.clientY);
    lastX = p[0]; lastY = p[1];
    // Seed the cursor position so the first real move's delta is
    // measured from the touch point, not from a stale position.
    mouseEvent('mousemove', 0, lastX, lastY, 0, 0);
    e.preventDefault();
    focusCanvas();  // focus only — never a pointer-lock request
  });
  look.addEventListener('pointermove', function (e) {
    if (e.pointerId !== lookId) return;
    var p = clampedToCanvas(e.clientX, e.clientY);
    var dx = p[0] - lastX, dy = p[1] - lastY;
    if (dx === 0 && dy === 0) return;
    lastX = p[0]; lastY = p[1];
    mouseEvent('mousemove', 0, lastX, lastY, dx, dy);
    e.preventDefault();
  });
  function lookEnd(e) {
    if (e.pointerId === lookId) lookId = null;
  }
  look.addEventListener('pointerup', lookEnd);
  look.addEventListener('pointercancel', lookEnd);

  // ---------- joystick (left zone; WASD) --------------------------------
  var joy = document.getElementById('lk-joy');
  var nub = document.getElementById('lk-nub');
  var joyId = null, ox = 0, oy = 0;
  var joyKeys = { KeyW: false, KeyA: false, KeyS: false, KeyD: false };
  var joyKeyName = { KeyW: 'w', KeyA: 'a', KeyS: 's', KeyD: 'd' };

  function joySet(code, on) {
    if (joyKeys[code] === on) return;
    joyKeys[code] = on;
    if (on) pressKey(code, joyKeyName[code]);
    else releaseKey(code);
  }
  function joyReleaseAll() {
    for (var code in joyKeys) joySet(code, false);
    nub.style.transform = '';
  }
  joy.addEventListener('pointerdown', function (e) {
    if (joyId !== null) return;
    joyId = e.pointerId;
    try { joy.setPointerCapture(joyId); } catch (err) {}
    ox = e.clientX; oy = e.clientY;
    e.preventDefault();
    focusCanvas();
  });
  joy.addEventListener('pointermove', function (e) {
    if (e.pointerId !== joyId) return;
    var R = 46; // full-deflection radius, CSS px
    var dx = e.clientX - ox, dy = e.clientY - oy;
    var len = Math.sqrt(dx * dx + dy * dy);
    if (len > R) { dx = dx * R / len; dy = dy * R / len; }
    nub.style.transform = 'translate(' + dx + 'px,' + dy + 'px)';
    var T = 0.35; // directional threshold (fraction of full deflection)
    joySet('KeyW', dy < -T * R);
    joySet('KeyS', dy > T * R);
    joySet('KeyA', dx < -T * R);
    joySet('KeyD', dx > T * R);
    e.preventDefault();
  });
  function joyEnd(e) {
    if (e.pointerId !== joyId) return;
    joyId = null;
    joyReleaseAll();  // no stuck keys, ever
  }
  joy.addEventListener('pointerup', joyEnd);
  joy.addEventListener('pointercancel', joyEnd);
  joy.addEventListener('lostpointercapture', function (e) {
    if (joyId !== null && e.pointerId === joyId) { joyId = null; joyReleaseAll(); }
  });

  // Safety net: if the page loses focus/visibility mid-gesture, drop
  // every held key and mouse button so nothing can stick.
  function releaseEverything() {
    var codes = Object.keys(held);
    for (var i = 0; i < codes.length; i++) releaseKey(codes[i]);
    joyReleaseAll();
    mouseButton(0, false);
    mouseButton(2, false);
  }
  window.addEventListener('blur', releaseEverything);
  document.addEventListener('visibilitychange', function () {
    if (document.hidden) releaseEverything();
  });
})();
