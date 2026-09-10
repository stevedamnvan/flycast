# Flycast agent entry point

## One execution authority

Current handoff: [DEVELOPMENT-HANDOFF.md](DEVELOPMENT-HANDOFF.md). The user
resumed implementation on 2026-09-09; the handoff now records the resumed state
and the in-session anchor generation result. Preserve unfinished work.

Read the active execution section of [the backlog](docs/neural/BACKLOG.md)
first, then its current card and [the remake plan](docs/neural/REMAKE-FEASIBILITY-PLAN.md).
The backlog owns current priority, dependencies, bounded tasks and acceptance.
Do not create a competing roadmap, scheduler or orchestration state file.
Historical audit/LOG instructions describe their old scope; they are evidence,
not active requests to pause or repeat a completed experiment.

The user's 2026-09-08 request authorizes autonomous progression toward a working
RTX Remix plus externally supplied DLSS 5 pipeline. It supersedes routine
per-block/per-milestone approval stops in old task cards, D-111 through D-113,
and earlier handoffs. It does not supersede safety, evidence requirements,
explicitly parked work, or the need for real missing third-party components.

## Execution loop

1. Read HEAD/status and preserve newer work. Select the backlog's active ready
   card, or the first unblocked dependency. Use existing FC identifiers.
2. State a concrete hypothesis/deliverable, bounded work and falsifying check.
   Routine implementation, focused tracing and scope refinement within this
   objective need no new human approval. Record changed bounds before running.
3. Implement and test. Retain failures. Review the actual output, not just logs.
   Apply the backlog's no-progress/pivot rule; no endless one-instruction traces.
4. Update the live card in place plus LOG/DECISIONS and the handoff when needed.
   Never prepend another collection of conflicting "current task" paragraphs.
5. Commit independently proven slices with explicit staging; verify the fork
   SHA and a clean worktree after required checks. Then take the next ready
   card automatically. An accepted checkpoint is not an instruction to stop.
6. When a card lacks media/hardware/runtime, name the exact block and continue
   independent safe work. Escalate only when all useful in-scope routes require
   an external dependency, prohibited action or genuinely new authority.

Requested execution is gpt-6-astra at low reasoning (Astra light), not Sol/high.
This records routing intent, not proof of live model settings. No subagents
by default. The short goal text and last verified tracker state are in BACKLOG.

## Pilot rules (D-224, opt-in "Soulcalibur Faithful RTX")

- Reuse first: the saved source-2601 packet, the existing capture, Toolkit
  project, generated maps, mod loader, launcher flags and comparison tools.
  Do not regenerate the texture set or rebuild Toolkit integration as an
  opening move; no second comparison platform, project or renderer.
- Visual evidence: same-source A/B for stills, then moving combat; repeated-
  baseline noise measured before any changed pixel counts as benefit. Pixel
  equality is for protected invariants and same-input checks, never for
  judging different artwork. Unreviewed looks are NOT_REVIEWABLE, not beautiful.
- Toolkit route: when the installed Toolkit's MCP server is available
  (`lightspeed.trex.mcp.core`, SSE on 127.0.0.1:8000 while the GUI runs, REST
  mounted as tools), pilot Toolkit operations MUST go through it; the scripted
  kit.exe route is the fallback only when MCP is absent. Discover before use.
- Opt-in scope: the profile changes nothing unless explicitly selected; Public
  Auto, Faithful, Uncanny and neural activation modes keep their meaning.
  Experiments live in copied layers and explicitly supplied profiles.
- Bounded material, lighting, and resolution experiments for this one scene may proceed alongside unfinished pipeline hardening. They require their immediate technical dependencies, not completion of every unrelated title/renderer gate. This does not close M2-camera, waive parked failures, or authorize release. Broad asset replacement and title expansion remain later work.

## Invariants

- Preserve native fallback, neural-off default, separately supported public
  DLAA/SR, D3D11On12/supplied-consumer provenance, accepted-history ownership,
  RTT/direct-framebuffer bypass, protected game overlays and late OSD/ImGui.
- No private Feature 18 implementation or undocumented neural parameters.
  Never inspect, patch, download, bundle or redistribute proprietary neural
  binaries. Public source/header inspection and isolated builds require
  dependency/license review; do not auto-fetch proprietary dependencies.
- Do not write live external configurations automatically. Existing explicit
  user Apply and previously authorized byte-restored test sweeps stay narrow
  exceptions, not blanket authorization. User-owned test workspaces are not
  permission to overwrite their runtime/media/configuration.
- Do not acquire proprietary game media. Preserve untracked/private evidence,
  legal media, user paths and worktrees. No resets, cleans, stashes, rebases,
  broad staging or discarding user changes. Commit only owned source/docs.
- Do not invent camera/world-space/material truth or relabel image-space
  approximation as reconstruction. The parked strict replay residual remains
  failed/parked. Never lower acceptance merely to close a goal.
- Reuse completed gates. Rerun only risk-relevant regression coverage; a changed
  combined presentation route requires its own focused provenance proof.
- Build the four configurations serially; they share a generated version file.
  Run every claimed test. Synchronous diagnostic capture is never performance
  evidence. Do not let a slow rendering lane change emulation/audio timing.
- No "production-ready", "optimal" or "highest fidelity" claim before the full
  applicable gates and representative legal-title matrix have actually passed.

## Evidence and return contract

Run `python neuraltest/backlog_contract_inspect.py` after changing the active
queue or routing. It checks document/dependency consistency only; it neither
schedules work nor proves that the recorded rendering evidence is true.

Read LOG, DECISIONS, DIAGNOSTICS, 00-render-path and QUALITY-PLAN as needed for
the touched path. Preserve exact SHAs, commands, exits, negative controls,
images, omissions and observed resource/frame identity. Use ACCEPTED,
CORRECTIONS_REQUIRED or NOT_REVIEWABLE for the scoped slice, separately from
the backlog state. After recording a result, continue to the next ready card.
Only the working-pipeline checklist in BACKLOG closes the standing objective;
a document, mock adapter, one vertex or standalone GPU image does not.
