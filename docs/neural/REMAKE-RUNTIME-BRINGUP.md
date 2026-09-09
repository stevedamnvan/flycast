# FC-067 standalone runtime bring-up

LOG436 captures three composed source frames in one warmed session with
`--capture-reverse-light`; each frame gets its own BMP. Visible fighter ghosting
is a failed temporal result. Endpoint resources currently have deliberately
separate namespaces. The pinned public API's CreateMesh allocates new internal
geometry hashes; investigate retained mesh skinning/instance transforms rather
than assuming reuse of an external handle proves geometry history. Public source:
[rtx_remix_api.cpp](https://github.com/NVIDIAGameWorks/dxvk-remix/blob/e876135b37295dc203ccdb7b20a8089629588201/src/dxvk/rtx_render/rtx_remix_api.cpp).

LOG433 adds a40-mesh static composed scene containing both fighters and temple.
`prepare_embedded_draw_inspect.py` supports `--group large|two|four` for the
retained H source groups; it runs source arithmetic and cross-capture checks,
then embeds camera-relative positions with explicit provenance.
`compose_remake_inspect.py BASE_ROOT NEW_OUTPUT ADDITION_ROOT...` accepts up to
three embedded groups, preserves their revisions/coordinate labels, and verifies
asset publication. These are diagnostic offline preparation tools, not a live
Flycast-to-Remix scene exporter or recovered world-space camera.

For an isolated snapshot, `--capture-reverse-light ABSOLUTE_NEW_BMP` uses
diagnostic distant-light direction(0,0,-1), radiance unchanged, instead of the
default(0,0,1). It is not recovered game lighting or an automatic title preset.
LOG425-426 retain complete submitted batches using this control. Run the
harness with its working directory outside the repository: the external runtime
writes its own cache/logs there. Never stage those runtime-generated files.

`remake_batch_join_inspect.py FIRST FIRST_ASSETS SECOND SECOND_ASSETS OUTPUT`
creates a new same-frame diagnostic artifact/assets directory after identity
and byte-hash checks. It retains omissions and both source cameras. This is
not temporal matching or complete scene acceptance. `remake_subset_inspect.py`
is a falsifying exclusion tool only; its omissions are explicit, not a fix.

Status: WIP real synthetic and sampled Soulcalibur snapshot readback (LOG401-402).
M1-GPU acceptance remains unproven: analytic controls and cleanup are pending.

Optional trailing `--capture ABSOLUTE_NEW_BMP` or
`--capture-depth ABSOLUTE_NEW_BMP` uses a separate application-owned D3D9
device registered through the public SDK. It does not call standalone Startup.
The output is fixed640x480; depth additionally writes unchanged RGBA32F bytes
to a create-new `.rgba32f` sidecar. The BMP maps depth/10 for display only.
Captures are synchronous diagnostics, never performance
evidence. New files only; this is not a production Flycast capture feature.
The prepared snapshot retains its calibrated camera aspect even at this fixed
diagnostic buffer size. Successful calls do not certify image correctness.
Actual final color now shows synthetic triangles and sampled temple geometry;
runtime teardown still reports undisposed objects. No clean-lifetime acceptance.

Synthetic-only `--capture-reverse-camera ABSOLUTE_NEW_BMP` reverses the
camera's X motion; `--capture-zero-light ABSOLUTE_NEW_BMP` zeros the supplied
distant-light radiance. Neither is allowed with a prepared game snapshot.
`remake_capture_geometry_check.py BMP [--camera-x VALUE] [--normals]` reports
analytic silhouette overlap for the fixed synthetic fixture; it does not
declare a gate passed. Bright-color segmentation depends on this fixture's
lighting. Historical normal visualization/metrics are INVALID: the public
source buffer is packed R32_UINT, not XYZ floats (LOG408). The harness rejects
`--capture-normals` before loading the runtime until typed readback exists.
LOG404-405 retain both matching and falsifying actual-image results.

## Prepared diagnostic snapshot

For a bounded three-endpoint transition, use `--frames 63` and append
`--next L_DIRECTORY --next M_DIRECTORY --capture ABSOLUTE_NEW_PREFIX`
after the existing artifact/assets/clips arguments. Each endpoint directory
contains `scene.json` and `assets/`. The first60 frames are explicitly logged
H warmup, followed by H/L/M once. Outputs are
`PREFIX.frame-SOURCE_FRAME.bmp`, all checked create-only. Endpoints must have
consecutive frame IDs, identical game/source SHA and shared diagnostic origin.
All scene resources remain retained through the session. Mesh/material IDs are
isolated across endpoints: this tests diagnostic transitions, not temporal
correspondence, complete gameplay or performance. Runtime teardown warnings
remain open. Packed normal capture is still rejected.

The standalone harness additionally accepts:

```text
remake-runtime-smoke --runtime ABSOLUTE_DLL --frames 3 --artifact ABSOLUTE_JSON --assets ABSOLUTE_DIR --clips NEAR FAR
```

This explicitly loads a prepared sampled-anchor scene, verifies bounded assets
and supplied clip containment before loading a DLL, and uses SubmitDiagnostic.
It retains calibrated aspect/axes and source-frame identity: no synthetic camera
translation or window-aspect replacement. Repeated presents of this immutable
snapshot are NOT moving gameplay evidence. Reported source SHA and omissions
remain visible. The source artifact's unknown game clip values stay unknown;
the command arguments are caller-supplied diagnostic limits only.

LOG395 ran the actual H artifact with a confirmed absent runtime: valid
0.1..104 diagnostic clips pass ingestion then exit3 runtime unavailable;
0.1..100 rejects before runtime loading with exit2. The synthetic command
also retains exit3 for absent runtime. No runtime Startup/Present was run.

Use remake-artifact-check with ARTIFACT ASSETS NEAR FAR to validate ingestion
without any runtime loading capability. artifact_loader_controls.py runs its
actual positive/13 rejection controls with temporary files. Keep prepared
game artifacts and DDS publications outside Git. Supplying a compatible legal
runtime and its dependencies is still required for any GPU bring-up.

`remake-runtime-smoke` is built alongside the pinned-header SDK mock when
`NEURALTEST_REMAKE_SDK=ON`. It is not linked into Flycast and is not a user
rendering option. It requires an explicit absolute path to a legitimately
supplied Remix runtime and1..120 frames:

```text
remake-runtime-smoke --runtime ABSOLUTE_PATH_TO_RUNTIME_DLL --frames 60
```

There is no automatic runtime search, download, dependency script, config-file
write, or SetConfigVariable call. DLL dependency search is limited to the
explicit DLL directory and Windows System32. A30-second watchdog terminates
only this standalone process if a library call stalls. Termination is a
failure, not clean shutdown or lifetime proof. Launch is explicit; ordinary
Flycast and the mock test never execute it.

The executable uses public InitializeLibrary/Startup/Present/Shutdown and the
existing synthetic scene adapter. Camera X moves from0 to0.5 over the bounded
sequence; a one-frame run uses zero. The camera aspect uses the actual client
rectangle, initially640x480. No Dreamcast geometry or unknown camera
is relabeled as synthetic world-space evidence. One immutable scene is created
and reused through camera-only redraws, then destroyed before Shutdown. That
ordering is not an independently proven GPU-completion mechanism.

The pinned public [SDK guide](https://github.com/NVIDIAGameWorks/dxvk-remix/blob/e876135b37295dc203ccdb7b20a8089629588201/documentation/RemixSDK.md)
documents standalone initialization, scene submission and Present. Its source
and the cached0.6.4 header were inspected; no runtime internals were inspected.
The prior license/dependency audit remains in REMAKE-M1-AUDIT.md.

## Evidence actually run

On the1043fa4e3 worktree, the automation target built with MSVC/Ninja, exit0.
The following real process controls returned the expected codes:

| Control | Exit |
|---|---|
| Missing arguments |2|
| Relative runtime path |2|
| Zero frames |2|
|121 frames |2|
| Explicit absent runtime,3 frames |3|

Existing SDK mock rerun:56 passed,0 failed, runtime/GPU/Present false. No actual
runtime load, Startup, Present, watchdog timeout, device recovery or Shutdown
was exercised. The reviewed local cache contains only the header and license
texts. Build log: ignored `fc067-runtime-smoke-build-a.log`.

## Required next runtime evidence

### Bring-up source correction

Review found the first draft recreated identical resources each frame. The
adapter now supports validated camera-only redraw without recreating assets;
any API draw failure invalidates further redraws. This remains an immutable
synthetic scene API, not general game geometry updates. The120-frame resource
count and cleanup tests are mock evidence only, not GPU lifetime tests.

Supply a compatible legal runtime and dependencies without overwriting the
user's existing files. Then execute this isolated bounded bring-up and retain
runtime identity, API outcomes and visual output. Even exit0 only proves the
reported API calls returned success: the executable explicitly reports
`gpu_image_proven=false`, `readback_proven=false` and
`completion_lifetime_proven=false`.

Implement public-output readback and synchronization against the actual
available interface before accepting M1-GPU. Require actual synthetic images,
moving camera, overlap and wrong-camera/light controls, retained numeric/pixel
comparisons, resource lifetime and failure checks. No camera or combined
Remix/DLSS5 gameplay acceptance follows from this bring-up executable.
