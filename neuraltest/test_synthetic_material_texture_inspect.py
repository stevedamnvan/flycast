import struct
import unittest
from synthetic_material_texture import chart


class SyntheticTextureTests(unittest.TestCase):
    def test_chart_header_and_pixels(self):
        data = chart()
        self.assertEqual(len(data), 148+64*64*4)
        self.assertEqual(data[:4], b'DDS ')
        self.assertEqual(struct.unpack_from('<I', data, 128)[0], 28)
        self.assertEqual(data[148:152], bytes([20, 220, 20, 255]))
        self.assertEqual(data[180:184], bytes([240, 20, 20, 255]))
        self.assertEqual(data, chart())

    def test_gradient_affine_field(self):
        data = chart(True)
        self.assertEqual(len(data), 148+64*64*4)
        for x,y in ((0,0),(32,32),(63,63)):
            u,v=(x+.5)/64,(y+.5)/64
            expected=[round(80+80*u),round(180-40*u-60*v),round(100-40*u+60*v),255]
            start=148+(y*64+x)*4
            self.assertEqual(list(data[start:start+4]),expected)


if __name__ == '__main__':
    unittest.main()
