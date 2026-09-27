"""Draw three original anime portrait fixtures; requires Pillow."""
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).parent / "fixtures"
ROOT.mkdir(parents=True, exist_ok=True)
PALETTES = [
    ("blue-maid", (235, 242, 252), (65, 157, 224), (33, 87, 158), (243, 210, 196), (52, 123, 210)),
    ("pink-scout", (255, 241, 238), (245, 128, 175), (166, 64, 130), (255, 218, 198), (139, 67, 156)),
    ("dark-mage", (238, 233, 249), (54, 47, 90), (21, 20, 48), (229, 184, 164), (150, 94, 210)),
]
for name, bg, hair, shadow, skin, iris in PALETTES:
    im = Image.new("RGB", (420, 520), bg)
    d = ImageDraw.Draw(im)
    d.ellipse((30, 30, 390, 390), fill=tuple(min(255, c + 8) for c in bg))
    d.polygon([(105, 520), (120, 395), (180, 350), (250, 350), (305, 395), (330, 520)], fill=shadow)
    d.polygon([(155, 520), (175, 395), (210, 420), (245, 395), (265, 520)], fill=(248, 248, 252))
    d.ellipse((83, 82, 337, 389), fill=hair)
    d.ellipse((112, 138, 310, 375), fill=skin)
    d.ellipse((90, 233, 124, 269), fill=skin)
    d.ellipse((301, 233, 335, 269), fill=skin)
    d.polygon([(99, 217), (85, 120), (160, 70), (221, 100), (175, 258), (160, 204)], fill=shadow)
    d.polygon([(155, 134), (215, 68), (329, 128), (313, 236), (280, 211), (240, 171), (206, 225)], fill=hair)
    for cx in (165, 258):
        d.ellipse((cx-25, 238, cx+25, 294), fill=(255, 255, 255))
        d.ellipse((cx-13, 245, cx+13, 289), fill=iris)
        d.ellipse((cx-7, 246, cx+7, 277), fill=shadow)
        d.ellipse((cx-7, 250, cx-1, 257), fill=(255, 255, 255))
        d.arc((cx-27, 237, cx+27, 271), 180, 350, fill=shadow, width=5)
    d.arc((195, 300, 227, 326), 0, 170, fill=(155, 80, 95), width=3)
    d.ellipse((127, 295, 158, 307), fill=(248, 164, 166))
    d.ellipse((264, 295, 295, 307), fill=(248, 164, 166))
    if name == "blue-maid":
        d.arc((101, 61, 321, 170), 188, 350, fill=(255, 255, 255), width=20)
        for x in range(129, 298, 22):
            d.ellipse((x-11, 73, x+11, 104), fill=(255, 255, 255))
        d.polygon([(177, 411), (210, 444), (243, 411), (210, 490)], fill=(244, 126, 175))
    elif name == "pink-scout":
        d.polygon([(259, 84), (312, 53), (284, 117), (321, 148), (267, 139)], fill=(255, 233, 91))
        d.polygon([(175, 414), (210, 442), (244, 414), (210, 464)], fill=(255, 230, 100))
    else:
        d.polygon([(55, 160), (110, 72), (318, 72), (368, 160)], fill=shadow)
        d.ellipse((181, 76, 240, 130), fill=(239, 198, 97))
    im.save(ROOT / f"{name}.png")
