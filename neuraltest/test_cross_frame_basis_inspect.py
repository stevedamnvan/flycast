import unittest
from cross_frame_basis_inspect import compare, relative_motion, normalized_rigidity
import struct


def sample(matrix=1,point=1,step=1):
    return ((step,'site'),dict(point_words=[point,0,0,1],matrix_words=[matrix]*16))


class BasisTests(unittest.TestCase):
    def test_normalized_rigid_and_shear_control(self):
        identity=[[1,0,0,3],[0,1,0,0],[0,0,1,0],[0,0,0,1]]
        self.assertTrue(normalized_rigidity([identity],[2,3])['rigid_under_existing_tolerance'])
        identity[0][1]=.1
        self.assertFalse(normalized_rigidity([identity],[2,3])['rigid_under_existing_tolerance'])

    def test_relative_translation_and_rank(self):
        def words(values):return [struct.unpack('<I',struct.pack('<f',v))[0] for v in values]
        identity=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]
        moved=identity[:];moved[12]=4
        points=[words(p) for p in ([0,0,0,1],[1,0,0,1],[0,1,0,1],[0,0,1,1])]
        r=relative_motion([words(identity),words(moved),words(identity)],points)
        self.assertEqual(r['homogeneous_source_rank'],4)
        self.assertEqual(r['affine_xyz_relative_transforms'][0][0][3],4)
        with self.assertRaisesRegex(ValueError,'singular'):relative_motion([words([0]*16)]*3,points)

    def test_step_identity_not_used(self):
        r=compare([[sample(step=i)] for i in (1,99,500)])
        self.assertEqual(len(r['common_source_sets']),1)
        self.assertFalse(r['world_identity_proven'])

    def test_changed_point_and_reused_matrix_ambiguity(self):
        self.assertEqual(compare([[sample()],[sample(point=2)],[sample()]])['common_source_sets'],[])
        r=compare([[sample(),sample(matrix=2,step=2)]]*3)
        self.assertFalse(r['common_source_sets'][0]['unambiguous_matrix_per_frame'])

    def test_changing_matrix_and_duplicate_count(self):
        r=compare([[sample(matrix=i)] for i in (1,2,3)])
        self.assertFalse(r['common_source_sets'][0]['matrix_words_unchanged'])
        self.assertEqual(compare([[sample()],[sample(),sample(step=2)],[sample()]])['common_source_sets'],[])
