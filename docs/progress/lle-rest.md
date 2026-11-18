# LLE, the rest (all of the Low-Level Engine in Rust)

The lane's state. Each round rewrites this file to the current state; the round's story goes in PORT_LEARNINGS.md
("LLE rest, round 1"). Earlier lanes over the same code: [lle-whole-classes-ne-fs-utilities-sound-ts-tm-es-input.md](lle-whole-classes-ne-fs-utilities-sound-ts-tm-es-input.md),
[lle-network-engine-whole-classes.md](lle-network-engine-whole-classes.md), [r6-lle-user-interface-toolkit-14-4k.md](r6-lle-user-interface-toolkit-14-4k.md),
[r4-3-graphics-engine-17k.md](r4-3-graphics-engine-17k.md).

## Round 1 (branch `rust-lle-rest1`, 2026-11-17)

| Commit | What | Net C++ (src + include, not include/rust) |
|---|---|---|
| 9ee4f5350 | The plug-in registry (`PIMModule`) deleted; the mixer and the Default random-map generator linked directly | -193 |
| 8dc2dbc87 | 123 dead engine members (a census of every member declared in the engine's headers) and the IME candidate window (`UIIMECandidate`, never created or opened) deleted | -778 |
| b32476f37 | The texture manager and the texture reach `GETexture`'s Rust object directly (`texobj.rs` `tex_direct`): 12 + 3 host operations gone | -137 |
| bd8223f79 | The census's second pass (`UIControl::GetBorderColors`, `GEUnpackAlpha`, `GEDeviceContext.h`) | -15 |

Round total: -1,123 net C++ lines, short of the 3,000 asked (see "Why short").

Verified on bd8223f79: both builds 0/0, the Rust gate, the oracle seeds 2-7, nettest, wontest (but
`won_cfgtab_test`, failing the same way before the round), gates.sh GATES PASS, the tour's menus against the
pre-round exe, the store gate, the macOS and Linux `cargo check`s (PORT_LEARNINGS "LLE rest, round 1").

## Round 2 (branch `rust-lle-rest2`, 2026-11-18)

| Commit | What | Net C++ |
|---|---|---|
| 18e05b0f7, f8bda8559 | `tools/dead_members.py`: the dead-member census as a tool (report, delete; generated headers skipped) | (tool) |
| 3b0813fb1 | 153 dead members of the computer player's classes (EEComputerPlayer, EECP*, EEAI*) | -704 |
| 98a70b5f5 | LLE: `UIControl::GetFields`, `UIForm::SetMouseMoveEvent` | -4 |
| 1e2dd117d | 120 dead members of the world, units, players, scenario and screens | -605 |
| b3f96958d | The store gate's link check: a name may stay unresolved only when the game's own objects define it | (tool) |
| 0346f7fd3 | The font bitmap's glyph textures' stage and reference answered in Rust (fbobj.rs) | -27 |
| 26ef1eddd | The census's second pass, to a fixpoint (the members only the first pass's had called) | -195 |
| e8dc0f323 | 25 dead free functions of include/ee (a column-0 pass) | -324 |
| db59996cb | 19 more dead free functions (their Rust mentions are comments) | -133 |

Round total: -1,992 net C++ lines (target 2,000).

Not done this round (next): the vertex host's texture operations (the primitives' cargo tests construct through
the same C entry points as the C++, so a constructor flag cannot tell a recorded world from the game: give the C++
its own entry point first); the form's group and background operations through `direct.rs`; folding
`DX7Rasterizer` (the store gate's `StoreRasterizer`, `StoreSurface` and `StoreScreen` subclass the bases; the GE
vertex array's virtuals exist for the store gate's array too); `UILBString` whole; `UIObjHost::Base` from the record.
What the census leaves: include/ee/wonapi (the WON code is kept as the port's reference), the reconstructed padding
virtuals (`Virtual14` ...: the vtables' shape), the generated rules header.

## The measure (task 1): what is left, file by file

`src/Low-Level Engine` after the round is about 13,300 lines and `include/lle` about 9,500. Lines and the host-op
`case`s (each one a call the Rust makes on a C++ object) of the larger units:

| Unit | Lines | Host cases | What it is |
|---|---|---|---|
| GEValues.cpp | 1,014 | 0 | the value types' out-of-line members: one-line forwarders to `ee_ge` values.rs, the file I/O, the string ids. Callers everywhere (the simulation's math): the classes stay the types. |
| GEModel.cpp | 761 | 71 | model, loader, manager, physical model, terrain mesh shells: forwarders and their hosts (files, materials, textures, surfaces). |
| UIForm.cpp | 747 | 130 | the form's forwarders and its host (the controls' virtuals, the groups, backgrounds, hot keys, events, the manager). |
| DX7Rasterizer.cpp | 730 | 74 | `DX7Rasterizer`, the one `GERasterizer`: ~100 virtual forwarders (the store gate's `StoreRasterizer` subclasses the same base), the screen/surface/movie hosts. |
| UIManager.cpp | 672 | 51 | the manager's forwarders and host. |
| GETexture.cpp | 663 | 60 | texture, manager, set, surface shells; after this round the hosts keep only C++ object work. |
| GEBitmap.cpp | 629 | 79 | bitmap, font bitmap, the object and vertex hosts. |
| UIControl.cpp / UIControlsTable.cpp | 515 / 436 | 97 / 97 | the base control's host (the game's 8 direct subclasses override its virtuals) and the table's factories. |
| FSMemoryFile.cpp | 396 | 0 | the stdio temporary file and the `FSFile&` overloads over the Rust state. |
| NESynchronousServer.cpp / Client / NEMessage.cpp | 373 / 250 / 324 | 40 / 31 / 0 | the game subclasses both and the message classes (virtual seams). |

Whole-movable now: nothing large. Every remaining class is either a type its callers hold by value or subclass (the
value types, `UString`, `UIControl`, `UIForm`, `NESynchronousServer`/`Client`, `NEMessage`), or a shell whose host
makes C++ virtual calls (`GESurface`/`GEScreen`/`GERasterizer` have store-gate fakes; the toolkit's controls have
game overrides). What can still go is the hosts' operations that only cross into C++ to reach another Rust object.

## Why short of 3,000

Earlier lanes moved the logic of every engine class (PORT_LEARNINGS "LLE lane" rounds 1-7, "LLE Network Engine"
rounds 1-7, the toolkit's rounds 1-21, the Graphics Engine's parts 1-17): what is left is the C++ surface the game
calls and the host operations. The dead-code census found 123 members; the next lines come one host case at a time.

## Next

- The same "direct when the C++ made it" path for the other hosts that bounce into Rust: the vertex host's
  `GEVertexArray` and `GETexture` operations (bmpobj.rs, prim.rs), the font bitmap's text, bitmap and texture
  operations (fbobj.rs), the form's group and background operations (`direct.rs`'s registry: an unregistered
  handle, as every replay's, falls back to the host, so the goldens replay unchanged).
- `DX7Rasterizer`: the store gate's `StoreRasterizer` is the only other `GERasterizer`; with it on the Rust
  rasterizer's own fakes the virtual layer could fold into one concrete class (about 250 lines).
- `UILBString` whole (about 60 lines); the toolkit's `UIObjHost::Base` answered from the record in Rust.
- The store gate's link check fails on the rust branch before this round (the one `ee_game.lib` brings the
  screens', units' and Lua's C symbols; the driver links with `/FORCE:UNRESOLVED` and runs): its allowed list wants
  those prefixes, or the driver a narrower library.
