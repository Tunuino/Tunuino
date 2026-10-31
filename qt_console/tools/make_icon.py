"""Create a square, high-visibility Windows shortcut icon from official Tunuino art.

The full horizontal wordmark remains unchanged in the installer and application.
Desktop icons require a compact square mark instead of a scaled-down banner.
"""
from pathlib import Path
from PIL import Image, ImageOps

root = Path(__file__).resolve().parents[1]
source = root / "assets" / "tunuino.png"
target = root / "assets" / "tunuino.ico"

with Image.open(source) as opened:
    original = opened.convert("RGBA")

# Remove opaque/transparent uniform borders by measuring against the corner.
# The logo has a dark rectangular backdrop, so alpha-only trimming is insufficient.
rgb = original.convert("RGB")
background = rgb.getpixel((0, 0))
pixels = rgb.load()
threshold = 32
xs, ys = [], []
for y in range(rgb.height):
    for x in range(rgb.width):
        p = pixels[x, y]
        if max(abs(p[k] - background[k]) for k in range(3)) > threshold:
            xs.append(x)
            ys.append(y)
if xs:
    original = original.crop((min(xs), min(ys), max(xs) + 1, max(ys) + 1))

# The full horizontal wordmark is unsuitable for an app icon. Crop the central
# square to feature the gauge/lettermark at readable desktop sizes.
width, height = original.size
side = min(width, height)
left = (width - side) // 2
top = 0
symbol = original.crop((left, top, left + side, top + side))
symbol = ImageOps.contain(symbol, (464, 464), Image.Resampling.LANCZOS)
canvas = Image.new("RGBA", (512, 512), "#121820")
canvas.alpha_composite(symbol, ((512 - symbol.width) // 2,
                                (512 - symbol.height) // 2))
canvas.save(target, format="ICO", sizes=[
    (16, 16), (24, 24), (32, 32), (48, 48),
    (64, 64), (128, 128), (256, 256)
])
print(f"Generated compact square desktop icon: {target}")
