import copy
import unittest
from draw_record_inspect import group_records


def fixture():
    return [dict(slot=slot,vertex=slot+4,draw=1,generation=frame+10,
                 producer=dict(epoch=3,ordinal=1781+frame,cycle=1000*(frame+1)),
                 position_ram_addresses=[f'{0x1000+16*(slot%48)+4*k:08x}' for k in range(3)],
                 position_words=[slot%48,2,3]) for frame in range(3) for slot in range(142)]


class DrawRecordTests(unittest.TestCase):
    def test_repeated_consumers_preserve_actual_addresses(self):
        rows=fixture();before=copy.deepcopy(rows);result=group_records(rows)
        self.assertEqual(rows,before)
        self.assertEqual(result['record_instances'],144)
        self.assertEqual(result['downstream_vertices'],426)
        self.assertEqual(result['frames'][0]['records'][0]['consumers'],
                         [dict(slot=s,vertex=s+4) for s in (0,48,96)])
        self.assertFalse(result['original_transforms_proven'])

    def test_changed_record_and_ownership_reject(self):
        for key,value,reason in [('position_words',[99,2,3],'changed between'),
                                 ('generation',11,'ownership'),('draw',2,'ownership'),
                                 ('position_ram_addresses',['00001000','00001008','0000100c'],'address sequence')]:
            rows=fixture();rows[48][key]=value
            with self.subTest(key=key),self.assertRaisesRegex(ValueError,reason): group_records(rows)

    def test_missing_duplicate_and_new_record_reject(self):
        rows=fixture()
        for bad in (rows[:-1],rows+[rows[0]],rows[:48]+[rows[0]]+rows[49:]):
            with self.subTest(),self.assertRaises(ValueError): group_records(bad)
        rows[48]['position_ram_addresses']=['00004000','00004004','00004008']
        with self.assertRaisesRegex(ValueError,'distinct record coverage'): group_records(rows)


if __name__=='__main__': unittest.main()
