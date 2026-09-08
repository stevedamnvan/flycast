"""Parser cardinality checks; actual packet controls run on retained captures."""
import unittest
from combat_packet_inspect import one


class PacketRecordTests(unittest.TestCase):
    def test_missing_record_rejected(self):
        with self.assertRaises(ValueError):
            one('unrelated line', 'FC067_PACKET_COPY')

    def test_repeated_record_rejected(self):
        with self.assertRaises(ValueError):
            one('FC067_PACKET_COPY exact=1\nFC067_PACKET_COPY exact=1', 'FC067_PACKET_COPY')

    def test_similar_tag_is_not_record(self):
        self.assertEqual(one('FC067_PACKET_COPY_EXTRA exact=0\nFC067_PACKET_COPY exact=1',
                             'FC067_PACKET_COPY'), {'exact':'1'})


if __name__ == '__main__':
    unittest.main()
