# LLE: bodies over one-statement shims (round 8)

The lane's state after LLE round 8 (branch `rust-lle8`, 2026-11-20). The round's story is PORT_LEARNINGS.md
"LLE round 8". Earlier state: [lle-rest.md](lle-rest.md) (rounds 1-2), [gfx-lle-cluster.md](gfx-lle-cluster.md),
[lle-network-engine-whole-classes.md](lle-network-engine-whole-classes.md), [game-leaves.md](game-leaves.md).

Scope (from this round): src/Low-Level Engine but its User Interface, include/lle but UI*, include/gx, include/dx7,
src/RTS Engine.

## The pattern

`ee_base::cpp_shims!` (ee_base shim.rs) makes, from one list of `fn name(args) -> ret = symbol;` lines, a trait of
the C++ calls a moved body makes, its FFI implementation (`ShimFfi`, absent from a test binary) and its
implementation on `ShimFake` (every call logged, answers and out values from per-shim queues). A moved body is
generic over the trait, so its unit tests assert the calls it makes, in order. The C++ side is an `extern "C"`
function of one statement per shim (a member's slot, a virtual call, a `new`/`delete`), in the class's own unit.

Plain members a body reads and writes become a `#[repr(C)]` record the C++ class derives from (as the units' and
the players' records before): `EeRtsPhysicsFields`, `EeRtsGraphicsFields`, `EeRtsMouseFields`. The Thread System,
Time Manager, Input System and sound samples are Rust objects already: the bodies call them directly.

## What moved (commits)

| Commit | What | Logic (whole tree) | Net C++ (src, include but include/rust) |
|---|---|---|---|
| c4fb6b43a | `UMemoryPool<T>` and `UList<T*>` whole in Rust (ee_base util pool.rs) | -212 | -303 |
| 0fe29478a | the mixer plug-in whole in Rust (ee_audio mixer ffi.rs, plugin.rs) | -120 | -180 |
| 5e73f23f7 | RTSPhysics' bodies (ee_game rts physics.rs); `ee_base::cpp_shims!` | -235 | -246 |
| 5b806af90 | RTSGraphics, RTSMouse, RTSMousePointer, RTSMiniMap, RTSTile, RTSWorld, RTSUnit, RTSPlayer (ee_game rts) | -939 | -1,164 |
| 66328b822 | NEMRecord's bodies (ee_game lle netrecord.rs) | -150 | -234 |
| b03aad460 | RTSMap's own bodies (ee_game rts map.rs) | -66 | -61 |
| 2ba3d27e0 | clippy and rustfmt (Rust only) | 0 | 0 |
| | **the round** (e0a5ecc60 -> b03aad460: 81,661 -> 79,939) | **-1,722** | **-2,188** (+610 -2,798) |

Shims: 306 one-statement C++ shims added (`ee_rts*cpp_*`, `ee_rtscpp_*`, `ee_nemcpp_*`, `ee_mixercpp_*`) and the
pool's five host functions; 5 deleted (the mixer plug-in's `ee_soundcpp_*` module entries, now Rust). In this
lane's scope (src/Low-Level Engine but User Interface, include/lle but UI*, src/RTS Engine) the logic went from
6,281 to 4,592 (-1,689).

## What is left, by directory

- src/RTS Engine: RTSMap.cpp (most of its bodies are EEMap's, game core round 8's; the rest are forwarders to
  ee_render map_raise.rs), RTSWorld::SetCameraTransformation (the camera's trigonometry), RTSUnit's handle helpers
  and the unit list's failure report (debugging).
- src/Low-Level Engine/Graphics Engine: GEModel, GETexture, GEBitmap, DX7Rasterizer, GERasterizer: host switches
  (one-statement cases, but over scoped enums, which logic_lines.py counts as logic: see below) and the few
  multi-statement cases (the model manager's sub-model, the font bitmap's).
- src/Low-Level Engine/Network Engine: NESynchronousServer/Client/Communications: their host switches (as above)
  and constructors' argument records.
- src/Low-Level Engine/File System: FSPath (vectors of FSArchive/FSFileSpec C++ objects), File System.cpp.
- include/lle: the value-type templates (U2DPoint, U2DRectangle: simulation math, by instantiation), the class
  declarations of the GE shells (GERasterizer.h's 100 virtuals: the store gate's StoreRasterizer subclasses it),
  the constant enums (GERenderState.h, ISKeyCode.h, NEMessageID.h), FSFile.h's typed overloads.

## The measure

`logic_lines.py`'s case pattern `case\s+[^:]+` stops at the first `:` of a scoped enum (`case EeGeModOp::X:`), so
every one-statement case of the cbindgen enums' host switches counts as logic: about 470 lines in this scope at
the round's base (a variant that accepts `::` measured 5,812 in-scope logic lines against 6,281).
