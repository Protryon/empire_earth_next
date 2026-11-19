# Calamities and the scenario manager: whole classes (`src/Empire Earth/Calamities`, `src/Empire Earth/Scenario Manager`)

The lane's state (one file per lane; the round's story is PORT_LEARNINGS.md "Calamities and scenario manager, round
1"). It follows the calamities lane (docs/progress/r5b-calamities.md, rounds 1-3) and the scenario lane
(docs/progress/r5b5-scenario-manager-and-triggers.md, rounds 1-9); their Rust homes are `ee_sim::tech` (the
calamities) and `ee_sim::scenario`.

## The measure (round 1, at origin/rust c449d76b0: 8,960 lines in the two directories, 2,243 in their headers)

Which classes can move whole now that the world views, the AI thread and the players are Rust's:

| File | Lines | What is left in it | Whole-movable now? |
|---|---|---|---|
| EESMEffect.cpp | 1,465 | the effect's strings and Copy, `EESMCreateUnit`, Set AI Variable's stores, the effect world host (one case per world statement: orders, goals, groups, cinematics, camera, calamity casts, class attributes, unit changes) | No: every case is a call into a C++ unit, group, goal, player or camera; the reads among them (`SMEUnitValue`, the unit change's values) can go to the views |
| EECalamityUpdate.cpp | 1,337 | the update/start/stop host: the classes' world steps (effects, sounds, units created and moved, orders, the camera shake) | No: world calls; its plain reads went to the views (this round) |
| EEScenarioManager.cpp | 818 | the cinematics (camera, UI, sound), the round's and the lists' and the stream's hosts, the dialog messages | No: the element lists' and streams' hosts serve the C++ elements the editor gate (EESimEditor.cpp) reads in place |
| EEEvent.cpp | 713 | the event effects' handlers (each a C++ setter over ee_tech's decision), the tables' load | The tables (`gEventEffects`, `sEvents`) could; the handlers are world calls |
| EESMObject.cpp | 642 | the object's reads host, `EESMUnitField`, `EEChangeUnit`'s wrappers, the trigger and its wrappers | The unit fields went to the views (this round); the trigger's wrapper lists are read in place by the editor gate |
| EECalamityCapture.cpp, EEScenarioCapture.cpp | 439, 598 | TEST ONLY records over the C++ objects | Not a class move |
| EEContagionHelper.cpp + EEContagionManager.cpp | 373 + 380 | the helpers' units and the manager's list; the functors | **Yes: moved (this round)** |
| EECalamityManager.cpp | 370 | the classes' and the active calamities' lists, the creations, the update loop | Partly: its lists hold C++ objects whose members the hosts use |
| EECalamityClass.cpp + the three bases | 342 + 193 | the class's media, Initialize, Start/Stop/Update/Reset adapters; the bases' target kind and cast modes | **The bases: moved into the class (this round)** |
| EECalamity.cpp | 336 | sounds, references, the I/O host of the Rust Load/Save, the dump | Not yet: its C++ members (points, rectangle, unit and sound pointers) are used by 40+ host statements |
| EEScenarioHost.cpp | 333 | the conditions' reads host, the writes, the stream host | Its plain player, class and unit reads went to the views (this round) |
| EESMArea.cpp, EESMCondition.cpp, EESMMediaStrings.cpp | 221, 89, 95 | strings, the capture wrappers, Load/Save adapters | Read in place by the editor gate (EESimEditor.cpp, the app-core lane's) |

## Done (round 1, branch `rust-calscen1`: -966 net C++ lines)

- **The contagion manager and its helpers whole in Rust** (`ee_sim::tech` contagion_store.rs, contagion_store_ffi.rs):
  the helper list, every helper's units (by handle, referenced), unit ids, functor, calamity and timers, the next id,
  the loaded calamity indexes; create, find, destroy, swap, add and remove with the calamity flags, infection and
  cure, the update and spread, both checksums, the dumps, Load, Save, RestorePointers. `EEContagionManager` is a
  thin C++ shim; `EEContagionHelper.cpp`/`.h` deleted. The world calls are one host (`ee_contagion_world`).
- **The three calamity class bases folded** into `EECalamityClass` (target kind from ee_tech's table; the cast mode
  by it; the creations in the manager); `EEGlobalCalamityClass`, `EELocationCalamityClass`, `EEUnitCalamityClass`
  (3 units, 3 headers) deleted.
- **The includes**: every project include the 16 units compile and link without (objdiff: no function changed).
- **Plain reads on the world views**: the scenario host's player and class reads and the context unit's tile,
  complex flag and every unit field of `GetUnitAttribute` (ee_sim::scenario unit_views.rs; `EESMUnitField` keeps
  the unit variables and the model's scale); the calamity hosts' target, caster and tile reads (ee_sim::tech
  calamity_views.rs).

## Next

- `SMEUnitValue` and `EEChangeUnit`'s value reads (EESMEffect.cpp), the object reads host's unit and map reads
  (EESMObject.cpp) on the views.
- The event tables (`gEventEffects`, `sEvents`, `LoadEvents`, `Trigger`'s dispatch) as Rust data.
- `EECalamity`'s C++ members into its Rust record (needs the hosts' 40+ uses of the points and pointers rewritten).
- A whole move of the scenario elements needs the editor gate's in-place reads (EESimEditor.cpp, the app-core
  lane's) on accessors first.

## Gates

See PORT_LEARNINGS.md "Calamities and scenario manager, round 1".
