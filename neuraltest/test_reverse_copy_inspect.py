import unittest

from reverse_copy_inspect import inspect, check_controls
import test_reverse_decode_inspect as decoder_fixture


class ReverseCopyTests(unittest.TestCase):
    def setUp(self):
        fixture = decoder_fixture.ReverseDecodeTests()
        fixture.setUp()
        self.scene, self.manifest = fixture.scene, fixture.manifest
        packet = fixture.session.split('words=')[1].splitlines()[0].split(',')
        self.session = (
            'FC067_REVERSE_COPY generation=1 cycle=7602643776 context=00509700 offset=32 '
            'bytes=32 path=2 address=e0000020 source_index=0 source=1000 destination=2020 exact=1\n'
            + ''.join(f'FC067_REVERSE_COPY_WORD generation=1 index={i} before={w} after={w}\n'
                      for i, w in enumerate(packet))
            + 'FC067_REVERSE_LINK generation=1 cycle=7602643776 destination=2020 packet=2020 exact=1\n'
            + fixture.session)

    def test_supported_copy(self):
        result = inspect(self.session, self.scene, self.manifest)
        self.assertTrue(result['transfer_provenance'])
        self.assertFalse(result['cpu_producer_provenance'])
        self.assertFalse(result['camera_recovered'])

    def test_offline_controls(self):
        self.assertEqual(check_controls(self.session, self.scene, self.manifest), 5)

    def test_wrong_generation(self):
        for token in ['COPY generation=1', 'LINK generation=1', 'COPY_WORD generation=1']:
            with self.subTest(token=token), self.assertRaises(ValueError):
                inspect(self.session.replace(token, token[:-1]+'2', 1), self.scene, self.manifest)

    def test_wrong_offset_or_pointer(self):
        for old, new in [('offset=32', 'offset=64'), ('packet=2020', 'packet=2040'),
                         ('destination=2020', 'destination=2040')]:
            with self.subTest(new=new), self.assertRaises(ValueError):
                inspect(self.session.replace(old, new, 1), self.scene, self.manifest)

    def test_wrong_transfer_or_cycle(self):
        for old, new in [('path=2', 'path=1'), ('address=e0000020', 'address=e0000000'),
                         ('cycle=7602643776', 'cycle=7602643777'), ('exact=1', 'exact=0')]:
            with self.subTest(new=new), self.assertRaises(ValueError):
                inspect(self.session.replace(old, new, 1), self.scene, self.manifest)

    def test_changed_bytes_or_missing_record(self):
        for session in [self.session.replace('before=e0000000', 'before=e0000001'),
                        self.session.replace('after=e0000000', 'after=e0000001'),
                        '\n'.join(self.session.splitlines()[1:]),
                        self.session + self.session.splitlines()[1] + '\n']:
            with self.subTest(session=session), self.assertRaises(ValueError):
                inspect(session, self.scene, self.manifest)


if __name__ == '__main__':
    unittest.main()
