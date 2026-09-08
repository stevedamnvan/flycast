"""Bound full-draw source-record reuse; never infer an original transform."""
from camera_source_inspect import inspect_capture
from transform_store_inspect import require
import re


def group_records(samples):
    require(len(samples)==426,'observation bound')
    frames=[]
    for frame in range(3):
        rows=samples[frame*142:(frame+1)*142]
        require([r['slot'] for r in rows]==list(range(142))
                and [r['vertex'] for r in rows]==list(range(4,146)), 'vertex domain')
        producer=rows[0]['producer'];generation=rows[0]['generation']
        require(producer['ordinal']==1781+frame and generation>0,'record frame identity')
        groups={}
        for row in rows:
            require(row['producer']==producer and row['generation']==generation
                    and row['draw']==1,'record ownership')
            addresses=row['position_ram_addresses'];words=row['position_words']
            require(len(addresses)==3 and len(words)==3,'record extent')
            require(all(isinstance(a,str) and re.fullmatch('[0-9a-f]{8}',a)
                        for a in addresses),'address encoding')
            addresses=[int(a,16) for a in addresses]
            base=addresses[0]
            require(isinstance(base,int) and base%4==0
                    and addresses==[base,base+4,base+8], 'XYZ address sequence')
            require(all(isinstance(w,int) and 0<=w<2**32 for w in words),'record word width')
            group=groups.setdefault(base,dict(base=base,words=list(words),consumers=[]))
            require(group['words']==words,'record changed between consumers')
            group['consumers'].append(dict(slot=row['slot'],vertex=row['vertex']))
        require(len(groups)==48,'distinct record coverage')
        bases=sorted(groups)
        require(all(bases[i]+12<=bases[i+1] for i in range(47)), 'overlapping XYZ records')
        frames.append(dict(producer=dict(producer),generation=generation,
                           records=[groups[b] for b in bases]))
    require(len({f['generation'] for f in frames})==3,'generation reuse')
    return dict(frames=frames,record_instances=144,downstream_vertices=426,
                original_transforms_proven=False,usable_camera_contract=False)


if __name__=='__main__':
    import argparse
    import json
    from pathlib import Path
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture',type=Path,required=True)
    args=parser.parse_args()
    print(json.dumps(group_records(inspect_capture(args.capture,True)['samples']),sort_keys=True))
