"""Independent synthetic indexed-address and direct-X variant goldens."""
import re
import unittest
from initial_edge_inspect import inspect_edge as supply
from factor_edge_inspect import inspect_edge as pred
from test_initial_edge_inspect import fixture as supply_fixture
from test_factor_edge_inspect import fixture as pred_fixture


def shifted(line,ops,events):
    line=re.sub(r'index=(\d+)',lambda m:f'index={int(m[1])+ops}',line)
    return re.sub(r'event=(\d+)',lambda m:f'event={int(m[1])+events}',line)


def first_supply(word=0x3f800001):
    base=0x8ce74250;index=word&65535;index=index if index<32768 else index|0xffff0000
    shift=(index<<4)&0xffffffff;offset=shift&0xfff0;origin=base-offset
    lines=supply_fixture(identity=True,w=word).splitlines();out=[];ops=[];events=[]
    program=[('readm',0x932,2,'r0:1:1','r5:1:0','-','ic',index),
             ('shld',0x934,0,'r0:1:2','r0:1:1','r2:1:0','-',shift),
             ('and',0x936,0,'r0:1:3','r0:1:2','r1:1:0','-',offset),
             ('add',0x938,0,'r0:1:4','r0:1:3','r4:1:0','-',base)]
    event=0
    for i,(name,pc,size,rd,a,b,c,value) in enumerate(program):
        ops.append(f'FC067_SUPPLY_OP index={i} pc={0x8c03c000+pc:x} op={name} size={size} rd={rd} rd2=- rs1={a} rs2={b} rs3={c}')
        if i==0:event+=1;events.append(f'FC067_SUPPLY_READ event={event} index=0 address=8c01000c size=2')
        event+=1;events.append(f'FC067_SUPPLY_VALUE event={event} index={i} slot=0 part=0 reg=0 version={i+1} count=1 value={value:x}')
    for line in lines:
        if line.startswith('FC067_SUPPLY_ENTRY'):line=line.replace('8c03c93a','8c03c932').replace('ops=11 inputs=19','ops=15 inputs=21')
        elif line.startswith('FC067_SUPPLY_INPUT reg=0 '):
            out.extend(f'FC067_SUPPLY_INPUT reg={r} value={v:x} expected={v:x} exact=1' for r,v in ((1,0xfff0),(2,4),(4,origin)));continue
        elif line.startswith('FC067_SUPPLY_OP '):
            if ops:out.extend(ops);ops=[]
            line=shifted(line,4,0).replace('r0:1:1','r0:1:5').replace('r0:1:0','r0:1:4')
        elif line.startswith(('FC067_SUPPLY_READ ','FC067_SUPPLY_VALUE ')):
            if events:out.extend(events);events=[]
            line=shifted(line,4,5)
            if 'reg=0 version=1 ' in line:line=line.replace('reg=0 version=1 ','reg=0 version=5 ')
        elif line.startswith('FC067_SUPPLY_EXIT'):line=line.replace('events=18','events=23')
        out.append(line)
    return '\n'.join(out)+'\n'


def first_pred():
    out=[];ops=[];events=[];event=0
    program=[('mov32','9a',4,20,0x43a00000),('mov32','9c',4,21,0x43700000),
             ('mov32','9e',4,22,0x7f800000),('mov32','9e',0,0,0x8c03c9ec),('mov32','a0',0,23,0),
             ('readm','a2',4,16,0x3f800000),('add','a2',0,4,0x8ce74254)]
    for i,(name,pc,size,reg,value) in enumerate(program):
        a=f'i{value:x}' if name=='mov32' else 'r4:1:0';b='i4' if name=='add' else '-'
        ops.append(f'FC067_PRED_OP index={i} pc=8c03c9{pc} op={name} size={size} rd=r{reg}:1:1 rd2=- rs1={a} rs2={b} rs3=-')
        if name=='readm':event+=1;events.append(f'FC067_PRED_READ event={event} index={i} address=8ce74250 size=4')
        event+=1;events.append(f'FC067_PRED_VALUE event={event} index={i} slot=0 part=0 reg={reg} version=1 count=1 value={value:x}')
    for line in pred_fixture().splitlines():
        if line.startswith('FC067_PRED_ENTRY'):line=line.replace('8c03c9a4','8c03c998').replace('ops=7 inputs=2','ops=14 inputs=1')
        elif line.startswith('FC067_PRED_INPUT reg=23 '):continue
        elif line.startswith('FC067_PRED_INPUT reg=4 '):line=line.replace('8ce74254','8ce74250')
        elif line.startswith('FC067_PRED_OP '):
            if ops:out.extend(ops);ops=[]
            line=re.sub(r'r4:1:(\d+)',lambda m:f'r4:1:{int(m[1])+1}',shifted(line,7,0)).replace('r23:1:0','r23:1:1')
        elif line.startswith(('FC067_PRED_READ ','FC067_PRED_VALUE ')):
            if events:out.extend(events);events=[]
            line=shifted(line,7,8)
            if 'reg=4 ' in line:line=re.sub(r'version=(\d+)',lambda m:f'version={int(m[1])+1}',line)
        elif line.startswith(('FC067_PRED_EXIT ','FC067_PRED_EDGE ')):line=line.replace('events=9','events=17')+' x=3f800000'
        out.append(line)
    return '\n'.join(out)+'\n'


class FirstVariantTests(unittest.TestCase):
    def test_signed_index_and_nonunit_w(self):
        for word in (0x3f800001,0x3f808001):
            result=supply(first_supply(word),first_record=True)
            self.assertEqual(result['source_point_words'][3],word)
            self.assertEqual(result['transformed_words'][3],word)
    def test_index_mismatch(self):
        text=first_supply()
        for old,new in [('rs3=ic','rs3=i8'),('value=fff0 expected=fff0','value=ffff expected=ffff'),('version=2 count=1 value=10','version=2 count=1 value=11')]:
            self.assertIn(old,text)
            with self.subTest(old=old),self.assertRaises(ValueError):supply(text.replace(old,new,1),first_record=True)
    def test_direct_x(self):
        result=pred(first_pred(),first_record=True)
        self.assertEqual(result['source_loads'][0],(0x8ce74250,0x3f800000))
        self.assertEqual(result['factor_word'],0x3e800000)
    def test_wrong_direct_x_and_center(self):
        text=first_pred()
        for old,new in [('x=3f800000','x=40000000'),('rs1=i43a00000','rs1=i43a00001')]:
            with self.subTest(old=old),self.assertRaises(ValueError):pred(text.replace(old,new,1),first_record=True)


if __name__=='__main__':unittest.main()
