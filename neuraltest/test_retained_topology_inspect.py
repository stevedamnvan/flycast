import copy
import unittest
from retained_topology_inspect import inspect


class RetainedTopologyTests(unittest.TestCase):
    def setUp(self):
        self.a = dict(game_id='fixture', frame_id=1, coordinate_space='fixture',
                      fixed_origin=[0, 0, 0], meshes=[dict(source_draw=7,
                      indices=[0], source_bindings=[], vertices=[dict(
                      source_vertex=3, position=[0, 0, 1], normal=[0, 0, 1],
                      original_vertex=[0, 0, 0, 4, 5, [6, 6, 6, 255], [7]])])])
        self.b = copy.deepcopy(self.a)
        self.b['frame_id'] = 2

    def test_pose_is_separate_from_attributes(self):
        self.b['meshes'][0]['vertices'][0]['position'][0] = 2
        row = inspect(self.a, self.b)['meshes'][0]
        self.assertTrue(row['immutable_attributes_equal'])
        self.assertEqual(row['max_position_delta'], 2)

    def test_changed_color_or_uv_cannot_be_immutable(self):
        for index, field in ((3, 'changed_uv_vertices'), (5, 'changed_color_vertices')):
            b = copy.deepcopy(self.b)
            b['meshes'][0]['vertices'][0]['original_vertex'][index] = 99 if index == 3 else [99, 6, 6, 255]
            row = inspect(self.a, b)['meshes'][0]
            self.assertEqual(row[field], 1)
            self.assertFalse(row['immutable_attributes_equal'])

    def test_changed_slot_is_not_topology_continuation(self):
        self.b['meshes'][0]['vertices'][0]['source_vertex'] = 4
        row = inspect(self.a, self.b)['meshes'][0]
        self.assertFalse(row['topology_equal'])
        self.assertNotIn('immutable_attributes_equal', row)

    def test_offset_change_is_reported_but_not_imported_base_color(self):
        self.b['meshes'][0]['vertices'][0]['original_vertex'][6] = [8]
        row = inspect(self.a, self.b)['meshes'][0]
        self.assertEqual(row['changed_offset_color_vertices'], 1)
        self.assertEqual(row['changed_color_vertices'], 0)
        self.assertTrue(row['immutable_attributes_equal'])

    def test_invalid_base_color_rejected(self):
        self.b['meshes'][0]['vertices'][0]['original_vertex'][5] = [999]*4
        with self.assertRaises(ValueError):
            inspect(self.a, self.b)

    def test_sequence_and_nonfinite_rejected(self):
        self.b['frame_id'] = 3
        with self.assertRaises(ValueError):
            inspect(self.a, self.b)
        self.b['frame_id'] = 2
        self.b['meshes'][0]['vertices'][0]['position'][0] = float('nan')
        with self.assertRaises(ValueError):
            inspect(self.a, self.b)


if __name__ == '__main__':
    unittest.main()
