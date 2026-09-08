import copy
import unittest

from camera_packet_inspect import inspect, check_controls, ORDINALS, VERTICES


def fixture():
    lines, frames = [], []
    for sequence, ordinal in enumerate(ORDINALS, 1):
        cycle, root = 1000*sequence, ('00100000' if sequence == 2 else '00500000')
        stamp = dict(available=True, clock='sh4-scheduler-cycles', epoch=3, ordinal=ordinal, cycle=cycle)
        manifest = dict(frame_id=ordinal+1, git_sha='synthetic', game_id='T1401N', producer_identity=stamp)
        scene = dict(frame_id=ordinal+1, git_sha='synthetic', game_id='T1401N',
                     vertices=[[0, 0, 0] for _ in range(149)], indices=list(VERTICES), draws=[])
        for draw, first in ((1, 0), (26, 3)):
            scene['draws'].append(dict(list=0, ordinal=draw, range_space='indices', first=first,
                                       count=3, tcw=123+draw, tsp=456))
        lines.append(f'FC067_CAMERA_PACKET_BEGIN epoch=3 ordinal={ordinal} cycle={cycle} root={root} sequence={sequence}')
        for slot, vertex in enumerate(VERTICES):
            xyz = [0x43000000+slot+sequence, 0x43800000+slot, 0x3e800000+sequence]
            scene['vertices'][vertex] = xyz
            packet = ','.join(f'{v:08x}' for v in [0xe0000000, *xyz, 0, 0, 0, 0])
            lines.append(f'FC067_CAMERA_PACKET_DECODE epoch=3 expected_epoch=3 ordinal={ordinal} cycle={cycle} slot={slot} vertex={vertex} child={root} offset={32*(slot+1)} type=3 part=0 bytes=32 words={packet}')
        for slot, vertex in enumerate(VERTICES):
            draw, first = (1, 0) if slot < 3 else (26, 3)
            xyz = ','.join(f'{v:08x}' for v in scene['vertices'][vertex])
            lines.append(f'FC067_CAMERA_PACKET_FINAL ordinal={ordinal} slot={slot} vertex={vertex} child={root} offset={32*(slot+1)} list=0 draw={draw} first={first} count=3 index={slot} tcw={123+draw} tsp=456 xyz={xyz}')
        lines.append(f'FC067_CAMERA_PACKET_END ordinal={ordinal} frames={sequence} packets={sequence*6}')
        frames.append((scene, manifest))
    return '\n'.join(lines)+'\n', frames


class CameraPacketTests(unittest.TestCase):
    def test_three_frames_two_draws_without_camera_promotion(self):
        session, frames = fixture()
        before = copy.deepcopy(frames)
        result = inspect(session, frames)
        self.assertEqual(result['observed_vertices'], 18)
        self.assertEqual(result['frames'], 3)
        self.assertEqual(len(result['packets']), 18)
        self.assertEqual(frames, before)
        self.assertEqual(check_controls(session, frames), 8)
        for flag in ('source_value_matching_used', 'upstream_transform_linked',
                     'usable_camera_contract', 'shared_camera_proven', 'production_enabled'):
            self.assertIs(result[flag], False)

    def test_missing_duplicate_reordered_and_live_negative(self):
        session, frames = fixture()
        lines = session.splitlines(keepends=True)
        swapped = list(lines)
        swapped[1], swapped[2] = swapped[2], swapped[1]
        for bad in (''.join(lines[1:]), session+lines[1], ''.join(swapped),
                    session+'FC067_CAMERA_PACKET_REJECT reason=expected-epoch-mismatch\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                inspect(bad, frames)

    def test_epoch_cycle_and_frame_identity(self):
        session, frames = fixture()
        for old, new in (('expected_epoch=3', 'expected_epoch=4'), ('cycle=1000', 'cycle=999'),
                         ('root=00500000', 'root=00500020'), ('packets=18', 'packets=19')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                inspect(session.replace(old, new, 1), frames)
        for key, value in (('epoch', 4), ('cycle', 1000), ('ordinal', 1781), ('available', False)):
            bad = copy.deepcopy(frames)
            bad[1][1]['producer_identity'][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                inspect(session, bad)

    def test_packet_geometry_layout_and_offsets(self):
        session, frames = fixture()
        for old, new in (('43000001', '43000002'), ('vertex=4 ', 'vertex=5 '),
                         ('index=0 ', 'index=1 '), ('offset=32 ', 'offset=64 '),
                         ('type=3 ', 'type=4 '), ('bytes=32 ', 'bytes=64 '),
                         ('part=0 ', 'part=1 '), ('words=e0000000', 'words=80000000')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                inspect(session.replace(old, new, 1), frames)

    def test_draw_ambiguity_or_wrong_material(self):
        session, frames = fixture()
        bad = copy.deepcopy(frames)
        extra = dict(bad[0][0]['draws'][0], ordinal=99)
        bad[0][0]['draws'].append(extra)
        with self.assertRaisesRegex(ValueError, 'ambiguous opaque membership'):
            inspect(session, bad)
        with self.assertRaisesRegex(ValueError, 'draw contract mismatch'):
            inspect(session.replace('tcw=124 ', 'tcw=125 ', 1), frames)

    def test_limits_and_missing_scene_reference(self):
        session, frames = fixture()
        with self.assertRaisesRegex(ValueError, 'frame bound'):
            inspect(session, frames+[frames[0]])
        for field in ('indices', 'vertices', 'draws'):
            bad = copy.deepcopy(frames)
            bad[0][0][field] = []
            with self.subTest(field=field), self.assertRaises(ValueError):
                inspect(session, bad)


if __name__ == '__main__':
    unittest.main()
