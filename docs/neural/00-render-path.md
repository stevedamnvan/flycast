# DX11 render path at 12bb436

This audit predates neural production-path changes.

## Frame ownership

1. `rend_start_render()` derives `isRTT`, clear state, swap interval and queues
   render/present work (`core/hw/pvr/Renderer_if.cpp:500-577`).
2. The PVR render thread dequeues a `TA_context`, computes scaled framebuffer
   dimensions, and defines display-bound work as `!isRTT &&
   !config::EmulateFramebuffer` (`Renderer_if.cpp:195-214`).
3. It calls `Renderer::Process()` and then `Renderer::Render()`
   (`Renderer_if.cpp:215-249`). `DX11Renderer::Process()` retains the context,
   updates display swap interval, and invokes the real TA parser
   (`core/rend/dx11/dx11_renderer.cpp:320-331`).
4. The DX11 renderer selects an RTT target or the display framebuffer in
   `configVertexShader()` (`dx11_renderer.cpp:354-386`), uploads parsed vertex,
   index, and modifier-volume buffers (`dx11_renderer.cpp:389-421`), then calls
   `drawStrips()` (`dx11_renderer.cpp:474-504`).
5. RTT frames are copied back to VRAM; framebuffer-emulation frames are written
   to VRAM; only display-bound geometry reaches presentation
   (`dx11_renderer.cpp:505-532`). The OIT renderer follows the same outer split
   at `core/rend/dx11/oit/dx11_oitrenderer.cpp:640-699`.
6. OIT performs its final A-buffer resolve in `renderABuffer(true)`
   (`dx11_oitrenderer.cpp:634-638`). The resolve sorts fragments and blends
   back-to-front in `core/rend/dx11/oit/dx11_oitshaders.cpp:509-669`.
7. `displayFramebuffer()` clears the swapchain target, derives the content box
   with `getWindowboxDimensions()`, and blits the internal framebuffer into it
   (`dx11_renderer.cpp:535-586`). Aspect selection is in
   `core/rend/transform_matrix.cpp:306-327` and geometry widescreen transforms
   are at `transform_matrix.cpp:99-127`.
8. Flycast OSD is composited after the scene blit through `drawOSD()` and
   `gui_display_osd()` (`dx11_renderer.cpp:515-521,1196-1202`; OIT
   `dx11_oitrenderer.cpp:680-688`). ImGui is drawn later in
   `core/rend/dx11/dx11context.cpp:257-274`.
9. `Renderer::Present()` reaches `DX11Context::Present()`, which uses vsynced
   `Present(interval, 0)` or nonblocking tearing / `DO_NOT_WAIT`
   (`dx11context.cpp:231-255`). The generic presented flag is owned at
   `Renderer_if.cpp:264-276`.
10. `RenderLastFrame()` only re-blits the retained framebuffer
    (`dx11_renderer.cpp:941-947`); it does not re-run the PVR draw path.

## Direct framebuffer and history boundaries

VRAM-direct display is scheduled from vblank (`Renderer_if.cpp:600-617`) and
handled by `DX11Renderer::RenderFramebuffer()` (`dx11_renderer.cpp:949-1047`).
It converts VRAM to a texture, blits it, then draws OSD. Save-state PVR calls are
`core/hw/pvr/pvr.cpp:67-104`; renderer state serialization is implemented at
`Renderer_if.cpp:681-702`.

## Shader/depth facts

The normal vertex shader applies `transMatrix` at
`core/rend/dx11/dx11_shaders.cpp:48-83`. `DIV_POS_Z` divides by input Z and
uses W for hardware Z; the legacy path fixes clip Z/W and writes logarithmic
depth in the pixel shader (`dx11_shaders.cpp:353-362`). Modifier volumes have a
separate vertex path (`dx11_shaders.cpp:88-125`) and depth-writing pixel entry
(`dx11_shaders.cpp:365-383`). Naomi 2 uses per-draw MV/normal/projection
matrices (`core/rend/dx11/dx11_naomi2.cpp:21-120,418-450`).

## Device and version

DX11 selects the high-performance adapter and creates the D3D11 device at
`core/rend/dx11/dx11context.cpp:73-116`; swapchain creation is at
`dx11context.cpp:131-187`. The generated `GIT_VERSION` symbol comes from
`CMakeLists.txt:138-176` and `core/version.h.in:3` and is suitable for NGX init
metadata.
