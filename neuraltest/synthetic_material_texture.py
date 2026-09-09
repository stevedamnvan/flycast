"""Create-only procedural DDS chart for retained material runtime experiments."""
import argparse
from pathlib import Path
from source_dds import encode


def chart(gradient=False):
    pixels = bytearray()
    for y in range(64):
        for x in range(64):
            if gradient:
                # Barycentric field for UV vertices (0,1),(1,1),(.5,0).
                # Extend the affine field outside the triangle for filtering.
                u, v = (x+.5)/64, (y+.5)/64
                c = 1-v
                b = u-.5*c
                a = 1-b-c
                colors = ((80, 120, 160), (160, 80, 120), (120, 160, 80))
                pixels.extend([round(a*colors[0][i]+b*colors[1][i]+c*colors[2][i]) for i in range(3)]+[255])
            else:
                pixels.extend((240, 20, 20, 255) if (x//8+y//8)%2 else (20, 220, 20, 255))
    return encode([dict(width=64, height=64, rgba=bytes(pixels))], srgb=False)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--gradient', action='store_true')
    args = parser.parse_args()
    if not args.output.is_absolute():
        parser.error('absolute create-only output required')
    with args.output.open('xb') as stream:
        stream.write(chart(args.gradient))
