import unittest
from transform_incarnation_inspect import inspect


def fixture(version_count=2):
    lines=['FC067_CT_BEGIN generation=1 cycle=1'];rows=[]
    for version in range(1,version_count+1):
        lines.append(f'FC067_CT_INCARNATION generation=1 slot=0 version={version} previous_seams={15 if version>1 else 0} previous_reads={3 if version>1 else 0} cycle={version*10} dense_id={version-1}')
        for kind in range(4):lines.append(f'FC067_CT_BLOCK generation=1 slot=0 kind={kind} base=1000 cycle={version*10}')
        for c in range(3):lines.append(f'FC067_CT_WRITE generation=1 slot=0 event={c+1} address={0x1000+4*c:x} source={version+c:x} actual={version+c:x} cycle={version*10}')
        for c in range(3):lines.append(f'FC067_CT_GATHER generation=1 slot=0 sample={version} component={c} address={0x1000+4*c:x} value={version+c:x} expected={version+c:x}')
        rows.append(dict(sample=version,generation=1,vertex=version,ram_x=0x1000,
                         decoder_pointer=0x2000+version*32,cycle=version*10,after=[0,version,version+1,version+2]))
    lines.append(f'FC067_CT_PREFIX generation=1 cycle={(version_count+1)*10}')
    for v in range(1,version_count+1):lines.append(f'FC067_LEDGER_BIND sample={v} generation=1 copy_id={v} vertex={v} base=1000 pointer={0x2000+v*32:x} exact=1')
    return '\n'.join(lines)+'\n',rows


class IncarnationTests(unittest.TestCase):
    def test_dense_observed_lifetimes_keep_legacy_default(self):
        text,rows=fixture(8)
        r=inspect(text,[0x1000],rows,True,dense=True)
        self.assertEqual(r['lifetimes'],8)
        self.assertEqual(r['selected_slots'][8],(1,7))
        with self.assertRaisesRegex(ValueError,'dense lifetime identity'):inspect(text.replace('dense_id=7','dense_id=6'),[0x1000],rows,dense=True)
        with self.assertRaisesRegex(ValueError,'lifetime sequence/budget'):inspect(text,[0x1000],rows)
        with self.assertRaisesRegex(ValueError,'lifetime sequence/budget'):inspect(text.replace('version=8','version=9'),[0x1000],rows,dense=True)

    def test_dense_hard_shared_budget(self):
        text,rows=fixture(4096)
        self.assertEqual(inspect(text,[0x1000],rows,dense=True)['lifetimes'],4096)
        text,rows=fixture(4097)
        with self.assertRaisesRegex(ValueError,'dense lifetime record budget'):inspect(text,[0x1000],rows,dense=True)

    def test_explicit_six_versions_and_unchanged_default(self):
        text,rows=fixture(6)
        self.assertEqual(inspect(text,[0x1000],rows,max_versions=6)['lifetimes'],6)
        with self.assertRaisesRegex(ValueError,'lifetime sequence/budget'):inspect(text,[0x1000],rows)
        text,rows=fixture(7)
        with self.assertRaisesRegex(ValueError,'lifetime sequence/budget'):inspect(text,[0x1000],rows,max_versions=6)
        with self.assertRaisesRegex(ValueError,'expanded lifetime record budget'):inspect('',list(range(683)),[],max_versions=6)
        with self.assertRaisesRegex(ValueError,'version contract'):inspect('',[],[],max_versions=8)

    def test_completed_prefetch_is_not_projection(self):
        text,rows=fixture()
        load='FC067_X_PREFETCH_LOAD generation=1 slot=0 pc=8c03c9d6 address=1000 value=2 expected=2\n'
        edge='FC067_X_PREFETCH_EDGE generation=1 slot=0 block=8c03c9d8 pointer=1004 value=2 expected=2 exact=1\n'
        text=text.replace('FC067_CT_PREFIX',load+edge+'FC067_CT_PREFIX')
        self.assertEqual(inspect(text,[0x1000],rows)['completed_prefetches'],1)
        for old,new in [('block=8c03c9d8','block=8c03c9a4'),('pointer=1004','pointer=1008'),('value=2 expected=2','value=3 expected=3')]:
            with self.subTest(old=old),self.assertRaises(ValueError):inspect(text.replace(old,new),[0x1000],rows)

    def test_old_copy_retains_its_version_after_rewrite(self):
        text,rows=fixture();result=inspect(text,[0x1000],rows,True)
        self.assertEqual(result['selected'],{1:(1,0,1),2:(1,0,2)})
        self.assertEqual(result['lifetimes'],2)
        self.assertFalse(result['arithmetic_verified'])

    def test_wrong_version_and_incomplete_retirement_reject(self):
        text,rows=fixture()
        for old,new in [('copy_id=1','copy_id=2'),('version=2','version=3'),
                        ('previous_reads=3','previous_reads=0'),
                        ('component=2 address=1008 value=3 expected=3','component=1 address=1008 value=3 expected=3')]:
            with self.subTest(old=old),self.assertRaises(ValueError):inspect(text.replace(old,new),[0x1000],rows)

    def test_duplicate_copy_and_write_after_consumption(self):
        text,rows=fixture()
        with self.assertRaisesRegex(ValueError,'reused selected copy'):
            inspect(text.replace('copy_id=2','copy_id=1'),[0x1000],rows)
        extra='FC067_CT_WRITE generation=1 slot=0 event=4 address=1000 source=1 actual=1 cycle=10\n'
        text=text.replace('FC067_CT_INCARNATION generation=1 slot=0 version=2',extra+'FC067_CT_INCARNATION generation=1 slot=0 version=2')
        with self.assertRaisesRegex(ValueError,'write after consumption'):
            inspect(text,[0x1000],rows)


if __name__=='__main__':unittest.main()
