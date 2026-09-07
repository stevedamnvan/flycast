# FC-067 camera/material extraction: approved investigation

User approved scoping this investigation after the preview stopping boundary.
Starting HEAD: `20e0c11a5aaaff2c19fec2b3443a1a419d00b36c`, clean checkout.
This supersedes the request for further scope approval in the preview handoff.
It does not resume pixel-parity probes or SDR light-multiplier tuning.

## Source findings -- inspected, not GPU extraction claims

The failed preview used the already shaded framebuffer as material. That is
not the only data Flycast has. `DX11Texture::texture` and `textureView` expose
the actual GPU texture separately. `UploadToGPU` creates B5G5R5A1, B4G4R4A4,
B5G6R5, B8G8R8A8 or A8 resources, with provided/generated mipmaps. The latter
may be palette indices, not alpha-only material; preserve palette interpretation.

The DX11 pixel shader starts from vertex color, samples texture/palette with
perspective-correct UV, applies ShadInstr, optional offset color, fog and other
operations. ShadInstr 3 multiplies sampled texture by vertex RGBA. Therefore
texture pixels and vertex color can be exported separately without inverse
lighting estimation from the framebuffer. Texture artwork may itself contain
painted lighting; call it **source texture**, not proven physical albedo.

`BaseTextureCacheData` retains Updates, rttGeneration, palette_hash, dimensions,
tex_type, gpuPalette, area and is_custom_replaced. Existing decoded replay
checks captured upload/RTT/palette state against same-frame resource references.
Reuse those checks. Never serialize pointers or identify assets solely by VRAM
address/TCW. The earlier exact decoded/native-buffer replay remains useful,
but its separate 13-pixel original-source residual remains failed and parked.

For normal Dreamcast, `ta_vtx.cpp::vert_cvt_base_` copies incoming x/y and
`xyz[2]` into z as invW. UV and base/offset colors are separately decoded.
`Vertex` labels its normal fields Naomi2-only; `PolyParam::isNaomi2` tests an
explicit projection-matrix index. None of this establishes a Soulcalibur game
camera matrix, world transform, physical depth scale or world-space normal.
Retain unknown camera provenance; do not reinterpret the production log depth.

## Actual existing-capture inventory

Read all 30 packet JSONs in the retained `fc067-replay-688d63e05-yes` interval,
frames 1804-1833, game T1401N. Counted **52 distinct first-texture state keys**
(TCW/upload generation/RTT generation/palette hash), not 52 proven GPU resources
or content hashes. All 100,228 textured draw records use ShadInstr 3:

| List | Textured draw records across 30 frames |
|---|---:|
| Opaque | 92,220 |
| Punch-through | 540 |
| Translucent | 7,468 |

There are also 30 untextured opaque and 19 untextured translucent records.
Pixel-format record counts: 0=5,400; 1=16,470; 2=3,960; 3=71,850; 5=2,548.
All 2,548 paletted records carry a palette hash. No textured record has a
nonzero RTT generation. These counts do not prove textures unchanged within a
frame or establish their semantic role; pixel extraction has not been run.

## First bounded implementation: source material sidecar

Implemented checkpoint: `MATERIAL-CAPTURE-AUDIT.md` (LOG #146, D-089).
The source-texture/palette sidecar and 30-frame native-preservation evidence are
accepted for that slice only. Review the material artifacts, then pursue the
approved camera source-witness track; do not reimplement extraction or tune
the rejected framebuffer-light preview. Camera/Remix/full M2 remain unproven.
Older launch instructions below are historical scope, not repeat assignments.

Implement this slice first in the existing native-D3D11/normal-DX11 developer
capture lane. No new renderer, Remix runtime, asset replacement or consumer
configuration. Use Astra/low and the current checkout; no default subagents.

1. Add an explicit off-by-default material-capture opt-in, bounded by the existing
   capture frame limit. Ordinary rendering must perform no material enumeration,
   readbacks, allocations or filesystem writes. Restrict the first implementation
   to native D3D11 normal renderer, no RTT/direct-framebuffer or Naomi2.
2. At the existing same-frame scene capture seam, resolve actual texture/SRV
   resources and match texture generation/identity to the saved draw record.
   Write a separate versioned material manifest keyed by frame/game/source SHA
   and draw list/ordinal/texture slot. Do not weaken the existing scene schema's
   omissions or upgrade its world-camera provenance.
3. Deduplicate actual resource content with descriptor plus full captured mip
   content hashes, while keeping all logical draw bindings. Preserve native mip
   dimensions, format, row packing and channel semantics; exclude padding from
   hashes. Bound to 256 resources and 64 MiB total raw texture/palette data per
   frame, with checked size arithmetic and fail-closed overflow. Bound retained
   frame count to 30. Unsupported formats/arrays/MSAA or unresolved generation
   must reject the material package, never silently substitute the framebuffer.
4. Preserve paletted index texture plus the actual applicable palette resource,
   selection/format and palette generation. Export optional decoded RGBA previews
   only after a controlled palette/channel fixture. Record custom replacement
   status; do not silently describe a supplied replacement as original media.
5. Keep source texture pixels, per-vertex base/offset RGBA, UVs, ShadInstr,
   sampling state, alpha/depth/list state and omissions distinct. Texture-only
   visualization must be labeled; no inferred roughness/metalness/normal map.
6. Validate native-format encode/decode and palette lookup with synthetic charts
   and wrong-channel/palette/generation negatives. In the legal Hoko interval,
   capture three frames first, then 30 only after boundedness/identity is green.
   Verify all referenced slots resolve, input identity matches, and capture
   leaves native presentation unchanged under the existing recorded strict
   comparison. Retain the known source residual separately if it recurs; this
   is not authority to restart precision diagnosis or claim that gate passed.
7. Deliver a texture contact sheet and source-texture versus vertex-color
   inventory, not a new relighting preview. Update evidence/backlog; build all
   four configurations serially, run enabled selftests and relevant fixtures;
   commit/push only code/docs. Never stage game textures, palettes, captures,
   external configurations, dependency trees or machine paths.

Stop for the material-sidecar review. This slice succeeds when actual source
assets can be inspected/reused without baking the final scene into their color;
it does not need a recovered camera or a path-tracing renderer to be useful.

## Camera track: source provenance before extraction claims

After the material-sidecar review, assess whether a title-specific CPU-side
projection witness can be captured at the pre-PVR transform boundary. A stored
matrix must be tied to the actual submission and validated through moving
reprojection plus wrong-frame/matrix controls before being called recovered.
Do not scan arbitrary memory and select a plausible-looking matrix, infer a
universal FOV from screenshot appearance, or confuse Naomi2 state with Dreamcast.
If such a witness cannot be located with the available code/data, report that
precisely and keep camera unknown. No camera extractor exists in this checkpoint.

Further runtime integration requires its own license/dependency and GPU
completion/readback/lifetime proof. This plan neither downloads proprietary
components nor changes the existing proven external DLSS 5 presentation route.
