# The player and tech tree lane

The lane's state: ee/EEPlayer.h (the player, the RTS player, the event list, the statistics, the groups' C++ names
and the tech tree classes), src/Empire Earth/Player, the tech tree (ee/EETech*, src/Empire Earth/Tech Tree), the
civilization builder (ee/EECiv*, src/Empire Earth/Civilization Builder) and the CP headers (ee/EECP*). Each round
rewrites this file to the current state; the rounds' stories go in PORT_LEARNINGS.md ("The player/tech lane, round
N"). The earlier player rounds are docs/progress/r5b-player.md, the tech tree's r5b2-tech-tree-events-calamities.md,
the civilization builder's r6-civilization-builder.md.

Measure: logic lines (code minus one-statement shims, the scratchpad's logic_lines.py) over the lane's files
(pl1_scope.py: ee/EEPlayer.h, ee/EETech*, ee/EECP*, ee/EECiv*, the Player, Tech Tree and Civilization Builder
folders).

## Round 1 (branch rust-playertech1, 2026-11-24)

1,223 logic lines at batch 23 (fba26cc96) -> 587 (the whole tree from batch 24: 19,021 -> 18,345 logic, 114,065 ->
112,106 C++ lines, 369 -> 368 compiled files, 855 -> 845 classes).

- **The player** (lead): EEPlayer's 99 one-statement members (the economy's, the selection's, the diplomacy's, the
  lists', the media's and the units' forwarders to ee_units player_*.rs) and RTSPlayer's 14 (ee_game rts units.rs)
  are inline in their classes; `EEPlayerRefs` (the Rust player's view) and `EEPlayerUnits` are declared in
  ee/EEPlayer.h for them. The empty virtuals (the original's ICF-shared `ret`s) are `{}` in the class. The unit
  buckets are Rust's `EeRtsUnitBucket`, a statistics point Rust's `EeUnitsStatPoint`. EEPlayer's AddPendingUnits/ReleasePendingUnits and the event list's destructor are Rust; RTSPlayer's
  seven pure virtuals are gone (EEPlayer declares them). SetGameResult stays out of line (a harness hook). The save's two contexts are
  one (`EEPlayerSaveContext`, made in the call's argument), so Load, Save, LoadUnit and SaveUnit are one statement.
  The unused typedefs (`EETechnologyTable`, `EEPlayerSavedGroups`), a forward declaration and the statistics'
  unused key are gone.
- **The tech tree** (fork A): the tree's, epochs' and nodes' one-statement members inline over a new ee_game module
  `tech_tree` (its own small header: EEPlayer.h does not include gamecpp.h); GetCost, GetCostString, SetPlayer,
  GetPlayer, GetHotKey and the epoch's node lists Rust (`EeTechEpochLists`); the inline bases RTSTechTreeNode and
  RTSTechTree folded into their classes; EETechTreeEpoch.cpp deleted; the last capture hook pointers gone.
- **The civilization builder and the CP headers** (fork B): `EEAreaEffectKind`, `EECivUpgrade`, `EECPPlacement` as
  Rust's (cbindgen); EECivilization's, the area effects' and the CP headers' one-statement members inline (civilization.rs
  its own ee_game module, `rust/game/civilization.h`); the area effects' running state's file I/O Rust; dead code
  dropped.

**C++ left**: EEPlayer.cpp's hosts (`EEPlayerHost`, the save's, the units', the tick's and the towns' operations: one
statement a case), the class media setters over `EEPlayerClassStats` (they need EEUnitStats complete), the members
whose bodies need a complete type the header does not have (`UnitsSwapped`: `EEPUnitSwap`; `DetachUnitFromAI`,
`NoticeForeignUnit`, `GetNearestArea`, `GetNumWonderUnits`, `GetPopulationLimit`: the world), the constructors and
destructor; the event list's world (`EEPPlayerEventList::Host`) and the statistics' gate (`Record`) and copy
(`GetSeries`); the tech tree's `Get` lookups (a hot path) and the node data's containers (the world layout reads them
by offset); EECivilization's `CanAfford` (the lobby's options).

**Next**: RTSPlayer folded into EEPlayer (its pure virtuals and the shims ee_game rts units.rs calls); the player's
remaining containers (towns, herds, sight sharers, allies, patrol groups) into the Rust lists; the class media
setters over one Rust operation; the statistics' name into the Rust save.
