# FC-067 standalone runtime bring-up

Status: implementation/CLI checks only. M1-GPU remains unproven.

## Prepared diagnostic snapshot

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
