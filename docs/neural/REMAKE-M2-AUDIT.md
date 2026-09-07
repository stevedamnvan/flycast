# FC-067 M2: PVR snapshot checkpoint

Current GPU continuation: `REMAKE-REPLAY-AUDIT.md`. The decoder now reaches
isolated native GPU replay, but a repeat-render/on-off one-step discrepancy
prevents full M2 acceptance. Snapshot and decoder evidence below remains valid.

Implementation base: `3a5051ec9` (2026-09-07), clean worktree before changes.
The initial checkpoint was export only; the bounded decoder follow-up below
advances it to decoded projected data. Native disk-decoded replay alignment and
wrong-camera/wrong-depth controls remain required before M2 acceptance.

## Capture contract

`neuraltest capture --remake-packet yes` adds `pvr-scene.json` alongside each
bounded quality frame. `rend.NeuralCapturePvrPacket` defaults false. There is
no ordinary-mode allocation, additional draw, neural evaluation, external
configuration write, or change to accepted history. The existing developer
synchronous capture owns frame identity and timing; this is not performance
evidence. The launcher fails if any requested packet is absent.

The versioned PVR-specific snapshot records vertex float **bits**, color bytes,
indices including `0xffffffff` strip restarts, list/draw ordinals and PVR state,
texture control words/upload generations/palette hashes/RTT generations, pass
counts, and the actual production normal/viewport transform in column-major
order. It does not serialize pointers or texture pixels. Its coordinate label
is PVR projected; game camera, world transform, and ordinary Dreamcast normals
remain unknown. It is not the M1 world-space synthetic schema.

Before serialization: at most 65,536 vertices, 262,144 indices, 8,192 draws,
10 passes, 256 game-ID bytes, valid index/draw ranges, and finite viewport.
Output is additionally capped at 32 MiB. Nonfinite vertex positions are retained
as raw bits and separately counted from nonfinite index references; exporting
observed buffer data is not accepting it for reconstruction.

Omissions are explicit: texture pixels, fog/global registers, retained
framebuffer pixels, offscreen geometry, game camera/lights, modifier-volume
geometry, sorted translucency resolve order, and Naomi 2 matrices/lights.
Thus this file is not yet a self-contained native scene replay.

## Evidence and failed attempts

- Four working-tree builds pass; three enabled selftests pass **225/225**.
  The opt-in public-header/mock adapter remains **56/56**, not GPU evidence.
- Initial real capture rejected `pvr-packet-position`. The isolated process
  was closed normally; launcher exit 1 and its failed capture are retained.
  Source observation showed why blanket rejection was unsuitable: each of
  frames 1804-1806 has 495 nonfinite buffer vertices but **zero** index references
  to them. These remain visible, not silently removed or promoted to 3D truth.
- Corrected native D3D11 capture `fc067-m2-pvr-working-03` completes three
  deterministic-replay frames, clean close. Vertex counts 15933/15933/15936,
  indices 19153/19153/19156, draws 3339/3339/3340, one pass and 3045 restart
  indices per frame; no Naomi 2 matrices. Packet inspector exits 0 for all.
- First inspection command incorrectly cast unsigned restart indices to signed
  integers and failed; corrected unsigned accounting was run. First final-build
  attempt used the harness `Vertex` instead of `::Vertex` in a new test, failed
  compilation, then rebuilt successfully. An inspection invoked before capture
  completion correctly failed with no captured frames; rerun after completion
  passed. These attempts are not counted as passes.
- Evidence lives outside Git; build logs use `fc067-m2-*`. User configuration
  and game media are not staged. Tests use an isolated native-only stage with
  copies of the existing legal replay and save data, never the active consumer.
- Packet-on versus packet-off captures of frames 1804-1806 have byte-identical
  `native-pvr-color.png` files (3/3). Packet-off writes zero packet files and
  closes cleanly. The original staging-source `emu.cfg` hash is unchanged.
  This is snapshot noninterference, not native replay alignment.
- Comparing two runs found exact vertices/indices/passes/viewport but differing
  draw records: the exporter had read `palette_hash` for RGB textures, although
  TexCache assigns it only for paletted formats. The snapshot now emits null
  for nonpaletted textures rather than reading that indeterminate field.
  Earlier packet draw records are rejected as cross-run generation evidence.
- Corrected runs `fc067-m2-pvr-working-04` and `-05` both close cleanly and
  validate. All three complete packet JSON files are byte-identical across
  runs; all three native PNGs remain byte-identical to the packet-off control.

## Next concrete implementation

### Bounded decoder follow-up (base `b20803d46`)

`ReadPvrScenePacket` and `neuraltest pvr-packet --in <packet> --frame <id>
--game-id <id>` now decode the actual file. The decoder preserves exact float
bits, indexed geometry, captured draw/pass state and unresolved texture IDs;
Naomi 2 is rejected because its transforms were omitted. Output is unchanged
on failure. Game camera/world provenance remains unknown; omitted state cannot
be erased to promote the packet into a complete scene. No renderer calls this
decoder yet and no resource pointers are deserialized.

Limits apply before file allocation (32 MiB), during JSON construction (depth
12, 2 million callback events, 256-byte strings, unique object keys), and before
decoded vectors (65,536 vertices, 262,144 indices, 8,192 draws, 10 passes).
The parser is the repository's existing MIT nlohmann/json dependency, with no
new download. Strict integer/byte ranges prevent narrowing; indexed nonfinite
positions fail while observed unused nonfinite storage is retained.

All four working-tree builds pass. Three enabled selftests pass **243/243**,
including 18 decoder checks added to the 225-check snapshot baseline. Actual
committed-SHA Soulcalibur packets at frames 1804-1806 decode successfully.
The actual wrong-frame CLI control exits 1 at frame/game mismatch. An earlier
working-03 packet exits 1 at missing required omissions, not at the later
palette check; it is not accepted as current-schema evidence. Synthetic tests
separately reject RGB palette metadata, overflowing indices/colors/draws,
omitted Naomi 2 transforms, hidden omissions, upgraded camera claims, nonfinite
viewport/referenced positions, duplicate keys, deep JSON, and oversized files.
The initial decoder suite ran 239/239 before four additional controls raised
it to 243/243. Logs are `fc067-decode-*` under the build directories.

Self-review disposition: **ACCEPTED for bounded decoding only**. Native replay
alignment, camera/depth mutation controls, and complete M2 acceptance remain open.

1. The decoder/round-trip prerequisite above is complete. Use it rather than
   the diagnostic PowerShell inspector to load data for the replay experiment.
2. Resolve captured texture references against the same frame's retained live
   resources, capture the remaining native state needed for replay, and replay
   decoded geometry to an isolated target. Restore original buffers/state;
   never let a failed experiment mutate production presentation.
3. Compare moving frames and overlap against native raster output. Inject
   viewport/camera-assumption and depth mutations and require measurable failure.
   A PVR-to-raster alignment proof cannot establish recovered world camera truth.
4. Only then record M2 acceptance or a precise falsifying disposition. Remix
   GPU runtime, relighting, full-world reconstruction, and combined DLSS 5
   presentation remain separate, unproven milestones.
