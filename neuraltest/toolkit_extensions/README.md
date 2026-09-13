# Capture import on the existing Toolkit MCP server

User-authorized integration extension, loaded from this checkout. It registers
capture and displacement tools on NVIDIA's existing `lightspeed.trex.mcp.core` server; it does not
start another server or modify the installed Toolkit files.

`flycast_inspect_displacement(shader_path)` reports composed USD in/out ranges,
height binding and authored layer sources. Null means no composed USD value;
it does not invent MDL/runtime defaults. `flycast_set_displacement` accepts
finite in/out floats0..0.2 (installed MDL hard range) on captured material Shader paths. Select a separate
project layers/ edit target first: root/capture/external and known baseline
layers are rejected. Authoring is unsaved and rolls back layer content on
failure. Save and layer-off restoration use the existing Toolkit MCP tools.

Launch the installed `kit.exe` with its existing `lightspeed.app.trex.kit`, plus:

```
--ext-folder "C:\Game Dev\Emulators\flycast\neuraltest\toolkit_extensions" --enable flycast.capture.mcp
```

Use `remix_mcp_client.py` to discover tools and confirm the intended loaded
project. `flycast_import_capture(capture_file, dry_run=true)` validates the
source and its local dependencies and reports a hash-named destination under
the project's linked capture directory. Execute with `dry_run=false` only
after inspecting that result. It rebases the capture's asset references instead
of overwriting mesh/material files with colliding names. Source assets must
remain available. An existing destination is refused; use
`flycast_activate_capture(capture_file)` to select an already imported capture.

Both operations use Toolkit's capture-specific API, replacing the prior
capture rather than introducing a second capture into the layer tree. They do
not save the workfile. Never insert a capture beneath a replacement layer:
Toolkit's automatic capture ordering can loop indefinitely.

Keep candidate material layers beneath the replacement mod layer. A candidate
directly beneath the workfile can cause Toolkit to silently switch the edit
target back to the mod. Query `remix_get_edit_target_layer` before binding and
inspect the saved layer afterward. Saving can persist parent insertion, so
explicitly remove the candidate through MCP and save the parent when restoring
layer-off. Verify the baseline bytes; a successful API response is insufficient.

Current live evidence covers capture import and activation, material prim
discovery, twelve saved texture bindings and baseline restoration. It does not
establish visual acceptance, moving hair attachment, or performance.

Disable by omitting the launch flags on the next start. Restart for code
updates; hot-reloading registration is not supported. Preserve unsaved work
before restarting Toolkit.

Observed ingestion limitation (LOG924): launching with these extension flags
caused ingestion-child progress callback timeouts; an explicit HTTP port did
not resolve it. Standard Toolkit launch without these flags successfully
ingested the same four assets through MCP. Until launch-argument inheritance
is isolated, ingest in a standard session, preserve/save intended work, then
restart with this extension for capture activation and bindings. Do not retry
failed ingestion indefinitely or modify installed Toolkit files.

`flycast_set_surface_constants(shader_path, roughness, metallic)` authors only
reflection_roughness_constant and metallic_constant, finite floats0..1, using
the same separate project layer restriction and rollback as displacement. It
does not save, change textures, or establish runtime/visual acceptance.

`flycast_bind_diffuse_in_layer(layer_file, shader_path, texture_file)` adds an
explicit target guard for ingested diffuse candidates. It rejects baseline,
unlinked and external targets, authors in Usd.EditContext, verifies the target
spec and baseline memory/disk invariants, and does not save. Runtime activation
and rollback behavior still require a preserved-state Toolkit restart and live
validation; passing path tests is not proof of live integration.

### Standalone capture timing (LOG1040)

The standalone helper has a30-second total watchdog. Startup wait and post-render
linger both consume that budget: requesting15000ms for each guarantees no room
for runtime loading/rendering/cleanup. For the verified Shrine exact-packet export,
1000ms startup plus1000ms linger completed normally with120 presents and the
same26 texture files as the timeout run. Keep the watchdog unchanged; retain
export logs and verify files/terminal exit before claiming success. A successful
exit does not close the separate undisposed-device-object warning. Use a fresh
output directory for a corrected run and retain the failed evidence.

### Image-processing input and scalar-map checks (LOG1053)

Do not pass capture DDS files directly to ComfyUI LoadImage without decoded
pixel equivalence proof. The installed loader first tries VideoFromFile;
Pillow RGB/255 code is a fallback, not proof of the active decoding path.
Two raw face DDS inputs produced a red/blue swap and alpha255->254 in the
bounded core-node test. Both outputs were rejected before ingestion.
Use the existing exact native PNG sources, verified against raw DDS pixels;
keep alpha and dimensions exact and verify the requested RGB operation before
Toolkit ingestion. Never infer correctness from successful node completion.

PBRify roughness/height RGB outputs are not necessarily equal-channel scalar
images. The installed Remix save node's linear setting does not reduce them
to one channel. Record actual channel/transfer conventions and validate the
runtime import before binding. Do not average channels, regenerate cached maps,
or claim physical roughness solely from a grayscale-looking preview. Preserve
raw outputs, source identity and baseline fallback while the convention is open.

LOG1054 verifies the corrected native-PNG path: two face adjustments preserve
alpha/dimensions, their ingested DDS pixels match exactly, and the guarded
candidate layer restores the baseline after three fresh renders. This is
technical pipeline evidence; its subtle appearance response was not accepted.

LOG1055 resolves the narrow roughness route: use the existing
`flycast_ingest_roughness_current_process` semantic importer. Installed Toolkit
produces linear BC4, and the runtime scalar sample uses R. Preserve the raw
model output and measure decoded BC4 error against R; unequal RGB alone is not
an import rejection. Two Shrine maps passed with p95 error2/255. Physical
roughness plausibility, moving review and height remain separate open gates.
