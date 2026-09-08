import unittest
from producer_ownership_inspect import verify_memory, verify_copies, bind_tape, bind_records


def fixture():
    return [dict(kind=kind, event=str(i+1), pc='8c03aa14', address='8ce6e460',
                 size='4', value='000000003f800000', cycle=str(i+1))
            for i, kind in enumerate(('write', 'read'))]


class ProducerOwnershipTests(unittest.TestCase):
    def test_later_frame_requires_explicit_matching_ordinal(self):
        text='FC067_PRODUCER_COPY id=1 ordinal=1782 generation=1791 offset=32 base=8ce6e460 x=00000001 y=00000002 z=00000003 cycle=2'
        record=dict(ordinal=1782,generation=1791,ta_offset=32,ram_x=0x8ce6e460,after=[0,1,2,3],cycle=3)
        self.assertEqual(bind_tape(text,[record],1782)['selected_copy_tape_matches'],1)
        self.assertEqual(verify_copies(text,1782)['copies_outside_tracked_reads'],1)
        with self.assertRaisesRegex(ValueError,'copy ordinal'):verify_copies(text)
        with self.assertRaisesRegex(ValueError,'no copy/tape matches'):bind_tape(text,[record])
        with self.assertRaisesRegex(ValueError,'ordinal bounds'):verify_copies(text,1784)

    def test_complete_record_and_marker_invalidation(self):
        base = 0x8ce6e460
        records = [dict(step=1, base=base, xyz=[1,2,3], family='divided')]
        lines = ['FC067_PRODUCER_ENTRY step=1']
        for i, pc in ((2,'8c03aa10'), (1,'8c03aa12'), (0,'8c03aa14')):
            lines.append(f'FC067_PRODUCER_MEMORY kind=write address={base+4*i:08x} size=4 value={i+1:08x} pc={pc}')
        lines.append(f'FC067_PRODUCER_STORE step=1 address={base:08x} value=00000001')
        reads = [f'FC067_PRODUCER_MEMORY kind=read address={base+4*i:08x} size=4' for i in range(3)]
        copy = f'FC067_PRODUCER_COPY id=1 base={base:08x} x=00000001 y=00000002 z=00000003'
        self.assertEqual(bind_records('\n'.join(lines+reads+[copy]), records)['copies_owned_by_complete_record'], 1)
        marker = f'FC067_PRODUCER_MEMORY kind=write address={base:08x} size=4 value=00000001 pc=8c03a9f2'
        self.assertEqual(bind_records('\n'.join(lines+[marker]+reads+[copy]), records)['copies_owned_by_complete_record'], 0)
        with self.assertRaises(ValueError):
            bind_records('\n'.join(lines+reads+[copy]).replace('pc=8c03aa12', 'pc=8c03a9f2'), records)

    def test_tape_join_and_mutations(self):
        text = 'FC067_PRODUCER_COPY id=1 ordinal=1781 generation=1790 offset=32 base=8ce6e460 x=00000001 y=00000002 z=00000003 cycle=2'
        record = dict(ordinal=1781, generation=1790, ta_offset=32, ram_x=0x8ce6e460, after=[0,1,2,3], cycle=3)
        self.assertEqual(bind_tape(text, [record])['selected_copy_tape_matches'], 1)
        for rows in ([record, record], [dict(record, ram_x=0x8ce6e464)],
                     [dict(record, after=[0,1,2,4])], [dict(record, cycle=1)],
                     [dict(record, generation=1791)]):
            with self.assertRaises(ValueError): bind_tape(text, rows)

    def test_copy_identity_and_words(self):
        text = '\n'.join(f'FC067_PRODUCER_MEMORY kind=read address={0x8ce6e460+4*i:08x} value={i+1:08x} cycle=1' for i in range(3))
        copy = 'FC067_PRODUCER_COPY id=1 ordinal=1781 generation=1790 offset=32 base=8ce6e460 x=00000001 y=00000002 z=00000003 cycle=2'
        text += '\n'+copy
        self.assertEqual(verify_copies(text)['copies_with_tracked_reads'], 1)
        for bad in (text.replace('z=00000003', 'z=00000004'), text+'\n'+copy,
                    text+'\n'+copy.replace('id=1', 'id=2').replace('offset=32', 'offset=64')):
            with self.assertRaises(ValueError): verify_copies(bad)

    def test_latest_writer(self):
        self.assertEqual(verify_memory(fixture())['new_family_last_writer_reads'], 1)

    def test_intervening_marker_invalidates(self):
        rows = fixture()
        rows.insert(1, dict(rows[0], event='2', cycle='2', pc='8c03a9f2'))
        rows[2].update(event='3', cycle='3')
        self.assertEqual(verify_memory(rows)['new_family_last_writer_reads'], 0)

    def test_wrong_bytes_sequence_and_time(self):
        for change in (dict(value='0000000040000000'), dict(event='3'), dict(cycle='0')):
            rows = fixture()
            rows[1].update(change)
            with self.assertRaises(ValueError): verify_memory(rows)

    def test_missing_writer_explicit(self):
        rows = fixture()
        rows[1]['address'] = '8ce6e464'
        self.assertEqual(verify_memory(rows)['reads_without_observed_writer'], 1)
