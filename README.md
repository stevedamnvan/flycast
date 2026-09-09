# Flycast — Experimental Neural Rendering Fork

This fork explores cleaner Dreamcast reconstruction and more transformative
rendering through public NVIDIA NGX DLAA/SR, externally supplied DLSS 5 Neural
Rendering, and an in-development RTX Remix integration. It builds on
[upstream Flycast](https://github.com/flyinghead/flycast); it is not an official
Flycast or NVIDIA release.

**Development branch:** [`feat/neural-rendering`](https://github.com/stevedamnvan/flycast/tree/feat/neural-rendering).
Switch to that branch for this work; the fork's default `master` branch does
not represent the experimental development checkpoint.

## What works, and what is still experimental

- Public NGX DLAA and Super Resolution are separate from external Neural Rendering.
- External DLSS 5 transport and presentation provenance have been demonstrated
  on the recorded D3D11On12 plus user-supplied consumer route. This is not a
  bundled or private DLSS 5 implementation, nor a guarantee for every setup.
- Neural rendering is off by default, with native rendering as the fallback.
  Public DLAA Auto remains the Faithful baseline; Uncanny is a user-selected,
  deliberately less faithful experiment, not a proven quality winner.
- RTX Remix scene/camera preparation and a standalone runtime test harness
  are under development. Actual Remix GPU rendering and the combined
  Remix → external DLSS 5 → Flycast presentation path are **not yet proven**.

See the [live backlog](docs/neural/BACKLOG.md) for scoped acceptance and
remaining work, and the [evidence log](docs/neural/LOG.md) for tests actually run.
Neither API success nor a mock test is treated as proof of rendered gameplay.

## Getting started with the experiment

1. Check out `feat/neural-rendering` with its submodules. Start with the
   [neural rendering guide](docs/neural/README.md) and
   [diagnostics/setup reference](docs/neural/DIAGNOSTICS.md).
2. Use an appropriately configured Windows x64 DX11 build. NGX and external
   consumer components are separate dependencies; upstream downloads below
   are not builds of this fork's experimental branch.
3. Open **Video → Neural Rendering (Experimental)**. Public DLAA/SR and
   **DLSS 5 Experimental** are distinct choices. External Neural Rendering
   additionally requires a compatible, separately installed consumer.
4. For intensity/style changes, see
   [External intensity panel](docs/neural/README.md#external-intensity-panel).
   Pending sliders are not proof of active settings. Explicit Apply uses a
   separately installed companion; restart and verify the consumer's active
   state. Keep game overlay protection enabled.

No game media, proprietary neural binaries, external consumer configuration,
or third-party runtime is distributed by this repository. Supply your own
legally obtained media and required components. The experimental features
are not production-ready and have not passed a complete representative-title
quality and stability matrix.

## Development references

- [Current work and acceptance checklist](docs/neural/BACKLOG.md)
- [Rendering quality plan](docs/neural/QUALITY-PLAN.md)
- [RTX Remix feasibility and architecture](docs/neural/REMAKE-FEASIBILITY-PLAN.md)
- [Isolated Remix runtime bring-up](docs/neural/REMAKE-RUNTIME-BRINGUP.md)
- [Architectural decisions](docs/neural/DECISIONS.md)

## Upstream Flycast

The following information, downloads, CI badges, and community links describe
upstream Flycast. Upstream CI status does not validate this fork's changes.

[![Android CI](https://github.com/flyinghead/flycast/actions/workflows/android.yml/badge.svg)](https://github.com/flyinghead/flycast/actions/workflows/android.yml)
[![C/C++ CI](https://github.com/flyinghead/flycast/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/flyinghead/flycast/actions/workflows/c-cpp.yml)
[![Nintendo Switch CI](https://github.com/flyinghead/flycast/actions/workflows/switch.yml/badge.svg)](https://github.com/flyinghead/flycast/actions/workflows/switch.yml)
[![Windows UWP CI](https://github.com/flyinghead/flycast/actions/workflows/uwp.yml/badge.svg)](https://github.com/flyinghead/flycast/actions/workflows/uwp.yml)
[![BSD CI](https://github.com/flyinghead/flycast/actions/workflows/bsd.yml/badge.svg)](https://github.com/flyinghead/flycast/actions/workflows/bsd.yml)

<img src="shell/linux/flycast.png" alt="flycast logo" width="150"/>

**Flycast** is a multi-platform Sega Dreamcast, Naomi, Naomi 2, and Atomiswave emulator derived from [**reicast**](https://github.com/skmp/reicast-emulator).

Information about configuration and supported features can be found on [**TheArcadeStriker's flycast wiki**](https://github.com/TheArcadeStriker/flycast-wiki/wiki).

Join us on our [**Discord server**](https://discord.gg/X8YWP8w) for a chat.

## Downloads ![android](https://flyinghead.github.io/flycast-builds/android.jpg) ![windows](https://flyinghead.github.io/flycast-builds/windows.png) ![linux](https://flyinghead.github.io/flycast-builds/ubuntu.png) ![apple](https://flyinghead.github.io/flycast-builds/apple.png) ![switch](https://flyinghead.github.io/flycast-builds/switch.png) ![xbox](https://flyinghead.github.io/flycast-builds/xbox.png)

Get builds for your system from the [**builds page**](https://flyinghead.github.io/flycast-builds/) or [**GitHub Releases**](https://github.com/flyinghead/flycast/releases).

- **Latest master builds:** regular builds from the `master` branch with recent fixes and updates.
- **Nightly dev builds:** experimental builds with the latest features and changes.
- **Stable tagged releases:** versioned release builds published on GitHub Releases.

Automated test results are available from the builds page as well.

## Install

### Android ![android](https://flyinghead.github.io/flycast-builds/android.jpg)

Install Flycast from [**Google Play**](https://play.google.com/store/apps/details?id=com.flycast.emulator).

### Flatpak (Linux ![ubuntu logo](https://flyinghead.github.io/flycast-builds/ubuntu.png))

1. [Set up Flatpak](https://www.flatpak.org/setup/).

2. Install Flycast from [Flathub](https://flathub.org/apps/details/org.flycast.Flycast):

`flatpak install -y org.flycast.Flycast`

3. Run Flycast:

`flatpak run org.flycast.Flycast`

### Homebrew (macOS ![apple logo](https://flyinghead.github.io/flycast-builds/apple.png))

1. [Set up Homebrew](https://brew.sh).

2. Install Flycast via Homebrew:

`brew install --cask flycast`

### iOS

Due to persistent harassment from an iOS user, support for this platform has been dropped.

### Xbox One/Series ![xbox logo](https://flyinghead.github.io/flycast-builds/xbox.png)

Grab the latest build from [**the builds page**](https://flyinghead.github.io/flycast-builds/), or the [**GitHub Actions**](https://github.com/flyinghead/flycast/actions/workflows/uwp.yml). Then install it using the **Xbox Device Portal**.

## Build from source

### macOS

Right-click the bootstrap script and choose **Open**:

`shell/apple/generate_xcode_project.command`

### Windows

Double-click the bootstrap script:

`shell\windows\generate_vs_project.bat`

### Linux

#### Dependencies

- **C/C++ compiler toolchain** (e.g. `gcc`/`g++`)
- **CMake**
- **make**
- **libcurl** (development headers)
- **libudev** (development headers)
- **SDL2** (development headers)
- **Graphics API**: Vulkan, OpenGL

#### Build

```
$ git clone --recursive https://github.com/flyinghead/flycast.git
$ cd flycast
$ mkdir build && cd build
$ cmake ..
$ make
```

## Packaging status

[![Packaging status](https://repology.org/badge/vertical-allrepos/flycast.svg)](https://repology.org/project/flycast/versions)
