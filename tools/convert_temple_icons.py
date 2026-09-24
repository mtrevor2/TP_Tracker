"""Convert supplied temple PNGs to the map's native GX RGBA8 texture format."""
from pathlib import Path
import argparse
import struct
from PIL import Image

parser = argparse.ArgumentParser()
parser.add_argument('source', type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
for name in ('Temple', 'TempleAvailable'):
    image = Image.open(args.source / (name + '.png')).convert('RGBA')
    image.thumbnail((64, 64), Image.Resampling.LANCZOS)
    canvas = Image.new('RGBA', (64, 64))
    canvas.paste(image, ((64-image.width)//2, (64-image.height)//2))
    header = bytearray(32)
    header[0:2] = bytes((6, 2))  # RGBA8, alpha enabled
    struct.pack_into('>HH', header, 2, 64, 64)
    header[20:22] = bytes((1, 1))  # linear min/mag filter
    header[24] = 1  # one image, no mipmaps
    struct.pack_into('>I', header, 28, 32)
    pixels = bytearray()
    for y in range(0, 64, 4):
        for x in range(0, 64, 4):
            block = [canvas.getpixel((x+dx, y+dy)) for dy in range(4) for dx in range(4)]
            pixels.extend(v for r,g,b,a in block for v in (a,r))
            pixels.extend(v for r,g,b,a in block for v in (g,b))
    (root / 'res/icons' / (name + '.bti')).write_bytes(header + pixels)
