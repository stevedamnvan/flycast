# Playable Soulcalibur remaster: step-by-step plan

Status: **active execution order** (D-243, user direction 2026-09-23). This plan
replaces the "Next deliverable" work in BACKLOG.md. BACKLOG.md still owns the
release checklist ("Working-pipeline acceptance"), which applies only at the end
(Phase 9). AGENTS.md hard rules still apply everywhere.

Goal: a build the user can **play** soon, then **optimize** it until it runs well
and looks like a real remaster (better lighting, characters, hair, blades), while
never breaking what already works.

---

## 0. How to use this plan (read every time)

1. Work on **one task at a time**, in the order written. Do not skip ahead.
2. Every task has: **Do**, **Check**, **Pass when**, **If it fails**.
   Only mark a task done when "Pass when" is true, with numbers or a picture.
3. After each task: add a short LOG entry (newest at the top of
   `docs/neural/LOG.md`, next free number), tick the task in the progress table
   (section 11), and make a clean commit (section 10).
4. **Two-attempt rule:** if a task fails twice with two *different* fixes, stop
   that task, write down what you tried and what happened in the LOG, mark it
   BLOCKED in the progress table, and move to the next task that does not depend
   on it. Never try the same fix twice.
5. **Stop and ask the user** when a task says so, or when you need: the user to
   play, the Toolkit GUI to be started, a download, money, deleting anything, or
   a change to anything outside this repo and the play workspace.
6. Keep LOG entries short (under 15 lines). Numbers, paths, commit SHA, result.
7. Do not build new diagnostics, audits or replay systems unless a task asks for
   it. Do not restart the parked exact-input two-run comparison (LOG1180..1184).

## 1. Facts you need (from LOG1185 and earlier work)

- Repo: `C:\Game Dev\Emulators\flycast`, branch `feat/neural-rendering`,
  remote `fork`. Game: `C:\Game Dev\Emulators\Soulcalibur (USA).chd`.
- Remix runtime: `C:\Game Dev\Emulators\external-runtimes\remix-1.5.2\runtime\.trex\d3d9.dll`.
- Play workspace (outside Git): `C:\Game Dev\Emulators\flycast-play\soulcalibur`.
  Launchers: `Play Soulcalibur (dlss5).bat`, `Play Soulcalibur (dlaa).bat`.
  It has its own `flycast.exe` and `tools\remake-runtime-smoke.exe`.
- Tracked Remix play config: `neuraltest/play/remix-play.conf`. The workspace
  copy `remix-play.conf` must always equal it (copy it after every change).
- Pipeline: Flycast renders -> sends the 3D scene to the Remix helper
  (path tracing) -> image comes back -> DLSS 5 (RenoDX add-on) or DLAA ->
  Flycast adds the native HUD and shows it.
- What was wrong and is now fixed:
  - The only sun pointed along the first camera's view direction, so it grazed
    the floor, hid all shadows and made characters dark. Fixed: a world-space
    sun `-0.635885233 -0.730868090 0.247955248` (default in `remake_play.py`,
    `remake_offline_render.py`, and `--scene-light-direction` in `remake_launch.py`).
  - The sky backdrop was drawn as a lit wall. Fixed on the shrine stage by
    `rtx.skyBoxTextures` in the play config. Other stages still need their hashes.
  - The launcher/helper had test time limits. Fixed for play by
    `remake_play.py` and `FLYCAST_REMAKE_HELPER_PLAY=1`.
- Still wrong (planned below): blades dull (the game's specular colour is not
  sent to Remix, and blade materials need metal/roughness work); hair shows a
  painted highlight band and soft edges; sky only tagged on one stage; the
  Remix Neural Radiance Cache fails to start in every run; 3D image is about
  3-4 frames behind the HUD; output only up to 1280x960.
- Numbers to beat (capture-free 1280x960 benchmark, LOG1116, old lighting):
  present p50 16.3 ms, p95 20.1 ms, p99 25.2 ms, 100% fresh output, mean
  latency 3.0 frames. Emulation speed was within 1% of native (LOG957, 640x480).
- Brightness of the shrine test frame 6364 (`remake_compare_sheet.py`
  geoL): native 136.8, old lighting 89.8, fixed lighting 126.9.

Test frame used everywhere below (call it FRAME):
`C:\Flycast-Evidence\face-evaluation-compact-locked-c\control\run\captures\frame-6364-present-6368`

## 2. Tools (all in `neuraltest/`, run from the repo folder)

| Tool | What it does |
| --- | --- |
| `remake_play.py` | Start a play session (see REMAKE-LAUNCH.md "Play sessions"). `--scripted-input` = unattended smoke test. |
| `remake_offline_render.py NAME FRAME --out-root DIR [--conf 'k = v'] [--direction 'x y z']` | Re-render a saved frame through Remix in seconds. Writes `DIR/NAME/render.png`. |
| `remake_compare_sheet.py FRAME OUT.png RENDER_DIR...` | Native vs renders side by side with brightness numbers (geoL). |
| `remake_sky_hashes.py PACKET --textures rtx-remix/captures/textures` | Prints the `rtx.skyBoxTextures` line for a stage. |
| `remake_launch.py ... --benchmark-warmup 5000 ...` | Capture-free performance benchmark (command in task 5.1). |

Offline renders and benchmarks use the GPU. Run them only when no other
`flycast.exe` or `remake-runtime-smoke.exe` is running (section 3).

## 3. Start of every session (always do this first)

1. `git status --short` and `git log --oneline -5`. If files you did not write
   are modified, do not touch them; ask the user if they block you.
2. `git fetch fork` and confirm `git rev-parse HEAD` equals
   `git rev-parse fork/feat/neural-rendering`. If not, stop and ask.
3. Disk space: `Get-CimInstance Win32_LogicalDisk` (PowerShell). If C: or D:
   has under 20 GB free, stop and ask the user.
4. GPU users: `tasklist /FI "IMAGENAME eq flycast.exe"` and
   `tasklist /FI "IMAGENAME eq remake-runtime-smoke.exe"`. Other sessions (RE4
   Dreamcast, under `C:\Flycast-Evidence\re4-dreamcast\`) run many Flycast
   copies for timing measurements. If any are running, do only non-GPU tasks
   (docs, Python tools, reading logs) or wait. **Never kill a process you did not
   start.** Never build while a game or helper is running.
5. Read the progress table (section 11) and continue at the first task that is
   not DONE or BLOCKED.

---

## Phase 1: the play route works

### Task 1.1: scripted smoke test, DLSS 5 look
- **Do:** (no other Flycast running) from the repo:
  `python neuraltest/remake_play.py --workspace "C:/Game Dev/Emulators/flycast-play/soulcalibur" --helper "C:/Game Dev/Emulators/flycast-play/soulcalibur/tools/remake-runtime-smoke.exe" --runtime "C:/Game Dev/Emulators/external-runtimes/remix-1.5.2/runtime/.trex/d3d9.dll" --game "C:/Game Dev/Emulators/Soulcalibur (USA).chd" --remix-config "C:/Game Dev/Emulators/flycast-play/soulcalibur/remix-play.conf" --look dlss5 --scripted-input --logs "C:/Game Dev/Emulators/flycast-play/soulcalibur/play-logs/smoke-dlss5-<date>"`
  It runs about 3-4 minutes and ends by itself.
- **Check:** in the logs folder `helper-g1.log` contains `play_session=1`;
  count `live_return` lines (sampled every 600); `live source failed` absent.
  Workspace `flycast.log` size.
- **Pass when:** `play_session=1`, no `live source failed`, `flycast.log`
  under 10 MB, console ends with `Flycast exited`. The final Flycast exit code
  is 0x80000003 because the input script ran out; that is expected here only.
- **If it fails:** read the last 50 lines of `helper-g1.log` and `flycast.log`,
  record them in the LOG, apply the two-attempt rule.

### Task 1.2: same smoke test, DLAA look
- **Do/Check/Pass:** as 1.1 with `--look dlaa` and a new `--logs` folder.

### Task 1.3: screenshot check
- **Do:** during 1.1 (after about 100 s), capture the Flycast window (PowerShell
  `CopyFromScreen` of its window rect, as in LOG1185) to the logs folder.
- **Pass when:** fighters are lit, cast shadows on the floor, HUD readable.
  Put the image path in the LOG.

## Phase 2: the user plays (stop and ask)

### Task 2.1: first real play session
- **Do:** ask the user to double-click `Play Soulcalibur (dlss5).bat` and play
  20-30 minutes: arcade mode, at least 3 characters and 3 stages, one window
  resize, then quit by closing the window. Then the same briefly with
  `Play Soulcalibur (dlaa).bat`. Ask them:
  1. Did it crash, freeze, or go back to the old look? When?
  2. Did input feel late compared with normal Flycast?
  3. Any stutter? Where?
  4. What looked worst (characters, hair, blades, stages, sky, shadows)?
  5. DLSS 5 or DLAA: which looked better?
- **Check:** read every `play-logs\<time>\helper-g*.log`; list `[play]` lines
  (session starts/ends) and any `live source failed`.
- **Pass when:** answers and log findings are written in the LOG as a numbered
  problem list. Copy that list into section 11 as new tasks under Phase 3.

## Phase 3: make play reliable (fix what Phase 2 found)

Fix in this order, one problem per commit: (a) crashes/freezes, (b) returning
to the old look (helper ended), (c) window resize/fullscreen, (d) stage and
round changes, (e) long menus, (f) input lag, (g) stutter.

For each problem:
- **Do:** reproduce it with `--scripted-input` if possible, else ask the user.
  Find the cause in `helper-g*.log` / `flycast.log` (use `--verbose-log` on the
  play launcher when you need renderer detail). Make the smallest fix.
- **Pass when:** the smoke test (1.1) still passes and the user confirms the
  problem is gone (or your reproduction no longer shows it).

Known candidate: if a helper dies mid-game, the launcher prints
`Remix session N ended` and Flycast shows the native image until the renderer
asks for a new session. If the user sees this, test whether the launcher can
start a replacement helper on the same generation; if Flycast does not accept
it, record that and ask the user before any C++ change.

## Phase 4: correct lighting and sky on every stage

### Task 4.1: add stage capture to the play launcher
- **Do:** add to `remake_play.py` options `--capture-at-source N` and
  `--capture-frames K` (K 1..30). They set the same environment the diagnostic
  launcher uses for captures: `FLYCAST_REMAKE_MOVING_CAPTURE=1`,
  `FLYCAST_REMAKE_PREVIEW_CAPTURE=<logs>\captures`,
  `FLYCAST_REMAKE_PREVIEW_CAPTURE_FRAMES=K`,
  `FLYCAST_REMAKE_PREVIEW_START_SOURCE=N` (see `remake_launch.py`, search
  `PREVIEW_CAPTURE`). Add a unit test in `test_remake_launch.py` style.
- **Check:** scripted smoke test with `--capture-at-source 5400 --capture-frames 3`.
- **Pass when:** `<logs>\captures\frame-*\remake-view.bin` and
  `original-native.png` exist for 3 frames, and the play smoke still passes.

### Task 4.2: capture every stage
- **Do:** make a stage table in section 11 (one row per stage the user reaches).
  For each stage, ask the user to start a Practice/Versus match on that stage
  with the play launcher using `--capture-at-source N --capture-frames 3`,
  where N is about 60 x (seconds from launch until they are fighting). Open
  `original-native.png` to confirm the stage. If it is the wrong screen, adjust
  N once and repeat.
- **Pass when:** every stage row has a capture folder path.

### Task 4.3: sky hashes per stage
- **Do:** for each stage capture folder F:
  `python neuraltest/remake_offline_render.py sky-<stage> F --out-root C:/Flycast-Evidence/stage-lighting-<date> --env DXVK_RTX_CAPTURE_ENABLE_ON_FRAME=60 --env DXVK_DISABLE_ASSET_REPLACEMENT=1 --env FLYCAST_REMAKE_HELPER_LINGER_MS=18000`
  then `python neuraltest/remake_sky_hashes.py F/remake-view.bin --textures rtx-remix/captures/textures`.
  Add the printed `ok` hashes to the `rtx.skyBoxTextures` line in
  `neuraltest/play/remix-play.conf` (keep existing hashes; comma separated).
  Look at REVIEW rows against the native picture; add one only if it is clearly
  far scenery. Never add SHARED rows.
- **Pass when:** every stage has its sky hashes in the tracked config, the
  workspace copy is updated, and a render of each stage with
  `--conf-file neuraltest/play/remix-play.conf` shows sky, not a grey wall.

### Task 4.4: sun check per stage
- **Do:** for each stage:
  `python neuraltest/remake_offline_render.py sun-<stage> F --out-root <same> --conf-file neuraltest/play/remix-play.conf`
  and `python neuraltest/remake_compare_sheet.py F <same>/sheet-<stage>.png <same>/sun-<stage>`.
- **Pass when:** on each stage, character shadows fall the same general way as
  in native and geoL is within 15 percent of native geoL.
- **If a stage fails:** try at most three other directions (keep Y negative,
  unit length) and record which works. If stages need different suns, write a
  LOG note and add a follow-up task "per-stage sun" (keyed by that stage's sky
  hashes); do not hack it in without a task.

## Phase 5: performance

### Task 5.1: benchmark with the new lighting
- **Do:** (no other GPU users) copy the play config to a new evidence folder
  and run the capture-free benchmark:
  `python neuraltest/remake_launch.py --flycast <copy of workspace flycast.exe in a new evidence workspace> --harness build-neural-automation/neuraltest/neuraltest.exe --helper build-neural-automation/neuraltest/remake-runtime-smoke.exe --runtime <runtime> --game <game> --out <evidence>/run --anchored-light --managed-session --output-size 1280x960 --smooth-normals-weld --alpha-cutout --observation-scope narrow --consumer-config <evidence>/remix-play.conf --benchmark-warmup 5000 --benchmark-fill "-0.487994879 -0.284207851 0.825279772 0.3" --scene-light-direction "-0.635885233 -0.730868090 0.247955248" --run --benchmark-selective-resource-refresh`
  The evidence workspace is a copy of the play workspace with `reshade-dlss5.ini`
  as `reshade.ini` (see `C:\Flycast-Evidence\shrine-attacks-performance-1280-b\off\workspace`).
- **Check:** `<evidence>/run/host/performance.json`:
  `percentiles_ms.present_interval_cpu` p50/p95/p99,
  `presentation_cadence.output_frame_repeats`, `native_presents`,
  `latency_frames_mean`, `vram.growth_bytes`.
- **Pass when:** it completes and the numbers are in the LOG next to the
  LOG1116 numbers. (Recording is the pass; improving is task 5.3.)

### Task 5.2: Neural Radiance Cache failure
- **Do:** in a helper log find `Failed to initialize NRC ... see error log`;
  read the Remix log (`rtx-remix/logs/remix-dxvk.log` or the run's copy) lines
  around it; search the Toolkit docs in
  `external-runtimes/remix-toolkit/docs` for the option names shown.
- **Pass when:** either NRC initialises (log no longer shows the failure) with
  a config-only change, or the LOG records the exact error and why it cannot be
  fixed by config. Do not download or replace runtime files.

### Task 5.3: reach the performance targets
Targets at 1280x960, DLSS 5 look, capture-free benchmark:
present p50 <= 16.7 ms, p95 <= 20 ms, p99 <= 25 ms, fresh combined output
>= 99 percent after 120 warm-up frames, latency mean <= 3 frames, VRAM growth
not positive over the run, emulation speed within 1 percent of native (method:
LOG957 paired real-time-audio run).
- **Do:** only if 5.1 misses a target. Take the largest cost first (use
  `--cpu-timing` diagnostic run, LOG807..845 method) and change one thing per
  commit. Re-run 5.1 after each change.
- **Pass when:** all targets are met in two runs, or two different fixes for the
  biggest cost both failed (then BLOCKED with numbers).

### Task 5.4: higher output resolution (optional, after 5.3 passes)
- Currently only 640x480 and 1280x960 exist. Adding 1920x1440 needs code in
  the host, helper and launchers. Ask the user before starting.

## Phase 6: blades and metal

### Task 6.1: blade materials (needs the Toolkit GUI: stop and ask)
- **Do:** ask the user to start the RTX Remix Toolkit with the Soulcalibur
  project so its MCP server runs (AGENTS.md pilot rules; port 8002 was last
  used). Find the blade material hashes (weapon review notes: LOG1178, blade
  work in `C:\Flycast-Evidence\blade-metal-review-b`). In a **new separate layer**
  (never the baseline `mod.usda`), set blade materials to metallic 1.0 and
  roughness about 0.15-0.25. Save and export to the mod.
- **Check:** offline render of FRAME before and after with the play config;
  compare sheet; crop the katana at 3x.
- **Pass when:** blades show bright reflections of sky/sun like native steel,
  nothing else changed (compare the rest of the sheet), baseline `mod.usda`
  SHA unchanged: `e3c097905777002034a4983a166e061a6278ca4d70db9686f5dcd49f263e8340`.
  Then ask the user to look at it in play.

### Task 6.2: game specular colour (only if 6.1 is not enough; ask first)
The Dreamcast "offset" (specular) colour is captured but not sent to Remix
(`core/rend/neural/remake_view_transport.cpp` exports only `col`). Sending it
means a packet format change on both sides. Write a design note and ask the
user before implementing.

## Phase 7: hair remaster

Do these in order; each one is judged by offline renders first and then by the
user in play. Hair is judged in motion too (play), not only in stills.

### Task 7.1: re-check the existing hair candidate under the new lighting
- A hair texture candidate with the painted highlight band reduced and
  byte-exact alpha already exists: layer `layers/hair_band_current_a.usda`
  (muted; LOG1170..1174). Old reviews were under the broken lighting.
- **Do:** with the Toolkit MCP (ask the user to start it), unmute the layer in
  a review session, render FRAME offline with the play config, compare against
  the same render with the layer muted. Mute it again afterwards.
- **Pass when:** sheet recorded and the user says keep or reject.

### Task 7.2: hair edges (alpha)
- The hair uses blended alpha; edges look soft. An opt-in cutout route exists
  (`--alpha-cutout`, `FLYCAST_REMAKE_ALPHA_CUTOUT`, D-240). The hair audit found
  the source texture has 1 mip level but the candidate 9 (LOG1179).
- **Do:** add `--alpha-cutout` as an option to `remake_play.py` (same env as
  `remake_launch.py`), and make the candidate's mip count match the source.
  Compare hair edges in play with and without.
- **Pass when:** the user prefers one; make it the default only if they do.

### Task 7.3: hair material response
- **Do:** through MCP, on hair materials only: roughness about 0.4-0.6,
  metallic 0; if the material exposes subsurface/transmittance (the installed
  `AperturePBR_Opacity.mdl` has `subsurface_transmittance_color`), try a warm
  transmittance so backlit hair glows slightly. One change per render; keep the
  painted band out of any height/normal map (LOG1168 rejected that).
- **Pass when:** hair reads as soft strands with a moving highlight, not a
  painted stripe, in offline renders and in play, and the user approves.

### Task 7.4: DLSS 5 settings for hair and faces
- DLSS 5 adds strand and skin detail but darkened faces when its input was dark
  (LOG1182). Its input is now correctly lit.
- **Do:** ask the user to play with `Play Soulcalibur (dlss5).bat` and press
  **Home** to open the ReShade/DLSS 5 overlay; try the intensity/style settings
  on hair and faces. Record their preferred settings, then save them in
  `reshade-dlss5.ini` in the workspace.
- **Pass when:** the user's chosen settings are recorded in the LOG.

### Task 7.5: new hair geometry (research only; ask first)
Replacing hair meshes with modern hair cards is the biggest possible upgrade,
but Remix's mesh replacement keys on geometry hashes that include vertex
positions, and the game skins characters on the CPU every frame, so those
hashes change every frame. It would need helper-side replacement keyed on the
packet's stable mesh identity plus new hair assets. Write a one-page design and
cost estimate; do not implement without user approval.

## Phase 8: skin, faces and full coverage

- **8.1 Skin:** as 7.3 for skin materials (roughness about 0.5, subsurface
  transmittance warm), judged offline then in play. User approves.
- **8.2 Coverage:** for each stage/character the user reaches, check that
  materials exist (BACKLOG "Delivery status": 67/67 captured character groups,
  137/154 world groups). Add missing ones with the existing Package D process
  (BACKLOG section "Package D / FC-067"). Record counts per stage.
- **8.3 Costumes and hidden characters:** list what exists in the game; process
  like 8.2. Ask the user which matter most.

## Phase 9: release candidate

- Run the BACKLOG "Working-pipeline acceptance" checklist items that apply,
  using the play build: 600-frame normal and OIT runs, 300-frame synchronized
  native/DLAA/Remix/combined evidence, lifecycle checks (resize, reload,
  save-state, shutdown), emulation within 1 percent.
- Package: the play workspace as a folder with a README (how to start, looks,
  known limitations), evidence index in the LOG, final commit pushed.
- Never call it "production ready" or "60 fps" unless the numbers passed; the
  user decides on looks.

## 10. Clean commit (every commit)

1. `git status --short`: only your files. Never stage `rtx-remix/`, logs,
   caches, `build-*` (ignored anyway; never force-add). Never commit anything
   from game media.
2. If you changed C++ or headers: build the four configurations one after
   another from PowerShell in the repo:
   `cmd.exe /c '"C:\BuildTools\VS2022\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build-neural-automation && cmake --build build-neural-baseline && cmake --build build-neural-no-ngx && cmake --build build-neural-off'`
   then `build-neural-<automation|baseline|no-ngx>/neuraltest/neuraltest.exe selftest`
   (each must end `failed=0`).
3. If you changed Python in `neuraltest/`: in `neuraltest/` run
   `python -m unittest test_remake_launch` (must say OK).
4. If you changed BACKLOG.md: `python neuraltest/backlog_contract_inspect.py`.
5. `git add <each file by name>` (never `-A`, `.` or `commit -a`).
6. Commit message: `type(neural): what changed` + blank line + short why +
   the attribution line your environment requires.
7. `git push fork feat/neural-rendering`; confirm HEAD equals
   `fork/feat/neural-rendering`; `git status --short` is empty.
8. If you changed the helper or Flycast and the play workspace should use it,
   copy `build-neural-automation/flycast.exe` to the workspace and
   `build-neural-automation/neuraltest/remake-runtime-smoke.exe` to
   `workspace/tools/`, then repeat task 1.1.

## 11. Progress table (update in place)

| Task | State | Evidence / LOG | Date |
| --- | --- | --- | --- |
| 1.1 smoke DLSS 5 | TODO | | |
| 1.2 smoke DLAA | TODO | | |
| 1.3 screenshot | TODO (LOG1185 smoke-a screenshot exists) | | |
| 2.1 user play | TODO | | |
| 3.x fixes from 2.1 | TODO (add rows) | | |
| 4.1 stage capture option | TODO | | |
| 4.2 capture every stage | TODO (add a row per stage) | | |
| 4.3 sky hashes | shrine DONE (LOG1185) | | 2026-09-23 |
| 4.4 sun per stage | shrine DONE (LOG1185) | | 2026-09-23 |
| 5.1 benchmark | TODO | | |
| 5.2 NRC failure | TODO | | |
| 5.3 perf targets | TODO | | |
| 6.1 blade materials | TODO | | |
| 7.1 hair candidate re-check | TODO | | |
| 7.2 hair edges | TODO | | |
| 7.3 hair material | TODO | | |
| 7.4 DLSS 5 settings | TODO | | |
| 8.1 skin | TODO | | |
| 8.2 coverage | TODO | | |
| 9 release candidate | TODO | | |
