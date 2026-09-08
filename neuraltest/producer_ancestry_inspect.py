"""Observed matrix contribution sets through RAM; no coordinate-space assertion."""
import argparse
from collections import Counter
import json
import re
from pathlib import Path
from producer_memory_edges_inspect import inspect as memory_edges
from producer_record_inspect import inspect as records
from producer_continuity_inspect import registers
from transform_ledger_inspect import decode
from transform_store_inspect import require
from ordered_float_expression import evaluate

UNKNOWN=(-1,'unknown')


def inspect(text, details=False):
    evidence=memory_edges(text)
    _, completed=records(text,True)
    ops={}
    for line in text.splitlines():
        if 'FC067_PRODUCER_OP ' in line:
            r=dict(re.findall(r'(\w+)=([^ ]+)',line));ops[int(r['descriptor']),int(r['index'])]=r
    values,origins,memory,stored,loads={},{},{},{},{}
    expressions,expression_memory,expression_stores={},{},{}
    dispatch=None;transform_inputs={}
    for line in text.splitlines():
        m=re.search(r'FC067_PRODUCER_(ENTRY|INPUT|MEMORY|STORE|READ|VALUE) (.*)',line)
        if not m:continue
        kind=m[1];r=dict(re.findall(r'(\w+)=([^ ]+)',m[2]))
        if kind=='ENTRY':
            current=int(r['dispatch'])
            require(dispatch is None or current>dispatch,'dispatch chronology')
            if dispatch is None or current!=dispatch+1:values,origins,expressions={},{},{}
            dispatch,step,descriptor=current,int(r['step']),int(r['descriptor'])
            operation_origins={};loads={};expression_ops={};expression_loads={}
        elif kind=='INPUT':
            reg,value=int(r['reg']),int(r['value'],16)
            if reg in values:require(values[reg]==value,'ancestry register continuity')
            values[reg]=value
            origins.setdefault(reg,frozenset({UNKNOWN}) if reg in (16,17,18,19,22) else frozenset())
            expressions.setdefault(reg,None if reg in (16,17,18,19,22) else ('literal',value))
        elif kind=='MEMORY' and r['kind']=='write':
            address,size=int(r['address'],16),int(r['size'])
            for i in range(size):
                memory[address+i]=frozenset({UNKNOWN});expression_memory[address+i]=None
        elif kind in ('STORE','READ','VALUE'):
            index=int(r['index']);op=ops[descriptor,index]
            if kind=='READ':
                address,size=int(r['address'],16),int(r['size'])
                loads[index]=frozenset().union(*(memory.get(address+i,frozenset({UNKNOWN})) for i in range(size)))
                nodes=[expression_memory.get(address+i) for i in range(size)]
                expression_loads[index]=nodes[0] if size==4 and all(n==nodes[0] for n in nodes) else None
                continue
            if kind=='STORE':
                tag=frozenset().union(*(origins.get(reg,frozenset({UNKNOWN})) for reg in registers(op['rs2'])))
                address,size=int(r['address'],16),int(r['size'])
                stored[step,address]=tag
                source=registers(op['rs2'])
                expression_stores[step,address]=expressions.get(source[0]) if len(source)==1 and size==4 else None
                # Only publish provenance in the independently observed RAM domain.
                for i in range(size):
                    if address+i in memory:
                        memory[address+i]=tag;expression_memory[address+i]=expression_stores[step,address]
                continue
            if index not in operation_origins:
                if op['op']=='ftrv':
                    tag=frozenset({(step,op['pc'])})
                    if details:
                        transform_inputs[step,op['pc']]=dict(
                            point_words=[values[reg] for reg in registers(op['rs1'])],
                            matrix_words=[values[reg] for reg in registers(op['rs2'])])
                elif op['op']=='readm':tag=loads[index]
                else:
                    source=[reg for field in ('rs1','rs2','rs3') for reg in registers(op[field])]
                    tag=frozenset().union(*(origins.get(reg,frozenset({UNKNOWN})) for reg in source))
                operation_origins[index]=tag
            dest=registers(op['rd'] if int(r['part'])>>16==0 else op['rd2'])
            part=int(r['part'])&65535;value=int(r['value'],16)
            if index not in expression_ops:
                def expr(token):
                    if token.startswith('i'):return ('literal',int(token[1:],16))
                    source=registers(token)
                    return expressions.get(source[0]) if len(source)==1 else None
                name=op['op']
                if name in ('fadd','fmul','fdiv'):
                    a,b=expr(op['rs1']),expr(op['rs2'])
                    expression_ops[index]=(name,a,b) if a is not None and b is not None else None
                elif name=='readm':expression_ops[index]=expression_loads[index]
                elif name=='mov32':expression_ops[index]=expr(op['rs1'])
                else:expression_ops[index]=None
            for offset in range(len(dest) if len(dest)<=2 else 1):
                values[dest[part+offset]]=(value>>(32*offset))&0xffffffff
                origins[dest[part+offset]]=operation_origins[index]
                expressions[dest[part+offset]]=(('verified-ftrv',step,part+offset,(value>>(32*offset))&0xffffffff)
                    if op['op']=='ftrv' else expression_ops[index])
    linked=[];unknown=[];histogram=Counter()
    for record in completed:
        key=record['step'],record['base']
        tags=[stored[key[0],key[1]+4*i] for i in range(3)]
        if tags[0] and UNKNOWN not in tags[0] and tags[0]==tags[1]==tags[2]:
            linked.append((key,sorted(tags[0])));histogram[record['family'],len(tags[0])]+=1
        else:unknown.append(key)
    evidence.update(records_with_contribution_sets=len(linked),records_without_complete_sets=len(unknown),
                    contribution_histogram={f'{family}:{count}':n for (family,count),n in histogram.items()},
                    world_camera_recovered=False)
    exact=0
    exact_ids=[]
    exact_nodes=[]
    for record in completed:
        nodes=[expression_stores[record['step'],record['base']+4*i] for i in range(3)]
        if all(node is not None for node in nodes):
            require([evaluate(node) for node in nodes]==record['xyz'],'ordered expression result')
            exact+=1
            exact_ids.append((record['step'],record['base']))
            if details:exact_nodes.append(((record['step'],record['base']),nodes))
    evidence['records_with_exact_ordered_expressions']=exact
    if details:evidence.update(record_contributions=linked,unproven_records=unknown,exact_expression_ids=exact_ids,exact_expression_nodes=exact_nodes,
                               transform_inputs=list(transform_inputs.items()))
    return evidence


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('ledger',type=Path)
    a=p.parse_args();print(json.dumps(inspect(decode(a.ledger.read_bytes())),sort_keys=True))
