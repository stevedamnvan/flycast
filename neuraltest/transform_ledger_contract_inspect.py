"""Independently verify emitted single-thread and concurrent C++ ledger bytes."""
import argparse
import json
from pathlib import Path
import subprocess
from transform_ledger_inspect import decode
from transform_store_inspect import require


def check_concurrent(data):
    lines=decode(data).splitlines();require(len(lines)==16000,'concurrent event coverage')
    seen={i:[] for i in range(8)}
    for line in lines:
        parts=line.split();require(len(parts)==2,'concurrent event shape')
        fields=dict(part.split('=',1) for part in parts)
        require(set(fields)=={'writer','event'},'concurrent fields')
        writer,event=int(fields['writer']),int(fields['event'])
        require(writer in seen,'concurrent writer');seen[writer].append(event)
    require(all(events==list(range(2000)) for events in seen.values()),'concurrent ordering/identity')


def inspect(executable):
    def run(mode):return subprocess.run([str(executable.resolve()),mode],capture_output=True,check=True,timeout=30)
    golden=run('--emit');require(decode(golden.stdout)=='FC067_CT_BLOCK generation=10 slot=920 kind=0\n'*1000,'single-thread golden')
    for _ in range(5):check_concurrent(run('--concurrent').stdout)
    return dict(golden_compressed_bytes=len(golden.stdout),concurrent_trials=5,events_per_trial=16000,
                per_writer_order_exact=True,gameplay_proven=False)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('executable',type=Path)
    print(json.dumps(inspect(parser.parse_args().executable),sort_keys=True))
