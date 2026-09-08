"""Read-only live-backlog consistency check, not a scheduler or evidence judge."""
import argparse
import json
from pathlib import Path
import re


def require(condition, message):
    if not condition:
        raise ValueError(message)


def inspect(agents, backlog):
    require(len(agents.encode('utf-8')) <= 8192, 'agent entry point is no longer concise')
    require('docs/neural/BACKLOG.md' in agents and 'D-114' in backlog, 'missing live authority')
    require('Current authority:' not in agents and 'Current result (' not in agents, 'historical routing stack returned')
    require(backlog.count('## Active execution authority') == 1, 'multiple active queues')
    active = backlog.split('## Existing FC registry and evidence', 1)[0]
    goal = re.findall(r'^> (.+)$', active, re.M)
    require(len(goal) == 1 and len(goal[0].split()) <= 90, 'standing goal missing/duplicated/oversized')
    for term in ('BACKLOG.md', 'RTX Remix', 'DLSS 5', 'safety', 'acceptance'):
        require(term in goal[0], 'standing goal lost '+term)
    require('### Working-pipeline acceptance' in active and 'no-progress' in active,
            'missing finish or pivot contract')
    current = re.findall(r'^Current card: \*\*(.+?)\*\*\.$', active, re.M)
    camera = re.findall(r'^Usable camera contract: \*\*(pending|supported|unsupported)\*\*\.$', active, re.M)
    require(len(current) == len(camera) == 1, 'missing/duplicate live pointer or camera outcome')
    queue = active.split('### Ordered queue', 1)[1].split('### Next-card bounds', 1)[0]
    cards = {}
    for line in queue.splitlines():
        if not line.startswith('| FC-'):
            continue
        cells = [cell.strip() for cell in line.strip('|').split('|')]
        require(len(cells) == 5, 'card row shape')
        name, status, deps, acceptance, evidence = cells
        require(name not in cards and acceptance, 'duplicate/empty card')
        require(status in ('todo', 'doing', 'done') or re.fullmatch(r'blocked\(.+ -> .+\)', status), 'invalid card status')
        require(status != 'done' or evidence not in ('', 'pending'), 'done card lacks evidence')
        cards[name] = dict(status=status, deps=deps, evidence=evidence)
    require(len(cards) >= 2, 'missing queue')
    labels = {name.rsplit(' / ', 1)[-1]: name for name in cards}
    require(len(labels) == len(cards), 'ambiguous card labels')
    required = {'M2-camera', 'M1-GPU', 'M2-scene', 'M3-relighting',
                'M4-presentation', 'M4-DLSS5', 'combined hardening', 'style expansion'}
    require(required <= set(labels), 'pipeline dependency missing')
    graph = {}
    for name, card in cards.items():
        deps = [other for label, other in labels.items() if label in card['deps']]
        if 'working pipeline' in card['deps']:
            deps.append(labels['combined hardening'])
        graph[name] = deps
        if card['status'] in ('doing', 'done'):
            require(all(cards[dep]['status'] == 'done' for dep in deps), 'active/done card has unfinished dependency')
            if 'usable contract' in card['deps']:
                require(camera[0] == 'supported', 'unsupported camera cannot unlock scene integration')

    def visit(name, path):
        require(name not in path, 'cyclic dependencies')
        for dep in graph[name]:
            visit(dep, path | {name})
    for name in cards:
        visit(name, set())
    doing = [name for name, card in cards.items() if card['status'] == 'doing']
    require(len(doing) <= 1, 'more than one active card')
    require((current[0] == 'none' and not doing)
            or (doing == current and current[0] in cards), 'current pointer does not match active card')
    ready = [name for name, card in cards.items() if card['status'] == 'todo'
             and all(cards[dep]['status'] == 'done' for dep in graph[name])
             and ('usable contract' not in card['deps'] or camera[0] == 'supported')]
    return dict(current=current[0], camera_outcome=camera[0], cards=len(cards),
                independent_ready=ready, rendering_evidence_verified=False,
                app_goal_changed=False, writes_performed=False)


def inspect_repo(repo):
    return inspect((repo/'AGENTS.md').read_text(encoding='utf-8'),
                   (repo/'docs/neural/BACKLOG.md').read_text(encoding='utf-8'))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    try:
        print(json.dumps(inspect_repo(args.repo), sort_keys=True))
    except (ValueError, OSError, KeyError, IndexError) as error:
        parser.exit(1, f'Backlog contract rejected: {error}\n')
