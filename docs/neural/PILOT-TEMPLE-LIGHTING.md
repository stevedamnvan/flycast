# Opt-in temple lighting candidate

## Current restoration guidance (2026-09-12)

Retained sword-hit test (practice-hit-lighting-a): source5458 from the corrected
HUD run, fixed key0,0,1 and fill0/.3,120 frames each control/repeat/fill, all exit0.
Baseline mod remains byte-identical. Repeat whole-image MAE0.456 versus fill23.377;
this proves visible illumination change, not improvement. Viewed fill restores
body readability but Sophitia face remains washed out. No candidate promotion.
Isolated exposure diagnostic practice-hit-exposure-a holds exact packet, key,
fill and materials fixed; disables auto exposure only in private supplied profile.
Fixed/repeat exit0, baseline exact. Viewed arena becomes substantially darker
while face remains washed out; reject as global fix. Next face source/material
response. No live/global settings changed; no appearance promotion.

Camera-light research (2026-09-12): NVIDIA distinguishes the Toolkit viewport's
camera-attached visibility light from authored stage lights. Its runtime fallback
light is explicitly a debugging aid, not the recommended shipping lighting.
Our supplied helper distant light is a separate authored diagnostic, not that
fallback option. Do not silently replace it with a continuously camera-following
key as a character fix. Stage lighting should have a reproducible direction in
the scene's supported coordinate space; world-space recovery here is still open.

NVIDIA recommends primitive lights for scene illumination, distant lights for
sun-like illumination, and modest radiance considered together with exposure.
Tonemapper exposure affects all scenes globally. Therefore hold exposure policy,
light direction/radiance and composed materials fixed in diagnostic comparisons;
do not repair dark characters by globally washing out the user-liked arena.
Use a fixed stage key and scene-motivated fill as an implementation hypothesis,
then verify fighters turning, camera motion/cuts and stable weapon reflections.
Any fill must survive both levels; no final numeric preset inferred from docs.

Local evidence: practice-character-response-a renders source5302 unchanged and
with character-only vertex RGB whitened, with repeat control; all120-frame runs
exit0 and baseline exact. Unchanged standalone already lights fighters brightly,
unlike live e. Logs show standalone direction(-.487995,-.284208,.82528) versus
live retained(0,0,1). This establishes a comparison confound, not sole causality:
accumulation/history and composition also differ. Next reproduce the live light
direction in the same frozen source before judging material edits. Whitened RGB
is diagnostic only; no texture,alpha,UV or normal replacement is promoted.

Sources:
- https://docs.omniverse.nvidia.com/kit/docs/rtx_remix/latest/docs/howto/learning-lighting.html
- https://docs.omniverse.nvidia.com/kit/docs/rtx_remix/latest/docs/toolkitinterface/remix-toolkitinterface-viewport.html
- https://github.com/NVIDIAGameWorks/dxvk-remix/blob/main/RtxOptions.md

The user likes the current Practice arena and floor textures. Preserve that
environment reference while correcting dark character lighting/material response;
do not globally brighten exposure or remove floor detail as an assumed fix.
The older rig results below are historical: LOG984 rejected the warm-key/cool-fill
fixed-exposure candidate for harsher faces/contours. Do not treat it as a promoted
lighting preset. LOG983 found exposure adaptation could hide a light-strength
change; compare actual settings and repeated controls before drawing conclusions.

The earlier character displacement correction (`character_displacement_review`)
improved Kilik contours in its recorded moving comparison. Its seven material
IDs are all absent from the Mitsurugi/Sophitia Practice packet5301 (manifest in
C:/Flycast-Evidence/practice-remix-temple-b). Enabling that layer cannot transfer
the fix to these variants. Audit their exact material identity, bindings and
response first; never alias facial/hair textures by resemblance. Retain the
existing correction for applicable sources and record active layers in every
new comparison. These are scoped corrections, not full character acceptance.

The PBRify pilot's diagnostic front light overexposes bright surfaces and
flattens form. `--temple-light-rig --anchored-light` on remake_launch.py
selects an authored warm key at radiance1 and cooler fill. Omission retains
the prior light. The helper requires the anchor flag; directions stay fixed
within an anchor generation and reset at a source-view cut. This is not
recovered world lighting. Light resource rebuilds retain the direction owner.

Private material correction: `D:\Flycast-Evidence\pilot-curated\refine_cloth.py`
raises roughness only in reviewed red-cloth regions of A9FE1461748274D9 and
D8C38119A5E41BD1, retaining generated variation and untouched albedo/normal.
Unselected roughness texels are byte-identical. First selector was rejected
for catching leather; v2 limits saturated red within reviewed atlas bounds.
The separate pbrify_cloth_refined_v2 layer is ingested/bound through Toolkit
MCP and can be removed to restore the original PBRify material result.
An MCP enum-reference error is avoided by omitting executor and using the
server default. The saved generation driver lacked its ingestion schema;
current defaults were recovered from the live MCP tool schema.

Validation (2026-09-10, incremental working-tree builds): automation, baseline
and no-NGX selftests873/0 each; disabled build successful; launcher17/0.
Negative controls cover missing anchor, changed epoch, camera rotation and
explicit cut reanchoring. Same-source1280x960 stills are under
`D:\Flycast-Evidence\pilot-curated`: cloth-v2-both-light1 and
cloth-v2-temple-rig1. Cloth loses the white plastic-like highlight; rig
adds directional form and contact shadows. Human visual approval pending.

`D:\Flycast-Evidence\pilot-temple-moving-v1` uses native alpha, welded normals,
exposure A and the rig at640x480. Host exit0, helper11 at orderly host end;
no forced children.40 captures cover source2908..2948 (one gap), not the
requested early combat window because first source arrived late. Review
shows recovery motion; protected HUD pixels are absent, so this does not
establish HUD invariance. Four anchor-generation changes are logged over
the run. Existing40-object shutdown warning remains. No fresh external
neural provenance, full combat stability, performance, or world-lighting
acceptance is claimed. Continue F's full live extent contract; later G must
repeat the combat/HUD comparison with the actual final candidate.

Colour-path check (2026-09-12): public dxvk-remix main d3d9_rtx.cpp selects
GetSampleView from SRGBTEXTURE, but opaque_surface_material_interaction.slangh
lines750-753 explicitly applies gammaToLinear(albedo), assuming non-sRGB textures.
Therefore helper SRGBTEXTURE=FALSE is consistent with this downstream path;
do not flip it as an unproven brightness fix (would risk double conversion).
Public source is not installed-binary provenance. Files retained colour-path-a.
Source: https://github.com/NVIDIAGameWorks/dxvk-remix/blob/main/src/dxvk/shaders/rtx/concept/surface_material/opaque_surface_material_interaction.slangh

Face-normal consistency (practice-face-normal-a, retained hit5458): Mitsurugi296
triangles, Sophitia430; zero degenerate triangles and zero normals opposed to
facet orientation. Normal/facet dot medians.9687/.9723, minima.7358/.6882;
403/545 distinct rounded normals. No flattened/zero/inverted-normal evidence.
Preserve welded normals. This does not prove anatomical geometry or world-space
reconstruction. Next image response must not undo this established correction.

Refined ACES single-option test practice-refined-aces-a completes two120-frame
renders exit0, baseline exact. Legacy ACES false in private profile only; source
key/fill/materials/local tone map fixed. Viewed face remains washed out; no
promotion. Avoid further unbounded preset searches; appearance stays open while
backlog playback/performance dependencies advance.
