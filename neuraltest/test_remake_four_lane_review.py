# SPDX-License-Identifier: GPL-2.0-or-later
import copy
import unittest
from remake_four_lane_review import verify_native_identity


class IdentityTest(unittest.TestCase):
    def setUp(self):
        self.native=dict(frame_id=10,producer_identity=dict(available=True,epoch=3,ordinal=9,cycle=100))
        self.combined=copy.deepcopy(self.native)
        del self.combined['producer_identity']['available']

    def test_exact_identity(self):
        verify_native_identity(self.native,self.combined,10)

    def test_wrong_frame(self):
        for side in ('native','combined'):
            with self.subTest(side=side):
                wrong=copy.deepcopy(getattr(self,side));wrong['frame_id']=11
                with self.assertRaises(ValueError):
                    verify_native_identity(wrong if side=='native' else self.native,
                        wrong if side=='combined' else self.combined,10)

    def test_each_producer_component(self):
        for key in ('epoch','ordinal','cycle'):
            with self.subTest(key=key):
                wrong=copy.deepcopy(self.native);wrong['producer_identity'][key]+=1
                with self.assertRaises(ValueError):
                    verify_native_identity(wrong,self.combined,10)

    def test_unavailable_producer(self):
        self.native['producer_identity']['available']=False
        with self.assertRaises(ValueError):
            verify_native_identity(self.native,self.combined,10)


if __name__=='__main__':
    unittest.main()
