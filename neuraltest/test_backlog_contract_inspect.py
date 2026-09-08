import unittest

from backlog_contract_inspect import inspect


AGENTS = 'Read docs/neural/BACKLOG.md. D-114 defines the autonomous loop.'


def fixture():
    rows = [
        ('M2-camera', 'doing', 'LOG #175'),
        ('M1-GPU', 'todo', 'public adapter'),
        ('M2-scene', 'todo', 'M2-camera usable contract'),
        ('M3-relighting', 'todo', 'M1-GPU, M2-scene'),
        ('M4-presentation', 'todo', 'M3-relighting'),
        ('M4-DLSS5', 'todo', 'M4-presentation'),
        ('combined hardening', 'todo', 'M4-DLSS5'),
        ('style expansion', 'todo', 'working pipeline')]
    return (
        '## Active execution authority\nD-114\n'
        '> Follow BACKLOG.md to working RTX Remix + DLSS 5 with safety and acceptance.\n'
        '### Ordered queue\nCurrent card: **FC-067 / M2-camera**.\n'
        'Usable camera contract: **pending**.\n'
        + '\n'.join(f'| FC-067 / {name} | {status} | {deps} | Bound work; test it. | pending |'
                    for name, status, deps in rows)
        + '\n### Next-card bounds\nFixed bounds and no-progress rule.\n'
        '### Working-pipeline acceptance\nActual moving game proof.\n'
        '## Existing FC registry and evidence\nHistorical rows here.\n')


class BacklogContractTests(unittest.TestCase):
    def reject(self, backlog, agents=AGENTS):
        with self.assertRaises(ValueError):
            inspect(agents, backlog)

    def test_current_and_independent_ready(self):
        result = inspect(AGENTS, fixture())
        self.assertEqual(result['current'], 'FC-067 / M2-camera')
        self.assertEqual(result['independent_ready'], ['FC-067 / M1-GPU'])
        self.assertEqual(result['cards'], 8)
        for flag in ('rendering_evidence_verified', 'app_goal_changed', 'writes_performed'):
            self.assertIs(result[flag], False)

    def test_duplicate_or_stale_current_rejected(self):
        base = fixture()
        for bad in (
            base.replace('### Ordered queue', 'Current card: **FC-067 / M1-GPU**.\n### Ordered queue'),
            base.replace('M1-GPU | todo', 'M1-GPU | doing'),
            base.replace('Current card: **FC-067 / M2-camera**.', 'Current card: **none**.'),
            base.replace('### Next-card bounds', '## Active execution authority\n### Next-card bounds')):
            with self.subTest(bad=bad):
                self.reject(bad)

    def test_premature_done_or_missing_evidence_rejected(self):
        base = fixture()
        premature = base.replace('M4-DLSS5 | todo', 'M4-DLSS5 | done')
        for bad in (premature, premature.replace('| pending |', '| LOG #test |'),
                    base.replace('M4-presentation | todo', 'M4-presentation | doing')):
            with self.subTest(bad=bad):
                self.reject(bad)

    def test_unsupported_camera_cannot_unlock_scene(self):
        base = (fixture().replace('Current card: **FC-067 / M2-camera**.',
                                  'Current card: **FC-067 / M2-scene**.')
                .replace('M2-camera | doing', 'M2-camera | done')
                .replace('| pending |', '| LOG #test |')
                .replace('M2-scene | todo', 'M2-scene | doing'))
        self.reject(base)
        self.reject(base.replace('contract: **pending**.', 'contract: **unsupported**.'))
        result = inspect(AGENTS, base.replace('contract: **pending**.', 'contract: **supported**.'))
        self.assertEqual(result['current'], 'FC-067 / M2-scene')

    def test_cycles_missing_stages_and_row_errors_rejected(self):
        base = fixture()
        for bad in (
            base.replace('M1-GPU | todo | public adapter', 'M1-GPU | todo | combined hardening'),
            base.replace('FC-067 / M4-DLSS5 |', 'FC-067 / unknown |'),
            base.replace(' | pending |', ' pending |', 1),
            base.replace('M1-GPU | todo', 'M1-GPU | blocked(why)')):
            with self.subTest(bad=bad):
                self.reject(bad)

    def test_light_goal_and_entry_point(self):
        base = fixture()
        self.reject(base, AGENTS + '\nCurrent authority: old trace')
        self.reject(base, AGENTS + 'x' * 8192)
        self.reject(base.replace('RTX Remix', 'unknown renderer'))
        self.reject(base.replace('> Follow', '> ' + 'word ' * 91 + 'Follow'))


if __name__ == '__main__':
    unittest.main()
