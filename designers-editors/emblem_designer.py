#!/usr/bin/env python3
"""
Emblem Designer - Editor (desktop, pure-stdlib Python / Tkinter)
================================================================
Live designer for centre emblems - the glyph that sits in the middle
of a shield. Sibling tool to the Shield Designer - Editor
(~/workspace/user/files/systerian_shield_editor.py): the shield tool
makes the shield; THIS tool makes the centre glyph. They are two
separate designers/editors, kept separate on purpose (JD, 2026-10-15).

CREDITS (credit due, per the Director's standing rule)
  Design & direction : JD (David Rittenhouse), Wet Wafer Works -
                       the two-designer split, the four houses as
                       the starting set, feature parity with the
                       shield generator, and the 64-creature horizon
                       were his spec.
  Built by           : Elfie (Muse) + the build crew of 2026-10-15.
  The house animals  : heraldic commons - the lion, serpent, eagle
                       and badger of the four Hogwarts houses are
                       centuries-old heraldic charges; the
                       stylizations in this file are original
                       geometric constructions, drawn from no
                       published artwork.
  House style        : violet #A970FF lines, orange #FF6F00 rivets,
                       red #FF3B30 halo - the Wet Wafer Works
                       shield-editor palette.
  Motto              : "Potential in the design - Charge in the
                       traces - Static in the bank."

VERSION HISTORY (JD's rule: every integration has its place in the
history and the credits, in this file AND in
~/workspace/emblem-designer/Emblem-History.md)
  v1  2026-10-15  First pass: registry-driven emblem designer with
                  the four house animals (Lion, Serpent, Eagle,
                  Badger) as original geometric stylizations;
                  data-driven dials with per-creature memory; one
                  mutation funnel; click-a-feature-point dial
                  lighting + scroll-wheel control; trace mode with
                  the point-to-point connection and the de-echo
                  window; auto-fit view; docked layout (canvas
                  left/top, panel right); live specs; dark/checker
                  preview backgrounds; SVG + transparent PNG export;
                  CLI renders (single, 2x2 sheet, shield composite).

ROADMAP (JD, 2026-10-15): "eventually I'd like it if the emblem
designer had support for 64 different prime creatures." v1 ships the
four houses; the architecture is built for 64 from birth - the
EMBLEMS registry below is the ONE extension point. A new creature is
one registry entry (a builder function, a dial table, a vertex
table); the picker, the dial panel, the specs and the exports are
all generated from the registry, so creatures #5..#64 cost no UI
surgery. Per-creature state lives in a dict keyed by creature id,
unbounded.

Interaction (parity with the shield generator, JD's spec):
  * PICK a creature from the scrollable list (grouped by family).
    Each creature remembers its own dial values while you switch.
  * The emblem carries FEATURE POINTS (mane tip, beak, blaze...).
    CLICK one -> the panel shows which DIALS govern it and lights
    them; its primary dial becomes the ACTIVE dial.
  * SCROLL WHEEL -> turns the active dial; the emblem re-flows live
    and the status line narrates what changed and what it does.
  * Sliders can also be dragged directly; every field redraws
    instantly through the one mutation funnel.
  * TRACE MODE -> click anywhere to lay circuit traces, point to
    point; double-click or Finish completes a trace. Feature points
    are clickable in trace mode too - a click on one selects it and
    is consumed, it never lays a point. One click appends exactly
    one point at the click's own coordinates; a second press inside
    the de-echo window (4 screen px, 500 ms) is the same physical
    click's echo and appends nothing.
  * The preview AUTO-FITS the full content bounds on every redraw -
    nothing ever clips. The panel is docked RIGHT at its natural
    width; the canvas is docked LEFT/TOP and fills the rest;
    resizes are debounced through the funnel.
  * BACKGROUND toggle: dark field, or a checkerboard that shows
    exactly what the transparent PNG export will carry.
  * EXPORTS: SVG (transparent, traces included) and PNG 1200x1200
    (transparent - the file that drops into a shield centre).

CLI:  --selftest                  geometry verification, all creatures
      --render <id> <out.png|.svg>  one emblem at its defaults
      --sheet <out.png>           2x2 contact sheet of the registry
      --composite <id> <out.png>  canonical shield + emblem centre
"""

import math
import struct
import sys
import time
import zlib

# ---------------------------------------------------------------------------
# Shared geometry helpers (design space 1000 x 1000, centre (500, 500))
# ---------------------------------------------------------------------------
ECX, ECY = 500.0, 500.0


def _mirror(pts):
    """Mirror a point list about the vertical centre line."""
    return [(1000.0 - x, y) for (x, y) in pts]


def _closed(pts):
    return list(pts) + [pts[0]]


def _rot(pt, cx, cy, deg):
    a = math.radians(deg)
    ca, sa = math.cos(a), math.sin(a)
    x, y = pt[0] - cx, pt[1] - cy
    return (cx + x * ca - y * sa, cy + x * sa + y * ca)


def _finite(lines, rivets, verts):
    for poly in lines:
        for x, y in poly:
            if not (math.isfinite(x) and math.isfinite(y)):
                return False
    for x, y in rivets:
        if not (math.isfinite(x) and math.isfinite(y)):
            return False
    for _k, _l, (x, y) in verts:
        if not (math.isfinite(x) and math.isfinite(y)):
            return False
    return True


# ---------------------------------------------------------------------------
# LION - symmetric front head, two-ring zigzag sun-mane (after
# ~/workspace/shield-protego/lion/make_lion.py, parameterized)
# ---------------------------------------------------------------------------
def _lion_ring(r_tip, r_val, phase, n):
    pts = []
    for i in range(2 * n):
        a = phase + math.pi * i / n
        r = r_tip if i % 2 == 0 else r_val
        pts.append((ECX + r * math.sin(a), ECY - r * math.cos(a)))
    return pts


def build_lion(prm):
    spikes = int(round(prm["mane_spikes"]))
    reach = prm["mane_reach"]
    inner_v = prm["inner_mane"]
    fw, fl = prm["face_width"], prm["face_length"]
    slant = prm["eye_slant"]

    def fp(x, y):
        return (ECX + (x - ECX) * fw, ECY + (y - 508.0) * fl)

    outer = _lion_ring(reach, reach - 86.0, 0.0, spikes)
    inner = _lion_ring(inner_v + 52.0, inner_v, math.pi / spikes, spikes)

    half = [(500, 268), (605, 282), (668, 348), (688, 470), (668, 596),
            (618, 690), (558, 734), (500, 748)]
    half = [fp(x, y) for (x, y) in half]
    face = half + _mirror(list(reversed(half[:-1])))

    ear_r = [fp(x, y) for (x, y) in [(612, 306), (682, 322), (638, 238)]]
    ear_l = _mirror(ear_r)

    brow_r = [fp(538, 428), fp(632, 452)]
    brow_l = _mirror(brow_r)
    eye_c = fp(588, 487)
    e1 = _rot(fp(552, 478), eye_c[0], eye_c[1], slant)
    e2 = _rot(fp(624, 496), eye_c[0], eye_c[1], slant)
    eye_r = [e1, e2]
    eye_l = _mirror(eye_r)
    eye_cl = (1000.0 - eye_c[0], eye_c[1])
    cheek_r = [fp(650, 540), fp(608, 640)]
    cheek_l = _mirror(cheek_r)
    nose = _closed([fp(462, 560), fp(538, 560), fp(500, 622)])
    muzzle = [[fp(500, 622), fp(500, 656)],
              [fp(500, 656), fp(452, 668)],
              [fp(500, 656), fp(548, 668)]]
    stave = [[fp(500, 330), fp(500, 396)], [fp(462, 330), fp(538, 330)]]

    lines = [_closed(outer), _closed(inner), face, _closed(ear_r),
             _closed(ear_l), brow_r, brow_l, eye_r, eye_l, cheek_r,
             cheek_l, nose] + muzzle + stave
    rivets = [outer[i] for i in range(0, 2 * spikes, 2)]
    rivets += [eye_c, eye_cl, fp(500, 622), fp(500, 748),
               fp(462, 330), fp(538, 330)]
    inner_top = min(inner, key=lambda q: q[1])
    verts = [
        ("mane_tip", "Mane tip", outer[0]),
        ("inner_mane", "Inner mane", inner_top),
        ("ear", "Ear R", fp(638, 238)),
        ("brow", "Brow stave", fp(500, 330)),
        ("eye", "Eye R", eye_c),
        ("nose", "Nose", fp(500, 622)),
        ("chin", "Chin", fp(500, 748)),
    ]
    return lines, rivets, verts


LION_DIALS = [
    ("mane_spikes", "Mane spikes", 10, 24, 1,
     "the count of sun-mane spikes on both rings - more spikes, finer mane"),
    ("mane_reach", "Mane reach", 380, 490, 2,
     "how far the outer mane tips reach from the centre"),
    ("inner_mane", "Inner mane", 230, 360, 2,
     "the radius of the inner mane ring - dial it out and the two rings crowd together"),
    ("face_width", "Face width", 0.8, 1.2, 0.02,
     "spreads or narrows the face, ears, eyes and muzzle together"),
    ("face_length", "Face length", 0.85, 1.15, 0.02,
     "lengthens or shortens the face about its own centre"),
    ("eye_slant", "Eye slant", -20, 20, 1,
     "tilts the eye lines, in degrees - positive slants the gaze down-in, fiercer"),
]

LION_DEFAULTS = {
    "mane_spikes": 18.0, "mane_reach": 478.0, "inner_mane": 300.0,
    "face_width": 1.0, "face_length": 1.0, "eye_slant": 0.0,
}

LION_VERTEX_DIALS = {
    "mane_tip": ["mane_reach", "mane_spikes"],
    "inner_mane": ["inner_mane", "mane_spikes"],
    "ear": ["face_width"],
    "brow": ["face_width", "face_length"],
    "eye": ["eye_slant", "face_width"],
    "nose": ["face_length", "face_width"],
    "chin": ["face_length"],
}


# ---------------------------------------------------------------------------
# SERPENT - vertical S-curve spine, twin offset edges, diamond head,
# forked tongue. Coils ease in at the neck so the body leaves the head
# vertically before it starts to sway.
# ---------------------------------------------------------------------------
def build_serpent(prm):
    coils = prm["coils"]
    amp = prm["amplitude"]
    width = prm["body_width"]
    hs = prm["head_size"]
    tongue = prm["tongue_length"]
    n = 160
    spine = []
    for i in range(n + 1):
        t = i / n
        ramp = math.sin(min(t / 0.12, 1.0) * math.pi / 2.0)
        x = ECX + amp * math.sin(2.0 * math.pi * coils * t) * ramp
        spine.append((x, 215.0 + 690.0 * t))
    left, right = [], []
    for i, (x, y) in enumerate(spine):
        t = i / n
        a = spine[max(0, i - 1)]
        b = spine[min(n, i + 1)]
        tx, ty = b[0] - a[0], b[1] - a[1]
        ln = math.hypot(tx, ty) or 1.0
        nx, ny = -ty / ln, tx / ln
        hw = (width / 2.0) * (1.0 - 0.78 * (t ** 1.15))
        left.append((x + nx * hw, y + ny * hw))
        right.append((x - nx * hw, y - ny * hw))
    lines = [spine, left, right]
    for idx in range(10, n - 9, 12):
        lines.append([left[idx], right[idx]])
    head_cy = 140.0
    top = (ECX, head_cy - 74.0 * hs)
    diamond = _closed([top, (ECX + 46.0 * hs, head_cy),
                       (ECX, head_cy + 62.0 * hs),
                       (ECX - 46.0 * hs, head_cy)])
    lines.append(diamond)
    lines.append([left[0], (ECX, head_cy + 62.0 * hs), right[0]])
    mid_y = top[1] - tongue * 0.55
    tip_y = top[1] - tongue
    lines.append([(ECX, top[1]), (ECX, mid_y), (ECX - 15.0, tip_y)])
    lines.append([(ECX, mid_y), (ECX + 15.0, tip_y)])
    rivets = [spine[i] for i in range(0, n + 1, 10)]
    rivets += [(ECX - 17.0 * hs, head_cy - 12.0),
               (ECX + 17.0 * hs, head_cy - 12.0)]
    crest_i = max(range(n + 1), key=lambda i: abs(spine[i][0] - ECX))
    verts = [
        ("head", "Serpent head", (ECX, head_cy)),
        ("tongue", "Tongue tip", (ECX, tip_y)),
        ("coil", "Coil crest", spine[crest_i]),
        ("body", "Body edge", left[n // 2]),
        ("tail", "Tail tip", spine[-1]),
    ]
    return lines, rivets, verts


SERPENT_DIALS = [
    ("coils", "Coils", 1, 4, 0.25,
     "how many S-bends the spine makes on its way down - quarter steps allowed"),
    ("amplitude", "Sway amplitude", 60, 200, 2,
     "how wide the coils swing, side to side"),
    ("body_width", "Body width", 30, 110, 2,
     "the body's full width at the neck; it tapers toward the tail on its own"),
    ("head_size", "Head size", 0.7, 1.4, 0.02,
     "scales the diamond head and its eyes"),
    ("tongue_length", "Tongue length", 40, 160, 2,
     "how far the forked tongue flicks above the head"),
]

SERPENT_DEFAULTS = {
    "coils": 2.5, "amplitude": 130.0, "body_width": 64.0,
    "head_size": 1.0, "tongue_length": 90.0,
}

SERPENT_VERTEX_DIALS = {
    "head": ["head_size", "tongue_length"],
    "tongue": ["tongue_length", "head_size"],
    "coil": ["amplitude", "coils"],
    "body": ["body_width"],
    "tail": ["coils", "amplitude"],
}


# ---------------------------------------------------------------------------
# EAGLE - symmetric front-facing: head with hooked beak, wings as fans
# of elongated feather spikes sweeping down-and-out on both sides.
# ---------------------------------------------------------------------------
def build_eagle(prm):
    span = prm["wing_span"]
    n = int(round(prm["feather_count"]))
    flen = prm["feather_length"]
    hs = prm["head_size"]
    beak = prm["beak_length"]

    def hp(x, y):
        return (ECX + (x - ECX) * hs, 240.0 + (y - 240.0) * hs)

    head_half = [hp(500, 148), hp(558, 176), hp(576, 234), hp(552, 298),
                 hp(524, 320), hp(500, 326)]
    head = head_half + _mirror(list(reversed(head_half[:-1])))
    brow_r = [hp(442, 232), hp(486, 248)]
    brow_l = _mirror(brow_r)
    base_y = 240.0 + 78.0 * hs
    beak_half = [(ECX, base_y - 8.0), (ECX + 34.0 * hs, base_y - 6.0),
                 (ECX + 10.0 * hs, base_y + beak * 0.7),
                 (ECX + 2.0 * hs, base_y + beak),
                 (ECX, base_y + beak * 0.8)]
    beak_line = beak_half + _mirror(list(reversed(beak_half[:-1])))
    body = _closed([(452, 345), (548, 345), (522, 478), (500, 505),
                    (478, 478)])
    lines = [head, brow_r, brow_l, beak_line, body]
    rivets = [hp(468, 256), hp(532, 256)]

    wing_r = []
    tips_r = []
    bases_r = []
    for i in range(n):
        frac = i / (n - 1) if n > 1 else 0.0
        # The wing's arm rises from the shoulder up-and-out; feather
        # bases march along it. Feathers hang DOWN from the arm,
        # rotating from down-out at the shoulder to near-vertical at
        # the wingtip, and the outer primaries are the longest - the
        # classic raised-wing silhouette (a fan of spokes it is not).
        bx = ECX + 45.0 + (span * 0.97 - 45.0) * frac
        by = 285.0 - 80.0 * frac
        bases_r.append((bx, by))
        phi = math.radians(25.0 + 75.0 * frac)
        dx, dy = math.cos(phi), math.sin(phi)
        li = flen * (0.55 + 0.55 * frac)
        if dx > 0.05:
            li = min(li, (ECX + span - bx) / dx)
        tip = (bx + li * dx, by + li * dy)
        tips_r.append(tip)
        px, py = -dy * 11.0, dx * 11.0
        wing_r.append([(bx + px, by + py), tip, (bx - px, by - py)])
        wing_r.append([(bx, by), tip])
        rivets.append(tip)
    wing_l = [_mirror(f) for f in wing_r]
    rivets += _mirror(tips_r)
    lines += wing_r + wing_l
    if n > 1:
        lines.append(bases_r)
        lines.append(_mirror(bases_r))
    verts = [
        ("crown", "Crown", hp(500, 148)),
        ("beak", "Beak tip", (ECX + 2.0 * hs, base_y + beak)),
        ("wingtip", "Wing tip R", tips_r[-1]),
        ("feather", "Wing arm R", bases_r[0]),
        ("body", "Body point", (500, 505)),
    ]
    return lines, rivets, verts


EAGLE_DIALS = [
    ("wing_span", "Wing span", 340, 490, 2,
     "the half-width the wingtips may reach, centre to tip"),
    ("feather_count", "Feather count", 3, 7, 1,
     "feathers per wing in the fan"),
    ("feather_length", "Feather length", 260, 430, 2,
     "the length of each feather spike (the span dial still caps the wingtips)"),
    ("head_size", "Head size", 0.7, 1.35, 0.02,
     "scales the head, brow and eyes about the head's own centre"),
    ("beak_length", "Beak length", 30, 90, 2,
     "how far the hooked beak drops below the head"),
]

EAGLE_DEFAULTS = {
    "wing_span": 450.0, "feather_count": 5.0, "feather_length": 340.0,
    "head_size": 1.0, "beak_length": 55.0,
}

EAGLE_VERTEX_DIALS = {
    "crown": ["head_size"],
    "beak": ["beak_length", "head_size"],
    "wingtip": ["wing_span", "feather_length"],
    "feather": ["feather_count", "feather_length"],
    "body": ["wing_span"],
}


# ---------------------------------------------------------------------------
# BADGER - symmetric broad front head, cheek fur zigzags, small ears,
# and the signature BLAZE: a bold central stripe, crown to nose.
# ---------------------------------------------------------------------------
def build_badger(prm):
    fw = prm["face_width"]
    bw = prm["blaze_width"]
    fur = int(round(prm["fur_spikes"]))
    ear = prm["ear_size"]
    snout = prm["snout_length"]

    def fx(x):
        return ECX + (x - ECX) * fw

    def sp(y):
        return 560.0 + (y - 560.0) * snout

    half = [(500, 252), (650, 258), (705, 330), (715, 450), (670, 580),
            (595, 665), (500, 690)]
    half = [(fx(x), y) for (x, y) in half]
    head = half + _mirror(list(reversed(half[:-1])))
    ear_r = _closed([(fx(605), 262), (fx(678), 268),
                     (fx(640), 268 - 58 * ear)])
    ear_l = _mirror(ear_r)
    tuft = [(ECX + dx, 252 if i % 2 == 0 else 240)
            for i, dx in enumerate((-70, -47, -23, 0, 23, 47, 70))]
    blaze = _closed([(ECX - bw / 2, 262), (ECX + bw / 2, 262),
                     (ECX + bw * 0.3, 600), (ECX - bw * 0.3, 600)])
    blaze_line = [(ECX, 262), (ECX, 600)]
    eye_r = [(fx(560), 428), (fx(596), 440)]
    eye_l = _mirror(eye_r)
    nose = _closed([(ECX - 34, sp(592)), (ECX + 34, sp(592)),
                    (ECX, sp(636))])
    mouth = [[(ECX, sp(636)), (ECX, sp(652))],
             [(ECX, sp(652)), (ECX - 34, sp(660))],
             [(ECX, sp(652)), (ECX + 34, sp(660))]]
    cheek_r = [(fx(620), 520), (fx(584), 610)]
    cheek_l = _mirror(cheek_r)

    def fur_side(sign):
        ax, ay = ECX + sign * 196 * fw, 455
        bx, by = ECX + sign * 88 * fw, 648
        m = max(2, fur // 2)
        dx, dy = bx - ax, by - ay
        ln = math.hypot(dx, dy) or 1.0
        px, py = -dy / ln, dx / ln
        pts = []
        for k in range(m + 1):
            t = k / m
            off = 13.0 if k % 2 == 1 else 0.0
            pts.append((ax + dx * t + px * off, ay + dy * t + py * off))
        return pts

    fur_r = fur_side(1)
    fur_l = _mirror(fur_r)
    lines = [head, ear_r, ear_l, tuft, blaze, blaze_line, eye_r, eye_l,
             nose, cheek_r, cheek_l, fur_r, fur_l] + mouth
    rivets = []
    for yy in (300, 380, 460, 540):
        t = (yy - 262) / (600 - 262)
        halfw = (bw / 2) + (bw * 0.3 - bw / 2) * t
        rivets += [(ECX - halfw, yy), (ECX + halfw, yy)]
    rivets += [(fx(578), 436), (1000 - fx(578), 436),
               (ECX, sp(636)), (ECX, 690)]
    verts = [
        ("crown", "Crown", (ECX, 252)),
        ("blaze", "Blaze", (ECX, 430)),
        ("ear", "Ear R", (fx(640), 268 - 58 * ear)),
        ("fur", "Cheek fur R", fur_r[len(fur_r) // 2]),
        ("nose", "Nose", (ECX, sp(636))),
    ]
    return lines, rivets, verts


BADGER_DIALS = [
    ("face_width", "Face width", 0.85, 1.3, 0.02,
     "spreads the broad head and cheeks - the badger's whole stance"),
    ("blaze_width", "Blaze width", 20, 90, 2,
     "the width of the signature centre stripe at the crown (it tapers to the nose)"),
    ("fur_spikes", "Fur spikes", 6, 14, 1,
     "zigzag count of the cheek fur, both cheeks together"),
    ("ear_size", "Ear size", 0.6, 1.5, 0.02,
     "the height of the small ears"),
    ("snout_length", "Snout length", 0.8, 1.3, 0.02,
     "pushes the nose and mouth down (or tucks them up) about the muzzle centre"),
]

BADGER_DEFAULTS = {
    "face_width": 1.05, "blaze_width": 46.0, "fur_spikes": 10.0,
    "ear_size": 1.0, "snout_length": 1.0,
}

BADGER_VERTEX_DIALS = {
    "crown": ["face_width"],
    "blaze": ["blaze_width"],
    "ear": ["ear_size", "face_width"],
    "fur": ["fur_spikes", "face_width"],
    "nose": ["snout_length", "blaze_width"],
}


# ---------------------------------------------------------------------------
# The EMBLEMS registry - THE extension point (see the roadmap note in
# the docstring). One entry = one creature: builder + dial table +
# defaults + vertex table. The picker, the dial panel, the specs and
# the exports are all generated from this table, so creatures
# #5..#64 are added here and nowhere else.
# ---------------------------------------------------------------------------
COMMON_DIALS = [
    ("scale", "Emblem scale", 0.4, 1.6, 0.02,
     "scales the whole emblem about the centre - placement, not shape"),
    ("spin", "Emblem spin", -180, 180, 3,
     "rotates the whole emblem about the centre, in degrees - placement, not shape"),
    ("line_weight", "Line weight", 0.5, 2.5, 0.05,
     "the stroke weight of every line in the emblem"),
]
COMMON_DEFAULTS = {"scale": 1.0, "spin": 0.0, "line_weight": 1.0}

HALO_DIAL = ("halo_width", "Halo width", 0, 30, 1,
             "the soft red under-stroke around the emblem - light, not shape "
             "(shared by all creatures: the house halo)")
GLOBAL_KEYS = {"halo_width"}
GLOBAL_DEFAULTS = {"halo_width": 10.0}

EMBLEMS = {
    "lion": {
        "name": "Lion", "house": "Gryffindor", "group": "House Animals",
        "builder": build_lion, "dials": LION_DIALS,
        "defaults": LION_DEFAULTS, "vertex_dials": LION_VERTEX_DIALS,
    },
    "serpent": {
        "name": "Serpent", "house": "Slytherin", "group": "House Animals",
        "builder": build_serpent, "dials": SERPENT_DIALS,
        "defaults": SERPENT_DEFAULTS, "vertex_dials": SERPENT_VERTEX_DIALS,
    },
    "eagle": {
        "name": "Eagle", "house": "Ravenclaw", "group": "House Animals",
        "builder": build_eagle, "dials": EAGLE_DIALS,
        "defaults": EAGLE_DEFAULTS, "vertex_dials": EAGLE_VERTEX_DIALS,
    },
    "badger": {
        "name": "Badger", "house": "Hufflepuff", "group": "House Animals",
        "builder": build_badger, "dials": BADGER_DIALS,
        "defaults": BADGER_DEFAULTS, "vertex_dials": BADGER_VERTEX_DIALS,
    },
}
EMBLEM_ORDER = list(EMBLEMS.keys())


def full_defaults(eid):
    d = dict(EMBLEMS[eid]["defaults"])
    d.update(COMMON_DEFAULTS)
    return d


def emblem_geometry(eid, prm):
    """Build an emblem and apply its placement (scale + spin about the
    centre). Returns (lines, rivets, verts) in design space - the ONE
    geometry every consumer (canvas, hit test, specs, exports) uses."""
    lines, rivets, verts = EMBLEMS[eid]["builder"](prm)
    scale = prm.get("scale", 1.0)
    spin = prm.get("spin", 0.0)
    if scale == 1.0 and spin == 0.0:
        return lines, rivets, verts

    def xf(pt):
        x, y = _rot(pt, ECX, ECY, spin)
        return (ECX + (x - ECX) * scale, ECY + (y - ECY) * scale)

    lines = [[xf(q) for q in poly] for poly in lines]
    rivets = [xf(q) for q in rivets]
    verts = [(k, l, xf(pos)) for (k, l, pos) in verts]
    return lines, rivets, verts


def geometry_bounds(lines, rivets, traces=(), pad=0.0):
    xs, ys = [], []
    for poly in lines:
        for x, y in poly:
            xs.append(x)
            ys.append(y)
    for x, y in rivets:
        xs.append(x)
        ys.append(y)
    for tr in traces:
        for x, y in tr:
            xs.append(x)
            ys.append(y)
    if not xs:
        return (0.0, 0.0, 1000.0, 1000.0)
    return (min(xs) - pad, min(ys) - pad, max(xs) + pad, max(ys) + pad)


def fit_view(bounds, cw, ch, pad=16.0):
    """(scale, ox, oy) fitting `bounds` into a cw x ch canvas with
    `pad` screen px to spare, centred: screen = design*scale + o.
    Pure function - the no-clip invariant is tested against it."""
    minx, miny, maxx, maxy = bounds
    bw = max(maxx - minx, 1.0)
    bh = max(maxy - miny, 1.0)
    scale = min((cw - 2 * pad) / bw, (ch - 2 * pad) / bh)
    ox = (cw - bw * scale) / 2.0 - minx * scale
    oy = (ch - bh * scale) / 2.0 - miny * scale
    return scale, ox, oy


# ---------------------------------------------------------------------------
# SVG export (transparent background, traces included, no chrome)
# ---------------------------------------------------------------------------
VIOLET_HEX = "#A970FF"
ORANGE_HEX = "#FF6F00"
HALO_HEX = "#FF3B30"
TRACE_HEX = "#6FA8FF"


def build_svg(eid, prm, traces, gparams, show_halo=True, show_rivets=True):
    lines, rivets, _verts = emblem_geometry(eid, prm)
    entry = EMBLEMS[eid]
    lw = prm.get("line_weight", 1.0)
    spec = "; ".join(f"{k}={v}" for k, v in prm.items())
    parts = [f"<!-- Emblem Designer spec: emblem={eid} "
             f"({entry['name']} - {entry['house']}); {spec}; "
             f"halo_width={gparams['halo_width']} -->\n"
             '<svg xmlns="http://www.w3.org/2000/svg" width="1000" '
             'height="1000" viewBox="0 0 1000 1000">']
    if show_halo and gparams["halo_width"] > 0:
        hw = (5.0 * lw + 2.0 * gparams["halo_width"])
        for poly in lines:
            parts.append('<polyline points="' +
                         " ".join(f"{x:.1f},{y:.1f}" for x, y in poly) +
                         f'" fill="none" stroke="{HALO_HEX}" '
                         f'stroke-opacity="0.35" stroke-width="{hw:.1f}" '
                         'stroke-linejoin="round"/>')
    for poly in lines:
        parts.append('<polyline points="' +
                     " ".join(f"{x:.1f},{y:.1f}" for x, y in poly) +
                     f'" fill="none" stroke="{VIOLET_HEX}" '
                     f'stroke-width="{5.0 * lw:.1f}" stroke-linejoin="round" '
                     'stroke-linecap="round"/>')
    for tr in traces:
        if len(tr) < 2:
            continue
        parts.append('<polyline points="' +
                     " ".join(f"{x:.1f},{y:.1f}" for x, y in tr) +
                     f'" fill="none" stroke="{TRACE_HEX}" stroke-width="5" '
                     'stroke-linejoin="round"/>')
        for vx, vy in tr:
            parts.append(f'<circle cx="{vx:.1f}" cy="{vy:.1f}" r="6" '
                         f'fill="{ORANGE_HEX}"/>')
    if show_rivets:
        for vx, vy in rivets:
            parts.append(f'<circle cx="{vx:.1f}" cy="{vy:.1f}" r="5.5" '
                         f'fill="{ORANGE_HEX}"/>')
    parts.append('</svg>')
    return "\n".join(parts)


# ---------------------------------------------------------------------------
# Pure-stdlib rasterizer + PNG codec (zlib/struct only - the tool is
# stdlib-only, so PNG rendering is hand-rolled: capsule-stamped lines
# into an RGBA buffer, 2x2-free painter's compositing, filter-0 rows)
# ---------------------------------------------------------------------------
def _stamp(buf, w, h, cx, cy, r, rgba):
    """Filled circle stamp. Opaque colours overwrite via row slices;
    translucent colours blend per pixel (halo / soft passes only)."""
    r_i = max(1, int(math.ceil(r)))
    x0, x1 = int(cx - r), int(cx + r)
    y0, y1 = int(cy - r), int(cy + r)
    sr, sg, sb, sa = rgba
    for yy in range(max(0, y0), min(h - 1, y1) + 1):
        dy = yy + 0.5 - cy
        rem = r * r - dy * dy
        if rem < 0:
            continue
        half = math.sqrt(rem)
        xa = max(0, int(cx - half))
        xb = min(w - 1, int(cx + half))
        if xb < xa:
            continue
        row = (yy * w + xa) * 4
        cnt = xb - xa + 1
        if sa >= 255:
            px = bytes((sr, sg, sb, 255))
            buf[row:row + cnt * 4] = px * cnt
        else:
            inv = 255 - sa
            for xx in range(cnt):
                o = row + xx * 4
                buf[o] = (sr * sa + buf[o] * inv) // 255
                buf[o + 1] = (sg * sa + buf[o + 1] * inv) // 255
                buf[o + 2] = (sb * sa + buf[o + 2] * inv) // 255
                buf[o + 3] = min(255, sa + buf[o + 3] * inv // 255)


def _seg_stamp(buf, w, h, a, b, r, rgba):
    ln = math.hypot(b[0] - a[0], b[1] - a[1])
    steps = max(1, int(ln / max(1.0, r * 0.6)) + 1)
    for i in range(steps + 1):
        t = i / steps
        _stamp(buf, w, h, a[0] + (b[0] - a[0]) * t,
               a[1] + (b[1] - a[1]) * t, r, rgba)


def rasterize(lines, rivets, size, halo_w=10.0, line_weight=1.0,
              traces=(), bg=None, draft=()):
    """Render geometry to an RGBA bytearray of size x size. Geometry
    is fitted to its own bounds with a 6% margin, so any emblem at
    any dial setting lands whole in the frame."""
    all_lines = list(lines) + [t for t in traces if len(t) >= 2] + \
        ([draft] if len(draft) >= 2 else [])
    bounds = geometry_bounds(all_lines, rivets,
                             [t for t in traces] + ([draft] if draft else []),
                             pad=6.0 + halo_w)
    minx, miny, maxx, maxy = bounds
    bw = max(maxx - minx, 1.0)
    bh = max(maxy - miny, 1.0)
    s = size / max(bw, bh) * 0.94
    ox = (size - bw * s) / 2.0 - minx * s
    oy = (size - bh * s) / 2.0 - miny * s

    def xf(pt):
        return (pt[0] * s + ox, pt[1] * s + oy)

    buf = bytearray(size * size * 4)
    if bg is not None:
        px = bytes((bg[0], bg[1], bg[2], 255))
        for yy in range(size):
            o = yy * size * 4
            buf[o:o + size * 4] = px * size
    lw_px = max(1.2, 5.0 * line_weight * s)
    if halo_w > 0:
        hr = (lw_px + 2.0 * halo_w * s) / 2.0
        for poly in lines:
            for a, b in zip(poly, poly[1:]):
                _seg_stamp(buf, size, size, xf(a), xf(b), hr,
                           (255, 59, 48, 80))
    for poly in lines:
        for a, b in zip(poly, poly[1:]):
            _seg_stamp(buf, size, size, xf(a), xf(b), lw_px * 0.85,
                       (169, 112, 255, 100))
    for poly in lines:
        for a, b in zip(poly, poly[1:]):
            _seg_stamp(buf, size, size, xf(a), xf(b), lw_px / 2.0,
                       (169, 112, 255, 255))
    for tr in list(traces) + ([draft] if draft else []):
        for a, b in zip(tr, tr[1:]):
            _seg_stamp(buf, size, size, xf(a), xf(b), max(1.5, 2.5 * s),
                       (111, 168, 255, 255))
        for pt in tr:
            _stamp(buf, size, size, *xf(pt), max(2.0, 6.0 * s),
                   (255, 111, 0, 255))
    rr = max(2.0, 5.5 * s)
    for pt in rivets:
        _stamp(buf, size, size, *xf(pt), rr * 1.6, (255, 111, 0, 90))
        _stamp(buf, size, size, *xf(pt), rr, (255, 111, 0, 255))
    return buf


def blit_over(dst, dw, dh, src, sw, sh, ox, oy):
    """Alpha-over a src RGBA buffer onto dst at (ox, oy)."""
    for yy in range(sh):
        dyy = oy + yy
        if dyy < 0 or dyy >= dh:
            continue
        for xx in range(sw):
            dxx = ox + xx
            if dxx < 0 or dxx >= dw:
                continue
            so = (yy * sw + xx) * 4
            sa = src[so + 3]
            if sa == 0:
                continue
            do = (dyy * dw + dxx) * 4
            if sa == 255:
                dst[do:do + 4] = src[so:so + 4]
            else:
                inv = 255 - sa
                dst[do] = (src[so] * sa + dst[do] * inv) // 255
                dst[do + 1] = (src[so + 1] * sa + dst[do + 1] * inv) // 255
                dst[do + 2] = (src[so + 2] * sa + dst[do + 2] * inv) // 255
                dst[do + 3] = min(255, sa + dst[do + 3] * inv // 255)


def png_encode(buf, w, h):
    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    raw = bytearray()
    stride = w * 4
    for yy in range(h):
        raw.append(0)
        raw += buf[yy * stride:(yy + 1) * stride]
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) +
            chunk(b"IDAT", zlib.compress(bytes(raw), 6)) +
            chunk(b"IEND", b""))


def png_decode(data):
    """Decode an 8/16-bit PNG (colour types 0,2,3,4,6) to RGBA bytes.
    Deliberately plain: correct first, fast second."""
    assert data[:8] == b"\x89PNG\r\n\x1a\n", "not a PNG"
    pos = 8
    idat = bytearray()
    plte = None
    trns = None
    w = h = depth = ctype = None
    while pos < len(data):
        ln, = struct.unpack(">I", data[pos:pos + 4])
        tag = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + ln]
        pos += 12 + ln
        if tag == b"IHDR":
            w, h, depth, ctype = struct.unpack(">IIBB", body[:10])
        elif tag == b"PLTE":
            plte = body
        elif tag == b"tRNS":
            trns = body
        elif tag == b"IDAT":
            idat += body
        elif tag == b"IEND":
            break
    raw = zlib.decompress(bytes(idat))
    ch = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[ctype]
    bpp = max(1, ch * depth // 8)
    stride = (w * ch * depth + 7) // 8
    out = bytearray(w * h * 4)
    prev = bytearray(stride)
    ppos = 0
    for yy in range(h):
        filt = raw[ppos]
        ppos += 1
        row = bytearray(raw[ppos:ppos + stride])
        ppos += stride
        if filt == 1:
            for i in range(bpp, stride):
                row[i] = (row[i] + row[i - bpp]) & 0xFF
        elif filt == 2:
            for i in range(stride):
                row[i] = (row[i] + prev[i]) & 0xFF
        elif filt == 3:
            for i in range(stride):
                a = row[i - bpp] if i >= bpp else 0
                row[i] = (row[i] + ((a + prev[i]) >> 1)) & 0xFF
        elif filt == 4:
            for i in range(stride):
                a = row[i - bpp] if i >= bpp else 0
                b = prev[i]
                c = prev[i - bpp] if i >= bpp else 0
                pp = a + b - c
                pa, pb, pc = abs(pp - a), abs(pp - b), abs(pp - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                row[i] = (row[i] + pr) & 0xFF
        prev = row
        if depth == 16:
            vals = [row[i] for i in range(0, stride, 2)]
        else:
            vals = row
        for xx in range(w):
            o = (yy * w + xx) * 4
            if ctype == 6:
                out[o:o + 4] = vals[xx * 4:xx * 4 + 4]
            elif ctype == 2:
                out[o:o + 3] = vals[xx * 3:xx * 3 + 3]
                out[o + 3] = 255
            elif ctype == 4:
                out[o] = out[o + 1] = out[o + 2] = vals[xx * 2]
                out[o + 3] = vals[xx * 2 + 1]
            elif ctype == 0:
                out[o] = out[o + 1] = out[o + 2] = vals[xx]
                out[o + 3] = 255
            elif ctype == 3:
                idx = vals[xx]
                out[o:o + 3] = plte[idx * 3:idx * 3 + 3]
                out[o + 3] = trns[idx] if trns and idx < len(trns) else 255
    return w, h, bytes(out)


# ---------------------------------------------------------------------------
# GUI
# ---------------------------------------------------------------------------
VIOLET = "#A970FF"
ORANGE = "#FF6F00"
HALO_C = "#8E2A24"
BG = "#050510"
TRACE_C = "#6FA8FF"
INITIAL_CW, INITIAL_CH = 700, 780   # canvas size at first open only
MIN_CW, MIN_CH = 380, 440           # canvas floor in the root minsize
VIEW_PAD_PX = 16.0
RESIZE_DEBOUNCE_MS = 60
# De-echo window for trace-mode clicks (shield field bug #5): a second
# press within ECHO_PX screen px AND ECHO_S seconds of the previous
# appended draft point is the same physical click's echo.
ECHO_PX = 4.0
ECHO_S = 0.5


def run_gui():
    import tkinter as tk
    from tkinter import filedialog

    HIT_TOL_PX = 20.0

    gparams = dict(GLOBAL_DEFAULTS)
    # Per-creature state, keyed by creature id, unbounded - the dict
    # IS the 64-creature scaling story (roadmap in the docstring).
    emblem_state = {eid: {"params": full_defaults(eid)}
                    for eid in EMBLEM_ORDER}
    state = {
        "current": EMBLEM_ORDER[0],
        "halo": True, "rivets": True, "checker": False,
        "trace_mode": False,
        # ("vertex", kind, label) | ("trace", ti, pi_or_None) | None
        "selection": None,
        "active_dial": EMBLEMS[EMBLEM_ORDER[0]]["dials"][0][0],
        "traces": [], "draft": [],
        "view": {"scale": 0.6, "ox": 0.0, "oy": 0.0},
        "canvas_size": (INITIAL_CW, INITIAL_CH),
        "_resize_job": None,
        "_last_append": None,
        "_syncing": False,
    }
    # Mutable holder for the ACTIVE emblem's dial tables - rebuilt on
    # every emblem switch from the registry entry, never by hand.
    active = {"dials": [], "by_key": {}}

    def cur_entry():
        return EMBLEMS[state["current"]]

    def cur_params():
        return emblem_state[state["current"]]["params"]

    def get_param(key):
        if key in GLOBAL_KEYS:
            return gparams[key]
        return cur_params()[key]

    def dial_table():
        return cur_entry()["dials"] + COMMON_DIALS + [HALO_DIAL]

    def current_geometry():
        return emblem_geometry(state["current"], cur_params())

    def content_bounds_now():
        lines, rivets, _v = current_geometry()
        pad = 8.0 + (gparams["halo_width"] if state["halo"] else 0.0) + \
            3.0 * cur_params().get("line_weight", 1.0)
        traces = list(state["traces"]) + \
            ([state["draft"]] if state["draft"] else [])
        return geometry_bounds(lines, rivets, traces, pad=pad)

    def compute_specs(view_scale=None):
        entry = cur_entry()
        prm = cur_params()
        lines, rivets, verts = current_geometry()
        minx, miny, maxx, maxy = geometry_bounds(lines, rivets)
        per = 0.0
        for poly in lines:
            for a, b in zip(poly, poly[1:]):
                per += math.hypot(b[0] - a[0], b[1] - a[1])
        out = [
            f"Emblem        : {entry['name']} - {entry['house']}",
            f"Polylines     : {len(lines)}   Rivets: {len(rivets)}   "
            f"Feature points: {len(verts)}",
            f"Bounds        : {maxx - minx:7.1f} x {maxy - miny:7.1f} u",
            f"Perimeter est : {per:7.1f} u",
            f"Placement     : scale x{prm['scale']:.2f}, "
            f"spin {prm['spin']:+.0f} deg, line weight x{prm['line_weight']:.2f}",
            f"Halo (global) : {gparams['halo_width']:.0f}",
            "Dials         : " + "; ".join(
                f"{d[0]}={get_param(d[0]):g}" for d in dial_table()),
        ]
        if view_scale is not None:
            out.append(f"Preview fit   : x{view_scale:.3f} auto-fit "
                       "(nothing clips)")
        return "\n".join(out)

    root = tk.Tk()
    root.title("Emblem Designer - Editor")
    root.configure(bg="#101018")
    # Layout (as the shield editor): panel DOCKED RIGHT at natural
    # width (column 1, weight 0); canvas DOCKED LEFT/TOP (column 0 /
    # row 0, weight 1, sticky nsew) taking every remaining pixel.
    root.columnconfigure(0, weight=1)
    root.columnconfigure(1, weight=0)
    root.rowconfigure(0, weight=1)

    canvas = tk.Canvas(root, width=INITIAL_CW, height=INITIAL_CH,
                       bg=BG, highlightthickness=0)
    canvas.grid(row=0, column=0, sticky="nsew", padx=8, pady=8)

    panel = tk.Frame(root, bg="#101018")
    panel.grid(row=0, column=1, sticky="n", padx=4, pady=8)

    tk.Label(panel, text="EMBLEM DESIGNER - EDITOR", fg=VIOLET,
             bg="#101018", font=("Consolas", 13, "bold")).pack(anchor="w")
    tk.Label(panel, text="Centre glyphs for the shield - designed here, seated there",
             fg="#8a8aa5", bg="#101018",
             font=("Consolas", 8, "italic")).pack(anchor="w", pady=(0, 6))

    status = tk.Label(panel, text="", fg="#ffd9a0", bg="#101018",
                      font=("Consolas", 9), wraplength=330, justify="left")
    effect = tk.Label(panel, text="", fg="#9fb7ff", bg="#101018",
                      font=("Consolas", 8), wraplength=330, justify="left")

    # ------------------------- creature picker ----------------------
    # A scrollable, GROUPED listbox generated from the registry -
    # never hard-coded buttons. Group header rows are inert.
    tk.Label(panel, text="Creature (registry):", fg="#ccccdd",
             bg="#101018", font=("Consolas", 9)).pack(anchor="w")
    picker_frame = tk.Frame(panel, bg="#101018")
    picker_frame.pack(anchor="w", fill="x", pady=(0, 6))
    picker = tk.Listbox(picker_frame, height=6, width=30, bg="#1a1a2a",
                        fg="#eeeeff", selectbackground="#3a2a6a",
                        font=("Consolas", 9), highlightthickness=0)
    picker.pack(side="left", fill="x", expand=True)
    pscroll = tk.Scrollbar(picker_frame, orient="vertical",
                           command=picker.yview)
    pscroll.pack(side="left", fill="y")
    picker.config(yscrollcommand=pscroll.set)
    picker_rows = []        # listbox row -> creature id or None (header)

    def build_picker():
        picker.delete(0, "end")
        picker_rows.clear()
        seen_groups = []
        for eid in EMBLEM_ORDER:
            grp = EMBLEMS[eid]["group"]
            if grp not in seen_groups:
                seen_groups.append(grp)
                picker.insert("end", f"-- {grp} --")
                picker.itemconfig("end", fg="#8a8aa5",
                                  selectbackground="#1a1a2a")
                picker_rows.append(None)
            e = EMBLEMS[eid]
            picker.insert("end", f"{e['name']} - {e['house']}")
            picker_rows.append(eid)
        mark_picker()

    def mark_picker():
        picker.selection_clear(0, "end")
        for i, eid in enumerate(picker_rows):
            if eid == state["current"]:
                picker.selection_set(i)
                picker.see(i)

    # ------------------------- the mutation funnel ------------------
    dial_vars = {}          # key -> DoubleVar (rebuilt per emblem)
    dial_labels = {}        # key -> Label (highlight target)

    def governing_dials(sel):
        if not sel:
            return []
        if sel[0] == "vertex":
            return cur_entry()["vertex_dials"].get(sel[1], [])
        return []

    def narrate(key):
        d = active["by_key"][key]
        val = get_param(key)
        shown = f"{val:.2f}" if d[4] < 1 else f"{val:.0f}"
        status.config(text=f"Dial: {d[1]} = {shown}")
        effect.config(text="Effect: " + d[5])

    def sync_dial_widgets():
        state["_syncing"] = True
        try:
            for key, var in dial_vars.items():
                if abs(var.get() - get_param(key)) > 1e-9:
                    var.set(get_param(key))
        finally:
            state["_syncing"] = False

    def update(narrate_key=None, message=None):
        sync_dial_widgets()
        if narrate_key is not None:
            narrate(narrate_key)
        if message is not None:
            status.config(text=message)
        redraw()

    # ------------------------- view transform -----------------------
    def canvas_px():
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
        bounds = content_bounds_now()
        scale, ox, oy = fit_view(bounds, *canvas_px(), VIEW_PAD_PX)
        state["view"] = {"scale": scale, "ox": ox, "oy": oy}
        canvas.delete("all")
        cw, ch = canvas_px()
        if state["checker"]:
            canvas.create_rectangle(0, 0, cw, ch, fill="#3f3f4d", outline="")
            canvas.create_rectangle(0, 0, cw, ch, fill="#191922",
                                    stipple="gray50", outline="")
        else:
            canvas.create_rectangle(0, 0, cw, ch, fill=BG, outline="")
        lines, rivets, verts = current_geometry()
        prm = cur_params()
        sel = state["selection"]

        def S(pts):
            out = []
            for x, y in pts:
                out += [x * scale + ox, y * scale + oy]
            return out

        def P(x, y):
            return x * scale + ox, y * scale + oy

        lw = max(2, int(5.0 * prm["line_weight"] * scale))
        if state["halo"] and gparams["halo_width"] > 0:
            hw = max(2, int((5.0 * prm["line_weight"] +
                             2.0 * gparams["halo_width"]) * scale))
            for poly in lines:
                canvas.create_line(S(poly), fill=HALO_C, width=hw)
        for poly in lines:
            canvas.create_line(S(poly), fill=VIOLET, width=lw)
        if state["rivets"]:
            for vx, vy in rivets:
                sx, sy = P(vx, vy)
                r = max(2.5, 5.5 * scale)
                canvas.create_oval(sx - r, sy - r, sx + r, sy + r,
                                   fill=ORANGE, outline="")
        draw_traces = list(state["traces"]) + \
            ([state["draft"]] if state["draft"] else [])
        for ti, tr in enumerate(draw_traces):
            is_draft = ti >= len(state["traces"])
            tsel = bool(sel and sel[0] == "trace" and sel[1] == ti
                        and not is_draft)
            if len(tr) >= 2:
                canvas.create_line(S(tr),
                                   fill="#BFD9FF" if tsel else TRACE_C,
                                   width=4 if tsel else 3)
            for pi, (vx, vy) in enumerate(tr):
                psel = bool(tsel and sel[2] == pi)
                sx, sy = P(vx, vy)
                r = 6 if psel else 3.5
                canvas.create_oval(sx - r, sy - r, sx + r, sy + r,
                                   fill="#FFFFFF" if psel else ORANGE,
                                   outline=ORANGE if psel else "")
        # feature-point handles (topmost) - the ONE source of handle
        # positions, consumed by both this draw loop and hit_test().
        for kind, label, (vx, vy) in verts:
            vsel = sel == ("vertex", kind, label)
            sx, sy = P(vx, vy)
            r = 8 if vsel else 5.5
            canvas.create_oval(sx - r, sy - r, sx + r, sy + r,
                               fill=ORANGE if vsel else "#221a38",
                               outline=ORANGE, width=2)
        specs_label.config(text=compute_specs(view_scale=scale))
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


    # ------------------------- mutators -----------------------------
    def set_dial(key, value):
        if state["_syncing"]:
            return            # programmatic widget sync, not a user edit
        d = active["by_key"][key]
        v = min(d[3], max(d[2], float(value)))
        if key in GLOBAL_KEYS:
            gparams[key] = v
        else:
            cur_params()[key] = v
        state["active_dial"] = key
        update(narrate_key=key)

    def activate_dial(key):
        state["active_dial"] = key
        update(narrate_key=key)

    def set_flag(key, value):
        state[key] = bool(value)
        update()

    def set_emblem(eid):
        if eid not in EMBLEMS:
            raise ValueError(f"unknown emblem {eid!r}")
        state["current"] = eid
        state["selection"] = None
        state["_last_append"] = None
        active["dials"] = dial_table()
        active["by_key"] = {d[0]: d for d in active["dials"]}
        state["active_dial"] = cur_entry()["dials"][0][0]
        build_dial_rows()
        mark_picker()
        e = cur_entry()
        update(narrate_key=state["active_dial"],
               message=f"Emblem: {e['name']} - {e['house']}. Its own "
                       "dial values were remembered. Click a feature "
                       "point -> its dials light up; scroll turns the "
                       "active dial.")

    def set_trace_mode(value):
        trace_var.set(bool(value))
        state["trace_mode"] = bool(value)
        update(message=("TRACE MODE: click the canvas to lay points, "
                        "point to point; double-click or Finish "
                        "completes a trace. Clicking a feature point "
                        "selects it instead of laying a point."
                        if value else
                        "Trace mode off. Click a feature point or a "
                        "trace point to select it."))

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
        emblem_state[state["current"]]["params"] = \
            full_defaults(state["current"])
        state["selection"] = None
        state["active_dial"] = cur_entry()["dials"][0][0]
        e = cur_entry()
        update(narrate_key=state["active_dial"],
               message=f"Reset the {e['name']} to its defaults "
                       "(other creatures keep their dials).")

    # ------------------------- dial panel (generated) ---------------
    dial_box = tk.Frame(panel, bg="#101018")
    dial_box.pack(anchor="w", fill="x")

    def build_dial_rows():
        for child in dial_box.winfo_children():
            child.destroy()
        dial_vars.clear()
        dial_labels.clear()
        for key, label, lo, hi, step, _eff in active["dials"]:
            frame = tk.Frame(dial_box, bg="#101018")
            frame.pack(anchor="w", fill="x", pady=1)
            lab = tk.Label(frame, text=label, fg="#ccccdd", bg="#101018",
                           font=("Consolas", 9), width=15, anchor="w")
            lab.pack(side="left")
            var = tk.DoubleVar(value=get_param(key))
            sc = tk.Scale(frame, from_=lo, to=hi, resolution=step,
                          orient="horizontal", variable=var, length=170,
                          showvalue=1, bg="#1a1a2a", fg="#eeeeff",
                          troughcolor="#262640", highlightthickness=0,
                          command=lambda v, k=key: set_dial(k, v))
            sc.pack(side="left")
            dial_vars[key] = var
            dial_labels[key] = lab
            lab.bind("<Button-1>", lambda e, k=key: activate_dial(k))
            sc.bind("<Button-1>", lambda e, k=key: activate_dial(k),
                    add="+")

    # ------------------------- options + buttons --------------------
    opts = tk.Frame(panel, bg="#101018")
    opts.pack(anchor="w", pady=(8, 2))
    for name, key in (("Halo", "halo"), ("Rivets", "rivets"),
                      ("Checker bg", "checker")):
        v = tk.BooleanVar(value=state[key])
        tk.Checkbutton(opts, text=name, variable=v, bg="#101018",
                       fg="#ccccdd", selectcolor="#1a1a2a",
                       activebackground="#101018", font=("Consolas", 9),
                       command=lambda k=key, vv=v: set_flag(k, vv.get())
                       ).pack(side="left")

    trace_var = tk.BooleanVar(value=False)
    tk.Checkbutton(panel, text="TRACE MODE (click canvas to lay traces)",
                   variable=trace_var, bg="#101018", fg=TRACE_C,
                   selectcolor="#1a1a2a", activebackground="#101018",
                   font=("Consolas", 9, "bold"),
                   command=lambda: set_trace_mode(trace_var.get())
                   ).pack(anchor="w", pady=(4, 2))

    btns = tk.Frame(panel, bg="#101018")
    btns.pack(anchor="w", pady=2)
    tk.Button(btns, text="Finish trace", command=finish_trace,
              bg="#262640", fg="#eeeeff").pack(side="left", padx=2)
    tk.Button(btns, text="Undo point", command=undo_point,
              bg="#262640", fg="#eeeeff").pack(side="left", padx=2)
    tk.Button(btns, text="Clear traces", command=clear_traces,
              bg="#262640", fg="#eeeeff").pack(side="left", padx=2)

    btns2 = tk.Frame(panel, bg="#101018")
    btns2.pack(anchor="w", pady=2)
    tk.Button(btns2, text="Delete point", command=delete_point,
              bg="#262640", fg="#eeeeff").pack(side="left", padx=2)
    tk.Button(btns2, text="Delete trace", command=delete_trace,
              bg="#262640", fg="#eeeeff").pack(side="left", padx=2)

    def export_svg_dialog():
        e = cur_entry()
        path = filedialog.asksaveasfilename(
            defaultextension=".svg", filetypes=[("SVG", "*.svg")],
            initialfile=f"emblem-{state['current']}.svg")
        if path:
            export_svg(path)
            status.config(text=f"Exported SVG: {path}")

    def export_png_dialog():
        path = filedialog.asksaveasfilename(
            defaultextension=".png", filetypes=[("PNG", "*.png")],
            initialfile=f"emblem-{state['current']}.png")
        if path:
            export_png(path)
            status.config(text=f"Exported PNG (1200x1200, transparent): {path}")

    btns3 = tk.Frame(panel, bg="#101018")
    btns3.pack(anchor="w", pady=2)
    tk.Button(btns3, text="Export SVG", command=export_svg_dialog,
              bg="#262640", fg="#eeeeff").pack(side="left", padx=2)
    tk.Button(btns3, text="Export PNG", command=export_png_dialog,
              bg="#262640", fg="#eeeeff").pack(side="left", padx=2)
    tk.Button(btns3, text="Reset", command=reset_all,
              bg="#262640", fg="#eeeeff").pack(side="left", padx=2)

    status.pack(anchor="w", pady=(10, 0))
    effect.pack(anchor="w", pady=(2, 0))

    tk.Label(panel, text="LIVE SPECS", fg=VIOLET, bg="#101018",
             font=("Consolas", 9, "bold")).pack(anchor="w", pady=(10, 0))
    specs_label = tk.Label(panel, text="", fg="#b8e6b8", bg="#0a0f0a",
                           font=("Consolas", 8), justify="left",
                           anchor="w")
    specs_label.pack(anchor="w", fill="x", pady=(2, 0))

    def copy_specs():
        root.clipboard_clear()
        root.clipboard_append(
            f"Emblem Designer - {cur_entry()['name']} spec\n" +
            compute_specs() + "\nDials: " +
            "; ".join(f"{d[0]}={get_param(d[0])}" for d in dial_table()))
        status.config(text="Specs copied to clipboard.")

    tk.Button(panel, text="Copy specs", command=copy_specs,
              bg="#262640", fg="#eeeeff").pack(anchor="w", pady=(4, 0))
    tk.Label(panel, text="Pick a creature; click a feature point -> its dials light up.\n"
                         "Scroll wheel turns the ACTIVE (orange) dial.\n"
                         "Feature points are clickable in TRACE MODE too (consumed,\n"
                         "never lays a point). Clicks elsewhere lay traces point to\n"
                         "point, exactly where clicked; double-click finishes.\n"
                         "Delete removes a selected trace point; Esc clears.\n"
                         "The preview auto-fits - nothing clips. Resize or maximise\n"
                         "freely: working area docked left/top, panel docked right.",
             fg="#777790", bg="#101018", font=("Consolas", 8),
             justify="left").pack(anchor="w", pady=(8, 0))

    # ------------------------- exports (api + dialogs) --------------
    def export_svg(path):
        with open(path, "w") as f:
            f.write(build_svg(state["current"], cur_params(),
                              state["traces"], gparams,
                              state["halo"], state["rivets"]))

    def export_png(path):
        lines, rivets, _v = current_geometry()
        buf = rasterize(lines, rivets, 1200,
                        halo_w=gparams["halo_width"] if state["halo"] else 0.0,
                        line_weight=cur_params()["line_weight"],
                        traces=state["traces"], draft=state["draft"])
        with open(path, "wb") as f:
            f.write(png_encode(buf, 1200, 1200))

    # ------------------------- resize -------------------------------
    def on_canvas_configure(ev):
        size = (ev.width, ev.height)
        if size[0] <= 1 or size[1] <= 1 or size == state["canvas_size"]:
            return
        state["canvas_size"] = size
        if state["_resize_job"] is not None:
            root.after_cancel(state["_resize_job"])
        state["_resize_job"] = root.after(RESIZE_DEBOUNCE_MS,
                                          apply_resize)

    def apply_resize():
        state["_resize_job"] = None
        update()

    def flush_resize():
        if state["_resize_job"] is not None:
            root.after_cancel(state["_resize_job"])
            state["_resize_job"] = None
        update()

    # ------------------------- input handlers -----------------------
    def hit_test(dx, dy):
        """Priority: feature points -> finished trace points -> None.
        One shared test for both modes; click_at decides what a hit
        means per mode."""
        scale = state["view"]["scale"]
        tol = HIT_TOL_PX / scale
        _l, _r, verts = current_geometry()
        best = None
        for kind, label, (vx, vy) in verts:
            dist = math.hypot(vx - dx, vy - dy)
            if dist <= tol and (best is None or dist < best[0]):
                best = (dist, kind, label)
        if best:
            return ("vertex", best[1], best[2])
        for ti, tr in enumerate(state["traces"]):
            for pi, (vx, vy) in enumerate(tr):
                if math.hypot(vx - dx, vy - dy) <= tol:
                    return ("trace", ti, pi)
        return None

    def select_hit(hit):
        if hit[0] == "vertex":
            _t, kind, label = hit
            gov = cur_entry()["vertex_dials"].get(kind, [])
            names = ", ".join(active["by_key"][g][1] for g in gov)
            select(hit,
                   f"Feature: {label}. Governing dials: {names}. "
                   f"Scroll turns {active['by_key'][gov[0]][1]}."
                   if gov else f"Feature: {label}.")
        else:
            _t, ti, pi = hit
            select(hit,
                   f"Trace {ti + 1}, point {pi + 1} of "
                   f"{len(state['traces'][ti])}. Delete point / Delete "
                   "trace act on this selection.")

    def click_at(sx, sy, now=None):
        """The one canvas-click path. Click fidelity contract (shield
        field bug #5): in trace mode a click appends exactly one
        point, at the click's OWN coordinates - never snapped or
        captured. Two deliberate exceptions: a click landing on a
        FEATURE POINT selects it and is consumed (feature points are
        selectable in both modes); and a press inside the de-echo
        window (ECHO_PX screen px, ECHO_S seconds of the previous
        appended point) is the same physical click's echo and
        appends nothing, so a double-click finishes phantom-free."""
        dx, dy = to_design(sx, sy)
        if state["trace_mode"]:
            hit = hit_test(dx, dy)
            if hit and hit[0] == "vertex":
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
        d = active["by_key"][key]
        delta = 1 if (getattr(ev, "delta", 0) > 0 or
                      getattr(ev, "num", 0) == 4) else -1
        set_dial(key, get_param(key) + delta * d[4])

    def on_picker_select(_ev):
        sel = picker.curselection()
        if not sel:
            return
        eid = picker_rows[sel[0]]
        if eid is None:
            mark_picker()     # group header: inert, restore selection
            return
        if eid != state["current"]:
            set_emblem(eid)

    picker.bind("<<ListboxSelect>>", on_picker_select)
    canvas.bind("<Button-1>", on_click)
    canvas.bind("<Double-Button-1>", on_dblclick)
    canvas.bind("<Configure>", on_canvas_configure)
    if root.tk.call("tk", "windowingsystem") == "x11":
        canvas.bind("<Button-4>", on_wheel)
        canvas.bind("<Button-5>", on_wheel)
    else:
        canvas.bind("<MouseWheel>", on_wheel)
    canvas.bind("<Delete>", lambda e: delete_selected())
    canvas.bind("<BackSpace>", lambda e: delete_selected())
    canvas.bind("<Escape>", lambda e: clear_selection())

    # ------------------------- automation handle --------------------
    root.editor_api = {
        "state": state, "gparams": gparams,
        "emblem_state": emblem_state, "emblems": EMBLEMS,
        "canvas": canvas, "root": root, "panel": panel,
        "picker": picker, "picker_rows": lambda: list(picker_rows),
        "set_dial": set_dial, "get_param": get_param,
        "activate_dial": activate_dial, "set_flag": set_flag,
        "set_emblem": set_emblem, "set_trace_mode": set_trace_mode,
        "click_at": click_at, "select": select,
        "clear_selection": clear_selection,
        "finish_trace": finish_trace, "undo_point": undo_point,
        "delete_point": delete_point, "delete_trace": delete_trace,
        "delete_selected": delete_selected, "clear_traces": clear_traces,
        "reset_all": reset_all,
        "export_svg": export_svg, "export_png": export_png,
        "view": lambda: dict(state["view"]),
        "canvas_size": canvas_px, "flush_resize": flush_resize,
        "to_screen": to_screen, "to_design": to_design,
        "hit_test": hit_test,
        "geometry": current_geometry,
        "content_bounds": content_bounds_now,
        "specs_text": lambda: specs_label.cget("text"),
        "status_text": lambda: status.cget("text"),
    }

    # First paint: build the picker + dial rows for the opening
    # creature, then one funnel pass.
    build_picker()
    active["dials"] = dial_table()
    active["by_key"] = {d[0]: d for d in active["dials"]}
    build_dial_rows()
    e = cur_entry()
    update(narrate_key=state["active_dial"],
           message=f"Emblem: {e['name']} - {e['house']}. Click a "
                   "feature point, then scroll - feature points "
                   "reshape the emblem.")

    root.update_idletasks()
    root.minsize(MIN_CW + panel.winfo_reqwidth() + 24,
                 max(MIN_CH, panel.winfo_reqheight()) + 16)
    root.mainloop()


# ---------------------------------------------------------------------------
# Selftest + CLI renders
# ---------------------------------------------------------------------------
def _mirror_deviation(lines, rivets):
    """Worst distance from any point to the mirror image of the point
    set - the symmetry check for the symmetric creatures."""
    pts = [q for poly in lines for q in poly] + list(rivets)
    cell = 4.0
    grid = {}
    for (x, y) in pts:
        grid.setdefault((int(x // cell), int(y // cell)), []).append((x, y))
    worst = 0.0
    for (x, y) in pts:
        tx, ty = 1000.0 - x, y
        best = None
        gx, gy = int(tx // cell), int(ty // cell)
        for ax in (gx - 1, gx, gx + 1):
            for ay in (gy - 1, gy, gy + 1):
                for (qx, qy) in grid.get((ax, ay), []):
                    d = math.hypot(qx - tx, qy - ty)
                    if best is None or d < best:
                        best = d
        if best is None:
            return float("inf")
        worst = max(worst, best)
    return worst


def selftest():
    bad = 0

    def check(cond, msg):
        nonlocal bad
        print(("OK  " if cond else "FAIL") + "  " + msg)
        if not cond:
            bad += 1

    symmetric = {"lion", "eagle", "badger"}
    for eid in EMBLEM_ORDER:
        entry = EMBLEMS[eid]
        base = full_defaults(eid)
        lines, rivets, verts = emblem_geometry(eid, dict(base))
        check(len(lines) >= 5 and len(rivets) >= 2 and len(verts) >= 4,
              f"{eid}: builds at defaults "
              f"({len(lines)} polylines, {len(rivets)} rivets, "
              f"{len(verts)} feature points)")
        check(_finite(lines, rivets, verts), f"{eid}: finite at defaults")
        again = emblem_geometry(eid, dict(base))
        check(again == (lines, rivets, verts),
              f"{eid}: deterministic (same params -> identical output)")
        # min/max sweep, one dial at a time, then all-min / all-max
        table = entry["dials"] + COMMON_DIALS
        ok = True
        for key, _l, lo, hi, _s, _e in table:
            for v in (lo, hi):
                prm = dict(base)
                prm[key] = v
                l2, r2, v2 = emblem_geometry(eid, prm)
                if not _finite(l2, r2, v2) or not l2:
                    ok = False
        for combo in ({d[0]: d[2] for d in table},
                      {d[0]: d[3] for d in table}):
            prm = dict(base)
            prm.update(combo)
            l2, r2, v2 = emblem_geometry(eid, prm)
            if not _finite(l2, r2, v2) or not l2:
                ok = False
        check(ok, f"{eid}: finite across the full dial sweep "
                  "(min/max per dial, all-min, all-max)")
        if eid in symmetric:
            dev = _mirror_deviation(lines, rivets)
            check(dev < 1.0,
                  f"{eid}: mirror symmetry at defaults "
                  f"(worst deviation {dev:.4f} u < 1.0)")
        if eid == "serpent":
            head = [p for p in lines
                    if max(y for _x, y in p) < 215 and
                    min(y for _x, y in p) > 20]
            tongue_pts = [q for p in lines for q in p if q[1] < 60]
            check(bool(head), "serpent: diamond head present")
            check(bool(tongue_pts),
                  "serpent: forked tongue reaches above the head")
    return 1 if bad else 0


SHIELD_PNG = "/home/hatch/workspace/user/files/shield-protego-maxima.png"


def cli_render(eid, out):
    prm = full_defaults(eid)
    gp = dict(GLOBAL_DEFAULTS)
    lines, rivets, _v = emblem_geometry(eid, prm)
    if out.endswith(".svg"):
        with open(out, "w") as f:
            f.write(build_svg(eid, prm, [], gp))
    else:
        buf = rasterize(lines, rivets, 1200,
                        halo_w=gp["halo_width"],
                        line_weight=prm["line_weight"])
        with open(out, "wb") as f:
            f.write(png_encode(buf, 1200, 1200))
    print(f"wrote {out}")


def cli_sheet(out):
    size, cell = 1200, 600
    sheet = bytearray(size * size * 4)
    px = bytes((5, 5, 16, 255))
    for yy in range(size):
        o = yy * size * 4
        sheet[o:o + size * 4] = px * size
    gp = dict(GLOBAL_DEFAULTS)
    for i, eid in enumerate(EMBLEM_ORDER[:4]):
        prm = full_defaults(eid)
        lines, rivets, _v = emblem_geometry(eid, prm)
        buf = rasterize(lines, rivets, cell, halo_w=gp["halo_width"],
                        line_weight=prm["line_weight"])
        blit_over(sheet, size, size, buf, cell, cell,
                  (i % 2) * cell, (i // 2) * cell)
    with open(out, "wb") as f:
        f.write(png_encode(sheet, size, size))
    print(f"wrote {out}")


def cli_composite(eid, out):
    with open(SHIELD_PNG, "rb") as f:
        sw, sh, sbuf = png_decode(f.read())
    base = bytearray(sbuf)
    prm = full_defaults(eid)
    gp = dict(GLOBAL_DEFAULTS)
    lines, rivets, _v = emblem_geometry(eid, prm)
    buf = rasterize(lines, rivets, 640, halo_w=gp["halo_width"],
                    line_weight=prm["line_weight"])
    blit_over(base, sw, sh, buf, 640, 640, 750 - 320, 800 - 320)
    with open(out, "wb") as f:
        f.write(png_encode(base, sw, sh))
    print(f"wrote {out} ({sw}x{sh})")


if __name__ == "__main__":
    args = sys.argv[1:]
    if "--selftest" in args:
        sys.exit(selftest())
    if "--render" in args:
        i = args.index("--render")
        cli_render(args[i + 1], args[i + 2])
    elif "--sheet" in args:
        i = args.index("--sheet")
        cli_sheet(args[i + 1])
    elif "--composite" in args:
        i = args.index("--composite")
        cli_composite(args[i + 1], args[i + 2])
    else:
        run_gui()
