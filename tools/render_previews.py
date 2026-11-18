"""Convert captures from the real LVGL host test to PNG, without recreating UI."""
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "docs/previews"
OUTPUT.mkdir(parents=True, exist_ok=True)
paths = sorted((ROOT / "tests/build/previews").glob("*.ppm"))
for path in paths:
    image = Image.open(path)
    image.save(OUTPUT / (path.stem + ".png"))
print(f"Converted {len(paths)} LVGL captures to {OUTPUT}")
