import struct
import unittest
from triangle_tile_inspect import tile,error,weights


class TriangleTileTests(unittest.TestCase):
    def vertices(self,colors):
        words=struct.unpack('<2I',struct.pack('<2f',.5,.5))
        return [dict(original_vertex=[0,0,0,*words,c,[0]*4,0,0,[0]*4,[0]*4]) for c in colors]

    def test_constant_field_exact(self):
        vertices=self.vertices([[255]*4]*3)
        result=error(vertices,bytes([80,120,160,255]),1,1,8192,8)
        self.assertEqual(tile(vertices,bytes([80,120,160,255]),1,1,8192,8),bytes([80,120,160,255])*64)
        self.assertLess(result['mae'],1e-10)
        self.assertLess(result['maximum'],1e-10)

    def test_larger_tile_reduces_gradient_boundary_error(self):
        vertices=self.vertices([[80,120,160,255],[160,80,120,255],[120,160,80,255]])
        small=error(vertices,bytes([255]*4),1,1,8192,8)
        large=error(vertices,bytes([255]*4),1,1,8192,64)
        self.assertLess(large['mae'],small['mae'])

    def test_bounds_and_simplex_extension(self):
        self.assertEqual(weights(1,1),[0,.5,.5])
        with self.assertRaises(ValueError):
            tile([],b'',0,0,0,4096)


if __name__=='__main__':
    unittest.main()
