# FC-067 M1 implementation and public-source audit

## Disposition

Implementation baseline: `81057acbdaa236bea32511b5a3e50a0fc6f35b27`.
**ACCEPTED for the CPU scene-contract and public-header/mock-call slice only
(implementor self-review); independent review and synthetic GPU execution are
not claimed.** No production rendering source changed. M1 review boundary is
reached; M2 game-scene work is a separate next assignment after review.

## Pinned source and dependencies

Remix source revision: `e876135b37295dc203ccdb7b20a8089629588201`.
Reviewed public `remix_c.h` API version: 0.6.4. SHA-256 after CRLF-to-LF
normalization: `793bec004819ba2bd3d4026032b845c0b2fa7804513a2c6a3895c842f4bfd1e1`.
The opt-in CMake target checks this hash before compilation. No vendor header
or binary is checked into Flycast; the tested header and notices are in an
ignored local cache. The header retains its MIT notice; upstream root LICENSE
is zlib/libpng and LICENSE-MIT covers NVIDIA additions. This is a source/header
audit, not blanket clearance of all runtime dependencies or redistribution.

Reviewed upstream sources:

- [Public API](https://github.com/NVIDIAGameWorks/dxvk-remix/blob/e876135b37295dc203ccdb7b20a8089629588201/public/include/remix/remix_c.h):
  `CreateMaterial`, `CreateMesh`, `CreateLight`, `SetupCamera`, `DrawInstance`,
  `DrawLightInstance` and matching destroy callbacks. Meshes contain triangle
  surfaces, hardcoded vertices, indices, material handles, and hashes.
- [SDK guide](https://github.com/NVIDIAGameWorks/dxvk-remix/blob/e876135b37295dc203ccdb7b20a8089629588201/documentation/RemixSDK.md):
  standalone direct API; camera parameter extension; API version compatibility.
- [Build declarations](https://github.com/NVIDIAGameWorks/dxvk-remix/blob/e876135b37295dc203ccdb7b20a8089629588201/meson.build)
  and `external-build/meson.build`: the full runtime references external
  NGX/DLSS/DLFG, NRD, USD, MDL, and other dependencies. Packman-managed external
  components are referenced. We did not execute those dependency scripts.
- PCSX2 reference pinned to `907c71f32ef9f5ef68e832ccfcfa888a01d7cc3f`:
  [RemixConfig.h](https://github.com/Aelthien/pcsx2-rtx-remix/blob/907c71f32ef9f5ef68e832ccfcfa888a01d7cc3f/pcsx2/GS/Renderers/Common/RemixConfig.h)
  has user-supplied near/far/FOV reconstruction settings and GPL-3.0+ marking.
  No implementation was copied, built, or validated; no PS2 depth formula is
  accepted as Dreamcast projection truth.

Full runtime/GPU disposition: **NOT RUN**. No Remix runtime was supplied to this
harness; the local audit cache contains only the public header and license
texts, and `meson` was not found on PATH. A source build also needs reviewed
runtime dependencies above. The adapter intentionally has no DLL loading or
Present code, so supplying a DLL alone would not turn this test into GPU proof.
The next GPU slice needs explicit startup, surface, capture/readback, shutdown,
and completion-lifetime tests with a legitimately available runtime. No binary
download, consumer/config write, hook install, or runtime modification occurred.

## Flycast source seams inspected

| Existing symbol | Data we can reuse | What it does not establish |
|---|---|---|
| `Vertex`, `PolyParam`, `rend_context` in `core/hw/pvr/ta_ctx.h` | vertices, indices, OP/PT/TR lists, passes, draw texture/state | Normal Dreamcast world-space transforms, skeletons, camera, or lights |
| `BaseTextureCacheData` in `core/rend/TexCache.h` | Updates, RTT generation, texture/palette hashes | Material roughness, metallic response, or unlit albedo |
| normal shader in `core/rend/dx11/dx11_shaders.cpp` | actual projected PVR transform/divide and neural screen exports | General world-space unprojection from an arbitrary assumed FOV |
| existing bounded capture under `neuraltest` | deterministic execution/testing conventions | complete off-screen geometry or an already-compatible Remix output surface |

Naomi 2-only normal and matrix members are not evidence that Soulcalibur supplies
them. No production PVR capture hook was added in M1.

## Implemented scope

`neuraltest/remake_scene.*` defines a versioned in-memory packet with frame/game
identity, explicit coordinate/projection provenance, optional normals and
transforms, topology, texture/content/palette/RTT generations, and omissions.
It enforces aggregate mesh/vertex/index/element-byte limits. It is not an
untrusted-file parser: future ingestion must check budgets before allocation.
Unknown values can be retained in a packet but cannot silently become a scene.

The Remix adapter accepts only untextured, explicitly identity-transformed
world-space synthetic meshes with supplied normals and a fixed-axis pinhole
camera. It supplies a clearly synthetic gray material and distant light.
It rejects textured, incomplete, projected, unknown-normal/transform, and
unsupported-clipping cases. It does not claim to preserve arbitrary PVR
materials or translate every game draw state. These are M2/M3 work.

`RemixScene` calls the real header's types/function signatures against an
injected interface. No private ABI is re-created. The caller must keep it alive
through frame consumption and destroy it before runtime shutdown; GPU fence
and queue guarantees remain untested. Any partial submission failure requires
discarding the frame. Resource cleanup tests are CPU/mock evidence only.

## Tests actually run

- Four sequential MSVC/Ninja build configurations: automation, NGX, no-NGX,
  and feature-off; exit 0 for each.
- Automation, NGX, and no-NGX `neuraltest selftest`: **217 passed, 0 failed**
  each (172 existing plus 45 new CPU contract checks).
- Opt-in `remake-sdk-contract` compiled against the pinned real header and
  ran: **55 passed, 0 failed**, explicitly reporting
  `runtime_loaded=false gpu_rendered=false presented=false`.
- Five independent synthetic packets produce exact projected samples. Analytic
  pinhole coordinates, camera translation, reciprocal-depth interpolation,
  overlap ordering, background, and strip parity through degenerate sequences
  have direct expected-value checks.
- Negative controls reject invalid indices, NaN/Inf, bad schema/enums, frame
  mismatch, truncated/incomplete input, missing projection/normal/transform,
  wrong camera, reciprocal depth treated as linear, and wrong farther-first
  ordering. Mesh-create and draw failures test cleanup and frame-discard status.
- Initial build/test iteration passed 210/210 and 48/48; additional checks then
  expanded these to 217/217 and 55/55. No failed compiler/test attempt occurred
  before those final runs. Intentional failing controls are not hidden failures.
- Two configure negatives were actually run afterward: missing header exits 1
  with the explicit required-header diagnostic; a deliberately different header
  exits 1 at the SHA pin. Restoring the reviewed header directory configures
  successfully (exit 0). These rejected configure logs are retained.

Commands and full output are retained locally as `fc067-m1-*-build.log`,
`fc067-m1-*-selftest.log`, and `fc067-sdk-*.log` in the build directories.
These runs are not new real-game, performance, Gate 10, or image-quality proof.

## Next review and implementation

### Follow-up source review (2026-09-07)

Self-review of `bde1b460d` found that a later all-degenerate strip was rejected
after earlier API resources had already been created. Cleanup existed, but
preflight should reject this packet before external side effects. Topology
conversion now precedes every API resource call. The added late-mesh negative
control verifies zero material, mesh, and camera calls; the actual public-header
mock target ran 56/56 (exit 0). This is a bounded M1 correction, not independent
review or GPU acceptance. M2 native packet/replay validation remains pending.

Review the packet limits, unknown-data rejection, strip conversion, public ABI
pin, and failure/resource lifetime contract. On acceptance, proceed to M2:
one bounded Soulcalibur scene packet and native replay alignment under known
camera movement, with explicit projection assumptions and failing wrong-camera
controls. Keep the missing synthetic GPU/runtime slice visible; neither M2
packet export nor native replay can close it. No production Remix or combined
DLSS 5 presentation before its separate M3/M4 proof.
