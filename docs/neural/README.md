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

The harness accepts standard SR as `neural --backend sr --mode quality` (or
`performance`) with `--output-width` and `--output-height`. It asks NGX for the
optimal input size and rejects an input package of any other dimensions instead
of silently stretching or treating a 1:1 evaluation as SR.

The production-shader depth contract is exercised without game media by
`neuraltest depth-contract --api d3d11|d3d11on12 --out DIR`. Public NGX polarity
creation can be A/B tested in the harness with `neural --depth-polarity
inverted|normal`; production Flycast always uses the proven inverted PVR depth
declaration.

The synthetic motion convention is exercised by `neuraltest motion-contract
--out DIR`. A harness-only temporal DLAA pair can supply `--previous-in` and
constant `--motion-x/--motion-y` values; production motion remains geometry
derived and does not expose those overrides.

`neuraltest selftest` also executes the real production PVR export shader pair
on native D3D11 and D3D11On12. It checks accepted `[-4,+3]` render-pixel motion
and falsifies invalid/excessive guidance by requiring zero motion/confidence and
full current-color bias.

The ROM-free SDR and rectangle contract is exercised by `neuraltest
color-contract --out DIR`. It compiles the production DX11 presentation quad,
round-trips an `R8G8B8A8_UNORM` ramp/patch/alpha/checker chart, and checks exact
4:3, 16:9, and odd-sized content rectangles.

Project state is recorded in `BACKLOG.md`; commands and evidence are in
`LOG.md`; deviations are in `DECISIONS.md`; artifact definitions are in
`DIAGNOSTICS.md`.
