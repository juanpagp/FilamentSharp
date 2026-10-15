# Windowed (Non-Headless) Rendering

How to put FilamentSharp content on screen in a real OS window, where real
rendering occurs every frame — as opposed to the headless/offscreen path used
by `BasicRendering` and the integration tests. The working sample is
[`samples/WindowedRendering`](../samples/WindowedRendering); this document is
the full walkthrough of what it does and why, plus everything you need to run
it, extend it, or write your own.

## 1. The shape of a windowed application

There are exactly three pieces, and the ownership is deliberately one-directional:

```
┌─────────────────────────┐        ┌──────────────────────────────┐
│  Your application        │        │  FilamentSharp               │
│  (samples/WindowedRendering)     │                              │
│                         │        │  Engine (Vulkan/NOOP)        │
│  X11 window (Xlib)       │        │   └─ Renderer                │
│   └─ Window XID ─────────┼───────►│   └─ SwapChain(XID)          │
│                         │ native │   └─ View ← Scene ← Camera   │
│  event pump (XPending/   │ handle │        └─ renderables/lights │
│   XNextEvent)            │        │                              │
└─────────────────────────┘        └──────────────────────────────┘
```

* **The window is yours.** FilamentSharp never creates, maps, resizes, or
  destroys windows, and it never pumps events. `samples/WindowedRendering`
  does all of that with ~200 lines of AOT-safe Xlib interop (`Xlib.cs`).
* **The bridge takes only a handle.** `Engine.CreateNativeSwapChain(IntPtr)`
  forwards your X11 `Window` id (the XID — the same `nuint` that
  `XCreateSimpleWindow` returned) across the ABI as an opaque `void*`.
  Filament derives the surface from it: on the **Vulkan backend** it creates
  the `VkSurfaceKHR` through XCB from the XID; on the **OpenGL backend** it
  uses the XID directly as the GLX drawable.
* **The frame loop is explicit.** `Renderer.BeginFrame(swapChain)` →
  `Render(view)` → `EndFrame()`. `BeginFrame` returning `false` means
  backpressure (the previous present has not retired); render later, not now.

## 2. Prerequisites (read this before debugging anything)

1. **A reachable X server.** Real display (`DISPLAY=:0` …), a remote X
   forwarding session, or `Xvfb` all work. The sample fails fast with a clear
   message if `XOpenDisplay` fails; exit code `1`.
2. **A usable Vulkan ICD.** Real GPUs expose one through
   `/usr/share/vulkan/icd.d/`. Headless machines can get a software one:

   ```bash
   ./scripts/setup-headless-vulkan.sh   # installs lavapipe under external/
   ```

   `scripts/env.sh` exports `VK_ICD_FILENAMES` automatically when it finds the
   local runtime.

   > **Upstream behavior worth knowing (Filament v1.78.0):** if *no* Vulkan
   > ICD is visible at all, engine creation does not return an error —
   > Filament panics on its worker thread (`PostconditionPanic:
   > createVkInstance ... error=-9`) and the process aborts. The bridge
   > cannot convert a terminate-on-another-thread into an error return. On
   > such machines start the sample with `--backend noop` (explicit, prints a
   > warning, presents frames with no content) — or install an ICD.
3. **libX11.** Present on every desktop Linux; the bridge also links it, and
   `libX11` pulls in `libxcb`, which is what Filament's XCB surface creation
   resolves against. `ldd libFilamentSharp.Native.so` shows the dependency.

## 3. Running the sample

From the repository root (assets are located by walking up from the
executable):

```bash
# Desktop session (DISPLAY already set, real GPU or lavapipe):
dotnet run --project samples/WindowedRendering -- --frames 240

# Headless machine with a software stack — start a virtual X server:
Xvfb :99 -screen 0 1024x768x24 -nolisten tcp &
export DISPLAY=:99
dotnet run --project samples/WindowedRendering -- --frames 240
```

Useful flags: `--width/--height` (initial size), `--frames N` (0 = until the
window is closed; also the automation knob), `--fps N` (target frame rate),
`--title S`, `--backend vk|noop`, `--help`. Quit with the window's close
button, `q`, or `Esc`.

What you should see: a dark window with a lit, red/green checkered quad that
spins while the camera orbits it. On the reference stack (Xvfb + lavapipe,
llvmpipe) the 800×600 run sustains ~63 fps average over 240 frames and exits
with code 0, printing `rendered 240 frame(s), 0 resize(s)` and
`sample completed cleanly`.

### 3.1 CI recipe: screenshots without a monitor

`Xvfb` gives you a real X server with no hardware behind it, which means the
Vulkan-on-XCB path runs for real. To also *capture* pixels in CI:

```bash
Xvfb :99 -screen 0 1024x768x24 -nolisten tcp &
export DISPLAY=:99
./external/aot-publish/WindowedRendering/WindowedRendering --frames 120 &

# after a second, dump the root window (needs x11-apps: xwd)
sleep 1
xwd -root -display :99 | xwdtopnm 2>/dev/null | pnmtopng > shot.png
# pixel-assert shot.png in your usual CI step (same tolerance approach as
# docs/testing.md — software rasterizers round, so allow ±0.12 per channel)
```

`xwd` requires the `x11-apps` package and `netpbm` for the conversion; if you
cannot install them, the sample still verifies itself through its exit code
and frame count, and the headless readback tests cover pixel correctness.

## 4. Walkthrough of `samples/WindowedRendering`

### 4.1 Window setup (`Xlib.cs`, `Program.Run`)

1. `XOpenDisplay(null)` — connects using `$DISPLAY`. Returns null on failure
   (sample prints the DISPLAY value and exits 1).
2. `XCreateSimpleWindow` on the root window — returns the **XID** used later.
   The sample uses the screen's black pixel for border and background so the
   first presented frame does not flash.
3. `XStoreName` — window title.
4. `XInternAtom("WM_PROTOCOLS")` + `XInternAtom("WM_DELETE_WINDOW")` +
   `XSetWMProtocols` — asks the window manager to *notify* the client on
   close instead of killing the connection. The close then arrives as a
   `ClientMessage` we handle in the loop. Without this, clicking ✕ would just
   sever the X connection under our feet.
5. `XSelectInput` with `KeyPressMask | ExposureMask | StructureNotifyMask`:
   key presses (q/Esc), exposure (when we should (re)draw), and
   structure events (map + resize).
6. `XMapWindow` + `XSync` — puts the window on screen and flushes the request
   queue so the map is in flight before we start rendering.

### 4.2 Engine, swap chain, scene (`Program.RenderLoop`)

* `Engine.TryCreate(Backend.Vulkan, out engine)` with NOOP fallback (or
  `--backend noop` to skip the attempt entirely — see §2.2).
* `engine.CreateNativeSwapChain((IntPtr)window)` — the XID crosses the ABI
  as `void*`; Filament creates the surface (XCB) and the presentation queue.
* The scene is the same proven content as `BasicRendering`'s first scene:
  matc-compiled `lit_textured.filamat`, a generated 8×8 sRGB checker texture,
  an interleaved quad (position/uv/tangent — lit shading requires the tangent
  quaternion), a directional light, perspective camera.

### 4.3 The loop

```
while running:
    drain all pending X events        (XPending > 0 → XNextEvent)
        Expose / MapNotify        → exposed = true
        ConfigureNotify (w,h new) → rebuild swap chain, viewport, camera
        KeyPress q/Esc            → running = false
        ClientMessage WM_DELETE   → running = false
    if !exposed: sleep 4ms; continue     (never render into an unmapped window)
    animate (SetTransform spin; camera.LookAt orbit)
    if renderer.BeginFrame(swapChain):
        renderer.Render(view)
        renderer.EndFrame()
    else: sleep 1ms                      (backpressure; retry)
    pace to --fps
```

Two rules do most of the hard work here:

* **Never render before the window is presentable.** Presenting to a surface
  that has no valid extents is undefined behavior in the window system.
  `exposed` gates the first frame.
* **Never hold a frame open across event handling.** Everything between
  `BeginFrame` and `EndFrame` belongs to Filament; the resize path below
  deliberately runs outside that pair.

### 4.4 Resize: the one non-obvious part

On `ConfigureNotify` with a new size, the sample does:

```csharp
swapChain.Dispose();                                 // 1. kill the old surface
swapChain = engine.CreateNativeSwapChain((IntPtr)window);  // 2. same XID
s_width = cfg.Width; s_height = cfg.Height;          // 3. remember it
ApplyCameraAndViewport(camera, view);                // 4. aspect + viewport
```

Why recreate instead of "resizing"? A `VkSurfaceKHR`/GLX drawable is created
from the XID once; its *backing* can adapt, but Filament's contract for
changed window extents is: destroy the swap chain and create a new one bound
to the same handle. `beginFrame` re-derives the current extents, and the
projection/viewport must be updated to match or the image stretches (the
aspect correction is `width / (double)height`; `SetProjection(45.0, aspect,
0.1, 100.0)`; `SetViewport(0, 0, w, h)`).

In real desktops, resizes arrive as a burst of `ConfigureNotify` events; the
sample handles each one. The `swapChain` variable is deliberately **not** in a
`using` block — it is *reassigned* on resize; the live one is disposed in the
teardown section.

### 4.5 Teardown order (matters)

```text
scene.RemoveEntity(mesh/light)        // detach while scene still alive
EntityManager.Destroy(...)            // entities outlive the engine
swapChain.Dispose()                   // surface dies before the renderer
...renderable, light, camera, buffers, texture, instance, material (using)
engine.Dispose()                      // children LIFO inside if anything leaked
XDestroyWindow + XCloseDisplay        // the window outlives the engine
```

The bridge's registry guarantees that anything still alive at
`engine.Dispose()` is destroyed in reverse creation order (instances before
materials — Filament aborts otherwise), so a missed `using` degrades to a
warning, not a crash. But keep the order anyway: it is the documented
contract, and it is what the tests pin.

### 4.6 Frame pacing and present modes

The sample targets `--fps` (default 60) in software; on real hardware you
usually want vsync — Filament presents with FIFO mode by default, in which
case `EndFrame` blocks on the present and the loop self-paces. `BeginFrame ==
false` is the other signal (the swap chain has no free image): sleep briefly
and retry; do not spin. `SwapChainConfig` flags exist for sRGB swap chains
(`SrgbColorSpace`), alpha (`Transparent`), stencil buffers, and 4x MSAA if
your compositor/display path supports them.

## 5. Writing your own non-headless application

1. Create the window with your toolkit of choice — Xlib directly (copy
   `Xlib.cs`), or any wrapper; the sample sticks to raw Xlib to stay
   dependency-free and AOT-safe. What Filament needs from it is only the
   XID plus an event source.
2. `Engine.TryCreate(Backend.Vulkan, ...)` (fallback `Noop`), then
   `engine.CreateNativeSwapChain((IntPtr)xid)`.
3. Build your scene exactly as in the headless samples — the scene layer is
   display-agnostic (`View.SetViewport` sizes the camera to the window).
4. Pump events; on `Expose`/first map set a presentable flag.
5. On resize: dispose + recreate the swap chain (same XID), update viewport
   and camera aspect — outside any frame pair.
6. Per frame: animate, then `BeginFrame` → `Render` → `EndFrame`; treat
   `BeginFrame == false` as backpressure.
7. Tear down children first, engine, then the window.

## 6. Troubleshooting

| Symptom | Cause and fix |
|---|---|
| `XOpenDisplay failed (DISPLAY='')` | No X server. Start one (desktop login, `ssh -X`, `Xvfb :99`). The sample exits 1 by design. |
| Process aborts with `PostconditionPanic: createVkInstance ... error=-9` at startup | No Vulkan ICD visible (upstream terminate, §2.2). Install one (`setup-headless-vulkan.sh`) or run `--backend noop`. |
| `_XSERVTransmkdir: Owner of /tmp/.X11-unix should be set to root` | Cosmetic Xvfb warning in rootless containers; the server works. |
| Window shows but stays black for the first frames | Normal: the first `BeginFrame` on a fresh surface can skip (backpressure). Also make sure you render only after expose. |
| Tearing/stutter under a compositor | Compositor-dependent; enable vsync path (FIFO present) and avoid `--fps` above the display rate. |
| TINY window / huge content after resize | You skipped the viewport/aspect update (`§4.4`). |
| `vkCreateSwapchain` errors after resize | You recreated the swap chain *inside* a begin/end pair, or disposed the wrong instance. |
| llvmpipe is slow | It is a CPU rasterizer; ~60 fps at 800×600 on the reference container. Use a real GPU for production. |

## 7. Porting beyond Linux/X11

The ABI shape is a raw `void*` platform handle, so no ABI change is needed
for other window systems — only the platform layer that produces the surface
changes (Filament backend) and your windowing code:

* **Win32:** `HWND` as the handle; Filament creates a
  `VK_KHR_win32_surface`. Windowing via `CreateWindowEx` + message pump
  (`GetMessage`/`DispatchMessage`), close via `WM_CLOSE`.
* **macOS:** `CAMetalLayer`-backed `NSView`; needs a Filament macOS SDK for
  the bridge build (the pinned release ships one) — not built in this
  revision.
* **Wayland:** Filament supports `wl_surface`; the sample's Xlib code would
  be replaced with a Wayland client library. XWayland also runs X11 clients
  unmodified — usually the cheapest path.

## 8. What is deliberately not covered here

* Resize is handled on `ConfigureNotify`, but the reference container has no
  window manager (Xvfb), so the *automated* runs do not generate organic
  resize events; the path is exercised on desktops and reviewed here.
* No multi-window support (one window per swap chain; create several swap
  chains on one engine if you need this).
* No DPI/scale handling — X11 reports pixels; toolkits that scale (GTK/Qt)
  would own that.
* The GL backend requires a running X server at engine creation (upstream
  terminates otherwise) — the sample always uses Vulkan with NOOP fallback,
  which covers strictly more environments.
