"""Generate a multi-resolution Windows icon from the existing Tunuino logo."""
from pathlib import Path
from PIL import Image

root = Path(__file__).resolve().parents[1]
source = root / "assets" / "tunuino.png"
target = root / "assets" / "tunuino.ico"

with Image.open(source) as image:
    image = image.convert("RGBA")
    # Preserve the full logo, including its original aspect ratio.
    side = max(image.size)
    square = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    square.alpha_composite(image, ((side - image.width) // 2, (side - image.height) // 2))
    square.save(target, format="ICO", sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
print(f"Generated {target}")
