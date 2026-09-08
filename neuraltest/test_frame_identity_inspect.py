import unittest
from frame_identity_inspect import inspect, verify, validate_manifests


def fixture():
    lines=[]
    for i in range(3):
        p=i+1;cycle=100+p*10
        lines += [f'FC067_SUBMIT id={p} cycles={cycle} accepted=1 rtt=0 address=00100000',
                  f'FC067_DEQUEUE id={p} producer_cycles={cycle} address=00100000 rtt=0',
                  f'FC067_CAPTURE_ATTEMPT id={p+2} seen_before={i} seen_after={i+1} retained_before={i} retained_after={i+1} success=1 producer_id={p} producer_cycle={cycle}']
    return '\n'.join(lines)


class FrameIdentityTests(unittest.TestCase):
    def test_manifest_identity_controls(self):
        import copy
        witness=inspect(fixture())
        manifests=[dict(frame_id=r['frame_id'],producer_identity=dict(available=True,
                    clock='sh4-scheduler-cycles',epoch=2,ordinal=r['producer_id'],cycle=r['producer_cycle']))
                   for r in witness]
        self.assertEqual(validate_manifests(manifests,witness)['matched_frames'],3)
        for key,value in [('available',False),('clock','wall-clock'),('ordinal',99),
                          ('cycle',111),('epoch',0),('cycle',True),('cycle',-1)]:
            wrong=copy.deepcopy(manifests);wrong[0]['producer_identity'][key]=value
            with self.assertRaises(ValueError): validate_manifests(wrong,witness)
        wrong=copy.deepcopy(manifests);wrong[1]['producer_identity']['epoch']=3
        with self.assertRaises(ValueError): validate_manifests(wrong,witness)
        wrong=copy.deepcopy(manifests);del wrong[0]['producer_identity']
        with self.assertRaises(ValueError): validate_manifests(wrong,witness)

    def test_positive_and_controls(self):
        self.assertEqual(verify(fixture())['rejected_offline_controls'],4)

    def test_duplicate_producer(self):
        s=fixture()
        with self.assertRaises(ValueError): inspect(s+'\n'+s.splitlines()[0])

    def test_context_mismatch(self):
        with self.assertRaises(ValueError):
            inspect(fixture().replace('producer_cycles=110 address=00100000','producer_cycles=110 address=00200000'))

    def test_reordered_dequeue(self):
        lines=fixture().splitlines();lines[0],lines[1]=lines[1],lines[0]
        with self.assertRaises(ValueError): inspect('\n'.join(lines))

    def test_no_capture(self):
        with self.assertRaises(ValueError): inspect('')


if __name__ == '__main__': unittest.main()
