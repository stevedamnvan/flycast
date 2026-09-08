import unittest
from producer_record_inspect import assemble


def fixture():
    return [dict(pc=pc, step=1, address=0x8ce6e460+4*i, size=4, value=i+1)
            for pc, i in [('8c03aa10', 2), ('8c03aa12', 1), ('8c03aa14', 0)]]


class ProducerRecordTests(unittest.TestCase):
    def test_completed_reused_address(self):
        rows = fixture()
        later = [dict(r, step=2, value=r['value']+10) for r in rows]
        result = assemble(rows+later)
        self.assertEqual([r['xyz'] for r in result], [[1, 2, 3], [11, 12, 13]])
        self.assertEqual(result[0]['base'], result[1]['base'])

    def test_wrong_address_step_missing_reordered(self):
        for rows in (fixture()[:-1], fixture()[::-1],
                     [fixture()[0], dict(fixture()[1], address=0x8ce6e468), fixture()[2]],
                     [fixture()[0], dict(fixture()[1], step=2), fixture()[2]]):
            with self.assertRaises(ValueError): assemble(rows)

    def test_marker_is_not_geometry(self):
        marker = dict(pc='8c03a9f2', step=1, address=0x8ce6e460, size=4, value=31)
        self.assertEqual(assemble([marker]), [])
        self.assertEqual(len(assemble([marker]+fixture())), 1)
        with self.assertRaises(ValueError): assemble(fixture()[:1]+[marker]+fixture()[1:])
