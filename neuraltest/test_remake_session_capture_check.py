# SPDX-License-Identifier: GPL-2.0-or-later
import unittest
from remake_session_capture_check import receipts, verify


class SessionJoinTest(unittest.TestCase):
    def test_joins_and_rejects_wrong_identity(self):
        data=receipts('live_receive sequence=1 frame=10 producer=20 bytes=30 digest=40\n'
                      'live_return sequence=1 frame=10 published=1')
        record=dict(session_token='root-g1',sequence=1,source_frame=10,
                    producer_epoch=2,producer_ordinal=20,source_digest=40)
        self.assertEqual(verify(record,(10,2,20,100),{'root-g1':data}),'root-g1')
        for key,value in [('session_token','root-g2'),('source_digest',41),
                          ('source_frame',11),('sequence',2),('producer_ordinal',21),
                          ('producer_epoch',3)]:
            wrong=dict(record);wrong[key]=value
            with self.assertRaises(ValueError):
                verify(wrong,(10,2,20,100),{'root-g1':data})
        with self.assertRaises(ValueError):
            verify(record,(10,2,20,100),{'root-g1':(data[0],set())})

    def test_duplicate_receipt_rejected(self):
        line='live_receive sequence=1 frame=10 producer=20 bytes=30 digest=40\n'
        with self.assertRaises(ValueError):
            receipts(line+line)


if __name__ == '__main__':
    unittest.main()
