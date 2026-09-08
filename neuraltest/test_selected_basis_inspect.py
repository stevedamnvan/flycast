import unittest
import struct
from selected_basis_inspect import compare, anchored_camera, anchor_mesh


def fixture():
    def words(values):return [struct.unpack('<I',struct.pack('<f',v))[0] for v in values]
    frames=[]
    for i in range(3):
        frames.append([dict(draw=1,vertex=4,ordinal=1781+i,point_words=words([0,0,1,1]),
            matrix_words=words([1,0,0,0,0,1,0,0,0,0,1,0,i,0,0,1]))])
    return frames


class SelectedBasisTests(unittest.TestCase):
    def test_mesh_y_conversion_preserves_source(self):
        camera=anchored_camera(fixture()[1],[1,1])
        mesh=dict(schema='flycast-expression-evidence-mesh-v1',vertices=[dict(position=[2,3,4])],strict_reprojection_pass=False)
        result=anchor_mesh(mesh,camera)
        self.assertEqual(result['vertices'][0]['position'],[1,-3,4])
        self.assertFalse(result['strict_reprojection_pass'])
        self.assertFalse(result['renderable_by_remix_adapter'])

    def test_anchor_pose_and_wrong_calibration(self):
        frames=fixture();r=anchored_camera(frames[1],[1,1])
        self.assertEqual(r['camera_origin_in_source'],[-1,0,0])
        with self.assertRaisesRegex(ValueError,'not rigid'):anchored_camera(frames[1],[2,1])
        with self.assertRaisesRegex(ValueError,'inconsistent'):anchored_camera(frames[0]+frames[1],[1,1])

    def test_wrong_frame_moves_point(self):
        r=compare(fixture())
        self.assertEqual(r['source_word_changes'],[])
        self.assertEqual(r['wrong_first_frame_matrix_maximum_pixel_error'],2)
        self.assertEqual(r['wrong_first_frame_matrix_errors_over_threshold'],2)

    def test_frame_and_source_changes(self):
        frames=fixture();frames[1][0]['ordinal']=1781
        with self.assertRaisesRegex(ValueError,'frame identity'):compare(frames)
        frames=fixture();frames[1][0]['point_words'][0]=0x3f800000
        self.assertEqual(compare(frames)['source_word_changes'],[(1,4)])
