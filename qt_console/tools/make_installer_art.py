"""Create correctly proportioned Inno Setup wizard artwork from Tunuino branding."""
from pathlib import Path
from PIL import Image, ImageOps

root = Path(__file__).resolve().parents[2]
source = root / "qt_console" / "assets" / "tunuino.png"
assets = root / "installer" / "assets"
assets.mkdir(parents=True, exist_ok=True)

with Image.open(source) as src:
    logo = src.convert("RGBA")
    bbox = logo.getchannel("A").getbbox()
    if bbox:
        logo = logo.crop(bbox)

def composite_contained(canvas, bounds):
    x, y, width, height = bounds
    fitted = ImageOps.contain(logo, (width, height), Image.Resampling.LANCZOS)
    canvas.paste(fitted, (x + (width - fitted.width) // 2,
                          y + (height - fitted.height) // 2), fitted)

# Welcome/finish image: a large centered logo, no clipping.
large = Image.new("RGB", (328, 604), "#171c24")
composite_contained(large, (16, 100, 296, 400))
large.save(assets / "wizard-large.bmp", format="BMP")

# Header on all other pages: compact, fully contained logo on a white header.
# A header image is not a banner: oversized artwork gets clipped by Inno Setup.
small = Image.new("RGB", (180, 60), "#ffffff")
composite_contained(small, (8, 5, 164, 50))
small.save(assets / "wizard-small.bmp", format="BMP")
print("Generated correctly sized wizard artwork")
