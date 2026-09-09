import unittest
from remake_subset_inspect import subset


class SubsetTests(unittest.TestCase):
    def test_preserves_source_and_records_omission(self):
        source = dict(schema='flycast-prepared-remake-scene-v1',
                      meshes=[dict(source_draw=1), dict(source_draw=2)],
                      omissions=['unknown camera'], source_assets={'a': 'unchanged'})
        result = subset(source, [2])
        self.assertEqual(len(source['meshes']), 2)
        self.assertEqual(result['meshes'], [dict(source_draw=1)])
        self.assertEqual(result['source_assets'], source['source_assets'])
        self.assertEqual(result['diagnostic_excluded_draws'], [2])
        self.assertFalse(result['renderable_by_remix_adapter'])
        for excluded in ([], [3], [1, 2]):
            with self.assertRaises(ValueError):
                subset(source, excluded)


if __name__ == '__main__':
    unittest.main()
