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

## Round 3 (branch rust-playertech3, 2026-11-26)

Main measure C++ lines (tools/port_metrics.py): -767 for the round against origin/rust (batch 29; fork A -410, fork B
-320, the lead -37; the target was -1,500), compiled files -3 (EEAreaEffect.cpp, EECBCivilizationFiles.cpp,
EECivilizationManager.cpp merged into their neighbours), classes -3, logic -25, extern_c -15. The whole tree at the
tip: 88,167 C++ lines, 266 compiled files, 12,122 logic lines, 721 classes.

- **The civilization builder's shims** (fork A): civ_direct.rs answers the builder's toolkit calls (ee_ui::direct),
  its world, table and Rust-record reads (game options, unit sets, classes and upgrades, the tech nodes' researched
  and allowed flags, the sound manager, the capture's set walk) through ee_sim's views; game_cpp.rs's `shims!` takes
  a Rust path (`= civ_direct::name`) in place of a C++ symbol. EECBUIListBox's members are civ_list_state.rs';
  EECivilization's plain members the Rust base EeGameCivFields. About 160 shims, the dead members of the builder,
  the area effects and the CP headers (ee/EECPExperimentTimers.h whole), and five units merged into two.
- **The dead Rust exports** (fork A): ee_area_effect_* (but mark_dirty and checksum), ee_tech_effect_*,
  ee_screens_cb_out_*.
- **The tech tree as Rust records** (fork B): EeTechNodeFields gained mData, mTree, mButton and mLists (an epoch's
  lists), EeTechTreeFields mEpochs; the node, epoch and tree classes have no vtable or members of their own, so an
  object pointer is its record (layout node_fields and tech_tree_fields 0). Rust makes and frees the nodes, epochs and
  node data, walks the tree and dispatches the epoch's virtuals on mLists; about 140 host cases (EETechTree.cpp,
  EEPlayer.cpp, the scenario, the classic_lua hosts, the game's) answered in Rust; ee/EETechTreeRecord.h and
  EETechTreeEpochList gone.
- **The player's hosts** (lead): EEComputerPlayer's ScaleCost and TakeResource dispatched in Rust on mComputerPlayer
  (player_native.rs), its four overrides equal to EEPlayer's gone (batch 29 had already inlined its other members);
  the unit load's loaded id pushed in Rust (the player passed to ee_units_player_unit); the save primitives over
  ee_base's CppFile (EEPlayer.cpp's ee_units_file_* shims gone).

**C++ left**: EETechTree.cpp's media, UI, statistics, file (FSFileSpec virtuals), TkUpgrade and TkPushId cases; the
player's `new/delete EETechTree` (BODY and SAVE must move together); mUnitClassUpgrades (52 uses in many lanes'
files); the tech cases in Units/ (units13's), the UI's, the editor's and EEPlayer.cpp's BODY_TECH_* cases, all
answerable now the nodes are records; the event list's sounds; the herds' and patrol groups' fields via the layout;
EEComputerPlayer's CanAfford, GetPopulationLimit and GetHeroCount virtuals; UWideString and the temporary vectors.

**Next**: EEPlayer.cpp's BODY_TECH_* cases and the other lanes' tech cases over the node records; the tree's
new/delete in Rust; the event list's sounds into its Rust record; herds and patrol groups through the layout.

## Round 4 (branch rust-playertech4, 2026-11-26)

Main measure C++ lines (tools/port_metrics.py against the lead's merge of batch 30): -515 for the round (the target
was -1,500), compiled files -3 (EETechTree.cpp, EECalamity.cpp, EECalamityManager.cpp gone), logic -11,
extern_c -5. Two forks in two rounds each and the lead; batch 30 merged at the start. Behaviour unchanged.

- **The player's tech tree in Rust** (fork A): player_native.rs answers the body and save hosts' TECH_* cases; the
  player trees are boxed EeTechTreeFields made, copied from gTechTree, freed and saved through a table ee_game
  registers in ee_sim (tree_objects.rs `TreeBodies`, ee_rust_init); EETechTree's ctor-free members with no caller
  gone.
- **EEComputerPlayer's virtuals** (fork A): GetHeroCount, GetPopulationLimit, CanAfford and ScaleCost are one Rust
  entry (`ee_units_player_virtual`, player_econ.rs) that dispatches on mComputerPlayer; the explicit `EEPlayer::`
  base calls pass `base`; the four overrides and EEPlayer::GetPopulationLimit's body gone.
- **The other lanes' tech statements** (fork B): the units hosts' (ee_sim units tech_cases.rs), the editor's
  (screens editor_tech.rs), the scenario's, the actions', the UI's (screens tech_direct.rs), the AI's and the
  civilization builder's tech and upgrade one-liners answered in Rust (about 150 cases and shims).
- **mUnitClassUpgrades read in place** (fork B): the layout's player_unit_class_upgrades; player_upgrades.rs reads
  the vector's size, an element and a class's upgrade for every host that only read it. The vector's growth and
  the upgrades' new/delete stay C++.
- **EEPlayer.cpp's hosts** (fork A, round 2): the temporary vectors of ClearFootprint and KillHerdUnits are Rust
  Vecs, the result line's texts are made in Rust, the herd, town, patrol group, area and continent manager
  statements go through the layout (herd_fields, town_patrol_group, town_area, herd_area, player_name_text,
  player_civ_name_text) and ee_cp's bodies; 31 dead inline members of EEPlayer.h gone.
- **EETechTree.cpp gone** (lead): its operations that repeated another chain's C++ call use that one (the file's
  open/length/close, the class media and buttons, IsHuman, the statistics' Set, the sound and media loads), the
  buttons call ee_game's bodies, gTechTree is `objects::global_tree()`; the rest of the chain ends EEPlayer.cpp,
  the calamity host's new/delete join EEContagionManager.cpp. EECalamity.cpp (one save key) and
  EECalamityManager.cpp (a shim the Rust called back through EEGameOps.cpp) gone.

**C++ left**: EEPlayer.cpp's hosts are now mostly C++ object work (new/delete, class Load/Save, virtuals, the sound
and minimap globals, gResource into UWideString, gCivPowersManager); the civilization builder's file-context,
scratch and UI shims; the area effects' C++ members; the upgrade vector itself; TECH_CHECKSUM; the Pay cases
(virtual).

**Next**: the area effects' location/template/unit into their Rust record (frees most of EEAEManager.cpp); the
upgrade vector as a Rust record; a registered EEPlayerHost pointer in Rust for the body host's remaining calls; the
event list's sounds.
