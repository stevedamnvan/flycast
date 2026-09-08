import unittest

from ram_record_inspect import BEGIN, END, START, overlap, inspect_record, check_controls


def fixture():
    rows = [f'FC067_RAM_BEGIN cycle={BEGIN} context=00509700 generation=1 start=8ce74250 bytes=12 cap=64 max_cycles=400000000']
    # Synthetic values: no captured game data. An 8-byte write followed by 4 bytes.
    values = bytes(range(12))
    for event, first, count in [(1, 0, 8), (2, 8, 4)]:
        rows.append(f'FC067_RAM_WRITE event={event} generation=1 cycle={BEGIN+100} path=cpu pc=8c001000 address={START+first:08x} size={count} first={first} count={count} source_offset=0')
        for byte in range(first, first+count):
            rows.append(f'FC067_RAM_BYTE event={event} byte={byte} source={values[byte]:02x} expected={values[byte]:02x} actual={values[byte]:02x} previous_writer=0')
    rows.append(f'FC067_RAM_HLE cycle={BEGIN+101} syscall=0 command=4 idle=0 status=0 drive=1')
    for index in range(3):
        value = int.from_bytes(values[index*4:index*4+4], 'little')
        writer = 1 if index < 2 else 2
        rows.append(f'FC067_RAM_READ generation=1 cycle={END} pc={0x8c03cc7c+index*2:08x} address={START+index*4:08x} value={value:08x} expected={value:08x} coverage=fff writers={writer},{writer},{writer},{writer}')
    rows.append('FC067_RAM_STOP reason=target-covered events=2 reads=3 coverage=fff')
    return '\n'.join(rows)+'\n'


class RamRecordTests(unittest.TestCase):
    def test_complete_and_controls(self):
        result = inspect_record(fixture())
        self.assertEqual(result['last_writers'], [1]*8+[2]*4)
        self.assertEqual(result['write_events'], 2)
        self.assertFalse(result['camera_recovered'])
        self.assertEqual(check_controls(fixture()), 5)

    def test_aliases_and_partial_overlap(self):
        for region in range(7):
            for mirror in range(4):
                address = (region << 29) | 0x0c000000 | (mirror << 24) | (START & 0xffffff)
                self.assertEqual(overlap(address, 8), (0, 8, 0))
                self.assertEqual(overlap(address-4, 8), (0, 4, 4))
                self.assertEqual(overlap(address+10, 4), (10, 2, 0))
        for address, size in [(0xece74250, 4), (0x84e74250, 4), (START+12, 4),
                              (0x8cffffff, 8), (START, 0), (START, 0x1000001)]:
            with self.subTest(address=address, size=size), self.assertRaises(ValueError):
                overlap(address, size)

    def test_missing_reordered_or_extra_bytes(self):
        source = fixture()
        rows = source.splitlines()
        mutations = [source.replace(rows[2]+'\n', ''),
                     source.replace(rows[2]+'\n'+rows[3], rows[3]+'\n'+rows[2]),
                     source.replace(rows[2], rows[2]+'\n'+rows[2])]
        for changed in mutations:
            with self.assertRaises(ValueError):
                inspect_record(changed)

    def test_stale_overwritten_bytes(self):
        source = fixture()
        # A second overlapping write must name the previous per-byte writer.
        insert = (f'FC067_RAM_WRITE event=3 generation=1 cycle={BEGIN+102} path=cpu pc=8c002000 address={START:08x} size=1 first=0 count=1 source_offset=0\n'
                  'FC067_RAM_BYTE event=3 byte=0 source=00 expected=00 actual=00 previous_writer=1\n')
        source = source.replace('FC067_RAM_READ generation=1', insert+'FC067_RAM_READ generation=1', 1)
        source = source.replace('writers=1,1,1,1', 'writers=3,1,1,1', 1).replace('events=2 reads=3', 'events=3 reads=3')
        self.assertEqual(inspect_record(source)['last_writers'][0], 3)
        with self.assertRaises(ValueError):
            inspect_record(source.replace('previous_writer=1', 'previous_writer=0'))

    def test_wrong_contract_and_unknown_paths(self):
        for old, new in [('context=00509700', 'context=00509701'), ('path=cpu', 'path=unknown'),
                         ('size=8', 'size=3'), ('source_offset=0', 'source_offset=1'),
                         ('command=4', 'command=7'), ('drive=1', 'drive=0'),
                         (f'cycle={BEGIN+100}', f'cycle={BEGIN-1}'),
                         ('reason=target-covered', 'reason=emulator-reset')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                inspect_record(fixture().replace(old, new, 1))

    def test_no_rearm_or_unterminated_session(self):
        for source in [fixture()+fixture(), '\n'.join(fixture().splitlines()[:-1]),
                       fixture()+'FC067_RAM_BYTE event=3 byte=0 source=00 expected=00 actual=00 previous_writer=1']:
            with self.assertRaises(ValueError):
                inspect_record(source)

    def test_wrong_read_and_writer_ids(self):
        for old, new in [('address=8ce74258', 'address=8ce7425c'),
                         ('pc=8c03cc7e', 'pc=8c03cc80'), ('writers=1,1,1,1', 'writers=2,2,2,2'),
                         ('value=03020100', 'value=03020101')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                inspect_record(fixture().replace(old, new))

    def test_missing_and_duplicate_fields(self):
        for old, new in [(' drive=1', ''), ('size=8', 'size=8 size=8'),
                         ('size=8', 'size=8 extra=1')]:
            with self.assertRaises(ValueError):
                inspect_record(fixture().replace(old, new, 1))

    def test_time_and_event_caps(self):
        with self.assertRaises(ValueError):
            inspect_record(fixture().replace(f'cycle={BEGIN+100}', f'cycle={BEGIN+400000001}'))
        rows = fixture().splitlines()
        writes = []
        for event in range(3, 66):
            writes += [f'FC067_RAM_WRITE event={event} generation=1 cycle={BEGIN+102} path=cpu pc=8c001000 address={START:08x} size=1 first=0 count=1 source_offset=0',
                       f'FC067_RAM_BYTE event={event} byte=0 source=00 expected=00 actual=00 previous_writer={1 if event==3 else event-1}']
        first_read = next(i for i,line in enumerate(rows) if 'FC067_RAM_READ' in line)
        with self.assertRaisesRegex(ValueError, 'cap'):
            inspect_record('\n'.join(rows[:first_read]+writes+rows[first_read:]))

    def test_write_during_consumption(self):
        source = fixture()
        first_read = next(line for line in source.splitlines() if 'FC067_RAM_READ' in line)
        write = f'FC067_RAM_WRITE event=3 generation=1 cycle={END} path=cpu pc=8c001000 address={START:08x} size=1 first=0 count=1 source_offset=0'
        with self.assertRaisesRegex(ValueError, 'consumption'):
            inspect_record(source.replace(first_read, first_read+'\n'+write))


if __name__ == '__main__':
    unittest.main()
