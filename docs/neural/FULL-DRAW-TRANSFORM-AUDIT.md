# FC-067 complete opaque draw: bounded evidence

Scope: Soulcalibur draw1, vertices4..145, producer ordinals1781..1783.
This is not complete-scene, recovered world-camera, native-preservation or
Remix GPU/presentation acceptance.

## Result

The retained positive `fc067-draw-transform-b` contains48 distinct XYZ records
per frame feeding142 vertices,166 strip indices,24 restarts and92 triangles.
All144 record instances have576 primary transform seams,144 X-load/entry
links,864 actual RAM writes and1278 downstream component reads. No additional
accumulation occurs in this selected domain. Other domains retain their
previously proven accumulation requirements.

The independent arithmetic decoders and aggregate ownership verifier check
all426 vertex observations. A common normalized projection decomposes the
144 matrices within the existing0.001 scale tolerance. Calibrated positions
reproject with maximum errors0.0000197808/0.0000215880/0.0000245198 pixels.
Each frame supports92 of8339 opaque index triangles and explicitly omits8247.
Normals, physical scale, world camera and runtime material conversion remain
unknown. Zero reprojection error alone would not prove those missing semantics.

## Controls and limitations

- Independent full-domain protocol fixtures mock only separately tested
  arithmetic decoders. They check compact descriptors, all426 consumers,
  late-consumer loss, last-record edge ownership and missing coverage.
- Exact successor timestamps are required when a scheduler boundary separates
  blocks. Synthetic wrong-stamp and reversed-clock controls reject; no broad
  time tolerance replaces actual successor identity.
- Live wrong-X capture C rejects. Its producer cycles differ from B, as do
  scene and guidance/image files. This is not an exact-input preservation pair.
- Capture A exhausted conservative log accounting at sample368. It remains
  failed. Actual prefix maximum51 bytes justified an80-byte allowance instead
  of128; the8MiB emitted-data cap did not change. B session is5,960,324 bytes.
- The stage's append-only log exceeded the16MiB loader bound. It was archived
  intact before B; the loader limit was not relaxed and raw evidence not deleted.

## Reproduction

Use the retained capture directory with:

```text
python neuraltest/camera_transform_inspect.py --capture CAPTURE --full-draw
python neuraltest/camera_calibration_inspect.py CAPTURE --full-draw
python neuraltest/calibrated_scene_inspect.py CAPTURE --full-draw
python -m unittest discover -s neuraltest -p test_*inspect.py
```

Temporary core probes are retained only in ignored build evidence and removed
from production source. No third-party binaries/configuration/media accompany
this audit. LOG records build/checkpoint scope and failed attempts.
