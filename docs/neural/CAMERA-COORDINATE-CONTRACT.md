# FC-067 sampled camera and coordinate contract

## Accepted scope (LOG331-343)

This is a CPU evidence-coordinate contract, not a complete world-scene or
Remix GPU acceptance. H/L/M are three separately witnessed producer frames.
Each first-batch mesh contains 3682 vertices and 2152 triangles. The selected
1622 divided-path vertices preserve source words across these frames; their
shared matrix motion does not independently prove static-arena identity.

The selected source anchor uses calibrated top-three matrix rows. Preserve
general-affine contributions, nonunit W and recorded expression ordering.
Reflect source Y and view Y together, reverse triangle winding, and retain a
proper right/up/forward camera basis. Lens aspect is effective projection
aspect, not framebuffer aspect: vertical FOV45.99033235267587 degrees and
aspect1.2266665409901234 for the measured calibration and640x480 viewport.

For float conversion use one fixed sequence origin, chosen from H, for both
source positions and every camera position. Never independently recenter each
frame. Camera motion must remain nonzero in L/M. CPU Project/Unproject use
double intermediates but retain float inputs/output and unchanged public ABI.
LOG343's full-mesh mathematical projection errors remain below0.001 pixels
in all three frames. This does not imply identical GPU arithmetic.

## Explicitly unsupported

- Unique physical world coordinates, units, static arena identity and complete
  object/model/view decomposition are not established.
- Game near/far planes are unknown. Synthetic harness planes are not recovered
  game values and must not be silently copied into an accepted game packet.
- Normals, textured material rendering and complete scene coverage remain open.
- BGP/screen-plane content is not world geometry. General-affine and unknown
  domains must retain their labels rather than being forced rigid.
- Strict comparison against captured rounded PVR positions still fails as
  recorded in LOG315-343. CPU mathematical agreement does not waive that gate.
- No real Remix runtime, relighting, combined DLSS5 output or presentation is
  proven by the mock ABI or these projection checks.

## Next integration dependency

Inspect the existing exported mesh/material records for the selected draws and
bind their identities/generations to the accepted evidence vertices. Record
which normals, texture assets, transforms and clip semantics are available or
missing before implementing a game packet adapter. Do not change
ReadyForAdapter's incomplete/unknown/textured-material rejections to force
acceptance. Keep M2-camera pending until its usable game contract is explicit;
M1-GPU remains an independent route if an approved runtime is available.
