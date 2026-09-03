# Neural rendering decisions

## D-001: required surface and scope

Use the existing DX11 and DX11/OIT PVR renderers. Public NGX D3D11 is the first
backend. D3D11On12 supplies a D3D12 device/queue without a new PVR renderer.
RTT never enters the neural stage; direct-framebuffer frames reset history and
bypass evaluation. Evaluation cadence follows emulated display-bound frames.

## D-002: atomic input package

Renderer instrumentation produces one renderer-neutral `NeuralFrame` after the
display-bound scene and before OSD/ImGui. Translucency is represented through
confidence/bias rather than exported depth. Modifier volumes do not contribute
to neural depth.

## D-003: C++17 view adaptation

The pinned project requires C++17 (`CMakeLists.txt:62`). `std::span` is C++20.
The frame contract therefore uses a trivial pointer/count `ArrayView` with the
same non-owning lifetime semantics rather than increasing Flycast's global
language requirement.

## D-004: local baseline build deviations

The canonical Windows CI lane is MSVC x64 + Ninja Release with `USE_DISCORD=ON`.
This host lacks the CI-installed DirectX June 2010 SDK, so the local DX11
baseline uses `USE_DX9=OFF`. Upstream `ENABLE_CTEST=ON` also leaves
`BUILD_TESTING` cached independently and its Windows `HttpTest.cpp` requires
libcurl even though the Windows application uses WinHTTP. Existing tests will
therefore be exercised using the repository's Linux CI configuration; the
Windows application baseline is built with both test switches off.

## D-005: DLSS 5 boundary

Flycast implements only the public NGX DLAA/SR contract and never names private
feature IDs or inspects/configures third-party modules. Experimental DLSS 5 is
route-neutral: a compatible direct D3D11 consumer, D3D11-to-private-D3D12
bridge, or conditional D3D11On12/native-D3D12 route may consume that public
contract. Public NGX success is not DLSS 5 proof; FC-066 and Gate 10 require
positive returned-output consumption and presentation evidence.

## D-006: pinned community reference

Behavioral reference is DLSS5-Feeder v0.10.0-beta.2 at
`b60a8ffe4073dd65f8dbf804e47886607919b6b6`. Later tags exist and are
deliberately not followed. The reference is not a dependency and is not
vendored.

## D-007: Phase 1 fixture-driver boundary

The initial ROM-free harness uses a real headless D3D11 device and GPU
rasterization through a test-only driver. Flycast currently builds its renderer
inside the application executable rather than as a linkable core library. The
test driver permits early validation of artifact I/O, deterministic hashing,
threshold failures, and genuine-resolution sampling, but it is not accepted as
production DX11/DX11-OIT evidence. `rend_context`, the real TA parser, analytic
depth/motion truth, and production renderer linkage remain required before the
Phase 1 gate can close.

## D-008: production instrumentation seam is metadata-only initially

The guarded DX11 and DX11/OIT seam snapshots real `rend_context` OP/PT/TR
metadata and submits one Geometry package before native display composition.
Its initial color and depth references used existing renderer resources only to
establish ownership and cadence: DX11 color was BGRA8, the base depth resource
had no shader-resource view, and OIT exposed its typeless depth view. Those
references were not accepted as the required RGBA8/R32 neural inputs. No
presentation selection depends on stage output until all dedicated exports and
their gates exist.

## D-009: OP/PT depth is replayed into an isolated R32 target

The first production depth export replays only each render pass's opaque and
punch-through lists into a three-slot `R32_TYPELESS` resource ring using a
`D32_FLOAT` target and `R32_FLOAT` shader view. Existing shaders preserve both
legacy log-depth and native-depth behavior, including punch-through discard,
without adding a disabled-path shader permutation. Translucent and modifier
volume lists are never issued to this target. This costs extra OP/PT draws and
must meet FC-064 timing before acceptance; the choice can be replaced by an
equivalent cheaper export if measured over budget.

## D-010: instrumentation history advances on stage acceptance

The two fixed draw buffers are explicit current-work and last-successful
reference buffers. `CaptureGeometry` never rotates them. The renderer calls
`MarkEvaluated(frameId)` only after `NeuralStage::TrySubmit` returns
`Submitted`; busy, holding, unsupported, and failed frames overwrite only the
work buffer. A Geometry/FramebufferDirect source transition increments
`historyGeneration`, and direct frames carry no draw views and always request a
reset. RTT branches do not call the stage.

## D-011: incomplete motion is always biased to current color

The production DX11 export ring now owns the complete required texture format
set. Its export-only pixel-shader permutation reuses the existing texture,
alpha-test, clipping, and depth behavior for opaque and punch-through replay.
Until previous-position geometry is bound, motion is cleared to zero and every
pixel's bias-current-color mask remains one; confidence is zero. This is a safe
intermediate contract, not motion evidence. Draw ID zero remains background and
opaque/punch-through draws use global snapshot ordinal plus one. Translucent
draw-ID/mask coverage remains pending and cannot close FC-025 or FC-033.

## D-012: public NGX calls terminate at SEH leaf wrappers

The D3D11 backend uses SDK v310.7.0's Project-ID initialization, capability
parameters, `NGX_D3D11_CREATE_DLSS_EXT`, and
`NGX_D3D11_EVALUATE_DLSS_EXT`. Every external init/query/create/evaluate/
release/destroy/shutdown call is made by a POD-only SEH leaf; C++ ownership and
fallback policy remain outside those leaves. Output uses a fixed three-slot
RGBA8 UAV/SRV ring with `D3D11_QUERY_EVENT` readiness checks and
`D3D11_ASYNC_GETDATA_DONOTFLUSH`, so a busy slot skips instead of waiting.
Create uses `MVLowRes` only for standard SR and no flags for 1:1 DLAA; HDR,
jittered-MV, inverted-depth, auto-exposure, alpha-upscale, sharpening, and
output subrect flags are off. The stable custom
Project ID is Flycast-specific but still requires maintainer/NVIDIA review
before distribution.

## D-013: external NGX feature discovery is explicit

NGX initialization uses the specification's stable custom project ID
`7d5f2a1c-3b8e-4c6a-9f0d-2e4b6c8a1d3f` and the generated `GIT_VERSION` string.
Packaged builds rely on NGX's default application-directory lookup. Local
development may set `FLYCAST_NGX_FEATURE_PATH`; its single directory is passed
through `NVSDK_NGX_FeatureCommonInfo::PathListInfo`. This keeps the separately
licensed feature DLL outside the repository while making the harness invocation
deterministic. The first attempted live run exposed that `PATH` alone does not
satisfy NGX feature discovery.

## D-014: SR inputs must match public NGX optimal settings

For each standard SR mode, the D3D11 backend calls the public
`NGX_DLSS_GET_OPTIMAL_SETTINGS` callback after capability discovery. Feature
creation is rejected unless `NeuralFrame` render dimensions equal the returned
optimal dimensions for the configured output. The harness exposes explicit
output dimensions; this prevents a successful 1:1 Super Sampling call from
being mislabeled as an upscaling result. Dynamic-resolution min/max bounds are
recorded internally but are not accepted by this first fixed-resolution path.

## D-015: D3D12 NGX work is isolated in discardable ring slots

The public D3D12 backend owns three command allocators, command lists, output
textures, and per-slot fence values. A slot whose prior fence is incomplete
returns `Busy`. NGX create/evaluate executes inside POD-only SEH leaves; an
exception or failed external call prevents list submission and replaces the
unsubmitted allocator/list. Successful work transitions output from UAV to
shader-read and executes on the caller's direct queue. Harness readback waits
are separate and do not enter the backend or emulator frame path. Current
shutdown drains submitted work before releasing NGX and is not accepted yet as
the deferred-retirement solution for runtime resize/toggle.

## D-016: the harness proves the same-device D3D11On12 surface separately

Before each D3D12 neural run, the harness creates D3D11On12 on the exact D3D12
device and direct queue, wraps a D3D12 render target, acquires it, clears it
through D3D11, releases it to D3D12, flushes, and observes queue completion.
The synthetic neural inputs are then uploaded as native D3D12 resources. This
proves basic wrapped-resource/same-queue viability, not yet that Flycast's PVR
renderer or swapchain runs on that surface; FC-045 remains open for that reason.

## D-017: presentation retains only an accepted D3D11 neural output

`DX11Renderer` keeps an owning reference to the stage output only after
`TrySubmit` returns `Submitted`. The normal framebuffer view remains the source
for every disabled, busy, unsupported, holding, failed, or D3D12-without-wrapper
frame. The selected view enters the existing content-rect quad blit, so OSD and
later UI composition retain their native order. Mode/surface changes, resource
release, and the beginning of each new submission clear the retained view.
Global renderer reset and save-state deserialization explicitly increment the
instrumentation history generation through a renderer-neutral callback.

## D-018: experimental consumer acceptance is route-neutral

The experimental mode does not auto-enable `NeuralD3D12Surface`. Native D3D11
is a valid candidate transport and is labeled `d3d11-external-unclassified`
until evidence distinguishes direct consumption from a bridge. D3D11On12 is a
separately selected conditional route. Route selection follows measured
interception, returned-output provenance, latency, correctness, stability, and
maintenance cost rather than API label.

## D-019: compatibility rebuilds follow readiness transitions

The consumer compatibility recreation is armed only by a missing-to-present
component transition. It waits a configurable count of successful public-NGX
evaluations (default 300), consumes one idempotent release request, and bounds
release/recreate attempts (default two). Telemetry records attempts, failures,
successful rebuilds, and the initiating or retry reason. The value 300 is a
configuration default, not an architectural frame invariant.

## D-020: readiness and proof are separate states

Current status and harness reports distinguish selected route, missing
components, components present, public contract evaluated, and compatibility
rebuild activity. Later Gate 10 work must add output-produced, output-consumed,
and visually-confirmed evidence as separate fields rather than infer them from
the current ladder. Loaded modules and a successful public evaluation cannot
advance the claim past `contract-evaluated`; no current Flycast route is
recorded as passing Gate 10. D3D11On12 process classification is measured at
runtime rather than inferred from swapchain creation.

## D-021: the available Feeder source is not a Flycast bridge runtime

Feeder v0.10.0-beta.2 demonstrates a useful D3D11-to-private-D3D12 transport,
but its D3D11 path obtains color, depth, and motion from the ReShade effect
runtime and constructs its own neural contract. It therefore cannot satisfy
FC-048's requirement to mirror Flycast's exact public NGX resources, scalars,
and frame identity. It remains a design reference. The bridge route is blocked
until a compatible contract-preserving runtime is supplied or a bounded local
bridge build is separately authorized.

## D-022: the supplied add-on selects the conditional On12 route

In a corrected native-D3D11 Soulcalibur run, Flycast evaluated its public NGX
contract while the supplied RenoDX add-on installed only D3D12 NGX hooks and
reported no intercepted evaluation or feature 18 work. This blocks only the
exact supplied direct-D3D11 arrangement; it does not generalize to all D3D11
consumers. Because the same add-on positively consumes D3D12 NGX, this measured
failure activates FC-045's conditional D3D11On12 route.

## D-023: production On12 owns every flip-model backbuffer

The queue-created two-buffer swapchain is exposed to the DX11 renderer through
one wrapped D3D11 resource and RTV per D3D12 backbuffer. Acquire and release use
`IDXGISwapChain3::GetCurrentBackBufferIndex`; the previous single-buffer draft
lost the device on its first Present and is retained only as failed evidence.
Neural input rings are D3D12 resources wrapped for D3D11 export, released to
non-pixel-shader-resource state before public NGX evaluation, and accepted D3D12
outputs are wrapped back for the existing final content blit.

## D-024: public-output presentation telemetry is not visual confirmation

The production renderer tags the accepted D3D12 public output with the Flycast
frame ID, records when that view is selected by the final content blit, and
records a successful Present. These events prove application-side ownership and
cadence for the public output resource. Even when the external log records a
feature-18 evaluation at the same contract boundary, Flycast labels the resource
an external-mutation-unconfirmed candidate until a sentinel, pixel comparison,
and controlled add-on A/B prove that the presented pixels are neural output.
If the experimental route has not reached `contract-evaluated`, the public NGX
call may still run for detection/rebuild telemetry, but its output is not selected
and Flycast keeps the native framebuffer as the presentation source.
