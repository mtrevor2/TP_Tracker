from dependency_paths import DUSKLIGHT, RANDOMIZER
"""Convert the mapped, locally installed Henriko UI textures for the tracker sidebar."""
import argparse
import json
import struct
from pathlib import Path
from PIL import Image

parser = argparse.ArgumentParser()
parser.add_argument('texture_pack', type=Path)
parser.add_argument('--disc', type=Path, help='GameCube disc for icons without a texture-pack replacement')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
mapping = json.loads((root/'res/inventory.json').read_text())
sources = {p.name: p for p in args.texture_pack.rglob('*.dds')}
disc_icons = {}
if args.disc:
    from export_positions import archive, disc_files
    with args.disc.open('rb') as disc:
        offset, size = disc_files(disc)['/res/Layout/itemicon.arc']
        disc.seek(offset)
        disc_icons = archive(disc.read(size))

def decode_icon(data):
    """Decode the native C8/RGB5A3 inventory sprites (8x4 tiled pixels)."""
    assert data[0] == 9 and data[9] == 2, 'Expected C8/RGB5A3 item icon'
    width, height = struct.unpack_from('>HH', data, 2)
    palette_offset = struct.unpack_from('>I', data, 12)[0]
    offset = struct.unpack_from('>I', data, 28)[0]
    palette = []
    for index in range(struct.unpack_from('>H', data, 10)[0]):
        value = struct.unpack_from('>H', data, palette_offset + index * 2)[0]
        if value & 0x8000:
            palette.append(tuple(((value >> shift) & 31) * 255 // 31 for shift in (10,5,0)) + (255,))
        else:
            palette.append(tuple(((value >> shift) & 15) * 17 for shift in (8,4,0)) + (((value >> 12) & 7) * 255 // 7,))
    image = Image.new('RGBA', (width, height))
    for by in range(0, height, 4):
        for bx in range(0, width, 8):
            for y in range(4):
                for x in range(8):
                    index = data[offset]
                    offset += 1
                    if bx+x < width and by+y < height:
                        image.putpixel((bx+x, by+y), palette[index])
    return image

for entry in mapping.values():
    if entry.get('provider') == 'game':
        icon = decode_icon(disc_icons[entry['file']])
    elif entry.get('provider') == 'randomizer':
        icon = decode_icon((RANDOMIZER/'res'/entry['file']).read_bytes())
    else:
        source = sources[entry['file']]
        icon = Image.open(source).convert('RGBA')
    icon.thumbnail((96,96), Image.Resampling.LANCZOS)
    canvas = Image.new('RGBA',(96,96))
    canvas.alpha_composite(icon,((96-icon.width)//2,(96-icon.height)//2))
    output = root/'res'/entry['icon']
    output.parent.mkdir(parents=True,exist_ok=True)
    canvas.save(output)
print(f'Converted artwork for {len(mapping)} inventory entries.')
