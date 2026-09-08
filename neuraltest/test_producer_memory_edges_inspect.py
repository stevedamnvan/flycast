import unittest
from unittest.mock import patch
from producer_memory_edges_inspect import inspect


class MemoryEdgeTests(unittest.TestCase):
    def test_writer_identity_and_load(self):
        rows=['OP descriptor=0 index=0 pc=8c03c952','OP descriptor=0 index=1 pc=8c03c96e',
              'ENTRY step=1 descriptor=0','MEMORY kind=write address=8ce6e460 size=4 value=3f800000 pc=8c03c952',
              'STORE index=0 address=8ce6e460 size=4 value=3f800000',
              'READ index=1 address=8ce6e460 size=4','VALUE index=1 part=0 value=3f800000','EXIT']
        text='\n'.join('FC067_PRODUCER_'+r for r in rows)
        with patch('producer_memory_edges_inspect.arithmetic'):
            self.assertEqual(inspect(text)['executed_store_load_edges'],1)
            for bad in (text.replace('part=0 value=3f800000','part=0 value=40000000'),
                        text.replace('value=3f800000 pc=8c03c952','value=3f800000 pc=8c03c950')):
                with self.assertRaises(ValueError):inspect(bad)
