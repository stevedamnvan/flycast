import copy
import unittest

from reverse_decode_inspect import inspect


class ReverseDecodeTests(unittest.TestCase):
    def setUp(self):
        self.session = (
            'FC067_REVERSE_DECODE epoch=3 ordinal=1781 cycle=7605222912 context=00509700 '
            'offset=32 input_offset=36 vertex=4 type=3 part=0 bytes=32 '
            'words=e0000000,3f800000,40000000,3f000000,00000000,3f800000,ff0000ff,00000000\n'
            'FC067_REVERSE_FINAL offset=32 list=0 draw=1 first=4 count=166 vertex=4 '
            'tcw=392880 tsp=543696109 vertices=5 indices=170\n')
        self.manifest = dict(frame_id=1782, game_id='T1401N', producer_identity=dict(
            available=True, clock='sh4-scheduler-cycles', epoch=3, ordinal=1781, cycle=7605222912))
        self.scene = dict(schema='flycast-pvr-scene-v2', frame_id=1782, game_id='T1401N',
            vertices=[[] for _ in range(4)] + [
                [0x3f800000, 0x40000000, 0x3f000000, 0, 0x3f800000, [255, 0, 0, 255], [0]*4]],
            indices=[0]*4 + [4]*166,
            draws=[dict(list=0, ordinal=1, range_space='indices', first=4, count=166,
                        tcw=392880, tsp=543696109, naomi2=False, texture1=None,
                        texture=dict(upload_generation=1, palette_hash=None, rtt_generation=0))])

    def test_supported_witness(self):
        result = inspect(self.session, self.scene, self.manifest)
        self.assertFalse(result['transfer_provenance'])
        self.assertFalse(result['camera_recovered'])

    def test_packet_offset_controls(self):
        for old, new in [('offset=32', 'offset=36'), ('input_offset=36', 'input_offset=40'),
                         ('type=3', 'type=5'), ('bytes=32', 'bytes=64')]:
            with self.subTest(new=new), self.assertRaises(ValueError):
                inspect(self.session.replace(old, new, 1), self.scene, self.manifest)

    def test_stale_generation_and_order(self):
        for session in [self.session.replace('epoch=3', 'epoch=2'),
                        '\n'.join(reversed(self.session.splitlines())),
                        self.session + self.session, self.session + 'FC067_REVERSE_REJECT']:
            with self.subTest(session=session), self.assertRaises(ValueError):
                inspect(session, self.scene, self.manifest)

    def test_changed_packet_or_material(self):
        for session in [self.session.replace('3f800000', '3f800001', 1),
                        self.session.replace('tcw=392880', 'tcw=392881'),
                        self.session.replace('e0000000', '80000000')]:
            with self.subTest(session=session), self.assertRaises(ValueError):
                inspect(session, self.scene, self.manifest)
        for key, value in [('upload_generation', 2), ('palette_hash', 123), ('rtt_generation', 1)]:
            scene = copy.deepcopy(self.scene)
            scene['draws'][0]['texture'][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                inspect(self.session, scene, self.manifest)

    def test_restart_and_mismatched_final_scene(self):
        for index in [0xffffffff, 3]:
            scene = copy.deepcopy(self.scene)
            scene['indices'][4] = index
            with self.subTest(index=index), self.assertRaises(ValueError):
                inspect(self.session, scene, self.manifest)


if __name__ == '__main__':
    unittest.main()
