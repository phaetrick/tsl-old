#!/usr/bin/env python3
"""Build the VOLTAIC web asset set for sources/forum/public/voltaic/.

Sources, both of them generated rather than drawn, so neither can drift from
what the app itself renders:

  banner_new_2048x1000.png  qlmanage's raster of bannergen.py's banner.svg,
                            cropped out of the square thumbnail it pads to.
  voltaic_icon_alpha.png    the 512x512 RGBA mark that mkicon.py baked into
                            CPP-New/va/icon_voltaic.cpp -- i.e. the icon the
                            shipping app draws, byte for byte.

The icons are that mark composited on an opaque black square. Black, not
transparent: every icon in public/grainstorm and public/pocketanalog is an
opaque black tile with square corners, and a transparent favicon whose artwork
is a white-cored glow would vanish on a light browser chrome.
"""
import os
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = "/Users/patrickropohl/programming/sources/forum/public/voltaic"

BANNER = os.path.join(HERE, "banner_new_2048x1000.png")
MARK = ("/private/tmp/claude-501/-Users-patrickropohl-programming-"
        "grainstorm-iplug2/e0084df1-c28e-44df-9400-2cdc0ee6327c/scratchpad/"
        "voltaic_icon_alpha.png")

ANDROID = [36, 48, 72, 96, 144, 192]
APPLE = [57, 60, 72, 76, 114, 120, 144, 152, 180]
FAVICON = [16, 32, 96]
MS = [70, 144, 150, 310]


def icon_master():
    """The mark on an opaque black square, at the mark's own resolution."""
    mark = Image.open(MARK).convert("RGBA")
    tile = Image.new("RGBA", mark.size, (0, 0, 0, 255))
    return Image.alpha_composite(tile, mark).convert("RGB")


def main():
    os.makedirs(OUT, exist_ok=True)
    master = icon_master()
    written = []

    def put(img, name, **kw):
        p = os.path.join(OUT, name)
        img.save(p, **kw)
        written.append((name, os.path.getsize(p)))

    for n in ANDROID:
        put(master.resize((n, n), Image.LANCZOS), f"android-icon-{n}x{n}.png",
            optimize=True)
    for n in APPLE:
        put(master.resize((n, n), Image.LANCZOS), f"apple-icon-{n}x{n}.png",
            optimize=True)
    for n in FAVICON:
        put(master.resize((n, n), Image.LANCZOS), f"favicon-{n}x{n}.png",
            optimize=True)
    for n in MS:
        put(master.resize((n, n), Image.LANCZOS), f"ms-icon-{n}x{n}.png",
            optimize=True)
    # Same 192px art under the two names Safari has historically looked for.
    for name in ("apple-icon.png", "apple-icon-precomposed.png"):
        put(master.resize((192, 192), Image.LANCZOS), name, optimize=True)

    master.resize((64, 64), Image.LANCZOS).save(
        os.path.join(OUT, "favicon.ico"),
        sizes=[(16, 16), (32, 32), (48, 48), (64, 64)])
    written.append(("favicon.ico", os.path.getsize(os.path.join(OUT, "favicon.ico"))))

    banner = Image.open(BANNER).convert("RGB")
    # Page banner. next/image resizes down from this, so it is stored at the
    # 2x of the 1024x500 the artwork is authored at rather than at 1x.
    put(banner, "banner-2048x1000.webp", quality=92, method=6)

    # Open Graph. 1200x630 is what Facebook and LinkedIn crop to, and the
    # artwork is 2.048:1 against their 1.905:1 -- so scale to fill the height
    # and take the middle, rather than letterboxing white bars onto a picture
    # whose whole point is the horizon line. The wordmark is centred with room
    # to spare, so 45px off each side costs nothing.
    ow, oh = 1200, 630
    scale = oh / banner.height
    tall = banner.resize((round(banner.width * scale), oh), Image.LANCZOS)
    left = (tall.width - ow) // 2
    put(tall.crop((left, 0, left + ow, oh)), "og-1200x630.png", optimize=True)

    total = sum(s for _, s in written)
    for name, size in written:
        print(f"  {name:34s} {size/1024:8.1f} KB")
    print(f"  {'TOTAL':34s} {total/1024:8.1f} KB  -> {OUT}")


if __name__ == "__main__":
    main()
