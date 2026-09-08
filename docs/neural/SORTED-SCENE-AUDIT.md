# FC-067 witnessed scene and sorted-translucency contract

Starting HEAD: `8197983fe213c9d4719174ee9e6f930972a776f6`.
ACCEPTED bounded scene packet v2 correction (self-review), not full M2.
No world camera, visible-object identity, GPU path tracing or Remix presentation
is established by this slice.

## Actual contract defect

`ta_vtx.cpp::parseRenderPass` calls `sortTriangles` for triangle autosorting.
`ta_util.cpp::sortTriangles` reads translucent PolyParam first/count in vertex
space, appends triangle-list indices, and creates SortedTriangle commands with
polyIndex/first/count. It does not convert those source PolyParam ranges into
index ranges. Equivalent adjacent materials can merge under another polyIndex.
An empty sorted command explicitly signals sorting even when no triangle survives.
DX11 `drawSorted` consumes those commands, not source PolyParam ranges.

The v1 packet omitted sorted order and incorrectly validated every nonempty
source range against the index buffer. Empty culled draws can also retain unused
pre-compaction first values. The correction preserves empty offsets verbatim,
validates nonempty ranges against their explicit coordinate domain, and exports
bounded sorted commands/pass ownership in v2. Ordered nonoverlapping nonempty
sorted index ranges bound validation work. Restart tokens are forbidden in
triangle lists. Source vertex ranges can contain unused nonfinite vertices;
actual referenced nonfinite geometry remains rejected by the decoder.

Historical v1 packets remain readable without upgrading their omitted-order
claim. Same-frame DX11 developer replay compares captured v2 commands with live
retained commands before replacing them. Global state, textures and modifier
geometry still require retained resources: this is not standalone replay.
The rejected framebuffer-light preview explicitly refuses sorted topology.
Material inspection accepts the new schema without changing material semantics.

## Failing attempts retained

All named raw runs are under sibling `flycast-evidence/`; build/test logs are
ignored under `build-neural-automation/`.

- `fc067-scene-working-original` / `different`: export rejects draw range.
- `fc067-scene-diagnostic-b`: empty opaque offsets identify the first defect.
- Empty-range red selftest: 245 passed, 3 failed; green: 248 passed, 0 failed.
- `fc067-scene-fixed-original` / `different`: empty fix alone still rejects.
- `fc067-scene-late-original`: moving the probe to capture-function entry does
  not fix the contract. Metadata there is still 1301; it must not label the
  witnessed scene. Nonempty failures are translucent source vertex ranges.
- Sorted-range red selftest: 248 passed, 2 failed. Initial green: 250/250.
  Expanded controls: 262/262. These were actually executed automation tests.
- The intermediate `fc067-scene-c-build.log` has no corresponding game run.
- A search for nonexistent `test_material_inspect.py` failed; no test of that
  name is claimed. Existing material inspector requires a real capture package.
- A guessed `replay_inspect.py` path also did not exist. Replay proof JSONs
  were read directly. Material inspection initially rejected `--capture`;
  its correct positional capture argument then verified the package.

No failed scene export is counted as success because its surrounding three-frame
capture completed. No relaxed invalid nonempty range was used as a workaround.

## Successful actual snapshots

`fc067-sorted-working-original` and `fc067-sorted-working-different` each exit 0,
retain three frames and close cleanly. The probe runs immediately after the
current instrumentation frame is assigned, under its saved context-pointer and
epoch lineage checks. Both report epoch 1310/context 00509700, frame 1302,
15,563 vertices, 11,274 indices, 3,259 draws and 19 sorted commands. Their actual
scene.json bytes are identical, SHA-256:
`dafbacf031eb0f9e73e84e8f79a0de2eed8a417e99b557b2974ff7ee042ca2ec`.
The original snapshot passes the compiled bounded packet decoder.

The full shared-calibration verifier passes against both new runs, including
the previous exact source/arithmetic/TA/decode lineage and offline wrong-scale/
stale-epoch controls. Later retained native planes match the restored
8197983fe baseline: 54 comparisons, 27 per run. They are preservation evidence,
not screenshots of instrumentation frame 1302.

| Sample vertex | Source translucent draw | Source range | Sorted command | Material-state draw |
|---|---:|---|---:|---:|
| 14526 | 5 | 14526 + 3 vertices | 9: index 10737 + 3 | 5 |
| 14529 | 6 | 14529 + 3 vertices | 15: index 10773 + 36 | 20 |

Both source records have TCW 715959488, upload generation 1, palette hash
4029862303 and RTT generation 0. The second sorted command is merged; do not
equate its material-state draw 20 with the source geometry's draw 6. Both samples
are translucent three-vertex primitives, not established fighters or opaque
world geometry. Shared texture state does not by itself identify a semantic role.

Temporary source instrumentation/header are removed. Exact working-source patch
and header snapshots are ignored `fc067-sorted-scene-working.patch` and
`fc067-sorted-scene-probe-header.h`. No external consumer settings were changed.

## Restored-source verification

Four serial configure/builds (automation, NGX, no-NGX, feature-off) return 0.
Each enabled selftest passes 262/262; SDK mock passes 56/56 with runtime/GPU/
Present false. Logs use `fc067-sorted-restored-*` names in each build directory.
`fc067-sorted-restored-replay` captures three frames 1804-1806 with materials and
same-frame replay enabled, exits 0 and closes cleanly. All 27 unique native
planes match the prior exact-SHA baseline. Decoded/native and decoded/retained
replay differences are zero in each frame. Wrong viewport changes
264181/264231/264301 pixels; wrong depth changes 307159/307144/307135 pixels.
This three-frame result does not close the parked historical 30-frame residual.
Material inspector verifies all three v2 packages, 39 assets in the first frame
and 40 texture/palette previews. Exact-commit results belong in the ignored
postcommit note rather than repeatedly making documentation-only commits.

## Next concrete task and remaining acceptance

After exact-SHA confirmation, bind the snapshot
associations in a repeatable verifier with wrong-frame/vertex/range/material
controls. Capture the actual witnessed scene's source material and image/coverage
if needed to identify this primitive family. Do not use later retained images,
call these samples opaque-world calibration, repeat a matrix presence census,
or start a Remix renderer on a guessed camera. Whole-scene calibration and the
parked original-source replay residual remain unproven.
