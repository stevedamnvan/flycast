import unittest
from opaque_routine_inspect import inspect, verify


def fixture():
    path = ['8c048aa0','8c048ad2','8c048afe','8c048b1a','8c048e70']
    lines = []
    for i, count in enumerate([12,15,10,9]):
        lines.append(f'FC067_ROUTINE_BLOCK step={i+1} block={path[i]} next={path[i+1]} return=8c048e70 r0=00000002')
        for j in range(count):
            pc = ['8c048b28','8c048b2c'][j] if i == 3 and j < 2 else '00000000'
            lines.append(f'FC067_ROUTINE_SHIL step={i+1} pc={pc} op=writem')
    lines += ['FC067_ROUTINE_STOP reason=returned steps=4',
              'FC067_OPAQUE_READ pc=8c048aa4 value=00000000bda5382f expected=00000000bda5382f exact=1',
              'FC067_OPAQUE_READ pc=8c048aaa value=000000003d95af05 expected=000000003d95af05 exact=1']
    return '\n'.join(lines)


class RoutineTests(unittest.TestCase):
    def test_positive_and_controls(self):
        self.assertEqual(verify(fixture())['rejected_offline_controls'],4)

    def test_missing_metadata(self):
        with self.assertRaises(ValueError): inspect(fixture()+'\nFC067_ROUTINE_METADATA missing=true')

    def test_duplicate_return(self):
        with self.assertRaises(ValueError): inspect(fixture()+'\nFC067_ROUTINE_STOP reason=returned steps=4')

    def test_missing_operation(self):
        with self.assertRaises(ValueError): inspect(fixture().replace('FC067_ROUTINE_SHIL step=1','OTHER step=1',1))


if __name__ == '__main__': unittest.main()
