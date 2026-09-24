"""Side-by-side sheet: native frame plus offline renders, with brightness numbers.

Example:
  python neuraltest/remake_compare_sheet.py C:/Flycast-Evidence/<run>/captures/frame-6364-present-6368 ^
      C:/Flycast-Evidence/lighting-fix-b/sheet.png C:/Flycast-Evidence/lighting-fix-b/control C:/Flycast-Evidence/lighting-fix-b/sun

Each tile label shows geoL (mean luminance of 3D geometry pixels, from the
capture's guidance-draw-id.bin) and allL (whole frame). Native geoL is the
target brightness; within about 10 percent of native is a reasonable level.
Numbers describe brightness only; a human still judges how it looks.
"""
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw


def main():
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    frame, out, renders = Path(sys.argv[1]), Path(sys.argv[2]), [Path(p) for p in sys.argv[3:]]
    tiles = [('native', frame/'original-native.png')] + [(p.name, p/'render.png') for p in renders]
    images = [(n, Image.open(p).convert('RGB')) for n, p in tiles]
    w, h = images[0][1].size
    geo = None
    ids = frame/'guidance-draw-id.bin'
    if ids.exists():
        geo = np.frombuffer(ids.read_bytes(), dtype='<u2').reshape(h, w) != 0
    cols = min(3, len(images))
    rows = (len(images)+cols-1)//cols
    sheet = Image.new('RGB', (w*cols, (h+18)*rows))
    draw = ImageDraw.Draw(sheet)
    stats = {}
    for i, (name, im) in enumerate(images):
        if im.size != (w, h):
            im = im.resize((w, h))
        x, y = (i % cols)*w, (i//cols)*(h+18)
        sheet.paste(im, (x, y+18))
        lum = np.asarray(im).astype(np.float32) @ np.array([0.2126, 0.7152, 0.0722], np.float32)
        stats[name] = {'allL': round(float(lum.mean()), 1),
                       'geoL': round(float(lum[geo].mean()), 1) if geo is not None else None}
        draw.text((x+4, y+3), f"{name}  geoL={stats[name]['geoL']} allL={stats[name]['allL']}", fill='white')
    sheet.save(out)
    out.with_suffix('.json').write_text(json.dumps(stats, indent=1))
    print(json.dumps(stats, indent=1))


if __name__ == '__main__':
    main()
