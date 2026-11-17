# Object storage, lane S: screens B and the editor onto the world's views

The lane's state (object storage plan, lane S; branch `rust-objS`). Each round rewrites this file to the current
state. Lane V (`rust-worldviews`, `ee_sim::world`) builds the typed views; the move waits for it.

**Round 1, the preparation (before V).** TEST ONLY code and goldens only; no C++ case moved.

- The SGVW trace (`ee_game screens/sgvw.rs`, `EE_SGVW=<file>`): every read the move will touch, recorded where the
  Rust asks, so the same lines must come out whichever side answers. The world lock's holds by the reading thread at
  each read come from `ee_base ts/held.rs` (TEST ONLY: counts per thread and lock in the four lock entry points,
  on only under `EE_SGVW`). The goldens and how to check them: "SGVW" below.
- The measure of the cases the move takes (below), and the read paths and their locks ("Threads").

## SGVW

Recorded on the unmodified C++ (this branch's TEST ONLY hooks only), two runs each:

| Session | Lines | Runs | Golden |
|---|---|---|---|
| the editor gate (`python -I tools/sgvw.py editor OUT`: `sim_editor.py` with `EE_SGVW`; EE-SIM, a pinned clock) | 25,136 | identical byte for byte; the gate PASS 14,448 both times | `tools/sgvw/editor.txt` (exact) |
| the fixture tour (`slot.py run -- fixture_game.py --exercise tour --play-seconds 60 --input-check`) | 94 | identical once the CT count's answer is masked (`TIMED`: `V 377`, the session's `GetNumberOfCTsProcessedInPhysics`, a clock) | `tools/sgvw/tour.txt` (exact or `--set`) |
| the orders game (`--exercise random-game --orders --box-select`) | 74 | one run: nothing the tour does not read | none |

Check after the move: `python -I tools/sgvw.py editor OUT` then `compare tools/sgvw/editor.txt OUT`; a tour with
`EE_SGVW=<abs path>` then `compare tools/sgvw/tour.txt OUT` (`--set` if a timing change reorders the menu reads).

**What the goldens cover.** The editor gate reaches 32 of the 35 editor world ops (`sgvw.rs EDITOR_WORLD_OPS`;
`E` lines; the classes op alone 15,264 times, `PLAYER_GET`/`PLAYER_SET` 1,792/1,578, `ENEMY`/`SET_STANCE` 1,440
each). Not reached: the unit tab's `PICK` (348), `HOLD_GATE` (349) and `DELETE_SELECTION` (355): the gate's
script picks, holds and deletes units through the actions, never these ops. The live sessions reach only the read view's
settings and session groups (the settings form's sound, rasterizer, camera and version reads; `IS_SERVER`; the lobby
slot colours, `LE_PLAYER_COLOUR`) and the main menu's `GO_FORM_EXISTS`. **No session reaches the view's `WORLD`
and `PLAYER` groups, the player texts, the hot keys' snapshot, the diplomacy form's player, the post-game
records, or the three host world reads** (`MG_SELECTED_COMMAND`, `MG_DIALOG_SHOWN`, `DSU_SOUND`): in play the HUD
reads the world through its own hosts, not this view, and the fixture opens no diplomacy, options menu, chat,
hot key or summary. Before those cases move, either a fixture step reaches them (a hot key press, the diplomacy
form, the in-game options menu, a chat line), or they are checked only by round 4's stand-in-world golden
(`game_view_golden.rs`), which replays the C++ fills and cannot run over V's views.

## The measure

The plan's counts (99 in `EEScreens.cpp`, 42 in `EESEUnit.cpp`, 70 in `EESERBUnitTypes.cpp`) are not what the
files hold: `EEScreens.cpp` has 372 case labels, `ee_screenscpp_call`'s 97 of them all game calls (forms, the
session's messages, files, the WON calls, the campaign and intro managers), not reads. What the views can answer is
the list below, case by case, with the fields each reads. "View" names the view it maps onto once V lands
(`UnitView`, `ClassView`, `StatsView`, `PlayerView`, `MapView`, `TileView`, `WorldView`); "not V" is storage V
does not cover (the statistics, the tech tree, the computer player, the settings, the session, the UI's own
objects), which stays a C++ read until a lane owns it. Views are read-only: a case that writes keeps its write in
C++ (or a write op), only its read moves.

### `EEScreens.cpp` (3,034 lines; `UI Events/`)

**The read view's fills** (`ee_screenscpp_game_view`, `ee_screenscpp_game_texts`, 2,866-3,028: 157 lines with
`EEScreensViewPlayer`). Only the world and player groups are world objects:

| Group | Fields | View |
|---|---|---|
| `WORLD` | `GetNumGhostUnits()`, `mCameraControl.mMode`, `mCameraControl.GetZoom()`, `mGameOptions.GetLockSpeed()`, `GetGameSpeed()`, `mInCinematic`, `mUserPaused`, `GetNumPlayers()`, `mCurrentPlayer`, `GetTeamsLocked()`, `GetCheatsEnabled()`, `GetGameOptions().GetIsScenarioGame()`; `has_player` (`GetPlayer(mCurrentPlayer) != NULL`) | WorldView (the ghost count, the camera and the game options are EEWorld members V must expose: not in its list yet) |
| `WORLD` (rest) | `gWorldTable`'s speed step and range, the main game form's `mHotKeyMode` | not V (db table, UI) |
| `PLAYER` | `GetPlayer(i < 17)` / the current player, `GetColor()`, `GetObserverMode()`, `mChatType`, `mSelectedGroup` (`GetNumUnits`, `GetUnit(i)->mUnitClass->mUnitType == kUnitTypeAirport`) | PlayerView (colour, observer, chat type), its selected group as unit ids, UnitView -> ClassView `unit_type` |
| `TEXTS_PLAYER` | `GetName()`, `mCivilization.mName` | PlayerView (two texts; UTF-8 bytes) |
| `TEXTS_WORLD`, `TEXTS_RECORD` | `mStatistics.mCampaignName`, `mStatistics.GetRecords()[i].GetName()` | not V (the statistics) |
| `SOUND`, `SETTINGS`, `SESSION`, `COLOUR`, `CAMPAIGN`, `INTRO`, `TEXTS_SETTINGS/CAMPAIGN/INTRO/MAP_TYPE` | globals, `gGamePreferences`, `gCommunications`, the colour table, the two form managers, the lobby options | not V |

So of the about 160 fill lines, about 35 (the `WORLD` and `PLAYER` groups and the player texts) can go when V
lands; the other groups need their own owners (the settings and session to the app lane, the managers to this
lane's `EEUISCIFormManager`/`EECampaignFormManager` objects).

**Record fills** (Rust structs filled by C++; each one read path):

| Function | Fields | View |
|---|---|---|
| `ee_screenscpp_hot_key_units` (415-493) | current player's `mSelectedGroup`, `mFirstUnit` chain (`mNext`); unit `mUnitID`, `mComplexUnit`, `mHitPoints`, `mTile`, `mUnitClass->{mUnitType, mCategory}`, `mStats->mMaxVelocity`; complex unit `mUnitID`, `mCommand->mType`, `mUnitClass->{mCategory, mFamily, mAmmoType, mUnitType}`, `mHitPoints`, `mIsCommandable`, `mTileX`, `mContainer`, `mIsDisabled`, `mPreviousPart`, `mGoalState->mKind`, `mStats->mMaxHitPoints`, `mPhysicalModel.GetLocation()` (or the container's); `EESelectedGroup::Contains` | PlayerView (unit list, selection), UnitView, ClassView, StatsView. Not V: the aircraft target, `gWorld.mMap->GetLocationAt` / `GetTileAt` at the render viewport's centre (a map pick, a C++ call); `sHotKeyUnits` (the complex units kept for `HK_CENTER`) |
| `ee_screenscpp_diplomacy_player` (888-906) | `GetPlayer(i)`, `GetPlayerID`, `GetGameResult`, `GetObserverMode`, `IsHuman`, `GetSlot`, `GetResource(food..iron)`, `GetEnemy(q)` and `GetSendChatMessage(q)` for 17, `GetTributeFee` | PlayerView (all) |
| `ee_screenscpp_pgs_history`, `_pgs_record` (967-1011) | `gWorld.mStatistics` and its records, `EEGetResourceLevel(gGameResources)` | not V (the statistics) |

**Host cases that read the world** (the call chain under `ee_screenscpp_call`'s default):

| Case | Reads | Writes / calls | View |
|---|---|---|---|
| `MG_SELECTED_COMMAND` | the main group's `GetSelectedUnit()`, its `mComplexUnit`, `mUnitClass->mCommands[i]` | none | UnitView, ClassView (the selected unit stays the UI's) |
| `MG_DIALOG_SHOWN` | control 0x.. display mode, `gWorld.mInCinematic` | none | WorldView for the cinematic flag; the control stays a toolkit read |
| `DSU_SOUND` | `EEGetFirstSelectedUnit`, complex unit's class: `mAcknowledgeSounds[0,1]`, `mCreationSound`, `mDeathSound`, `mSelectionSounds[0,1]`, `mModel->mSounds1/2[0, 13]` | keeps the `EESoundObject*` in `sDebugSound` | ClassView only if it exposes the sound pointers; otherwise stays |
| `OP_DS_FG` | `GetPlayer(b)->GetColor()` | the control's text colour | PlayerView colour + toolkit write |
| `OP_HK_GROUP_BUTTON` | current player's `mSelectedGroup->GetFirstUnit()` | the button's `SetUnit` | PlayerView selection + toolkit call (needs the unit pointer) |
| `HK_CENTER` | `sHotKeyUnits[i]->mPhysicalModel.GetLocation()` | camera `SetChanged`, `SetLocation`, mini map, `gCameraFollowing` | UnitView location; the camera stays C++ |
| `MG_FOLLOW_LEAD` | `GetUnit(EEGetLeadUnitID(current))` | camera `Follow` | stays (a call with the unit pointer) |
| `PGS_ADD_SERIES` | a statistics record's series | the graph's `AddGraphSeries` | not V |
| `MG_MARK_HIDDEN_MOVED`, `MG_SET_FORMATION`, `MG_CLEAR_GHOSTS`, `MG_TO_EDITOR`, `MG_ATTACK`, `PDCS_REQUEST`, `HK_UPDATE_SELECTION`, `HK_RESET_SAVED`, `LE_FULL_PLAYER`, `GAME_PAUSE`, `CL_UNPAUSE`, `PDCS_PAUSE`, `SS_LOCK`, `LOBBY_BEGIN/END`, `TEXT_LOAD_SAVED_GAME` | (world calls or writes) | | stay C++ |

`ee_screenscpp_text` and `ee_screenscpp_put_text` read no world object.

### `EESEUnit.cpp` (1,120 lines; the unit tab's host `ee_screenscpp_seun`, 65 cases)

| Case | Reads | Writes / calls | View |
|---|---|---|---|
| `CLASS_COUNT` | `EEUnitClassCount()` | | ClassView (the table's count) |
| `CLASS` | `gUnitClassTable[i]`: `mUnitType`, `mIsScenarioSelectable`, `mIsGaiaPlaceable`, `mIsPlayerPlaceable`, `mAIKind`, `mCategory`, `mDomains`, `IsLandBuilding()` | | ClassView (`IsLandBuilding` as a Rust rule over the class's fields) |
| `CARGO_COUNT` | the editor's selected unit i (`GetSelectedUnit`), its `mComplexUnit`, `GetNumCargoUnits()` | | UnitView (cargo count); the selection stays the editor form's |
| `PLACE_INFO` | `GetPlayer(i0)->GetPlayerID()`; the list's `mUnitTypes[line]` -> class `mTechnology`, `mIsGaiaPlaceable`, `mIsPlayerPlaceable`, `mCategory`; the player's tech tree `Get(tech)->mData->mKind` | | PlayerView, ClassView; the tech tree node not V; the list member stays a toolkit read |
| `PICK` | `gUnitPicker.PickUnit` (the renderer's), then `mUnitID`, `mUnitClass->mCategory` | `Dereference` | UnitView/ClassView for the two fields; the pick stays C++ |
| `LIST_ADD` | `AddUnitType`: class `mNameStringID`, `mFamily`, `mEpoch` (and the sort's comparators) | the list's insert | ClassView for the fields; the insert is the toolkit's |
| `CARGO_ADD` | selected unit's `mComplexUnit->mCargo[j]`, its class `mNameStringID` | the cargo list's insert (keeps the unit handle) | stays (the list holds `EEUnitHandle`s) |
| `START_BUILD` | as `PLACE_INFO`, the node's `mData->mID`, `GetUnitClass(0)` | the tab's build tool members, `Initialize` | read part as `PLACE_INFO`; writes stay |
| `HOLD_GATE` | `GetUnit(id)`: class `mTechnology`, `GetTechTreeNode()->GetUnitClass(0)`, class pointer, `mPlayer->mPlayerID` | `Kill`, the gate tool | stays (a kill) |
| `GATE_HIGHLIGHTS`, `DELETE_SELECTION`, `SHOW_UNIT`, `POST_GHOST` | a player / the current player's id / a unit pointer / the list's class | calls | stay |

The other 55 cases are toolkit, action, ghost-lock, mouse and keyboard ops: not the views'. So the unit tab has 6
plain reads (`CLASS_COUNT`, `CLASS`, `CARGO_COUNT`, `PLACE_INFO`, and the reads inside `PICK` and `LIST_ADD`), not 42.

### `EESERBUnitTypes.cpp` (1,027 lines; the shared host `ee_screenscpp_sepl`, 71 cases, with `SeplWorld` 12 and `SeplPlayer` 10)

| Case | Reads | Writes / calls | View |
|---|---|---|---|
| `WORLD_GET` (`SeplWorld`, 12) | `mPopulationLimit`, `mNumberOfWondersToWin`, `mVictoryEnabled`, `mTeamsLocked`, `mCheatsEnabled`, `mUnitImprovementsAllowed`, `mUsingCarbon`; `mGameOptions.{mUnitLimit, mWondersForVictory, mVictory, mLockTeams, mCheatCodes}` | | WorldView (12 fields; the game options are an EEWorld member V must expose) |
| `WORLD_SET` (12) | the same, after | `SetPopulationLimit`, 11 stores | write stays C++ (the read after it can be the view's) |
| `PLAYER_GET` (`SeplPlayer`, 10) | `mPlayers[i0]`: `mResourceCaps[k]`, `mSharedLOS`, `mPlayerID`; tech tree `mCurrentEpoch`, `mEndEpoch`, `mStartEpoch`; `gWorld.mGameOptions.mPlayers[i0-1].{mColor, mB36}`; the computer player's 8 flags (`mActive`, `mIsCheating`, `mCanAttack`, `mCanDefend`, `mCanBuild`, `mCanExplore`, `mIgnoreOthers`, `mCanGather`) and `mCitizenPercentage` | | PlayerView (caps, LOS, id), WorldView (game options' players); not V: the tech tree, `EEComputerPlayer` |
| `PLAYER_SET` (10) | as `PLAYER_GET` | `SetSharedLOS`, `SetActive`, `SetCitizenPercentage`, stores | write stays |
| `NUM_PLAYERS` | `GetNumPlayers()` | | WorldView |
| `PLAYER_EXISTS` | `GetPlayer(i) != NULL` | | WorldView / PlayerView by id |
| `IS_COMPUTER` | `mPlayers[i] && mPlayers[i]->mComputerPlayer` | | PlayerView (a "has computer player" flag) |
| `ENEMY` | `mPlayers[i]->mEnemies[j]` | | PlayerView (`mEnemies` is in V's member list) |
| `MAX_PLAYERS` | `gMaxPlayers` | | not V (a global) |
| `LOAD_THREAD` | player's tech tree `mStartEpoch` | posts to the load thread | not V (tech tree) |
| `NAME_TO_EDIT`, `NAME_TO_LABEL` | `mPlayers[i]->mName` | the control's text | PlayerView name + toolkit write |
| `CIV_SELECT` | `mCivilizationFile`, the civilization manager | the combo's selection | PlayerView text + toolkit |
| `CIV_APPLY`, `APPLY_COLOR`, `NAME_FROM_EDIT`, `EPOCH_RANGE`, `SET_STANCE` | player fields | writes and calls | stay |
| `TECH_TYPES`, `FILL_TECHS` | `gTechTree`, class table (`IsLandBuilding`, `mAIKind`), the player's tech tree nodes (`mShowInEditor`, `mScenEditorObsoleted`) | the tech lists | ClassView for the classes; the tech tree not V |
| `UNIT_LIMIT_RULES` | `EEGetRules()` | | not V |

The other 45 cases are the toolkit's. Plain reads the views take: `WORLD_GET` (12 sub-cases), the V-covered part
of `PLAYER_GET` (`CAP`, `SHARED_LOS`, `ID`, `OPTIONS_COLOR`, `OPTIONS_CPU`: 5 of 10), `NUM_PLAYERS`,
`PLAYER_EXISTS`, `IS_COMPUTER`, `ENEMY`, and the name reads of `NAME_TO_*`: about 23 reads, not 70. The tech tree
(4 `PLAYER_GET` sub-cases, `LOAD_THREAD`, `FILL_TECHS`) and the computer player (`PV_AI`, `PV_CITIZENS`) need a
view V does not plan.

### What V's API needs for this lane

- WorldView: the player count, the current player, the ghost count, the camera's mode and zoom, the in-cinematic
  and user-paused flags, the game speed, the team lock and cheats, and `mGameOptions` (lock speed, scenario game,
  unit limit, wonders, victory, lock teams, cheat codes, the per-slot colour and `mB36`); `mPopulationLimit`,
  `mNumberOfWondersToWin`, `mVictoryEnabled`, `mTeamsLocked`, `mCheatsEnabled`, `mUnitImprovementsAllowed`,
  `mUsingCarbon`. Most are not in V's member list (RTSWorld `mPlayers/mMap/mNumPlayers`, EEWorld
  `mCurrentPlayer`): they must be added, or the lane's world reads stay C++.
- PlayerView: by index 0..16 with absent slots, colour, observer mode, chat type, name and civilization name (text),
  the selected group as unit ids, the unit list, id, slot, game result, human, resources, enemies, send-chat flags,
  tribute fee, resource caps, shared LOS, "has computer player", civilization file.
- UnitView / ClassView / StatsView: as the hot key snapshot lists; the class table by index with its count; the
  class's command list (`mCommands`).

## Threads

The screens and the editor run on the UI thread (in the game; in EE-SIM's editor gate on the sim's thread). The
world lock is `gWorld.mLock` (an `ee_base` `EeTsLock`: readers share; a writer takes the section, then waits for no
readers). A nested read by a thread that already reads is safe (the reader count only grows). A read taken by a
thread that holds the WRITE lock deadlocks (it waits for "no readers", which the writer has consumed): a view's
token must never be a new read lock where the thread already writes.

### What the trace measured

The trace's `|L<r><w> t<n>` (the world lock held by the reading thread when a read began):

| Path | Thread | World lock | Reads |
|---|---|---|---|
| the editor's actions (`ProcessActions`: `EE_SCREENS_ACTION_LOCK` takes `LockUIForCopyData` and the world's **write** lock around each action) and the tab switch (`0x421`, `SELECT_TAB` under the write lock) | UI (t1) | **write** (`L01`) | 16,738: every player tab read (`PLAYER_GET/SET` 39/40, `WORLD_GET/SET` 52/53, names, colours, enemies, civilizations, epochs, the load thread), the unit tab's placement, cargo, selection, gate and pick ops, and half the class reads |
| `UpdateInterface`'s `UpdateUI` (per frame; read lock) | UI (t1) | read (`L10`) | 8,367: the unit tab's list refresh (`CLASS` 345, `CLASS_COUNT`, `LIST_ADD`), `SHOW_UNIT`, `UNIT_LIMIT_RULES`, `FILL_TECHS`; in the tour the settings form's reads and the in-game `CTS_PROCESSED` |
| the editor's construction (the player tab's first `NUM_PLAYERS`, `PLAYER_EXISTS`, `MAX_PLAYERS`, `TECH_TYPES`, `FILL_TECHS`) | a second thread (t2) | **none** | 8 |
| the main menu's events, the lobby's updates (`0x436`-`0x439`: the UI lock only), the lobby's slot colours, `IS_SERVER`, the version text | UI (t1) | **none** | 3 (editor gate), about 70 (tour, orders) |

### The read paths the move touches and their locks

From the UI thread's message switch (`ui_thread.rs handle_message`) and the C++ entry points:

- **Read lock held**: `UpdateUI` (every control's update, hot keys, per-frame form work, the editor tabs'
  `Update`); `0x40e` (a posted event: `PROCESS_EVENT`); `0x40d`/`0x414`/`0x419` (form swap, push, pop: their open and
  close events); `0x403`, `0x424`, `0x435`; `0x417`'s error callback; `0x429`'s pop.
- **Write lock held**: every action (`ProcessActions` under `EE_SCREENS_ACTION_LOCK`: the editor's and the game's
  actions, their host ops); `0x421` (the editor's tab switch: `Leave`/`Enter`/`Load` of the tabs); `0x407`'s screen
  flag; `ResizeGameScreen`'s mini map redraw.
- **No world lock**: `0x418` (chat show), `0x422`/`0x427` (information), `0x430` (pop after the host left), the
  lobby's `0x436`-`0x439`, `0x408` (load saved game), `0x411`/`0x410`/`0x412`/`0x413`/`0x415` (editor and game
  transitions), `0x41f`/`0x420` (civilization builder), `ProcessActions`' own bookkeeping outside an action; the
  main menu's events; the editor's construction on the second thread; EE-SIM's scripted steps outside these.

### Where the move must take a token

`EeTsLock` is not re-entrant for a writer: a thread that holds the write lock and asks for a read waits forever
(the read waits for "no readers", which the write took). A nested read under a read is safe. So:

1. **Never take a read lock inside an action or the tab switch.** Two thirds of the editor's world reads (all of
   the player tab's) run under the write lock. The views there need a token that says "this thread writes": V's
   `ee_world_read_begin` must not be a plain `ReadLock`. Options: a token made from the caller's state
   (`ee_base::ts::held::held(gWorld.mLock)` is (0, 1): borrow, take nothing), or the action host passing its write
   token down to the editor host (`ee_screenscpp_sepl`/`_seun` callers in `editor_*_ffi.rs`, the action path in
   `game_actions.rs`/`editor_whole.rs`).
2. **Read lock already held (`UpdateUI`, posted events, form swaps)**: a nested read is safe; a token that borrows
   is cheaper and equal.
3. **No lock: take one.** The lobby and main-menu reads (settings and session groups: not V's anyway), the
   editor's construction on the second thread (`NUM_PLAYERS`, `PLAYER_EXISTS`, `TECH_TYPES`, `FILL_TECHS`: the
   world is loaded and the physics stopped there, but a token must still be a real read lock), the chat show and
   the information messages if a moved read is reached from them, and `0x408`/`0x411`-`0x415` transitions.

The simplest rule for V: `ee_world_read_begin` takes the read lock only when `held()` says this thread holds
neither (the `held` counts are TEST ONLY today; in the game the lock itself would need a per-thread owner or the
callers pass the state). This lane's move will pass the token explicitly: the action and tab-switch paths
(write), `UpdateUI` and the message handlers (read), the construction (take).
