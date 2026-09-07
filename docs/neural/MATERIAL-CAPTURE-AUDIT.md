# FC-067 source-material sidecar checkpoint

Implementation baseline: `fa5f74b2c36d3566cb7068f005eb8dbc131f2209`.
**ACCEPTED for bounded developer source-material extraction (self-review), not
camera recovery, PBR material recovery, standalone replay, or Remix GPU use.**

## Contract and implementation

`capture --remake-materials yes` requires packet=yes, native D3D11, normal DX11,
and at most 30 frames. `rend.NeuralCapturePvrMaterials` defaults false. Callback
creation and enumeration are behind this flag and the existing active capture
frame gate; no ordinary-mode texture enumeration, allocation/readback or file
write is added. RTT/direct framebuffer, Naomi2 and unsupported resource layouts
fail closed. This is synchronous evidence work, never performance evidence.

The separate `materials/manifest.json` records frame/game/SHA, every draw's two
logical slots (including explicit nulls), TSP/TCW/shading instruction, source
upload/RTT/palette state, palette base, custom replacement status and sampling
overrides. Vertex/offset colors and UVs remain in the PVR packet; metadata states
their raw BGRA8 native-DX11 binding rather than conflating them with source texture.
The packet's original omission list and unknown camera are not overwritten.

Actual texture/SRV identity and full mip visibility are checked before reading.
Capture-seam resource generations must match the packet before and after
readback. This is **not a per-draw historical texture snapshot**: future titles
that mutate resources within a frame need further provenance. Source textures
can contain painted lighting and are not asserted to be physical albedo.

Supported native formats: B5G5R5A1, B4G4R4A4, B5G6R5, BGRA8, RGBA8, A8 palette
indices. Preserve every actual mip with tight native rows, dimensions, format,
byte count and locale-independent FNV64; no row padding enters the hash. Identical
descriptor/full-byte content is deduplicated without trusting hash uniqueness.
At most 255 logical textures plus one palette and 64 MiB of readback bytes per
frame are allowed. Unsupported/custom compressed formats reject, not substitute.
The 32x32 BGRA palette resource is retained with palette selection and format.
No proprietary asset or external configuration is committed.

## Executed evidence

- Four final working configurations build, exit 0. Automation/NGX/no-NGX
  selftests each pass 243/243. SDK mock remains 56/56, no runtime/GPU/Present.
- `material-contract` uses native D3D11 WARP: six formats, three mip levels,
  145 raw/RGBA/alpha comparisons, locale-independent empty-content hash, and
  14 negative controls. Controls cover byte-budget/output preservation,
  wrong/missing palette, wrong channels, upload/RTT/palette generation,
  unsupported format and texture arrays. No injected device-loss/Map failure
  or 256-resource runtime stress is claimed.
- Three-frame working B capture and separate off control close cleanly and
  match native color 3/3. Material inspector verifies every raw mip, draw binding,
  generation, frame/game/SHA and palette view. The first frame has 38 logical
  textures plus the palette: 39 assets and 40 texture/palette previews.
- Working moving on/off captures both close cleanly, 30 frames 1804-1833.
  All 30 packages verify. Each contains 39 assets, 7,155,710 raw bytes and
  6,678-6,698 slot bindings. The nine PNG planes (native, source, depth, motion,
  bias, confidence, draw ID, overlay classification, final composite) match
  byte-for-pixel 30/30 between on/off runs. This does not close the separate
  parked decoded-replay original-source residual.
- CLI wrong-API and 31-frame controls return 2 before game launch. Python
  hash/BGRA/UNORM-alpha positives and wrong-channel negative pass. The contact
  sheet was inspected: faces, clothing, floor/scenery and fonts are visible.
  Some whole resource allocations contain noisy/unclassified regions; this is
  not UV-region segmentation or evidence that every texel is authored material.

Working evidence uses sibling `flycast-evidence/fc067-material-*` directories;
source config stays SHA256
`1EF718689784DCE64CAD1CE8BEC776E710B4A3705ECFF2E5A276A1A3B2F92992`.

Failed attempts retained: first build hits Windows min/max macros and const
ComPtr access; corrected without casting away const. Fixture build hits Windows
`small` macro and is renamed. First real capture A completes but its inspector
rejects locale-grouped hash strings; it is not accepted material evidence.
Classic-locale hashing, a grouped-locale regression and fresh capture B correct
that. One inspector bounds patch fails context matching without applying, then
is corrected. A Windows wildcard rg path fails and is rerun against the folder.

## Reproduction and next task

1. Run `neuraltest material-contract --out NEW_DIR`.
2. Use the existing deterministic native capture command with
   `--remake-packet yes --remake-materials yes --remake-replay no`, first 3 frames,
   then 30 after inspection. Restore only disposable-stage replay save files.
3. Run `python neuraltest/material_inspect.py CAPTURE --out NEW_DIR` (NumPy/Pillow).
   Inspect source-textures.png and verification.json. A launcher success alone
   is not raw-data integrity validation; run the inspector before acceptance.

Next: review these actual source-material artifacts, then the already approved
camera track in CAMERA-MATERIAL-PLAN.md. Identify a pre-projection source witness
before any matrix extraction claim. Do not resume framebuffer-light tuning,
precision diagnosis, renderer replacement or private-runtime acquisition.
The full goal remains incomplete: world-camera and real Remix execution are
unproven; M2 strict original-source replay equality remains parked, not passed.
