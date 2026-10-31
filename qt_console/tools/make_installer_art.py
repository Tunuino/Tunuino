"""Build visible, branded Inno Setup wizard artwork from the official Tunuino PNG."""
from pathlib import Path
from PIL import Image, ImageOps

root = Path(__file__).resolve().parents[2]
source = root / "qt_console" / "assets" / "tunuino.png"
assets = root / "installer" / "assets"
assets.mkdir(parents=True, exist_ok=True)

with Image.open(source) as opened:
    logo = opened.convert("RGBA")
    # Remove only fully transparent outer margins; keep the logo proportions.
    bounds = logo.getchannel("A").getbbox()
    if bounds:
        logo = logo.crop(bounds)

def place(canvas, target_width, center_y):
    resized = ImageOps.contain(logo, (target_width, canvas.height), Image.Resampling.LANCZOS)
    if resized.height > canvas.height:
        raise ValueError("Logo does not fit")
    canvas.paste(resized, ((canvas.width - resized.width) // 2,
                           center_y - resized.height // 2), resized)

# Inno Setup scales this tall artwork into the left welcome/finish sidebar.
large = Image.new("RGB", (328, 604), "#161b22")
place(large, 302, 230)
large.save(assets / "wizard-large.bmp", format="BMP")

# Full-width top header image on intermediate wizard pages.
small = Image.new("RGB", (600, 110), "#161b22")
place(small, 420, 55)
small.save(assets / "wizard-small.bmp", format="BMP")
print("Generated branded installer wizard images")
