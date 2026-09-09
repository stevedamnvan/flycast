"""Bounded process tests against the actual C++ loader, never a runtime."""
import copy
import json
from pathlib import Path
import subprocess
import tempfile


def run(executable, artifact, assets):
    source=json.loads(Path(artifact).read_text())
    def invoke(path):
        return subprocess.run([str(executable),str(path),str(assets),'.1','104'],
                              capture_output=True,text=True,timeout=30)
    positive=invoke(artifact)
    assert positive.returncode==0,positive.stderr
    mutations=[('schema',lambda a:a.update(schema='wrong')),
        ('source SHA',lambda a:a.update(git_sha='')),
        ('coordinate',lambda a:a.update(coordinate_space='pvr-projected')),
        ('source clips',lambda a:a['camera'].update(nearPlane=.1)),
        ('draw binding',lambda a:a['meshes'][0]['source_bindings'][0].update(draw=999999)),
        ('index',lambda a:a['meshes'][0]['indices'].__setitem__(0,999999)),
        ('normal',lambda a:a['meshes'][0]['vertices'][0].update(normal=[0,0,0])),
        ('palette',lambda a:a['meshes'][0]['source_bindings'][0].update(palette_hash='unproven')),
        ('filename',lambda a:next(iter(a['source_assets'].values())).update(file='../outside.dds')),
        ('hash',lambda a:next(iter(a['source_assets'].values())).update(sha256='0'*64)),
        ('omissions',lambda a:a.update(omissions=[]))]
    with tempfile.TemporaryDirectory(prefix='flycast-loader-controls-') as directory:
        candidate=Path(directory)/'candidate.json'
        for name,mutate in mutations:
            value=copy.deepcopy(source);mutate(value)
            candidate.write_text(json.dumps(value))
            result=invoke(candidate)
            assert result.returncode==1 and 'artifact rejected:' in result.stderr,(name,result)
        for bad in ('{', '['*40+'0'+']'*40):
            candidate.write_text(bad);result=invoke(candidate)
            assert result.returncode==1 and 'artifact rejected:' in result.stderr,result
    return dict(positive=1,rejected=len(mutations)+2,runtime_loaded=False)


if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('executable','artifact','assets'):parser.add_argument(name,type=Path)
    args=parser.parse_args()
    print(json.dumps(run(args.executable.resolve(),args.artifact.resolve(),args.assets.resolve())))
