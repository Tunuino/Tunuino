"""Generate a readable Windows desktop icon from the official Tunuino artwork."""
from pathlib import Path
from PIL import Image, ImageOps

root = Path(__file__).resolve().parents[1]
source = root / "assets" / "tunuino.png"
target = root / "assets" / "tunuino.ico"

with Image.open(source) as opened:
    logo = opened.convert("RGBA")
    alpha_bounds = logo.getchannel("A").getbbox()
    if alpha_bounds:
        logo = logo.crop(alpha_bounds)

# A wide logo is illegible when scaled into a transparent square.
# Use a solid square tile, then fit the complete mark to nearly its full width.
# This changes only the Windows icon; installer wizard artwork stays unchanged.
canvas = Image.new("RGBA", (512, 512), "#121820")
mark = ImageOps.contain(logo, (496, 400), Image.Resampling.LANCZOS)
canvas.alpha_composite(mark, ((512 - mark.width) // 2, (512 - mark.height) // 2))
canvas.save(target, format="ICO", sizes=[
    (16, 16), (24, 24), (32, 32), (48, 48),
    (64, 64), (128, 128), (256, 256)
])
print(f"Generated high-visibility Windows icon: {target}")
