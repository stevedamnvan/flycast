"""Check bounded alternate-source diagnostics, not original-transform lineage."""
import re
from collections import Counter
from transform_store_inspect import load_session, require


def rows(text, marker):
    result={}
    for line in text.splitlines():
        if marker not in line:
            continue
        r=dict(re.findall(r'(\w+)=([^ ]+)',line))
        key=(int(r['ordinal']),int(r['offset']))
        require(key not in result,'duplicate packet identity')
        require(1781<=key[0]<=1783 and 0<=key[1]<32768*32 and key[1]%32==0,'packet bounds')
        result[key]=r
        require(len(result)<=98304,'record count')
    return result


def inspect(candidate, reference, count=10212):
    allowed=('FC067_ALT_REJECT','FC067_ALT_WRITER_REJECT','FC067_ALT_OP_REJECT','FC067_MISSING_REJECT')
    for text in (candidate,reference):
        require(len(text.encode('utf-8'))<=16*1024*1024,'session byte bound')
        require(not any('REJECT' in l and not any(m in l for m in allowed) for l in text.splitlines()),'diagnostic rejected')
    packets=rows(candidate,'FC067_ALT_REJECT')
    expected=rows(reference,'FC067_ALT_REJECT')
    writers=rows(candidate,'FC067_ALT_WRITER_REJECT')
    require(len(packets)==count and packets.keys()==expected.keys()==writers.keys(),'complete packet set')
    require(Counter(k[0] for k in packets)=={1781:count//3,1782:count//3,1783:count//3},'frame coverage')
    bases=set()
    for key,p in packets.items():
        ref=expected[key];w=writers[key]
        for field in ('generation','context','path','address','words','executed_sq_pc','executed_sq_address'):
            require(p[field]==ref[field],f'packet {field} mismatch')
        require(p['path']=='2' and p['exact']=='1' and ref['exact']=='1','SQ copy contract')
        require(p['address']==p['executed_sq_address'] and int(p['executed_sq_pc'],16)!=0,'executed SQ identity')
        require(w['valid_mask']=='7','read/store validity')
        pcs=tuple(int(v,16) for v in w['writers'].split(','))
        xyz=tuple(int(v,16) for v in w['ram'].split(','))
        require(len(pcs)==3 and all(v!=0 for v in pcs),'writer PCs')
        require(len(xyz)==3 and xyz[0]!=0 and xyz[0]%4==0 and xyz==(xyz[0],xyz[0]+4,xyz[0]+8),'source address structure')
        bases.add(xyz[0])
    return dict(packets=count,frames=3,source_bases=len(bases),packet_parity=True,
                runtime_diagnostic_consistency=True,original_transform_lineage=False,
                independent_absolute_source_address_proof=False)


if __name__=='__main__':
    import argparse,json
    from pathlib import Path
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('candidate',type=Path);p.add_argument('reference',type=Path)
    a=p.parse_args();print(json.dumps(inspect(load_session(a.candidate),load_session(a.reference)),sort_keys=True))
