import copy
import unittest
from large_transform_targets_inspect import group


def fixture():
    return [dict(vertex=1420+v,generation=10+f,ordinal=1781+f,draw=277,
                 ram_x=0x1000+32*(v%921)) for f in range(3) for v in range(2745)]


class LargeTargetTests(unittest.TestCase):
    def test_exact_gapped_targets_and_consumer_reuse(self):
        result=group(fixture())
        self.assertEqual(result['bases'],[0x1000+32*i for i in range(921)])
        self.assertEqual(result['consumers'][0],[1420,2341,3262])
        self.assertEqual(result['target_records'],2763)
        self.assertFalse(result['original_transform_generation_proven'])
    def test_changed_map_missing_address_and_generation_reject(self):
        for key,value in [('generation',11),('vertex',4163),('ram_x',0x1000),('draw',1)]:
            rows=fixture();rows[-1][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):group(rows)
        rows=fixture();rows[0]['ram_x']=0x1004
        with self.assertRaises(ValueError):group(rows)


if __name__=='__main__':unittest.main()
