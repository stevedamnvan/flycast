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

`ExperimentalDLSS5Backend` remains an explicit unsupported adapter until a
public NVIDIA developer contract exists. Hook-compatible mode issues only the
standard D3D12 NGX Super Sampling evaluation. It never names private feature
IDs or inspects/configures third-party modules.

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
