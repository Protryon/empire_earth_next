# empire_earth_next: the modern-compiler port

This repository is a fork of `empire_earth` (remote `upstream`), the source reconstruction of *Empire Earth: The Art
of Conquest*. It starts from upstream's `fixes-all` branch (the reconstruction plus every original-bug fix) and builds
the game with a current toolchain. Behaviour may diverge from the original; the long-term goal is a portable game.

## Upstream sync

Upstream rewrites `fixes-all` on every update (its fix commits are replayed onto the new upstream `main`). Take
upstream changes by merging, never by rebasing, so this repository's history is never rewritten:

```
git fetch upstream
git merge upstream/fixes-all
```

Source changes merge normally. The build tooling is this fork's own: upstream's VC6 build files (`tools/build.py`,
`relink.py`, `exe_roundtrip.py`, `implib.py`, `exe_fulllink.py`, `exe_units.py` (deleted here), `rsrc_rebuild.py`, `verify.py`, `ee.py`,
the fetch scripts, `include/dinkum/`) and the exe's link tables (`link/exe/*.tsv`, `names.txt`, every `imports.txt`:
deleted here, keep them deleted) were rewritten or deleted here. When upstream changes one of them, keep this fork's version
(`git checkout --ours <file>`) and port anything relevant by hand. Upstream's matching stand-ins
(`include/LostDebug.h`, `EE_LOST_*`) are deleted here too: resolve new uses by dropping them.

## Where the port stands (2026-11-04)

1. **Compiler swap, validated.** The source was made to compile with clang-cl and link with lld-link next to the
   original VC6 build. Both builds were compared game tick by game tick: one-bit float differences (x87 rounding
   points) from the first tick, integer state identical for about 3,000 ticks (RNG to PT 3,066, AI to PT 2,856), then
   chaotic drift. No logic difference appeared inside that window, and the clang build played whole games. The VC6
   build has since been removed.
2. **Dinkum 2.33 replaced by the MSVC STL and the UCRT** (every module at once: STL objects and heap memory cross the
   DLL boundaries). All 846 units compile and all 8 images link with clang-cl, lld-link and the Windows SDK only.
   The game passes the menu/settings tour, plays, and plays AI games to their time cap in the harness.
3. **The test harness is this fork's own** (`tools/harness/`, ported from the reconstruction project): it stages a
   test-profile build and reads game memory by member name from the build's own record layouts.
4. **One static exe** (plan step 1 below, done): every game DLL and the language resources are linked into
   `EE-AOC.exe` with the static runtime; the plug-ins are a static registry. Tour, play and a 2-AI game to the time
   cap pass, also under full page heap.
5. **x86_64 only** (2026-11-05): the exe, wgpu-native and the harness are x86_64; the i686 build is gone
   (PORT_LEARNINGS.md "x64 only").
6. **A plain link** (2026-11-05): every symbol the exe uses is defined in source (member aliases are references to
   the members, the ICF/vtable-slot aliases are bodies, the WON library is a unit of stand-ins); the exe is one
   lld-link with no stubs and no link tables (PORT_LEARNINGS.md "Plain link").

## Rules of the build

- Compiler flags (`tools/clang_build.py`): x86_64 (SSE2 floating point), MSVC 19.44 compatibility, C++17, `/GS-`,
  `-fno-strict-aliasing -fwrapv`. The last two tame undefined behaviour the code relies on; removing them is planned
  work (`-fno-delete-null-pointer-checks` went once the sanitizer games came back clean) (`tools/clang_ubdiff.py`, `build.py --sanitize-null`; see
  PORT_LEARNINGS.md).
- One image, one plain lld-link: every module's objects (engine and plug-ins first), the image libraries, the
  Windows SDK import libraries (and a one-line .def for DINPUT.dll), the static libraries and runtime. The engine and renderer
  units still declare their classes `dllexport` (clang-cl's vector deleting destructors; PORT_LEARNINGS.md "One
  static exe"), so the exe has an export table. A unit's `// UNIT-FLAGS:` line only adds its defines now.
- `link/` holds only the units' lists (`<module>/units.txt`, link order). The frozen VC6 layout reference
  `tools/harness/vc6_layout.json.gz` serves the harness (VC6 offsets translated by member name).
- Test-only builds (`--test-profile`, `--sanitize-null`) change only staged copies of a few units; `src/` is never
  edited by a build.

## Next steps

- The remaining UB flags: `-fwrapv` (`clang_ubdiff.py` + `-fsanitize=signed-integer-overflow` games), then
  `-fno-strict-aliasing`.
- x64 only, then macOS/Linux: the plan below. What is left of the census is the `-Wshorten-64-to-32` class (1,274 sites, 1,139 of them `size()` into `long`, e.g.
  reverse loops). They are benign (no container approaches 2^31 elements) and the code is headed for a Rust
  rewrite, so they stay unless a site turns out to matter. Saved games and network messages are fine on x64:
  loaders read typed 32-bit ids and rebuild pointers, messages copy `long`/`float` members, and every raw on-disk
  record has the same size on both targets (`tools/record_sizes.py`; each is pinned by a `static_assert`).

## Plan: x64 only, then macOS and Linux (owner, 2026-11-04)

Goal: the exe is built and tested as x86_64 only; the i686 build (and wgpu-native's i686 v24 pin) goes. Then the
same source builds on macOS and Linux.

State (2026-11-04): all 858 units compile for x86_64 (`clang_build.py --x64`), only `-Wshorten-64-to-32` remains
(1,278 sites, benign). Raw on-disk records are size-pinned and equal on both targets; saved-game ids and network
messages are x64-safe (PORT_LEARNINGS "64-bit census").

Steps:
1. **x64 link** (done, 2026-11-04): `python -I build.py --x64` -> `build/clang-x64-exe/exe/EE-AOC.exe` (PE32+
   AMD64, 15.6 MB, system DLLs only), lld /MACHINE:X64 at /BASE:0x400000 (member aliases are absolute 32-bit
   symbols) with the x86_64 runtime/SDK libraries and wgpu-native x86_64 (v24.0.3.1, see below). `exe_fulllink.py` maps the link
   tables' 32-bit names to x64 names by demangled signature and writes x64 stubs; member offsets come from the x64
   build's record layouts. Gate met: pass 2 0 unresolved / 0 duplicates, and the pass-1 census and stubs equal the
   i686 ones symbol by symbol (won 304, icf 36, vslot 262, pure 11, data 75). Three x64 runtime hazards fixed in
   source (x87 precision control, UI thread message pointers, saved-id -1 sentinels); PORT_LEARNINGS.md "x64 link".
2. **x64 harness** (worktree agent, in parallel): `exe_boot.c` as a native x64 debugger (guarded APIs read their
   arguments from RCX/RDX/R8/R9 and the stack's home area; breakpoints and context use Rip/Rsp), the presentation
   bridge, time shim and seed pin as x64 DLLs/offsets from the build's layouts. Tested first against a small x64
   test program calling every guarded API, then on the x64 exe.
3. **Switch** (done, 2026-11-05): tour + AI games on x64 (branch x64run), then x64 became the only target (branch
   plainlink): `python -I build.py` builds the x86_64 exe (build/clang, test profile build/clang-test), the i686
   build paths, the i686 wgpu-native pin, the 32-bit harness (mingw32 exe_boot.exe, the x86 arch layer, thiscall
   typedefs, the i686 self-test half, i686 symbol names translated by x64names.py) are removed, CI builds x64
   (PORT_LEARNINGS.md "x64 only"). Before that: the x64 test-profile build passes the
   settings tour + 120 s of play and the seeded 2-AI Hard game to its time cap, with the binary and the text db
   tables. Bugs found: functions the link aliased to a VC6 ICF twin of another class whose members moved on x64
   (eight bodies written in source; `tools/alias_layouts.py` checks the rest), and wrong-class printf arguments in
   traces (format checking on the engine's formatters). 32 vs 64 bit: identical map generation; the AI's trace is
   identical to an i686 `/arch:SSE2` build to about game second 290 and both differ from the x87 i686 build from
   ~256 s, so the outcome gap to the shipped i686 build is the floating-point model (PORT_LEARNINGS.md "x64
   runtime"). Left: the differential tooling, human driver and save/load sessions on x64.
4. **macOS/Linux** (infrastructure and portable layers done on branch posix, 2026-11-05; the plan below).
   Decisions: SDL3 (zlib, static, 3.4.18) for windows, events, keyboard/mouse, clipboard, text input and the native
   handles wgpu needs, behind `platform/PlatWindow.h` (a Rust port can put winit behind the same interface);
   stb_truetype with Liberation Sans/Serif/Mono (OFL) registered as Arial/Times New Roman/Courier New behind
   `platform/PlatFont.h`, one implementation on every system (FreeType if GDI's hinted metrics turn out to matter:
   PORT_LEARNINGS "Linux and macOS"); `<windows.h>` off Windows is `platform/wintypes.h`, types and macros only,
   found through `include/compat/` on POSIX compiles; Linux is cross-compiled here against a Debian 11 sysroot and
   built natively in CI; macOS is built in CI only (the Xcode SDK may not leave Apple hardware); one build entry
   point beside build.py (`tools/xbuild.py`), CI in `.github/workflows/posix.yml` (branch `posix-ci`).
   State: the POSIX platform layer (time, threads, files with case-insensitive paths, preferences, strings,
   sockets, messages) passes its tests on Windows (205 checks, Win32 implementation), ubuntu-latest and
   macos-latest arm64 (217: plus the volume mapping); PlatFont's test and the SDL3 window test (window -> native
   handle -> wgpu surface, GPU clear and PlatFont text read back exact, frames presented) pass on Windows (WARP),
   Linux (Xvfb + lavapipe, X11) and macOS (Metal on the runner's paravirtual GPU). The Linux census compiles 455
   (CI's libstdc++ 13: 456) of 862 units, 324 more fail only on the LP64 record asserts; macOS (libc++) is behind:
   105 with -fms-compatibility (it retypes `va_list` against the SDK's), 389 (+287 assert-only) without it,
   against Linux's 407 (+287) in that dialect; the rest is old zlib/libpng's classic-Mac
   checks, see PORT_LEARNINGS. The steps, each gated by the Windows harness (tour, AI game) where
   it changes the Windows game, and by `xbuild.py census` / CI elsewhere:
   1. **LP64** (branch lp64): fixed-width types in the records, FSFile reads/writes and network messages; then
      `DWORD*`/`unsigned long*` mismatches the census lists ("cannot initialize a parameter"). Gate: census
      779/862 (the 324 assert-only units compile).
   2. **The rest of the direct Win32 calls in shared code**: EEWonNet.h (29 units: compile the WON client out off
      Windows, or PlatMutex/PlatAtomic/PlatSockAddrIn), TCP Protocol.cpp onto PlatSocket (mechanical: `socket` ->
      PlatSocketOpen, `WSAEventSelect(s, *ev, FD_READ)` -> PlatSocketSelectEvent with the TSEvent's PlatEvent,
      `sockaddr_in` -> PlatSockAddrIn (same 16 bytes), the `gethostbyname` loops -> PlatSocketResolveIPv4,
      GetAdaptersInfo -> PlatSocketGetAdapters; gate: the harness's loopback multiplayer session), the three
      MessageBoxA -> PlatMessageShow, GX's Sleep/GetTickCount64/OutputDebugStringA and the sound engine's
      GetLastError -> Plat*, `_snprintf`/`_stricmp` leftovers, the x87 control word (`_controlfp`, UMath: a no-op on
      SSE targets), UResourceFile's string resources (FindResource/LoadString on the exe) -> a table generated from
      Language.rc. Gate: the census lists only the window/input/text units below.
   3. **Window and messages** (one switch: the window changes owner). SDL3 is linked into the Windows exe first
      (build.py: SDL3-static.lib and its system libraries; nothing calls it yet), then in this order:
      a. `GEWindow`/`GEDeviceContext` (GEWindow.cpp): GEWindow holds a PlatWindow*; GetOSRepresentation returns it;
         ClientToScreen/GetClientRect -> PlatWindowGetClientSize. The HDC map goes with GDI (step 5).
      b. The game window and the start screen (Empire Earth.cpp: RegisterClassExA/CreateWindowEx at WinMain's
         setup ~1887, EEStartScreen::WindowProc/Start 449-520 with its bitmap DC, the WH_CALLWNDPROC hook):
         PlatWindowSystemStart + PlatWindowCreate; the start screen's bitmap is presented through GX.
      c. The message loop: EEUserInterface::WindowProc (EEUserInterface.cpp 1762-1950) becomes
         EEUserInterface::HandleEvent(const PlatInputEvent&) with the same bodies: WM_CLOSE/WM_DESTROY ->
         PLAT_EVENT_CLOSE/QUIT, WM_SIZE (minimise mutes, sMinimized) -> RESIZED/MINIMIZED/RESTORED,
         WM_ACTIVATE/WM_ACTIVATEAPP (input suspend/resume) -> FOCUS_GAINED/LOST, WM_KEYDOWN/UP -> KEY_DOWN/UP
         (mVirtualKey), WM_CHAR -> TEXT (one event per byte as before), WM_IME_* -> TEXT_EDITING (step 6),
         WM_SETCURSOR -> PlatCursorShow, WM_SYSCOMMAND (Alt+Tab/screen saver) -> nothing. TSThread::Receive
         (TSThread.cpp 83-90: PeekMessage/DispatchMessage) pumps PlatWindowPollEvent on the UI thread, which is
         WinMain's (EEUserInterface::Run runs on it: macOS requires the main thread). TSThread::Post's
         PostThreadMessage (TSThread.cpp 65) becomes a per-thread queue (PlatMutex + deque + PlatEvent), no
         window involved.
      d. The screen (DX7Screen.cpp, DX7Screen.h: SetWindowPos/AdjustWindowRect/ShowWindow, display modes):
         PlatWindowSetSize/PlatWindowSetFullscreen (borderless), the mode list from PlatWindowGetDisplaySize.
      e. GX's window handle: `GXDirectDraw::SetCooperativeLevel(HWND)` (GX.h 300, GX.cpp 1641) becomes
         `SetWindow(const PlatNativeWindow&)`; GXSurfaceData::Present (GX.cpp ~880) builds the wgpu surface from
         the PlatNativeWindow kind (Win32 HWND, X11 Window, Wayland surface, Metal layer: the code is
         tools/sdl_test's CreateSurface) and sizes it from PlatWindowGetClientSize instead of GetClientRect; the
         test present hook takes a PlatWindow*. GX.h then needs no `<windows.h>` (RECT -> GXRECT, HANDLE -> void*,
         HDC callbacks go in step 5).
      f. The WON lobby (EELobbyInterface.cpp, WONLobbyGUI.h: FlipToGDISurface, GDI child windows): compiled out off
         Windows (the services are gone); on Windows unchanged until WON's source exists.
      Gate: tour + AI game on Windows with the SDL window, screenshots equal to the Win32-window build.
   4. **Input**: the Input System (Input System.cpp, ISKeyboard.cpp, ISMouse.cpp: DirectInput 7 devices and buffered
      data) is fed from PlatInputEvent: ISKeyboard keeps its DIK-indexed keys and reads KEY_DOWN/UP's mScanCode
      from a queue the UI thread fills (Acquire/Unacquire become focus changes; DI_BUFFEROVERFLOW stays the queue's
      overflow); ISMouse keeps its state struct (DIMOUSESTATE moves into ISMouse.h as the engine's own type): lX/lY
      from mDeltaX/mDeltaY (relative mode via PlatMouseSetRelative where DirectInput was exclusive), lZ from
      mWheel x 120, the buttons. UIEditBox/UIForm key-state reads are PlatKeyIsDown/PlatKeyIsToggled (UIForm done).
      DINPUT goes from the imports, compat/dinput.h from the tree. Gate: the harness's input fuzzing.
   5. **Text** (GDI out): fonts registered at start-up (Liberation, embedded or from the data root's fonts/
      directory; aliases for the dbfonts face names); GEFont (GEFont.cpp: CreateFont + GetTextMetrics) ->
      PlatFontCreate/PlatFontGetMetrics, GEFont::GetOSFont returns PlatFont*; GEText's GetTextExtentPoint32 ->
      PlatFontMeasure; the renderer's DX7DrawTextGDI ("DX7Rasterizer Draw2D.cpp" 240-330: SelectObject,
      SetTextColor/SetBkColor, ExtTextOutA) -> PlatFontDraw on the surface's CPU copy (Lock instead of GetDC);
      GXSurface::DrawText's GPU-resident path (two scratch DCs to recover coverage) -> PlatFontDraw into a scratch
      buffer, uploaded (PlatFont gives the coverage directly); GEFontBitmap's glyph atlas is unchanged on top of
      that; Empire Earth.cpp's and GESurface.cpp's remaining DCs (bitmap loading) -> surface locks. Gate: menu
      screenshots against the GDI build (expected: the hinting differences of PORT_LEARNINGS) and the tour's text
      layouts (no new wraps).
   6. **Input method**: EEUserInterface's IMM calls (ImmGetContext, composition strings, candidate lists: 39 sites),
      UIEditBox (18) and UIIMECandidate (5) -> PLAT_EVENT_TEXT_EDITING and PlatTextInputStart(edit area); the
      system draws the candidate list. CJK text needs the multibyte encoding decision first (UTF-8 PlatString with
      the fonts), so Western text input comes first.
   7. **The POSIX game**: `xbuild.py game --target linux-x86_64|macos-arm64` links every unit with src/Platform/Posix,
      SDL and Common, SDL3 and wgpu (a macOS .app bundle: Contents/Resources is the data root); the platform
      layer's data root (`EE_DATA_ROOT` or the executable's directory) replaces the install path. CI runs it to the
      main menu under Xvfb/lavapipe (Linux) and Metal (macOS); the harness itself stays Windows-only.
   LP64 (`long` is 64-bit there): done on Windows first (branch lp64, PORT_LEARNINGS.md "LP64"): every value in a file or
   network message has a fixed width (FSFile's int32_t/uint32_t overloads with 4-byte long adapters, int32_t fields
   in the 37 raw records and the 133 message members, 32-bit wire locals), the RNG state, checksums, saved ids and
   "none" sentinels are 32-bit, and printf/scanf formats match their arguments (`-Wformat` clean for x64). Gates:
   `tools/record_sizes.py` (no record holds a long), `tools/lp64_census.py` (the census), `tools/objdiff.py`
   (Windows object code unchanged but for the listed functions). Left for the POSIX port itself: wchar_t width,
   Windows SDK structs read from files, little-endian formats (PORT_LEARNINGS.md "LP64").

## Plan: one static exe, no proprietary third-party code (owner, 2026-11-04)

Decisions: one statically linked `EE-AOC.exe` (static CRT `/MT`; only Windows system DLLs remain); Bink movies
converted once to MPEG-1/MP2 and played with pl_mpeg; only the T&L renderer is kept. The long-term direction is a
port to Rust, so new third-party code stays small, C, permissively licensed and behind narrow interfaces that a Rust
crate can replace.

1. **Fold the game DLLs into the exe.** (Done.) The engine, TCP protocol, mixer, Default random maps and
   DX7HRTnLDisplay are linked into the exe. The PIM plug-in scan (`PIMModule.cpp`, LoadLibrary + `PIMGetName`/
   `PIMGetType`/`PIMIsValid`) is a static table of the same entry points; `Language.dll`'s resources are the exe's.
   DX7HRDisplay is dropped. `/MT` replaces `/MD`. The harness stages one exe. Open: the engine's export table
   (until polymorphic `new[]`/`delete[]` are gone or clang emits vector deleting destructors on its own), the
   per-module header views (`tools/odr_check.py`: 28 inline functions with divergent copies, mostly older ODR
   clashes inside the game).
2. **Audio output: miniaudio** (done). `src/Low-Level Engine/Sound Engine/Mixer/Mixer.cpp` mixes the game's channels
   (volume, pan, pitch by resampling, loop, position) into one miniaudio playback device (WASAPI, DirectSound,
   waveOut, else the null device); samples are decoded whole on load (WAV/MP3/FLAC). miniaudio 0.11.21 is vendored
   (`include/miniaudio/`, options in `ee_miniaudio.h`, implementation `src/ThirdParty/miniaudio/miniaudio.c`).
   `Mss32.dll`, the Miles headers and `redist\win32` are gone, and so is the harness's silent mixer (the null device
   serves).
3. **Music: GothicKit/dmusic** (done). Vendored at `src/ThirdParty/dmusic` (MIT; commit b73b270, with two additions:
   `DmSegment_setRepeats`, `DmPerformance_setMasterGrooveLevel`; see its README). `SEConductor`/`SEScore` are
   rewritten on it with their public interface unchanged; the performance renders at the mixer's rate into its
   device thread (`SPIMGetOutputRate`, `SPIMSetMusicSource`). Not carried over: motifs (dmusic has none; the game
   never plays them), notifications (the game registers none), reverb (TinySoundFont has none), end-and-intro
   transitions (dmusic plays the end pattern). The DirectMusic COM code, its GUIDs and `dmusici.h` are gone.
4. **Movies: pl_mpeg** (done). pl_mpeg (MIT, commit c871f2b) is vendored (`include/pl_mpeg/`,
   `src/ThirdParty/pl_mpeg/pl_mpeg.c`). `DX7Movie` decodes on its thread at pl_mpeg's clock, converts frames into
   the back buffer's pixel format, and its sound goes through a ring into the mixer (`SPIMSetMovieSource`,
   resampled). A movie the game names `X.bik` plays from `X.mpg` beside it (keep the `.bik`: the game looks it up
   first); `tools/convert_movies.py <game dir>` converts with a user-installed ffmpeg (MPEG-1 at 30 fps, MP2). The
   scenario editor lists `.mpg` files. `binkw32.dll`, the Bink header and the mixer's DirectSound bridge are gone:
   the exe imports only Windows system DLLs. Fixed on the way: a movie created with a rectangle ignored it (the
   original reset the flag), and the playback thread started before its events existed.
5. Then: DirectDraw/D3D7 and DirectInput (plan below), Winsock.

Each step ends with the harness gates and a PORT_LEARNINGS.md entry.

## Plan: DirectX -> wgpu (2026-11-04; phases 0-3 done, see below)

Goal: a new renderer plug-in ('rast') on wgpu replaces DX7HRTnLDisplay (DirectDraw 7 + Direct3D 7), and DirectInput
goes too, so the game uses no DirectX. 64-bit first: the exe is going 64-bit only, so the renderer is written
pointer-size clean and built for x86_64 from its first commit; the i686 build serves only while the exe is 32-bit.

What it rests on (measured):
- The renderer is behind the engine's abstract classes: GERasterizer (79 virtuals, 54 pure), GEScreen (21),
  GESurface (33), GEVertexArray (16), GEPolygonArray (10), GEMovie (8); the DX7 code is ~6,600 lines in 13 files.
  Outside it only EELobbyInterface touches DirectDraw (FlipToGDISurface for the GDI lobby windows), and DirectInput
  is the Input System's three files (ISKeyboard, ISMouse, Input System.cpp).
- The engine and the game write pixels themselves: Lock/GetMemoryAddress in GESurface.cpp (image loading), the
  fonts (GEFontBitmap, GetTextDC/TextLock: GDI text drawn on a surface), the minimap, the terrain colour table,
  EEE3DVolcano, Empire Earth.cpp. Surfaces must stay lockable in the game's pixel formats (GEPixelFormatID: 32-bit
  ARGB/XRGB, 565, 555, 4444, 5551, DXT1).
- Direct3D 7 is used as a fixed-function pipeline: SetTransform (world/view/projection), one directional light plus
  ambient, materials and colour-vertex sources, vertex fog (start/end/colour), alpha test and blend, z test/write,
  culling, flat/Gouraud shading, fill mode, colour keys, texture-stage colour/alpha ops (modulate etc.), vertex
  buffers drawn with DrawPrimitiveVB/DrawIndexedPrimitiveVB. 2D is DirectDraw blits (stretch, colour key, filter).
- wgpu-native (gfx-rs, MIT/Apache-2.0) ships static libraries for windows-x86_64-msvc and windows-i686-msvc
  (v29.0.0.0: webgpu.h + wgpu.h, wgpu_native.lib 49 MB static, no default-library directives). Its DX12 backend
  runs on WARP (Windows' software adapter), so the headless VM keeps a working renderer without a GPU.

Design:
- **Surfaces**: every GESurface keeps a CPU shadow in its pixel format (Lock/GetMemoryAddress/GetBytesPerRow keep
  their meaning; GetTextDC hands out a DIB section on the shadow for GDI text) and a GPU texture, uploaded when the
  shadow is dirty (Unlock/TextUnlock mark it; dirty rectangles keep UI uploads small). 16-bit formats convert to
  RGBA8 on upload (WebGPU has no 565/555 texture formats); DXT1 uses BC1 where the adapter has it, else decodes on
  the CPU. Render targets (the back buffer) are GPU-only; a lock on one reads back (Capture, screenshots).
- **2D**: CopyTo/CopyWithFilter/Clear become textured-quad draws (point or linear sampling, colour key as a
  discard against the key colour), batched per target.
- **3D**: the fixed-function state maps onto a few WGSL pipelines (an "uber" vertex/fragment shader with uniforms
  for transforms, light, material, fog, alpha reference and the texture-stage combiner), with pipelines cached by
  the fixed state (blend, depth, cull, topology, vertex layout). GEVertexArray/GEPolygonArray become wgpu vertex and
  index buffers with the same vertex layouts as the DX7 classes.
- **Presentation**: a wgpu surface on the game window. The game renders at its chosen resolution into an
  offscreen target that is scaled to the window (borderless full screen instead of exclusive display modes; the
  mode list becomes the resolutions the game supports). The lobby's GDI windows: pause presentation while they
  show, instead of flipping to the GDI surface.
- **Input**: ISKeyboard/ISMouse keep their interface on window messages and raw input (WM_INPUT) instead of
  DirectInput.
- **Movies**: DX7Movie's player moves over unchanged (it only uses surfaces).
- **Rust**: wgpu is a Rust library. The C++ renderer talks to it through webgpu.h, whose concepts the wgpu crate
  shares, so a later Rust renderer can keep the design and drop the C layer.

Phases (each gated by the harness, with the DX7 renderer kept in the build until phase 4 for comparisons):
0. **Probe** (done, 2026-11-04): `tools/fetch_wgpu.py` fetches wgpu-native v29.0.0.0 (x86_64 and i686,
   sha256-pinned, into toolchain/wgpu/). `tools/wgpu_probe/` (probe.c, build.py) links wgpu_native.lib statically
   with the static CRT on the first try (system libraries in build.py's LIBS), opens a window, presents 60 cleared
   frames and reads an offscreen clear back exactly, in both bitnesses: PASS, 7.0 MB (x86_64) / 5.6 MB (i686). On
   this GPU-less VM wgpu picks DX12 on WARP ("Microsoft Basic Render Driver") by itself; Vulkan and GL have no
   driver here.
1.-3. **Done (2026-11-04), as one step**: instead of a second plug-in, the DX7 renderer itself now runs on **GX**
   (`include/gx/GX.h`, `DX7 Display Plug-ins/GX/`): a reimplementation of exactly the DirectDraw 7/Direct3D 7 subset
   the renderer calls (~60 methods, the same names and DirectX 7 constant values, prefix GX) on wgpu. The renderer
   sources were ported by renaming (DirectX types and constants -> GX) plus a few hand edits (one display device
   from GXDirectDrawEnumerateEx, the mode-enumeration callback type, GDI text on GPU targets). Surfaces keep a CPU
   copy in their pixel format and a BGRA8 GPU texture, synchronised lazily (upload before GPU use, read-back before
   CPU use; blits between CPU-side surfaces stay on the CPU with exact colour keys); the 3D device is one WGSL
   fixed-function shader (two variants: smooth/flat) with pipelines cached by blend/depth/cull/topology state.
   Presentation: a wgpu surface on the game window, the frame scaled to the client area (no display mode change, no
   exclusive mode). `tools/gx_test/` checks GX alone (fills, keyed blits on GPU and CPU, textured/lit/z-tested
   draws, pixel centres, GDI text both ways, presentation) in both bitnesses: PASS. The exe imports no DDRAW.
   Gates: settings tour + 60 s of play complete, menus and the 3D scene match the DX7 build's screenshots
   (`tools/harness`: the bridge dumps frames through GX's test hook instead of patching ShowScene).
   Both architectures pinned wgpu-native **v24.0.3.1** (now x86_64 only) (v25+ fail every i686 DX12 pipeline; with v29 the x86_64
   game's read-backs never complete; PORT_LEARNINGS.md).
   Open: GXSurface::DrawText's bounds for unusual fonts, sampling the primary surface (falls back to white; seen 3x
   per session), DXT1 surfaces (unused by the game), wireframe fill mode, 16-bit dithering.
4. **Input and lobby; DX7 out**: raw input instead of DirectInput, the lobby presentation, the DX7 SDK download
   gone, and the renderer's DX7 names (DX7Rasterizer, DX7Surface, folder "DX7 Display Plug-ins") renamed. Gate: the
   exe imports no DirectX DLL (DINPUT is the last one).
5. **64-bit**: the exe itself on x86_64 (the remaining -Wshorten-64-to-32 census work), linking the x86_64
   wgpu-native.

Risks: D3D7 fixed-function details (lighting equations, specular, fog, 16-bit dithering, colour-key edge cases);
Lock semantics while the render thread draws; upload cost for UI-heavy screens; WARP speed for the harness;
wgpu-native API churn (pinned release, webgpu.h is still changing); the static CRT with a Rust static library
(phase 0); binary size (+several MB).

## Cross-platform: census and plan (branch xplat, 2026-11-04)

`tools/winapi_census.py` lists every Windows-specific name in the first-party source (`src/`, `include/`; the
vendored libraries excluded): Win32 calls, constants and types by family, Windows-only headers, MSVC runtime and
language extensions, and member functions whose names are A/W expansions of Win32 macros (`UIControl::DrawTextA`:
they keep that spelling only while `<windows.h>` is included everywhere). It prints the counts by family, by area
and the top files of each family, and writes every site to `build/winapi_census.tsv`. Areas marked [out] belong to
the DirectX -> wgpu work (renderer, Input System, GE screen/surface/window) and are only catalogued; the platform
layer's own Win32 implementation (src/Platform/Win32, [impl]) is where Windows calls are meant to be.

Headline: at the branch base 4,065 sites, 2,867 of them in scope (1,942 without the `#include` lines); now 3,340
sites in 792 files, 1,871 in scope (950 without the `#include` lines). The Win32 calls that moved into
src/Platform/Win32 count as [impl]. Families now (in scope = outside [out] and [impl]):

| family | sites | in scope | names | files | top names |
|---|---|---|---|---|---|
| directx | 834 | 11 | 346 | 24 | DD_OK 38, D3DVERTEX_STD_COLOR 37, DDERR_WASSTILLDRAWING 35, D3DRENDERSTATE_CLIPPING 13 |
| winsock | 313 | 313 | 43 | 2 | WSAGetLastError 37, sockaddr_in 36, closesocket 23, AF_INET 22 |
| com | 60 | 7 | 6 | 10 | HRESULT 45, CoUninitialize 4, GUID 4, S_FALSE 3 |
| registry | 40 | 0 | 17 | 1 | HKEY 14, RegCloseKey 5, KEY_ALL_ACCESS 3, RegOpenKeyExA 2 |
| time | 59 | 5 | 19 | 12 | Sleep 32, LARGE_INTEGER 3, QueryPerformanceCounter 3, GetTickCount 2 |
| threads | 42 | 14 | 25 | 8 | LeaveCriticalSection 5, EnterCriticalSection 4, INFINITE 4, CRITICAL_SECTION 3 |
| files | 62 | 15 | 39 | 7 | MAX_PATH 14, INVALID_HANDLE_VALUE 4, GetCurrentDirectoryA 3, WIN32_FIND_DATAA 3 |
| messagebox | 5 | 5 | 4 | 1 | MessageBoxA 2, MB_ICONHAND 1, MB_SYSTEMMODAL 1, MB_SETFOREGROUND 1 |
| gdi | 108 | 50 | 38 | 8 | SelectObject 12, HDC 9, SetTextColor 8, HGDIOBJ 7 |
| ime | 63 | 63 | 24 | 4 | HIMC 8, ImmGetContext 8, ImmReleaseContext 8, ImmGetCompositionStringA 6 |
| windowing | 222 | 178 | 106 | 22 | HWND 23, DefWindowProc 19, WPARAM 8, LPARAM 8 |
| process | 139 | 67 | 33 | 28 | GetLastError 32, HANDLE 24, HINSTANCE 22, ERROR_SUCCESS 11 |
| msvc-string | 47 | 10 | 26 | 10 | LPSTR 14, LPCSTR 6, _snprintf 4, _stricmp 1 |
| msvc-lang | 63 | 47 | 13 | 18 | CALLBACK 12, __int64 11, _PC_53 9, WINAPI 7 |
| win-types | 206 | 63 | 16 | 31 | TRUE 35, DWORD 31, FALSE 29, DWORD_PTR 26 |
| win-headers | 975 | 921 | 12 | 745 | windows.h 741, mbstring.h 190, ddraw.h 10, d3d.h 10 |

### The platform layer

`include/platform/*.h` is a C interface (extern "C", `<stdint.h>` types, opaque handles, documented results) so a
POSIX implementation, or a Rust crate exporting the same symbols, can replace `src/Platform/Win32/` without
touching the callers. The Win32 implementation calls exactly the Win32 functions the code called before (the
harness's time shim hooks those imports), and is linked with the engine (`tools/units.py` PLATFORM_UNITS).

| Header | Covers | Routed |
|---|---|---|
| PlatTime.h | millisecond clock, high-resolution counter, sleep, timer period, local time, system timers | Time Manager, TMPoll, TMTimer, all Sleep calls but RTSGraphics::Main's, GEModel Caching |
| PlatThread.h | recursive mutex, events (wait, wait-any), atomics, thread id, last native error, threads (create, priority, suspend/resume, exit code) | TSCriticalSection, TSEvent (+WaitAny), TSReadWriteLock, TSScheduler, TSThread (not its message queue), Thread System.cpp, the mixer lock, the NE/music/sound/TCP wait loops |
| PlatString.h | _stricmp/_strnicmp/_itoa/_strdup and the 19 _mbs*/_ismb* multibyte functions | UString, UWideString, the UI text controls, the game (292 sites) |
| PlatPreferences.h | key/value store under roots (the registry on Win32) | URegistry (no HKEY in its interface) |
| PlatFile.h | files, directory search, attributes, temp files, read-only file mapping | FSFileSpec, FSBasicFile, FSDebugFile, FSMemoryFile, FSArchive, File System.cpp |
| PlatPath.h | game paths on the native file system (volumes, separators, case-insensitive lookup), the data root | (POSIX PlatFile; the POSIX game's start-up) |
| PlatSocket.h | IPv4 sockets, readiness events (WSAEventSelect), name lookups, adapters; Winsock error numbers | not yet: TCP Protocol.cpp (step 4.2) |
| PlatMessage.h | message box, debugger output | the three MessageBoxA, UMemoryPool::DumpStats |
| PlatWindow.h | window, events, key/mouse state, cursor, clipboard, text input, native handles for wgpu (SDL3) | UIForm::GetNumLockOff (PlatKeyIsToggled); the rest in step 4.3-4.6 |
| PlatFont.h | TrueType fonts, GDI-style metrics and text drawing (stb_truetype, every system) | not yet: GEFont/GEText/the renderer's text (step 4.5) |

Implementations: src/Platform/Win32 (the game's), src/Platform/Posix (Linux, macOS; tests/platform pass on all
three), src/Platform/SDL (PlatWindow), src/Platform/Common (PlatFont). `platform/wintypes.h` and
`platform/PlatKeys.h` give the Windows types and the key numbering off Windows (types and macros only).

### What is left, in the order to take it

1. **`<windows.h>` everywhere** (741 units). Every unit includes it before `lle/LowLevelEngine.h`, because the engine
   headers use Win32 types (HINSTANCE in ESInitialize, HWND/HDC in the UI and graphics headers) and because member
   names were spelled through the A/W macros (`DrawTextA`, `TSScheduler::CreateEvent` is really `CreateEventA`,
   `FSFileSpec::CreateDirectory`). Step: rename those members to their plain names in one batch (they are the
   game's own; the census family win-macro-names lists them), then make the engine headers self-contained without
   `<windows.h>` and drop the include from units that no longer need it.
2. **TSThread's message queue** (Post/Receive: PostThreadMessage/PeekMessage, and dispatching window messages from
   the UI thread). Needs a platform message queue per thread plus the window system's event pump (below).
3. **Windowing, GDI, IME** (Empire Earth.cpp WinMain/WndProc, EEUserInterface, UIManager, GEFont/GEFontBitmap GDI
   text, UIEditBox/UIIMECandidate IME): a platform window + event interface (an SDL-like shape: create window,
   poll events, cursor, clipboard, text input/composition) and a font rasteriser in place of GDI text on surfaces.
   This meets the wgpu renderer's presentation (PORT.md "Plan: DirectX -> wgpu") and the Input System rewrite.
4. **Winsock** (TCP Protocol.cpp, 312 sites): BSD sockets are close to portable; a thin PlatSocket layer
   (or `#ifdef _WIN32` for WSAStartup/closesocket/ioctlsocket/WSAGetLastError) and the adapter enumeration
   (GetAdaptersInfo) behind one function.
5. **The rest of `process`**: resources (UResourceFile: the language strings from the exe's resources -> a data
   file), MessageBox, GetVersion/UGetOSType, LoadLibrary (left only in the test profile), the floating-point control
   word (`_controlfp`, UMath: x87 precision; becomes a no-op on x64/SSE, but check the determinism it was for).
6. **LP64 (Linux/macOS 64-bit)**: Windows `long` is 32-bit on both x86 and x64, so the x64 census found the record
   formats safe; on LP64 `long` is 64-bit. Done (branch lp64): files, records and network messages have fixed
   widths, sentinels, RNG and checksums are 32-bit, formats match (plan step 4; PORT_LEARNINGS.md "LP64").
7. Paths: the game builds `C:\...` paths with backslashes (FSFileSpec volume + directory + file); a POSIX PlatFile
   maps them (volume dropped or mapped to a root, separators, case-insensitive lookup of the game data).

## Plan: open model and texture formats (owner, 2026-11-04; branch assets)

Goal: models and textures in open, editable formats, so mods can make them with common tools: glTF 2.0 instead of
the `.cem` models, PNG instead of the `.sst`/`.tga` textures. docs/MODEL_FORMATS.md has the formats, what the game
reads of them and the mapping.

State (2026-11-04):
1. **Survey** (done): every field of `.cem` 1.0-2.0 and `.sst`/`.tga` with its reader; the retail data (AoC and the
   original game, 866 models, 2792 textures) parsed end to end.
2. **Converters** (done): `tools/eemodel/eemodel.py` (export, convert, check, validate, render, install, cppgate;
   standard library only). Gates: 866/866 models cem -> glTF -> cem byte-identical to the canonical 2.0 form, all
   glTF structurally valid; 2792/2792 textures PNG -> game view equal; renders of the glTF match the game's transform
   composition.
3. **The game reads them** (done, live gates pending): `GEOpenFormatsFile.cpp` serves a model's `.glb`/`.gltf` and a
   texture's `.png` as the binary bytes the loaders already read (cgltf and stb_image vendored, C++ gate
   `eemodel.py cppgate`). `eemodel.py install GAMEDIR` writes the open forms into a game copy.

Next: the live gates (tour + an AI game on a copy with the installed open formats, screenshots against the binary
data); then retire the binary readers' file paths from the game data (the converter keeps reading them); later, a
native glTF loader once the engine's model class is rewritten (the in-memory conversion keeps the old loader as the
only consumer for now). Not converted: the `.tga` files read outside the texture manager (pointers, palette, volcano,
scenario pictures, backgrounds).

## Other next steps

- Remove the clang dialect bridges: the `#ifdef __clang__` `const T&` forwarding overloads become `const` parameters,
  the `theTempN` call-site locals go.
- The WON library's source, to replace its stand-ins (PORT_LEARNINGS.md "For WONAPI/WONLobby").
- Drop `/FIXED /BASE:0x400000` (ASLR, a high image base): nothing needs a 32-bit base since the plain link.
- Port the rest of the harness (human driver, per-PT differential) where still useful.
- The platform layer's next steps ("Cross-platform: census and plan" above), then 64-bit and other platforms.
