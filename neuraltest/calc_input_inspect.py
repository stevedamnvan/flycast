"""Latest observed writers of CALC reads; untracked domains remain explicit."""
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
    checked=untracked=0
    for line in text.splitlines():
        m=re.search(r'FC067_PRODUCER_(OP|ENTRY|MEMORY|READ|VALUE) (.*)',line)
        if not m: continue
        kind=m[1];r=dict(re.findall(r'(\w+)=([^ ]+)',m[2]))
        if kind=='OP': ops[int(r['descriptor']),int(r['index'])]=r
        elif kind=='ENTRY':
            require(not pending,'unconsumed CALC input')
            descriptor=int(r['descriptor'])
        elif kind=='MEMORY' and r['kind']=='write':
            address,size,value=int(r['address'],16),int(r['size']),int(r['value'],16)
            for i in range(size): memory[address+i]=(r['pc'],(value>>(8*i))&255)
        elif kind=='READ':
            index=int(r['index']);op=ops[descriptor,index]
            if not 0x8c03c998<=int(op['pc'],16)<0x8c03c9e0: continue
            address,size=int(r['address'],16),int(r['size'])
            require(size==4,'CALC input width')
            words=[memory.get(address+i) for i in range(size)]
            if all(w is None for w in words):
                untracked+=1;continue
            require(all(w is not None for w in words),'partial CALC input ownership')
            signature=op['pc']+':'+','.join(sorted({w[0] for w in words}))
            counts[signature]=counts.get(signature,0)+1
            pending[index]=sum(w[1]<<(8*i) for i,w in enumerate(words))
        elif kind=='VALUE' and int(r['index']) in pending:
            require(int(r['part'])==0 and int(r['value'],16)==pending.pop(int(r['index'])),'CALC input writer/value mismatch')
            checked+=1
    require(not pending and checked>0,'CALC input completion')
    return dict(verified_input_words=checked,untracked_input_words=untracked,
                latest_writer_counts=counts,initial_transform_ancestry_verified=False)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('ledger',type=Path)
    args=parser.parse_args();print(json.dumps(inspect(decode(args.ledger.read_bytes())),sort_keys=True))
