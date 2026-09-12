# Capture import on the existing Toolkit MCP server

User-authorized integration extension, loaded from this checkout. It registers
two tools on NVIDIA's existing `lightspeed.trex.mcp.core` server; it does not
start another server or modify the installed Toolkit files.

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
