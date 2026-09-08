import unittest
from unittest.mock import patch
from calc_input_inspect import inspect


class CalcInputTests(unittest.TestCase):
    def test_actual_bytes_and_wrong_value(self):
        text='\n'.join([
            'FC067_PRODUCER_OP descriptor=0 index=0 pc=8c03c9a4',
            'FC067_PRODUCER_ENTRY descriptor=0',
            'FC067_PRODUCER_MEMORY kind=write address=8ce6e464 size=4 value=3f800000 pc=8c03c950',
            'FC067_PRODUCER_READ index=0 address=8ce6e464 size=4',
            'FC067_PRODUCER_VALUE index=0 part=0 value=3f800000'])
        with patch('calc_input_inspect.arithmetic'):
            self.assertEqual(inspect(text)['verified_input_words'],1)
            with self.assertRaisesRegex(ValueError,'writer/value mismatch'):
                inspect(text.replace('part=0 value=3f800000','part=0 value=40000000'))
            with self.assertRaisesRegex(ValueError,'partial CALC'):
                inspect(text.replace('kind=write address=8ce6e464 size=4','kind=write address=8ce6e464 size=2'))
