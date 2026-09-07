# FC-067 bounded visible approximation result

Implementation base: `f57bbe9c863a7be4b3e93fc8b66fd96cb7c0f285`.
Source capture: `688d63e05`, Soulcalibur `T1401N`, Hoko Temple frames 1804-1833.
Working evidence: sibling `flycast-evidence/fc067-preview-working-e`.

## Disposition

**ACCEPTED as an offline hypothetical visualization only. NO-GO for promoting
this preview as a remake-quality renderer or default.** This is not recovered
camera truth, physically based relighting, runtime performance, Remix, or DLSS 5.

The native image is used as image-projected material, including baked lighting
and shadows. A 60-degree vertical FOV and reciprocal raw-PVR-z depth are explicit
assumptions in arbitrary units. Derived face normals face that assumed camera.
One camera-relative light modulates SDR bytes: no albedo separation, shadow
rays, normal smoothing, offscreen geometry, or physically based transport.

Paired samples 1804, 1819 and 1833 visibly expose polygon facets on moving
fighters and over-brighten/clip floor and skin. Water remains largely protected;
source shadows do not respond to the new light. This rejects promotion of this
specific treatment, not all future renderer/material approaches. A 30-frame
comparison is retained at approximately 60 Hz and 100 ms/frame slow playback.
No measured temporal improvement or user perceptual approval is claimed.

## Implemented scope

`neuraltest remake-preview` reuses the bounded decoder, accepts 1-30 frames,
explicit FOV 30-100, and a new output directory. It supports only 640x480 affine
screen-domain single-clear-pass captures. PNG dimensions are prechecked;
manifest size/depth and packet allocations are bounded. Frame, game, SHA and
actual RGBA source hash must match. Each output copies its input manifest.

Opaque greater-depth, depth-writing surfaces are eligible. Uncertain/translucent/
punch-through footprints protect where their raw depth is not behind the nearest
opaque approximation. No texture-alpha, cull, tile-clip or modifier reconstruction
is fabricated. Eligibility boundaries are eroded. The explicit y=144..431 crop
is not HUD classification: all outside pixels remain unchanged and the comparison
displays only the labeled crop. Modifier shadows remain baked source appearance.

Per-frame outputs: native, approximation, light-off, coverage, normals,
inverted-normal, wrong-depth, wrong-projection, source manifest. Aggregate:
frames.csv and preview.json. The Pillow script packages two labeled GIFs and
three paired sample PNGs; it does not generate new game content.

## Evidence actually run

- Four working configurations configure/build serially, exit 0. Three enabled
  selftests pass 243/243 each; SDK mock passes 56/56 with no runtime/GPU/Present.
- Working E processes 30/30 matching source hashes and identities. Light-off is
  byte-identical and protected-channel mismatches are zero for all frames.
- Eligible pixels: 126,441-138,500; changed pixels: 126,192-138,083. Inverted
  normals differ at 126,036-137,983 pixels, wrong depth at 126,316-138,359,
  wrong projection at 103,030-117,588. These prove non-inert mutations, not
  quality. Independent analytic goldens reject wrong projection, reciprocal
  depth misuse and inverted lighting normals.
- Wrong-frame manifest rejects `preview-manifest-identity`; wrong-frame PNG
  rejects `preview-native-input-hash`, both exit 1. Existing output and 31-frame
  bounds reject with their intended reasons, exit 1.
- Source emu.cfg remains SHA-256
  `1EF718689784DCE64CAD1CE8BEC776E710B4A3705ECFF2E5A276A1A3B2F92992`.
  Production source/defaults and external configuration are unchanged.

Rejected attempts retained: interrupted build later verified finished by a
successful no-work build; A fails insufficient coverage because whole-background
protection masks front geometry; B's declared depth-aware protection passes
three frames. C and D fail source hashing: C uses the harness image offset and
BGRA, D fixes the offset only. Actual production raw-hash code and capture bytes
prove RGBA; E uses that single declared order and passes all 30. Failed attempts
are not provenance evidence. Two guessed decoder headers were absent; the real
declaration is in pvr_scene_capture.h. An audit patch failed on a diagnostic
heading mismatch and applied nothing before correction.

## Next action and stopping boundary

Deliver the moving comparison, then stop this approximation branch. No further
light-tuning phase or generic diagnostic work. The direct current-packet Remix
no-go remains; a genuine remake step needs a separately scoped camera/material
strategy and actual runtime/completion/readback harness. Do not promote assumed
projection to truth, start a second renderer, or acquire proprietary dependencies.

Strict M2 source equality remains 29/30 and parked, not passed. Decoded versus
original-buffer replay remains 30/30. Full reconstruction/GPU requirements and
the active goal are not completed by this visualization.
