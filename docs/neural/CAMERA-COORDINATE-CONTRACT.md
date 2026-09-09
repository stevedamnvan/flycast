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
- Authored normals remain unavailable. D-130/LOG370 provide explicitly
  geometry-derived flat normals, not recovered game normals. D-131/LOG379
  bind verified source textures to the sampled geometry; actual textured
  rendering and complete scene coverage remain open.
- BGP/screen-plane content is not world geometry. General-affine and unknown
  domains must retain their labels rather than being forced rigid.
- Strict comparison against captured rounded PVR positions still fails as
  recorded in LOG315-343. CPU mathematical agreement does not waive that gate.
- No real Remix runtime, relighting, combined DLSS5 output or presentation is
  proven by the mock ABI or these projection checks.

## Next integration dependency

Current update (LOG402-413): the prepared H artifact has now rendered through
the authorized runtime with visible temple textures; this is a selected static
snapshot, not complete-scene or moving-camera acceptance. Do not repeat artifact
assembly as the next task. Inspect existing multi-frame transform evidence for
a bounded sequence while preserving the common H origin and every omission.
Runtime cleanup remains open independently (empty-scene37/common rendered40
objects at exit); its warning cannot be waived by a successful screenshot.
The text below records the historical pre-runtime integration dependency.

The selected H geometry/material publication join is now implemented (LOG379).
Do not repeat asset-binding checks as a substitute for scene submission.
Next assemble a bounded developer artifact carrying source identity, converted
positions, explicit derived-normal provenance, UV/color attributes, published
texture association and all coordinate/coverage exclusions. Unknown game
near/far values must not acquire synthetic defaults through serialization.
Do not clear omissions or relabel the source anchor as proven physical world
space to pass ReadyForAdapter. Keep M2-camera pending until its usable game
contract is explicit; M1-GPU remains an independent route when a compatible
supplied runtime becomes available. The parked captured-rounding failure is
distinct from the passing CPU mathematical projection test; neither alone
proves or disproves a complete working runtime camera.
