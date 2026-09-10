# Bounded experimental Remix launch

This runs the existing deterministic Soulcalibur OIT experiment, not an
unrestricted interactive gameplay session. Camera coordinates remain diagnostic;
the full working-pipeline checklist in BACKLOG.md is not complete.

Supply your legal game, installed Remix runtime, built harness/helper, and a
Flycast executable in your already prepared external-consumer host directory.
The launcher does not install components or configure the consumer.

```powershell
python neuraltest/remake_launch.py --flycast "<prepared-host>/flycast.exe" --harness "<build>/neuraltest/neuraltest.exe" --helper "<build>/neuraltest/remake-runtime-smoke.exe" --runtime "<supplied-runtime>/d3d9.dll" --game "<legal-media>/Soulcalibur.chd" --out "<new-evidence-directory>" --anchored-light
```

Without `--run`, this only validates paths and prints the command plan. Append
`--run` to execute. `--anchored-light` is optional and fixes the authored light's
first direction throughout the anchored sequence; it does not recover game light.

Append `--manual-input` to disable scripted input and use your controller during
this bounded session. This also removes the replay-specific producer start gate;
the renderer still requires supported3D Soulcalibur content before exporting.
The same frame/time bounds remain. Menus may stay native, and failing to enter
supported gameplay before the helper's source timeout is a failed experiment,
not a reason to force neural processing onto menus. No manual gameplay acceptance
is claimed by the command's preflight tests.

The bounded run uses D3D11On12, DX11 OIT, 640x480, deterministic input replay,
2100 warmup frames and1200 measured host samples, with660 helper frames.
Image capture and synchronous evidence mode are disabled. Timing results do not
meet the final performance gate merely because the launcher exits successfully.

Existing Flycast/ReShade logs are copied and verified in the new evidence
directory before launch; the two original named logs are then removed so their
owners start fresh logs rather than appending another run. Previous logs remain
recoverable from those verified copies. Configuration and third-party binaries
are not modified. Do not run
another session against the same prepared host simultaneously.

`launch.json`, `publisher.log`, `consumer.log` and the `host` report preserve
commands, owned executable hashes and process outcomes. Zero exit codes prove
only successful bounded execution, not active external DLSS 5, image quality,
camera truth, resource cleanup or presentation provenance. Check those separately.

## Experimental managed sessions

`--managed-session` enables a launcher-owned control mapping. Each renderer
restart or rejected anchor-support change requests a fresh channel generation
and a fresh helper process; old receipts and temporal history are not reused.
The run is capped at eight generations and the original whole-run deadline.
Managed helpers use explicit session-worker mode rather than rotating after600
returns. The worker has a10000-frame ceiling and retains the120-second runtime
watchdog (300 seconds for explicit diagnostic capture). Ordinary helper commands
keep their original frame limits. Channel closure still retires the worker.
The old helper must unwind within its bounded timeout. Unexpected helper errors
abort supervision rather than being hidden by a restart. Logs are separated as
`consumer-g1.log`, `consumer-g2.log`, etc.; `launch.json` retains superseded exits.

`--renderer-reinit-after 2400` is a developer fault-injection option, not needed
for ordinary launch. Managed recovery is still being validated; do not treat it
as an unrestricted interactive mode or a completed transition matrix.

For bounded image evidence, `--capture-frames 120 --capture-start-source 2700`
records at most120 source frames starting at2700. Capture runs use the separate
diagnostic timeout and are marked ineligible for performance evidence. Preview
metadata includes session tokens; verify those against helper receipts and
completed Presents instead of assuming labels prove provenance.

`--remix-only` requires bounded capture and skips neural evaluation of the returned
scene while preserving the same owned native effects and HUD composition. It is
a diagnostic comparison, not a performance lane. Metadata explicitly records
`comparison_lane=remix-only` and `neural_evaluation_skipped=true`; legacy
`evaluated-remix.png` names the owned pre-HUD presentation surface, not evidence
of neural execution. The default remains the combined experiment. Compare exact
returned pixels as well as scene/native/HUD identity before attributing differences
to neural processing; separate live Remix runs need not return identical pixels.

For exact-input isolation, first use `--effect-identity --capture-frames 30`
with a positive `--capture-start-source`. Then pass that run's captures directory
as `--locked-input-root` at the same start frame. Existing source/effect/temporal
identity checks remain mandatory; missing or mismatched archives reject the run.
These synchronous diagnostics retain the30-frame limit and cannot prove the
full300-frame moving quality or performance gate.

`--returned-dlaa` selects public DLAA on the returned Remix image for bounded
capture; it is mutually exclusive with `--remix-only`. Use an already prepared
hooks-disabled host and verify its active log reports no neural hooks. Merely
requesting DLAA is not proof the supplied consumer left it untouched. This is
not the target-native PVR DLAA lane. The launcher never edits hook policy.
