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

1,223 logic lines at batch 23 (fba26cc96) -> 528 at the batch 25 merge (587 before it: game core 13 made ee/EETechTreeTechID.h Rust's; the whole tree from batch 24: 19,021 -> 18,345 logic, 114,065 ->
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

## Round 2 (branch rust-playertech2, 2026-11-25)

Main measure C++ lines (tools/port_metrics.py): -1,257 for the round (forks -779, lead -478; the target was -1,500),
compiled files -11 (the lane's: RTSPlayer.cpp, EEPPlayerEventList.cpp, EEPlayerStatistics.cpp, EETechCapture.cpp,
EETechTreeNode.cpp, EECivilization.cpp, EECalamityCapture.cpp, EEAreaEffectTemplate.cpp, EEAECapture.cpp,
EECBUIListBox.cpp, EECivCapture.cpp), classes -4, logic -70 (the lane's scope 528 -> 496). The whole tree at the tip:
104,305 C++ lines, 326 compiled files, 15,114 logic lines, 776 classes.

- **The player's hosts** (lead): ee_sim units player_native.rs answers the body, player, tick, save, unit-save and
  towns' save operations that only wrote a pointer member (the world layout gained mActiveGroup, mGroups,
  mHumanPlayer, mHealedUnitSet, mAirTraffic, mLastUnit and the town record's base), a Rust record field (the
  player's, a complex unit's, a unit's, a node's, a town's), read a unit, class, stats or the physics through the
  world views, or only called another Rust export; the game's hosts ask it before the C++ (the bodies' test hosts
  are unchanged). About 130 cases and EETownCounter went.
- **RTSPlayer folded into EEPlayer** (lead): one class (RTSPlayer a typedef for the engine's files); its overridden
  virtuals gone, the hosts calling the Rust bodies directly. RTSPlayer.cpp, EEPPlayerEventList.cpp and
  EEPlayerStatistics.cpp joined EEPlayer.cpp (the constructors, the statistics' Load/Save/GetSeries and
  EEAddDeadUnitStatistic inline). The class media setters share one guard.
- **The containers** (fork B): mTowns, mHerds, mAllies, mSightSharers, mPatrolGroups, mMediaWarnings,
  mConstructionSites and mWonders are lists of the player's Rust object (`ListOp` over `EeUnitsPlayerLists`); the
  world view reads them; their host cases and the four layout fields are gone.
- **The tech tree and the civilization** (fork A): the node data's class costs and unit classes a Rust table
  (`EeTechClassTable`), the tree's player index and player in its record, the capture headers gone (Rust calls the
  capture records), CanAfford and the civilization's other members inline over civilization.rs, the civilization
  host's per-field shims as record shims; the small civilization builder units merged (lead).

**C++ left**: EEPlayer.cpp's hosts' cases on C++ objects (the tech tree's Get and walks, the unit classes' upgrades
vector, the herds', patrol groups' and towns' methods, the UI and sound, the text and the temporary vectors the C++
calls take), the virtuals the computer player overrides (TakeResource, ScaleCost, CanAfford, GetPopulationLimit and
their host cases), the event list's sounds (mSound, mPlayed, mSounds), the base save's color and name; the tech
tree's epoch list and the node's mData/mTree/mButton; EECivilization's vectors and name.

**Next**: the event list's sound members into its Rust record; the unit save's loaded id (pass the player to
ee_units_player_unit); the herds' and patrol groups' fields through the world layout as the towns'; the player's
remaining C++ pointer members (mUnitClassUpgrades as a Rust list of upgrades, mTechnologies) toward a Rust-owned
record; the dead Rust exports fork A listed.
