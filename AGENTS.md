# Flycast agent entry point

## Current assignment

Read `docs/neural/REMAKE-FEASIBILITY-PLAN.md` first, then the current-priority
sections of `COURSE-CORRECTION-flycast-dlss5.md`, `docs/neural/BACKLOG.md`, and
`TESTER-HANDOFF-flycast-dlss5.md`. Read `docs/neural/DECISIONS.md`, `LOG.md`,
`DIAGNOSTICS.md`, `00-render-path.md`, and `QUALITY-PLAN.md` as required for the
code paths touched. D-084 and the new plan supersede historical next-task text,
not existing safety contracts or evidence.

The active bounded implementation is **FC-067 / M1: reuse-first scene adapter
and synthetic feasibility harness**, not a production renderer replacement.
Use the current `feat/neural-rendering` checkout; inspect HEAD and preserve
newer and dirty work. Implementation routing requested by the user is
`gpt-6-astra`, reasoning `low` (user: Astra light). This document records routing intent, not proof
that the live model was switched. Do not spawn additional agents by default.

## Invariants

- Preserve native fallback, feature-off behavior, public DLAA/SR, existing
  D3D11On12 plus supplied-consumer provenance, accepted-history ownership,
  RTT/direct-framebuffer bypass, and late protected overlays/OSD/ImGui.
- No private Feature 18 code or undocumented neural parameters. Do not inspect,
  patch, download, bundle, or redistribute proprietary neural binaries.
- Public open-source renderer source/header inspection and isolated builds are
  allowed for FC-067, subject to dependency/license review. Never auto-fetch
  proprietary dependencies or stage external source trees/binaries/media.
- Do not change live external configuration. FC-050 explicit user Apply and
  previously authorized restored test sweeps are narrow exceptions, not M1
  authorization to edit active consumer or Remix settings.
- No resets, cleans, stashes, rebases, broad staging, or discarding user work.
  Stage only owned source/docs; exclude captures, dependencies, local paths,
  proprietary assets, and third-party configurations.
- Record actual commands, exit codes, exact SHAs, failures, and claim limits.
  Build configurations serially: they share a generated version header.
  Run every test claimed. Never infer scene correctness from module detection,
  a README, a synthetic image, or one still screenshot.

## Return contract

Complete one independently testable slice; update the backlog, decisions, log,
and handoff with `ACCEPTED`, `CORRECTIONS_REQUIRED`, or `NOT_REVIEWABLE` evidence
and the next concrete task. Acceptance is scoped to that slice, never implicit
approval of the entire remake or original Gates 11-18. Do not resume blocked
hardware/title work ahead of FC-067 unless a new regression requires it.
