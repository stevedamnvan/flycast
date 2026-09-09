import struct
import unittest
from source_attribute_sample import sample, triangle_sample


class AttributeSamplingTests(unittest.TestCase):
    pixels = bytes([255,0,0,255, 0,255,0,255])

    def test_centers_and_linear_midpoint(self):
        self.assertEqual(sample(self.pixels,2,1,.25,.5,8192),(255,0,0,255))
        self.assertEqual(sample(self.pixels,2,1,.5,.5,8192),(127.5,127.5,0,255))

    def test_wrap_mirror_clamp(self):
        self.assertEqual(sample(self.pixels,2,1,1.25,.5,0),(255,0,0,255))
        for flags in (1<<16,1<<18,(1<<16)|(1<<18)):
            self.assertEqual(sample(self.pixels,2,1,1.25,.5,flags),(0,255,0,255))
        self.assertEqual(sample(self.pixels,2,1,-.25,.5,0),(0,255,0,255))

    def test_wrap_linear_seam(self):
        self.assertEqual(sample(self.pixels,2,1,0,.5,8192),(127.5,127.5,0,255))
        self.assertEqual(sample(self.pixels,2,1,0,.5,8192|(1<<16)),(255,0,0,255))

    def test_source_bgra_and_barycentric_variation(self):
        vertices=[]
        for color in ([0,0,255,255],[0,255,0,255],[255,0,0,255]):
            words=list(struct.unpack('<2I',struct.pack('<2f',.5,.5)))
            vertices.append(dict(original_vertex=[0,0,0,*words,color,[0]*4,0,0,[0]*4,[0]*4]))
        self.assertEqual(triangle_sample(vertices,[.5,.25,.25],bytes([255]*4),1,1,8192),
                         (127.5,63.75,63.75,255))
        with self.assertRaises(ValueError):
            triangle_sample(vertices,[1,1,1],bytes([255]*4),1,1,8192)

    def test_invalid_and_unsupported(self):
        for u,tsp in ((float('nan'),0),(.5,2<<13)):
            with self.assertRaises(ValueError):
                sample(self.pixels,2,1,u,.5,tsp)


if __name__ == '__main__':
    unittest.main()
