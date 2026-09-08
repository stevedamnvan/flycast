import copy
import re
import unittest

from camera_source_inspect import inspect, OFFSETS, PCS
from test_camera_packet_inspect import fixture as packet_fixture
from test_xyz_operand_inspect import fixture as operand_fixture
from transform_span_inspect import records


def fixture(full_draw=False):
    """Independent synthetic operands and geometry only, no captured game bytes."""
    packet_log,frames=packet_fixture(full_draw)
    offsets=tuple(range(32,4545,32)) if full_draw else OFFSETS
    n=len(offsets)
    operand_log,_,_=operand_fixture()
    operand_log='FC067_XYZ_ENTRY '+operand_log.split('FC067_XYZ_ENTRY ',1)[1].split('FC067_SQ_FLUSH ',1)[0]
    words=[0xe0000000,0x3f800000,0x40000000,0x3f000000,0x3f803f80,0x3f800000,0xff0000ff,0]
    lines=[]
    for frame_index,(scene,manifest) in enumerate(frames):
        ordinal=1781+frame_index; generation=10+frame_index
        root='00100000' if frame_index==1 else '00500000'
        packet_lines=[line for line in packet_log.splitlines()
                      if f'ordinal={ordinal} ' in line]
        for slot,offset in enumerate(offsets):
            sample=frame_index*n+slot+1; cycle=1000*frame_index+100+slot
            sq=0xe0000000+offset; destination=0x100000+generation*0x10000+offset
            begin=(f'FC067_CS_BEGIN sample={sample} epoch_hint=3 ordinal_hint={ordinal} generation={generation} '
                   f'slot={slot} context={root} offset={offset} sq={sq:08x} cycle={cycle}')
            lines.append(begin)
            part=operand_log.replace('step=1 ',f'step={sample} ').replace('cycle=7602643776',f'cycle={cycle}')
            part=part.replace('context=00509700',f'context={root}').replace('ta_offset=32',f'ta_offset={offset}')
            part=part.replace('sq=e0000020',f'sq={sq:08x}')
            part=part.replace('value=e0000020 expected=e0000020',f'value={sq:08x} expected={sq:08x}')
            out=[]
            for line in part.splitlines():
                if 'FC067_XYZ_OP ' in line:
                    if full_draw and sample != 1: continue
                    index=int(records(line,'FC067_XYZ_OP')[0]['index'])
                    line=re.sub(r'pc=[0-9a-f]+',f'pc={PCS[index]:08x}',line)
                if 'FC067_SQ_STORE ' in line:
                    row=records(line,'FC067_SQ_STORE')[0]
                    line=line.replace('FC067_SQ_STORE ',f'FC067_CS_STORE sample={sample} ')
                    line=line.replace('event='+row['event'],f'event={int(row["event"])-1}')
                    line=re.sub(r'address=[0-9a-f]+',f'address={sq+int(row["offset"]):08x}',line)
                out.append(line)
            lines.extend(out)
            end=full_draw and slot==n-1
            if end:
                lines.append(f'FC067_CS_STORE sample={sample} event=8 cycle={cycle} pc=8c03ccee address={sq:08x} offset=0 size=4 expected=f0000000 actual=f0000000 exact=1')
            lines.append(f'FC067_CS_FLUSH sample={sample} cycle={cycle} coverage=fff writers=8c03cc82,8c03cc84,8c03cc86 events={8 if end else 7}')
            lines.append(f'FC067_CS_COPY sample={sample} expected={sample} epoch_hint=3 ordinal_hint={ordinal} generation={generation} context={root} offset={offset} sq={sq:08x} cycle={cycle} source={0x10000+(sq&32):x} destination={destination:x} exact=1')
            for index,word in enumerate(words):
                if index==0 and end: word=0xf0000000
                lines.append(f'FC067_CS_WORD sample={sample} index={index} before={word:08x} after={word:08x}')
        cycle=manifest['producer_identity']['cycle']
        lines.append(f'FC067_CS_FRAME epoch=3 ordinal={ordinal} cycle={cycle} selected={142 if full_draw else 63} total={(frame_index+1)*n}')
        for line in packet_lines:
            if 'slot=' in line:
                slot=int(re.search(r'slot=(\d+)',line)[1]); offset=offsets[slot]
                line=re.sub(r'offset=\d+',f'offset={offset}',line)
                vertex=int(re.search(r'vertex=(\d+)',line)[1]); scene['vertices'][vertex]=words[1:4]
                if 'DECODE ' in line:
                    sample=frame_index*n+slot+1; destination=0x100000+generation*0x10000+offset
                    lines.append(f'FC067_CS_LINK sample={sample} slot={slot} epoch=3 ordinal={ordinal} generation={generation} current_generation={generation} child={root} offset={offset} destination={destination:x} packet={destination:x} exact=1')
                    line=re.sub(r'words=\S+', 'words='+','.join(f'{w:08x}' for w in words),line)
                    if full_draw and slot==n-1: line=line.replace('words=e0000000','words=f0000000')
                else:
                    line=re.sub(r'xyz=\S+', 'xyz='+','.join(f'{w:08x}' for w in words[1:4]),line)
            lines.append(line)
    lines.append(f'FC067_CS_END reason=reset-after-complete samples={3*n}')
    return '\n'.join(lines)+'\n',frames


class CameraSourceTests(unittest.TestCase):
    def setUp(self):
        self.session,self.frames=fixture()

    def test_complete_both_slots_without_camera_claim(self):
        result=inspect(self.session,self.frames)
        self.assertEqual(result['returned_xyz_loads'],54)
        self.assertEqual(len(result['samples']),18)
        self.assertFalse(result['original_ram_producer_proven'])
        self.assertFalse(result['usable_camera_contract'])
        self.assertFalse(result['production_enabled'])

    def reject(self,old,new,reason):
        self.assertIn(old,self.session)
        with self.assertRaisesRegex(ValueError,reason):
            inspect(self.session.replace(old,new,1),self.frames)

    def test_wrong_identity_generation_pointer_and_epoch(self):
        self.reject('expected=1 ','expected=2 ','expected copy identity')
        self.reject('current_generation=10 ','current_generation=11 ','context generation')
        self.reject('packet=1a0020 ','packet=1a0040 ','copy decoder pointer')
        self.reject('epoch_hint=3 ','epoch_hint=4 ','actual producer')

    def test_wrong_bytes_writer_and_physical_slot(self):
        self.reject('after=3f800000','after=3f800001','copied bytes')
        self.reject('pc=8c03cc82 address=','pc=8c03cc80 address=','last-writer')
        self.reject('address=e0000024','address=e0000004','physical SQ slot')

    def test_wrong_operand_address_and_instruction(self):
        self.reject('index=13 address=00002050','index=13 address=00002054','load address disagreement')
        self.reject('pc=8c03cc68 op=readm','pc=8c03cc66 op=readm','operation shape')
        self.reject('reg=6 name=r6 value=00002000 expected=00002000',
                    'reg=6 name=r6 value=00002001 expected=00002001','arithmetic disagreement')

    def test_missing_duplicate_reset_and_early_end(self):
        for bad in (self.session+'FC067_CS_REJECT reason=reset\n',
                    self.session+self.session.splitlines()[0]+'\n',
                    self.session.replace('FC067_CS_WORD sample=18 index=7','NOT_A_WORD sample=18 index=7'),
                    'FC067_CS_END reason=reset-after-complete samples=18\n'+self.session.rsplit('FC067_CS_END ',1)[0]):
            with self.subTest(),self.assertRaises(ValueError): inspect(bad,self.frames)

    def test_actual_scene_mutation_and_clock(self):
        frames=copy.deepcopy(self.frames); frames[1][0]['vertices'][4][0]^=1
        with self.assertRaises(ValueError): inspect(self.session,frames)
        self.reject('cycle=100 coverage=fff','cycle=99 coverage=fff','clock bounds')


class FullDrawSourceTests(unittest.TestCase):
    def setUp(self):
        self.session,self.frames=fixture(True)

    def test_compact_complete_domain(self):
        before=copy.deepcopy(self.frames)
        result=inspect(self.session,self.frames,True)
        self.assertEqual(result['observations'],426)
        self.assertEqual(result['returned_xyz_loads'],1278)
        self.assertEqual(len(records(self.session,'FC067_XYZ_OP')),27)
        self.assertEqual({r['vertex'] for r in result['samples']},set(range(4,146)))
        self.assertFalse(result['original_ram_producer_proven'])
        self.assertFalse(result['usable_camera_contract'])
        self.assertEqual(self.frames,before)

    def test_strip_header_controls(self):
        for old,new,reason in (
                ('pc=8c03ccee','pc=8c03ccec','strip-end header store'),
                ('offset=0 size=4 expected=f0000000','offset=4 size=4 expected=f0000000','strip-end header store'),
                ('actual=f0000000','actual=e0000000','strip-end header store'),
                ('event=8 cycle=241','event=9 cycle=241','store event sequence')):
            self.assertIn(old,self.session)
            with self.subTest(old=old),self.assertRaisesRegex(ValueError,reason):
                inspect(self.session.replace(old,new,1),self.frames,True)

    def test_restart_requires_header_on_actual_boundary(self):
        frames=copy.deepcopy(self.frames)
        # Inserting a restart after the first vertex creates a newly required
        # header store; update indices honestly so packet association still holds.
        frames[0][0]['indices'].insert(1,0xffffffff)
        frames[0][0]['draws'][0]['count']+=1
        lines=[]
        for line in self.session.splitlines():
            if 'FC067_CAMERA_PACKET_FINAL ordinal=1781 ' in line:
                row=records(line,'FC067_CAMERA_PACKET_FINAL')[0]
                line=line.replace('count=142 ','count=143 ')
                index=int(row['index'])
                if index: line=line.replace(f'index={index} ',f'index={index+1} ')
            lines.append(line)
        with self.assertRaisesRegex(ValueError,'strip-end store coverage'):
            inspect('\n'.join(lines)+'\n',frames,True)

    def test_missing_descriptor_and_copy_identity(self):
        line=next(x for x in self.session.splitlines() if 'FC067_XYZ_OP ' in x)
        for bad in (self.session.replace(line+'\n','',1),
                    self.session.replace('sample=426 expected=426','sample=426 expected=425',1),
                    self.session.replace('samples=426','samples=425',1)):
            with self.subTest(),self.assertRaises(ValueError): inspect(bad,self.frames,True)

    def test_budget_rejection_is_fail_closed(self):
        with self.assertRaisesRegex(ValueError,'live source observer rejected'):
            inspect(self.session+'FC067_DRAW_REJECT reason=8MiB-log-budget\n',self.frames,True)


if __name__=='__main__': unittest.main()
