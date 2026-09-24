# Skin and metal materials: design

Status: **approved direction** (user, 2026-09-23: do skin and metal before new
hair geometry). Executed as PLAYABLE-REMASTER-PLAN Phase 6. Nothing here is
implemented yet. Figures marked "estimate" are planning numbers.

## 1. Why characters do not "pop" today

- None of the 67 captured character textures has an accepted material upgrade.
  23 have generated candidate maps (PBRify: albedo, roughness, normal, height),
  only 1 has a metallic map, and none is active
  (`C:/Flycast-Evidence/character-texture-readiness-b/full-character-ledger.json`).
  Characters path-trace as their painted 1999 textures with default material.
- **Mixed atlases:** each character texture (256x256) mixes skin, cloth, straps,
  hair and metal. Remix gives one material per texture hash, so any per-surface
  look must come from per-texel maps (metallic, roughness, subsurface...).
- **Painted lighting:** metal areas have painted white highlights. Path-traced
  metal with painted highlights looks flat or double-lit (ledger notes on
  BCBC713835472B28 and F645D8A55F6EDF87; LOG1137 blade gain "subtle" when only
  metallic changed, under the old broken lighting).
- **Low-poly shapes:** detail (rivets, engraving, stitching, pores) must come
  from normal and height maps. This does not hit the per-frame reposing problem
  that blocks new hair geometry, because materials key on the texture hash.

## 2. What the installed runtime supports (checked 2026-09-23)

From `flycast/rtx-remix/captures/materials/AperturePBR_Opacity.mdl` and the
Remix 1.5.2 `d3d9.dll`:

| Feature | Material inputs | Use |
| --- | --- | --- |
| Diffusion-profile subsurface scattering (screen space) | `subsurface_diffusion_profile`, `subsurface_radius` (RGB), `subsurface_radius_texture`, `subsurface_radius_scale`, `subsurface_max_sample_radius`, `subsurface_single_scattering_albedo`/`_texture` | Soft skin; red light spreads furthest |
| Transmission | `subsurface_transmittance_color`/`_texture`, `subsurface_thickness_texture`, `subsurface_measurement_distance`, `subsurface_volumetric_anisotropy` | Backlit ears/fingers, hair |
| Metal | `metallic_texture`, `metallic_constant`, roughness texture | Steel, gold, silver, bronze |
| Anisotropy | `anisotropy_texture`, `anisotropy_constant` | Brushed/stretched blade highlights |
| Normal and height | `normalmap_texture`, `height_texture`, `displace_in`, `displace_out` (runtime displacement mode) | Engraving, rivets, chainmail, pores |
| Thin film | `thin_film_thickness_constant`, `thin_film_thickness_from_albedo_alpha` | Optional tempered-steel tint; experiment only |
| Denoiser | runtime `rtx.enableRayReconstruction` (with `rtx.upscalerType`, `rtx.qualityDLSS`, `rtx.dlssPreset`) | DLSS Ray Reconstruction: sharper, steadier reflections |

The runtime option values (for example which `rtx.qualityDLSS` value is DLAA,
and the displacement mode option) are not verified. Read them from the Remix
log or runtime docs before use; do not guess.

## 3. Design

### 3.1 Material class regions (the foundation)

Per character atlas, a small JSON file lists polygons in source texel
coordinates, each with a class:

```json
{"runtime_hash": "68747214071C64EE", "fighter": "Sophitia", "size": [256, 256],
 "regions": [{"class": "skin", "polygon": [[0,0],[120,0],[120,90],[0,90]]},
             {"class": "hair", "polygon": [[117,187],[179,187],[179,248],[117,248]]}]}
```

- Tracked in `neuraltest/play/material-regions/<RUNTIMEHASH>.json`. They hold
  coordinates only, no game pixels, so they may be committed. Rasterized masks
  and previews are made from game textures and stay outside Git (evidence).
- Texels in no region are class `keep`: their maps equal the current baseline,
  so an unfinished atlas never changes.
- Classes: `skin`, `eye`, `lips`, `hair`, `cloth`, `leather`, `steel`, `silver`,
  `gold`, `bronze`, `lacquer`, `wood`, `organic`, `fur`, `keep`.
- Regions are drawn from used-UV evidence (which texels triangles actually
  sample), never from colour alone. Same method as Sophitia's hair (LOG1167).

### 3.2 Starting values per class (tune in review)

| Class | Metallic | Roughness | Albedo | Subsurface | Other |
| --- | --- | --- | --- | --- | --- |
| skin | 0 | 0.45-0.55 | source, upscaled | profile on, radius RGB about (1.0, 0.37, 0.19) x scale | thickness thin at ears/fingers if marked |
| eye | 0 | 0.08-0.15 | source | off | wet highlight |
| lips | 0 | 0.35 | source | on (as skin) | |
| hair | 0 | 0.4-0.6 | source | transmittance warm | anisotropy about 0.5 (Phase 7) |
| cloth | 0 | 0.75-0.9 | source | off | |
| leather | 0 | 0.45-0.6 | source | off | |
| steel | 1 | blades 0.12-0.2, armour 0.25-0.4, worn 0.4-0.55 | F0 about (0.56, 0.57, 0.58) x detail | off | blades: anisotropy 0.5-0.8 along the blade |
| silver | 1 | 0.15-0.3 | F0 about (0.95, 0.93, 0.88) x detail | off | |
| gold | 1 | 0.2-0.35 | F0 about (1.0, 0.77, 0.34) x detail | off | |
| bronze | 1 | 0.3-0.45 | F0 about (0.8, 0.55, 0.35) x detail | off | |
| lacquer | 0 | 0.15-0.3 | source | off | Mitsurugi's red armour: glossy, not metal |
| wood | 0 | 0.6-0.8 | source | off | |
| organic | 0 | 0.35-0.6 | source | profile on, smaller radius | Nightmare's flesh and sword |
| fur | 0 | 0.8-0.95 | source | off | |

**Metal albedo ("detail"):** inside a metal region, detail = source luminance
divided by the region's median luminance, clamped to 0.6..1.15. This keeps
painted engraving and wear but flattens painted highlights. Roughness gets
+0.1 x (1 - normalized detail) so darker, worn areas are rougher.

**Subsurface scale:** the game's units are not centimetres. Measure a head
height in packet units (world positions of one face mesh). Start with
`subsurface_radius_scale` = head height x 0.005, then try x0.5 and x2. Pick the
one where skin looks soft but eyes, brows and lips stay sharp.

**Normal and height:** start from the existing PBRify normal/height candidates
only inside metal and leather regions; flat elsewhere at first. Never put
painted lighting into normal/height maps (LOG1168 rejected that). Height with
displacement only on engraved metal, and only if its cost is measured.

### 3.3 Map building and ingestion

- A tool (`neuraltest/remake_material_maps.py`, to be written in task 6.2)
  reads the region JSON, the source atlas (Remix capture DDS) and the existing
  upscaled albedo, and writes albedo, metallic, roughness, anisotropy,
  subsurface radius, transmittance and normal maps at the albedo size.
- **Mips (important, LOG1135-1137):** ordinary box mips leak mask values into
  neighbouring areas from mip 3; "conservative" mips make distant metal vanish
  from mip 4. Author every mip level explicitly, and ingest scalar maps
  (metallic, roughness, anisotropy) as BC4 DDS through the existing typed MCP
  tool `flycast_ingest_scalar_dds_current_process` in
  `neuraltest/remix_capture_mcp.py`, which preserves authored mips.
- All ingestion and binding goes through the Toolkit MCP into one new layer
  `layers/skin_metal_a.usda`. Never edit the baseline `mod.usda`.
- Reuse first: `layers/character_correction.usda` (inactive) already holds
  blade metallic/polished maps (LOG933) and gold cap roughness (LOG931);
  `skin_response_review_b.usda` and `character_displacement_review.usda` hold
  earlier skin/displacement tries. Read them before authoring new values.

### 3.4 Ray Reconstruction

Today the Remix output uses Remix's standard denoiser with its upscaler off
(`rtx.upscalerType = 0`, `rtx.resolutionScale = 1.0`), then DLSS 5 or DLAA runs
on the returned image. The trial turns on DLSS Ray Reconstruction inside Remix
at full resolution (no upscaling), then keeps the DLSS 5/DLAA stage. Risk: two
temporal stages in a row (smearing, extra latency). It is judged by metal
reflection sharpness and stability in motion, frame time and latency, against
the same scene without it. Kept only if it wins and the user agrees.

## 4. Scope (from the captured atlases, LOG1188 contact sheet)

- **Metal-bearing atlases, about 20:** blade strip atlases (BCBC713835472B28,
  F645D8A55F6EDF87, BC31E0E97FE752C4, 8B60DEA8F6D8E65B), gold staff caps
  (259014235DE60F8A), Mitsurugi armour (D7F586B86069CDA6) and blade/hilt
  (51B4B028A25C0B1E), Sophitia's shield (F99F377520942C9F) and straps,
  Nightmare armour (0269414FCDACD07C, 33094B480F506048, 4D0B61AB2E49BCBD),
  Voldo gold (553D1276F761CC64, 2203CCC3A1CC91F1, 82437C068C133D6A,
  21C7BB03B7325CEB), Astaroth's axe (59299B7617960FEA), Ivy's sword
  (5C5452DA5B0F0744), Maxi's weapon (EF9E9CBC931F0DE2), sword guard
  (42217A1984A23AD2), armour mix (62BCD7B9D1AEBDB5).
- **Skin-bearing atlases, about 18:** 8 face atlases (145398E2FC5B2FEA,
  E23741259F4E003B, F8CC33F333E700CD, 91446E8A159E8C2F, 0DCBE839C56F7DD2,
  D4CC0F0E458AAECF, 42B44C45992DA7F8, 2644F8D1634C6D62) plus body skin
  (Voldo, Astaroth, Maxi, Mitsurugi's torso, Ivy, Sophitia and others).
  Nightmare's flesh uses `organic`.
- Uncaptured fighters and costumes: later, same method (plan 8.3).

Face atlases hold repeated expression cells: regions must cover all cells the
same way, and eye/lip regions must be marked in every cell.

## 5. Cost (estimate)

| Step | Work | Agent days | Money |
| --- | --- | --- | --- |
| Regions | About 35 atlases; user review per character | 5-8 | $0 |
| Map builder | Tool, tests, mip authoring | 3-4 | $0 |
| Skin setup | Profile, scale, eyes, lips | 2-3 | $0 |
| Ray Reconstruction trial | Config, measurements | 2-3 | $0 |
| Pilot and rollout | Sophitia + Mitsurugi, then the rest | 4-6 | $0 |

About 3-4 weeks in total. Optional paid work: hand-painted normal detail for
hero pieces (for example Sophitia's shield, Nightmare's armour), only if the
user asks after the pilot.

## 6. Risks

| Risk | Effect | Mitigation |
| --- | --- | --- |
| Frame near the 16.7 ms budget (LOG1116) | Subsurface, Ray Reconstruction and displacement each cost GPU time | Each is a separate switch; measured with the 5.1 benchmark; the numbers decide |
| Faces are 30-60 pixels tall in play | Skin gain mostly on bodies and close-ups | Judge in play, not only in close crops |
| DLSS 5 restyles skin | Gains hidden or changed | Always compare DLSS 5 and DLAA looks |
| Material change not reaching the frame | Wasted tuning (LOG1137 saw only a subtle difference) | Prove each binding first with an extreme control (magenta tag method, LOG933) |
| Mip leaks at distance | Metal/cloth bleed or metal vanishing far away | Authored mips via the typed BC4 route |
| Regions wrong | Metal skin, shiny cloth | User reviews an overlay per character before ingestion |
