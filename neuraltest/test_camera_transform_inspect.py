"""Independent aggregate protocol fixture; arithmetic seams have separate real-oracle tests.

Mock only the individually tested arithmetic decoders. This exercises the
composition, record ownership and coverage checks without replaying game data.
"""
import copy
import unittest
from unittest.mock import patch
import camera_transform_inspect as checker

BASES=[0x8ce74250,0x8ce74230,0x8ce74240,0x8ce6e460,0x8ce6e470,0x8ce6e480]
INITIAL=[0x40800000,0x40a00000,0x40c00000]
ADDED=[0x40a00000,0x40e00000,0x41100000]
FINAL=[0x43938000,0x436b0000,0x3e800000]


def fixture():
    lines=[]; samples=[]; count=0
    for frame in range(3):
        gen=1790+frame;ordinal=1781+frame;cycle=1000+frame*1000
        lines.append(f'FC067_CT_BEGIN generation={gen} ordinal_hint={ordinal} cycle={cycle}')
        for slot,base in enumerate(BASES):
            sample=frame*6+slot+1
            samples.append(dict(generation=gen,slot=slot,producer=dict(ordinal=ordinal,cycle=cycle+900),
                                position_ram_addresses=[f'{base:x}'],position_words=FINAL[:],sample=sample,draw=slot,vertex=slot))
            if slot==3: continue
            extra=slot in (4,5);pre=ADDED if extra else INITIAL;n=9 if extra else 6
            def header(kind):
                lines.append(f'FC067_CT_BLOCK generation={gen} ordinal_hint={ordinal} slot={slot} kind={kind} base={base:x} cycle={cycle+1}')
                lines.append(f'FC067_{checker.TAGS[kind]}_ENTRY cycle={cycle+1}')
            def writes(kind,values,start,pc,tag=None):
                for j,value in enumerate(reversed(values)):
                    lines.append(f'FC067_CT_WRITE generation={gen} slot={slot} event={start+j} kind={kind} address={base+8-4*j:x} pc={pc+2*j:x} source={value:x} actual={value:x} cycle={cycle+1}')
                    if tag: lines.append(f'FC067_{tag}_STORE address={base+8-4*j:x} pc={pc+2*j:x} value={value:x}')
            header(0);header(1);writes(1,INITIAL,1,0x8c03c94e,'INITIAL')
            if extra:
                writes(4,ADDED,4,0x8c03c97c)
                lines.extend(['FC067_EXTRA_FILTER checks=1',f'FC067_EXTRA_SELECTION generation={gen} slot={slot} target={base+8:x}',
                              'FC067_EXTRA_EXIT events=42',''])
            count+=1
            lines.append(f'FC067_X_LOAD slot={slot} generation={gen} pc=8c03c9d6 address={base:x} value={pre[0]:x} expected={pre[0]:x} writer={6 if extra else 3} count={count}')
            lines.append(f'FC067_X_EDGE slot={slot} generation={gen} block=8c03c9a4 pointer={base+4:x} value={pre[0]:x} expected={pre[0]:x} exact=1')
            header(2);lines.append(f'FC067_PRED_EDGE x={pre[0]:x}')
            header(3);writes(3,FINAL,n-2,0x8c03c9ca,'CALC')
            for j,value in enumerate(FINAL):
                lines.append(f'FC067_CT_GATHER sample={sample} generation={gen} slot={slot} component={j} address={base+4*j:x} value={value:x} expected={value:x} events={n} writers='+','.join([str(n-j)]*4))
        lines.append(f'FC067_CT_FRAME generation={gen} ordinal={ordinal} complete=55 counts='+','.join([str((frame+1)*5)]*4))
    return '\n'.join(lines)+'\n',dict(samples=samples)


def run(text,source):
    def pred(section,base,*args):
        pre=ADDED if base in BASES[4:] else INITIAL
        return dict(factor_word=FINAL[2],source_loads=[(base+4,pre[1]),(base+8,pre[2])],written_registers=[4,17,18,19,68])
    def calc(section,base,*args):
        pre=ADDED if base in BASES[4:] else INITIAL
        return dict(final_words_xyz=FINAL[:],live_input_words={16:pre[0],17:pre[1],19:FINAL[2]})
    def extra(section,target,*args):
        return dict(loads=[(0,0)]*5+[(target-8+4*i,v) for i,v in enumerate(INITIAL)],
                    stores=[(target-4*i,v) for i,v in enumerate(reversed(ADDED))])
    with patch.object(checker,'inspect_supply',return_value=dict(transformed_words=INITIAL[:],source_point_words=[],xf_matrix_words=[],source_loads=[])), \
         patch.object(checker,'inspect_initial',return_value=dict(initial_xyz_words=INITIAL[:])), \
         patch.object(checker,'inspect_pred',side_effect=pred),patch.object(checker,'inspect_calc',side_effect=calc), \
         patch.object(checker,'inspect_extra',side_effect=extra):
        return checker.inspect(text,source)


class AggregateTests(unittest.TestCase):
    def test_protocol_golden(self):
        result=run(*fixture())
        self.assertEqual(result['observations'],15)
        self.assertEqual(result['unsupported_slots'],[3])
        self.assertTrue(result['initial_x_reload_proven'])
        self.assertFalse(result['world_camera_recovered'])
    def test_protocol_negatives(self):
        text,source=fixture()
        for old,new in [('complete=55','complete=63'),('writer=3 count=1','writer=2 count=1'),
                        ('FC067_X_EDGE slot=0 generation=1790','FC067_X_EDGE slot=0 generation=1791'),
                        ('events=6 writers=6,6,6,6','events=6 writers=5,5,5,5'),
                        ('kind=1 base=8ce74250','kind=1 base=8ce74254')]:
            with self.subTest(old=old):
                self.assertIn(old,text)
                with self.assertRaises(ValueError): run(text.replace(old,new,1),source)
        changed=copy.deepcopy(source);changed['samples'][0]['position_words'][0]^=1
        with self.assertRaises(ValueError):run(text,changed)


if __name__=='__main__':unittest.main()
