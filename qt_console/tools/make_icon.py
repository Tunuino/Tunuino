"""Generate a true square Windows icon using the gauge symbol of Tunuino.

The horizontal wordmark is retained in the installer and app branding.
"""
from pathlib import Path
from PIL import Image, ImageOps, ImageEnhance

root = Path(__file__).resolve().parents[1]
source = root / "assets" / "tunuino.png"
target = root / "assets" / "tunuino.ico"

with Image.open(source) as image:
    logo = image.convert("RGB")

# The source is a 3:1 horizontal logo. Its speedometer emblem occupies the
# upper central portion. Isolate that emblem instead of shrinking the entire
# wordmark into a Windows desktop icon.
w, h = logo.size
gauge = logo.crop((int(w * .33), int(h * .015),
                   int(w * .67), int(h * .57)))
# Center the gauge on a full square dark tile so Windows displays a large icon.
canvas = Image.new("RGB", (512, 512), "#101720")
gauge = ImageOps.contain(gauge, (480, 440), Image.Resampling.LANCZOS)
gauge = ImageEnhance.Contrast(gauge).enhance(1.12)
canvas.paste(gauge, ((512 - gauge.width) // 2, (512 - gauge.height) // 2))
canvas.save(target, format="ICO", sizes=[
    (16, 16), (24, 24), (32, 32), (48, 48),
    (64, 64), (128, 128), (256, 256)
])
print(f"Generated square gauge icon: {target}")
