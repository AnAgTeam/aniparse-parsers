#!/usr/bin/env python3
"""
Generate the demo source's artwork — covers and reader pages — as original,
self-contained images. Nothing here is scanned or traced from a real work: every
image is drawn from primitives in the project's ukiyo-e palette, so the demo
source ships a complete, non-infringing reading experience baked into the library.

Output: src/parsers/demo/assets/{covers,ch1}/*.png
Run:     python3 tools/gen_demo_assets.py
"""
from __future__ import annotations
import math
import os
import random
from PIL import Image, ImageDraw, ImageFont, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "src", "parsers", "demo", "assets")

# ---- palette (from UI-PLAN.md, light/woodblock) ----------------------------
WASHI      = (244, 236, 217)
WASHI_DK   = (236, 225, 200)
PAPER      = (251, 245, 231)
INK        = (38, 32, 25)
INK_SOFT   = (74, 66, 54)
VERMILION  = (208, 71, 42)
GOLD       = (201, 154, 63)
NAVY       = (27, 58, 95)
NAVY_DK    = (18, 30, 50)
MUTED      = (107, 100, 85)

# ---- fonts -----------------------------------------------------------------
def _font(path: str, size: int):
    try:
        return ImageFont.truetype(path, size)
    except Exception:
        return ImageFont.load_default()

CYR   = "/System/Library/Fonts/HelveticaNeue.ttc"
CYR_B = "/System/Library/Fonts/Helvetica.ttc"
CJK   = "/System/Library/Fonts/Hiragino Sans GB.ttc"

def cyr(size, bold=False):  return _font(CYR_B if bold else CYR, size)
def cjk(size):              return _font(CJK, size)


# ---- helpers ---------------------------------------------------------------
def vgrad(size, top, bot):
    """Vertical gradient image."""
    w, h = size
    base = Image.new("RGB", (1, h))
    px = base.load()
    for y in range(h):
        t = y / max(1, h - 1)
        px[0, y] = tuple(round(top[i] + (bot[i] - top[i]) * t) for i in range(3))
    return base.resize((w, h))


def radial(size, inner, outer, cx=0.5, cy=0.4, r=0.9):
    w, h = size
    img = Image.new("RGB", size, outer)
    px = img.load()
    maxd = math.hypot(w, h) * r
    ox, oy = cx * w, cy * h
    for y in range(h):
        for x in range(0, w, 2):
            d = min(1.0, math.hypot(x - ox, y - oy) / maxd)
            c = tuple(round(inner[i] + (outer[i] - inner[i]) * d) for i in range(3))
            px[x, y] = c
            if x + 1 < w:
                px[x + 1, y] = c
    return img


def paper_texture(img, strength=6, seed=0):
    """Subtle grain so flat fills don't look digital."""
    rnd = random.Random(seed)
    px = img.load()
    w, h = img.size
    for y in range(h):
        for x in range(0, w, 3):
            n = rnd.randint(-strength, strength)
            r, g, b = px[x, y]
            px[x, y] = (max(0, min(255, r + n)),
                        max(0, min(255, g + n)),
                        max(0, min(255, b + n)))
    return img


def halftone(size, color, bg, dot=7, gap=13, seed=1):
    w, h = size
    img = Image.new("RGB", size, bg)
    d = ImageDraw.Draw(img)
    rnd = random.Random(seed)
    for gy in range(0, h, gap):
        for gx in range(0, w, gap):
            r = dot * (0.4 + 0.6 * rnd.random())
            d.ellipse([gx - r, gy - r, gx + r, gy + r], fill=color)
    return img


def text_center(d, box, text, font, fill, spacing=4):
    x0, y0, x1, y1 = box
    tb = d.multiline_textbbox((0, 0), text, font=font, spacing=spacing, align="center")
    tw, th = tb[2] - tb[0], tb[3] - tb[1]
    d.multiline_text(((x0 + x1 - tw) / 2 - tb[0], (y0 + y1 - th) / 2 - tb[1]),
                     text, font=font, fill=fill, spacing=spacing, align="center")


def bubble(d, cx, cy, w, h, text, font, tail=(0.5, 1.0)):
    box = [cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2]
    d.ellipse(box, fill=(252, 249, 242), outline=INK, width=4)
    # tail
    tx = box[0] + (box[2] - box[0]) * tail[0]
    ty = box[1] + (box[3] - box[1]) * tail[1]
    d.polygon([(tx - 12, ty - 6), (tx + 12, ty - 6), (tx + tail[0] * 20 - 4, ty + 26)],
              fill=(252, 249, 242), outline=INK)
    text_center(d, box, text, font, INK, spacing=2)


def figure(d, box, tone=NAVY_DK):
    """An abstract standing silhouette — atmosphere, not a character."""
    x0, y0, x1, y1 = box
    w = x1 - x0
    h = y1 - y0
    cx = (x0 + x1) / 2
    # head
    hr = w * 0.16
    d.ellipse([cx - hr, y0, cx + hr, y0 + 2 * hr], fill=tone)
    # body (tapered coat)
    d.polygon([(cx - w * 0.22, y1), (cx + w * 0.22, y1),
               (cx + w * 0.12, y0 + 2 * hr), (cx - w * 0.12, y0 + 2 * hr)], fill=tone)


# ---- covers ----------------------------------------------------------------
COVERS = [
    # (slug, kanji, title, subtitle, top, bottom, accent)
    ("fox-lantern",   "狐", "ЛИСИЙ\nФОНАРЬ",       "巻 一",  NAVY,     NAVY_DK,  VERMILION),
    ("midnight-tram", "電", "ПОЛНОЧНЫЙ\nТРАМВАЙ",   "夜行",   (36,28,44),(20,16,26), GOLD),
    ("ink-snow",      "雪", "ТУШЬ\nИ СНЕГ",          "墨雪",   (58,74,92), (30,42,56), PAPER),
    ("red-umbrella",  "傘", "КВАРТАЛ\nКРАСНЫХ ЗОНТОВ","紅傘",   (96,30,28), (48,16,16), GOLD),
]

def make_cover(slug, kanji, title, subtitle, top, bot, accent):
    W, H = 800, 1200
    img = radial((W, H), tuple(min(255, c + 26) for c in top), bot, cy=0.35)
    img = paper_texture(img, strength=5, seed=hash(slug) & 255)
    d = ImageDraw.Draw(img, "RGBA")
    # faint halftone wash top-right
    ht = halftone((W, H), (255, 255, 255, 12) if False else (255, 255, 255),
                  bot, dot=3, gap=22, seed=7).convert("RGBA")
    ht.putalpha(18)
    img.paste(Image.new("RGBA", (W, H)), (0, 0))  # noop keep RGBA draw
    img = Image.alpha_composite(img.convert("RGBA"), ht).convert("RGB")
    d = ImageDraw.Draw(img, "RGBA")
    # giant ghost kanji
    kf = cjk(560)
    kb = d.textbbox((0, 0), kanji, font=kf)
    kw, kh = kb[2] - kb[0], kb[3] - kb[1]
    d.text(((W - kw) / 2 - kb[0], H * 0.30 - kb[1]), kanji,
           font=kf, fill=(255, 255, 255, 26))
    # vermilion seal
    d.rounded_rectangle([W - 150, 46, W - 46, 150], radius=10,
                        fill=(accent[0], accent[1], accent[2], 235))
    text_center(d, [W - 150, 46, W - 46, 150], subtitle, cjk(40), (250, 244, 232))
    # title band
    by = H - 300
    d.rectangle([0, by, W, H], fill=(bot[0], bot[1], bot[2], 205))
    d.rectangle([48, by + 34, 48 + 70, by + 40], fill=accent)
    d.multiline_text((48, by + 60), title, font=cyr(66, bold=True),
                     fill=(250, 245, 234), spacing=6)
    d.text((50, H - 92), "АСАГАО · сканлейт", font=cyr(26), fill=(210, 202, 188))
    img.save(os.path.join(OUT, "covers", f"{slug}.png"), optimize=True)


# ---- reader pages ----------------------------------------------------------
def page_base():
    # No paper grain on pages: flat gradients + ink compress to a fraction of the
    # size, which matters because every page is embedded into the binary.
    W, H = 1080, 1620
    img = Image.new("RGB", (W, H), PAPER)
    return img, ImageDraw.Draw(img, "RGBA"), W, H


def panel(d, box, top, bot, border=6):
    inner = vgrad((int(box[2] - box[0]), int(box[3] - box[1])), top, bot)
    return inner, box


def compose_panels(img, d, panels):
    for inner, box in panels:
        img.paste(inner, (int(box[0]), int(box[1])))
    for _, box in panels:
        d.rectangle(box, outline=INK, width=6)


def page_title():
    img, d, W, H = page_base()
    d.rectangle([70, 70, W - 70, H - 70], outline=INK, width=6)
    # case chip
    d.rectangle([W/2 - 70, 150, W/2 + 70, 300], fill=INK)
    text_center(d, [W/2 - 70, 150, W/2 + 70, 300], "ДЕЛО\n1", cyr(44, bold=True), PAPER)
    text_center(d, [120, 620, W - 120, 900], "ОХОТА\nНА ЛИСИЙ ОГОНЬ",
                cyr(96, bold=True), INK, spacing=14)
    d.line([W/2 - 180, 1000, W/2 + 180, 1000], fill=VERMILION, width=6)
    text_center(d, [120, 1080, W - 120, 1220],
                "Когда фонарь у моста\nгаснет — не оборачивайся.",
                cyr(40), INK_SOFT, spacing=10)
    # small kanji seal
    d.text((W - 210, H - 220), "狐", font=cjk(150), fill=(*VERMILION, 60))
    return img


def page_grid_a():
    img, d, W, H = page_base()
    m = 60
    panels = []
    panels.append(panel(d, [m, m, W - m, 620], (58, 74, 92), (24, 34, 48)))
    panels.append(panel(d, [m, 650, (W)/2 - 15, 1180], (40, 30, 46), (18, 14, 24)))
    panels.append(panel(d, [(W)/2 + 15, 650, W - m, 1180], (120, 60, 40), (44, 22, 18)))
    panels.append(panel(d, [m, 1210, W - m, H - m], (70, 86, 78), (28, 40, 34)))
    compose_panels(img, d, panels)
    figure(d, [W*0.16, 760, W*0.16 + 240, 1150])
    bubble(d, W*0.66, 300, 420, 190, "Ты снова\nздесь…", cyr(40), tail=(0.35, 1.0))
    bubble(d, W*0.70, 940, 340, 150, "Дождь.", cyr(40, bold=True), tail=(0.6, 1.0))
    # speed lines in last panel
    for i in range(14):
        y = 1240 + i * 24
        d.line([m + 20, y, W - m - 20, y + 8], fill=(255, 255, 255, 30), width=2)
    return img


def page_wide():
    img, d, W, H = page_base()
    m = 60
    panels = [panel(d, [m, m, W - m, 760], (30, 42, 56), (14, 20, 28))]
    panels.append(panel(d, [m, 790, W - m, 1330], (150, 120, 60), (70, 52, 24)))
    panels.append(panel(d, [m, 1360, W - m, H - m], (20, 16, 26), (8, 6, 12)))
    compose_panels(img, d, panels)
    figure(d, [W*0.6, 240, W*0.6 + 220, 700], tone=(200, 200, 210))
    bubble(d, W*0.30, 300, 460, 200, "Оно погасло\nмесяц назад.", cyr(38), tail=(0.7, 1.0))
    # lantern glow
    d.ellipse([W*0.42, 980, W*0.42 + 160, 1140], fill=(*GOLD, 180))
    d.ellipse([W*0.42+40, 1020, W*0.42 + 120, 1100], fill=(255, 240, 200, 230))
    bubble(d, W*0.72, 1180, 360, 150, "…свети.", cyr(40, bold=True), tail=(0.4, 1.0))
    return img


def page_climax():
    img, d, W, H = page_base()
    m = 50
    # single dramatic panel
    inner = radial((W - 2*m, H - 2*m), (150, 60, 44), (18, 12, 20), cy=0.42)
    img.paste(inner, (m, m))
    d.rectangle([m, m, W - m, H - m], outline=INK, width=8)
    # burst lines from center
    cx, cy = W/2, H*0.44
    for a in range(0, 360, 6):
        r0, r1 = 120, 900
        x0 = cx + r0 * math.cos(math.radians(a))
        y0 = cy + r0 * math.sin(math.radians(a))
        x1 = cx + r1 * math.cos(math.radians(a))
        y1 = cy + r1 * math.sin(math.radians(a))
        d.line([x0, y0, x1, y1], fill=(255, 255, 255, 22), width=2)
    figure(d, [cx - 130, cy - 180, cx + 130, cy + 320], tone=(10, 8, 12))
    bubble(d, W*0.30, H*0.80, 520, 220, "Это был не\nфонарь.", cyr(52, bold=True), tail=(0.6, 1.0))
    return img


PAGES = [page_title, page_grid_a, page_wide, page_grid_a, page_climax, page_wide, page_grid_a, page_climax]


def main():
    os.makedirs(os.path.join(OUT, "covers"), exist_ok=True)
    os.makedirs(os.path.join(OUT, "ch1"), exist_ok=True)
    for args in COVERS:
        make_cover(*args)
        print("cover", args[0])
    for i, fn in enumerate(PAGES, 1):
        img = fn()
        img.save(os.path.join(OUT, "ch1", f"p{i:02d}.png"), optimize=True)
        print("page", i)
    print("done ->", os.path.normpath(OUT))


if __name__ == "__main__":
    main()
