# FC-067 second combat transform: projection to a bounded RAM span

Starting HEAD: `7bbdb0ebb162f6713d280002a766fabca2e48984`, clean.
ACCEPTED bounded executed-block/source-arithmetic/RAM observation (self-review).
The source primitive family is unknown. This is not an opaque draw, TA lineage,
world camera, or M2/Remix GPU acceptance.

## Why this site, without another census

Existing retained observations already include `8c0610b4`; unlike the verified
petal site, its translated block reads a contiguous vertex-input loop. The new
temporary probe selects its first executed FTRV within emulated seconds38-39,
then observes exactly eight completed blocks, bounded by 0.1 emulated seconds.
It does not scan RAM for matrices or select a nearest-value vertex match.

After register-allocation Cleanup has flushed registers and the actual next PC
has been chosen, a diagnostic records the block, next PC, relevant registers,
and a pinned 32-byte RAM window derived from the first output-pointer register.
The selected five block identities retain their own compiled instruction
descriptors (128 descriptors/128 operations per descriptor cap). Descriptors
are emitted only when the corresponding generated block actually executes.
Missing metadata is rejected. This is a bounded block-path observation, not a
general instruction tracer or claim that every individual memory access has
its own post-store callback.

## Actual result

The selected FTRV executes at cycle7601913984. Exact input/matrix/output words
are retained outside Git. First executed blocks are `8c0610a0`, `8c0610c2`,
`8c0610d2`; every recorded next-PC matches the next observed block.
The selected transform output survives unchanged into the projection block.

The projection block computes reciprocal z, separate x/y products, a separate
depth multiplier, and separate x/y additions. It writes z/y/x to three descending
addresses, then advances the output pointer to the next16-byte record. The
actual rounded register results equal the pinned RAM snapshot at block exit:

| Component | Guest RAM address | Actual binary32 word |
|---|---|---|
| x | 8ce6e460 | 43dc80af |
| y | 8ce6e464 | 439f9171 |
| exported z | 8ce6e468 | 3d1dec06 |

Observed depth multiplier is `3f733333` (approximately0.95), screen offsets
`43a00000,43700000` (320,240), MXCSR`0000fffd` (toward-zero). The separate
mul/add path is not silently replaced with fused projection. The exact-rational
binary32 oracle agrees with all three final values; the captured FTRV dot/output
check agrees too. This does not linearize production log depth or establish a
physical/common camera scale. In particular, do not substitute the previously
observed petal multiplier for this candidate's observed multiplier.

`combat_path_inspect.py` checks the bounded execution sequence, metadata,
operand dependencies, arithmetic, pointer progression, pinned RAM result and
unchanged neighboring bytes through the first projection block. It rejects five
offline mutations: step identity, reciprocal operands, x-store source, stop
reason and written x word. Six unit methods cover analytic projection, separate
depth scale, wrong offset sign, captured separate-rounding arithmetic, invalid
contracts and word bounds. These are not runtime injection or TA tests.

## Attempts, preservation, and limits

- A build fails on a misspelled existing API (`disasm` versus `dissasm`); log
  retained, no game run claimed for A.
- B builds and captures three frames1804-1806, exit0/clean close. Its early
  compile-list cap misses a required block description, and its moving RAM
  window does not retain the first x word. Those observations only guide C/D.
- C captures the selected blocks' actual compiled descriptors. Host locale
  inserts separators into hex PC text; D explicitly uses the classic locale.
- D pins the first output span and records offsets/MXCSR. Build and three-frame
  capture return0/clean close. Its 27 unique native planes equal the retained
  eb1b67bcf baseline (54 comparisons when duplicated by the shared helper).
  B also matches that baseline. C is not independently labeled a native-parity
  pass merely because the capture completed.

Raw B/C/D runs are sibling `flycast-evidence/fc067-combat-path-working-*`.
Build/report logs are ignored `build-neural-automation/fc067-combat-path-*`.
The exact D probe is retained as `fc067-combat-path-d-probe.patch`; temporary
production edits are removed. No guest state, rendering, external configuration,
neural binary, or texture/game asset is changed or staged. Capture is not timing
or performance evidence.

## Restored-source validation

All four serial configure/builds return 0. Enabled selftests pass 262/262 each;
SDK mock passes 56/56 with runtime/GPU/Present false. All 51 inspector test
methods and five binary32 methods pass. D's actual-input checker passes with
five offline controls. Exact-commit confirmation is retained in an ignored
postcommit note. No temporary source probe is included in the commit.

## Next concrete task

Follow the first produced12-byte span at8ce6e460 from this executed projection
generation through actual reads/copies and toward TA, bounded by event count,
cycle age, overwrite and reset. A later equal value or numeric address reuse is
not a link. Use the existing packet/context/draw association checks if it reaches
TA. If it is another effect or unsupported path, record that precise result;
do not promote it to opaque fighter/arena coverage to keep a camera plan green.
No repeat of the site census or parked1-LSB replay diagnosis is authorized.
