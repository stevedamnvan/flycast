# Opt-in temple lighting candidate

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
