import unittest
import struct
import io
from PIL import Image
from source_dds import encode
from source_dds import capture_bundle
from source_dds import publish_capture
from unittest.mock import patch
import hashlib
import tempfile
from pathlib import Path
import json


class DdsTests(unittest.TestCase):
    def test_create_only_publication_and_readback(self):
        data=encode([dict(width=1,height=1,rgba=b'RGBA')],srgb=False)
        bundle=dict(frame_id=1,assets={0:dict(dds=data,sha256=hashlib.sha256(data).hexdigest())},
                    bindings=[],renderable_by_remix_adapter=False)
        with tempfile.TemporaryDirectory() as temporary:
            out=Path(temporary)/'new'
            with patch('source_dds.capture_bundle',return_value=bundle):
                result=publish_capture({}, {}, [], None, out)
                self.assertEqual((out/'asset-0.dds').read_bytes(),data)
                self.assertEqual(json.loads((out/'manifest.json').read_text()),result)
                with self.assertRaises(FileExistsError):publish_capture({}, {}, [], None, out)
                self.assertFalse((out/'manifest.pending').exists())
                bundle['assets'][0]['sha256']='wrong'
                bad=Path(temporary)/'bad'
                with self.assertRaisesRegex(ValueError,'readback'):publish_capture({}, {}, [], None, bad)
                self.assertFalse((bad/'manifest.json').exists())
    def test_bundle_retains_generation_and_encoded_hash(self):
        captured=dict(textures={3:dict(levels=[dict(width=1,height=1,rgba=b'RGBA',source_hash='original')])},
            draws=[dict(source_draw=7,bindings=[dict(asset=3,slot=0,upload_generation=2,
                palette_hash=4,rtt_generation=6,tcw=8,tsp=10)])])
        with patch('scene_material_binding_inspect.source_textures',return_value=captured) as verified:
            result=capture_bundle(dict(frame_id=11,game_id='fixture',git_sha='fixture'),{},[7],None)
        verified.assert_called_once()
        binding=result['bindings'][0];asset=result['assets'][3]
        self.assertEqual(binding['dds_sha256'],hashlib.sha256(asset['dds']).hexdigest())
        self.assertEqual((binding['upload_generation'],binding['palette_hash'],binding['rtt_generation']),(2,4,6))
        self.assertEqual(asset['source_mip_hashes'],['original'])
        self.assertEqual(asset['dds'][148:],b'RGBA')
        self.assertFalse(result['renderable_by_remix_adapter'])
    def test_header_and_exact_payload(self):
        levels=[dict(width=2,height=2,rgba=bytes(range(16))),dict(width=1,height=1,rgba=b'RGBA')]
        d=encode(levels,srgb=False)
        self.assertEqual(len(d),168)
        self.assertEqual(d[:4],b'DDS ')
        self.assertEqual(struct.unpack_from('<I',d,4)[0],124)
        self.assertEqual(struct.unpack_from('<5I',d,128),(28,3,0,1,0))
        self.assertEqual(d[148:],bytes(range(16))+b'RGBA')
        self.assertEqual(struct.unpack_from('<I',encode(levels,srgb=True),128)[0],29)
        for srgb in (False,True):
            decoded=Image.open(io.BytesIO(encode(levels,srgb=srgb)))
            self.assertEqual(decoded.convert('RGBA').tobytes(),levels[0]['rgba'])
    def test_invalid_chain(self):
        with self.assertRaises(ValueError):encode([],srgb=False)
        with self.assertRaises(ValueError):encode([dict(width=1,height=1,rgba=b'abc')],srgb=False)
        with self.assertRaises(ValueError):encode([dict(width=1,height=1,rgba=b'abcd')]*2,srgb=False)
