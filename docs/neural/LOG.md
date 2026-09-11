# Neural rendering evidence log

LOG806 substep H host-side cost attribution at 1280x960 (diagnostic
CPU-timing runs, never performance evidence). Two matched runs with the
LOG805 perf-a launch plus `--cpu-timing`: `pilot-extent1280-cpu-a` and the
control `pilot-extent640-cpu-a` (same temple rig, native alpha, welded
normals, exposure profile A; only the output size differs). Host stage
medians in ms, 640 then 1280 (600 samples each): emulated frame period
19.0 then 30.8; frame-render 15.6 then 26.9; frame-submit-neural 12.2 then
22.3; returned-evaluate 4.5 then 12.2 (evaluate-raster 1.3 then 5.0,
view-scene 4.2 then 5.1, evaluate-history-accept 0.6 then 2.4,
evaluate-output-own 1.5 then 1.8, evaluate-input-upload 1.0 at 1280);
scene-feed 6.1 then 8.1; emulation thread waiting for the render thread
2.4 then 11.1; off the render thread, return-worker 3.8 then 9.4 and
feed-worker 11.0 then 12.4 (unchanged by extent). Reading: the render
thread executes the returned-image work serially inside the frame and that
work grows with pixels: repeated full-image depth validation (channel
receive, well-formed check, history accept, motion raster on current and
previous depth), four to five full copies of the 4.9 MB colour and depth
buffers per frame (channel receive assign, neural input build, history
accept, raster UpdateSubresource of current and previous depth), and the
helper's own conversion and return (LOG805). The emulator is throttled to
that render thread (wait 11.1 ms), so the 60 fps goal at 1280x960 is a
host CPU pipeline problem, not a path-tracing or upscaler problem. Next
(H, source work, four builds and three selftests before commit): (1)
finer scopes inside evaluate-raster, return-worker and history-accept
(validate, upload, copy, draw) so each fix is measured; (2) single
vectorizable range validation per image instead of repeated passes, with
the validation semantics unchanged; (3) ping-pong the raster's depth
textures so the previous depth is not re-uploaded; (4) move the copies
into the return worker or share buffers by reference, keeping the
ownership rules; (5) helper: fused depth extract and clamp, R32F depth
target if the runtime accepts it, and split colour and depth return work
across two threads; (6) re-measure with performance-eligible runs at the
same denominator. No source change in this entry; no external
configuration touched.

LOG805 substep F moving review and substep H first cost attribution at
1280x960. (1) Moving capture `pilot-extent1280-v11-moving` (same launch as
LOG804, 40 captures from source 2560): 40 consecutive sources 2560..2599
with no gaps; the HUD band (top 110 rows) of every composite is identical
to the native backbuffer (0 pixels above 25/255 in all 40 frames); the mean
absolute frame-to-frame change of the composites rises smoothly from 1.3
to 15.0/255 as Kilik's attack and the camera move, with no isolated spike
(no pop, no history reset inside the strip); weapon trails, hit sparks and
the flash are the original effects composited over the Remix scene
(`moving-strip-kilik.png`, `moving-strip-taki.png`, sent for review).
Observation for the look review, not a defect claim: green patches on
Kilik's trousers in frames 2585..2595 during the hit flash. Technical
ACCEPTED for the moving path at this extent; look NOT_REVIEWABLE. Run
stats: 823 accepted evaluations, 1173 presents, capture run
(performance_eligible=false). (2) Performance-eligible runs at 1280x960
(no captures, no accumulation, no CPU instrumentation, 1200 samples each,
exit 0, orderly shutdown): perf-a native shading profile (exposure A plus
`rtx.upscalerType 0`, `rtx.resolutionScale 1.0`): 1181 accepted, 1176
presents, 6 output repeats (99.5 percent fresh over the steady presents),
present p50/p95 26.48/29.68 ms, helper period p50 26.75; perf-b exposure A
with the runtime's default upscaler (DLSS, graphics preset Auto): 1184
accepted, 6 repeats, present 29.22/33.02, helper period 29.89; perf-c
DLSS forced to MaxPerf (`rtx.dlssPreset 2`, `rtx.upscalerType 1`,
`rtx.qualityDLSS 1`, all three confirmed in the effective config): 1181
accepted, 8 repeats, present 29.46/32.83, helper period 30.11. Reference
at 640x480 (LOG797 weld-a): present 20.01/24.59, helper period 20.05. The
60 fps goal is therefore not met at 1280x960 (about 34 to 38 fps), and
the shading resolution is not the cost: forcing DLSS performance mode
changes nothing and native shading is the fastest of the three.
Attribution from the helper's per-image timing (medians over the steady
images, in ms, 640 then 1280 native): draw 2.7 then 2.1, present 0.2 then
0.2, depth lock wait 0.6 then 1.6, depth convert 1.2 then 4.5, return 1.0
then 4.1, receive wait 8.3 then 9.3, prepare 3.6 then 12.1, turnaround
20.9 then 33.5. The growth is in the helper's CPU-side per-pixel work on
the returned image (depth conversion, return copy, prepare: about 5.8 ms
at 640 against 20.7 ms at 1280, four times the pixels), not in the path
tracer's draw. Substep H next item: move or parallelize those conversions
(the user has stated that more CPU threads are welcome) and re-measure with
the same denominator; VRAM by phase and normal-renderer coverage remain
open. Profiles used only through `DXVK_RTX_CONFIG_FILE`
(`pilot-curated/profiles/exposure-probe-a{,-dlss,-dlssperf}.conf`); no
external configuration edited. No source change in this entry.

LOG804 substep F: first live 1280x960 combined presentation with captures;
technical ACCEPTED for the extent path as a diagnostic result, not
performance, temporal-quality or visual acceptance. Resume state: the v6
diagnostic had been prepared from a baseline-configuration executable
(input replay not compiled in; the memory rule "prepared executables come
from build-neural-automation" applies) and the automation build predated
the observed/predicted depth diagnostics, so v6 is void. Rebuilt the
automation configuration and reran the same launch as v5 (1280x960, temple
rig, radiance 1, native alpha, welded normals, exposure profile A, 12
captures from source 2560). v7: the scene stage now passes at 1280x960 with
no anchor rejection at all (v5's rejections came from the stale binary);
every frame then skipped at stage native-effects with
`passes=1 autosort=1 extent=1280x960` because the OIT effects capture in
`dx11_oitrenderer.cpp` still gated on `width == 640 && height == 480`.
v8 with that gate on the selected extent: effects captured
(541786752 logical bytes per snapshot at 1280x960, backing 1280x960), 598
publishes, 590 returns, but no evaluation, guidance or presentation and
no captures; the return path was leaving silently. v9 added the
`Remake neural input rejected` diagnostic (never fired) and exposed
`Remake GPU guidance rejected: remake-raster-input-bound` on all 587
returns: `remake_motion_raster.h` was hard-wired to 640x480 (textures,
input bounds, constants, row pitches, viewport). v10 with the raster on
the selected extent (`pilot-extent1280-v10-raster`): 1200 host samples,
762 publishes, 754 returns, 752 accepted evaluations, 752 GPU guidance,
752 source-effect composites, 1173 preview presents (1163 remake presents,
425 output repeats, mean latency 4.6 frames), 12 captures at
1280x960 (`captures/frame-2560-present-2564` and following: native
backbuffer, evaluated Remix output, composite with native effects and HUD;
`live1280-sheet.png`). HUD, health bars, timer and weapon trail composite
correctly; one anchor support change at source 3100 re-anchors in session
as before. Present p50/p95 29.4/35.8 ms against 20.0/27.3 at 640x480 in
the LOG797 performance run; helper turnaround p50 41.9 ms; 428 feed
skips (worker busy, native fallback) on the alternate frames. This is a
capture run (performance_eligible=false) and the 60 fps goal is not met
at this extent yet; the cost attribution belongs to H. Controls retained
from LOG803 (wrong-size, truncated depth, wrong receipt, stale return,
unsupported extent). Builds: four configurations serial, 0 errors;
selftest 883/0 in automation, baseline and no-ngx; remake-sdk-contract
272/0; python suites 23/0. Source committed with the other session's F
implementation (extent contract, channel ABI 5, temporal, motion units,
effects, preview, transient window) after this build and test pass.

LOG803 F implementation in progress after46fc20eb0: output-size1280x960
is an immutable launcher environment contract shared by host/helper. Channel
ABI5 negotiates width/height and reserves bounded maximum image/depth slots;
only active extent bytes are copied/hashed. Neural conversion, temporal
extent identity, motion units, OIT effects, preview/guidance metadata and
archive receiptv2 dimensions are parameterized. Old dimensionless receipts
retain640x480 semantics. Default extent remains640x480. Three enabled suites
883/0 and disabled build passed at the first build checkpoint. CPU transport
tests pass at640x480 and1280x960; wrong-size, truncated-depth, wrong-receipt
and stale-return controls reject; unsupported1280x480 rejects at channel
creation. These are not GPU or live presentation acceptance.

First live attempt pilot-extent1280-v1 failed before gameplay (SH4 exception
when blocked) and exposed host matching a640x480 window despite the960
render setting. Second attempt uses explicit transient1280x960 window
settings (SDL now preserves stored window state for transient dimensions),
and scaled title-specific HUD atlas bounds. It runs host1200 samples to
completion but publishes no source: view-title-or-viewport-unsupported.
F correction now permits exactly the selected doubled backing framebuffer
while retaining the original native640x480 viewport/lens certificate and
source-coordinate checks. Builds pass, but v3/v4 publish no scene because
the observed-anchor check fails. v4 diagnostics show zero initial copy
matches despite5724 decoded observations at producer2090. A same-build
640x480 control (pilot-extent640-v4-control) reproduces the same failure,
so it is not established as resolution-specific. v5 confirms5724 unique,
unchanged copies but no XYZ links at2090. Later3300 has862 valid XYZ/lens
matches and first projection residual0.000146 yet no accepted anchor;
the early missing links therefore do not explain the entire run. Added
observed/predicted depth diagnostics for the next build/run to distinguish
the remaining depth certificate from projection rejection. Do not weaken
anchor checks or change lens constants.
No high-resolution live pass claimed. Failed runs and logs retained under
D:\Flycast-Evidence.

LOG802 D/E candidate committed as46fc20eb0. Two reviewed Kilik red-cloth
regions now use PBRify roughness variation raised from about.334 to.720;
unselected texels, albedo and normal remain unchanged. Separate
pbrify_cloth_refined_v2 layer ingested/bound through Toolkit MCP. v1 selector
rejected for leather contamination; v2 narrower red/atlas selection reviewed.
The temple rig is opt-in, warm key plus cool fill with fixed directions per
anchor generation; default light unchanged. All three enabled selftests
873/0, disabled build successful, launcher17/0. Same-source1280x960 still
shows less plastic-like red cloth and directional form/contact shadows.
pilot-temple-moving-v1 has40 captures2908..2948 (one gap), host0/helper11 at
orderly end, four anchor generations, no forced processes. Reviewed recovery
motion, not the requested earlier combat; no protected HUD pixels in the
sample, so HUD acceptance stays open. Existing40-object teardown warning
remains. See PILOT-TEMPLE-LIGHTING.md for scope. User subsequently requested
continued polished/playable remaster goal; active in this task, no stop at
handoff. Broader material masks, delighting and full working-pipeline gates
remain open.

LOG801 resume comparison (2026-09-10): verified HEAD
485b7fe07293ad48eba64c3a2850b4a170670d32; preserved existing dirty docs and
private artifacts. The quoted installation checkpoint was superseded by
LOG800: 26 texture directories and 104 ingested DDS maps exist. Live Toolkit
MCP remix_get_layers confirms pbrify_reimagined above curated and draft.
Reviewed final-full, final-kilik and final-temple: floor grain, carving and
costume detail are clearer; trousers are overly glossy and bright skin/trim
still lose detail. PBRify is enhancement/map inference, not proven delighting.

Resumed E with the existing helper's explicit light-radiance option, exposed
in pilot-curated/pilot_render.py as --radiance (default unchanged). Added
command, packet/config hashes and radiance to its run receipt. Saved two
120-frame 1280x960 still renders of the same welded source-2601 packet and
exposure-A profile: resume-pbrify-light3-exposureA-repeat and
resume-pbrify-light1-exposureA, both exit0. Helper SHA256
78f567b23c10a7529aa04cb73096e633c24742c0044586413d7e3a4bd6c6a872,
packet SHA256 907940fdbbc04cd099956f2d7e19e5ace16fd4720a18ec08439780b163ea8958.
Logs confirm radiance3/1 and anchored light. Existing shutdown warning
(40 common device objects not disposed) remains; no resource acceptance.
No live configuration, materials, factory light or production source changed.

pilot-curated/resume-light-comparison.json/png retain evidence: old-vs-fresh
radiance3 repeat mean absolute channel difference1.127/255; fresh3-vs-1
20.033/255, so the lighting change exceeds observed repeat noise. Changed
pixel counts alone are unsuitable (992621 repeat pixels). Mean luminance
111.35 to90.96; luminance>=250 share0.1302% to0.0017%; below16 share10.10%
to10.72%. Visual review: bright detail improves, floor darkens, glossy cloth
persists. This is a candidate, not final lighting or user visual acceptance.
Scoped comparison ACCEPTED; E remains CORRECTIONS_REQUIRED: warm key/cool
fill and movement attachment still pending. D retains roughness/metal-mask
and baked-shadow work; F live resolution is unchanged. No performance,
temporal or full-pipeline claim. Installation/generation need not be repeated.

LOG800 pilot substep D, reimagined route: PBRify material set generated,
ingested and bound; technical ACCEPTED as a reversible layer, look
NOT_REVIEWABLE pending the user; exposure probes for E. Direction: the
user wants the materials AI-reimagined and RTX-favorable (delit albedo,
real normal/height/roughness, metal masks, 4x). Web review (NVIDIA
relighting and material guides, ComfyUI-RTX-Remix, PBRify_Remix): path
tracing wants baked shading stripped from albedo, strong normal and height
maps, roughness variation, emissive masks only for luminous details, and
light from primitive lights at radiance about 1 tuned with the tonemapper.
Installation (user go-ahead "Let's do this"): ComfyUI 0.35.0 at
`C:\Game Dev\Emulators\comfyui\ComfyUI` with its own venv (torch
2.11.0+cu128, RTX 5090 seen), NVIDIA ComfyUI-RTX-Remix nodes (V3 API) in
`custom_nodes/comfyui-rtx_remix`, PBRify_Remix 1.7.2 ComfyUI pack (CC0,
trained on ambientCG: `4x-PBRify-UpscalerV4.safetensors`,
`4x-PBRify_UpscalerSPANV4.pth`, `1x-PBRify_{Height,NormalV3,RoughnessV2}.pth`)
in `models/upscale_models`; headless on 127.0.0.1:7860, the port the
Toolkit's own `lightspeed.trex.comfyui.core` 1.1.2 expects (that extension
has no REST endpoint, so ComfyUI is driven by its HTTP `/prompt` and
`/history` API and every ingestion, layer and binding step goes through the
Toolkit MCP). Driver `pilot-curated/pbrify_run.py` (generate, ingest, bind,
check): the shipped `integration_pbrify.json` with its download nodes
replaced by the local models, one prompt per captured 256x256 original (RGBA
PNG, alpha preserved), 4 maps at 1024 in about 4 s each, 26 materials in
under 2 minutes; 104 maps ingested one item per MCP call into
`assets/ingested/pbrify/` (ingestion renames normals to
`<name>_OTH_Normal.n.rtex.dds`, octahedral, `inputs:encoding = 0`); layer
`layers/pbrify_reimagined.usda` created through the MCP as the strongest
sublayer above the curated and draft layers; metal constants from the
reviewed classes authored in the layer text. Results at 1280x960 native
shading, welded packet, edge energy floor / frieze / trousers: legacy 7.00 /
13.36 / 18.31; route C 6.59 / 15.04 / 15.92; reimagined 9.20 / 30.99 / 24.71
(`pbrify-kilik.png`, `pbrify-temple-right.png`, `pbrify-floor.png`): wood
grain, frieze carving and costume detail read at combat distance. Defect
and fix: with height maps on every material, parallax pulls neighbouring
atlas texels at silhouettes (green and red fringes on Kilik's costume,
`fringe-zoom.png`); without height 533791 pixels change and the fringes
vanish, so height stays only on the tiling floor material (final render
`pbrify-final-1280x960-native`, 8.93 / 31.33 / 21.79, `final-*.png`).
Exposure (user: "all of these look over exposed"): probes as supplied
profiles only (`pilot-curated/profiles/exposure-probe-a.conf`:
`rtx.localtonemap.exposure` 0.45, shadows 1.0, highlights 6.0,
`rtx.autoExposure.evMaxValue` 3, bloom off; probe B adds
`rtx.ignoreAllVertexColorBakedLighting`), clipped pixels 1.1% to 0.0% and
0.9% to 0.1% on the reimagined set, mean luminance 139 to 114; the
diagnostic radiance-3 headlight remains the main cause and is substep E's
first item. Not done: per-region metal masks for the mixed atlases,
delighting of painted shadows in the temple atlases (PBRify does not delight;
PBRFusion4 needs about 15 GB of extra models and a separate go-ahead),
moving-stability check (live path, F). Runtime option names verified in the
1.5.2 binary before use. Docs-only change; builds and selftests unchanged since LOG798 (868/0 x3, contract 260/0).

LOG799 pilot substep D, curated material set: candidate "route C" built and
rendered; technical ACCEPTED as a reversible layer, look NOT_REVIEWABLE
pending the user; two corrections to earlier evidence. (1) Control
correction: the runtime registers every subdirectory of `rtx-remix/mods/`
as a mod, the renamed `soulcalibur.off` junction included (helper log:
"Adding asset search path: ...mods\soulcalibur.off\"), so every earlier
"no-mod" render made by renaming the junction (LOG796 35 px, LOG798 398,
1375 and 332 px) was a mod-on repeat and measured only repeat noise. The
true control moves the junction out of `mods/` (`rtx-remix/mods-hidden/`,
driver fixed, `pilot-curated/pilot_render.py`). Measured against a true
legacy render of the welded source-2601 packet at 1280x960 native shading:
the AI draft layer differs by 753546 pixels, so the draft was never inert
against legacy; LOG796's channel proofs (override versus override inside
the mod) stand, LOG798's "draft maps are inert at every resolution" is
withdrawn and replaced by (3) below. (2) Inventory and classes: contact
sheet of the 26 captured originals (`pilot-curated/originals-sheet.png`);
reviewed classes skin (3), cloth (4), armour mix (1), weapon (1), lacquer
(1), wood floor (1), temple wood/paint (5), stone/bronze (2), gold and
silver grilles (2), alpha atlases (6), recorded in
`pilot-curated/curated_layer.py` (`MATERIALS`, `CLASSES`) with the
per-class roughness/metal constants. The 26 originals were ingested
unchanged (256x256, BC7, mip 0 at 46.2 and 38.6 dB against the capture)
through the MCP ingestion queue, one item per call, into
`assets/ingested/orig/`; the curated layer `layers/curated_pbr.usda` was
created and bound through the MCP (create_layer, override_textures,
save_layer) as the strongest sublayer of `mod.usda`, and the constants were
authored into that owned layer text (the MCP has no attribute-set tool);
the AI draft layer is untouched underneath. (3) Albedo routes, same frame,
edge energy (FIND_EDGES mean) on floor / frieze / trousers: legacy 7.00 /
13.36 / 18.31; route B (256 originals bound as replacements) 5.29 / 6.49 /
10.78, visibly smeared, the frieze pattern collapses to blobs; a 600-frame
render hit the 30 s watchdog, and profiles with
`rtx.neverDowngradeTextures`, `rtx.alwaysWaitForAsyncTextures`,
`rtx.nativeMipBias = -1` and `-2` (all confirmed in the log) change nothing
(5.26 to 5.34 / 6.48 / 10.8), so the replacement path samples a
256-texel map far below its top mip for reasons not identified here; route
D (originals upsampled 4x Lanczos, three materials) restores what it
touches (floor 6.29, trousers 15.90; frieze unchanged because that atlas was
not included); route C (the Toolkit AI tool's 1024 albedo, curated constants,
no AI normal or roughness maps) 6.63 / 15.08 / 16.06, sharp everywhere
(`albedo-routes-kilik.png`, `albedo-routes-frieze.png`). Stored values
are not the cause: mean colour of the floor map is 123.7/91.4/49.5
captured, 123.8/91.3/49.4 ingested original, 124.4/92.5/50.6 AI; all
albedo rtex files are BC7_UNORM (DXGI 98). Route C keeps the alpha
atlases on a 4x Lanczos upsample of the original with alpha (the AI output
is RGB). Constants take effect: route C constants against legacy-like
constants (0.7/0.1) differ by 123196 pixels, concentrated on the floor and
Taki. Candidate render `curated-routeC-1280x960-native` (crops
`routeC-kilik.png`, `routeC-taki.png`, `routeC-temple-right.png`). Tone:
trousers HSV saturation 124 legacy, 135 draft, 128 curated; the
"washed-out" impression from half-scale sheets was not supported by the
numbers. Not done: per-region masks for the mixed atlases, moving-stability
check (needs the live path, substep F), human look decision. Profiles used
only through `DXVK_RTX_CONFIG_FILE` (`pilot-curated/profiles/`); no
external configuration edited. Docs-only change; no source touched, builds and selftests unchanged since LOG798 (868/0 x3, contract 260/0).

LOG798 pilot substep C, higher-resolution visual reference (standalone,
non-performance evidence): resolution benefit demonstrated at a known real
shading resolution; the AI draft maps are inert at every resolution, so the
"replacement textures need resolution" hypothesis of LOG794 is withdrawn.
Route: D-226 gives the helper a diagnostic render size
(`FLYCAST_REMAKE_HELPER_RENDER_SIZE=WxH`, 4:3, 640 to 2560 wide, refused when
a live channel is active because return slots are 640x480 by contract; never
set by the launcher); the window, backbuffer, readback surfaces, returned
image, BMP and depth output follow it. Renders of the saved source-2601
packet with the draft mod loaded at 640x480, 960x720, 1280x960 and 1440x1080
(`pilot-resolution/`, `run.json` per render, exit 0). Real shading
resolution: the runtime's default profile (graphics preset Auto, upscaler
DLSS) does not report its internal resolution in the log, so the benefit
measurements use an explicit profile handed through `DXVK_RTX_CONFIG_FILE`
(`profiles/native-shading.conf`: `rtx.upscalerType = 0`,
`rtx.resolutionScale = 1.0`; the log confirms both values), under which the
internal shading resolution equals the helper backbuffer; a second profile
(`reference-accumulation.conf`: `rtx.useDenoiserReferenceMode = True`,
100 accumulated frames on the frozen packet scene) gives the art reference.
Results at 1280x960: draft mod against no mod (junction hidden) differs by
398 pixels under the default profile (repeat noise 1188), 1375 under native
shading and 332 under reference accumulation; the 1280 crops
(`native1280-nomod-vs-mod-*.png`) are indistinguishable. The draft maps are
therefore visually inert regardless of resolution, and the earlier reading
that they "need a higher consumer resolution" is withdrawn. Cause, measured on
the floor material: the AI diffuse (1024x1024, BC7, 11 mips) is 33.9 dB PSNR
from a bilinear 4x enlargement of the original 256x256 texture (34.7 dB after
box-downsampling back to 256), with edge energy 3.45 against 1.6 for the
bilinear enlargement and 6.45 for the original at its own scale: the upscaler
adds a little sharpness and no detail, and the draft normal and roughness maps
were already shown inert (LOG796). Binding, mips and sampling are fine: a
1024-texel diagnostic checker (8-texel cells) bound to the floor through the
MCP override renders as a visible fine checker at 640, 1280 and 1440
(`checker1024-floor-640-1280-1440.png`; aliased at 640, cleanly resolved at
1280 and 1440), so high-resolution texel detail reaches the screen when a
map actually carries it. Resolution benefit with the original textures
(`ladder-kilik-640-960-1280-1440.png`, equal on-screen scale): edges, painted
costume detail and shading read progressively cleaner from 640 to 960 to
1280; 1440 adds little over 1280 at this viewing scale. Cost (standalone,
rough): 120 frames plus readback took about 3.4 s at 640, 3.7 s at 1280 and
3.9 s at 1440 on this GPU (helper wall time minus the 15 s mod-loading wait
and 1 s linger), i.e. about 2 to 4 ms per frame more than 640; the live cost
belongs to substep F. Recommendation for F: 1280x960 as the first live test
point (960x720 is a clear but smaller gain). Consequence for D: curation
starts from the original artwork with authored detail, not from the AI 4x
output. Renders of the welded packet at 640, 1280 and 1440 under native shading (smooth-*-native, crops smooth-kilik-640-vs-1280-vs-1440.png, smooth-taki-640-vs-1280.png, flat-vs-smooth-1280.png) after the user's note that the ladder had been rendered from the flat packet; the user keeps the smooth fix. Builds: four configurations serial, 0 errors (the off build had no work). Selftest 868/0 in automation, baseline and no-ngx; remake-sdk-contract 260/0; python suites pass.

LOG797 pilot substep B: same-source smooth-normal comparison (D-225 weld) and
the per-material alpha candidate; technical ACCEPTED for both as reversible
options, look NOT_REVIEWABLE pending the user. (1) Same-source normals: live
capture sessions cannot pin a source parity (the feed alternates sources, so
`--capture-start-source 2601` yielded 2602/2604/2606 in
fc075-abcap-d224-smooth-2601 and 2600/2602/2604 in the LOG794 session), so
the same-source A/B uses the saved source-2601 packet: the new
`neuraltest/remake_packet_normals.py` rewrites only the normals of a flat
packet with the D-225 level-2 rule (refusing a packet that is not flat, which
also prevents smoothing twice) and the standalone helper renders both under
identical lighting (`pilot-normals/`: flat-1, flat-2, smooth2-1, smooth2-2;
48 meshes, 8946 triangles, 8078 welded vertex groups of 26838 expanded
vertices). Result: flat versus smooth 14648 to 15310 pixels changed (mean
58) against repeat noise 76 and 541; the returned depth images agree in
coverage on all 307200 pixels (38 pixels differ in value by more than 1e-6,
float noise), so silhouettes are unchanged; crops `normals-same-kilik.png`
and `normals-same-taki.png`: the per-facet steps on legs, torso, arms and
Taki's suit are gone, belts, armour plates and the blade keep their edges,
no seam artefacts visible at 3x. D-225 level 2 (`FLYCAST_REMAKE_SMOOTH_NORMALS=2`,
launcher `--smooth-normals-weld`) is implemented in the exporter: within one
source draw, vertices whose source position, texture coordinate and base
colour are bit-identical (the same logical vertex the game resubmits for a
neighbouring strip) share a smoothing group; coincident positions with
different attributes are never welded; the 60 degree crease is unchanged;
level 1 (index-keyed) remains available. Toolkit smoothing capability: the
installed Toolkit has no mesh-normal smoothing tool (its material/texture
tools and the MCP tool list were inspected); the runtime lights the normals
it receives. Moving evidence with level 2 live: fc075-abcap-d225-weld-moving captured 40 frames (sources 2561 to 2603) through an attack, a cross and hit effects; the eight-frame review strip (moving-strip.png) shows curved fighters with no visible shading pops or seam errors, silhouettes and native effects unchanged; consecutive returned-image changes rise monotonically with the motion (3078 to 98947 pixels) with no isolated spike. Performance-eligible fc075-perf-d225-weld-a (OIT route, level 2 on): 99.35 percent fresh of steady presents (99.72 of remake presents), 9 output repeats, latency mean 3.71 (max 4), present p50 20.01 ms, p95 24.59, p99 27.28, GPU span p50 12.43; VRAM growth 946 MB with 45 owned-object growth (the LOG794 alternating pattern, still unattributed): the weld costs nothing measurable at the gate. (2) Alpha candidate: `--alpha-combined-off`
(promoted alpha surfaces kept native, not exported) captured the same sources
2601/2603/2605 as the baseline (fc075-abcap-d224-alphaoff-2601; the log has 0
alpha-ownership lines against 595 in the baseline session; original-native
images identical, 0 pixels). Composited images differ by 86406, 89371 and
87648 pixels (mean 57): the lattice, banners and rail balusters render as the
original native surfaces composited over the Remix scene instead of bright
translucent panels (`alphaoff-lattice.png`), and the floor shows the native
shadow blobs instead of glass shadows (`alphaoff-floor.png`); returned
image 119428 pixels. Native blend order and masks are the original's by
construction (the surfaces are not exported); occlusion of native surfaces
by Remix geometry is not handled on this route (they composite over), which
the review must weigh. The decal/opaque treatments are not started: they
need per-material source semantics that the D-183 route does not carry yet.
Builds: four configurations serial, 0 errors. Selftest 868/0 in automation, baseline and no-ngx; remake-sdk-contract 260/0; launcher, manifest, packet-normals and backlog python suites pass (18, 2, 3, 6 tests).

LOG796 pilot substep A, material-channel proof through the Toolkit MCP server:
ACCEPTED for albedo, roughness and normal on one stage and one fighter
material; two Toolkit limitations recorded. Route: the new
`neuraltest/remix_mcp_client.py` (standard-library SSE/JSON-RPC client) drove
`lightspeed.trex.mcp.core` 1.2.2 on 127.0.0.1:8000: open project, create the
diagnostic sublayer, ingest the diagnostic maps, override textures, save,
restore, remove the layer. Renders: standalone helper (build-neural-automation
`remake-runtime-smoke.exe` sha256 7b8abbf9...) on the saved source-2601 packet,
120 frames, anchored light, 640x480, evidence `pilot-channel-proof/` with
`run.json` per render (all exit 0; not performance evidence). Repeated-baseline
noise (max per-channel difference above 24 of 307200 pixels): with-mod repeats
9 to 976 pixels (mean magnitude about 32), no-mod repeats 314 to 627. Mod versus
no-mod with the mod junction hidden: 35 and 36 pixels, i.e. the AI draft is
invisible at 640x480 as LOG794 found. `DXVK_DISABLE_ASSET_REPLACEMENT=1` is not
a valid no-mod control: it also drops the helper's API light (about 115000
pixels changed). Two Toolkit findings: (1) a sublayer of mod.usda is weaker
than mod.usda's own opinions, so an override layer only works when mod.usda is
a thin root whose material opinions live in a sublayer; the project was
restructured to `mod.usda` (root) plus `layers/ai_pbr_draft.usda` (the D-222
typed defs, unchanged content) and the diagnostic layer was inserted first
(strongest). The runtime composes the sublayers (renders identical to the
single-file mod: 21 pixels against base-mod-1). (2) The REST/MCP texture
override drops its `force` flag before reaching the core
(`replace_texture_with_data_models` calls `replace_textures(body.textures)`),
so non-ingested maps are silently skipped with "OK"; the diagnostic maps were
therefore ingested through the MCP ingestion queue (a six-item batch converted
only the even items; the three others succeeded one at a time), and the MCP
`create_layer` schema's LayerType enum reference does not resolve (layer type
omitted). Diagnostic maps (uncompressed DX10 DDS, ingested to BC7 `.rtex.dds`):
flat green and magenta albedo, roughness 0 and 1, a 45 degree 8x8 checker
normal and a flat normal. Results against base-mod-8, coverage mask = pixels
where the green and magenta renders differ by more than 80 in R and G:
floor `78918ECF7600A708` (mask 75144 px): green 75144 inside (mean 93) plus
153522 outside (mean 36, the green floor's bounce over the whole scene, visibly
a tint); roughness 0: 61089 inside (temple reflected in a mirror floor) and
46103 outside; roughness 1: 2224 inside (the draft roughness is already near
matte); normal checker: 51414 inside (tilted tiles, visible) and 193318 outside
(shadow and bounce changes plus floor area the albedo mask misses in shadow);
flat normal: 419 inside, 67 outside, so the AI draft normal map is visually
inert. Taki mask/top/shoes material `940953E6DC0A196B` (mask 1625 px): green
1625 inside (mean 153) and 2703 outside (mean 38, bounce and edges);
roughness 0: 589 inside; roughness 1: 625 inside; normal checker: 722 inside;
flat normal: 4 inside. Controls: a missing replacement (ingested green map
bound, file hidden) renders within noise of the baseline inside the floor
mask (44675 pixels at mean 34, the same low-magnitude tint band as repeats
over the floor) and the runtime logs "asset data cannot be found or
corrupted", i.e. the original is used; a wrong binding is caught by the
manifest tool's slot-suffix check (unit-tested) and the earlier normal-in-
diffuse control (LOG794) remains the runtime evidence; mapping survives
restart: base-mod-1/8/9 are three helper starts, 21 to 976 pixels apart. The
manifest tool now reads mod sublayers and reports wrong-slot and missing
bindings (none in the draft). Review sheets `floor-sheet.png`,
`taki-sheet.png`, `albedo-sheet.png` are in the evidence directory. Not
established: any visual benefit of the draft maps (none at 640x480), mip or
tangent behaviour beyond "DX normal convention renders with the expected
tilt direction", and anything about motion. Next: substep B (same-source
smooth-normal comparison and the per-material alpha candidate).

LOG795 D-224 pilot opened; documents reconciled; scene inventory and material
manifest. State on entry: HEAD 81654643daeaf0b4333978ee4a99da649dd40a6b equal
to fork `feat/neural-rendering`; worktree carried only the private untracked
items (four build logs, `metrics.txt`, `remake-runtime-smoke.dxvk-cache`,
`rtx-remix/`). Revised in place: BACKLOG (checkpoint, pilot substep table A-H
under FC-067/M2-scene, M3, M4, hardening cards; superseded next action),
AGENTS (pilot rules), REMAKE-FEASIBILITY-PLAN (M3/M5 wording), QUALITY-PLAN
(pilot visual criteria), DECISIONS (D-224), handoff and REMAKE-LAUNCH.
Inventory (substep A): stage Hoko Temple, fighters Kilik (red/gold costume,
staff) and Taki (magenta costume, blue/grey armour), identified by eye from
the captured textures; runtime remix-1.5.2+68edea01 (helper log), Toolkit
1.5.2.0; capture `capture_2026-09-10_17-52-53.usd` from the saved source-2601
packet (`fc075-abcap-d220-baseline`), 48 meshes, 26 materials, 26 textures,
all 256x256 R8G8B8A8; legal media Soulcalibur (USA) CHD only. The new
`neuraltest/remake_material_manifest.py` joins each captured runtime material
hash to the helper's texture identity and content digest by identical pixel
payload (the DDS headers differ, so whole-file digests do not join): 26 of 26
joined, no unmatched packet texture, no captured material outside the packet;
the digests equal the helper's `texture_register` lines of
`fc075-perf-d222-mod-a`. Toolkit MCP discovery: the installed Toolkit ships
`lightspeed.trex.mcp.core` 1.2.2 (FastMCP, SSE transport on 127.0.0.1:8000,
the REST API of port 8011 mounted as tools under the `remix` prefix, plus a
model-replacement prompt); it listens while the GUI (kit.exe) runs. Per the
user's rule it is the required Toolkit route for the pilot; the scripted
kit.exe invocations of D-222 stay as the fallback when the GUI is closed. Usage from the packet: 19 opaque materials, 7 alpha-
blended (D-183 promoted; 4 with binary source alpha, 3 graded), none cutout at
source 2601; 4 stage materials are shared by 4 meshes each; all 26 have
diffuse/normal/roughness replacements present. The manifest is committed as
`docs/neural/soulcalibur-remix-manifest.json` (hashes and relative paths, no
artwork). Not established here: channel correctness (LOG796), any visual
benefit, full-title coverage.

LOG794 D-221/D-222/D-223: the re-anchor keeps its returns, the Remix Toolkit
pipeline runs end to end, and the faceted look is attributed. (1) D-221: at
the source-3099 re-anchor the host no longer discards the three returns in
flight (accepted evaluations of pre-cut sources 3096 to 3098, presented in
source order); the temporal/raster history and presentation carry-over are
retired at the first post-cut return instead (`Remake anchor history
retired: source=3101 after_source=3099`). Performance-eligible fc075-perf-d221-a and -b (OIT route, D-221 build, no mod loaded): 99.35 and 99.35 percent fresh of steady presents (99.72 percent of remake presents), 9 output repeats each, latency mean 3.79/3.08 (max 4), no identity fault; the re-anchor now costs four held-native presents and no automatic present; present p50 19.44/19.27 ms, p95 24.39/23.91, p99 27.25/26.51; VRAM growth 945 and 408 MB (the alternating 946/408 MB pattern with 45/39 owned objects stays unattributed). The fresh-output criterion of the 600-frame gate is therefore met on the OIT route in two runs; the normal-renderer half of the gate cannot be measured because the native-effects lane does not activate there (LOG792), and the VRAM/object attribution the gate asks for is still open.|fc075-perf-d222-mod-a (mod loaded, 78 replacement DDS): 99.54 percent fresh, helper draw p50 2.67 ms against 2.72 without the mod, present p50 19.33 ms; no measurable cost at 640x480. (2) D-222, Remix
USD capture without the GUI: the runtime's documented
`DXVK_RTX_CAPTURE_ENABLE_ON_FRAME=<frame>` triggers a capture, and
`DXVK_DISABLE_ASSET_REPLACEMENT=1` is required (the capturer refuses while
replacement assets are enabled, including the helper's API light). The helper
gained two diagnostic environment bounds, never set by the launcher:
`FLYCAST_REMAKE_HELPER_LINGER_MS` (keeps the runtime alive after the last frame
so the export finishes) and `FLYCAST_REMAKE_HELPER_STARTUP_WAIT_MS` (lets a mod
finish loading before the first frame). Running the helper standalone on the
saved source-2601 packet (`remake-view.bin` from the fc075-abcap-d220-baseline
capture) produced `rtx-remix/captures/capture_2026-09-10_17-52-53.usd` with 26
textures (R8G8B8A8, DXGI 28), 48 meshes and 26 materials named by the
runtime's material hash. Toolkit 1.5.2.0 (user-installed through the NVIDIA
App; the source clone at tag 2024.5.1 was not built): a project was created
with the wizard core (junctions, not elevated symlinks; the shipped CLI wrapper
passes string paths that its own validators reject), the 26 textures went
through the AI PBR generator (diffuse 4x, normal DX, roughness; PNG, RGB only),
the source alpha was resampled back into the six diffuse maps whose source
alpha is not opaque (cutouts and the promoted alpha surfaces), the material
ingestion converted the 78 maps to BC7 `.rtex.dds` with octahedral normals,
and `mod.usda` was authored from the material hashes. The runtime's mod loader
requires typed prims (`def Material` with a child `def Shader` that is a
UsdShadeShader; untyped `over` prims are skipped silently, verified by a
wrong-texture control: 294093 of 307200 pixels changed once typed). With the
real textures the standalone render of the same packet differs from the no-mod
render by 9469 pixels against a run-to-run noise of about 9000: at the lane's
640x480 consumer output (the runtime renders the helper's 640x480 backbuffer,
with its own upscaler below that) the 4x textures and generated PBR maps make
no measurable difference. The texture work therefore needs a higher consumer
render resolution, which changes the return image contract (640x480 slots) and
the host's evaluation input, and costs path-tracing time; a user decision.
Gameplay cost of the loaded mod: fc075-perf-d222-mod-a (mod loaded, 78 replacement DDS): 99.54 percent fresh, helper draw p50 2.67 ms against 2.72 without the mod, present p50 19.33 ms; no measurable cost at 640x480. (3) D-223: the
user's observation that the characters look faceted is the flat face normal
the export assigns (the source stream carries pre-lit colors and no normals),
lit per facet by the path tracer where the source's baked Gouraud colors hid
the polygon count. `FLYCAST_REMAKE_SMOOTH_NORMALS=1` (launcher
`--smooth-normals`, default off) averages face normals per source vertex
within a 60 degree crease. Measured in the fc075-abcap-d223-smooth-normals capture session (source 2602 against the flat-normal source 2603 of the fc075-abcap-d220-baseline capture): the per-facet shading steps on the legs, torso and arms are gone and surfaces read as curved; the polygon silhouette is unchanged because the geometry is unchanged. Skips in that session: 605 worker-busy native fallback, 11 no-return credit, 1 support change; capture runs are not performance-eligible. Look decision with the user. Selftest 868/0
(automation, baseline, no-ngx), remake-sdk-contract 260/0, launcher tests 16.

LOG793 D-220: the helper's first-packet startup, the translucent look, texture
identity across sessions, and the re-anchor stall. (1) First-packet startup:
the helper received its first live packet before creating its device, so the
runtime's own startup (device, shaders, Reflex: 16:30:48.9 to 16:30:52.9 in
fc075-cpu-d220-a, about 4 s) was spent holding the host's three sources;
shortening the synthetic warmup from 60 to 8 frames alone did not help (prepare
4529 ms, 185 credit skips). The live return-only session now receives its first
source after startup and warms for 8 frames: fc075-cpu-d220-b first-packet
prepare 231 ms, 12 credit skips (185 and about 150 before). (2) Attribution of
the slower emulated frame: CPU-timing fc075-cpu-d220-b against
fc075-submit-timing-r (D-218 build) shows a uniform 8 to 10 percent rise across
the emulator-thread scopes (frame-submit-neural 9.3 to 10.4 ms, scene-feed 3.0
to 3.4, returned-evaluate 4.6 to 5.0, view-scene 1.09 to 1.17; frame-gap
unchanged 4.7), not one stage; consistent with contention from the added
threads, not attributed further. VRAM growth: 406 MB in fc075-cpu-d220-b and
fc075-perf-d220-c, 948 MB in fc075-perf-d220-d; the 946/948 MB runs end with
194 owned objects (the maximum) against 188, six renderer objects not released
by the run's end; not attributed further. (3) Performance-eligible
fc075-perf-d220-c and -d (OIT route, D-220 helper): present p50 18.82/18.95 ms,
p95 25.07/24.91, p99 30.25/29.23; 8 and 10 output repeats, latency mean
3.01/3.71 (max 4), no identity fault; gate reading 98.98 and 98.89 percent
fresh of steady presents (99.63 percent of remake presents). The non-fresh
steady presents are 11 and 12, of which 8 to 9 sit at samples 998 to 1006 in
both runs: the in-session re-anchor at source 3099 (D-207; support report
last_accepted 1012, shared_last 424, rotation 108 degrees from the reference)
produces five automatic presents (the new generation's pipeline latency) and
two to three held-native presents, one of them a 266 ms present interval: the
first evaluation after the history reset re-initialized the motion raster
(D3DCompile of both shaders, about 240 ms between the geometry-motion and
GPU-guidance lines). The raster holds no cross-frame history (its retained
output is a separate object that is still reset), so retirement now keeps it.
The support report logs frames_since_last. (4) Texture identity: the helper
logs a content digest (FNV-1a over the DDS bytes) per texture registration;
fc075-perf-d220-c and -d registered the same 36 keys with 36 identical
digests, no re-registration with changed bytes, so replacement assets keyed on
texture content would match across sessions. (5) Translucent look, bounded A/B
at source 2601 (capture runs, not performance-eligible): alpha-combined off
changes 43294 of 307200 pixels of the returned Remix image against the
baseline capture, concentrated in the gate interior lattice, the banner
fringes and the floor line (the promoted alpha surfaces render as bright
translucent material where the source blends a dark interior); opaque alpha
forced to one changes 5571 pixels. The translucent look is therefore the D-183
promoted alpha surfaces ray-traced as translucent, not opaque-list texture
alpha. Composition unchanged; the two controls stay launcher options. (6) The
RTX Remix Toolkit is not installed; its source clone was refused by the
session's permission classifier and the NVIDIA App route needs a desktop GUI;
no third-party binary was inspected or acquired. Performance-eligible fc075-perf-d220-e and -f (raster retained, same helper): present p50 18.14/18.04 ms, p95 23.94/23.47, p99 30.44/32.19; the re-anchor's held-native presents now take 15 to 22 ms (no 266 ms present) but the re-anchor still costs 8 to 10 presents (four to five automatic, three to five held-native), and scattered single repeats vary between runs (16 and 20 repeats against 8 and 10 in -c/-d, several with 9 to 10 ms present intervals: a present before the next evaluation), so the gate reads 98.52 and 97.96 percent fresh of steady presents; not passed. frames_since_last=0 at the rejection: the last accepted source is the previous frame, a genuine one-frame cut (D-207). Selftest
868/0 (automation, baseline, no-ngx), remake-sdk-contract 260/0, launcher
tests 16.

LOG792 D-219: the helper's turnaround, the credit skips, and the gate again.
Credit-skip states (`no-return-credit` skips now log the channel's sequence,
returned sequence, sources and slot states): in fc075-submit-timing-o, 157
of 239 skips found all three transport slots ready and unreceived, so the
helper, not the host, held the sources. Its receive phase was 4.9 ms p50 of
receive work (deserialize, byte-serial receipt digest, texture references),
not idle time, and its turnaround 13.3 ms: draw 2.5, GPU readback lock
3.4, depth lock 0.5, depth conversion 1.4, return 2.6 (two byte-serial
digests over 2.4 MB). Changes: (1) the returned image and depth transport
digests (ImageSlot fields only, never persisted, both ends share the
function) hash eight bytes per step: return 2.6 to 0.9 ms, host return
worker unchanged at about 4.2 ms; (2) the helper receives the next packet
while the GPU completes the current readback (lock wait 3.4 to 0.33 ms);
the first version waited for the next packet without bound and delayed the
return (turnaround 21 ms, latency mean 3.78 frames), so the wait is bounded
at 3 ms and a timeout is not an error; (3) named auto-reset events replace
the helper's `Sleep(2)` receive poll and back the host's return worker poll,
so neither wait is timer-resolution bound (small effect measured, kept);
(4) `--renderer {dx11-oit,dx11}` on the launcher. Skip histogram
(fc075-submit-timing-q, -r): 143 and 153 of 187 and 160 credit skips fall
in the first 200 frames after the first feed, the helper's first-packet
startup (the first packet carries every texture, about 10 MB); steady-state
skips 7 to 44 per 1000 frames. Performance-eligible fc075-perf-d219-a (OIT
route): present p50 19.03 ms, p95 27.04, p99 33.55; 1038 remake presents,
1036 accepted, 12 output repeats, latency mean 3.50 (max 4), no identity
fault; gate reading (per-present samples, first 120 measured presents as
warmup, a repeat counted stale): 1029 fresh of 1080 steady presents (95.3
percent), 99.1 percent of the 1038 remake presents; helper turnaround 20.1
ms p50 with the bounded prefetch, period 19.2, draw 2.5. The present p50 of
19.03 against 16.8 to 17.5 in the LOG790/791 runs is a slower emulated frame
(PVR source-join period 19.0 against 17.0 in fc075-perf-d218-c/-d) and is
not attributed (no CPU timing in a performance run); VRAM growth 946 MB
against 408 MB in those runs, not attributed. fc075-perf-d219-b is
discarded: the host's swap chain `ResizeBuffers failed: 887a0001` at 00:24
(before the lane started) re-initialized the renderer, advanced the session
generation twice, and the retired helper did not exit within the launcher's
8 s, which aborted the launcher; an external display event, and a launcher
robustness gap (the retired helper's exit is bounded by the runtime, not by
the launcher). fc075-perf-d219-c (`--renderer dx11`): the lane never
activated; every source skipped with `native-effects/unsupported-renderer`,
so the gate's normal-renderer run cannot be made with the native-effects
lane on this route. The 600-frame gate is therefore still not passed (95.3
percent of steady presents, one route). User observation recorded for
the queue: some textures look almost translucent in the combined image; the
code shows two candidate mechanisms, neither verified against the image:
the D-183 promoted alpha surfaces (six per frame here) are exported with
their source alpha and drawn with SRC_ALPHA/INV_SRC_ALPHA and no depth
writes, which the consumer ray-traces as translucent materials whereas the
source blends them over the opaque geometry beneath; and opaque-list meshes
pass the texture alpha through (`ApplyLegacyAlpha`: texture alpha selected
regardless of the source's ignore-texture-alpha bit) while their blend state
is off. Selftest 868/0 (automation, baseline, no-ngx), remake-sdk-contract
260/0, launcher tests 16.

LOG791 correction to LOG790: the live channel's receipt digest is persisted
in the locked archives and recomputed byte-serially on replay
(`remake_input_replay.cpp`, LOG780), so the word-wise digest committed with
D-218 would have made every archive written by that build fail
`archive-source-receipt-mismatch` on replay. Reverted to the byte-serial
FNV-1a before any archive was written with it; no evidence run in LOG790 used
a locked archive. The LOG790 publish figure (3.5 ms) was measured with the
word-wise digest; the byte-serial digest costs about 1 ms more on the feed
worker, off the present path. Re-measured with the byte-serial digest,
performance-eligible fc075-perf-d218-d: present p50 17.05 ms, p95 26.10,
p99 32.72; 1035 remake presents, 940 accepted, 105 output repeats, latency
mean 3.49 (max 6), no identity fault; helper period p50 19.3 ms, p90 25.4.
Against the 600-frame gate (BACKLOG exit criteria: after at most 120 warmup
presents, at least 99 percent of steady eligible presents show actual
combined output; no stale fallback, identity fault, unbounded wait or
growing owned-resource count; latency and P50/P95/P99 reported), computed
from the per-present samples with the first 120 measured presents as warmup
and an output repeat counted as stale: fc075-perf-d218-c 1018 fresh of 1080
steady presents (94.3 percent; 98.2 percent of the 1037 remake presents),
fc075-perf-d218-d 933 of 1080 (86.4 percent; 90.1 percent of 1035). The gate
is not passed. Owned GPU objects 143 to 188 (maximum 194) and VRAM growth
408 MB in both runs, the same in each, reached once the lane starts (not
attributed to a leak; not yet proven either). The run-to-run spread (22
against 105 repeats on the same build) follows the helper's period: its
receive-draw-readback-return sequence takes about 13 ms p50 and 19.5 ms p90
per packet against a 16.7 ms frame period, so any jitter backs the three
credits up. Only the OIT renderer route has been measured; the normal
renderer run the gate also asks for has not been made.

LOG790 D-218: the host present interval reaches the emulated frame period.
Performance-eligible run fc075-perf-d218-c (no CPU timing, replay, anchored
light, managed session): present interval p50 16.77 ms, p95 24.98 ms; 1200
presents, 1037 remake presents, 1025 accepted evaluations, 22 output repeats,
latency mean 3.11 frames (max 5), no source-frame repeat or gap, no identity
fault, GPU timestamp span p50 9.34 ms; one `anchor-source-support-changed`
in-session re-anchor (source about 3099, points 1000, shared with last
accepted 424; the same source region passed as a view cut in earlier runs, so
it is cadence-dependent support overlap, not an observation change; not
attributed further). The preceding run of that build with three sources in
flight measured the same (fc075-submit-timing-m, CPU timing on: emulation
period 17.2 ms, render 13.1 ms, gap 3.7 ms, present p50 17.0 ms, 1025 of 1036
accepted). Controls from LOG789 stand: native 11.1 ms, DLAA 12.6 ms; the
present interval now sits at the emulated 60 Hz period, not below it.
Findings in order, all with the CPU timing diagnostic (runs
fc075-submit-timing-c to -m under D:\Flycast-Evidence). (1) Every remake CPU
scope now samples only once the lane is active (the sub-stage scopes used to
sample the first 600 feeds, which start before the first evaluation; the
LOG789 sub-stage numbers are from that earlier window). With consistent
windows the unattributed 4.7 ms inside `frame-submit-neural` was
`submit-capture-geometry`: native draw correspondence (`geometry-match`
3.9 ms) computed for a lane that never submits native guidance; it is skipped
when `FLYCAST_REMAKE_ASYNC_NEURAL=1` with a channel (matches empty, previous
positions untrusted), 4.74 to 0.82 ms. Native paths are unchanged. (2) The
feed worker's publish (6.6 ms) was the packet serializer writing one word per
stream call and the consumer reading the same way; the writer now stages the
identical bytes and writes once, the live channel deserializes from the
mapped payload directly (files and the parity check keep the stream path);
the byte-serial receipt digest is unchanged, because the locked archives
persist it and the replay recomputes it (LOG780): publish 6.6 to 3.5 ms
measured with a word-wise digest that was then reverted (LOG791). Packet
build without per-triangle heap use: 3.5 to 2.1 ms. The anchor's per-vertex
embedding runs in chunks on worker threads with order-free reductions and
the first failing vertex in packet order still deciding the error: 4.5 to
3.5 ms. Feed worker 15.9 to 10.9 ms. (3) The emulation thread was then the
limit (period about 21 ms against about 11 native). Per-hook cycle
accounting (`--hook-cycles`, diagnostic, inflates the period by several ms)
attributed it: SQ writes 4.5 ms, RAM stores 3.0, block entries 2.5, register
reads 1.6, arithmetic 1.1, boundaries 0.6, ftrv 0.5. Cuts that keep every
observation: the SQ observation record (about 600 bytes) is built in place in
a batch whose storage is retained through a pool instead of being copied
into a vector reallocated every frame (SQ writes 4.5 to 1.0 ms); the RAM
store ring keeps a 24-byte hot record with the transform payload in a
parallel array consulted only on a serial match; per-register live-origin
bytes let the recompiler skip the register-write boundary call for registers
holding no live origin (596 000 to 81 000 calls per frame; the skipped call
only reset dead entries) and let block-entry validation visit live registers
only. Emulation period 21.8 to 16.5 ms (fc075-submit-timing-k), the emulated
60 Hz period. (4) With the frame at 16.5 ms the feed-return round trip (feed
worker about 11, helper turnaround about 13, return worker about 4 ms plus
queueing) no longer fit two sources in flight: 394 credit skips and 345
output repeats. The channel now keeps three sources (and image slots) in
flight (`kInFlight`); credit skips 169, repeats 22, latency +0 frames at the
mean (3.1 against 3.5 with the repeats). Helper draw time returned to 2.45 ms
p50 in that run (5.2 to 6.1 while the pipeline starved), consistent with
CPU contention rather than the packet. Helper `live_return` lines now carry
`period_ms`, `receive_wait_ms` and `prepare_ms`. Selftest 868/0 (two channel
expectations added for the third slot) in automation, baseline and no-ngx,
remake-sdk-contract 260/0, launcher tests 16. Not claimed: quality, the
600-frame gate, or anything about the consumer's rendering; the anchored
scene remains `diagnostic-camera-embedded-anchor-not-world-reconstruction`.

LOG789 D-217: whole-frame attribution, persistent evaluation resources, and
the source-observation hooks. Whole-frame scopes (`frame-process`,
`frame-render`, `frame-pvr-draw`, `frame-submit-neural`, `frame-display`,
`frame-present`, `frame-gap`) and emulation-thread probes
(`emu-frame-period`, `emu-wait-frame-finished`, `emu-wait-render-end`) now
sample once the lane is active; the earlier frame scopes sampled the warmup.
Findings in order. (1) Deferring output ownership to the next frame
(fc071-deferown-a/b/c) removed the 1.9 ms acquire wait but moved the same
wait into the input upload (5.7 ms), present p50 29.7 against 28.2; reverted.
(2) Per-evaluation resource creation was the cost: the inverted-depth upload
texture and the motion raster's six targets, depth, upload textures and
buffers were created every frame. They now persist (two output sets so the
retained previous draw IDs are never the set being rendered; dynamic buffers
mapped on the raster's deferred context) and one D3D11on12 acquire covers the
upload, the raster and its copies (fc072-persist-timing-a): returned-evaluate
9.0 to 5.0 ms (raster 2.4 to 1.3, ownership 3.4 to 1.4, upload 0.6 to 0.3),
present interval unchanged at 30 ms. (3) The whole-frame timeline explained
why: the render thread's frame (17.5) plus a 10.6 ms gap in which it idled,
while the emulation thread's period was 27.8 ms; the two threads were
serialized by QueueRender waiting for the previous render and by the
emulation thread itself taking about 19 ms per frame, against about 11 in
the native control, because the source-observation recompiler hooks the
anchored lane depends on (LOG7xx camera anchor: `xyzTransforms` from observed
ftrv and RAM stores) ran about 250 000 store observations, 2.1 million
register-write boundary calls and 363 000 block-entry validations per frame.
(4) Hook cost cut without changing what is observed: the RAM-store observer
resets only its scalar fields and looks the derived-store origin up in one
call (the storing register rides in the upper half of a 4-byte value) instead
of a preparation call before every store; the register-write boundary, the
mov32 origin copy and the block-entry validation return immediately when no
origin is live, and the recompiler now skips those calls inline through a
non-thread-local mirror of the live-origin count (`sourceArithmeticLiveFlag`),
so 2.1 million boundary calls per frame became 0.6 million and 363 000
block-entry calls became 82 000 (fc074-storehook-timing-a,
fc074-liveflag-timing-c): emulation period 28.9 to 21.7 ms, host present
interval p50 30.8 to 21.6 ms with CPU timing on, no identity, gap or repeat
fault, anchor rejections one support change in the first run and none in the
second. Accepted evaluations
fell to 818 of 1200 with 261 output repeats: the helper's draw time rose from
2.5 to 5.4 ms while the emulation thread ran hotter, which reads as CPU
contention between the emulator, the workers and the consumer's own threads;
not yet attributed. The atomic hook counters themselves cost about 3 ms per
frame (fc074-fastpath-timing-b, discarded); they are plain thread-local
counters now. The lane is now bound by the render thread: process about 4 ms
plus render about 17 ms (submit-neural 13.1 of which feed 3.0, evaluation 4.7
and about 5.4 in the native neural export path; display 2.0; PVR draw 1.0),
all with CPU-timing logging inflating each. Next: attribute the 5.4 ms native
neural export path and the process step, then re-measure without CPU timing.
Selftest866/0, remake-sdk-contract260/0, launcher tests16.

LOG788 consumer configuration sweep; GPU-sharing attribution withdrawn. The
launcher gained `--consumer-config PATH`, which hands a user-authored rtx.conf
to the consumer through its documented `DXVK_RTX_CONFIG_FILE` override (the
1.5.2 runtime logs "Found config file" and the parsed keys); the launcher
writes no configuration and records the path and digest. No rtx.conf existed
before, so every earlier run used the runtime's Auto preset, which resolves to
Ultra on this GPU. Three variants, same settings as LOG787 (1200 frames,
replay, unlimited lane), read at the same points: (a) bounce cut (path bounces
1, ray interactions 2/1/1, PSR bounces 1, combined denoising, DI samples 2/1):
present p50 28.5 ms, helper draw2.55, turnaround15.8; (b) graphics preset
Low: 28.6, draw2.50, turnaround15.3; (c) diagnostic minimum (preset Low, ray
reconstruction, denoiser, volumetrics, bloom and upscaler off, zero bounces;
not a visual candidate): 29.5, draw2.53, lock wait2.8, turnaround12.5;
baseline (Ultra) 28.2, turnaround15.1. Frame GPU span stayed17.9 to18.8 and
the PVR pass13.6 to14.0 in all four. Reading: the consumer's rendering cost
does not reach the host present interval at all, so the LOG787 statement that
the lane is GPU-bound by sharing near26 ms is withdrawn. The span above the
11.5 ms control is the host's own frame with the render thread's device-bound
remake work inside it (acquire wait2.0, motion raster2.4, composite1.6,
snapshot1.4, view scene1.3, submit1.0; LOG786), which stretches the timestamp
span without consuming GPU. Option (c) of the LOG787 decision is closed by
measurement; the consumer configuration stays the user's and no variant is
recommended for visuals. Variant runs are diagnostic, not the evidence lane.
Launcher tests16.

LOG787 helper CPU cut and D-216 frame budget. Helper: readback surfaces and
CPU buffers persist across images, the depth copy walks rows instead of
307200 memcpy calls, and texture bytes registered by the host live in
immutable shared storage that the legacy uploader references by identity, so
no texture bytes are copied or compared per image (the diagnostic ownership
step skips referenced meshes; the re-upload rule is unchanged: different bytes
for an identity replace the storage and rebuild the resource). Timing run
fc067-perf-helpercut-timing-c (same settings as LOG786): helper turnaround
15.1 ms p50 (19.7), draw2.6 (5.9), lock waits4.0 and0.5; host present interval
p50 28.2 ms (31.4),1107 of1200 presents combined,1108 accepted, repeats7,
latency mean2.85 (max3), no identity, repeat or gap fault. D-216 budget runs:
fc067-perf-budget4-a (first design: estimate-gated, credit capped at two
frames) starved both stages after the first consumer submit's one-second
warmup set the estimate; fc067-perf-budget4-c (debt model, both stages gated)
starved the evaluation because the feed spent the credit first (20
evaluations,1079 deferrals,0 combined presents); fc067-perf-budget4-d (debt
model, feed throttled, evaluation never deferred): feed553 runs and549
explicit skips (every other frame),552 accepted evaluations,1109 of1200
presents combined with561 repeats, no fault, present interval p50 26.3 ms
against28.2 unlimited. The budget halves the render-thread remake work but
the frame GPU span stays18.9 ms p50 (11.5 for the DLAA control alone), so
with the consumer sharing this GPU the lane is GPU-bound near26 ms and no
host budget reaches the12.6 ms control; the600-frame gate is not passed and
the budget stays off by default. Selftest866/0, remake-sdk-contract260/0,
launcher tests16.

LOG786 D-215: owned output ring, output-ownership split, consumer phase
timing. The owned copy of each evaluated output now comes from a ring of three
textures instead of a new texture per evaluation. Replay run
fc067-perf-ownedring-a (same settings as LOG785):1093 accepted evaluations,
0 rejected, repeats17, present interval p50 31.4 ms, unchanged; VRAM growth
+391 MB against+928 MB in LOG785's run with the same latency, so the growth
recorded since LOG784 was the per-evaluation owned texture waiting for
deferred destruction, now attributed and removed. Timing run
fc067-perf-ownedring-timing-b: output ownership3.6 ms p50 of which the
D3D11on12 acquire of the consumer output is2.0 (a driver wait, not host
work) and the source-effects composite the rest; the copy itself is not the
cost. Helper timing added to every `live_return` line (draw, present,
readback, lock wait, depth readback, depth lock wait, turnaround): draw5.9 ms
p50, present0.3, color lock wait3.6 (DXVK performs the readback wait in the
lock), depth lock wait0.8, turnaround19.7 (p95 26.2). The consumer's GPU work
per image is therefore about4 to5 ms at640x480; about9 ms of its turnaround is
its own CPU work (per-image surface creation, per-pixel depth copy, return
memcpy and digest), which is host-owned helper code. Budget at60 fps on this
machine, from the measured controls: GPU11.5 ms (DLAA lane alone) plus about
5 per consumer image fits one image per frame; CPU does not: the render
thread carries12.6 ms native plus about13 ms of remake work that is now
almost entirely device-bound (motion raster2.4, output acquire2.0, composite
and copy1.6, snapshot1.4, view scene1.3, consumer submit1.0, history0.6,
upload0.6, overlay copy0.5). Reaching the control within1 percent therefore
needs the returned-image evaluation recorded on a deferred context by a worker
and executed at present, not more CPU-only moves. Selftest856/0,
remake-sdk-contract260/0, launcher tests16.

LOG785 D-214: the return worker receives from the channel itself, and the
control figure is corrected. The worker polls the host channel (one poll per
millisecond when empty), checks the image is well formed, prepares the motion
stream against the newest image ahead of it (temporal scenes are registered by
channel sequence when the render thread publishes an overlay) and converts the
input; the render thread takes one prepared image at a time and applies the
same identity gate as before (age, epoch, original overlay match). A closed
channel is reported by the worker and handled on the render thread as before.
First run fc067-perf-receiveworker-a rejected57 of1084 returns at age3: the
worker's receive releases return credit before the render thread has taken the
image, so the next publish landed on the two-slot overlay ring entry that image
still needed; the ring now has four slots by sequence. Run
fc067-perf-receiveworker-b (same settings as LOG783):1093 accepted evaluations
of1094 returns,0 rejected,1080 worker-prepared streams,14 rebuilt,1102 of
1200 presents combined, output-frame repeats17, latency mean2.9 frames (max4),
no identity, repeat or gap fault,1 feed-worker busy fallback. Timing run
fc067-perf-receiveworker-timing-b: scene feed3.7 ms p50 (snapshot1.5, view
scene1.3, overlay copy0.5), returned evaluation9.6 (output ownership3.7,
raster2.5, submit1.0, history0.7, upload0.6), return worker4.4, feed worker
19.4 (packet build4.1); the render thread carries about13 ms of remake work.
Present interval p50 31.3 ms, unchanged from LOG784 although the receive left
the render thread. Controls measured on this build with the same replay and
harness settings on the hooks-disabled host, no helper running:
fc067-perf-control-native-a present interval p50 11.1 ms (p95 11.9),
fc067-perf-control-dlaa-a 12.6 ms (p95 13.7, PVR GPU 11.1 ms p50). The "19 ms
native control" cited from LOG780 on is not reproduced by any control run in
this evidence set and is withdrawn; the600-frame gate compares against these
controls. Against the DLAA control the combined lane costs18.7 ms per frame:
about13 ms of render-thread work plus GPU sharing with the external consumer
(PVR GPU13.7 ms p50 against11.1 alone; frame GPU span17.7 against11.5), so at
60 fps the GPU budget is already exceeded on this machine before host CPU work
is counted. Selftest856/0 (worker receive from an in-process channel pair,
idle on an empty open channel, closed report cleared by discard),
remake-sdk-contract260/0, launcher tests16. VRAM growth again+928 MB with
latency near3 frames and+389 MB with latency near2 (fc067-perf-receiveworker-a),
so the growth follows presentation latency, not the workers; unattributed.

LOG784 D-213: returned-image preparation and packet build off the render
thread. A second worker takes each accepted returned image after the render
thread's identity gate and prepares the geometry motion stream and the neural
input conversion; the render thread keeps input upload, motion raster,
consumer submit and output ownership. The stream is prepared against the
newest image ahead of it in the chain (or the accepted history when none) and
is used only when that image became the accepted history by evaluation time;
otherwise it is rebuilt on the render thread, never guessed. One prepared image
is taken at a time: an image not yet evaluated is never overwritten by a later
one (it ages out after8 frames instead). The feed worker now also builds the
packet from texture bytes the render thread staged by draw; the alpha
ownership selections follow the clipped packet there after the render thread
verified the source bindings and list ranges. Intermediate runs recorded the
two defects this design had to remove: fc067-perf-returnworker-a/timing-a
(history taken at dispatch, so1099 of1102 streams were rebuilt and the alpha
lane kept the render-thread packet build), fc067-perf-returnworker-b/timing-b
(worker build without the by-reference predicate while nothing was held: every
packet carried all textures again, feed worker31.5 ms,409 busy fallbacks),
fc067-perf-returnworker-c/timing-c (two prepared results drained in one frame
overwrote an unevaluated image:860 accepted,245 repeats). Final replay run
fc067-perf-returnworker-d (same settings as LOG783): present interval p50
31.0 ms (36.3),1095 of1200 presents combined,1071 accepted evaluations of1076
returns,1073 worker-prepared streams,0 rebuilt, output-frame repeats33,
latency mean3.06 frames (max5, within the8-frame bound), no identity,
repeat or gap fault, resource growth+39 one-time,2 feed-worker busy
fallbacks,0 return-worker busy fallbacks. Timing run
fc067-perf-returnworker-timing-e attributes the render thread: scene feed
6.9 ms p50 (return receive2.6, snapshot1.6, view scene1.5, overlay copy0.6,
texture staging0.01 once every texture is held), returned evaluation9.5
(output ownership: wrap plus owned copy3.7, motion raster2.5, submit1.0,
upload0.7, history accept0.6); feed worker18.7 (packet build4.2), return
worker4.0. Not passing: about32 fps against the19 ms native control. Two
things this measured: the render thread still carries about16 ms, most of it
device-bound (output ownership, raster, submit, snapshot), and the external
consumer's return rate was host-limited in every run so far (return-credit
busy skips0 to7 per1200 frames), so the earlier "about20 per second"
consumer limit (LOG781) is not established; a faster host is needed to find it.
Also recorded: VRAM growth was+926 to+928 MB in every worker run against+387
MB before, with the owned resource-object count unchanged; the measure is the
process-local DXGI segment and the cause is not attributed. Selftest849/0
(return worker: idle accept, malformed depth not prepared, discard generation,
ordered single take; worker packet build: registration while nothing is held,
reference with a held identity, unstaged texture as an explicit skip; returned
image well-formed gate), remake-sdk-contract260/0, launcher tests16.

LOG783 D-212 texture references on the live packet wire (user approved both
pending decisions with the goal of native60 fps and a good-looking combined
image). Packet wire version5 adds one carriage word per mesh: carried (bytes
as before), registered (bytes travel and the consumer must remember them under
the texture identity for the channel session) or referenced (no bytes). The
host keeps a sent-identity set cleared with the channel and bounded like the
consumer (4096 identities,512 MB); registration is recorded only after the
feed worker reports Published. The helper restores referenced bytes from its
cache before any validation, so every later stage sees a carried packet; a
missing or over-budget reference is a failed live source (`texture-reference-
missing`, `texture-reference-cache-bound`), never a guessed texture. Archives
are unaffected: references are disabled whenever a capture or locked-archive
environment is set, so the300-frame lanes keep their lineage and digests, and
carried-only packets stay byte-identical (version1..4). Replay run
fc067-perf-texref-a (same settings as LOG781): packet1.06 MB (12.6 before),
1113 publishes,63 registered and49276 referenced meshes over36 textures, no
reference fault; present interval p50 36.3 ms (39.4),1102 of1200 presents
combined,1106 accepted evaluations (951), output-frame repeats4 (154),
latency mean2.0 frames (2.48), resource growth+45 one-time, VRAM+387 MB,
zero identity/repeat/gap faults. Timing run fc067-perf-texref-timing-a
(`--cpu-timing`, diagnostic): feed worker15.5 ms p50 (25.0 before), scene feed
11.8 (packet build4.05, snapshot1.6, view scene1.4, about4.7 unscoped:
readiness loop, overlay copy, return receive/verify), returned evaluation12.5
(motion stream3.3, raster2.2, upload1.5, submit0.7, history0.5). Still not
passing: about27 fps against the19 ms native control; the render thread still
carries about24 ms of feed plus evaluation. Selftest830/0 (registered once,
referenced afterwards, untextured never registered, adapter and writer
contracts, anchored version5 pose, feed-worker registration report),
remake-sdk-contract260/0, launcher tests16.

LOG782 render-thread attribution after D-211 (fc067-perf-feedworker-timing-b,
`--cpu-timing`, same replay settings; diagnostic, not performance evidence):
present interval p50 38.8 ms;1200 emulated frames in47.1 s. Scene feed11.6 ms
p50 (snapshot1.3, view scene1.25, packet build with texture reads4.5, the rest
texture readiness, overlay copy and the locked return receive/verify); feed
worker25.0 ms off the render thread. Returned-image evaluation12.8 ms p50 with
new sub-scopes: motion stream2.85, motion raster2.3 (max257 with a driver
wait), input build and upload1.55, consumer submit0.87 (max900, driver wait),
history accept0.57, about4.6 ms unscoped. No single stage remains; reaching the
native control within1 percent needs the returned-image path (receive, verify,
input build, motion stream) off the render thread as well and a lighter
per-frame packet (textures are re-sent every frame,12.6 MB), which would change
the packet wire format and therefore the locked-archive lineage of the300-frame
lanes; that choice is recorded for the user, not made here. Automation
selftest813/0; sub-scopes are env-gated and cost nothing when off.

LOG781 implementation after the timing measurement: D-211 feed worker. The
render thread keeps the device-bound stages (snapshot, view scene, texture
reads, packet build, overlay copy) and hands anchor, temporal capture,
serialization and digest to one worker thread with a single pending job; a busy
worker is an explicit `worker-busy-native-fallback` skip, never a wait. The
worker owns the camera anchor so lineage stays sequential; results are drained
on the render thread in source order, which applies re-anchor/view-cut history
retirement and all logging as before. RemakeLiveChannel host bookkeeping is
mutex-protected with the serialization and digest outside the lock on a mapping
kept alive by the publisher. Replay run fc067-perf-feedworker-a (same
settings): present interval p50 39.4 ms (61.6 before),1200 emulated frames in
48.1 s (74.4 before),957 publishes,958 returns, no rejected return,2
worker-busy fallbacks,34 return-credit-busy skips,1097 of1200 presents
combined,0 identity faults, resource objects137 to182 (+45, one-time after the
first accept), VRAM growth388 MB. Not passing: emulation still runs at about25
fps against the19 ms native control; the remaining render-thread cost is the
returned-image evaluation (11.6 ms p50 including driver waits) and packet build
(4.6 ms). Also recorded: the helper returned about20 images per second, so at
full emulation speed the combined-presentation share cannot approach99 percent
with this consumer on this GPU; that limit belongs to the external consumer, not
to a criterion to lower. Selftest813/0 (feed worker fixtures: idle accept, busy
fallback, ordered results and receipts, publish failure as skip, stopped worker
refuses), remake-sdk-contract260/0, launcher tests16 with `--cpu-timing`.

LOG780 first600-frame gate measurement on the combined lane at HEAD:
fc067-perf-combined-a (replay,2100 warmup,1200 measured, no capture, launcher
exit0):1101 of1200 presents combined (after the120-frame warmup about99
percent), zero identity/repeat/gap faults, but present interval p50 61.6 ms and
1200 emulated frames in74.4 s, about16 fps against the native control's19 ms,
so the lane slowed emulation instead of falling back. Timing run
fc067-perf-combined-timing-a with the new launcher `--cpu-timing` option
(diagnostic, never performance evidence) attributes the render-thread cost per
frame: scene feed36.8 ms p50 (channel publish16.6 with byte-serial FNV digest
over12.6 MB, camera anchor5.6, packet build4.6, snapshot1.4, view scene1.3)
plus returned-image evaluation11.6 ms. The digest algorithm is persisted in the
locked archives' receipts and verified on replay, so it is not changed; the
architecture is (LOG781).

LOG779 remaining300-frame lanes of the four-lane matrix, sources2400..2699.
Native PVR public-DLAA lane attempt a (hooks-disabled host, same replay,
--start-producer2399) stopped at240 captures2400..2639 although the harness
accepted and reported300: the renderer and quality capture clamped
NeuralCaptureFrames at240 while the CLI bound had been raised; retained as a
failed300 lane. All three ceilings now read300, matching the explicit extended
diagnostic ceiling of the launcher lanes, and the harness reports
requested_frames while capture-complete.json keeps the captured count. Attempt b
captures exactly300 frames2400..2699, clean close, host ReShade log reports
SAFE MODE/hooks disabled. Returned-DLAA lane a on the same hooks-disabled host
with the locked source b archive: launcher0, host0, one helper generation with
1102 receives/1102 published and orderly close(11),300 captures2400..2699.
Four-lane review300-a (native PVR, native PVR + public DLAA, Remix-only source
b, combined b):300 frames, no gaps, zero native pixel mismatches, all frozen
Remix inputs identical. Public/combined comparison300-a:300 matched frames, no
unmatched frames or gaps, HUD mismatch0, returned RGB MAE0 on every frame
(identical locked returned pixels). Scope unchanged: exact producer/native
pixels and frozen Remix inputs; not temporal-history/NGX-input identity, not
external output provenance, not performance, no winner. Selftest806/0 in
automation, baseline and no-ngx; remake-sdk-contract260/0; four configurations
built serially before the lanes ran; no build during any capture.

LOG778 manual session fc067-anchor-manual-i (open stage, D-210): launcher exit0,
host report complete (12000 frames at4:31), one helper generation to an orderly
end,1726 sources,1726 accepted returns,0 rejected;1646 returns clamped beyond the
far plane with measured maximum1.00000048 (four float steps), none above the
limit, no near-side value. D-210 verified:104 accepted frames carried far
off-screen vertices (up to9 per frame, raw public-projection error up to25
pixels at up to17642 diagonals) with bounded on-screen effect at most0.0014
pixels and tangential error at most0.0097 pixels. anchor-projection-mismatch
skips fell from183 (session h) to78; every remaining one is tangential
(0.0103..2.81 pixels, p50 0.025, effect at most0.0049 pixels,153..23674
diagonals out): the lateral float quantization of a vertex about a thousand
units away seen at0.1 depth is about0.3 pixel per representable step, so no
float world embedding can meet the exact guard for them. Recorded as the float
embedding precision limitation (about4 percent of exported frames native on this
stage), not relaxed; an exact double-precision frustum clip of such vertices
would change mesh topology and stays a separate item. Anchor: two re-anchors
(102 degrees with145 shared; a complete content change with0 shared points),
four view cuts including the same round-start dolly pair as sessions e and h,
0 ambiguous frames, lineage selection on all but three multi-basis frames.

LOG777 implementation after session h: D-210. The projection guard reports the
actual coordinates, distance in viewport diagonals and the radial/tangential/
effect decomposition of a failing vertex; vertices at least four diagonals
outside the viewport whose bounded on-screen effect and tangential error both
satisfy the unchanged 0.01 pixel guard are accepted and counted
(`offscreen_accepted`, raw and bounded maxima in the observed-camera log and
the packet omissions). Unit fixtures: measured-kind radial error (67 pixels at
about19000 diagonals) has effect below0.01 pixel; a tangential error rejects; a
vertex two viewports out does not qualify; exact near-plane fixtures report no
off-screen acceptance. Unverified in play until the next manual session on the
open stage.

LOG776 manual session fc067-anchor-manual-h (open stage of session e, worker
budget420, clip-range depth policy, retirement fix; window untouched): launcher
exit0 with the host report written (12000 frames complete at4:31, all harness
transitions pass, performance_eligible false as for every manual session). One
helper generation for the whole run: first publish at2:29,2003 sources,2003
accepted returns,0 rejected, orderly channel-closed retirement (11) at host end.
D-209 far side exercised and verified:1961 of2003 returns carried beyond-far
values (pixels per return p50 387, p90 2761, max5641), measured maxima
1.00000012..1.00000036 (one to three float steps above1), `above_limit`0 on
every return, no near-side value on this stage. Anchor: one re-anchor, genuine
(102 degrees,145 shared); four view cuts (the round-start dolly pair identical
to session e at8 and89 source frames,43.6 degrees over2 frames, and12.2 units
in a single frame with468 shared);0 anchor-ambiguous-source-basis; accepted
frames had one to three rigid bases with lineage selection on all but two
multi-basis frames;183 anchor-projection-mismatch skips (p50 0.059, p90 1.25,
max67.5 pixels), the far off-screen precision limitation of session d on this
stage, guard unchanged. Retained as verification evidence for D-208 lineage,
D-209 and the session-worker budget; still diagnostic anchored scene, not a
recovered world camera.

LOG775 manual session fc067-anchor-manual-g (worker budget420, clip-range depth
policy): the host window was resized at0:44 (raster contract640x480 to
570x427), the swapchain resize failed (887a0001) and the renderer
re-initialized, requesting helper generation2 before any source had been
published. Helper g1, still inside its180 second first-source wait, saw the
channel close and exited2 as an invalid artifact, so the launcher refused to
hide it (launcher exit1, flycast forced at0:44, no returns, nothing verified).
This is the same limitation as g6 in LOG768. A session worker retired before
its first source now exits as the ordinary channel-closed retirement (11), the
launcher rule is unchanged, and the first-source wait expiring is still a
failure. The window must not be resized during a manual session; the resize
failure itself is recorded, not addressed. Automation selftest801/0,
remake-sdk-contract260/0, launcher tests16, four configurations built serially.
D-209 on both sides and the worker budget remain unverified in play.

LOG774 implementation after session f: the session-worker diagnostic runtime
budget is420 seconds, matching the manual host bound the worker must outlive
(ordinary helpers keep300/120/30); returned depth before the near plane
(negative, the projection of0<z<n) is returned as0 and counted
(`before_near_clamped`, `min_depth`) alongside the far side of D-209. Automation
selftest and remake-sdk-contract fixtures cover the worker budget and the near
side. Both unverified in play until the next manual session, which should be on
the open stage of session e so the far side is exercised too.

LOG773 manual session fc067-anchor-manual-f (D-209 far side, lineage selection,
same player on a closed stage with many moving objects): first publish at0:58,
one helper served4548 sources and4544 accepted returns to5:58 with no
beyond-far value at all (`beyond_far_clamped`0 and `max_depth`1 exactly on every
return, `above_limit`0), so the far-plane policy was not exercised. Four returns
were rejected as return-depth-range for depth_min -0.00024..-0.00038: points
between the camera and the near plane, the near-side mirror of D-209. Five
re-anchors, all genuine (114..166 degrees,42..146 shared); four view cuts
(13..89 degrees over4..97 source frames);59 anchor-ambiguous-source-basis frames
all before the first accepted export (no lineage yet, no size majority); accepted
frames carried up to11 rigid bases with lineage selection on all but four
multi-basis frames;65 anchor-projection-mismatch skips (p50 0.020, p90 0.039,
max0.23 pixels). The helper's300 second runtime watchdog ended the session at
about6:00 while the host was at frame8637 of12000 under its420 second bound;
the launcher refused to hide it (launcher exit1, flycast forced, no host
report). Retained as evidence for lineage and re-anchoring, a failed experiment
for the run bound.

LOG772 implementation after session e: RemakeCameraAnchor selects the anchoring
basis by exact point lineage with the last accepted set when a frame carries
several rigid bases (same shared-support thresholds; size majority only without
lineage; an even split without lineage stays ambiguous) and reports
frames_since_last, the export interval of the from-last motion, in the observed
camera and view-cut logs. The helper applies D-209: returned depth in
(1, f/(f-n)] is returned as the far plane with per-return counts and the
measured maximum (`beyond_far_clamped`, `above_limit`, `max_depth`,
`far_limit`); larger values stay as measured and reject; the raw depth artifact
is unaltered. Automation selftest798/0 (lineage split fixture, interval
fixture, far-plane clamp within the projection limit only, untouched in-range
depth, invalid planes unchanged), remake-sdk-contract257/0, launcher tests16.
Four configurations built serially (automation, baseline, no-ngx, off). The
policy is unverified in play until the next manual session on the open stage.

LOG771 manual session fc067-anchor-manual-e (lineage basis selection, same
player, the open stage with two large rigid scenery groups requested): first
publish at2:30 (source9668, three rigid bases,238 moving points);72 exported
sources over17 seconds,23 lineage-selected frames (two or three bases),48
single-basis frames,0 anchor-ambiguous-source-basis (666 in session d), no
re-anchor,7 view cuts,5 anchor-projection-mismatch skips (0.015..0.94 pixels).
The two translation-only view cuts (9677 and9766:0.29 and0.10 degrees with
2.65 and2.85 units,514/487 shared) are a steady dolly of about0.33 units per
emulated frame measured across the eight-frame export interval, not a cut: the
from-last motion is per exported interval, now logged as frames_since_last;
thresholds unchanged, since a false cut only retires history. The helper
rejected65 of71 returns as return-depth-range (depth_min0.93..0.96, depth_max
printing as1, no nonfinite values) whatever the basis count, exited14 at the
64-return bound and the launcher refused to hide it (flycast forced at2:48,
launcher exit1, no host report). Session d's27 rejections were the same
phenomenon on a closed stage; this stage shows the far plane in nearly every
frame. Retained as a failed experiment for returns and partial evidence for basis
lineage.

LOG770 manual session fc067-anchor-manual-d (dominant basis, idle tolerance, same
player, moving-props stage requested): one helper for the whole run,3030
receives/3030 returns,27 returns rejected as return-depth-range (depth_max prints
as1: values one rounding step above1) and survived as single native frames,3
generation changes all genuine (shared94..122 points,102..145 degrees),10 view
cuts in place, launcher tests16, helper ends channel-closed at host end. Host
timed out at300 seconds before writing its report (12000 frames took longer on
this stage than session b's4:50): manual host bound raised to420 seconds. Every
accepted frame had one basis;666 frames stayed ambiguous in bursts (for example
3868..4167 and8114..8173) because two large rigid groups had no2x majority: next
select the basis by exact point lineage with the last accepted set, not by size.
637 frames failed the exact projection guard (pixels p50 0.029, p90 0.117, max
187.5) on far off-screen vertices (expected coordinates such as-342,109 and
-1.36,-1497 pixels); the guard is unchanged and this is recorded as a precision
limitation of float world embedding at large offsets, not relaxed. Launcher exit
1 reflects the host timeout only.

LOG769 implementation after session c: RemakeCameraAnchor groups exact points by
rigid basis, anchors on the dominant basis and labels the rest as moving objects
(new omission; report fields bases/moving_points; an even split or more than64
bases stays anchor-ambiguous-source-basis). RemakeLiveIdleWaitMs gives session
workers60 seconds between sources (ordinary helpers keep5). Automation selftest
794/0 with fixtures: dominant basis with a six-point second basis accepts and
labels; even split rejects; idle wait explicit. Manual host bound420 seconds.

LOG768 manual session fc067-anchor-manual-c (chain lineage, cut retirement,
resilient helper): two view cuts (43 and33 degrees,325/427 shared) retired
history with the anchor and light retained; four re-anchors were all genuine
(54..173 shared,53..173 degrees); no false re-anchor.7073 frames were rejected as
anchor-ambiguous-source-basis on a stage whose scenery uses more than one rigid
transform, a limitation session b never reached. Five helper generations retired
on the5 second bounded receive timeout during2D screens between rounds (g1..g5
outcome11, expected retirements); g6 never received a source and exited2 at host
end, so the launcher reports a non-orderly shutdown. Retained as partial evidence.

LOG767 implementation after session b: support lineage now compares exact
object-space points with the last accepted set (thresholds unchanged, reference
basis unchanged); a large single-frame basis jump (>20 degrees or >1 unit) with
continuing support retires temporal/raster history, pending returns and
presentation carry-over through retireRemakeTemporalHistory() while keeping the
anchor, light and the frame's own native effects; the helper survives an Invalid
return (live_return_rejected with depth min/max/nonfinite, bounded to64 per
session). Automation selftest791/0 including drift-chain acceptance (24 shared
with last,16 with reference) and low-overlap rejection. Manual-input launcher
budget:3000 warmup plus9000 frames,300 second helper budget, helper
--source-wait-seconds180 (30..300 validated).16 launcher tests.

LOG766 manual sessions with the player present. fc067-anchor-manual-a (old
budget) ended after57 seconds with no fight reached; retained failure.
fc067-anchor-manual-b (manual budget): one helper served3442 returns over about
4:45 with18 in-session re-anchors and near-continuous acceptance3505..7086, but
only5 re-anchors were genuine cuts (53..159 degrees); the other13 happened in
smooth motion (at most5 degrees,690..1103 points shared with the previous frame)
because support was compared with the first-view snapshot, which drifts inside
one arena. This confirms the visibility-churn case in real play.20 single-frame
anchor-projection-mismatch skips (0.049..0.073 pixels). The helper died at source
7085 on return-depth-range (outcome14) and the launcher refused to hide it.

LOG765 commit357a5d6d24fae53b5c743a5eb5c218dc52f76bc2 pushed to fork
feat/neural-rendering (remote ref verified equal to HEAD). It carries both the
capture-boundary/archive-index slice (LOG743-756) and the anchor-generation slice
(LOG759-764, D-207) because they share renderer/test/doc files; a hunk split was
not performed. Verification before commit: automation/baseline/no-NGX/feature-off
serial builds exit0 in the VS x64 developer environment, three selftests789/0,
16 launcher tests, contract inspector unchanged (current FC-067 / M2-scene,
camera outcome pending). Private untracked items (metrics.txt, dxvk cache,
rtx-remix/) remain untracked. Working pipeline acceptance remains open.

LOG764 bounded cut capture fc067-anchor-boundary-f-capture (same executable as e,
--capture-frames40 --capture-start-source3090, managed, anchored light): launcher0,
host0/helper11 orderly, one helper generation, re-anchor at3099, accepted
2185..3098 and3100..3302.40 captures: sources3090..3096 and3102..3134; the five
native-presented sources3097..3101 have no capture, matching LOG763. Reviewed
images:3096 composited shows the fight view with protected HUD (names, bars, timer,
counter);3102 composited shows the post-cut round-end camera of the same arena
(fighters and statue) with returned Remix content matching original-native.png,
so the cut is the KO/REPLAY transition and the new generation renders the new
view, not a stale one. Observation retained as a separate gap: the native "REPLAY"
text at3102 is absent from the composited output and its original-overlay-mask is
empty, so the round-end overlay is not covered by the current exact T1401N overlay
protection (FC-055 coverage), unrelated to the anchor change. Synchronous capture
timing is not performance evidence.

LOG763 in-session re-anchor demonstration fc067-anchor-boundary-e (same automation
executable class as d, helper fixed): one helper generation for the whole run,
host0/helper11 channel-closed, orderly shutdown, launcher0. Accepted sources
2185..3098 then3100..3302; only the cut frame3099 is skipped. Helper logs
live_anchor_generation_change and anchor_generation_change3098->3100 with origin
0.173203,0.381436,6.93976, resets correspondence and recreates the anchored light
at3100 (direction re-fixed at the new first view, exactly as a fresh session did;
not world-consistent lighting across the cut).1122 receives/1122 returns. The
pre-cut3098 return is dropped at current3100 (retained=0);3100 returns at3102 and
every later source through3302 returns. Presentation:3096 evaluated at current
3098, native3099..3101, held-native3102..3103, remake-evaluated from current3104
(source3102). Native fallback per cut is five presented frames instead of about
120 (LOG760). Log-level evidence only at this checkpoint; no quality, temporal or
performance claim. A bounded40-frame capture3090..3129 (run f) follows.

LOG762 first in-session attempt fc067-anchor-boundary-d: the renderer re-anchored
(3100 accepted as generation1, origin0.17320329,0.381436348,6.93976116, position
0) and published3100/3101, but the helper live loop threw "live source continuity
rejected" at3100 because its own async continuity check still required an
unchanged origin; the launcher refused to hide the superseded failure (exit1).
The renderer also kept presenting the pre-cut evaluated image (source3096) at
current3099..3101 because only pending returns were dropped. Fixes: the helper
live loop and D3D9PacketScene accept AnchorGenerationChange (logged, correspondence
reset), and the renderer retires evaluated/composited presentation through the new
retireRemakeHistory() at the cut. Retained as a failed run.

LOG761 implementation of the measured-cut policy: RemakeCameraAnchor gains
Reanchor()/Generation()/Origin(). A re-anchored generation labels its fixed view
with the retired view's camera-relative position of the new reference source (a
distinct diagnostic label, not a world relation; nudged if it would repeat) and
adds an explicit omission. New shared predicate remake::AnchorGenerationChange:
same producer chain and anchored scope, finite differing origin; DiagnosticContinuation
and AsyncSourceContinuation stay strict and fail across it. Managed renderer path:
on anchor-source-support-changed retire the fixed view, temporal/raster history,
pending returns and presentation carry-over in-session and keep channel/helper,
instead of closing the channel and requesting a fresh helper. Helper AnchoredSceneLight
re-fixes on origin change (epoch/game/scope/invalid direction still reject) and
counts re-anchors. Shared-support thresholds, projection and depth guards unchanged.
Automation selftest789/0 including: support counts against reference and last
accepted sets; basis motion separated per set; rejection leaves reference/output
unchanged; re-anchor requires a rejected report and yields origin-1,0,0 with two
omissions; generation zero keeps the zero origin; predicate epoch/ordinal/scope/
finite/SHA controls; light re-anchor and unchanged epoch rejection.16 launcher
tests pass (run from neuraltest/). Non-managed sessions keep the old permanent
rejection; the diagnostic label is preserved.

LOG760 boundary diagnosis fc067-anchor-boundary-b and -c (automation executable,
managed session, anchored light, no capture). The new anchor support report at
source3099/producer3098 is identical in b, c, d and e: points1000, reference903,
shared_reference154, last_accepted1012, shared_last424, rotation108.139deg and
translation8.02363 from the reference view, rotation81.1573deg and translation
13.5101 from the last accepted view (3098). Run c adds the per-accepted-frame
baseline over908 smooth frames2093..3098: shared_last/points min0.739 p5 0.982
median1.0; per-frame rotation p95 0.29deg max1.62deg; translation p95 0.078 max
0.741;3098 itself is0.109deg/0.211. Conclusion: source3099 is a genuine source-
view cut (about50x the smooth-motion rotation maximum), while424 exact shared
object-space points show the same arena continues. This is not visibility churn;
no support threshold is changed. Fresh-session gap measured in b: rejection at
01:50.285, g2 requested23ms later, first g2 acceptance3123 (24 frames), then93
no-return-credit skips3125..3218 while the fresh Remix runtime started; about120
frames of native fallback per cut. The same3099 rejection appears in five prior
logs (extended source a/b, combined a/b, combined b live), so the boundary is a
deterministic input-replay event. Raw logs remain under D:\Flycast-Evidence; each
run directory also holds flycast-live-copy.log for its own host log.

LOG759 resume at995308035f8c320da6c305fc5b06b44f1931703b with all dirty files
preserved; no running game/helper/build. Baseline build (VsDevCmd x64) with the
support-report diagnostic passes775/0 (three new fixtures), but the staged baseline
executable lacks TEST_AUTOMATION input replay: run fc067-anchor-boundary-a never
entered the fight (no "Input replay opened",0 anchored frames,1171
estimated-view-missing-observed-anchor skips, helper exit2 channel-closed before
runtime load). Retained as a failed control. Staged runtime executables must come
from build-neural-automation (hash of the prior combined-b executable matches it).

LOG758 user paused implementation and requested a development handoff/docs.
Created DEVELOPMENT-HANDOFF.md and linked pause authority from AGENTS/BACKLOG.
The repeated source3099/producer3098 support rejection is the next investigation,
not a diagnosed false rejection or an implemented camera fix. Preserve dirty
capture/index work and all raw evidence. No runtime/build was launched for this
documentation task; no completion, new runtime pass or remote push is claimed.

LOG757 resume at995308035f8c320da6c305fc5b06b44f1931703b with owned dirty
capture/index changes preserved. No running Flycast/helper/build processes were
observed. Existing extended-remix-combined-review-b report contains300 frames
2400..2699, no unmatched sources or gaps, and zero HUD mismatch. It explicitly
does not establish identical temporal histories/NGX inputs, fresh external
provenance, performance or a winner. NGX and no-NGX replay-prewarm selftest logs
each report772 passed/0 failed; feature-off log ends with successful linking.
User priority correction now moves live scene/camera continuity ahead of further
matrix expansion. Source inspection confirms first-view anchoring and rejection
on insufficient shared source support; no acceptance threshold was changed.
Next distinguish visibility churn from actual scene/basis changes using existing
managed-run evidence, then implement only a source-justified continuity fix.

LOG756 combined b reaches exact300 captured source IDs2400..2699 in original g1
at this checkpoint. Begin full frozen source/effect/HUD/Present comparison against
source b, with its original host log preserved in combined a's previous log.
No full comparison/terminal result yet; count alone does not close acceptance.

LOG755 combined b log confirms index preparation at00:07.796 before g1 request.
24 captures present and exact locked source acceptance continues through2424,
past prior three-frame timeout. This verifies startup scheduling only at this
checkpoint; full300 sequence, composition, termination and comparison remain
pending. No helper/session renewal observed at this checkpoint; no quality or
performance acceptance inferred.

LOG754 combined a ultimately exits host0/helper11 orderly, no forced children,
but only3 captures so failed300 acceptance. Pre-session archive preparation build
passes772/0 including same-size producer timestamp invalidation and preparation
tests. Launch combined b with same archive2400..2699 and unchanged timeout rules;
index now prepares before first session request. Remaining configurations pending
after live retry. No success inferred from the run merely starting.

LOG753 extended combined a retains only three captures2400..2402 before helper
receive timeout; g2 then rejects changed anchored scene at offset141. Failed300
run, not permission to weaken source equality. Cold index construction occurs
during first evaluation and starves live publication. Move index preparation
before initial session request (before helper delivery), retaining per-input
validation and unchanged receive/watchdog bounds. Code not built while run live;
await terminal exit and repeat only after validation. Prior timing improvement
does not by itself prove safe pipeline scheduling.

LOG752 replay-index automation build/test completes0:770/0 including duplicate
addition/removal and packet extent/restoration. Actual300-archive read-only repeat
timings4050ms cold,62ms and62ms warm; exact first source validation passes. This
is archive lookup timing, not gameplay performance. Launch extended combined a
against source b for exact2400..2699, same watchdogs. Cold index cost and all
frame/provenance/composition requirements remain part of live test; no300-result
acceptance yet. Remaining build configurations pending after runtime exit.

LOG751 implement bounded one-archive/thread source lookup index. Enumerate at most
512 entries each call; packet path/size/write-time changes rebuild identities by
full deserialization. Selected packet, source equality, receipt and color/depth
hashes are reread/revalidated every call. Frozen diagnostic archives are assumed
not concurrently edited with forged/restored filesystem metadata; this cache is
not a hostile-filesystem authenticity boundary. New duplicate/add-remove/extent/
restoration tests and bounded1..5 lookup timing option added. Automation build
running; no timing improvement or long replay acceptance claimed yet.

LOG750 source b image review passes all300 sources2400..2699 with nonempty exact
HUD/composition/backbuffer/Present checks. Midpoint2550 inspected; no quality
winner. Remaining builds complete0, baseline/no-NGX766/0 and feature-off linked.
Existing read-only check-locked-remake-input validates first source against300
archive but takes4093ms for one lookup: reader deserializes every packet each
call. A300-frame replay would exceed unchanged watchdog; do not run that known
cost blindly. Next add bounded invalidation-aware archive lookup indexing while
retaining selected packet/source/pixel/depth validation and ambiguity rejection,
then benchmark and rerun negative controls before gameplay. This is diagnostic
archive IO, not production graphics-performance acceptance.

LOG749 extended source b exits launcher0 and captures exact300 sources2400..2699,
all with effect identity. Boundary first-frame omission corrected in this run;
full image review underway, so count alone is not composition acceptance. Remaining
baseline/no-NGX/feature-off builds started after host/helper exit. Next replay
this exact archive through combined and hooks-disabled returned DLAA, plus native
PVR baseline, preserving full300 requirement and failed prior source run.

LOG748 live extended source b now captures2400 at current2402. Present log shows
held-native source2400 at current2400/2401 then returned source2400 at2402,
followed by2401 and2402. This verifies the boundary transition preserves the
first source without backwards time in this run.62 captures at checkpoint;
complete300-frame count/composition and terminal exit still pending. Prior a
review passes its298 existing images but remains failed300 evidence.

LOG747 capture-boundary automation build/test completes0 with766/0. Retry extended
source b uses same2400..2699 requested interval and unchanged runtime/settings;
only bounded boundary hold differs. Image review of prior298 frames still running.
No accepted300-frame result yet; remaining build configurations pending.

LOG746 extended source a terminates launcher0/host clean close,298 capture records:
fails requested300 because startup2400/2401 absent. No relabeling. Bounded interval
now avoids archive work beyond end; source run remains non-performance evidence.
Build boundary correction and run original image review in parallel after runtime
termination; no build overlapped gameplay. Common40-object helper warning remains.

LOG745 extended source starts at2402: expected2400/2401 omitted. Logs show those
outputs owned but held-native presentation until2402. Policy started warming on
first delayed candidate using current-frame floor, necessarily excluding earlier
sources. Implement explicit bounded comparison-boundary warmup before first reply;
hold current native source, accept matching delayed result without backwards time,
retain eight-frame timeout and unchanged default policy. Three CPU controls added;
not built while source run is live. Existing incomplete run remains failed300
evidence; no relabeling or padding. Manifest request-bound fields added for later
runs;16 launcher tests still pass.

LOG744 extended-effect automation build completes0;763/0 selftests include five
bound/end controls. Launch explicit300-frame Remix-only exact-effects source
window2400..2699, existing diagnostic watchdogs and managed worker unchanged.
Require all300 consecutive sources before acceptance; no padding startup loss.
This run is synchronous image evidence, never cadence/performance evidence.
Other build configurations still pending; no longer-matrix result yet.

LOG743 four-lane postcommit builds terminate0 and995308035 pushed. Implement
explicit extended-effect capture300 ceiling, legacy30 default, invalid/unbounded
request rejection and inclusive source-end check before locked archive reads.
Renderer capture and replay share the bound; existing watchdogs unchanged. Five
CPU boundary assertions added. First launcher unit run fails because test insertion
split an existing method; corrected method boundary,16 tests pass. Automation
build underway; longer gameplay not yet run or accepted. A300-frame requested
window with missing startup frames must remain incomplete, never padded.

LOG742 four native-identity unit tests pass (exact, wrong frame, each producer
component, unavailable producer); regenerated four-lane artifact b passes all28
real source joins. Second drive has about68.5GB free before longer experiment.
Next scope is explicit300-frame frozen/effect diagnostics with legacy30 default
and source end bound; this does not raise performance watchdogs or lower full
matrix acceptance. No third-party configuration or runtime writes authorized by
this extension. Existing28-frame artifact remains a short comparison only.

LOG741 native-PVR DLAA b completes28 frames2704..2731 and clean close. All28
producer epoch/ordinal/cycle identities match combined captures; native RGBA
pixel mismatches are zero. Four-lane review generated for native PVR, native PVR
public Auto, Remix-only and combined with exact frozen Remix inputs. Midpoint
inspected and slowed moving artifact exposed for user review; not a moving
perceptual verdict or fresh external-output proof.28 frames are not the required
300-frame matrix. New review tool scope is source joins; individual capture/
Present checks remain separate. Additional negative tests/tool checkpoint pending.

LOG740 d60792aac pushed and fork SHA verified; postcommit four builds terminate0
with three758/0 suites. Native-PVR DLAA capture attempt a rejected CLI before
launch because proof-overlay requires late-overlay-proof; retain failure. Retry b
omits that unrelated option:28 frames at producer2703, target640x480, public Auto,
OIT On12, same replay, source observation off, no Remix helper. Existing OFF host
logs archived before launch, external config untouched. Match actual producer
identity and original source pixels before joining this lane to moving comparison.

LOG739 postcommit83bd8c806 four-build chain terminates0, three758/0 selftests.
Save the separately tested Remix-only/frozen/returned-DLAA comparison controls
and retained LOG726-738 evidence. Only owned source/docs are staged; third-party
configuration, captures, binaries and the three existing user untracked items
remain excluded. Full working-pipeline acceptance remains open.

LOG738 remaining serial builds complete0: baseline/no-NGX758/0 and feature-off
linked/no work; automation758/0 already verified. Save standalone owned-lifetime
and depth-diagnostic code as83bd8c806. Postcommit four-build/selftest chain started;
comparison implementation and this evidence remain pending their separate commit.
No new gameplay pass is inferred from compilation; intermittent depth failure
remains open. Next current-matrix omission is native-PVR DLAA, not another repeated
returned-image isolation run.

LOG737 returned-DLAA c terminates launcher0/host0/final helper11 orderly close,
no forced children and no remaining host/helper process. Earlier depth failure
not reproduced, not fixed by diagnostic reporting.15 launcher tests pass again;
backlog consistency passes. Remaining baseline/no-NGX/feature-off builds started
only after GPU processes exited. Save independently validated comparison and
owned-lifetime slices once those builds complete; full working-pipeline goal
and intermittent depth failure remain open.

LOG736 returned-DLAA retry c reaches28 captures2704..2731 without reproducing
the earlier depth rejection; intermittent failure remains open. Public/combined
comparison passes exact scene/native/HUD/returned-color/depth/effect/alpha inputs,
composition and completed Presents for all28, no unmatched frames. Fresh public
host reports SAFE MODE/hooks disabled. Pre-effects outputs differ on all28:
between-output RGB MAE15.64237. Public/combined source MAE4.01799/17.08349,
gradient error3.07671/3.88591, temporal delta1.95552/2.37250, source-relative
temporal residual1.69466/2.25445, black drift0.24439/10.04806, saturation-range
drift-0.19158/-7.37309 (8-bit RGB metrics). Midpoint inspected: combined softer.
No full-guidance equality, fresh external sentinel proof, moving perceptual winner
or full matrix acceptance inferred. Runtime still finishing. Current tuple is
not supported as a quality improvement by these component metrics; do not force
a winner or generalize to all settings. Retain public/native baselines and advance
visual settings only inside supported geometry/identity constraints.

LOG735 automation builds and selftest758/0 pass with depth reason assertions.
Actual owned-job abort test launches native Flycast under performance harness,
observes child6348 of harness13664 with exact executable path, abruptly terminates
only that harness, then verifies child absent after2s. This proves this concrete
abort cleanup case, not the whole transition matrix. Returned-DLAA retry c starts
with new helper/host diagnostics and job ownership; same unchanged source archive
and hooks-disabled host. Remaining serial configurations still pending.

LOG734 implement unchanged depth acceptance with distinct extent/projection/
nonfinite/range/missing diagnostics; existing negative tests now assert reasons.
Performance harness creates its child suspended, assigns noninherited kill-on-
job-close ownership, then resumes. Assignment failure terminates only its new
child and fails launch. First patch matched another CreateProcess call; inspection
caught and corrected it before build. Automation build/test underway; live abort
verification and remaining configurations pending. No orphan-cleanup pass yet.

LOG733 returned-DLAA retry b FAILS before target interval: helper outcome14 at
source2267/sequence87, return-depth-contract rejection; zero captures. Do not
attribute to jitter without evidence: validator combines depth range/finite and
near/far equality checks, so current message cannot identify the cause. Supervisor
correctly refuses unexpected helper retirement but terminates only harness33780;
its owned Flycast child30940 remains live. Verified exact executable and parent,
requested normal window close. This exposes a cleanup gap requiring bounded owned
process-tree lifetime handling, not blanket process-name kills. Public-DLAA
isolation remains failed; prior combined exact-input28 frames remain valid scoped
evidence. Next distinguish depth rejection components and fix owned child cleanup.

LOG732 combined locked run finally closes host0/helper11 orderly, no forced
children. Returned-DLAA attempt a fails before gameplay: host3, no helper session,
missing deterministic scripts input in existing hooks-disabled host. Copy only
the existing input replay into its absent destination and verify equal SHA256;
no external policy/configuration edited. Retry b uses separate evidence root.
Preserve the failed launch; no public-output acceptance before actual captures.

LOG731 owned launcher adds capture-only returned-DLAA selection, mutually exclusive
with Remix-only;15 tests pass. Existing separate host text has EnableHooks=0 and
historical SAFE MODE log; verify a fresh run before claiming uncontaminated public
output. No external configuration modified. Combined locked run remains live;
do not overlap another GPU run. Returned-DLAA is not target-native PVR DLAA.

LOG730 locked combined comparison passes28 consecutive2704..2731; archived2702
and2703 are unmatched startup frames, not a30-frame pass. Comparator checks exact
scene/native/HUD/returned-color input and original HUD/composition/completed
Present; independent hash joins also match depth, native-effect identity and
alpha exclusions for all28. Midpoint2718 inspected: large brightness alteration
already present in Remix-only; combined result softer. No moving perceptual
winner, complete guidance equality or new external provenance claim. Runtime
still finishing at this checkpoint. Preserve locked replay artifact and advance
the public-DLAA isolation lane instead of another live-input comparison retry.

LOG729 exact Remix-only archive completes launcher0/clean host close.30 captured
sources2702..2731 (not requested2700 start) are consecutive, each with effect
identity, exact nonempty HUD/composition/backbuffer and completed Present. Start
delay retained. Launch combined locked replay at actual2702 against this archive;
do not relabel the30-frame diagnostic as300-frame acceptance. External host reports
upscalingOFF/intensity1/tone1/white203/preset0/style0/enabledON; unchanged tuple is
an isolation control, not a promoted visual preset. Transition 'pass' strings on
an uninjected run do not establish the transition matrix.

LOG728 remaining Remix-only builds complete0: baseline/no-NGX758/0, feature-off
linked. Existing120-frame captures lack native-effect-identity.bin because that
synchronous diagnostic was disabled. Do not bypass replay's effect check. Expose
owned --effect-identity and --locked-input-root launcher options with existing
30-frame bound, positive comparison start and archive preflight;14 launcher tests
pass. Planned30-frame Remix-only archive at2700 enables exact frozen replay. This
short isolation slice does not replace the outstanding300-frame moving matrix.

LOG727 Remix-only actual run completes host0/final helper11 orderly channel close,
no forced children.120 consecutive2700..2819 captures pass nonempty exact HUD,
composition/backbuffer and completed Present checks. Final2819 panel inspected:
bright returned lighting remains before neural evaluation, HUD retained. Exact
geometry/material comparator matches120 frames/5040 mesh records to worker capture.
Strict Remix-versus-neural comparison FAILS at2700: returned pixels differ despite
matching source geometry. Retain rejection; no causal neural-quality conclusion.
Use existing locked-input replay for isolation, not relaxed input equality or a
new random live rerun. Automation build758/0;13 launcher tests pass. Other build
configurations still required for this uncommitted comparison implementation.

LOG726 postcommit1883bf345 serial four-build command terminates0. Next bounded
implementation adds a capture-only Remix comparison branch: same returned source,
owned OIT effects and original HUD, but no neural submit/history advance. Existing
owned presentation slot is reused; explicit comparison_lane and skipped-evaluation
metadata distinguish it from combined output. Legacy evaluated_remix field/file
names identify that presentation slot, not proof of neural execution. Require
actual moving capture, matching source inputs and zero neural-submit log entries
before accepting this lane. Defaults and external configuration remain untouched.

LOG725 remaining serial builds terminate0: baseline and no-NGX each758/0;
feature-off links successfully. Automation session-worker test also758/0.
These are incremental builds, not fresh exact-SHA builds. Backlog contract
inspection passes document consistency only. Save the independently tested
worker/capture slice; complete gameplay, real handover continuity, external
moving quality and performance remain open rather than narrowed to this pass.

LOG724 worker-continuity capture completes: launch host0, final helper11 with
orderly channel closure and no forced children. Review verifies120 consecutive
sources2700..2819, no gaps, zero nonempty-HUD/composition/backbuffer mismatches
and completed Presents. Independent receipt/packet identity joins verify all120
belong to g1. Two checker tests and12 launcher tests pass. Final panel2819 was
visually inspected: HUD retained; returned scene remains substantially brighter
and combined scene softer than native. No moving perceptual winner or fresh
external provenance claimed. This closes the artificial600-return discontinuity
for this interval, not real scene-change recovery (later g2 still exists).
Prior92-source handover failure remains recorded. Remaining serial baseline,
no-NGX and feature-off builds started; do not count them passed until terminal.
Next integration priority is the synchronized native/public-DLAA/Remix/combined
moving comparison using the existing provenance route, not another repetition
of helper-lifetime success. Capture evidence remains performance-ineligible.

LOG723 implement explicit --session-worker for managed returned-scene helpers:
finite10000-frame ceiling, same120s runtime watchdog (300s diagnostic capture),
same host/controller deadlines and channel-close termination. Legacy frames1..660
unchanged; worker requires exact extended returned route/request660. Five boundary
tests added; automation build/test command0. Planned repeat of120 captures from
2700 checks removal of artificial first600-return handover; it need not span a
real scene-boundary restart and must not be labeled proof of that remaining gap.

LOG722 image composition review completes120 records with no failed nonempty
HUD/composition/backbuffer/Present checks; gap2780->2873 retained. Final panel2911
visually inspected: native game effects and HUD present, transformed scene remains
bright/soft. No temporal/perceptual winner declared. The92-source capture gap is
not acceptable as seamless gameplay. Next remove artificial600-return helper
rotation from managed operation by adding a bounded session-lifetime worker mode;
retain old diagnostic frame limits for existing tools, whole-run watchdogs,
explicit channel close and failure reporting. Do not keep paying60-frame startup
for an arbitrary diagnostic batch boundary in an interactive pipeline.

LOG721 managed-capture-a completes launcher0/host0, orderly final helper11
channel closure, no forced children.120 captured records span g1=81/g2=39;
session/receipt/packet identity and completed Present checker passes, first
accepted evaluation resets history in each captured generation. Source gap
2780->2873 remains:92 intervening sources without captured combined results.
This is not seamless recovery or proof of every gap-frame native pixel. Checker
negative controls reject wrong token/digest/frame/sequence/epoch/producer and
unpublished return; duplicate receipt rejected. Captured packet byte digest is
not independently recomputed by this checker; scope is identity joins. Image
composition review running. Capture excluded from all performance conclusions.

LOG720 add validated session_token to preview metadata and developer-only
capture-start source gate. Extend owned launcher with bounded1..300 image capture
using existing420s host/300s helper diagnostic budgets, performance_eligible=false.
No enlargement of the failed180s noncapture acceptance budget. Planned120-frame
capture starts at source2700 to span first600-return helper handover. Automation
build/test launched; source token is renderer-owned and must be independently
joined to helper command/receipt/frame and completed Present, not trusted alone.

LOG719 remaining serial configurations complete: baseline/no-NGX753/0,
feature-off linked; automation753/0 already passed.11 launcher tests pass,
including exact log archive/config preservation and copy-failure retention.
Save managed-session slice as scoped ownership/lifetime progress. Full restart
deadline, visual handover, cross-generation output identity, cleanup/performance
and external combined provenance after handovers remain open. Next capture
across an actual helper-generation transition; retain failed earlier runs.

LOG718 managed-lifetime-a completes launcher0, host0/1200 samples/clean close,
903 returns across g1=600,g2=214,g3=89. g1 exits0 at bound; g2 superseded and g3
host-closed each exit11 with exact channel-closed reason. Raw final[0,11] retained,
orderly_host_shutdown=true, forced_children empty. This accepts bounded lifetime
renewal, not restart-time-budget, capture/provenance or full cadence acceptance.
Per-helper40-object cleanup warning persists. Host stage logs may append prior
runs: consumer logs/output root are isolated, but do not derive current-only
presentation counts from whole stage logs. After verified copy, remove only the
two named old logs before launch to ensure new host log isolation; archived copies
remain recoverable. No configurations removed. New log isolation change awaits
focused test; other configurations must be rebuilt before checkpoint.

LOG717 recorder source confirms Configure resets warmupRemaining to configured
2100 after renderer recreation; do not silently alter accounting to make180s
restart run pass. Managed helper lifetime previously ended after600 returns;
request a fresh generation on closed channel in managed mode, with supervisor
still rejecting unexpected retired-helper failures. Clean host shutdown may
classify final helper11/channel-closed as orderly only after actual published
returns; raw exits remain recorded, host failure/timeout never passes. Nine
launcher tests pass including false-success controls. Automation build launched.
Next validate sustained multi-helper lifetime separately, retaining the failed
restart timing acceptance and full transition requirements.

LOG716 managed-restart-c still fails overall host deadline: raw exits[1,0],
no forced launcher children. g1 retires after279 Presents; g2 completes660
Presents/600 returns. Explicit closure shortens restart handover (g2 source
sequence starts earlier), but does not establish time-budget acceptance. Because
g2 reaches its bounded limit before the later anchor change, this run does not
validate that scene-boundary case. Preserve failure; do not call explicit close
a throughput fix. Next separate recorder post-restart warmup/sample accounting
from actual render cadence and continuous helper lifetime before further runs.
No deadline increase or final-gate scope reduction authorized by this result.

LOG715 explicit channel retirement marks ready0 only for consumer owner or
successfully claimed publisher. Failed duplicate opens retain no close authority.
Add three actual channel tests: duplicate isolation with successful transfer,
immediate closed receive after publisher retirement, and rejected reclaim.
Supervisor accepts documented channel-closed retirement only for superseded
helper; unrelated failures still abort. Automation build/test launched; live
handover speed and unchanged-bound completion remain pending.

LOG714 managed-restart-b fails overall: host1 timeout, final helper0 with660
Presents; generations1..4 retire and generation5 delivers600 returns. Recovery
is functional but repeated handovers exceed unchanged host budget. No forced
launcher children; no clean overall pass. Do not increase budget to hide gaps.
Next remove avoidable5s receive-timeout handover by explicitly retiring a claimed
publisher channel so its helper unwinds promptly. Failed/unclaimed publishers
must not close someone else's channel. Preserve per-generation errors and test
duplicate-open isolation. Earlier attempted empty documentation patch failed
without modifying files; corrected documented managed-session scope is retained.

LOG713 scene-boundary candidate builds in all four configurations; enabled
selftests750/0. Eight launcher tests pass, including explicit managed-mode gate
and rejection of superseded helper crash/invalid-continuity outcomes. Only clean
exit or documented bounded source timeout can retire a superseded helper without
aborting supervision; final helper failure still fails the run. Launch managed-
restart-b with2400 restart and unchanged bounds to test fresh generation after
both renderer restart and later support-change rejection. Outcome pending.

LOG712 managed-restart-a advances g1->g2 at renderer restart and actual g2
process publishes447 new returns, with completed evaluated Presents through
source700/current708. Fresh-session resumption is experimentally demonstrated,
but whole run remains incomplete: g2 exits11 after subsequent repeated
anchor-source-support-changed rejects. Do not report this as a clean recovery
matrix. Route that exact managed-mode rejection to retired channel/history plus
fresh generation request, keeping the anchor validator unchanged. Eight-session
cap remains; no generic invalid-data retry or old-token reuse. This new scene-
boundary behavior is not built/run yet; current host is still completing samples.

LOG711 handshake controls pass750/0 automation tests: actual named mapping,
eight unique token generations, exhaustion, invalid version without advance and
unavailable owner. Launch managed-restart-a through owned launcher with explicit
restart injection at2400, unchanged bounded host/helper budgets and no capture.
This is the first real managed-resumption test; outcome pending. All previous
single-channel failed restarts remain retained. No external settings changed.

LOG710 begin opt-in managed session handshake. Launcher owns16-byte named
control mapping with magic/version/generation/owner PID. Renderer validates live
owner, atomically requests generation1..8 and derives fresh root-gN channel;
resource release/epoch change requests renewal. Existing single-claim data channel
and receipt checks remain unchanged. Launcher starts a new helper per requested
generation after bounded old-helper unwind, retaining superseded exit results.
No automatic feature enablement outside explicit --managed-session; no external
config writes. Automation build and existing launcher tests run; handshake
negative controls, full builds and actual restart resumption remain pending.

LOG709 final serial builds pass after duplicate cleanup: automation/baseline/
no-NGX738/0, feature-off linked. Save session visibility/manual-input and marker
correction checkpoint with LOG706/708 failed continuation retained. Full live UI
and manual-player acceptance remain pending. Next fresh-session ownership must
replace token and reset retained renderer/consumer history together; no receipt
counter reset or old-channel reclaim permitted.

LOG708 session-restart-b host exits0 with1200 samples, renderer_reinit=pass and
clean_close=yes. Helper exits1/outcome11 after274 Presents: automatic continuation
still fails. Marker parses as JSON with main_frame2400; live renderer logs old
token already claimed/relaunch required. This accepts marker correction and
explicit stopped-session diagnosis only, not full restart recovery or visually
verified native fallback. UI screenshot not obtained. Diff review removes two
duplicate classic-locale calls (savestate/device-removal already had them),
leaving six newly corrected streams. Six launcher tests pass. Prepare scoped
checkpoint and continue lifecycle integration without repeating this stop test.

LOG707 restart follow-up source inspection confirms channel publisher claim is
single-use (atomic publisherPid0->PID). Reusing old token after teardown is not
a supported reconnect; do not clear ownership/sequence checks to force recovery.
Post-restart log tail contains0 combined Presents and0 accepted Remix evaluations
across13252 lines. This supports stopped delivery, not visual native-frame proof.
Mark channel-publisher-already-claimed as stopped/relaunch-required rather than
indefinitely waiting. Add classic locale to remaining actual-device-removal JSON
marker. Prior marker-fix automation build/selftests738/0 passed; current changes
await full builds and focused runtime repeat. Helper restart remains explicit.

LOG706 session-restart-a is CORRECTIONS_REQUIRED: helper exits1/outcome11 after
271 Presents with bounded receive timeout following renderer restart. Host
finishes1200 samples and clean close but exits1 because restart marker contains
locale-grouped main_frame2,400, invalid JSON and mismatching expected2400.
Actual log reports reinit requested/completed at2400; this does not waive the
failed marker or establish automatic session recovery. Add classic locale to
seven developer transition JSON streams to avoid grouping. This reporting fix
is not yet rebuilt/retested and does not fix the helper timeout. Preserve both
failures. Native fallback/no stale neural output still needs scoped inspection.

LOG705 all status builds complete: automation/baseline/no-NGX738/0, feature-off
links. Launch focused session-restart-a using current binary and existing bounded
noncapture run, renderer reinit at main frame2400 after source delivery begins.
1200 samples after2100 warmup; unchanged660 helper/180s host bounds. This tests
changed-route stop/fallback reporting, not automatic resumption. Preserve any
helper source-timeout as failed continuation rather than hiding it behind host
success. No external config edits, no synchronous evidence capture.

LOG704 add mutex-published Remix session fields to existing live neural status:
requested, stopped/relaunch-required, channel open and retained source frame.
Settings panel and late overlay distinguish these from helper liveness and
external presentation proof. No session auto-restart or fallback policy change.
Four status tests cover default/waiting/open-unverified/stopped precedence.
Automation selftest738/0; serial configuration build chain launched. Actual UI
visual exercise and runtime restart transition remain pending; labels alone do
not prove those behaviors. Manual-input launcher remains separately uncommitted.

LOG703 inspect interactive seams: user neural mode UI is settings_video.cpp;
Remix channel still comes from environment and renderer requires a new token on
epoch change. Helper has660-frame and source-wait bounds; do not expose this as
unrestricted gameplay yet. Add explicit --manual-input to owned launcher: pass
input-replay=no and remove replay-specific producer2090 start gate, retaining
renderer title/3D/viewport guards and all runtime bounds. Six launcher tests pass,
including default scripted behavior and manual command construction. Manual live
gameplay has not been run; no interactive acceptance claim. Next integrate clear
session status/unsupported-restart behavior with existing neural UI before any
ordinary-gameplay promotion. No renderer/binary changes in this slice.

LOG702 repository launcher live-b exits0: host/helper[0,0], no forced children,
1200 host samples and clean_close=yes,660 helper Presents/outcome0. Existing
40-object cleanup warning persists. Five preflight tests pass: no writes/default
opt-in, inherited controls scrubbed without parent mutation, existing output,
missing input and unique channels. Add REMAKE-LAUNCH.md with explicit prepared
host requirements and bounded experimental scope. No fresh external provenance,
general interactive gameplay or full timing acceptance inferred. Forced-launcher
termination/descendant cleanup remains untested; normal clean close is verified.

LOG701 first launcher live attempt fails before process start: Windows rename
cannot archive a log across drives (WinError17). Preserve the failed output
directory. Replace rename with verified byte-identical copy, keeping originals
until the host writes its next log. Retry with a new output name; no runtime
or game process was started by the failed attempt.

LOG700 add repository-owned remake_launch.py bounded opt-in launcher using the
existing performance harness/helper, explicit prepared Flycast/runtime/legal
game paths and new output directory. Default is read-only preflight; --run starts
the established1200/2100 host and660 helper experiment, --anchored-light forwards
the tested option. Scrub inherited remake controls, preserve prior named logs in
new output, record owned executable hashes, never edit external configuration.
Preflight exits0. Live fc067-launcher-live-a launched with same supplied inputs;
outcome pending. This is a reproducible experiment, not unrestricted interactive
gameplay or external presentation proof. No proprietary dependencies acquired.

LOG699 guarded live run anchored-light-guard-a completes host/helper0 with clean
close,1200 host samples,600 published returns and252 light recreations all0,0,1.
Helper SHA067F0A83B3CDE8819E33754D7E373FDEE466EF6F6F7A475D18F969431CAF6322.
No image capture or synchronous sentinel; this verifies guarded live delivery,
not full performance acceptance. Latest completed Present source2783/current2785
logs external_nr=false; no fresh external-output provenance claim is made.
Earlier matched light comparison completes294 frames with0 HUD mismatches;
returned RGB MAE across candidates averages7.29293 (8-bit units), not a quality
score or isolated light-only effect because temporal histories may differ.
Midpoint2344 visually inspected: both outputs remain bright/soft; no winner.
Moving GIF retained but not claimed as fully reviewed perceptual acceptance.
Fixed-light option remains opt-in.40-object runtime warning remains unresolved.

LOG698 anchored light guard is now checked on every submitted packet, not only
resource recreation. Retain sequence-owned direction outside resource lifetime;
reject changed epoch/game/origin, unanchored scope, nonfinite/nonunit forward.
Eight focused tests pass including preservation after rejected inputs and changed
camera rotation. All four serial builds exit0; automation, baseline and no-NGX
selftests734/0, feature-off no work required. These are incremental builds.
LOG697 live300-frame evidence precedes the guard refactor; do not claim a fresh
post-guard gameplay run. Next verify the guard against the saved valid sequence
and finish matched moving-light review before committing this bounded slice.

LOG697 anchored-light-a host/helper exit0, clean host close,300 consecutive
captures2191..2490. Review completes with no gaps or failed nonempty HUD,
composition/backbuffer/Present checks.253 actual light creations all log0,0,1.
294 overlap frames against observer-combined-b preserve exact camera/geometry
and12863 mesh material identities; six unmatched frames each side retained.
Helper SHA256156EC007666CEC7A52B42C46497C3297086C1C2BF5B014F9AB4E191E7397AD8C;
host4FDF75A2E3805C58ED7A735914C7CE17DCD14434961B4353EA4E1616536BD0AB.
External configuration hash unchanged. Midpoint2341 reviewed: fighters/arena
present and HUD intact, bright/soft experimental rendering remains; no quality
winner or moving lighting-stability acceptance from this still. Existing40-object
runtime cleanup warning persists. Matched moving light comparison generated on
evidence drive; review report is not external provenance or performance proof.
Next finish fixed-light scope/continuity negative controls and remaining builds,
then inspect matched moving lighting before deciding whether this opt-in should
be the supported anchored-scene default. All broader acceptance remains open.

LOG696 light integration inspection finds current live helper light direction
is reselected from camera.forward on each material resource rebuild. Add explicit
--scene-light-anchor harness opt-in: retain first direction across rebuilds,
require anchored diagnostic packet scope, log each actual CreateLight direction.
Default unchanged; this is an authored directional light, not recovered game
lighting. Automation build/selftest command exits0. Launch bounded300-frame
capture using unchanged intensity3/external config,1200 host samples after2100
warmup,660 helper frames, existing420s host/300s diagnostic helper budgets.
Compare direction logs and completed output/HUD against prior moving evidence;
image capture is excluded from performance claims. Pending live outcome and
remaining configurations. No proprietary binary or external config modified.

LOG695 full-sequence anchor analysis completes300 frames.15 unique meshes/1596
expanded vertices match every frame with maximum anchored displacement
0.0000171661376953125. Ignoring camera motion (comparing the same points in each
frame's view coordinates) yields maximum displacements9.4674..17.3037 for these
same meshes. Three persisted analysis tests pass: static/translation/ambiguity,
camera-motion negative and missing-track coverage. Diagnostic projected point
map visually compared with original native frame2197: stationary points occupy
arena regions; large moving clusters align with fighters. This is visual region
correspondence, not a per-triangle semantic classifier. Red points also occur in
upper scene; do not declare all background stationary or all red points fighters.
Report anchor-stability-control-003c0b42.json and point map retained outside repo.
Result supports a stable anchored arena subset for this interval, not general
physical world/camera recovery. Next integrate this supported subset into a
controlled fixed-coordinate light comparison using existing public light controls,
checking whether lighting remains anchored as camera/fighters move. No new
external configuration edits, no weakened camera acceptance, no strict replay
restart. Serializer commit003c0b42f is pushed; analysis files remain uncommitted.

LOG694 implement read-only anchored coordinate stability analysis, matching only
unique topology/UV/texture/state buckets, not draw ordinal. First attempt fails
because augmented offset assignment loses the string length-prefix advance;
fix explicit length read and retain failure. Corrected run completes300 archived
frames2197..2496, with30 unique baseline-to-last matches and changing camera
pose. Report anchor-stability-003c0b42.json retained on evidence drive. Static,
known3-unit translation and duplicate-bucket rejection controls pass. This is
coordinate drift evidence only: arena/fighter classification and all-frame
stability acceptance remain pending. No renderer or camera policy changed.

LOG693 resume verifies final baseline/no-NGX selftests726/0, feature-off final
build complete, automation CLI suite726/0; no live gameplay/build process remains.
Prior tool run verified all300 archived packets2197..2496 against the legacy
writer; incomplete packet returned1 and missing CLI argument2 as required.
Const writer accepted only for byte-preserving serialization and modest observed
publication reduction. No end-to-end speed acceptance. User priority correction
ends this profiling slice and returns active work to supported camera/scene
integration. Anchor inspection confirms common observed basis, not recovered
physical world/game camera. Static arena stability must be independently tested;
screen reprojection alone cannot establish camera semantics. Intentional source
trails remain protected. Two attempted wildcard/nonexistent source-file searches
failed harmlessly; explicit source discovery located the header implementations.

LOG692 serializer baseline/no-NGX builds and726/0 suites pass, feature-off
links. Add read-only wire-parity --packet CLI using existing72MiB parser bound
and old/new writer oracle; automation rebuild and726/0 suite pass. Run it over
the300 saved real-game packets, plus incomplete archive and missing-argument
negative controls. Full output retained by tool execution; final count below.

LOG691 const-wire-a host/helper exit0 and clean host close.600 publication
scopes median16.4851ms versus17.9098ms prior; parent scene-feed35.8003 versus
37.5663ms. Modest observed improvement only, not solved throughput. Other
medians packet-build4.386/anchor5.2602/snapshot1.4526/view1.2505ms;597
returned-evaluate scopes10.9258ms. Finish configuration checks and actual captured
packet oracle coverage before acceptance. No full pipeline/performance claim.

LOG690 const-wire-parity-final automation build exits0 and726/0 suite passes:
old mutable writer and new const writer are byte-identical for versions1/3/4
and version2 thresholds0/128/255. const-wire-a repeats the same bounded CPU
scope gameplay run, no image capture/config changes. Live timing pending;
remaining builds and real captured packet compatibility still required.

LOG689 corrected oracle build also exposes existing function-like expected
macro collision. Rename local stream referenceWire; do not undefine shared
macros or change production behavior. Second failed build retained.

LOG688 const-wire-parity build fails C2079 because the new harness oracle uses
ostringstream without including sstream. Add the direct include; preserve failed
build log and rerun. Initial720/0 preceded the oracle and is not parity evidence.

LOG687 implement read-only wire output without packet/DDS deep copy. Existing
input parser remains; writer preserves versions1..4, byte/count bounds and
prevalidation. Initial automation build and720/0 suites pass. Add old mutable
writer as explicit harness-only parity oracle plus version1/2/3/4 exact-byte
checks (including threshold0/128/255). Parity build pending; no runtime speedup
or full wire acceptance claimed. Live publication never invokes the oracle.

LOG686 cpu-scope-c host/helper exit0. Publication is the largest measured feed
substep; interim38 samples median19.2303ms versus packet-build5.8829 and
anchor6.5126. Source inspection finds SerializeRemakeViewPacket deep-copies the
entire packet including DDS payload before its shared mutable reader/writer.
Next const write path must preserve every schema/bound/validation and exact
serialized bytes, versions1..4; no digest or source-identity weakening. This
identifies a concrete avoidable copy, not its isolated measured contribution.

LOG685 cpu-scope-b host/helper exit0. Interim126 snapshot/conversion samples
median1.4747/1.4472ms, versus parent scene-feed37.9744ms; neither alone explains
the parent. Add packet-build/camera-anchor/channel-publish subscopes, same exact
opt-in and600-call cap each, preserving nested timing semantics. No behavior
optimization stacked. Rebuild pending; final distribution retained in raw log.

LOG684 cpu-scope-a host/helper exit0.600 scene-feed scopes median37.2976ms,
P95 43.2204;597 returned-evaluate scopes median11.0625ms/P95 12.6994.
These include driver waits and early returns; no timing sums or pure GPU claims.
Narrow the measured larger feed stage with source-snapshot and view-scene
subscopes, each capped600. Parent includes child times, so never add them.
No behavior change. Rebuild and focused timing next before choosing optimization.

LOG683 CPU-scope automation build exits0 and720/0 selftests pass. Start
cpu-scope-a: existing1200-host/660-helper noncapture OIT route on second drive,
only CPU_TIMING=1 added. At most600 logs per owned stage; classify this as
diagnostic, not performance acceptance. Other configuration builds remain pending.

LOG682 combined-profile-a host/helper exit0 and clean host close; WPR never
recorded, so no sampled profile exists. Add off-by-default exact-value
FLYCAST_REMAKE_CPU_TIMING=1 scopes around eligible scene feed and returned
evaluation. Each scope logs at most600 invocations per render thread/process;
disabled path reads no clock. Elapsed CPU wall time includes driver waits and
early returns, not pure GPU execution or per-function observer cost. No GPU
flush/readback/wait added, no history/presentation policy changed. Build pending.

LOG681 WPR CPU start rejects with0xc5585011, failed to enable policy to profile
system performance. No policy change/escalation or private binary inspection.
combined-profile-a remains a bounded normal run, not an acquired CPU trace.
Fallback next: opt-in coarse owned CPU timings at observer/scene preparation
and submission boundaries, with uninstrumented baseline retained. Do not infer
function costs from this failed sampling attempt or keep retrying the privilege.

LOG680 installed WPR supports CPU sampling and reports no active recording;
tracerpt is present, but this Release build has no local PDBs. Plan a bounded
10-second CPU trace during active combined gameplay, stored on second drive;
inspect only owned Flycast execution, do not resolve/inspect private neural
binary internals. This first trace may establish process/module scheduling only,
not function attribution. Use no trace timing as performance acceptance. If
profiling privileges are unavailable, retain exact error and use owned timing
instrumentation rather than escalating or installing a dependency automatically.

LOG679 optimized observer-cadence-combined-a host/helper exit0 and clean host
close.595 remake-evaluated Presents,597 accepts,zero identity mismatches,mean
latency1.994975/max2. Active interval lower-rank Present P50/P95/P99
71.7934/87.356/93.1035ms versus LOG66160.6342/69.873/75.2718ms: no combined
speedup; regression retained, not explained away by isolated observer results.
Only10 active GPU-valid samples; do not use aggregate pass timings to infer a
bottleneck.40-object helper warning persists. Next bounded sampled profiling
of owned observer/scene-feed work with no further speculative optimization.

LOG678 1509ba9c8 pushed with matching fork SHA; tracked worktree clean before
this evidence update. Postcommit serial four builds exit0 and enabled suites
720/0. Launch observer-cadence-combined-a on second-drive outputs,1200 host
samples/660 helper frames, no image capture, unchanged default watchdogs,
consumer tuple and640x480 OIT D3D11On12. Compare active combined interval
against palette-cadence-a; no claim of full600-frame/99-percent acceptance or
cycle/audio equivalence merely from a clean process exit.

LOG677 remaining baseline/no-NGX/feature-off builds exit0, baseline/no-NGX
suites720/0. Together with automation this completes four builds and three
enabled suites for the performance exit correction. Exhaustive helper tests
prove forced termination returns failure; earlier real clean-close runs exercise
the success route. No newly injected forced-close process test claimed.

LOG676 performance exit helper automation build and720/0 suite pass, including
all eight report/check/forced-close combinations. Remaining configurations run
serially. Next optimized combined cadence uses no image capture and second-drive
outputs; compare active interval against LOG661 rather than treating isolated
observer improvement as a measured combined speedup.

LOG675 observer optimization9200902e1 is pushed with matching fork SHA.
Postcommit serial four builds exit0 and three enabled suites712/0; this was an
incremental workspace validation including the separately pending harness fix,
not a clean exact-SHA build. Add exhaustive eight-combination tests to the actual
performance exit helper: only written report, successful checks and no forced
termination returns0. Automation rebuild/test pending. No shutdown timeout or
rendering policy change; this cannot fix the underlying resource warning.

LOG674 full review process exits0:300 frames2197..2496, no gaps or failed
composition checks. Moving artifact generated; midpoint2347 visually inspected
with intact HUD and the existing bright experimental rendering, not a new
quality winner or full moving perceptual acceptance. Accept the bounded observer
invalidation optimization based on two isolated timing runs,712/0 suites across
enabled configurations, feature-off build and exact264-frame source parity.
Forced-close exit predicate remains a separate uncommitted harness correction.

LOG673 observer-combined-b host/helper exit0, clean host close and660 helper
Presents. All300 records2197..2496 complete; review JSON passes nonempty exact
HUD/composition/backbuffer/completed-Present checks with no source gaps.
All264 baseline overlaps2197..2460 match byte-exact camera/geometry/material,
original-native PNG and original HUD mask. GIF generation still finishing;
do not claim moving visual review from numeric checks alone. Storage retry
succeeds without deleting old evidence.40-object helper warning remains.

LOG672 remaining serial baseline/no-NGX/feature-off builds exit0; baseline and
no-NGX selftests712/0. observer-combined-a review completes281 records with
gap2441->2461 and zero completed-record composition failures, not300 success.
observer-combined-b repeats identical settings/bounds with captures and host
report on the second drive. Config hash remains unchanged; incomplete original
attempt preserved. No builds run concurrently with gameplay.

LOG671 observer-combined-a host/helper exit0 and clean host close,660 helper
Presents;40-object warning persists.300 attempted capture directories but only281
completed records: archive-write-failed begins2442, with system drive about200MB
free at inspection.227 completed overlapping sources2215..2441 match exact
camera/geometry/material/native PNG/HUD mask against palette-upload-a. Full300
acceptance is rejected; preserve partial files. No user data deleted. Create a
separate evidence directory on the second local drive (~94GB free) for subsequent
bounded captures. Do not rerun large captures on the near-full system drive.

LOG670 extend material comparator with explicit --exact-geometry requiring
byte-exact camera/lens/origin, vertex/normal/UV/color and index payloads with
length delimiters. Default material-only scope unchanged. Index and camera
mutation controls are detected only in the expanded scope. Partial live check
accepts115 overlapping frames including exact original-native PNG and HUD mask;
full observer-combined-a run/review remains pending. No tolerance introduced.

LOG669 automation rebuild after forced-close reporting correction exits0 and
712/0 selftests pass. Start observer-combined-a using existing300-capture
1200-host/660-helper diagnostic bounds, identical tuple and temporal guidance.
Compare source-qualified material and geometry payloads against palette-upload-a,
plus nonempty exact HUD/composition/completed Presents. Diagnostic capture is
excluded from timing conclusions; no new external configuration/provenance sweep.

LOG668 observer-invalidation-b repeat exits0 with clean_close=yes and1200
samples, Present P50/P95/P99 42.6773/48.8893/50.1789ms. Both candidate runs
improve observer-only median versus68.1854ms baseline; combined output parity
and full configuration builds remain necessary before acceptance. Do not call
the35-37 percent total-frame reduction a directly sampled function-time result.

LOG667 shutdown inspection confirms performance harness forcibly terminates after
its existing5-second close bound but omitted forcedTermination from exit status.
Add !forcedTermination to final success predicate; preserve reports, timeout and
all other checks. This is a reporting correction, not a shutdown fix. Current
observer-invalidation-b uses the pre-correction executable; build/runtime failure
validation follows after it terminates. Prior clean_close=no results remain
failed shutdown evidence even though the old command exited0.

LOG666 observer-invalidation-a exits0,1200 samples, Present P50/P95/P99
44.176/49.0806/50.2646ms versus baseline68.1854/74.812/77.6966ms.
First-run median improvement35.2 percent; not yet accepted as repeatable or
combined-output-safe. clean_close=no persists. Start identical candidate repeat
observer-invalidation-b before combined geometry/material comparison. No other
observer optimization is stacked. Profiler discovery found installed Windows WPR
and VS collector; neither recorded a trace or changed any system session.

LOG665 candidate changes only arithmetic-origin invalidation: disengage optional
transform and clear value/epoch rather than assigning the entire empty record.
Active-count decrement and all lookup predicates remain unchanged. Automation
build and712/0 selftests pass, including explicit cleared-authority control.
observer-invalidation-a repeats the identical1200-sample observer-only run.
Timing benefit is pending; do not commit as a speed fix without measurement.
Baseline clean_window_close=false is retained; unrequested transition flags in
the launch report do not establish that those transitions were exercised.

LOG664 observer-cadence-a exits0 with1200 native samples and no neural accepts,
Present P50/P95/P99 68.1854/74.812/77.6966ms. Native baseline median11.1101ms.
Observer-only reproduces severe slowdown without Remix or neural evaluation;
GPU timestamp spans alone do not identify active GPU work versus starvation.
Harness reports clean_close=no despite process command exit0; retain as unresolved
shutdown evidence. Optimize observer work without dropping required provenance,
then rerun isolated timing and exact scene/material comparisons. No DLSS timing
blame or performance success inferred from this diagnostic.

LOG663 native-cadence-a exits0 with1200 native Presents, no identity gaps or
repeats. Present P50/P95/P99 11.1101/11.6342/11.8108ms, base PVR GPU median
0.10032ms. Start observer-cadence-a identical native run with only
FLYCAST_NEURAL_SOURCE_OBSERVATION=1 added. This isolates observer overhead from
the combined pipeline; no scene feed/helper/neural evaluation or config changes.
Do not convert frame-rate comparisons into cycle/audio equivalence claims.

LOG662 active595 combined samples in palette-cadence-a have Present lower-rank
P50/P95/P99 60.6342/69.873/75.2718ms, only19 valid GPU samples. Do not infer
per-pass bottlenecks from the aggregate615 valid samples, mostly outside the
combined interval. Start native-cadence-a with identical executable/host config,
replay/resolution/API/OIT and1200 samples/2100 warmup, native mode and remake
environment disabled, no helper. Preserve original logs by archive. This tests
total experimental overhead; source observation instruments CPU memory/transform
operations and must be isolated before attributing slowdown to RTX rendering.
Cycle/audio equivalence still needs explicit measurement, not FPS inference.

LOG661 palette-cadence-a host/helper exit0,1200 host samples and660 helper
Presents. Report contains595 consecutive remake-evaluated outputs2163..2757,
zero gaps/identity mismatches in that interval, all displayed samples reset=false,
mean latency1.994975/max2 frames. Overall1200-sample Present P50/P95/P99 is
60.6342/69.8332/78.644ms;615 GPU timing samples valid and585 invalid. Owned
neural GPU objects initial137/final152/growth15; VRAM falls168542208 bytes.
Helper still warns40 undisposed common objects. Not a performance or full
acceptance pass: startup/native tail are included, normal renderer unsupported,
paired native emulation/audio timing absent. Aggregate stage reset count597
must not be misreported as returned-history resets: displayed combined samples
and returned evaluation logs carry their own history state. Next isolate the
active interval and paired native timing before optimizing any presumed GPU
bottleneck. Material continuity is improved, not rendering speed proven.

LOG660 ea0842b79 pushed and fork SHA verified; postcommit serial four builds
exit0 and automation/baseline/no-NGX selftests each711/0. Existing untracked
user artifacts preserved. Next palette-cadence-a repeats the noncapture OIT
1200-sample host/660-frame helper route with default watchdogs and unchanged
consumer settings, checking sustained delivery after the material fix. No
synchronous image capture, no claim of complete GPU timing or normal-renderer
coverage. Normal native-effects snapshot is still unsupported in production.

LOG659 corrected serial validation exits0: baseline and no-NGX selftests711/0,
both builds and feature-off build link. Together with LOG656 automation build,
711/0 and native material fixture, the four configurations are checked for this
slice. These are incremental builds, not fresh exact-SHA evidence. LOG657's
300 consecutive interval and material equality accept the bounded upload fix;
full pipeline acceptance remains pending. Commit/push and postcommit checks next.

LOG658 material comparator controls reject four malformed packets (short header,
truncated body, trailing byte, wrong magic) and detect a mutated DDS payload.
Backlog contract inspection passes its document-only scope. Source2311 visual
inspection shows intact HUD and brighter/softer experimental world; no quality
winner declared. Baseline build links, but the first test command used the wrong
neuraltest executable directory and exited unsuccessfully before subsequent
builds. Retain remake-palette-validation-test.log; corrected serial validation
uses neuraltest/neuraltest.exe and is pending.

LOG657 palette-upload-a host/helper exit0,660 helper Presents. The executed
gameplay review accepts300 consecutive sources2161..2460 with no gaps and no
failed nonempty-HUD/composition/backbuffer/completed-Present checks. No
material-cache-pending entries occur in this run. The material comparator
accepts293 overlapping sources and12984 exact mesh material/state/generation
and producer identities against upload-a; unmatched sources remain reported.
This closes the observed material-delivery gap in this interval, not camera
truth, temporal quality, external provenance or performance acceptance.
The40-object runtime cleanup warning remains. Evidence: fc067-palette-upload-a-review
outside Git. Remaining configuration builds and comparator negative controls
must finish before this slice is committed; no new provenance sweep required.

LOG656 upload-diagnostic-a host/helper0 identifies stalled2182..2184 as
format65/A8 gpu_palette1 generated_mips0 owned_upload0. Extend owned indices
and capture the actual32x32 palette upload plus16/256-bank generation hashes.
Only exact resource/index revision and palette-bank generation seed the cache;
unqualified cases retain GPU staging. Clear palette owner at renderer teardown.
Native WARP material fixture passes171 raw/RGBA comparisons,52 negatives,
32 async exact-mip comparisons and98 async controls, including first-request
ready from owned indices/palette without a flush. Automation selftest711/0.
Next palette-upload-a repeats the300-source bounded capture; no observed gap
reduction yet. The first nonpaletted-only experiment's unchanged gaps remain
falsifying evidence, not a claimed success.

LOG655 upload-a host/helper0 and660 helper Presents, but all six prior material
wait sources remain2182/2183/2184/2279/2373/2374. No material-gap improvement
established; do not promote memory overhead as a performance win. Add format,
GPU-palette/generated-mip and upload-copy availability to existing pending
diagnostics, then run a short capture-free121-helper/240-host replay to identify
the unsupported path before extending the implementation. External settings
and generation guards remain unchanged; no timing-performance claim.

LOG654 upload-copy GPU fixture passes on native D3D11 WARP; exact encoded DDS
matches GPU readback for supported BGRA/16-bit complete mip uploads. Ownership,
foreign resource, upload/RTT revision and malformed layout negatives run. DDS
is encoded once per upload, not per draw;64MiB accounting includes raw and DDS.
Initial retained-resource compile failed on const ComPtr.get; corrected using
its actual const pointer conversion. Resource is retained to prevent address
reuse ambiguity. Final material fixture records167 raw/RGBA comparisons and48
negative controls plus existing async controls; automation selftest711/0.
Next upload-a repeats the300-source moving capture with identical external
settings/default light and diagnostic-only budget. Compare source packet/native
bytes and exact missing-source reasons to moving-long-a; no speed/quality claim
from synchronous capture, and no gap reduction claim before that result.

LOG653 material availability implementation candidate: retain bounded owned
CPU upload bytes only with the explicit async-neural route enabled. Match live
resource identity, upload revision, RTT revision, format, dimensions and mip
count before encoding source DDS without GPU readback. Process-wide64MiB bound;
unsupported/GPU-paletted/generated-mip cases retain existing async staging.
No raw geometry/history changes or waits. Reset at upload/delete and carry owner
on texture move. Material GPU fixture adds exact DDS/readback equality, source
mutation ownership and stale upload/RTT/layout negatives. Builds/fixtures/live
gap regression pending; do not claim this removes any measured gap yet.

LOG652 exact gap classification:2182/2183/2184/2279/2373/2374 report
stage=texture reason=material-cache-pending;2377 reports stage=credit
reason=no-return-credit. Do not attribute all gaps to texture loading or relax
the ownership/backpressure guard. Inspect both dependencies at their existing
bounded handoff points. A failed documentation patch made no file change.

LOG651 moving-long-a host/helper0,660 helper Presents/outcome0,300 captures
2164..2470. New reviewer independently verifies nonempty HUD, exact composition,
backbuffer RGB and completed Presents for all300; minimum protected18283 pixels.
Gaps2181->2185,2278->2280,2372->2375,2376->2378 remain explicit: not continuous300.
The earlier light-one-b negative correctly exits1 and retains visibly marked
failed frames2224..2232. Positive review exits0; quantized native/returned/combined
GIF and three stills saved at fc067-moving-long-a-review outside Git. Midpoint
2318 inspected; visual quality/scene completeness and external provenance are
not established by composition checks. Codex open request queued, not confirmed
visible. Runtime logs confirm diagnostic watchdog300/performance_eligible=false.
Four builds complete, three enabled suites711/0. Existing40-object helper cleanup
warning remains. At2182..2184 raw logs explicitly report material-cache-pending,
with returned2181 held until2185 material submission. Next address bounded
material availability without stale texture reuse or blocking emulation; inspect
all gaps before assigning a common cause. No lighting/default promotion.

LOG650 extended moving capture preparation: add explicit final
--diagnostic-capture-budget switch to the async returned-scene helper only.
It selects a300-second whole-process diagnostic watchdog after source receipt;
default30/120-second limits and production GPU/frame policies are unchanged.
The switch reports performance_eligible=false and rejects unsupported routes
before runtime loading. Automation build/suite711/0; actual unsupported-route
CLI exits2. Planned moving-long-a:300 unique captured sources,660 helper frames
(60 warmup),1200 host samples,420-second host watchdog, default authored light3,
same external configuration and sentinel off. This is not performance evidence.
Retain gaps/failures;300 captures alone do not prove300 consecutive accepted frames.

LOG649 relative HUD cohort live confirmation: hud-relative-a host/helper0,
240 helper Presents/outcome0,120 captures2162..2285 with gaps2181->2185 and
2278->2280. All120 independently checked nonempty masks, exact native/evaluated
composition, exact backbuffer RGB and completed-Present joins. Protected pixels
18392..20089, zero reported mismatches. Visual2282 confirms restored original
text/bars, unlike hud-broader-a. No guarantee for uncaptured2288 or later scenes.
Runtime executable SHA256 ECBD824FBAE3016EF75026A42733FFB688E231A9F1D24303A040B5CBFBF9C61A.
Automation/baseline/no-NGX suites705/0; four incremental builds pass. Initial
ArrayView.size() compile failure retained, fixed to the actual size member.
ACCEPTED for this broader captured HUD correction, not continuous300-frame,
full title/world reconstruction or performance acceptance. Residual helper
40-object cleanup warning persists. External configuration was unchanged.

LOG648 hud-broader-a host/helper0,240 helper Presents and120 captured sources.
The absolute-floor fix remains valid for its12 tested frames but broader coverage
falsifies sufficiency: visible native HUD at2282 again has empty protection.
Do not keep widening per-frame absolute depth limits. Implement a same-frame,
title-scoped atlas/layout certificate requiring header, timer and both health
bar sides at coherent foreground depth (2-percent agreement). Captured plate
layer ratio.9 supplies the foreground equivalent. Normalize only the copied
title classifier record to.18; raw geometry/depth and generic matching unchanged.
Missing/incoherent cohorts fall back to existing strict classification, never
reuse old masks. Add negative tests and rerun the same120-source capture.
This relative profile is an implementation candidate, not yet live accepted.

LOG647 hud-fixed-a host/helper0,121 helper Presents;12 captures sources2221..2232
join12 completed Presents. Source2224 now protects20024 pixels instead of0;
all12 report zero HUD/backbuffer mismatches with19889..20059 protected pixels.
Independent image-array checks verify nonempty mask and exact native/evaluated
composition for all12; visual2224 inspection confirms crisp original HUD/text.
All four incremental builds and three699/0 suites pass. Runtime executable SHA256
82DB0834C99D72C4C5FCC7F7AE64CB13BD157AAA8D0F3F34851EDB52B8485F03.
External config hash unchanged222D059C727A683C8DEACE07C938002BDD6A69EB0DC9C908743E17A92B54EFBC.
ACCEPTED for this captured HUD rejection, not full title/camera/provenance gate.
The light comparer passes40 exact-scene/native/mask matched frames from the
earlier light-one-a interval, preserving gap2181->2185 and all unmatched frames;
its long-interval empty-mask failure remains retained. Short GIF/report outside
Git at fc067-light-one-short-review, no winner. New comparer checks composition
and completed Presents, never declares external provenance or matched history.
Next extend moving gameplay coverage across the repaired interval; no more
lighting tuning until overlay dropouts are excluded in that bounded sequence.

LOG646 hud-drop-b host/helper0; per-draw evidence at current2227..2238 shows
known foreground HUD atlases at minimum.142271 and name/background-plate layers
at.128044. Existing classifier required.15/.138 respectively, rejecting valid
atlases despite matching screen shape, blend and region. Four captured fixtures
fail before production change (695 passed/4 failed); unknown-atlas control passes.
Correction extends only known title-specific floors to.142/.128, preserving
the.24 upper bound, atlas/list/region/shape/RTT guards and generic classifier.
Four-build matrix and live pixel regression pending. This does not establish
arbitrary-camera HUD coverage; do not promote it solely from classifier tests.

LOG645 focused classifier capture attempt hud-drop-a host/helper0 but no
captures/evaluations: comparison-start requires moving capture, while per-draw
diagnostics require nonmoving capture. This incompatible launcher combination
is rejected by existing guards; not a classifier result. Corrected bounded
hud-drop-b removes comparison-start, starts publication at producer2150,
requests12 nonmoving captures,121 helper frames and360 host samples. Preserve
the failed attempt and existing guards; no renderer change is inferred from it.

LOG644 light-one-b host/helper0, helper121 Presents/outcome0; residual40-object
cleanup warning remains. Existing extended watchdog suffices without code changes.
The new light comparer rejects frame2224 for empty protection classification;
its Python invocation fails (the surrounding shell's later command returned0,
not a comparer pass). No comparison artifact/winner produced. Native2224 image
has HUD/text/health bars, combined2224 visibly alters them; both baseline and
candidate preview records report protected_pixels0. Prior zero HUD mismatch
counts at this source are vacuous, not overlay acceptance. Source2220 reports6
protected draws,2221 only1,2224 none. This is now the integration priority:
collect existing bounded per-draw classifier evidence and correct the actual
rejecting condition. No lighting promotion or guessed root cause. Captures and
failed comparison retained; new comparer stays fail-closed.

LOG643 authored Remix light control: optional final --scene-light-radiance
decimal0..30 in the standalone legacy game-scene helper; default3 unchanged.
Public CreateLight receives the value and helper reports it separately from
external settings. Automation/baseline/no-NGX suites694/0; all four incremental
builds complete. Actual CLI malformed nan and unsupported synthetic route each
exit2 before runtime loading. No external configuration changes.
light-one-a host0/helper1; helper's existing30-second watchdog applies at120
frames and expires after48 returned sources, leaving43 captures. This is not
a completed60-source run. Forty overlapping captured source packets are
byte-identical to integration-moving-a, zero mismatched overlapping packets.
Inspected2206 returned images preserve scene placement; radiance1 is slightly
darker, but substantial brightness remains. No quality winner, no default change,
and no assertion that the headlight alone explains the appearance. Next bounded
confirmation may use121 helper frames (existing120-second extended-return
watchdog),240 host samples and60 capture maximum; do not change GPU wait policy.
The original197 captures join197/197 completed Presents with zero reported HUD,
world-composite and backbuffer mismatches. These are composition checks, not
proof of full geometry or temporal visual correctness. Failed runs retained.

LOG642 integration priority correction: detailed GPU query availability is no
longer the immediate dependency. Postcommit enabled suites677/0 and feature-off
incremental build verified at c7c860888. Bounded integration-moving-a requests300
unique displayed captures,660 helper Presents and1200 host samples, sentinel off,
unchanged consumer configuration. Executable SHA256
102192E643B3D5FBF3C88D7EA626C47D716DD982BF1F179215E1D594045534FA.
Host reaches180-second watchdog: exit1, helper exit1/outcome11 after262 Presents.
Retain197 capture directories, not a300-frame pass. No process remains live.
Visual inspection of source2372 confirms fighters, arena, native hit effects and
HUD in combined output; combined scene is substantially brighter and softer than
native. This still frame does not prove temporal quality or full scene coverage.
Next inspect the retained moving interval and completed-Present joins before
changing rendering. Capture overhead is not performance evidence. Preserve the
timeout failure; any follow-up long capture needs explicitly larger bounded
diagnostic watchdog, not a rendering-timeout relaxation. GPU timing remains open.

LOG641 final camera/cadence checkpoint: automation, baseline NGX, no-NGX and
feature-off incremental builds succeed; three enabled suites677/0. Live camera
rejection reduction and617 exact cadence joins verified separately in LOG640;
invalid-timing handling does not establish GPU performance. Active backlog
consolidated to remove stale live-run instructions and contradictory stop claims.
No media/runtime/config/captures staged; preexisting user artifacts retained.

LOG640 cadence-b host/helper0; recorder now retains1200 CPU samples:597 accepted
evaluations,617 combined Presents,595 distinct sources,149 longest consecutive
source IDs, no frame-identity mismatch. Independent Counter join of every
(output source,current frame) exactly equals raw completed-Present log617/617.
584 valid GPU timing samples and616 unavailable samples; all unavailable PVR/
guidance/total fields independently checked null. Renderer query-ring busy616
no longer erases CPU cadence. Report source gaps0 refers to current-frame sample
coverage, not returned-source continuity; repeats24 includes held-native frames.
Displayed latency mean2.06462/max5. This validates recorder/camera improvement
only: full300-frame moving pixel proof, steady600-frame lane, timing completeness,
normal renderer and native timing comparison remain open. Do not promote149 to300
or count helper660 Presents as600 consecutive displayed gameplay frames.

LOG639 cadence-a host/helper0. New timing-valid diagnostics report1200 valid,
0 disjoint samples, while renderer ring_busy_count=616: LOG638's disjoint causal
claim is falsified. Earlier stage busy0 was NOT renderer query-ring busy. The
query ring exhausted after12 active samples and recorder omitted CPU metadata
until GPU queries resolved after combined work. Keep disjoint-null handling as
defensive behavior, but separate every CPU frame record from optional GPU query
storage. EndFrame owns ordered sample metadata even on query-ring busy; Present
marks that sample; later query resolution updates only timing by sequence.
No GPU wait or fabricated timing. Rerun sustained and compare every reported
completed combined frame with the independent raw Present log.

LOG638 correction to LOG635/637 interpretation: raw completed-Present logs show
cx315 Presents/278 distinct sources2165..2473, cy617 Presents/595 distinct sources
2163..2779. The reported10 was timing-sample-derived, not actual presentation.
cz diagnostic host/helper0 shows continued completed presentation beyond2175
and no policy-failure transition. Camera refinement DID materially improve live
delivery; the earlier claimed unchanged10-frame limit was false. Preserve that
failed diagnosis explicitly. Performance samples jump2174->2791: ResolveAvailable
discarded whole samples on disjoint/zero-frequency GPU timestamps. Retain their
valid frame identity/CPU Present data, report GPU values null and separate valid/
invalid timing counts; exclude invalid GPU values from percentiles. Also record
actual frame.resetHistory rather than hardcoded true. This is telemetry only,
not an invented GPU duration or relaxed presentation gate. Live verification pending.

LOG637 cy host/helper0,597 evaluated/owned outputs. Camera rejects fall281->1,
but combined presentation stays10/1200, so do not attribute the early stop to
camera errors. First combined interval source2163..2172/current2165..2174; later
evaluation remains consecutive. Remaining camera reject occurs2709, long after
the stop. Synthetic677/0 and live reduction support numerical improvement only,
not full camera or delivery acceptance. Add transition-only presentation-stop
diagnostics (current/candidate/enable/guidance/source/producer/latch) without
changing selection behavior; run bounded early interval to locate actual gate.
Do not extend numerical search or weaken fallback based on a wrong causal guess.

LOG636 deterministic64-pose clipped/off-screen camera fixture reproduces failure
before production edits: step2 projection0.010986px,676/1 suite. Bounded nearby
float-lattice correction moves failure to step12/0.031738px (676/1); ray-aligned
representable-coordinate-plane search moves failure to step44/0.014648px (676/1).
Add bounded same-ray sampling inside the existing depth error budget, followed
by unchanged projection/clip/depth checks:677/0 passes all64 poses and existing
wrong-camera/nonrigid/identity tests. Bounds:27 coordinate planes,129 depth
samples, at most728 neighboring points, only on an otherwise rejected rounded
vertex; no lens, world-origin, source or acceptance changes. Retain all three
failed attempts under remake-anchor-fixture/lattice/ray logs and successful
remake-anchor-budget logs. No claim that synthetic success fixes gameplay yet.
Next cy sustained OIT uses identical cx bounds/consumer, only camera correction.

LOG635 cx host/helper0,660 helper Presents. Raw renderer log597 accepted
evaluations,597 owned evaluated outputs,597 native-effect compositions,0 GPU
guidance rejects,281 camera-anchor rejects. Performance collector1200 Presents
reports10 remake-evaluated and1190 native,2 held-native samples,90 evaluation
records versus597 backend submissions: delayed-source collector attribution
is not equivalent to raw accepted-source count. No steady-delivery acceptance.
Observed source gaps677, output repeats2, measured displayed age mean1.75/max2;
owned objects137 initial/152 final (backend12 retained, not by itself proof of
leak), VRAM growth-170180608. Present interval P50/P95/P99=55.4746/67.1296/75.4532ms;
PVR13.2716/15.7066/17.3692ms, guidance9.7929/20.7868/23.3590ms. Tracing/effect-copy
cost remains; no native timing equivalence claimed. Presentation policy latches
native after gaps beyond8 frames, so evaluated outputs do not imply presentation.
Next isolate/correct camera float round-trip for clipped/off-screen vertices:
current anchor loop only corrects enclosure, and exits on initially enclosed
vertices even if projection error exceeds0.01px. Build a deterministic failing
numeric fixture, preserve threshold/clip/depth/source identity, then bounded
representable-coordinate correction. Do not weaken fallback or accuracy guards.

LOG634 cw host/helper0,1200 measured samples but0 accepted evaluations and0
combined Presents: REJECTED sustained integration, not pass. Native1200/1200;
VRAM growth-541274112 bytes, owned objects137 initial/131 final are native-path
observations only. Camera projection guard also rejected some later sources
(e.g.0.014648px versus0.01 bound); retain this separate integration limitation.
Root launcher issue verified with isolated process-env probe: null assignment
leaves empty present variable on installed runtime; comparison guard rejects
empty start even without capture. Correct launcher by removing absent variables
through Env provider in setup/restoration. Preserve strict invalid-input guard.
Retry cx under otherwise unchanged sustained bounds; no new production shader
or external configuration change. Native-effects copy/source tracing cost and
camera rejection remain to assess after actual combined evaluation is active.

LOG633 033b6ca7f post-commit four incremental builds and three676/0 suites pass.
Next sustained OIT run cw reuses LOG542 bounds:660 helper Presents (60 warmup,
600 returned-source budget),1200 host samples to leave shutdown margin,180-second
host watchdog. Live scene/camera/geometry temporal/native-effects integration;
no locked inputs, PNG preview, marker/sentinel, effect-identity readback or color
experiment. Keep source observation and paired returned-depth transport visible
as overhead; this does not by itself establish final99-percent/timing acceptance.
Measure exact matched receipts, accepted/evaluated Presents, source age/order,
resource trend and eligible delivery. Do not confuse1200 host samples with600
combined frames. Existing user consumer configuration remains unchanged.

LOG632 cv host/helper0,240 helper Presents. Existing three-way audit actually
passes cs/ct/cv for all28 frames2252..2279: identical evaluated color/depth/motion/
mask, matching marked/clean pre-marker output hashes, external output differs on
all28, exact post-native-effect HUD/world/backbuffer and completed Presents.
Consumer tuple positively reported: upscalingOFF intensity1 globalTone1 white203
preset0 style0 enabledON. OFF SAFE MODE and full30 accepted/reset sequence checked
separately; OFF produces no external preview Present. D-191 correction preserves
history without granting external output eligibility. cs/ct executable349FE868
and cvE0A30776 differ only by the recorded readiness/history correction, not
source/guidance inputs; provenance proof is scoped to these exact binaries and
28 moving source frames. ACCEPTED changed-guidance provenance slice, not quality,
performance or full pipeline. cr/cu and the over-broad PNG check remain failed
evidence. Close this dependency; do not repeat generic transport or mask tuning.

LOG631 cu host/helper0, SAFE MODE confirmed,30 successful public evaluations
but each reset; no external presentation accepted. Existing three-way audit
actually run and fails neural input mismatch2252. Readiness return occurred
before retained source history even after SubmitStatus::Submitted, unlike ON.
Move only the external presentation readiness check below successfully submitted
source-reference retention. Failed/busy evaluations still return before history;
external-ineligible output still cannot be wrapped/presented. This distinguishes
public evaluation ownership from external presentation proof, not acceptance of
an unavailable external result. Rerun OFF and compare exact histories/inputs;
retain cu failure. No broad gate or completed-pipeline claim.

LOG630 ct restored run host/helper0,28 captures. Initial combined hash check
failed because it incorrectly required marked pre-effect PNGs to equal restored
PNGs: the former intentionally contain the sentinel. Isolated source/effect/
motion/mask files match28/28; actual evidence-readback tuples (color/depth/motion/
mask and pre-marker returned hash) match all30 cs/ct evaluations2250..2279.
No check was relaxed on the actual pre-marker hash contract. cu hooks-disabled
restored control launched in supplied OFF stage with unchanged config; host29799,
helper77069. Complete the existing audit only after terminal state and SAFE MODE
verification. Source/effect input equality is distinct from marker presentation.

LOG629 cs corrected marked run host/helper0,240 helper Presents,30 accepted
sources2250..2279 with one reset and28 captures2252..2279. Independently checked
exact full accepted sequence plus28 source packet/color/depth/effect/alpha/motion/
mask pairs against co. Marker log has34 observations (including repeated/final
Presents), not34 distinct source frames; final acceptance uses source joins.
The prior five-second delay is absent. ct restored-output control launched with
same executable/configuration/archive/boundary; cs logs archived uniquely.
Existing cleanup warning remains. Full marked/clean/OFF acceptance awaits the
remaining controls; this short synchronous run is not a performance result.

LOG628 cr terminates host0/helper1 and produces no accepted replay captures:
all attempted sources2250..2279 reject producer identity. Source2250 is producer
2281 versus archive/cq2249. Its startup log explicitly reports evidence arming
delayed5000ms; cq has no such delay. ensureNeuralResources returns before renderer
instrumentation progresses while PVR producers continue. Fix only the bounded
remake-evidence launcher to set EvidenceStartDelayMs=0; its existing source2250
comparison boundary remains authoritative. Do not normalize producer/frame IDs,
change game clock, relax archive checks or alter ordinary evidence defaults.
Retain cr as a failed comparison and verify the corrected replay live.

LOG627 5e68316f0 post-commit four incremental builds and three676/0 suites pass.
Tracked worktree was clean; only the three preexisting untracked entries remain.
Begin focused changed-temporal-guidance external-output check: cr marked lane
uses ck frozen scene/color/depth/effects, boundary2250, temporal raster enabled,
shading experiment explicitly disabled. Existing supplied ON configuration is
unchanged. This verifies the changed guidance dependency, not generic transport.
Require marked/restored/OFF exact evaluated input hashes, common accepted history
and source identities, active consumer tuple and final completed Presents before
acceptance. Do not count synchronous evidence as performance. No live external
configuration or third-party binary modifications are authorized or performed.

LOG626 final shading-experiment checkpoint: all four incremental configurations
build; automation, baseline NGX and no-NGX selftests each pass676/0 in
remake-color-final-* logs. Disabled-mode source-color retention is suppressed.
Backlog consistency and diff whitespace checks pass. Scope is tested opt-in
mechanism plus rejected/no-effect gameplay evidence, not a quality improvement.
cp/cq logs and comparisons remain outside Git; user assets remain untouched.

LOG625 cq host/helper exit0. cn/cq comparer passes28 exact-source/effect/history/
HUD/backbuffer/Present checks, with public temporal metrics exactly matching co:
source3.4292784, gradient2.7305470, temporal1.7783678. Independent SHA256 check
finds co/cq motion identical28/28, masks different28/28, and pre-effect neural
output PNGs identical28/28. The changed public mask has no observable output
effect in this installed configuration/interval; do not generalize to all NGX
models or claim private parameter semantics. Park D-190 as an off-by-default
diagnostic experiment with no demonstrated quality benefit. Preserve cp failure
and cq no-effect result. Avoid retaining extra source color when experiment is
disabled. Next complete the focused combined changed-guidance provenance slice
using frozen-source inputs, then sustained ordinary combined integration. No
more threshold tuning, no reset-only default promotion, no generic Gate10 rerun.

LOG624 motion-preserving shading variant automation build and676/0 selftest
pass. cq public frozen gameplay launched with unchanged OFF configuration and
executable SHA2565CA26151AB24F9CCD9DC6540FB67C2B1F86B488A568EB7CF0DE9DE14F0E87D06.
Host/helper launch handles84742/12779; inspect terminal state before shared
builds. cp logs archived by checked unique-name move. cq comparison pending;
do not treat prior four-build results as verification of this final correction.

LOG623 cp host/helper exit0;28 matched captures2252..2279 pass comparer source,
effect, history-start, HUD, backbuffer and completed-Present checks. Activation
is positively logged. Comparison cn/cp generated and midpoint inspected.
Reject as quality improvement: cp source MAE3.4842126 and gradient2.7860038
versus co3.4292784/2.7305470; temporal delta1.7631238 versus1.7783678 does not
outweigh degradation. Native effect/HUD composition remains exact. Public
backend explicitly forwards frame.mask as pInBiasCurrentColorMask; this is not
an absolute history discard contract. Correct the experiment's concrete flaw:
shading rejection zeroed otherwise valid geometric motion. Preserve that motion
and confidence while biasing current color, retaining zero motion for actual
geometric failure. The GPU changed-shading fixture now moves4 pixels and must
retain -4 motion. No threshold search or default promotion; rerun focused checks
and same frozen public interval before combined propagation. cp evidence retained.

LOG622 D-190 four incremental builds complete successfully; automation, NGX and
no-NGX suites each pass676/0 (remake-color-build3/selftest3 and baseline/no-ngx/
off logs). Added CPU assertions prove color independence, failed-evaluation
retention and accepted advancement; reset assertion also checks color release.
cp public-DLAA temporal plus color consistency launched on frozen ck inputs,
source boundary2250, hooks-disabled supplied stage, unchanged configuration
SHA256656051579D08B667346575164B3C3D8490F40DDD55DB74C8199B0996B56AF2C7.
Executable SHA256E29E153B29DFC3E0CF667A6E7484BD3F51718FD71980E8C3C51BB493813047CD.
Bounded240-frame host/helper,30 requested captures; do not rebuild shared tools
until terminal. Host/helper live handles76142/63981 at launch, not completion
evidence. Compare source/effect identity and accepted sequence against cn/co,
then component metrics and moving output. No external-consumer configuration
changes and no performance claim. Prior co stage logs preserved separately.

LOG621 a701b7155 plus WIP: implemented D-190 opt-in returned-color consistency
in production guidance shader with bilinear sampling, SDR8/255 rejection and
reason8; retained source RGB advances with accepted geometry/depth only. Added
GPU stable/change/alpha-only and malformed-color controls to both API fixtures.
First build launch failed before compilation because VsDevCmd used an invalid
host argument; no build log was created. Corrected host_arch launch compiled
and selftest passed673/0, including the new GPU controls. Added three explicit
CPU color ownership/failed-evaluation/accepted-evaluation assertions afterward;
serial automation/baseline/no-NGX/feature-off builds and suites are underway
under remake-color-* logs. No live benefit, performance, or full acceptance is
claimed. Existing failed evidence and user assets remain untouched.

LOG620 2026-09-09 a701b7155 plus existing comparer-label WIP: resumed from
explanation-only turn; process inspection finds no live host/helper/python.
co publisher reports clean_close=yes, helper shutdown outcome0; existing common
device-object cleanup warning remains unresolved. Archived final OFF-stage logs
without overwriting existing evidence. Host reports SAFE MODE/all hooks off.
Completed cn/co comparison artifact contains28 frames2252..2279, zero HUD
mismatches and no declared winner; labels correctly identify public DLAA.
Public reset/temporal source MAE0.7440678/3.4292784, gradient0.8546587/2.7305470,
temporal delta2.4127223/1.7783678 and source-relative temporal residual
0.4274891/1.4893458. Midpoint image inspected: no perceptual winner inferred.
Independent SHA256 comparison matches all28 cm/co files for each of motion,
bias, confidence, draw ID, raster depth and rejection reason. This localizes
the degradation to a path that also exists without the external neural hooks;
it does not establish new external-output provenance or prove a unique cause.
Executed nearest-neighbor returned-source reprojection over27 frame pairs and
8,577,180 trusted RGB samples: current-to-previous/zero/reversed motion MAE
1.9286914/1.9310742/1.9642259. Lighting changes and interpolation confound this
metric; do not relabel it motion ground truth. Next bounded integration change
is accepted-returned-color consistency protection, with stable/change negative
controls and the same frozen moving comparison, not another broad audit.
All captures are synchronous diagnostics, not performance. No production patch
or new build/selftest is claimed in this inspection; full goal remains open.

LOG619 cn public-DLAA reset-only host/helper exit0. Host positively reports
SAFE MODE EnableHooks=0/all hooks off. Exactly30 accepted2250..2279 evaluations
all reset; all28 displayed2252..2279 captures independently match ck scene,
returned color/depth, effect/alpha identity and exact HUD/world/backbuffer/
completed-Present. Same executable SHA25654C7606E44D776F84807D0A6FE5CE92F7431F91CD4F2432FF123B2C8C799CC42;
OFF-stage config remains656051579D08B667346575164B3C3D8490F40DDD55DB74C8199B0996B56AF2C7.
co public temporal is launched against the same archive/boundary/stage. Compare
cn/co only after sequence and source checks; preserve public-DLAA labels and do
not call the isolation pair a new Feature18 presentation proof by itself.

LOG618 a701b7155: post-commit four incremental builds and three673/0 selftests
pass. Next isolate public DLAA reset/temporal on ck frozen inputs2250..2279.
Use the existing supplied hooks-disabled stage, without editing its config;
require its host SAFE MODE report and actual successful public evaluation.
The ON-stage cl/cm settings remain recorded separately. Preserve preexisting
OFF-stage logs under a uniquely named pre-cn archive after confirming that stage
has no live process. cn public reset-only precedes co public temporal; both
must match frozen source/effect/accepted sequences and final presentation before
comparing with each other or attributing external-consumer effects. These are
synchronous diagnostic lanes, not performance or a new broad transport audit.

LOG617 comparison checkpoint: final serial automation/NGX/no-NGX/feature-off
builds exit0 and all three enabled suites pass673/0 (remake-compare-final-*).
Comparer now uses explicit exceptions so optimized Python cannot remove its
identity/presentation guards. Optimized ci/cj negative exits1 before creating
output; cl/cm positive2 exits0 and retains the same mixed metrics in the verified
comparison directory. CLI help and actual positive/negative paths were run;
not claiming an unrun general comparer test suite. Supplied host logs for cl/cm
both report upscaling OFF,intensity1,tone1,white203,preset0,style0,enabled ON.
An attempted no-op LOG patch failed to locate its anchor and made no edit; this
entry records the actual checkpoint. Scoped ACCEPTED: guarded frozen-source
comparison/replay and measured mixed result, not temporal quality victory or
new external mutation proof. Preserve native/reset-only fallback and defaults.

LOG616 cm temporal locked replay host/helper exit0. New fail-closed comparer
verifies28 consecutive2252..2279 captures: exact packet/returned color/depth/
effect/alpha/native/HUD source hashes, original frame IDs, both accepted sequences
2250..2279 with reset only at2250 for temporal, and exact HUD/world/backbuffer/
completed-Present checks. All28 pre-effect neural outputs differ. Same executable
SHA25654C7606E44D776F84807D0A6FE5CE92F7431F91CD4F2432FF123B2C8C799CC42.
Before-native-effects metrics reset -> temporal: source RGB MAE12.259687 ->
14.251173; gradient MAE2.005757 ->3.587813; temporal RGB delta3.310339 ->2.047959;
source-relative temporal residual2.133504 ->2.046508; black drift8.183640 ->7.288865;
saturation-range drift-9.403790 ->-9.418341. Lower temporal change is accompanied
by worse source/edge error; do NOT declare a winner or promote this experimental
history mode. Not a public-DLAA quality win, a long sequence or external mutation
proof. Preserve GIF (quantized5x-slow preview), midpoint PNG, components, source
hashes and log hashes under fc067-temporal-comparison-cl-cm. Midpoint viewed and
moving comparison opened for user. Native effects/HUD remain after evaluation.
Comparer negative ci/cj rejects differing source input before writing any output.
Next isolate public reconstruction versus supplied-consumer response on this
same frozen sequence, including focused changed-guidance external provenance;
do not return to coverage-only tuning or conflate lower delta with image quality.

LOG615 cl reset-only locked replay host/helper exit0. Exactly30 evaluations
2250..2279 all reset;28 displayed captures2252..2279. Independently verify all
28 packet/color/depth/effect-identity/alpha-exclusion files match ck byte-for-
byte. The original/current frame guard does not require remapping. Run cm with
the same executable and frozen archive, temporal raster enabled, same2250
boundary. Require identical source/accepted sequence and completed final checks
before computing/displaying visual differences. cl is a reset reference, not
proof that temporal reconstruction wins. No external consumer settings changed.

LOG614 ck frozen archive host/helper exit0: exactly30 captures2250..2279, all
effect-identity files nonempty, independent exact HUD/world/backbuffer and
completed-Present checks pass. First evaluations2248/2249/2250 all reset.
Frameguard build1/test1 exit0,673/0 including exact nonzero temporal source,
past/future/zero rejection and legacy image-only remapping separation. cl starts
reset-only locked replay at2250 against ck; expected displayed comparison window
2252..2279 (28 frames), with2250/2251 retained for shared history warm-up. Keep
scene/effect equality and original-frame guards; no visual pass before both
lanes prove the same accepted sequence and source bytes. No external config edits.

LOG613 ci/cj host/helper all exit0 and both accept exactly2250..2281, with
30 consecutive captures2252..2281. Same staged build SHA256
9BD585142D6F1F0F69E03E889D68CF8AF0E4591B65611A47183AB758801B9CA8;
original config unchanged. All30 scene packets, native PNGs, HUD masks and alpha
exclusion files match byte-for-byte. NONE of the30 returned color/depth pairs
match. Reject this as exact-source visual evidence; do not attribute upstream
Remix differences to temporal reconstruction. Boundary build1/test1 passes671/0.
Next freeze one archive's returned color/depth and use existing scene/effect
identity checks for both lanes. Permit temporal preparation with locked input
only during bounded comparison capture plus explicit effect-identity mode;
require exact original/current source-frame equality after locked scene/effect
validation. Retain all accepted geometry/depth ownership rules. No frame remap
is permitted for temporal replay. This extends the proven replay seam, not a
replacement renderer or new consumer configuration.

LOG612 9b0479ced plus WIP: post-commit four incremental builds and three668/0
selftests pass. Begin bounded moving comparison, not another coverage sweep.
Temporal preparation currently rejects locked-image replay; do not bypass it.
Add capture-only FLYCAST_REMAKE_COMPARE_START_FRAME: no neural evaluation before
the requested returned source, so reset-only/temporal lanes can start together.
Absent request preserves ordinary execution; malformed/zero/overflow or absent
bounded moving capture rejects. Both lanes must actually accept the requested
first frame with reset, and source color/depth/scene bytes must match in compared
frames. Missing boundary, different source inputs or gaps are not silently
accepted as matched temporal evidence. Initial bound: frame2250,30 captures,
240 helper frames, otherwise existing supplied stage/consumer unchanged.

LOG611 0cd0dad43 plus WIP: ch host/helper exit0; host SHA256
015AC902C8E3672BE12FD7723D7B35FB35C11D65DE4EDA3B780C1AE02EC78E59.
58 accepted retained evaluations2173..2242 (nonconsecutive),57 history-enabled,
zero GPU-guidance rejections; all prior-reference joins exact. Three captures
2175..2177 independently match guidance receipt/frame/digest and completed
Presents, with exact HUD RGBA, world composite and backbuffer RGB. Raw trusted
counts154555/105126/110070 (50.31/34.22/35.83 percent), geometry246357/220820/
242576; all motion finite, all reactive motion zero. Maximum motion7.96/6.30/
5.54 pixels. Previous-depth rejects5528/8956/3142 instead of cg's156k..174k.
Viewed2175 mask shows coherent arena/floor coverage with fighters/edges largely
reactive; final image retains source weapon/effect content and HUD. This is
bounded functioning guidance, not proof of temporal image improvement, external
output mutation under changed guidance, faithful style, performance or full
pipeline acceptance. Stop coverage-only tuning here. Complete the serial build
checkpoint, then exact-source moving reset-only versus temporal comparison and
focused changed-guidance provenance. Preserve external config unchanged and
native reset-only fallback. All captures remain synchronous/non-performance.
Final serial automation/NGX/no-NGX/feature-off builds exit0; all three enabled
selftest suites pass668/0 (remake-footprint-final-*). Backlog consistency passes.
Checkpoint scope: ACCEPTED off-by-default source-owned guidance capture and
sampling-consistency correction; visual-quality/provenance comparison remains
open. No external binaries/configuration, user paths or private media are staged.

LOG610 previous-footprint attempts retained: build1 succeeds but tests665/3 fail
HLSL compilation with uninitialized-output diagnostics from early returns in the
unrolled neighbor loop. Accumulate identity/depth predicates before returning.
Build2/test2 then666/2 fail the neighboring-ID reason control; diagnostic build3
confirms reactive output but reason6 instead of5. Remove the redundant center-ID
early return before Jacobian validity and rely on the full3x3 identity check.
Build4/test4 exit0,668/0 with exact reason assertions unchanged on both APIs.
Shifted previous samples, neighbor-depth, neighbor-ID and excessive-shift controls
now pass. Next ch combined gameplay with both footprint checks, unchanged global
tolerances and source-owned history. No shader failure is a gameplay pass.

LOG609 0cd0dad43 plus WIP: current-footprint build1/test1 exit0,668/0.
cg host/helper exit0, three captures2165..2167 show trusted26705/20349/13477
pixels (8.69/6.62/4.39 percent), up from the sparse strict-depth captures.
Current-depth rejects fall to53715/77923/59110; previous-depth now dominates
174241/156475/159781. No quality winner is claimed. Next extend the footprint
to previous pixels using the analytic previous-position Jacobian, evaluating
derivatives before divergent branches. Require all3x3 previous draw IDs to match
and previous depth to follow that same plane; singular maps reject. Add shifted
previous-slope positives and neighbor-depth, neighbor-ID and excessive-shift
negatives before ch gameplay. Global view-depth tolerances remain unchanged.

LOG608 0cd0dad43 plus WIP: implement bounded current-depth sample footprint
on each triangle's projection-depth plane, requiring all3x3 returned neighbors
to fit their corresponding plane intervals. Radius is one render pixel of local
depth slope, not an increased global view-depth tolerance or claimed consumer
jitter. Previous-ID/depth checks remain unchanged. Add both-surface analytic
tests for a0.75px shifted sample on a sloped/deforming triangle, a neighboring
crossing, one-pixel thin-surface isolation and a6px wrong-sample control. All
must preserve reactive zero motion on wrong surfaces before live cg testing.

LOG607 0cd0dad43 plus WIP: reason-build2/test2 exit0,668/0 including exact
reason/depth GPU controls. cf host/helper exit0; staged host SHA256
6BED4B6223B099A8B8EBBCEDF280C178A1A28A4E5476C1FBF6A356E449B4778A.
Three complete reason/depth captures2164..2166: current-depth rejects
216561/268335/262912 of307200 pixels, versus prior-ID58817/13333/17056 and
prior-depth1867/472/947. Trusted1875/1359/1800 agrees with ce. Thus current-depth
consistency is the dominant rejection, not missing mesh coverage (uncovered
7030/7025/7030) or assignment alone. At smooth raster interiors,61k..63k rejected
depth differences are below1e-6; other differences are materially larger.
A robust global gradient/offset fit is NOT accepted: on two frames its median
residual is worse than zero shift. Do not hard-code a fitted camera offset.
For smooth nonflat interiors,91.0/92.8/95.5 percent of residuals fit within one
pixel of local raster depth slope, versus59.9..63.1 percent within half a pixel.
This suggests sample-footprint disagreement but is not proof of an undocumented
runtime jitter. Next use analytic sloped-surface fixtures with displaced sample
locations and crossing/thin-surface negatives to establish a bounded slope-aware
depth consistency rule; keep sharp discontinuities and incorrect surface IDs
reactive. Do not globally raise view-space tolerance or invent consumer jitter.

LOG606 0cd0dad43 plus WIP: bounded guidance capture now retains independent
per-pixel rejection categories and actual raster projection depth. Existing
four neural outputs retain their semantics; extra developer-raster MRTs are
not consumer parameters. Categories distinguish current-depth, correspondence,
previous clip/bounds/identity/depth and uncovered pixels. Automation reason-build1
and test1 exit0,668/0. Extend both-surface fixtures to assert the exact diagnostic
category and raster depth for their controlled failures, then run cf unchanged
depth tolerances. This finite diagnostic chooses the next actual integration
correction; sparse ce coverage remains rejected as useful temporal quality.

LOG605 0cd0dad43 plus WIP: corrected ce host/helper exit0; host SHA256
92DD154168F985F4F85BBCCD9387BE0F870B81BEB8613DAA94284259A18DA79D.
58 retained accepted evaluations2162..2231 (nonconsecutive),57 history-enabled,
zero GPU-guidance rejects. Independently join all prior references to preceding
accepted sources and all three captures2164..2166 to completed Presents. Exact
HUD/world/backbuffer checks pass. Recompute raw guidance counts and receipt joins:
trusted1875/1359/1800 pixels (0.61035/0.44238/0.58594 percent); visible geometry
IDs83609/31840/37258. Motion finite throughout; every reactive pixel has zero
motion. Viewed bias/ID images show sparse/stippled acceptance, mostly excluding
fighters and floor. This is NOT useful temporal-quality acceptance. Returned
depth medians about0.99 and75th percentiles about0.99849 do not by themselves
identify the cause. Keep tolerances unchanged until distinguishing raster/depth
sample disagreement from previous-ID/previous-depth rejection. Add a genuinely
foreign-device previous-view control on both GPU surfaces: automation build/test
remake-guidance-foreign-* exit0,668/0; source-mismatch capture controls also pass.
ce logs archived uniquely after terminal checks. Next retain raster depth and
rejection categories in bounded diagnostics, compare with actual returned depth
on source-aligned moving geometry, and correct the demonstrated mismatch. Do not
promote low coverage, loosen tolerances blindly or return to transport bring-up.

LOG604 0cd0dad43 plus WIP: cd host/helper exit0 but coverage acceptance FAILS.
Only one guidance capture (2171) exists, with zero trusted/moving pixels and38481
geometry pixels. Logs identify remake-raster-previous-wrong-device rejects on
subsequent evaluations; this is not valid temporal coverage evidence. The guard
added after cc compared resource GetDevice to a wrapped creation-device pointer.
Correct it to compare canonical IUnknown identities of prior/current resource
owners, retaining wrong-device rejection. Do not remove the guard or infer that
cd measured a useful temporal path. Raw failed run/logs remain retained. Next
build/test the correction and rerun ce using unchanged supplied configuration.

LOG603 0cd0dad43 plus WIP: post-commit four incremental builds and three666/0
selftests passed. Next bound adds guidance readback only inside the existing
three-frame synchronous preview capture, with exact displayed/accepted source
equality. Retain raw half-float motion, confidence, uint16 draw IDs and bias plus
visualizations and component counts. A mismatched owner must reject rather than
attribute current guidance to an older displayed frame. Run cd live temporal
combined gameplay, independently recompute counts and inspect spatial coverage.
No per-frame ordinary readback, runtime/config change or quality promotion.

LOG602 6cf1b2202 plus WIP: cc runs the connected TEMPORAL_RASTER combined OIT
experiment, host/helper exit0. Staged host SHA256
11F789EAC0496F9D8A3C53C9751EB38D3C7B1C1BDB62782AD158AAF81B01CA45.
Independently join58 retained accepted evaluations (2184..2250, nonconsecutive),
57 history-enabled evaluations and zero GPU-guidance rejects. Every previous
reference equals the preceding accepted source. Captures2186..2188 independently
pass protected RGBA, world composition, backbuffer RGB and completed-Present;
protected counts18540/18509/18562. Viewed2186 final shows source weapon arc and
impact effects retained. No comparison winner is inferred from that still.
The unchanged supplied config hash is222D059C727A683C8DEACE07C938002BDD6A69EB0DC9C908743E17A92B54EFBC.
Unique cc Flycast/ReShade logs archived after terminal process checks. All of
this is synchronous non-performance evidence; active history does not establish
trusted-pixel coverage or external output mutation under the changed guidance.
Add persistent pipeline/output object accounting and reject a wrong-device prior
ID view. Next capture the actual receipt-matched guidance surfaces and quantify
trusted/reactive coverage, then compare moving reset-only and temporal output
with focused changed-input provenance. Do not reopen general transport proof.
Final serial automation/NGX/no-NGX/feature-off builds all exit0; each enabled
selftest suite passes666/0 (remake-motion-final-* logs). Backlog consistency
inspection and diff whitespace checks pass; neither is rendering evidence.
The final object-accounting/device-guard edits are build/test covered, not a
second exact-SHA runtime capture. This checkpoint is accepted for off-by-default
GPU guidance integration only, not temporal visual quality or complete pipeline.

LOG601 6cf1b2202 plus WIP: implement isolated deferred RemakeMotionRaster with
bounded geometry/depth uploads, owned motion/confidence/draw-ID/bias outputs and
state-restoring execution. GPU fixtures run the same shader on native D3D11 and
D3D11On12: exact static zero, -4px translation (one half-float ULP tolerance),
perspective/deforming correspondence, incorrect previous/current depth, incorrect
previous ID and missing history. First run664/2 fails static numerical residue;
shader suppresses below1/4096-pixel divide noise. Second664/2 fails an exact-bit
translation check by one half-float ULP; use explicit storage tolerance, not a
sign/scale relaxation. Strengthen perspective fixture with differing current and
previous Z and independent homogeneous truth. Third run666/0 passes both surfaces.
Earlier guessed nonexistent effects-header inspection failed without edits.
Next live bound: FLYCAST_REMAKE_TEMPORAL_RASTER=1 additionally requires the existing
temporal preparation/camera scope. Dispatch GPU guidance, copy its four surfaces
to the neural ring and enable history only when retained draw IDs match the last
accepted scene. Accept new draw-ID ownership only after successful evaluation;
reset with renderer/channel. Current projected-depth tolerances are experimental
view-space0.001 absolute plus0.0001 relative; actual trusted coverage/quality are
not yet proven. No default setting or external configuration change. Run bounded
cc combined gameplay then independently inspect guidance/history/final captures.
All allocations/captures here are correctness work, not performance acceptance.

LOG600 6cf1b2202 plus WIP: cb host/helper both terminate exit0. Independently
join58 retained evaluations (2163..2233, nonconsecutive) to accepted submissions;
every previous_evaluated equals the preceding retained source. Motion preparation
has58 rows, zero rejections, 0..26 trusted draws and maximum113.517502 render
pixels. Three captures2166..2168 independently pass exact protected RGBA, world
composition, pre-OSD backbuffer RGB and completed-Present joins. This is CPU
candidate coverage, not GPU motion or temporal-quality proof. Unique cb host,
helper and archived Flycast/ReShade logs remain ignored; synchronous capture is
not performance evidence. No consumer configuration was changed.
Next integration adds shared returned-scene vertex/pixel shaders with homogeneous
previous-position interpolation and returned current/previous depth plus previous
draw-ID rejection. Added remake_motion_shader.h and production shader-compilation
coverage; automation remake-motion-shader-build1/test1 exit0,664 passed/0 failed.
Shader compilation is NOT raster correctness: shaders are not yet dispatched by
the renderer. Next wire a bounded deferred raster using these exact shaders, run
static/translation/perspective deformation and wrong-depth/wrong-ID controls,
then connect accepted-history resources. Preserve reset/zero/full-bias fallback
until that integration is proven. Inspection mistakes (Windows rg wildcard and
nonexistent guessed effects-header path) produced no edits and were corrected.

LOG599 6cf1b2202 plus WIP: BuildRemakeMotionStream projects receipt-owned geometry
with each frame's camera, reuses MatchDraws minimum-cost assignment, then requires
exact full64-bit texture/palette/RTT generations, topology, UV and vertex colors.
Ordinal remains a weak assignment hint; close best/second-best costs reject.
Alpha/cutouts, changed topology/generations/color, excessive motion and incompatible
reference gaps stay reactive with zero candidate motion. Candidate screen positions
are unclamped; assignment bounding/centroid hints use the content rectangle.
TEMPORAL_PREPARE now builds/logs these streams against previous successful
evaluation before submission, but does not upload them or enable neural history.
Automation build1/test1 passes664/0. Executed static exact zero, +4X, -3Y one-
vertex deformation, camera translation, sign/scale negatives, repeated-object
ambiguity, one-to-one reordered geometry, folded-key/full-generation collision,
palette/RTT/UV/topology/alpha/color/gap and malformed-index controls. Next cb actual
bounded replay to measure candidate coverage and preserve output, then GPU raster
and returned-depth/disocclusion. CPU streams alone are not per-pixel motion proof.

LOG598 returned-reference checkpoint: automation build2, baseline, no-NGX and
feature-off serial builds complete0; all three enabled selftests648/0. Session
18693 confirmed terminal. Backlog consistency and diff whitespace checks pass.
Scope ACCEPTED for live geometry/depth reference ownership only; motion remains
zero and neural history reset. Native/default behavior is unchanged and no
third-party configuration, media or binaries are staged. Commit this prerequisite
and proceed directly to geometry correspondence/raster, not another ownership
or generic external-provenance phase.

LOG597 temporal preparation ca completes host0/helper0,121 helper Presents.
Executed full reference-chain audit:58 retained successful evaluations, first
2162/previous0, last2232/previous2231,57 compatible previous references. Every
retained source joins an accepted evaluation; every previous pointer equals
the prior retained source, not current emulated frame. No temporal-source rejection.
Three2165..2167 captures independently pass native-HUD/world/backbuffer RGB and
completed-Present checks; viewed2166 unchanged in content/overlay placement.
Host SHA256337FBC5A5441A09D230B45C25A9592B412BF09E03363A02542255641301C16F9,
unchanged supplied config222D059C727A683C8DEACE07C938002BDD6A69EB0DC9C908743E17A92B54EFBC.
Automation build2/test2 passes648/0 including corrected actual-source mutation
test. Remaining serial build/test session18693 running. No live injected-failure
claim from unit controls; no motion, disocclusion or temporal quality acceptance.
40 common-device cleanup warning remains. Logs archived uniquely. Next wire
owned scene records through existing minimum-cost matcher and previous-position
stream, preserving exact full generation/topology checks; then GPU raster and
returned-depth consistency before enabling history.

LOG596 22c1a9ead plus WIP returned temporal preparation: add geometry-only
snapshot (128 meshes/65536 vertices/262144 indices, no DDS payload), exact
receipt/frame/producer/camera clips, and owned previous successful evaluation
depth. Explicit FLYCAST_REMAKE_TEMPORAL_PREPARE=1 requires anchored live neural
input and rejects locked replay. Attach snapshot only on publication; validate
return before evaluation; retain after successful submission and experimental
contract acceptance. No change to public neural history/reset/zero-motion/full
bias yet. Capture-independent ownership makes the next motion raster possible.
Automation build1/test1 passes648/0, including wrong receipt/frame/producer/depth,
failed/duplicate/busy acceptance, source-gap/origin reset and simultaneous geometry/
depth advancement. Review corrected an ineffective source-edit isolation test:
it previously edited a separate copy instead of the actual captured packet.
Updated test mutates captured packet geometry and texture generation; rebuild
pending. No live temporal preparation or geometry-motion quality claim yet.

LOG595 final camera-anchor matrix: automation enclosure-loop build and632/0
selftests, baseline final build and632/0, no-NGX final build and632/0, feature-off
final build all complete0. Session16314 confirmed terminal, no active build
assumed from log files alone. Backlog consistency and git diff --check pass.
Source diff reviewed: opt-in publication path only; native/default renderer and
reset/zero-motion neural fallback remain unchanged. D-185 records the scoped
anchor contract. Commit this independently verified camera integration, then
returned-scene accepted-history/motion work; do not repeat general provenance.

LOG594 bz final guarded camera run PASSES its bounded check: host0/helper0,
121 helper Presents,62 anchored publications and ZERO camera-anchor rejections.
Maximum source-view/embedded projection residual0.00244140625 pixels, below the
unchanged.01 threshold. Three captures2165..2167 independently pass native-HUD,
world composition, backbuffer RGB and completed-Present joins; nonempty capture
and zero-rejection assertions both executed. Viewed2166 retains fighters, temple,
native weapon effect and HUD. Fixed reference producer2091 retained throughout.
Host SHA256683A3FDF17D1A36B359F3702D765D8F77D6F7C0BF8E2B499CFA7EC63C1F45C9F;
consumer config unchanged222D059C727A683C8DEACE07C938002BDD6A69EB0DC9C908743E17A92B54EFBC.
Prior bw/bx/by and boundary fixture failures remain retained. Automation632/0
passes; remaining serial baseline/no-NGX/feature-off build/test session16314 is
running. This is ACCEPTED bounded live common-source camera integration, not a
recovered physical world, full moving quality matrix, new external-provenance
proof or performance result.40 common-device cleanup warning remains.
Next returned-scene temporal integration: current renderer explicitly resets
every returned evaluation, zero motion/full bias. Reviewed pinned public header
CopyRenderingOutput exposes final color/depth/normals/object picking/GUI, no
motion output; no undocumented enum or private binary inspection is authorized.
Reuse owned geometry and camera with previous successfully evaluated receipt to
construct motion/correspondence; retain reset/full bias until that path is proven.

LOG593 by run completes host0/helper0,121 helper Presents, three captures2164..2166
independently pass native-HUD/world/backbuffer/completed-Present checks.62 camera
publications retain maximum0.00244140625-pixel residual, but ONE clip-unsupported
rejection remains. The explicit zero-rejection audit assertion FAILS; retain it,
do not call this uninterrupted camera acceptance. Host SHA256
FD3EF61B5E51A1DDA327EF7CCC6B82AC24D1E8A2326901E320BFED367D73227D; config unchanged.
The bounded ray alignment could itself round back outside after initial enclosure
correction. Replace split loops with one128-step enclosure/ray loop that rechecks
both after every adjustment, preserving exact clip planes, pixel/depth guards
and fail-closed rejection on exhaustion. New error identifies unrepresentable
enclosure separately. Next build/selftest then actual guarded replay; no claim
that this source edit fixes the remaining live case before running it.

LOG592 camera-anchor-boundary-ray build/test completes0,632/0. The previously
failing near-boundary fixture now passes with original planes and projection
threshold; true-inverse and transpose-negative controls also pass. Next by live
bounded replay (same2090 start/three captures/helper121 and unchanged consumer
configuration). Require nonempty exact HUD/backbuffer/Present audit and report
every anchor rejection, not only residuals of accepted packets. Other builds
and scene/camera acceptance remain pending.

LOG591 boundary reproduction: camera-anchor-boundary-before build succeeds,
selftest631/1 FAILS the new cameraZ2/near0.1 intersection fixture. First bounded
view-depth nextafter correction also FAILS631/1; retained after/diagnostic logs
show projection error0.068359 pixels at normalized XY96.549133,-11.282026.
World-coordinate quantization jumps farther than a view-depth ULP, so scaling
ray X/Y by the requested rather than represented depth is insufficient. Next
bounded four-step alignment uses actual projected depth after world rounding;
planes, .01-pixel guard and depth tolerance remain unchanged. Ray build/test
running session97598 was diagnostic terminal1; replacement ray build has its
own session/logs. No claimed fix or runtime pass from these failed tests.

LOG590 bx corrected inverse run completes host0/helper0,121 helper Presents and
three captures2166/2167/2169. Nonempty independent native-HUD/world/backbuffer RGB
and completed-Present checks pass all3; viewed2167 retains fighters/temple/effects.
62 published camera packets, max projection residual0.00244140625 pixels.
No projection-mismatch rejections, but11 clip-unsupported rejections remain;
do not call this continuous camera acceptance. Host SHA256
B6CACCAEC97DA7B105C7F028EEFE09E723C149EF487A5BD116D446B80BBCF53F; config unchanged.
Logs archived uniquely. Source inverse/rounded-pose fix is supported by this
comparison, with remaining enclosure failure explicit. Added near-unit unchanged
basis and transpose-negative tests, then a cameraZ2/near0.1 boundary fixture:
float(2+near)-2 may fall outside unchanged near plane. Pre-fix build/test is
running session49849; require its actual result before applying correction.
Plan is representable ray-preserving intersection adjustment, not widened clip
planes or relaxed projection tolerance. No new third-party config or provenance
sweep. Serial other builds still need rerunning for this correction.

LOG589 inverse correction WIP: use true3x3 inverse for relative source basis and
actual published float camera basis, then embed vertices around the rounded
published camera position. Normals use the appropriate transpose and normalize.
Keep .01-pixel/depth guards unchanged; rejection now reports measured pixels,
expected viewport coordinates and before/after depth. Automation build3/test3
passes629/0. Retained bq source2171 packet inspection (owned wire, not third-party
binary) finds26826 vertices,25033 within viewport, min viewZ0.10000000149 and
max absolute normalized XY33.7462. Thus near-clipped/offscreen geometry makes
numerical consistency relevant; this inspection does not prove bw's exact cause.
Next bx bounded unchanged-config run using corrected executable; no success
claim until actual guarded frames and nonempty capture audit pass.

LOG588 guarded bw run: host0/helper0 and121 helper Presents, but camera guard
rejects89 frames and ZERO preview captures are saved. This is NOT a passing
camera/presentation run.62 accepted publications have maximum reported residual
0.009765625 pixels; this excludes rejected candidates and cannot characterize
the full interval. Read-only image audit printed empty[] because it lacked a
nonempty-count assertion; no pixel/Present pass is claimed. Retain bw logs and
artifacts unchanged. SHA256B3C2F11F79EAFCB40211F6E84315D8ED684C50ADB1DD8967837B77C15E771352,
same supplied config asbv. Remaining serial builds running session6104; do not
edit/rebuild concurrently. Next inspect/correct numerical embedding: transpose
was used as inverse of a near-orthonormal observed matrix, and double pose was
rounded separately from embedded positions. Use the actual published camera
basis/position to derive coordinates and distinguish finite-precision rejection
from real source-domain discontinuity. Keep .01 guard; do not claim live guarded
acceptance from clean exits or the earlier unguarded captures.

LOG587 bv actual source-anchor run completes host0/helper0,121 helper Presents,
62 anchored publications with one fixed reference producer2091. Camera moves
from0,0,0 atsource2092 to-1.78144777,0.122328535,-1.31217861 at2233; no camera
rejections. Three captures2165..2167 independently pass native HUD/full world
composition, backbuffer RGB and completed-Present joins2167..2169. Viewed2166:
fighters/temple/native impact/HUD visible. Host SHA256
3313FA38C5EEC0D82D972805F4AA9AC8FDF5EA3F92C789A658CE04354FD278F8;
unchanged supplied config222D059C727A683C8DEACE07C938002BDD6A69EB0DC9C908743E17A92B54EFBC.
This proves live supplied-anchor camera transport, not recovered game/world camera,
external output provenance or performance. Existing40-object cleanup warning
persists. First attempted test-log read while build live was missing; original
session48537 later completed0 and628/0 result read. After this run add per-vertex
source-view versus embedded-camera projection guard (.01 render pixels and
depth-relative1e-5), double-intermediate inverse accumulation, logged max residual
and explicit wrong-camera sign control. These additions await build/run and must
not be attributed to bv. Next bounded same-path capture with guard enabled,
then remaining serial builds; retain actual failure if enclosure precision rejects.

LOG586 95cc51894 plus WIP: implement off-by-default FLYCAST_REMAKE_CAMERA_ANCHOR=1
on the existing async producer. Select only transforms already validated into
exported observed vertices, require one common normalized top-three matrix,
unit input W, at least16 distinct source inputs and continuing reference-point
support. Preserve matrix/output W as source evidence; no homogeneous source
rewrite. Compute relative rigid view against the fixed first published reference,
embed positions/normals into that reference and publish camera through version4.
Anchor state commits only after successful publication; renderer/token resets
clear it. This is transport/source-anchor state, not neural evaluation history.
Missing/ambiguous/nonrigid/changed source support rejects before publication.
Automation camera-anchor-build1/test1 succeeds628/0, including stable geometry
under source translation, unchanged projection and six source/domain controls.
Other builds and actual runtime pending. Next bounded bv existing ON-stage
Soulcalibur replay at2090, three captures, helper121-frame bound; supplied configs
unchanged. Require live camera movement, original HUD/final output and source
projection checks; do not claim game camera/world or temporal guidance accepted.

LOG585 camera wire version4: serial automation/baseline/no-NGX/feature-off builds
complete exit0; enabled selftests619/0 each. Malformed schema/basis/truncated
origin reject atomically. Existing opaque version1 and cutout/alpha tests pass.
No runtime run required for unchanged identity camera path; moving live camera
still awaits producer integration. Read-only retained source-witness-a analysis
finds two8c03a9ea matrix groups per1782..1784. They differ ONLY in matrix[3,3]
(0 versus1), not independent camera domains. All selected input W bits are1;
preserve actual output W distinction. Largest group shares753 points with1783
and736 with1784; second group193 throughout. Normalized top-three affine relative
transforms have determinant0.9999999903/0.9999999516 and orthogonality residual
5.24e-8/8.73e-8. Both group deltas coincide because their top-three rows coincide,
NOT independent corroboration. This identifies a reusable common source anchor
candidate, not static-world semantics. Next implement receipt-qualified fixed
first-frame anchor and relative camera from this common observed transform,
requiring unchanged source-point support, proper rigid delta and explicit reset
on incompatibility; keep world/physical semantics unproven. Embed scene positions
and normals into that anchor and carry its camera through version4. Wrong-camera,
wrong-origin and source-domain controls must fail; then actual moving comparison.
No additional settings/provenance search before that integration.

LOG584 52945db59 alpha slice committed/pushed to fork; remote SHA equals local
52945db59ccf271fc691109e696238e8d22f1808. Post-commit incremental four builds
and three608/0 selftests pass (alpha-52945 logs); not freshly reconfigured SHA
runtime evidence. Only pre-existing metrics/cache/rtx-remix untracked remain.
Next camera integration found a concrete transport restriction: D3D9 uploader
already applies packet camera but live wire only permits identity pose/origin.
Implement version4 for explicit diagnostic-camera-embedded-anchor scope, carrying
proper camera pose and fixed sequence origin; versions1..3 remain unchanged.
Initial automation build/selftest camera-wire-build1/test1 passes616/0 including
translated/rotated geometry projection, moving camera, origin discontinuity and
invalid pose/scope controls. Added malformed input schema/basis/truncated-origin
atomic rejection checks afterward; these require the next build. No live camera,
world reconstruction or new runtime acceptance claimed. A guessed reference
filename read failed; actual orientation/coordinate helpers were inspected instead.

LOG583 alpha integration checkpoint: serial alpha-close automation, baseline,
no-NGX and feature-off builds complete exit0. Three enabled selftests each608/0;
public SDK mock contract200/0 (runtime_loaded=false, no GPU claim).
test_effect_material_inspect.py runs2 tests OK; backlog_contract_inspect passes
document consistency only; git diff --check passes. Reviewed clean bt first
composited capture: relit world/railings, native impact and protected HUD visible;
this still image is not moving quality acceptance. Earlier LOG576-582 retain
moving source and GPU/negative controls, timeout and gaps. Premature read of
off/SDK logs while serial build was live found missing files; re-polled original
session63844 to terminal0 and read completed logs. Windows wildcard rg searches
failed and were replaced by rg --files filtering; no rendering test inferred.
D-183 records single-owner alpha material policy. Scope ACCEPTED for bounded
opt-in alpha integration; full camera/temporal/performance acceptance remains open.

LOG582 ee5984002 plus alpha WIP: resumed both bu handles; both were missing,
and authoritative logs show helper outcome0/121 Presents and host clean_close=yes,
with no remaining Flycast/helper process. Archived bu logs without overwriting.
Executed remake_alpha_provenance_audit.py --archive bq --marked bs --clean bt
--off bu against the retained evidence: PASS28 matched presented originals,
2173..2181 and2185..2203; all28 external outputs differ from disabled control.
Exact four guidance inputs, marked/clean pre-marker output, native effect and
alpha-exclusion bytes, completed marker Presents, pre-effect PNG hash, native
HUD composition and final backbuffer RGB checks pass. Active consumer tuple is
upscaling OFF/intensity1/global tone1/diffuse white203/preset0/style0/enabled ON.
This closes the changed alpha-path provenance check, not camera, quality,
cadence or full-pipeline acceptance. Intentional native weapon trails remain
content to preserve, not defects inferred from their mere presence. Prior br
timeout and bq sequence gap remain retained. No external config modified;
40 common-device cleanup warning remains. Next finish serial build/selftest
matrix and commit this integration slice, then scene/camera implementation;
do not repeat the just-completed provenance matrix without a changed dependency.

LOG581 alpha matrix continuation: clean restored bt completes host0/helper0,
28 captures on exactly the same archived originals as bs (2173..2181,
2185..2203). Same executable4A830734BB65B8FDAAD7A12F03B4C2EF60D9DDCF16203E45EBA5B11B564C3FF2.
bs/bt logs archived uniquely. Disabled bu uses existing EnableHooks0 workspace,
config656051579D08B667346575164B3C3D8490F40DDD55DB74C8199B0996B56AF2C7,
same executable/bq archive/2090 start/restored mode/longer helper bound. It is
running at this checkpoint. No supplied configuration was edited. Full audit
pending bu completion; matched counts alone are not external-output proof.

LOG580 ee5984002 plus WIP alpha external-output matrix. Marked br replay against
bq matches source scene/native stack and alpha exclusions but helper's30-second
post-start watchdog expires after33 returned sources/19 saved captures; host0,
helper wrapper1 with explicit timeout. Retain br artifacts and do not accept the
failed run as a completed matrix. Retry bs uses existing helper121-frame mode
(120-second post-start bound), unchanged host executable
4A830734BB65B8FDAAD7A12F03B4C2EF60D9DDCF16203E45EBA5B11B564C3FF2/config hash,
same bq archive and2090 start, marker mode; running at this checkpoint. br logs
archived uniquely. Add parameterized remake_alpha_provenance_audit.py based on
the actually used prior post-effects audit, with mandatory exact exclusion-byte
matching against archive; final audit not yet run because clean/OFF lanes remain.
bs completes host0/helper0,121 helper Presents and28 saved marked captures,
originals2173..2181 and2185..2203. Sentinel log reports1024/1024 pixels; do not
infer the full external-output result from that alone or capture booleans.
Next clean restored bt and hook-disabled bu, same bq archive/host hash with
the longer helper bound, then run the parameterized full audit.40 common-device
cleanup warning persists. Both br timeout and successful bs remain retained.

LOG579 ee5984002 plus WIP: preview captures serialize native-alpha-exclusions.bin
with exact source ordinal and expected packed parameter words for each selected
draw. Locked replay checks this alongside existing exact scene/input/native-stack
identity; missing, changed, or unexpected selection evidence rejects before
evaluation. Empty legacy captures remain compatible only without an exclusion
artifact. Tests execute exact roundtrip plus missing/changed negatives;
alpha-provenance-build2/selftest2 passes608/0. First build also passed before
explicit complete-type include and extra tests; no failure hidden.
Start bounded bq30-frame archive, full native-effect identity and alpha-combined
enabled, executable SHA2564A830734BB65B8FDAAD7A12F03B4C2EF60D9DDCF16203E45EBA5B11B564C3FF2,
unchanged config222D059C727A683C8DEACE07C938002BDD6A69EB0DC9C908743E17A92B54EFBC.
bp logs archived uniquely. This synchronous archive is not performance evidence;
marked/clean/OFF comparisons and other configuration builds remain pending.
bq completes host0/helper0 with30 valid capture directories. Executed sidecar
audit verifies every full native-effect identity and exclusion header/count;
selections contain6 or7 source draws. The attempted consecutive30 assertion
FAILS: sources2171..2181 then2185..2203 (gap2182..2184). Preserve that failure;
the archive is not30 consecutive frames. The longest retained run is19 frames
2185..2203. This does not block exact matched provenance; replay only positively
matching archived inputs, retain gaps, and do not claim300-frame quality cadence.
Next marked/clean/OFF replay using bq and selection-aware matching; no need to
repeat archive merely to improve the count.

LOG578 ee5984002 plus WIP: source effect snapshots now retain the exact native
packed polygon table generated from source PVR state. Compose validates selected
alpha ordinals and expected words against that owned table, clones GPU parameters,
and applies ZERO/ONE only to the clone. Original stack/parameters remain intact.
Executed transparency-contract nativeD3D11 and D3D11On12 verifies excluded replay
leaves background unchanged, original replay still changes it, and a second
exclusion repeats correctly (alpha-override-* logs). No new readback/wait in
ordinary Compose. Receipt-local alpha selections follow accepted overlay/source
ownership. Explicit FLYCAST_REMAKE_ALPHA_COMBINED=1 requires native-effects and
neural evaluation; incompatible raw-preview combination rejects. Selection uses
only final exported alpha packet meshes and exact live snapshot binding checks,
not a hardcoded material ordinal. Automation combined build/selftest605/0 pass.
Live bp launched with3 diagnostic captures, executable SHA256
FF6315CA0B6C7308CEEBC5250E2D0B02F5711BDB2CEF05F5FB4F129B325A3EEF,
unchanged config222D059C727A683C8DEACE07C938002BDD6A69EB0DC9C908743E17A92B54EFBC.
bo stage logs archived uniquely. Live result, focused external-output evidence,
other configuration builds and moving material-quality acceptance pending.
bp completes host0/helper0. Source-owned exclusions vary with exported scene
(observed8 at2216); evaluated returned scenes compose remaining native effects.
Independent Pillow/numpy/log audit actually passes3 captures2174..2176: original
HUD RGBA under mask, final/backbuffer RGB equality, evaluated+effects flags and
completed-Present joins2176..2178. Viewed2176: relit railings plus native blue
weapon arc/orange impact and protected HUD; former dark native railing layer no
longer overlays the promoted material in this image. No perceptual winner or
external mutation proof inferred. Runtime40-object cleanup warning persists.
Next serialize/verify receipt selection provenance for focused marked/clean/OFF
comparison on this changed combined path, run remaining build matrix and moving
material checks, then commit independently proven slice. This is no longer only
a raw alpha preview, but full working-pipeline acceptance remains open.

LOG577 ee5984002 plus WIP: extend executed native D3D9 GPU fixture with8
ordinary-alpha source-selection cases and8 disabled-blend controls, plus correct
disabled-depth-write and deliberately enabled-depth-write cases. Black-background
RGB agrees with independent alpha product within1UNORM step; wrong blend and
depth-write controls measurably fail. alpha-gpu-build1/selftest1 passes598/0.
Add source-qualified PlanAlphaEffectExclusion: validates full producer identity,
exact owned packed polygon words, unique bounded ordinals, ordinary(4,5) direct
accumulation and no secondary-volume state before producing ZERO/ONE replay
words. Additive, wrong source/state, duplicates, out-of-range and secondary state
reject without changing output. alpha-ownership-build1/selftest1 passes605/0.
This planner is not yet wired to GPU parameters or combined rendering. Next
retain native packed polygon words with the source effect snapshot and create
a receipt-local parameter override for only exported alpha meshes; do not mutate
the original native parameter buffer. Keep combined guard until GPU/source
ownership controls pass. Other configurations and real combined run pending.

LOG576 ee5984002 plus WIP alpha-material prototype. Optional sourceAlphaBlend
distinguishes ordinary source-alpha materials from opaque/cutout; version3 wire
carries it, old versions default false. Owned source export admits only list2
ordinary(4,5) direct accumulation, supported modulation/filter/fog/non-bump paths,
excluding classified HUD, additive particles, volumes/secondary textures and
unsupported source ranges. Draw/texture identity remains source-qualified, not
hardcoded census ordinal. D3D9 uses explicit source-alpha/inverse-source-alpha,
selected vertex/texture alpha and disabled depth write; public SDK adapter still
rejects this unimplemented material path. Exact FLYCAST_REMAKE_ALPHA_PREVIEW=1
is separate raw-return preview only: fail closed if native effect replay or
combined neural presentation is requested, preventing dual ownership. Full
world material/ordering/alpha quality is not yet accepted.
Automation alpha-preview-build1 and598/0 selftests pass, including explicit
source material, version3 round trip and additive exclusion. Dedicated GPU alpha
goldens remain pending; do not call existing cutout tests alpha-blend truth.
Live bo launched for3 captures with host SHA256
738B5B3936FA610F5143A8BDD94B717DDC9AE7E557E3700124078FA759EBC4F1,
unchanged config222D059C727A683C8DEACE07C938002BDD6A69EB0DC9C908743E17A92B54EFBC,
async neural0/native effects0/alpha preview1. Raw material experiment, not
combined DLSS5 evidence. bn stage logs archived uniquely; live result pending.
bo completes host0/helper0. Live logs report6 alpha material meshes, raw returned
captures2177..2179. Viewed2178 composite: railings now participate in relit scene;
weapon arcs/particles absent because native effects deliberately off. Capture
metadata reports evaluated=false/effects=false and0 HUD/world/backbuffer
mismatches; independent pixel/Present audit still pending for this prototype.
No alpha fidelity/ordering or external-consumer acceptance inferred from the
image. Next dedicated GPU blend/alpha-source/depth-write controls, then a
source-qualified native-stack ownership split for exactly the exported meshes,
preserving additive effects and HUD; no hardcoded ordinal exclusion. Other
build configurations remain pending; this production/harness slice is WIP.

LOG575 ee5984002 read-only retained effect census, without another game run.
Add strict effect_material_inspect.py for existing canonical identities and
test_effect_material_inspect.py; executed2 tests include independent pixel/
blend/ordinal truth and5 malformed-input controls. Analyze all28 retained bf
identities:1,850,413 source-alpha/additive fragments (4,1),1,486,838 ordinary
source-alpha/inverse-source-alpha fragments (4,5); no ONE/ZERO shortcut in this
retained visible population. Source2175 draw257 spansx0..592,y98..204 with18,190
fragments:7,693 alpha0,8,849 alpha255,1,648 partial alpha. This is a broad world
candidate, not safely opaque. Ordinal257 appears in only5/28 frames; never use
that ordinal as durable material identity. Earlier3-frame range partial counts
1648/1673/1716, full retained ordinal range1557..1716. The census reports raw
primary/secondary state and surviving coverage, not complete draw/material truth
or automatic promotion approval. Installed public remix_c.h exposes
MaterialInfoOpaqueEXT.useDrawCallAlphaState and InstanceInfoBlendEXT explicit
alpha blend fields; do not equate refractive MaterialInfoTranslucentEXT with
PVR source-alpha blending. Next connect source-qualified translucent geometry/
texture identity and explicit blend state in a bounded separate material lane,
with native-stack ownership exclusion only after correspondence is proven.

LOG574 finish native-reference ownership checks. Reject foreign device/context,
missing owner, wrong source and repeated capture; copied pixels remain owned
after mutating the source background. Optional copy adds exactly1 object and
640*480*4 logical bytes. Existing repeated GPU replay goldens now run against
the saved background after source mutation. Actual transparency-contract runs
pass native D3D11 and D3D11On12 (effect-reference-owned-d11/on12.log); automation
selftests595/0 pass. Full serial configuration matrix launched separately;
completion now verified: all4 incremental builds and all3 enabled selftests595/0
pass (effect-reference-final/baseline/no-ngx/off logs). Preserve LOG573's exact3
native-parity frames, no rerun.
Next integration question is which currently native-only translucent-list draws
are world surfaces versus effects/HUD. Source list membership alone does not
prove a blend needs translucency: inspect actual source/destination factors,
accumulation selection, depth-write state, material/texture and overlay identity.
Do not convert arbitrary translucent geometry to opaque or double-render a
promoted surface in both Remix and the retained native stack. Use the existing
source draw/effect identity to make any future ownership split explicit.

LOG573 da9b0bc1c plus WIP: exact developer flag
FLYCAST_REMAKE_EFFECT_NATIVE_REFERENCE=1 retains the native opaque resolver
input at effect capture. The bounded preview replays the same owned effect
stack over this input and emits native-opaque-resolver-input.png plus
native-effects-replayed-native.png. Optional resource count/bytes include this
copy; flag remains off normally, diagnostic captures are not performance.
First build fails immutable shared-owner mutation; retain before publishing
const ownership. Second fails const ComPtr.get; use its const pointer conversion.
Third automation build and595/0 selftests pass. These existing tests do not
prove the new live native-parity comparison. Live bn launched with executable
F89E12D26928D9A6FB21A63EB68EF2FFDFDA94FD26E53C5E6C4293EEF879E1EC,
unchanged config222D059C727A683C8DEACE07C938002BDD6A69EB0DC9C908743E17A92B54EFBC;
three diagnostic captures requested, result pending. bm logs archived uniquely.
bn finishes host0/helper0. Independent Pillow/numpy comparison actually runs on
all3 native-reference outputs2171..2173: RGB mismatch0, maximum delta0, MAE0
against original-native.png for each. Thus the retained effect resolver exactly
reproduces native output over its actual opaque background in this scope. Do
not fix the conspicuous relit shapes by discarding valid native effects or by
inventing a depth/history reset. Their changed appearance belongs to applying
source effects over different lighting/color, not a demonstrated replay error.
This closes the immediate native-parity diagnosis; material-aware treatment of
translucent world content remains separate from exact preservation of weapon
trails/HUD. New optional native-reference capture remains WIP pending its
focused ownership tests and other build configurations before commit.

LOG572 correct the LOG571 attribution before implementation. Viewed retained
bl2178 returned-remix.png and neural-before-native-effects.png: neither contains
the fence-like dark structures visible in composited-remix.png. Therefore these
structures are introduced at native-effect replay, not demonstrated stale Remix
geometry. The previous static/short-replay comparison omitted this stage and
could not establish temporal causality. Do not implement speculative runtime
history resets on that evidence. A startup-late bm test had already launched;
both processes exit0 but source gaps recur2176->2184->2245, so it is not a
no-gap control. Retain all outputs/logs; source2248..2250 captures exist.
Source inspection: native OIT rejects hidden fragments against depthTexture at
fragment creation; retained resolver subsequently only sorts/blends the surviving
stack. Compose uses no new depth test. This does not yet prove incorrect depth
or blend behavior: native destination-dependent shading may simply be exposed
against relit color. Next retain the actual native opaque resolver input and
compare the same stack over native versus neural backgrounds, with exact source
identity, before changing occlusion or suppressing source effects. Preserve
intentional trails and reject guessed geometry exclusion. No code/config change.

LOG571 da9b0bc1c follow-up world artifact isolation. Post-commit all4 incremental
builds and all3 selftests595/0 completed (da9b0bc1c-post-* logs); fork SHA matched.
Run retained bl source2178 for60 identical frames through actual legacy Remix
final-memory and raster-memory captures, both exit0. Viewed settled final image:
the live fence-like dark shapes are absent. Then run exact bl2176/2177/2178 in
one63-frame sequence (60 warmup,3 endpoints); exit0 and viewed2178 also lacks
those shapes. All files are external fc067-world-static-remix/raster.bmp and
fc067-world-sequence.bmp.frame-*.bmp; logs world-static-*/world-sequence.
This narrows the cause but does not prove a specific runtime defect. Live bl
records source gaps2103->2111->2173 and2174->2176, recreating uploader resources
without proven runtime temporal reset. Short replay has a2177 resource refresh
yet lacks the artifacts, so refresh alone is insufficient explanation. Next
controlled source-gap/scene-retirement experiment and reviewed public runtime
object-history interfaces; do not deform source geometry to hide a temporal
scene artifact. Reviewed installed public remix_c.h camera declarations expose
no named history-reset entry; do not invent one. No proprietary binary/config
inspection/change, no performance or camera acceptance claim.

LOG570 implement live protectedOverlay metadata on captured draws. The async
world exporter marks it using the same concatenated OP/PT/TR ordinal and
IsOverlayOrdinal classification as native overlay-mask replay, only with
automatic native-overlay policy0. BuildRemakeViewScene excludes marked draws
before texture/mesh transport; old archives default unmarked. No new geometric
HUD heuristic or game-specific screen rectangle. Tests cover protected cutout
exclusion and unchanged unprotected cutout inclusion. Live validation pending;
do not assume both observed HUD cutouts are classified until the run reports it.
Automation build/selftests pass595/0. Live bl uses executable SHA256
0B57DBFCA4FF4AFD81D0D68B8AFCCE0BFCAE0A4AD8A8FB079A1129A1402B4D45,
unchanged supplied configuration222D059C727A683C8DEACE07C938002BDD6A69EB0DC9C908743E17A92B54EFBC,
and helper omission control off. Logs report8 protected draws excluded and0
remaining cutout meshes. Both processes exit0. Independent image/log audit
passes3 captures2176..2178: protected native HUD RGBA, final/backbuffer RGB,
completed evaluated Presents2178..2180; masks cover19985/18521/18548 pixels.
Viewed final2178: HUD and intentional effects remain; dark geometry still exists.
This accepts the bounded HUD/world separation, not recovered camera/world
cutout quality, changed external-output provenance, or performance. Remaining
serial build matrix is running; commit only after actual completion. A doc patch
with an unmatched backlog context was rejected and reapplied with correct scope.
Serial matrix now completed: all4 incremental builds, all3 enabled selftests
595/0, mock SDK200/0. Logs cutout-overlay-build1/baseline/no-ngx/off/sdk retain
the actual runs. The checkpoint supports opt-in palette/alpha transport and
native HUD/world separation, not genuine world-cutout or full pipeline acceptance.

LOG569 focused cutout attribution: add harness-only exact environment control
FLYCAST_REMAKE_TEST_OMIT_CUTOUTS=1, which skips cutout draw submission but retains
the received packet, textures and camera unchanged. Runtime log explicitly
labels this negative control. No Flycast production toggle or external config
change. Automation cutout-control-build1 succeeds. Live bk uses the same host
executable hash as bj, ten bounded captures, and omission enabled only in the
helper process. Exact packet matching against bj is required before attributing
image differences; run result pending. bj stage logs archived uniquely.
bk finishes host0/helper0, negative-control log confirms omission. All3 shared
bj/bk scene packet files are byte-identical. Returned RGB MAE4.766927/4.437184/
4.451638 affects299358..300642 pixels; temporal variation prevents treating this
as a cutout coverage mask. Dark shapes remain visibly present with omission.
Two further isolated60-frame runtime raster-memory runs read the exact same
source2180 packet, cutouts on/off, both exit0 (cutout-static-on/off.log). Viewed
both BMPs; independent RGB difference is3426 pixels boundedx28..357,y22..75,
zero below y100. These meshes are HUD stage/time and round timer, not missing
world geometry. This falsifies the assumed world-completeness benefit of this
specific live cutout case. Reuse protected-overlay classification to exclude
them from the ray-traced scene; do not discard the tested palette/alpha support
or falsely accept world-cutout coverage. Existing dark geometry requires separate
scene/camera work. Static runtime cleanup warning37 persists. No external config
changes or new performance/provenance claim. Control build succeeded; full
post-control build/selftest matrix remains pending before committing this WIP.

LOG568 base4e43b3565 plus WIP corrects GPU-paletted draw binding: allow only
PalSelect differences for shared index resources, derive bank from draw TCW,
snapshot/revalidate its authoritative native palette hash rather than cached
texture hash. CPU-expanded paths remain exact. First build2 links Flycast but
fails harness linkage because native pal_hash arrays are absent; add explicit
harness-owned arrays, retaining that failure. build3/selftest3 passes593/0,
including shared-bank positive and address/format/CPU-expanded negative checks.
Live bj starts with executable SHA256
0803D989F87F640BDA79A369520E2CED3758D33A4D2C20FD028CB090499431FC;
configuration hash unchanged222D059C727A683C8DEACE07C938002BDD6A69EB0DC9C908743E17A92B54EFBC.
bi stage logs are archived under their unique bi names; runtime result pending.
Live bj completes host0/helper0 with two cutout meshes and alpha_reference255
successfully published, rendered and returned. Three captures2179..2181 join
completed evaluated Presents2181..2183. Independent Pillow/numpy checks actually
run: exact composite/native HUD pixels under original mask, composite/backbuffer
RGB equality, and completed-Present log join pass all3. Protected pixels are
18598/18516/18499. This synchronous test is not performance evidence; external_nr
remains explicitly unproven for this capture. The existing40 common-device-object
cleanup warning persists. Visual inspection of source2180 native and composed
images confirms weapon arcs/impact effects and HUD, but conspicuous dark geometry
prevents declaring cutout fidelity accepted. Next compare exact-source cutout
on/off output to separate new mesh behavior from existing camera/shadow geometry.
Remaining serial build matrix finishes successfully: all4 incremental builds,
all3 enabled selftests593/0, mock SDK200/0; logs palette-cutout-baseline,
palette-cutout-no-ngx, palette-cutout-off and palette-cutout-sdk. Backlog inspector
and diff whitespace check also pass. WIP remains uncommitted pending the focused
cutout visual comparison; no third-party artifacts/configuration staged.

LOG567 base4e43b3565 plus uncommitted cutout/palette work: extend the existing
asynchronous cache to own paired index/palette copies, retain independently
completed readbacks, and qualify reuse by resource, bank and upload/RTT/palette
generation. DDS expansion uses the existing BGRA-palette decoder; A8 alone
is rejected, never interpreted as alpha. The live renderer passes its actual
palette texture. Bump-map cutouts remain omitted because their alpha semantics
are not the implemented texture-alpha contract. Automation incremental build
and589/0 selftests pass (palette-cutout-build1/selftest1.log). The executed WARP
material-contract fixture passes bank256, independent RGBA/transparent-pixel
truth, changed-palette generation, bank separation and missing-palette controls
(palette-cutout-fixture1.log; external fc067-palette-cutout-fixture1 artifacts).
Live bi is launched with three bounded captures, executable SHA256
867E4E95D6DCD8156C8B53DE7C96B8C4C13D5BA0B49DEE07ED8DE8F2F6003B49;
supplied configuration remains222D059C727A683C8DEACE07C938002BDD6A69EB0DC9C908743E17A92B54EFBC.
Runtime acceptance and remaining build configurations are pending; no new
transport/provenance or performance acceptance is claimed.
Live bi finishes host0/helper1 (bounded first-source timeout), without captures
or scene delivery. The previous palette-DDS failure is replaced by
view-texture-binding. Source inspection identifies GPU-paletted cache keys
excluding PalSelect (TexCache.h getTextureCacheData), while the exporter requires
full cached-TCW equality and snapshots cached palette_hash. Those are not
draw-specific identities for shared index resources. Next correct this binding
and generation qualification using actual draw bank and authoritative palette
state; do not weaken checks blindly. bi failures remain in consumer/publisher
logs and stage logs pending unique archival. This is a new actionable integration
finding, not a runtime or external dependency blocker.

LOG566 base4e43b3565 plus worktree implements opt-in source/PT geometry, texture
and alpha-state hookup plus a native D3D9 GPU fixture. Fixed-function alpha first
fails vertex128/texture1/reference1; a half-step ADDSIGNED attempt fails
vertex128/texture254/reference128. Both attempts are retained, not accepted.
Pivot to a Flycast-owned ps_3_0 shader with explicit8bit rounding/discard and
alpha1 after acceptance. First compile fails because strict mode rejects legacy
sampler syntax; diagnostic rerun identifies that error, corrected by the proper
legacy compile mode. No proprietary shader/binary was inspected or modified.
GPU fixture now tests40 correct alpha cases and40 wrong-rounding cases, a green
surface visible only through rejected-depth holes, and an executed wrong-opaque
control. Source tests verify off-default export, missing-reference rejection,
threshold0 and disjoint opaque/PT IDs. Current automation selftests589/0 pass.
Live runtime and other build configurations remain pending. One patch containing
duplicate operations on a file was rejected before any edit, then corrected.
Final hookup automation suite589/0 passes. Bh executable
FAA246D308A28CE24F7805CD8E91D5AC8A53E14C699B4F1EF384EA4AEAAC0A11
runs with cutout opt-in and unchanged supplied ON settings. It logs two exported
cutout meshes/reference255, then alternates material-cache-pending and
view-dds-mips-or-palette-unsupported. Host240 exits0; helper100 receives no scene
and expires its90-second first-source wait with exit1. No evaluated captures,
cutout runtime or presentation pass. Existing EncodeRemakeMaterialDds explicitly
rejects A8 index textures. Next bind their actual palette resources/generation/
bank through the current asynchronous cache; existing DecodeMaterialTexel already
knows the palette lookup semantics. Do not weaken the rejection. Guard-only null
device corrections were added after bh staging and require final builds.
Final WIP validation completes all four incremental builds, three589/0 suites,
and mock SDK200/0. No live cutout result is accepted; source/shader/texture
hookup remains uncommitted while palette support is completed. Follow-up must
also exclude bump-map alpha from the plain cutout contract: the PVR bump path
derives alpha differently, so texture-alpha interpretation is not sufficient.

LOG565 based5fe260a6 selects a concrete scene omission: BuildRemakeViewScene
accepts only opaque list0; punch-through list1 is absent, and the D3D9 uploader
does not configure alpha testing. First implement explicit optional uint8 source
alpha threshold in Mesh and version2 view wire only when present. Opaque packets
continue byte-layout version1; null is opaque and threshold0 remains enabled.
Tests round-trip0/128/255, preserve old opaque input, and reject unsupported
cutouts in both current consumers instead of rendering opaque silhouettes.
Automation selftests582/0 and public-header mock SDK contract197/0 pass.
This is transport/schema groundwork for missing geometry, not rendered cutout
support or camera recovery. No cutout scene export is enabled yet. Next bind the
actual source threshold, implement/test GPU alpha semantics, then opt-in moving
coverage. Source PVR shader rounds alpha to8bit then keeps alpha>=reference;
UseAlpha/IgnoreTexAlpha and shading instruction affect the tested alpha and must
not be discarded. Initial file lookups for remake_view_scene.cpp and wildcard
remake paths failed; actual implementation is remake_view_scene.h and directory
search with filename filters. No proprietary files changed.
All four incremental builds and all three enabled582/0 suites pass; backlog
consistency inspection passes. Public-header mock197/0 is not a GPU result.

LOG564 ada37d1fc post-effect provenance matrix uses one incremental executable
CFAA18F9EC94D13D4BA0E9A97A216C0344151DC2925AFB48693D3FBDD8070CD5.
Its embedded build label remains3e78a6f4f; do not call this a fresh exact-SHA
build. Bd archives30 sources2174..2203 with identity evidence; helper100/host240
both exit0. Be marked replay uses bd, helper121 (bounded120-second source window),
host240 and publication start2090. All30 source identities match and28 captures
complete after startup; both processes exit0. Current/source counters differ
from archive IDs and must be joined through recorded replay_original_frame,
never assumed equal. Bf clean/restored and bg existing hook-disabled controls
are pending. All are synchronous diagnostic runs, not performance measurements.
No external settings changed. Local read-only audit effects-proof-be-bf-bg checks
the four neural input hashes, full effect identity, marker/clean returned hashes,
pre-effect PNG identity, post-effect/HUD/backbuffer/completed-Present joins and
consumer-reported tuple; it must actually run before claiming the matrix passes.
Bf completes28 captures, bg completes30 effect/input checks with native fallback
and no experimental evaluated captures; all four matrix host/helper pairs exit0.
The local audit actually runs and passes28 consecutive originals2176..2203:
all four neural input hashes equal across marked/clean/OFF; full source effects
equal the archive; pre-marker returned hashes equal marked/clean and differ from
OFF for every accepted frame; clean pre-effect PNG equals returned hash; native
effect difference, original HUD composition, backbuffer RGB and completed Present
joins are exact. Every marked counterpart has1024/1024 sentinel pixels. Both ON
logs positively report the unchanged tuple1/1/203/preset0/style0/upscalingOFF;
OFF remains existing EnableHooks0 with configuration hash656051579D08B667346575164B3C3D8490F40DDD55DB74C8199B0996B56AF2C7.
Selected moving-combat source2192 was visually inspected with preserved blue
weapon arc, impact particles and HUD; no visual-quality winner is inferred from
this still or from provenance. ACCEPTED: bounded post-effect external-output
regression. Camera/world/scene completeness, final performance, helper cleanup
warning and full working-pipeline checklist remain open. An initial backlog
patch missed its context and made no changes; corrected using current text.
Checkpointc67e0763e post-commit four incremental builds and three574/0 suites
pass. The queue inspector rejected marking the umbrella M4-DLSS5 row doing
while its presentation dependency remains unfinished; the shell continued to
commit despite that failed native-command status. Correct the umbrella to todo
without withdrawing the scoped LOG564 evidence, then rerun the inspector before
push. This is a queue-status correction, not a rendering/provenance failure.

LOG563 ba executableF196D5DEAE5299C39F3F3E994F692FD400D523FFBDF70FC6F3720CEFB2194B52
repeats the ay locked-input comparison with word diagnostics; both processes
exit0. All2175..2177 reject at canonical word28: retained211709912 versus
current249159016. Header10 + resolver-prefix2 + constant offset16 maps exactly
to ditherDivisor.x. setupPixelShaderConstants declares an uninitialized local
and only assigns this field when dithering is enabled; it also copied92 bytes
into a96-byte GPU allocation. Correct by value-initializing all fields and adding
explicit tail padding with sizeof96 assertion. Full identity equality remains;
old archives are retained, not rewritten/normalized. New archive/replay and native
pixel comparison are required before accepting this correction in gameplay.
Bb uses executableCFAA18F9EC94D13D4BA0E9A97A216C0344151DC2925AFB48693D3FBDD8070CD5;
automation build and574/0 selftests pass. Both live processes exit0 and capture
three sources2171..2173. Independent native PNG comparison against retained aj
is byte-exact for all three; identity words28..31 and35 (unused dithering fields
and tail) are now zero. Bc uses the identical executable, bb archive and source
publication start2090 to cover the archived interval; matched result pending.
Bc terminates both processes0. All three sources2171..2173 match full effect
identity exactly; source2173 completes evaluated output and successful Present
at current2175 (then held within the existing age bound). Its archived identity
is byte-identical to bb; independent native-mask/evaluated composition and
backbuffer RGB checks are exact, with19,809 protected pixels and zero mismatches.
This accepts bounded effect replay linkage, not external NR provenance. The
supplied configuration hash is unchanged. A launch-label substitution mistake
left bc consumer/publisher output using bb filenames, replacing those two bb
text logs; they are now correctly renamed bc. Bb host evidence, stage logs,
images/identities and actual terminal exit observations remain; do not claim
the overwritten standalone logs were preserved. Ba/az falsifying logs remain.
All four initialized-code incremental builds and all three enabled574/0 suites
pass; backlog consistency and diff whitespace checks pass. No third-party
configuration or binaries are staged. The next work is the focused post-effect
external-output matrix, not another proof of snapshot allocation or transport.

LOG562 base77f8dc8ed plus worktree wires optional effect identity capture and
matched replay. Exact FLYCAST_REMAKE_EFFECT_IDENTITY=1 permits at most30 capture/
replay identity attempts per renderer. Replay uses the directory selected by the
existing full scene/input verifier and rejects missing/changed/truncated/trailing
identity bytes before neural submission. No opt-in still rejects locked effects.
Stream round-trip/corruption/truncation/trailing controls and automation574/0 pass.
Live ay uses executable SHA2566DD8214920B6257B36AB7C7DE8460E892A5435AC2712E0BE27B56C7EB0CB2A4E,
unchanged supplied ON configuration, helper100/host240 after2100 warmup. Both
exit0; three sources2175..2177 capture successfully, with exact identity file
sizes2,482,252/2,709,932/2,845,152 bytes. Independent read-only image checks verify
original-mask native/evaluated composition, exact backbuffer RGB and pre/post
effect difference for all three. No external-output proof is claimed. Existing
helper40-object cleanup warning persists. The matched az replay uses the same
executable and ay archives; its result must be recorded after terminal observation.
Az rejects all three exact scene-matched sources2175..2177 with effect-replay-content;
therefore no effect replay or external proof passes. Helper exits0; the rejection
is retained, not normalized away. Added mismatch word/value diagnostics to locate
whether the difference is resolver state, fragment data or ordering before any
correction. The recorded canonical equality requirement remains unchanged.
Both az processes terminated0; no replay captures were accepted. Final four
incremental builds and three574/0 selftests pass after mismatch diagnostics.
ON configuration hash remains222D059C727A683C8DEACE07C938002BDD6A69EB0DC9C908743E17A92B54EFBC.
Read-only inspection of ay2175 finds apparently uninitialized words in the shared
ditherDivisor region. The selected nondithered final resolver reads shadowScale
but not that region. This is a candidate explanation, not yet a measured az
mismatch location. Next use the new word diagnostics before changing semantic
state selection. The linkage remains uncommitted until its live mismatch is
resolved; all ay/az logs and failed evidence are retained.

LOG561 basebf2375f4e plus worktree connects the canonical description to an
explicit owned-snapshot GPU readback API. Records compiled layer count and the
currently selected nondithered effect resolver variant at capture, preserving
current rendering behavior. Validates source/context/device, reads retained
pixels/polygon/constants/pointer content, and canonicalizes with exact state.
The API stages the bounded native allocation synchronously; it is not invoked
by ordinary Capture/Compose, is not a performance path and is not yet connected
to evidence files or locked replay. Initial automation build and574/0 selftest
pass, including both native/On12 wrong-source and original-pointer mutation
controls. Final controls additionally compare all extracted words against the
uploaded CPU fixture. Final validation logs use effect-readback-final prefixes.
All four final incremental builds and three574/0 suites passed; the backlog
consistency inspector also passed (document consistency only).
No live-game or post-effect external proof is claimed by these fixtures.

LOG560 base947aeafa2 plus worktree implements the bounded CPU native-effect
canonical identity component. It preserves source epoch/ordinal/cycle, resolver
state, per-pixel stable native depth/poly ordering, exact color/depth/sequence and
referenced polygon words. GPU addresses and unreachable capacity are excluded.
Tests verify relocated equivalence, six independent semantic mutations, equal-key
order sensitivity and exact rejection reasons for cycles, pointer/polygon range,
nonfinite depth and truncation; failed calls leave no output. These CPU controls
run inside the existing transparency fixtures and do not increase the574 test
count or constitute GPU extraction proof. Four incremental builds and all three
enabled selftests passed before final reason-check strengthening; final rerun
logs use effect-identity-final prefixes and all four builds / three574/0 suites
passed with the strengthened checks. Renderer behavior and locked replay guard
remain unchanged. Next implement source-owned diagnostic extraction and checked
replay linkage; no new live gameplay/provenance success claimed. The initial
selftest.cpp lookup failed (file absent); tests were connected to the existing
RunTransparencyContractFixture entry instead. No third-party assets changed.

LOG559 checkpoint947aeafa2e68f7e3fe118556394e282dd2df042e: verified four
post-commit incremental build logs (automation, baseline, no-NGX, feature-off),
three enabled selftest logs each574 passed/0 failed, and no remaining build
process. Pushed feat/neural-rendering to the user fork and verified ls-remote
equals that exact SHA. Tracked worktree clean before this documentation update;
the existing metrics/cache/rtx-remix untracked entries remain untouched. These
are incremental build checks, not new exact-SHA gameplay or performance proof.
Source inspection confirms the post-effect provenance dependency: current locked
input comparison serializes the opaque scene and checks returned input hashes,
whereas effect ownership is a separate GPU snapshot. The intentional
locked-effects-replay-unsupported guard remains necessary. Recorded canonical
visible-stack equality and falsifying controls as the next implementation bound;
no renderer behavior changed and no new provenance success is claimed. An initial
lookup for oit_header.h failed because the actual HLSL header is embedded in
dx11_oitshaders.cpp; the subsequent source inspection used that owning file.

LOG558 c30f2b587 plus worktree corrects native-effect resource accounting. Count
unique snapshot allocations across current/two pending/accepted/evaluated owners,
not shared_ptr aliases. Expose logical copied bytes and six actual owned data
objects per complete snapshot; borrowed resolver shaders are excluded. GPU
fixtures on both D3D11 surfaces verify duplicate-owner, separate-allocation and
empty-owner controls. All four builds and enabled574/574 selftests pass.
Short ax probe uses120 startup samples, no helper, no image capture, same supplied
ON stage and executable SHA2563C3FE896D0036678B6FC8BB7114EE7520FB7FE7ACD8FA5A865B3207C6C86E64A.
It closes0 and reports538,100,352..538,100,392 logical copied bytes per snapshot
(about513MiB). Resource report now correctly moves125 ->131 objects as the first
snapshot appears, growth6. This zero-warmup startup allocation is neither a leak
pass nor final performance evidence; do not repeat the launcher's generic
resources=pass as a stability conclusion. Actual VRAM, temporary command-list/
composite resources and other legacy remake resources remain outside this new
snapshot-only accounting scope. No rendering/effect math or provenance changes.
Next return to the focused post-effect provenance dependency; use these byte
counts when designing subsequent copy/lifetime optimization, without claiming
that memory size alone proves the GPU-time bottleneck.


LOG557 bea3a75776be08c97817a5ee246542792c59a675 commits the independently proven
opt-in native OIT effect snapshot/replay and contribution capture. Four serial
post-commit incremental builds and574/574 enabled selftests pass in the preserved
worktree; these are not isolated fresh exact-SHA tests. The prior moving-capture
cap/log suppression changes are intentionally excluded for a separate FC-054
commit. Their bounds remain default3/ordinary30/explicit360, with overflow,
malformed opt-in and361 rejection tests; LOG552 retains349 actual captured frames
but only299 consecutive and helper watchdog failure. This proves bounded capture
functionality, not the300-frame acceptance gate. The helper/capture throughput
detour remains parked. Neither slice closes camera, general effect occlusion,
normal-sort/multipass, new external provenance or resource/performance acceptance.


LOG556 effect contribution capture at3f3600b44 plus worktree. The previous turn
made implementation/GPU/live-presentation progress (LOG555), not full acceptance.
Retain the already-owned pre-effect neural texture only for explicit preview
capture, alongside the post-effect image. Write neural-before-native-effects.png,
absolute RGB difference, and total/unprotected changed-pixel counts. No extra
GPU copy or retention in ordinary non-capture mode. Default capture remains3;
aw requests30 around source2192, whose retained native image visibly contains
blue weapon arc, orange impact light and particles. Current capture excludes
external-provenance and performance claims. Automation build and574 selftests
pass. Aw executable SHA256602DA5BF382FEC96C9D7F238EEC6ED4E39F43DAF83F9AF5AA1D9420AB9008588,
existing ON config hash unchanged, paired100-Present helper/240-sample host,
warmup2100/start-producer2100. Host/helper both close0;37 effect composites,
30 consecutive captured sources2175..2204. Independent audit verifies every
pre/post difference image and changed-pixel count, original-HUD composition,
actual backbuffer and completed-Present join. All30 native images match retained
aj native pixels exactly. Source2192 visibly restores the blue weapon arc,
orange impact light and particles absent before effect replay;30,300 unprotected
pixels change (46,065 including HUD),18,415 HUD pixels remain protected. Across
the interval unprotected changed pixels range15,774..77,714. Translucent world
fences/details also remain native-rendered: hybrid preservation, not fully relit
materials or recovered camera. A local four-panel slow-loop viewer is created;
its browser opening is queued, not claimed interaction-tested. All four builds
and enabled574/574 selftests pass. Failed controls remain in LOG554/555. Scoped
snapshot/replay/effect contribution is ACCEPTED; general occlusion, normal-sort/
multipass, external post-effect provenance, resource/timing and full goal remain
open. No proprietary configuration changed; supplied hash remains unchanged.


LOG555 source-owned OIT effects implementation at3f3600b44 plus worktree.
Exact FLYCAST_REMAKE_NATIVE_EFFECTS=1 snapshots native fragment/pointer/poly/
constant resources before destructive final resolve, attaches the snapshot to
the original receipt-owned overlay, and replays the same resolver over accepted
neural output before original HUD composition. Single autosorted640x480 pass;
512MiB pixel-buffer cap, bounded existing receipt slots, no CPU readback/wait.
Raw preview and locked replay are rejected in this first experiment. A deferred
command list restores caller state; private pointer copies permit repeat replay.
Both D3D11 GPU fixtures prove exact additive/absent pixels, snapshot isolation
after source-pointer mutation, repeat output and wrong-producer rejection. These
are pre-depth-filtered fixture fragments, not geometry-occlusion proof. All four
builds complete and all three enabled selftests pass574/574. Retained failures:
ComPtr raw ownership/const get/private constant access; header include order;
first fixture used wrong high-byte color expectation. Corrected final logs are
effect-stack-build6/selftest6 and baseline/no-ngx/off. No test count inflation.
Live ak launched with existing ON config hash unchanged,100 helper Presents,
240 host samples after2100 warmup,3 synchronous captures, no locked replay or
sentinel. Staged executable SHA256270AEB3E994E270E107F06AAB116F2B77AA90AFB80E8842598570B281EA71099.
Actual gameplay outcome pending. Costly whole-buffer copies are not performance
acceptance; changed final output needs focused provenance before external proof.
Ak helper exits1 before source arrival; host exits0,240 samples, no presentation
evidence. Al launches host/helper together and reaches source2101, but rejects
publication because the native effect snapshot is unavailable; helper's90-second
initial wait expires, host closes0. This falsifies gameplay integration acceptance.
No effects are displayed in either run. Added pass-layout/extent/capture-stage
skip detail for the next focused diagnosis; do not repeat timing-only retries.
Am confirms one autosorted640x480 pass but resource capture fails; host0/helper1.
An tests bounded backing-texture cropping (GPU fixture640x512 ->640x480 passes)
but live backing is640x480 and capture still fails; host0/helper1. Cropping is
valid allocation handling, not the cause of this live failure. Short120-frame
startup probes replace further full-game retries: ao lacks a channel so its
producer is correctly unavailable; ap/aq/ar reject device identity; as identifies
resource4, the native resolve shader, despite a public-factory allocation-owner
probe. The temporary allocation probe was removed. Copied resource ownership
and factory/context identity remain checked; shaders are retained from the
owning renderer's native resolver, not accepted from external input. At now
captures successfully under the supplied host,120 samples and clean close.
Automation selftest574/574 includes repeated cropped stack replay, wrong producer
and genuinely separate-device rejection on both D3D11 surfaces. Final other
three builds after these integration corrections remain pending. Au now runs
the actual1780+ combat interval with the same bounded helper and three captures,
executable SHA2569183509D46350FE57C001AD2AE6E9A25EE75F791CABCC4D5588A5AC86C85BB28.
Resource counting and timing for the new snapshots are not yet complete; do not
use the launcher's generic resource/pass labels as acceptance for this path.
Au closes host/helper0 and retains returns but never evaluates: the new locked
replay guard treated an empty environment value as an active path. The guard now
requires a non-empty path, matching the existing reader, and logs rejection
reasons. Av (SHA25680D317556F961C7BB066E0EB9C1D3B43749D32E9B02AEA05224114B3AC40C985)
completes37 source-owned effect composites and clean host/helper exits. Three
actual source/current pairs1856/1858,1857/1859,1858/1860 independently pass exact
mask/native/evaluated composition, backbuffer RGB and completed-Present joins.
All three original native images are pixel-identical to retained floor-z native
references at the same source IDs. Visual review of1857 shows the HUD present
and no new gross composition failure, but these three frames do not show an
unambiguous sword trail. Effect-specific moving/occlusion proof, changed-output
external provenance, performance/resource accounting and full acceptance stay
open. The helper's pre-existing40-object cleanup warning remains. Final serial
baseline/no-NGX/off rebuilds complete; all three enabled selftests pass574/574.
The supplied ON config SHA256 remains unchanged. This slice remains uncommitted
pending the visible-effect comparison; no claimed full external or effect gate.


LOG554 native-effect integration prerequisite at3f3600b44 plus worktree.
Extracted the existing OIT blend arithmetic into NativeEffectBlendHlsl and made
native resolve use it, retaining destination-dependent coefficients, secondary
buffer selection and per-layer clamp. Added all64 source/destination modes with
two alpha/color sets to the actual GPU overlay fixture; both D3D11 surfaces pass
independent table-based truth within one UNORM LSB and reject flattened alpha.
This does not prove delayed effects, depth/occlusion or full gameplay replay.
Initial build failed on Windows min macro; first CLI attempt omitted --out;
the first selftest had5 failures from missing test include resolution, the next
had1 because wiring landed in PixelInclude instead of OitInclude. All attempts
remain in effect-blend logs. Corrected runs passed574/574 on automation, baseline
and no-NGX and all four builds completed. Subsequent review added the shared
blend source to shader-cache identity; all four final incremental rebuilds
completed successfully (effect-cache logs). Gameplay parity remains pending;
the production refactor is uncommitted until that focused check.
Next integrate bounded receipt-owned ordered native effects, not a single
flattened RGBA overlay: native shader inspection proves secondary-buffer and
per-layer saturation prevent that shortcut. Capture throughput remains parked.


LOG553 integration-priority correction at3f3600b44 plus preserved worktree.
Previous conversational status turn made no implementation progress. Read-only
source inspection now confirms BuildRemakeViewScene rejects draw.list!=0 and
BuildRemakeViewPacket explicitly omits translucent layers. This changes the next
action: park PNG-throughput work and implement source-owned native effect
preservation, beginning with actual sorted/OIT blend/depth ownership. No runtime
test or visual acceptance is claimed here. LOG552 remains299, not300. Existing
presentation/provenance evidence is retained rather than repeated. Backlog bounds
require additive/alpha/occlusion and wrong-source controls before gameplay proof.


LOG552 moving-aj launched from3f3600b44 plus bounded moving-capture/logging
changes. Requests360 fresh OIT captures, no replay/sentinel,660 helper Presents,
existing120-second helper watchdog and240-second host watchdog. No concurrent
builds. Staged executable SHA25696BDD4DD500E46DFC5AA35E6CAA4C899DB3C9B6E93AC1FEC981FEAA32571AFE8.
Free disk about49GB before launch. Actual continuity/pixels/exit results pending;
do not count images as consecutive frames. Previous af failure remains retained.
Aj ends with349 captures and299 consecutive sources1907..2205, one gap1903 to
1907. All349 pass independent mask composition/backbuffer/completed-Present
checks; minimum17439 protected pixels, no empty masks. Helper hits120-second
watchdog after354 returns and exits1; host1200 samples exits0 cleanly. Do not
round299 to300 or splice an extra frame from another run. The long-capture
diagnostic cap works, but full300-frame requirement remains unproven. A local
comparison.html references original native/backbuffer PNGs with visible frame
IDs and explicitly slowed playback; it is a review aid, not pixel authority.
Next address bounded capture overhead rather than repeat provenance or change
the source-continuity requirement. Resource cleanup warning remains open.

LOG551 depleted health plate: retained ah current2194 diagnostics identify
texture801607344, blend37, translucent list2, planar depth .207822 and exact
left/right bar bounds20,37,272,65 /368,37,620,65. This is separate from the
recognized fill atlas795315888. Add a title-specific planar/region-constrained
plate rule within existing depth/list/order guards, with different-texture and
world-region negatives. A nearby additive bar effect (texture765955760/blend33)
remains unclassified; do not claim all HUD effects accepted from this fix.
Old code fails only the depleted-plate positive (573 pass,1 fail); corrected
automation suite passes574/574. Targeted plate-ai replay is live using the
23-frame retained subset, same failed interval and100 helper Presents rather
than120; watchdog unchanged. Remaining serial builds run separately, so this
is diagnostic pixel evidence only. Do not duplicate the active run.
Plate-ai verifies exact original2192/current2194: native and raw returned PNGs
match ah byte-for-byte, protection rises15233 to18415 with zero lost coverage,
both depleted red/black plate outlines are visibly restored. Independent mask
composition/backbuffer equality and completed evaluated Present join pass.
All four serial builds succeed; enabled suites574/574. Broader additive HUD
effects, full moving sequence, world translucency and final acceptance remain
open; this is the bounded plate/depth repair, not full-title acceptance.
Both plate-ai processes exit0, helper100 Presents and host240 samples/clean
close. The40-object helper shutdown warning remains unchanged and open.

LOG550 targeted ah uses the23-frame copied subset and remains live; no restart.
For the next long run, suppress per-draw HUD diagnostics only under exact
MOVING_CAPTURE=1 (af wrote225MB of logs). Retain all image/identity/receipt/
Present evidence and short-capture draw tracing. Reuse the tested exact opt-in
predicate; this does not alter rendering, masks, fallback or timing acceptance.
The current ah executable predates this logging-only change.
Ah reaches the exact failed source2192/current2194. Original-native and raw
returned PNGs are byte-identical to af; protected pixels increase0 to15233.
Independent mask composition/backbuffer equality and completed evaluated
Present join pass. Names,timer and health fills are visibly restored. Visual
review still finds the depleted health-bar outline incomplete: depth-range
repair accepted only in its scope, not complete HUD coverage. Preserve that
remaining defect rather than equating mask equality with full HUD acceptance.
Ah helper later hits its30-second watchdog after54 returns and exits1; host
240 samples exits0 cleanly. This does not invalidate the earlier exact-frame
pixel/Present join, but the whole run is not a clean-pass result. Logs archived.

LOG549 failed-frame ag replay retains one earlier capture but cannot reach2192:
ReadLockedRemakeInput deserializes all330 full packets on each lookup; observed
accepted-source intervals are4-5 seconds. Helper receive timeout exits11 after
66 Presents/six returns; host240 samples exits0 cleanly. Source/receipt checks
were not relaxed. All remaining build configurations succeed and enabled
selftests pass571/571. Preserve failed logs/capture. Copy23 existing source
archives2170..2192 into a new bounded subset directory for immediate targeted
replay, with originals untouched. Full-directory repeated-read cost remains a
separate tooling defect; do not widen watchdogs to hide it.

LOG548 moving HUD depth repair: full af draw diagnostics show all five known
atlases extending above .21; maxima .230007(header) and .230914(timer/names/
fills/counters). Extend only the title-profile atlas paths' common upper bound
to .24, retaining finite/lower/list/order/region/shape/span checks. New captured
name/bar tests and unknown-atlas/world-region/.25-depth negatives accompany
the correction. This is an empirical captured-envelope correction, not general
camera-independent HUD identification or full-title coverage acceptance.
Old code fails both captured-depth positives (569 pass,2 fail). Corrected
automation build succeeds and selftest passes571/571; all added negatives
remain rejected. Real-game failed-frame recapture and remaining configuration
builds are pending. No commit or repaired-image acceptance yet.

LOG547 moving-af is CORRECTIONS_REQUIRED:330 captures exist, longest source
sequence286, so300 consecutive frames are not proven. Helper reaches120-second
watchdog after335 returns and exits1; host1200 samples closes cleanly/exit0.
Independent pixel audit fails its nonempty-mask check at source2192/current2194:
native image visibly contains full HUD but actual backbuffer has none. Earlier
short-window HUD acceptance does not cover this camera/combat interval. Retain
all images and failed audit; do not omit that frame or report full overlay pass.
The captured diagnostic log is about225MB; consider suppressing per-draw HUD
tracing only in long moving mode, but first identify the actual missing-mask
cause. New evidence changes next action to fixing HUD loss before another long
capture. No clean-run/300-frame/full-pipeline acceptance.

LOG546 moving-evidence window implementation: previous30-frame cap cannot
satisfy the standing300-consecutive-frame moving-image requirement. Add a
separate exact opt-in for max360, keeping ordinary cap30/default3. Tests cover
360 acceptance,361 rejection, malformed opt-in, integer overflow and unchanged
default. Source-credit/memory ownership and stale fallback stay unchanged;
captures remain synchronous and performance-ineligible. Disk free check is
about57GB before launch; bounded330 full source archives may consume several
GB. Do not delete old evidence or weaken existing helper watchdogs.
All four serial builds succeed and three enabled suites pass566/566. The
moving-af run is now live with330 requested captures, no locked replay or
sentinel,660 helper Presents and240-second host watchdog. Staged executable
SHA2567F360C278155342AFB84B2F047018624F971C06599BA8084B0E1BD8815CFD1B6.
Output root fc067-moving-af-composites; logs remake-moving-consumer-af and
remake-moving-publisher-af. Capture count/continuity, image review and process
outcomes are pending; do not start a duplicate while these processes remain live.

LOG545 OIT integration committed/pushed as70f38dd52; fork SHA verified. Tracked
worktree was clean after commit; preexisting metrics/cache/rtx-remix remain
untracked and untouched. Post-commit serial builds succeed in all four configs;
all enabled selftests pass565/565. These are incremental builds, not a fresh
exact-SHA runtime claim. Ownership follow-up finds helper-owned D3D9 pointers
not explicitly released, but current public NVIDIA rtx_remix_api.cpp Shutdown
itself releases registered device/factory references to zero. Therefore do not
blindly add Release after Shutdown (possible dangling-pointer use). The source
at main is supporting contract research, not verified installed1.5.2 source;
the short68edea01 raw URL failed. No runtime binary inspected or cleanup patch
applied. Exact installed-source ownership and the40-object warning remain open.

LOG544 clean ON ad launched with the same floor-z locked inputs and hud-aa
executable, restored (unmarked) presentation evidence,30 bounded captures.
Helper budget is100 Presents/40 sources, below ac's43-source watchdog point;
the30-second watchdog is unchanged. This is a bounded provenance comparison,
not a shortened substitute for the completed600-source delivery run. Next
unchanged hook-disabled OFF and exact-input/frame-qualified comparison.
Clean ON ad exits0 on helper and host, with clean host close.28 original frames
overlap marked-ac: all color/depth/motion/mask hashes and pre-marker returned
hashes match exactly. OFF ae is now launched in the existing unchanged
hook-disabled stage, using an identical executable SHA and floor-z inputs,
100 helper Presents and restored evidence. No external-output conclusion until
OFF and the captured/displayed-frame joins are checked.
OFF ae exits0 on both processes and cleanly closes. It logs accepted public
control evaluations but correctly withholds experimental presentation because
readiness=missing-components; do not weaken that guard to obtain OFF captures.
Frame-qualified local audit joins25 consecutive originals1856..1880 across
marked-ac,clean-ad and OFF-ae: exact color/depth/motion/mask hashes; identical
marked/clean pre-marker output hashes; all25 ON outputs differ from OFF. Each
clean evaluated PNG hashes to the logged output, independently composes with
its original HUD mask/native image, matches the actual backbuffer and joins
completed Present. Each marked counterpart has1024/1024 marker pixels and
completed Present. Native-output substitution is falsified for every clean
world region. This closes focused repaired-OIT external-output provenance only;
ac's watchdog failure remains recorded, not a clean-run claim. Full300-frame
moving-image, camera/scene and final resource/timing acceptance remain open.
Audit script and raw data stay outside Git (fc067-oit-proof-ac-ad-ae.py).

LOG543 OIT marked-ac launched on the repaired hud-aa executable. Uses existing
floor-z retained scene/color/depth archives, strict producer/whole-scene/input
verification and30 bounded preview captures. Explicit marker evidence only;
not performance. No external configurations changed. Next clean-restored ON
and unchanged hook-disabled OFF must match exact inputs before any new OIT
external-output claim. Reject startup-equal outputs rather than counting them.
Marked-ac completes27 retained captures, but the helper reaches its existing
30-second watchdog after43 returned sources/103 Presents and exits1. Host
completes240 samples and exits0 cleanly. Preserve the timeout, logs and partial
captures; do not report this as a clean whole-run pass. Frame-qualified marker
and completed presentation joins remain inspectable; external alteration is
still pending the exact-input clean ON/OFF comparison. No watchdog relaxation.

LOG542 sustained OIT ab launched: existing tested hud-aa executable SHA256
573DE2074EC34B97E31DAAD5AED9ADBB15168113071C54CEEBB64B040D33B6C4,
660 helper Presents (60 warmup/600 source budget),1200 host samples,180-second
host watchdog. Explicit OIT/returned-neural opt-ins; no preview capture, locked
replay or synchronous sentinel. No concurrent builds. Source observation and
helper paired GPU readback still exclude final low-overhead performance claims.
Inspect receipts, accepted evaluations, completed monotonic presentation, age,
busy/fallback counts and clean shutdown; no generic provenance rerun here.
Both processes now exit0; host completes1200 samples and clean close. All600
sender/receiver sequence/frame/producer/byte/digest tuples match, all600 paired
returns publish, zero busy drops.597 unique accepted evaluations have matching
published source/sequence and bounded age.598 successful evaluated Presents
contain595 distinct sources,544 consecutive source IDs, no backward IDs and
max age5. Every displayed source belongs to an accepted evaluation. Zero
preview pixel captures. Supplied configuration hash unchanged; no artifact
files produced by return-only mode. Helper still reports40 undisposed common
device objects: cleanup remains open. Archived remake-oit-*-ab logs retain the
run. This is sustained OIT delivery evidence, not300-frame moving pixel proof,
new external-mutation provenance or final timing/resource acceptance. Next
focused exact-input OIT external-output proof on the repaired scene/HUD path.

LOG541 bounded name-atlas correction: floor-z diagnostics contain60 identified
name draws, texture686272176, aligned quads, expected left/right name regions,
depth envelope .138055..203861. Existing .15 lower bound rejects animated names
without accepted native history in the returned-scene lane. Test a name-only
.138 lower limit, preserving all other classification checks and .21 upper
bound; unknown atlas/world region/lower-depth controls must stay rejected.
This uses retained diagnostics rather than launching another broad tracing run.
The old classifier fails only the new captured-depth positive (564 pass,1 fail).
After the atlas-specific correction all four serial builds succeed and all
three enabled suites pass565/565. Fresh hud-aa OIT capture is launched with
three bounded frames, no locked replay and unchanged external configuration;
visual/Present checks remain pending until it closes. No commit acceptance yet.
Hud-aa now closes successfully on both processes. Frames1851..1853 have restored
KILIK and TAKI text, continuous health bars and the repaired wooden floor in
the actual backbuffers. Independent original-mask composition and backbuffer
checks pass all three; each joins a completed remake-evaluated Present two
frames later. External configuration hash remains unchanged. Archived logs
are remake-hud-*-aa and captures fc067-hud-aa-composites outside Git. This is
bounded visible HUD repair, not full-title coverage or renewed external-output
proof. Next run sustained OIT delivery with synchronous captures disabled;
retain explicit opt-in and full working-pipeline acceptance requirements.

LOG540 floor-z matched-frame check: same staged floor-y executable, fresh
scene export,30 bounded captures, no locked replay. Helper120 Presents and
host240 samples both exit0. Frame1860/current1862 matches coverage-v's original
native PNG and overlay mask byte-for-byte. The new returned image visibly
contains the wooden floor where the failed old image has a black gap/raised
strip. Independent composite and backbuffer equality pass; the log joins
source1860 to completed remake-evaluated Present (hresult0). Consumer config
hash remains unchanged. This is scoped floor repair, not fresh external-output
provenance, performance, complete HUD or world/camera acceptance. Images and
archived remake-floor-*-z logs remain outside Git. Added near/far end-to-end
conversion-to-transport regressions. An early test rebuild hit LNK1104 because
the launcher was running; retained that failure and reran only after both
processes exited. No live process was interrupted or restarted.
Serial retry succeeds for all four configurations; all three enabled selftests
pass561/561 including both end-to-end crossing tests. Backlog inspection and
diff whitespace checks pass. The floor commit excludes the still-working OIT
opt-in, coverage implementation and HUD diagnostics; their evidence is retained
here without treating those code changes as committed or fully accepted.

LOG539 floor-y live validation (40d9f268f plus working tree): fresh OIT scene
export, no locked replay,120 helper Presents and240 host samples; both exit0
and host closes cleanly. Captures1851/1853,1852/1854,1853/1855 now visibly show
the wooden floor beneath both fighters. Source1853 joins accepted evaluation,
owned output and completed remake-evaluated Present at current1855. Independent
PNG composition/backbuffer checks pass all three captures. This is not an
exact-frame old/new comparison (retained failed floor frame1860), nor new
external-mutation proof or performance acceptance. Names remain missing;
camera-relative limitations and helper40-object shutdown warning remain.
All four incremental configurations build successfully; automation,NGX and
no-NGX selftests pass559/559. Staged executable SHA256
76A2B1B4A4291A1A13B19591960C13CB698CA73B369D7124A69215445A7FBBE0;
consumer configuration retains hash222D059C727A683C8DEACE07C938002BDD6A69EB0DC9C908743E17A92B54EFBC.
Evidence remains outside Git in fc067-floor-y-composites and the archived
remake-floor-*-y logs. Next obtain same-frame floor evidence and finish the
bounded scene-export regression before committing this independently proven fix.

LOG538 scene-clip-y: user priority correction moves shared missing floor/scene
export ahead of further HUD diagnostics. No old helper/host processes remain;
name-x host log reports clean close with240 samples. Source inspection finds
estimated vertices outside .1..2501 discarded before triangle clipping, which
omits their entire draw. Two new near/far crossing conversion regressions fail
on the old code (557 passed,2 failed; negative build succeeds). Preserve finite
estimated positions until existing bounded triangle clipping instead; supplied
clips and default observed-only behavior remain unchanged. Real-game floor
causality is still a hypothesis, not a proven repair. Initial read attempt used
the parent directory and failed harmlessly; corrected to repository directory.
The corrected automation build exits0 and selftest passes559/559, including
both previously failing crossing cases. Backlog contract inspection passes;
it is document consistency only. Remaining configuration builds and fresh
gameplay floor comparison are pending; no commit or full-scene acceptance yet.

LOG537 name-x bounded diagnostics: explicit preview attempts now log current
top-HUD draw texture/blend/flags/quads/bounds/depth/classified/stability. Current
frame is explicitly distinct from retained source; no inferred source ownership.
Automation builds pass. OIT name-x uses the same retained input root and awaits
the capture interval to identify missing name classification before changing
profile rules. No external config changes or additional acceptance claims.

LOG537 focused GPU coverage fixture w passes on native D3D11 and D3D11On12.
The fixture shares the production blend descriptor, draws zero over initialized
coverage at targets1/5, checks MAX preserves red and leaves other channels
unchanged, then verifies disabled-blend overwrite erases red. Existing exact
overlay composite checks also pass33 protected pixels/no mismatch on both APIs.
Commands actually ran; no GPU behavior inferred from descriptor inspection.
The shared descriptor extraction is behavior-preserving. Next name-draw
classification and shared floor export; full OIT acceptance stays rejected.

LOG537 coverage-v exact-input result: both processes exit0. Retained original
1860/current1862 capture has byte-identical native and returned Remix inputs to
failed oit-t. Protected coverage grows13952->14858, restoring906 pixels without
losing any prior coverage. Independent composition/backbuffer equality passes;
visual mask review confirms health-bar holes closed. This paired pre-fix capture
is the actual overwrite negative, not a hypothetical failure. Character-name
protection remains incomplete and shared floor export is unresolved. Do not
accept full HUD/OIT from this repair. Dedicated GPU blend fixture and remaining
build checks are still part of pre-commit regression.

LOG537 coverage-v implementation: reactive replay uses independent MAX blending
only on mask target1 and overlay target5; all other target writes disabled.
Normal depth/motion export and native color blending retain previous states.
Code review initialized valid blend enums on all independent targets before
the second build. Automation build and557/557 tests pass; these existing tests
do not independently prove GPU accumulation. OIT coverage-v now replays the
exact retained oit-t scene inputs to test the failed HUD region; no success
claimed until pixel comparison. Full HUD classification and floor coverage
remain separate open items.

LOG537 normal-u completes exit0 on both processes. Whole-scene/producer replay
accepts original1858/1859/1860; one completed pixel capture1860/1862. Its returned
Remix input is byte-identical to oit-t, while original overlay masks differ in
5352 values and native images in5343 channel values. Visual normal mask has
continuous health bars, unlike OIT's holes. Scene equivalence separates shared
floor/export limitations from OIT overlay coverage. Source inspection finds
reactive coverage runs without depth and disables blending while writing zero
overlay for unclassified translucent draws, allowing erasure in OIT submission
order. Next test monotonic MAX accumulation on reactive mask/overlay targets,
not depth/motion targets, against the failed frame and a falsifying overwrite
control. Missing left-name classification and floor coverage remain separate;
do not declare full HUD protection from fixed health bars alone.

LOG537 ownership inspection: OIT and normal opaque draws both use indexed strips;
OIT resolves before inherited display and restores native replay resources.
No OIT-only mutation established yet. Normal-u now uses the same owned executable
and retained oit-t scene/input archives through the whole-scene/producer checker.
If accepted, compare original masks/native images at exact producer identity;
do not assume the floor/HUD defect is OIT-specific or patch state speculatively.
Both processes launched, no result yet. Failed wildcard/missing-path searches
were corrected; original code and external configuration remain untouched.

LOG537 oit-t CORRECTIONS_REQUIRED: both processes exit0; all four builds and
enabled557/557 tests pass. Three actual backbuffers (1858/1860,1859/1861,
1860/1862) independently equal evaluated world plus original native under R8
mask, and join successful Presents; protected counts13984/13965/13952. Raw-world
substitution negatives differ. However visual inspection REJECTS acceptance:
the original native health bars are intact, while the protection mask has holes
that admit transformed scene geometry over them; the returned scene also loses
much of the arena floor. Mask equality alone did not detect missing protection.
Retain native/mask/evaluated/composite images in fc067-oit-t-composites. Next
inspect OIT replay vertex/index/state ownership and overlay coverage before
any sustained OIT run or commit. This is not an OIT HUD/provenance/quality pass.

LOG537 OIT opt-in begins from40d9f268f: OIT drawStrips resolves before inherited
submitNeuralFrame/renderNeuralExports/displayFramebuffer; original resolved
fbTex and source R8 mask are retained by the existing owned overlay snapshot.
Exact ASYNC_OIT=1 now enables this shared path; absent/malformed values retain
bypass. Automation builds and557/557 tests pass. Live oit-t requests three
synchronous original-HUD/composite/backbuffer captures, with no locked replay
or sentinel. Both processes launched; actual pixel validation remains pending.
This does not add unsupported translucent world geometry or prove full OIT
quality/provenance/performance. Remaining serial builds still required.

LOG536 final slots-s: host exits0/1200 samples/clean close. Archived host/consumer
logs;600 receipt pairs independently match sequence/frame/producer/bytes/digest,
598 evaluated Presents remain monotonic and no image artifacts exist. External
configuration hash unchanged222D059C727A683C8DEACE07C938002BDD6A69EB0DC9C908743E17A92B54EFBC.
Bounded two-slot delivery slice ACCEPTED; full pipeline acceptance remains open.

LOG536 slots-s helper completes660 Presents/exit0:600 paired returns all publish,
zero busy drops versus long-r's9. Current host logs show597 unique accepted
evaluations and598 evaluated Presents over595 unique sources, longest consecutive
source sequence543. Each displayed source joins an accepted evaluation at age<=8;
host tail still running at inspection. All four builds and enabled556/556 tests
pass. Build/selftest activity overlapped this live run, so it is explicitly NOT
a clean performance comparison. Source tracing/readbacks and40-object helper
cleanup warning also remain. Next complete close/receipt checks, then commit
and proceed to OIT rather than repeatedly proving the normal route.

LOG536 two-slot return implementation from930aa72da: shared layout version4
matches two source credits with two bounded paired-image slots; receive oldest
ready first. Receipt/integrity/depth/stale/duplicate/close validation remains.
Updated independent-slot/FIFO tests pass in automation556/556. Both helper and
Flycast rebuilt and staged for slots-s600-source no-file live run, currently
running. Do not claim dropped-return improvement until observed; remaining
serial builds and capacity/compatibility review still required before commit.

LOG535 long-r completes exit0 on both processes:600 live sender/receiver scene
receipts match,600 paired returns contain307200 depth values,591 return publishes,
589 unique accepted evaluations and605 completed evaluated Presents over587
distinct sources1859..2463. Longest consecutive displayed source sequence222;
maximum displayed age5, monotonic IDs, no image artifacts. Host completes1200
samples and closes cleanly. Skip diagnostics count4 material-cache-pending and79
no-return-credit (including startup/end); nine consumer return attempts are busy.
External config hash remains222D059C727A683C8DEACE07C938002BDD6A69EB0DC9C908743E17A92B54EFBC.
This is bounded live delivery, not exact-input pixel proof for600 frames or final
performance: source tracing and synchronous helper GPU readbacks remain enabled.
Helper still reports40 undisposed common objects. Next address the single-image
return mailbox contending with two outstanding source credits, then OIT support;
do not hide drops/repeats or relax age limits. Three other serial builds and
enabled556/556 tests pass; SDK193/193 and Python340/340 rerun separately.

LOG535 extended no-file run begins at8927f1752: only exact async return-only
invocation permits up to660 Presents; longer runs use120-second total watchdog
after initial source arrival. Existing capture modes keep120/30-second bounds.
Automation helper build passes; ordinary121 and extended661 arguments both
reject before runtime loading. Live long-r requests600 source frames paired
with1200 host samples, no image capture/retained replay/sentinel. Both processes
are live; no sustained-coverage or performance result yet. The owned host is
the already tested clip-q executable, not a newly claimed exact-SHA build.

LOG534 clip-q live result: helper and host exit0. All18 formerly rejected source
frames1878..1895 are now published and have completed remake-evaluated Presents;
zero clip-unsupported records remain. Overall61 publications,56 unique accepted
evaluations and58 evaluated Presents over54 distinct source frames, versus
return-o's16 unique displayed sources. Each displayed source joins an accepted
evaluation and is at most8 current-frame ticks old. All four serial incremental
builds pass; enabled556/556 tests pass. This is live delivery/display evidence,
not a new pixel provenance, visual-quality or600-frame performance acceptance.
Material-cache-pending still causes shorter gaps; helper still ends after60
sources and reports40 undisposed objects. Next extend the bounded no-file live
run for sustained coverage, retaining watchdog/failure bounds and native fallback.

LOG534 clipping implementation: clip expanded camera-relative triangles against
the unchanged supplied0.1/2501 planes before strict adapter validation. Preserve
inside triangle ordering, interpolate UV and all packed color channels, retain
flat normals and original source vertices, and bound generated geometry. Empty
or nonfinite scenes still reject atomically. Five new analytic checks pass;
automation enabled selftest556/556. First build failed because the new fixture
used Vec3 access on an array position; corrected to indexed access and retained
the failed q build log. Live clip-q now tests publication through the previous
gap with diagnostic disk writes, sentinel and locked replay disabled. No live
success or final cadence claim yet; remaining serial builds and review pending.

LOG534 gap-p isolates the sustained-display failure: all source frames1878..1895
reach packet construction and reject with clip-unsupported. This is the adapter
enclosure check in remake_scene.cpp (behind/near/far-plane vertices), not proven
PVR tile clipping and not channel-credit or disk-write starvation. Both live
processes exit0; logs archived as remake-gap-{flycast,reshade,consumer,publisher}-p.
The opt-in skip diagnostics build passes all four serial incremental builds and
enabled551/551 tests. A Windows wildcard path search failed and was corrected
using directory plus file filters; no runtime or evidence was restarted.
Next implement bounded triangle clipping to the existing supplied diagnostic
near/far planes, interpolating UV/color and retaining source geometry unchanged.
Do not widen clips, remove adapter rejection, discard crossing triangles wholesale
or call the supplied enclosure recovered camera truth. Analytic crossing/fully
outside/unchanged controls and actual publication through1878..1895 must pass.

LOG533 return-o result: both processes exit0;60 live published/received scene
receipts match,60 paired returns contain307200 depth values,58 publish and56
are retained/evaluated once. No BMP/depth artifacts exist at the output prefix;
no sentinel, preview-capture or locked-replay records occur. Actual evaluated
Presents number23 over16 unique sources, so this is NOT sustained delivery.
Source1877/current1879 is followed by source1896/current1898; the last displayed
1877 correctly expires after current1885. Later evaluations continue but the
existing timeout latch keeps native fallback. Next identify the publication
gap (scene/texture readiness versus credit); do not relax age8 or allow silent
native/neural reentry. Helper's40-object cleanup warning remains open. This
proves disk-free paired return behavior only, not zero-copy/performance or
externally changed pixels in this uncaptured run.

LOG533 return-only helper begins fromcae204ca90c3ac435be2e545612bfa5fadeaa9a9.
Previous integration commit pushed to fork and remote SHA matched; post-commit
four serial incremental builds passed and enabled551/551 selftests passed.
Remove per-source BMP plus RGBA32F depth file writes only in an explicit async
return-only mode; retain paired readback/receipt publication, source-age8 and
120-Present helper bound. Automation and other three builds pass, enabled551/551
selftests pass. Non-async return-only invocation rejects with code2 before
runtime load (PowerShell wrapper also reports exit1 from that expected native
negative). Live return-o is running without retained-input substitution, sentinel
or preview capture; no external configuration changes. No success or performance
claim until its actual returned/evaluated/displayed counts and absent files are
checked. Original source effects remain content, not an optimization target.

LOG532 k/l/m ACCEPTED bounded changed-route integration: marked ON, clean ON,
and existing hook-disabled OFF all finish exit0/120 helper Presents and240
host samples/clean close. All12 retained inputs match color/depth/motion/mask
across controls; marked/clean additionally match returned output hashes.
Original frames1864..1873 are ten consecutive externally altered outputs;
1861/1862 remain unchanged startup controls and are excluded. For each of the
ten, clean evaluated PNG hashes equal the logged returned hash, independent
original-native/R8-mask recomposition equals the full composite, and actual
pre-OSD backbuffer RGB equals that composite. Marked counterparts have1024/1024
sentinel pixels and completed candidate Presents; clean captures join successful
remake-evaluated Presents. Host logs report Feature18 create/evaluate and stable
off/1/1/203/0/0/enabled tuple; OFF explicitly reports all hooks disabled.
Read-only Python checks actually ran; fresh Python340/340 and SDK193/193 pass.
Visual inspection of source1872 shows fighters, temple and intact HUD, retaining
the approximate scene/material limitations. Evidence is under fc067-restored-l
and logs remake-{marked,restored,off}-{flycast,reshade}-{k,l,m}; no performance,
fresh exact-SHA, recovered camera or full-pipeline acceptance is inferred.
Next commit this tested slice and address sustained delivery/OIT. Do not rerun
general transport or optimize away intentional source trails.

LOG532 marked-k follow-through: both existing process handles returned exit0;
host completed240 samples/clean close and helper120 Presents. Twelve retained
inputs reached developer evaluation with zero reported capture failures; ten
distinct displayed-source pixel directories exist (1821..1830), with the later
marker readbacks reporting1024/1024. This is marked-output reachability, not yet
a twelve-frame external-output claim. Archived both host logs before launching
restored-l concurrently with its helper against the same archive-j inputs.
The staged owned executable SHA256 is
3F09AC78C09B234B0D50B8AF2838F8805371E6D65082F342E644B5E676972C9C;
external configuration remains at222D059C727A683C8DEACE07C938002BDD6A69EB0DC9C908743E17A92B54EFBC.
Helper still reports40 undisposed common device objects. A read-only inspection
command failed on a spelled-out numeric argument and was corrected; no test
or capture was restarted. Clean and hook-disabled matching remain next.

LOG532 widened archive-j: add developer capture-count parser, default3 and
explicit1..30, invalid/overflow/trailing-text disabled. Automation plus other
three serial incremental builds pass; enabled551/551 tests pass. Request12
captures with60 source frames: helper exits0/120 Presents, host exits0/240
samples/clean close. All12 capture/archive directories validate through the
existing locked reader; source1861,1862,1864..1873 have successful pixel captures.
Transport inspector verifies60 pairs,58 published returns,two busy drops,56
retained age1..2,two not retained,ten source gaps. These numbers do not imply
continuous display or external provenance for all frames. Next marked/clean/OFF
controls use fc067-window-j-composites, retaining first-two startup results.
No fallback threshold or external settings changed. Goal's300-frame moving
and separate600-frame/99-percent normal/OIT acceptance remain unchanged/open.

LOG532 final g/h/i regression: SDK mock193/193 and Python340/340 pass; backlog
and diff checks pass. Prior all-four incremental builds and enabled549/549
remain green. No standalone runtime/camera/whole-pipeline closure inferred.

LOG532 matched g/h/i controls: marked ON, restored ON and unchanged safe-mode
OFF each exit0 with helper90 Presents and host180 samples/clean close. All three
archived inputs match exact color/depth/motion/mask hashes; marked/restored
returned hashes match all3. Marked frames1818/1819 reach1024/1024 sentinel pixels
and completed Presents; clean captures preserve exact original HUD/evaluated
world/backbuffer. OFF logs explicit EnableHooks=0 safe mode. Inputs corresponding
to original1864/1866 have identical ON/OFF returned hashes and are NOT external
mutation evidence. Original1867 is confirmed in this bounded changed-route
scope: ON returned78277A1F80E8F281 differs from OFF855EEC5C57EE0251 under exact
four-input equality; clean evaluated PNG hashes to ON and independently
recomposes to the actual backbuffer, with original HUD exact. Consumer logs
reported off/1/1/203/0/0/enabled and Feature18 creation/evaluation. This is ONE
retained-input externally altered displayed frame, not sustained moving/full
pipeline acceptance. All runs use identical owned exe SHA256
7876109651E6713F2D7D55C8C67924CCEFF8FA52FE30CA33013806EB35940704.
Embedded Git label remains stale3e78a6f4f; actual basefc415a22f plus working tree,
not fresh exact-SHA evidence. All four incremental builds and549/549 enabled
tests pass. Next widen the bounded archive sequence beyond consumer startup,
not another generic transport phase; retain unchanged first-two controls.

LOG532 live archive-f: unchanged supplied-ON config, helper exits0/90 Presents,
host exits0/180 samples/clean close. Captured frame1864-present1866,
1866-present1867,1867-present1868 directories include exact original packets,
returned BGRA/depth and input-hash manifests; each pixel capture reports success.
All three archives pass the existing check-locked-remake-input command (1228800
color bytes,307200 depth values each). Add performance launcher diagnostic
--remake-evidence none|marker|restored, restricted to explicit async neural/
preview capture plus dlss5 On12, with existing480-frame bottom-right sentinel
settings. Invalid mode and unarmed invocation both reject before launch.
Automation rebuild/selftest549/549 pass. Async evaluation now invokes existing
consumer-status/evidence hash logging; this edit follows archive-f, so archive-f
is not sentinel proof. Next run locked replay marked/clean/OFF controls. The
launcher retains its historical performance name, but these runs are explicitly
synchronous/ineligible for performance. All raw artifacts remain outside Git.

LOG532 source-checked async replay implementation: explicit preview capture
retains source packet ownership with the existing pending/accepted/evaluated
overlay slots. New frame-... capture directories can write the existing locked
replay format: source packet, BGRA, projection depth, receipt and validated
color/depth hashes. Writer rejects mismatched source receipt and existing files.
ASYNC_LOCKED_INPUT_ROOT invokes the existing exact producer/scene/input verifier
before selecting retained pixels; current overlay receipt is attached only after
verification, and replay original frame is logged/captured explicitly. No
substitution is represented as live consumer delivery. First build failed on
missing ReadLockedRemakeInput declaration; explicit header inclusion fixes it.
Automation incremental build and549/549 selftests pass, including four new
archive/negative/roundtrip checks. Live archive/replay and external marked/
clean/OFF controls remain pending; no external provenance acceptance yet.

LOG532 supplied-ON eval-e: unchanged configuration SHA256
222D059C727A683C8DEACE07C938002BDD6A69EB0DC9C908743E17A92B54EFBC;
owned staged executable B6652F53692A40C7ACD14046785D60FDCF612EB0827ED7A5831692D54B9141AE.
Host exits0/180 samples/clean close; helper exits0/90 Presents. Host reports
active off/1/1/203/0/0/enabled tuple and feature18 create/first evaluation.
Three evaluated-composite pairs1861/1863,1862/1864,1864/1865 havezero HUD/world/
backbuffer mismatches; last backbuffer viewed with fighters/temple/HUD. These
facts alone are NOT external provenance. Reject public-D versus supplied-E
frame1864 output comparison: original native/HUD mask match, but raw Remix
inputs differ despite same scene digest, so output difference is not an
exact-input control. Preserve rejected comparison. Next retain/replay exact
returned inputs under existing source-qualified scene checks and run marked/
clean/policy-off controls on the evaluated display path. Added its original-ID
QueueNeuralOutputPresent notification so the existing sentinel/backbuffer
verifier can observe copied evaluated output; this edit is after eval-e and
must be tested separately. No settings or binary internals changed.

LOG532 live public-DLAA eval-d: helper exits0/90 Presents and host exits0/180
samples/clean close. Inspector validates30 paired source/image files,27
published returns,three busy,25 retained age1..2,two not retained,ten gaps.
All25 retained scenes have unique accepted neural evaluations;19 evaluated
Remix Presents complete. Three source/current pairs1864/1866,1866/1867,
1867/1868 capture evaluated color separately and have17509/17553/17493 protected
pixels,zero HUD/world RGBA mismatch and zero backbuffer RGB mismatch. Independent
Pillow recomposition and exact successful evaluated-Present joins pass all3;
shifted-display/raw-world substitution controls reject. Raw/evaluated images
differ. First backbuffer visually inspected: fighters/temple/HUD visible with
known camera-relative omissions; not temporal or remake-quality acceptance.
All four incremental builds pass; enabled545/545 tests pass. Source-only
transport inspector intentionally does not claim presentation/external proof.
Synchronous captures are not performance evidence. Next test supplied external
route with changed-route provenance and negative controls; public success is
not Feature18/output evidence. Helper40-object cleanup warning remains.

LOG532 evaluated-display implementation: select owned evaluated source/color/
original overlays when async neural is requested, composite evaluated world
then original HUD, and retain separate remake-evaluated cadence/Present labels.
Capture keeps evaluated-remix.png separately; world expectation uses evaluated
color, while HUD uses original native. First build failed C2662 because the
custom ComPtr get() lacks a const overload; use its existing pointer conversion.
Corrected automation build passes545/545 selftests, including evaluated-source
latency/hold accounting without standalone/external provenance claims. Live
public-DLAA eval-d launched with unchanged OFF configuration and synchronous
three-frame preview capture; results pending. Remaining builds run serially.

#532 2026-09-09 fc415a22f plus working tree | Begin ordinary returned-scene neural submission behind FLYCAST_REMAKE_ASYNC_NEURAL=1. Reserve the stage for returned sources (no alternating native history), require source-qualified original overlay and age, suppress duplicate source attempts, share the existing validated color/depth upload, clear native motion/confidence/draw correspondence, set explicit zero jitter/reset/full bias, and own a copy of accepted D3D12 output plus original source/overlay. Do not mark returned evaluation as accepted native correspondence. The evaluated display hookup remains pending, so this opt-in suppresses raw preview and keeps native fallback. Automation build and existing544/544 selftests pass; no live evaluation/presentation or new-path correctness claim yet. Code review corrected preliminary numeric mode checks to actual Dlaa/Dlss5Experimental enum symbols before building. Next connect the owned evaluated snapshot to the proven overlay/display path and extend its separate accounting/proof; do not commit this unproven intermediate slice.

LOG531 final focused checks: independent Pillow recomposition of all three
saved PNG sets equals the actual backbuffer RGB, with exact successful Present
joins. Six deliberately shifted-display/omitted-overlay controls reject.
SDK mock193/193 and Python340/340 pass; all four builds and enabled544/544
selftests previously passed for this slice. ACCEPTED only for bounded raw-Remix
display and original-HUD composition. Full working-pipeline acceptance remains
open. No proprietary binary/config/media or private capture is staged.

LOG531 pixel c: add separate developer-only FLYCAST_REMAKE_PREVIEW_CAPTURE
directory, maximum three attempts per renderer instance, exact640x480 SDR
extent and non-overwriting frame directories. Retain original native/R8 mask,
returned Remix, protected composite and actual pre-OSD backbuffer. Three source/
current pairs1861/1863,1862/1864,1864/1865 have17441/17375/17509 protected pixels,
zero protected/unprotected RGBA mismatch and zero composite/backbuffer RGB
mismatch. Each exact pair joins a successful raw-Remix Present. Inspected first
backbuffer: fighters, temple and intact HUD visible; black/missing regions and
approximate rendering remain, not scene/camera quality acceptance. Consumer
exits0/90 Presents; host exits0/180 samples/clean close. Inspector verifies30
source/image pairs,29 published returns,one busy,26 retained at age1..2 and
seven gaps. Report explicitly marks synchronous capture enabled and preview
performance excluded; do not use timings from this run. All four incremental
builds pass and enabled selftests544/544. No external config changes. Files stay
outside Git under fc067-pixels-c-composites and fc067-pixels-c-host. Raw-preview
transport inspector still correctly says presentation_proven=false because it
does not inspect the new backbuffer evidence. Combined DLSS5 remains next.

LOG531 live b: synchronized launch succeeds, helper exits0/90 Presents and
host exits0/180 samples/clean close. Existing inspector verifies30 source/image
pairs,29 published returns,one busy drop,26 retained original-overlay receipts
at age2,three replies not retained,seven source gaps. Actual context logs record
five successful held-native and22 raw-Remix Presents. Returned display advances
1861..1876, then holds1876 through current1884; a subsequent delivery gap trips
the eight-frame fallback latch. Later retained1896..1911 replies do not reenter.
Measured180-frame window includes10 Remix/five held/165 public Presents,zero
missing/identity errors,four output repeats,15 source gaps and two Remix
transitions; window is not the complete27-preview-Present interval. Helper still
reports40 undisposed common objects. No pixel proof or whole-pipeline performance
claim: raw helper files are not Flycast backbuffer captures. Next capture actual
composited preview pixels with exact original HUD/source IDs, then connect
returned-scene evaluation; do not repeat generic transport validation.

LOG531 live attempt a rejected: consumer starts07:21:54, publisher07:22:21;
host reaches producer1780 at64.54 seconds, after the consumer's90-second
initial deadline. Helper exits2 before runtime load; host exits0/180 samples
with no async publications or preview Presents. This is launch skew, not
presentation evidence. Preserve remake-present-{consumer,publisher,flycast}-a
logs. Retry b launches both processes concurrently, same binary/settings and
new channel/output paths. Owned exe SHA256
5D0F13B51DE22A2033BE4C7ABA24413B50C33BA3AB07158B886ABB4D4DA73007;
external OFF config remains656051579D08B667346575164B3C3D8490F40DDD55DB74C8199B0996B56AF2C7.

LOG531 build completion: serial automation, NGX baseline, no-NGX and
feature-off incremental builds all exit successfully; all three enabled
selftests pass544/544. Actual moving in-Flycast preview is the next check.

#531 2026-09-09 1c54a274f plus working tree | Resume actual returned-image display integration: compose original receipt-owned HUD over raw Remix, align entry via a native hold, bound stale output to eight frames and latch fallback, and separate raw/held/neural accounting. Remove an unused conditional placeholder before building. First automation incremental build/selftest passes543/543; review finds same-frame raw output could incorrectly count a background accepted evaluation as presented. Correct accounting and add its negative check: rebuilt automation passes544/544. Remaining configurations are launched serially in remake-present-*.log; final exits must be checked before claiming success. Backlog inspector and diff check pass. No live preview, pixel preservation, combined DLSS5, or performance acceptance yet; changes remain uncommitted pending relevant runtime proof.

LOG530 regression: all four serial incremental builds pass; enabled534/534
selftests, SDK193/193, Python340/340, R8 ownership/material fixture and backlog/
diff checks pass. Existing external config remains byte-hash unchanged. These
are incremental working-tree runs, not fresh exact-SHA automation evidence.

#530 2026-09-09 3fa03b59e plus working tree | Add immutable original-native color/R8 HUD-mask snapshots before ordinary publication, issue receipt only on success, and retain two pending plus one accepted snapshot. Returned image acceptance now requires matching original overlay ownership. Return-credit preflight prevents speculative GPU copies while busy. All reset/expiry/close paths release snapshots; no display override. CPU selftests534/534 pass, including wrong frame/producer/receipt/epoch/age and preflight release controls. WARP fixture verifies original native and actual R8 mask bytes survive source mutation and failed capture preserves prior owner (three owned comparisons,20 negative controls total; previous material/cache goldens unchanged). Actual async-overlay-a capture-disabled On12 host completes180 samples/clean close; helper exits0/90 Presents. Inspector requires original overlay receipts:30 matched moving sources/image pairs,30 published replies,zero busy drops,27 retained with exact original color/mask ownership at age2,three not retained,seven source gaps. No composition/presentation, full-HUD classification, performance or combined-DLSS5 proof claimed. Existing40-object helper cleanup warning remains. Sender/consumer/inspector logs and raw images remain private outside tracked source.

LOG529 final checks: guard-inclusive four serial incremental builds pass with
523/523 enabled selftests, SDK193/193, Python339/339 and the60-source inspector.
Async-c own executable SHA256 is
ACC8FCA47625B086D4FEA1E862F6E7ED8BAAF4429FA2718FFF429F95DDDD60B5;
source build labels may remain stale from the shared generated version header.
This is explicitly working-tree/incremental evidence, not a fresh exact-SHA run.

LOG529 regression: four serial incremental builds, enabled523/523 selftests,
SDK193/193 and Python339/339 pass before final C++ exception containment guard.
The guard stops only the experimental feed on ordinary C++ failure and retains
existing presentation. Generic harness resource checks do not yet account for
all new cache/channel objects; no combined leak/performance gate is claimed.

LOG529 async-c: capture-disabled ordinary host completes240 samples/clean close;
helper exits0/120 Presents over60 moving source packets. Inspector matches all60
sender/receiver sequence/frame/producer/byte/digest receipts and checks60 nonempty
640x480 color images plus finite normalized depth planes. Producer publishes61
sources; helper publishes59 paired replies and explicitly drops sequence5 with
return-busy. Host retains56 pairs aged2..3 renderer frames; three published replies
are not retained, and11 source gaps are recorded. The initial inspector wrongly
required every async return to publish and fails on the explicit busy drop;
corrected inspector reports that drop separately, rejects unaccounted missing
replies, and never certifies presentation/temporal quality. Six new Python
controls cover receipt mutation, duplicate publication, age violation, missing
return and explicit busy accounting. Final frame1948 visually inspected: moving
fighters/temple rendered; no HUD composite, aesthetic or camera-truth acceptance.
Existing40-object helper cleanup warning persists. No external config edits.
Next retain original source HUD/native surfaces keyed by issued receipt and
integrate delayed presentation with distinct source/current identities.

LOG529 async-b: corrected ordinary producer completes180 public-DLAA samples,
capture disabled, clean close; helper receives source frames1782/1783/1791,
producers1781/1782/1790, with owned10,171,143-byte packets and three paired depth
returns. All three output BMPs/depth files exist; final BMP visually inspected:
both fighters, temple and floor rendered, no protected HUD composition. Helper
exits0/63 Presents and explicitly logs source gap1783->1791 with runtime temporal
reset unproven. Existing40-object cleanup warning persists. Sender INFO receipts
were hidden by the default log level, so exact bidirectional receipt aggregation
is not claimed for b. Change only those opt-in logs to NOTICE and run c with60
moving source frames. No capture-time wait or feature enablement was used to fix
the source stamp dependency. No combined-output or production pacing claim.

#529 2026-09-09 d38fa2c03 plus working tree | Wire opt-in ordinary DX11 scene feed to bounded async texture cache and return-credit channel. Capture remains disabled; source publication uses complete current scene/materials, no capture wait. Retain at most one valid paired reply with original source identity and8-frame expiry, but no returned presentation yet. Explicit helper async mode accepts forward source gaps under invariant game/build/epoch/origin and recreates uploader resources; external runtime temporal reset remains unproven, strict old diagnostic path unchanged. Automation build523/523 tests pass. First actual async-a helper exits2 after90s with no source; host completes180 ordinary public-DLAA samples/clean close, but producer IDs are0. Cause: QueueRender stamps producer identity only when NeuralCaptureFrames>0. Corrected condition explicitly includes async opt-in while retaining RTT/Naomi2 exclusions; no fake capture enabled. Corrected async-b run pending. Existing OFF consumer config hash remains656051579D08B667346575164B3C3D8490F40DDD55DB74C8199B0996B56AF2C7. Do not read generic harness transition=pass flags as executed transitions; no transitions requested. Source observation overhead/helper captures are not whole-pipeline performance acceptance.

LOG528 regression: all four serial incremental builds pass; automation/NGX/
no-NGX selftests516/516 each, SDK193/193, restored GPU material fixture and
backlog/diff checks pass. Production synchronous capture remains the default.

#528 2026-09-09 1d05115c3 plus working tree | Implement owned nonblocking D3D11 material readback ticket and bounded generation-qualified DDS cache; optional ReadRemakeViewTexture cache path preserves default synchronous capture. WARP GPU fixture matches18 raw mips across six formats, five initial DDS encodings and five actual changed-upload DDS encodings (report async_exact_mips=28 counts all of these comparisons),92 async controls; original145 raw/RGBA comparisons and19 negative controls remain. Covers busy/budget/output atomicity, upload/RTT/palette/resource mismatch, reset/double retirement, exact generation reuse, changed content,128-entry cap,120-frame expiry and epoch reset. Controlled mutation disables cache generation invalidation: fixture exits1 at material-cache-generation-change-never-serves-old-bytes; restore rerun passes. Failed mutation output retained separately. Fixture-only Flush/deadline polling is excluded from performance. Generated fixture SHA remains stale3e78a6f4f despite actual source base1d05115c3; results are working-tree/incremental, not fresh exact-SHA. No actual game/On12/performance result claimed for this slice. Next wire ordinary-frame feed using complete current scene plus ready materials; explicitly handle skipped source frames in the helper with reset semantics, preserving strict consecutive diagnostic controls and matching returned-frame overlay ownership.

LOG527 regression: all four serial incremental builds pass, automation/NGX/
no-NGX selftests516/516 each and SDK193/193 pass. Backlog contract and diff
whitespace checks pass. Real shared-memory delayed-return fixtures run locally;
no new real-game capture or performance pass is claimed for this credit change.

#527 2026-09-09 838c88f09 plus working tree | Begin asynchronous ownership prerequisite: add return-aware publication to the existing two-slot channel, retaining each source receipt until its image is received or explicitly expired. Existing capture exchange uses it; legacy one-way Publish is unchanged. Added explicit source-age/epoch expiration without advancing neural history. Actual shared-memory RED controls consume both packets before returning images: unrestricted publication overwrites the first receipt,512pass/4fail. Return-credit guard makes516/516 pass, including first delayed reply retains original frame, busy does not advance sequence, receipt retirement frees one credit, age boundary, expired reply rejects atomically and epoch expiry. Automation build passes. This is not ordinary-frame feed, GPU performance, camera acceptance or combined gameplay acceptance. Texture export still calls blocking GPU Map and must be replaced by retained generation-qualified staging before ordinary-frame delivery. New epoch also still requires a channel/session restart; expiring receipt ownership does not bypass existing source-order validation. No runtime/config/media changes.

LOG526 completion evidence: marked-c and clean-a pass all three exact returned
RGBA/inverted-depth, reset/full-bias and protected/native versus world/public
composition checks. OFF-a also captures3/clean-close0. Strict external verifier
confirms clean-a3/3 using marked-c and OFF-a fresh logs, exact input/output hashes,
same-frame sentinel Present and consumer tuple off/1/1/203/0/0/enabled. These runs
use the identical staged working-tree binary stamped3e78a6f4f, not a clean
exact-SHA build. Source-qualified retained pixels remain explicitly locked replay;
LOG525 separately establishes live returned-input delivery. This does not prove
continuous gameplay, temporal guidance or performance. OFF config SHA256 remains
656051579D08B667346575164B3C3D8490F40DDD55DB74C8199B0996B56AF2C7.
Failed final-build attempt retained: LNK1104 because capture still owned the test
executable; serial retry after actual process completion passes automation build
and508/508 selftests. All four serial incremental builds pass; all three enabled
selftests508/508, SDK193/193 and Python333/333 pass. Backlog contract and diff
whitespace checks pass. Final clean image visually inspected: nonempty fighters,
arena and protected HUD without the sentinel; no aesthetic winner claimed. Confirmation
now also explicitly rejects requested-native-fallback; original hash/marker/host
criteria were not weakened. Next implementation is bounded asynchronous ordinary
scene delivery and matching returned-frame/overlay ownership, not another replay
viewer or general transport audit.

#526 2026-09-09 3e78a6f4f plus working tree | Added bounded source-qualified locked returned-input loader, explicit replay metadata/receipts, producer-based capture scheduling and read-only check-locked-remake-input command. Synthetic tests cover renderer/build relabeling only, game clock, geometry and texture mismatch, valid archive, missing producer, changed pixel hash, truncated depth and producer timing. First compile-c fails missing sstream/iomanip in tests; fixed explicitly. Initial actual locked-on-a captures3 and locked-detail-b captures1 both exit0 but reject replay and remain native; no provenance accepted. Exact epoch/ordinal/cycle and zero jitter match retained live inputs. Detailed live rejection is source-input-hash-mismatch; grouped user-locale RED test reproduces it (507pass/1fail). Classic-locale hash formatting fixes it (508/508). A premature generic mismatch inference from fallback-rebuilt packets is not retained as a scene-difference finding; rejected replay packets/status now survive archival. Read-only checker validates original retained live archive. Initial patch-context mismatch and malformed PowerShell foreach pipeline failed without changes, corrected. Marked three-frame locale-corrected rerun is pending. Existing confirmation rules untouched, supplied config not edited. Original live dataset remains fc067-nr-flush-source-a; no replay is labeled fresh/live or performance evidence.

LOG525 regression: four serial incremental builds and all enabled497/497 tests
pass; SDK193/193. Final logging clarification reports live_provider=true for
actual live-channel input instead of the old hardcoded false; retained-file
replay remains false. This does not change the source/readback behavior.

#525 2026-09-09 5e4a41f75 plus working tree | Retained nr-on-b scene alone renders visible fighters/arena,63 Presents and clean exit0. First coexist-a attempt missed temporal overlap (host exits05:36:17, helper starts05:36:29); retained as invalid coexistence evidence. Coexist-b overlaps a running hook-enabled host and exits0/nonempty. Added explicit FLYCAST_REMAKE_NOACTIVATE_TEST to match live SW_SHOWNOACTIVATE: coexist-c also overlaps, renders visible scene and exits0 with63 Presents; host remains live and later completes3600 samples/clean close. These workloads are isolation, NOT performance acceptance. Retained depth hashes match isolated/noactivate on all three frames; color hashes differ, so exact scene packets do not lock neural input images. Added producer-context Flush after constructing the owned packet and before developer exchange/wait. Actual nr-flush-a live run now returns and applies all three paired frames1782-1784, Flycast exits0/clean-close3 and Remix exits0/63 Presents. Input inspector passes exact color/depth conversion, zero motion/confidence/ID, full bias, accepted eval and exact HUD/world composition for all three. Same-frame marker logs show1024/1024 on all three. Final image reviewed: nonempty fighters/arena, protected HUD and bottom-right marker. Consumer-reported tuple unchanged off/1/1/203/0/0/enabled, EnableHooks2. Config hash remains222D059C727A683C8DEACE07C938002BDD6A69EB0DC9C908743E17A92B54EFBC. Flush-run improvement supports pending-work submission before waiting, not a general driver root-cause claim. Strict combined provenance remains UNCONFIRMED until clean restored candidate/ON/OFF use identical returned input images and exact source provenance. No temporal/async/quality/performance acceptance; old40-object cleanup warning persists. Automation build/selftests497/497 pass; other final regression pending.

LOG524 final checks: all four serial incremental builds and enabled497/497
selftests pass; SDK193/193, Python333/333 passed before final help/empty guard.
Byte inspection confirms all1228800 color bytes and all1228800 depth bytes of
nr-on-b frame1784 are zero. The swapchain warning also occurs in successful
input-on12-a, so it does not discriminate failure. No helper process remains.
Configuration hash is unchanged. Next replay the retained failing packet alone
versus concurrent active host to distinguish source validity from coexistence;
that replay must be labeled a diagnostic, never purported live gameplay.

LOG524 nr-on-b is NOT ACCEPTED: absolute renderer scheduling produces three
capture packages and source deliveries, but first two returned pairs miss the
10s developer wait and frame1784's returned color/depth are all zero. The helper
publishes all three then exits124 at shutdown watchdog; Flycast exits0/clean-close3.
Its public log contains a D3D9 invalid swapchain-handle warning; cause unproven.
Old/new Flycast logs append, so preserve nr-on-a archive and delimit rerun records
before any confirmation. No external image promoted. Renderer frame1782 maps
to producer1829/cycle7738677632 versus public baseline producer1781/cycle7605222912:
renderer-ID alignment alone does NOT establish identical game inputs. Added
rejection of the wholly zero color+depth pattern, preserving opaque black scenes.
Next exact producer alignment plus isolated concurrent-runtime failure, not an
unchanged retry or weaker provenance. Supplied config hash remains unchanged.

#524 2026-09-09 96c704fc6 plus working tree | Reconfigured automation to correct previously stale embedded58fba7087 SHA to96c704fc6; rebuilt and492/492 tests passed. Used existing unchanged hook-enabled stage; host positively reports EnableHooks=2 and tuple off/1/1/203/0/0/enabled. nr-on-a FAILS combined validation: source capture times out exit1, Remix consumer times out waiting for source exit2; three native-input marker Presents are1024/1024 but NOT combined proof. Cause: eligible capture skip1780 is delayed by neural menu bypass, unlike absolute evidence frame1782. Added explicit --start-frame renderer-ID threshold; default skip and conservative bypass unchanged. Four new reset/scheduling controls pass, automation496/496. nr-on-b rerun targets1782-1784 and is pending. Both runs use unchanged supplied text config, original hash222D059C727A683C8DEACE07C938002BDD6A69EB0DC9C908743E17A92B54EFBC verified after first run; only owned executables staged locally. No private binary inspection/config edit. Source, host and failure logs retained. Other configurations and final combined provenance remain pending.

LOG523 final metadata clarification rebuilt successfully in all four serial
incremental configurations; enabled selftests492/492 each. An early log read
found two not-yet-created test logs while the verified build process was live;
the same process later exited0. No restart or failure conclusion was inferred.
Actual GPU runs above preceded only that metadata/statistics clarification.

#523 2026-09-09 b19b17b34 plus working tree | Moved bounded source exchange before TrySubmit and added explicit FLYCAST_REMAKE_INPUT_TEST reset-only returned-scene submission. CPU conversion validates640x480 paired frame/producer/clips, swaps BGRA/RGBA without alpha/gamma edits and uses1-d inverted normalized projection depth, never relabeling it native PVR logarithmic depth. Five new controls pass; automation/baseline/no-NGX selftests492/492, all four serial incremental builds, SDK193/193, Python333/333 and backlog inspection pass before final metadata-only clarification. Actual input-a nativeD3D11 and input-on12-a D3D11On12 runs each exit0/clean-close3; each consumer exits0/63 Presents. On all six retained frames, GPU source color equals returned bytes, GPU depth equals1-returnedR32 exactly, motion/confidence/drawID are zero, bias is255, reset is true, evaluation accepted and public output exists. Final protected pixels equal native and other pixels equal public output exactly. Final nativeD3D11 capture visually reviewed: fighters, arena and HUD present. Existing40-object runtime cleanup warning remains. Retained failed commands: rg pattern beginning with-- parsed as option; two docs patch context mismatches rejected without changes; corrected them. Final metadata clears native correspondence statistics and appends diagnostic profile label; rebuild pending. Scope ACCEPTED for bounded Remix-to-publicDLAA input/evaluation/final-composition connection, NOT externalDLSS5, swapchain scanout, temporal quality, full geometry, ordinary async gameplay or performance. Next changed-route supplied-consumer provenance and accepted returned-surface motion/coverage, not another paired transport phase.

LOG522 regression completed: all four serial incremental configurations succeed,
enabled selftests487/487 each, SDK193/193 and Python333/333. Backlog consistency
inspection and diff whitespace check pass. These are not fresh exact-SHA builds.

#522 2026-09-09 a406db312 plus working tree | Channel version3 now publishes optional returned color/projection-depth atomically with matching source identity and camera clips. Truncated, NaN, out-of-range and wrong-clip depth reject before publishing color; color-only diagnostics remain supported. Automation build succeeds and487/487 selftests pass. Actual pair-a concurrent Soulcalibur replay completes three frames and clean close; consumer exits0 with63 Presents. New pair inspector verifies exact returned BGRA and extracted R32 bytes against each corresponding public consumer readback at1782/1783/1784, each307200 depth values and prepared_before_composite=true. Composite inspection reports zero protected/world mismatches and unchanged native targets on all three frames. Existing40-object runtime cleanup warning remains. Polling the earlier completed build handle returned unknown process; build/test logs confirmed completion, no restart was inferred from an observation timeout. Remaining serial configurations are running; no full regression claim yet. Projection conversion, compatible motion/masks, neural submission and normal Present remain pending. No performance or combinedDLSS5 acceptance.

#521 2026-09-09 641e99b1e plus working tree | Added --capture-d3d9-scene-memory-depth to capture public typedRGBA32F depth alongside same-frame returned final color. Files use CREATE_NEW, fixed640x480 rows, HRESULT and source frame/sequence; missing copy/readback fails diagnostic run. Actual depth-a source capture exits0/clean-close3 and consumer exits0/63 Presents; all three public depth copies hresult0. Every channel finite307200 pixels; R range approximately.96243..99999964, G/B0 and A1. Linear-view-distance hypothesis FAILS (median absolute mismatch~51.3, ratio~.019), retained first inspector output. Exact supplied projection from remake_d3d9_scene.h instead gives median normalized residual1.84e-6/2.81e-6/2.23e-6 over302372/302345/302360 emitted-opaque-ID-overlap pixels;99th percentiles.000136/.000513/.000262. Wrong reversed normalized interpretation has median error~.99631. These are analytic hypotheses, not an injected runtime mutation or full guidance acceptance; normalized far-depth compression/omitted surfaces remain caveats. New inspector retains channels, both interpretations and unproven status. Public web search for enum semantics returned no results; no third-party internals inspected. Four incremental builds and482/482 enabled selftests pass. A malformed read-only command used a word instead of numeric offset and failed without effects. Next explicit matched return-depth transport/conversion and neural submission, not another native polarity/transport phase. No Present, combinedDLSS5 or performance acceptance.

LOG520 final incremental verification: all four builds and enabled482/482
selftests pass after retaining the early estimated view through archival;
SDK193/193 passes. Actual est-a GPU evidence remains scoped to its working-tree
run; no fresh exact-clean-SHA or performance claim.

#520 2026-09-09 e60fe9463 plus working tree | Implemented opt-in mixed observed/projected-depth camera-relative embedding, not a false extension of observed transform authority. Default keeps untraced geometry excluded. Four new controls prove default exclusion, opt-in triangle coverage, explicit no-transform estimated position with reprojection, and rejection without current observed anchors. Actual est-a concurrent three-frame replay exits0/clean close; consumer exits0 with63 Presents and40 meshes on each frame1782/1783/1784. Serialized scope verified mixed-observed-and-projected-depth-estimate-not-world-reconstruction; candidate estimated vertex counts12960/12943/12974. Source bytes11757255 each, receipts/digests17488492577410055763/10299926284692884669/4613164358644813945. All returned compositions pass zero protected/unprotected mismatch and unchanged native/final targets. Final image viewed: BOTH fighters and arena floor now visible, with supplied lighting; missing translucent effects, background/HUD imperfections and physical/world uncertainty remain. External comparison.html loops the three native/returned endpoints and was queued for display; not real-time performance evidence. Four incremental builds and482/482 enabled selftests passed before a final getter-consistency fix retaining the prepared estimated view during archival; that final fix needs incremental verification. No full camera/world, actual returned Present, async pacing or combinedDLSS5 acceptance. This is meaningful visible-coverage progress, not goal completion.

#519 2026-09-09 1dc3d5cb5 plus working tree | Extracted ExchangeRemakePacket from archival and added PrepareRemakeBeforeComposite at displayFramebuffer entry on explicit native capture/channel route. Prepared result is retained by exact frame/epoch/ordinal/cycle; late archival neither resets nor republishes it. Receipt includes prepared_before_composite. Initial mechanical patch failed due to reverse hunk ordering and applied nothing; corrected patch/order retained, no build failure. Actual pre-a concurrent source/consumer run completes3 frames/clean close and63 consumer Presents, both exit0. All three1782/1783/1784 receipt JSON records prepared_before_composite=true and source digests7215357914537890766/15428173449637296790/17235634553982447852. Composite inspector passes all frames with0 protected/unprotected mismatch and native/final targets unchanged. Protected counts20802/21380/20713. Four serial incremental builds and three478/478 selftests pass. This proves returned ownership available before composition, not native replacement, async pacing, full scene, full HUD-layer separation or combinedDLSS5. Scope ACCEPTED for early diagnostic handoff; goal remains active. Next missing fighter/arena scene coverage plus actual returned routing, not more unchanged transport/HUD proof.

#518 2026-09-09 819dab76e plus working tree | Added mode2 only for classified normal SrcAlpha/InvSrcAlpha HUD reactive replay. Production neural shader discards final-alpha-zero samples, preserving earlier coverage; other blend/opaque/punch-through paths unchanged, mask output saturated. Actual alpha-a concurrent replay completes3 frames/clean close, consumer63 Presents/exit0. Protected counts20802/21380/20713 (previous21771/22451/21765), protected/unprotected mismatches0, same-frame native and final targets unchanged. Final image viewed: some zero-alpha background removed, timer/header rectangles REMAIN. Full alpha/layer-separated HUD acceptance not achieved. Four serial incremental builds, three478/478 selftests, SDK193/193 pass. Actual depth-contract and overlay-contract fixtures ran on both nativeD3D11 and D3D11On12 and exit0; these are regression checks, not a complete new blend-mode fixture. Prior opaque-rectangle captures retained as negative comparison. Native renderer fallback remains unchanged; returned Present, full scene, async pacing and combinedDLSS5 remain pending. Next pre-Present ownership and missing-scene integration alongside the bounded unresolved HUD issue, not repeated transport proof.

#517 2026-09-09 06756db0a plus working tree | Traced missing HUD to exact draw evidence: punch-through header/timer excluded by list2-only title profile; filled layers rejected at stability1/2 despite known atlas/layout. Added title-only captured texture/blend/list/region/depth rules independent of history for those UI identities, preserving generic classifier. Ten new positive/wrong-texture/region/depth/RTT/title controls; automation478/478 selftests pass. Actual hud-a three-frame concurrent source/Remix capture exits0/clean close; GPU composite checker passes protected counts21771/22451/21765, protected and unprotected mismatch0, same-frame native/final targets unchanged. No-overlay analytic substitution mismatches all protected pixels; native-only285429/284749/285435. Final image viewed: bars/names/timer/stage/credits now visible, BUT transparent HUD quads copy native-background rectangles. This is retained visible defect, not full overlay acceptance. Existing shader writes overlay mask independent of translucent alpha; next focused coverage correction, then pre-Present integration and missing fighters/arena. Four incremental builds, enabled478/478 selftests and SDK193/193 pass. No actual returned Present or combinedDLSS5 acceptance.

#516 2026-09-09 b79bd87c2 plus working tree | Added same-frame producer-qualified diagnostic returned-color upload and GPU composition using the existing DX11 overlay shader, not a CPU mock/new shader. Separate target/deferred command list, immediate-state restoration, native/OIT/RTT/framebuffer exclusions and explicit developer environment opt-in. composite-a actual native replay exits0/clean-close3, consumer exits0/63 Presents. Captured GPU result has zero protected and unprotected mismatches against native-mask/returned-color truth on1782/1783/1784. Protected counts1004/1659/1004; analytic no-overlay substitution mismatches1004/1659/1004 and native-only substitution mismatches306196/305541/306196. These are image substitution controls, not injected GPU shader mutations. Image viewed: incomplete temple and HUD outlines only, not full HUD/fighters/world acceptance. Four builds and468/468 enabled selftests pass. Cross-run native comparison to return-d FAILED9/8/0 pixels; retained failure, no native equality claim from that comparison. Added direct same-frame before/after native-color and final-target byte checks; composite-b verification pending. No Present, real-time pacing or combinedDLSS5 proof. Backlog remains active.

LOG516 continuation: composite-b completes both processes with exit0, native clean-close3 and consumer63 Presents. Inspector passes all three frames: protected/unprotected mismatches0/0 and direct same-frame native/final-target bytes unchanged. Analytic wrong-native-only mismatches306196/305540/306195; wrong-no-overlay1004/1659/1004. Final four serial incremental builds and468/468 enabled selftests pass; SDK193/193. ACCEPTED only for same-frame offscreen GPU composition with existing mask and native-target preservation. Full HUD mask, complete scene, pre-Present returned routing, asynchronous feed and combinedDLSS5 remain pending. The cross-run9/8/0 failure remains failed, not waived by the narrower same-frame test.

#515 2026-09-09 2a871ee9d plus working tree | Implemented reverse640x480 BGRA slot with exact source receipt/frame/producer, pixel integrity, bounded owned copy, duplicate/source/size controls, busy skip and orderly-close drain. Runtime public final-color readback publishes; Flycast polls/retains without changing native presentation/history. First build FAILED LNK2019 GenericLog in neuraltest; replaced diagnostic logging dependency with stderr and retained return-channel-build.log. Corrected four builds and three468/468 selftests plus SDK193/193 passed. return-a actual replay completed three source frames/63 Presents but FAILED return acceptance: first return published, two later busy, no retained Flycast receipt. Added explicit developer-only10-second per-frame wait. return-b exits0/clean close and consumer publishes all three frames1782/1783/1784, but harness does not retain stderr receipts; consumer publication alone is insufficient. Added per-frame remake-return.json after validated owned receipt; this latest archive change still needs build and actual rerun. All failed/partial evidence retained. No image presentation, asynchronous cadence, full coverage or combinedDLSS5 claim. Scope remains NOT_REVIEWABLE for end-to-end receipt until artifact rerun.

LOG515 continuation: return-c archived all three received pixel buffers, byte-identical to consumer BMP payloads, but receipt JSON FAILED parsing because the process locale inserted thousands separators. Retained malformed evidence and corrected the stream to classic locale. return-d then exits0 on both processes, native capture clean-close3 frames, consumer63 Presents. Three valid JSON receipts match source sequence/frame/digest and consumer return publication; all three1228800-byte BGRA buffers match consumer BMP pixel payloads by SHA256. Frames1782/1783/1784 pixel hashes E3F7540A2777433AAF488BA9157441979EC050D5F8F1ABD42372F5798F6B7503, FCA2CD49D8A22B91A57E8B66335403C54082E99A267CCA837B568673A3C4A631, FD350D8A6E19D035D38B3CA757AD2913785DDCB1A39FB53B6CB1FFDC90B62109. return-c final image viewed: partial temple, no fighters/full arena. Forty-object runtime teardown warning persists. Final four serial incremental builds and enabled468/468 selftests pass; SDK193/193, Python333/333 pass. Failed commands/builds/runs retained. ACCEPTED for bounded diagnostic round-trip pixels/ownership only; no Present, async cadence, full M2 or goal acceptance. Next move qualified return handling before existing protected composition; current capture hook executes after it.

#514 2026-09-09 58fba7087 plus working tree | Replaced the diagnostic saved-scene handoff with actual two-process shared-memory delivery. Extracted stream serialization from the existing bounded wire codec; no new scene format or private API. Added one-owner/one-publisher local channel with two72MiB slots, monotonic source/publication order, bounded payload/digest checks, exclusive slot ownership, immediate busy skip and closed-consumer fallback. Quality capture publishes its owned packet when the explicit channel environment token is present; disk archival is independent. The runtime --live-channel mode waits boundedly for live input, reads no saved scene packet and avoids activating its window over the emulator. Existing-output capture refusal also covers channel-generated frame suffixes. All four serial incremental builds pass; three enabled selftests457/457, SDK193/193, Python333/333. Fourteen channel tests cover missing/duplicate endpoints, invalid token, owned receipt/payload, FIFO reuse, busy/no-sequence-advance, duplicate source frame, failed serialization recovery and shutdown fallback. Physical process-kill/corruption injection and asynchronous production pacing are not claimed.

Actual fc067-channel-a-58fba7087 run starts the consumer before native Soulcalibur replay. Source capture fc067-live-channel-source-a completes3 frames/clean close; consumer exits0 with63 Presents and three public output readbacks. Automatically asserted exactly matching (sequence,frame,bytes,FNV64) tuples: (1,1782,5445798,7215357914537890766), (2,1783,5711510,15428173449637296790), (3,1784,5179366,17235634553982447852). Receiver producer ordinals1781/1782/1783 and saved_packets_read=false are logged. Viewed final fc067-live-channel-a.bmp.frame-1784.bmp: partial temple rendered, still missing fighters/full arena. Runtime40-common-device-object warning persists. Source and consumer logs retained outside Git; the process environment was restored after the run. No third-party binary/configuration or native presentation changes. ACCEPTED only for bounded live source-to-consumer delivery and corresponding standalone GPU output. Real-time pacing, ordinary-frame feed, return to Flycast, protected composition, full coverage and combinedDLSS5 remain open. Next returned-image ownership/asynchronous integration, not repetition of the now-verified source transmission.

#513 2026-09-09 f9b9e19db plus working tree | Implemented running-Flycast source-to-scene conversion, not another observation counter. Fully witnessed opaque draws become separate camera-relative positions/derived flat normals while original PVR attributes, draw identity and transform W remain retained. Title/lens/viewport assumptions are explicit; .01-pixel projection tolerance is diagnostic, strict arithmetic equality remains failed/parked. Input/reference/expanded-vertex bounds and missing/duplicate/changed-origin, wrong-frame/epoch/title/lens/sign/depth/viewport, unsupported translucent/Naomi2 and strip-break controls added. Shared scene types/validation moved into core; original test namespace remains a compatibility shim. Current live DX11 primary textures are generation-checked, decoded with existing format logic into owned RGBA DDS, and joined to shared packets without reading prepared scene files. Palette/A8 is explicitly unsupported in this direct path. Wire transport tests cover channel/ownership preservation, schema, truncation, trailing data, overwrite refusal and producer continuity. All four serial incremental builds pass; enabled selftests443/443 each, SDK193/193 and Python333/333 pass. These are working-tree/incremental results, not exact-clean-SHA proof.

Actual opt-in native Soulcalibur replay fc067-live-view-packet-a completes3 frames and clean close. New owned packets have20/21/19 meshes,924/952/890 triangles and270/269/272 omitted nonempty draws at1782/1783/1784; maximum accepted relation errors .00165489/.00304806/.00346516 pixels. Corresponding frame-qualified remake-view.bin contains geometry and current texture bytes; no old transform tape/prepared geometry was consumed. Native capture remains native. The existing standalone Remix tool consumed these three packets with63 Presents, rebuilding changed resources at1783/1784. Initial fixed-light final output was BLACK and is retained (live-view-remix-a); raster control shows the expected incomplete temple. Only the live-artifact diagnostic uses a camera-forward supplied headlight after the coordinate-system change: live-view-headlight-b exits0 with three readbacks; final image viewed and shows the partial temple. Fighters, full arena, continuous live delivery, returned Flycast image, HUD composite, temporal quality and combinedDLSS5 remain unproven. Runtime40-common-device-object warning persists. No performance claim, external configuration write or proprietary binary modification. Scope disposition: ACCEPTED for bounded live-derived packet assembly and saved-packet GPU handoff only; full M2/goal remains incomplete.

Retained failures: initial test compile selected neuraltest::Vertex instead of ::Vertex; corrected qualification. Shared-uploader compile needed an explicit iostream include. Several context-mismatched edit attempts made no changes and were corrected. A host-log copy initially used the runtime cwd with a repository-relative path; corrected to the exact absolute source and retained host.log. No failed result is relabeled as pass. Next bounded continuous delivery/returned-image connection plus missing fighter/arena correspondence, not another camera-fit or prepared-scene phase.

#512 2026-09-09 6779f24cc plus working tree, witness checkpoint cec465a25 | User integration correction: active card now M2-scene; obsolete runtime-unavailable entries corrected without accepting camera/world reconstruction or cleanup. Added bounded packet-owned DDS bytes to the existing D3D9 uploader, preserving source texture identity, mip layout and native path. Public path-only material adapter rejects memory textures before API calls. Draw-time file reads are absent in memory mode; diagnostic loader still reads the captured source once before runtime startup, so this is NOT live gameplay submission. First actual memory run exits2 before runtime with byte-limit: geometry-only8MiB budget was incorrectly used for texture payloads. Retained owned-texture-raster-memory.log; separate aggregate texture bound64MiB added, geometry limit unchanged. Tests cover independent producer/packet mutation, malformed format, truncated/trailing bytes, ambiguous sources, aggregate payload bounds and path-only API rejection. Four serial incremental builds succeed; three enabled selftests415/415, SDK193/193, Python333/333, backlog contract and diff checks pass. Post-cec465a25 four incremental builds and three enabled selftests also run successfully with the memory changes still explicitly uncommitted; not a fresh exact-SHA build.

Actual63-frame memory raster, file raster, and memory Remix runs all exit0 (owned-texture-*-b.log). Exact RGBA comparison at1782/1783/1784:0 differing pixels and max channel delta0 for file versus memory raster. Memory Remix final1784 visually inspected: both fighters, temple, textured floor visible. Three returned captures per run retained as fc067-owned-texture-*-b.bmp.frame-*.bmp outside Git. Existing40-common-device-object warning persists; no cleanup/performance/temporal or combinedDLSS5 acceptance. ACCEPTED only for the owned-memory compatibility texture slice. Next actual supported source conversion/live submission and variable draw/resource handling, then returned-image ownership, protected overlays and externalDLSS5 chaining. No capture-only result closes the standing goal. Earlier mistaken diagnostic source-path lookup failed harmlessly; no external configuration or proprietary binary changes.

#511 2026-09-09 6779f24cc plus working tree | Reused saved source witnesses (no new gameplay run) to test fixed float32 reciprocal/multiply and direct-division candidates and inspect actual matrix rows by FTRV PC. Both fixed arithmetic candidates FAIL bit-exact reconstruction; maximum XY residual0.00390625 pixels, depth2.3841858e-7. Original fitted relation stays empirical, not substituted for execution truth. Observed group8c03a9ea has2965 retained transforms across3 captures, median row-norm ratios614.714447081469/565.5372405666603, maximum normalized-row orthogonality error1.7163e-9. Under explicitly stated orthogonal/uniform-scale basis assumption, these imply vertical FOV45.9903307484 degrees and projection aspect1.22666655659 at640x480, consistent with the earlier offline diagnostic camera. W ranges0..1, so never force homogeneous W to1. Matrix-vector comparison uses double arithmetic and is not native arithmetic parity. Added/run2 witness-bit decoder tests. Probe report retained under ignored automation output. Next validate the covered subset's coordinate/calibration interpretation and exact executed derivations, then use the measured calibration for a labeled supported live-scene experiment; no world-space/camera or full-game acceptance.

LOG510 numeric correction after reviewing the full report: maximum validation
XY residual is0.001697714 pixels in frame1784, approximately0.00170;0.00133
describes frame1783 only. Divide-by-W control RMS XY is hundreds/thousands of
pixels across all3 frames. No acceptance threshold was relaxed.

#510 2026-09-09 6779f24cc plus working tree | Added explicit pvr-source-witness.json alongside bounded developer captures: exact projected XYZ bits, component origin serials, unique FTRV input/matrix/output bits, frame/game/Git/producer identity and observed-dependency-only scope. Automation build404 tests pass, including serialization/deduplication and missing identity rejection. Actual fc067-source-witness-a completes3 captures/clean close; host log retained. Added/run source_witness_inspect.py: fit first-frame coordinate relations and test later frames plus deliberately shifted source-row control. Divide-by-Z model yields approximately x=X/Z+320,y=Y/Z+240,depth=0.95/Z; validation1783/1784 RMS XY about0.00010/0.000128 pixels or less, maxXY about0.00133 pixels. Wrong-row RMS hundreds/thousands of pixels. This is empirical relation evidence, not bit-exact operation reconstruction or intrinsic/extrinsic camera acceptance; report explicitly reconstruction_accepted=false. Full relation report retained in ignored automation evidence. Next validate exact arithmetic derivations and matrix/calibration semantics against this concrete candidate, then construct supported live scene submission. No third-party binary/config changes, no full regression yet for witness export.

#509 2026-09-09 d927a395e plus working tree | Added diagnostic common-origin coverage: all XYZ payloads must agree on executed transform serial/PC/input/matrix/output; all referenced vertices must satisfy it for a complete draw. Reject duplicated source vertex records and guard indexed/strip ranges. Common-origin and mixed-origin tests pass. Actual fc067-transform-draw-coverage-a completes3 captures/clean close: frames1782/1783/1784 have2508/2525/2497 complete common-origin vertices,20/21/19 complete draws and4/3/5 partial draws. Host log retained. Automation/NGX/no-NGX/feature-off serial builds pass, enabled selftests402/402 each, Python331/331 and SDK179/179 (no runtime/GPU claim). This establishes a bounded observed dependency subset, not camera semantics, full evaluated derivation retention, world geometry or Remix presentation. D-147 defines next acceptance. Next identify/export the covered draw subset's owned transform and arithmetic evidence, validate coordinate/camera relation against live projected vertices, then feed supported geometry to Remix with native fallback for omissions. Do not substitute further count-only experiments for that connection. Incremental working-source evidence, not freshly restamped exact-SHA capture.

#508 2026-09-09 d927a395e plus working tree | Replaced blanket block-entry clearing with actual architectural-register bit validation and reset-epoch refresh; extended supported scalar/move tracking and destination invalidation through non-FTRV blocks. Clear on interpreter fallback, SR/FPSCR synchronization and floating-register bank swaps. Added retained-entry/changed-register tests; automation400 selftests pass. Actual fc067-cross-block-origin-a completes3 captures/clean close and now retains7533/7581/7498 owned transform-dependent XYZ COMPONENTS at1782/1783/1784, versus prior zero. This is the first captured cross-block dependency coverage; not complete-vertex counts, camera semantics or geometry reconstruction acceptance. Capture's old 'direct-only' suffix was stale; changed diagnostic wording afterward to observed-dependency-only. Also added conservative MMU-entry invalidation afterward; rebuilt automation400 tests, but that MMU path was not exercised by this non-MMU capture. Host log retained. Next quantify complete XYZ triples/common transform identity and actual draw coverage, retain/evaluate complete arithmetic derivations rather than only original transform payloads, and reject unsupported mutation paths. Full configuration regression pending; no Remix activation or performance/parity claim.

#507 2026-09-09 d927a395e plus working tree | Added bounded origin-loss diagnostics at block entry and unsupported register destinations; exact live-tag accounting avoids scanning/copying all tags each block. Automation build398 selftests pass. fc067-origin-boundaries-a completes3 captures but64 report slots fill with startup ordinary read/constant overwrites; that limitation is retained. Logging-only filter excludes readm/mov32 from quota without changing invalidation. Rebuilt fc067-origin-boundaries-b completes3 captures/clean close; actual game path reports4 live tags discarded at8c03c94c,8c03c95c and8c03c984, aligning with the historical8c03c93a/FTRV8c03c944 path. Also observed fipr/fsca/frswap destination losses (opcode names resolved from current SHIL enum order); do not silently propagate across bank swaps. Both host logs retained, including append-only history; new20.880s game-path records are distinct from prior startup lines. Next implement cross-block handoff validated against actual entry register bits, with overwrite/fallback/bank-change invalidation in every traversed block. This identifies a concrete blocker in the diagnostic scope, not recovered camera or complete transform correspondence. Full regression pending.

#506 2026-09-09 d927a395e plus working tree | Connected FTRV-origin register tags through exact scalar arithmetic and register moves in FTRV-containing blocks; reject conflicting transform serials, stale operand values and unsupported destination writes; attach owned tags to observed RAM stores. Added multiply-origin/mixed-origin/overwrite tests; automation398 selftests pass. Initial fc067-arithmetic-lineage-a timed out at90s without accepted captures; append-only log extraction printed older zero-component capture lines, explicitly excluded. Replaced per-block full register-tag clearing with epoch invalidation, rebuilt398 tests, and ran fc067-arithmetic-lineage-epoch-a:3 captures/clean close, but ZERO captured XYZ transform components in all3 frames. Host log retained. Same-block arithmetic scope is therefore insufficient, not a successful geometry link. Next inspect cross-block producer dataflow and unsupported operations, including which boundary drops the actual observed producer's origin; do not repeat unchanged same-block captures or weaken origin checks. Original expression history ownership and complete mutation coverage remain pending, as do full build regression and camera/Remix acceptance.

#505 2026-09-09 d927a395e plus working tree | Added opt-in scalar arithmetic observations only in compiled blocks containing FTRV: actual pre-operation lhs/rhs bits and emitted PC/register-layout metadata, actual post-operation result bits, exact add/subtract/multiply/divide verification with finite/nonsingular constraints, rolling4096 records. Game arithmetic remains original; verifier re-evaluates diagnostic result after execution. Automation build395 selftests pass, including wrong-result-bit and singular-division rejection. Soulcalibur fc067-transform-arithmetic-a completes3 captures/clean close; producer1788 reports20550637 observed operations and0 exact-result rejections. Host log retained. This proves scoped operand/result observation, not transform dependency propagation, complete frame retention or camera truth. Next carry explicit transform identities through verified operations with input-value/register-definition checks, reject mixed/unsupported origins, then attach owned derivation to RAM stores and TA vertices. Stop operation-count-only iterations. Full configuration regression pending; no performance/parity/Remix presentation claim.

#504 2026-09-09 d927a395e plus working tree | Fixed missing ownership forwarding across actual direct RAM-to-RAM stores using the existing same-block read-slot proof, exact observed read value and destination writer/address/value check. Added second-address ownership and wrong-writer/value tests; automation build392 selftests pass. Actual fc067-transform-ram-copy-a completes3 captures/clean close, but XYZ transform components remain ZERO at1782/1783/1784. Host log retained. Therefore direct RAM-copy propagation alone does not close rendered geometry lineage; no correspondence promotion. Next record/evaluate the intervening SHIL arithmetic operands/results (initial scalar add/subtract/multiply/divide path), retaining executed FTRV identity and rejecting unsupported/cross-block dependencies. Stop direct-only replay iterations. Full regression pending; current changes uncommitted.

#503 2026-09-09 d927a395e plus working tree | Added owned optional transform copies to RAM-writer candidates, executed register reads, queue words and child-qualified TA vertex records. Partial SQ writes/consumption clear word transforms; mismatched XYZ writer bytes discard component transforms. RAM invalidation revokes future lookup while existing owned copies survive. Automation build390 selftests pass including ring-eviction ownership and invalidated-lookup controls. Actual fc067-owned-vertex-transform-a completes3 captures/clean close but reports ZERO direct transform components in captured XYZ for all3 frames. This falsifies the direct-only route as sufficient for rendered geometry; do not portray intermediate direct-store counts as vertex correspondence. Host log retained. Next priority is explicit intervening projection/arithmetic and RAM-copy dataflow, with evaluated expression/operand evidence and fail-closed unsupported cases. Do not repeat direct-only captures or expand lifetime scaffolding as a substitute. Full configuration regression pending; no camera or Remix scene activation claim.

#502 2026-09-09 d927a395e plus working tree | Added direct FTRV-result-to-RAM-store correspondence: same-block reaching definition must be the executed FTRV component with no intervening overwrite/fallback. Emitted invocation slot records actual transform serial; RAM store requires live exact-serial lookup and bit-identical component output before tagging its writer record. No recency/value-only match. Direct-definition/overwrite and nonunit-W/ring-eviction tests pass; automation388 selftests. Actual fc067-direct-transform-store-a completes3 captures/clean close; cumulative accepted direct component stores reach7969428/7977185/7984978 at producers1785/1786/1787. Counts are store events, not unique rendered vertices or camera truth. Follow-up serial-exhaustion/allocation-failure hardening clears the invocation slot unless retention actually advances; rebuilt automation/selftests pass after capture (normal capture did not exercise that failure branch). Host log retained. The writer's transform serial is not yet propagated with owned transform data through RAM reads to captured vertices; ring eviction must never be hidden. Next retain matched transform records across that handoff and prove intervening projection arithmetic separately. Full configuration regression pending; current changes uncommitted. Prior checkpoint d927a395e pushed/remote verified.

#501 2026-09-09 3f2555857 plus working tree | Completed accumulated source-hook regression: automation/NGX/no-NGX/feature-off builds link, all3 enabled selftests384/384, Python331/331 and public SDK contract179/179 pass (SDK runtime/GPU/presentation false). Compared fc067-live-ram-producer-a against fc067-live-transform-a: all3 pvr-scene.json files byte-identical; native PNG differences are1/9/0 pixels at frames1782/1783/1784, max channel delta1. Exact-pixel wrapper comparison therefore FAILED/parked, not passed via tolerance, and not a new major parity phase. No geometry packet changes observed in that bounded comparison. D-146 records diagnostic-only acceptance and outstanding transform-to-store correspondence. Current capture binaries remain incremental working-source builds, not newly restamped exact-SHA evidence. Next connect actual transform dataflow/ownership; no combined scene, camera or presentation claim.

#500 2026-09-09 3f2555857 plus working tree | Invalidated RAM-writer candidates at DMA, pointer-block and SQ-block copy entry points. Added opt-in x64 canonical FTRV wrapper that snapshots actual4-component input and16-component matrix, calls the original canonical function pointer without replacing its arithmetic, then retains actual4-component output in a bounded4096-entry producer-thread ring. Invocation PC is emitted from the current compiled operation; no fixed title/frame/PC windows. Automation build384 selftests pass. Soulcalibur fc067-live-transform-a completes3 captures/clean close; producer log proves executed observations (late PC8c03a9b0, monotonic serials reaching45353651), and observed RAM-producer coverage remains14692/14655/14724. Host log retained. Ring is rolling, not a complete frame transform set; observations are not yet linked to RAM writes or copied into the scene snapshot. No recovered camera/world semantics, all-path mutation coverage, wrapper pixel parity, or combined Remix presentation claim. Next attach executed transform provenance through the producer's intervening arithmetic/store dataflow, retain needed records by ownership rather than assuming latest transform or matching floats, and test wrong-transform/reset cases. Full configuration regression pending for current hooks.

#499 2026-09-09 3f2555857 plus working tree | Extracted DirectSourceRead so the actual JIT selection is exercised by same-block unrelated/scalar-overwrite/vector-overlap/secondary-destination/interpreter-boundary tests;381 selftests pass. Added opt-in bounded16384-slot physical-RAM writer observation cache with exact-address/value lookup, byte/halfword invalidation, collision replacement and reset/fallback clearing. Actual executed RAM writes feed captured read provenance, then SQ bytes, decoded vertices and owned snapshots. No ordinary-mode cache allocation. Automation rebuild384 selftests pass (alias, partial write, collision negatives). Soulcalibur fc067-live-ram-producer-a completes3 captures/clean close, retaining complete observed XYZ RAM-producer PCs for14692/14655/14724 vertices at frames1782/1783/1784 respectively; missing coverage stays unknown. Host log retained. These are observed JIT-store candidates, not proof all possible RAM mutation mechanisms are tracked; DMA/HLE/uninstrumented writes, lifetime and transform semantics remain unproven. Next bind these actual producer instructions to executed transform inputs/results with generation/invalidation evidence, rather than repeating the downstream counts. Full new configuration regression pending. No native parity/performance/Remix presentation claim.

#498 2026-09-09 3f2555857 plus working tree | Prior store-reset matrix terminal success: NGX/no-NGX374 selftests and feature-off link pass. Added opt-in same-block direct read-to-store analysis: scan backwards to first overlapping destination (rd/rd2), accept only32-bit memory read feeding the store source register, stop at interpreter fallback, bound read slots256; no PC-offset/value-only selection. Selected reads capture effective address before execution and actual register value after; SQ callback attaches RAM byte addresses/read PC only on matching observed value. No ordinary-mode descriptor allocation. Automation build376 selftests pass, including byte-address retention and wrong-value rejection. Soulcalibur fc067-live-ram-read-a completes3 captures/clean close with14764 complete XYZ RAM-read links per frame1782/1783/1784; host log retained. This is live downstream read lineage, not provenance of the earlier writes that produced RAM values or camera/transform semantics. Compile-analysis intervening-write/alias negatives and new full configuration regression remain pending; do not promote this to complete lineage acceptance. Next exercise those focused rejection controls, then connect actual RAM producer writes/transform execution without fixed-frame gates.

#497 2026-09-09 3f2555857 plus working tree | Added reset-epoch invalidation refreshed by producer TLS and conservative writer invalidation before x64 interpreter fallback. Automation build and374 selftests pass, including reset-before-submission and unsupported-path clearing. Bounded gameplay fc067-live-sq-stores-reset-a completes3 captures/clean close and retains14764 fully witnessed XYZ-store vertices per frame. Capture lists9 observed PCs: three each in cc82/cc84/cc86, ccc8/ccca/cccc and ab5c/ab5e/ab60 groups (8c03 prefix); matching historical owned-source header identifies preceding read groups cc7c..cc80, ccc2..ccc6 and ab56..ab5a. This is an evidence-led next seam, not permission to assume pc-minus6 is dataflow identity. Next link actual read destination/source register lineage to stores, retaining exact observed values and rejecting intervening writes; remove historical fixed-frame gates. NGX/no-NGX/feature-off serial regression is running, not yet claimed passed. Host log retained; no external writes beyond owned evidence. Camera/transform semantics remain unproven.

#496 2026-09-09 3f2555857 plus working tree | Added opt-in post-shop_writem non-MMU x64 SQ-store observation with actual effective address, executed PC, size and source value. Fixed64-byte queue-local writer metadata carries one PC/value per byte; actual TA copy compares XYZ bytes before retaining PCs and PREF completion expires the selected queue. Automation rebuild/selftest372/372 pass, including byte lanes/queue selection and consumed-queue expiration. Actual Soulcalibur fc067-live-sq-stores-a completes3 captures/clean close; all14764 joined vertices in each frame1782/1783/1784 carry all12 witnessed XYZ-byte store PCs in the owned snapshot. Host log retained. This demonstrates observed store-to-submission-to-vertex handoff, not authoritative upstream RAM reads/transform lifetime or camera. Interpreter fallback/reset invalidation needs hardening before stronger ownership claims; full configuration regression pending. A malformed numeric Select-Object argument failed read-only and was corrected. No performance or exact native parity claim. Prior checkpoint3f2555857 was pushed and remote SHA verified; current store changes are uncommitted.

#495 2026-09-09 5576059d4 plus working tree | Observer/snapshot checkpoint regression completes: automation, NGX, no-NGX and feature-off builds link serially; all3 enabled selftests370/370, public SDK contract179/179 (no runtime/GPU/presentation claim), Python331/331 and backlog consistency check pass. Diff whitespace check passes. Actual live snapshot evidence remains LOG494; exact native observer-on parity remains failed/parked LOG491. ACCEPTED only as opt-in diagnostic submission/vertex ownership handoff, not production activation or transform/camera recovery. No external consumer changes. Next executed SQ-store/RAM lineage integration; avoid repeating downstream snapshot proof.

#494 2026-09-09 5576059d4 plus working tree | Added bounded child-qualified source joins owned by rend_context and copied into the optional live PVR snapshot with producer identity. Copy validates vertex range and exact submitted/decoded XYZ; wrong vertex rejects atomically, and source retirement leaves snapshot intact. Automation build/selftest370/370 pass. Soulcalibur fc067-owned-source-snapshot-a completes3 captures/clean close, each retaining14764 joins: presentation1782/1783/1784 maps to producer1781/1782/1783. Host log copied into evidence folder. This proves actual live capture handoff, not upstream transforms, world camera or Remix presentation. NGX/no-NGX/feature-off serial regression launched; results pending. Historical ancestry patch identifies post-shop_writem SQ writer instrumentation as next upstream connection; do not import its hardcoded frame/address windows wholesale. Existing native pixel discrepancy remains parked. No third-party configuration or binary edits.

#493 2026-09-09 5576059d4 plus working tree | Connected child-local sealed SQ records to actual polygon vertex decoding through exact producer identity, TA offset and32-byte comparison, retaining decoded vertex index and rejecting duplicate consumers. Automation rebuild and367 selftests pass. Actual bounded Soulcalibur run fc067-live-vertex-join-c completes3 captures/clean close; retained host log reports14806 copies and14764 joined polygon vertices in late gameplay producers. Producer ordinals are not presentation frame IDs. Unknown upstream transform remains explicit; sprite/background/DMA/unsupported domains are not promoted. Failed attempts: initial logging code mistook rend_context for TA_context and compilation failed; an accidentally launched old executable produced run-a and366 tests, excluded from new mapping evidence. Corrected logging build/run-b completed but mapped zero vertices: typed vertex begins after4-byte PCW. Adjusting packet base to include PCW yielded run-c matches; failed logs/captures retained. Earlier path search and malformed numeric Select-Object command failed without edits. No synchronous performance claim, camera recovery or live Remix acceptance. Source observation snapshot handoff, upstream transform hookup and full build matrix remain next.

#492 2026-09-09 5576059d4 plus working tree | Both pending source-material off/on captures complete3 frames with clean close (fc067-sq-material-off/on). Per user reprioritization, the reproducible8-pixel/max1 native discrepancy stays failed/parked rather than blocking live integration. Connected executed non-MMU x64 SQ scopes to bounded child-local TA copy records, retaining actual submission PC/address, invocation serial, cycle, TA offset and compared source/destination words. Queue publication seals each child against the actual root producer stamp; unknown/DMA submissions remain unattributed. Allocation failure discards observation authority without aborting emulated copy. Automation full build exits0. This is real submission-to-TA collection, not upstream RAM-generation, decoded-vertex correspondence, camera recovery or live Remix output. Those remain next. Material bytes/global comparison deferred; no claim that follow-up capture completion proves equality. Full configuration matrix and actual populated-record capture remain pending; no commit or presentation acceptance yet.

#491 2026-09-09 5576059d4 plus working tree | Same executable SHA25670575ADBB69410BD7781244B61A0742F3213825BA0F03F19E274B361EED906BB off/on/repeat-off captures each complete3 frames/clean close, retained host logs under fc067-sq-matched-{off,on,repeat}; process env restored. All PVR packet JSON values equal across1782/1783/1784 and all variants. Off/repeat-off native pixels exact3/3; off/on exact1782/1784 but1783 differs8 pixels,maxchannel1, reproducing prior mismatch. Thus observer-enabled exact native pixel parity remains FAILED, not dismissed as generic run noise. Source packet equality narrows cause but does not cover all renderer/global state. Next inspect affected channels/regions and shader/global-state or FP/call-boundary effects, retaining zero tolerance; no TA attribution expansion until explained. Binary still stamps prior configure base8c5cd655e; comparisons explicitly same binary/working source, not clean557 exact-SHA. No performance claim from captures.

#490 2026-09-09 5576059d4 plus working tree | Fixed deeper nesting: explicit thread-local active flag suppresses all nested scopes even while current record is empty. Added third-level nesting, serial exhaustion and exception-unwind checks; automation build/selftest366/366 pass. Added one-shot invocation log to opt-in wrapper. Actual enabled native Soulcalibur3-frame replay completes/clean closes (fc067-source-sq-enabled); retained host log proves wrapper invoked pc8c22500a,address e0000000. Compared native pixels to earlier observer-off restamped baseline:1782 and1784 byte-exact,1783 differs. Therefore full native parity NOT_ACCEPTED; must isolate with same-build off/on/repeat and exact packet comparisons before TA append. Process-local environment restored in finally; no config-file changes. Source scope is SQ invocation only, not RAM lifetime/transform. All failed/falsifying output retained. LOG489 selftests363/363 passed before nesting correction.

#489 2026-09-09 5576059d4 plus working tree | Added opt-in x64 non-MMU shop_pref wrapper preserving original ctx->doSqWrite arguments/call, with thread-local scoped observed instruction PC, SQ address and monotonic invocation serial. Gated by feature plus explicit process environment FLYCAST_NEURAL_SOURCE_OBSERVATION=1 at JIT compile time; default emitter unchanged. Serial is explicitly NOT RAM content generation or recovered transform. No TA record append yet, no title/camera interpretation. Automation full build passes; scoped PC/address/nested suppression/restoration/exit tests added, selftest result follows. MMU/interpreter/non-x64 paths remain unsupported for this observer. Need deeper reentrancy/overflow checks, actual opt-in native parity and TA join before using as lineage evidence. LOG488 automation full build and359 selftests passed.

#488 2026-09-09 5576059d4 plus working tree | Inspected ta_thd_data32_i: copies32 bytes into current ta_tad before parser transition, but arguments do not establish upstream writer PC/RAM transform provenance. Added feature-gated optional unique-owned SourceObservationBatch to each TA_context, cleared by Reset; no default allocation or synthetic observations. This is lifetime attachment only, not emitter hookup. SourceCopyObservation still requires actually observed generation/writer, so do not fabricate them from queue stamp or projected output. Automation full rebuild/selftest terminal result follows; feature-off and explicit owner-reset test remain pending. LOG487 automation selftest completed359/359. Next x64/SQ provenance-bearing observer and matching context-local copy append, preserving original arithmetic and incomplete fallback.

#487 2026-09-09 5576059d4 plus working tree | Current ta_ctx.cpp assigns captureProducer at queue submission, after TA copy collection, while TA_context reset clears producer identity/child chain. Corrected observation API to BeginContext(generation) without inventing a future producer stamp, then Seal(actual queue identity,count), rejecting copies newer than queue cycle. Existing stamped fixture helper retained. Added collection-before-publication and future-copy negative tests; automation target/selftest terminal result follows. Still no live hook/record emitter or child-chain aggregation. Each batch must remain context-local; global offset ordering is not valid across linked children. LOG486 automation selftest completed357/357.

#486 2026-09-09 5576059d4 plus working tree | Began extraction with owned SourceObservationBatch: one producer identity, max16384 SQ-copy records, exact before/after words, ordered cycle/TA offsets, generation/writer presence, fail-closed duplicate/alteration/incomplete publication and frame/epoch-qualified lookup. Added five selftest controls. This header is transport state only and is not hooked into TA/x64; it does not emit observations, reconstruct transforms or establish camera truth. Prior patch inspection shows actual CameraSourceCopy lives with TA context and clears on reset; next attach a suitable per-context observer, retaining actual child/context offsets and witnessed producer/source mapping rather than assuming global offsets or fixed vertex lists. No emulated arithmetic or production render activation changed. Automation target/test result follows terminal verification.

#485 2026-09-09 5576059d4 live transform hook audit | Current tracked core has no compact-consumer/ledger provider hook; harness transport types alone do not emit live records. Located retained ignored fc067-ancestry-g.patch and source-probe headers, plus copy-consumer-h/large-ledger patches. Ancestry patch crosses TAWriteSQ/pvr_mem, ta.cpp, ta_ctx, ta_vtx, SH4 interpreter/memory/store queue and rec-x64. Explicit gates fc067CsNextOrdinal1781..1783, cameraPacketFrames<3, descriptor initial-frame emission and fixed probe completion show these are temporary evidence instrumentation, not a reusable live provider. x64 producer descriptors and SQ-to-TA source-copy ownership are the concrete prior handoff points to reuse. Do not apply whole temporary patch or claim harness ConsumerTape constitutes live capture. Next extract bounded per-frame source observation with producer identity/reset/overflow, initially opt-in x64/title-scoped; preserve original shader/math and fallback on incomplete correspondence. Initial broad core search included dependency files and was slow; subsequent source-scoped searches located exact retained patches without modifying them.

#484 2026-09-09 8c5cd655e snapshot checkpoint | Four serial incremental builds pass (snapshot-checkpoint.log each), Python331/331, SDK179/179, enabled automation/NGX/no-NGX selftests352/352. D-144 records owned capture-boundary scope. Actual game/accessor/generation controls LOG481-483 and diagnostic color marker LOG474 retained. ACCEPTED for bounded projected snapshot and observed current binding checks only; not async ownership, camera truth, general palette retirement, live Remix or combinedDLSS5. Commit owned source/docs only; stale version evidence LOG480 remains rejected and working-tree actual captures remain explicitly labeled. Next witnessed live transform/camera provider instead of another unchanged snapshot proof.

#483 2026-09-09 8c5cd655e plus working tree | Extended existing bounded capture-only snapshot check with altered draw-metadata copies: flip one upload-generation bit then restore it and flip one RTT-generation bit. Live texture/cache and snapshot remain unchanged. Automation build and selftest352/352 pass. Actual3-frame Soulcalibur native replay fc067-live-snapshot-negative completes/clean closes. Retained snapshot-host.log, explicitly asserted exactly3 verified rows for1782/1783/1784: bindings verified, wrong-frame/upload/RTT all rejected. Positive/negative runtime evidence scoped to current cache metadata; not paletted-generation mutation, GPU-resource lifetime after retirement or async consumer synchronization. No external configuration, binary or game-media mutation. Next full snapshot regression/checkpoint and live transform/camera provider; do not repeat unchanged three-frame binding proof.

#482 2026-09-09 8c5cd655e plus working tree | Added same-render-thread PvrSnapshotTextureBindingsMatch for list/ordinal, both TCW identities, upload/RTT/palette metadata and cleared raw pointers. DX11 capture-only post-success check accesses exact frame, rejects frame+1 and compares real cache bindings. Automation full build passes. Actual3-frame native Soulcalibur replay fc067-live-snapshot-bindings completes/clean closes; retained snapshot-host.log reports frame1782/1783/1784 bindings=verified wrong-frame-rejected=yes. Positive actual cache/returned accessor evidence, not simulated generation mutation/lifetime retirement or async ownership proof. These diagnostic log checks are not a fail-closed automated gate yet; deliberately corrupted-generation controls and broader tests remain required. No normal gameplay or Remix activation. Extra recursive log listing was unnecessarily broad and truncated; exact build-neural-automation/flycast.log provided authoritative retained lines.

#481 2026-09-09 8c5cd655e plus uncommitted snapshot/marker changes | Reconfigured automation build (LOG480) rerun native D3D11 Soulcalibur capture3,skip1780,input-replay yes,remake-packet yes,480 raster. fc067-live-snapshot-restamped completes frames1782/1783/1784 and clean_close=yes; completion and packets correctly stamp base8c5cd655e. This is working-tree evidence, not exact-clean-SHA. Packet counts vertices15967/15967/15970,draws3347/3347/3348,texture-bound draws3345/3345/3346,zero nonfinite index references; pvr-projected and camera unknown retained. Successful production capture executes the new snapshot path but accessor contents/generation lifetime still need direct tests. Viewed native1784: fighters, temple, HUD, shadows and bright weapon effect visible, supporting D-143 requirement to preserve native authored effects rather than classify all apparent trails as defects. No neural/Remix output in this native run, no performance measurement. Original stale-stamp run retained unchanged. Next direct generation/accessor validation and complete snapshot checkpoint; live camera/scene translation still required.

#480 2026-09-09 8c5cd655e plus working tree | Actual native D3D11 Soulcalibur input-replay capture with3 retained frames,skip1781,remake-packet yes,480 raster completes and closes cleanly. External fc067-live-snapshot-first contains1783/1784/1785 (not expected1782 start), packet/source/depth/motion outputs and completion marker. Provenance falsification: completion metadata says c28fc95c2, while executable newly linked from current working source in pvr-snapshot-publication.log. Inspected core/version.h confirms stale configure-time GIT_HASH; CMakeLists configure_file stamps only on configuration. This run is provisional, NOT exact-SHA snapshot-hook acceptance. Reconfigure automation and rebuild before rerun; retained failed provenance unchanged. No proprietary binary/config changes. Accessor success still needs explicit observation; capture success alone is not a live Remix claim.

#479 2026-09-09 8c5cd655e plus working tree | Publication review found pvrSnapshot_ moved before final capture-complete marker write could fail. Moved publication after that final failure point. Added initially-unavailable and missing-texture capture rejection checks for requested/other frame access; automation full build and selftest352/352 pass (pvr-snapshot-publication.log). Tests do not exercise successful GPU capture or force completion-marker disk failure, so those lifecycle cases remain pending. No previous successful snapshot is falsely claimed tested here. Existing capture code increments other counters before marker failure; not changed as unrelated scope. Next actual bounded Soulcalibur capture and generation/lifetime fixture, then full configuration regression. Windows wildcard rg again rejected literal glob; exact source main.cpp identifies launcher rend.NeuralCapturePvrPacket assignment.

#478 2026-09-09 8c5cd655e plus working tree | Connected owned snapshot to existing opt-in bounded QualityCaptureWriter path. Snapshot remains pending until successful capture, published once by move, cleared on next Capture/configuration change; frame-qualified render-thread-only accessor rejects other frame IDs. No new ordinary-gameplay activation, external runtime or async consumer. Full automation build first failed std::max Windows macro expansion after header dependency; corrected parenthesized call, retry build and selftest350/350 pass. One initial patch targeted unrelated nonexistent line and failed without changes. Actual game capture/accessor lifecycle and texture-generation fixtures remain unrun; do not label compiled hook as live-runtime acceptance. Broader four-build regression pending. No texture pixels or raw cache pointers retained; projected coordinate and omission labels preserved.

#477 2026-09-09 8c5cd655e plus working tree | Added five snapshot controls: framebuffer bound, referenced nonfinite position, invalid pass coverage, zero frame all reject without replacing output; sorted translucent source-vertex ranges and separate GPU sorted indices retain their meanings. Automation neuraltest target rebuild and selftest350/350 pass (pvr-snapshot-bounds.log). No new live hook yet. Dedicated texture-cache lifetime/generation fixture remains pending; do not infer its acceptance from no-texture tests. Located owned capture seam at DX11Renderer capture setup around textures.pvrContext assignment, gated by existing explicit NeuralCapturePvrPacket and bounded capture frames. Initial rg wildcard paths were invalid on Windows; direct files used for authoritative inspection. Full four-build/runtime regression remains pending for snapshot changes.

#476 2026-09-09 8c5cd655e plus working tree | Implemented SnapshotPvrScenePacket as bounded owned in-memory projected packet, sharing context/range validation with existing disk writer. Copies vertex/index/pass/sorted metadata and texture generations, nulls raw texture pointers, retains unknown camera/material omissions, rejects RTT/bounds/referenced invalid positions; caller must hold source/cache ownership during copy. No frame-submit hook or live camera reconstruction yet. Initial automation neuraltest target builds; added four controls for owned copy, source mutation, RTT rejection and invalid index preserving prior output; rebuilt and ran automation selftest345/345. Existing writer/decoder tests also run in that suite, no exact-source full-build matrix yet. Full texture-generation/palette, sorted/nonfinite and bound tests plus actual snapshot hook follow. Changes uncommitted, not live Remix gameplay or performance evidence. Initial wildcard rg syntax on Windows failed and was replaced by directory search.

#475 2026-09-09 8c5cd655e live integration dependency audit | Inspected pvr_scene_capture.h/.cpp, quality_capture writer seam and DX11Renderer::retainPvrReplayBase/replayPvrPacket. Current live capture serializes projected rend_context data; replay retains native global state/resources and explicitly refuses OIT/RTT/other modes. Prepared successful Remix scene is not a live provider: prepare_embedded_draw_inspect requires capture/tape/ledger/reference and constrains frame choices1782/1783/1784; large_calibrated_scene_inspect binds offline transform records at ordinals1781-1783; camera_embedding_inspect labels camera-relative embedding and recovered_world_transform=false; compose_remake_inspect retains mixed source provenance. Therefore direct hookup of these files would be offline replay, not objective completion. Next bounded owned in-memory PVR snapshot plus supported live transform/camera lineage, preserving unknown-domain fallback. No production behavior changed or camera truth inferred. Initial search used obsolete dx11rend.cpp path, failed, then corrected to dx11_renderer.cpp. Existing runtime marker work remains separate and uncommitted.

#474 2026-09-09 8c5cd655e plus working tree | Public output enum exposes final/depth/packed normals/object picking/GUI, no documented albedo output. Added sequence-only --capture-d3d9-scene-color-marker diagnostic: retained current geometry/UV, all vertex colors red at1783 then green1784, original1782. Target builds. Actual63-frame legacy-color-marker run exits0 with three public final-output readbacks. Viewed1783 red response and1784 green response on fighter/arena/floor: current dynamic vertex-color changes reach raytraced output in corresponding captured frame, inconsistent with a whole-frame stale final output in this control. Response is spatially uneven with prior appearance retained; not exact modulation, temporal-quality acceptance, material truth or a full-frame identity proof. Strong artificial marker is never a quality candidate or production behavior. No external configuration/binary changes. Next quantify matched marker regions and investigate supported legacy vertex-color interpretation where needed, then advance live scene integration; do not repeat raster-only proof. Python final checkpoint suite from LOG473 completed331/331 successfully.

#473 2026-09-09 794ed8753 actual-packet legacy checkpoint | Four serial incremental builds pass after capture-order/control changes (legacy-final-regression.log per configuration). SDK179/179 and3x341 enabled selftests pass; Python result recorded at checkpoint verification. Actual scene captures and controls remain LOG467-472. ACCEPTED only for isolated actual-packet upload and deterministic raster attribute/control scope; raytraced temporal/attribute fidelity, cleanup warning40, full scene coverage and combined gameplay remain open. Commit only owned source/docs. No exact-postcommit build, runtime-binary redistribution, performance or full-goal completion claim.

#472 2026-09-09 794ed8753 plus working tree | User correction recorded in D-143: preserve game-authored temporal effects; previous settled-frame residuals do not isolate unwanted ghosting. Moved raster-only diagnostic capture before Present while keeping public Remix final-output capture after Present. Target builds. Actual current/repeat/frozen63-frame runs each exit0 (legacy-prepresent-*.log); all three current/repeat RGB images byte-identical. Frozen/current1782 exact,1783 changes7400 pixels MAE0.0958951823,1784 changes9133 pixels MAE0.1785525174. Viewed final raster. Old post-Present1784 is byte-identical to new pre-Present1783: confirms one-frame stale diagnostic backbuffer in old raster capture, not evidence of a live Flycast or public Remix-output delay. Corrected raster path now demonstrates current attributes affect the correct moving endpoint under same geometry, but full PVR shading and raytraced attribute semantics remain unproven. Native authored-effects preservation requires matched moving native/pass coverage. Full regression after capture-order refactor remains pending; no commit or combined pipeline acceptance.

#471 2026-09-09 794ed8753 plus working tree | Added --capture-d3d9-scene-raster and sequence-only raster-frozen controls. Explicit public API factory disables legacy raytraced conversion; GetBackBuffer readback is labeled raster_contract_only/backbuffer_only, never Remix proof. Target builds. Actual current/repeat/frozen63-frame runs each exit0, three readbacks each, original external configuration unchanged. Viewed current final raster with both fighters/temple. Pixel analysis: current versus repeat RGB byte-exact at1782/1783/1784. Frozen versus current exact at1782 and1783;1784 changes7400 pixels, RGB MAE0.0958951823,max130. This supports deterministic sensitivity to attributes but not correct source-frame alignment: earlier unchanged endpoint needs explicit pre-Present/frame-marker check, since post-Present backbuffer may not represent the labeled source frame. Preserve this uncertainty rather than claiming same-frame transport. Runtime outputs legacy-raster-current/repeat/frozen.bmp.frame-*.bmp and corresponding logs remain outside Git. Full regression for new capture flags pending.

#470 2026-09-09 794ed8753 plus working tree | Ran63-frame single-M --capture-d3d9-scene settled reference with original M attributes (legacy-game-settled-m.bmp/log). Exit0, viewed scene, warning40 persists. Compared validated moving1784 against settled using RGB absolute differences: full921600 samples MAE2.863089; floor31350 MAE2.621882; fighter111150 MAE5.775628. This is same-renderer/source endpoint residual, not native ground truth or pure trail energy; run variability is not yet removed. Added explicitly labeled sequence-only --capture-d3d9-scene-frozen-attributes negative control: original H color/UV with current geometry/normals, no production use. Target builds and63-frame control exits0; viewed final image. Whole-image frozen-versus-current MAE for1782/1783/1784=2.179000/2.098883/1.953652, while independent unchanged current runs differ2.062195/2.008111/1.856471. Frame1782 has identical input attributes and already differs: stochastic/session variability confounds this comparison. NOT_ACCEPTED as dynamic-attribute image proof. Next isolate D3D9 raster output for deterministic attribute contract, then use controlled repeated/session-matched raytraced comparisons; do not claim frozen-control pixel differences alone prove attribute transport. One preliminary rg used runtime cwd with repository-relative path and failed; actual run unaffected. Full regression after new CLI control pending.

#469 2026-09-09 794ed8753 plus working tree | Reran actual H/L/M sequence after compatibility/history validation: legacy-game-validated.log exits0 with63 successful Presents and three readbacks. Viewed middle frame1783: both fighters, temple and textured floor visible. Same40-common-device-object warning remains. Output legacy-game-validated.bmp.frame-1782/1783/1784.bmp retained outside Git in isolated runtime cwd. Scoped runtime regression only, not measured temporal improvement, injected device-failure coverage, resource-clean shutdown or combined DLSS5 acceptance.

#468 2026-09-09 794ed8753 plus working tree | Extracted shared legacy resource/sampler compatibility checks and added10 scene-contract controls: dynamic attributes retain resources; missing sampler, trilinear, unsupported shading, texture/palette/RTT generation, reindex and draw-ID changes reject. Uploader checks consecutive source metadata for changed frame IDs, allowing diagnostic warmup redraws, and records previous packet only after successful draw/light submission; this is not production accepted-Present history. Four serial incremental configurations pass (legacy-packet-regression.log each), SDK179/179, Python unittest discovery331/331, automation/NGX/no-NGX selftests341/341. Expected capture tamper/existing-output rejection messages occur in passing selftests. These CPU controls do not inject COM failure or prove cleanup/temporal quality. Actual rerun and matched visual comparison remain separate evidence; no exact-postcommit claim.

#467 2026-09-09 794ed8753 plus unfinished working tree | Connected bounded D3D9PacketScene to --capture-d3d9-scene for actual composed H/L/M packets. Original TSP imported; fixed-function texture modulation, address/filter settings and current position/normal/BGRA/UV uploaded through retained dynamic buffers. Asset directory changes allowed only with exact DDS byte equality; changed generations/content/topology rejected, not silently frozen. First target build failed DWORD/std::max type deduction; explicit DWORD correction builds remake-runtime-smoke successfully (legacy-scene-build.log and retry retained). Actual63-frame legacy-game-first run exits0, all63 Presents and three final Remix readbacks succeed for1782/1783/1784. Viewed first/last images: both fighters, temple, floor and changing pose present. Output names legacy-game-first.bmp.frame-1782.bmp through1784 under isolated runtime directory; initial viewer request omitted the extra .bmp prefix and failed before corrected inspection. Runtime40-common-object warning persists. No synchronous performance claim, exact temporal/fidelity gate, full PVR shader semantics, live Flycast export or combined DLSS5 acceptance. Full four-build regression and rejection/lifetime controls still pending; slice remains CORRECTIONS_REQUIRED and uncommitted. No external configuration or binary changes.

#466 2026-09-09 c9e9b0780 dynamic-route diagnostic checkpoint | Four serial incremental builds,SDK169,Python326,3x331 selftests pass. Actual legacy/color/factory controls remain LOG463-465; CPU sampler/tile tradeoffs LOG460-462. D-141 records selected next experiment. Checkpoint only owned source/docs, preserving metrics/cache/runtime logs unstaged. No exact-postcommit or production readiness claim. Next actual packet D3D9 upload with original positions/normals/colors/UVs and verified sampler metadata; full shading, camera, temporal and combined-DLSS5 gates remain open.

#465 2026-09-09 c9e9b0780 corrected legacy color control | Actual60-frame legacy-standard-frozen-first.bmp exits0 and viewed gray deformed triangles, versus colored LOG464 standard-factory dynamic output. Same geometry progression/light/camera, source color intentionally varied versus frozen. Added finite/progress/interface validation, disabled copying resource owner and latched draw/setup failure against partial retry; caller still aborts failed frame. Four serial incremental builds pass. Compatibility fixture establishes dynamic legacy attributes visibly affect Remix final output; not yet exact color matching, motion quality, game geometry or performance. Runtime warning40 common objects persists. Full tests follow; failure latch has not been exercised with injected actual D3D9 failure.

#464 2026-09-09 c9e9b0780 legacy factory root cause and output | Added backbuffer-only capture: actual60-frame legacy-backbuffer-first.bmp visibly contains changing colored/deformed triangles while Remix output remains black, proving native draw versus raytraced-output distinction. White2x2 texture/modulate control also stayed black. Pinned public rtx_remix_api.cpp calls dxvk::CreateD3D9(true,...,forceNoVkSwapchain,false,true); d3d9_main.cpp names fourth argument WithDrawCallConversion (defaulttrue), and standard Direct3DCreate9Ex uses defaulttrue. Changed only legacy fixture to standard exported Direct3DCreate9Ex then public registration. Actual60-frame legacy-standard-factory-first.bmp exits0, final Remix output visibly contains colored/deformed triangles; viewed image. Retained all black failures. This establishes legacy draw capture under standard factory, not temporal quality, source-faithful game rendering or combinedDLSS5.40-object cleanup warning persists. Target builds pass; dynamic/frozen/control matrix, sampler/full regressions and lifetime checks pending. No runtime config/binary modifications.

#463 2026-09-09 c9e9b0780 D3D9 dynamic buffer falsification | Added isolated dynamic six-vertex D3D9 default-pool buffer, DISCARD updates for position/diffuse colors, fixed-function perspective and depth surface, ordinary DrawPrimitive/BeginScene/EndScene, public light and existing final-output capture. Synthetic-only CLI dynamic/frozen-color. Both60-frame initial runs exit0 but viewed images completely black; log reports no valid camera. Added explicit public analytic camera matching fixed projection; third60-frame run exits0, no camera warning, image still black. These are FAILED image/compatibility experiments despite successful HRESULT/Present/readback. Runtime target builds pass. Pinned public d3d9_rtx.cpp inspection identifies legacy draw classification/injection paths; root cause of missing geometry not established. No consumer config or binary changes. Next focused public registration/injection/legacy draw ownership diagnosis and direct raster-versus-Remix output separation; do not declare D3D9 dynamic updates compatible or pivot back to accepted frozen attributes. Full regression and cleanup checks pending.

#462 2026-09-09 c9e9b0780 tile test correction | Initial constant-field test expected exact zero floating residual and failed at5.587173648711899e-15. Preserved exact tile-byte equality assertion, changed analytic residual bound to1e-10 (numerical roundoff only, not image-quality tolerance). Rerun follows. No new runtime or production change this turn.

#461 2026-09-09 c9e9b0780 bounded tile accuracy/cost experiment | Added read-only triangle-tile error sweep with within-triangle off-lattice probes and simplex boundary extension, sizes8/16/32/64. Actual M draws1222/1503/1887,8 distributed triangles each: mean RGB MAE at8px3.645/3.048/.902;64px.247/.375/.186, worst channels4.896/15.215/.606 at64px. Full three-draw64px allocation would be46186496bytes per generation before padding/mips; this is calculated data size, not measured transfer/performance. Small tiles lose detail; fixed large per-frame DDS baking is not established as viable. Retain sampler/tile work as diagnostic, not accepted final route. Next bounded public/legacy D3D9 dynamic vertex-buffer compatibility inspection: it may carry original color/UV directly without regenerating diffuse textures. Do not start a second production backend or assert stable runtime motion from buffer identity alone; inspect supported runtime path and prove with synthetic changing-attribute/motion controls first.

#460 2026-09-09 c9e9b0780 source-attribute sampling | Read local TSP layout and DX11 sampler behavior: FilterMode bits13-14,clampV15/U16,flipV17/U18; clamp precedes mirror,otherwise wrap. Current40 captured draws report FilterMode1/ShadInstr3. Added bounded nearest/bilinear mip-zero RGBA sampler with negative UV/wrap/mirror/clamp, raw BGRA decode and triangle barycentric color modulation;5 executed tests pass including seams/nonfinite/unsupported trilinear. Actual L/M read-only run verifies topology and DDS asset hashes/linear format, evaluates8337 centroids:4490 change,max channel delta129.22427308325675,mean maximum channel delta3.0344219239736097. This is diagnostic mip-zero texture*base-color only, not original full PVR shading (no offset/fog/mip selection) or GPU atlas fidelity. No assets overwritten. Next bounded atlas/interpolation-error experiment must preserve within-triangle variation and report minification/seam limitations, then actual moving-attribute transport; do not return to frozen-only tuning.

#459 2026-09-09 34bebd7a0 retained geometry checkpoint | Four serial incremental builds,SDK169,Python318,3x331 selftests pass. D-140 records frozen-only ablation and next-work boundary. Actual63-frame retained/rebuilt/settled controls and regional comparisons remain LOG457-458; no full-gameplay, source-attribute or performance pass. Commit explicit owned source/docs only; generated metrics/cache/runtime logs remain unstaged. Exact-postcommit verification distinct.

#458 2026-09-09 34bebd7a0 matched frozen-attribute controls | Added rebuilt-frozen and settled-frozen sequence controls. Both use identical triangle batches and first-H material/UV/color values; rebuilt changes resources across H/L/M, settled renders final M63times and captures only final output. Both actual runs exit0 and final images viewed. Against settled M, independent unchanged BMP RGB MAE: floor rectangle[360,550)x[385,440),31350samples:retained2.02 versus rebuilt2.38; fighter rectangle[350,540)x[190,385),111150samples:retained5.77 versus rebuilt7.34. Rounded values, diagnostic comparison only—not native-ground-truth trail energy; denoiser noise, frozen attributes and short sequence remain limitations. Large body/floor artifacts reduced but not eliminated. Runtime target build passes. Added bounded bone/topology/duplicate-frame/draw-failure mocks; execution follows.

#457 2026-09-09 34bebd7a0 actual retained triangle control | Added bounded triangle splitting <=256 triangles/instance (63batches for current40draw8337triangle sequence), owned weights/indices/transforms, float32 affine edge+normal mapping, staged transform validation before draw, frame/topology checks. New artifact-sequence-only --capture-retained-frozen-attributes explicitly freezes initial colors/UV/textures and logs ablation; no source namespace changes between frames. Actual63-frame H/L/M run exits0 with3readbacks/completion0; all images viewed. Large floor/leg trails visibly reduced versus LOG455, upper-body artifacts remain. This is qualitative diagnostic evidence, not measured temporal acceptance: source attributes intentionally frozen, source camera still diagnostic,40-object shutdown warning persists. No production backend change or combined output. Runtime target build passed; new CPU checks and expanded adapter/failure regression pending. Next falsifying retained-motion control and source-attribute-preserving transport; do not accept frozen color/UV as final design.

#456 2026-09-09 50ce0e453 source-color checkpoint | Four serial incremental builds,SDK161,Python318,3x331 selftests pass after final source-color CLI change. Actual three-frame composition and gradient negative controls retained in LOG452-455. Backlog contract and diff checks pass. Commit only owned source/docs/procedural chart generator+tests, not DDS/images/runtime configs or binaries. No exact-postcommit claim. Next real retained triangle transport with bounded attributes, not more static color controls.

#455 2026-09-09 50ce0e453 actual composed source-color comparison | Added --capture-source-color artifact-only opt-in using the prior reverse diagnostic light and explicit texture*vertex-color BlendEXT for every sequence instance. Logs report explicit vertex color; baked source lighting is not removed or recovered. Actual63-frame H/L/M run exits0 with3readbacks,completion0; all3BMPs viewed. Characters/temple render; moving right fighter still trails on1783/1784. Prior no-blend1782 also viewed; first-frame source-color/no-blend compare PSNR30.145074,maxdelta129,differing307031,strict exit1—not a fidelity winner. Same diagnostic snapshots/camera/light, no native-PVR match claim; material source identity now exercised beyond synthetic.40 common-object teardown warning persists. Source color does not fix isolated per-endpoint geometry identity. Production defaults/config unchanged. Target build passes; full incremental regression follows.

#454 2026-09-09 50ce0e453 blend/texture regression | Four serial incremental configurations pass;SDK161 and Python318 pass. Three neural selftest invocations tracked under one live session; completion recorded in tool output. Runtime comparisons in LOG450/452/453 remain diagnostic only, including strict-image mismatches. Next apply explicit source-color control to actual captured scene as an opt-in comparison, then bounded dynamic attributes; no automatic factory or external configuration change.

#453 2026-09-09 50ce0e453 explicit vertex-color contract | Pinned public surface_shared.h defines RtTextureArgSource Texture1/VertexColor0=2 and DxvkRtTextureOperation Modulate3/SelectArg1=1 (not D3DTOP values). Public rtx_materials.h defaults Arg2=None. Added optional BlendEXT control with texture*vertex color, baked-lighting false and preserved skinning pNext. Actual60-frame gradient-blend-first.bmp exits0, viewed spatial RGB gradient; same source vertex bytes without extension in LOG452 remain gray. Texture versus explicit vertex blend comparison PSNR55.230131,maxdelta39,differing33015,strict compare exit1, not byte-identical acceptance. Defaults/production unchanged; no game-wide color policy inferred. Two procedural chart/independent affine-field tests pass; new mock checks preserve blend+bone chain. Four serial incremental builds pass; full tests follow.

#452 2026-09-09 50ce0e453 gradient falsification | Added analytic procedural linear DDS gradient and direct vertex-color synthetic reference (three distinct BGRA colors, white constant albedo, same dim light/camera). Both actual60-frame runs exit0; viewed gradient-texture-first.bmp has expected spatial color while gradient-reference-first.bmp is gray. Compare exit1,PSNR23.118427,maxdelta101,differing55898. Thus prior byte-transport/mock checks do not establish visible vertex-color shading, and texture-versus-vertex equivalence FAILED. Do not accept texture baking from this comparison. Pinned public toRtDrawCallState BlendEXT conversion assigns texture operations/argument sources and baked-lighting flag; current adapter supplies none. Next inspect public enum/default semantics and test explicit blend state with wrong/no-blend control. This changes priority before dynamic attributes; prior source-color appearance/identity claims remain scoped/unproven. Runtime target build passed; current source WIP, no full regression.

#451 2026-09-08 50ce0e453 texture focused checks | Rebuilt SDK target and ran159 checks, all pass, including valid texture replacement with unchanged mesh count/retained path and nonexistent texture rejection before mutation. Procedural DDS chart test passes exact dimensions/linear format/colors/determinism. Full four-configuration regression and long-run texture lifecycle remain pending; no commit or broad acceptance yet.

#450 2026-09-08 50ce0e453 retained texture material transport | Added optional --replacement-texture to synthetic material replace/repeat modes only. Before runtime and before material mutation use existing public DDS contract validation; retained path storage survives API calls. Create-only procedural64x64 linear RGBA DDS chart uses existing serializer outside repo. Actual60-frame texture-replace-first.bmp shows red/green checkerboard on retained triangle; texture-repeat-first.bmp remains gray; both runtime exits0 and images viewed. No mesh rebuild, no runtime config/binary changes. This is first asynchronous texture preload/material replacement, not repeated texture animation, source color/UV preservation, cache-growth/performance or gameplay acceptance. Target build passed; new texture mock/serializer tests and full regression pending. Next bounded source-attribute representation must preserve within-triangle variation and avoid unlimited texture generations.

#449 2026-09-08 ca92ae159 affine/material checkpoint regression | Four serial incremental builds,SDK157,Python316 and3x331 selftests pass (LOG448). Actual normal/material controls in LOG444-447 retained including nonidentical/falsifying results. D-138 records scope. Next dynamic per-vertex attribute representation with retained source geometry; do not freeze attributes or declare combined pipeline done. Explicit source/docs checkpoint excludes untracked external-runtime metrics/cache/logs. Exact-postcommit checks distinct from these incremental results.

#448 2026-09-08 ca92ae159 replacement failure ownership | Fixed successful-destroy slot clearing before CreateMaterial, retained returned handles even with failure, preserved original handles on duplicate-register error, and validated camera before any material mutation. Added10 checks: successful retained replacement, invalid-camera no mutation, cleanup counts, failure with/without returned handle, no partial draw/no stale resume. SDK157 passes; four serial incremental builds exit0. LOG447 actual runtime establishes the unchanged successful material sequence; new failure cases are mocks, not injected actual runtime failure. No public configuration/binary changes or production integration. Full Python/neural selftest results follow separately.

#447 2026-09-08 ca92ae159 retained-mesh material replacement | Inspected only pinned public rtx_remix_api.cpp and rtx_asset_replacer.cpp at e876135b37295dc203ccdb7b20a8089629588201. CreateMaterial queues material registration; makeMaterialWithTexturePreload uses emplace and explicitly ignores repeated handle; destroyExternalMaterial erases it. Added synthetic-only single frame1 destroy/recreate versus duplicate-register control, no mesh recreation. Actual60-frame replace/repeat runs exit0; material-replace-first.bmp visibly red while material-repeat-first.bmp stays gray, both viewed. Runtime target build passes. This proves constant-material replacement on retained synthetic meshes, not arbitrary vertex attributes, dynamic textures, temporal stability, resource-lifetime matrix or production viability. Initial helper game guard corrected against actual Synthetic game string before execution. New helper failure ownership/mock checks and full regression pending. Logs/BMPs external; no third-party binary/config changes.

#446 2026-09-08 ca92ae159 affine interior comparison | Independently read unchanged BMP bytes using PowerShell; interior rectangle x150..239,y240..299,RGB16200 samples: retained/reference MAE rounds0.54 with reference mean90.05; retained/wrong-normal MAE3.21 with mean92.71. Explicitly excludes edge/history uncertainty; supports this normal fixture only. Added dim creation/retained affine/non-skinning rejection mock checks. Four serial incremental builds exit0. No exact-postcommit or real-game claim.

#445 2026-09-08 ca92ae159 dim affine normal control | Affine synthetic modes now explicitly use public distant radiance0.03 instead of3; new reference-only wrong-normal mode preserves undeformed normals while transforming positions. Three actual60-frame runs exit0; retained/reference/wrong-normal BMPs and logs external, all viewed and unsaturated. Whole-image retained/reference PSNR58.396676,maxdelta13,differing54247; retained/wrong-normal PSNR46.481653,maxdelta15,differing55983. These existing compare commands use strict comparison defaults and are not pass declarations (combined command exit1). Correct reference is closer than wrong-normal but denoising/history differs (first retained frame identity). Target builds pass; full regressions/new affine mock checks pending. No real-fighter normal/history or performance acceptance.

#444 2026-09-08 ca92ae159 plus affine GPU diagnostic | Added synthetic-only --capture-affine-retained/reference and retained nonrigid affine bone redraw. Matrix stretches/tilts edges and carries unit normal basis; reference directly transforms submitted vertices/normals. Both actual60-frame external-runtime runs exit0; affine-retained-first.bmp and affine-reference-first.bmp plus logs remain outside repo. Viewed both: same apparent tilted/clipped triangle silhouette. Existing compare reports118955 differing pixels,maxdelta92,PSNR45.862882 (not byte equality or a threshold pass); retained run begins identity then transforms, reference begins transformed, so histories differ. Saturated white lighting prevents strong normal validation. Target runtime/SDK builds pass; unchanged SDK144 checks pass but do not specifically test new affine method. Full regression,new mock checks,unsaturated normal/light negative control remain pending. Do not call this normal correctness, fighter ghosting fix, or temporal pass. Production/rendering settings untouched.

#443 2026-09-08 ca92ae159 triangle-affine transport feasibility | Added read-only float32 affine fixture mapping each source triangle's two edges plus unit normal to the next triangle. Three executed tests pass (translation, rotation/scale/shear, reverse-direction falsification, nonfinite/degenerate rejection). Actual L/M inspection first verifies topology through retained_topology_inspect, then evaluates8337 triangles,0 rejected,63 <=256-bone batches. Maximum position residual0.000147350971331567 scene units; normalized direct-transformed normal residual5.58964546602418e-08; unchanged-position control maximum0.423131950516208. This is CPU arithmetic, not GPU skinning, recovered skeleton, full source identity or perceptual acceptance. Color/UV transport remains unsolved; no source attributes frozen. Keep per-triangle transform as bounded diagnostic candidate, not silently adding63 instances to production. Next verify GPU normal/position behavior of a nonrigid affine bone and evaluate documented material update mechanism for dynamic attributes. Current C++ unchanged from ca92ae159; no new full build claim.

#442 2026-09-08 481642a17 retained synthetic checkpoint | Full Python inspector suite313/313 passes after base/offset distinction. LOG439 four serial incremental builds,3x331 selftests and144 SDK checks cover unchanged C++ sources. Commit only owned synthetic transport, analytic checker, topology inspector/tests and governing docs. This is scoped transport/diagnostic acceptance, not actual fighter retained rendering or combined-pipeline completion. Untracked metrics/cache/runtime logs preserved unstaged. Exact postcommit verification remains distinct.

#441 2026-09-08 481642a17 dynamic-attribute public-contract review | Inspected pinned public header InstanceInfo/BoneTransforms/BlendEXT and actual loader lines120-125. Header has per-instance blend/tFactor but no arbitrary vertex-color/UV update extension; loader imports raw base color[5], not offset[6]. Corrected inspector to distinguish them; six focused tests pass. Actual L/M still has15 affected draws:14 changing base colors and draw2371 changing60 UV vertices. Fighter draws1222/1503/1887 show194/256/185 distinct base-color delta tuples, so a single additive tint is not equivalent. LOG440 count11345 included either base/offset change and must not be read as imported-base-color count. No binary/config changes. Initial wildcard rg path failed on Windows; reran with directory and -g filters. D-137 records transport and ablation boundaries; full Python regression running separately.

#440 2026-09-08 481642a17 plus actual retained-attribute audit | Read-only H/L and L/M prepared snapshot inspection finds all40 draws preserve index topology and source-vertex order. L/M has15 draws with changed immutable attributes versus25 unchanged:11345 split vertices change raw base/offset color,60 change UV. Bindings remain equal. Thus public position/bone-only reuse cannot faithfully carry every source attribute; retaining first-frame color/UV silently is invalid. Added diagnostic retained_topology_inspect and4 executed passing tests for pose separation, color/UV changes, changed source slot, sequence and nonfinite rejection. No identity/skeleton/runtime acceptance implied. Initial raw single-line JSON display was oversized/truncated; subsequent inspection parses and emits bounded summaries. Next determine the public per-instance/material transport for changing color/UV or explicitly separate that unsupported domain before real deformation capture. Existing synthetic changes remain uncommitted; production unaffected.

#439 2026-09-08 481642a17 plus retained-deformation regression | Four serial incremental builds automation/baseline/no-NGX/off exit0. Three explicitly invoked selftests331/331 each and SDK144/144 pass. Initial neuraltest invocations without selftest printed usage and are not test evidence. Python307 tests initially failed one newly added landmark (x379,y121 lies outside the narrow deformed near apex); corrected independently projected point to x380, rerun307 passes. Added explicit apex-offset CLI/metadata, nonfinite rejection, both-sign deformation and independent landmark coverage. Read-only reanalysis of retained actual synthetic-skinning-first.bmp gives correct IoU0.994436847339632 versus wrong-sign0.5636695906432748. No runtime rerun or performance claim this turn. git diff --check passes. Scoped synthetic transport/analyzer regression accepted; fighter temporal stability, normal-consistent real deformation and combined presentation remain unproven. Next inspect source-topology continuity and bounded public bone transport for actual meshes; do not manufacture a game skeleton.

#438 2026-09-08 481642a17 WIP retained synthetic skinning | Added synthetic-only public skinning control: two retained triangle meshes, three one-weight bones, apex bone translation0..0.5 over60frames; reversed control uses-0.5. Actual both runtime runs exit0, final BMPs viewed; measured correct analytic coverageIoU0.99443685/0.99456066 versus wrong-sign0.56366959/0.56354466. No mesh re-creation in redraw path; strengthened mock checks public weights/indices/transforms, retained resource counts and out-of-bound rejection:144 SDK checks pass. Target builds pass. Full regression, analyzer deformation unit coverage and actual temporal motion-vector evidence remain pending; not proof of fighter ghosting correction. Runtime captures/logs external synthetic-skinning-first and synthetic-skinning-reversed-first. User asked about Tracy mid-work; assessment only, no profiler installation/integration. Skinning source WIP/uncommitted; standing task remains active.

#437 2026-09-08 2fca97065 sequence regression and public history contract | Four serial incremental builds pass;3x331 selftests and141 SDK checks pass (304 Python tests/actual63-frame sequence and swapped-order rejection in LOG436). Inspected pinned public remix_c.h plus public rtx_remix_api.cpp at e876135b37295dc203ccdb7b20a8089629588201, never binary internals. CreateMesh allocates/copies buffers and assigns fresh internal geometry hashes on every call (lines897-968); external handle comes from supplied hash, not proof of retained internal history. Header has no previous-position field in HardcodedVertex/no UpdateMesh entry; exposes MeshInfoSkinning and InstanceInfoBoneTransformsEXT, maximum256 bones. Next bounded synthetic retained-mesh skinning/transform test with analytic deformation and wrong transform control; do not claim stable IDs alone solve ghosting or invent private parameters. Existing per-endpoint namespace captures remain the failed temporal baseline. Source/docs checkpoint follows; no production/world-camera/combined-pipeline acceptance.

#436 2026-09-08 2fca97065 WIP composed sequence | M first/second join and all three embedded groups prepare frame1784 with own content/material checks, then40-mesh composition succeeds. Rebuilt runtime with explicit reverse-light forwarding to every sequence scene. Actual63-frame run (60 Hwarmup then1782/1783/1784) exits0 with three successful per-frame readbacks,63 Presents and bounded completion. Viewed all three composed-sequence-first.frame-ID BMPs: both fighters/temple present and pose changes; visible temporal ghosting around moving fighter/legs, NOT temporal quality acceptance.40-object teardown warning unchanged. Actual swapped M/L sequence under same reverse-light option rejects before runtime exit2.304 Python inspectors pass. Separate mesh namespaces intentionally remain, temporal identity not proven; next inspect public mesh update/previous-position contract and test correct correspondence, retaining ghosting capture as negative evidence. Full regression/commit pending for frame selector/sequence-light forwarding. Three frames are not continuous-gameplay or combined-DLSS5 acceptance.

#435 2026-09-08 2fca97065 WIP next composed frame | Added explicit bounded --frame1782/1783/1784 to embedded preparation, retaining existing three-frame arithmetic verification and per-frame content/material checks. Actual second-J1783 and second-K3 1784 prepare16meshes each with Hfixed origin and nonzero camera; no strict gate relaxation. First/second L join33meshes succeeds; all three large groups independently prepare frame1783 against first-L scene/material capture, then compose40meshes/four source groups. Actual60-frame static L runtime/reverse-light exits0 and composed-l-first.bmp viewed: both fighters and temple at next pose. Separate H/L runs, NOT one-session motion yet.304 Python tests pass. Added reverse-light forwarding to existing three-endpoint sequence path, not yet rebuilt/run; don't claim sequence control proven. Msecond packet prepared, remaining Mgroups/join and full sequence remain next. Full regression/commit pending,40-object cleanup warning unchanged.

#434 2026-09-08 f3f351f77 composition checkpoint checks | Added focused synthetic composition tests preserving distinct revisions and rejecting incorrect source/reference/frame/draw/equivalence/world claims and duplicate groups. Tightened content digest format/agreement, selected asset coverage and loader asset-digest checks. Four serial incremental builds pass after final loader changes;3x331 selftests,141 SDK checks,304 Python inspectors pass. Actual composed loader positive+22 malformed controls pass, including content/asset digest mutations. LOG433 retains actual40-mesh static image before metadata-only tightening; no new runtime image or exact-SHA claim. D-136 records diagnostic embedding/mixed provenance limits. Checkpoint owned source/docs only, preserving untracked runtime-generated files unstaged. Next moving composed endpoints without repeating static lighting isolation; complete gameplay and combined DLSS5 remain open.

#433 2026-09-08 f3f351f77 WIP composed fighters and temple | Added mixed-diagnostic-anchor composition with four per-group original source revisions, coordinate labels, embedding/equivalence records and exact draw ownership. Reuses existing camera/geometry/shared-asset conflict checks and byte-verified create-only publication; no source SHA erased. Actual composedH40meshes runtime60frames/reverse-light exits0, composed-h-first.bmp visually reviewed: both fighters together in temple, static diagnostic.40-object teardown warning persists. Loader retains bounded group provenance in runtime log. Follow-up coverage review replaced count-only mesh membership with exact sorted draw multiset equality; actual loader positive+20 rejection controls pass (missing/wrong group provenance, false world claim, incorrect ownership, duplicate mesh, false equivalence plus prior malformed controls).303 Python inspectors pass. New composition helper still needs focused synthetic tests/full regression/checkpoint; initial runtime capture predates ownership-only tightening. No moving gameplay, complete image semantics, quality or combined-DLSS5 acceptance. Next checkpoint robust composition then prepare moving joined endpoints; do not repeat lighting/culling diagnostics.

#432 2026-09-08 f3f351f77 fighter preparation regressions | Original serial build handle70520 completed exit0 for automation/baseline/no-NGX/off without restart. Three enabled selftests331/331 each and rebuilt SDK141/141 pass. LOG431 has303 Python/18 actual loader rejection controls and separate visible two/four group runtime captures. This closes current incremental regression execution, not exact-postcommit verification or combined scene acceptance. Source remains WIP for provenance-preserving composition and review; no full-goal closure.

#431 2026-09-08 f3f351f77 WIP remaining fighter groups | Added large/two/four selection to preparation CLI using existing verified arithmetic/lifetime inspectors; no source trace shortcuts. Actual two-D prepares2meshes/6114split vertices, four-C prepares4/6279, each preserving source/reference revisions, content equivalence, coordinate provenance and explicit derived normals. Actual two60-frame reverse-light runtime runs exit0; both BMPs viewed and show complementary fighter body geometry. All seven large draws now rendered separately, not yet one composed scene or moving gameplay. Embedded loader actual positive plus18 malformed controls pass, including missing provenance, false world claim, wrong source/reference and coordinate metadata.303 Python inspectors pass. Strong overlighting and40-object runtime teardown remain unaccepted. Next compose temple plus three embedded groups preserving per-group provenance; do not flatten different SHAs into one claimed source. Full serial regression started after these results; no completion/commit claim yet.

#430 2026-09-08 f3f351f77 WIP first embedded fighter runtime | Preparation accepts explicitly labeled diagnostic-camera-embedded-anchor with original sourceSHA/embedding provenance; loader validates and retains bounded provenance and runtime logs it. No relabel to recovered source anchor. New preparation CLI composes existing full921 arithmetic, content equivalence, embedding, source-DDS verification, original topology check and actual C++ flat normals. Actual H draw277 preparation4773split vertices/1591triangles succeeds with own asset5. Target runtime/artifact builds pass; actual60-frame reverse-light run exits0 and embedded277-first.bmp viewed: visible partial fighter head/torso/hands/boots, remaining body draws omitted, strong overlighting.40-object warning remains.303 Python tests pass. This is first large fighter-derived geometry GPU evidence, not complete fighter/animation or quality acceptance. New loader provenance mutation tests/full regression remain next, then remaining six draws and provenance-preserving combined scene. All game bytes/output external; source changes WIP/uncommitted.

#429 2026-09-08 f3f351f77 cross-revision content equivalence | Added explicit verifier comparing every scene field except git_sha, material metadata except its two revision fields, then independently decoded DDS bytes/bindings for selected draws. Both revisions retained, no blanket SHA equality. Actual H full921-A/source60dedc1e1, two-draw-D/d558fb32c and four-draw-C/36538ae12 all match standard-H/755b90f8e scene content exactly, including15967 complete raw vertices, indices, draw state and passes. Canonical scene content SHA2560b3669d5400b0e3e5f8c60b4406bc915925375783465472edb17f263f5397173. Selected seven draws' seven texture assets independently match bytes/generations/bindings; returned asset hashes retained in tool output. Mutation test rejects frame/vertex/game/extra-field differences;303 Python tests pass. Scope H only, not cross-frame/runtime equivalence. Next preserve this record and distinct diagnostic embedding provenance through preparation and runtime loader, then render actual large meshes without altering original source metadata. No production change/full build/commit claimed.

#428 2026-09-08 f3f351f77 WIP diagnostic embedding | Added distinct diagnostic-camera-embedded-anchor conversion, finite/proper basis and source vertex identity checks plus sub0.001pixel comparison against original source screen words. Actual full921-A H2745vertices embeds into joinedH reference with max0.0000353434pixels; sourceSHA60dedc1e1 and referenceSHA755b90f8e both retained, not relabeled equal. Initial conversion forgot build_frame's prior Y reflection when preserving strip order; corrected winding reversal before integration and strengthened nondegenerate triangle test. Full302 Python tests pass before final triangle-test strengthening; focused strengthened test passes afterward. Original actual projection run predates winding-only correction (positions unchanged); no normals/runtime render claimed for this new mesh. Next verify cross-capture source attributes/topology/material generations before any same-frame join, preserve distinct embedding provenance through prepared serialization/loader. Existing strict failed gate unchanged. Source WIP/uncommitted; no production code or external config changes.

#427 2026-09-08 f3f351f77 remaining large-draw evidence revalidation | Added optional source_details return to existing large/incarnated calibrated inspectors; default summaries unchanged, all prior arithmetic/lifetime/calibration/source-binding checks still execute. Actual full921-A, two-draw-transform-D and four-draw-transform-C retained captures/tapes/ledgers revalidate across1782/1783/1784. Per-frame groups2745vertices/1591triangles,3392/2038,3531/2093 respectively:9668 vertices/5722triangles across seven disjoint draws277,1503,1887,161,854,1222,2266. Four-draw uses existing explicit affine-contribution mode, no new tolerance. Full921 max reprojection errors0.0000357423/0.0000378189/0.0000357514.301 Python tests pass. Coordinates explicitly calibrated-camera-relative, not shared source anchor; no runtime join/lighting/gameplay claim. Next implement labeled diagnostic camera-relative-to-anchor embedding with analytic reprojection, handedness and source-identity controls before preparing these meshes. Original source normals/world scale remain unknown. A pair of literal-wildcard source searches failed and were replaced with directory -g searches; no evidence inferred from them. Optional-details edits WIP/uncommitted.

#426 2026-09-08 b4469d5a5 joined scene checkpoint | Added bounded same-frame batch join preserving source identity, unique draws, shared-asset equality, all DDS hashes, omissions and both independently derived cameras. Recorded1e-12 diagnostic camera roundoff bound, not strict reprojection acceptance. Actual first/second H join yields33meshes/22assets. Actual60-frame reverse-light runtime exits0 and joined BMP viewed: substantially more textured temple/background, still incomplete fighters/world.40 common-object teardown warning remains. Four serial incremental builds pass;3x331 selftests,141 SDK checks,301 Python inspector tests, actual loader positive+13 rejection controls pass. New join tests cover identity/camera/draw/asset conflicts, create-only output and changed publication bytes. All raw captures/assets remain external. Scoped tooling checkpoint only; next remaining source geometry/fighter coverage and moving joined endpoints, not full-gameplay/combined-DLSS5 acceptance. Exact postcommit source verification still separate.

#425 2026-09-08 b4469d5a5 plus explicit diagnostic lighting | Temporary light direction(0,0,-1) instead of(0,0,1), unchanged radiance3/full16meshes/clips0.1..2501, actual60frames exits0 and reveals textured temple plus distant scenery. Viewed output; geometry deletion is not needed for this bounded visibility result. Reverted temporary global edit and rebuilt, then added explicit --capture-reverse-light option with default unchanged, logged direction/recovered_game_lighting=false, and adapter mock assertion. Target builds pass; SDK141/141. Actual option run60frames exits0 and visible full submitted batch was viewed again (second-reverse-light-option.bmp/log outside Git). This proves response to diagnostic lighting, not recovered game lights/complete geometry/faithful shadows. No culling/sky override retained. Next source-safe batch join using this explicitly labeled lighting, followed by full regression checkpoint. Runtime teardown still not accepted; complete combined gameplay remains open.

#424 2026-09-08 b4469d5a5 retained-geometry controls | Tested two temporary adapter-only changes separately on draw2522 (mesh id2523), retaining full16-mesh packet/clips0.1..2501/60frames: doubleSided0, then original doubleSided1 plus public SKY category. Each target build and runtime exits0, both final BMPs remain near-black and were viewed. Neither mutation is a sufficient fix. Both source mutations reverted; restored runtime target rebuild exits0. No guessed classification or title-ID hardcode retained. Native state decoded from actual ta_structs.h: CullMode3,DepthMode4,ZWriteDis0,ShadInstr3,UseAlpha0,IgnoreTexA1,FogCtrl1. SKY enum field presence alone never establishes title semantics. Retain second-single-sided2522 and second-sky2522 BMP/logs externally. Two initial source searches used missing guessed filename/literal wildcard and failed; corrected directory search located remake_remix_tests.cpp. Next isolate lighting direction versus material/occlusion using explicit diagnostic control, not more culling/category guesses. Full regression/commit pending for subset CLI; production renderer unchanged.

#423 2026-09-08 b4469d5a5 WIP per-draw visibility controls | Actual four60-frame runs restore each of2522/2570/2647/2648 individually to the12-mesh near subset at identical0.1..2501 clips; all exit0 and four BMPs viewed.2522 strongly darkens/obscures temple; other three individually retain recognizable textured temple. Additional full16-minus2522 (15meshes) run exits0 and restores visible temple, confirming2522 as primary trigger without dropping the other distant draws. Captures second-restore-DRAW and second-without2522 BMP/logs retained in isolated runtime folder. Native captured four draws share ISP2613051392,TSP543696109, opaque list0; not automatically sky. Current adapter forces doubleSided=1 and default category on all instances. Public pinned header exposes SKY and other category flags, but no classification or parameter change implemented merely from those names. Next establish source surface/material semantics and test a bounded explicit treatment while retaining2522, rather than shipping exclusion. No full regression/commit or combined-pipeline acceptance.

#422 2026-09-08 b4469d5a5 plus WIP visibility isolation | Second batch has four distant/large draws2522/2570/2647/2648 (depth up to2499.94) alongside near temple meshes. Repeated full batch from isolated runtime working directory still near-black, exit0; not a cwd-only fault. Added create-only bounded subset CLI retaining assets/camera/source metadata and explicit exclusion omissions; focused mutation test and all299 Python inspector tests pass. Excluding those four gives visible textured temple at diagnostic clips0.1..104; repeated SAME subset at original0.1..2501 remains visible, isolating geometry exclusion from clip-range confound. Both60-frame runs exit0, BMPs visually reviewed, external second-h-near-subset-b4469d5a5 and second-h-subset-same-clips-b4469d5a5 BMP/logs retained. This implicates excluded geometry but does not distinguish shadowing/occlusion/material error. Do not ship deletion as a fix. Next per-draw isolation/native state inspection before full join. Prior repo-cwd runtime generated untracked metrics.txt/cache/rtx-remix logs; preserved unstaged, subsequent runtime runs isolated outside repo. Source WIP, no C++ change/full-build/commit claim; cleanup warning and full pipeline open.

#421 2026-09-08 b4469d5a5 second-batch visible-output rejection | Reopened LOG420 final BMP: nearly black, not accepted visible geometry evidence (maximum RGB channel2,29702 nonblack pixels of307200). Actual60-frame depth capture exits0 with readback/completion success and same40-object warning; retained second-h-depth-b4469d5a5.bmp/.rgba32f/.log outside Git. Raw307200 depth samples all finite; visualization effectively uniform. This does not establish visible second-batch coverage or faithful depth; do not count successful submission as scene expansion. LOG420's image-viewed wording is not a visual pass. Next compare submitted projected triangle coverage/native draw domain before joining batches, to distinguish non-visible geometry from shading or contract failure. No production/config/binary changes or gameplay acceptance.

#420 2026-09-08 025288284 second traced batch expansion | Added --batch1|2 to existing preparation CLI, retaining same inspectors. Actual second-I ordinal1781 prepares16meshes/1389split vertices (463triangles), own assets and shared H origin; independently derived camera agrees with first batch apart from sub-double-roundoff origin. Runtime clips0.1..104 rejects before load (retained). Measured all submitted depths min4.0353617668,max2499.9421386719; explicit diagnostic enclosure0.1..2501 rerun exits0/60Present/readback/completion; image viewed. This interval is NOT recovered game near/far. Runtime40object warning persists. Second batch remains separate, not combined full scene; background/nonopaque/offscreen exclusions persist. Next source-draw/material-safe join with first batch and compare against native capture; no unproven camera shift or binary/config changes. Preparation CLI change WIP; no full regression/commit claimed.

#419 2026-09-08 00b2e11bf plus sequence regression closure | Extracted DiagnosticContinuation checks used by runtime, with7 positive/negative C++ cases covering consecutive IDs, skipped frame, game, SHA, changed/missing/nonfinite origin. Four serial builds pass;3x331 selftests,140 SDK checks,298 Python tests,actual artifact loader positive/13 rejection controls pass. Initial apply_patch expected wrong Result member type and failed without modifying files; corrected against actual std::string declaration. Guarded prepare_remake_endpoint positive first-L rerun completes with own new asset directory and same camera/source/omissions as prior endpoint; subsequent artifact-check succeeds. No current runtime changes beyond shared validation since LOG418 per-frame sequence evidence. Documented63-frame diagnostic sequence and isolated temporal identities. Accept scoped preparation/transition harness checkpoint only; complete scene, correct typed normals, runtime cleanup, continuous gameplay and combined DLSS5 remain open. Explicit source/docs staging only; exact postcommit verification still required, no full goal completion.

#418 2026-09-08 00b2e11bf plus WIP per-transition readback | Moved diagnostic readback inside frame loop for three post-warmup sequence endpoints; generated source-frame filenames checked create-only before runtime. Actual63-frame run logs60 warmup, then1782/1783/1784 each with successful readback; viewed all3 temple images. One runtime/session, not three separate runs. Resource namespaces remain intentionally isolated, so this does not prove temporal correspondence or source-to-final frame identity under adversarial substitution.40 common objects still remain. Swapped M/L endpoint CLI rejects before runtime with identity/origin mismatch exit2. Target builds pass. Full regression and additional sequence mutations (origin/SHA/skipped source) pending; all captures external, source WIP. Next strengthen sequence contract/tests and checkpoint; longer gameplay/complete actors/presentation/consumer remain open.

#417 2026-09-08 00b2e11bf plus WIP one-session endpoint transition | Packet retains diagnosticOrigin from loader; CLI --next LROOT --next MROOT before final --capture loads all3 bounded packets before runtime and checks consecutive frame/game/SHA/shared origin. Separate mesh/material ID namespaces avoid claiming cross-endpoint temporal identity; all3 scene owners retained to session end. First3-frame actual run exits0/API success but captured black, rejected as visible sequence evidence. Added explicit60 repeatedH startup warmup then H/L/M once (63 total), logging warmup/source IDs. Actual run exits0,63 Present/readback/completion success and final temple image viewed;40 common objects remain. Final image alone does not prove each intermediate output or moving continuity; per-transition captures and falsifying sequence controls still required. Both black and warmed BMP/logs retained outside Git. Target builds passed for each run; source WIP, no full regression/commit. No game interpolation, camera acceptance, runtime config/binary edits or combined DLSS5 proof.

#416 2026-09-08 00b2e11bf plus third endpoint and preparation guards | Actual first-M ordinal1783 preparation completes17meshes,frame1784, own verified assets and shared H origin; camera(-0.0337967845,0.0067713420,-0.1267396644),all6 omissions/strict failures retained. Actual60-frame endpoint runtime at clips0.1..104 exits0/readback+completion success; viewed textured temple.40 common objects remain. No transition yet: H/L/M are separate immutable endpoint runs. Added bounded JSON reads (reference32MiB,scene64MiB,materials16MiB), explicit H reference frame/game/SHA/finite origin validation;2 new tests including6 reference mutations pass and full298 inspectors pass. M preparation ran before guard edit completed; no claim it exercised the new checks. Script source remains WIP pending positive guarded preparation/control checkpoint. Next one-session H-to-L-to-M diagnostic transition with explicit source IDs and bounded resource ownership; no interpolated camera or full-gameplay acceptance.

#415 2026-09-08 00b2e11bf plus WIP independently prepared L endpoint | Added prepare_remake_endpoint.py composing existing ancestry/anchor/reflection/fixed-origin/source-DDS/join/flat-normal inspectors, create-only output. Actual first-L ordinal1782 prepares17meshes, own verified assets, frame1783 source755b90f8e and all6 omissions/strict failures retained. Shared H fixed origin gives camera(-0.0179854134,0.0036687233,-0.0652765953), not silently recentered. Runtime60-frame endpoint capture with diagnostic clips0.1..104 exits0, readback/event completion succeed; viewed recognizable textured temple, still dark/incomplete.40 runtime common objects remain. Files retained in external fc067-endpoint-l-00b2e11bf and soulcalibur-endpoint-l.bmp/log. This is a second independently prepared/static-rendered endpoint, NOT a moving sequence or full game camera. Script WIP needs bounded input/reference validation and negative unit tests before commit. Next M endpoint, then bounded frame-to-frame submission with source IDs and lifetime preserved; no synthetic camera interpolation substituted for game data.

#414 2026-09-08 00b2e11bf retained next-frame recheck | Located first-L/M tapes/ledgers in ignored build directory and separate first-L/M capture roots, distinct from standard-H multi-frame color capture. Ran ancestry_scene_inspect on first-L capture, matching first-l.bin/first-l.zlib, four-draw-transform-c topology, calibration614.7144309686947/565.5372185498106,producer ordinal1782. Exit0; actual frame1783 summary3682vertices/2152triangles,T1401N,SHA755b90f8e. Strict failures11949/11951/12126/12127/12182 remain and renderable/world-camera/complete-scene false. Output retained in ignored fc067-sequence-l-recheck.json. This establishes available independently traced next-frame input for preparation, not runtime sequence proof. Windows literal wildcard source search initially failed and was corrected using rg -g. No production/source rendering edits, new captures, runtime/gameplay or matrix acceptance in this turn. Next prepare L/M using their own source materials and one shared H origin, then static endpoints before implementing bounded sequence submission.

#413 2026-09-08 32323c528 plus lifetime controls checkpoint | Four incremental serial builds pass,3x324 selftests,133 SDK checks,296 Python inspectors pass. LOG410 reverse-order raw depth is byte-identical; LOG411 event completion passes without eliminating warning; LOG412 empty-scene37objects versus rendered40 proves scene handles are not sole cause. Preserve all failures and normal-format correction; no M1 cleanup closure. Actual prepared artifact lists6 exclusions including background/nonopaque/offscreen domains. Existing standard-h capture contains frame1782/1783/1784 folders; this is discovery only, not proof that all have compatible camera/asset ancestry. Updated stale camera document to distinguish actual static snapshot from historical pre-runtime task. Next inspect retained multi-frame transform evidence and scoped sequence feasibility; do not relabel incomplete static temple as moving gameplay. Source/docs-only checkpoint, runtime/config/media/captures remain external.

#412 2026-09-08 32323c528 plus WIP empty-scene lifetime control | Added synthetic-only --capture-empty that submits no camera/mesh/material/light API scene calls, retains same owned device/capture/completion/Shutdown path. Built runtime target; actual60-frame run exits0,60 Presents/readback/completion success,37 CommonDeviceObjects reported at exit versus40 on rendered runs. No caller scene handles exist in this control, falsifying them as sole cause; extra3 objects may be rendering-triggered but cause/lifetime not established. Not proof of per-frame growth, harmlessness or clean shutdown. Empty capture/log retained externally. Stop repeating idle waits or Present bypasses already falsified; retain runtime ownership issue as open while next source-scene coverage work proceeds where independent. New control/full regression and checkpoint pending; no M1 acceptance or changed production path.

#411 2026-09-08 32323c528 plus WIP bounded completion check | Added D3D9 EVENT query after scene/capture resources leave scope, IssueEND/GetDataFLUSH bounded2seconds with process watchdog retained; capture route only, no performance evidence. Built target, actual60-frame depth capture exits0, readback succeeds and completionHRESULT0, but40-object teardown warning persists. This falsifies pending queued GPU work as a sufficient cleanup correction; no clean-lifetime claim. Public-source inspection: rtx_common_object.h counts live CommonDeviceObject instances and reports them at exit, not directly caller mesh handles. Public DxvkDevice destructor already waits idle. Do not label warning harmless or infer per-frame growth from it. Initial guessed dxvk_objects.cpp URL returned404; tree metadata located dxvk_objects.h and rtx_common_object.h. Only public text inspected, no runtime internals or binary changes. Completion code WIP pending failure controls/regressions. Continue ownership diagnosis while preserving real scene progress; no gate closure.

#410 2026-09-08 32323c528 plus WIP runtime depth-order control | Added synthetic-only --capture-depth-reverse-order; reverses submitted mesh order without changing geometry/camera/light. Actual60-frame runtime capture exits0 and raw RGBA32F SHA256 exactly matches original depth:9C89CB350608AE66435FA77A5761FA9F4ABF76B7941FD382398A6D540EEC8B49. All depth metrics identical, establishing order-independent visibility only for this bounded fixture. Separately tested direct ownedDevice PresentEx instead of public api.Present to isolate SDK-wrapper contribution to teardown warning:60-frame capture exits0 but40 undisposed objects persist. Hypothesis not sufficient; reverted this temporary source change, retained capture-depth-direct-present.log/BMP/raw. No binary/config modification. Target built for both runs; after revert target rebuild pending. Cleanup cause still unknown, no broad acceptance. Next focused completion/lifetime evidence rather than adopting the ineffective wrapper bypass.

#409 2026-09-08 8eef3db66 plus controls regression checkpoint | Removed unreachable invalid normal-float conversion; documented unsupported packed-normal capture and correct depth sidecar. Four incremental serial builds pass;3x324 selftests,133 SDK,296 Python inspector tests pass. New raw checker controls exercise empty geometry, allNaN and malformed byte count; empty/invalid matches now report null errors rather than division failure. Actual retained depth reanalysis exactly reproduces LOG408 numbers. Normal rejection was rebuilt/run exit2 before runtime in LOG408; full normal support remains absent. Accept diagnostic camera/light controls and bounded color/depth analyzers only; neither screenshot metrics nor tests close M1-GPU. Pending exact-SHA verification, runtime reversed-submission/overlap controls, typed normal route, resource completion/cleanup and moving game scene. Source/docs only; proprietary binaries/config/assets/raw captures stay external.

#408 2026-09-08 8eef3db66 plus WIP depth interior and invalid-normal diagnosis | Raw depth32192 interior pixels (same expected surface in a2pixel neighborhood) haveMAE0.0000172865,max0.0000452107; full matched max0.02506332 remains retained. Wrong far-first analytic ordering yields interiorMAE0.01634188. These are analysis controls, not runtime geometry mutation. Actual60-frame raw normal capture exits0 but sample RGB floats containNaN with first word0xFFFEFFFE. Pinned public rtx_resources.cpp declares m_primaryWorldShadingNormal as VK_FORMAT_R32_UINT; the public API copies this resource, not the distinct floating-point DLSSRR normal surface. Therefore earlier normal float/BMP interpretation and IoU are INVALID evidence, not a camera mismatch or trustworthy numeric normals. Do not crop/scale to repair them. Added fail-closed --capture-normals rejection before runtime until correctly typed integer readback exists. Keep all invalid captures/logs. Final-color and raw depth use different source formats and are not invalidated by this finding. Need rebuild/rejection test and inspector unit coverage; typed normal route remains open, no source commit/full gate acceptance.

#407 2026-09-08 8eef3db66 plus WIP raw public guidance | Added --capture-depth, RGBA32F raw sidecar for float guidance, create-new sidecar guard, logged dimensions/channel order/type/frame and output enum. Display depth uses arbitrary value/10 visualization only; raw values are unchanged. Built runtime target and ran60-frame synthetic-depth-raw capture:exit0, readbackHRESULT0, sidecar write success; runtime still reports40 undisposed objects. Viewed image, sampled near0.9509513/background1. New read-only bounded raw-depth diagnostic compares cameraX0.5, clips0.1..100 and Z2/Z4 planes against projected depth:coverageIoU0.9937585715,near28920/far6765/background271515,nonfinite0; matched-pixelMAE0.0000676030,max0.02506332. Maximum error is not waived (near/far transition classification may contribute; unproven). This supports depth representation/coverage, not final depth-order acceptance; reversed-order and interior-only tests remain. New raw checker lacks unit controls yet. No M1 closure/full regressions/source commit. Next isolate edge versus interior residual, retain normals raw and test negative depth ordering without changing acceptance thresholds.

#406 2026-09-08 8eef3db66 plus WIP analyzer coverage | Added5 unit tests for synthetic capture analyzer: exact coverage/known35550pixel area, wrong-camera mismatch, black image, malformed signature/size, nonfinite camera and landmark checks; all5 pass. Inspector now bounds file size before reading and caps read length, avoiding unbounded allocation from an arbitrary input path. Public pinned rtx_context.cpp blitImageHelper uses whole source/destination mip extents, not an explicit active content rectangle. This is a sampling hypothesis for normal mismatch, not a proven cause or permission to crop/stretch to make metrics pass. Actual runtime normal geometry remains discrepant. Next retain raw float normal/depth evidence and frame/render rectangle identity before deriving a correction. No new GPU run or full regression claimed in this entry.

#405 2026-09-08 8eef3db66 plus WIP actual runtime camera/light controls | Added synthetic-only --capture-reverse-camera and --capture-zero-light; both reject combination with a prepared game snapshot. Actual60-frame reverse camera reachesX-0.5:exit0, viewed right-shifted geometry, analytic IoU0.995491459 at-0.5 versus0.296372435 at deliberately wrong+0.5. Zero-light control sets only registered distant-light radiance to0, preserving scene/camera; actual60-frame capture exits0 and shows dark output with0 bright pixels versus35716 lit baseline, confirming observable response to supplied lighting. Target/runtime+SDK builds pass;133 SDK tests pass. This is real runtime control evidence but not moving game footage, depth-order proof, physically validated lighting, or cleanup acceptance. Source inspector and new controls remain WIP pending unit tests/full regressions; retained external BMP/logs include synthetic-reverse-camera and synthetic-zero-light. Next resolve normal-buffer sampling/overlap and capture unit coverage, then checkpoint without closing M1-GPU prematurely.

#404 2026-09-08 8eef3db66 synthetic camera/output diagnostic | Ran60-frame corrected-alpha normal capture through actual runtime, exit0; viewed signed-float visualization showing geometry rather than prior uniform gray. Added read-only remake_capture_geometry_check.py for harness640x480 BMPs, analytic union of Synthetic triangles at cameraX0.5,90degree vertical FOV/aspect4:3; no gameplay acceptance or automatic pass threshold. Actual synthetic-alpha7 final bright coverage:expected35550,observed35716,intersection35549,IoU0.9952963575. Deliberately reversed analytic cameraX-0.5 givesIoU0.2963111176. Normals visualization yieldsobserved42713/IoU0.8322993000, materially larger than final silhouette: do not use it as pixel-aligned truth until sampling/output mapping is understood. This control changes analytic expectation, not runtime camera; actual reversed-camera and light/depth controls remain required. Initial git status was mistakenly issued in the external runtime folder and failed; reissued in repository successfully. New inspector remains WIP/uncommitted; existing exact-source rebuild remains pending, no full gate claim.

#403 2026-09-08 716a36493 plus runtime diagnostic checkpoint | Four serial incremental builds automation/baseline/no-NGX/off exit0. Three enabled selftests324/324 each, strengthened SDK133/133,290 Python inspectors, actual artifact loader positive/13 malformed controls pass. Relative capture and existing-file capture options reject before runtime with exit2; no original image overwritten. LOG398-402 contain real failed and successful runtime/image evidence. D-134 records public defaults, separate capture initialization, window-before-Shutdown requirement and remaining undisposed-object failure. Accept this slice only as diagnostic capture/material correction, not M1-GPU closure: analytic scene/light/camera controls, raw float evidence, capture failure hardening and resource lifetime remain open. No new production Flycast rendering behavior or performance claim. Explicit source/docs-only checkpoint; third-party runtime, config, game textures and BMPs remain outside Git. Next exact-SHA check and analytic scene controls, then advance toward moving game output, not repeated transport reproof.

#402 2026-09-08 716a36493 plus WIP actual Soulcalibur snapshot pixels | Extended capture CLI to combine existing --artifact/--assets/--clips with trailing --capture or --capture-normals. Rebuilt target successfully. Executed retained H artifact with14 verified DDS assets, clips0.1..104,60 repeated immutable snapshot frames; output soulcalibur-snapshot-first.bmp and full log kept outside Git beside isolated runtime. Exit0,60 successful Presents,readbackHRESULT0. Viewed image: recognizable textured Hoko Temple buildings/steps, dark and visibly incomplete; no fighters/full frame claim. Artifact remains sampled17-mesh diagnostic with6 omissions, calibrated camera aspect, source1782/SHA755b90f8e; no moving game/camera/complete depth acceptance. Runtime still reports40 undisposed common device objects. Previous strengthened alpha/opacity SDK assertion was rebuilt and133/133 passed after LOG401. Full regressions, capture malformed-path controls, exact image/scene controls and source commit still pending. This replaces mock-only status with actual bounded game-derived snapshot evidence without promoting M2-scene or combined pipeline.

#401 2026-09-08 716a36493 plus WIP first visible synthetic Remix readback | Added public normals capture: initial UNORM control black; corrected diagnostic surface to RGBA32F and mapped signed normals for BMP viewing, yielding uniform gray (no visible geometry). Reviewed pinned public remix.h constructor: opaque alphaTestType defaults7, whereas adapter zero-initialized it. Changed only adapter alphaTestType to7 and reran60-frame final-color capture. Actual captured synthetic-alpha7.bmp visibly contains the two overlapping triangles, unlike retained synthetic-copy.bmp near-black control. Runtime exits0,60 Present successes/readbackHRESULT0;40 undisposed objects persist. This is visible synthetic scene evidence only, not analytic overlap/depth/light/camera or game acceptance. Target builds pass; existing SDK133 checks pass before adding explicit opacity/alpha assertion. Added that assertion; rerun required. All images/logs outside repository. No third-party config/binary edits. Next run focused regression/negative material control, normals/analytic camera comparison, reduce synthetic overexposure for distinguishable overlap; retain cleanup issue and complete capture interface bounds before committing WIP.

LOG400 numeric correction: the initial0/307200 nonblack statement below was a transcription error, not the command result. Actual byte inspection reports307200/307200 nonblack pixels and10 unique RGB colors, visually near-black. Therefore this is not an exactly zero-filled image; scene visibility and output identity remain unproven. Retain this correction with the failed attempt.

#400 2026-09-08 716a36493 plus WIP public output capture | Read pinned public RemixSDK.md, C example and public rtx_remix_api.cpp source, not binary internals. Added standalone-only --capture ABSOLUTE_NEW_BMP path using public CreateD3D9/RegisterD3D9Device, final-color CopyRenderingOutput, D3D9 GetRenderTargetData/LockRect and create-new BMP writer. No live config changes. Target builds successfully. Actual60-frame capture exits0/readbackHRESULT0 but visually black. Changed only capture swap effect DISCARD to COPY and repeated60 frames: again exit0/readbackHRESULT0, visually black; this falsifies discard preservation as a sufficient correction. synthetic-copy.bmp has0/307200 nonblack RGB pixels. Both retained outside repo with capture-first.log/capture-copy.log and BMPs. Both report40 undisposed common device objects; NRC initialization fails and runtime logs fallback to importance-sampled indirect illumination. No scene/output acceptance: need distinguish absent illumination/geometry from wrong output-copy timing or target using depth/normal controlled readback. Owned-device route and standalone route remain separate; API Shutdown owns registered-device teardown per reviewed public source. Implementation WIP; no full regression/source commit or GPU-image proof claimed.

#399 2026-09-08 716a36493 plus WIP standalone cleanup diagnosis | Added flushed harness markers; rebuilt only remake-runtime-smoke successfully. Same synthetic3-frame runtime now positively reports all3 submits/Present and Shutdown success, then process exit-1073740771 during subsequent cleanup. Preserved boundary-run.log outside Git. Moving DestroyWindow before FreeLibrary but after Shutdown still fails inside DestroyWindow (window-order-run.log). Moving DestroyWindow before Shutdown and FreeLibrary exits0 with3 Present successes;120-frame moving synthetic run also exits0 with120 Present successes. Both report runtime error at process teardown:37 and40 common device objects respectively not disposed of. These counts are not proof of growth per frame; resource completion/cleanup remains open. No pixel readback/image proof, no game scene or combined consumer test. Changes remain WIP, no full regression or source commit claimed. Next inspect public standalone ownership contract and implement actual output capture; retain cleanup error as acceptance failure, not clean-shutdown proof. External logs: window-before-shutdown-run.log and window-before-shutdown-120.log under isolated official package directory. Runtime binary internals untouched.

#398 2026-09-08 356ea0e9e actual runtime bring-up, failed | User explicitly authorized downloading official Remix1.5.2 release after discovery. Archive231778218bytes verified against GitHub SHA256 cc424be4dd1a0c6fd922bc6a7f8e5f6582baea7043a38afa6686d8b6faabad01 and extracted outside repository with dependencies intact. Ran existing standalone remake-runtime-smoke --runtime [external runtime]/.trex/d3d9.dll --frames3 from isolated package directory. Runtime reports remix-1.5.2+68edea01, RTX5090, driver616.56, Vulkan1.4.351 and640x480 swapchain initialization. Process handle37702 subsequently terminated exit1, no harness summary, Present acceptance or captured image. Last runtime log includes invalid swapchain-handle warning and subsequent swapchain recreation; root cause not established. Runtime-created logs/cache remain outside Git. No binary internals inspected or live consumer configuration changed. This supersedes missing-runtime dependency: M1-GPU doing, next flushed public-call boundary instrumentation and focused diagnosis, then real readback/controls. M2 camera/gameplay/combined presentation remain unproven. No new build/selftest claimed for this documentation-only checkpoint.

#397 2026-09-08 c28fc95c2d33fa5a98a8496b234f18310acc0f38 exact snapshot checkpoint and dependency stop | four fresh serial configure/builds pass,3x324 selftests,133 SDK checks(runtime/GPU/Present false),290 inspectors,actual loader positive/13 controls pass. Exact snapshot harness validates H then exits3 for confirmed absent runtime,source identity/6omissions retained. Pushed fork and verified exact SHA/clean source worktree. Same unavailable-runtime dependency persisted through LOG395,396 and this turn; bounded loader/CLI/regression work is now complete. All downstream gameplay/presentation/hardening acceptance requires actual runtime output; no ready independent card remains. Mark current none and M1-GPU/M2-camera blocked pending explicit compatible supplied x64 Remix runtime path/dependencies. Do not infer missing runtime from DLSS consumer presence, download prohibited binaries, manufacture camera acceptance or keep expanding mocks. Goal remains unachieved; source artifact/native/provenance evidence preserved.

#396 2026-09-08 906688af1 snapshot integration regression closure | four serial builds pass,3x324 selftests,133 SDK checks(runtime/GPU/Present false),290 inspectors and actual loader positive/13 rejection controls pass. Documented explicit snapshot CLI, null game-clip semantics, preserved camera aspect and non-gameplay repeated-snapshot limitation in REMAKE-RUNTIME-BRINGUP.md. Scoped CLI integration accepted; no runtime DLL supplied/loaded, Startup/Present/readback/completion remain untested. Checkpoint/exact verification next, then real runtime dependency is required for meaningful rendering bring-up; do not grow mock-only work to avoid it. No game assets/third-party config/binaries staged.

#395 2026-09-08 906688af1 runtime snapshot connection WIP | standalone harness now accepts optional --artifact ABSOLUTE_JSON --assets ABSOLUTE_DIR --clips NEAR FAR. Loads/validates before DLL loading; snapshot uses SubmitDiagnostic, retains camera/aspect/source frame, does not apply synthetic camera movement, and labels moving_gameplay_proven=false. Automation build passes. Explicit confirmed-absent runtime control with actual H artifact/clips0.1..104 validates snapshot then exits3 runtime unavailable;clips0.1..100 exits2 before runtime load. Original synthetic CLI with absent runtime still exits3. No DLL loaded, no windows/Present/GPU proof. Remaining builds/tests and CLI documentation before checkpoint; source changes WIP.

#394 2026-09-08 906688af100536cc6af2154a492cd5d196fde014 exact loader checkpoint | four fresh serial configure/builds pass,3x324 selftests,133 SDK checks(runtime/GPU/Present false),290 inspectors and actual loader positive/13 malformed controls pass. Pushed fork and verified exact SHA before runtime-harness edits. No game asset/configuration/binary committed.

#393 2026-09-08 88729193a loader validation closure | durable artifact_loader_controls.py runs the actual loader-only process on retained H artifact and13 temporary malformed cases: wrong schema/sourceSHA/coordinates/source clips/draw ownership/index/normal/palette/filename/hash/omissions, malformed JSON and depth40. Positive exits0, all controls exit1 with artifact rejection; no runtime calls. Temporary owned JSON controls removed on completion, original artifact/assets unchanged. Baseline/no-NGX/off builds pass after prior automation build; enabled selftests324/324 each,SDK133/133 and290 inspectors pass. Loader accepted for bounded CPU file ingestion only, not binary loading or GPU rendering. No source media/assets/configuration staged. Checkpoint/exact verification next, then explicit runtime harness connection preserving calibrated aspect and diagnostic limits. Original Windows macro build failure retained in LOG391.

#392 2026-09-08 88729193a actual C++ loader check WIP | added loader-only remake-artifact-check target (no Remix DLL loading), retained sourceGitSha in Packet with length/byte accounting, checked draw binding ownership and asset semantic. Generated new external fc067-prepared-h-88729193a.json from revalidated ancestry/conversion/verified-publication/preparation, no overwrite. C++ load with caller0.1..104 exits0:17meshes,6456vertices,frame1782,T1401N,sourceSHA755b90f8e,aspect1.22666657,6omissions,runtime/rendered false. Caller far100 exits1 clip-unsupported. Automation build,324selftests and133SDK checks pass; source path/code files remain uncommitted. Still required malformed-input/asset controls, other builds and explicit runtime CLI integration. This is diagnostic ingestion, not gameplay camera or GPU proof; palette-backed bindings remain unsupported rather than guessed.

#391 2026-09-08 88729193a C++ artifact loader WIP | LoadDiagnosticArtifact uses existing MIT JSON parser with32MiB input/32 nesting cap,128meshes and aggregate geometry prechecks, explicit diagnostic clip arguments, calibrated camera axes/lens, null source-game clip requirement, retained omissions, raw UV/BGRA and derived normals. Resolves only generated asset filenames, checks declared bytes/SHA256 via Windows BCrypt and binding hash before diagnostic readiness; non-null palette identity currently rejects explicitly. Linked only into SDK test target so far, no runtime CLI activation. First compile failed because Windows near/far macros erased parameter names; renamed to clipNear/clipFar. Retained fc067-loader-build.log failure and successful fc067-loader-build-b.log. Automation build and existing133 SDK checks pass, but none exercise loader yet. Remaining required: source identity retention, malformed/positive actual artifact tests, other configurations and real-runtime CLI connection. No acceptance/commit, third-party binary/configuration or production render change.

#390 2026-09-08 05fe3d9ac1414b9b965ec7c9289c8da450e7fbde exact diagnostic-entry checkpoint | four fresh serial configure/builds pass,3x324 selftests,133 SDK checks(runtime/GPU/Present false),290 inspectors pass. Pushed fork and verified exact SHA/clean worktree. Inspected existing core/deps/json/json.hpp3.10.5 MIT notice; reuse rather than new dependency. Current synthetic runtime loop overwrites camera aspect from window dimensions: diagnostic artifact ingestion must not apply this to measured camera aspect. Next implement bounded32MiB prepared-file ingestion with existing mesh/vertex/index/packet caps and fail-closed asset paths/content; preserve separate explicit diagnostic clips and synthetic CLI behavior. No new runtime load, external configuration or production rendering change.

#389 2026-09-08 d1c59e204 diagnostic submission implementation | added SampledAnchor coordinate label, ReadyForDiagnosticAdapter and SubmitDiagnostic with explicit caller clip declaration. Shared structural/material/normal/clip validation remains; ordinary ReadyForAdapter still rejects sampled coordinates and omissions. Diagnostic route requires supplied camera provenance and nonempty limitations, retains original Packet omissions and diagnostic flag across redraw, and emits diagnostic-api-submitted-not-rendered-or-presented rather than ordinary success text. Eight checks cover ordinary rejection, undeclared clips with zero calls, retained limitations, redraw continuity, rejected provenance change, erased limitations, missing normals and excluded geometry. Four serial builds pass;3x324 selftests,133 SDK checks(runtime/GPU/Present false),290 inspectors and diff check pass. Scoped public-ABI diagnostic route accepted, no actual prepared-game import or runtime load. Next checkpoint/exact verification, then bounded prepared-artifact ingestion into standalone runtime harness. No production renderer or third-party changes.

#388 2026-09-08 2620f8db8cd0a8d6c460540a7e11fd533c173dfc exact clip checkpoint | four fresh serial configure/builds pass,3x324 selftests,125 SDK checks(runtime/GPU/Present false),290 inspectors pass. Pushed fork and verified exact SHA. Inspected adapter ownership/ReadyForAdapter: sampled-anchor data cannot be submitted honestly as World/no-omissions. D-133 defines a separate diagnostic entry point retaining coordinate/clip provenance and exclusions across redraw, while sharing safety checks and preserving ordinary readiness. Next implement this interface; no generic bypass, cleared omissions or gameplay acceptance. No external runtime/configuration or production renderer changes.

#387 2026-09-08 3a30f5ea1 clip regression completion | four serial builds and3x324 selftests pass,SDK125/125 remains runtime/GPU/Present false. Combined with290 inspectors and actual H positive/negative interval checks, clip checker accepted for CPU diagnostic containment only. Checkpoint next; no camera/gameplay/runtime acceptance.

#386 2026-09-08 3a30f5ea1 explicit diagnostic clip WIP | diagnostic_clips validates positive finite caller interval, normalized forward axis and bounded prepared vertices, reports all out-of-range source draw/split-vertex depths without altering artifact/camera.290 inspectors pass including bad ranges, exclusion and source nonmutation. Actual H preparation:6456 split vertices,depth0.8441100120544436..102.98729705810548; caller0.1..100 excludes27 split vertices,0.1..104 excludes0. These27 correspond to repeated triangle vertices, not a contradiction of17 unique source vertices in LOG385. Source JSON unchanged, game near/far null, renderable=false. No automatic range choice/default or runtime claim. Four-build regression running at entry creation; record terminal result before checkpoint, then developer submission connection with explicit diagnostic provenance.

#385 2026-09-08 87abbd3c4 native clipping and actual H depth envelope | revalidated standard-H selected ancestry/tape/ledger and expression reconstruction:3682 vertices,minimum view depth0.8441100120544434,maximum102.98729705810547,zero nonpositive. Synthetic near0.1 excludes0;far100 excludes17, falsifying use of existing synthetic camera defaults for this sample. Native DX11 creates none/front/back cull states from DepthClipEnable=false descriptor; normal pixel shader writes logarithmic SV_DEPTH from PVR-derived reciprocal depth. These source facts do not recover upstream game clipping or unseen geometry. D-132 requires separate caller-supplied diagnostic interval with all-vertex enclosure checking and null source game clips retained. No production code, build, renderer capture, GPU/runtime or acceptance change this turn. Next implement that explicit validation, not a hidden synthetic default.

#384 2026-09-08 60610246bb033086a725d216eb8b2cdeb7573461 exact preparation checkpoint | four fresh serial configure/builds pass,3x324 selftests,125 SDK checks(runtime/GPU/Present false),289 inspectors pass. Pushed fork and verified exact SHA/clean worktree. Inspected pinned public header CameraInfoParameterizedEXT nearPlane/farPlane and native DX11 vertex shader four side clipplanes; neither supplies recovered game near/far semantics. Search for conventional nearPlane naming in native render files found no hits (search exit1 is absence, not proof no clipping exists). Matrix API alternative is not a substitute for missing projection evidence. Next inspect actual sampled view-depth envelope/native clipping behavior, retaining distinction between diagnostic enclosure and recovered game clips. No new render/runtime/physical-camera acceptance, no binary/configuration changes.

#383 2026-09-08 471950730 scene preparation regression | four serial builds and3x324 selftests pass; SDK125/125 remains runtime/GPU/Present false. Together with LOG382's289 inspectors and actual H assembly, bounded preparation accepted for in-memory diagnostic scene artifact only. Commit next, then exact verification and remaining coordinate/clip/runtime submission gates. No game rendering, camera acceptance or performance claim.

#382 2026-09-08 471950730 scene preparation WIP | prepare requires verified publication, converted/reflected anchor and fixed sequence camera. Bounds128draws/65536split vertices before actual C++ --flat-normals calls, checks count/unit finite normals, rejects any omitted face until explicit omission mapping exists, preserves original raw vertex attributes/material bindings and all exclusions. Camera near/far serialized as null with accepted_game_camera=false; no implicit synthetic defaults.289 inspectors pass. Actual standard-H ancestry plus source-anchor conversion/fixed-origin/publication join prepares17meshes/6456split vertices,3003593JSON bytes,near/far null,renderable=false. Actual C++ normal conversion runs once per selected draw. In-memory artifact only, no new file or runtime submission. Four-build/selftest regression running at entry creation; terminal result required before checkpoint. Next evaluate explicit experimental submission gates, not further asset-only work.

#381 2026-09-08 12c1d20d01c6e7eeb8f2eccdcb19fbf173104517 exact join checkpoint | four fresh serial configure/builds pass;3x324 selftests,125 SDK checks (runtime/GPU/Present false),287 inspectors pass. Pushed fork and verified exact SHA/clean source worktree. Read current working-pipeline acceptance: requires300 moving combined gameplay frames and600-frame normal/OIT performance evidence, neither available. Updated sampled coordinate document to distinguish derived normals/verified texture join from runtime rendering, and parked captured-rounding failure from passing mathematical CPU projection. No further asset-only validation phase: next bounded scene artifact must retain unknown clips, coordinate/coverage exclusions and source identity rather than silently satisfying readiness with synthetic defaults. No new runtime or image-quality claim.

#380 2026-09-08 5d8636b2a join regression completion | four serial builds pass;3x324 selftests and125 SDK checks pass, still runtime/GPU/Present false. Combined with287 inspector tests and actual H join in LOG379, published loader/join accepted for bounded evidence-artifact assembly only. Checkpoint next; experimental rendering, complete scene and camera acceptance remain open.

#379 2026-09-08 5d8636b2a geometry/publication join WIP | expression evidence mesh now retains source Git SHA. remake_asset_join_inspect joins exact frame/game/SHA, unchanged source vertex attributes, original source triangles (or explicitly reflected winding), per-draw ownership and reverified published DDS assets. Existing omissions and strict failure flags retained.287 inspectors pass, including wrong frame/game/SHA, attributes and topology controls. Actual standard-H ancestry/tape/ledger replay yields17 draw packets,3682 vertices,2152 triangles and14 verified published assets; strict_pass=false and renderable=false remain. This is not world/camera/GPU acceptance. Regression builds/selftests in progress; record terminal result before checkpoint. No capture files or external configuration modified.

#378 2026-09-08 5d8636b2a published loader WIP | verify_published recomputes strict capture bundle, bounds manifest size, compares frame/game/source SHA/bindings/asset metadata to capture-derived expectations, and byte-compares every fixed-name DDS. Manifest-provided paths are never followed. Synthetic wrong-frame manifest and same-size changed-file controls reject;286 inspectors pass. Actual external H publication reverified14 assets/17 bindings against original raw capture. No runtime loading or geometry acceptance. Next join reconstructed artifact identity and per-draw geometry to this verified publication, preserving exclusions; source loader changes uncommitted.

#377 2026-09-08 5d8636b2afd948f61e8946b51b9a82ab3af1ef4a exact publisher checkpoint | four fresh serial configure/builds pass; enabled selftests324/324 each, SDK125/125 explicitly runtime/GPU/Present false, Python286/286. Pushed fork and verified exact SHA/clean worktree before subsequent loader edits. No new game rendering or performance evidence claimed.

#376 2026-09-08 ca2dd40b9 publication regression completion | four serial build configurations pass;3x324 selftests and125 SDK checks pass (runtime/GPU/Present false), diff check passes. Combined with LOG375's286 Python tests and actual external14asset readback, publisher slice accepted for scoped file generation only. Checkpoint next; no new real-game rendering, native parity, camera, GPU or performance acceptance.

#375 2026-09-08 ca2dd40b9 DDS publication WIP | publish_capture performs existing capture/mip verification before create-only output directory creation, writes DDS with exclusive file creation, verifies SHA256 readback, writes/readbacks pending manifest then renames it last. Existing output rejects; failed hash leaves no completion manifest and no cleanup of user data.286 Python tests pass including create-only/readback/failure checks. Published actual reconstructed H selection to external evidence directory fc067-source-dds-ca2dd40b9-h:14 files,17 bindings,3846848 DDS bytes; independently reread all hashes successfully. Manifest retains source frame/game/SHA/generations, semantic exclusions and renderable=false. No original files overwritten, external configurations changed or game assets staged. C++ regression builds/tests in progress at log entry creation; record completion before checkpoint. Next connect published manifest to reconstructed geometry artifact, not automatic runtime acceptance.

#374 2026-09-08 f242bc1b209ccd531e65a7a3bb67bce5af12d70e exact verification and reconstructed selection | four fresh configure/build configurations pass serially;3x324 selftests,125 SDK checks (runtime/GPU/Present false),285 Python tests pass. Actual standard-H ancestry/tape/ledger and four-draw-transform-C selected expression mesh revalidated. Its3682 vertices/2152 triangles map to draws1,26,44,70,1071,1114,2180,2246,2378,2410,2462,2660,2715,2771,2923,2990,3014. Same frame1782 material capture verifies17 bindings to14 assets,3846848 in-memory DDS bytes. Every selected draw is represented; no output file publication or renderer acceptance claimed. Strict camera/complete-scene failures retained. Pushed fork and verified exact SHA; clean source worktree. Next publish bounded owned assets/identity manifest without overwriting existing output or committing game data.

#373 2026-09-08 cbd44afba source bundle validation | source_dds.capture_bundle reuses strict scene/material identity and raw mip hash checks, produces bounded in-memory UNORM DDS assets with SHA256, source mip hashes, per-draw/slot TCW/TSP and upload/palette/RTT association; no file publication or physical-albedo claim. Actual standard-H retained frames1782/1783/1784 each tested first17 textured opaque draws:17 bindings to1 asset,262292 DDS bytes/frame. All generated payload hashes and binding links agree;9 stale upload/palette/RTT controls reject. This selection is not the17 reconstructed fighter/arena draws and does not claim that geometry join. Positive bound texture reaches public mock material/draw callbacks. Four builds,3x324 selftests,125 SDK checks,285 inspectors and diff check pass. Scoped caller binding/in-memory conversion accepted; owned file publication and reconstructed selection join remain next after checkpoint/exact verification. No third-party configuration/binary or game asset committed.

#372 2026-09-08 cbd44afba source-texture binding WIP | Material now optionally carries explicit source TextureIdentity. Known mesh texture may pass only with matching known material ID/upload/palette/RTT tuple, sourceColorExperiment and valid existing DDS. Unbound known textures retain old rejection; mismatches fail before API allocation. Nine checks exercise matching readiness, five identity/known negatives with zero API calls, missing asset, unowned binding and preserved incomplete-scene rejection. Four builds pass,3x324 selftests,124 SDK checks (runtime/GPU/Present false),284 inspectors and diff check pass. D-131 distinguishes caller tuple consistency from capture-byte/asset provenance and native PVR shading. Still WIP: verified capture importer association and positive bound API transport next before commit. No production renderer, external configuration or proprietary binary changed.

#371 2026-09-08 d314b594ade34818e280a70f3660abce8d4dd5c2 exact checkpoint | all four configurations freshly configured and serially built at exact committed SHA, each exits0. Fresh enabled selftests324/324 each, SDK115/115 explicitly runtime/GPU/Present false, Python284/284. Pushed fork feat/neural-rendering and verified exact remote SHA with clean source worktree. Existing H geometry conversion proof stays scoped to LOG370; not rerun as runtime evidence. Next source-texture integration must bind DDS assets to captured identity and upload/palette/RTT generation, reject stale associations, retain explicit source-color-not-physical-albedo semantics and leave camera/incomplete-scene rejection intact. No live configuration or proprietary dependency modified.

#370 2026-09-08 6c8964f4a flat-normal WIP validation | added bounded --flat-normals diagnostic mode to existing camera-project executable, leaving projection protocol unchanged. Actual standard-H ancestry/tape/ledger and four-draw-transform-C topology passed existing same-capture inspector with calibration614.7144309686947/565.5372185498106; converted source anchor/reflection/winding and one fixed origin before actual C++ call.3682 vertices/2152 triangles produce6456 split vertices, zero degenerate omissions. Independent numpy cross products on the identical float input coordinates agree with maximum component error3.0178264442959346e-08. Strict reprojection failures are retained; no camera/runtime acceptance. Synthetic strip break parity and worst-case pre-omission expansion negative pass. Four serial builds,3x324 selftests,115 SDK checks,284 Python tests and diff check pass. Exploratory literal wildcard searches returned Windows path errors; explicit file enumeration corrected discovery, no evidence discarded. Scoped conversion ACCEPTED for CPU geometry preparation only; checkpoint/exact verification next, then explicit source-texture integration. No proprietary binary/configuration touched.

#369 2026-09-08 6c8964f4a WIP flat-normal conversion | implemented explicit DeriveFlatNormals in developer scene module. Rejects projected/invalid coordinate enums, malformed topology, invalid/nonfinite referenced positions and preallocation worst-case vertex/index/geometry-byte expansion. Splits triangle vertices, preserves attributes and mesh metadata, counts zero-area/repeated-index omissions, keeps original mesh untouched, labels result geometry-derived-flat. Double-intermediate cross products derive normals in supplied mesh coordinates, not recovered authored/world normals. Four serial builds pass; three selftests322/322, SDK113/113 (runtime/GPU/Present false), Python284/284 and diff check pass. Ten new checks cover attribute/source preservation, reversed winding, proper rotation, repeated/coincident degeneracy, projected domain, budgets, index and infinity. No failed build/test attempt. Not committed or accepted yet: actual reconstructed samples and dedicated strip-break/expanded-budget controls remain next; no game packet submission, native parity or real-runtime claim.

#368 2026-09-08 291bcd250 source normal audit | TA_Vertex3 and AppendPolyVertex3 confirm selected Dreamcast packet carries xyz/UV/base/offset colors, no normals. Vertex normal fields are marked Naomi2 and elan.cpp getNormal/setNormal reads that separate packed signed-normal path. Existing capture correctly retains unknown-for-dreamcast. D-130 selects explicit geometry-derived flat normals, not invented authored smoothing or camera truth; next bounded implementation preserves attributes and budgets split vertices. No source normals recovered, game rendering or runtime test claimed. Two exploratory rg calls used unsupported literal wildcard/nonexistent directory paths and returned errors; corrected explicit file reads and the valid PVR directory supplied the cited evidence. No production code changed this turn.

#367 2026-09-08 acd9b9487bdbadcc0a1e08e8cdda0c8df15eb8bc exact checkpoint | recovered completed serial build state: no live cmake/MSBuild/ninja processes, all four retained exact-build logs end in successful executable linking. Fresh automation/baseline/no-NGX selftests each312/312, SDK103/103 explicitly runtime/GPU/Present false, Python inspectors284/284. Pushed feat/neural-rendering to fork and verified exact remote SHA. Reviewed existing standalone runtime harness and dependency instructions; reviewed cache still contains only public header and two licenses, not a supplied runtime. No binary download/configuration write or runtime success claimed. Next independent scene work addresses unknown normals and explicit source-texture semantics; M1-GPU must await its real dependency rather than further mock expansion. Native eight-pixel discrepancy and strict camera residual remain open. Previous user-facing next-steps reply was planning, not implementation progress.

#366 2026-09-08 DDS header closure | narrow loader requires serializer flags,pitch,depth,pixel-format flags,caps/reserved values and DX10 alpha mode, not just magic/dimensions. Six mutated-header cases plus missing-file reject in actual C++ readiness. Four builds pass;3x312 selftests,103 SDK checks and284 inspectors pass. Adapter source/material/path slice is scoped ACCEPTED for CPU/public-ABI contract only. Runtime loading, native parity, complete game scene/normal semantics remain open. Checkpoint next, then exact-SHA verification and actual-runtime dependency/scene integration; do not expand mock infrastructure as a substitute for runtime proof.

#365 2026-09-08 DDS file/lifetime validation | synthetic152byte1x1 RGBA8 DDS written in unique temporary fixture directory, passed actual readiness and mock material API; retained adapter path remains readable after caller path mutation. sRGB29,151byte truncation and156byte trailing payload reject. Fixture file/directory removed after test. Path allocations count toward aggregate packet bytes,4097character path rejects. Four builds pass;3x312 selftests and96 SDK checks pass. These are file/ABI tests, not runtime texture loading or sampling. Next tighten DDS header flags/caps validation and missing-file guards if required, then checkpoint adapter slice and proceed to actual runtime dependency/scene integration without declaring mock success a working pipeline.

#364 2026-09-08 explicit DDS path interface WIP | Material gains owned sourceDds path and explicit sourceColorExperiment designation; missing designation/relative path reject. CPU readiness checks existing absolute .dds,64MiB limit,DX10 RGBA8 UNORM2D single array and exact bounded mip payload size. Adapter retains wstring paths through lifetime and supplies albedoTexture. Existing captured TextureIdentity.known still rejects: this is not automatic game texture/PBR promotion. Automation rebuild passes311 selftests/90 SDK checks, including two path negatives; positive DDS path/ABI lifetime and malformed-file tests, aggregate path budget and other3builds remain pending. Next finish these before accepting path support or generating captured texture files. No new texture files written or third-party configuration changed.

#363 2026-09-08 independent DDS/color-space evidence | Pillow decodes firstframe14 actual base textures in both explicit UNORM/SRGB DDS variants with exact RGBA bytes:28 independent decodes. Added durable independent decoder check to serializer tests. Pinned public rtx_texture.cpp confirms AUTO preserves assetInfo.format; only FORCE_BC_SRGB triggers conversion. D-129 records source-format/lifetime boundaries. Next activate bounded existing-file DDS paths for explicitly labeled caller materials, retaining owned strings and unchanged synthetic defaults; game texture readiness must require explicit source-texture semantic choice, not silently accept captured PBR equivalence.

#362 2026-09-08 pinned public texture loader and DDS serializer | reviewed e876135b37295dc203ccdb7b20a8089629588201 rtx_asset_data_manager.cpp: findAsset permits DDS only. rtx_remix_api.cpp copies paths into PreloadSource captured by queued command and calls preloadTextureAsset with ColorSpace::AUTO; asset file lifetime must outlive asynchronous consumption. No binary inspection. Added bounded DDS DX10 serializer preserving decoded RGBA mip payloads, explicit UNORM/SRGB choice, no gamma conversion and no runtime color-space-default claim.284 inspectors pass with header/payload/color-format and malformed-chain controls. Actual firstframe14selected assets serialize in memory; no user files overwritten. Next validate these bytes with an independent DDS loader and trace AUTO color-space behavior before adapter path activation; serialization alone is not runtime load/render proof.

#361 2026-09-08 per-surface association/failure matrix | mock now checks mesh hash-specific material handle rather than merely nonnull. Inject first/second material creation failure with null/non-null returned handle: all four discard before camera/draw and release every returned material plus previously created mesh. Automation rebuild and SDK88/88 pass; only mock test source changed this turn, prior4build/3x309 evidence remains separate. Next inspect pinned public texture loader format/lifetime contract before adding path-backed source texture materials; do not assume an RGBA byte array is accepted by albedoTexture or silently reinterpret source color as physical albedo.

#360 2026-09-08 explicit per-mesh material ownership | Mesh carries optional caller-specified albedo/roughness; Synthetic explicitly initializes its old appearance. Readiness rejects unknown/nonfinite/out-of-range material parameters without relaxing texture/normal/scene guards. Adapter creates one material per mesh, retains returned handles even on failed creation, and releases them after mesh resources. Distinct parameter mock fixture, 120frame resource reuse and partial mesh failure cleanup pass. Four builds,3x309 selftests and80 SDK checks pass. This is explicit untextured material transport, not game physical material inference or GPU appearance. Next validate material-create failure with returned handle and per-surface handle association, then add source-texture material support under explicit semantics.

#359 2026-09-08 pinned public BGRA interpretation | current-main web source was inspected only for navigation; final authority is existing reviewed revision e876135b37295dc203ccdb7b20a8089629588201. Its rtx_remix_api.cpp binds HardcodedVertex.color as VK_FORMAT_B8G8R8A8_UNORM. D-128 records exact source link. Added native-BGRA byte packing with independent red/blue/alpha word goldens; no channel swap or gamma conversion. Four serial builds pass;3x307 selftests and77 SDK checks pass. Runtime-specific use of color remains unproven. Initial Windows rg glob invalid, corrected to -g. Next implement per-mesh explicit material parameters/resource ownership rather than one hardcoded synthetic material, retaining strict scene/normal/texture readiness guards until supported.

#358 2026-09-08 public vertex color transport | c6d50ec73 exact4 builds,3x306 selftests,75 SDK checks,282 inspectors and material fixture pass; fork SHA verified, clean before changes. Adapter Vertex now retains raw publicColor defaultwhite and passes it unchanged instead of overwriting withwhite. Nonwhite0x12345678 public mock golden passes, SDK76/76. Reviewed pinned header exposes uint32 color but no channel documentation; no captured BGRA mapping or consumer color interpretation claimed. Public-source web search returned no results. Initial baseline link failed LNK1104 because selftest was started while executable linking; premature old-binary tests are not final evidence. Corrected serial builds then tests pass4builds/3x306. Next verify public consumer channel interpretation from reviewed source or controlled GPU fixture before assigning captured colors; continue material representation without bypassing missing scene/normal guards.

#357 2026-09-08 direct material writer guard tests | C++ material fixture invokes actual WritePvrMaterials with null devices/nonexistent scene and verifies exact rejection reasons for missing snapshot, modified source, NaN fog, infinite clamp and infinite scalar, with no output directory created. Native D3D11-WARP material fixture passes145 raw/RGBA comparisons and19 negatives. Four builds and3x306 selftests pass. Bounded capture/material data slice can checkpoint with D-127 limits; eight-pixel native parity issue remains open and full pipeline acceptance unchanged. Next exact-checkpoint verification followed by public adapter material representation review.

#356 2026-09-08 material slice review | added durable global provenance negatives for missing authority, false source invariant, wrong fog-enable type, nonfinite fog, malformed clamp, frame/game/Git/scene mismatch and TCW bump distinction.282 inspectors pass. D-127 preserves source-color versus PBR, CPU versus GPU and metadata versus native-parity boundaries. Snapshot/material slice remains uncommitted pending C++ finite-guard negative fixture and final review; native discrepancy remains open. Next add direct C++ rejection tests before checkpoint, then move to adapter representation rather than another CPU rendering expansion.

#355 2026-09-08 bounded source-material samples | added explicit-level bilinear repeat reference with texel-center and negative-UV/wrap-seam goldens; no raster derivative/LOD inference. Same-frame new capture textures, UVs, native BGRA vertex/offset colors and verified fog state evaluate3682vertex samples per frame at explicitly chosen mip0.3546/3549/3549 outputs differ from texture-only input; all outputs finite.281 inspectors pass. This proves a connected bounded CPU source-material data path, not final pixel equivalence, actual game LOD, PBR albedo or Remix rendering. Initial rg Windows filename glob was invalid and retried with -g; no rendering evidence derived from that search. Next consolidate material/capture slice with focused negative coverage, then address real adapter-supported material representation and missing normal semantics rather than expanding CPU sampling into a substitute renderer. Native8pixel parity discrepancy remains open.

#354 2026-09-08 same-frame effective globals binding | added identity/provenance/type/finite checks linking shader_globals to source scene SHA/frame/game; bump mode determined from documented TCW PixelFmt4 rather than texture image format. New capture3frames each bind17 non-bump selected draws. First equation invocation rejected color-clamp requested bit; source review confirms native disables FogClamping at default0/1 bounds. Corrected effective-state check accepts only that inactive-default case or requested-off, still rejects active nondefault clamp. All51 selected states evaluate an explicitly synthetic color sample with captured fog; not actual sampled pixels or GPU shading proof. Six frame/provenance mutation controls reject;280 inspectors pass including default-vs-active clamp controls. Next integrate verified texture sampling and per-vertex attributes for a bounded source-material evaluation fixture; retain incomplete normals and open native parity discrepancy.

#353 2026-09-08 direct snapshot-source invariant | diagnostic-only before/after byte comparison surrounds the named CPU field copies after native PixelConstants upload. Entire object representation compared locally; padding never serialized. Material output requires unchanged source flag. Native3frame fc067-fog-source-invariant-6697 capture exits0/clean_close=yes; all3 manifests positively report cpu_snapshot_source_bytes_unchanged=true. Four builds,3x306 selftests and280 inspectors pass. This proves CPU snapshot input nonmutation for the run, not GPU constant readback or full-frame parity; LOG350-352 image differences remain unresolved and retained. No more broad native recaptures for this question. Next bind new same-frame shader_globals to selected source-material evaluation, rejecting absent/invalid provenance. Keep image parity as an explicit open regression item, not a fabricated acceptance or blocker for independent material work.

#352 2026-09-08 same-build material-toggle control | new CPU snapshot executes only when NeuralCapturePvrMaterials is enabled; ordinary gameplay skips its field copies. Four builds and3x306 selftests pass. Same executable, reset save inputs and deterministic replay: fc067-fog-toggle-yes-6697 and -no-6697 each3frames exit0/clean_close=yes. All scene JSON pairs byte-identical; disabled lane has no materials directory. Native1782/1784 exact;1783 differs8pixels maximum1channel. Full-sequence parity still FAILS. This toggle covers existing synchronous material readback as well as the new snapshot, so it cannot isolate which operation or timing causes the difference. Next isolate snapshot copying while retaining material readback, or instrument unchanged GPU constants directly; do not repeat this broad toggle or waive the mismatch.

#351 2026-09-08 native repeat falsification | source scene JSON at1782/1783/1784 equals dff556917 baseline except git_sha. Repeat identical current-binary capture fc067-fog-globals-repeat-6697-wip exits0,clean_close=yes,3frames. Versus prior WIP:1782 differs9pixels max1channel;1783/1784 exact. Versus dff baseline:1782/1784 exact;1783 differs8pixels max1channel. All fog metadata exactly matches across both runs. This demonstrates same-binary native output variation at1782, not complete causal attribution or full-sequence native parity. Do not cherry-pick matching frames across runs. Next use a controlled snapshot-on/off comparison in the same build or equivalent focused isolation, retaining both attempts and separating metadata correctness from image parity.

#350 2026-09-08 fog capture runtime check | finite fog/clamp/scalar rejection added before material writes. Four builds pass and enabled selftests306/306 each. Bounded native D3D11 normal Soulcalibur capture fc067-fog-globals-6697-wip runs3frames, exit0,clean_close=yes with producer tracing disabled. All3 material manifests contain uploaded fog globals: enabledtrue,vertex/RAM RGB approximately0.05882353/0.24705882/0.28235295,density130560,clamp0..1,alpha reference1,shadow scale0.375. Native PNG hashes versus hook-free dff556917 baseline differ at1782/1783 and match1784; parity NOT accepted. Next quantify/investigate mismatch and retain this attempt, not infer old-frame constants or call metadata availability a rendering pass. New state remains uncommitted; no external consumer configuration touched.

#349 2026-09-08 effective fog capture implementation WIP | snapshot initialized fog RGB, clamp RGBA, density, alpha reference and shadow scale from the exact CPU PixelConstants uploaded to the native shader; record Fog toggle separately. Material manifest receives shader_globals with upload provenance; missing snapshot rejects. Render entry invalidates previous snapshot so it cannot silently serve a later frame. Only initialized named fields copied, not padding or optional dither bytes. Initial automation build passes before final frame-entry invalidation edit. Remaining builds/selftests, finite-value guard, focused capture and identity/native-parity verification remain pending; no capture pass claimed. Native rendering equations and third-party configuration unchanged.

#348 2026-09-08 missing effective fog evidence | retained scene packets explicitly omit fog-and-global-register-state. Material sidecars contain filtering override0,anisotropy1,mip bias-1.5 but not effective fog enable/color/clamp constants. Therefore full source shading cannot be proven from these captures. Added selected non-bump mode3 color equation with explicit missing-state rejection: ignore vertex/texture alpha, multiply RGB, add offset, optionally interpolate vertex fog by offset alpha. Unsupported clamp/mode/bump state rejects; no sampler, final framebuffer or PBR equivalence claim. Native clampColor is identity when FogClamping disabled, so no invented saturation step.280 inspectors pass including fog-on/off independent golden and missing-fog rejection. Next add bounded capture of effective shader constants at the renderer-owned capture seam, then verify with a focused native capture; do not repeat producer tracing or infer old-frame fog from a new capture.

#347 2026-09-08 requested draw-state contract | decoded TSP/PCW using ta_structs.h bit definitions, retaining original words and explicit global_overrides_applied=false. H/L/M selected17draws each request offset1,fog_control1,filter_mode1,clamp_u/v0,flip_u/v0,use_alpha0,ignore_texture_alpha1. Native normal/OIT shader source applies offset RGB before vertex-fog interpolation; these cannot be omitted from source-material equivalence.279 inspectors pass with single-bit sampler/alpha/clamp controls and combined PCW/fog/filter golden. This is requested state only: global Fog override, fog color, clamp constants, sampler overrides and effective alpha rules remain to bind before shader evaluation. Next recover those already-captured global values where present and implement the bounded selected-mode color equation with explicit unsupported-state rejection, not generic PBR claims.

#346 2026-09-08 vertex source attributes | exporter layout and native DX11 input descriptors reviewed: historically named color_rgba_bytes are raw packet bytes bound as BGRA8. Added bounded identity-preserving UV/UV1, color/offset and secondary-color conversion with finite UV and byte-range checks, original words retained and normals unknown. Actual H/L/M3682vertex sets have nonwhite colors2784/2782/2784, nonzero offset fields1530/1534/1534 and462 out-of-unit UV vertices each. Nonzero offset fields alone do not establish active offset shading; draw-state handling remains necessary.278 inspectors pass including independent BGRA/offset/negative-UV golden and NaN rejection. First summary command had unmatched parenthesis and did not run; corrected command produced counts above. Next decode active offset/fog/sampling/alpha state from actual renderer bit definitions and preserve corresponding semantics in the scene material lane. No GPU or full shading equivalence claim.

#345 2026-09-08 source-color conversion | selected material join now retains full original draw state. Bounded hash/size-checked source_textures converts formats85/86/87 using existing decoder, with no gamma transform or attempted lighting removal; other formats explicitly reject. H/L/M each convert14assets/30mips/3844776 RGBA bytes in memory; no user assets overwritten. Independent RGB565 red, RGB5551 opaque red and BGRA channel/alpha goldens pass. All selected bindings report shading_instruction3. Native normal/OIT shader source confirms this multiplies vertex color and texture RGBA; offset/fog/sampling remain separate and cannot be discarded by a texture-only material.277 inspectors pass including source-channel goldens and duplicate binding rejection. Next join retained original vertex UV/color fields into the material lane and explicitly account for offset/fog/sampling before renderer integration. No physical albedo, normals, GPU rendering or scene-completeness promotion.

#344 2026-09-08 scene/material binding | checkpoint6697d6e60 exact4 builds pass,3x306 selftests and75 SDK checks pass; fork remote matches6697d6e60d8781ef1fee852883959f4f6c8cfb38 and worktree was clean. New bounded selected-draw material join checks scene/frame/game/SHA, both slots, draw state and upload/RTT/palette generations without claiming PBR conversion. H/L/M selected17draws each bind17textured slots and14distinct assets in formats85/86/87, no custom replacements or nonzero RTT generations. Raw selected mip sizes/hashes checked separately; frame/generation/duplicate-binding mutation controls reject in each frame. Normals remain unknown and adapter remains ineligible. Initial broad manifest display was truncated; corrected to bounded selected summaries. Next convert these verified source texture formats into a labeled source-color material lane, preserving UV/color/shading semantics and explicit missing normals rather than fabricating physical albedo.

#343 2026-09-08 consistent CPU projection precision | use double intermediate subtraction, basis dot products and lens arithmetic in Project, retaining float public inputs/output. With one fixed H-origin translation, all3682vertices in each H/L/M mesh pass unchanged0.001-pixel mathematical projection bound: maxima0.0004116088848604704,0.00045506475180445705,0.0007891734658187488; zero failures. Initial selftest FAIL305/306: recovered inverse residual1.24937e-06 exceeded1e-6 because Unproject retained float intermediate math. Matching double intermediates in Unproject reduces residual6.58208e-07 without tolerance change. Four final serial builds pass, enabled selftests306/306 each and public SDK contract75/75 pass. This is a bounded CPU reference correction, not renderer-shader/Remix GPU evidence. Strict captured-rounding gate unchanged; complete-scene/physical-world/near-far/material semantics remain unsupported. Next consolidate tested camera slice and explicit domain limitations, then move toward actual scene integration rather than repeating precision micro-tests.

#342 2026-09-08 fixed-origin three-frame test | subtract the same converted H camera origin from every source vertex and every H/L/M camera, before float conversion. H camera becomes zero; L/M retain nonzero relative translations, not per-frame recentering. Each3682vertex mesh passes unchanged mathematical coordinate conversion. Actual C++ maximum error H0.0009850629212451167/0 over0.001, L0.0010310647521691862/2 over, M0.0007738390277154394/0 over. Thus fixed rebase alone FAILS the full sequence. All float inputs with double intermediate math yields respective maxima0.0004465442107175477,0.00036108301901549567,0.00043143787479493767 with0 failures, suggesting a bounded arithmetic test next; float output rounding and actual C++ still require measurement.275 inspectors pass including fixed-origin relative geometry/camera-motion preservation and nonfinite-origin rejection. Existing strict captured-reprojection failures remain unchanged, runtime/renderability false. Next test actual C++ double intermediates without changing API input/output types, common origin, source identity or acceptance threshold.

#341 2026-09-08 precision isolation, no production fix | camera_precision_inspect compares actual C++ output with independently evaluated double arithmetic and component-by-component float inputs on all3682 converted H vertices. Origin-only quantization maximum0.001634562333492795 pixels,16 over0.001; axes-only0.00020726915863633621,0 over; lens-only0.0004697940657933941,0 over; source-only0.0003880940821545664,0 over. Entire camera quantized0.0018970872060890542,19 over; all float inputs with double arithmetic0.002163084580388386,23 over; actual C++0.0020168246046523564,16 over. Therefore merely switching arithmetic to double cannot remove input quantization.273 inspectors pass, including independent golden, wrong-output and nonfinite-output controls; subprocess timeout30sec and65536point bound. Next test a fixed common coordinate-origin rebase before float conversion, preserving all relative H/L/M poses and source identity, not per-frame view-space substitution. Strict captured gate and adapter ineligibility unchanged; no runtime/GPU claim.

#340 2026-09-08 full H mesh C++ float falsification | added developer-only bounded stdin projection executable under existing compact-tools option, calling actual remake::Project without renderer/runtime dependencies. Same retained H ancestry, source-anchor conversion and paired reflection feed3682 vertices/2152 triangles. Compared against independent calibrated mathematical projection, maximum pixel error0.002016824599195388 at mesh index2158;16 vertices exceed unchanged0.001. This FAIL is distinct from the already-failed recorded-binary32 reprojection gate, which remains false; adapter eligibility remains false. Single-point LOG339 must not be extrapolated to all geometry. Six executable protocol checks ran: analytic valid input exits0; zero/oversized counts, truncated point, trailing data and malformed camera each exit1. Next isolate float source/camera quantization versus projection arithmetic before proposing a correction; no tolerance relaxation or game clip inference. No capture, proprietary component or live configuration modified.

#339 2026-09-08 recovered camera C++ float/ABI fixture | actual recovered H position, proper axes, vertical FOV and effective aspect exercised in C++ against an independently calibrated origin projection. Golden pixel error7.62939e-05 is below unchanged0.001; inverse source-coordinate residual7.72998e-07 is below1e-6. Wrong framebuffer4:3 substitution rejects. Mock public ABI verifies exact recovered axes/FOV/aspect and explicitly synthetic clip range through camera redraw; it is not gameplay mesh, GPU or presentation evidence. All4 serial builds pass; automation/baseline/no-NGX selftests306/306 each and SDK contract75/75 pass. No production changes or third-party config edits. Next measure full converted H mesh in actual C++ float path, retaining existing strict captured-rounding failure and incomplete-scene rejection rather than promoting the one-point golden to scene acceptance.

#338 2026-09-08 paired source/view conversion | anchor_orientation_inspect applies Y reflection to both anchored source positions and view transform, preserving proper camera basis, and reverses triangle order for reflected source geometry. Actual H mesh3682 vertices/2152 triangles conversion preserves prior mathematical projection at maximum6.366462912410498e-12 pixels; strict captured-rounding gate remains false. Recovered vertical FOV45.99033235267587deg,effective projection aspect1.2266665409901234 derived from sx/sy and640x480, not silently forced4:3. Near/far remains explicitly not inferred; no adapter eligibility/runtime claim.270 tests pass with reflection/winding golden and reflected-camera rejection. Next run actual C++ float projection/public ABI fixture for recovered parameters, since Python double equivalence does not establish float implementation/GPU behavior.

#337 2026-09-08 explicit harness camera orientation | reviewed cached public remix_c.h CameraInfoParameterizedEXT fields position/forward/up/right. Camera now retains explicit default orthonormal axes; validation rejects nonfinite/nonunit/nonorthogonal/reflected bases at1e-6. Project uses basis dot products, Unproject composes axes, readiness checks depth along forward rather than world Z; adapter passes all9 axis components. Analytic90degree yaw projection/inverse goldens, wrong-axis/reflection negatives and roll-to-public-ABI mock checks added. Automation build and other3 serial builds all pass; three enabled selftests302/302 each, SDK mock69/69,269 inspectors and diff whitespace check pass. No actual runtime/GPU rendering or game packet promotion. Next convert anchored source geometry and camera together, preserving handedness and triangle winding, then check recovered-pose projection; Y-only camera flip would be reflected and correctly rejects. Work remains uncommitted after dff556917.

#336 2026-09-08 anchored mesh and concrete adapter gap | anchor_mesh maps evidence Y-up coordinates back to captured normalized Y-down before applying inverse source-anchor pose; retains original payload/strict failures and does not mark Remix eligibility. Actual H integrated mesh3682 vertices/2152 triangles roundtrips at max2.842170943040401e-14; strict reprojection remains false.269 tests pass including explicit Y conversion and retained failure. Inspected current remake_scene Camera and RemixScene::DrawFrame: camera supports position only and hardcodes SDK forward(0,0,1)/up(0,1,0)/right(1,0,0), so recovered rotation cannot be transmitted. Next implement bounded explicit orientation in harness camera/project/unproject/adapter with reviewed public header and rotated/wrong-axis tests; no private runtime inspection or production renderer change. Initial Windows rg path glob failed, corrected with -g and exact adapter filename.

#335 2026-09-08 source-anchored camera transforms | anchored_camera requires identical captured XYZ matrix rows across selected divided variants, supplied positive calibration, existing rigid tolerance and inverse/decomposition residuals. Actual H/L/M integrated lineage runs yield source-basis camera origins(-4.40220,1.17539,4.57924),(-4.42019,1.17172,4.51396),(-4.43600,1.16862,4.45250); maximum normalized orthogonality error1.1804559019168437e-7. These are chosen source-coordinate camera poses, not metric-world or static-arena semantic proof. Captured positive-Y-down convention retained; SDK conversion not yet proven.268 tests pass, including wrong-calibration and inconsistent-matrix rejection. Inspected selected draw state has texture-generation metadata, no sufficient semantic arena label; no inference from that metadata. Next apply anchor to supported expression meshes and validate roundtrips/SDK axis conversion in harness. No production/runtime change or capture this slice.

#334 2026-09-08 selected source-basis continuity | integrated H/L/M runs expose pre-FTRV source/matrix words only for accepted single-divided-contribution vertices. All1622 draw/vertex keys have identical four source words across3frames; no address/step matching substitutes for the accepted per-frame chain. Using first-frame matrix on each later sample produces mathematical projection error>=0.001 at all3244 tests,max1583.2214968632688 pixels (offscreen included, not an image-quality metric).267 tests pass with changed-source/frame negatives and analytic wrong-matrix control. Recorded binary32 projection remains separately checked; this new comparison is mathematical. Consistent source coordinates plus rigid shared motion support a chosen anchor basis, not unique physical world-camera identity. Next derive explicitly anchored scene/camera transforms for supported geometry and inspect draw/material context; no new producer capture.

#333 2026-09-08 calibrated rigidity and accepted draw binding | actual H integrated checker maps divided variants to1622 selected vertices in9 draws: zero-fourth-row2378/2410/2462, homogeneous-one2660/2715/2771/2923/2990/3014; same captured XYZ rows. H/L/M source-set relative transforms after supplied shared calibration have maximum orthogonality error8.728566058824327e-8 under unchanged1e-6 gate, determinants0.9999999902880323/0.9999999516480775.265 tests pass including deliberate shear failure. This establishes a rigid relative candidate transform, not static world/arena or physical camera ownership. Next reconstruct chosen candidate-basis geometry and test selected cross-frame point consistency/wrong-frame controls, while retaining semantic ambiguity and checking scene/material context. No production changes, capture or push this slice.

#332 2026-09-08 candidate relative motion and rank | retained H/L/M analysis of two divided-path multisets runs after full ancestry arithmetic validation.1852-point and255-point sets both homogeneous rank4 (smallest singular values42.9068 and10.9106); no planar/collinear degeneracy. Relative top-three-row affine transforms to first frame agree exactly between candidates(maxdifference0). Raw fourth-row distinction remains preserved; homogeneous row used only in mathematical XYZ analysis. Since the two paths share XYZ matrix rows, their agreement is not independent proof of camera motion or static arena semantics.264 tests pass including analytic translation/rank and singular matrix rejection. Next bind selected draw/vertex coverage to candidate matrices and test normalized relative rigidity; source continuity alone does not establish world camera. No capture/build/push claimed this slice.

#331 2026-09-08 retained cross-frame source-set inventory | new bounded checker groups verified pre-FTRV source words with multiplicity by execution site/matrix, compares exact source multisets without addresses or step IDs, flags multiple matrices per set. H/L/M actual runs yield292/292/294 sets,288 common,8 ambiguous. Largest divided-path candidates1852 distinct points(1945 executions) and255 points each have one matrix/frame with changing words. Not static arena or world-camera proof; includes unselected executions, identical source sets may belong to repeated objects.263 tests passed including changed-point/count rejection, step-number independence and repeated-set ambiguity. First full report was output-truncated; added summary mode and reran for complete counts. Next compare relative matrix transforms/rank of these candidates and join selected draw identities; no new producer capture required.

#330 2026-09-08 exact dff556917 checkpoint pushed | original serial build handle3357 completed exit0 all4 configure/builds. Exact committed three enabled selftests each298/298,260 inspectors, SDK mock63/63,compact13 and ledger contracts pass. Fresh fc067-dff556917-native3frame capture exits0/clean_close; all manifests git_sha=dff556917 and producer cycles7605222912/7608559168/7611895424, active session hook-free. Clean worktree before push. Fork push succeeded and ls-remote verifies dff5569177b5d6df9196db80dde5686245638419 on feat/neural-rendering; upstream origin untouched. No camera/world/Remix GPU completion claim. Next inspect retained source points/composite transforms across samples for a justified consistent scene basis, not further transport or sampled-expression reproof. Evidence-document updates follow this exact source checkpoint.

#329 2026-09-08 focused precommit contracts | SDK mock63/63, compact13 checks and176-byte golden decode(1 record), ledger1000-line golden plus5 concurrent16000-line trials actually pass with child-process exit checking for emitted contracts. Runtime_loaded/gpu_rendered/presented remain false in SDK mock. git diff --check has no whitespace errors. User-path scan corrected from invalid Windows path-glob to rg -g selection, then found no matching paths in producer/expression modules (rg exit1 means no match). Source-only checkpoint follows; no third-party binaries/configuration or captured media staged. LOG328 retains full restored build/selftest results.

#328 2026-09-08 hook-free precommit validation | preserved final probe as ignored fc067-final-probe.patch/header, removed only owned11-file core changes via inverse apply_patch and deleted temporary header (recoverable from retained copy). Initial conversion scope check rejected reversed Git b/a prefixes before mutation; corrected converter applied successfully. git diff core empty. Serial automation/baseline/no-NGX/feature-off builds all exit0; logs fc067-restored-328-*.log.260 inspector tests passed. Three enabled selftests each explicitly verified exit0/passed298 failed0; verbose combined output initially truncated, reran with per-variant exit/summary. Expected capture-rejection negative controls are not suite failures. Added D-125 to preserve expression/rounding/calibration evidence limits. No commit/push/exact-new-SHA runtime claim yet. Next review explicit staging scope and remaining focused SDK/compact/ledger tests, commit and exact-commit validation. Captures/media/probes stay outside commit.

#327 2026-09-08 sampled first-batch frame matrix complete | fresh first-M producer1783, unchanged binary/event/byte caps,240000ms diagnostic timeout completes3frames clean_close/exit0. Integrated checker proves3682 exact-expression vertices/2152 triangles at frame1784; six strict residual failures,max0.0015575012967019575, all8 incident nondegenerate triangles wholly outside a common screen clip plane in recorded and mathematical projections. Rechecked L: five failures,max0.0014602259088860592,all4 incident triangles likewise rejected. Neither finding changes all-vertex0.001 acceptance. BGP construction/queue passes3frames. All3 native PNG and scene JSON pairs M/L byte-identical. First batch H/L/M and second I/J/K3 now cover each sampled producer frame independently. This is not cross-frame object correspondence, absolute world camera, complete scene or real Remix GPU evidence. Next consolidate and checkpoint owned harness work with temporary hooks removed through inverse patch and required serial build/selftest matrix; stop extending this producer diagnostic.

#326 2026-09-08 first-batch next-frame lineage | fresh first-L uses existing binary, producer1782, first batch, BGP,240000ms diagnostic timeout and unchanged event/byte caps.3frames complete clean_close/exit0. Integrated checker proves3682 selected exact-expression vertices/2152 triangles and builds frame1783 bounded mesh. Strict residual failures now five vertices, retained explicitly; prior-frame invisible-outlier finding must not be generalized without checking these triangles. BGP construction/queue checks pass3frames. All3 native PNG and scene JSON pairs byte-identical to K3. Next final first-batch producer1783, then consolidate per-frame residual/coverage evidence and checkpoint; no further producer discovery required. No world-camera/Remix GPU claim.

#325 2026-09-08 clean final second-batch frame | ignored diagnostic launcher now accepts explicit180000(default) or240000ms timeout only. Fresh K3 uses240000, same binary and all frame/event/byte limits; completes3frames clean_close/exit0. Integrated ordinal1783 analysis proves920 selected exact-expression vertices/463 triangles at frame1784 and BGP construction/queue binding across3frames. All3 native PNG and scene JSON pairs byte-identical to accepted J. Strict9-vertex rounding failures remain explicit, mesh not marked Remix-renderable/world-camera/complete. K/K2 failures retained, not retroactively accepted. This completes separate-frame second-batch expression coverage for1781/1782/1783 via I/J/K3, not cross-frame object identity or whole-scene camera. Next first-batch1782/1783 under same bounded diagnostic timeout. No production build or test-suite rerun claimed this evidence-only slice.

#324 2026-09-08 K2 timeout with complete diagnostic artifacts | fresh K2 capture returns captureExit=1/timed out at180000ms. No remaining Flycast/neuraltest processes after terminal result. Three frame directories, complete marker and source ledger exist; shutdown log reaches180.625s. Integrated ordinal1783 checker actually passes920 vertices/463 triangles for frame1784 and background construction/binding across3frames. This is artifact-level diagnostic evidence only, not a clean run-level pass; timeout remains retained. Nine strict reprojection failures remain explicit. Next increase only bounded diagnostic wall timeout to240000ms for clean K3 rerun, preserving all frame/event/byte/correctness limits and performance exclusion. No producer changes necessary and no timeout-output-success relabeling. Backlog consistency check ran/pass earlier this turn.

#323 2026-09-08 interrupted K capture retained | observation handle39569 disappeared after interruption; authoritative process inventory has no Flycast/neuraltest process and K evidence directory contains input replay only, no captured frame. K is incomplete, not a capture/ancestry pass. Preserve directory and staging log; retry under fresh K2 identity, never overwrite K. Prior-turn handoff refresh and exact-threshold diagnostic alignment passed260 tests. No final-stage or camera completion inferred from these edits.

#322 2026-09-08 next-frame ancestry accepted in second batch | explicit producer ordinal accepts1781(default),1782,1783 only; malformed value yields no accepted capture. VALUE and actual memory/copy ownership select same single frame, preserving existing caps. Python copy/tape/scene checks require explicitly matching ordinal and report selected-frame coverage instead of mislabeled first-frame fields. Automation build succeeds, temporary J patch/header retained;260 tests ran/pass including wrong ordinal/default mismatch/out-of-bounds controls. Fresh J3frame native capture took longer than I but remained live and completed clean_close/exit0 without restart. Integrated ordinal1782 checker passes all920 vertices/463 triangles and builds frame1783 expression mesh; strict9-vertex rounding failures remain. Existing BGP construction/queue checker passes4 vertices each of3frames, no background world/material claim. All3 scene JSONs exact to I; native1782/1784 exact,1783 differs8 pixels/max1 channel. Next1783 second batch and1782/1783 first batch with unchanged single-frame caps; no whole-scene camera or Remix GPU completion. Earlier frequent short polls were unnecessary; use bounded longer waits while communicating on future captures.

#321 2026-09-08 expression-derived mesh consolidation | added bounded in-memory mesh builder retaining original vertex payload, draw/record identity, camera-relative XYZ, indexed topology, unknown normals and strict residual failures. Integrated real H and second-I runs build3682/2152 and920/463 meshes respectively, both frame1782 only. Neither marked complete, strict-pass or Remix-renderable; no camera-relative approximation promoted to world-camera truth. No mesh file exported or production consumer enabled. Tests cover duplicate identities, absent matrix witnesses and exact-threshold failure without removing geometry;259 tests ran/pass. Next single-producer-frame selector must update observer and ownership/scene checks coherently: current producer VALUE/ownership gates1781, ownership copy/tape checker also hardcodes1781. Existing BGP flag already captures construction plus queue binding and can accompany later-frame runs. Initial rg glob-as-Windows-path failed; corrected with -g producer*inspect.py. Preserve same caps per separately captured frame, no unbounded3frame ledger expansion.

#320 2026-09-08 second packet batch expression/calibration coverage | automation build succeeds; temporary second-I patch/header retained, old staging log moved to ignored before-second-I log after no live Flycast process. Fresh3frame native capture exits0/clean_close. Integrated --batch2 same-capture checker passes scene/tape binding for920 vertices/463 triangles each frame and detailed first-frame expression lineage for all920/463 across16 draws.429 selected rigid matrices agree with supplied calibration within4.587823434576421e-5, doubled calibration rejects.4335 total exact records;1943 incomplete outside selected coverage remain explicit.26799 observed reads/25304 writes, zero missing observed writers. Nine selected vertices reuse one offscreen record with0.0016548912790312897 projection residual; all5 incident nondegenerate triangles reject common screen plane for recorded and mathematical positions. Strict all-vertex failure preserved. All3 scene JSONs byte-identical to H; native images1782/1784 exact,1783 differs8 pixels/max1 channel/MAE6.510416666666667e-6. Initial parity command used nonexistent native-color.png and failed; corrected to native-pvr-color.png, no nonexistent-image pass.257 tests ran/pass. Background4 vertices and later-frame ancestry/whole-scene/world-camera remain pending. Next consolidate calibrated geometry scope and remaining background/later-frame requirements; no blind producer discovery or claim of Remix GPU rendering.

#319 2026-09-08 outlier visibility closed and next batch bounds | integrated H rerun checks every triangle incident to selected failing vertex. Both triangles are wholly outside common right clip plane in recorded and mathematical screen coordinates, so neither intersects640x480 rectangle. This is sufficient geometric rejection, not a new all-vertex pass or GPU raster measurement; strict0.001 failure retained. Common-plane tests reject all-right triangle while retaining offscreen straddling and boundary-touch cases.257 inspector tests ran/pass. Mapped second packet batch217 addresses overlaps213 of first656; only4 additional addresses required in memory observer. Next reuse existing producer capture with SPARSE_SECOND/PACKET_ONLY (920 vertices/463 triangles, background excluded explicitly), add4 known mapped addresses only under second flag, preserve existing caps/first-frame ancestry scope. No new camera/Remix GPU claim.

#318 2026-09-08 rounding explanation and selected matrix calibration | reran integrated H checker after exposing verified pre-FTRV operands and selected contributor identities. Outlier projects mathematically to8917.752288824597/2236.157021257253 versus recorded ordered binary32 result8917.7509765625/2236.15673828125; extracted predivision values reproduce recorded result exactly with existing rational toward-zero evaluator. New numeric regression retains >0.001 failure rather than altering tolerance or geometry. Offscreen does not imply invisible triangle.1823 selected matrix contributions:1708 rigid calibration witnesses agree within0.00015164787316734873,115 remain general affine; maximum rigid orthogonality residual9.688158935540743e-8. Captured fourth rows1629 homogeneous-one and194 zero are retained as raw metadata; only mathematical top-three-row analysis uses an explicit homogeneous row, never rewriting captured data. Same checker deliberately doubled X calibration and rejected incompatible normalized calibration.256 inspector tests ran/pass; final integrated H rerun exits0 with strict reprojection failure still explicit. No new production build/capture. This supports bounded calibration witnesses, not all-contribution rigidity, absolute world-camera identity or whole-scene coverage. Next quantify visible clipped-triangle impact, then second remaining batch; do not spend another producer discovery pass on explained rounding.

#317 2026-09-08 selected reprojection failure localized | ancestry_scene_inspect optional explicitly supplied calibration now reports selected-record residuals after full same-capture expression/memory/copy/tape/scene checks. Ran H against four-draw-transform-C topology with calibration614.7144309686947/565.5372185498106: all3682 selected vertices recover projected expression shape, zero rejects, one exceeds unchanged0.001 tolerance: vertex11886/draw2378 at0.0013122620966896648 pixels. All-record failure from LOG316 remains retained. Diagnostic process exit0 means analysis completed, not camera acceptance; report explicitly says calibration independently proven=false and retains failures.255 inspector tests actually ran and passed. No production change/build/capture this slice. Next diagnose ordered binary32 versus mathematical projection for the outlier and independently check selected matrix calibration; reprojection cancellation cannot establish calibration. Camera contract remains pending.

#316 2026-09-08 predivision camera-relative extraction diagnostic | expose bounded exact expression nodes, parse shared unit reciprocal and observed320/240 center, extract predivision XYZ and retain separate output depth scale. H4309 records fit projected expression shape:2274 CALC scale1 and2035 divided scale0.949999988079071;18 direct records correctly reject this shape. Using prior explicitly supplied calibration614.7144309686947/565.5372185498106, mathematical reprojection maximum0.001682296220678836 exceeds existing0.001 tolerance across all records; no tolerance increase or whole-domain pass. Camera-relative normalization is conditional on calibration, not proof of its uniqueness or physical world units.255 tests pass including scaled-depth independence and wrong center. Next identify selected-record residuals/outliers, and prove selected matrix calibration separately; never infer camera correctness from reprojection where normalization cancels algebraically.

#315 2026-09-08 first remaining batch full expression coverage | standard-H capture3frames exits0/clean_close; integrated full-session acceptance/arithmetic/ordered expression/memory/copy/tape/scene checker passes all3682 selected vertices and2152 triangles across17 draws in producer1781. No selected other-copy-path gap remains.26640 tracked reads match25160 preceding writes with zero missing writers.14764 total observed copies include6202 outside tracked read domains; they are not promoted. All3 native-color PNG and all3 scene JSON pairs are byte-identical to G. Build/253 tests pass; H patch/header retained. This closes first-batch source-expression coverage only: later frames have scene binding but not same detailed ancestry, second remaining batch and background/complete camera contract still separate. Next retain exact source expressions for calibrated camera-relative reconstruction, then extend the second bounded batch without rebuilding already-proven transport.

#314 2026-09-08 standard copy ownership bounds | existing standard XYZ read callback now logs actual cc7c/cc7e/cc80 reads through the same bounded memory observer; accepted pre-copy path logs same copy identity schema as alternate path. No broader address/frame/event budget; existing invalid-candidate omission and selected-tape mandatory checks remain. Standard/alternate copy records share chronological IDs and actual TA offsets. Build/capture pending; this does not add matrix truth or bypass incomplete histories.

#313 2026-09-08 exact expressions accepted-scene join | ancestry_scene_inspect runs full-session/tape/scene acceptance, ordered arithmetic, byte memory edges and record/copy binding together. G yields1990 selected vertices with exact expressions and1096 wholly supported triangles in firstframe. Draw70 covers67/280,1114 covers79/296,2246 covers64/64, prior9 draws cover886/886; partially covered triangles excluded. This is accepted bounded source-expression-to-scene evidence, not camera-relative 3D reconstruction. Other1692 selected vertices use standard copy path, which current ownership COPY logger does not cover.253 tests pass. Next connect existing standard gather sites cc7c/cc7e/cc80 and standard copy identity into same ownership stream to avoid leaving supported known machinery outside unified coverage; do not infer missing copy events from matching bytes.

#312 2026-09-08 ordered expressions reconstruct outputs | expression tracking now carries ordered fadd/fmul/fdiv nodes through verified stores/loads, preserves captured literals and independently verified FTRV leaves, and invalidates nodes on unknown writes/gaps. Evaluating per-node binary32 rounding reproduces4327 completed XYZ records exactly, matching supported contribution-set count;1951 remain incomplete. Wrong summation-order fixture has same contributors but yields1 versus0 and distinguishes order; unknown expression/zero divisor reject. Propagation-only mocked fixtures also mock expression evaluation explicitly; separate real-ledger expression execution and numeric tests are not mocked. Full253 tests pass. FTRV leaves rely on existing exact-rational matrix checks, not inferred source space. Next join exact expression IDs to selected scene acceptance and derive calibrated camera-relative geometry without claiming absolute world-camera decomposition.

#311 2026-09-08 selected ancestry sets and controls | same ancestry-G record/copy/tape identity join finds all1990 selected alternate consumers have supported matrix contribution sets, including368 CALC consumers. Other1692 selected consumers remain outside this alternate-copy analysis. Isolated propagation tests verify memory-origin continuity, gap exclusion, intervening same-value overwrite invalidation, and retention of2 matrix contributions; these mocks test propagation only, integrated real G arithmetic/edges ran separately. This validates contribution membership, not yet an independently reconstructed multi-contribution expression or camera space. Next retain ordered arithmetic DAG/rounding for accumulated points and reconstruct selected outputs; do not mistake unordered contributor sets for full expression equality.

#310 2026-09-08 composed contribution sets diagnostic | new producer_ancestry_inspect composes verified executed memory edges with dispatch/register origins; memory writes invalidate old tags, readm inherits observed memory tags or UNKNOWN, and FTRV creates a distinct execution contribution. Fresh geometry registers after dispatch gaps are UNKNOWN. G reports4327 completed records with matching nonempty XYZ sets:2035 divided,18 direct,2274 CALC(1492 one contribution,549 two,174 three,56 four,3 five).1951 remain incomplete. This is diagnostic composition pending focused falsifying controls and selected-consumer validation, not full ancestry acceptance. Earlier candidate initialized gap inputs empty; changed geometry-input defaults to UNKNOWN and reran, same counts. Preserve every contribution rather than last-matrix substitution. Next test provenance invalidation/missing contributions and join supported sets to selected CALC consumers before camera semantics.

#309 2026-09-08 executed memory edges | ancestry-G full3frame source/scene acceptance passes; all3 scene JSONs exact versus F, native images first2 not byte-identical and third exact. New checker binds STORE event to matching same-step actual MEMORY write bytes/PC, invalidates identity at every later write, then checks observed READ/VALUE against that exact store identity. G verifies10059 edges including INITIAL->accumulation, accumulation->accumulation and both->CALC;zero tracked reads lack executed store identity,42340 reads outside tracked RAM domain explicit. Wrong load word and wrong writer-PC controls added. This is edge proof, not yet transitive matrix contribution lineage; next compose dispatch/register origins with these memory edges for selected CALC outputs, preserving accumulated contributions rather than choosing only last FTRV.

#308 2026-09-08 ancestry-G bounded arithmetic | build and248 tests pass; fresh3frame capture exits0/clean_close,zero full-session rejection,tape present. Ledger25280 blocks,309290 VALUE,52399 READ,39993 STORE,250963 INPUT,35630 MEMORY,3404 COPY,228 OP. Existing arithmetic/event checker passes observed-mode4225 FTRVs atc944,1718 atc96a,2200 ata9ea,18 ata9b0,plus17674 fadd/6425 fdiv/14555 fmul. All within unchanged event/byte caps. This verifies observed arithmetic operations, not yet full initial/accumulation memory ancestry or selected scene parity. G patch/header retained. Next byte/record lineage must propagate through initial writes, accumulation loads/adds and CALC readback without assuming that a load's register address proves its contents' origin; validate full scene/tape/native comparisons on G before acceptance.

#307 2026-09-08 known ancestry inclusion bounds | explicit INCLUDE_ANCESTRY adds only c932,c93a,c94c,c95c for known SUPPLY/INITIAL/accumulation alongside INCLUDE_CALC. Descriptor cap24 only under new flag; retain32768 combined steps,524288 total events,512/block,128 ops/32 inputs, existing ledger byte caps and producer1781. No changes to legacy ownership or native fallback. Capture may fail if actual event volume exceeds bound; do not accept partial data. Build and same-frame capture pending.

#306 2026-09-08 CALC predecessor read bytes | existing combined-F ledger independently verifies6828 CALC loaded words against preceding observed memory bytes;5855 reads lie wholly outside tracked domains and stay explicit. Writer families are known INITIAL(c952/c950/c94e), accumulation(c982/c97e/c97c), and6 X-prefetch reads from prior CALC c9ce. Full read/value consumption and partial ownership checks fail closed; synthetic wrong value and partial-byte writer controls added. This narrows required ancestry capture to known paths, not a new discovery scan. Next instrument known SUPPLY/INITIAL/accumulation executions under bounded same-frame contract and connect only selected supported histories; do not promote untracked5855 or treat last-writer bytes as matrix source truth.

#305 2026-09-08 all alternate consumers have completed records | combined-F byte ownership maps2931 copies to1150 completed records. Composite copy/tape join and full3frame scene validator pass. Selected breakdown accounts for all1990 alternate consumers:1622 divided-path plus368 CALC(draw1114=141,70=123,2246=104), none missing at completed-record level. Other1692 selected consumers remain other copy paths. CALC arithmetic+record ownership is not original-transform ancestry: its earlier RAM inputs require same-capture producer linkage. Ownership CLI now accepts explicit include-calc bound without changing default. Next track CALC input reads back through known INITIAL/SUPPLY/accumulation machinery in same frame, preserving unsupported histories and run-specific image-parity caveat.

#304 2026-09-08 combined CALC arithmetic/assembly | explicit include_calc parser bound32768 preserves default8192. Combined-F verifies15112 block event sequences,21555 read addresses,22164 stores,6425 fdiv/14555 fmul/12520 fadd and2218 FTRVs under observed FP mode. Existing same-block contiguous Z/Y/X assembly extended to known CALC sites yields4225 CALC records plus2035 divided/18 direct. All3 pvr-scene JSON files byte-identical to E; native-color parity FAILS first2frames:9/8 pixels differ by at most1 channel value,MAE8.13802e-6/6.51042e-6; third frame exact. Preserve this falsification, no exact-image claim. New-family attribution excludes CALC to retain earlier metric meaning. Initial-transform ancestry of CALC inputs remains unproven in this combined capture. Next bind4225 CALC records to memory/copy/tape and verify predecessor input-read lineage; do not call their loaded inputs original coordinates.

#303 2026-09-08 combined CALC-F transport complete | automation build and247 tests pass; fresh3frame capture exits0/clean_close with zero full-session rejection and tape present. Bounded ledger has15112 ENTRY/EXIT,166507 VALUE,22164 STORE,21555 READ,113085 INPUT,35630 MEMORY,3404 COPY and161 OP records, within explicit combined bounds. These counts are transport inventory, not arithmetic acceptance; existing default8192-step parser intentionally needs an explicit combined contract rather than silent cap relaxation. F patch/header retained. Next reuse CALC arithmetic for observed added ops, verify event completeness and native/tape parity, then connect new CALC outputs to the368 consumers. No initial-transform/world-camera claim.

#302 2026-09-08 bounded existing-CALC inclusion | explicit INCLUDE_CALC adds only known blocks c998,c9a4,c9c0,c9d8 to preserved dispatch observer. Combined descriptor cap20(14 observed alternate plus up to4 added, spare2), step cap32768 only under flag versus legacy8192. Preserve128 ops/32 inputs/512 events perblock,524288 total events and8MiB/128MiB ledger limits. This observes existing load/projection/store lifecycle alongside alternate producers in same producer1781; does not invent initial transform lineage or weaken old CT rejection. Build/capture pending. Fail closed if the bounded total is insufficient; assess evidence before changing any other cap.

#301 2026-09-08 remaining writers localized to known CALC | accepted-E selected pipeline now inventories actual per-byte latest writers at source-read time for each unmatched alternate consumer, requires exact target coverage and rejects mixed/missing writers. All368 resolve XYZ to c9ce/c9cc/c9ca:draw1114=141,2246=104,70=123. These PCs are existing verified CALC sequence (large_transform_arithmetic_inspect), not another unknown producer. Initial diagnostic target set1078 included710 other-copy-path vertices and correctly found only368 alternate copy matches; integrated target set now explicitly unmatched alternate identities. Next reuse existing CALC arithmetic/initial-transform lifecycle for these records in unified observation, preserving marker invalidation and accepted frame identity. Do not launch another blind producer search or claim old-frame arithmetic automatically proves this frame.

#300 2026-09-08 integrated projection and triangle scope | selected acceptance now invokes exact Z-projection checker and counts only triangles whose all3 vertices have accepted matrix/record/read/copy/tape binding. Preserved-E firstframe supports886 triangles across9 draws (2378:56,2410:120,2462:164,2660:80,2715:93,2771:135,2923:56,2990:48,3014:134); these9 draws have no partial triangles. Do not extrapolate producer1781 provenance to other captured frames or call this world-camera proof. Remaining368 alternate consumers localize to draw70:123,1114:141,2246:104. Other1692 selected consumers remain on other copy paths. Next inspect actual latest writers for these3 draws using retained memory/copy/tape evidence, not another broad trace. Initial internal metric named fully_proven_triangles was immediately narrowed to first_frame_provenance_supported_triangles to avoid unscoped claims.

#299 2026-09-08 exact explicit Z projection | retain observed pre-divide numerator and output-block depth scale/screen offsets alongside raw FTRV point/matrix words. Independent exact-rational dot then per-operation binary32 rounding reproduces all2035 divided XYZ records exactly. All2035 deliberately W-divided controls reject or mismatch, and all2035 shifted-X-offset controls mismatch. Raw zero-fourth-row matrices remain unchanged; synthetic zero-W fixture explicitly tests Z division. Scope is reconstructed recorded positions, not world space/physical camera; direct family18 records remains separately interpreted. Next integrate this projection assertion in selected capture acceptance and quantify supported selected triangles plus remaining368 consumers.

#298 2026-09-08 raw matrix/source extraction | continuity API optionally retains exact point/matrix words and FTRV identity for2053 completed records. Divided path has2 raw matrices differing only fourth row(all0 versus homogeneous1); direct path has1. Existing full affine-layout check correctly rejects zero-fourth-row matrix; retain failure. Exact comparison proves first3rows identical between divided variants. Explicit mathematical3x4 affine analysis with added homogeneous analysis row yields calibration614.7144309686947/565.5372185498106 and orthogonality residual1.3666307295553536e-9, identical to direct-family result and compatible with prior calibration. Raw matrices are not changed; zero W must not be discarded or relabeled as game-provided affine output. Recorded downstream divides use Z. This supports a projection hypothesis, not yet scene reprojection or world camera.246 tests pass; next implement/test explicit Z-divided3x4 contract with captured offsets/depth scaling and selected output reconstruction.

#297 2026-09-08 preserved-counter E accepted integrated slice | capture3frames exit0/clean_close=yes; full-session guard and accepted tape/scene checks pass. All3 native-pvr-color PNGs and all3 pvr-scene JSON files are byte-identical to counter-off D. This verifies the scratch/flags-preserving counter intervention for this bounded run, not a universal JIT correctness claim. Integrated selected checker binds1622 selected consumers across9 draws to645 distinct matrix/store executions in producer1781. All2053 completed records have continuity, no unlinked records;2529 total owned copies,1990 selected copy/tape composite joins. Full scene binding remains3682 vertices/2152 triangles in each of3frames.246 tests pass. This slice joins recorded FTRV operands/results to selected output, but source coordinate meaning, original camera decomposition and remaining producer paths are not proven; public false flags remain conservative. Retain rejected C and successful counter-off D. Next extract source points/matrices for these645 executions, test calibrated projection/rigidity against prior reconstruction, and close remaining368 alternate selected consumers without inventing camera truth.

#296 2026-09-08 counter-off isolation D | same current binary with only PRODUCER_CONTINUITY absent completes3frames exit0/clean_close,zero current-session rejections,valid tape. Scene sizes return to B's15967/15967/15970 vertices and3347/3347/3348 draws, versus C15919/15600/15600. All B/C/D producer-cycle timestamps agree, ruling out simple frame-index offset. Counter intervention implicated but single A/B does not prove mechanism. Inspection: RAX excluded from allocated guest registers; counter INC changes host flags. New counter saves/restores RAX and RFLAGS explicitly around increment; build/rerun pending, no claim fixed. RTC uses wall time outside GGPO, so retain as possible confound until repeated controlled evidence. Failed rg Windows glob/nonexistent directory and root-manifest lookup corrected with exact source/frame paths. Do not accept C selected provenance.

#295 2026-09-08 integrated selected acceptance falsifies C | matrix/record identity export finds the one unlinked record(step4766,base2363945104) has no observed owned copies; all2539 owned copies have linked matrix-record IDs. HOWEVER new integrated scene/tape acceptance fails: ownership-C.bin does not exist. Current-session log contains missing associations beginning producer1782 vertex14445/draw2990/offset505984, later opaque-draw-ambiguity, and LEDGER_END semantic_failed=1. LOG293's no-observer-rejection wording was too broad: only ledger-contained arithmetic checks had passed, while rejection lived in execution.log. C is REJECTED for selected-geometry provenance; no copy-to-scene promotion. Retain C arithmetic as diagnostic only. New integrated CLI checks full current-session rejection before tape/ledger to prevent recurrence. Next diagnose replay/topology divergence before a fresh continuity capture; never combine B's accepted tape with C's dispatch evidence. Independent APIs now optionally return bounded record IDs for joining, without changing default reports.

#294 2026-09-08 matrix/store dispatch continuity | independent continuity checker first verifies arithmetic/record assembly, then carries physical-register values and matrix-origin sets only across consecutive global dispatch serials. Gaps clear provenance; readm replaces provenance rather than inheriting its address. Ownership-C verifies71274 cross-block register comparisons over172 segments and links2055 of2056 completed XYZ records to one matching-family FTRV execution;1 remains unlinked. This is recorded execution continuity, not source coordinate/world-camera meaning or a unified selected-consumer acceptance. Focused isolated tests exercise matching edge, gap exclusion and wrong-register rejection; full-suite result pending below. Next identify the unlinked record's selected-consumer relevance and join linked record IDs through ownership/tape, retaining exclusions.

#293 2026-09-08 ownership-C dispatch observation | fresh3frame capture exits0/clean_close=yes with JIT fallback guard and dispatch counter;245 tests pass. Independent ownership/arithmetic passes but C is not identical to B:2038 divided records versus2035,24824 memory writes versus25160,2539 owned copies/998 executions versus2529/995. Do not merge B/C as exact-input evidence. C has2200 divided/18 direct FTRVs,3404 copies/3377 tracked bindings and zero missing last writers. Dispatch inventory shows all observed f2->fe (1025) and f4->fe (1013) edges consecutive, whereas162 f2/f4->aa16 skip paths have gaps and one e0->f4 also has a gap. Thus serial evidence distinguishes uninterrupted output paths from hidden-block paths; do not bridge gaps. Next use serial plus register continuity for each completed record, rejecting or explicitly excluding gaps; preserve run-specific counts. No camera acceptance.

#292 2026-09-08 dispatch continuity bounds | add explicit PRODUCER_CONTINUITY test flag: every compiled x64 block increments one64-bit serial, selected producer entries report it. Consecutive serials can establish no intervening dispatched block; gaps must reject an inferred edge rather than silently bridge filtered traces. No full-game block log or larger ledger budget. Audit also finds JIT interpreter_fallback calls opcode handler directly, bypassing Sh4Interpreter::ExecuteOpcode; add ownership-window rejection there. Thus ownership-B still cannot prove absence of this fallback without fresh capture. Existing141s observation overhead not a performance result. Build/capture pending; retain prior evidence, no legacy ownership relaxation.

#291 2026-09-08 completed record ownership | new timeline join assigns record identity only after all12 XYZ bytes have the expected same-step writer PCs and exact values. Every later observed write clears ownership of affected bytes, including same-value metadata writes. At read time snapshot component owners; count a copy only when all3 components share one completed record execution and its exact XYZ. Ownership-B binds2529 copies to995 distinct completed records; this is not automatically selected-domain coverage and not matrix-to-record continuity. Wrong component writer and same-value marker controls exercise rejection/invalidation. Next link the FTRV block execution to these store blocks with actual continuity evidence; do not infer from adjacency of selected blocks alone.

#290 2026-09-08 accepted tape composite join | ownership-B complete source-tape/scene verifier passes3682 vertices/2152 triangles in each of3frames. First-frame producer-copy join matches1990 selected tape records on ordinal/generation/TA offset, exact source address/XYZ and chronology;1692 selected records use other copy paths,1414 logged copies are not selected. Tape has no copy-ID field, so explicitly report composite-key join, not direct copy-ID proof. Wrong source address/word/time/generation and duplicate association controls added. Matrix invocation-to-record continuity remains separate and pending; no complete camera claim. Next unify completed record identity with the last-writer timeline and composite-key selected consumers, and extend provenance fields only where required rather than falsely inferring them.

#289 2026-09-08 guarded ownership-B copy linkage | fresh capture completes3frames exit0/clean_close=yes with corrected interpreter/HLE guards and no observer rejection. Independent arithmetic, record assembly and10470 last-writer read comparisons remain valid. New copy verifier checks strictly increasing copy IDs/TA offsets and fresh exact XYZ source reads:3377 copies bind to tracked reads;27 copies explicitly outside tracked source-read domain out of3404 total. Reused consumed reads, duplicate copy IDs and wrong words reject. This is pre-copy linkage, not successful copy-to-tape/TA acceptance or complete matrix invocation continuity. Raw B patch/header retained. Next join IDs/generation/offset/words to existing executed-copy/source-tape records, preserving outside-domain counts.

#288 2026-09-08 ownership guard audit correction | source audit finds interpreter and HLE-command guards only covered legacy armed transform leases, not mapping-only ownership window. Ownership-A remains observed byte equality, not complete provenance. Extend interpreter fail-closed guard and existing bounded HLE allowlist to explicit ownership window; retain existing logged HLE status writes and DMA/bulk/SQ overlap rejection. Cache test flag for scope check. Add bounded existing-copy-path records carrying copy ID, ordinal, generation, TA offset, source base and exact XYZ words, so next capture can bind timeline to source tape rather than match values alone. Existing32768-copy/frame limit remains; parser explicitly routes MEMORY/COPY to separate ownership analysis. Build/tests pending; no acceptance promotion.

#287 2026-09-08 ownership-A actual capture | resumed exact live session23003, which completed3frames exit0/clean_close=yes in about141s, no observer rejection; no timeout/restart occurred. Ledger contains25160 writes and10470 alternate-source reads in35630 consecutive memory events. New independent byte-level last-writer checker verifies every read against observed preceding writes, with zero missing writers;7818 read components have one of the new family XYZ stores as their latest writer. Integrated arithmetic/record assembly also passes unchanged. This establishes observed last-writer bytes, not yet the same invocation's matrix-to-consumer lineage or completeness against every emulator write mechanism. Marker overwrite even with identical bytes invalidates new-family attribution. Ordered stream/test patches retained ignored. Next bind each completed XYZ record and its producer execution to the corresponding copy/TA record; audit non-x64 write coverage before promoting ownership. Diagnostic141s run is not performance evidence.

#286 2026-09-08 consumer ownership observation bounds | temporary explicit RECORD_OWNERSHIP records ordered actual x64 writes and alternate-source reads overlapping existing656 target XYZ domains during producer1781 only. Shared131072 event cap, existing compressed/expanded ledger limits, exact RAM byte comparison and unchanged legacy transform rejection. DMA/bulk/SQ-to-RAM overlap rejects instead of inventing ownership. Static JIT gate bypass is test-flag-only; ordinary neural/gameplay paths unchanged. This is pending build/capture instrumentation, not acceptance. Next match completed record stores to this comprehensive observed write/read order; unsupported writers remain failures.

#285 2026-09-08 explicit producer record assembly | producer_record_inspect first runs complete observed-mode arithmetic/memory checks, then assembles only same-block Z/Y/X ordered stores at contiguous base+8/+4/base addresses. Values-B verifies2035 divided and18 direct XYZ records over360 distinct addresses; reused addresses retain distinct step identities, never last-value-only collapse. Marker-only writes produce no geometry; marker interruption, wrong address, wrong step, reversed or missing components reject. All238 inspector tests pass. This is completed-store record assembly, not consumer-time ownership or proof against writes outside selected blocks. Next bind records to selected read/copy sequence with explicit invalidation; do not accept a value-only address match as provenance. No new production changes or camera/Remix acceptance.

#284 2026-09-08 producer memory chain and layout evidence | extend independent checker to require descriptor-derived complete event order/coverage, integer add/sub/mov32 results, all8872 actual read addresses and9489 store addresses/sizes/SSA source values. Values-B passes along with prior FP checks. This checks observed SSA operands, not independent memory-load content or cross-block provenance. Store inventory exposes1103 a9f2 integer marker writes at XYZ-base addresses subsequently overwritten by2035 aa14 X writes; corresponding aa12 Y/aa10 Z occupy+4/+8, while2200 a9f4 metadata writes occupy+12. Only2035 of2200 divided transforms reach observed XYZ stores: preserve165 non-storing executions rather than invent outputs. Direct path has18 XYZ triplets at a9c2/a9c0/a9be and18 metadata stores at+12, with9 earlier a9b8 markers there. This refines the lifetime problem: an initial marker may alias a later X, so it must invalidate pending ownership until actual XYZ completion, never count as position truth. Tests add wrong address, internally consistent wrong RAM value versus SSA source, and missing event with adjusted count. Next explicit record assembly and consumer-time ownership, including non-storing executions.

#283 2026-09-08 producer values-B live mode and scalar arithmetic | automation build passes; fresh three-frame native capture exits0/clean_close=yes. Independent verifier requires observed FPSCR40001 and stable MXCSR rounding/DAZ/FTZ across all6654 selected blocks; all2218 FTRV results match exact-rational binary32 reference. Extended reference also matches2200 fdiv,6105 fmul and4070 fadd results, retaining observed SSA inputs rather than claiming preceding load or cross-block lineage. All9489 post-store comparisons remain exact. Missing/changed FP mode controls pass. New scalar-operation wrong-result controls added; full suite rerun below. Selected-consumer/source ownership and full record-layout verification remain next, not camera/world truth. No proprietary or production rendering change.

#282 2026-09-08 independent producer dot reference | new producer_matrix_inspect replays recorded SSA results into each observed FTRV operand vector and compares all four outputs against exact-rational dot products rounded to binary32. On values-A all2200 a9ea plus18 a9b0 executions match assumed toward-zero rounding. This does not verify preceding arithmetic, load contents, cross-block lineage or live FP mode; output explicitly reports these limitations.231 inspector tests pass including wrong-result, duplicate-component and incomplete-envelope controls. Initial Windows rg glob invocation failed and was replaced with directory/-g search. Add FPSCR/MXCSR entry and MXCSR exit fields to temporary observer for next bounded capture, with unchanged prior budgets; this last observer edit is not yet built/run. Next verify live mode and full divided/screen-offset arithmetic plus actual store/source correspondence before accepting new lifetimes.

#281 2026-09-08 dynamic producer A capture | automation build links;228 inspector tests pass. Three-frame native capture exits0/clean_close=yes. Bounded ledger decodes without REJECT records:6654 ENTRY/EXIT pairs,77718 VALUE,8872 READ,9489 STORE,75060 INPUT and122 OP records. Executed envelope check verifies consecutive steps, nonnested matching descriptors, exact event counts and all9489 post-store values equal actual RAM. Recorded result events identify2200 FTRV executions at a9ea and18 at a9b0. These are selected-region execution/read/write observations, not independent FTRV arithmetic or selected-consumer transform acceptance. Retained values-A patch/header and raw evidence; next independent arithmetic/layout verifier with falsifying controls must connect these outputs to selected source records before changing lifetime ownership.

#280 2026-09-08 dynamic producer bounds | complete temporary post-store observation with actual RAM comparison, alongside read addresses and operation results. Explicit PRODUCER_VALUES observes producer1781 only, at most8192 selected block executions,512 events/block and524288 total events within existing ledger limits. Region remains a900..aa20,16 descriptors/128 operations/32 entry registers. Entry/exit step IDs establish selected-block execution order, not unobserved caller continuity. Non-RAM/unsupported stores, mismatched stored values and budget overflow fail closed. No transform ownership gate relaxed. Build and runtime verification pending.

#279 2026-09-08 producer-family A discovered | automation build and228 tests pass; mapping-only three-frame capture exits0/clean close, emits14 executed descriptors/122 operations and entry snapshots in bounded ledger, no observer rejection. First17-draw source map remains independently verified3682 vertices/2152 triangles/frame. This run deliberately observes layout without claiming transform execution verification; existing transform-mode unknown-writer rejection remains unchanged. Found two FTRV sites a9b0/a9ea, both loading source XYZ plus W=1. a9ea path divides at a9f8, scales and adds320/240 before final XYZ writes aa10/aa12/aa14. a9b0 path writes transformed coordinates directly at a9be/a9c0/a9c2. a9f2 itself writes integer r0 metadata, so do not assume every overlapping target-buffer write is a position store. Next bounded dynamic operand/read/write collection for both paths must distinguish header/record layout and actual selected source positions, then verify arithmetic and lifetime invalidation/continuation. First-once entry snapshots are not a continuous invocation trace. A patch/header retained ignored; no new original-transform coverage accepted.

#278 2026-09-08 alternate transform family discovery bounds | new temporary explicit producer-family observation covers SHIL blocks intersecting a900..aa20 around actual unknown writer a9f2, maximum16 descriptors/128 operations/32 entry registers each. Emit each descriptor and input snapshot only on first execution during producer1781..1783 before failure; route through existing8MiB/128MiB ledger. Existing writer rejection remains intact. Scope identifies dependency layout, not execution arithmetic or accepted transform lineage. Build/capture pending; no private binary inspection.

#277 2026-09-08 dense C exposes alternate transform producer |228 tests, automation build and legacy default-four integrated regression pass. Dense C captures3frames exit0/clean close, ledger2097123 bytes semantic_failed=1; first failure is now unobserved-record-writer, not capacity. Ledger shows2274 lifetime starts before rejection and all current seam counts2274. Exact offending write: PC8c03a9f2,address8ce6e460,size4,slot0,kind4,active_slot656, after completed known CALC/prefetch at cycle7603461376. Thus another executed producer modifies the target buffer outside existing transform seams. Do not relabel this as accumulation or accept it merely because final bytes can match. Next capture the actual producer block/family containing a9f2 and its operand dependencies, then bind its arithmetic/source semantics before allowing overwrite. No remaining transform reconstruction acceptance. C patch/header and failed ledger retained ignored; narrowed ledger inspection used UNKNOWN_WRITE (not absent UNOBSERVED marker) to identify cause.

#276 2026-09-08 dense lifetime contract bounds | explicit dense mode assigns sequential observed IDs per generation, capped4096; verifier independently checks IDs and remaps arithmetic slots/selected consumers to those IDs. Legacy4/6 interpretation stays default. Native current-state array remains656 records; ledger retains every observed lifetime, including unused ones, with shared per-frame starts and derived3-frame seam/prefetch/accumulation ceilings. Existing8MiB compressed/128MiB expanded and arithmetic tolerances unchanged. Tests include4096 accepted/4097 rejected, skipped version and duplicate dense ID, plus unchanged legacy rejection. Build/tests/capture pending.

#275 2026-09-08 six-version B falsification and required pivot |226 tests, automation build and old default-four integrated regression pass. B captures3frames exit0/clean close but fails at slot0/version6 with15 seams,4095 mask and0 reads; ledger1869747 bytes semantic_failed=1, no new transform acceptance. Independent ledger scan finds1941 lifetime starts before first failure, well below4096/frame, while slot0 still has later selected consumers through offset433792. The selected-value histogram omits unused intermediate transforms, so it cannot size a per-address version quota. Do not iterate6->8->10 caps or discard unused executions. Next explicit dense lifetime IDs with shared4096/frame budget, preserving default legacy contract and all chronology/unused-record checks; virtual slots must represent observed lifetimes rather than version*all-addresses Cartesian space. Retain B failure and current six-version experiment as capacity evidence only.

#274 2026-09-08 explicit six-version contract | verifier accepts explicit4(default)/6 only, with six-version target product capped4096/frame. Synthetic six lifetimes pass only explicit6, default4 and seventh lifetime reject; invalid version contract/global budget controls added. Integrated arithmetic virtual slots use explicit version count. Temporary native FC067_SIX_VERSIONS selects6; original default remains4, aggregate bounds derive from656*3*versions and six accumulations/lifetime. Review found stale12060u seam-count literal from prior1005 domain; corrected to derived bound rather than retaining accidental excess. Build/tests pending, no new capture acceptance.

#273 2026-09-08 remaining transform A falsifies inherited lifetime cap | capture3frames exits0/clean close and225 tests pass, but first failure is incarnation-incomplete-or-cap at slot0/version4, with all15 seams,4095 mask and9 reads complete. Ledger1633929 bytes semantic_failed=1; no transform acceptance. Independent accepted source-tape histogram across3frames has879 one-value,426 two-value,345 three-value,264 four-value and54 five-value address instances: at least five selected lifetimes are necessary for18 addresses/frame. Thus four is an inadequate capacity assumption, not evidence of wrong geometry. Next add an explicit six-version experimental contract while preserving old default4;656*6=3936 records/frame stays inside original4096 ceiling. Keep8MiB/128MiB ledger and arithmetic/ownership tolerances; do not blindly raise global limits or accept missing lifetimes. Full rejected log dump exceeded tool output budget; narrowed first-error and histogram query retained the actionable evidence. A patch/header/ledger retained ignored.

#272 2026-09-08 remaining transform integration A bounds | derived ignored656/217-address headers from accepted tapes, exact final selected copy offsets473728/462400. Merged retained four-draw versioned lifetime/prefetch instrumentation with current alternate read/store tracking for first656-address batch. Same four incarnations/address/frame (7872 aggregate), six accumulations/lifetime (47232 aggregate),32768 copies/frame,98304 acquisition IDs,8MiB compressed/128MiB expanded ledger; explicit lossless level6. Actual alternate producer entry starts a normal copy lease; six reads feed the same lifetime association before SQ copy. Prefix closes at473728, all3682 selected vertices remain required. Automation build passes. First broad JSON source extraction exceeded output budget and failed parsing before edits; narrower exact-segment extraction succeeded. New capture not accepted yet; original-transform arithmetic, lifetime and scene verification must all pass. No generated targets or temporary core hooks to be committed.

#271 2026-09-08 sparse incarnation regression | integrated prior four-draw C reconstruction passes after normalized-domain adaptation, retaining2093 supported triangles/frame and explicit-affine/strict-camera scope. Report retained ignored as fc067-sparse-incarnation-regression.json. No new remaining-batch transform capture has run yet; those domains are source-mapped only. Next merge proven versioned lifetime instrumentation with alternate read/store copy support, using actual656/217-address maps and unchanged lifetime/ledger bounds.

#270 2026-09-08 background falsification complete and transform preflight | automation build and225 tests pass. Live wrong-depth expectation capture exits0/clean close but independent verifier rejects background arithmetic. All three native-pvr-color.png and pvr-scene.json pairs match positive B byte-for-byte. Pure capture mutation tests reject depth/scale/generation/duplicate/missing/final-scene changes. Background geometry slice remains limited to decoded inputs, not material/absolute VRAM provenance. Remaining transform target preflight passes656 and217 non-overlapping stable address domains with generations1790/1791/1792. Adapt integrated incarnation reconstruction to normalized sparse vertex lists, preserving gaps and old range behavior; rerun prior four-draw reconstruction before new capture. A guessed test filename was absent; no test result inferred. Next bounded versioned-transform collection uses actual accepted tapes, not address-only identity.

#269 2026-09-08 background falsification bounds | separate pure capture verifier from file loading; test entire input/queue/output/scene chain against depth, scale, generation, duplicate, missing and final scene-byte mutations. Add live diagnostic-only wrong depth expectation, preserving actual registers and final rendering. Same3-frame/byte bounds, no new runtime gate scope. Build/tests/capture pending.

#268 2026-09-08 background B geometry construction verified | automation build and224 inspector tests pass; three-frame native capture exits0/clean close. Background verifier binds all3 prequeue records to actual queue context/generation, reconstructs float32 depth bias/floor, textured/untextured extent and fourth-corner rules, and matches all12 final XYZ vertices exactly to captured scene vertices0..3. Same capture retains verified920-vertex/463-triangle packet map. This proves decoded-input-to-background-geometry construction only; absolute VRAM read provenance, UV/color/material lineage and world camera remain unproven. Wrong-depth/scale arithmetic unit controls differ, but live background corruption and complete capture-verifier mutation tests remain next before checkpoint. Preserve A missing-record failure and B ignored patch/header. Packet original-transform work remains active after focused background falsification.

#267 2026-09-08 background A retained missing-record failure | automation build and224 tests pass; capture3frames exits0/clean close and second packet map remains verified, but background verifier rejects complete background records. Source inspection shows FillBGP precedes QueueRender producer stamping, so direct captureProducer.ordinal was zero. B uses only an explicit prequeue ordinal hint and requires a later actual queue binding with matching context/generation; unconfirmed hints cannot pass. Background A patch/header retained ignored. No background acceptance yet.

#266 2026-09-08 background geometry capture bounds | temporary FC067_BGP records three decoded input XYZ vertices, strip/stride/tag, texture/hscale and depth register bits immediately before FillBGP overwrites/extends geometry; records four final XYZ corners afterward. Restricted producer1781..1783 and existing byte budget; independent verifier must require exactly3 input/12 output records and bind scene vertices0..3. Scope is background geometry, not texture/color/UV provenance or a world transform. Build/capture pending.

#265 2026-09-08 extended map negative and second packet batch B | first-batch live wrong-pointer capture exits0/clean close but rejects compact-consumer-association, emits no tape/completion. Packet/background partition test passes;221 inspector tests and automation build pass. Second packet-only batch B captures3frames exit0/clean close; independent verifier binds920 vertices/463 triangles/frame,217 source addresses and135 reused-with-different-values addresses/frame. Explicit output retains four omitted background vertices and background_provenance_proven=false. Remaining two packet batches now map2615 triangles/frame; this is not original-transform reconstruction. Next capture actual FillBGP register/VRAM-decoded inputs and final four vertices, verify its background construction independently, then collect versioned transforms for mapped packet domains. Do not invent a world transform for the background. Retained B patch/header ignored; no checkpoint/full-build claim.

#264 2026-09-08 second packet/background partition bounds | extended first-batch wrong-pointer capture started. Add explicit packet-only second-batch selection of920 vertices/16draws, requiring excluded draw0 to be exactly vertices0..3 in both native selector and independent topology verifier. CLI reports four omitted background vertices and background_provenance_proven=false. Full924-vertex mode remains unchanged and cannot silently pass without background. Separate FillBGP evidence remains required, not waived. Build/test/capture pending; no background or transform acceptance.

#263 2026-09-08 remaining first batch source map B accepted in scope | automation build and220 inspector tests pass; first17-draw capture exits0/clean close and emits complete tape. remaining_draw_inspect.py independently binds all3682 vertices/2152 triangles in each of1782..1784 to actual copied packet bytes and captured topology;656 source addresses/frame,363 with multiple selected values/frame. No observer rejection. Original transforms and world camera remain false. Next live wrong-pointer regression for this extended map, then remaining packet-derived second batch plus separately labeled background provenance. Source review confirms FillBGP owns draw0/vertices0..3, reads VRAM and background registers, replaces depth and constructs fourth corner; these cannot be assigned fictitious SQ provenance. Preserve their separate dependency rather than weakening packet acceptance. B patch/header retained ignored; full checkpoint not yet run.

#262 2026-09-08 alternate compact association bounds | explicit temporary FC067_ALT_MAPPING uses actual same-call SQ pointer/address, per-byte writer/read source stamps, current ordinal/generation and exact XYZ to create the existing bounded CameraSourceCopy record before the real TA copy. Shared after-copy equality and decoder pointer/scene verification remain mandatory. Preserve32768 copies/frame,98304 acquisition IDs,4096 selected vertices/frame and all format limits. Diagnostic-only descriptor/alternate log rows suppressed in mapping mode, not rejection checks. No original-transform identity inferred. First17-draw batch capture/build pending; four missing background samples remain unsupported.

#261 2026-09-08 live diagnostic corruption controls | negative-enabled automation build passes;220 inspector tests pass. Three separate address/value/generation captures each produce3frames and exit0/clean close. Each retains all10212 packet words exactly against writer B, but independent verifier rejects source address structure: X-address corruption breaks contiguity; mismatched read value/generation prevents stamp consumption and leaves zero source addresses. No game-memory mutation, no partial tape accepted. Negative patch/header retained ignored. Absolute address derivation beyond trusted executed callbacks and original-transform identity remain unproven; next extend actual source-copy association for alternate producer families using same-frame stamps, preserving exact pointer/bytes/generation controls and unsupported background domain. This is not a new committed checkpoint.

#260 2026-09-08 independent diagnostic verifier | added bounded alternate_source_inspect.py comparing complete packet identity/generation/context/words/executed SQ fields against an independent prior capture, validating writer masks and source triplet structure. Four new tests include address/value/generation/PC corruption, missing/duplicate rows and overflow.220 inspector tests pass; real read A passes10212 packets/385 bases. Explicit uniform-triplet-shift test demonstrates that these fields cannot independently prove absolute source addresses; result retains independent_absolute_source_address_proof=false and original_transform_lineage=false. Next live diagnostic-only negative modes mutate X address stamp, read-value stamp or generation stamp without changing game state; all must reject before any source-map acceptance. New negative wiring not yet built/run.

#259 2026-09-08 runtime read A diagnostic complete | automation build and216 inspector tests pass; three-frame capture exits0/clean close without overflow/unrelated rejection.10212 alternate SQ copies all have validity7 writer stamps and nonzero contiguous XYZ source triplets,385 distinct bases across capture. All packet keys/32-byte words match writer B; all8730 prior missing-offset associations remain covered. Negative wrong-address/value/generation controls and independent reusable verifier remain required before extending accepted compact source maps. This is actual read/store/copy diagnostic evidence, not original-transform lineage or world-camera recovery. A patch/header retained ignored; no new commit or complete gate claim.

#258 2026-09-08 runtime read binding bounds | add six fixed pre-read address/post-read value stamps at proven instructions; consume each once at its matching XYZ store only when value, size, ordinal and generation agree. Carry source address in64 existing SQ byte stamps and log three addresses alongside writer diagnostics, unchanged count/byte limits. No RAM address guessed from instruction layout or current memory. Build/capture and falsifying controls pending; zero address remains unsupported, not an accepted association.

#257 2026-09-08 alternate writer descriptors C | automation build and three-frame capture pass, exit0/clean close; bounded current log contains9 executed descriptors/204 operations with no overflow or unrelated rejection. All15 writer occurrences across five producer-block variants have exactly one preceding matching-version read definition: ab56/ab58/ab5a feed ab5c/ab5e/ab60; ccc2/ccc4/ccc6 feed ccc8/ccca/cccc. Both use the same versioned base register plus0/4/8 offsets; no coordinate arithmetic intervenes between reads and stores. This proves descriptor dependency, not actual runtime read addresses or original-transform provenance. Next add bounded pre-read address/post-read value stamps for those six sites, bind to actual per-byte SQ writer stamps and copied packets, and reject wrong address/value/generation controls. C patch/header retained ignored.

#256 2026-09-08 writer B complete diagnostic | three-frame capture exits0/clean close, no diagnostic overflow/rejection. All10212 alternate SQ copies have validity7 XYZ stamps and full packet equality against D. All8730 missing-offset matches covered: batch1 ab5c/ab5e/ab60=4866,ccc8/ccca/cccc=1104; batch2 corresponding2658/102. No source read/transform acceptance yet. Next descriptor selection includes the six actual writer PCs, same16-descriptor/128-op bounds, then use their operand dependencies for runtime read binding. Prior A failure retained and owned stage log rotated recoverably. B patch/header retained ignored; no complete camera or new checkpoint claim.

#255 2026-09-08 writer A retained budget failure | build and216 tests pass; capture3 frames exits0/clean close but diagnostic hits8MiB-log-budget and is not complete evidence. Before overflow observed XYZ writer triplets ab5c/ab5e/ab60 and ccc8/ccca/cccc with validity7; this narrows next acquisition, not acceptance. B restricts alternate packet/writer logging to SQ path2, because complete A/B/D copy inventories already proved every missing association uses that path; unrelated bulk path0 rows are excluded, not dropped selected geometry. Preserve same byte/count limits and exact-copy requirements. No source transform or complete writer acceptance yet.

#254 2026-09-08 cross-block SQ writer bounds | temporary tracker retains64 byte stamps only: actual writer PC, exact post-store byte, producer ordinal and context generation. Existing executed non-MMU SQ-store callbacks update stamps; every SQ copy clears its bank to prevent stale reuse. Alternate-copy diagnostics report three component writers and validity only when all four bytes agree with copied words and current identity. No fabricated read address or transform; unsupported/unobserved writers remain invalid. Keep32768 records/frame and8MiB logs. Build/capture pending. This is writer discovery, not full provenance acceptance.

#253 2026-09-08 alternate descriptors C/D | automation build and216 inspector tests pass. C three-frame capture exits0/clean close but load_session rejects accumulated stage execution.log over16MiB; retain C as rejected evidence, no limit change. Move only owned stage log to ignored retained C log, rerun unchanged binary as D. D exits0/clean close and bounded reader accepts; all15396 alternate packet words match B. Four executed descriptors contain16 operations total: ab78/cce6 are pref plus pointer advance; ab88/ccec read a flag byte, write packet header, test, pref, branch and advance. XYZ is not produced within those flush blocks. Next use existing actual SQ-store callbacks to retain bounded per-byte last-writer PC/value/ordinal stamps across blocks, then associate those stamps with missing copies. This avoids serial predecessor-block guessing; source reads remain pending. Original stage log is recoverable at ignored fc067-alternate-copy-c-stage-log-retained.log. No camera/source-transform acceptance or new commit.

#252 2026-09-08 alternate producer descriptor bounds | capture descriptors for blocks containing the four proven SQ pref sites, emitted only upon actual execution in the three-frame diagnostic window. Maximum16 descriptors and128 SHIL operations each, existing8MiB logging bound. Record all operand/register-version descriptions without claiming runtime values or source provenance. This identifies the complete local producer dependency shape before selecting read/write callbacks; both families acquired together. Temporary diagnostic only, acceptance/build pending.

#251 2026-09-08 executed SQ B verified | automation build and216 inspector tests pass; three-frame native capture exits0/clean close. All15396 alternate packet keys and full32-byte words match A, all copies exact, no extra rejection. All8730 prior missing-offset matches have a nonzero actual JIT pref PC and same-call SQ address equality. Batch1 counts:ab78=3792,ab8e=1074,cce6=867,ccf2=237; batch2:ab78=2028,ab8e=630,cce6=81,ccf2=21. Prior ab88/ccec context snapshots were not actual flush PCs, validating the need for executed-site binding. Next collect actual XYZ reads/stores for all four sites in these two producer families, retaining source/lifetime uncertainty until causal binding succeeds. B patch/header retained ignored. No compact/source-transform acceptance, new renderer checkpoint, or world-camera claim.

#250 2026-09-08 executed SQ witness bounds | temporary enabled-only non-MMU JIT pref wrapper passes exact block address plus guest instruction offset and actual address through the unchanged SQ handler, clears the witness immediately afterward, and adds those fields to the bounded alternate-copy log. MMU/bulk paths remain unwitnessed rather than guessed. Preserve all LOG248 limits; compare full packet bytes with A and require same-call SQ address equality. First source inspection command used a nonnumeric Select-Object count and failed before reading; corrected immediately. Build/capture acceptance pending; no source transform provenance added.

#249 2026-09-08 alternate-copy A executed | automation build and216 inspector tests pass; three-frame native capture exits0/clean close.15396 alternate vertex-shaped copies observed, all32-byte post-copy comparisons exact, no other diagnostic rejection or overflow. Intersecting producer ordinal/TA offset with prior C inventories locates all5970 batch1 and2760 batch2 missing associations. Every located copy uses path2 (actual SQ entry), not path0 bulk entry. Context PC snapshots cluster at ab78/ab88 and cce6/ccec; PR snapshots at bd54/b69c and d0de/d9ee. These are diagnostic snapshots, not independently proven executed instructions or RAM origins, and cross-run offset lookup alone is not exact-input identity. Next instrument actual JIT execution of SQ flush sites for these two families, then their XYZ source reads/writes. Four absent packet samples remain separate. No CameraSourceCopy entries fabricated and no compact tape accepted. A patch/header retained ignored; no new production checkpoint or complete camera claim.

#248 2026-09-08 alternate-copy diagnostic bounds | next native three-frame capture logs every vertex-shaped32-byte TA copy not associated with the current indexed-gather observer, including inactive leases. Record actual offset/context/generation/path/address, context PC/PR explicitly as snapshots (not authoritative executed caller), all eight input words and full post-copy equality. Maximum32768 records/frame,98304 total and existing8MiB log limit; overflow rejects. Offline intersection with the exact missing-offset inventories determines selected coverage. This diagnostic does not create CameraSourceCopy records or change compact acceptance. Then identify a real executed producer from observed paths; no guessed RAM provenance.

#247 2026-09-08 corrected remaining inventories C | prior1c process handle no longer exists; authoritative capture-complete records3 frames and current launch log has5970 missing associations, exactly1990/frame across12 draws, with no other rejection. Fresh2c capture exits0/clean close with2760 missing associations, exactly920/frame across16 draws, plus four missing packet samples/frame at slots0..3. No partial tape or COMPACT_COMPLETE is emitted. Both distributions are identical across producer ordinals1781..1783. Batch1 missing draws:70,1114,2246 (type3),2378,2410,2462,2660,2715,2771,2923,2990,3014 (type4). Batch2 missing draw2371(type3), all other non-background selected draws(type4). Source inspection confirms these are textured packed-color packets, with type4 using16-bit UV; this is packet format, not proof of a particular producer. Current JIT gather selector observes only writes cc82/cc84/cc86, so next bounded acquisition must inventory actual copy paths/callers for missing offsets, including copies without an active gather. Four absent samples are kept separate from missing copy associations. Inventory C patch/header retained ignored; no source mapping or new camera coverage accepted. Previous explanatory goal turn made no implementation progress; this turn completes both diagnostic inventories and narrows the next acquisition.

#246 2026-09-08 inventories B retained diagnostic failure | both3-frame inventory runs exit0/clean close. Batch1 reports8658 missing associations, but those totals are not accepted: after first missing packet, finish still appended later valid records with non-contiguous sample IDs, causing compact-consumer-association and poisoning later-frame source collection. Corrected inventory C never appends any further tape records once any source is missing, while continuing expected draw/packet checks and permanently disabling tape output. Build passes;1c capture started. B patch/header retained ignored; classify B as diagnostic failure, not proof that previously verified draws lack sources.

#245 2026-09-08 remaining map1a/2a rejected | both3-frame captures exit0/clean close, but no accepted tape: first failures compact-missing-source-copy at observed acquisition counts12079/12068 respectively. Batch1 verifier fails missing tape as expected; no mapping acceptance. Next diagnostic-only inventory keeps collecting selected missing packet identities (ordinal/vertex/expected draw/TA offset/type/PCW/copy count), bounded by existing selected counts and8MiB logs, while any missing source permanently disables tape output. Do not fabricate sources or silently omit the affected geometry. Inventory all missing selected packets in each three-frame run rather than one-at-a-time traces; derive actual alternate producer coverage from the resulting distribution. Temporary original A patch/header retained; inventory edit not yet built/run.

#244 2026-09-08 remaining sparse capture wiring | generated ignored exact sorted vertex/draw arrays from three-frame verified C topology for both remaining batches. Temporary observer binary-selects the exact lists, using3682 maximum selected slots and per-run counts11046/2772; gaps never enter the target set. Original-transform tracing remains disabled, copy/byte limits unchanged. remaining_draw_inspect.py verifies against a separately specified topology capture. Automation build and216 inspector tests pass; retained remaining-map patch/header. Batch1 live capture started; no source mapping acceptance yet.

#243 2026-09-08 explicit sparse selection implemented | compact selection now accepts exact sorted vertex tuples alongside legacy ranges, up to34 unique draws with unchanged4096 total vertices. Duplicate draw IDs, unordered/duplicate vertices, cross-draw overlap, fabricated gap vertices and changed captured topology reject. Two independent sparse fixtures plus all216 inspector tests pass. Actual topology-derived sparse domains verify identically across all three C scenes:17 draws/3682 vertices and17 draws/924 vertices. This is selector/preflight evidence only; no new source-copy capture yet. Next generate ignored exact vertex/draw tables from these verified lists, restore bounded mapping hooks and capture both remaining batches with wrong-pointer controls before deriving transform targets. Preserve all skipped index gaps; do not convert sparse lists back into ranges.

#242 2026-09-08 exact755b90f8e61c46e47cf0c856c1afd49420b0884d checkpoint | original handle73145 completed all four serial configure/builds. Three enabled selftests298/298,214 inspector tests, SDK63 mock, compact13/176-byte golden and five16000-event concurrent ledger trials pass. Exact committed integrated four-draw C affine verifier reruns successfully. Fresh fc067-755b90f8e-native captures3 frames exit0/clean close; manifests all755b90f8e, producer clocks7605222912/7608559168/7611895424, current session hook-free. Fork push/ls-remote verifies full SHA; worktree clean before updates. Actual remaining opaque topology is34 draws/4606 vertices/2617 triangles. All three C frames confirm stable, disjoint exact vertex lists. Two batches cover all remaining draws within4096 vertices each: batch1(3682 vertices/2152 triangles) ordinals1114,70,2180,2462,2771,1071,3014,2410,2715,1,2660,44,2246,2923,2378,26,2990; batch2(924/465)2946,2838,2966,2890,2570,2874,2697,2858,2758,2522,2650,2371,2915,2648,2647,0,2914. Draw2462 demonstrates non-contiguous indices:271 vertices across312-index span. Next support explicit topology-derived vertex lists for at most34 draws and4096 selected vertices per batch, with overlap/gap/ownership negatives. Derive actual source maps before versioned arithmetic; no capture/reconstruction claim for remaining batches yet.

#241 2026-09-08 restored affine checkpoint checks | original build handle14742 remained live in feature-off build, then completed successfully; all four serial restored builds pass. Three enabled selftests298/298, SDK63 mock, compact13/176-byte golden and five16000-event concurrent ledger trials pass, including explicit compression-level roundtrips/invalid-level controls.214 inspector tests passed in LOG240. Temporary core hooks remain removed. Commit only owned offline verifier/transport/tests and governing docs; retain all failed strict-calibration/capture attempts and explicit-affine limitations. Exact-commit matrix and fresh native capture follow after commit, not claimed by these precommit results.

#240 2026-09-08 focused wrong-prefetch D and hook removal | D build/three-frame capture exit0/clean close. Only expected prefetch X is deliberately perturbed; unmodified live ledger independently rejects prefetch edge. Initial test-driver assertion expected the generic observer-rejection marker first and failed because the stronger edge predicate rejected earlier; corrected driver confirms the precise prefetch failure without filtering the ledger. Negative patch/header retained ignored. Inverse apply_patch removes temporary core hooks and owned header; git diff --quiet -- core passes. Four restored builds started serially; no completed build/commit claim yet. Next finish checkpoint tests and record explicit affine assumptions separately from the still-failed all-rigid gate.

#239 2026-09-08 full matrix diagnosis and explicit affine experiment | audited all10128 contributions:15 primary and78 accumulation matrices fail strict orthogonality. Added explicit affine-contribution mode, preserving strict default, all selected vertices, every arithmetic contribution and existing rigid residual/scale tolerances. Two independent fixtures show shared rigid calibration plus shear reconstructs without treating shear as a camera witness; wrong doubled calibration, missing rigid witness and malformed/degenerate inputs reject.214 inspector tests pass. Integrated four-draw C with explicit flag verifies6936 lifetimes/3657 selected lifetimes/10593 consumers,117 unused lifetimes and3 independently checked prefetches.10035 rigid witnesses retain calibration [614.7144639490892,565.5372201633035], maximum scale difference0.0003233951130141577 and orthogonality residual3.433763228781826e-7. All93 general-affine contributions retained with maximum residual0.011071517326912068 and algebraic reconstruction error1.1368683772161603e-13. All2093 triangles/frame reconstruct, maximum0.00006454178787862475 pixels; wrong-scale control rejects. Across separately captured disjoint accepted draw scopes,5722 of8339 opaque triangles have bounded camera-relative reconstruction,2617 remain outside those scopes. This is not a unified scene or physical/world-camera proof, and nonrigid object semantics remain unknown. Next focused live wrong-prefetch control, then remove hooks and checkpoint compression/prefetch/affine slice with full builds/tests. No third-party writes or default/rendering-path changes.

#238 2026-09-08 level6 capture C and calibration falsification | C build/capture3 frames exit0/clean close. C++ tests independently roundtrip identical event bytes at levels1/3/6/9 and reject-1/0/10; default346-byte golden and five16000-event concurrent trials pass.212 inspector tests pass. C ledger6626555 bytes/tape1694896 bytes fit unchanged budgets. Independent lifecycle and arithmetic verify6936 records/27744 seams/3192 accumulations/51192 writes/70695 reads, but integrated calibration rejects not scaled orthogonal axes. No full reconstruction acceptance. Diagnostic factorization finds15 nonorthogonal primary matrices: five selected address lifetimes/frame, all draw1222, virtual slots1893..1897 at bases8ce73b60/8ce73b90/8ce73bb0/8ce73c40/8ce73ca0. These are selected geometry, not discardable unused records. Next inspect these executed transforms and their projection relation against existing calibrated witnesses; distinguish nonrigid object transforms from camera calibration without loosening residuals, omitting selected vertices or declaring camera truth. Include accumulation matrices in the diagnosis. C retained patch/header and failed integrated result remain evidence; no new commit/full checkpoint claim.

#237 2026-09-08 four-draw B retained stream-budget failure | capture3 frames exit0/clean close,212 inspector tests pass including independent prefetch wrong-edge/pointer/bytes controls. B current session first rejects stream-budget; no compact tape emitted, so integrated verifier fails missing input rather than accepting partial evidence. Temporary B patch/header retained ignored. Next test explicit lossless zlib level6 while keeping default3, identical event content and8MiB/128MiB limits. Constructor now accepts bounded1..9 level with default3; temporary capture requests6, not yet built/tested. Missing guessed transform_ledger_contract.cpp lookup corrected via file discovery; no conclusion drawn from it. Validate byte-identical decompression and invalid levels before live C, retain B failure.

#236 2026-09-08 prefetch/final-unused diagnosis and B bounds | independent event scan finds14 final lifetimes flagged incomplete in A; each has all four seams and zero reads, not missing projection. B uses the same complete-byte/seam/no-partial-read retirement rule at frame end; these remain unused, never selected-consumer proof. Already projected X loads get explicit PREFETCH_LOAD/PREFETCH_EDGE records requiring exact bytes/pointer/generation and observed c9d8 edge; ordinary preprojection continuity still requires c9a4. Offline verifier independently checks prefetch state and edge rather than treating it as projection. No bound/tolerance expansion. B capture build passes; capture acceptance pending.

#235 2026-09-08 four-draw transform A retained rejection | build and3-frame capture exit0/clean close,211 inspector tests pass, but original-transform capture rejected. Bounded ledger decodes31862283 expanded bytes. First failure is X continuity at slot166/base8ce6ef00: c9d6 loads43de6110 with exact expected bytes/writer6, then observed edge is c9d8 (pointer8ce6ef04 and same FR0), not assumed c9a4. Earlier actual record history proves all four seams completed and XYZ consumed at copies7077/7110/7135 before this later load. Thus the observer incorrectly conflates a later prefetch of already projected data with a new projection input. Do not accept c9d8 as the normal predecessor or weaken X checks. Next separate completed-record prefetch observations from preprojection X continuity, verify their actual edge independently, and explicitly account for final unused incarnations without promoting them to selected consumers. Capture also reports final armed-record-incomplete; diagnose those lifetimes from evidence rather than suppressing the gate. A patch/header and rejected ledger retained ignored. Four-draw arithmetic/reconstruction remains pending.

#234 2026-09-08 four-draw transform A bounds | reuse D's versioned capture with the independently derived1005-address four-draw map and exact375744 final copied packet. Fixed1005 live slots, maximum four incarnations/address/frame,12060 aggregate instances and72360 accumulations (six/version). Keep3531 selected vertices/frame/10593 records,32768 copy slots/frame,98304 attempts,8MiB compressed/128MiB expanded evidence bounds and unchanged arithmetic tolerances. Generated header remains ignored; no assumptions that four incarnations or the current compressed budget suffice. Original-transform acceptance requires independent lifetime/arithmetic/scene binding and negative controls.

#233 2026-09-08 four-draw B negative complete | three-frame capture exit0/clean close; current-session parser confirms compact-consumer-association rejection, no completion marker and no output tape. Backlog contract passes. Four-draw mapping is accepted in its source-copy/decoder/scene scope only. Next original-transform incarnation collection; temporary map hooks remain in worktree, retained ignored, no commit/full-build/reconstruction claim for this new batch.

#232 2026-09-08 four-draw map A complete | capture3 frames exit0/clean close; independent scene binding verifies all10593 records,3531 vertices/2093 triangles per frame.1005 disjoint target addresses and the vertex-to-address map are stable across context generations1790/1791/1792; exact last selected copy offset375744 in each frame.201 addresses per frame feed differing selected XYZ values, unlike the prior two-draw sample. Thus version identity is required even among selected consumers, not only for intervening unused geometry. No original-transform or reconstruction claim yet. Wrong-pointer B capture started; derive future incarnation capture from this actual map, not previous1122-target header/341472 endpoint.

#231 2026-09-08 four-draw mapping preparation | explicit domain table selects draws161/854/1222/2266, skipping all intervening vertices;3531 vertices/frame,10593 consumer records across three frames. Five focused selector tests and211 total inspector tests pass, including gap-vertex negatives at all three discontinuities. Automation capture build passes. Temporary mapping hooks restored from retained map A with original-transform tracing disabled; existing32768-copy/frame,98304-attempt,4096-selected-vertex and byte limits retained. Four-draw A patch/header retained ignored. Real capture started; acceptance requires every selected copy/decoder/scene association, not selector tests alone.

#230 2026-09-08 exact36538ae123c8e9dbdbe60be6741ccc0ab24d60cd checkpoint | original exact-build handle12557 remained live, then completed all four serial configure/builds successfully. Three enabled selftests298/298,210 inspector tests, SDK63 mock, compact13/176-byte golden and five16000-event concurrent ledger roundtrips pass. Fresh fc067-36538ae12-native captures3 frames exit0/clean close; current session has no temporary CS/CT/EXTRA/COMPACT markers, manifests all36538ae12 with producer clocks7605222912/7608559168/7611895424. Fork push/ls-remote verify full SHA and clean worktree before these updates. Next topology checked across all three existing D scene captures: draw161 vertices823..1419 (597/365 triangles),854 vertices4165..5195 (1031/597),1222 vertices5929..7271 (1343/781),2266 vertices11172..11731 (560/350). Disjoint combined3531 vertices/2093 triangles inside4096 selected-vertex limit. Capture their actual source-copy maps as one explicit non-contiguous batch, then derive versioned transform targets; do not reuse prior address maps or assume the previous endpoint covers them. Existing proven3629 triangles and prospective2093 would total5722 distinct opaque triangles, but prospective coverage is NOT yet proven.

#229 2026-09-08 restored checkpoint checks complete | all four serial restored builds pass. Three enabled selftests298/298 each, public-SDK mock63/63, compact13 checks/176-byte cross-language golden and five16000-event concurrent ledger trials pass.210 inspector tests and the tightened positive integrated D reconstruction rerun pass. Live E wrong-version rejects independently; all temporary core hooks removed. Checkpoint only owned offline validators/tests and governing documents, not generated addresses, runtime/configuration, legal media or private captures. Exact-commit builds/native capture remain next after commit; do not substitute these precommit checks for them.

#228 2026-09-08 live wrong-version E and restored checkpoint | E build/capture exit0/clean close; emitted incarnation versions deliberately incremented without altering the emulated record lifecycle. Independent integrated verifier rejects lifetime sequence/budget as intended. E patch/header retained ignored, then inverse apply_patch removes all temporary core hooks and deletes the owned probe header; git diff --quiet -- core passes. Four restored builds started serially. Added independent duplicate-selected-copy and write-after-consumption negatives;210 inspector tests pass. Positive integrated D rerun and restored matrix still running when this entry was written; no completion claim for those checks yet. No third-party writes, production enablement or performance claim.

#227 2026-09-08 independent incarnation/arithmetic/reconstruction | new lifecycle verifier reconstructs6732 lifetimes from actual incarnation/seam/write/gather order, binds10176 consumers to3366 selected lifetimes and identifies24 unused lifetimes. Two synthetic tests include old-copy-after-rewrite positive, wrong copy/version, retirement count and partial-read negatives;209 inspector tests pass. Version-separated arithmetic verifies26928 seams/3225 accumulations/50067 writes/68589 reads across all6732 records. Integrated incarnated_scene_inspect.py verifies9957 matrix contributions against unchanged calibration [614.7144639490892,565.5372201633035], maximum scale difference0.00028067435368939186 and orthogonality residual2.479637237810923e-7; wrong doubled-X calibration rejects. Draw1503/1887 reconstruct1078/960 triangles each frame,2038 combined, maximum error0.00007008206114278437 pixels. These draws plus earlier draw277 cover3629 distinct opaque triangles across separate captures, not a unified captured scene;4710 of8339 remain outside those scopes. Two-draw lifetime/arithmetic/calibrated evidence is independently verified; fresh live wrong-version control and full checkpoint checks next. Wrong filename/glob rg attempts returned file-not-found during oracle lookup; corrected to extra_writer_inspect.py, no test or runtime conclusion drawn from those errors. New core hooks remain temporary/uncommitted and retained ignored.

#226 2026-09-08 incarnation D acquisition complete, arithmetic pending | build and three-frame capture pass exit0/clean close. Current session reports10176 compact records/1628176 bytes and8181113-byte compressed ledger semantic_failed=0, inside8MiB cap. Independent bounded decompression yields1053151 events/6732 incarnations with no semantic rejection. Existing two-draw scene verifier independently binds all3392 vertices/2038 triangles per frame to1122 addresses. This proves acquisition and scene binding only; old fixed-slot arithmetic verifiers cannot accept versioned lifetimes unchanged. Next independently verify incarnation lifecycle, associate each selected copy with the contemporaneous version, then run established arithmetic/calibration oracles with wrong-version controls. Retain B/C failures and D patch/header; temporary core still present, no commit or exact-SHA/full-build claim.

#225 2026-09-08 incarnation C retained cap; exact selected endpoint | build/capture exit0/clean close;2932682-byte ledger decodes, semantic_failed=1.2449 incarnations recorded in first frame, including18 zero-read completed retirements. First rejection explicitly slot0/version4/seams15/mask4095/reads9 after gather12068: fifth incarnation, not incomplete state. Independent accepted map inspection proves selected TA offsets232928..341472 with exact maximum341472 in each of three frames. D closes transform/copy acquisition immediately after the exact copied packet at341472; later geometry is omitted, not accepted. This is a verified selected endpoint, not a guessed prefix or shifted starting window. All10176 decoder associations still mandatory and absent endpoint still fails prefix ownership. Four-version cap and all other bounds unchanged.

#224 2026-09-08 incarnation B retained rejection | build and three-frame capture exit0/clean close;1457835-byte ledger decodes but semantic_failed=1.1265 supply/initial instances and1122 projection pairs recorded before incarnation-incomplete-or-cap;143 second-version records progressed beyond A. Previous zero-read records were prohibited despite complete projection, so C permits retirement with zero reads only when all seams/XYZ bytes are complete and no partial XYZ read exists. Such versions are explicitly unused in emitted previous_reads=0, never selected-consumer evidence. Any incomplete seams/bytes or partial XYZ group remains a rejection. Bound remains four versions/address; no acceptance claim until independent lifecycle and arithmetic checks.

#223 2026-09-08 incarnation capture B bounds | permit at most four sequential incarnations per each of1122 observed addresses/frame (13464 aggregate instances across three frames), with six accumulations per incarnation and80784 aggregate. Fixed1122-slot live memory remains bounded. A new supply can retire its predecessor only after all four seams, complete XYZ bytes and complete nonzero XYZ reads; incomplete reuse or fifth incarnation rejects. Emit generation/slot/version/previous-state/cycle before each supply, preserving all prior arithmetic events in the bounded ledger. No duplicate seam inside an incarnation is accepted. Original3-frame/3392-vertex/copy/byte limits and arithmetic tolerances unchanged. This is an acquisition bound, not evidence that four incarnations exist or suffice; independent lifecycle/arithmetic verification remains required.

#222 2026-09-08 two-draw transform A falsifies single-record-per-address capture | automation build and three-frame capture exit0/clean close, but ledger semantic_failed=1 and no accepted tape. Bounded decoder reads1372772 compressed bytes successfully. First rejection is transform-duplicate-or-cap in generation1790 after gather5563; seam counts are1122 each, below the old2763 limit, so this is duplicate address lifecycle, not capacity exhaustion.1114 records had all seams and reads before rejection. This falsifies reusing a single immutable transform identity per address over the expanded interval. The accepted source map's zero differing selected values does not imply no intervening rewrites. Preserve A patch/header/ledger and source-map A/B evidence. Next implement bounded per-address transform incarnations, preserving write/read chronology and linking each selected copy to its actual incarnation; do not suppress the duplicate check, shift capture start heuristically, or claim later geometry from earlier address reuse. Also correct the retained literal2763u to the declared3366 per-seam budget when implementing the new lifecycle.207 inspector tests pass before this diagnostic run; original-transform proof for these two draws remains pending.

#221 2026-09-08 two-draw original-transform acquisition bounds | independently derived1122 disjoint XYZ bases and3392 consumers from the accepted map across generations1790/1791/1792; stable address-to-vertex mapping,3366 target instances. Three target tests pass including draw-boundary and changed-map negatives. Temporary capture expands only target slots921->1122, per-seam instances2763->3366 and six-accumulation aggregate16578->20196; retains4096 selected vertices,32768 copy slots/frame,98304 attempts and existing byte/clock/arithmetic tolerances. Observation interval closes at producer queue rather than obsolete5069-copy prefix. No sampled data or generated target header is committed. Capture and arithmetic acceptance remain pending.

#220 2026-09-08 d558fb32c plus temporary two-draw mapping hooks | automation build passes. Actual fc067-two-draw-map-a captures three frames, exit0/clean close; all10176 records/1628176 bytes independently bind selected draws1503/1887 to3392 vertices and2038 triangles/frame,1122 source addresses/frame, zero differing values at reused addresses.36204 acquisition attempts and six incompatible omitted candidates total; every selected decoder still finds its exact copy. Original-transform tracing disabled. Wrong-pointer run fc067-two-draw-map-b-negative also captures three frames cleanly but rejects compact-consumer-association and emits no tape. Temporary source retained in ignored fc067-two-draw-map-a.patch/header. This is source-copy/decoder mapping only, not original-transform identity, reconstruction, native preservation or performance proof. Next derive the1122-address map and extend original arithmetic collection to this actual domain, retaining existing tolerances and explicit bounds.

#219 2026-09-08 d558fb32c plus owned offline changes | generalized compact scene binding to an explicit bounded draw batch; added independent two-draw synthetic coverage for all10176 records/1628176 bytes, both sides of the draw boundary, final consumers, overlapping/oversize selections and duplicate scene ownership. Four focused tests and206 total inspector tests pass. Existing full921 A tape/capture reruns successfully through the default domain, preserving1591 triangles/2745 vertices per frame. Backlog contract and git diff --check pass. This proves validator behavior only; actual draws1503/1887 source-copy capture remains next and no new original-transform, GPU or presentation evidence is claimed. No core hooks restored, production changes, third-party writes or new build claim in this slice. Changes remain uncommitted pending the source-map experiment.

#218 2026-09-08 d558fb32ced514f5f08422aaefda6b9e57e5df0b | all four exact configure/builds pass; three enabled selftests298/298, SDK63 mock, compact13/176-byte golden, ledger346-byte golden plus five16000-event concurrent roundtrips and204 inspector tests pass. Exact committed integrated reconstruction checker reruns full921 A successfully. Fresh fc067-d558fb32c-native captures3 frames exit0/clean close; current session hook-free, all manifests d558fb32c and producer cycles7605222912/7608559168/7611895424. Fork push and ls-remote verify exact SHA; worktree clean before this documentation update. Next two-draw source map uses3392 selected vertices/frame/10176 records total, existing160-byte consumer records and8MiB budgets. Bound all-copy attempts/owned buffers by existing32768/frame and98304 aggregate limits. Incompatible source-copy candidates outside selected draws may be explicitly omitted; every selected decoder must still find an exact valid copy or reject. No arbitrary skip or missing selected source is accepted. Do not invoke original-transform tracing during this mapping pass.

#217 2026-09-08 checkpoint checks | verified the original restored-build process remained live, then observed all four serial builds complete. An early feature-off log-tail request preceded creation of that log and failed; it did not trigger a restart. Automation/baseline/no-NGX selftests298/298 each, SDK63-check mock, compact13-check176-byte golden, ledger single-thread346-byte golden plus five16000-event concurrent roundtrips, and204 inspector tests pass. Core hooks remain removed. D-122 records first-record distinctions and compression thread ownership. Commit follows with only owned offline validators/tests/docs; exact-commit configure/build/native checks remain next, not claimed here.

#216 2026-09-08 checkpoint preparation | positive/negative full921 frame1783 scene JSON is byte-identical;5871 raw-depth words differ (observed examples one ULP), while every PNG and the other two depth files match. No blanket native/input preservation claim. Full921 temporary patch/header retained before inverse apply_patch removal; core diff empty and no staged core changes. Restored build matrix starts next. Remaining owned changes are offline validators, serialized diagnostic transport/tests and governing docs, not production renderer integration.

#215 2026-09-08 full921 A integrated reconstruction | combined variant build and3-frame capture pass, exit0/clean close. Independent aggregate verifies2763 records/11052 primary seams/1299 accumulations/20475 writes/28809 component reads; all8235 consumers bind, zero unsupported target addresses.4062 matrices satisfy unchanged shared calibration and wrong-scale control rejects. All1591 draw277 triangles/frame reconstruct with maximum errors0.000035742255363402364/0.00003781893798304736/0.00003575143870193642 pixels;6748 of8339 whole-scene opaque triangles remain omitted. Four independent synthetic tests cover signed index/nonunit W, wrong indexed address, direct-X loading and wrong center/X controls;204 inspector tests pass. Live full921 B wrong-edge captures3 frames cleanly and aggregate rejects observer rejection.31 of32 PNG/raw-depth artifacts match; frame1783 depth.f32 differs, so no blanket exact-input preservation claim. Failure remains retained. Temporary full921 patch/header saved ignored. Scoped arithmetic/calibrated reconstruction ACCEPTED; full camera/world/scene/Remix/GPU/presentation remains pending. Next checkpoint the mutex/variant slice, then batch next high-coverage draws1503 and1887:2038 triangles/3392 vertices (7272..10663) total, inside4096 selected-vertex bound. Top10 draws total6570 triangles/11046 vertices; do not iterate tiny draw ordinals or silently exceed current consumer/copy bounds.

#214 2026-09-08 first15 B arithmetic | B builds/captures3 frames exit0/clean close with15 complete records/frame. Extended exact SUPPLY c932 validator proves its signed16-bit index load/address program and binds those bytes to the separately loaded fourth float; all45 indexed FTRV records pass. Exact PRED c998 validator proves constants, direct XYZ reads/reciprocal and successor edge for3 records;42 normal c9a4 predecessors retain the old validator. Aggregate passes45 records/180 seams/12 accumulations/306 writes/594 component reads and all138 selected consumer bindings. Direct X reads are distinguished from42 external c9d6 X loads. New index-result and first-projection-constant one-bit controls reject. Prior2718-record/1287-accumulation large B arithmetic still passes;200 inspector tests pass. Independent synthetic first-variant fixtures remain pending. Next one full921-address integrated capture is a focused regression of the combined program variants and serialized transport, not another repeat of the old isolated906. Bound2763 instances,1299 expected accumulation observations only if actually observed, existing32 descriptors/128 operations/32 inputs/64 writes/8MiB compressed/128MiB raw bounds remain. No whole-scene/world-camera/Remix GPU claim.

#213 2026-09-08 first15 A selector falsification | A captures3 frames exit0/clean close but rejects duplicate/cap after the first SUPPLY/INITIAL. The actual15-operation c932 program reads16 bits at source+12, shifts by4, masksfff0 and adds r4 before the ordinary FTRV path; r4 alone was an incorrect repeated-record selector. Exact captured live inputs/operations falsify it. B uses that observed bounded read-only address program for selection only, and retains executed read/SSA/successor store verification as acceptance authority. Source pointer/RAM bounds and r1/r2 constants remain explicit. Nonunit fourth float is still independently loaded at source+12; the index interpretation must not replace its floating value. No A arithmetic acceptance; failed artifact retained.

#212 2026-09-08 first15 arithmetic capture bounds | Select only the15 observed missing targets,45 instances and at most45 events per primary seam across3 frames. Add exact executed blocks c932(SUPPLY) and c998(PRED); retain known INITIAL/CALC and accumulation observers with their existing byte/writer/clock gates. The c932 entry selector tentatively uses r4 (to be proven by actual descriptor/address program and successor stores), c998 r4 is already observed. Wrong selector cannot pass the successor-pointer/write binding. Preserve nonunit W and up to6 contributions; c998's direct X read must replace, not fake, an external c9d6 X-load witness. No final reconstruction acceptance until separate exact-program validators pass both variants.

#211 2026-09-08 discovery F | F builds and captures3 frames exit0/clean close, no ledger rejection; independent consumer/last-writer checks still pass138 selected consumers/45 targets.339 retained history entries identify initial writes for all45 instances after the same executed sequence8c03ced4,c920,c932,c94c. Six instances include two accumulation contributions before projection;39 have none. This is the missing SUPPLY block variant8c03c932, not arbitrary address reuse. Three actual c998 entry snapshots use r4=8ce6e460. Its14-operation descriptor initializes center320/240, infinity/zero constants, loads X/Y/Z and computes reciprocal before c9c0; c9a4 normal predecessor remains unchanged. Next capture the two exact variants c932/c998 with original live operand/value callbacks for only15 target addresses, then extend validators with separate exact programs. No original arithmetic acceptance for missing records yet; no new game world-camera/GPU claim.

#210 2026-09-08 next batch bounds | Extend the fixed discovery table to the last8 observed writes per byte (15 targets x12 bytes x8 entries), enough to distinguish final projection, up to6 previously bounded accumulations and the initial write when present. Earlier history remains explicitly truncated, not inferred. Emit the history only when a component's current writer changes, at its actual read. Retain64 component reads/address/frame,65536 observed writes/frame,3 frames and8MiB ledger cap. Also record the actual8c03c998 compiled descriptor (128 operations/32 inputs, shared32-descriptor cap) and at most128 executed entry snapshots/frame; no guessed target pointer or arithmetic acceptance from a descriptor alone. This batch seeks the real initial producer plus alternate projection contract; known906 four-seam tracing remains disabled.

#209 2026-09-08 discovery E / guarded ledger | mutex-guarded8-thread x2000-event C++ test passes five exact Python roundtrips, preserving each writer's order; a reusable runner repeats all five plus the45000-byte single-thread golden. E gameplay captures3 frames exit0/clean close;157352-byte ledger decodes without rejection and complete8235 consumer tape passes scene binding. Independent observed-writer checker validates2376 byte snapshots across198 acquisition samples and joins all138 previously unsupported selected consumers/45 address instances. Selected X-writer histories:129 consumers use8c03c9a4,c9c0,c9a4,c9c0;9 use8c03cb84,cf10,c998,c9c0. Actual final stores remain8c03c9ce/cc/ca. This identifies an alternate first-projection predecessor8c03c998, not the earlier reused-memory8c0610d2 family; original initial FTRV/source chain and unobserved-writer exclusion are still pending. Wrong writer byte, missing/duplicate byte and explicit rejection mutations reject. Inspector suite200 passes. D failed checksum remains rejected, and A/B/C failures remain retained. E patch/header saved ignored; temporary core hooks plus isolated ledger mutex/contract tests remain owned/uncommitted. Next collect bounded first-projection c998 descriptor and preceding initial-writer history for the15 targets in one batch, then reuse the arithmetic chain rather than repeat final-store discovery.

#208 2026-09-08 discovery D / concurrent ledger regression | D builds/captures3 frames exit0/clean close; complete8235 consumer tape passes scene binding, but157350-byte compressed ledger rejects incorrect data check despite runtime semantic_failed=0. No discovery/source claim is accepted from that ledger. Code audit found SH4 writer and renderer decoder callbacks sharing unsynchronized zlib/vector state. A new8-thread x2000-event standalone control reproduces unguarded append failure (exit1, no output). Added append/finish mutex serialization and atomic temporary used/closed flags; destructor still requires joined callers. Green concurrent roundtrip and recapture remain required. This is a diagnosed regression in developer evidence transport, not a reason to relax checksum or gameplay provenance.

#207 2026-09-08 discovery pivot | C builds/captures3 frames cleanly but rejects event65 at8c046dd2 (2-byte write). It retains1466 bytes/64 writes for slot0 only: earlier unrelated RAM reuse dominates a first-write trace. No tape/accepted provenance. Stop the first-write tactic; do not enlarge its text log. D maintains a fixed15-address by12-byte last-writer table, recording observed writer ID/address/size/value/cycle plus4 executed block IDs. Snapshot only at actual geometry reads (at most64 component reads/address/frame,3 frames), with at most65536 observed writes/frame. Older overwritten records are replaced in fixed storage, not appended. Exact read bytes must match the recorded writer bytes. This locates a candidate producer family, not proof against every unobserved DMA/HLE writer or acceptance of original arithmetic. Known906 producer tracing remains disabled. C patch/header retained ignored before D edits.

#206 2026-09-08 60dedc1e1 plus discovery B | previous engineering turn made progress; intervening user question was a read-only InstaMAT2Remix assessment, not implementation. B captures3 frames exit0/clean close but rejects an actual2-byte write at8c046d94 to8ce6e460 after the three earlier floating stores.400-byte failed ledger and missing tape remain explicit; compact inspection attempted on the absent tape fails and provides no provenance. The original4/8-byte assumption was too narrow for pre-transform memory reuse. C discovery records all normal1/2/4/8-byte SH4 stores with exact width masking, preserving64-event/address/frame and RAM bounds. No new arithmetic or transformation claim; earlier writers may be unrelated prior uses of the same address. Record the final writer family before actual selected consumption, not merely the earliest write in a frame.

#205 2026-09-08 60dedc1e1 plus temporary first-writer discovery | bounded discovery A builds and captures3 frames exit0/clean close but rejects discovery-write-bound after3 actual stores. Slot0 first writes are8c0610e0/e2/e4 in executed block8c0610d2, preceded by8c06f840/8c0610a0/8c0610c2. Known906 tracing correctly emits zero primary seams. Failed369-byte ledger retained; no completed tape, so attempted compact verification rejects missing artifact, not scene proof. B discovery accepts only actual4/8-byte writes overlapping the selected12-byte record, preserving full address/size/source/actual bytes and64-event/address/frame cap. Crossing writes are discovery only, not silently accepted transform lineage. Unknown size/RAM extent/count still reject with explicit details. No acceptance or new capture result claimed for B yet.

#204 2026-09-08 60dedc1e120f8c355a3ba2e211c06e91765b9987 | exact four serial configure/builds pass; three enabled selftests298/298, SDK mock63/63, compact13-check cross-language176-byte golden, ledger346-byte cross-language golden/budget/terminal tests and199 inspector tests pass. Committed reconstruction checker reruns B successfully. Fresh fc067-60dedc1e1-native captures3 frames exit0/clean close; all manifests have60dedc1e1, producer cycles7605222912/7608559168/7611895424 and current session has no CS/CT/EXTRA/COMPACT markers. A log-tail request for the not-yet-started feature-off build returned file-not-found; original running handle was retained, and all four later completed. Fork push and ls-remote verify the exact full SHA; worktree clean before this documentation update. Next batch is first-writer discovery on only the15 unsupported addresses, at most64 writes/address/frame over3 frames inside the same5069-copy prefix. Record actual writer PC, bytes, clock and a bounded recent executed-block history; no original-transform acceptance from discovery alone. Known906 arithmetic is disabled in this probe, not repeated.

#203 2026-09-08 checkpoint preparation | retained large-ledger-B core patch/header in ignored evidence, then removed only owned temporary hooks with inverse apply_patch. Core diff empty; line-ending-only index metadata refreshed without staged core changes. Four restored serial builds pass; automation/baseline/no-NGX selftests298/298 each, public-SDK mock63/63, compact C++13 checks with exact176-byte Python golden, and ledger C++ budget/golden/terminal checks with exact346-byte compressed/45000-byte expanded Python comparison pass. Inspector suite199 passes, including new synthetic binding, indexed aggregate, explicit calibration-budget and partial-domain controls. Backlog contract passes. Source/docs commit follows; exact-commit rebuild and native capture remain required and are not claimed here. Next source work remains the unsupported15-address batch, not repetition of the verified906.

#202 2026-09-08 d7eb260ad plus offline large-domain arithmetic | prior turn classified progress; actual worktree retained. Added indexed reuse of the existing four-seam, accumulation, rational binary32, X-continuity and RAM-write validators, without a new game capture. Live B passes2718 records/10872 primary seams/1287 accumulations/20169 writes/28215 acquisition reads. Five actual-ledger one-bit controls reject at supply input, accumulation arithmetic, final projection arithmetic, actual RAM store and gathered value. Two independent aggregate protocol tests (mocked arithmetic, not game/GPU proof) pass with descriptor, writer, clock, identity and read negatives. All4005 primary/accumulation matrices satisfy unchanged shared calibration: maximum normalized-scale difference0.00020382287004849786 and orthogonality residual2.479637237810923e-7; doubled shared X scale rejects. All observed centers equal320/240. Reusing calibrated scene builder with explicit bounded source domain gives2699 supported vertices/1529 supported triangles per frame;62 of this draw's1591 triangles and6810 of all8339 opaque triangles remain omitted. Three maximum reprojection errors are0.000035742255363402364,0.00003781893798304736,0.00003575143870193642 pixels. This is bounded calibrated camera-relative reconstruction, not a unique physical/world camera, complete scene, real Remix GPU or combined DLSS5. Live artifacts are unchanged B; reports retained in ignored arithmetic-controls/reconstruction logs. New source/tests/docs remain uncommitted; restored builds/checkpoint and unsupported-producer expansion remain next. No external binaries/configurations/media acquired or changed.

#201 2026-09-08 d7eb260ad plus temporary large transform ledger | verified pending build A reached final executable links; prior process handle had expired, so completion was read from retained build log. Live A captures3 frames exit0/clean close but fails transform-duplicate-or-cap after the5069-copy prefix. Retains1112756 compressed/12620997 expanded bytes,906 complete four-seam targets and15 unsupported first-frame addresses. B records and closes the observer immediately after the5069th successful actual copy; later geometry remains excluded, all selected decoder copies still required. Build B succeeds; live B captures3 frames exit0/clean close. B retains3336952 compressed/37865040 expanded bytes,425653 lines and no ledger rejection. Each frame reports906 traced addresses and15 unsupported; cumulative2718 records, with actual accumulation events preserved. Independent compact scene checker passes all8235 consumers/1591 triangles per frame. New linear-indexed binding checker joins8097 selected consumers to traced reads and explicitly omits138 consumers belonging to45 unsupported address instances. This is binding evidence, not independently verified original arithmetic or a camera/GPU acceptance. Six actual-ledger mutations reject (observer rejection, missing/duplicate binding, wrong copy, wrong vertex, wrong prefix); a seventh initially changed an unselected acquisition read and was accepted because it lay outside the selected consumer assertion. Corrected selected-copy1294 one-bit gather mutation rejects. All193 existing inspector tests pass; new checker synthetic tests and broad arithmetic verification remain pending. A combined patch initially failed on a stale documentation context and made no changes; corrected patch applied. Temporary core integration remains owned/uncommitted; no third-party runtime/config/media changes. Next verify2718 records with indexed reuse of existing arithmetic validators, preserving accumulation and unsupported domains, then quantify fully supported triangles.

#200 2026-09-08 d7eb260ad plus compact transform envelope | previous turn classified progress; verified HEAD and only owned postcheckpoint docs dirty. Reused existing zlib dependency instead of inventing a new semantic event format. Added streaming C++ ledger with8MiB compressed/128MiB raw cap and independent bounded Python decoder; existing event text/rejection markers preserved exactly. Developer target configures/builds. C++ empty/raw-cap/incompressible-cap/golden/terminal checks pass; Python recovers45000 golden bytes from346 emitted bytes. Three decoder tests cover corrupt/truncated/trailing/concatenated streams, exact expansion cap and malformed line data. Added independently tested target-map derivation:921 addresses with gaps,2745 consumers,2763 target instances, same mapping across3frames; missing/changed target, generation and consumer controls reject.193 inspector tests pass. Generated target selector header retained ignored. This proves envelope/map preparation only; no new live transform capture, expanded original-transform coverage, emulator build matrix or commit claimed. Next route existing four-seam/accumulation/X-load/writer callbacks into the ledger for the actual921-address set, with unknown records unsupported. No further format-only tests as a substitute for runtime integration.

#199 2026-09-08 d7eb260adff8ea469057d6d1df160549ce686ab7 | compact consumer map committed with explicit source/docs staging; no temporary core hooks, media, third-party configuration or binaries staged. Four serial exact-SHA configure/builds pass; three selftests298/298, SDK mock63/63, compact C++13 checks with Python byte equality, inspector188 and binary32 five pass. Committed large-map verifier rerun against D:8235 associations,1591 triangles/frame,921 addresses/frame, original-transform false. Fresh hook-free native3frame capture exit0/clean close, manifests d7eb260ad and no CS/CT/X/EXTRA/COMPACT markers; cycles7605222912/7608559168/7611895424. Pushed fork and verified full remote SHA, worktree clean at checkpoint. Read-only accepted tape analysis confirms identical address sets and vertex-to-address maps across3frames, but address domain8ce6e460..8ce749c0 contains gaps; never substitute contiguous-range identity. Next original-transform collection uses actual921-address map with compact ledger and unknown-writer rejection. Subsequent postcheckpoint log/backlog notes are owned changes outside this binary SHA.

#198 2026-09-08 f49cdd51c plus compact-map checkpoint | four serial restored builds finish exit0; three enabled selftests298/298, SDK mock63/63, compact C++13 checks with independent176-byte Python equality pass. Core hook diff empty; source-map/transport tests remain scoped and do not close original transform/camera/GPU gates. Backlog contract passes after consolidating the current task. Next explicit staging/commit, exact-SHA builds/selftests and fresh hook-free capture; no postcommit result claimed yet.

#197 2026-09-08 f49cdd51c plus large compact map | previous turn classified progress. Verified selected vertex IDs1420..4164; widened actual-offset sorted copy storage with3 allocations/32768 records each,98304 gathers cap and8MiB summary-log cap. Builds A/B/C/D pass; all five A..E captures3frames exit0/clean close. A fails duplicate-or-unsupported at5070. B adds bounded diagnostic: completed active gather, same context/generation and TA offset166368, changed SQ address,5069 actual copies. It was never copied. C permits retirement only of such uncommitted observations with no existing copy, then fails later copy-position-owner-or-clock at5563 outside selected domain. All2745 selected vertices had already linked in first frame. D bounds acquisition to actual5069-copy prefix and independently requires all8235 selected decoder associations; completes1317616-byte tape. New scene-binding verifier validates frame/game/Git, exact producer, clocks, packet uniqueness, actual XYZ/vertex bytes and draw domain; synthetic late-consumer/frame/clock/byte controls pass.188 inspector tests pass. Actual D covers1591 triangles/frame and921 source XYZ addresses/frame, zero reused-address value disagreements; original transforms still false. E wrong pointer rejects compact-consumer-association and creates no tape. No exact-input image-preservation/performance claim. Retained large-D patch/header ignored; removed all core hooks, core diff empty. Four restored builds started and remain pending at this log entry. Consolidated stale next-card paragraphs into one active original-transform assignment for921 addresses, retaining failed scopes; no commit checkpoint claimed.

#196 2026-09-08 f49cdd51c plus temporary compact callback migration | previous turn classified progress. Restored owned source/copy/decoder probe, recorded actual X/Y/Z read addresses during executed gather, carried them with actual TA copy generation, before/after packet bytes and pointers, and appended compact records after unique opaque draw association. Exact426-record expected count; exclusive-create output only after full decoder completion and no source rejection. Full automation build A links. Live A3frames exit0/clean close,68176-byte tape independently byte-equal to all same-run verbose source/copy/decoder records. No original-transform reproof or expanded draw coverage claimed. Live wrong-decoder-pointer B3frames exit0/clean close rejects compact-consumer-association, no completion marker and no tape file. Archived owned append-only stage log intact before B to preserve16MiB loader bound; raw captures retained. Temporary patch/header ignored and core hooks still uncommitted. No paired image-preservation/performance claim, no four-build/commit checkpoint. Next compact capture of draw277 with actual pointer-indexed sparse copies and explicit capacity/program bounds; no more old-domain emitter regression needed.

#195 2026-09-08 f49cdd51c plus C++ consumer emitter | previous turn classified progress. Added bounded in-memory C++ encoder with explicit little-endian packing, CRC32, exact expected count,12288-record cap, failed/finished states and semantic copy/pointer checks. Partial or invalid captures cannot finalize a tape; no file I/O or emulator callback is implicit. New off-by-default developer CMake target configures/builds exit0. Executed13 checks including full1966096-byte maximum tape, partial completion, overflow, wrong identity/pointer/copy/reserved/address controls. Actual C++176-byte golden exactly matches independent Python bytes and decoder, including64-bit fields and CRC. Added reusable cross-language runner with30-second process timeout. No live compact capture, full emulator build matrix or additional transform coverage claimed. Next actual copy/decoder callback integration for draw277 with explicit source/transform ledger limits; no further format-only phase. Owned changes remain uncommitted.

#194 2026-09-08 f49cdd51c plus compact consumer transport | previous turn classified progress. Generalized offline domain checker with explicit bounded4096-vertex/8192-index limits while retaining256/4096 defaults; added large-domain/cap/short-position controls. Actual draw277 exact topology, vertex IDs and draw metadata agree over3frames:2745 vertices,3321 indices,576 restarts,1591 triangles. Added160-byte little-endian consumer record and16-byte header with fixed fields,64-bit host pointers, before/after packet bytes, CRC and12288-record maximum; decoder flags transport-only and all rendering provenance false. Four new tests cover independent layout,8235-record roundtrip, corruption/extent/count/identity/copy controls.186 inspector tests pass. Repacked426 already verified actual consumer records in memory, exact roundtrip68176 bytes. No live emitter, new emulator build/capture, transform coverage expansion or GPU proof claimed. Next C++ emitter/cross-language tests and actual copy/decoder integration with separate transform-ledger budget; no blind expansion of verbose logs. New format, checker and documentation remain owned/uncommitted.

#193 2026-09-08 f49cdd51cfac9ff9fda8a35cd1e92d67ca8cbf37 | bounded complete-draw harness/audit committed, no core probe or third-party artifact staged. Four serial exact-SHA configure/builds pass; three selftests298/298, SDK mock63/63, inspector181 and binary32 five pass. Fresh hook-free native Soulcalibur capture3frames exit0/clean close, all manifests short SHA f49cdd51c, no CS/CT/X/EXTRA markers. Producer cycles7605222912/7608559168/7611895424; no paired image-preservation claim. Pushed fork and verified full remote SHA; worktree clean at checkpoint. Read-only retained-scene survey finds3075 opaque draw entries and8339 triangles/frame; top10 cover6570. Draw277 has1591 triangles/2745 vertices, draw1503 has1078/1846, draw1887 has960/1546, draw1222 has781/1343; ranking/counts match all3 frames. These exceed old256-vertex probe capacity. Next design bounded compact capture for largest supported program family; no blind verbose-log widening or repeat draw1. Survey script ignored. First postcheckpoint documentation patch failed context atomically, corrected. Subsequent log/backlog notes are not included in the exact-SHA binary.

#192 2026-09-08 88b82d7f4 plus complete-draw harness | previous turn classified progress. Added independent aggregate protocol tests for426 consumers and48 shared records/frame, compact-descriptor omission, late-consumer loss, last-record edge/target and incomplete coverage. Arithmetic decoders retain separate real-oracle tests; protocol mocks are explicitly labeled. Full-mode calibration deduplicates by verified record generation/base, verifies144 matrices and exact288 center inputs; independent144/145 cap and last-matrix wrong-scale controls pass. Full-domain scene fixture covers142 vertices and140 synthetic strip triangles with winding and late-vertex/depth failures. All181 inspector tests pass. Actual B calibrated scene reconstructs92 triangles/frame, omits8247 of8339, max reprojection errors1.9780721487450137e-5 /2.1587993671801087e-5 /2.4519787672261373e-5 pixels. No whole-scene/world-camera/Remix claim. Retained final temporary patch/header ignored, removed all owned core hooks with inverse patch, verified core diff empty. Four serial restored builds pass; three enabled selftests298/298 and SDK mock63/63 pass. Expected selftest capture overwrite/hash negative diagnostics are not failures. D119 and full-draw audit added, handoff and current card updated. ACCEPTED only for bounded complete-draw arithmetic/calibrated geometry; preservation pairing remains failed and full M2 camera pending. Next coverage selection uses retained triangle contribution/shared programs rather than repeated one-point traces. Exact postcommit build/capture remains next checkpoint verification, not claimed here.

#191 2026-09-08 88b82d7f4 plus temporary full-draw transform probe | previous turn classified progress. Restored retained four-seam/accumulation/X-load and unknown-writer instrumentation, preserving widened source map. Actual48-base/142-consumer map identical across retained frames; leases last through every mapped consumer. Shared32-descriptor cap,128 ops/32 inputs,144 records/576 seams,864 accumulation maximum,8MiB log cap recorded before execution. Build A fails missing mem_b declaration; added direct sh4_mem include, B/C/D link. Capture A3frames exit0/clean close, first two frames48 records each, then budget rejection at sample368; retained as failed. Append-only execution log also exceeds16MiB loader bound; do not raise it. Read-only diagnostic session scan finds actual maximum prefix51 bytes versus charged128. Changed prefix allowance to80 with cap unchanged; archived exact owned stage log by explicit move, recoverable, no raw evidence deleted. Fresh B3frames exit0/clean close,5,960,324-byte session,576 blocks/144 X loads/144 X edges/864 writes/1278 gathers and426 source/copy/decoder links; no live rejection or accumulation. Draft full-mode arithmetic verifier passes426 outputs using compact descriptors and preindexed write/gather ownership. Initial verifier rejects scheduler-crossing supply and reciprocal edges; exact observed successor ENTRY cycles differ by448. Added optional exact successor stamp with monotonicity, no loose tolerance, preserving legacy strict default; synthetic correct/wrong/reversed clock controls pass.177 inspector tests pass; independent widened aggregate tests still pending. C same-binary wrong-X3frames exit0/clean close rejects, but preservation FAILS: producer cycles differ one frame (B first7608559168 versusC7605222912),17/29 PNG,3/3 depth,2/3 motion and3/3 scene files differ. No retry-to-green, no exact-input or native-preservation claim. Retained ignored patch/header fc067-draw-transform-b hashes FA804033C85ED3231C7DEBEFB95982C2C3218AC17577E38D8388443CD4C5405B / FFA56A4E5DA1132E130A5964269A33AB680B58EF0B02948B0A7E923CE09DFBF7 and comparison script. Original coordinate semantics/world camera/Remix presentation remain unproven. Next independent aggregate tests and full92-triangle calibrated draw, not another one-point trace. Temporary hooks remain owned/uncommitted; no four-build or commit claim.

#190 2026-09-08 88b82d7f4 plus working tree | widened independent synthetic fixture to426 observations with one shared27-operation descriptor and actual strip-terminal stores. Five new tests cover compact reuse, header writer/offset/value/event controls, inserted restart topology, missing descriptor/copy identity/completion, and budget rejection. RED: complete synthetic capture plus FC067_DRAW_REJECT was accepted; fixed verifier to reject that marker, GREEN. Added three record-grouping tests for explicit48-record reuse, unchanged words, generation/draw ownership, missing/duplicate observations and new record rejection. First retained-capture grouping failed because upstream addresses are hexadecimal strings, not integers; corrected strict address schema and goldens without changing acceptance. All176 inspector tests pass; retained full-draw capture A groups144 record instances feeding426 vertices with unchanged XYZ words per shared record. This is a source reuse map, not original transform or camera proof. No new game run, build, commit, push or exact-input preservation claimed. Old six-record transform collector consumes a record after its first XYZ triple; next integration must retain writer protection until every mapped consumer has read it. Temporary core probe remains owned/uncommitted. One guessed retained-header filename failed, corrected by enumeration.

#189 2026-09-08 88b82d7f4 plus temporary full-draw source probe | previous turn classified progress. Restored retained source/copy/decoder hooks and widened only draw1 to142 vertices across3frames;426 observations, bitset duplicate tracking, reused descriptor emitted once, actual pointer/generation/decoded association,8MiB formatted-log budget. Automation build A links; capture A3frames exit0/clean close,3.61MB current session,426 observed packets and1278 returned XYZ loads pass generalized checker. Original RAM transforms remain unproven. Initial checker rejected STORE count:75 strip endpoints include an eighth header store at8c03ccee, offset0/valuef0000000. Now independently require that store exactly at actual source-index strip boundaries; XYZ seven-store program remains separately checked, all other extra writes reject. An atomic patch context mismatch made no partial edits. Legacy scanner became quadratic at426 samples; preindexed copy/link positions and shared descriptors reduce the updated run to about1 second. Superseded read-only verifier PID21064 was positively identified and stopped after optimized verification succeeded; its exit1 is retained, not a pass.168 existing tests pass; widened-domain independent synthetic tests still pending. B live wrong-copy-ID capture3frames exit0/clean close and rejects, but paired input/image preservation FAILS: producer ordinal1781 cycleA7611895424 versusB7605222912, actual loaded values/stores and many images/depth/motion differ. Do not relabel this as the older small GPU discrepancy or retry to erase it. Actual entry pointers/read addresses agree. Positive mapping reveals142 decoded vertices reuse48 distinct XYZ records per frame, spanning8ce74230..8ce74520. Next transform collection can target these48 proven records (144 over3frames) while retaining426 downstream associations. Temporary patch/header retained ignored; hooks and checker changes still uncommitted, no broader production/camera/Remix acceptance.

#188 2026-09-08 88b82d7f4 plus draw-domain checker | previous turn classified progress. Inspected complete opaque draw1 in all three retained scenes:142 unique vertices4..145,166 indices including24 restart sentinels,92 nonrepeated-index strip triangles. Independent domain checker verifies exact frame/game/draw identity, range, vertex bounds, restart parity and256-vertex cap; entire index stream, vertex IDs and original draw/texture metadata agree across frames. Three synthetic methods pass, including restart/winding and wrong range/index/duplicate-domain/space/budget controls. This fixes the broader capture domain and avoids treating166 indices as166 vertices; it does not prove original transforms for142 vertices. No new probe, build, game run or GPU output claimed. Next compact collection budgets recorded before execution:426 actual observations,256/frame capacity,32 descriptors with128 operations,32 live words/transform,8MiB emitted-data cap, actual generation links and overflow/unsupported rejection. Existing calibration/partial-scene checker changes remain uncommitted; core remains hook-free.

#187 2026-09-08 88b82d7f4 plus partial calibrated scene checker | previous turn classified progress. Exposed actual preprojection record words from the verified aggregate, then mapped only the five supported vertices per frame through shared calibration. Original vertex/color/UV and draw/texture metadata are retained, normals unknown, output explicitly incomplete and nonrenderable by Remix adapter. Actual scene strip topology yields just1 fully supported triangle of8339 opaque index triangles in each frame;8338 omitted. Reprojection maximum across three frames1.8989340986763636e-5 pixels. This exposes the primary coverage gap and changes next priority to a complete bounded draw, not more same-point proofs. Initial run rejected valid0xffffffff restart indices; inspected production packet validator and actual sentinel census, then implemented explicit strip restart/parity reset. Counts describe nonrepeated-index triangles, not proven nonzero geometric area. Four synthetic methods cover restart, coverage omission, frame/vertex/word/range/draw identity and projection/depth negatives; full inspector suite165 passes. No new game run, GPU renderer, normals, world-camera, binary/config mutation or complete-pipeline claim. Changes uncommitted; calibration and packet slices require checkpoint checks. Next bounded coverage target is full opaque draw1, at most256 vertices/frame over three frames, with compact reusable descriptors and explicit budgets before execution.

#186 2026-09-08 88b82d7f4 plus calibration checker | previous turn classified progress. Exact88b82d7f4 four configure/builds pass; three selftests298/298, SDK63/63, inspector158 and binary325 pass. Fresh hook-free3-frame native capture exit0/clean close has correct SHA; full scene differs only by SHA and29 PNGs/3 depth/3 motion equal X-load B. Fork verified88b82d7f4e03d177bc1f5f26f2d085edd9dd4733, worktree clean before this slice. Reused existing factorizer on15 verified initial matrices plus6 verified added-contribution matrices, not a new trace. All21 are scaled orthogonal; maximum residual2.0128530217546158e-7. First-sample normalized scales614.7144300057109/565.5372521827936 agree across all21 within0.00017180337988520478, below existing0.001 bound. All15 projection centers are verified320/240; unit reciprocal depth remains separately proven. Doubled shared X calibration rejects. Three independent synthetic calibration methods pass, including rigid/uniform-scale equivalence, incompatible calibration, shear/shape/bounds; full inspector suite161 passes. This supports a bounded calibrated camera-relative representation, not unique physical intrinsics, world scale, whole-scene or production acceptance. Slot3 remains unsupported. Next scene-packet contract must carry explicit supported domains, axis/depth conventions and coverage rather than assuming all PVR draws share calibration. No emulator changes or runtime/config fetch. New checker remains uncommitted pending normal slice checks. A read-only request for a few lines of single-line scene JSON produced an oversized truncated result during prior exact-check inspection; later checks parse it structurally and emit only summaries.

#185 2026-09-08 checkpoint preparation | previous turn classified progress. Preserved final temporary core patch/header with SHA25639F8117E42F3D61EB172C4DE8ED2C02AEDA7DF447A644FAC6EC2F026588BBD59 /8C6CE00FD8C57E00F545C28B31566F0CFAE0DE1A49790958FA0B5FF4E947C262 and removed hooks; core diff empty. Independent aggregate protocol fixture tests composition/ownership using mocked already-tested arithmetic decoders, explicitly not a full synthetic GPU fixture.158 inspector tests plus5 binary32 tests pass; actual X-load positive and negative/mutations rerun correctly,29 PNGs/3 depth/3 motion remain equal. Four restored builds pass and three selftests298/298. Runtime bring-up committed separately as08c79c8d1; its post-commit four serial configure/builds and three298/298 selftests pass, SDK63/63. Other owned verifier/doc edits intentionally remained pending between the two distinct commits, not a clean-worktree claim. Backlog consolidated to remove superseded active trace instructions; next is a coordinate-contract decision, not repeating the now-proven loads. Transform evidence slice follows with exact-commit checks still required. Initial guessed handoff path was absent; actual root TESTER-HANDOFF updated. No third-party files/media/config or temporary core hooks staged.

#184 2026-09-08 1043fa4e3 plus temporary X-load probe | previous turn classified progress. Existing CALC descriptor identified next-record X load at8c03c9d6 into FR0. Bounded observer covers15 actual loads only for the five supported records, compares current RAM ledger and immediate next JIT entry FR0/r4 at8c03c9a4, then uses independently verified PRED register-write set and final CALC input. Initial A capture exits0/clean close but is rejected: generated callback gate only tested old ctPending, so X observation occurred one block late. Corrected assembly gate tests either pending event; failed A retained. Build B links; B positive and C expected-X-only negative each capture3frames exit0/clean close. Draft aggregate verifies15 X links,60 original seams,6 accumulations,108 writes,45 final ledger reads through existing accepted18 RAM/gather packet map; slot3 original transform remains unsupported. Live C and three textual writer/PC/generation controls reject. Actual X load/ledger records identical; all29 PNGs,3 raw depth and3 motion files are byte-identical B/C. This does not erase older retained preservation discrepancies or prove GPU performance.156 existing inspector tests pass before chronology tightening; actual B rerun passes added writer/load/edge/PRED/CALC chronology checks. Aggregate independent synthetic coverage/review, restored builds and commit remain pending. No production camera/coordinate-space or Remix/DLSS5 combined acceptance. All temporary changes remain owned/uncommitted.

#183 2026-09-08 1043fa4e3 plus working tree | previous turn classified progress. Source review of standalone bring-up found per-frame recreation of identical mesh/material/light resources and guessed window dimensions. Added validated camera-only redraw of one immutable retained scene, invalidation after draw failure, actual adjusted640x480 client sizing and live client aspect. SDK mock tests cover redraw before submission, moving camera with no new resources, unknown-camera zero-call rejection, failed redraw and refusal to resume,120 frames with fixed1 material/2 meshes/1 light and exact once-only cleanup. Build B and first60/60 tests pass; expanded build C and63/63 mock tests pass. Both standalone and mock targets link. These are public-call/CPU-resource tests, not loaded-runtime lifecycle, watchdog, GPU completion or image evidence. Real runtime remains unavailable in the configured cache. Do not spend another tranche expanding mocks merely to avoid that dependency. Next active work returns to M2-camera initial-X load/register lineage and aggregate evidence review. Worktree remains owned/uncommitted; no four-build or exact-SHA checkpoint claimed.

#182 2026-09-08 1043fa4e3 plus working tree | previous turn classified progress; independent M1-GPU dependency checked to avoid source tracing becoming the only route. Reviewed cache still contains only pinned header/licenses; actual runtime absent from this configured cache, no new whole-machine search or binary fetch. Read pinned public SDK guide and actual0.6.4 Startup/Present/Initialize/Shutdown declarations. Added separate developer remake-runtime-smoke executable: explicit absolute DLL,1..120 frames,30-second own-process watchdog, existing synthetic adapter, no config setters, bounded CPU resource retention, API-success reporting explicitly distinct from GPU/readback/completion proof. Automation target builds exit0; actual missing-arguments/relative-path/zero-frame/121-frame controls exit2, absent-runtime control exit3. Existing SDK mock56/56 rerun, runtime/GPU/Present false. Real runtime startup, output/readback, watchdog firing, completion lifetime and combined presentation NOT RUN. Runtime bring-up guide records exact gaps. Failed discovery commands retained: Windows rg wildcard path and guessed header/adapter locations were absent; corrected with file enumeration and CMake's actual include path. No third-party binary inspection/fetch/configuration mutation. M1-GPU remains incomplete; M2-camera's fifteen-sample draft, missing initial-X link, unsupported slot3 and image/depth discrepancy are unchanged. No commit or exact-SHA regression claim.

#181 2026-09-08 1043fa4e3 plus temporary accumulation probe | previous turn classified progress. Added independent synthetic identity-matrix/nonunit-W goldens and eight falsifying controls, plus explicit relocated-target/generation checks;156 inspector tests pass. Temporary accumulation builds A/B link. A captures3frames exit0/clean close, advances five records through four seams and first three gathers, then rejects unarmed slot3. B records slot3 unsupported (never calls full-six proven), retains every armed-record rejection and fixed caps, and collects15 samples over three contexts:60 original seams,6 FTRV-plus-add accumulations,108 actual writes,45 final ledger reads. Draft aggregate checker passes mathematical initial/accumulation/projection and actual final RAM/SQ/TA binding for those15; initial-X reload provenance remains explicitly false. Same-binary C expected-target-only negative captures3frames exit0/clean close and is rejected by the verifier. Native frame1782 visually inspected: fighters, arena and HUD present. Preservation comparison FAILS exact PNG/raw-depth equality: B versus C and earlier extra-writer E disabled differ at8 one-level components in frame1783 native/source/final plus derived flicker planes, and1023/5905 depth bytes atframes1782/1783. Complete scene JSON and all3 motion planes remain exact. This matches the size/pattern of the already retained repeatability discrepancy, not proof of its cause; no retry-to-green or visual/performance acceptance. Report initially asserted equality and failed, then was changed to retain component-level differences. Full source/camera/Remix GPU acceptance remains pending. Temporary hooks and verifier are still uncommitted; no restored four-build or exact-SHA claim. Next is explicit initial-X load-to-live-register lineage and aggregate synthetic/negative review, with slot3 still unsupported.

#180 additional-writer diagnostic, 2026-09-08 | extra-writer A/B exhaust six provisional candidates and remain rejected; B retains its last rejected instruction record. C uses the actually observed fourth-word address expression as a read-only eligibility filter:312 checks, one selected candidate, block8c03c95c,31 operations,21 live inputs,42 events, generation1790. Executed FTRV at8c03c96a transforms four source words (W=3f008001), then three separate FADDs add existing XYZ before exact read-back stores to8ce6e478/474/470. Draft extra_writer_inspect independently reproduces the integer address expression, rational FTRV and rounded additions; actual result passes. D expected-target-only live negative rejects; actual INPUT/OP/READ/VALUE/STORE/FILTER/ENTRY/EXIT records equal C. Eight targeted textual controls reject. E same-binary probe-disabled capture has no EXTRA tags. C/D/E each capture3frames, exit0/clean close;29 PNGs,3 depth.f32,3 motion.rg16f and complete scene JSON are byte/value identical. No raw r8 files exist in this format; do not count absent files as mask proof. All153 existing/relocation inspector tests pass. Original full-chain unknown-writer rejection remains active in C/D: this is one arithmetic witness, not complete source lineage, skeletal semantics, camera or Remix GPU acceptance. New verifier still needs independent synthetic fixtures and review before commit. Automation build C passed; full restored four-build matrix has not run for this uncommitted slice. Failed attempts retained: initial CLI passed a logfile to a directory loader; first mutation check altered an earlier SUPPLY row rather than EXTRA and therefore failed its expected rejection, corrected to explicit EXTRA rows; an incomplete LOG patch failed without changes. Temporary probes and verifier draft remain owned uncommitted work.

#180 coverage attempts | temporary builds A through E link. Four native3-frame captures exit0 and clean close but remain rejected, not camera evidence. A: overly broad interpreter guard rejects the already-known HLE route before any seam. B: record ledger armed before selected FTRV rejects pre-transform RAM reuse; subsequent correction requires full observed12-byte initial overwrite after arming. C: broad SQ-to-RAM guard still rejects before arming; corrected to actual overlap with armed/unconsumed records, including both optimized SQ paths. D: reaches two FTRV/initial-store pairs, then correctly rejects actual writer pc8c03c97c at8ce6e478, outside the four known seams. D prefix validates two FTRV mathematical results and their initial stores at8ce6e470/8ce6e480, generation1790/cycle7602363328; their W words are3f000001/3f000002. These prefixes are not final packet lineage. Initial-edge, initial-store, reciprocal and final-projection checkers now accept explicitly bound record/cycle/generation targets while keeping legacy defaults/math checks; existing150 tests and three new relocation/negative methods pass. Aggregate transform checker is an unaccepted draft and full source check rejects D as intended. Next is the one actual additional writer block, not another packet search or a invented camera. Temporary hooks remain in worktree, no unproven code committed.

#180 in progress 2026-09-08 1043fa4e3 plus temporary transform probe | pass C reuses the accepted gather/copy probe and four known operand seams, selected by actual record pointers and TA generations rather than fixed timestamps. Four seams x18 invocations maximum, six12-byte RAM ledgers with64-write cap each; current descriptor/live/event bounds remain128/32/512. Actual record stores are compared to their source operand; unexpected writer/fallback paths reject. Coverage capture and independent transform validation pending. A guessed record_value_inspect.py path was absent; existing record_calc_inspect.py was located and read. No camera or combined runtime claim.

#179 exact-checkpoint addendum | 1043fa4e383382f2e6399084913d0b5d9dea729b four serial builds pass, three enabled selftests298/298, SDK56/56 runtime/GPU/Present false, Python150+5 and backlog checker pass. Fresh hook-free exact capture has3frames, exit0/clean close, correct short SHA and no probe tags; scene/producer/27 PNGs/9 raw files equal C/D/E and older0e095eb75 native. Known discrepancies against c6450eafd native remain. First exact-report assertion incorrectly expected a full SHA instead of the established short manifest schema; corrected after inspecting version.h and full local/fork SHA. Both reports retained ignored. Fork matches full SHA and worktree was clean before current probe.

#179 restored-source checks | four serial configure/builds pass; automation/NGX/no-NGX selftests each298/298; SDK56/56 with runtime/GPU/Present false. Backlog checker passes. An audit filename collision was caught in diff review: the historical CAMERA-SOURCE-AUDIT.md was restored unchanged and this slice moved to CAMERA-GATHER-AUDIT.md. Core diff is empty. The failed atomic rename/restore attempt made no partial edits. Exact-commit checks follow after committing the verifier/doc slice.

#179 2026-09-08 c6450eafd plus removed source probe | collection pass B proves18 actual indexed RAM/SQ/copy/decoded XYZ links across two draws and three producer frames,54 returned XYZ loads. The independent verifier reuses scalar SSA arithmetic and checks all12 byte writers, both physical SQ slots, context generations, actual copy/decoder pointers and32 copied bytes. Six new synthetic methods pass; aggregate Python150 inspector plus5 binary32 methods pass. C positive passes; D same-binary expected-ID-only negative rejects18 times with unchanged actual observations; E disabled has no probe tags. C/D/E each exit0/clean close and preserve complete scene/producer identity,27 PNG planes and9 raw guidance files exactly against one another and older0e095eb75 native. Each retains the known1023/5905 raw-depth-byte and8 one-level color-component discrepancies against latest c6450eafd native. Native frame1783 visually inspected: fighters, arena and HUD present; no visual-quality promotion. A/B remain failed shutdown-diagnostic attempts; the first correction incorrectly depended on a later next-frame hint, replaced by completion at actual final producer queue. Initial build failed on unqualified Xbyak Label then corrected; patch-context and inverse-diff parsing attempts failed without partial edits before successful restoration. Temporary core hooks retained ignored and removed; core diff empty. ACCEPTED source-chain scope only, original RAM producer/camera/Remix GPU/combined gameplay pending. Next collection pass C reuses known transform/projection seams at the six proven source records; required restored/exact checkpoint builds and selftests follow.

#178 exact-checkpoint addendum | c6450eafd2c443a3b4a148a4dc4b26073ff913d7 four serial builds pass; enabled selftests3x298/298, SDK56/56 runtime/GPU/Present false, Python144+5 and backlog checker pass. Fresh hook-free exact-SHA capture returns three frames, exit0/clean close, no probe tags, same scene/producer identity. It reproduces the prior negative B graphics exactly, including retained raw-depth/color differences from the older native baseline. Therefore the probe is not necessary to reproduce those differences; cause remains unproven and strict preservation against the older baseline remains failed. Fork SHA verified and worktree clean before current temporary pass B. Exact report retained ignored; parked raster residual is not reopened.

#178 2026-09-08 c47f26b11 plus removed decoder probe | collect six actual opaque vertices across three producer frames, type3 packets at offsets32/64/96/4608/4640/4672, separately bound to draws1/26 and exact indexed vertices4/5/6/146/147/148. A verifies18 maps and eight offline controls. B same binary expected-epoch mutation rejects with actual map fields and full scene arrays unchanged, but fails strict GPU preservation: eight one-level color components in frame1783 native/source/final and raw-depth byte differences1023/5905 in frames1782/1783. C is one same-binary disabled control, not a retry-until-pass. A/C each match27 planes,3 producer identities,9 raw guidance files; C has no probe tags. A/B/C exit0 and clean close. Six synthetic methods pass; aggregate Python144+5 pass before restored checkpoint tests | accepted decoded packet map, not upstream FTRV/camera or complete A/B graphics preservation. Failed assumptions/attempts retained: initial checker expected type4 instead of observed type3; first paired-preservation script and its follow-up raw-depth assertion failed on B. Temporary core probe removed and patch retained ignored. Next actual gather/SQ/TA ownership at the mapped offsets, with the camera card still doing. Required restored/exact checkpoint builds and tests follow.

#177 exact-checkpoint addendum | c47f26b11996f223663dacf39e3629d75769d8db four serial builds pass; enabled selftests3x298/298, SDK56/56 runtime/GPU/Present false, Python138+5 and backlog checker pass. Retained opaque positive and expected negative reject are verified along with accepted historical translucent B. Fork SHA confirmed and worktree clean; ignored exact report retains logs. No fresh gameplay capture was claimed for the algebra-only change.

#177 2026-09-08 60196ee3a plus working tree | M2-camera algebra substep reuses existing transform_semantics_inspect instead of duplicating it. The new opaque CLI validates the entire retained initial-supply/record/SQ/TA chain, preserves nonunit W and binds actual split multiply/add plus unit reciprocal-depth scaling. Positive retained capture passes; retained same-build negative rejects its live predecessor. Accepted historical translucent capture B passes; first trying historical A correctly rejected its incomplete copy/decode witness. Four new independent synthetic methods pass; aggregate Python138 inspector plus5 binary32 methods and live backlog consistency pass. Wrong borrowed translucent depth scale, forced unit W, wrong X scale and uncompensated basis fail; compensated basis remains equivalent | accepted one-opaque-sample algebra, not new gameplay capture or shared camera. Broader six-vertex/three-frame collection remains active; no production code/runtime/config/media changes. Failed read-only attempts retained from the interrupted investigation: oversized single-line scene JSON and combined reads were truncated, and guessed source/audit filenames were absent; corrected bounded JSON selection and actual file discovery located reusable existing algebra. Required checkpoint builds/selftests follow.

#176 exact-checkpoint addendum | 60196ee3a9e3277046b03ab157a1789621a0b17b four serial builds pass, three enabled selftests298/298 each, SDK56/56 with runtime/GPU/Present false, Python134+5 and backlog checker pass. Fork SHA verified and clean. No new gameplay capture was needed for governance/checker-only changes; ignored exact verification report retains commands/log locations.

#176 2026-09-08 0e095eb75 governance correction | user requests a lighter standing goal and autonomous adherence to the backlog through working RTX Remix plus supplied DLSS 5. D-114 supersedes routine human permission stops; AGENTS becomes one concise backlog entry point, and obsolete current-task stacks are removed from live launch documents while audits/LOG/Git preserve history. The backlog defines one active M2-camera card, independent M1-GPU work, gated scene/relighting/presentation/chaining/hardening, a two-tranche no-progress pivot and concrete combined-gameplay acceptance | documentation/workflow change, not a new rendering result. The initial old tracker was blocked; a later fresh get_goal returned null, so create_goal successfully registered the exact short backlog objective as active without false completion or app-storage edits. No runtime/config/media changes. Read-only backlog checker and six focused methods pass, including malformed row, duplicate active pointer, unfinished dependencies, unsupported camera, cycles, missing stages and oversized/stale-entry controls. It does not schedule work or verify rendering evidence. Required checkpoint builds/tests follow. Failed attempts retained: the checker first rejected a malformed five-column table, which was corrected; an apply_patch with out-of-order contexts failed before edits and was split into ordered hunks. Initial combined file reads were truncated and replaced by focused section reads; an obsolete official goal URL failed and was replaced by the working official use-case link.

#175 exact-checkpoint addendum | 0e095eb7578bc11dfd33a7593a0548b4fc488795 four serial builds, three298-check selftests, SDK56 mock/runtime-GPU-Present false and Python128+5 pass. Fresh hook-free three-frame capture has exact SHA and matches27 image planes,3 producer identities and9 raw-guidance hashes; diagnostic tags absent. Fork SHA confirmed and worktree clean. Evidence is retained in the ignored exact verification report; #176 changes routing, not these observed results.

#175 restored-validation addendum | four serial configure/builds pass; automation/NGX/no-NGX selftests3x298/298, public SDK56/56 runtime/GPU/Present false, Python128 inspector plus5 binary32 methods pass. Native/producer/raw preservation and positive/live-negative checks pass. Core diff empty, diagnostic patch/header retained ignored. Same-task continuation with model gpt-6-astra and thinking low was accepted by the app tool; no claim of retroactively changing the earlier turn. Exact-SHA checkpoint verification follows.

#175 2026-09-08 56cfc50ba plus removed diagnostic | user-approved immediate initial-record predecessor | A finds actual block8c03c93a, eleven ops/nineteen live words/eighteen events, loading four source words from8c8ea410..1c and executing FTRV at8c03c944. Independent exact-rational reference matches all four dynamic outputs; first three reach the selected initial-store invocation unchanged through an actual edge. W3f8005df is preserved, not normalized. B same-build expected-Z-only mutation409052c6->409052c7 rejects; actual data and prior store chain are unchanged. Six offline controls and seven synthetic methods pass. A/B exit0/clean close;54 unique image/6 producer/18 raw-guidance comparisons match56cfc50ba baseline. Temporary hooks/header removed; ignored patch/header pair retained. Python128 inspector plus5 binary32 methods pass | scoped transform/edge accepted, matrix/source coordinate semantics and full M2 remain unknown. Restored build/selftest and exact-SHA checkpoint follow. Failed read-only lookups retained: Windows rg wildcard was invalid, transform_value_inspect.py and rec_x64.h did not exist, and execution.log was requested before the active capture had copied it; corrected concrete source searches/latest completed capture supplied the actual evidence. No failed capture or numerical control was hidden.

#175 2026-09-08 56cfc50ba authorization/task update only | user explicitly approves one immediate predecessor supplying FR0-FR2 to the selected initial-store block and requests lighter-mode continuation without repeated audits. Governing entry points now route to the bounded task in OPAQUE-INITIAL-STORES-AUDIT.md; D-112 retains non-recursive bounds and unchanged full M2 acceptance | worktree was clean at starting SHA. No new production code, build, capture or test result is claimed in this authorization entry. Model routing requests Astra low; actual dispatch confirmation must be reported separately. Existing goal remains incomplete; the goal API exposes terminal status only, not objective editing/resume, so do not falsify completion to replace it.

#174 restored-validation addendum | four serial configure/builds pass; enabled selftests3x298/298, SDK56 runtime/GPU/Present false, Python121 inspector plus5 binary32 methods pass. A-C preservation and accepted-store/rejected-control checks pass. Core diff empty. Exact-SHA verification/native capture follow checkpoint; coordinate calculation remains unproven.

#174 2026-09-08 d38c25f76 plus removed diagnostic | initial record store boundary | A/B observe block8c03c94c, ten ops/five live-ins/ten events, copying FR0/1/2 unchanged into the earlier record. No loads or floating-point/transform calculation occurs. B targets the live wrong-input query at actual FR2 rather than unused r4; C same-build expected-FR2-only mutation rejects without changing data. Four offline controls and five synthetic methods pass. A-C exit0/clean close;81 unique image/9 producer/27 raw-guidance comparisons match baseline; prior factor/source-generation chain passes. Temporary hooks/header removed, ignored patch/header pairs retained | store operands accepted; initial coordinate calculation NOT_REVIEWABLE within this block. Scope stop, no automatic predecessor/caller/site expansion. Next remaining-M2 requirements audit and exact unmet-scope return, not another self-authorized trace. Restored builds/checkpoint follow.

#173 exact-checkpoint addendum | d38c25f763a9405b47d70d37323ed4b6105be83e four builds/3x298 selftests/SDK56/Python116+5 pass. Fresh exact-SHA hook-free capture matches27 unique planes, three producer identities and nine raw guidance files; diagnostic tags absent. Fork verified and clean before #174.

#173 restored-validation addendum | four serial configure/builds pass; enabled selftests3x298/298, SDK56 runtime/GPU/Present false, Python116 inspector plus5 binary32 methods pass. Retained A/B preservation and positive/negative predecessor checks pass. Core diff empty; exact-SHA verification and hook-free native capture follow commit.

#173 2026-09-08 20e3d3eab plus removed diagnostic | reciprocal record-depth edge | A observes predecessor8c03c9a4, seven ops/two inputs/nine events: earlier Y/Z reads, unit numerator, positive comparison and exact toward-zero1/Z divide. Actual successor8c03c9c0 retains pointer/factor. B same-build expected-factor-only mutation rejects at live edge, with actual data unchanged. Five offline edge/program controls and a sixth overwritten-source-generation control reject; the latter first passes the prior final-record/calculation verifier. Six synthetic methods pass. A/B exit0/clean close;54 unique image/6 producer/18 raw-guidance comparisons match baseline; prior calculation chain passes. Temporary probes/header removed, ignored patch/header pair retained | accepted reciprocal record depth only; initial record calculation/coordinate system/world camera unknown. Next bounded already-observed initial-record producer8c03c94e/50/52, no recursive edge expansion. Restored builds/checkpoint follow.

#172 exact-checkpoint addendum | 20e3d3eab6cafcf3f68c8cea488f04d0a390f968 four builds/3x298 selftests/SDK56/Python110+5 pass. Fresh exact-SHA hook-free capture matches27 unique planes, three producer identities and nine raw guidance files; diagnostic tags absent. Fork verified and clean before #173.

#172 restored-validation addendum | four serial configure/builds pass, enabled selftests3x298/298, SDK56 runtime/GPU/Present false, Python110 inspector plus5 binary32 methods pass. Retained A-C preservation and positive/negative calculation checks pass; core diff empty after removing probes. Exact-SHA builds/native capture follow commit.

#172 2026-09-08 8531ed2bb plus removed diagnostic | final opaque coordinate calculation | A missing FP mode retained/rejected; B observed block8c03c9c0 has17 ops/seven live-ins/18 events and proves X=a*q+320,Y=b*q+240,Z=q with four exact rational/binary32 intermediate checks, MXCSRfffd toward zero. C same-build expected-offset-only mutation rejects without changing registers/RAM/packet. Five corrected offline controls reject; eight synthetic methods cover independent golden arithmetic and faults. Initial store-address control mutated the earlier RAM line and was wrongly accepted; restricted CALC mutation plus shadow-line regression fixes that failed attempt. All A-C exit0/clean close,81 unique image/9 producer/27 raw-guidance comparisons match baseline; prior RAM chain passes. Temporary hooks/header removed, ignored patch/header pairs retained | final block accepted only; q origin/semantics and world camera remain unknown. Next one bounded actual predecessor edge supplying FR3, no recursive caller or repeated final-block trace. Restored builds/checkpoint pending. Failed register-header search corrected to sh4_if.h.

#171 exact-checkpoint addendum | 8531ed2bbc4d6292a18f103aabd7524f14da1803 four builds/3x298 selftests/SDK56/Python102+5 pass. Fresh exact-SHA hook-free capture matches27 unique planes, three producer identities and nine raw guidance files; diagnostic tags absent. Fork SHA verified and clean before #172.

#171 restored-validation addendum | all four serial configure/builds pass; enabled selftests3x298/298, SDK56 with runtime/GPU/Present false, Python102 inspector plus5 binary32 methods pass. Core diff empty after probe removal. Retained A-E preservation/verifier script rerun passes. Exact-commit verification and hook-free capture follow checkpoint.

#171 2026-09-08 1a9e4ab2d plus removed diagnostic | bounded12-byte record writer replay | A unobserved fallback, B identified HLE085b/8c001006 and C unhandled drive-status command4 all retained/rejected. D instruments guarded HLE writes and observes six CPU stores: initial XYZ overwritten by final PCs8c03c9ca/cc/ce, then exact consumer loads. E same-build expected-byte-only mutation rejects at event3 without changing game bytes. Five offline controls reject; ten synthetic methods pass. A-E all exit0/clean close,135 unique native image planes/15 producer stamps/45 raw guidance hashes match hook-free1a9 baseline; all prior XYZ chains and controls pass. Temporary12-core-file hooks/header removed, patch/header pairs retained ignored | accepted observed byte-writer replay only, not universal write coverage, coordinate calculation or camera. Next bounded final-store block calculation. Initial preservation script KeyError on C old schema corrected to explicit fail-closed schema validation and rerun successfully; wrong source-path searches retained as failed checks. Restored build matrix/checkpoint verification follows.

#170 exact-checkpoint addendum | 1a9e4ab2d300cff9684f2ec99a8b0d166bcca52b four builds/3x298 selftests/SDK56/Python92+5 pass. Fresh exact-SHA hook-free capture matches27 image planes, three producer identities and nine raw guidance files; probe markers absent. Fork verified and clean before #171.

#170 restored-validation addendum | four serial configure/builds pass, enabled selftests3x298/298, SDK56 runtime/GPU/Present false, Python92 inspector plus5 binary32 tests pass. Core diff empty after diagnostic removal. Exact-commit verification and hook-free capture follow checkpoint.

#170 2026-09-08 d98ac822b plus removed diagnostic | selected indexed XYZ gather | A records five same-cycle loop iterations and is retained/rejected as single-target evidence. B context/TAoffset/SQ-qualified entry records27 ops, six live-ins, eight actual load addresses and20 live results; scalar SSA replay binds index05df/stride16/base8ce6e460 to reads8ce74250/54/58 and the actual XYZ SQ stores. C live wrong-base expected query rejects without changing input registers or packet bytes. Five independent offline operand mutations reject; eight synthetic methods pass. A/B/C each exit0/clean close,81 unique image comparisons/nine producer comparisons and27 raw guidance hashes match baseline. Temporary hooks/header removed and exact patch/header pairs retained ignored | accepted indexed address/returned-load proof only; positions precomputed in RAM, world camera/M2 still unknown. Next exact12-byte record writer generation, not recursive callers or effect-camera reuse. Restored matrix/checkpoint pending. Failed Windows wildcard searches corrected with rg -g.

#169 exact-checkpoint addendum | d98ac822bd15c7d25796ad79ee724b34848670f7 four builds/3x298 selftests/SDK56/Python84+5 pass. Fresh exact-SHA hook-free capture matches27 planes and three producer identities; both probes absent. Fork verified, clean before #170.

#169 restored-validation addendum | all four serial configure/builds pass; enabled selftests3x298/298, SDK56 runtime/GPU/Present false, Python84 inspector plus5 binary32 methods pass. Core diff empty after probe/header removal. Exact-commit and hook-free capture verification follow checkpoint.

#169 2026-09-08 c6a8ee668 plus removed probes | bounded SQ filling-store proof | A no activation and B256-event cap retained/rejected. C observes eight executed32-bit stores covering32 bytes, followed by flush event9, all at cycle7602643776; XYZ PCs8c03cc82/84/86 bind through R1 to opaquevertex4. D flips only expected X word; actual bytes unchanged, runtime store-mismatch and verifier rejection. All A-D exit0/clean close;108 unique plane and12 producer comparisons match baseline. Verifier accepts C/rejects A/B/D and five offline mutations; eight new synthetic methods pass. Queue flush does not clear bytes; corrected alias/flush handling and last-writer retention documented. Temporary nine-file hooks plus header removed, exact pairs saved ignored | accepted SQ filling stores only, not position calculation or camera. Next bounded executed-block operands, see OPAQUE-SQ-WRITERS-AUDIT.md. Restored matrix/exact checkpoint pending.

#168 exact-checkpoint addendum | c6a8ee668438e8bfb9bb5ff634759c961b322120 four builds/3x298 selftests/SDK56/Python76+5 pass; exact-SHA hook-free capture matches27 planes and three producer identities, probe absent. Fork verified and clean before #169.

#168 restored-validation addendum | all four serial configure/builds pass, three enabled selftests298/298, SDK56 runtime/GPU/Present false, Python76 inspector plus5 binary32 methods pass. No core diff remains. Exact checkpoint verification and hook-free native capture follow commit; no camera or overall M2 promotion.

#168 2026-09-08 fd79db700 plus removed bounded probe | actual selected TA copy | A logs TAWriteSQ/slot1/address e0000020 at cycle7602643776, eight exact original/copied words, and same-context destination into decoder packet32/member36/opaquevertex4. B live generation2 query rejects actual generation1 while original copy words remain identical. A/B exit0/clean close; each preserves27 native planes and three producer stamps against fd79db700. Verifier accepts A/rejects B and five actual-input offline mutations; six new synthetic methods pass. Temporary seven-file core probe removed and patches retained ignored | R1 accepted only for selected type3 path. Next R2 physical SQ last-writer witness under reviewed bounds; no CPU producer/camera/GPU claim. Restored full matrix/checkpoint pending. Initial Python test import duplicated five old tests; module import corrected before final count.

#167 exact-checkpoint addendum | fd79db7000d6198aba13cfd0b27d57aefee723a3 four builds/3x298 selftests/SDK56/Python70+5 pass. Fresh hook-free capture reports exact SHA and matches27 planes plus three producer identities; probe absent. Fork SHA verified, clean worktree before #168.

#167 restored-validation addendum | four serial configure/builds pass; enabled selftests298/298 each, SDK56 runtime/GPU/Present false, Python70 inspector plus5 binary32 tests pass. Decoder production changes removed before restored builds. Exact-commit confirmation and fresh hook-free native preservation follow checkpoint; no transfer/camera promotion.

#167 2026-09-08 7a683e2fc plus removed diagnostic | user-approved R1 reverse packet locator | helper RED selftest293/2 fails reused/backwards generations; corrected high-water/remap checks298/298. Probe A captures vertex-member offset36 incorrectly as32-byte packet; retained and rejected. Corrected B records type3 packet offset32/member36 and final opaque draw1/vertex4. Both captures exit0/clean close and each preserves27 unique native planes plus three producer stamps against84231a2af. New verifier accepts B, rejects A; five synthetic methods pass. Temporary decoder probe removed and patch retained ignored | R1 PARTIAL: TA-buffer-to-final-vertex only, no original transfer, CPU producer or world camera. Next actual copy witness for this packet, not another locator. Restored full matrix and checkpoint delivery pending. Exploratory nonexistent dx11rend.cpp/test_transform_inspect.py paths and oversized one-line scene dump were corrected by scoped path/JSON access; no success claimed for those commands.

#166 2026-09-07 84231a2af | exact checkpoint and M2 requirements audit | four serial builds pass, enabled selftests284/284 each, SDK56 with runtime/GPU/present false, Python65+5 pass. Fresh exact-SHA hook-free capture exits0/clean close; all27 unique image planes and producer identities match independent baseline, SHA checked. Fork ref verified84231a2af. Current source predicates and retained frame1804 failed strict replay proof inspected | evidence checkpoint delivered; M2 INCOMPLETE. M2-REQUIREMENTS-AUDIT.md records missing opaque/world camera, parked strict equality, bounded determinism and absent GPU runtime proof. Stop automatic caller tracing at approved scope boundary; request a concrete new bounded route before more camera investigation. No false goal completion.

#165 2026-09-07 87492e1d3 plus removed diagnostic | known-site opaque candidate investigation A-I | actual transform stores, wrong-expected runtime control, six source reads, three z/y/x copy stores, two copy reads and a four-block invocation returning scalar pair outputs observed. A-I named captures close cleanly and compared runs retain27 unique native planes plus producer identities. Retained I verifier rejects four offline mutations;65 inspector tests pass. I build initially fails missing sstream/locale headers, corrected I2 passes; saved-diff application failures retained. Temporary rec_x64 hooks removed and exact patches saved ignored | UNCORRELATED to PVR, not a camera or opaque-geometry success. Restored full matrix/checkpoint verification next. Do not expand arbitrary caller tracing from this bounded result; audit M2 gaps. No proprietary artifacts/configuration staged.

#164 2026-09-07 d2954272e | source review and separate no-reset exact-SHA preservation | mainui save/load is host-frame triggered; emu.stop waits for executor termination without selecting a guest cycle, so equal marker frames/byte counts do not prove identical states. No production scheduling change. Separate native capture exits0/clean close, three manifest SHAs verified, producer ordinal/cycle matches metadata A,27 unique planes exact | ACCEPTED metadata/native-preservation slice. Reset image comparison remains failed and original offset cause remains unproven. Next opaque source coverage; no repeat reset capture loop. Initial documentation patch failed on an outdated heading and was corrected before commit.

#163 2026-09-07 d2954272e | exactcommit identity validation | four builds,3x284 selftests,SDK56,Python61+5 pass. Save/load capture exits0 with valid marker, but equal-frame native comparison FAILS. Producer metadata now shows a3336256-cycle offset at first two retained frames despite equal epoch4/ordinals756-758 and main-loop save/load1000/1030. Native/depth/draw-ID differ; exact input equality is not established | CORRECTIONS_REQUIRED reset comparison; fail-fast prevents push. Inspect main-loop transition versus emulated scheduling, no identical recapture loop or silent alignment. Raw exactcommit reset run retained. No world camera/Remix promotion.

#162 2026-09-07 29e4a66de plus working tree | capture producer identity and in-memory reset harness | four serial builds pass; enabled selftests284/284 each, SDK mock56/56 with no runtime/GPU/presentation, Python61 inspector plus5 binary32 tests pass. Three prelaunch invalid controls exit2. Actual save/load C capture exits0/clean close, strict marker verifies frames1000/1030, and27 unique planes exactly match B. Epoch4/ordinals756-758 identify retained reset-era submissions. Trace F independently agrees with no-reset production metadata A for three producer cycles; four offline trace mutations reject | ACCEPTED bounded working-tree identity/reset evidence, commit/exact-SHA validation pending. Original offset remains unexplained. Retain trace linker failure, Windows max-macro build failure, TA_context test-link failure and invalid locale-formatted marker A; corrected builds/tests do not erase them. See FRAME-IDENTITY-AUDIT.md and D-102. Main-scene camera/native replay/Remix GPU remain open.

#161 2026-09-07 29e4a66de unchanged executable | equal-frame repeat I | same replay/settings/restored save inputs; capture exits0/clean close. All27 unique image planes equal restored H at same IDs, unlike first exactcommit run. This falsifies a consistent commit-dependent image shift, not the observed nondeterminism. Source frameId counts CaptureGeometry/CaptureSource invocations, not emulated vblanks | investigate missing/extra renderer capture invocation; preserve failed run, no silent alignment or determinism promotion.

#160 2026-09-07 29e4a66de | postcommit qualification | four builds,3x262 selftests,SDK56,55 inspector and5 binary32 tests pass; game capture exits0/materials verify. Equal-frame native comparison FAILS with substantial changes. New1782 equals old1783 and new1783 equals old1784 in native color; launch records/replay hash equal. Root cause pending. Shell continued after failed validator and pushed evidence commit; recorded procedural failure | CORRECTIONS_REQUIRED exact-frame validation, not renderer regression proof or1-LSB issue. Preserve raw run; diagnose frame identity, no silent realignment.

#159 2026-09-07 916d0cc19 plus removed bounded probe | FC-067 combat packet | actual reads/SSA copies/SQ flush/RAM generation/TA input/copy/decode/snapshot link to frame1782 translucent quad and visually inspected koi texture. Ten offline packet controls reject;55 inspector and5 binary32 tests pass. Four restored builds,3 enabled262/262 selftests,SDK56/56 pass. Uninstrumented H capture has27 unique planes exact, exact witnessed scene,3 verified material packages; no replay/performance claim. Initial negative-target failure retained/fixed | ACCEPTED bounded fish-sprite observation only; camera/main-scene/Remix GPU open. Next existing opaque coverage/source-seam review, not blind census. See COMBAT-PACKET-AUDIT.md.

#158 2026-09-07 7bbdb0ebb plus removed temporary bounded probe | FC-067 combat candidate projection/RAM path | existing site8c0610b4 selected at cycle7601913984, eight actual completed blocks retained; first projection block8c0610d2 produces words43dc80af/439f9171/3d1dec06 at pinned8ce6e460. Captured matrix arithmetic, separate mul/add projection, pointer progression, RAM snapshot and five offline controls verify; six arithmetic tests pass. D build/capture exit0/clean close,27 unique native planes exact; B also exact. A compiler API typo, B missing metadata/moving window, C locale-formatted PCs retained as limited attempts | ACCEPTED block-path/projection/RAM observation only. Source primitive family, TA and opaque coverage remain unknown. Next bounded generation reads/copies with overwrite/reset limits. Probe removed/saved ignored; see COMBAT-PATH-AUDIT.md. No production rendering/configuration or proprietary data staged.

#157 2026-09-07 eb1b67bcf plus offline verifier | FC-067 scene/material association | compiled decoder and full prior lineage verify both identical snapshots; nine synthetic association methods pass and 12 actual-input offline mutations reject. Matched frame1302 differs only in explicit source SHA; material inspector verifies three packages, 36 first-frame assets, 35 previews. Source draws5/6 and sorted-state20 bind indexed asset31/palette336; visually inspected as a petal-like sprite. Native frame shows Emperor's Garden intro, not opaque combat coverage. New capture returns1: frame1302 decoded/native1 pixel, retained/native7, decoded/retained6, maxdelta1; frames1303/1304 exact, wrong viewport/depth controls fail throughout. RED missing-module failure retained | ACCEPTED bounded association, not replay/whole-scene calibration/M2. Next opaque combat lineage or precise unsupported disposition; do not restart precision loop. See SCENE-ASSOCIATION-AUDIT.md; no core or external configuration changes.

#156 verification addendum | temporary probe removed; four restored configure/builds return 0, enabled selftests 262/262 each, SDK mock 56/56 runtime/GPU/Present false. Three-frame native/material/replay capture exits 0 and closes cleanly, 27 unique native planes exact, decoded/native/retained replay differences zero, wrong viewport/depth controls differ materially. Material inspector verifies three v2 packages and 39 first-frame assets. Incorrect positional CLI usage and nonexistent guessed inspector path retained in audit | ACCEPTED bounded contract correction (self-review), not full M2 or historical 30-frame parity closure. Exact-commit confirmation follows in ignored note; next repeatable snapshot association and actual primitive/material/visibility coverage.

#156 2026-09-07 8197983fe plus working changes | FC-067 sorted scene contract | empty-draw red 245/3 then green 248/0; sorted source-range red 248/2, initial green 250/0 and expanded controls 262/0. Actual original/different snapshots export byte-identically at instrumentation frame 1302, epoch 1310; 15563 vertices, 11274 indices, 3259 draws, 19 sorted commands. Full prior lineage/shared calibration and 54 later native-plane comparisons pass. Source association finds translucent draws 5/6 with shared texture state; second sorted command uses merged material state 20. Early/late rejected attempts retained; late stale metadata 1301 not accepted. Temporary probe removed; restored/exact-SHA regression pending | CORRECTIONS_REQUIRED checkpoint until remaining regression completes. See SORTED-SCENE-AUDIT.md; no world/visible-object/Remix GPU claim or external configuration change.

#155 2026-09-07 0f60b8261 plus removed temporary selector/epoch probe | FC-067 shared calibration pair | original and candidate-4 different matrix reach decoded vertices 14526/14529 in epoch 1310/context 00509700; same actual polygon bulk cycle/count/origin and matching 96-byte packet/copy spacing. Full chains and exact arithmetic pass with explicit observed event profiles. Normalized scales agree within shared reprojection error 2.995133e-05 pixels over two observed/six synthetic points. Offline wrong-scale and stale-epoch mutations reject; five independent shared tests pass. Both captures exit 0/clean close with 3 frames and 54 native planes exact. Initial different-sample checks reject nondiscriminating reciprocal rounding and original event numbers; preserve original strict controls, permit explicit additional profiles without skipping actual arithmetic/value/dependency checks | ACCEPTED two linked instances only; second point is off-screen, semantic geometry-family and whole-scene coverage not proven. Next actual scene-generation snapshot and draw/list/material/visibility association. Probe/header removed; patches/raw evidence ignored. See SHARED-CALIBRATION-AUDIT.md; no proprietary/configuration/assets staged.

#154 2026-09-07 55976c40d | FC-067 linked composite-transform algebra | immutable positive context capture passes full lineage and exact-dot/output-word checks; actual rounded projection equals decoded xyz. Scaled-axis factorization has Gram residual 3.318944e-08. Wrong layout rejects; doubled x scale and uncompensated model/view change cause 285.431/254.553-pixel errors. Compensated rigid model/view alternatives project identically for one observed and three synthetic points. Four synthetic test methods pass. No new game sequence or binary64 instruction emulation claimed; no failed attempts in this algebra tranche | ACCEPTED one linked composite's algebra only. Shared calibration/unique world camera/physical scale/Remix remain unproven. Next minimum additional distinct linked samples with exact context ownership to test shared projection/depth calibration; do not pursue a unique world origin as a prerequisite for labeled view-space feasibility. See TRANSFORM-SEMANTICS-AUDIT.md.

#153 2026-09-07 be6a91b9e plus removed temporary source/TA-copy/decoder probes | FC-067 exact source-to-decoded-vertex link | actual copied TA context 00509700, byte offset 507616, full packet exact; ordinary converter appends vertex 14526 with xyz words 44175b90/42ab1024/3d95b6cd, identical to traced source arithmetic. Wrong expected context xor32 fails while actual chain remains unchanged; positive B/negative C each exit 0, clean close, 3 frames, and match native planes 27/27. Five parser methods pass including ten malformed fields, invalidation and reordering. Attempt A compared payload pointer with packet start, missed decode and ended at recycle; corrected offsetof(TA_VertexParam,vtx0)==4 assertion and comparison succeed. Nonexistent ta_vtx.h source-search error corrected. Exploratory matrix row normalization suggests scaled orthogonal axes but no semantic gate | ACCEPTED one source-to-decoded-vertex chain only (self-review), not world camera/physical depth/presentation/Remix GPU. Probe/header removed; exact patches and headers/raw captures retained ignored. Next source projection/model/view semantics with falsifying controls, not another lineage census. See TRANSFORM-DECODE-AUDIT.md.

#152 2026-09-07 6ad78a7bb plus removed temporary x64/TA-entry probes | FC-067 bounded source-to-TA-input chain | four derived RAM reads agree with actual registers; three compiled-SSA-linked stores agree with queue readbacks. Actual area-3 SQ flush writes an exact 32-byte RAM packet. That pointer lies at byte offset 704 in a 928-packet polygon TAWrite input, all bytes equal, event 584739/cycle 6002105984. Wrong diagnostic expected offset 736 fails while actual chain remains unchanged; positive C/negative D each match baseline native planes 27/27 and close cleanly with exit 0. Five parser methods pass including ten malformed fields, invalidation and reordering controls. Limited G2 A/B/C stages and G3-A 500001-event timeout retained; expanded G3-B reaches input. Initial synthetic positive fixture omitted terminating access and failed; corrected fixture plus explicit final-write check passes. Two rg wildcard path errors corrected | ACCEPTED one source-transform-to-bulk-TA-input association only (self-review), not decoded vertex/context, camera, physical depth, presentation or Remix GPU. Next exact context/decoded vertex then source projection semantics. See TRANSFORM-TA-INPUT-AUDIT.md. No proprietary binaries/configuration/assets staged.

#151 2026-09-07 adff200af plus removed temporary x64 probe | FC-067 derived-coordinate arithmetic | B/C-negative native captures exit 0, three frames, clean close; exact-rational reference matches reciprocal, scale and two fused operations under observed toward-zero rounding, plus three actual RAM writes. Wrong shadow x-offset rejects one operation; wrong nearest rounding rejects reciprocal by one ULP. Actual source/operands/results/stores unchanged; 54 native plane comparisons match restored adff200af. Five arithmetic and four derived-parser test methods pass, including ten malformed-field cases. Limited A omitted division and is retained. Four restored-source configurations build successfully | ACCEPTED local arithmetic/store observation only, not TA, camera, physical depth or Remix GPU. Next bounded derived-generation submission lineage. Probe patches/raw evidence retained outside tracked source; no binary/configuration/asset changes staged. See TRANSFORM-DERIVED-AUDIT.md.

#150 2026-09-07 fcc8dc84f plus removed temporary x64 probe | FC-067 bounded witnessed-span consumer trace | four actual loaded values agree with original transform stores; three compiled-SSA-linked copies reach 8c8b6d00/04/08 with exact post-store readbacks. Original span ends at event 10, instruction 8c070c92, replacing 4163a016 with 3d95b6cd. Wrong first-read expected bit fails without changing executed source, copies, overwrite or native images; C/D each match baseline 27/27 planes. New verifier and five parser test methods pass, including ten field mutations and reordered-copy rejection. A pre-access-only/B loaded-value/C copy-readback stages retained; no build/capture failures in this tranche | ACCEPTED one CPU buffer-consumer chain only, not TA/DMA/SQ/camera/world/Remix. Probe removed; next capture actual derived-coordinate arithmetic and stores as a new generation before pursuing TA. See TRANSFORM-CONSUMER-AUDIT.md; no proprietary assets, runtime or external settings staged.

#149 2026-09-07 23e3d416d plus removed temporary x64 probe | FC-067 instruction-site and transform-to-RAM witness | corrected targeted F/G builds and three-frame captures close cleanly. FTRV at 8c070c3c, SH4 cycle 6000468096, feeds four exact ordered guest-RAM writes/readbacks at 8c00f2fc-8c00f308. Wrong diagnostic expected bit in G rejects one word without changing guest data; both runs match nine native image planes 27/27 each. Inspector verifies identical executed input/matrix/output/cycle and rejects missing-negative control. Initial compile context errors, C site-cap overflow, uncorrelated D/E candidates, and append-only-log parsing failure retained in TRANSFORM-STORE-AUDIT.md. Probe removed, four restored-source builds pass | ACCEPTED one intermediate-RAM witness only (self-review), not TA/camera/world reconstruction. Next bounded read/overwrite lineage for this span, no census or renderer integration. Raw data/patches remain outside tracked source.

#148 2026-09-07 b7e621b48 plus temporary census | FC-067 actual x64 FTRV execution observation, no matrix extraction | corrected B automation build and legal three-frame native capture exit 0/clean close. First observed call at SH4 cycle 537104512; 45,001,051 cumulative calls by cycle 7600021632. Calls remain uncorrelated to submitted vertices. Nine named captured image planes equal opt-out 27/27. Initial INFO_LOG run supplies no evidence because Release compiles it out; glob comparison initially fails expected-count assertion despite 29 identical PNGs, then explicit nine-plane selection passes. Source config hash unchanged. Probe removed; exact temporary patch/raw logs retained outside tracked source | ACCEPTED execution census only, not camera/depth/world recovery or performance. No more presence testing; next is bounded guest-instruction/value lineage to actual TA packets. See CAMERA-SOURCE-AUDIT.md.

#147 2026-09-07 b7e621b48 | FC-067 read-only camera source-seam investigation | FTRV exposes executed matrix/vector arithmetic; x64 canonical helper lacks guest-PC/submission lineage. SQ and RAM-backed DMA both feed TA packets, which retain no originating transform. Interpreter option identified; no actual Soulcalibur FTRV use or camera correlation captured. Windows wildcard search failure corrected with concrete directory search. No new build/game/GPU test | ACCEPTED source audit only, camera recovery NOT_REVIEWABLE. CAMERA-SOURCE-AUDIT.md defines a bounded causal witness experiment with explicit uncorrelated/unsupported exit; no global-last-matrix association, arbitrary memory scan, production change or runtime integration.

#146 2026-09-07 fa5f74b2c plus working tree | FC-067 opt-in bounded source-material sidecar, native format/mip/palette readback, generation/SRV matching, full-byte deduplication, inspector and texture contact sheet | four final working builds pass; enabled selftests 243/243 each, SDK mock 56/56. WARP material fixture passes 145 raw/RGBA/alpha comparisons, grouped-locale hash regression and 14 negative controls. Three-frame on/off native color exact 3/3; 30-frame captures close cleanly and all nine captured PNG planes match pixel-for-pixel 30/30. All 30 material packages independently verify: 39 assets, 7,155,710 raw bytes/frame, 6,678-6,698 slot bindings; first-frame sheet has 40 texture/palette views. First capture A is rejected for locale-grouped hashes despite launcher success; fixed hashing and fresh B/moving captures verify. Initial Windows macro/ComPtr build failures and inspector patch mismatch retained in MATERIAL-CAPTURE-AUDIT.md. CLI wrong-API/frame bounds reject before launch. Source config hash unchanged | ACCEPTED source-material extraction only (self-review), not physical albedo, per-draw mutable resource history, world camera, Remix GPU or full M2 closure. No private binary/config changes; assets/captures/media remain outside Git. Next review material artifacts then the approved camera source-witness track; no precision or framebuffer-light loop.

#145 2026-09-07 20e0c11a5 | user approves camera/material investigation; inspect TA vertex intake, DX11 texture upload/cache, pixel-shader material combination and existing same-frame replay resource checks; inventory 30 retained Hoko Temple packets | separate source textures and vertex/offset colors are available before final shading; all 100,228 textured draw records use ShadInstr 3. Observed 52 first-texture state keys, 2,548 paletted records, zero nonzero-RTT-generation textured records. Camera matrices/world transforms are not supplied by ordinary Dreamcast vertex intake. These are source/packet findings, not extracted assets or recovered camera proof. CAMERA-MATERIAL-PLAN.md scopes bounded texture/palette sidecar implementation and a later provenance-first camera investigation | ACCEPTED source-audit and next-task scope only. No production source/configuration changed, no new game run or GPU extraction claimed. No framebuffer-as-material tuning or precision-probe restart. The ignored fc067-material-inventory.ps1 checks frame/game/SHA and reproduces the inventory. Full reconstruction and runtime proof remain pending.

#144 2026-09-07 f57bbe9c8 plus working harness | FC-067 offline camera-relative preview using captured indexed PVR geometry, assumed projection, face normals, one SDR light multiplier and conservative crop/coverage; package moving native/preview comparisons | four builds exit 0, enabled selftests 243/243 each and SDK mock 56/56. Working E processes all 30 source-hash/identity-matched Hoko Temple frames; light-off exact and protected mismatch zero throughout. Wrong-frame manifest/PNG, existing-output and frame bound reject with expected exit 1. Non-inert depth/projection/normal mutations and analytic negative controls pass. Failed A/C/D and three-frame B retained in REMAKE-PREVIEW-AUDIT.md; A overprotects, C/D incorrectly hash source layout, E corrects to actual production RGBA raw contract. Native/preview samples expose faceting and over-brightening rather than convincing new materials; 60-Hz-average and slow GIFs are retained outside Git | ACCEPTED visualization only; NO-GO for promotion. Deliver comparison, stop approximation branch, no new tuning loop. Full FC-067 reconstruction/GPU and strict source equality remain incomplete. Production renderer/defaults and all external configuration unchanged; source config hash unchanged. No capture/media/dependency/user path staged.

#143 2026-09-07 9fafcb873 | FC-067 user-approved end to precision-diagnostic loop; preserve/remove temporary single-draw probe, audit current packet against actual Remix adapter requirements, document limited direct-handoff no-go and next bounded visible approximation; rebuild restored source and restage tester executable | all four configurations configure/build serially with exit 0; automation, baseline NGX and no-NGX selftests each pass 243/243; SDK contract passes 56/56 with runtime_loaded=false/gpu_rendered=false/presented=false. Built/staged executable hashes match `7CD378B462D10D61A62505D95150F7095F5CC3DEFB69302443B249B6521DA7D6`. Source emu.cfg remains `1EF718689784DCE64CAD1CE8BEC776E710B4A3705ECFF2E5A276A1A3B2F92992`. No new game/GPU capture or performance test was run for this documentation-only pivot. Prior single-draw A-G attempts and invalid C UV diagnostic are retained in REMAKE-DISPOSITION.md; no root cause inferred. An initial AGENTS patch failed context matching without applying and was corrected | ACCEPTED limited feasibility/scheduling disposition only. M2 strict source equality remains CORRECTIONS_REQUIRED/parked, FC-067 and the active goal remain incomplete. Current packet lacks known camera/world/normals/material completeness; current adapter lacks GPU execution/completion/readback. No-go applies to direct current-input/current-adapter handoff, not all future Remix use. Next produce one explicitly labeled moving camera-relative approximation or an input-specific no-go. No production code, gate tolerance, default, external configuration, third-party binary or media changed. Build/test logs use ignored `fc067-pivot-*` paths.

#142 2026-09-07 15b3c6eb9 plus working harness | FC-067 durable repeat-raster GPU fixture, production native VS/PS source and runtime shader-model-4/flags-zero compilation, eight D24/D32 opaque/blended untextured/textured lanes, new/reused targets, all artifacts and wrong controls | four working builds pass; native D3D11 and D3D11On12 each pass 512/512 exact repeats. Wrong viewport changes at least 6493 color and 8272 depth samples; wrong depth changes 8190 depth samples. Existing-output control exits 1; invalid API exits 2. Existing depth-contract and motion-contract exit 0. Earlier shader-model-5/optimized untextured and textured iterations pass but are retained as superseded compiler contracts, not runtime-equivalent evidence. Final working output directories fc067-repeat-final-native/on12 | ACCEPTED synthetic fixture only; does not reproduce the game's residual. M2 remains CORRECTIONS_REQUIRED. Next capture the actual failing draw's pre-target surfaces, geometry and shader variant for a same-command fixture. No production source, consumer configuration, gate tolerance or default changed; synchronous output is not performance evidence.

#141 2026-09-07 a62fb12c2 plus temporary probe | FC-067 per-draw pixel timeline and native/original-buffer state readbacks | six automation probe builds pass. Three-frame capture A/D/E/F exit 1 with 13 one-step color mismatches; B/C exit 0. First selected-pixel divergence is sorted DrawIndexed trace command 53, first 17497, count 18. F matches 30 state dumps but differs at 1178 D24 samples by one integer step, zero stencil differences; depth at the selected color pixel matches. All texture mips, constants, samplers and recorded pipeline/geometry/shader-object state match in that failing run. Initial texture/depth format omissions in C/E are retained and not interpreted as equality. Incremental binaries retain an older generated SHA stamp, so these are explicitly working-tree diagnostics, not exact-SHA acceptance. Probe removed and its patch retained locally; source config hash unchanged | ACCEPTED causal localization only; M2 remains CORRECTIONS_REQUIRED. Next isolate repeated production-shader depth/blend output and target allocation behavior with controlled fixtures. No root cause, production fix, driver precision explanation, or relaxed gate claimed. See REMAKE-REPLAY-AUDIT.md.

#140 2026-09-07 688d63e05 | FC-067 M2 native-only repetition and early/late framebuffer causal controls, using existing capture hooks without source changes | two native-only 30-frame captures exit 0; A matches earlier off run 30/30, A/B only 29/30 (frame 1805: three pixels, max delta one). Three-frame early/late paired run exits 0; 30-frame repeat exits 1 with early/late native exact 30/30, decoded/retained replay exact 30/30, source/replay exact 29/30 (frame 1804: 13 pixels, max delta one). Both wrong controls still fail materially. Initial PathInfo-to-Bitmap diagnostic fails and its resulting counts are rejected; corrected Stop-on-error comparison exits 0. Source configuration hash unchanged, disposable capture settings removed, external config untouched | ACCEPTED causal narrowing only; CORRECTIONS_REQUIRED M2. Replay is not required for the observed cross-run variation; late framebuffer mutation and early-readback-as-fix are falsified in the failing repeat. Remaining draw inputs/state or repeat-render behavior must be localized without lowering exactness. See REMAKE-REPLAY-AUDIT.md; no performance, recovered camera, Remix rendering, or combined DLSS 5 claim.

#139 2026-09-07 857d63963 plus working tree | FC-067 M2 isolated decoded GPU replay, retained original-buffer control, deliberately wrong viewport/depth, prior-frame preservation, resource-state checks and scoped restoration | four builds pass, three selftests 243/243, SDK mock 56/56. Three-frame failures retained: 13 one-step source pixel differences recur despite an intervening 3/3 exact run; original-buffer replay duplicates the residual and decoded/control images match exactly. Final working moving replay passes 30/30 source/control exactness and wrong controls; matching on/off native comparison is only 28/30 (3 and 11 one-step differing pixels with exact captured geometry). Initial locale-corrupted proof JSON corrected, no tolerance lowered. Full details and failed hypotheses in REMAKE-REPLAY-AUDIT.md | accepted diagnostic harness only; CORRECTIONS_REQUIRED for M2/goal completion. Next: native-state/repeat-render residual diagnosis and exact-SHA moving pairs. No external configuration or third-party binary changed; source config hash unchanged.

#138 2026-09-07 b20803d46 plus working tree | FC-067 M2 bounded disk decoding with exact vertex-bit roundtrip, parser/file/array limits, unresolved texture state, mandatory omissions, and fail-closed identity/provenance | four builds exit 0; three enabled selftests 243/243 (initial decoder suite 239/239 then four further controls); actual Soulcalibur packets 1804-1806 decode. Wrong-frame CLI exits 1 at identity; old working-03 packet exits 1 at omissions, not palette, with explicit synthetic RGB palette rejection tested separately. No production renderer call, external settings change, or GPU replay is introduced | self-reviewed ACCEPTED for decoder only. Next is resource resolution, remaining captured native state, isolated decoded raster replay, and wrong-viewport/depth controls. Goal/M2 remain incomplete.

#137 2026-09-07 3a5051ec9 plus M2 working tree | cross-run snapshot comparison falsifies palette metadata for nonpaletted textures: vertices/indices/passes/viewport exact but draw records differed because palette_hash is only assigned for paletted formats. Export null for ordinary RGB instead of reading indeterminate cache storage | four corrected builds exit 0, three selftests 225/225; fresh isolated captures working-04 and working-05 each clean-close and validate, with whole packet JSON byte-identical 3/3 and native PNGs identical to packet-off 3/3 | deterministic projected snapshot is proven for this interval; earlier palette metadata rejected. This remains export/noninterference, not decoded native replay or world reconstruction.

#136 2026-09-07 3a5051ec9 plus working tree | FC-067 M2 opt-in bounded PVR snapshot beside quality frames, exact float bits/state/texture generations with unknown camera provenance; add diagnostic inspector and eight selftests | four builds pass; enabled selftests 225/225 each; SDK mock 56/56. Three actual Soulcalibur Hoko Temple packets validate at frames 1804-1806, 15933/15933/15936 vertices, 3339/3339/3340 draws, 495 nonfinite but entirely unreferenced vertices each. Packet-on/off native PNGs byte-identical 3/3, disabled writes zero packets, clean closes and unchanged source configuration hash. Initial blanket nonfinite rejection capture exited 1 after deliberate normal close; unsigned-index inspection error, Vertex namespace compile error, and premature inspector invocation are retained and corrected as detailed in REMAKE-M2-AUDIT.md | ACCEPTED only as self-reviewed export/noninterference slice. Disk-decoded replay, wrong-camera/depth controls, world reconstruction and GPU Remix remain pending. No third-party configuration/binary or media staged.

#135 2026-09-07 bde1b460d plus working tree | self-review corrects late degenerate-strip rejection by preflighting all topology before API resource creation | serial full builds of automation, baseline NGX, no-NGX, and feature-off exit 0; three enabled selftests each 217/217; actual public-header/mock target 56/56 including zero API calls for a degenerate later mesh. Logs: fc067-review-full-build.log and fc067-review-selftest.log in build directories; focused SDK build fc067-review-build.log. These are working-tree checks, not exact-new-commit or GPU evidence. One documentation patch failed context verification and made no edits; corrected patch applied | ACCEPTED only for bounded correction by self-review. Native replay, real game packet export, camera recovery, and runtime GPU rendering remain pending. No external configuration, production renderer, or proprietary binary changed.

#134 2026-09-07 81057acbd plus M1 working tree | exercise optional SDK target configuration with a missing public header and an intentionally different header, then restore the reviewed header path | missing-header configuration exits 1 with required-header diagnostic; wrong-header configuration exits 1 with reviewed-revision mismatch; restored configuration exits 0. Full logs remain in fc067-sdk-missing-negative.log, fc067-sdk-pin-negative.log, and fc067-sdk-restored-configure.log under the automation build | negative controls prove the target cannot silently compile against an absent or unreviewed API; no runtime loaded or external configuration changed. This is build-input validation, not rendering evidence.

#133 2026-09-07 81057acbd plus M1 working tree | implement a bounded versioned harness-only scene packet, explicit unknown-data/omission rejection, analytic projection/overlap/camera/strip fixtures, and an off-by-default adapter contract target against public Remix header revision e876135b37295dc203ccdb7b20a8089629588201; inspect source licenses and build dependencies without running vendor fetch scripts | all four sequential builds pass; automation/NGX/no-NGX selftests each pass 217/217, including 45 new CPU checks. The real-header/mock-call executable passes 55/55 with runtime_loaded=false, gpu_rendered=false, presented=false. Creation/submission failure controls prove scoped CPU resource cleanup; no failed build/test attempt preceded these final results (initial iteration passed 210/210 and 48/48 before expanded checks). Planning-commit regression logs also retain four successful build commands and 172/172 enabled selftests | ACCEPTED for contract/adapter slice by implementor self-review only; independent review, synthetic GPU, game-scene reconstruction, and Remix/DLSS 5 presentation remain unproven. No configured Remix runtime or meson command is available to this harness, and full runtime build dependencies require separate review/provisioning. No production source, active consumer configuration, binary, or game asset changed. See REMAKE-M1-AUDIT.md; review before M2 bounded Soulcalibur packet/native alignment. Raw logs and reviewed vendor header/notices stay ignored.

#132 2026-09-07 49c96a098 plus planning tree | user authorizes reuse-first remake feasibility and lighter-model implementation; add AGENTS entry point and REMAKE-FEASIBILITY-PLAN, assign FC-067 M1, update course correction, quality priority, backlog, decisions, and tester handoff | public Remix SDK documentation and PCSX2 RemixConfig source establish candidate APIs/reference structures only; no adapter build, GPU render, scene reconstruction, or new gameplay test was run for this docs-only change. Existing 49c96a098 four-build and 172/172 selftest evidence remains separate | begin bounded developer-only scene packet/analytic controls/adapter disposition with GPT-6 Astra/low (Astra light, explicitly corrected by the user from the proposed Sol/high), then review before M2. Preserve the proven neural route; no active configuration, runtime, assets, or production source changed. Model handoff success must be reported from the actual app operation, not inferred from this document.

#131 2026-09-07 b20909f6a plus working tree | add an explicit external-consumer intensity panel under DLSS 5 Video settings, percentage conversion, a hidden asynchronous companion launcher with a ten-second bound, locally persisted companion/config paths, and pending-only Uncanny selection; build automation, NGX, no-NGX, and feature-off serially; run all three enabled selftests and exercise the production launcher against the installed companion using a disposable INI | all four builds link and all three selftests pass 172/172. The real companion writes overall/structure 2.00 and global/local tone 0.75 with Cinematic style, automatic mask off, and UI correction on; its backup matches the original fixture SHA-256 byte for byte. Existing unrelated keys, hook policy, and preset remain intact. A fixture path containing spaces and ampersand succeeds without a shell; a missing companion is rejected. Unit tests reject out-of-range percentages, unsupported style, and invalid paths. No active consumer configuration was changed. No failed build/test attempts occurred in this slice; missing-helper rejection is an intentional negative control | FC-050 gains user-triggered settings control, not active-consumer or presentation proof. The UI labels values as pending rather than read from the consumer and requires restart after Apply. Manual click/layout, restart uptake, and timeout/partial-write recovery remain tester coverage; backups and requested-config success cannot replace consumer-reported tuple verification. Disposable fixtures, helper binaries, and build logs remain ignored and outside the staged changes.

#1 2026-09-02 12bb436 | `cmake -S . -B build-neural-baseline -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=artifact-neural-baseline -DENABLE_CTEST=ON -DUSE_DISCORD=ON` | MSVC 19.44.35228, SDK 10.0.26100.0; configure succeeded | baseline configured

#2 2026-09-02 12bb436 | `cmake --build build-neural-baseline --config Release --target install --parallel` | stopped at 1008/1099; missing `d3dx9shader.h`; June 2010 DirectX SDK absent | failed, retained

#3 2026-09-02 12bb436 | reconfigure with `-DUSE_DX9=OFF -DENABLE_CTEST=ON`, build | stopped at 661/707; `tests/src/HttpTest.cpp` missing `curl/curl.h` on Windows | failed, retained

#4 2026-09-02 12bb436 | reconfigure with `-DUSE_DX9=OFF -DENABLE_CTEST=OFF -DBUILD_TESTING=OFF`, then build/install | `flycast.exe` linked and installed; exit 0 | baseline Windows DX11 build pass

Toolchain: Windows 11 10.0.26220; CMake 4.4.3; Ninja 1.13.2; Visual Studio
2022 Build Tools 17.14.37516.0; MSVC 19.44.35228 / 14.44.35207; Windows SDK
10.0.26100.0.

#5 2026-09-02 working tree | configure/build `FLYCAST_NEURAL=ON`, `FLYCAST_NEURAL_NGX=OFF`, `FLYCAST_NEURALTEST=ON` | `flycast.exe`, `flycast-neural.lib`, and `neuraltest.exe` linked; exit 0 | instrumentation configuration pass

#6 2026-09-02 working tree | configure/build with `FLYCAST_NEURAL_NGX=ON`, SDK `C:/Game Dev/Emulators/NVIDIA-DLSS-v310.7.0` | dynamic-CRT release/debug imports validated; full build exit 0 | NGX configuration pass

#7 2026-09-02 working tree | configure/build `FLYCAST_NEURAL=OFF`, `FLYCAST_NEURAL_NGX=OFF` | full `flycast.exe` link exit 0; no neural target in graph | feature-off configuration pass

#8 2026-09-02 working tree | configure NGX with `C:/definitely-missing-sdk` | generation stopped with the required `include/nvsdk_ngx.h` path diagnostic | negative configuration pass

#9 2026-09-02 working tree | `build-neural-baseline/neuraltest/neuraltest.exe --version` | `neuraltest phase-0`, exit 0 | harness skeleton pass

#10 2026-09-02 e5c88da | MSVC x64 configure and `cmake --build build-neural-baseline --target neuraltest -j 8`, neural ON / NGX OFF | five harness translation units and `flycast-neural.lib` linked; exit 0 | Phase 1 target build pass

#11 2026-09-02 working tree | `render`, 5-run `determinism`, and `scaling` smoke on `static-triangle` / `textured-checker-edge` | hash `f38656d535ada799`; 5/5 exact; 4x 204915 and 8x 850539 pixels differ from nearest, max delta 249 | test-only D3D11 driver pass, not production renderer evidence

#12 2026-09-02 working tree | 14 fixtures x `dx11`,`dx11-oit`: 5-run `determinism` and 1x/4x/8x `scaling` | 28 deterministic and 28 scaling commands passed; axis-aligned non-gate scenes reported informational zero differences | Phase 1 command matrix pass within test-driver boundary

#13 2026-09-02 working tree | render all 14 fixtures in both requested lanes; passthrough; exact compare; wrong-history negative control | 28 packages; passthrough max delta 0; wrong-history 32325 differing pixels, max delta 248, PSNR 9.935606, expected exit 1 | artifact/threshold controls pass

#14 2026-09-02 working tree | `depth`, `motion`, no-NGX `neural --backend dlaa`, and `capture` probes | depth/motion report no data with exit 0; DLAA reports unsupported with exit 0; capture exits 3 | missing production instrumentation and FC-054 are explicit, no false pass

#15 2026-09-02 working tree | full MSVC x64 builds with neural ON/NGX OFF, neural ON/NGX ON at SDK v310.7.0, then neural OFF | `flycast.exe` linked in all three configurations; `neuraltest.exe` linked in both enabled configurations | configuration matrix remains green

#16 2026-09-02 working tree | WARP `render --fixture rotate-quad --scale 4` and 5-run determinism | Microsoft Basic Render Driver; 5/5 exact at 1x; JSON manifest parsed successfully | WARP test-driver path pass, not FC-049 export evidence

#17 2026-09-02 0caaeeb | build `motion_reference.cpp` and run `neuraltest selftest` | 17/17 checks pass: signature, tiers 1-3, one-to-one, reactive/unmatched, rigid fit, history, Halton, phase count, scene cut | CPU reference subset pass; strip, Naomi 2, HLSL, full recovery, and renderer wiring remain open

#18 2026-09-02 f271894 | build recovery/stage changes and run `neuraltest selftest` | 23/23 checks pass; three failures at frames 1/30/60 enter one hold, 60 presents alone cannot exit before 1000 ms, resume emits one reset, repeated frame submits once | fallback-hold and emulated-frame cadence unit subset pass; NGX retry/device/timing remain open

#19 2026-09-02 working tree | compile guarded production `rend_context` instrumentation seam | first compile lacked `nowide` includes, second lacked `glm`, third found non-const Flycast `ComPtr::get`; dependencies and accessor qualification corrected without bypassing diagnostics | failed attempts retained; no acceptance claim

#20 2026-09-02 working tree | MSVC x64 full `flycast` + `neuraltest` build, neural ON / NGX OFF; `neuraltest selftest` | full link exit 0; 26/26 checks pass including a real `rend_context` metadata snapshot, first-frame reset, repeated-frame tier-1 match, deterministic hash, and atomic overflow state | production DX11/OIT metadata/cadence seam build pass; MRT exports and production pixel gates remain open

#21 2026-09-02 working tree | rebuild guarded config/settings/runtime-mode changes | `flycast.exe` and `neuraltest.exe` linked; renderer requirement text and unsupported native-fallback note compiled | UI subset build pass; live capability, metrics, and debug-view controls remain open

#22 2026-09-02 working tree | incremental build first outside, then inside `VsDevCmd.bat -arch=x64 -host_arch=x64`; `neuraltest selftest` | outside-developer-shell compile failed at standard headers and the stale 25-check binary was disregarded; proper MSVC build linked both targets and current binary passed 26/26 | corrected invocation pass; failed launch retained and is not test evidence

#23 2026-09-02 working tree | fresh Ninja Release configure/build with `FLYCAST_NEURAL=OFF`, NGX/test off, DX9/tests off | 1090 steps; `flycast.exe` linked exit 0; no `flycast-neural` target in generated graph | feature-off build remains green after guarded production seam

#24 2026-09-02 working tree | Ninja Release configure/build with neural ON, NGX ON, SDK v310.7.0; run `neuraltest selftest` | `flycast.exe` and `neuraltest.exe` linked exit 0; 26/26 checks pass | NGX-linked configuration remains green; no live NGX lifecycle/evaluation exists yet

#25 2026-09-02 cfe2285 | build production three-slot R32 depth-export replay with neural/NGX ON; rebuild fresh feature-off tree; run selftest | both `flycast.exe` builds linked exit 0; 26/26 checks pass; export issues only OP/PT lists and unbinds OIT UAVs before replay by construction | depth resource/ownership build pass for DX11 and OIT; no runtime pixels captured, so Gate 3 remains open

#26 2026-09-02 665bfd4 | build last-successful draw-history ownership and FramebufferDirect atomic-package wiring with neural/NGX ON and feature OFF; run `neuraltest selftest` | both `flycast.exe` links exit 0; 30/30 checks pass; rejected synthetic frame does not replace reference, later matching returns to accepted frame, source transitions increment generation twice | FC-030 pass; FramebufferDirect/RTT runtime cadence evidence and remaining reset call sites stay open

#27 2026-09-02 780f5a0 | build CPU temporal/reference additions and run `neuraltest selftest` | `flycast.exe` and `neuraltest.exe` linked exit 0; 37/37 checks pass: changed-count strip retains >=90% overlap, N2 column-major projection exact, unmatched/oversize vectors zero and bias current, 4:3 content rect 1440x1080 at x=240 vs 1920x1080 widescreen, both depth inverses within 1e-3 | CPU/unit subset pass; real vertex correspondence and HLSL exports remain open

#28 2026-09-02 ed2baff | first source-extracted production HLSL contract test | export permutation failed at compile because the shared modifier-volume function referenced native-only `PSO.col`; corrected with an explicit export branch | failed attempt retained; no gate claim

#29 2026-09-02 ed2baff | build fixed three-slot DX11 export package with neural/NGX ON, compile native/export production HLSL from source, run `neuraltest selftest`, and rebuild feature OFF | enabled and disabled `flycast.exe` links exit 0; current selftest 39/39; package owns RGBA8 color, R32 depth, RG16F motion, R8 bias/confidence, and R16_UINT draw-ID resources | resource/atomic-contract and shader-compile pass only; no runtime game capture, OP/PT draw IDs only, and motion is intentionally zero with mask 1, so Gates 3-6 remain open

#30 2026-09-02 2bd9bf9 | first NGX backend compile | failed on an anonymous-namespace boundary, exact SDK engine enumerator spelling, and const access through Flycast's custom `ComPtr`; all three were corrected directly | failed attempt retained; no runtime or build claim

#31 2026-09-02 2bd9bf9 | build public NGX D3D11 backend in SDK-enabled tree; fresh configure/build instrumentation-only tree; run both selftests | NGX-enabled and NGX-disabled `flycast.exe`/`neuraltest.exe` linked; 41/41 checks pass in both binaries | lifecycle/resource/fallback structure build pass; live NGX create/evaluate remains unrun pending harness GPU texture wiring

#32 2026-09-02 cdefd7f | first live-harness compile | MSVC rejected `std::max(1u, frames)` after the Windows `max` macro expanded; replaced it with an explicit zero-frame normalization and rebuilt both targets | failed attempt retained; no runtime claim

#33 2026-09-02 working tree | first RTX 5090 live D3D11 DLAA probes | initial directory-level input failed because `neural` requires a frame package; corrected input reached NGX but reported availability 0 because the code used the wrong project GUID and `PATH` did not populate NGX's feature search list | failed attempts retained; exact specification GUID, `GIT_VERSION`, and documented external feature path then applied

#34 2026-09-02 working tree | live `neural --api d3d11 --backend dlaa --frames 240` on `camera-translate`, `particles`, and `textured-checker-edge` using SDK v310.7.0 release runtime | RTX 5090: each run submitted 240/240, result 1 (`Success`), exception 0, busy/fallback 0, invalid frames 0; final hashes `cbbbf5ea47d3eb30`, `bf648c0497c22f21`, `1b68d2f0356be50c`; per-frame readback is harness-only | public D3D11 DLAA lifecycle/cadence subset pass; inputs are deterministic harness frames, not production PVR captures

#35 2026-09-02 working tree | D3D11 unsupported matrix on the same harness | WARP returned init `0xBAD00001`; missing feature runtime returned availability 0 / feature-init `0xBAD00004`; `--no-ngx` returned explicit disabled reason; all exited cleanly without exception | missing-runtime, WARP/non-NVIDIA, and explicit-disable failure subset pass; injected failures remain pending

#36 2026-09-02 working tree | rebuild SDK-enabled `flycast`/`neuraltest`, instrumentation-only `flycast`/`neuraltest`, and feature-off `flycast`; run both enabled selftests | all targets linked; SDK and no-SDK harnesses each passed 41/41 | three-configuration build matrix remains green after live-runner integration

#37 2026-09-02 1b7120f | add public optimal-settings query and explicit SR output/mode arguments | NGX returned 1280x960 Quality input for 1920x1440 output and accepted it; deliberate 320x240 mismatch returned a precise unsupported reason | real-upscale dimension contract pass; no 1:1 SR claim

#38 2026-09-02 working tree | D3D11 SR Quality 1280x960 -> 1920x1440 and Performance 1280x960 -> 2560x1920, 240 frames each, on `camera-translate`, `particles`, `textured-checker-edge` | all six runs submitted 240/240 with result 1, zero exceptions, busy skips, fallbacks, and invalid frames; final hashes Quality `0820ac65892d2064`,`2c7143441a2eab4d`,`4c7a8579e94196ec`, Performance `8fab374f23153673`,`43939767f31a17c3`,`7b8e8287d018b3d1` | D3D11 public SR live matrix pass on static harness inputs; production temporal inputs remain pending

#39 2026-09-02 working tree | compare Performance output against direct 8x reference for the same frame | camera: 33718 differing pixels, max 193, PSNR 33.681036; particles: 11437 / 249 / 36.994271; checker: 379356 / 242 / 42.291854 | measurements recorded without an acceptance threshold; DLAA/reference downsample comparison remains pending

#40 2026-09-02 working tree | per-frame static-output metric on 240-frame camera Quality run | 239 hash changes; worst adjacent frame affected 1195/2764800 pixels, max delta 41, PSNR 69.0399 dB; zero black frames | static convergence/flicker measured, not declared threshold-green

#41 2026-09-02 working tree | correct create flags to the exact DLAA/SR contract and rerun 240-frame checker DLAA | DLAA flags 0 submitted 240/240 with zero invalid/busy/fallback/exception; 238 adjacent hash changes, worst 506/76800 pixels, delta 27, PSNR 60.0902 dB | DLAA no longer incorrectly advertises low-resolution motion; `MVLowRes` is SR-only

#42 2026-09-02 6388f36 | first D3D12 backend compile | SDK v310.7.0's D3D12 create helper required explicit creation/visibility node masks unlike D3D11; corrected both to node mask 1 and rebuilt | failed attempt retained; no runtime claim

#43 2026-09-02 working tree | first public D3D12 DLAA evaluation using dedicated allocator/list/output/fence slot | RTX 5090 submitted 1/1, result 1, exception 0, valid output hash `837c174685cf0994`; same one-frame D3D11 output matched exactly | native D3D12 backend live smoke pass

#44 2026-09-02 working tree | D3D12 DLAA and zero-jitter DLAA-hook on `camera-translate`, `particles`, `textured-checker-edge`, 240 frames each | all six runs submitted 240/240, zero invalid/busy/fallback/exception; final hashes matched the D3D11 results | public standard evaluate-shape hook subset pass; no third-party module was loaded or inspected

#45 2026-09-02 working tree | D3D12 Quality/Performance SR on the same three fixtures, then 12 cross-API final-image comparisons | six SR runs submitted 240/240 with zero invalid frames; DLAA, hook, Quality, and Performance D3D11/D3D12 pairs all had zero differing pixels and max delta 0 | FC-046 harness cross-API parity pass; static synthetic inputs only

#46 2026-09-02 working tree | WARP and `--no-ngx` texture/export probes on D3D11 and D3D12; no-SDK build D3D12 repeat | both APIs allocated the complete GPU input set; D3D12 additionally created/cleared/released a same-queue D3D11On12 wrapped target; all reported explicit unsupported without SDK invocation | FC-049 pass within harness boundary

#47 2026-09-02 working tree | rebuild SDK, instrumentation-only, and feature-off configurations; run both enabled selftests | all three `flycast` targets and both harnesses linked; each selftest passed 41/41 | configuration matrix green after D3D12 backend and harness additions

#48 2026-09-02 working tree | first compile of accepted-output presentation wiring | MSVC rejected a raw `ID3D11ShaderResourceView*` at Flycast's `Quad::draw`, which requires an owning `ComPtr` lvalue | failed attempt retained; output ownership was corrected instead of weakening the quad API

#49 2026-09-02 working tree | rebuild SDK, instrumentation-only, and feature-off `flycast`; run both enabled selftests | all three `flycast.exe` targets linked; SDK and no-SDK selftests each passed 41/41 | D3D11 submitted output is now retained through the final content-rect blit before OSD, framebuffer-direct submission follows the same accepted-output rule, and global render reset/save-state deserialize notify neural history; runtime game evidence remains unavailable

#50 2026-09-02 working tree | implement route-neutral experimental consumer mode; build SDK, instrumentation-only, and feature-off `flycast`; run SDK/no-SDK selftests plus component-absent D3D11/D3D12 and no-NGX controls | all three `flycast.exe` targets linked; SDK and no-SDK selftests passed 45/45; component-absent public NGX submitted 5/5 on each API while reporting `missing-components`, zero rebuilds, and no DLSS 5 confirmation; no-NGX returned clean unsupported with the selected D3D11 route retained | M0 readiness/rebuild policy pass: no forced D3D12, configurable 300-evaluation default, transition-triggered idempotent release, two-attempt default bound, and reason telemetry; public contract output only, so Gate 10 remains entirely open

#51 2026-09-02 2e6995c4b plus working tree | audit Feeder v0.10.0-beta.2, then run `Soulcalibur (USA).chd` through corrected `config:rend.NeuralMode=8` native D3D11 staging with ReShade 6.8.0.2155, supplied RenoDX DLSS 5 v4.7 add-on, public DLSS 310.8.0.0, and signed NR SHA-256 `6EB209E764F39872625DEBD6ABAF45E2BB6322F6F270F781F70C059AE30B3927` | Flycast evaluated the genuine public D3D11 contract and reported `d3d11-external-unclassified` / `contract-evaluated`; the add-on loaded but installed D3D12 NGX hooks only and recorded no intercepted D3D11 evaluation or feature-18 create/evaluate; two earlier staging runs used the wrong transient-key namespace and are explicitly invalid route evidence | M1A blocked because available Feeder constructs an image-derived ReShade contract and standalone NIGos is absent; M1B blocked for this exact supplied add-on, not for all possible D3D11 consumers

#52 2026-09-02 working tree | complete the conditional production D3D11On12 device/queue, wrapped neural input/output resources, and queue-created swapchain; run Soulcalibur no-host controls | the first single-wrapped-backbuffer draft lost the device on first Present with D3D12 reason `0x887A002B`; replacing it with a two-entry wrapped RTV ring selected by `GetCurrentBackBufferIndex` ran stably, shut down cleanly, and reported route `d3d11on12`; no-host readiness remained `missing-components` | production On12 ownership/synchronization subset pass; failed single-buffer attempt retained, and full resize/fullscreen/device-loss/OIT/OSD/ImGui/timing matrix remains open

#53 2026-09-02 working tree | run the corrected On12 route with the supplied Soulcalibur CHD, ReShade/add-on/runtime set, and frame-tagged public-output presentation telemetry; repeat add-on-off and no-NGX controls; rerun the final binary in fresh `m1c-final-positive-20260902`, `m1c-final-fallback-20260902`, and `m1c-final-no-ngx-20260902` stages | external log positively recorded first Flycast D3D12 NGX evaluation, lazy adoption of the public feature handle, signed DLSSNR 310.8 initialization, feature 18 create, and successful 640x480 feature-18 evaluations through count 60; Flycast frame 1 reported candidate public-output wrap, accepted-output blit selection, and successful Present; the final add-on-off run stayed stable for 12 seconds with `missing-components`, retained native presentation, emitted no candidate-output line, and exited cleanly, while the final no-NGX run stayed stable for 10 seconds with `disabled`/unsupported, no accepted output, and a clean exit | Gate 10 route identity/contract observation/feature create-evaluate/cadence subset is positive, but exact external-output resource identity, sentinel/pixel differentiation, F6 A/B, and latency are not proven; DLSS 5 is not yet ready

#54 2026-09-02 working tree | add an opt-in D3D12 input/returned-output readback and public-output marker, then iterate Soulcalibur evidence timing and bias controls | first-frame input and returned hashes were identical (`742A0703FA4DA325`) both before and after forcing the diagnostic bias mask to zero; moving the capture target to evaluation 60 did not produce a capture in that run and was reverted; ordinary Windows-capture images were rejected as A/B evidence because the save-data scene animates | failed/ambiguous attempts retained; exact-equality falsifies any visual-mutation claim for the captured frame and motivated a direct swapchain sentinel check

#55 2026-09-02 working tree | run `m3-sentinel-positive-20260902` with the supplied add-on/runtime, `m3-sentinel-no-addon-20260902` without the add-on, and `m3-sentinel-no-ngx-20260902` from the no-NGX build; attempt F6 controls including a 5000 ms evidence-arm delay | positive frame 1 logged input and pre-marker returned FNV-64 `742A0703FA4DA325`, marked hash `67B941B19BB5C325`, 24022 us synchronous diagnostic wait, 1024/1024 marker pixels in the 640x480 swapchain backbuffer, then successful Present; ReShade logged feature-18 create/evaluate through count 60; the no-add-on run produced the same internal hashes but no candidate-output, swapchain-evidence, or marker presentation and exited cleanly; no-NGX stayed `disabled` with zero candidate/present-evidence lines and exited cleanly; injected F6 attempts did not prevent feature-18 creation/evaluation and therefore are not valid OFF evidence | public-output GPU round trip and native-fallback isolation pass; external feature-18 output identity, neural pixel change, F6 A/B, and production latency remain open, so Gate 10 and DLSS 5 readiness remain blocked

#56 2026-09-02 working tree | build the evidence change in NGX, no-NGX, and feature-off configurations through `VsDevCmd.bat -arch=x64 -host_arch=x64`; run both current selftests | all three `flycast.exe` targets linked and NGX/no-NGX selftests passed 45/45; an initial build outside the developer environment failed only on missing MSVC standard headers and was superseded by the recorded developer-shell build | configuration matrix pass; evidence mode remains compiled out with the neural feature and remains dormant by default in neural builds

#57 2026-09-02 working tree | replace zero-as-empty presentation bookkeeping with an explicit pending flag, rebuild all three configurations, rerun both 45-check selftests, then run `m3-sentinel-frame0-delayed-proof-20260902`, `m3-sentinel-frame0-no-addon-final-20260902`, and `m3-sentinel-frame0-no-ngx-final-20260902` against the supplied Soulcalibur CHD | the delayed positive run preserved frame 0 through capture, accepted-output readiness, blit, 1024/1024 swapchain marker verification, and successful Present; hashes were input/returned `F318E0E2F9B92325` and marked `35EBAAE2FFBCA325`, with a 26337 us synchronous diagnostic wait; the supplied add-on created/evaluated feature 18, while the refreshed no-add-on and no-NGX controls produced zero candidate-output and zero present-evidence lines | frame-0 identity bug fixed and public-output routing proof remains positive; identical pre-marker input/returned pixels still block a neural-mutation claim, F6 injection remains invalid OFF evidence, and diagnostic wait time is not production latency

#58 2026-09-02 f78bdda9e | run a visible-window F6 attempt with ReShade forced modifiers disabled, then run an explicit `EnableHooks=0` control at a 10000 ms evidence-arm delay | the F6 run produced no external disabled-state log and is not accepted as OFF evidence; its frame-0 input `F2769AE566DAC75B` and returned output `CD655CA230C9CFC7` differed, but there was no identical-input OFF pair; the policy control logged `SAFE MODE: EnableHooks=0, all hooks off (no NR)`, Flycast reported `missing-components`, and no candidate-output or present-evidence line was emitted; that control's input `3845A7D9FC90C747` differed from the F6 run | explicit external-host-policy-off fallback passes, but the differing source frames prohibit ON/OFF attribution; exact matched-input A/B or an external returned-resource identity signal is still required

#59 2026-09-02 working tree | add bounded full-contract evidence capture, run 120-frame `m3-full-contract-on-20260902` and explicit-policy-OFF `m3-full-contract-off-20260902`, then prove changed frame 9 through `m3-gate10-final-on-20260902`; rebuild NGX, no-NGX, and feature-off targets and run both selftests | all 120 frame/color/depth/motion/mask tuples matched exactly across ON/OFF and 118 returned-output hashes differed; ON alone logged feature-18 evaluation through count 60, while OFF logged `SAFE MODE: EnableHooks=0, all hooks off (no NR)`; frame 9 used native `0E9F202CA588F23F`, public-DLAA OFF `80C161B4A9783CA2`, and external-consumer ON `F2D37E657D2077C0`, then logged 1024/1024 marker pixels and successful Present on the same frame ID; all three build configurations linked and NGX/no-NGX selftests passed 45/45 | Gate 10 passes for the named D3D11On12 plus supplied RenoDX route with zero Flycast display-frame latency; synchronous evidence timings are diagnostic only, and overall production readiness remains blocked on FC-045, Gate 8, temporal quality, failure, and performance work

#60 2026-09-03 c0e0d0f0d | quality-phase rebaseline: inspect all `master...HEAD` changes and required neural documents; rebuild existing NGX, no-NGX, and feature-off Ninja Release trees; run both current selftests; do not enable synchronous Gate 10 evidence | all three builds completed, NGX and no-NGX selftests passed 45/45, worktree started clean, and HEAD matched the audited SHA; the first developer-shell lookup used the obsolete recorded Visual Studio path and failed before CMake ran, then `vswhere` located `C:/BuildTools/VS2022` and all actual builds ran there | transport/provenance remains experimentally proven from unchanged Gate 10 evidence; Gates 11-18 are pending and are mapped without replacing existing FC IDs

#61 2026-09-03 bd2adfe5b plus working tree | compile the production DX11 pixel shader in native and neural-export permutations; rasterize zero clear, farther opaque, nearer overlapping opaque, punch-through, reverse-order, and deliberately wrong comparison fixtures on native D3D11 and D3D11On12; capture exact R32/color artifacts; add `DepthInverted` to both public NGX create paths; run correct-inverted versus wrong-normal public DLAA declarations for 8 frames on each API; rebuild NGX, no-NGX, and feature-off configurations and run both selftests | both surfaces returned identical clear `0`, far `0.166836038`, near `0.263784289`, and punch-through `0.263784289`; native/export depth matched exactly; reverse submission was byte-identical; wrong polarity retained far red over near green; NGX D3D11 and D3D12 each submitted 8/8 with result 1, zero exceptions/invalid frames, and correct/wrong static outputs were byte-identical at hash `98F88138FD4684A3`; all three builds linked and both selftests passed 48/48; an attempted input fixture named `depth-layers` failed before rendering because it does not exist and was replaced by `opaque-pt-tr-stack` | Gate 11 passes from the falsifying production-shader fixture and exact cross-surface evidence; static NGX output equality is explicitly not polarity evidence, and the logarithmic PVR depth representation is retained

#62 2026-09-03 e52c6b21c plus working tree | rasterize synthetic current/previous unjittered positions to RG16F, apply jitter only to current raster position, validate static/+4X/-3Y/camera/deformation cases, score correct/reversed/doubled vectors by pixel reprojection, then feed the same previous/current pair through two-frame public DLAA on D3D11 and D3D12; rebuild NGX, no-NGX, and feature-off configurations and run both selftests | samples were static `[0,0]`, +4X `[-4,0]`, -3Y `[0,3]`, camera `[-5.99609375,2]`, deformation `[-1.34570312,1.00878906]` versus analytic `[-1.34634149,1.00975609]`, and jitter-only `[0,0]`; correct/reversed/doubled reprojection MAE was `0`/`47.8868056`/`37.318971`; DLAA PSNR against current was `34.529586`/`23.734309`/`25.660892` dB; all six API/control evaluations submitted 2/2 with zero invalid frames, corresponding D3D11/D3D12 hashes matched exactly, all three builds linked, and both selftests passed 50/50; the first temporal-run metadata encoded an extra quote after the motion array, was rejected during review, corrected, and all six accepted runs were repeated with JSON parsing green | Gate 12 synthetic contract passes: motion is previous-minus-current in render pixels with scale 1; wrong sign and double scale fail; production PVR previous-position streams remain pending

#63 2026-09-03 41b8ad7f2 plus working tree | compile the production DX11 presentation shaders; round-trip a 640x320 grayscale/RGB/CMY/black-white/alpha/checker chart; verify required and odd-sized content rectangles; run the chart for 8 public-DLAA frames on native D3D11 and D3D11On12; compare API outputs and constant-region samples; rebuild NGX, no-NGX, and feature-off trees and run both selftests | production `R8G8B8A8_UNORM` round-trip had 0 differing pixels and max delta 0; exact 4:3 targets were 1440x1080, 1920x1440, and 2880x2160, and 16:9 was 3840x2160; both public APIs submitted 8/8 with zero invalid frames and identical hash `64F37ECF41D2A40F`; their outputs were byte-identical, and 214,320 constant-interior RGB/alpha samples had max/mean delta 0 while reconstruction changes remained around transitions; first DLAA attempts omitted the required explicit external feature path and correctly returned unsupported, and the first read-only PowerShell sample script passed `PathInfo` rather than a string to `Bitmap`, failed before reading pixels, and was corrected | Q1 SDR/channel/exposure and rectangle math are proven; unit exposure and no-HDR remain correct; production FC-053 raster selection and black-border exclusion remain pending

#64 2026-09-03 f0187247e plus working tree | separate topology/UV/resource identity from centroid/bounds/depth pose; carry texture-cache update, palette hash, and direct-RTT generations; replace first-compatible matching for small exact-structure buckets with deterministic minimum-cost assignment; record assigned/second cost; reject changed generations and mark buckets above eight ambiguous; rebuild all three configurations and run both selftests | translated production-captured geometry retained its structural signature and tier-1 match; repeated reversed draws followed nearest pose one-to-one; same-address texture, palette, and RTT generation controls all produced zero confidence; nine identical draws all produced ambiguous zero-confidence results; one intermediate run exposed a stale-history similarity match after TCW identity changed and failed 1 of 57 checks, then the first full-matrix binaries exited with stack overflow after a second 8192-draw instrumentation object was added directly to the selftest stack; texture identity was made mandatory before similarity and the fixture object was moved to bounded heap ownership; final NGX and no-NGX selftests passed 60/60 and all three executables linked | Q2 structural identity, resource revisions, and bounded minimum-cost assignment are green; Gate 13 remains open on previous-position rasterization and complete evidence-based confidence/disocclusion controls

#65 2026-09-03 596a1fe8f | launch the supplied Soulcalibur CHD from an isolated no-add-on staging directory with native D3D11 public DLAA and synchronous evidence explicitly disabled; retain the first invocation and retry with corrected Windows argument quoting | the first `Start-Process -ArgumentList` invocation split the media path at `C:\Game` and exited cleanly before boot; the corrected quoted invocation identified game `T1401N`, entered REIOS boot, remained live for the bounded 15-second smoke interval, and was then explicitly terminated by the harness; no third-party neural consumer was present and no neural quality capture was produced | legal-media boot/runtime smoke did not expose a crash in the changed draw-record path, but it is not clean-shutdown, presentation, title-quality, or Gate 13 evidence

#66 2026-09-03 5720049d3 plus working tree | add two bounded geometry snapshots coupled to the accepted draw-history buffers; construct per-current-vertex accepted XYZ/validity by exact strip-index position; reject reset, overflow, reindex, Naomi 2, out-of-range, and conflicting shared-vertex mappings; run the three-build matrix and both selftests | a translated/deformed exact-topology triangle retained all three accepted prior positions; reordered indices retained a structural match but emitted zero trusted vertices; two repeated draws mapping shared current vertices to different historical vertices invalidated all conflicts; NGX and no-NGX selftests passed 63/63 and all three executables linked | accepted-history CPU correspondence is green, but FC-032 remains incomplete until the stream is bound to every required DX11 export shader and rasterized motion matches Gate 12 truth

#67 2026-09-03 0af1b4349 plus working tree | bind accepted XYZ/validity as a second DX11 vertex stream; add neural-only normal/Naomi 2 shader variants; rasterize separate current/previous unjittered screen positions; gate RG16F motion, confidence, and bias by match, validity, and magnitude; execute the production shader fixture on native D3D11 and D3D11On12; rebuild NGX, no-NGX, and feature-off; run both selftests; bounded no-add-on Soulcalibur process smoke | trusted production output was exactly `[-4,+3]`, mask 0, confidence 255, draw ID 7 on both surfaces; invalid and over-128-pixel controls were `[0,0]`, mask 255, confidence 0; an unevaluated pose retained the older accepted XYZ; incomplete stream upload now aborts neural submission; all three executables linked and NGX/no-NGX selftests passed 68/68; Soulcalibur stayed live for 15 seconds and was harness-terminated, with no title-quality capture; failed attempts retained: one malformed `cmd` quote failed before any tool ran, `neural_selftest` and `--selftest` were invalid harness spellings, direct PowerShell batch invocation lost MSVC include variables, the first valid source compile found a duplicate macro-enum name, the first fixture compile found a missing lambda terminator, and the first fixture execution used default LESS depth so no pixels passed until the production GREATER rule was applied | normal DX11 and the base guidance replay used by DX11 OIT now carry real exact-topology motion; Gate 12 remains green and FC-032 advances, while Gate 13 remains open on complete confidence/disocclusion evidence and Naomi 2 historical transforms

#68 2026-09-03 305436ff1 plus working tree | replace the remaining compatible-draw greedy path with bounded deterministic minimum-cost assignment; derive confidence from assignment separation, bounded reindex-fit residual, accepted-history age/skips, scene cut, per-vertex validity, content generations, and production magnitude; add rigid-reindex positive and deformation, repeated-order, particle, generation, stale-history, conflict, and scene-cut negative controls; rebuild NGX, no-NGX, and feature-off and run both selftests; launch the supplied Soulcalibur media in a no-add-on public-DLAA process with synchronous evidence disabled | all three executables linked; NGX and no-NGX selftests passed 71/71; rigid reindex produced zero-residual trusted prior positions while a 25-pixel non-rigid deformation exceeded the 0.25-pixel threshold and emitted zero trust; three skipped frames and unmatched-area scene cut invalidated the complete previous stream; Soulcalibur game ID `T1401N` reached REIOS boot and remained live for the bounded 15-second interval before harness termination; the first smoke invocation used incorrect transient section namespaces, stayed live but emitted no diagnostic log, and is excluded, while the corrected invocation explicitly set public DLAA, native D3D11, and evidence capture off | Gate 13 is green for normal Dreamcast geometry; Naomi 2 remains conservative zero-validity pending accepted matrix history, and Gate 14 accepted-depth/draw-ID disocclusion remains open; the Soulcalibur run is process smoke only, not quality, presentation, clean-shutdown, or performance evidence

#69 2026-09-03 d323ec0ca plus working tree | retain the last successfully submitted depth/draw-ID ring slot; export expected accepted draw identity; exclude retained history from new export-slot selection; resolve the public bias mask with production current-to-previous reprojection, encoded-depth tolerance, clear/outside/identity checks, and wrapped-resource ownership; add a ROM-free correct/omitted-pass fixture for static, camera pan, depth tolerance/disagreement, crossing objects, newly visible/revealed background, and scene cut; rebuild all three configurations, run both selftests, and run bounded native/On12 Soulcalibur public-DLAA smoke with synchronous evidence off | both production surfaces protected the same 192 disocclusion-only pixels, emitted exact mask SHA-256 `FEF0708C7C9007D312CA899F4E9B5483B955C17EC63D8E235F07001632155860`, and reduced synthetic trail energy from 12,288 in the omitted-pass control to 0; all three executables linked and NGX/no-NGX selftests passed 76/76, including a skipped-evaluation ring-slot negative control; both Soulcalibur processes reached `T1401N`, remained live for 10 seconds, and logged zero errors before harness termination; On12 explicitly logged its surface and a returned public output; the first commit command included an invalid documentation path, staged only the documentation fallback, and was amended with the complete explicit source list before push | Gate 14 is green; this is production guidance/disocclusion and process-smoke evidence, not translucency, overlay, title-quality, clean-shutdown, or performance evidence

#70 2026-09-03 43863d966 plus working tree | replay normal translucent lists into only the public current-color bias mask with depth and confidence disabled; emit separate OIT reactive coverage from the final visible A-buffer stack; reserve u2/u3 for OIT UAVs so RT0 scene color and RT1 coverage coexist; merge coverage without replacing opaque/punch-through trust; preserve the exact-zero synchronous Gate 10 evidence contract; compile and execute the exact production OIT resolve on native D3D11 and D3D11On12; launch supplied Soulcalibur media through public DLAA with renderer 6 on both surfaces | both GPU fixtures classified empty/modifier-only `[0]`, two single translucent pixels `[255,255]`, and a two-layer pixel `[255]`; both wrote byte-identical `reactive-mask.png` SHA-256 `0151DECFCB2FEFF9F9E5FDAE31C0404A1B86E9362E7960F4A9EA2E68213DC0DD`; baseline selftest passed 80/80; native and On12 OIT Soulcalibur children reached responsive `T1401N` windows for 10/12 seconds before harness termination, and On12 logged a returned public output; failed attempts retained: the first build incorrectly passed MSBuild `/m` to Ninja, the first extended shader compile exposed the RT1/u1 UAV collision and drove the u2/u3 correction, the first GUI-process harness observed only the detached launcher before child tracking was corrected, and the first fixture compile found a local `opaque` name collision that was corrected | Gate 15A is green for conservative normal/OIT translucency and modifier-volume exclusion; this is not overlay separation, title-quality, clean shutdown, or performance evidence

#71 2026-09-03 3b56cfb9c plus working tree | add strict accepted-history HUD classification, a separate R8 overlay target, exact late original-PVR composite before OSD/ImGui, per-game auto/full-frame/disabled policy, framebuffer-direct preservation, and a three-frame-latched predominantly-2D generative bypass; compile and execute the production composite on native D3D11 and D3D11On12; extend Gate 15A with a base-mask-union regression; build NGX, no-NGX, and feature-off configurations, run both selftests, and run supplied Soulcalibur `T1401N` through automatic normal DX11, full-frame OIT, and experimental automatic D3D11On12 modes | both overlay fixtures restored all 33 protected pixels byte-for-byte, changed zero unclassified pixels, failed all 33 pixels when the composite was deliberately omitted, and wrote byte-identical `composited.png` SHA-256 `669172F12DED34E458A9E915E88483B805FB554515F7B513F6CA7A487A92629F`; both transparency fixtures preserved the independent base mask while merging coverage and retained mask SHA-256 `0151DECFCB2FEFF9F9E5FDAE31C0404A1B86E9362E7960F4A9EA2E68213DC0DD`; all three executables linked and NGX/no-NGX selftests passed 93/93; all three 12-second Soulcalibur processes remained responsive before bounded harness termination, automatic and full-frame policies logged game ID/policy, full-frame protection became active, and experimental mode reported the expected missing external components while retaining fallback; failed attempts retained: the first On12 fixture invocations used unsupported switch spellings, the first media launch selected renderer 0/OpenGL and is excluded, and the first raw 2D classifier run exposed rapid scene-boundary transitions that drove the tested three-frame latch | Gate 15B is green for the production mechanism and conservative classifier, and Gate 15A remains green after correcting a post-commit discovery that the merge shader wrote `SV_Target0` while the first integration bound RT1; title-quality, capture-matrix, clean-shutdown, and performance evidence remain open

#72 2026-09-03 3dec95cf2 plus working tree | add default-on Match Neural Output raster sizing for target-native DX11 lanes without changing RTT/manual/SR paths; add documented public Auto/J/K preset hints to D3D11 and D3D12 creation; record exact production raster and preset status; run 240-frame public-DLAA Auto/J/K fixtures on both APIs, a 120-frame 1280x960-to-1920x1440 Quality-SR fixture, and supplied Soulcalibur fullscreen target-native and manual/SR controls | 2560x1440 fullscreen produced exact 4:3 input/output 1920x1440 at content `(320,0 1920x1440)` with match active; manual 2x and Quality SR retained 1280x960 input into 1920x1440 with match inactive; all preset runs submitted 240/240 with zero invalid frames, D3D11/D3D12 were byte-identical, Auto and K shared SHA-256 `4F0B7AE8DE4379D0A1C78478AC822CF36F83E6DC5A6D3781B07A02FE7B3FC17B`, and J differed in 399 pixels with SHA-256 `116D5EEC2A29EC7C46736E9F78BC45C76535B9C22E2001EE7F388A9434223DD7`; the SR run submitted 120/120; all three production executables linked and both NGX/no-NGX selftests passed 95/95; both production processes remained responsive until bounded harness termination; failed attempt retained: the first preset invocation pointed at the capture parent rather than `frame-0000/color.png` and exited before evaluation | FC-053 production sizing and public preset contract are green; Auto remains default because K has not beaten it across titles, and Gate 16 remains partial pending complete A-E moving/title/external-consumer comparisons

#73 2026-09-03 12a9a57b7 plus working tree | add a synchronous developer-only production capture writer, bounded Windows `neuraltest capture` launcher, component metrics, profile/style descriptors and UI recommendations; capture supplied Soulcalibur `T1401N` through native D3D11, public-DLAA normal D3D11, DX11 OIT, D3D11On12, and no-NGX fallback; run missing-media and completed-destination negatives; rebuild NGX, no-NGX, and feature-off and run both selftests | final public-DLAA packages completed with clean window close on all three production renderer/surface combinations; normal D3D11 captured frames 121-123 and D3D11On12 121-122 with accepted output, exact 640x480 content, HUD mismatch 0, 93.850911 percent trusted pixels, and temporal-flicker artifacts after the first frame; the corrected true OIT run captured frames 121-122, identified `renderer=dx11-oit` inside each manifest, accepted public output, reported HUD mismatch 0 and 47.630208 percent trusted versus 52.369792 percent reactive coverage; native PVR and actual contract source hashes matched exactly while public output differed; no-NGX produced full guidance/final packages with `evaluation_accepted=false`, no public/external artifact, and the precise build-without-NGX reason; native passthrough produced no falsely labeled public output; missing media returned 3 and an already-complete destination returned 2 without replacing evidence; both selftests passed 98/98; failed attempts retained: renderer 3 selected OpenGL OIT rather than DX11 OIT, early manual launches used the wrong `rend:` namespace and ignored neural options, the first CLI OIT requests used `pvr:rend` rather than the actual `config:pvr.rend` transient key and their manifests exposed normal DX11 so they were rejected, a run without the supplied public feature path correctly captured unsupported status, the first accepted package exposed valid native color but black source/public output because the export copy inherited stale blend state, the first compile lacked the stb include path, another compile hit a still-running Flycast link lock, and an initial feature-off command named a nonexistent build directory | FC-054 production capture transport and FC-016 error semantics are green, including the corrected explicit opaque export copy; FC-054/FC-065 remain doing because GPU timings are null, external output and comparison index are absent, and only the Soulcalibur intro rather than the representative moving gameplay matrix is covered; Gate 17 and Gate 18 remain open

#74 2026-09-03 c406ed558 plus working tree | add `neuraltest capture-index` to discover production frame packages recursively and generate a relative-path HTML contact sheet plus machine-readable JSON; execute it over the retained baseline capture tree and an empty-root negative control | the real tree indexed 26 production packages and exposed native/source/guidance/public/final/difference/flicker images only where present; cards reported actual renderer/API, accepted state, external-contract state, submit status, profile, manifest, and metrics; JSON reported `winner_declared=false` and 26 relative manifest paths; the empty root wrote an empty diagnostic index and returned 3 | the comparison-index mechanism is green and retains failed packages without promoting them; Gate 17 remains open because an index cannot replace moving-sequence, external-output, multi-profile, or representative-title acceptance

#75 2026-09-03 c3cad2f03 plus working tree | add hidden opt-in asynchronous production performance telemetry and a bounded `neuraltest performance` launcher; measure supplied Soulcalibur `T1401N` with synchronous capture/evidence off in native-off, public-DLAA normal DX11, DX11 OIT, D3D11On12, and no-NGX fallback lanes; run missing-media, completed-destination, and invalid-bounds negatives | a 600-sample post-warmup normal-D3D11 DLAA run completed with clean close, zero query-ring stalls, VRAM 267886592 to 267862016 bytes (-24576), and P99 base PVR 0.024128 ms, guidance 0.231488 ms, stage evaluation 0.253344 ms, and composite 0.026944 ms; a 60-sample native-off control had zero submissions/guidance and P95 base PVR 0.013056 ms; true OIT completed 60 samples with actual `renderer=dx11-oit`, zero ring stalls, and P95 base PVR 0.359360 ms, guidance 0.075872 ms, evaluation 1.060992 ms, composite 0.030464 ms; D3D11On12 completed 60 samples with zero ring stalls and explicitly recorded cross-queue evaluation as unavailable/null rather than claiming its 0.005 ms D3D11 command-gap control; the no-NGX build completed 60 samples and clean close with zero submissions, 183 native fallbacks, zero query-ring stalls/create/evaluate failures, VRAM growth -655360 bytes, and P95 base/guidance/composite times of 0.009760/0.084864/0.018688 ms; subsequent 30-sample native-off and no-NGX controls wrote null evaluation summary/raw values with the distinct `not-applicable-neural-off` and `not-observed-no-accepted-submission` scopes, zero ring pressure, and zero VRAM growth; all final JSON parsed successfully and all launches closed cleanly; negative controls returned 3/2/2 without launching or replacing evidence; failed attempts retained: the first tracker build included `comptr.h` before D3D types, the first run reset its warmup every frame and timed out, the first locale-sensitive report emitted invalid comma-grouped VRAM integers, initial VRAM sampling occurred before warmup and was moved to the measured-window boundary, the first OIT performance run never completed because its override did not call the base Render instrumentation, and parallel post-commit CMake configure passes raced on source-tree `core/version.h` before a serial no-NGX retry succeeded | FC-063 gains bounded no-NGX production fallback evidence and FC-064/Gate 18 advance with non-blocking telemetry and short resource-growth evidence; this does not close D3D12-queue timing, external-consumer timing, long-duration leak, transition/failure, latency, or representative-title coverage

#76 2026-09-03 60c2bc5d9 plus working tree | add default-off developer fault controls at the public feature-create/evaluate and output-ring/fence boundaries, an accepted-evaluation arming delay, device-removed status latching, launch/capture provenance, and real host-present recovery notification; exercise supplied Soulcalibur `T1401N` on native D3D11 and D3D11On12; run invalid-injection and out-of-range-delay negatives | three create, evaluate, and ring/delayed-fence faults on each API each entered one bounded hold, reset history, and resumed public DLAA; native D3D11 post-hold submissions were 265/266/266 with create/evaluate/busy counts 3/3/3, while D3D11On12 recorded 264 submissions for each and the same exact injected counts; every 30-sample run closed cleanly with zero telemetry-ring pressure and zero VRAM growth except the bounded D3D11On12 device-status allocation interval at +262144 bytes; synthetic device-removed status incremented once on each API, admitted zero later submissions, and remained on native fallback; two-frame create-failure captures on each API had no public/external output and source-color/final-composited SHA-256 equality; after five accepted evaluations, evaluate, busy, and device-status captures on each API showed a public-output frame followed by a rejected frame with no public/external artifact and byte-identical source/final color, proving stale accepted output was not reused; invalid injection and delay controls returned 2 without launching; both selftests passed 99/99 after adding the removed-state invariant; failed attempts retained: the first create-fault run exposed that DX11 never called `NotifyHostPresent`, so hold could not recover until actual rendered presents were wired, and the first delayed On12 capture skipped the pre-fault accepted frame before the corrected skip-four control captured the accepted-to-fallback transition | FC-063/FC-045 advance with safe repeatable failure evidence; actual DXGI device removal, injected SEH, runtime removal while active, renderer/resize/fullscreen transitions, and long-run coverage remain open, so Gate 18 remains partial

#77 2026-09-03 fed8e0505 plus working tree | retain source, accepted-evaluation, and selected-output frame identity in each asynchronous GPU-query slot; attach the observation only after a real host Present; give native D3D11 the output-frame identity already carried by D3D11On12; sort asynchronously resolved samples by submission sequence; add deterministic repeat/drop/gap/alternation/latency accounting and a negative unit fixture; exercise supplied Soulcalibur `T1401N` through public DLAA on both production surfaces, a mid-window evaluate-failure control, experimental DLSS 5 with no external consumer, and no-NGX fallback | native D3D11 and D3D11On12 each completed 60 measured frames with 60 accepted evaluations and 60 neural presents, zero missing/accepted-but-unpresented frames, identity mismatches, source repeats/gaps, output repeats, native/neural alternations, or frame latency, plus zero query-ring pressure and zero VRAM growth; a deliberate evaluate failure after 35 accepted frames produced 54 native and 6 neural measured presents, one detected neural/native transition, zero accepted-output drops, zero stale repeats, zero identity errors, and zero latency; the no-consumer DLSS 5 control completed 30 native presents, correctly counted 30 accepted public candidates as not presented, and claimed zero neural presents; no-NGX completed 30 native presents with zero accepted/neural frames and no cadence error; NGX, no-NGX, and feature-off builds linked and both selftests passed 100/100; failed attempts retained: the initial compile command named an unconfigured uppercase `BUILD` directory, and the first PowerShell summary expression had an empty pipeline element before the corrected structured summary | FC-064 and Gate 18 now have production frame-identity and presentation-cadence evidence on both routes; long-run, active runtime removal, real device loss, external timing, transition matrix, and representative-title coverage remain open

#78 2026-09-03 9701b6fa5 plus working tree | add a bounded process-owned performance transition that records a delayed Win32 resize, minimize, restore, and exact resize-back only after each state and both target rectangles are observed; reject invalid transition names and delays above 60000 ms; run the supplied Soulcalibur `T1401N` five-second-delayed sequence through normal DX11 and DX11 OIT on native D3D11 and D3D11On12 | all four final launch reports parsed and recorded every action true, 600 measured Presents, clean window close, zero missing or accepted-but-unpresented frames, zero identity mismatches, output repeats, query-ring pressure, or frame latency, and one source-frame-ID gap at the minimize boundary; normal D3D11 recorded 565 neural plus 35 explicit native fallback presents and one transition while resize resources/public NGX recovered, with 3 busy results, 7 fallbacks, 8 resets, and -54013952 bytes VRAM growth; normal D3D11On12 recorded 600 neural presents, 1 fallback, 2 resets, and -131072 bytes growth; OIT D3D11 recorded 600 neural presents, 2 busy results, 6 fallbacks, 10 resets, and +3256320 bytes growth; OIT D3D11On12 recorded 600 neural presents, 1 fallback, 3 resets, and +1179648 bytes growth; invalid transition and delay controls returned 2 without launching; failed attempts retained: the first delayed D3D11 report exposed one stray quote in transition JSON and was rejected despite clean rendering, and three On12 attempts refined restore-state verification from immediate/asynchronous checks to synchronous state changes with delayed observation before the final accepted run | FC-045/FC-063/FC-064 advance with active-render window-transition evidence on both APIs and both DX11 renderers; fullscreen, monitor move, alt-tab, renderer restart, load/unload, active runtime removal, actual device loss, and longer/title/external timing coverage remain open, so Gate 18 remains partial

#79 2026-09-03 b3399f96c | run paired 10000-sample public-DLAA normal-DX11 production soaks after 600 warmup frames on native D3D11 and D3D11On12 with synchronous capture/evidence disabled | each route recorded 10000 accepted evaluations, 10000 neural Presents, zero native Presents, missing or accepted-but-unpresented frames, identity mismatches, source repeats/gaps, output repeats, native/neural alternations, frame latency, or query-ring pressure, then closed cleanly; D3D11 recorded 10600 total submissions, one initial busy result, three fallbacks, two resets, local VRAM 306855936 to 303792128 bytes (-3063808), and P99 base/guidance/evaluation/composite values of 0.025664/0.210240/0.630240/0.030400 ms; D3D11On12 recorded 10600 submissions, zero busy, two fallbacks, two resets, local VRAM exactly flat at 391602176 bytes, and P99 base/guidance/composite values of 0.042336/11.182784/0.060000 ms, with evaluation correctly null because the work is on the uninstrumented D3D12 queue | FC-064 gains bounded long-run cadence and VRAM evidence for the normal renderer on both surfaces; OIT long-run, resource-object counts, external-consumer timing, D3D12 query-heap timing, broader titles, and remaining Gate 18 transitions remain open

#80 2026-09-03 143162f89 plus working tree | add a hidden exact-main-frame developer trigger for real renderer/API-context teardown and recreation, an after-initialization marker, strict launcher verification, and fresh post-restart performance sampling; run supplied Soulcalibur `T1401N` through normal DX11 and DX11 OIT on native D3D11 and D3D11On12 with restart at main frame 300, 60 replacement-renderer warmup frames, and 600 measured frames | all four markers recorded frame 300, the requested renderer, completed initialization, and restarted sampling; every replacement renderer recorded 600 accepted evaluations and 600 neural Presents with zero native Presents, missing or accepted-but-unpresented frames, identity mismatches, output repeats, fallback, query-ring pressure, or frame latency, one expected new-lifetime reset, and clean close; normal D3D11, normal D3D11On12, and OIT D3D11On12 had zero measured-window VRAM growth, while OIT D3D11 released 3612672 bytes; an out-of-range frame threshold returned 2 without launching; failed attempts retained: the initial long incremental build completed after its output session expired, the first PowerShell result-summary expression contained an empty pipeline element before the corrected JSON summary, the first strict-marker run proved that stream-buffer iteration does not set `eof()` and was rejected until marker-open validation replaced that invalid assumption, and the first post-commit configure omitted the Visual Studio developer environment and rejected the unknown architecture before the serial developer-shell rerun succeeded | FC-045/FC-052/FC-063/FC-064 and Gate 18 advance with same-renderer context-recreation evidence on both surfaces and both DX11 paths; renderer/API switching, fullscreen, game load/unload, active runtime removal, actual device loss, OIT long-run, external timing, and broader-title coverage remain open

#81 2026-09-03 eb27232ef plus working tree | add a mutually exclusive exact-frame developer switch between normal DX11 and DX11 OIT, record and strictly verify source/destination renderer identity, and require a fresh destination-renderer performance interval; run both switch directions at main frame 300 on native D3D11 and D3D11On12 with supplied Soulcalibur `T1401N`, 60 destination warmup frames, and 600 measured frames | all four markers recorded the exact frame and requested `2 -> 6` or `6 -> 2` direction, and every final report identified the destination renderer; each destination recorded 600 accepted evaluations, 600 neural Presents, zero native Presents, missing or accepted-but-unpresented frames, identity errors, output repeats, latency, or query-ring pressure, and clean close; all had one expected new-lifetime reset, three measured windows had zero VRAM growth, and native-D3D11 normal-to-OIT released 37703680 bytes; three runs recorded one setup fallback before their measured window and On12 OIT-to-normal recorded none; the mutually exclusive reinit-plus-switch negative returned 2 without launching; failed attempt retained: one diagnostic `rg` command passed a PowerShell wildcard literally on Windows, returned 1 after `git diff --check` had already passed, and was rerun with explicit paths | FC-045/FC-052/FC-063/FC-064 and Gate 18 advance with bidirectional renderer-variant switching on both selected surfaces; D3D11/D3D11On12 surface switching, fullscreen, game load/unload, active runtime removal, actual device loss, OIT long-run, external timing, and broader titles remain open

#82 2026-09-03 91414611c plus working tree | add a mutually exclusive exact-frame switch between native-D3D11 and D3D11On12 neural/public-NGX surfaces, record and strictly verify source/destination surface identity, and require a fresh destination-API performance report; run both directions at main frame 300 while retaining normal DX11 and DX11 OIT with supplied Soulcalibur `T1401N`, 60 destination warmup frames, and 600 measured frames | all four markers recorded the exact frame and requested `0 -> 1` or `1 -> 0` direction, and every final performance report identified the destination API and retained renderer; each recorded 600 accepted evaluations, 600 neural Presents, zero native Presents, missing or accepted-but-unpresented frames, identity errors, output repeats, latency, fallback, or query-ring pressure, one expected new-lifetime reset, and clean close; both normal-DX11 measured windows had zero VRAM growth, while native-D3D11-to-On12 OIT grew by 131072 bytes and On12-to-native-D3D11 OIT grew by a bounded 3334144 bytes; a simultaneous renderer-plus-surface switch request returned 2 without launching; failed attempt retained: the first strict destination check read `performance-complete.json` instead of `performance.json` and rejected a clean completed transition until the report path was corrected and all four final runs were repeated | FC-045/FC-052/FC-063/FC-064 and Gate 18 advance with bidirectional selected-surface switching on both DX11 renderers; fullscreen, monitor move, game load/unload, active runtime removal, actual device loss, OIT long-run/resource accounting, external timing, and broader titles remain open

#83 2026-09-03 0a113b21a | run paired 10000-sample public-DLAA DX11-OIT production soaks after 600 warmup frames on native D3D11 and D3D11On12 with synchronous capture/evidence disabled | each route recorded 10000 accepted evaluations, 10000 neural Presents, zero native Presents, missing or accepted-but-unpresented frames, identity mismatches, source repeats/gaps, output repeats, native/neural alternations, frame latency, or query-ring pressure, then closed cleanly; native D3D11 recorded 10600 submissions, 3 setup fallbacks, 5 resets, local VRAM 878596096 to 879243264 bytes (+647168), and P99 base/guidance/evaluation/composite values of 1.228448/0.140384/2.788896/0.067520 ms; D3D11On12 recorded 10600 submissions, 2 setup fallbacks, 5 resets, local VRAM exactly flat at 938659840 bytes, and P99 base/guidance/composite values of 2.022080/10.202720/0.064864 ms, with evaluation correctly null because the work is on the uninstrumented D3D12 queue | FC-064 closes the bounded OIT long-run cadence check on both selected surfaces; the small native OIT VRAM delta is retained rather than called a leak or no-growth result, and resource-object counts, external-consumer timing, D3D12 query-heap timing, broader titles, and remaining Gate 18 failures/transitions remain open

#84 2026-09-03 029cf3d4a plus working tree | extend the process-owned transition harness with Flycast's real unmodified F11 path, separately verify request/observed enter/request/observed exit/exact rectangle restoration, and run a five-second-delayed borderless desktop-fullscreen roundtrip through normal DX11 and DX11 OIT on native D3D11 and D3D11On12 with supplied Soulcalibur `T1401N` | all four accepted launch reports recorded every fullscreen action true, exact original-rectangle restoration, 600 measured neural Presents, zero native Presents, missing or accepted-but-unpresented frames, identity mismatches, output repeats, native/neural alternations, frame latency, or query-ring pressure, and clean close; each retained one source-frame-ID gap at the mode boundary; native D3D11 normal/OIT released 317444096/256200704 bytes across the measured resize interval, On12 normal released 524288 bytes, and On12 OIT was flat; setup fallback/reset counts were 4/4, 2/4, 3/7, and 2/6 respectively; failed attempts retained: the first verifier incorrectly required resize-only flags during fullscreen and collapsed exit observations, and the second proved 350 ms was too short for SDL/Windows exit propagation before the accepted one-second observation interval | FC-045/FC-063/FC-064 and Gate 18 advance with supported borderless desktop fullscreen on both surfaces and renderers; Flycast does not expose exclusive fullscreen here, while alt-tab/focus, monitor move, game load/unload, active runtime removal, actual device loss, external timing, and broader-title coverage remain open

#85 2026-09-03 e8a333dc8 plus working tree | add a hidden exact-frame same-media lifecycle control that retains identity only in memory, invokes the real emulator unload path, observes cleared content state, reloads and starts the same media, and strictly verifies the marker without recording its path; run at main frame 300 through normal DX11 and DX11 OIT on native D3D11 and D3D11On12 with supplied Soulcalibur `T1401N` and telemetry spanning the boundary | all four markers recorded unload observed, unchanged game ID/media identity, and completion at frame 300; every run completed 600 accepted evaluations and 600 neural Presents with zero native Presents, missing or accepted-but-unpresented frames, identity errors, output repeats, native/neural alternations, frame latency, or query-ring pressure and clean close; each retained three source-frame-ID gaps rather than manufacturing continuity; setup fallback/reset counts were 5/4, 4/4, 5/10, and 4/10; measured-window VRAM deltas were +57344, -524288, +2936832, and -327680 bytes respectively | FC-026/FC-034/FC-045/FC-052/FC-063/FC-064 and Gate 18 advance with real same-media unload/reload across both renderers and surfaces; cross-title load, save-state runtime, alt-tab/focus, monitor move, active runtime removal, actual device loss, external timing, and broader-title coverage remain open

#86 2026-09-03 c43d4191f plus working tree | add a cross-platform production `dc_serialize`/`Emulator::loadstate` memory image, a hidden exact-frame save/load developer control, a path-free completion marker, strict performance-launcher verification, and invalid-delay/mutual-exclusion controls; run supplied Soulcalibur `T1401N` save at main frame 200 and load at frame 230 through normal DX11 and DX11 OIT on native D3D11 and D3D11On12 with telemetry spanning the boundary | all four accepted markers parsed as JSON, restored the same nonzero 27797870-byte state entirely from process memory, and recorded the exact requested frames; each completed 600 Presents with zero missing or accepted-but-unpresented frames, identity mismatches, source repeats, output repeats, or frame latency and one explicit source-frame gap; native D3D11 normal and both On12 cases presented 600/600 neural frames with no alternation, while native D3D11 OIT reproducibly presented 599 neural plus one fresh native fallback and counted two path transitions at the load reset, never stale output; fallback/reset counts were 3/3, 4/7, 2/3, and 2/7, with measured VRAM deltas +106496, -454656, -327680, and -106496 bytes; zero delay and a simultaneous renderer-reinit request each returned 2 without launching | failed attempts retained: the first memory helper incorrectly required the TA maximum dry sizing pass to equal the smaller actual write pass, rejected a completed 36173902-to-27797870-byte serialization, and emitted a locale-grouped invalid JSON byte count; the corrected helper bounds allocation by the dry pass, shrinks to the completed write length, and emits locale-independent JSON; the first three-target build command requested the intentionally absent `neuraltest` target from the feature-off configuration before that configuration was correctly built as Flycast-only; FC-026/FC-034/FC-045/FC-052/FC-063/FC-064 and Gate 18 advance with real save-state lifecycle evidence across both renderers and surfaces; cross-title load, alt-tab/focus, monitor move, active runtime removal, actual device loss, external timing, and broader-title coverage remain open

#87 2026-09-03 e4bb18ab7 plus working tree | add a hidden exact-frame production pause/resume control through `gui_togglePause`, require observed `GuiState::Pause` and `GuiState::Closed` states, write a strict path-free marker, reject zero duration and ambiguous simultaneous lifecycle controls, and run supplied Soulcalibur `T1401N` from pause frame 200 to resume frame 260 through normal DX11 and DX11 OIT on native D3D11 and D3D11On12 with telemetry spanning the interval | all four markers recorded the exact frame pair and both observed states; every run completed 600 Presents with zero missing or accepted-but-unpresented frames, identity mismatches, source repeats, output repeats, or frame latency and one explicit source-frame gap; native D3D11 normal and both On12 cases presented 600/600 neural frames without alternation, while native D3D11 OIT presented 599 neural plus one fresh native fallback and two counted path transitions; fallback/reset counts were 3/2, 4/5, 2/2, and 2/5, with measured VRAM deltas -675840, +2797568, -524288, and +811008 bytes; zero duration and simultaneous same-media-reload requests each returned 2 without launching | failed focus attempts retained outside the repository: four normal-D3D11 600-sample runs remained cadence-clean, but this non-interactive desktop rejected process-owned foreground transfer through `SetForegroundWindow`, attached input queues, Alt-input permission, `SwitchToThisWindow`, and bounded title-bar activation, so every focus transition correctly failed and the unproven code was removed; FC-045/FC-052/FC-063/FC-064 and Gate 18 advance with real pause/resume coverage, while focus/alt-tab, monitor move, cross-title load, active runtime removal, actual device loss, external timing, and broader-title coverage remain open

#88 2026-09-03 9031b970e plus working tree | add per-sample live Flycast-owned neural GPU-object accounting for renderer export/wrapper resources and D3D11/D3D12 public-NGX backend resources, require its scoped report fields in the performance launcher, run a four-case 600-sample acceptance matrix, then run four parallel resource-lifetime-specific 10000-sample soaks after 600 warmup frames with supplied Soulcalibur `T1401N`; audit connected displays and legally available Dreamcast media | native D3D11 normal/OIT remained exactly 91 objects from initial through minimum/maximum/final, split 82 renderer plus 9 backend; D3D11On12 normal/OIT remained exactly 125, split 115 plus 10; all four long runs presented 10000/10000 accepted outputs with zero native, missing, accepted-but-unpresented, identity, source-repeat/gap, output-repeat, alternation, latency, or query-ring-pressure counts and clean close; DXGI local-VRAM growth was -2408448, -2846720, 0, and 0 bytes; the host exposed only `DISPLAY1` at 2560x1440 and media discovery found no second Dreamcast title beyond the supplied Soulcalibur image (the other full game image was Saturn and was not used) | FC-064 closes Flycast-owned neural resource-object growth for normal/OIT on both selected surfaces; opaque NGX/external allocations, external timing, D3D12 query-heap timing, monitor move, cross-title coverage, active runtime removal, actual device loss, and focus/alt-tab remain open, so Gate 18 is still partial

#89 2026-09-03 f45fb333a plus working tree | add a distinct one-shot `runtime-unavailable` injection after an exact accepted-evaluation threshold; defer it with nonblocking `Busy` while ring work is outstanding, then release the public feature/parameters/API and backend GPU objects, clear stage output, latch terminal native fallback, expose a stage counter, and run 120-sample mid-window transitions after 120 warmup frames on normal/OIT native D3D11 and D3D11On12 with supplied Soulcalibur `T1401N` | native D3D11 normal/OIT recorded 61 neural plus 59 native Presents and released 9 backend objects from 91 to 82; D3D11On12 normal/OIT recorded 60 neural plus 60 native and released 10 objects from 125 to 115; all four recorded exactly one terminal status and one explicit path transition with zero missing, accepted-but-unpresented, identity, source-repeat, output-repeat, or latency faults and clean close; an earlier D3D11 run injected during warmup and correctly measured 120 native frames, and an invalid count of 2 returned 2 without creating output | FC-063 gains controlled active-runtime-unavailability and leak-free retirement evidence; no binary was modified, moved, unloaded, or deleted, so physical loaded-runtime removal, actual device loss, external timing, focus/alt-tab, monitor move, and broader titles remain open and Gate 18 remains partial

#90 2026-09-03 a47915256 plus working tree | capture supplied Soulcalibur `T1401N` frames 302-331 after a 300-frame skip in native 640x480, target-resolution 880x660 public DLAA with Faithful/Enhanced/Photoreal profile metadata, and a 5120x3840 8x-native reference downsampled to 880x660; generate a comparison index; after the first 8x manifests exposed host-locale grouping as `[5,120, 3,840]`, imbue every production capture JSON stream with the classic locale, recapture all 30 8x frames into a new directory, and make index discovery reject/count malformed render/output/content arrays | all five accepted lanes completed cleanly with 30 unique consecutive final outputs, zero reported frame repeats/drops or HUD mismatches; target-DLAA profile sequences were byte-identical across all corresponding frames, confirming Flycast did not write external profile settings; the corrected 8x manifests all parsed as exact two-element `[5120, 3840]`; the final index reported 150 valid packages, 30 retained rejected packages, zero invalid accepted arrays, and `winner_declared=false`; one combined PowerShell summary line failed at parse time and the junction-only index attempt returned zero because reparse directories are not traversed, while separate summary and original-root validated-index commands succeeded | FC-054 gains locale-stable metadata and rejection-aware indexing, and FC-065 gains a current moving Soulcalibur native/DLAA/profile/8x subset; the sequence is not gameplay, no external output was present, profile image identity is expected, and Gate 17 remains open for gameplay, external comparisons, and other legal titles

#91 2026-09-03 2635de430 | place only the current Flycast executable under a new name beside the unchanged user-supplied Gate 10 staging components and request ten Soulcalibur frames 302-311 through the D3D11On12 experimental lane without reading or modifying any add-on/runtime/config binary | all ten captures completed cleanly and reported public output plus consumer components and an evaluated contract, but each status explicitly remained `neural output remains unconfirmed`; the capture writer consequently produced candidate files whose current `neural-rendering-output` label is not sufficient presentation provenance | rejected as Gate 17 external-output evidence: no per-capture mutation/sentinel proof was run, Feature 18 evaluation alone cannot prove presentation, and the candidate directory is retained outside the repository; external moving quality comparison remains open

#92 2026-09-03 88078a72a plus working tree | split external contract evaluation from external-output confirmation in capture metadata, require confirmation before writing `neural-rendering-output.png` or setting its presence flag, show confirmation rather than mere contract activity in the comparison index, then run three Soulcalibur frames through the unchanged supplied D3D11On12 staging route | all three frames reported contract evaluated and public output present while confirmation/presence remained false and no externally labeled file was written; capture completed and closed cleanly | FC-054 false-positive provenance path closed; capture-specific external mutation/presentation proof remains open

#93 2026-09-03 61b19c7c5 plus working tree | add locale-stable exact raw color/depth/motion/mask/returned hashes to schema-3 quality manifests; add same-build evidence SHA identity, a bounded default-zero or exact-production-mask sentinel launcher, and a fail-closed external-capture verifier requiring ON consumer activity, exact five-hash candidate equality, 1024/1024 same-frame swapchain sentinel pixels, completed Present, explicit `EnableHooks=0` OFF policy, exact four-input equality, and different OFF output; run supplied Soulcalibur `T1401N` through the unchanged user-supplied D3D11On12 route without reading or changing component binaries/configuration | clean unmarked quality frames 302/303 had returned hashes `A749AF2CA45D0AE4`/`76F0A3A0557F764B`; exact-production-mask ON evidence frames 179/180 matched every input and returned hash, had distinct marked hashes, 1024/1024 marker pixels, and completed same-frame Presents; policy-OFF frames 181/182 matched all four inputs but returned `9AE7F9F80C9263A4`/`B99A8733A0CFF53B`; the verifier promoted exactly two files and attached their proof while SHA-256 confirmed the promoted PNGs were byte-identical copies of the clean candidates | FC-054 gains capture-specific external mutation and presentation provenance for a bounded moving Soulcalibur sample; failed attempts retained outside the repository: the first hash strings inherited host digit grouping before classic-locale correction, early frame 31-35 captures were unaccepted while the consumer initialized, the first replay correctly exposed the legacy forced-zero mask mismatch, a third temporal output did not exactly match and was not accepted, and the deliberate zero-mask negative exited 1 with zero promotions; Gate 17 remains partial pending gameplay/profile/title coverage and moving visual acceptance

#94 2026-09-03 e92b34687 plus working tree | build a dedicated `TEST_AUTOMATION=ON` NGX configuration; make its 64-bit input-cycle format portable to MSVC, resolve replay media names with cross-platform filesystem semantics, log successful replay open/first application, add `capture --input-replay yes|no`, retain the exact script/hash/byte count, and drive supplied Soulcalibur `T1401N` through a repeatable Hoko Temple combat interval; add capture correspondence diagnostics, correct final validation of the PVR `0xFFFFFFFF` primitive-restart sentinel, and correct the public Quality-SR odd-width contract while preserving ordinary manual even-width behavior | two independent native frame-1802 scouts were pixel-identical; all initial native/Auto/J/K 30-frame source sequences matched exactly; the pre-fix gameplay path incorrectly scene-cut every frame with 100% reactive pixels even though 68 candidates covered 37150465/50340803 accumulated area, because only 4 survived sentinel validation; the fix produced 68/68 validated draws and 85.22% trusted pixels at frame 1802, then 29/30 valid-history frames with 82.81% average trusted coverage, one conservative topology reset, zero repeats/drops/HUD mismatches, and average target-DLAA temporal variance 9.67 versus native 31.93; J measured 9.51 but worsened trail/edge/thin-line/color/saturation/black components, while K provided no material gain, so Auto remains default; public NGX rejected both 640x480 and legacy-even 426x320 inputs with an exact 427x320 request, after which the corrected 427x320-to-640x480 Quality run submitted 3/3 | FC-032/FC-044/FC-054/FC-065 and Gates 13/16/17 advance with real, reproducible gameplay motion; failed attempts retained outside the repository include the initial MSVC `long`/`u64` compile failure, accidental legacy-DX9 configuration requiring unavailable `d3dx9shader.h`, wrong transient replay namespace, replayed pause state, all-reactive DLAA sequences, and both rejected SR dimension probes; gameplay 8x/external profile lanes, capture GPU timings, Naomi 2 history, broader legal coverage, and other legal titles remain open

#95 2026-09-03 47a4ff6bf plus working tree | complete a ten-frame 5120x3840 8x-native Soulcalibur combat reference; add a default-zero exact emulated-frame threshold to both D3D12 evidence marking/readback and D3D11On12 swapchain verification; place only the current automation executable and retained input script under unique names beside the unchanged supplied ON/OFF components; capture overlapping active-combat windows, isolate exact candidates, run fail-closed confirmation, and generate a 671-package comparison index | 8x produced exact 640x480 output with nine valid-history frames, one conservative scene cut, zero repeats/drops, and 77.07% average trusted coverage; target-native Auto versus 8x differed at 231973 pixels with max delta 178 and PSNR 33.02 on frame 1802, so 8x remains reference-only; the final ON and explicit-policy-OFF evidence each recorded ten exact frame-2025-through-2034 contracts with matching inputs, ON returned-output mutation, 1024/1024 sentinel pixels, and same-frame completed Presents; one unmarked active-combat candidate exactly matched ON/OFF frame 2025 and was promoted by the verifier, showing Kilik and Taki in close-range motion with intact HUD/color/silhouettes | FC-044/FC-054/FC-065 and Gates 16/17 advance; failed attempts retained outside the repository include a start frame after bounded shutdown, an initial swapchain verifier that consumed attempts before the new threshold, append-only staging logs rejected for mixed SHAs, and a nominal-frame clean window whose hashes differed because launch timing changed the gameplay state; only one gameplay frame earned external-output provenance, so no external temporal or full Gate-17 winner claim is made, and other legal titles remain unavailable

#96 2026-09-03 7761b8835 plus working tree | extend the production performance launcher with opt-in test-automation input replay, retained FNV64/byte provenance, and strict invalid-value rejection; place only the current executable under a unique name beside the unchanged supplied ON/OFF components; run 1800 warmup plus 600 measured Soulcalibur combat frames on D3D11On12 with synchronous quality/evidence capture disabled | ON host logs recorded feature-18 creation/evaluation and the explicit OFF host logged `SAFE MODE: EnableHooks=0, all hooks off (no NR)`; ON presented 600/600 accepted outputs while OFF presented 600/600 native frames and counted all 600 accepted public candidates as unpresented; both covered exact source frames 1802-2401 with zero missing Presents, identity errors, source gaps/repeats, output repeats, alternations, latency, or query-ring pressure and clean close; ON/OFF retained replay FNV64 `0A96F4E2FB52C75C`, constant 125/116 Flycast-owned objects, and bounded local-VRAM deltas +196608/+131072 bytes; ON Present P50/P95/P99 was 13.8265/14.8639/15.6154 ms versus OFF 13.4586/14.6828/16.1706 ms | FC-064 and Gate 18 gain real external-consumer gameplay cadence/resource coverage; the invalid replay value returned 2 without launching; synchronous evidence is absent, and D3D12/external evaluation timing remains honestly null because it is outside the D3D11 timestamp domain, so no isolated external-cost claim or Gate-18 closure is made

#97 2026-09-03 66e9b3aff | repeat the supplied external-consumer ON versus explicit-policy-OFF 1800-warmup/600-sample deterministic Soulcalibur combat measurement through production DX11 OIT on D3D11On12 with synchronous capture/evidence disabled | ON presented 600/600 accepted outputs while OFF presented 600/600 native frames and retained all 600 accepted public candidates as unpresented; both covered exact source frames 1802-2401 with zero missing Presents, identity errors, source gaps/repeats, output repeats, alternations, latency, or query-ring pressure and clean close; ON/OFF retained replay FNV64 `0A96F4E2FB52C75C`, constant 125/116 Flycast-owned objects, and +131072-byte local-VRAM deltas; ON Present P50/P95/P99 was 13.8293/15.2448/16.5339 ms versus OFF 13.4453/14.6009/15.0220 ms, with ON/OFF guidance P99 3.48608/0.91680 ms | FC-064 and Gate 18 gain external-consumer OIT gameplay cadence/resource coverage; D3D12/external evaluation timing remains null outside the D3D11 query domain and no isolated external-cost claim is made; broader legal titles, actual device loss, physical loaded-runtime removal, focus/alt-tab, and monitor move remain unavailable or unproven

#98 2026-09-03 eab47a312 | add opt-in D3D12 backend timestamp queries around the public NGX evaluation, resolve each two-query slot only after its existing submission fence is already complete, retain the originating emulated frame ID, filter aggregate samples to accepted IDs in the bounded report window, and run 1800-warmup/600-sample deterministic Soulcalibur combat through public DLAA plus the supplied external-consumer ON and explicit-policy-OFF routes on normal DX11 and DX11 OIT | public normal/OIT retired 597/598 accepted-window timings with evaluation P50/P95/P99 0.170304/0.190528/0.227136 ms and 0.200960/0.772416/0.929152 ms; supplied ON normal/OIT retired 598 each at 2.653568/2.916768/2.961088 ms and 2.487072/2.896224/3.016384 ms; policy-OFF normal/OIT retired 598 each at 0.171040/0.195104/0.627168 ms and 0.225024/0.290720/0.319744 ms; all six runs accepted 600 evaluations, ON/public presented 600 neural frames, OFF presented 600 native frames with 600 explicitly withheld candidates, and every run had zero missing Presents, identity errors, source/output repeats, native/neural alternations, frame latency, or query-ring pressure, flat Flycast-owned object counts, bounded +131072 to +196608-byte local-VRAM deltas, and clean close; per-frame evaluation fields remained null because retirements are delayed | FC-043/FC-064 and Gate 18 gain nonblocking D3D12-queue evaluation P50/P95/P99 for normal/OIT and the supplied consumer route; ON/OFF spans are reported separately and are not subtracted into an isolated external-cost claim; failed attempts retained outside the repository include an initial target-name error, an unfiltered 122-sample smoke report, the corrected 119/120 frame-filter smoke, and a PowerShell summary parse failure; ordinary rendering creates no timing resources, synchronous evidence remains disabled, and actual device loss, physical loaded-runtime removal, focus/alt-tab, monitor move, and broader legal titles remain unavailable or unproven

#99 2026-09-03 ff2aabd0c | add capture-only D3D11 disjoint/timestamp queries on exactly the frames the bounded writer retains; serialize base-PVR, guidance, accepted evaluation, overlay/presentation-blit, and total-frame spans; enable D3D12 backend timestamps for capture, retire them only by exact emulated frame ID after releasing the D3D11On12 wrapped presentation resource, and bound that developer-only poll to 500 ms; run exact-SHA Soulcalibur gameplay frames 1802-1804 through normal D3D11, normal D3D11On12, and OIT D3D11On12 | all nine packages parsed with `capture_gpu_timings_present=true`, `capture_stalls_gpu=true`, and `eligible_for_performance_metrics=false`; normal D3D11 accepted all three and recorded evaluation 0.179840/0.290784/1.604480 ms; normal and OIT D3D11On12 accepted all six, each reported the first exact-frame retirement unavailable after the bound, then recorded frames 1803/1804 at 1.941856/1.355904 ms and 0.280512/0.273248 ms; every package also carried finite PVR, guidance, composite, and total spans | FC-054 capture-package GPU timing gap advances without contaminating performance telemetry or assigning delayed work to the wrong frame; failed attempts retained outside the repository include COM include-order compilation failure, immediate pre-release D3D12 polling that waited the full 30-second bound and returned null, an initial frame-121 D3D12 result followed by stable exact retirements, and short native-D3D11 automation windows that correctly serialized null evaluation while the existing three-busy-event recovery hold was active; a later 1800-frame window proved normal D3D11 acceptance without weakening recovery; broader-title and external temporal quality coverage remain open

#100 2026-09-03 7a8af6411 | add developer-only `--evidence-presentation marker|restored`, preserving marker as the Gate 10 default while restored presentation snapshots the evaluated D3D12 output, performs unmarked and marked proof readbacks, restores the unmarked image before D3D11On12 sampling, and logs whether the sentinel reaches final presentation; run exact-SHA deterministic Soulcalibur combat through the unchanged supplied external-consumer route and confirm a restored candidate sequence against separate ON-marker and explicit-policy-OFF controls | the fail-closed verifier promoted 60 consecutive frames 1957-2016 with exact five-hash ON and four-input OFF matches, 60 unique external PNGs, zero rejected packages, zero HUD mismatches, 48 valid-history frames, and 12 conservative scene cuts; final candidates had zero marker pixels while the ON proof retained full 1024-pixel swapchain markers and completed Presents; a 60-frame 640x480 temporal animation was retained | FC-054/FC-065 and Gate 17 gain a confirmed moving external gameplay sequence without presenting the sentinel to the candidate; failed attempts retained outside the repository include an evidence range that ended before the candidate interval, 240 nominally aligned clean frames with zero raw-hash matches because ordinary launch timing differed, an invalid capture taken inside the marker run whose output hash was itself marked, unavailable ffmpeg, an interrupted slow lossless-WebP encode, and a numeric-frame side-by-side rejected and removed after exact-hash joining proved it compared different game states; Gate 17 remains partial because one title and one supplied external setting do not establish the representative matrix or a winner

#101 2026-09-03 2df3619e2 | refactor D3D11On12 output wrapping and, only for bounded restored synchronous quality capture under explicit experimental policy OFF, retain the accepted public D3D12 output as a reference while withholding it from presentation; pair a fresh 30-frame public-DLAA reference with exact-SHA confirmed external frames by exact color/depth/motion/mask hashes and generate a same-state moving comparison | all 30 public outputs were retained, every policy-OFF final composite was byte-identical to native PVR, every public returned hash matched the proof's OFF hash and differed from the external output, and the public comparison index accepted 30 packages with zero rejects; external versus public raw temporal delta was 5.852924 versus 6.098338, but external/public source PSNR was 24.391432/30.219362, gradient MAE 4.404373/3.637432, edge recall 89.370230%/92.029170%, color drift 3.547196/0.233438, and saturation drift 11.999162/3.334620 | FC-044/FC-054/FC-065 and Gates 16/17 gain an exact-input temporal comparison, but the supplied external setting is not promoted as the Faithful winner: its roughly four-percent lower raw temporal delta does not offset the measured loss of source identity, so public Auto remains the safer baseline; failed attempts retained include two public-DLAA timing lanes with zero exact-input pairs, an invalid numeric-frame comparison, a malformed PowerShell launch that started no process, a corrected Python-metrics syntax failure, a mixed-SHA append log rejected before a fresh OFF control was captured, and a post-commit Ninja validation initially invoked the Visual Studio-only `/m` option before being corrected; broader legal titles and an evidence-winning external setting remain unavailable

#102 2026-09-03 83737a303 | recover and inspect the interrupted post-restoration normal/OIT D3D11On12 public-DLAA performance runs, each using deterministic Soulcalibur combat after 1800 warmup frames with 600 measured frames and all synchronous evidence disabled | both completion and launch reports record 600 accepted evaluations, 600 neural Presents, zero missing or withheld frames, identity errors, source/output repeats or gaps, alternation, latency, or query-ring pressure, and clean close; each holds exactly 127 Flycast-owned neural objects (115 renderer plus 12 backend, including two optional asynchronous timing resources) with zero growth and local VRAM growth of 131072 bytes; normal/OIT evaluation sample counts are 597/598 with P50/P95/P99 0.171904/0.187808/0.204928 ms and 0.255552/0.767968/0.951808 ms; whole-run reset counts are retained as 245/229 | FC-064 confirms the restored developer capture path does not add resources or synchronous waits to these measured public lanes; this is a bounded regression, not a new long-run test or full Gate-18 acceptance

#103 2026-09-03 83737a303 plus working tree | add a read-only bounded compare-captures command with strict schema-3 JSON, unique four-input matching, fixed identity/geometry, consecutive chronology, decoded/raw artifact hash verification, actual paired-input byte equality, external confirmation validation, and public-reference equality to the external OFF proof; retain component RGB/alpha and raw temporal metrics without implicit resampling or an automatic winner; run the three-build matrix, both expanded selftests, and the retained exact-SHA Soulcalibur external/public sequence | both selftests pass 125/125, including actual depth-file and output-PNG mutation rejections with no report, existing-report preservation, duplicate JSON/hash ambiguity, frame-gap/order/build and within-lane preset mismatch, and invalid external proof controls; all 30 real frame pairs verify all five saved buffer hashes and byte-identical inputs; mean peer PSNR is 26.307988 dB, external/public source PSNR 24.391432/30.219362 dB, source RGB MAE 9.245484/2.761471, and raw temporal RGB MAE 5.852924/6.098338 | FC-054/FC-065 makes the earlier ad-hoc comparison reproducible in the harness; lower raw temporal delta includes motion and cuts and must not be called a stability improvement on its own; expected falsifying attempts are rejected before report writes, synthetic scratch files are removed only from the test-owned temporary directory, existing game captures are unchanged, and no production renderer or third-party component/configuration is changed; Gates 16-18 remain partial

#104 2026-09-03 5b702f5d2 plus working tree | prioritize bounded Soulcalibur HUD coverage before external-setting comparisons; retain the empty-mask diagnostic, add capture-only per-draw evidence and nonempty/comparable protected-pixel metrics, correct sorted translucent guidance to follow actual SortedTriangle index ranges/topology with explicit submission ordinals, retain indexed OIT/per-strip paths and reactive-only translucent motion | initial diagnostic-before frames 1958-1960 contained zero protected pixels, falsifying real-HUD preservation inferred from zero mismatches; the sorted fix records the actual health-bar/name/counter geometry and normal DX11 now protects 1560/3264/3264 name-quad pixels with zero byte mismatches and zero mask pixels below y=100 on both D3D11 and D3D11On12; before/after native-color pixels and raw depth bytes are identical for all three frames, and native/depth/overlay buffers match across APIs per renderer; OIT still has zero protected pixels on both APIs, correctly reported unverified; all four public-DLAA replay captures close cleanly; three builds plus automation build pass, both selftests pass 130/130, and explicit overlay/transparency fixtures on both APIs retain 33/33 exact protected pixels, zero changed world pixels, and failing omitted-pass controls | FC-054/FC-055 advance but FC-055 and real-title Gates 15A/15B are explicitly reopened: batched and repeated HUD elements, coordinate-space scaling, OIT classification, complete moving HUD coverage, and external post-fix image quality remain pending; retained failed attempts include the initial ArrayView operator[] test compile error, corrected to .data before the expected RED result of 126 passes/2 failures, followed by GREEN 128/128 and expanded 130/130; missing optional AGENTS/log lookups, literal-wildcard rg diagnostics, an unavailable dependency-tool handler, and rejected patch contexts yielded no acceptance evidence; initial provisional hud_preservation_verified metadata is documented as selected-pixel-only and renamed hud_protected_pixels_verified; Gate 10 and external components/configuration are unchanged; raw local evidence remains outside Git under gate15b-soulcalibur-hud-5b702f5d2

#105 2026-09-03 dea7cb94f plus working tree | finish the bounded Soulcalibur HUD-first slice before external-setting comparison: replace bounding-box guesses with strip/triangle-list quad proof, use native PVR dimensions under high-resolution raster scaling, exclude empty sorted placeholders from texture occurrence counts, separate overlay continuity from motion identity, add an exact visible `T1401N` HUD profile and bounded one-to-one depth pairing for duplicated name layers, and expose profile/native-screen/primitive evidence in capture metadata | four three-frame public-DLAA captures across normal/OIT and native D3D11/D3D11On12 closed cleanly; normal frames 1959-1960 protected 20369/20380 pixels including both duplicated character-name layers, OIT protected 19566/19560/19548 pixels, every protected byte matched native, all classified bounds ended at y=86, and per-renderer native/depth/mask bytes matched across APIs; two 30-frame D3D11On12 moving sequences covered frames 1802-1831 with normal/OIT protected ranges 16229-19216 and 17474-20461, names present in all 60 frames, zero mismatch sum, and zero mask pixels at or below y=100; NGX, no-NGX, feature-off, and automation executables linked, both selftests passed 138/138, and overlay/transparency fixtures on both APIs retained 33/33 exact protected pixels, zero changed world pixels, and failing omitted controls | FC-055 and Gate 15B are green for this bounded Soulcalibur normal/OIT HUD defect but remain doing/partial for representative titles and uncertain-overlay negatives; retained falsifying attempts include the initial missing-field RED compile, an unsupported `--help` capture invocation, a concurrent locked-executable LNK1104, a missing declaration include, the first OIT profile capture leaving health bars at stability one, the first normal-profile capture omitting duplicated names, and the resulting one-to-one depth-pair correction; malformed shell quoting and an invalid inline PowerShell conditional produced no evidence; a test initially inspected the post-accept swapped buffer and reported stability two until the assertion was moved to the captured buffer; Gate 10, external binaries/configuration, native fallback, and user media remain unchanged, and all raw evidence stays outside Git under gate15b-soulcalibur-hud-batching-dea7cb94f

#106 2026-09-03 cb653b613 plus working tree | bind actual external-consumer settings to capture confirmation instead of inferring them from Flycast recommendations: parse the complete consumer-reported active tuple with classic-locale finite numbers, require one stable enabled tuple across the ON host log, attach typed provenance to promoted manifests and the confirmation report, validate within-lane stability, preserve old unverified captures, and expose verified settings separately in compare-captures; rebuild NGX, no-NGX, feature-off, and automation trees, run both selftests, replay retained exact production confirmation/comparison evidence, and attempt a fresh post-HUD Soulcalibur route without changing consumer configuration | both selftests passed 146/146, including missing/conflicting/disabled/malformed settings controls; an isolated copy of the retained 30-frame exact-input production sequence reconfirmed 30/30 from its archived ON/OFF logs and a new 30-pair byte/hash-verified report recorded upscaling off, intensity 1, global tone 1, diffuse white 203 nits, preset 0, style 0, enabled on, and `actual_external_settings_verified=true`; the original comparison still passes 30/30 and correctly remains unverified for backward compatibility; fresh current-SHA restored and ON-marker runs each completed 30 frames and the host reported the same tuple, but the existing safe-mode OFF staging route timed out at 180 seconds with missing consumer components and no current-schema exact-input evidence, so the fresh candidate remains unpromoted and no post-HUD comparison or setting winner is claimed | FC-044/FC-054/FC-065 advance by making future settings comparisons attributable and falsifiable; failed attempts retained include one malformed MSVC command quote, the bounded policy-OFF timeout, and a conflicting-settings confirmation returning 1 before any promotion; external binaries were inspected only by file hash, existing text configuration was inspected but not changed, generated, or staged, Flycast did not map selector numbers or write third-party settings, synchronous evidence remains performance-ineligible, and raw evidence stays outside Git under gate16-settings-proof-reconfirmation-2df3619e2 and gate16-external-settings-cb653b613-wt

#107 2026-09-03 c025a69b0 harness with cb653b613 capture binary | resolve the fresh settings-attributed Soulcalibur comparison after LOG #106: prove the safe-mode capture-only path at frame zero, preserve and reject a mixed-SHA append log, rerun late ON-marker and policy-OFF controls with a 2000-2239 evidence window, confirm the existing restored candidate, and exact-input compare the promoted external output to the retained public result | the start-zero OFF smoke completed 2/2 on committed SHA; the wide cb653b613 ON and OFF runs each completed 30 frames cleanly, ON covered through frame 2135 and fresh OFF covered frames 2000-2136, and confirmation promoted 30/30 restored candidates with settings proof upscaling off, intensity 1, global tone 1, diffuse white 203 nits, preset 0, style 0, enabled on; compare-captures verified 30 unique pairs, every saved hash, and byte-identical color/depth/motion/mask inputs; external/public raw temporal RGB MAE was 2.752928/2.774712, source PSNR 25.677898/38.543206 dB, source RGB MAE 10.256009/0.932040, trail 11.791142/1.449482, edge displacement 1.473169/0.604473, thin-line continuity 54.349064/83.946814, color drift 2.321686/0.620147, saturation drift 8.476401/1.507168, and black drift 5.392439/1.262206, with zero repeats or drops | the verified consumer setting remains rejected as the Faithful winner: its 0.8% lower raw frame delta does not offset large source and component losses; this sequence contained zero protected HUD pixels, so LOG #105 remains the HUD proof and no post-composite HUD claim is made here; a mixed c025a69b0/cb653b613 OFF append log was correctly rejected before promotion and preserved, then a fresh single-SHA log passed; no third-party setting or binary was modified, synchronous evidence is performance-ineligible, broader legal titles and a controlled lower-intensity/tone comparison remain unavailable, and raw evidence stays outside Git under gate16-external-settings-cb653b613-wt

#108 2026-09-03 3bdf2ae91 plus working tree | complete FC-050 with a mutex-protected render-thread-to-UI status snapshot, readable live mode/API/reason/raster/frame/counter/timing/route/overlay/bypass fields, and a real seven-surface developer selector over the production guidance ring; suppress neural Present accounting whenever a debug surface replaces the PVR blit; compile every visualization variant from the production HLSL source; rebuild NGX, no-NGX, feature-off, and automation configurations; run both selftests; launch deterministic Soulcalibur through the motion view | all four executables linked and NGX/no-NGX selftests passed 149/149; the production shader contract compiled source/depth/motion/bias/confidence/draw-ID/overlay variants; the live T1401N replay opened, established a 640x480 guidance contract, transitioned `view=motion` from unavailable to active on the first fresh guidance frame, and logged `history_effect=none presentation_accounting=native`; no shader compilation failure was logged, and the bounded smoke was harness-terminated after the activation marker rather than treated as a clean-close or image-quality test | FC-050 is complete at the settings/status/debug-contract level; visualization is off by default, cannot display stale guidance after bypass/export failure, does not write external configuration, does not advance accepted history, and cannot be counted as neural presentation evidence; the first automatic CMake regeneration outside the Visual Studio x64 environment failed at architecture detection before compilation and was superseded by explicit `VsDevCmd -arch=x64 -host_arch=x64` configure/build passes

#109 2026-09-03 d1adcc874 plus working tree | add an opt-in exact production capture immediately after neural/game-HUD composition and a second capture after `gui_display_osd` submits ImGui draw data; make completion wait for both boundaries; require a real content-rectangle delta; run deterministic Soulcalibur frames 182-183 on normal DX11 and DX11 OIT across native D3D11 and D3D11On12; run a no-OSD negative; rebuild NGX, no-NGX, feature-off, and automation configurations and run both selftests | all eight positive frames closed cleanly and recorded 1824 or 2016 changed content pixels with maximum channel delta 182; the difference images contain only the visible `F:90.0`-style Flycast FPS OSD while `final-composited.png` and public output were captured earlier; the no-OSD control recorded zero changed pixels/max delta and the launcher returned 1; NGX and no-NGX selftests remained 149/149 | FC-051 and pixel Gate 8 are green for both production renderers and surfaces; the developer proof is off by default, synchronous, and performance-ineligible; runtime mode toggles and the rest of FC-061 remain open; retained failures include an initial malformed developer-shell quote before compilation, the first OIT run writing both pre-OSD packages but no late package because its separate render loop lacked the new call then ending at the automation timeout, and an invalid PowerShell empty-pipe summary expression before the corrected report

#110 2026-09-03 b3849876f plus working tree | add a hidden exact-main-frame live neural-mode off/on round trip; keep asynchronous performance sampling continuous across the production resource/stage/history retirement path; record active mode and accepted reset state per sample; strictly require native zero-evaluation fallback while off, reset on the first accepted re-entry, bounded requested-mode fallback, and clean cadence; run deterministic Soulcalibur `T1401N` with replay FNV64 `0A96F4E2FB52C75C` for 600 measured samples on normal DX11 and DX11 OIT across native D3D11 and D3D11On12 | every marker records mode `2 -> 0 -> 2` at exact main frames 300 and 360 without renderer or sampler restart; all four cases contain exactly 60 off-mode samples, 60 native off-mode Presents, zero off-mode evaluations, two mode transitions, a reset on the first accepted re-entry, zero missing/accepted-but-unpresented/identity/repeat/latency/query-ring faults, zero Flycast-owned object growth, and clean close; both On12 cases present 540/540 requested-mode neural outputs, while native-D3D11 normal/OIT each expose one conservative requested-mode native fallback and 539 neural outputs; NGX, no-NGX, feature-off, and automation executables link and both selftests pass 149/149 | FC-059 is done and FC-063/FC-064/Gate 18 advance with real runtime mode lifecycle evidence; native fallback remains the safe policy and no external configuration is changed; invalid native-lane, orphan-duration, and overlapping-transition controls return 2; the first real run passed the production invariants but was retained as a failed launcher attempt because an exact-two presentation-alternation assertion incorrectly conflated one explicit nonblocking busy fallback with the commanded mode boundaries, leading to live-mode and first-reentry-reset telemetry rather than hiding the fallback

#111 2026-09-03 cca4de4de plus working tree | close FC-021/FC-022 Gate 1 with a default-off synchronous BGRA8 production PVR capture compiled identically into neural-capable and compile-time feature-off DX11 builds; capture after the final normal/OIT PVR resolve and before neural submission, overlays, OSD, or presentation; make the neural input layout lazy; add a strict `neuraltest native-parity` launcher with exact replay retention, raw byte/hash pairing, surface and zero-activity diagnostics, and a material wrong-frame control; run Soulcalibur `T1401N` replay FNV64 `0A96F4E2FB52C75C` at 1440x1080 for both renderers on native D3D11 and enabled-build D3D11On12 versus feature-off native D3D11; run all 14 synthetic fixtures under both renderer labels for five runs; rebuild NGX, no-NGX, and feature-off targets and run both selftests | all four production cases pass 5/5 exact frame pairs; enabled mode zero reports instrumentation disabled, zero draw records/previous positions, no neural input layout, no export resources, zero guidance replays, and zero backend objects; D3D11On12 markers prove the requested surface was active only in the enabled build; wrong-frame controls differ in 569830/1555200 normal pixels and 569896/1555200 OIT pixels while exact-pair tolerance is zero; all 28 synthetic determinism cases pass five runs; NGX/no-NGX selftests pass 149/149 and a two-frame production passthrough capture confirms lazy layout creation does not break enabled rendering | FC-021 and FC-022 are done and FC-061 advances; the feature-off binary cannot instantiate D3D11On12 by construction, so the cross-surface comparison intentionally uses enabled/mode-off On12 against the compile-time feature-off native-D3D11 baseline and records both actual surfaces; the first manual launcher attempt lost argument boundaries and wrote no parity artifacts, the first valid capture exposed locale-grouped invalid JSON and was superseded by classic-locale output, and the first zero-activity harness run rejected valid markers because its text-reader required `eof()` after iterator reads; all failed attempts are retained under ignored build evidence; capture is developer-only, performance-ineligible, and stages no media or user path

#112 2026-09-03 5bb99bd28 plus working tree | add strict `neuraltest production-scaling`; run exact replayed Soulcalibur frame 30 at 640x480, 2560x1920, and 5120x3840 through normal/OIT production DX11 on native D3D11 and D3D11On12; verify actual renderer/surface, frame/dimension/Git identity, retain raw hashes, compare every 4x/8x pixel to exact nearest enlargement, isolate source-edge blocks, require subpixel diversity, and execute the zero-difference nearest negative through the same predicate | normal 4x/8x differs at 713193/2842727 pixels including 160244/639317 edge samples and 14888/15020 diverse edge blocks; OIT differs at 714179/2842153 pixels including 160653/642093 edge samples and 14959/15083 diverse blocks; native D3D11 and D3D11On12 statistics and hashes are exact per renderer; all four runs close cleanly, the nearest control is rejected, and completed-output reuse/missing-input/zero-frame controls return 2/3/2 | FC-012 and FC-023/Gate 2 are done and FC-061 advances; the initial compile outside the VS x64 environment failed before code compilation because standard-library include paths were absent, the first positive run preceded final marker hardening and was superseded, and an empty-pipe PowerShell summary parse failed before reading evidence then was corrected; all raw media/captures remain ignored outside Git and the synchronous proof is performance-ineligible

#113 2026-09-03 56bbee8f7 | refresh legacy Gates 4-7 on the committed harness: run the GPU motion contract, native-D3D11 and D3D11On12 depth controls, two-frame production native/passthrough Soulcalibur capture on OIT D3D11On12, and exact shared-stage passthrough on current textured-checker packages rendered under both DX11 labels; audit the original thresholds against current unit and production evidence | static motion is exactly `[0,0]`, +4X is `[-4,0]`, correct reprojection MAE is 0 while reversed/doubled are 47.8868056/37.318971, both depth surfaces again return clear/far/near/PT `0/0.166836038/0.263784289/0.263784289` with the wrong control failed, both renderer-label passthrough submissions report byte-identical color, and production mode 1 accepts both frames without falsely labeling public or external output | legacy Gates 4, 6, and 7 are green; normal Dreamcast portions of Gate 5 are green, including the retained >=90 percent edge-clip and reactive/untrusted controls, but Naomi 2 remains deliberately zero-validity because only CPU column-major projection is proven and accepted previous matrices do not yet reach production HLSL; FC-061 therefore remains doing; the first passthrough command pointed at the run parent rather than its frame package, failed before submission, and was corrected with the exact frame directory

#114 2026-09-03 693e5dafd plus working tree | add a bounded `focus-roundtrip` production-performance transition that establishes the launched Flycast window as foreground, creates a real visible top-level control window, positively observes foreground focus leave Flycast, restores and observes focus on the same visible non-minimized Flycast window, retains all six request/observation results, and destroys the helper on every exit; run deterministic Soulcalibur `T1401N` public DLAA for 600 measured samples on normal DX11 and DX11 OIT across native D3D11 and D3D11On12 | all four focus cycles pass and all 2400 accepted evaluations become 2400 neural Presents with zero missing or accepted-but-unpresented frames, identity mismatch, source/output repeat, native/neural alternation, frame latency, query-ring pressure, or Flycast-owned object growth; every case retains one source-frame gap and explicit reset at the focus boundary and closes cleanly; the unknown-transition control returns 2 and a five-second-step transition under a one-second deadline times out with exit 1 | FC-045/FC-063/FC-064 and Gate 18 gain observed focus-loss/restore lifecycle coverage on all four normal-Dreamcast production paths; this proves the foreground ownership boundary relevant to Alt+Tab without claiming a synthesized keyboard gesture, cross-monitor movement, external-consumer timing, actual device loss, physical runtime removal, or broader-title stability; a first ten-sample negative attempt remained alive long enough to complete all five half-second steps and correctly passed, so it was retained and replaced by the stricter deadline control

#115 2026-09-03 9c3079952 plus working tree | add a hidden performance-only `seh-exception` control inside the production D3D11 and D3D12 NGX evaluate leaves; raise application-defined Windows software exception `0xE0424E47` before the real runtime call, retain the filtered exception code in asynchronous performance telemetry, require explicit native fallback plus recovered neural output and clean cadence, and run three post-warmup exceptions through deterministic Soulcalibur `T1401N` on native-D3D11 normal and D3D11On12 OIT public DLAA | both 600-sample runs record three evaluation failures, exact exception `0xE0424E47`, one bounded hold, 62 native Presents, 538 recovered neural Presents, one source gap, and two explicit native/neural transitions; both have zero missing or accepted-but-unpresented frames, identity mismatch, source/output repeat, frame latency, query-ring pressure, or Flycast-owned object growth and close cleanly; selecting the SEH control with injection count zero completes 30 frames but fails acceptance with exit 1 | FC-063 and Gate 18 gain Flycast-owned production SEH containment and recovery evidence without modifying or faulting third-party code; the first attempt deliberately raised an access violation, Flycast's global fault handler correctly intercepted it before the local test filter and terminated the process, so that failed run is retained and the stimulus was corrected to an application-defined exception; actual device loss, physical runtime removal, cross-title coverage, and broader-title stability remain open

#116 2026-09-03 f4b5189f2 plus working tree | add a hidden exact-main-frame control that calls `ID3D12Device5::RemoveDevice` on Flycast's live D3D11On12 backing device, requires exact removed reason `DXGI_ERROR_DEVICE_REMOVED` (`0x887A0005`), rebuilds the renderer, and accepts only a fresh post-recovery performance interval; run deterministic Soulcalibur `T1401N` public DLAA after removal at main frame 300 through normal DX11 and DX11 OIT on D3D11On12; run native-D3D11 and overlapping-injection negatives | both markers record removal requested/observed, exact reason `0x887A0005`, successful renderer initialization, and restored D3D11On12; each fresh 600-sample interval accepts 600 evaluations and presents 600 neural frames with zero native, missing, accepted-but-unpresented, identity, source-repeat/gap, output-repeat, alternation, latency, query-ring-pressure, or Flycast-owned object-growth counts and closes cleanly; native D3D11 and simultaneous evaluate-injection requests return 2 without launching | FC-043/FC-045/FC-063/FC-064 and Gate 18 gain controlled actual D3D12 device-removal/reconstruction evidence for both production DX11 renderers; this is not spontaneous driver/TDR or physical runtime-removal evidence; the first functionally clean normal-DX11 run was rejected because ambient locale formatting wrote `0x88,7A0,005` in the marker, so marker output was corrected to the classic locale and both cases were rerun successfully

#117 2026-09-03 972895d2d plus working tree | retain per-draw Naomi 2 model-view/projection matrices in the same accepted double buffer as draw/topology/vertex history; bind accepted object-space XYZ plus accepted matrices to the production Naomi 2 neural vertex permutation; require exact topology and reject reindexed or missing-matrix history; extend the production HLSL fixture and CPU correspondence controls; rebuild the committed source in NGX, no-NGX, neural-feature-off, and automation configurations and rerun both expanded selftests | the production shader rasterizes analytic `[-4,+3]` render-pixel motion with mask 0 and confidence 255 on both native D3D11 and D3D11On12, and both surfaces are exact; clearing accepted-matrix validity produces zero motion/confidence and mask 255; the instrumentation control retains the accepted identity matrix while the current model-view translates, validates all three prior vertices, and rejects a reordered index stream with zero confidence; all four executables link and both NGX and no-NGX selftests pass 153/153 | FC-031/FC-032/FC-036 and legacy Gate 5 advance from conservative Naomi 2 fallback to exact-topology accepted-history motion without trusting reindexed geometry; legal Naomi 2 game-media validation remains unavailable and production raster jitter remains open for quality Gate 12; the first post-commit configure was invoked outside the MSVC architecture environment and stopped at CMake architecture detection before generation, then the established x64 developer environment was restored and the complete matrix passed

#118 2026-09-03 cb653b613 capture binary plus a95c5ef78 working tree | follow the authorized Gate 16/17 course correction: back up the supplied external text configuration, sweep conservative consumer-reported settings on the deterministic 30-frame Soulcalibur Hoko Temple interval, require restored-output plus marker captures and exact policy-OFF provenance for every accepted tuple, compare exact color/depth/motion/mask inputs to public DLAA Auto, generate a comparison index and moving triptych, run an intensity-zero negative, then restore the original configuration byte-for-byte | the original 1874-byte `reshade.ini` hash `222D059C727A683C8DEACE07C938002BDD6A69EB0DC9C908743E17A92B54EFBC` was restored exactly; seven nonzero tuples passed 30/30 settings/provenance confirmation. Intensity 0.5 improved substantially over 1 but still recorded trail/edge/thin/color/saturation/black `6.32/1.02/70.22/1.41/4.86/3.22` versus public `1.45/0.60/83.95/0.62/1.51/1.26`; intensity 0.25 Default improved to `3.64/0.80/77.49/0.99/3.15/2.15`; Natural style at 0.25 reached `2.99/0.69/81.47/1.10/2.67/1.86`; and the least damaging nonzero 0.125/Natural/preset-2 tuple reached `2.08/0.65/82.69/0.82/2.11/1.39` with raw temporal RGB MAE `2.7836` versus public `2.7747`. Tone 0 and tone 1 were byte-identical at intensities 1 and 0.5, while external preset 2 and preset 0 were byte-identical over all 30 exact-input frames with upscaling off. The 30-frame moving review shows the 0.125 effect is barely visible, slightly softens hair/face detail, and adds no clear material benefit. Intensity 0 correctly failed confirmation because no exact-input policy-OFF difference could prove external output, and was retained as a rejected negative rather than a winner | no external tuple beats public Auto for Faithful on this title, so outcome B applies provisionally and public Auto remains the Faithful baseline. Web guidance was used only to choose hypotheses: NVIDIA states tone zero preserves engine color, while current community reports favor roughly 0.5-0.7 intensity and Natural style; local evidence overruled those suggestions. Local-tone/structure keys were not swept because this host log does not positively report them in the accepted tuple. Failed evidence is retained, including wrong-executable SHA confirmations, malformed PowerShell summary pipes, and the intensity-zero fail-closed rejection. All raw captures, configuration snapshots, hashes, logs, the 576-package index, and `intensity0125-moving-comparison.gif` remain outside Git under `flycast-evidence/gate16-settings-sweep-20260903`; no third-party binary was inspected, changed, staged, or redistributed, and no proprietary media was acquired

#119 2026-09-03 cb653b613 capture binary plus a95c5ef78 working tree | answer the user-requested visibly transformative Photoreal experiment with consumer-reported intensity 1.5, global tone 1.5, preset 2, Default style, upscaling off, and enabled on; run matching restored and marker captures under the Flycast Photoreal Experimental/realistic labels; confirm 30/30 fail-closed external provenance; exact-input compare to public Auto and to the accepted intensity-1 Default-style sequence; generate and expose a synchronized source/Photoreal/public moving comparison plus full-resolution frames; restore the supplied text configuration | all 30 frames confirm the requested tuple and exact color/depth/motion/mask inputs. The Photoreal result visibly changes facial detail, skin lighting, hair mass, and color, while metrics intentionally leave the Faithful envelope: source RGB MAE 10.256, source PSNR 25.678 dB, trail 11.791, edge displacement 1.473, thin-line continuity 54.349, color drift 2.322, saturation drift 8.476, and black drift 5.392, with zero repeats/drops. Exact comparison against intensity 1/tone 0/preset 0 Default style returns 30/30 byte-identical output frames and mean pair MAE 0, proving intensity/tone 1.5 and preset 2 do not make this consumer path stronger than its intensity-1 result. The original 1874-byte configuration was restored byte-for-byte to SHA-256 `222D059C727A683C8DEACE07C938002BDD6A69EB0DC9C908743E17A92B54EFBC` | the output is valid only as clearly labeled Photoreal Experimental evidence and is never promoted to Faithful. Cinematic style 2 was not attempted because the installed feeder documentation warns it can crash at next boot. Raw evidence and `photoreal-i15-moving-comparison.gif` remain outside Git under `flycast-evidence/gate16-settings-sweep-20260903`; no third-party binary was inspected, changed, staged, or redistributed

#120 2026-09-03 cb653b613 capture binary plus a95c5ef78 working tree; external control companion 5043a22 | run the user-requested uncanny configuration on the exact deterministic Soulcalibur interval: Cinematic style 2, intensity 2, local structure 2, global/local tone 0.75, automatic mask off, UI correction on, upscaling off, and Photoreal Experimental/realistic/max-coverage Flycast labels; retain matched source/public/external moving evidence; isolate Cinematic saturation and the unreported structure field; build and selftest a source-controlled host companion that applies only explicit text settings with exact backup and requested-state logging | both Cinematic intensity 1/tone 1 and intensity 2/tone 2 complete 30/30 confirmed external captures and are byte-identical, so Cinematic's reported overall intensity also saturates at 1 on this interval. The requested full tuple completes 30/30 exact-input frames. Against public Auto it records source MAE 6.886, PSNR 28.066 dB, trail 8.076, edge displacement 1.010, thin-line continuity 70.325, color drift 2.364, saturation drift 7.462, black drift 4.941, and temporal RGB MAE 3.0059 versus 2.7747. An isolation control holds Cinematic/intensity-2/global-tone-0.75 constant and adds only `NRLocalStructure=2.0`; all 30 exact-input outputs change with mean RGB MAE 1.401243 and maximum channel delta 36. Because the consumer log omits local structure/tone/mask, those fields are requested plus isolated-output-proven, not consumer-reported. The original 1874-byte configuration is again restored byte-for-byte to SHA-256 `222D059C727A683C8DEACE07C938002BDD6A69EB0DC9C908743E17A92B54EFBC`. The separate DLSS5-Feeder checkout on branch `feat/flycast-neural-controls` commits `5043a22 Add Flycast neural control companion`; its selftest passes, omission of `--apply` returns 2 without changing the file, explicit apply creates an exact backup, and `--show` confirms the restored active config has no overrides | Structure 200 is functional and visibly stronger, but the result is deliberately uncanny/Photoreal Experimental and does not beat public Auto for Faithful. The host companion remains outside Flycast's render path because Feeder's image-derived contract must not replace the emulator-native D3D11On12 contract. It reports requested settings and `consumer-active-state-unverified` until the existing capture/log proof confirms them; it does not load, inspect, implement, patch, or redistribute proprietary Neural Rendering. Raw evidence and `cinematic-structure2-maxcoverage-moving-comparison.gif` remain outside Git under `flycast-evidence/gate16-settings-sweep-20260903`

#121 2026-09-03 a95c5ef78 plus working tree | promote the proven uncanny experiment from a one-off label to a first-class Flycast profile without changing the external consumer: add `UncannyCinematic` as quality-profile value 3, expose it in Video settings, capture it through `--profile uncanny`, retain the factory default at Faithful, and recommend Cinematic model, Structure 200 percent, Tone 75 percent, and maximum scene coverage as explicitly non-faithful/user-controlled metadata | automation, NGX baseline, no-NGX, and compile-time feature-off builds link; automation, NGX baseline, and no-NGX selftests each pass 156/156; a deterministic one-frame Soulcalibur native-D3D11 smoke closes cleanly and records `Uncanny Cinematic / Realistic 3D`, the exact recommendation, 640x480 render/output/content dimensions, and accepted evaluation. No external configuration is written, and the separately restored consumer config remains unchanged | Uncanny is now eligible as a persistent user-chosen default but is not the factory/automatic default. Changing the shipped default remains Gate 17 evidence-gated. The first build command incorrectly forwarded MSBuild `/m` to Ninja and was corrected before compilation; a second malformed `cmd` quoting attempt also failed before compilation. Smoke artifacts remain ignored under `build-neural-automation/uncanny-profile-smoke-20260903`

#122 2026-09-03 a95c5ef78 plus working tree | complete FC-035 on content-bearing production frames: build the retained-base normal-DX11 scene replay, capture deterministic Soulcalibur frames 1804-1806 with active public DLAA and a matching native control, compare neural source and native PVR bytes, inject one evaluation failure on accepted-count 1802 so it lands inside the capture, repeat with automatic overlay policy, retain an OIT control, build automation/NGX/no-NGX/feature-off, and run all available selftests/production GPU fixtures | active frames report Halton `[-0.375,-0.0555556]`, `[0.125,0.277778]`, and `[-0.125,-0.277778]`, `raster_jitter_applied=true`, reason `separate-neural-scene-replay`, accepted evaluation, and exact 640x480 content. Their neural sources differ from the unjittered control in 239575/251162/252298 pixels while every corresponding `native-pvr-color.png` is byte-identical. The active failure lands on frame 1805, records accepted false and `injected D3D11 evaluate failure (1/1)`, writes no public output, and makes final composite exactly equal both its native PVR image and the separate native control. Automatic overlay classification records 19012/19122/16264 protected pixels with zero mismatch and conservatively reports `protected-overlay-present` plus zero jitter. OIT accepts all three evaluations while reporting zero jitter and `oit-scene-replay-pending`. All four builds link; automation, NGX, and no-NGX selftests pass 156/156, including exact production coverage-shift/zero-motion fixtures for standard PVR and Naomi 2 on both D3D11 surfaces | FC-032, FC-035, FC-036, and quality Gate 12 are green for normal Dreamcast plus exact-topology Naomi 2 shader evidence. OIT remains intentionally unjittered pending its own scene replay, not falsely claimed as active. The first failure capture injected at startup rather than the retained interval and was superseded by `--inject-after 1802`; an initial PowerShell summary contained an empty pipe and was corrected. Post-jitter Gate 16/17 candidate recapture is now the critical path; raw captures remain ignored under `build-neural-automation/jitter-retained-base-*`

#123 2026-09-03 2a4258cf2 plus working tree | enable the normal-DX11 public-DLAA scene-replay jitter contract for DLSS5 experimental, rebuild exact-SHA automation/NGX/no-NGX/feature-off, then capture policy-off and conservative intensity-0.125/Natural external lanes over the deterministic Soulcalibur interval | the first external attempt was rejected because its executable manifest still named stale SHA `a95c5ef78`; the reconfigured exact-SHA lanes each accepted 30 jittered frames, and candidate/marker inputs matched 30/30, but candidate versus policy-off matched zero complete contracts even though native scene images matched 30/30 at a one-frame offset. Absolute-frame Halton indexing had allowed external host startup timing to shift the jitter phase. Replacing that index with the accepted-evaluation count, reset on discontinuity and advanced only by `MarkEvaluated`, raises selftests from 156 to 159 and makes a focused consumer-on/policy-off replay match 3/3 color/depth/motion/mask contracts with identical phases `[-0.4375,0.388889]`, `[0,-0.166667]`, and `[-0.25,0.166667]`. Automation, NGX, no-NGX, and feature-off builds link; all three available selftests pass 159/159. The supplied consumer text config was restored byte-for-byte after every attempt at 1874 bytes and SHA-256 `222D059C727A683C8DEACE07C938002BDD6A69EB0DC9C908743E17A92B54EFBC` | accepted-history-indexed jitter is the production and comparison contract. Stale-build and absolute-frame evidence are retained but rejected. Full exact-commit 30-frame conservative and Uncanny comparisons remain the immediate Gate 16/17 task

#124 2026-09-04 c11ba8312 | rebuild automation, NGX, no-NGX, and feature-off from the exact accepted-history-jitter commit; run all three available 159-check selftests; stage only that executable beside the unchanged user-supplied components; then capture exact candidate/marker/policy-off sequences for conservative intensity-0.125/Natural, Uncanny Cinematic/Structure-200/Tone-75 maximum coverage, and Uncanny with automatic HUD protection over deterministic Soulcalibur combat | all builds link and automation/NGX/no-NGX selftests pass 159/159. Each external lane passes 30/30 fail-closed settings, returned-output, marker-presentation, policy-off, and exact color/depth/motion/mask provenance. Conservative reduces raw temporal RGB MAE from public Auto 5.531692 to 5.437669, but raises source MAE 3.383300 to 3.897791, trail 6.025232 to 6.623554, edge displacement 0.817360 to 0.832689, saturation drift 3.893033 to 4.467180, and black drift 2.770840 to 2.909819 while lowering thin-line continuity 77.870859 to 77.405134. Maximum-coverage Uncanny lowers raw temporal MAE to 5.392928 but moves far outside Faithful: source MAE 10.499910, trail 13.254495, edge 1.191709, thin-line 64.874065, color drift 1.697859, saturation drift 12.646120, and black drift 8.796212. The automatic-HUD Uncanny lane protects an average 15312.97 pixels per frame with zero mismatches/repeats/drops; 29/30 frames conservatively use zero jitter for `protected-overlay-present`, while one overlay-free frame uses scene-replay jitter. Its paired public/Uncanny raw temporal MAE is 5.318013/5.316574, but Uncanny again raises trail 4.737947 to 12.775519, edge 0.632667 to 1.056560, saturation 2.959955 to 12.431682, and black drift 2.256323 to 8.646104 while lowering thin-line continuity 83.416368 to 69.260848. Three synchronized 30-frame comparisons were generated and visually inspected. The first confirmation command correctly failed closed because logs had not yet been archived from the stage; images were retained, marker logs were saved, and only the policy-off control was rerun. An initial build verification also used two stale directory names and one compiler process without the MSVC environment; the corrected exact directories all passed. The original config was restored after every bounded change to 1874 bytes and SHA-256 `222D059C727A683C8DEACE07C938002BDD6A69EB0DC9C908743E17A92B54EFBC` | outcome B remains for Faithful: public DLAA Auto wins on this title. Uncanny is positively validated as a persistent user-selected transformative preset with byte-exact HUD protection, but its artifact and identity costs forbid factory-default promotion from this single-title evidence. Raw captures and GIFs remain outside Git under `flycast-evidence/gate16-postjitter-c11ba8312`; no third-party binary, config, capture, game media, or user path is staged

#125 2026-09-04 b032d3a2f | attempt the same Uncanny automatic-HUD candidate on DX11 OIT using exact candidate, marker, and policy-off captures after confirming that no additional legal Dreamcast media exists at the emulator workspace root | all three 30-frame captures close cleanly and their color/depth/motion/mask contracts match exactly 30/30 at frames 2167-2196. OIT correctly reports zero jitter with `oit-scene-replay-pending`, and the supplied config is restored to 1874 bytes and SHA-256 `222D059C727A683C8DEACE07C938002BDD6A69EB0DC9C908743E17A92B54EFBC`. The fail-closed verifier rejects the candidate: protected OIT HUD composition covers part of the fixed 32x32 proof sentinel in 26 frames, leaving 989/1024 marker pixels; only frames 2193-2196 retain 1024/1024. A follow-on comparison and GIF generation consequently reject because the external output remains unconfirmed | this is a retained OIT diagnostic failure, not external-settings evidence. Do not weaken the sentinel requirement or cherry-pick four frames. A future focused proof change may place the sentinel outside protected-overlay coverage or bind expected overlay occlusion explicitly; current conservative OIT zero-jitter and native fallback remain unchanged. Raw rejected captures remain outside Git under `flycast-evidence/gate17-uncanny-oit-b032d3a2f`

#126 2026-09-04 ca9ec174b | replace the fixed sentinel origin with an explicit developer-only `--evidence-marker top-left|bottom-right` contract while retaining top-left as default; derive both the returned-D3D12-output copy and D3D11 swapchain verifier from the same output-relative helper; rebuild exact-SHA automation, NGX, no-NGX, and feature-off configurations; run all available selftests; rerun Uncanny Cinematic/Structure-200/Tone-75 automatic-HUD OIT as exact candidate, bottom-right marker, and policy-off captures; compare the confirmed result to the independent policy-off public output; generate and inspect a synchronized moving triptych; run a focused normal-DX11 top-left regression | all four exact-SHA builds link and automation/NGX/no-NGX selftests pass 162/162. The OIT candidate confirms 30/30 exact input/output contracts at frames 2163-2192 with the consumer-reported tuple upscaling off, intensity 2, global tone 0.75, diffuse white 203 nits, preset 0, style 2, enabled on. The bottom-right presentation proof retains 1024/1024 pixels throughout the accepted interval and completed same-frame Presents without changing protected-overlay composition; average protected HUD coverage is 15,686.73 pixels with zero mismatches, repeats, or drops. Against public DLAA Auto, Uncanny records source RGB MAE 10.166851 versus 2.574558, raw temporal RGB MAE 5.328867 versus 5.313476, trail 12.716295 versus 4.703119, edge displacement 1.054290 versus 0.629929, thin-line continuity 69.339793 versus 83.482103, low-frequency color drift 1.745385 versus 0.116905, saturation drift 12.415877 versus 2.929241, and black-level drift 8.585521 versus 2.240497. A three-frame normal-DX11 control then retains the unchanged top-left 1024/1024 marker and same-frame Present. The original consumer config is restored byte-for-byte to 1874 bytes and SHA-256 `222D059C727A683C8DEACE07C938002BDD6A69EB0DC9C908743E17A92B54EFBC` | the OIT HUD-safe Uncanny lane is now valid external-output evidence and remains explicitly non-faithful; Faithful stays public DLAA Auto. The first build command failed before compilation because the shell lacked the MSVC standard-library environment, a diff review caught and corrected an origin offset initially applied to the readback copy rather than the marker copy before commit, and a self-referential public comparison was correctly rejected before the policy-off public reference passed. Raw evidence and `uncanny-oit-hud-safe-moving-comparison.gif` remain outside Git under `flycast-evidence/gate17-uncanny-oit-ca9ec174b`; no third-party binary, config, capture, game media, or user path is staged

#127 2026-09-07 fcd5bc220 plus working tree | replace the paused OIT jitter draft with an isolated production replay: snapshot the pre-native opaque base, allocate dedicated opaque/multipass color, pixel-list/pointer, and reactive resources, route the final jittered OIT resolve into neural color plus aligned reactive coverage, restore native resource identities on every exit, count the added objects, unbind the correct u2/u3 slots, and compile/rasterize the real OIT vertex shader with zero versus +1-pixel jitter; build automation, NGX, no-NGX, and feature-off configurations; run all enabled selftests; capture deterministic Soulcalibur frames 1804-1806 in active public-DLAA and native OIT lanes on native D3D11 and D3D11On12 | all four executables link and automation/NGX/no-NGX selftests pass 163/163. The new fixture preserves pixel count and moves exact coverage bounds by +1 X/+1 Y on both surfaces. Active gameplay frames report Halton `[-0.375,-0.0555556]`, `[0.125,0.277778]`, and `[-0.125,-0.277778]`, `raster_jitter_applied=true`, and `separate-neural-scene-replay`; every matching `native-pvr-color.png` is byte-identical to its native control on both surfaces. Earlier direct-D3D11 attempts varied by 1 LSB over at most 20 pixels; review found the replay was clearing the native pointer texture instead of its dedicated pointer texture. After correcting that ownership error all six final parity pairs are exact. A pre-correction D3D11On12 active-frame injected-evaluate failure emitted no public artifact and final output equaled native, while automatic HUD protection retained zero jitter for `protected-overlay-present` and zero protected-pixel mismatch | FC-035's OIT extension and the OIT portion of quality Gate 12 are green in the working tree. The rejected 1-LSB controls remain outside Git under `flycast-evidence/oit-jitter-wip-20260907`; final exact-SHA rebuild, 600-frame performance/cadence regression, affected external Gate 16/17 recapture, and the newly planned late status OSD remain next. No third-party binary, config, media, capture, or user path is staged

#128 2026-09-07 32786f2c1 plus working tree | add an off-by-default `rend.NeuralStatusOverlay` Video setting, extend the thread-safe live snapshot with profile/preset/jitter, format a compact lower-right panel with mode/route/state/resolution/FPS/frame interval/accepted-busy-fallback counters, label drops unavailable and external DLSS 5 output unverified, draw it only in the late Flycast OSD, and extend capture CLI `--proof-overlay` with `neural-status`; build automation, NGX, no-NGX, and feature-off; run all enabled selftests; run deterministic Soulcalibur overlay-on/off captures on normal and OIT D3D11On12, a direct-D3D11 positive, and an injected evaluate-failure transition | all four executables link and automation/NGX/no-NGX selftests pass 164/164. At frame 1804, normal and OIT on/off pairs are byte-identical for native PVR, source color, depth, motion, bias mask, confidence, draw ID, public DLAA output, and final protected-game composite. Only `presented-with-flycast-overlays.png` changes after `gui_display_osd`: 26,751 content pixels on normal and 26,523 on OIT, with late-proof pass true. Visual inspection rejected an initial top-left layout because it covered Soulcalibur's HUD; the accepted panel is lower-right and readable at 640x480. The direct-D3D11 proof changes 26,751 late pixels and closes cleanly. The injected frame displays `Recoverable failure`, native fallback, and updated accepted/fallback counts, then recovers on following frames | FC-050's requested live OSD and its FC-054 capture seam are implemented in the working tree without becoming external-output evidence. Resize and a manual live settings-click remain focused UI smoke coverage; exact-SHA commit/rebuild/push and affected Gate 16/17 OIT visual recapture remain next. Captures stay outside Git under `flycast-evidence/neural-status-overlay-wip-20260907`; no media, SDK, runtime, third-party config, or user path is staged

#129 2026-09-07 bd5686167 plus working tree | begin the affected OIT Gate 16/17 recapture with the restored supplied consumer configuration, conservative intensity 0.125/Natural/tone-zero candidate, marker, and explicit policy-off lanes; reject phase-shifted evidence; add a synchronous-capture-only one-shot discontinuity before the first retained frame; rebuild automation, NGX, no-NGX, and feature-off; run all enabled selftests; execute a bounded real Soulcalibur OIT reset smoke | both 30-frame and widened 60-frame attempts are rejected: marker and policy-off match exact same-frame color/depth/motion/mask, but the candidate starts with a different accepted-evaluation Halton phase because external-host startup accepted a different number of pre-capture frames. At nominal frame 2261 candidate jitter is `[0.375,0.0555556]` while marker/off are `[-0.4375,0.388889]`, so no visual or metric result is accepted. The capture writer now consumes one start boundary after warm-up and calls the existing discontinuity before geometry capture, incrementing history generation, setting reset, and returning the accepted-evaluation jitter index to zero. All four builds link and automation/NGX/no-NGX selftests pass 168/168. A three-frame real OIT capture closes cleanly; its first retained combat frame records `reset_history=true`, incremented generation, and phase-zero jitter `[0,-0.166667]`. Public NGX was unavailable in that isolated smoke stage, so its repeated reset frames are mechanism evidence only, not DLAA quality evidence. A malformed PowerShell empty-pipe summary failed before evidence inspection and was corrected. The supplied 1874-byte configuration was restored after every attempt to SHA-256 `222D059C727A683C8DEACE07C938002BDD6A69EB0DC9C908743E17A92B54EFBC` | exact-input proof remains strict. Production gameplay and asynchronous performance behavior are unchanged because the reset is armed only by a non-empty bounded capture. Rebuild/stage the exact resulting commit, then rerun candidate/marker/policy-off and require matching phase plus exact inputs before resuming visual judgment. Rejected raw captures remain outside Git under `flycast-evidence/gate16-oit-jitter-bd5686167`; no third-party binary, configuration, capture, media, or user path is staged

#130 2026-09-07 9466e1c2b | rebuild and stage the exact capture-reset commit; rerun Soulcalibur OIT conservative 0.125/Natural/tone-zero, Uncanny Cinematic/intensity-2/tone-0.75/local-structure-2 maximum-coverage, and the same Uncanny tuple with automatic HUD protection; require exact candidate/marker/policy-off provenance and consumer-reported settings; compare moving results to public Auto; restore the supplied configuration; regenerate user-viewable moving comparisons; rerun all exact-SHA selftests | all three external lanes confirm 30/30 exact color/depth/motion/mask contracts. Every first retained lane frame records reset history and a common phase; jittered maximum-coverage frames begin at `[0,-0.166667]`, while the HUD-safe lane correctly reports zero jitter for `protected-overlay-present`. Conservative slightly lowers raw temporal RGB MAE from public `0.610280` to `0.604905` and edge displacement from `0.978536` to `0.978264`, but raises source MAE `1.839098` to `2.080172`, trail `8.812967` to `8.905790`, color drift `0.044727` to `0.230871`, saturation `1.987151` to `2.432000`, and black drift `0.563773` to `0.613786`; it does not displace public Auto. Maximum-coverage Uncanny records temporal/source/trail/edge/thin/color/saturation/black `0.624877/7.951702/12.398491/1.105515/68.438055/0.921269/10.350593/4.123283` versus public `0.610280/1.839098/8.812967/0.978536/72.634196/0.044727/1.987151/0.563773`. HUD-safe Uncanny slightly lowers temporal MAE `5.776949` to `5.735870` but raises source `2.175469` to `10.040473`, trail `3.318915` to `11.650328`, edge `0.554630` to `0.983441`, color `0.097595` to `1.812866`, saturation `2.471401` to `12.374512`, and black `1.896269` to `8.505836`, while lowering thin-line continuity `85.693872` to `71.461472`. It protects an average 15,803.1 HUD pixels with zero mismatch/repeat/drop. The consumer logs positively report upscaling off, intensity 0.125/tone 0/style 1 for Conservative and intensity 2/tone 0.75/style 2 for Uncanny; local structure remains requested plus prior isolated-output-proven rather than falsely consumer-reported. The original 1874-byte config is restored exactly to SHA-256 `222D059C727A683C8DEACE07C938002BDD6A69EB0DC9C908743E17A92B54EFBC`. Automation, NGX, no-NGX, and feature-off exact-SHA builds link, and all three enabled selftests pass 168/168. Failed attempts remain explicit: parallel exact-SHA reconfiguration collided on Flycast's shared generated version header and was rerun serially; the first confirmation rejected an append-only log containing old-SHA evidence; the second rejected a 2000-2239 sentinel window that did not cover retained frames 2721-2750, after which the evidence window alone moved to 2700-2939 | outcome B remains for Faithful and public Auto stays its default. Uncanny is now current-OIT, reset-aligned, HUD-safe evidence for a deliberately transformative user-selected default, not a factory-default promotion. Raw captures, reports, scripts, and both moving GIFs remain outside Git under `flycast-evidence/gate16-oit-capture-reset-9466e1c2b`; no third-party binary, config, capture, media, or user path is staged
