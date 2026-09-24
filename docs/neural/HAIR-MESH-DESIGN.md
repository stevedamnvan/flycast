# Modern hair meshes: design and cost estimate

Status: **proposal, needs user approval** (PLAYABLE-REMASTER-PLAN task 7.5).
Nothing here is implemented. Figures marked "estimate" are planning numbers,
not quotes or measurements.

## 1. Why Remix mesh replacement cannot do this

- Remix matches replacement meshes by a geometry hash. This runtime's rule is
  `positions, indices, geometrydescriptor` (printed in every helper Remix log).
- Soulcalibur poses characters on the SH4 CPU. Each frame the packet carries
  already-posed world-space vertices and no bones (`Mesh::transform` is absent
  for characters). So hair positions, and therefore the hash, change every frame.
  A Toolkit mesh replacement would match at most one frame.
- Material replacement still works, because it keys on the texture hash, which
  is stable. That is why the texture/material routes (plan tasks 7.1-7.4) are
  possible today and new geometry is not.

## 2. What we know about the original hair (Sophitia, LOG1165-1168)

- The hair is not its own mesh: 184 triangles inside a fixed texture region
  (atlas box about x117..179, y187..248 of a 256x256 character atlas), spread
  over six meshes that share that atlas with the body.
- That region was present and separate from every other triangle in all 300
  retained frames (5300..5599). The texture identity was stable.
- So a hairstyle can be identified per frame by: character atlas texture
  identity + UV region. Tools for this already exist (uv-audit, LOG1167).

## 3. Proposed design: "wrap" replacement inside the helper

The helper (`neuraltest/remake_d3d9_scene.h`) already draws every packet mesh
through D3D9 for Remix. Add one opt-in step there:

1. **Hide:** skip the original hair triangles (atlas identity + UV region, per
   hairstyle, from a small JSON "hair binding" file).
2. **Bind (offline, once per hairstyle):** take one reference frame. An artist
   fits the new hair asset onto the exported original head/hair (OBJ export of
   that frame's character meshes, new tool). A binding tool then attaches each
   new-hair vertex to the nearest original hair triangle: barycentric point plus
   an offset in that triangle's local frame (normal, tangent). Vertices far from
   any original hair (for example longer new hair) bind to a rigid head frame
   fitted from head-region vertices (least-squares rigid fit).
3. **Deform (every frame, CPU):** rebuild each original triangle's frame from
   the current posed vertices and move the new hair with it. Swinging ponytails
   follow automatically because the original low-poly hair already swings.
   Cost estimate: 5k-20k vertices x a few operations = well under 0.5 ms on one
   core; can run on the existing return worker.
4. **Draw:** upload the deformed hair as an extra D3D9 draw with its own
   textures. Remix assigns a hair material by texture hash in a separate mod
   layer (Toolkit MCP): albedo, alpha (cards), normal, roughness, and the
   installed `AperturePBR_Opacity` **anisotropy** for the long highlight along
   strands, plus a warm `subsurface_transmittance_color` for backlight.
5. **Guidance for DLSS 5/DLAA:** Flycast's motion vectors come from the original
   geometry, so they are wrong at the new hair's silhouette. The returned depth
   (which includes the new hair) already feeds the depth-consistency bias mask;
   verify it marks the new hair as reactive, otherwise add the hair region to
   the bias mask. Expect some softening in fast motion.
6. **Switch:** `remake_play.py --hair-meshes DIR`, off by default. Missing or
   failing bindings fall back to the original hair (never missing hair).

Why not strand hair: Remix has no strand/curve primitive. Cards (textured
alpha-tested strips) are the standard real-time approach and what Remix renders.

## 4. Phases, cost and go/no-go gates

Engineering time is in agent working days (a day is about one long focused session).

| Phase | Work | Engineering (estimate) | Money | Gate |
| --- | --- | --- | --- | --- |
| A. Spike (Sophitia, offline only) | Export tool, binding tool, deform + hide + extra draw in the helper, run on the 300 retained frames 5300..5599 via offline re-render; placeholder hair cards (simple Blender cards or a CC0 asset) | 6-9 days | $0 | User watches a before/after moving comparison. Pass: hair follows head and ponytail with no popping or detachment, and the frame-cost increase is measured. **Stop here if not approved.** |
| B. Live integration | Play-launcher switch, return-worker deform, guidance/reactive handling, fallback, tests, performance runs | 8-12 days | $0 | Play session with Sophitia; perf targets (plan 5.3) still met |
| C. Art, per hairstyle | New hair cards (mesh + albedo/alpha/normal/roughness/flow), fit onto the reference frame | see section 5 | see section 5 | User approves each hairstyle in play |
| D. Per-hairstyle integration | Find UV region, reference frame, binding, material in its own layer, check in play | 1-2 days each | $0 | User approval |

Roster scope (estimate; the game's full roster/costume list is not yet counted,
BACKLOG "Delivery status"): about 12-14 characters have visible hair; with
costume variants about 15-20 hairstyles.

Engineering total, first character end to end (A+B+one D): about 3-4 weeks.
Then about 1-2 days per extra hairstyle, so all hairstyles about 5-8 more weeks
of integration, in parallel with art delivery.

## 5. Art options (estimate; get real quotes before deciding)

| Option | Cost per hairstyle | All 15-20 | Notes |
| --- | --- | --- | --- |
| Commission a freelance hair-card artist | about $150-800 (higher for premium) | about $3,000-15,000 | Best match to each character's design; needs a clear brief and licence to use in this private project |
| Marketplace hair-card packs | about $20-100 per pack | about $300-1,500 | Poor style match likely; check licence allows modification; still needs fitting (0.5-1 day each) |
| Do it yourself in Blender (free) | $0 | $0 | About 1-3 days each for a practised hobbyist; quality depends on skill |
| AI 3D generation | n/a | n/a | Not usable today for alpha hair cards; do not plan on it |

Do not use Unreal MetaHuman grooms or other assets whose licence is tied to
another engine or product. Never ship or publish Soulcalibur's own assets.

## 6. Performance risk (to measure in Phase A)

Hair cards are many overlapping alpha-tested layers, which is one of the more
expensive things to path trace. Estimate: +0.5-2 ms GPU per character on the
RTX 5090 at 1280x960 with 5k-20k card triangles. The current 1280x960 frame is
already near the 16.7 ms budget (p50 16.3 ms, LOG1116), so Phase A must measure
this and Phase B may need a lower card count or opaque inner layers.

## 7. Risks

| Risk | Effect | Mitigation |
| --- | --- | --- |
| Hair shares meshes and atlas with the body | Hiding must be per triangle, not per mesh | UV-region selection proven for Sophitia across 300 frames; verify per character |
| New hair longer or fuller than the original | Clips into shoulders or weapons; no physics | Keep designs close to the original volume; head-rigid binding for extra length |
| Motion vectors wrong at new silhouettes | DLSS ghosting or softness on hair in motion | Reactive/bias mask on hair pixels (step 5) |
| Different costumes or hidden characters change the atlas | Binding fails to match | Falls back to original hair; add bindings per variant |
| DLSS 5 restyles hair anyway | Less visible gain | Compare in play with DLAA; tasks 7.1-7.4 may already give most of the gain for far less work |
| Frame-cost increase | Misses 60 fps target | Measure in Phase A; card budget per character |

## 8. Recommendation

Do plan tasks 7.1-7.4 (texture, edges, material, DLSS 5 settings) first. They
cost days, not weeks, and no money. If hair is still the weakest part after
that, approve **Phase A only** (6-9 agent days, $0) and decide on art spending
after seeing the spike on Sophitia.

To approve, tell the agent: "Hair meshes: approve Phase A". Nothing else in
this document is authorised until then.
