# SPDX-License-Identifier: GPL-2.0-or-later
import unittest
from remake_anchor_stability import compare, summarize, view_coordinates


class AnchorAnalysisTest(unittest.TestCase):
    def test_static_translation_ambiguity(self):
        a = {'x': [(1, [(0, 0, 0), (1, 0, 0)])]}
        self.assertEqual(compare(a, a)[0][0]['maximum'], 0)
        b = {'x': [(2, [(3, 0, 0), (4, 0, 0)])]}
        self.assertEqual(compare(a, b)[0][0]['rms'], 3)
        self.assertEqual(compare(a, {'x': a['x']*2}), ([], 1))

    def test_camera_ignored_negative(self):
        a = {'x': [(1, [(0, 0, 5)])]}
        camera = [45, 1, .1, 100, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0]
        moved = camera.copy()
        moved[4] = 2
        self.assertEqual(compare(view_coordinates(camera, a), view_coordinates(moved, a))[0][0]['maximum'], 2)
        self.assertEqual(compare(a, a)[0][0]['maximum'], 0)

    def test_missing_track_not_full_coverage(self):
        a = {'x': [(1, [(0, 0, 5)])]}
        rows, _ = compare(a, a)
        tracks = summarize([dict(matched=rows), dict(matched=[])])
        self.assertEqual(tracks['x']['samples'], 1)


if __name__ == '__main__':
    unittest.main()
