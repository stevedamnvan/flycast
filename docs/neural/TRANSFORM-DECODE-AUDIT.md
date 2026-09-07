# FC-067 source transform to decoded vertex

Baseline: `be6a91b9e4355a2628936265d2abf8bdd81bb4c3`.
**ACCEPTED one source-transform to decoded-vertex chain (self-review).**
World camera, physical-depth calibration, visible frame ownership, complete
world geometry and Remix GPU execution remain unproven. No parked gate closes.

## Exact identity across the TA copy and decode

The full prior transform/arithmetic/SQ/RAM/bulk-input chain is verified again.
The temporary diagnostic arms the exact input packet pointer at the witnessed
TAWrite entry. After context/overflow checks, ta_thd_data32_i copies that packet
into the actual TA buffer. Its post-copy bytes agree with both the input and
the saved 32-byte packet; context Address is 00509700 and byte offset is 507616.

ta_parse_vdrc supplies the actual child context while the ordinary vertex
converter executes. Its input points at the payload, four bytes past the PCW.
The corrected probe subtracts offsetof(TA_VertexParam, vtx0), with a compile-time
assertion that the offset is four; it compares that exact packet pointer with
the copied buffer slot. No content search or nearest-coordinate association is
used. Both actual child and root context Addresses are 00509700.

The ordinary converter appends vertex index 14526. Its actual x/y/z words are
44175b90/42ab1024/3d95b6cd, exactly the captured packet's coordinate words and
the preceding derived arithmetic outputs. The full packet is also unchanged.
This observation is at the initial xyz conversion; later material/color/strip
processing and final draw/presentation visibility are not separately observed.

## Control and preservation

Positive B and negative C captures each close cleanly with three native frames
1804-1806, exit 0. C changes only the diagnostic expected context address by
xor 32 (00509700 to 00509720). The context check correctly fails; actual copied
context/slot, parsed root/child, vertex index, packet and xyz remain identical.
Both runs match all nine named native image planes of the restored be6a91b9e
baseline: 27 each, 54 comparisons. As before, retained later images prove
preservation, not ownership of the earlier witnessed vertex by frame 1804.

The combined inspector requires the entire earlier TA-input chain, one ordered
copy/decode pair, correct bounds/generation, actual context and xyz equality,
failing wrong-context expectation and native images. Five synthetic parser
test methods pass, including ten malformed fields, missing negative, invalidation
and event reordering. They mock the preceding TA inspector and do not execute
the guest; the real paired capture invokes all prior inspectors.

## Lifetime and failures

Temporary shared state uses a mutex and atomic phase between CPU and render
threads. It invalidates on an observed copied-slot overwrite, list initialization
or context recycle before decoding. It carries only one copied generation;
context pointers are compared rather than dereferenced from the wrong thread.
No unsynchronized render FrameCount is read by the CPU probe. This is bounded
to this disposable capture, not a complete production ownership protocol.
Save-state/reset/device transitions, unseen writers, all decoder variants,
multi-context/reused-slot ABA and all-title behavior are not proven.

Attempt A copied the packet but compared the payload pointer directly with the
packet start, so no decode matched before context recycle. It is an unaccepted
decode attempt, retained as evidence. Source inspection identified the four-byte
PCW offset; corrected B and wrong-context C verify. One source search named a
nonexistent ta_vtx.h and returned an error; the actual ta_structs.h layout was
then inspected. All three builds/game captures in this tranche return 0.

All probes and the temporary header are removed. Exact tracked-file patches
and corresponding header snapshots are retained in ignored build-neural-automation
as fc067-context-positive.patch / -negative.patch and
fc067-context-positive-header.h / -negative-header.h. Raw captures remain in
sibling flycast-evidence/fc067-context-working-a, -b and -c-negative.
No proprietary binary, configuration, media or raw asset is staged.

After removing the probes, all four restored-source configure/builds return 0.
Three enabled selftests pass 243/243 each; SDK mock passes 56/56 with runtime,
GPU and presentation false. The combined source/TA/context parser suite passes
25 methods and five binary32 tests pass. Exact-commit rebuild/capture evidence
is retained separately in the ignored postcommit note.

## Next concrete task: source semantics, not another lineage census

The source transform now has an actual decoded-vertex witness. Assess what the
recorded matrix/vector and observed reciprocal/scale/offset operations imply
about projection, local/model/view transforms and recoverable camera parameters.
An exploratory row-normalization calculation gives approximately
614.714361/565.537123/0.99999982 row norms and nearly orthogonal normalized axes.
This is a hypothesis for a scaled rigid affine composite, not a camera gate.

Build an independent bounded algebra/reprojection check with wrong-layout,
wrong-scale and wrong-decomposition controls. Explicitly demonstrate remaining
model/view ambiguity rather than selecting a convenient world basis and calling
it recovered. If more source evidence is necessary, acquire only the minimum
additional causally linked samples needed to distinguish competing explanations.
Return the precise supported/unsupported scene-contract disposition. Do not
repeat FTRV/SQ/TA/decode presence checks, guess FOV/normals, revive the rejected
framebuffer-light approximation or start runtime integration prematurely.
