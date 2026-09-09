import tempfile
import unittest
from pathlib import Path
from prepare_remake_endpoint import bounded_json, validate_reference


class EndpointTests(unittest.TestCase):
    def test_reference_and_mutations(self):
        reference=dict(schema='flycast-prepared-remake-scene-v1',fixed_origin=[1.,2.,3.],
                       frame_id=1782,game_id='T1401N',git_sha='fixture')
        validate_reference(reference)
        for field,value in [('schema','wrong'),('fixed_origin',[0,0,float('nan')]),
                            ('fixed_origin',[True,0,0]),('frame_id',1783),
                            ('game_id','wrong'),('git_sha','')]:
            with self.subTest(field=field,value=value),self.assertRaises(ValueError):
                validate_reference(dict(reference,**{field:value}))

    def test_bound_before_parse(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'input.json'
            path.write_text('{"ok":true}')
            self.assertEqual(bounded_json(path,64),{'ok':True})
            with self.assertRaises(ValueError):
                bounded_json(path,2)


if __name__=='__main__':
    unittest.main()
