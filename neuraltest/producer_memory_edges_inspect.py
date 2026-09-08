"""Exact executed-store to later executed-load edges, not world-space meaning."""
import argparse
import json
import re
from pathlib import Path
from producer_matrix_inspect import inspect as arithmetic
from transform_ledger_inspect import decode
from transform_store_inspect import require


def inspect(text):
    arithmetic(text,3,True,True)
    ops,memory,pending,counts={},{},{},{}
    step=descriptor=None
    matched=outside=unattributed=0
    for line in text.splitlines():
        m=re.search(r'FC067_PRODUCER_(OP|ENTRY|EXIT|MEMORY|STORE|READ|VALUE) (.*)',line)
        if not m: continue
        kind=m[1];r=dict(re.findall(r'(\w+)=([^ ]+)',m[2]))
        if kind=='OP': ops[int(r['descriptor']),int(r['index'])]=r
        elif kind=='ENTRY':
            require(not pending,'unfinished load edge')
            step,descriptor=int(r['step']),int(r['descriptor'])
        elif kind=='EXIT':
            require(not pending,'unfinished load edge');step=None
        elif kind=='MEMORY' and r['kind']=='write':
            address,size,value=int(r['address'],16),int(r['size']),int(r['value'],16)
            for i in range(size):memory[address+i]=[(value>>(8*i))&255,r['pc'],step,None]
        elif kind=='STORE':
            address,size,value=int(r['address'],16),int(r['size']),int(r['value'],16)
            found=[memory.get(address+i) for i in range(size)]
            if all(w is None for w in found):continue
            pc=ops[descriptor,int(r['index'])]['pc']
            require(all(w is not None and w[:3]==[(value>>(8*i))&255,pc,step] for i,w in enumerate(found)), 'store event/memory identity')
            for w in found:w[3]=(step,int(r['index']))
        elif kind=='READ':
            index=int(r['index']);address,size=int(r['address'],16),int(r['size'])
            found=[memory.get(address+i) for i in range(size)]
            if all(w is None for w in found):outside+=1;continue
            require(all(w is not None for w in found),'partial load edge')
            pending[index]=(sum(w[0]<<(8*i) for i,w in enumerate(found)), found)
        elif kind=='VALUE' and int(r['index']) in pending:
            expected,found=pending.pop(int(r['index']))
            require(int(r['part'])==0 and int(r['value'],16)==expected,'load edge value')
            origins={w[3] for w in found}
            if None in origins:unattributed+=1;continue
            require(len(origins)==1,'mixed executed writers')
            writer=found[0][1];reader=ops[descriptor,int(r['index'])]['pc']
            key=writer+'->'+reader;counts[key]=counts.get(key,0)+1;matched+=1
    require(not pending,'load edge completion')
    return dict(executed_store_load_edges=matched,reads_outside_memory_domain=outside,
                reads_without_executed_store_identity=unattributed,edge_counts=counts,
                complete_transform_ancestry=False)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('ledger',type=Path)
    a=p.parse_args();print(json.dumps(inspect(decode(a.ledger.read_bytes())),sort_keys=True))
