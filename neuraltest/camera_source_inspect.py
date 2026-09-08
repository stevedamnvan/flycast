"""Validate bounded actual indexed RAM -> SQ -> decoded TA XYZ, not original transforms."""
import argparse
import json
from pathlib import Path
import re

from camera_packet_inspect import inspect as inspect_packets, ORDINALS
from transform_span_inspect import records
from transform_store_inspect import load_session, require
from xyz_operand_inspect import inspect_operands

OFFSETS = (32,64,96,4608,4640,4672)
NAMES = ('readm add readm add readm add setge and and shld shld add add '
         'readm readm readm writem writem writem readm readm writem writem '
         'writem shl jcond writem').split()
PCS = [0x8c03cc68,0x8c03cc68,0x8c03cc6a,0x8c03cc6a,0x8c03cc6c,0x8c03cc6c]
PCS += list(range(0x8c03cc6e,0x8c03cc98,2))


def inspect(session, frames):
    mapped = inspect_packets(session, frames)
    require('FC067_CS_REJECT' not in session and 'FC067_XYZ_REJECT' not in session,
            'live source observer rejected')
    def rows(kind, count):
        result = records(session,'FC067_CS_'+kind)
        require(len(result)==count, kind+' coverage')
        return result
    begins, copies, links = rows('BEGIN',18), rows('COPY',18), rows('LINK',18)
    flushes, word_rows, frame_rows = rows('FLUSH',18), rows('WORD',144), rows('FRAME',3)
    stores = rows('STORE',126)
    ends = rows('END',1)
    require(ends[0] == dict(reason='reset-after-complete',samples='18'), 'completion boundary')
    for kind, items in [('BEGIN',begins),('COPY',copies),('LINK',links),('FLUSH',flushes)]:
        require([int(r['sample']) for r in items]==list(range(1,19)), kind+' sample order')
    require(len(records(session,'FC067_XYZ_ENTRY'))==18
            and len(records(session,'FC067_XYZ_EXIT'))==18, 'operand invocation count')
    result = []
    generations = []
    sq_buffer = None
    for frame_index, row in enumerate(frame_rows):
        packet = mapped['packets'][frame_index*6]
        producer = packet['producer']
        require(all(int(row[k])==producer[k] for k in ('epoch','ordinal','cycle'))
                and row['selected']=='63' and int(row['total'])==(frame_index+1)*6,
                'actual producer coverage')
    begin_positions = [m.start() for m in re.finditer('FC067_CS_BEGIN ',session)]
    for i,(begin,copy,link,flush,packet) in enumerate(zip(begins,copies,links,flushes,mapped['packets'])):
        sample = str(i+1)
        producer = packet['producer']
        slot = i%6
        require(int(begin['slot'])==slot and int(begin['offset'])==OFFSETS[slot]
                and begin['context']==packet['child'] and int(begin['offset'])==packet['ta_offset'],
                'selected packet identity')
        for key in ('epoch','ordinal'):
            require(int(begin[key+'_hint'])==int(copy[key+'_hint'])==int(link[key])==producer[key],
                    'hint differs from actual producer')
        require(begin['generation']==copy['generation']==link['generation']==link['current_generation']
                and int(begin['generation'])>0, 'context generation mismatch')
        if slot==0:
            generations.append(int(begin['generation']))
        require(int(begin['generation'])==generations[-1], 'within-frame context generation')
        require(copy['sample']==copy['expected']==link['sample']==sample, 'expected copy identity')
        require(copy['context']==link['child']==begin['context']
                and copy['offset']==link['offset']==begin['offset']
                and copy['sq']==begin['sq'] and int(link['slot'])==slot, 'copy ownership')
        require(copy['destination']==link['destination']==link['packet']
                and int(copy['destination'],16)>0 and int(copy['source'],16)>0
                and copy['exact']==link['exact']=='1', 'actual copy decoder pointer')
        sq=int(begin['sq'],16)
        require(sq>>26==0x38 and sq&31==0, 'SQ address shape')
        base=int(copy['source'],16)-(sq&32)
        if sq_buffer is None: sq_buffer=base
        require(base==sq_buffer and base&31==0, 'physical SQ buffer identity')
        cycle=int(begin['cycle'])
        require(cycle<=int(copy['cycle'])==int(flush['cycle'])<=producer['cycle']
                and int(copy['cycle'])-cycle<=4000000, 'copy clock bounds')
        own_words=[r for r in word_rows if r['sample']==sample]
        require([int(r['index']) for r in own_words]==list(range(8)), 'copy word coverage')
        actual=[int(r['after'],16) for r in own_words]
        require(all(r['before']==r['after'] for r in own_words)
                and actual==packet['packet_words'], 'copied bytes differ from decoded packet')
        own_stores=[r for r in stores if r['sample']==sample]
        require([int(r['event']) for r in own_stores]==list(range(1,8)), 'store event sequence')
        shadow=[None]*12
        writers=[None]*12
        last=cycle
        for row in own_stores:
            address=int(row['address'],16); offset=int(row['offset']); size=int(row['size'])
            require(address>>26==0x38 and address&32==sq&32 and offset==address&31
                    and size==4 and offset+size<=32, 'physical SQ slot or store extent')
            require(last<=int(row['cycle'])<=int(copy['cycle']), 'store clock order')
            last=int(row['cycle'])
            require(row['expected']==row['actual'] and row['exact']=='1', 'actual SQ store mismatch')
            value=int(row['actual'],16)
            require(0<=value<1<<32, 'store width')
            for byte in range(size):
                if 4<=offset+byte<16:
                    b=offset+byte-4
                    shadow[b]=(value>>(8*byte))&255; writers[b]=int(row['pc'],16)
        expected_writers=[pc for pc in (0x8c03cc82,0x8c03cc84,0x8c03cc86) for _ in range(4)]
        require(writers==expected_writers and bytes(shadow)==b''.join(w.to_bytes(4,'little') for w in actual[1:4]),
                'XYZ byte last-writer coverage')
        require(flush['coverage']=='fff' and flush['events']=='7'
                and flush['writers']=='8c03cc82,8c03cc84,8c03cc86', 'flush coverage')
        section=session[begin_positions[i]:begin_positions[i+1] if i<17 else len(session)]
        ops=records(section,'FC067_XYZ_OP')
        require([r['op'] for r in ops]==NAMES and [int(r['pc'],16) for r in ops]==PCS,
                'known gather operation shape')
        target=dict(step=sample,block='8c03cc68',cycle=begin['cycle'],context=begin['context'],
                    ta_offset=begin['offset'],sq=begin['sq'])
        operands=inspect_operands(section,target,'FC067_CS_STORE','FC067_CS_FLUSH')
        require(operands['position_words']==actual[1:4], 'returned XYZ differs from copied packet')
        for first,second in [('FC067_CS_FLUSH','FC067_CS_COPY'),('FC067_CS_COPY','FC067_CS_WORD')]:
            require(section.index(first+' ')<section.index(second+' '), 'copy event ordering')
        # LINK may occur after another source observation but must follow its own actual copy.
        def position(tag):
            return next(m.start() for m in re.finditer(tag+' ',session)
                        if records(session[m.start():].splitlines()[0],tag)[0]['sample']==sample)
        require(position('FC067_CS_COPY')<position('FC067_CS_LINK'), 'decode precedes actual copy')
        result.append(dict(sample=i+1,producer=producer,draw=packet['draw'],vertex=packet['vertex'],
                           slot=slot,ta_offset=OFFSETS[slot],generation=int(begin['generation']),
                           position_ram_addresses=operands['position_ram_addresses'],position_words=actual[1:4]))
    require(generations==sorted(set(generations)), 'reused context generation')
    require(session.rfind('FC067_CS_LINK ')<session.index('FC067_CS_END '), 'end before decoder coverage')
    return dict(samples=result,observations=18,returned_xyz_loads=54,source_to_packet_proven=True,
                original_ram_producer_proven=False,usable_camera_contract=False,production_enabled=False)


def inspect_capture(path):
    frames=[]
    for ordinal in ORDINALS:
        frame=path/f'frame-{ordinal+1:06d}'
        def read(name):
            p=frame/name
            require(p.stat().st_size<=8*1024*1024,'JSON bound')
            return json.loads(p.read_text(encoding='utf-8'))
        frames.append((read('pvr-scene.json'),read('manifest.json')))
    return inspect(load_session(path),frames)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture',type=Path,required=True)
    args=parser.parse_args()
    try:
        print(json.dumps(inspect_capture(args.capture),sort_keys=True))
    except (ValueError,OSError,KeyError,IndexError,TypeError,StopIteration) as error:
        parser.exit(1,f'Camera source rejected: {error}\n')
