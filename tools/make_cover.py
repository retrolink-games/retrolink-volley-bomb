"""Box-art covers from the game's own sprites: 240x320 (cover.png) and 240x240 (cover-square.png), scaled 4x."""
from PIL import Image, ImageDraw, ImageFont
import os
G = os.path.join(os.path.dirname(__file__), "..", "graphics")
TTF = os.path.join(os.path.dirname(__file__), "fonts", "Silkscreen-Regular.ttf")
def spr(name, frame, h):
    im = Image.open(f"{G}/{name}.bmp").convert("RGBA")
    key = im.getpixel((0, 0))
    im = im.crop((0, frame*h, im.width, frame*h+h))
    im.putdata([(0,0,0,0) if p[:3]==key[:3] else p for p in im.get_flattened_data()])
    return im
def big(im, k): return im.resize((im.width*k, im.height*k), Image.NEAREST)
LAYOUTS = {
    "cover": dict(H=320, band=(112, 200), brand=34, volley=64, bomb=96, sub=134,
        blobs=[(34, 238, 0, 0), (36, 150, 1, 3), (142, 214, 0, 3), (186, 15, 1, 0)],
        booms=[(96, 176, 1, 2), (8, 96, 2, 1), (196, 196, 0, 1), (108, 250, 0, 1), (150, 150, 1, 1)]),
    "cover-square": dict(H=240, band=(96, 150), brand=24, volley=44, bomb=74, sub=110,
        blobs=[(34, 158, 0, 0), (30, 124, 1, 3), (150, 128, 0, 3), (186, 15, 1, 0)],
        booms=[(88, 124, 1, 2), (6, 70, 2, 1), (200, 96, 0, 1), (112, 196, 0, 1)]),
}
def render(name, L):
    W, H = 240, L["H"]
    bg = Image.open(f"{G}/bg_p1.bmp").convert("RGBA").crop((0, 0, 240, 160))
    top = bg.transpose(Image.ROTATE_180)
    c = Image.new("RGBA", (W, H))
    c.paste(top.crop((0, 0, 240, H // 2)), (0, 0)); c.paste(bg.crop((0, 160 - H // 2, 240, 160)), (0, H // 2))
    sky = top.getpixel((120, 150))
    d = ImageDraw.Draw(c)
    d.rectangle((0, L["band"][0], W, L["band"][1]), fill=sky)
    f_big = ImageFont.truetype(TTF, 32); f_sm = ImageFont.truetype(TTF, 8)
    CREAM, DARK, ORANGE, INK = (244, 236, 214), (18, 18, 22), (255, 138, 92), (238, 246, 245)
    def text(y, s, f, col, shadow=DARK):
        w = d.textlength(s, font=f); x = (W - w) // 2
        d.text((x+2, y+2), s, font=f, fill=shadow); d.text((x, y), s, font=f, fill=col)
    # RetroLink brand: pixel mark + RETRO (sand) LINK (mint), as on retrolink.games
    import re
    BOLD = os.path.join(os.path.dirname(__file__), "fonts", "Silkscreen-Bold.ttf")
    svg = open(os.path.join(os.path.dirname(__file__), "retrolink-mark.svg")).read()
    mark = Image.new("RGBA", (30, 16))
    md = ImageDraw.Draw(mark)
    for x, y, w, h, col in re.findall(r'x="(\d+)" y="(\d+)" width="(\d+)" height="(\d+)" fill="#(\w+)"', svg):
        x, y, w, h = (int(v)//24 for v in (x, y, w, h))
        md.rectangle((x, y, x+w-1, y+h-1), fill=tuple(int(col[i:i+2], 16) for i in (0, 2, 4)))
    mark = big(mark, 1)
    f_brand = ImageFont.truetype(BOLD, 16)
    SAND, MINT = (242, 207, 138), (143, 214, 200)
    wr = d.textlength("RETRO", font=f_brand); wl = d.textlength("LINK", font=f_brand)
    x0 = 16
    tx = (W - wr - wl) // 2
    for s_, col_, dx in (("RETRO", SAND, 0), ("LINK", MINT, wr)):
        d.text((tx+dx+1, L["brand"]+1), s_, font=f_brand, fill=DARK); d.text((tx+dx, L["brand"]), s_, font=f_brand, fill=col_)
    text(L["volley"], "VOLLEY", f_big, INK); text(L["bomb"], "BOMB", f_big, ORANGE)
    # four blobs: bottom row upright, top row hanging (like the court layout)
    for i, (x, y, flipv, fr) in enumerate(L["blobs"]):
        b = big(spr(f"blob_p{i+1}", fr, 32), 2)
        if i == 2 or i == 3: b = b.transpose(Image.FLIP_LEFT_RIGHT)
        if flipv: b = b.transpose(Image.FLIP_TOP_BOTTOM)
        c.alpha_composite(b, (x, y))
    for (x, y, fr, k) in L["booms"]:
        c.alpha_composite(big(spr("boom", fr, 32), k), (x, y))
    text(L["sub"], "1-4 PLAYERS  LINK CABLE", f_sm, INK)
    out = c.resize((W*4, H*4), Image.NEAREST)
    p = os.path.join(os.path.dirname(__file__), "..", "docs", "art", name + ".png")
    out.save(p); print(p)
for n, L in LAYOUTS.items(): render(n, L)
