"""Generate src/resources/app.ico from the built-in character drawing.

The icon mirrors the placeholder character drawn by reminder_window.cpp:
an amber rounded square with ink eyes, a smile and soft cheeks.

Usage: python tools/make_icon.py
"""

import struct
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "src" / "resources" / "app.ico"
PREVIEW = ROOT / "build" / "app-icon-preview.png"

SIZES = [16, 20, 24, 32, 40, 48, 64, 128, 256]
SUPERSAMPLE = 8

FILL = (255, 183, 77, 255)
OUTLINE = (230, 126, 34, 255)
INK = (93, 64, 55, 255)
CHEEK = (255, 138, 128, 110)


def render(size: int) -> Image.Image:
    s = size * SUPERSAMPLE
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    inset = s * 0.07
    face = (inset, inset, s - inset, s - inset)
    radius = (s - 2 * inset) * 0.22
    line = max(1.0, (s - 2 * inset) * 0.012)

    d.rounded_rectangle(face, radius=radius, fill=FILL, outline=OUTLINE, width=max(1, round(line)))

    fx, fy = inset, inset
    fw = s - 2 * inset

    eye_r = fw * 0.055
    eye_y = fy + fw * 0.38
    for cx in (fx + fw * 0.30, fx + fw * 0.70):
        d.ellipse((cx - eye_r, eye_y - eye_r, cx + eye_r, eye_y + eye_r), fill=INK)

    smile_w = max(1.5, fw * 0.032)
    smile_box = (fx + fw * 0.28, fy + fw * 0.46, fx + fw * 0.72, fy + fw * 0.76)
    d.arc(smile_box, start=25, end=155, fill=INK, width=max(1, round(smile_w)))

    # Round line caps for the smile, matching LineCapRound in GDI+.
    import math

    for angle in (25, 155):
        a = math.radians(angle)
        cx = (smile_box[0] + smile_box[2]) / 2 + (smile_box[2] - smile_box[0]) / 2 * math.cos(a)
        cy = (smile_box[1] + smile_box[3]) / 2 + (smile_box[3] - smile_box[1]) / 2 * math.sin(a)
        r = smile_w / 2
        d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=INK)

    cheek_r = fw * 0.055
    cheek_y = fy + fw * 0.58
    for cx in (fx + fw * 0.21, fx + fw * 0.79):
        d.ellipse((cx - cheek_r, cheek_y - cheek_r, cx + cheek_r, cheek_y + cheek_r), fill=CHEEK)

    return img.resize((size, size), Image.LANCZOS)


def write_ico(path: Path, images: list[Image.Image]) -> None:
    blobs = []
    for img in images:
        from io import BytesIO

        buf = BytesIO()
        img.save(buf, format="PNG", optimize=True)
        blobs.append(buf.getvalue())

    header = struct.pack("<HHH", 0, 1, len(images))
    offset = 6 + 16 * len(images)
    entries = b""
    for img, blob in zip(images, blobs):
        w = 0 if img.width >= 256 else img.width
        h = 0 if img.height >= 256 else img.height
        entries += struct.pack("<BBBBHHII", w, h, 0, 0, 1, 32, len(blob), offset)
        offset += len(blob)

    path.write_bytes(header + entries + b"".join(blobs))


def main() -> None:
    images = [render(size) for size in SIZES]
    OUT.parent.mkdir(parents=True, exist_ok=True)
    write_ico(OUT, images)
    PREVIEW.parent.mkdir(parents=True, exist_ok=True)
    images[-1].resize((512, 512), Image.NEAREST).save(PREVIEW)
    print(f"Wrote {OUT} ({OUT.stat().st_size} bytes, {len(images)} sizes)")
    print(f"Preview {PREVIEW}")


if __name__ == "__main__":
    main()
