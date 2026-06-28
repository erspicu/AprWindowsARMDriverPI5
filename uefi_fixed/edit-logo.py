#!/usr/bin/env python3
# 在 8-bit 索引 BMP 底部加 "edited by erspicu_brox"，保留原 header/palette/尺寸，
# 只改像素值 -> 檔案大小與結構與原檔一致（EDK2 LogoDxe 直接相容、不動 FV 預算）。
import sys
from PIL import Image, ImageDraw, ImageFont

src, dst = sys.argv[1], sys.argv[2]
TEXT = sys.argv[3] if len(sys.argv) > 3 else "AprPI5WinDriver"

orig = bytearray(open(src, "rb").read())
img = Image.open(src)
assert img.mode == "P", f"expected 8-bit P mode, got {img.mode}"
W, H = img.size

# palette 內最接近黑 / 白的索引（文字用黑、描邊用白 -> 任何背景都可讀）
pal = img.getpalette()
def closest(t):
    best, bd = 0, 1 << 30
    for i in range(len(pal) // 3):
        r, g, b = pal[3*i:3*i+3]
        d = (r-t[0])**2 + (g-t[1])**2 + (b-t[2])**2
        if d < bd: bd, best = d, i
    return best
black, white = closest((0, 0, 0)), closest((255, 255, 255))

draw = ImageDraw.Draw(img)
font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 24)
bb = draw.textbbox((0, 0), TEXT, font=font, stroke_width=2)
tw, th = bb[2]-bb[0], bb[3]-bb[1]
x, y = (W - tw)//2 - bb[0], H - th - 22 - bb[1]
draw.text((x, y), TEXT, font=font, fill=black, stroke_width=2, stroke_fill=white)

# 把修改後的像素寫回原檔位元組（保留 header 0..bitsoff-1 與 palette）
bits_off = int.from_bytes(orig[10:14], "little")
row = ((W * 8 + 31)//32) * 4   # 4-byte 對齊的列寬
mod = img.tobytes()            # P 模式: 上到下、無 padding、每像素 1 byte
for ty in range(H):
    frow = H - 1 - ty          # BMP 由下而上
    base = bits_off + frow*row
    mrow = ty*W
    orig[base:base+W] = mod[mrow:mrow+W]

open(dst, "wb").write(orig)
print(f"wrote {dst}: {len(orig)} bytes (orig {len(open(src,'rb').read())}), text idx black={black} white={white}")
