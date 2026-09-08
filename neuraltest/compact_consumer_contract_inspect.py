"""Run the built C++ contract and independently verify emitted tape bytes."""
import argparse
from pathlib import Path
import subprocess
from compact_consumer_inspect import decode,encode
from test_compact_consumer_inspect import record
from transform_store_inspect import require


def inspect(executable):
    result=subprocess.run([str(executable.resolve()),'--emit'],capture_output=True,timeout=30,check=True)
    require(result.stdout==encode([record()]),'C++/Python encoded bytes differ')
    require(decode(result.stdout)['records']==[record()],'decoded independent golden differs')
    return dict(bytes=len(result.stdout),cross_language_exact=True,transport_only=True,
                cpp_report=result.stderr.decode('utf-8').strip(),live_capture_proven=False)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable',type=Path)
    import json
    print(json.dumps(inspect(parser.parse_args().executable),sort_keys=True))
