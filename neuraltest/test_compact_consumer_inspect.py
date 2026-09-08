import struct
import unittest
from compact_consumer_inspect import encode,decode,HEADER,RECORD,MAX_RECORDS


def record(sample=1):
    packet=[0xe0000000,0x3f800000,0x40000000,0x3f000000,0,0,0,0]
    return dict(sample=sample,epoch=3,ordinal=1781,cycle=0x100000002,generation=10,
                copy_source=0x100002020,copy_destination=0x200003040,decoder_pointer=0x200003040,
                vertex=1420,draw=277,ta_offset=32,sq_address=0xe0000020,
                ram_x=0x8c001000,ram_y=0x8c001004,ram_z=0x8c001008,reserved=0,
                before=packet[:],after=packet[:])


class CompactConsumerTests(unittest.TestCase):
    def test_exact_roundtrip_and_independent_layout(self):
        source=record();data=encode([source]);result=decode(data)
        self.assertEqual(RECORD.size,160)
        self.assertEqual(len(data),176)
        self.assertEqual(data[:8],b'FC067C01')
        self.assertEqual(data[HEADER.size+24:HEADER.size+32],b'\x02\x00\x00\x00\x01\x00\x00\x00')
        self.assertEqual(result['records'],[source])
        self.assertTrue(result['transport_only'])
        self.assertFalse(result['source_proven'])
    def test_large_domain_and_bounds(self):
        source=[record(i+1) for i in range(8235)]
        self.assertEqual(len(decode(encode(source))['records']),8235)
        with self.assertRaises(ValueError):encode([])
        with self.assertRaises(ValueError):encode([record()]*(MAX_RECORDS+1))
    def test_corruption_truncation_and_trailing_data(self):
        data=encode([record()]);corrupt=bytearray(data);corrupt[-1]^=1
        for bad in (data[:-1],data+b'\0',corrupt,b'BADMAGIC'+data[8:],data[:8]+struct.pack('<I',MAX_RECORDS+1)+data[12:]):
            with self.subTest(),self.assertRaises(ValueError):decode(bad)
    def test_identity_pointer_and_actual_copy_controls(self):
        for key,value in [('sample',2),('decoder_pointer',123),('generation',0),('reserved',1),
                          ('ram_y',0x8c001008),('cycle',True),('copy_source',2**64)]:
            source=record();source[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):encode([source])
        source=record();source['after'][1]^=1
        with self.assertRaises(ValueError):encode([source])


if __name__=='__main__':unittest.main()
