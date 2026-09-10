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
directory before launch; original log files may then be rewritten by their
owners. Configuration and third-party binaries are not modified. Do not run
another session against the same prepared host simultaneously.

`launch.json`, `publisher.log`, `consumer.log` and the `host` report preserve
commands, owned executable hashes and process outcomes. Zero exit codes prove
only successful bounded execution, not active external DLSS 5, image quality,
camera truth, resource cleanup or presentation provenance. Check those separately.
