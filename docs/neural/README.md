# Emulator-native neural rendering

This directory tracks the optional Windows x64 DirectX 11 neural-rendering work.
The feature is off by default and supports the standard and per-pixel-transparency
DX11 renderers. Other renderers continue unchanged.

The intended pipeline is:

`TA -> PVR raster -> NeuralFrame -> INeuralBackend -> existing presentation`

Public NVIDIA NGX Super Resolution is the supported external API for DLAA and
DLSS SR. The NGX SDK remains an external, user-supplied build dependency. No
NVIDIA runtime or community add-on is shipped by Flycast. A D3D12 surface is
used only through D3D11On12; this project does not introduce a new PVR renderer.

An NGX-enabled local run normally places `nvngx_dlss.dll` beside `flycast.exe`.
For development and `neuraltest`, `FLYCAST_NGX_FEATURE_PATH` may instead name the
directory containing the user-supplied feature DLL. The path is passed to NGX's
documented feature search list and is never copied into the build or repository.

Project state is recorded in `BACKLOG.md`; commands and evidence are in
`LOG.md`; deviations are in `DECISIONS.md`; artifact definitions are in
`DIAGNOSTICS.md`.
