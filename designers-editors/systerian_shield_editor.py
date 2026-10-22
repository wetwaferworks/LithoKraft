#!/usr/bin/env python3
"""
Systerian Shield Editor (desktop, pure-stdlib Python / Tkinter)
================================================================
Live editor for the Protego Maxima shield emblem.

CREDITS (credit due, per the Director's standing rule)
  Design & direction : JD (David Rittenhouse), Wet Wafer Works -
                       the shield, the vertex/dial interaction, the
                       floating ring, the docked layout, and every
                       field finding were his spec or his catch.
  Built by           : Elfie (Muse) - generator, editors, codec port.
  Review & fix crews : the Elfie-dispatched crews of 2026-10-15
                       (code review + field-fix runs #2, #3, #4).
  The glyphs         : the Cistercian monks, c. 1300 - borrowed with
                       respect, credited, not owned.
  Motto              : "Potential in the design - Charge in the
                       traces - Static in the bank."

VERSION HISTORY (this file overwrites itself on the workstation -
the history travels inside it; full reports and per-pass baselines
are archived in ~/workspace/shield-protego/)
  v1  2026-10-15  First pass: codec-exact glyph ring, vertex/dial/
                  scroll interaction, traces, SVG export.
  v1.1            Live specs pane + Copy specs; SVG carries its
                  spec comment.
  v2  Code review pass: one mutation funnel (every change redraws,
      data-driven dials/vertices, glyph + trace-point clicking,
      floating OUTSIDE ring with halo parity.
  v3  Field-fix #2 (JD's live findings): auto-fit view (no
      clipping), true constant-distance hug, glyphs clickable in
      trace mode, Glyph phase/spin/spacing dials; Echo-Point
      Injection (double-click phantom points) de-echoed.
  v4  Field-fix #3: responsive layout - sidebar docked right,
      working area docked left/top, regenerates to full window on
      maximize; debounced resize through the funnel.
  v5  Field-fix #4: the Control-Point Mirage - taper vertex
      handles (bezier control points) seated onto the true curve;
      every handle now measures 0.000000 from the outline.
  v5.1            Credits + history block added (this block);
                  no behavior change - export byte-identical.

Interaction (JD's spec):
  * The shield outline carries VERTEX POINTS (crests, valleys, corners,
    side joints, taper joints, the bottom point).
  * CLICK a vertex -> the panel shows which DIALS govern that vertex and
    highlights them; the vertex's primary dial becomes the ACTIVE dial.
  * SCROLL WHEEL -> turns the active dial; the outline re-flows live and
    the status line narrates what changed and what it does.
  * Sliders can also be dragged directly; every field redraws instantly.
  * TRACE MODE -> click anywhere to lay circuit traces; double-click or
    the Finish button completes a trace.
  * CLICK a glyph in the ring -> the panel names the character and its
    code point and lights up the glyph dials. CLICK a trace point ->
    selects that trace (Delete point / Delete trace act on it).
    Glyphs are clickable in TRACE MODE too: a click that lands on one
    selects it and is consumed - it never lays a point. Outline
    vertices in trace mode take the point exactly where clicked -
    joint-to-joint connection is a deliberate feature, preserved.
  * RING SIDE -> the glyph ring can ride INSIDE the line (default) or
    OUTSIDE it, floating beyond the outline on the same contour; the
    Glyph distance dial sets how far from the edge on either side (the
    anchors ride at that true offset - no padding floor), and
    halo/rivet styling applies identically on both sides.
  * GLYPH PLACEMENT dials - Glyph phase slides the ring along its
    guide path, Glyph spin rotates each glyph about its anchor, Glyph
    spacing scales the arc-length step between glyphs. Selecting a
    glyph hands the scroll wheel to Glyph phase.
  * The preview AUTO-FITS: scale and centring are recomputed on every
    redraw from the full content bounds, so nothing ever clips.
  * RESPONSIVE LAYOUT: the control panel is docked RIGHT at its
    natural width; the canvas working area is docked LEFT and TOP and
    fills every remaining pixel. Resizing or maximising the window
    re-fits the view to the canvas's actual size - a canvas
    <Configure> event is a view mutation, debounced and routed
    through the same funnel as every other change.
  * TRACE MODE click fidelity: one physical click appends exactly one
    point, at the click's own coordinates - never snapped, never
    captured. A second press inside the de-echo window (4 screen px
    and 500 ms of the previous appended point) is the same physical
    click's echo (a double-click's second delivery, switch chatter)
    and appends nothing; the double-click then finishes the trace
    with no phantom point. Point-to-point spanning is untouched.
  * VERTEX HANDLES lie ON the outline (field-fix #4): the taper
    joints are bezier CONTROL points, which the curve approaches but
    never touches, so vertex_handles() seats every handle at the
    nearest point of the drawn outline. Handles are editor chrome:
    they move nothing in the geometry and are never exported.

Every state change in the GUI passes through ONE mutation funnel
(update()): sync the widgets, narrate, redraw, refresh the specs pane.
There is no code path that mutates a variable without a live redraw.

Glyph geometry is the Systerian (litho glyph) codec, identical to the
Systerian Translator: char = glyph of its code point; stave + digit
strokes in a 100x140 cell.

CLI:  --dump-svg PATH   write the current design as SVG and exit
      --selftest        verify glyph geometry against the translator
"""
import bisect
import math
import sys
import time

# ---------------------------------------------------------------------------
# Systerian codec geometry (identical construction to systerian_translator.py)
# ---------------------------------------------------------------------------
STAVE = ((50.0, 10.0), (50.0, 130.0))
U = {  # unit strokes (upper right quadrant)
    1: [((50, 10), (80, 10))],
    2: [((50, 50), (80, 50))],
    3: [((50, 10), (80, 40))],
    4: [((50, 50), (80, 20))],
    5: [((50, 10), (80, 10)), ((80, 10), (50, 40))],
    6: [((78, 12), (78, 42))],
    7: [((50, 10), (80, 10)), ((80, 10), (80, 40))],
    8: [((50, 50), (80, 50)), ((80, 50), (80, 80))],
    9: [((50, 10), (80, 10)), ((80, 10), (80, 40)), ((80, 40), (50, 40))],
}


def _mx(seg):
    return ((100 - seg[0][0], seg[0][1]), (100 - seg[1][0], seg[1][1]))


def _fy(seg):
    return ((seg[0][0], 140 - seg[0][1]), (seg[1][0], 140 - seg[1][1]))


def _place_segments(place, digit):
    segs = [tuple(map(tuple, s)) for s in U[digit]]
    if place in ("tens", "thousands"):
        segs = [_mx(s) for s in segs]
    if place in ("hundreds", "thousands"):
        segs = [_fy(s) for s in segs]
    return segs


def glyph_segments_for_value(value):
    v = int(value) % 10000
    segs = [STAVE]
    thou, rem = divmod(v, 1000)
    hund, rem = divmod(rem, 100)
    ten, one = divmod(rem, 10)
    if one:
        segs += _place_segments("units", one)
    if ten:
        segs += _place_segments("tens", ten)
    if hund:
        segs += _place_segments("hundreds", hund)
    if thou:
        segs += _place_segments("thousands", thou)
    return segs


def _sub(p, q):
    return (p[0] - q[0], p[1] - q[1])


def _cross(a, b):
    return a[0] * b[1] - a[1] * b[0]


def _seg_intersection(s1, s2):
    eps = 1e-9
    p, r = s1[0], _sub(s1[1], s1[0])
    q, s = s2[0], _sub(s2[1], s2[0])
    denom = _cross(r, s)
    if abs(denom) < eps:
        return None
    qp = _sub(q, p)
    t = _cross(qp, s) / denom
    u = _cross(qp, r) / denom
    if -eps <= t <= 1 + eps and -eps <= u <= 1 + eps:
        return (p[0] + t * r[0], p[1] + t * r[1])
    return None


def glyph_vertices(segs):
    pts = []
    for s in segs:
        pts += [s[0], s[1]]
    for i in range(len(segs)):
        for j in range(i + 1, len(segs)):
            p = _seg_intersection(segs[i], segs[j])
            if p:
                pts.append(p)
    uniq = []
    for p in pts:
        if not any(math.hypot(p[0] - q[0], p[1] - q[1]) < 0.5 for q in uniq):
            uniq.append(p)
    return uniq


def char_value(ch):
    return ord(ch) % 10000


# ---------------------------------------------------------------------------
# Shield geometry (design space 1500 x 1800) — mirrors make_shield.py v2
# plus JD's crest-width parameter (lower cosine exponent = broader crests)
# ---------------------------------------------------------------------------
CX = 750.0
TOP_Y = 350.0

DEFAULTS = {
    "hump_height": 190.0,
    "crest_width": 45.0,     # percent; exponent = 2 - 1.5 * pct/100
    "shield_width": 1020.0,
    "point_depth": 1500.0,   # y of the bottom point
    "side_bow": 0.0,         # +/- outward bow of the straight sides
    "glyph_size": 1.04,
    "glyph_inset": 108.0,
    "glyph_phase": 0.0,      # tangential shift of the ring along its path (u)
    "glyph_spin": 0.0,       # per-glyph rotation offset, degrees
    "glyph_spacing": 1.00,   # arc-length step multiplier between glyphs
    "halo_width": 10.0,
}

DIALS = [
    # key, label, min, max, step, effect narration
    ("hump_height", "Hump height", 0, 340, 2,
     "raises or lowers all three crests; valleys stay on the baseline"),
    ("crest_width", "Crest width %", 0, 100, 1,
     "broadens the crest plateaus - the ridge vertices spread and the top curve flows more uniform"),
    ("shield_width", "Shield width", 640, 1400, 4,
     "spreads the whole shield; crests, valleys and side joints ride outward together"),
    ("point_depth", "Point depth", 1150, 1700, 4,
     "lengthens or shortens the taper into the bottom point"),
    ("side_bow", "Side bow", -90, 130, 2,
     "bows the straight side runs outward (or cuts them in) between top corner and taper"),
    ("glyph_size", "Glyph size", 0.5, 2.0, 0.02,
     "scales the Systerian border ring only - the outline does not move"),
    ("glyph_inset", "Glyph distance", 8, 230, 2,
     "the glyph anchors ride at exactly this offset from the edge, on the chosen side (Inside/Outside) - dial it low and the ring hugs the line; at small distances the glyph bodies straddle the line - the outline does not move"),
    ("glyph_phase", "Glyph phase", -600, 600, 4,
     "slides the whole ring along its guide path - every glyph walks the contour by this many units (wraps around)"),
    ("glyph_spin", "Glyph spin", -180, 180, 3,
     "rotates every glyph around its own anchor, in degrees from its path heading"),
    ("glyph_spacing", "Glyph spacing", 0.5, 1.5, 0.02,
     "arc-length step between glyphs, times this - under 1 crowds the glyphs toward the start of the path, over 1 spreads them past a full lap (positions wrap)"),
    ("halo_width", "Halo width", 0, 30, 1,
     "the soft under-stroke around outline and glyphs - light, not shape"),
]

# Vertex -> governing dials (first entry = primary / scroll-wheel dial)
VERTEX_DIALS = {
    "crest": ["crest_width", "hump_height", "shield_width"],
    "valley": ["hump_height", "crest_width", "shield_width"],
    "corner": ["shield_width", "hump_height"],
    "side": ["side_bow", "shield_width", "point_depth"],
    "taper": ["point_depth", "side_bow", "shield_width"],
    "point": ["point_depth", "shield_width"],
}

# Dials governing a clicked glyph (first entry = primary / scroll-wheel dial)
GLYPH_DIALS = ["glyph_phase", "glyph_inset", "glyph_size",
               "glyph_spin", "glyph_spacing"]


def _bezier(p0, p1, p2, n):
    pts = []
    for i in range(n + 1):
        t = i / n
        x = (1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * p1[0] + t ** 2 * p2[0]
        y = (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * p1[1] + t ** 2 * p2[1]
        pts.append((x, y))
    return pts


def shield_params(p):
    hw = p["shield_width"] / 3.0
    x0 = CX - p["shield_width"] / 2.0
    expo = 2.0 - 1.5 * (p["crest_width"] / 100.0)
    joint_x = p["shield_width"] * 0.495 + p["side_bow"]
    joint_y = 990.0
    taper_x = p["shield_width"] * 0.314
    taper_y = TOP_Y + (p["point_depth"] - TOP_Y) * 0.885
    return hw, x0, expo, joint_x, joint_y, taper_x, taper_y


def shield_outline(p):
    hw, x0, expo, joint_x, joint_y, taper_x, taper_y = shield_params(p)
    bot = (CX, p["point_depth"])
    pts = []
    steps = 360
    for i in range(steps + 1):
        x = x0 + 3 * hw * (i / steps)
        frac = ((x - x0) / hw) % 1.0
        bump = abs(math.cos((frac - 0.5) * math.pi)) ** expo
        pts.append((x, TOP_Y - p["hump_height"] * bump))
    pts += _bezier((x0 + 3 * hw, TOP_Y), (CX + joint_x + 40, 660),
                   (CX + joint_x, joint_y), 60)[1:]
    pts += _bezier((CX + joint_x, joint_y), (CX + taper_x, taper_y), bot, 80)[1:]
    pts += _bezier(bot, (CX - taper_x, taper_y), (CX - joint_x, joint_y), 80)[1:]
    pts += _bezier((CX - joint_x, joint_y), (CX - joint_x - 40, 660),
                   (x0, TOP_Y), 60)[1:]
    return pts


def shield_vertices(p):
    """Named control vertices of the outline, with their kinds."""
    hw, x0, expo, joint_x, joint_y, taper_x, taper_y = shield_params(p)
    top = TOP_Y - p["hump_height"]
    vs = []
    for h in range(3):
        vs.append(("crest", f"Crest {h + 1}", (x0 + hw * (h + 0.5), top)))
    for i in (1, 2):
        vs.append(("valley", f"Valley {i}", (x0 + hw * i, TOP_Y)))
    vs.append(("corner", "Top corner L", (x0, TOP_Y)))
    vs.append(("corner", "Top corner R", (x0 + 3 * hw, TOP_Y)))
    vs.append(("side", "Side joint R", (CX + joint_x, joint_y)))
    vs.append(("side", "Side joint L", (CX - joint_x, joint_y)))
    vs.append(("taper", "Taper joint R", (CX + taper_x, taper_y)))
    vs.append(("taper", "Taper joint L", (CX - taper_x, taper_y)))
    vs.append(("point", "Bottom point", (CX, p["point_depth"])))
    return vs


def _nearest_on_polyline(pt, poly):
    """The point of the polyline nearest to pt (segment-wise
    projection; the closing segment is included, so a closed loop
    needs no special case)."""
    px, py = pt
    best = None
    best_d2 = None
    n = len(poly)
    for i in range(n):
        ax, ay = poly[i]
        bx, by = poly[(i + 1) % n]
        vx, vy = bx - ax, by - ay
        seg2 = vx * vx + vy * vy
        t = 0.0
        if seg2 > 0.0:
            t = ((px - ax) * vx + (py - ay) * vy) / seg2
            t = 0.0 if t < 0.0 else (1.0 if t > 1.0 else t)
        qx, qy = ax + t * vx, ay + t * vy
        d2 = (qx - px) ** 2 + (qy - py) ** 2
        if best_d2 is None or d2 < best_d2:
            best_d2 = d2
            best = (qx, qy)
    return best


def vertex_handles(p):
    """(kind, label, handle_pos) for drawing and hit-testing the
    vertex handles: every handle lies ON the drawn outline.

    shield_vertices() reports each vertex's DEFINING position, but
    the two taper joints are defined by bezier control points - and a
    quadratic bezier, B(t) = (1-t)^2 p0 + 2(1-t)t p1 + t^2 p2, never
    passes through its control point p1 (it is only pulled toward
    it), so a handle drawn at the control point floats off the curve
    (up to ~118 u at wide/deep settings - field bug #6, the
    "Control-Point Mirage"). Seating each handle at the nearest
    point of the outline polyline puts taper handles mid-taper, on
    the curve they govern; vertices already on the curve (crests,
    valleys, corners, side joints, the bottom point) are their own
    nearest point and do not move.

    This is the ONE source of handle positions: the GUI's draw loop
    and hit test both consume it, so what is drawn is what is
    clickable. Handles are editor chrome - they change no geometry
    and are never exported."""
    outline = shield_outline(p)
    return [(kind, label, _nearest_on_polyline(pos, outline))
            for kind, label, pos in shield_vertices(p)]


def offset_inward(pts, d, centroid):
    out = []
    n = len(pts)
    for i in range(n):
        p = pts[i]
        q = pts[(i + 1) % n]
        r = pts[(i - 1) % n]
        tx, ty = q[0] - r[0], q[1] - r[1]
        ln = math.hypot(tx, ty) or 1.0
        tx, ty = tx / ln, ty / ln
        nx, ny = ty, -tx
        if (p[0] - centroid[0]) * nx + (p[1] - centroid[1]) * ny > 0:
            nx, ny = -nx, -ny
        out.append((p[0] + nx * d, p[1] + ny * d))
    return out


def _smooth(pts, w=7):
    n = len(pts)
    out = []
    for i in range(n):
        sx = sy = 0.0
        for k in range(-w, w + 1):
            q = pts[(i + k) % n]
            sx += q[0]
            sy += q[1]
        out.append((sx / (2 * w + 1), sy / (2 * w + 1)))
    return out


def _arclength(pts):
    cum = [0.0]
    for i in range(1, len(pts) + 1):
        a = pts[i - 1]
        b = pts[i % len(pts)]
        cum.append(cum[-1] + math.hypot(b[0] - a[0], b[1] - a[1]))
    return cum


def _point_at(pts, cum, s):
    total = cum[-1]
    s = s % total
    i = bisect.bisect_right(cum, s) - 1
    a = pts[i % len(pts)]
    b = pts[(i + 1) % len(pts)]
    seg = cum[i + 1] - cum[i] or 1.0
    t = (s - cum[i]) / seg
    return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t,
            math.atan2(b[1] - a[1], b[0] - a[0]))


def ring_path(p, side="inside"):
    """The contour the glyph ring rides: a constant-distance offset of
    the outline, on the chosen side. The distance dial MEANS what it
    says - the anchors ride at that offset from the line. There is no
    collision floor: dial the distance low and the ring hugs the line
    (glyph bodies then straddle it - the designer's choice, set by the
    dial, not by hidden padding).

    Inside (default) keeps the original construction byte-for-byte
    (vertex-normal offset + w=7 smoothing) so the canonical shield is
    unchanged. Outside mirrors the same offset with only a light w=2
    smoothing, so the path stays a faithful parallel curve and the
    anchor-to-outline distance tracks the dial within a few units
    everywhere, corners included (measured in the field-fix report).
    """
    outline = shield_outline(p)
    centroid = (CX, 880.0)
    if side == "outside":
        return _smooth(offset_inward(outline, -p["glyph_inset"], centroid), w=2)
    return _smooth(offset_inward(outline, p["glyph_inset"], centroid))


def glyph_ring(p, text, side="inside"):
    """World-space (segments, vertices, anchor) per glyph of the text.

    The anchor is the glyph's centre (where its local (50, 70) lands) -
    the hit-test point for click-to-select. Glyph orientation is
    path-relative on both sides: the stave top points away from the
    shield centroid, so outside glyphs read exactly like inside ones.
    """
    centroid = (CX, 880.0)
    path = ring_path(p, side)
    cum = _arclength(path)
    total = cum[-1]
    gs = p["glyph_size"]
    # Placement dials (neutral at defaults, so the canonical ring is
    # reproduced exactly): phase shifts every anchor along the path,
    # spacing scales the arc-length step (positions wrap via _point_at),
    # spin rotates each glyph about its anchor.
    phase = p.get("glyph_phase", 0.0)
    spacing = p.get("glyph_spacing", 1.0)
    spin = math.radians(p.get("glyph_spin", 0.0))
    ca, sa = math.cos(spin), math.sin(spin)
    ring = []
    if not text:
        return ring
    for k, ch in enumerate(text):
        s = phase + spacing * (k + 0.5) * total / len(text)
        px, py, ang = _point_at(path, cum, s)
        tx, ty = math.cos(ang), math.sin(ang)
        nx, ny = ty, -tx
        if (px - centroid[0]) * nx + (py - centroid[1]) * ny < 0:
            nx, ny = -nx, -ny

        def xf(lx, ly):
            x = (lx - 50.0) * gs
            y = (ly - 70.0) * gs
            xr = x * ca - y * sa
            yr = x * sa + y * ca
            return (px + xr * tx - yr * nx, py + xr * ty - yr * ny)

        segs = glyph_segments_for_value(char_value(ch))
        wsegs = [(xf(*a), xf(*b)) for a, b in segs]
        wverts = [xf(*v) for v in glyph_vertices(segs)]
        ring.append((wsegs, wverts, (px, py)))
    return ring


def content_bounds(p, text, side="inside"):
    """Design-space bounds (minx, miny, maxx, maxy) of everything the
    preview must show: the outline plus the glyph ring's full cell
    extents, with the halo allowed for. The GUI auto-fits the view to
    these bounds on every redraw, so no setting can clip."""
    outline = shield_outline(p)
    xs = [q[0] for q in outline]
    ys = [q[1] for q in outline]
    minx, maxx, miny, maxy = min(xs), max(xs), min(ys), max(ys)
    ext = math.hypot(50.0, 70.0) * p["glyph_size"]   # cell bounding radius
    if p["halo_width"] > 0:
        ext += p["halo_width"]                        # glyph/line halo margin
    for wsegs, wverts, anchor in glyph_ring(p, text, side):
        for a, b in wsegs:
            for q in (a, b):
                minx = min(minx, q[0]); maxx = max(maxx, q[0])
                miny = min(miny, q[1]); maxy = max(maxy, q[1])
        for q in wverts:
            minx = min(minx, q[0]); maxx = max(maxx, q[0])
            miny = min(miny, q[1]); maxy = max(maxy, q[1])
        minx = min(minx, anchor[0] - ext); maxx = max(maxx, anchor[0] + ext)
        miny = min(miny, anchor[1] - ext); maxy = max(maxy, anchor[1] + ext)
    return minx, miny, maxx, maxy


def fit_view(bounds, cw, ch, pad=16.0):
    """(scale, ox, oy) fitting `bounds` into a cw x ch canvas with `pad`
    screen px to spare on every side, centred: screen = design*scale +
    (ox, oy). Pure function - the no-clip invariant is tested against it."""
    minx, miny, maxx, maxy = bounds
    bw = max(maxx - minx, 1.0)
    bh = max(maxy - miny, 1.0)
    scale = min((cw - 2 * pad) / bw, (ch - 2 * pad) / bh)
    ox = (cw - bw * scale) / 2.0 - minx * scale
    oy = (ch - bh * scale) / 2.0 - miny * scale
    return scale, ox, oy


def compute_specs(p, text, side="inside", view_scale=None):
    """Derived measurements of the current design, in design units."""
    outline = shield_outline(p)
    xs = [q[0] for q in outline]
    ys = [q[1] for q in outline]
    width = max(xs) - min(xs)
    top = min(ys)
    height = p["point_depth"] - top
    per = _arclength(outline)[-1]
    hw, x0, expo, joint_x, joint_y, taper_x, taper_y = shield_params(p)
    if p["hump_height"] > 0 and expo > 0:
        c = 0.95 ** (1.0 / expo)
        frac_half = math.acos(min(1.0, c)) / math.pi
        plateau = 2 * frac_half * hw
    else:
        plateau = 0.0
    ring_len = _arclength(ring_path(p, side))[-1]
    lines = [
        f"Outline width     : {width:8.1f} u",
        f"Outline height    : {height:8.1f} u  (crest {top:.0f} -> point {p['point_depth']:.0f})",
        f"Outline perimeter : {per:8.1f} u",
        f"Crest height      : {p['hump_height']:8.1f} u   plateau width {plateau:.1f} u",
        f"Crest exponent    : {expo:8.3f}   (crest width {p['crest_width']:.0f}%)",
        f"Side joints       : y {joint_y:.0f}, x +/-{joint_x:.0f} (bow {p['side_bow']:+.0f})",
        f"Taper             : {p['point_depth'] - joint_y:8.1f} u  (joint -> point)",
        f"Glyph ring        : {len(text)} glyphs, {side.upper()} at {p['glyph_inset']:.0f} u on {ring_len:.0f} u of path",
        f"Glyph stave       : {120 * p['glyph_size']:8.1f} u   (size x{p['glyph_size']:.2f}, distance {p['glyph_inset']:.0f})",
        f"Glyph placement   : phase {p.get('glyph_phase', 0.0):+.0f} u, spin {p.get('glyph_spin', 0.0):+.0f} deg, spacing x{p.get('glyph_spacing', 1.0):.2f}",
    ]
    if view_scale is not None:
        lines.append(f"Preview fit       : x{view_scale:.3f} auto-fit (nothing clips)")
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# SVG export
# ---------------------------------------------------------------------------
def build_svg(p, text, traces, show_halo=True, show_rivets=True,
              show_inner=True, ring_side="inside"):
    outline = shield_outline(p)
    inner = offset_inward(outline, 30.0, (CX, 880.0))
    ring = glyph_ring(p, text, ring_side)
    spec_comment = ("<!-- Systerian Shield spec: " +
                    "; ".join(f"{k}={v}" for k, v in p.items()) +
                    f"; inscription={text!r}" +
                    ("" if ring_side == "inside" else f"; ring_side={ring_side}") +
                    " -->\n")
    parts = [spec_comment +
             '<svg xmlns="http://www.w3.org/2000/svg" width="1500" '
             'height="1800" viewBox="0 0 1500 1800">',
             '<rect width="1500" height="1800" fill="#050510"/>',
             '<polygon points="' + " ".join(f"{x:.1f},{y:.1f}" for x, y in outline) +
             '" fill="#0D0D22"/>']
    if show_inner:
        parts.append('<polyline points="' +
                     " ".join(f"{x:.1f},{y:.1f}" for x, y in inner + [inner[0]]) +
                     '" fill="none" stroke="#A970FF" stroke-opacity="0.35" stroke-width="2"/>')
    if show_halo and p["halo_width"] > 0:
        parts.append('<polyline points="' +
                     " ".join(f"{x:.1f},{y:.1f}" for x, y in outline + [outline[0]]) +
                     f'" fill="none" stroke="#FF3B30" stroke-opacity="0.35" stroke-width="{p["halo_width"] * 2:.0f}"/>')
    parts.append('<polyline points="' +
                 " ".join(f"{x:.1f},{y:.1f}" for x, y in outline + [outline[0]]) +
                 '" fill="none" stroke="#A970FF" stroke-width="5"/>')
    for wsegs, wverts, _anchor in ring:
        for a, b in wsegs:
            parts.append(f'<line x1="{a[0]:.1f}" y1="{a[1]:.1f}" x2="{b[0]:.1f}" '
                         f'y2="{b[1]:.1f}" stroke="#A970FF" stroke-width="4" stroke-linecap="round"/>')
        if show_rivets:
            for vx, vy in wverts:
                parts.append(f'<circle cx="{vx:.1f}" cy="{vy:.1f}" r="5" fill="#FF6F00"/>')
    for tr in traces:
        if len(tr) < 2:
            continue
        parts.append('<polyline points="' +
                     " ".join(f"{x:.1f},{y:.1f}" for x, y in tr) +
                     '" fill="none" stroke="#6FA8FF" stroke-width="5" stroke-linejoin="round"/>')
        for vx, vy in tr:
            parts.append(f'<circle cx="{vx:.1f}" cy="{vy:.1f}" r="6" fill="#FF6F00"/>')
    parts.append('</svg>')
    return "\n".join(parts)


# ---------------------------------------------------------------------------
# GUI
# ---------------------------------------------------------------------------
VIOLET = "#A970FF"
ORANGE = "#FF6F00"
HALO_C = "#8E2A24"
FIELD = "#0D0D22"
BG = "#050510"
TRACE_C = "#6FA8FF"
INITIAL_CW, INITIAL_CH = 640, 780   # canvas size at first open only;
                                    # afterwards the canvas fills the
                                    # window's working area and the view
                                    # auto-fits into its ACTUAL size
MIN_CW, MIN_CH = 360, 440           # canvas floor in the root minsize:
                                    # the working area never collapses
                                    # below a usable size
VIEW_PAD_PX = 16.0      # breathing room the auto-fit leaves on every side
RESIZE_DEBOUNCE_MS = 60  # a drag-resize fires many canvas <Configure>
                        # events; they coalesce into ONE funnel redraw
                        # per pause, and the final size always lands
# De-echo window for trace-mode clicks (field bug #5): a second press
# within ECHO_PX screen px AND ECHO_S seconds of the previous appended
# draft point is the same physical click's echo (a double-click's
# second delivery, mouse-switch chatter) and appends nothing.
ECHO_PX = 4.0
ECHO_S = 0.5


def run_gui():
    import tkinter as tk
    from tkinter import filedialog

    DIAL_BY_KEY = {d[0]: d for d in DIALS}
    HIT_TOL_PX = 20.0        # click tolerance for vertices/traces, screen px

    p = dict(DEFAULTS)
    state = {
        "text": "PROTEGO MAXIMA",
        "halo": True, "rivets": True, "inner": True,
        "trace_mode": False,
        "ring_side": "inside",         # "inside" | "outside"
        # ("vertex", kind, label) | ("glyph", index, char) |
        # ("trace", trace_index, point_index_or_None) | None
        "selection": None,
        "active_dial": "crest_width",
        "traces": [], "draft": [],
        # live view transform, recomputed by every redraw (auto-fit):
        # screen = design * scale + (ox, oy)
        "view": {"scale": 0.42, "ox": 0.0, "oy": 0.0},
        # the canvas's ACTUAL pixel size - the only size the view may
        # be fitted into. Written solely by the canvas <Configure>
        # handler (on_canvas_configure); starts at the initial size
        # until the window first maps and reports the real one.
        "canvas_size": (INITIAL_CW, INITIAL_CH),
        "_resize_job": None,           # pending debounced resize redraw
        # (design point, monotonic time) of the last draft append -
        # the de-echo record for trace-mode clicks (field bug #5)
        "_last_append": None,
        "_syncing": False,             # reentrancy guard for widget sync
    }

    root = tk.Tk()
    root.title("Systerian Shield Editor - Protego Maxima")
    root.configure(bg="#101018")

    # Layout (JD, field-fix #3): the control panel is DOCKED RIGHT at
    # its natural fixed width (column 1, weight 0 - it can neither
    # stretch nor be overlapped); the canvas working area is DOCKED
    # LEFT and TOP (column 0 / row 0, weight 1, sticky nsew) and takes
    # every remaining pixel, expanding rightward and downward from the
    # window's top-left corner - never centred in dead space, never
    # detached from the left/top edges.
    root.columnconfigure(0, weight=1)
    root.columnconfigure(1, weight=0)
    root.rowconfigure(0, weight=1)

    canvas = tk.Canvas(root, width=INITIAL_CW, height=INITIAL_CH,
                       bg=BG, highlightthickness=0)
    canvas.grid(row=0, column=0, sticky="nsew", padx=8, pady=8)

    panel = tk.Frame(root, bg="#101018")
    panel.grid(row=0, column=1, sticky="n", padx=4, pady=8)

    tk.Label(panel, text="SYSTERIAN SHIELD EDITOR", fg=VIOLET, bg="#101018",
             font=("Consolas", 13, "bold")).pack(anchor="w")
    tk.Label(panel, text="Potential in the design - Charge in the traces - Static in the bank",
             fg="#8a8aa5", bg="#101018", font=("Consolas", 8, "italic")).pack(anchor="w", pady=(0, 6))

    tk.Label(panel, text="Inscription (Systerian = litho glyph):", fg="#ccccdd",
             bg="#101018", font=("Consolas", 9)).pack(anchor="w")
    text_var = tk.StringVar(value=state["text"])
    tk.Entry(panel, textvariable=text_var, width=26, bg="#1a1a2a", fg="#eeeeff",
             insertbackground="#eeeeff", font=("Consolas", 10)).pack(anchor="w", pady=(0, 6))

    status = tk.Label(panel, text="", fg="#ffd9a0", bg="#101018",
                      font=("Consolas", 9), wraplength=330, justify="left")
    effect = tk.Label(panel, text="", fg="#9fb7ff", bg="#101018",
                      font=("Consolas", 8), wraplength=330, justify="left")

    dial_vars = {}        # key -> DoubleVar (kept: the slider sync path)
    dial_labels = {}      # key -> Label (highlight target)

    # ------------------------------------------------------------------
    # The mutation funnel. EVERY state change ends in update(): widget
    # sync, narration, redraw (which refreshes highlights + specs).
    # Invariant: no mutator mutates and returns without calling update().
    # ------------------------------------------------------------------
    def governing_dials(sel):
        if not sel:
            return []
        if sel[0] == "vertex":
            return VERTEX_DIALS[sel[1]]
        if sel[0] == "glyph":
            return GLYPH_DIALS
        return []

    def narrate(key):
        d = DIAL_BY_KEY[key]
        val = p[key]
        shown = f"{val:.2f}" if d[4] < 1 else f"{val:.0f}"
        status.config(text=f"Dial: {d[1]} = {shown}")
        effect.config(text="Effect: " + d[5])

    def sync_dial_widgets():
        state["_syncing"] = True
        try:
            for key, var in dial_vars.items():
                if abs(var.get() - p[key]) > 1e-9:
                    var.set(p[key])
        finally:
            state["_syncing"] = False

    def update(narrate_key=None, message=None):
        sync_dial_widgets()
        if narrate_key is not None:
            narrate(narrate_key)
        if message is not None:
            status.config(text=message)
        redraw()

    # ------------------------- view transform -------------------------
    # One transform, recomputed from the full content bounds on every
    # redraw: screen = design * scale + (ox, oy). Drawing, hit-testing
    # and the smoke tests all go through these two helpers, so what is
    # visible is exactly what is clickable, and nothing ever clips.
    def canvas_px():
        """The canvas's actual pixel size - the view's ONLY size
        source. state["canvas_size"] is kept current by the canvas
        <Configure> handler; before the first real size lands, fall
        back to the widget's own report, then to the initial size."""
        cw, ch = state["canvas_size"]
        if cw > 1 and ch > 1:
            return cw, ch
        w, h = canvas.winfo_width(), canvas.winfo_height()
        if w > 1 and h > 1:
            return w, h
        return INITIAL_CW, INITIAL_CH

    def to_screen(dx, dy):
        v = state["view"]
        return dx * v["scale"] + v["ox"], dy * v["scale"] + v["oy"]

    def to_design(sx, sy):
        v = state["view"]
        return (sx - v["ox"]) / v["scale"], (sy - v["oy"]) / v["scale"]

    def redraw():
        bounds = content_bounds(p, state["text"], state["ring_side"])
        scale, ox, oy = fit_view(bounds, *canvas_px(), VIEW_PAD_PX)
        state["view"] = {"scale": scale, "ox": ox, "oy": oy}
        canvas.delete("all")
        outline = shield_outline(p)
        inner = offset_inward(outline, 30.0, (CX, 880.0))
        sel = state["selection"]

        def S(pts):
            out = []
            for x, y in pts:
                out += [x * scale + ox, y * scale + oy]
            return out

        def P(x, y):
            return x * scale + ox, y * scale + oy

        canvas.create_polygon(S(outline), fill=FIELD, outline="")
        if state["inner"]:
            canvas.create_line(S(inner + [inner[0]]), fill="#4a3a75", width=1)
        if state["halo"] and p["halo_width"] > 0:
            canvas.create_line(S(outline + [outline[0]]), fill=HALO_C,
                               width=max(2, int(p["halo_width"] * scale * 2)))
        canvas.create_line(S(outline + [outline[0]]), fill=VIOLET, width=3)

        for idx, (wsegs, wverts, anchor) in enumerate(
                glyph_ring(p, state["text"], state["ring_side"])):
            gsel = bool(sel and sel[0] == "glyph" and sel[1] == idx)
            if state["halo"] and p["halo_width"] > 0:
                for a, b in wsegs:
                    ax, ay = P(*a)
                    bx, by = P(*b)
                    canvas.create_line(ax, ay, bx, by, fill=HALO_C, width=6)
            for a, b in wsegs:
                ax, ay = P(*a)
                bx, by = P(*b)
                canvas.create_line(ax, ay, bx, by,
                                   fill=ORANGE if gsel else VIOLET,
                                   width=3 if gsel else 2)
            if state["rivets"]:
                for vx, vy in wverts:
                    sx, sy = P(vx, vy)
                    r = 3
                    canvas.create_oval(sx - r, sy - r, sx + r, sy + r,
                                       fill=ORANGE, outline="")
            if gsel:
                ax, ay = P(*anchor)
                canvas.create_oval(ax - 9, ay - 9, ax + 9, ay + 9,
                                   outline=ORANGE, width=2)

        for ti, tr in enumerate(state["traces"] + ([state["draft"]] if state["draft"] else [])):
            tsel = bool(sel and sel[0] == "trace" and sel[1] == ti)
            if len(tr) >= 2:
                canvas.create_line(S(tr), fill="#BFD9FF" if tsel else TRACE_C,
                                   width=4 if tsel else 3)
            for pi, (vx, vy) in enumerate(tr):
                psel = bool(tsel and sel[2] == pi)
                sx, sy = P(vx, vy)
                r = 6 if psel else 3.5
                canvas.create_oval(sx - r, sy - r, sx + r, sy + r,
                                   fill="#FFFFFF" if psel else ORANGE,
                                   outline=ORANGE if psel else "")

        # vertex handles (topmost, clickable) - positions from
        # vertex_handles(): every handle sits ON the outline
        for kind, label, (vx, vy) in vertex_handles(p):
            vsel = sel == ("vertex", kind, label)
            sx, sy = P(vx, vy)
            r = 8 if vsel else 5.5
            canvas.create_oval(sx - r, sy - r, sx + r, sy + r,
                               fill=ORANGE if vsel else "#221a38",
                               outline=ORANGE, width=2)
        specs_label.config(text=compute_specs(p, state["text"],
                                              state["ring_side"],
                                              view_scale=scale))
        refresh_dial_highlights()

    def refresh_dial_highlights():
        gov = governing_dials(state["selection"])
        for key, lab in dial_labels.items():
            if key == state["active_dial"]:
                lab.config(fg=ORANGE)
            elif key in gov:
                lab.config(fg="#ffd9a0")
            else:
                lab.config(fg="#ccccdd")

    # ------------------------- mutators -------------------------------
    def set_dial(key, value):
        if state["_syncing"]:
            return            # programmatic widget sync, not a user edit
        d = DIAL_BY_KEY[key]
        p[key] = min(d[3], max(d[2], float(value)))
        state["active_dial"] = key
        update(narrate_key=key)

    def activate_dial(key):
        state["active_dial"] = key
        update(narrate_key=key)

    def set_flag(key, value):
        state[key] = bool(value)
        update()

    def set_trace_mode(value):
        trace_var.set(bool(value))
        state["trace_mode"] = bool(value)
        update(message=("TRACE MODE: click the canvas to lay points, "
                        "point to point - vertices take the point exactly "
                        "where you click them, joint to joint; double-click "
                        "or Finish completes a trace. Clicking a GLYPH "
                        "selects it instead of laying a point."
                        if value else
                        "Trace mode off. Click a vertex, a glyph, or a "
                        "trace point to select it."))

    def set_ring_side(side):
        state["ring_side"] = side
        update(message=f"Glyph ring: {side.upper()} the line, "
                       f"{p['glyph_inset']:.0f} u from the edge.")

    def set_text(text):
        state["text"] = text
        sel = state["selection"]
        if sel and sel[0] == "glyph" and sel[1] >= len(text):
            state["selection"] = None     # the selected glyph no longer exists
        update()

    def select(selection, message):
        state["selection"] = selection
        gov = governing_dials(selection)
        narrate_key = None
        if gov:
            state["active_dial"] = gov[0]
            narrate_key = gov[0]
        update(narrate_key=narrate_key, message=message)

    def clear_selection():
        if state["selection"] is not None:
            state["selection"] = None
            update(message="Selection cleared.")

    def finish_trace():
        state["_last_append"] = None
        if len(state["draft"]) >= 2:
            state["traces"] = state["traces"] + [list(state["draft"])]
            state["draft"] = []
            state["selection"] = ("trace", len(state["traces"]) - 1, None)
            update(message=f"Trace {len(state['traces'])} finished "
                           f"({len(state['traces'][-1])} points), and selected.")
        elif state["draft"]:
            state["draft"] = []
            update(message="Draft discarded - a trace needs 2+ points.")
        else:
            update(message="No draft to finish - enable TRACE MODE and click the canvas.")

    def undo_point():
        state["_last_append"] = None
        if state["draft"]:
            state["draft"] = state["draft"][:-1]
            update(message=f"Draft point removed; {len(state['draft'])} left.")
        else:
            update(message="No draft point to undo.")

    def delete_point():
        sel = state["selection"]
        if sel and sel[0] == "trace" and sel[2] is not None:
            ti, pi = sel[1], sel[2]
            traces = [list(tr) for tr in state["traces"]]
            del traces[ti][pi]
            if len(traces[ti]) >= 2:
                state["selection"] = ("trace", ti, None)
                msg = f"Point deleted; trace {ti + 1} now has {len(traces[ti])} points."
            else:
                del traces[ti]
                state["selection"] = None
                msg = f"Point deleted; trace {ti + 1} fell below 2 points and was removed."
            state["traces"] = traces
            update(message=msg)
        else:
            update(message="No trace point selected - click a trace point first.")

    def delete_trace():
        sel = state["selection"]
        if sel and sel[0] == "trace":
            ti = sel[1]
            traces = [list(tr) for tr in state["traces"]]
            del traces[ti]
            state["traces"] = traces
            state["selection"] = None
            update(message=f"Trace {ti + 1} deleted; {len(traces)} remain.")
        else:
            update(message="No trace selected - click a trace point first.")

    def delete_selected():
        sel = state["selection"]
        if sel and sel[0] == "trace" and sel[2] is not None:
            delete_point()
        elif sel and sel[0] == "trace":
            delete_trace()
        else:
            update(message="Nothing deletable selected - select a trace point first.")

    def clear_traces():
        state["traces"] = []
        state["draft"] = []
        state["_last_append"] = None
        if state["selection"] and state["selection"][0] == "trace":
            state["selection"] = None
        update(message="All traces cleared.")

    def reset_all():
        p.update(DEFAULTS)
        state["selection"] = None
        state["active_dial"] = "crest_width"
        update(narrate_key="crest_width",
               message="Reset to the canonical shield.")

    # ------------------------- widgets --------------------------------
    for key, label, lo, hi, step, _eff in DIALS:
        frame = tk.Frame(panel, bg="#101018")
        frame.pack(anchor="w", fill="x", pady=1)
        lab = tk.Label(frame, text=label, fg="#ccccdd", bg="#101018",
                       font=("Consolas", 9), width=15, anchor="w")
        lab.pack(side="left")
        var = tk.DoubleVar(value=p[key])
        sc = tk.Scale(frame, from_=lo, to=hi, resolution=step, orient="horizontal",
                      variable=var, length=170, showvalue=1, bg="#1a1a2a", fg="#eeeeff",
                      troughcolor="#262640", highlightthickness=0,
                      command=lambda v, k=key: set_dial(k, v))
        sc.pack(side="left")
        dial_vars[key] = var
        dial_labels[key] = lab
        lab.bind("<Button-1>", lambda e, k=key: activate_dial(k))
        sc.bind("<Button-1>", lambda e, k=key: activate_dial(k), add="+")

    opts = tk.Frame(panel, bg="#101018")
    opts.pack(anchor="w", pady=(8, 2))
    for name, key in (("Halo", "halo"), ("Rivets", "rivets"), ("Inner line", "inner")):
        v = tk.BooleanVar(value=state[key])
        tk.Checkbutton(opts, text=name, variable=v, bg="#101018", fg="#ccccdd",
                       selectcolor="#1a1a2a", activebackground="#101018",
                       font=("Consolas", 9),
                       command=lambda k=key, vv=v: set_flag(k, vv.get())).pack(side="left")

    side_row = tk.Frame(panel, bg="#101018")
    side_row.pack(anchor="w", pady=(2, 0))
    tk.Label(side_row, text="Glyph ring side:", fg="#ccccdd", bg="#101018",
             font=("Consolas", 9)).pack(side="left")
    side_var = tk.StringVar(value=state["ring_side"])
    for sname in ("inside", "outside"):
        tk.Radiobutton(side_row, text=sname.capitalize(), value=sname,
                       variable=side_var, bg="#101018", fg="#ccccdd",
                       selectcolor="#1a1a2a", activebackground="#101018",
                       font=("Consolas", 9),
                       command=lambda: set_ring_side(side_var.get())).pack(side="left")

    trace_var = tk.BooleanVar(value=False)
    tk.Checkbutton(panel, text="TRACE MODE (click canvas to lay traces)", variable=trace_var,
                   bg="#101018", fg=TRACE_C, selectcolor="#1a1a2a",
                   activebackground="#101018", font=("Consolas", 9, "bold"),
                   command=lambda: set_trace_mode(trace_var.get())).pack(anchor="w", pady=(4, 2))

    btns = tk.Frame(panel, bg="#101018")
    btns.pack(anchor="w", pady=2)
    tk.Button(btns, text="Finish trace", command=finish_trace, bg="#262640", fg="#eeeeff").pack(side="left", padx=2)
    tk.Button(btns, text="Undo point", command=undo_point, bg="#262640", fg="#eeeeff").pack(side="left", padx=2)
    tk.Button(btns, text="Clear traces", command=clear_traces, bg="#262640", fg="#eeeeff").pack(side="left", padx=2)

    btns2 = tk.Frame(panel, bg="#101018")
    btns2.pack(anchor="w", pady=2)
    tk.Button(btns2, text="Delete point", command=delete_point, bg="#262640", fg="#eeeeff").pack(side="left", padx=2)
    tk.Button(btns2, text="Delete trace", command=delete_trace, bg="#262640", fg="#eeeeff").pack(side="left", padx=2)

    def export_svg():
        path = filedialog.asksaveasfilename(defaultextension=".svg",
                                            filetypes=[("SVG", "*.svg")],
                                            initialfile="shield-protego-maxima.svg")
        if path:
            with open(path, "w") as f:
                f.write(build_svg(p, state["text"], state["traces"],
                                  state["halo"], state["rivets"], state["inner"],
                                  ring_side=state["ring_side"]))
            status.config(text=f"Exported SVG: {path}")

    tk.Button(btns2, text="Export SVG", command=export_svg, bg="#262640", fg="#eeeeff").pack(side="left", padx=2)
    tk.Button(btns2, text="Reset", command=reset_all, bg="#262640", fg="#eeeeff").pack(side="left", padx=2)

    status.pack(anchor="w", pady=(10, 0))
    effect.pack(anchor="w", pady=(2, 0))

    tk.Label(panel, text="LIVE SPECS", fg=VIOLET, bg="#101018",
             font=("Consolas", 9, "bold")).pack(anchor="w", pady=(10, 0))
    specs_label = tk.Label(panel, text="", fg="#b8e6b8", bg="#0a0f0a",
                           font=("Consolas", 8), justify="left", anchor="w")
    specs_label.pack(anchor="w", fill="x", pady=(2, 0))

    def copy_specs():
        root.clipboard_clear()
        root.clipboard_append("Systerian Shield - Protego Maxima spec\n" +
                              compute_specs(p, state["text"], state["ring_side"]) +
                              "\nDials: " +
                              "; ".join(f"{k}={v}" for k, v in p.items()))
        status.config(text="Specs copied to clipboard.")

    tk.Button(panel, text="Copy specs", command=copy_specs, bg="#262640",
              fg="#eeeeff").pack(anchor="w", pady=(4, 0))
    tk.Label(panel, text="Click a vertex or a glyph -> its dials light up. Glyphs are clickable\n"
                         "in TRACE MODE too (a click on one selects it, never lays a point).\n"
                         "Scroll wheel turns the ACTIVE (orange) dial.\n"
                         "Delete removes a selected trace point; Esc clears the selection.\n"
                         "In TRACE MODE, clicks lay traces point to point - vertices take the\n"
                         "point exactly where clicked, joint to joint; double-click finishes.\n"
                         "The preview auto-fits - nothing clips. Resize or maximise freely:\n"
                         "the working area stays docked left/top and re-fits; the panel\n"
                         "stays docked right.",
             fg="#777790", bg="#101018", font=("Consolas", 8), justify="left").pack(anchor="w", pady=(8, 0))

    # ------------------------- resize --------------------------------
    # A canvas <Configure> event is a VIEW mutation: the working area
    # changed size (window resize, maximise, restore), so the view
    # must be re-fitted and regenerated. The handler records the new
    # ACTUAL size - the single write to state["canvas_size"] - and
    # routes the redraw through the funnel's update() like every
    # other mutation, debounced: a drag-resize streams Configure
    # events, and each one cancels the pending redraw and rearms it,
    # so the app redraws at most once per RESIZE_DEBOUNCE_MS pause
    # and the final size always gets a full, correct redraw.
    # Platform-neutral: driven by the canvas's own Configure events,
    # never by OS-specific calls - Windows maximise and X11 resize
    # arrive through exactly this path.
    def on_canvas_configure(ev):
        size = (ev.width, ev.height)
        if size[0] <= 1 or size[1] <= 1 or size == state["canvas_size"]:
            return          # degenerate report, or a move-only event
        state["canvas_size"] = size
        if state["_resize_job"] is not None:
            root.after_cancel(state["_resize_job"])
        state["_resize_job"] = root.after(RESIZE_DEBOUNCE_MS,
                                          apply_resize)

    def apply_resize():
        state["_resize_job"] = None
        update()            # the funnel: sync, (no narration), redraw

    def flush_resize():
        """Apply any pending debounced resize NOW (tests use this;
        the running app never needs it - the timer lands on its own)."""
        if state["_resize_job"] is not None:
            root.after_cancel(state["_resize_job"])
            state["_resize_job"] = None
        update()

    # ------------------------- input handlers --------------------------
    def hit_test(dx, dy):
        """What lives at design point (dx, dy)? Priority: outline
        vertices -> glyphs (full cell bounding circle) -> finished
        trace points -> None (empty space). One shared test for both
        modes; click_at decides what a hit means per mode (edit mode
        selects any hit; trace mode consumes glyph hits only)."""
        scale = state["view"]["scale"]
        tol = HIT_TOL_PX / scale
        best = None
        for kind, label, (vx, vy) in vertex_handles(p):
            dist = math.hypot(vx - dx, vy - dy)
            if dist <= tol and (best is None or dist < best[0]):
                best = (dist, kind, label)
        if best:
            _d, kind, label = best
            return ("vertex", kind, label)
        gtol = math.hypot(50.0, 70.0) * p["glyph_size"]
        best = None
        for idx, (_segs, _verts, anchor) in enumerate(
                glyph_ring(p, state["text"], state["ring_side"])):
            dist = math.hypot(anchor[0] - dx, anchor[1] - dy)
            if dist <= gtol and (best is None or dist < best[0]):
                best = (dist, idx)
        if best:
            idx = best[1]
            return ("glyph", idx, state["text"][idx])
        for ti, tr in enumerate(state["traces"]):
            for pi, (vx, vy) in enumerate(tr):
                if math.hypot(vx - dx, vy - dy) <= tol:
                    return ("trace", ti, pi)
        return None

    def select_hit(hit):
        if hit[0] == "vertex":
            _t, kind, label = hit
            gov = VERTEX_DIALS[kind]
            names = ", ".join(DIAL_BY_KEY[g][1] for g in gov)
            select(hit,
                   f"Vertex: {label}. Governing dials: {names}. "
                   f"Scroll turns {DIAL_BY_KEY[gov[0]][1]}.")
        elif hit[0] == "glyph":
            _t, idx, ch = hit
            select(hit,
                   f"Glyph {idx + 1} of {len(state['text'])}: {ch!r} = "
                   f"code point {char_value(ch)}. Placement dials: Glyph "
                   "phase (scroll), Glyph distance, Glyph size, Glyph "
                   "spin, Glyph spacing - plus the ring-side radios.")
        else:
            _t, ti, pi = hit
            select(hit,
                   f"Trace {ti + 1}, point {pi + 1} of "
                   f"{len(state['traces'][ti])}. Delete point / Delete "
                   "trace act on this selection.")

    def click_at(sx, sy, now=None):
        """The one canvas-click path (screen coords in; `now` is a
        monotonic timestamp in seconds - tests inject it, real events
        leave it None).

        Click fidelity contract (field bug #5): in trace mode a click
        appends exactly one point, at the click's OWN coordinates -
        never snapped or captured. Two exceptions, both deliberate:
        a click landing on a GLYPH selects it and is consumed (no
        point is laid - glyphs are selectable in both modes); and a
        press inside the de-echo window (ECHO_PX screen px and ECHO_S
        seconds of the previous appended point) is the same physical
        click's echo - a double-click's second delivery or switch
        chatter - and appends nothing, so the double-click finishes
        the trace phantom-free. Everything else appends, including
        clicks on existing points and clicks on outline VERTICES:
        point-to-point connection, joint to joint, is a feature the
        designer uses deliberately and is preserved exactly as
        clicked (vertices are selected by clicking them in edit
        mode)."""
        dx, dy = to_design(sx, sy)
        if state["trace_mode"]:
            hit = hit_test(dx, dy)
            if hit and hit[0] == "glyph":
                select_hit(hit)
                return
            t = time.monotonic() if now is None else now
            last = state["_last_append"]
            if last is not None:
                (lx, ly), lt = last
                if (t - lt <= ECHO_S and
                        math.hypot(dx - lx, dy - ly) <=
                        ECHO_PX / state["view"]["scale"]):
                    return    # the same physical click's echo
            state["draft"] = state["draft"] + [(dx, dy)]
            state["_last_append"] = ((dx, dy), t)
            update(message=f"Trace draft: {len(state['draft'])} point(s). "
                           "Double-click or Finish to complete it.")
            return
        hit = hit_test(dx, dy)
        if hit:
            select_hit(hit)
            return
        clear_selection()

    def on_click(ev):
        canvas.focus_set()
        click_at(ev.x, ev.y)

    def on_dblclick(ev):
        if state["trace_mode"]:
            finish_trace()

    def on_wheel(ev):
        key = state["active_dial"]
        d = DIAL_BY_KEY[key]
        delta = 1 if (getattr(ev, "delta", 0) > 0 or getattr(ev, "num", 0) == 4) else -1
        set_dial(key, p[key] + delta * d[4])

    canvas.bind("<Button-1>", on_click)
    canvas.bind("<Double-Button-1>", on_dblclick)
    canvas.bind("<Configure>", on_canvas_configure)
    # One wheel binding family per windowing system: binding both
    # <MouseWheel> and <Button-4>/<Button-5> double-steps on X11 Tk
    # builds that deliver both event types for a single notch.
    if root.tk.call("tk", "windowingsystem") == "x11":
        canvas.bind("<Button-4>", on_wheel)     # Linux scroll up
        canvas.bind("<Button-5>", on_wheel)     # Linux scroll down
    else:
        canvas.bind("<MouseWheel>", on_wheel)   # Windows / macOS
    canvas.bind("<Delete>", lambda e: delete_selected())
    canvas.bind("<BackSpace>", lambda e: delete_selected())
    canvas.bind("<Escape>", lambda e: clear_selection())
    text_var.trace_add("write", lambda *_: set_text(text_var.get()))

    # Automation/debug handle: the same mutators the widgets call.
    root.editor_api = {
        "p": p, "state": state, "canvas": canvas, "root": root,
        "set_dial": set_dial, "activate_dial": activate_dial,
        "set_flag": set_flag, "set_trace_mode": set_trace_mode,
        "set_ring_side": set_ring_side, "set_text": set_text,
        "click_at": click_at, "select": select,
        "clear_selection": clear_selection,
        "finish_trace": finish_trace, "undo_point": undo_point,
        "delete_point": delete_point, "delete_trace": delete_trace,
        "delete_selected": delete_selected, "clear_traces": clear_traces,
        "reset_all": reset_all,
        "view": lambda: dict(state["view"]),
        "canvas_size": canvas_px, "flush_resize": flush_resize,
        "panel": panel,
        "to_screen": to_screen, "to_design": to_design,
        "hit_test": hit_test, "vertex_handles": lambda: vertex_handles(p),
        "content_bounds": lambda: content_bounds(p, state["text"],
                                                 state["ring_side"]),
        "specs_text": lambda: specs_label.cget("text"),
        "status_text": lambda: status.cget("text"),
    }

    update(narrate_key="crest_width",
           message="Click a crest vertex or a glyph, then scroll - "
                   "vertices reshape the outline, glyphs walk the ring.")

    # Window floor: the panel at its full requested width, the canvas
    # never below MIN_CW x MIN_CH, plus the grid padding around both
    # (canvas padx 8+8 and panel padx 4+4 horizontally; pady 8+8
    # vertically). The window can grow without limit - maximising is
    # the intended way to give the design room - but never shrink
    # past the point where either side stops being usable. Set AFTER
    # the first update(), so the panel's requested size already
    # includes the populated status/specs labels.
    root.update_idletasks()
    root.minsize(MIN_CW + panel.winfo_reqwidth() + 24,
                 max(MIN_CH, panel.winfo_reqheight()) + 16)
    root.mainloop()


# ---------------------------------------------------------------------------
def selftest():
    sys.path.insert(0, "/home/hatch/workspace/user/files")
    try:
        import systerian_translator as ref
    except Exception as e:  # reference not present: geometry-only checks
        ref = None
        print("reference translator not importable:", e)
    bad = 0
    for v in range(0, 10000):
        mine = glyph_segments_for_value(v)
        if ref is not None:
            theirs = ref.glyph_segments_for_value(v)
            if [tuple(map(tuple, s)) for s in mine] != [tuple(map(tuple, s)) for s in theirs]:
                bad += 1
                if bad < 4:
                    print("MISMATCH", v)
    print("glyph check:", "OK all 10000 values" if bad == 0 else f"{bad} mismatches")
    out = shield_outline(dict(DEFAULTS))
    print(f"outline points: {len(out)}; crest y min: {min(y for _, y in out):.1f}")
    ring = glyph_ring(dict(DEFAULTS), "PROTEGO MAXIMA")
    print(f"glyph ring: {len(ring)} glyphs")
    return 1 if bad else 0


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        sys.exit(selftest())
    if "--dump-svg" in sys.argv:
        i = sys.argv.index("--dump-svg")
        path = sys.argv[i + 1] if i + 1 < len(sys.argv) else "shield-protego-maxima.svg"
        with open(path, "w") as f:
            f.write(build_svg(dict(DEFAULTS), "PROTEGO MAXIMA", []))
        print("wrote", path)
    else:
        run_gui()
