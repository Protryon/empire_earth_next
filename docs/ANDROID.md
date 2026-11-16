# Android port (arm64)

The game builds for `android-arm64` (API 26+, Vulkan through wgpu-native) with `tools/xbuild.py`, and
`tools/android_apk.py` packs the result into an APK without Gradle. It runs on a phone with an on-screen, fully
editable touch-controls overlay. Nothing here changes the Windows, Linux or macOS behaviour except where "Shared code"
says so.

## What is in it

| Part | Where |
|---|---|
| Build target `android-arm64` (NDK clang, SDL3 + wgpu-native static, `libmain.so`) | `tools/xbuild.py` |
| APK packer (javac, d8, aapt2, zipalign, apksigner; bundles `data/` as `assets/data.zip`) | `tools/android_apk.py` |
| `SDL_main` entry, data root, JNI input bridge | `src/Platform/SDL/PlatAndroid.cpp` |
| Activity, launcher (unpacks the data on first run), touch-controls overlay and editor (Java, clean-room) | `android/` |
| Android native window as a wgpu surface (`PLAT_NATIVE_ANDROID`) | `include/platform/PlatWindow.h`, `PlatWindow.cpp`, `GX.cpp` |
| Sound: miniaudio AAudio, OpenSL ES fallback | `ee_miniaudio.h`, `Mixer.cpp` |
| bionic gaps: `funopen` instead of `fopencookie`, no `backtrace` before API 33 | `PlatFile.cpp`, `EESim.cpp` |

### The touch controls

`android/java/net/eenext/aoc/controls` draws the overlay and pushes ordinary SDL mouse and keyboard events through
JNI, so the game's own input path is unchanged. Buttons, sticks, a trackpad-style cursor, key combinations, mouse
wheel and a keyboard (chat) button are widgets in a profile the player edits on the phone (long press / the Edit
button: add, move, resize, set key bindings, per-widget properties; double-tap a widget for its properties). Cursor
speed, finger-speed acceleration and an optional gyro are sliders in the settings dialog. Profiles are JSON in the
app's files folder. A first run (no saved profile) starts from `android/assets/controls-default.json`; the
editor's Export button writes the current profile as `controls-export.json` in the app's external files folder,
which is how that default was made. `EEPresets.java` has the Add menu's hot-key presets.

## Shared code: what changes on every platform

These are the only edits that are not behind `__ANDROID__`. Each was needed for the phone and is also safe or better
on desktop; please check them first.

1. **GX ring buffers** (`GX.cpp`, `GXInternal.h`). `GXCore::Ring` made one `wgpuQueueWriteBuffer` per draw
   (thousands per frame), which each allocate a staging buffer. The rings now have a CPU shadow that `Submit()` writes
   with one `wgpuQueueWriteBuffer` per ring before the commands that read it. Offsets and layout are unchanged.
2. **Pooled text textures** (`GXSurface::DrawText`). Each text draw created and destroyed a texture; they are now
   pooled by size (512 entries) and an entry is reused only after a submit, because the earlier blit is still in the
   open encoder. This stopped the GPU memory growing on the phone.
3. **`wgpuDevicePoll` after present**, so wgpu frees released resources promptly.
4. **Swap-chain format**. The game's colours are already gamma encoded, so an sRGB swap chain encodes them twice
   (washed out, grey blacks). The first format of the capabilities list was used when BGRA8Unorm was missing, and
   Android Vulkan offers RGBA8 with the sRGB variant first. Now: BGRA8Unorm, then RGBA8Unorm, then any non-sRGB
   format, then whatever is offered. Windows/Linux/macOS pick BGRA8Unorm exactly as before.
5. **Event waits** (`PlatThread.cpp`, POSIX). All events shared one condition variable that `PlatEventSet`
   broadcast, so the world lock's frequent sets woke every waiting thread each time (most of the kernel time in an
   Android simpleperf profile). Each waiting thread now sleeps on its own condition, registered with the events it
   waits on, and a set wakes only those. Semantics are unchanged: `PlatEventWaitAny` still picks the lowest set
   event, auto-reset events are cleared by the one wait they release, and a set with no waiter makes no system call.
   Windows is unaffected (it has its own implementation).

Everything else is `#ifdef __ANDROID__` (camera, below, and the window, logging and audio details).

### Android-only behaviour worth knowing

* **Camera** (`EECameraControl.cpp`): the 30 degree horizontal field of view of the original is far too narrow and
  tall on a wide phone screen. On Android the vertical field of view is that of 0.80 rad horizontal at 1920x1080,
  widened to the screen's shape, and the pitch is a steady 35 degrees at every zoom.
* **Focus loss** (`EEUserInterface.cpp`): not minimised on Android (the editor's dialogs take focus; minimising sent
  the player to the home screen).
* **Audio** (`Mixer.cpp`): `PIMIsValid` does not open and close a stream (an AAudio stream closed at once races its
  callback thread and aborts), and `ma_device_stop` runs before `ma_device_uninit`.
* **Surface lifetime** (`GX.cpp`): Android destroys and recreates the `ANativeWindow` on rotation and restarts, so
  the surface is looked up again on every present and recreated when it changed.

## Building

Needs, under `toolchain/` (git-ignored; `fetch_android.py` does not exist yet, so it is set up by hand):

* `android-ndk-r27c` (Windows host build; the path is in `xbuild.py`),
* `android-sdk` with `build-tools;35.0.0` (34.0.0 also installed) and `platforms;android-34` (cmdline-tools'
  `sdkmanager`), and a JDK on `PATH`,
* `wgpu/android-aarch64`: the wgpu-native `android-aarch64` release (same pinned version as the other targets),
  and `wgpu/x86_64/include` for the headers,
* `android-debug.keystore`: any debug keystore (`keytool -genkeypair ...`, alias `androiddebugkey`, password
  `android`).

```
python -I tools/xbuild.py sdl3 --target android-arm64            # once
python -I tools/xbuild.py game --target android-arm64 -j 10      # -> build/xbuild/android-arm64/game/libmain.so
python -I tools/android_apk.py --data PATH/TO/Data               # -> build/android/ee-aoc.apk
python -I tools/android_apk.py --no-data                         # code only; push the data by hand
```

The APK embeds the game data, which is retail-derived content: keep the APK for your own devices.

## Status

Verified on a Redmi Note 8 Pro (Mali-G76, 2340x1080, MIUI): installs, plays (single-player skirmish),
sound works, controls editor works. Not tested: other GPUs and devices, tablets, multiplayer, the movie player.
CI does not build the Android target (it needs the Windows-hosted NDK path); no Gradle project is provided.
