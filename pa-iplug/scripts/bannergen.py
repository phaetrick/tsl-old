#!/usr/bin/env python3
"""Generate the app banner / icon artwork as SVG.

This reproduces, outside the app, what CPP-New/va/iconrender.cpp draw5()/draw4()
render with Skia, so the artwork can be iterated on without building and running
the app. The layout is a direct port of View::measureText
(CPP-New/graphics/src/view.cpp), and the reflection follows draw4's recipe.

The wordmark is converted to outlines with fontTools, so the output renders
identically everywhere and no font has to be embedded in the SVG.

Usage:
    python3 bannergen.py                       # -> ./bannerout/
    python3 bannergen.py --name VOLTAIC --mark vee --out /tmp/art

Requires: fontTools (pip install fonttools). To rasterise on macOS, without
installing anything:  qlmanage -t -s 1024 -o . banner.svg

--------------------------------------------------------------------------------
Three things here are easy to get wrong; all three were bugs during the redesign:

1. The reflection gradient is declared in the *flipped* coordinate space, so its
   direction is inverted relative to the finished image. To fade downward on
   screen it must point upward in local coordinates -- hence `y - fs*0.4`, which
   mirrors draw4's `startytext - fontsize*.4f`. Point it the other way and the
   gradient lands off-canvas, pads with its first stop, and the reflection comes
   out a flat solid grey.

2. For the same reason the wordmark's position is baked into the path rather than
   applied with a wrapping <g transform>. An extra transform between the gradient
   and the geometry would displace the gradient too.

3. The horizon (where the background gradient reaches black) is at 2*startyimg:
   the gradient spans 0..v.height*2 but its stop is expressed over v.height. The
   mark is drawn as a black tile straddling that line, exactly as draw5 does
   (drawRect(black) then drawImageRect(icon)). Drawn as a bare stroke instead, it
   reads as floating above the wordmark rather than embedded in the picture.
--------------------------------------------------------------------------------
"""
import argparse
import math
import os
import re
import struct
import sys
import tempfile

from fontTools.ttLib import TTFont
from fontTools.pens.boundsPen import BoundsPen
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.misc.transform import Transform

HERE = os.path.dirname(os.path.abspath(__file__))
PA = os.path.dirname(HERE)                       # pa-iplug/
ROOT = os.path.dirname(PA)                       # programming/
ROBOTO = os.path.join(PA, "resources", "fonts", "Roboto-Regular.ttf")
FUTURA_H = os.path.join(ROOT, "CPP-New", "graphics", "fonts", "futura_extra_bold.h")
# Overused Grotesk, the face the website uses, instantiated from its variable
# original at wght=800 (the weight the site's own navbar/logo renders at).
# SIL Open Font License 1.1 -- unlike Futura it can be embedded in a shipping
# binary without a licence purchase, which is why the wordmark moved to it.
GROTESK = os.path.join(PA, "resources", "fonts", "OverusedGrotesk-ExtraBold.ttf")
# Jost* at wght=900, instantiated from its variable original. A Futura revival,
# SIL OFL, so unlike Futura it can be embedded in a shipping binary. Measured
# against Futura Extra Bold on O-circularity, stem, V width and word width it is
# the closest free face tested: 4.9% mean deviation, vs 7.9% for League Spartan
# and 13.5% for Overused Grotesk. Its stem is still lighter (0.300 vs 0.352 of
# cap) -- Jost's axis stops at 900, so that gap cannot be closed.
JOST = os.path.join(PA, "resources", "fonts", "Jost-Black.ttf")

W, H = 1024, 500
VH = 250                            # draw5 does v.height /= 2 before measuring
ORANGE = "rgb(255,152,0)"           # tsl::sk_colours::orange

INSET = 0.52                        # mark size as a fraction of the tile
CORE = 0.062                        # white-hot core width, fraction of tile size
# Drop shadow under the tile, measured off sources/forum/pics/gsbanner.png:
# background darkens ~27% at the tile edge and recovers within ~10px on a 162px
# tile, i.e. a tight blur with a slight downward offset -- not a soft halo.
SHADOW_DY, SHADOW_BLUR, SHADOW_ALPHA = 0.012, 0.024, 0.55
SPILL_GAP = 0.10                    # gap below the mark before its light pools
# Sun elevation above the horizon, in degrees. Ground-patch length goes as
# 1/tan(elevation): at dawn (~0) shadows run to infinity, lifting the sun pulls
# them in. 3pm is roughly 35 degrees. The sun stays on the centre line and the
# viewer is centred too, so everything projects straight down it: no lean.
LOCKUP_GAP = 0.02                   # mark-to-text gap, fraction of font size
SUN_ELEVATION_DEG = 35.0
FOCAL = 1024.0                      # focal length in px (~54 deg horizontal FOV)


# ---------------------------------------------------------------- font loading

def extract_baked_font(header_path, out_path):
    """Decode a binary_to_compressed_c.cpp style uint32 array back into a .ttf.

    The font is read out of the baked header rather than kept as a separate file
    so this script adds no font binary to the repo.
    """
    src = open(header_path).read()
    m = re.search(r"(\w+)_size\s*=\s*(\d+)", src)
    if not m:
        raise SystemExit(f"no _size found in {header_path}")
    size = int(m.group(2))
    body = src[src.index("{", src.index("_data")):]
    words = [int(w, 16) for w in re.findall(r"0x([0-9a-fA-F]{1,8})", body)]
    data = b"".join(struct.pack("<I", w) for w in words)[:size]
    with open(out_path, "wb") as f:
        f.write(data)
    return out_path


_upem = _glyphs = _cmap = _hmtx = None


def set_font(path):
    global _upem, _glyphs, _cmap, _hmtx
    f = TTFont(path)
    _upem = f["head"].unitsPerEm
    _glyphs = f.getGlyphSet()
    _cmap = f.getBestCmap()
    _hmtx = f["hmtx"]


def _run(text):
    """Yield (glyphname, x_offset) in font units."""
    x = 0
    for ch in text:
        gn = _cmap.get(ord(ch), ".notdef")
        yield gn, x
        x += _hmtx[gn][0]


def ink_bounds(text, fs):
    """Tight ink bounds in SVG coords (y down), like SkFont::measureText(&bounds)."""
    s = fs / _upem
    xmin = ymin = 1e18
    xmax = ymax = -1e18
    for gn, xo in _run(text):
        bp = BoundsPen(_glyphs)
        _glyphs[gn].draw(bp)
        if bp.bounds is None:
            continue
        x0, y0, x1, y1 = bp.bounds
        xmin = min(xmin, x0 + xo); xmax = max(xmax, x1 + xo)
        ymin = min(ymin, y0);      ymax = max(ymax, y1)
    left, right = xmin * s, xmax * s
    top, bottom = -ymax * s, -ymin * s          # font units are y-up, SVG is y-down
    return dict(w=right - left, h=bottom - top,
                cx=(left + right) / 2, cy=(top + bottom) / 2, top=top)


def text_path(text, fs, x0=0.0, y0=0.0):
    """Outline path with the baseline origin baked in at (x0, y0). See note 2."""
    s = fs / _upem
    pen = SVGPathPen(_glyphs)
    for gn, xo in _run(text):
        _glyphs[gn].draw(TransformPen(pen, Transform(s, 0, 0, -s, x0 + xo * s, y0)))
    return pen.getCommands()


def layout(text):
    """Port of View::measureText(v, font, text, .5, ...) plus draw5's follow-up.

    Note the text is scaled to a fixed 50% of the banner width, so a shorter name
    yields a *larger* wordmark.
    """
    fs = 100.0
    b = ink_bounds(text, fs)
    fs = 100.0 * (W / b["w"])
    b = ink_bounds(text, fs)
    if b["h"] > VH:
        fs *= VH / b["h"]
    fs *= 0.5
    b = ink_bounds(text, fs)
    x = W * 0.5 - b["cx"]
    y = VH * 0.5 - b["cy"]
    y += VH - fs * 0.25                         # startytext += v.height - fontsize*.25
    imgsize = math.floor(y / 2.5)
    startyimg = math.floor((y - imgsize) * 0.5)
    return dict(fs=fs, x=x, y=y, b=b, imgsize=imgsize, startyimg=startyimg,
                stopyimg=startyimg + imgsize, startximg=(W - imgsize) / 2,
                horizon=2 * startyimg)          # see note 3


# ------------------------------------------------------------------- the marks
# All marks are generated from the DSP the original logo used (createLogo2): a
# square wave taken through an FFT and truncated to bins < 14, which for a square
# leaves harmonics 1, 3 and 5. The wobble in the strokes is real Gibbs ringing.

def ringing(t):
    return (math.sin(t) + math.sin(3 * t) / 3 + math.sin(5 * t) / 5) / (1 + 1 / 3 + 1 / 5)


def _poly(pts):
    return "M" + "L".join(f"{x:.2f} {y:.2f}" for x, y in pts)


def _sweep(size, x0, y0, span, fn):
    """Draw fn(u)->amplitude across a horizontal span centred on the box."""
    N = 480
    cx, cy = x0 + size * 0.5, y0 + size * 0.5
    w = size * span
    return _poly([(cx - w * 0.5 + (i / (N - 1)) * w, cy - fn(i / (N - 1)) * size)
                  for i in range(N)])


def mark_arc(size, x0, y0, span=1.0):
    return _sweep(size, x0, y0, span, lambda u:
                  math.sin(math.pi * u) ** 0.65 * ringing(u * 1.6 * 2 * math.pi) * 0.40)


def mark_coil(size, x0, y0, span=1.0):
    return _sweep(size, x0, y0, span, lambda u:
                  min(1.0, math.sin(math.pi * u) * 2.4)
                  * ringing(u * 3.2 * 2 * math.pi) * 0.34)


def mark_bolt(size, x0, y0, span=1.0):
    return _sweep(size, x0, y0, span, lambda u:
                  0.34 - (abs(u - 0.5) * 2) * 0.62
                  - ringing(u * 1.15 * 2 * math.pi) * 0.20)


def mark_vee(size, x0, y0, span=1.0):
    """mark_bolt inverted -- the single sweep opens downward into a V."""
    return _sweep(size, x0, y0, span, lambda u:
                  -(0.34 - (abs(u - 0.5) * 2) * 0.62
                    - ringing(u * 1.15 * 2 * math.pi) * 0.20))


MARKS = {"arc": mark_arc, "coil": mark_coil, "bolt": mark_bolt, "vee": mark_vee}

_uid = [0]


def _nid():
    _uid[0] += 1
    return f"u{_uid[0]}"


def glow_wire(d, size, pid, core=CORE):
    """Incandescent wire: orange halo -> orange body -> white-hot core.

    `core` is the core width as a fraction of `size`; the rest are ratios off it.
    In the banner the core is sized to match the Futura Extra Bold stem
    (~0.21 x cap height) so the mark and the wordmark carry the same weight.
    """
    c = size * core
    return f"""
  <path d="{d}" fill="none" stroke="{ORANGE}" stroke-width="{c*3.2:.2f}"
        stroke-linecap="round" stroke-linejoin="round" filter="url(#{pid}g)" opacity="0.9"/>
  <path d="{d}" fill="none" stroke="{ORANGE}" stroke-width="{c*2.2:.2f}"
        stroke-linecap="round" stroke-linejoin="round" filter="url(#{pid}g2)"/>
  <path d="{d}" fill="none" stroke="{ORANGE}" stroke-width="{c*1.55:.2f}"
        stroke-linecap="round" stroke-linejoin="round"/>
  <path d="{d}" fill="none" stroke="#fff" stroke-width="{c:.2f}"
        stroke-linecap="round" stroke-linejoin="round"/>"""


def dark_pool(d, size, pid):
    """A black counterpart to glow_wire: soft dark halo hugging the mark.

    Alternative to the hard-edged tile -- it darkens the background around the
    mark so it still sits *in* the picture, without a square.
    """
    c = size * CORE
    return f"""
  <path d="{d}" fill="none" stroke="#000" stroke-width="{c*9:.2f}"
        stroke-linecap="round" stroke-linejoin="round" filter="url(#{pid}dk)" opacity="0.92"/>
  <path d="{d}" fill="none" stroke="#000" stroke-width="{c*5:.2f}"
        stroke-linecap="round" stroke-linejoin="round" filter="url(#{pid}dk2)" opacity="0.85"/>"""


def wire_shadow(d, size, pid):
    """Drop shadow cast by the mark itself.

    Needed whenever there is no tile: the tile's shadow is attached to the rect,
    so dropping the rect drops the shadow with it.

    The offset is much larger than the tile's (0.12 vs 0.012 of size) and that is
    forced, not a style choice: the mark's own orange halo is drawn on top and
    spreads ~size/22, so a tight shadow is completely covered by it. Measured
    against the no-shadow render, this lands ~24% darkening relative to the local
    background -- matching the ~27% under Grainstorm's tile.
    """
    c = size * CORE
    return f"""
  <g transform="translate(0,{size*0.12:.2f})">
   <path d="{d}" fill="none" stroke="#000" stroke-width="{c*3.4:.2f}"
         stroke-linecap="round" stroke-linejoin="round"
         filter="url(#{pid}ws)" opacity="0.90"/>
  </g>"""


def path_yspan(d):
    """(top, bottom) of a generated mark path."""
    ys = [float(p.split(" ")[1]) for p in d.replace("M", "").split("L")]
    return min(ys), max(ys)


def path_ymax(d):
    """Lowest point of a generated mark path."""
    return max(float(p.split(" ")[1])
               for p in d.replace("M", "").split("L"))


def floor_bottom(y_base, y_horizon, img_height):
    """Where an object's ground patch ends, from the sun's elevation.

    Level camera at height h: a floor point at distance d projects to
    u = f*h/d below the horizon. An object of world height H throws a patch of
    length L = H/tan(elevation) toward the viewer, and since H/h = p/u for an
    object of image height p standing at u, the projected end works out at

        u1 = u0 / (1 - p / (f * tan(elevation)))

    Note what dropped out: distance. Each object's patch follows from its own
    image height and its own depth below the horizon, so two objects at
    different distances stay consistent automatically -- no shared fudge factor.

    tan(elevation) in the denominator is why a dawn sun (elevation -> 0) throws
    patches that run off the frame, and why lifting the sun shortens them.
    """
    u0 = y_base - y_horizon
    denom = 1 - img_height / (FOCAL * math.tan(math.radians(SUN_ELEVATION_DEG)))
    if denom <= 0.05:                       # sun too low: patch passes the camera
        denom = 0.05
    return y_horizon + u0 / denom


def persp_bands(use_id, cx, y_base, y_bottom, y_horizon, pid, tag,
                half_extent=300.0):
    """Project a floor patch by slicing it into bands and scaling each by the
    exact perspective law: horizontal scale = u/u0, where u is depth below the
    horizon. No lean -- the viewer is centred and looking straight ahead, so the
    projection is symmetric about the centre line.

    SVG transforms are affine, so a single transform cannot produce a trapezoid;
    banding is the standard way round that. Content is defined once in <defs>
    and referenced with <use>, so the copies cost almost nothing.

    The clip goes on an OUTER group and the transform on an inner one -- putting
    both on the same element scales the clip rectangle too and the bands stop
    lining up.
    """
    u0 = y_base - y_horizon
    if u0 <= 0 or y_bottom <= y_base:
        return ""
    # Band count is derived, not fixed: the visible artefact is the horizontal
    # jump between neighbouring strips at the widest point, which is
    # half_extent * (total scale change) / bands. Keep that under half a pixel
    # and the seams disappear. A fixed count silently starts showing stripes as
    # soon as the taper gets steeper.
    dscale = (y_bottom - y_horizon) / u0 - 1.0
    bands = int(max(24, min(420, math.ceil(half_extent * dscale / 0.45))))
    parts = []
    for k in range(bands):
        ya = y_base + (y_bottom - y_base) * k / bands
        yb = y_base + (y_bottom - y_base) * (k + 1) / bands
        sx = ((ya + yb) / 2 - y_horizon) / u0
        cid = f"{pid}{tag}{k}"
        parts.append(
            f'<clipPath id="{cid}"><rect x="0" y="{ya:.2f}" width="{W}"'
            f' height="{yb - ya + 0.7:.2f}"/></clipPath>'
            f'<g clip-path="url(#{cid})"><g transform="translate({cx:.2f},0)'
            f' scale({sx:.4f},1) translate({-cx:.2f},0)">'
            f'<use href="#{use_id}" xlink:href="#{use_id}"/></g></g>')
    return "".join(parts)


def light_spill(d, size, pid, ymax, y_horizon):
    """Orange light thrown by the glowing mark onto the dark surface below it.

    A glowing object casts light, not shadow -- draw5 agrees, its icon
    reflection runs GRAY -> SK_ColorRED rather than to black. Mirrored about the
    mark's lowest point so the spill starts exactly where the mark ends.

    NOTE the gradient runs *upward* (ymax - fade): it is declared inside the
    flipped group, so its direction inverts on screen. Same trap as the
    wordmark reflection.

    Heavily blurred on purpose: mirroring a V yields a legible inverted V, and
    the two together read as an X. Blurring it past recognition turns it back
    into a pool of light, which is what it is meant to be.

    Dropped by SPILL_GAP below the mark and started well under full strength --
    a spill that begins at full brightness right at the mark's tip reads as a
    blob welded to it and flattens the whole picture.

    Its length is NOT chosen by eye -- it comes out of the shared camera model,
    so it is consistent with the wordmark's.
    """
    top = ymax + size * SPILL_GAP
    ytop, ybot = path_yspan(d)
    bottom = floor_bottom(top, y_horizon, ybot - ytop)
    return (f'<g filter="url(#{pid}sp)">'
            + persp_bands(f"{pid}spill", W / 2, top, bottom, y_horizon, pid, "sb",
                          half_extent=size)
            + "</g>")


def path_bbox(d):
    """(x0, y0, x1, y1) of a generated mark path."""
    xs, ys = [], []
    for seg in d.replace("M", "").split("L"):
        a, b = seg.split(" ")
        xs.append(float(a)); ys.append(float(b))
    return min(xs), min(ys), max(xs), max(ys)


def banner_lockup(name="VOLTAIC", mark_key="vee"):
    """Single line on flat black: the mark stands in for the leading letter.

    No background gradient, no reflection, no shadow -- so the mark has to be
    sized like a glyph rather than like a logo. Its ink height is set to the cap
    height and its white core to the Futura stem width, so it reads as the
    word's first letter sitting on the same baseline.
    """
    L = layout(name)
    i = _nid()
    fs = L["fs"]
    cap = L["b"]["h"]
    rest = name[1:]
    # layout()'s baseline leaves headroom for a mark stacked above. There is no
    # mark above any more, so centre the single line in the frame instead.
    y = H / 2 + cap / 2

    # Probe the mark once to learn its aspect, then choose the size that makes
    # its ink exactly cap-high. Scaling via `size` keeps stroke widths
    # proportional; a <g transform="scale()"> would distort them.
    p0 = path_bbox(MARKS[mark_key](1000.0, 0.0, 0.0))
    h_ratio = (p0[3] - p0[1]) / 1000.0
    w_ratio = (p0[2] - p0[0]) / 1000.0
    size = cap / h_ratio
    # The mark must be the LOGO, scaled -- not a V redrawn to letter proportions.
    # icon() generates the path at 512*INSET but keys widths and blurs to 512, so
    # the logo's real invariants are core/path = CORE/INSET and
    # blur/path = 1/(22*INSET). Sizing the stroke to the Futura stem instead gave
    # a V at 1.47x the logo's weight with half its glow: same shape, wrong mark.
    core_frac = CORE / INSET
    blur1, blur2 = size / (22 * INSET), size / (70 * INSET)
    mark_w = w_ratio * size

    rb = ink_bounds(rest, fs)
    gap = fs * LOCKUP_GAP
    # Centre on what is VISIBLE, not on the path geometry: the mark's halo bleeds
    # well past its bbox on the left while the plain white text has no bleed on
    # the right, so geometric centring reads as shifted left.
    bleed = size * core_frac * 3.2 / 2 + 3 * blur1
    total = bleed + mark_w + gap + rb["w"]
    startx = (W - total) / 2 + bleed

    b0 = path_bbox(MARKS[mark_key](size, 0.0, 0.0))
    d = MARKS[mark_key](size, startx - b0[0], y - b0[3])    # ink bottom on baseline
    # text_path takes a baseline ORIGIN; shift so the ink left edge lands where
    # we want it, not the origin
    ink_left = rb["cx"] - rb["w"] / 2
    tp = text_path(rest, fs, startx + mark_w + gap - ink_left, y)

    return f"""<svg viewBox="0 0 {W} {H}" xmlns="http://www.w3.org/2000/svg">
 <defs>
  <filter id="{i}g" x="-60%" y="-60%" width="220%" height="220%">
   <feGaussianBlur stdDeviation="{blur1:.2f}"/></filter>
  <filter id="{i}g2" x="-40%" y="-40%" width="180%" height="180%">
   <feGaussianBlur stdDeviation="{blur2:.2f}"/></filter>
 </defs>
 <rect width="{W}" height="{H}" fill="#000"/>
 {glow_wire(d, size, i, core=core_frac)}
 <path d="{tp}" fill="#fff"/>
</svg>""", L


def banner(name, mark_key="vee", backdrop="tile"):
    """backdrop: 'tile' (black square, as draw5 draws it), 'shadow' (bare mark
    with its own drop shadow), 'glow' (soft dark pool), or 'none' (mark alone)."""
    L = layout(name)
    i = _nid()
    fs, x, y = L["fs"], L["x"], L["y"]
    xi, yi, sz = L["startximg"], L["startyimg"], L["imgsize"]
    # with no tile to sit in, the mark can breathe wider
    inset = INSET if backdrop == "tile" else INSET * 1.25
    d = MARKS[mark_key](sz * inset, xi + sz * (1 - inset) / 2,
                        yi + sz * (1 - inset) / 2)
    if backdrop == "tile":
        mark_block = (
            f'<rect x="{xi:.2f}" y="{yi}" width="{sz}" height="{sz}"'
            f' rx="{sz*0.055:.1f}" fill="#000" filter="url(#{i}sh)"/>\n'
            f' <g clip-path="url(#{i}c)">{glow_wire(d, sz, i)}</g>')
    elif backdrop == "glow":
        mark_block = dark_pool(d, sz, i) + glow_wire(d, sz, i)
    elif backdrop == "shadow":
        mark_block = wire_shadow(d, sz, i) + glow_wire(d, sz, i)
    elif backdrop == "spill":
        mark_block = (light_spill(d, sz, i, path_ymax(d), L['horizon'])
                      + glow_wire(d, sz, i))
    else:
        mark_block = glow_wire(d, sz, i)
    tp = text_path(name, fs, x, y)
    ymax = path_ymax(d)
    # Fades must span exactly the patch each one covers. The patch length now
    # comes from the sun elevation, so a fixed fade (draw4's fontsize*0.4) either
    # runs out early or -- as at 35 degrees -- never reaches zero, leaving the
    # far end at full strength with a hard cut.
    mtop, mbot = path_yspan(d)
    spill_top = ymax + sz * SPILL_GAP
    spill_bottom = floor_bottom(spill_top, L["horizon"], mbot - mtop)
    text_bottom = floor_bottom(y, L["horizon"], L["b"]["h"])
    spill_src = (
        f'<g id="{i}spill" transform="translate(0,{2*(ymax + sz*SPILL_GAP):.2f})'
        f' scale(1,-1)"><path d="{d}" fill="none" stroke="url(#{i}spg)"'
        f' stroke-width="{sz*CORE*5.5:.2f}" stroke-linecap="round"'
        f' stroke-linejoin="round"/></g>')
    refl_src = (
        f'<g id="{i}refl" transform="translate(0,{2*y:.2f}) scale(1,-1)">'
        f'<path d="{tp}" fill="url(#{i}tg)"/></g>')
    text_refl = (f'<g filter="url(#{i}bt)">'
                 + persp_bands(f"{i}refl", W / 2, y, text_bottom, L["horizon"],
                               i, "tb", half_extent=W / 2 - x)
                 + "</g>")
    return f"""<svg viewBox="0 0 {W} {H}" xmlns="http://www.w3.org/2000/svg"
     xmlns:xlink="http://www.w3.org/1999/xlink">
 <defs>
  {spill_src}
  {refl_src}
  <linearGradient id="{i}bg" gradientUnits="userSpaceOnUse" x1="0" y1="0" x2="0" y2="{H}">
   <stop offset="0" stop-color="{ORANGE}"/>
   <stop offset="{yi/VH:.4f}" stop-color="#000"/>
   <stop offset="1" stop-color="#000"/>
  </linearGradient>
  <linearGradient id="{i}tg" gradientUnits="userSpaceOnUse"
     x1="{x:.2f}" y1="{y:.2f}" x2="{x:.2f}" y2="{2*y-text_bottom:.2f}">
   <stop offset="0" stop-color="#8a8a8a"/><stop offset="1" stop-color="#8a8a8a" stop-opacity="0"/>
  </linearGradient>
  <filter id="{i}bt" x="-12%" y="-12%" width="124%" height="124%">
   <feGaussianBlur stdDeviation="{fs/36:.3f} 0"/></filter>
  <filter id="{i}g" x="-60%" y="-60%" width="220%" height="220%">
   <feGaussianBlur stdDeviation="{sz/22:.2f}"/></filter>
  <filter id="{i}g2" x="-40%" y="-40%" width="180%" height="180%">
   <feGaussianBlur stdDeviation="{sz/70:.2f}"/></filter>
  <filter id="{i}sh" x="-25%" y="-25%" width="150%" height="150%">
   <feDropShadow dx="0" dy="{sz*SHADOW_DY:.2f}" stdDeviation="{sz*SHADOW_BLUR:.2f}"
                 flood-color="#000" flood-opacity="{SHADOW_ALPHA}"/></filter>
  <filter id="{i}dk" x="-80%" y="-80%" width="260%" height="260%">
   <feGaussianBlur stdDeviation="{sz/10:.2f}"/></filter>
  <filter id="{i}dk2" x="-60%" y="-60%" width="220%" height="220%">
   <feGaussianBlur stdDeviation="{sz/18:.2f}"/></filter>
  <filter id="{i}ws" x="-60%" y="-60%" width="220%" height="220%">
   <feGaussianBlur stdDeviation="{sz/20:.2f}"/></filter>
  <filter id="{i}sp" x="-60%" y="-60%" width="220%" height="220%">
   <feGaussianBlur stdDeviation="{sz/13:.2f} {sz/20:.2f}"/></filter>
  <linearGradient id="{i}spg" gradientUnits="userSpaceOnUse"
     x1="{xi:.2f}" y1="{spill_top:.2f}"
     x2="{xi:.2f}" y2="{2*spill_top-spill_bottom:.2f}">
   <stop offset="0" stop-color="{ORANGE}" stop-opacity="0.50"/>
   <stop offset="1" stop-color="{ORANGE}" stop-opacity="0"/>
  </linearGradient>
  <clipPath id="{i}c"><rect x="{xi:.2f}" y="{yi}" width="{sz}" height="{sz}"
      rx="{sz*0.055:.1f}"/></clipPath>
 </defs>
 <rect width="{W}" height="{H}" fill="url(#{i}bg)"/>
 {mark_block}
 {text_refl}
 <path d="{tp}" fill="#fff"/>
</svg>""", L


def icon(mark_key="vee", size=512):
    """Must stay identical to the tile the banner draws -- the banner renders the
    app icon, so any divergence would ship two different logos."""
    i = _nid()
    d = MARKS[mark_key](size * INSET, size * (1 - INSET) / 2, size * (1 - INSET) / 2)
    return f"""<svg viewBox="0 0 {size} {size}" xmlns="http://www.w3.org/2000/svg">
 <defs>
  <linearGradient id="{i}bg" gradientUnits="userSpaceOnUse" x1="0" y1="0" x2="0" y2="{size}">
   <stop offset="0" stop-color="#241503"/><stop offset="1" stop-color="#000"/></linearGradient>
  <filter id="{i}g" x="-60%" y="-60%" width="220%" height="220%">
   <feGaussianBlur stdDeviation="{size/22:.2f}"/></filter>
  <filter id="{i}g2" x="-40%" y="-40%" width="180%" height="180%">
   <feGaussianBlur stdDeviation="{size/70:.2f}"/></filter>
 </defs>
 <rect width="{size}" height="{size}" fill="url(#{i}bg)"/>{glow_wire(d, size, i)}
</svg>"""


PAGE_CSS = """
:root{--ink:#f2f2f6;--dim:#bcbcc8;--line:#4a4a56;--bg:#22222a;--panel:#2e2e38;}
@media (prefers-color-scheme:light){:root{--ink:#141418;--dim:#4e4e58;--line:#c8c8d2;
 --bg:#eeeef2;--panel:#fff;}}
:root[data-theme="dark"]{--ink:#f2f2f6;--dim:#bcbcc8;--line:#4a4a56;--bg:#22222a;--panel:#2e2e38;}
:root[data-theme="light"]{--ink:#141418;--dim:#4e4e58;--line:#c8c8d2;--bg:#eeeef2;--panel:#fff;}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--ink);padding:32px 20px 80px;
 font:15px/1.6 ui-sans-serif,-apple-system,"Segoe UI",Roboto,sans-serif}
.wrap{max-width:1120px;margin:0 auto}
h1{font-size:24px;font-weight:600;margin:0 0 6px;letter-spacing:-.01em}
h2{font-size:13px;font-weight:600;letter-spacing:.09em;text-transform:uppercase;
 color:var(--dim);margin:44px 0 14px;padding-bottom:8px;border-bottom:1px solid var(--line)}
p.sub{color:var(--dim);margin:0 0 14px;max-width:72ch}
.bx{background:var(--panel);border:1px solid var(--line);border-radius:10px;
 overflow:hidden;margin:0 0 22px}
.bx svg{display:block;width:100%;height:auto;border-bottom:1px solid var(--line)}
.cap{display:flex;justify-content:space-between;gap:16px;flex-wrap:wrap;
 padding:11px 15px;font-size:13px}
.cap b{font-weight:600;letter-spacing:.04em} .cap span{color:var(--dim)}
.icons{display:flex;gap:30px;flex-wrap:wrap;align-items:flex-end}
.icons figure{margin:0;text-align:center}
.ic{border-radius:22%;overflow:hidden;background:#000;box-shadow:0 0 0 1px var(--line)}
.icons svg{display:block;width:100%;height:auto}
figcaption{color:var(--dim);font-size:12px;margin-top:9px}
code{font-family:ui-monospace,Menlo,monospace;font-size:.9em;
 background:color-mix(in srgb,var(--dim) 22%,transparent);padding:1px 5px;border-radius:4px}
"""


def build_page(name, mark_key, backdrop="tile", lockup=True):
    svg, L = (banner_lockup(name, mark_key) if lockup
              else banner(name, mark_key, backdrop))
    icons = "".join(
        f'<figure><div class="ic" style="width:{w}px">{icon(mark_key)}</div>'
        f'<figcaption>{w}px</figcaption></figure>' for w in (200, 128, 64, 40))
    backdrops = "".join(
        f'<div class="bx">{banner(name, mark_key, b)[0]}<div class="cap"><b>{b}</b>'
        f'<span>{d}</span></div></div>'
        for b, d in (("spill", "orange light thrown onto the dark surface"),
                     ("tile", "black square, as draw5 draws it"))) if not lockup else ""
    others = "".join(
        f'<div class="bx">{(banner_lockup(name, k)[0] if lockup else banner(name, k, backdrop)[0])}'
        f'<div class="cap"><b>{k}</b>'
        f'<span>alternate mark</span></div></div>'
        for k in MARKS if k != mark_key)
    return f"""<meta charset="utf-8"><title>{name} — banner + icon</title>
<style>{PAGE_CSS}</style>
<div class="wrap">
<h1>{name} — banner and app icon</h1>
<p class="sub">Generated by <code>pa-iplug/scripts/bannergen.py</code>, which ports
<code>View::measureText</code> and the reflection recipe from
<code>iconrender.cpp draw4</code>. Wordmark is Futura Extra Bold, read out of
<code>CPP-New/graphics/fonts/futura_extra_bold.h</code>.</p>
<div class="bx">{svg}<div class="cap"><b>{name}</b><span>mark "{mark_key}"
 &nbsp;·&nbsp; cap height {L['b']['h']:.0f}px &nbsp;·&nbsp; tile {L['imgsize']}px
 &nbsp;·&nbsp; horizon y={L['horizon']}</span></div></div>
<h2>App icon</h2>
<div class="icons">{icons}</div>
<h2>Other backdrops</h2>
<p class="sub">The banner renders the app icon bitmap, so <b>&quot;none&quot; and &quot;glow&quot;
require the icon PNG to be authored with a transparent background</b> — Grainstorm's is
(mode LA, alpha 0..255), Pocket Analog's is not (RGBA, fully opaque), so as things stand an
opaque icon would paint its own square regardless.</p>{backdrops}
<h2>Alternate marks</h2>{others}
</div>"""


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--name", default="VOLTAIC", help="wordmark text (default: VOLTAIC)")
    ap.add_argument("--mark", default="vee", choices=sorted(MARKS),
                    help="which generated mark to use (default: vee)")
    ap.add_argument("--out", default=os.path.join(os.getcwd(), "bannerout"),
                    help="output directory")
    ap.add_argument("--font", choices=("jost", "grotesk", "futura", "roboto"),
                    default="jost",
                    help="wordmark typeface (default: jost, the free Futura match)")
    ap.add_argument("--layout", choices=("lockup", "stacked"), default="lockup",
                    help="lockup: mark replaces the first letter, flat black, no\nshadows. stacked: mark above the wordmark (default: lockup)")
    ap.add_argument("--backdrop", choices=("spill", "shadow", "tile", "glow", "none"),
                    default="spill",
                    help="what sits behind the mark (default: shadow)")
    args = ap.parse_args()

    if args.font == "jost":
        if not os.path.exists(JOST):
            sys.exit(f"missing {JOST}")
        set_font(JOST)
    elif args.font == "grotesk":
        if not os.path.exists(GROTESK):
            sys.exit(f"missing {GROTESK}")
        set_font(GROTESK)
    elif args.font == "futura":
        if not os.path.exists(FUTURA_H):
            sys.exit(f"missing {FUTURA_H}")
        tmp = os.path.join(tempfile.gettempdir(), "futura_extra_bold.ttf")
        set_font(extract_baked_font(FUTURA_H, tmp))
    else:
        set_font(ROBOTO)

    os.makedirs(args.out, exist_ok=True)
    if args.layout == "lockup":
        svg, L = banner_lockup(args.name, args.mark)
    else:
        svg, L = banner(args.name, args.mark, args.backdrop)
    outputs = {
        "banner.svg": svg,
        "icon.svg": icon(args.mark),
        "preview.html": build_page(args.name, args.mark, args.backdrop,
                                   lockup=(args.layout == "lockup")),
    }
    for fn, data in outputs.items():
        with open(os.path.join(args.out, fn), "w") as f:
            f.write(data)
        print(f"  wrote {os.path.join(args.out, fn)}")
    print(f"\n{args.name}: cap height {L['b']['h']:.0f}px, tile {L['imgsize']}px, "
          f"horizon y={L['horizon']}")
    print("rasterise with:  qlmanage -t -s 1024 -o . banner.svg")


if __name__ == "__main__":
    main()
