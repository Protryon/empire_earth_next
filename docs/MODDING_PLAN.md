# Empire Earth: AoC data-driven content and modding plan

Status: research plan only. No repository file was changed. Repository: `C:\Users\max\Desktop\empire_earth_next`
(clang/C++17 port, fork of the `empire_earth` reconstruction, based on `fixes-all`). Paths below are relative to it.
`src/EE` stands for `src/Empire Earth`. Identifiers and comments are provisional: they come from reverse engineering.

Companion document: the field-level inventory of the binary `db\*.dat` formats is being written separately to
docs/DATA_FORMATS.md. It did not exist yet when this plan was written. This plan refers to the tables by
name and covers the logic side, the load flow, the caps and the architecture.

Data inspected: the extracted `data.ssa` contents in `(an extracted data.ssa)\` (db, unit ai scripts, civilizations) and
the loose `Random Map Scripts\*.rmv` of the install. The install's `Data\db` and `Data\Unit AI Scripts` folders are
empty: every table ships inside `data.ssa`. The inspection scripts are `tools/datafmt/mp_dbobj.py` and
`mp_events.py`.

---

## 0. Executive summary

- **Most balance data is already data.** The binary db tables hold 848 unit records with about 336 fields each, the
  tech tree, the 2000 tech-event effects, 129 AI behaviour records, the calamities, civ powers, civ-builder
  categories, damage tables, 95 world constants and the whole UI (forms, controls, hotkeys). The text formats `.tai`
  (unit AI state machines) and `.rmv` (random-map parameters) are already human-editable.
- **The modding gap has three parts.**
  1. *Format*: opaque fixed-layout binary records, edited only as whole files, with integer cross-references.
  2. *Caps*: fixed arrays sized to exactly the shipped content. 871 unit slots for 848 records; 2000 event-effect
     slots in a file that is exactly 2000 records; 1500 events; 15 epochs; 60 families × 68 attack kinds; 35
     civ-builder categories; 17 terrain types in a 32-bit mask; 6 weapon/armour kinds; 7 resource slots; 16 players.
  3. *Logic in code*: a few hundred rules and constants keyed on literal unit-type, tech and epoch IDs, magic
     indices into `dbworld.dat`, and `switch`es that give each civ power, calamity kind, victory type and AI
     building kind its behaviour.
- **The target.** A mod layer of folders of text definitions. Every entry has a stable string key. Merge works per
  entry, with an explicit load order. The engine reads a canonical, validated content database. Conditions, effects
  and triggers form one small deterministic declarative language that replaces the scenario triggers, the tech
  events and the civ-power switch. Localisation lives in text files keyed by string.
- **The path.** First a converter from binary to text with a byte-exact round trip. Then the engine loads text by
  compiling it into the same in-memory records. The existing loaders stay unchanged, so behaviour can be proven
  unchanged with the deterministic AI-game harness. Only after that do the caps come off and the hard-coded rules
  move out one at a time.

---

## 1. What is data-driven today and what is hard-coded

### 1.1 Data-driven today

**Binary db tables.**
- 34 file keys are defined in `src/EE/Database/EEDbObject.cpp:11-44`. They are loaded in a fixed order by
  `EEInitializeDatabase` (`src/EE/Empire Earth/Empire Earth.cpp:2272-2486`). `dbobjects` is loaded separately by
  `EEUnitClass::CreateUnitClasses` (`src/EE/Units/EEUnitClass.cpp:2900`).
- The 7 UI tables are loaded by `UIManager::LoadDatabase` (`src/Low-Level Engine/User Interface/UIManager.cpp:603`;
  names at `User Interface.cpp:19-25`).

| Table | Shape (inspected) | Controls | Notes |
|---|---|---|---|
| dbobjects | 848 × 0x7a8-byte records (`include/ee/EEDbObject.h:17`, `EEUnitClassRecord.h`, ~336 fields) | Every unit, building, resource, projectile and ambient: stats, costs, category (which C++ class), AI kind, counters, AI script name, model/sound links | 700 enabled. Unit type == record index for all 848 (checked). Max type 847. |
| dbtechtree | 15 epoch sections of 0x10c-byte records (`include/ee/EETechTreeRecord.h`) | Techs and epochs: costs, prerequisites (4), epoch, obsolete epoch, researched/obsolete event, button, hotkey | Epoch ids are tech ids 0..14 (`include/ee/EETechTreeTechID.h:7-26`) |
| dbevents + dbeffects | 1500 events (100-byte name + id + effect index list) and 2000 × 88-byte effects (`include/ee/EEEvent.h:12-62`) | What researching a tech does: stat changes on a class or unit set, model/sound swaps, prerequisite changes, unit upgrades, area effects, terrain passability | **This is already a small effect language** (`src/EE/Scenario Manager/EEEvent.cpp:191-252`: 22 kinds; `Fn1CAEA4` :445-640+: about 45 stat attributes). 289 events have effects; the highest effect index used is 1565. |
| dbworld | 95 × 28-byte {float, float, id, index, long, long} (`include/ee/EEDatabaseTables.h:30-70`) | Global constants: pop limits (26/27), wonder countdown (0x22), max wonders (77), civ points (76), game speeds (0x51/0x52), elevation (53-56), cloak damage (0x58), difficulty damage scaling (0x59) ... | **No names in the file.** About 100 call sites in 35 files read it by number. |
| dbfamily | Families × 68 attack kinds (`src/EE/Database/EETerrainTypeTable.cpp:144-246`) | Damage multipliers (the "rock-paper-scissors") | At most 60 rows; 68 columns fixed |
| dbweapontohit | 6×6 (`src/EE/Database/EEUnitBehavior.cpp:22,126-141`) | Weapon vs armour-kind to-hit | 6 is a hard size |
| dbcpbehavior | 129 × 56 bytes (`include/ee/EECPBehavior.h:10-39`) | Nearly all strategic AI tuning, with Easy/Medium/Hard values in value1/value2/float | Records 16, 17, 60, 74-76, 79-81 and 83-85 have no name yet |
| dbaiunittargeting | 3 × 10 weights | Melee/short/long target weights | |
| dbcalamity | 0xEC-byte records (`include/ee/EECalamityClass.h:31-73`) | Prophet calamities and powers: cost, range, lifetime, unit set, 4 float + 6 int parameters | Some fields are 0 in every record (free slots) |
| dbcivpowers | 152-byte records (`include/ee/EECivPowersManager.h:12-31`) | Civ powers (Flaming Arrows, Expansionism, ...): bonus, unit set, tech, chance, amount | IDs offset by 0x26. The behaviour is in code (see 1.2). |
| dbcivilization | 35 categories × 600 bytes, `mLevels[60]` (`include/ee/EECivilizationManager.h:35-79`) | Civ-builder bonus categories, costs, levels | The meaning of each attribute index is in code |
| dbpremadecivs, `*.civ` | `.civ` = name + list of `(category<<6)|level` (`src/EE/Civilization Builder/EECBCivilizationFiles.cpp:300-325`) | Civilisations | Already user-editable via the in-game Civ Builder |
| dbunitset | Unit sets | Named sets of classes, used by events, calamities and powers | Code uses them in only 12 places (`gUnitSetManager.Contains`) |
| dbareaeffect, dbupgrade, dbanimals, dbunitbehavior, dbstartingresources, dbgamevariant, dbrandommap, dbterrain*, dbcliffterrain, dbcolortable | | Area effects (healing, morale, economy), unit upgrade chains, herds, start resources (5 amounts), variants (Scenario/Tournament/Standard), terrain | |
| dbgraphics, dbgfxeffects, dbsounds, dbmusic, dbambientsounds, dbbuttons | | Media tables, command buttons | |
| dbuicontrols (2.9 MB), dbuiforms, dbuiback, dbuifonts, dbuicontrolevents, dbuiformevents, dbuihotkey | | **The whole UI layout and the hotkeys are data.** Control and form events bind to C++ handlers (`src/EE/UI Events/`). | |

**Text formats.**
- **`.tai` unit AI scripts** (72 files). They are state machines of the form
  `State { anyof(..)/allof(..)/Trigger true(State) false(State) }` with `#include`.
  - Parsed by `EELoadScript` (`src/EE/Units/EEUnitAction.cpp:805-950`) through `gDataPath`.
  - Each class's script name is a dbobjects field (`include/ee/EEUnitClassRecord.h:53`).
  - The vocabulary is fixed in C++: about 250 triggers in `EEUnitTrigger::RegisterAll` (`EEUnitAction.cpp:140`) and
    about 167 actions in `EEUnitAction::RegisterAll` (`:572`). Mods can recombine them but cannot add new ones.
- **`.rmv` random-map scripts** (per map type and size). They use `#define`/`#if` variables and placement commands
  (`PlaceWorldUnitInRing`, `ResourcePlacementLimit`, ...).
  - About 144 keywords are parsed by the Default generator plug-in
    (`src/EE/Random Map/Random Map Generators/Default/Unit10.cpp`, `Default.cpp:166`).
  - The terrain algorithm (clumps, fractals) is native code.
  - **Enumerated from `gStartupDirectory\Data\Random Map Scripts\`, not through `gDataPath`**
    (`src/EE/Random Map/EERandomMap.cpp:863,934`). Mods therefore cannot overlay them today.
- **Scenario triggers** live inside scenario and save files. They are binary and edited with the Scenario Editor.
  Details in 1.2.H.
- **Localisation.** About 1017 `gResource->GetString(id)` call sites read numeric string ids from `Language.dll`,
  which is now `src/EE/Language/Language.rc`: a 5008-line RC file of string tables. Almost no user-visible English
  is hard-coded in the game code (the hits are debug and registry strings). Unit, tech and epoch names are
  `mNameID` fields in the db.

**The VFS already supports an overlay.**
- `gDataPath` (`src/EE/Empire Earth/Empire Earth.cpp:1152-1185`) searches *directories first, then archives*
  (`src/Low-Level Engine/File System/FSPath.cpp:148-172`).
- The order is: AoC `Data\`, base-game `Data\`, then AoC `data.ssa`, base `data.ssa`.
- A loose `Data\db\dbobjects.dat` therefore already overrides the archive. This is how EE has been modded so far:
  whole-file replacement, with no merging.
- Campaign archives are appended at load (`src/EE/Empire Earth/EEGameState.cpp:201,244`).

### 1.2 Hard-coded game rules (ranked)

Value = modding value (H/M/L). Effort = extraction difficulty (L = constants or a table lookup, M = a mapping
table plus call-site edits, H = new mechanism, save/network format changes or many sites).

| # | System | Value | Effort | Evidence |
|---|---|---|---|---|
| A | **Content caps** (unit slots, event effects, epochs, families, terrain types, categories) | H | M-H | §2.3 |
| B | **Magic `dbworld` indices**: ~100 sites read `gWorldTable.Get*Value(<n>)` | H | **L** | e.g. `EEUnitClass.cpp:960` (0x58 cloak damage), `EEVCWonder.cpp:47` (0x22), `EEGameOptions.cpp` (26/27/77), `EEGameState.cpp:376-389` (0x51/0x52), `EEUIFScenarioEditor.cpp:498` (76) |
| C | **Combat pipeline literals and rules** | H | L-M | below |
| D | **Civ powers: behaviour is a switch naming tech IDs** | H | M | `src/EE/Civilization Builder/EECivPowersManager.cpp:169-274` |
| E | **Civ-builder attribute map** (category level -> which stat) | H | M | `src/EE/Civilization Builder/EECivilizationManager.cpp:499-605` |
| F | **Unit-type special cases**: 1082 `kUnitType*` refs (164 named ids, `include/ee/EEUnitClassIDs.h`), 833 `kUnitCategory*` refs, ~20 type switches | H | H (many small edits) | below |
| G | **AI: epoch→unit tables, building kinds, composition heuristics, ~300-500 tuning literals** | H | M | below |
| H | **Scenario triggers**: fixed condition/effect enums, flat AND/OR chain | H | H | below |
| I | **Victory conditions**: Wonder and Conquest only, in code | M-H | M | `src/EE/Empire Earth/EEWorld.cpp:2103-2180`, `EEVCWonder.cpp:33-96`, `src/EE/Player/EEPlayer.cpp:690-706` |
| J | **Difficulty: 3 levels baked into record layouts and tactical ifs** | M | M | below |
| K | **Game options enums**: map sizes, speeds, load types, epoch range | M | L | `src/EE/Empire Earth/EEGameOptions.cpp:233, 304-333, 362` |
| L | **Calamity kind → C++ class map and per-calamity literals** | M | M | `src/EE/Calamities/EECalamityManager.cpp:80+`, `EECCTimeWarp.cpp:155,378-383`, `EECCEarthquake.cpp:266` |
| M | **Resource model**: 5 real resources in 7 slots, gold≡iron, gather-slot map | M | H | below |
| N | **Epoch-keyed media**: music per epoch, citizen models per epoch | L-M | L | `src/EE/Data Pre-Loading Thread/EEMusicObject.cpp:65+` (27 epoch refs), `src/EE/Units/EEUCBasicCitizen.cpp:481+` |
| O | **Unit category → C++ class factory** (35 categories) | M (total conversions) | H (behaviour is C++) | `src/EE/Units/EEUnitClass.cpp:2918-3058` |
| P | **`.tai` trigger/action vocabulary** | M | M (scripting hooks) | `EEUnitAction.cpp:140, 572` |
| Q | **Random-map algorithm** (native plug-in) and map-size table | M | M (sizes) / H (algorithm) | `EEGameOptions.cpp:304-333`, Default plug-in |
| R | **UI**: already data (dbui*). The binding of events to C++ handlers is code. | L | L (convert tables) | `src/EE/UI Events/` |

**C. Combat.**
- **Damage pipeline**, `EEUnitClass::ApplyHit` (`src/EE/Units/EEUnitClass.cpp:922-998`):
  - `hit = gToHitTable.GetAttackFactor(ammo, targetClass) × gWeaponToHitFactors[target][attacker]`
  - `dmg = hit × attack`, multiplied by world constant 0x58 when the attacker is cloaked (:960)
  - subtract `mArmor[weapon]`
  - apply the elevation factor, which always moves damage by at least ±1 (:962-977)
  - minimum damage 1 (:979)
  - an ally or own unit takes 0, or `ceil(dmg × k447AE4)` if the attacker is a herd animal (:981-986)
  - a dead land food source keeps `0.25f` of its food (:995)
- **Duplicates of the formula**: `ApplyAreaDamage` (:838-914) and the physics crush in
  `src/EE/Physics Thread/EEPhysicsModel.cpp:780-787`.
- **Elevation**: applies only to a hard-coded list of categories (`EEUnitClass.cpp:1151-1180`).
- **Morale**: `0.8f` / `0.6f` (`src/EE/Units/EEComplexUnit.cpp:2728-2735`). Morale from buildings counts
  `kUnitTypeHouse` only (`src/EE/Civilization Builder/EEAEMoraleFromBuildings.cpp:47,73`).
- **Tick lengths**: regeneration and garrison heal 1000 ms, power regeneration 5000 ms (`EEUnitClass.cpp:4633,4692,4718`).
- **Difficulty damage tables**: which table applies by difficulty and human/computer is decided in code
  (`src/EE/Database/EETerrainTypeTable.cpp:107-138`).

**F. Representative type special cases.**
- Volcano projectile has no damage (`EEUnitClass.cpp:869,928`); capital-ship special weapon swaps class (:934).
- Granary (`EEComplexUnit.cpp:3639`), Teleporter (:3114), Roman Legionnaire special attack (:4058), space buildings
  (:2089).
- House recomputes area effects (`EEPlayer.cpp:1031,4257`). Exploration pulses from TownCenter/Capitol (:1026).
  Slavery spawns from Capitol/TownCenter 50/50 male/female (:1718-1727). RoboticFarm (:3692).
- Capitol for cloaking (`src/EE/Calamities/EECCCloaking.cpp:74,156`); RadioMan (`EECCSpecialLocation.cpp:91`).
- Most-referenced types: Farm 63, Airport 63, PathPoint 49, NavyYard 43, Dock 39, TownCenter 35, Capitol 34,
  Granary 33.
- Fix pattern: a *tag* or *role* per unit class ("town_center_like", "granary", "house_morale") or a unit set,
  checked instead of an id.

**G. AI** (src/EE/AI Thread, 87 files).
- **Epoch → unit class tables**: anti-air, fishing boat, gates, towers (`EECPCivilizationManager.cpp:543-660`).
- **The 19 building kinds the AI counts**, with priest-tower literals (`EECPCivilizationManager.cpp:90-146`).
  Military-building switch at :413-463; house at :491; minimum military buildings per difficulty × deathmatch at
  :426-440.
- **Default favourite units** 20/242/72/730 (`EEComputerPlayer.cpp:1095-1098, 4767`). Extra population 50 for classes
  0x20 and 0x1e0 (`:1001-1006`).
- **Raw class ids in the build manager**: `EECPBuildManager.cpp:775,935,979-1111,1493,1825`. Paratrooper 0x2e1 needs
  power 0x34 (`EECPUnitSelector.cpp:642`).
- **Domain-share heuristics**, all literal ranges (`EECPUnitSelector.cpp:958-1074, 983-996, 1052`). Counter weights
  3/2/1 (`:1214-1215`).
- **Timers**: `EECPResourceManager.cpp:355,1493-1497,1533`, `EECPDefenseCommander.cpp:788,1493,1590,1700,1775`,
  `EECPCivilizationManager.cpp:265,521-533`.
- **Target scoring** ×9 / cap 20 / ×3 (`EECPAttackSubGroupUnit.cpp:282-298`). Epoch catch-up 60-120 s (`EEComputerPlayer.cpp:1054-1056`).
- **Size of the constant pool**: ~1,230 float literals, 276 ms literals and 206 `GetRandom(a,b)` ranges. An
  estimated 300-500 of them are real tuning.

**H. Scenario triggers** (`include/ee/EEScenarioIDs.h`).
- **Vocabulary**:
  - 8 condition types (:39); 7 effect types (:51), with 13 object actions (:88), 12 player actions (:105),
    media (:73), game (:62) and AI (:7) actions.
  - 67 player values (:122); 75 unit attributes (:241).
- **Evaluation**:
  - Conditions are dispatched in `src/EE/Scenario Manager/EESMCondition.cpp:118-252`; effects in
    `EESMEffect.cpp:930-1425`.
  - Logic is a flat left-to-right AND/OR chain with NOT and no nesting (`EESMObject.cpp:1839`).
  - Effects can have a delay and a random-OR group (:1876-1903).
  - Polled every 500 ms (`EEScenarioManager.cpp:91,905`).
- **State**: 10 integer variables per player (`include/ee/EEPlayer.h:1074`) and 3 per unit.
- **Storage**: "fat structs" (condition 0x88 bytes, effect 0x194 bytes) in shared pools, serialized version-gated
  (`EEScenarioManager.cpp:1620,1965`).
- **Reusable parts**: the entity selector (`EESMObject`: owner, family/class, count, area, attribute range) and the
  area (`EESMArea`). These are worth keeping as primitives.

**I. Victory.**
- `EEWorld::CheckGameOver` (`EEWorld.cpp:2103`) knows three rules: wonder countdown (the only
  `EEVictoryCondition` subclass, `EEVCWonder`, always created by the team), conquest by "vital" units
  (`EEPlayer.cpp:690-706`), and no humans left. Scenario effects can also call Victory or Defeat
  (`EESMEffect.cpp:1004-1013`).
- `EEVictoryCondition` (`include/ee/EEVictoryCondition.h`) is a ready extension point.

**J. Difficulty.**
- 3 levels (`include/ee/EEComputerPlayer.h:446-448`).
- The values per level sit in fixed record fields, mapped in `EEComputerPlayer.cpp:1120-1311`.
- Tactical code has more difficulty checks: `EECPASGAirTransport.cpp:516,599,635,652,684,723,773,790`.

**K. Game options.**
- Map sizes 42/58/75/105/160/220 (`EEGameOptions.cpp:304-333`).
- Limits are checked in `EEGameOptions::operator=` (:233): speed < 4, size < 6, difficulty < 3, load type < 6,
  epoch < 15.
- Pop cap clamped to world constant 26; wonders ≤ world constant 77.

**M. Resources.**
- `EEResourceType`: none, any, food, wood, stone, gold, iron (`include/ee/EEResourceType.h`).
- Gold and iron are interchangeable (`EEPlayer.cpp:1593,1617,3961-3968`).
- The resource → gather-slot map is in code (`src/EE/User Interface Thread/EEFOFindNewTargetNearOldTarget.cpp:5812-5835`).
- Population is bonus + share + the world limit (`EEPlayer.cpp:1571`).

---

## 2. Load-time data flow, ID spaces and caps

### 2.1 Load order (EEInitializeDatabase, `Empire Earth.cpp:2272-2486`)

1. startingresources, unitbehavior, sounds, graphics, gfxeffects, music
2. **world** (constants)
3. family (damage tables)
4. buttons, **techtree**, events + effects, animals, colortable, aiunittargeting
5. terrain, terraincolor, terraintype, cliffterrain
6. civpowers, civilization, randommap, weapontohit, calamity, unitset, areaeffect, upgrade, cpbehavior
7. gamevariant, premadecivs
8. `UIInitialize` (the UI tables)
9. user folders, ambient sounds, pools

Then `EEInitializeGameObjects` (:2496) builds random maps, options and **unit classes from dbobjects**
(`EEUnitClass::CreateUnitClasses`, `EEUnitClass.cpp:2898-3100`).

Every loader is `FindFile(key)` → `FSFile` → a raw `Read(record, sizeof)` loop. That is the seam for a text loader:
it can hand each loader an in-memory file (`FSMemoryFile` already exists, `FSPath.cpp:149`).

### 2.2 ID spaces and cross-references (all plain integers)

- **Unit type = dbobjects record index = `gUnitClassTable` slot.** The linking pass pairs slot *i* with record *i*
  (`EEUnitClass.cpp:3062-3091`: projectile, scaffolding, death, death-spawn, packed and remains classes).
  - Referenced from: techtree (`mUnitClass`, `mDefaultUnitClass`), event effects (`mUnitClass`, `mNewUnitClass`),
    unitsets, civpowers (`mUnitType`), calamities, upgrades, animals, .rmv names (by *name*: `Res-Walrus`),
    scenario objects, save games and the AI's literal ids.
- **Tech id**: epochs are 0..14 and techs come after, in one space (`EETechTreeTechID.h`). Referenced from events
  (`mTechnology`), prerequisites, civ powers (`EECivPowersManager.cpp:169+`: by name constant), scenario effects
  (ModifyTechTree), saves (`ApplySavedResearch`).
- **Event id** (0..1499) → effect indices (0..1999), from the techtree's `mResearchedEvent`/`mObsoleteEvent`.
- **Others**: string id → `Language.rc`; button id → dbbuttons; media/sound/graphic ids → dbgraphics/dbsounds;
  unit set id; area effect id; terrain type (bit index < 17, `EEEvent.cpp:~600`); family/attack kind
  (dbfamily row/column); civ upgrade id `(category<<6)|level` in `.civ` files.
- **Names**: `.rmv` and `.tai` refer to some things by name (`.rmv` unit names; `.tai` script names stored in
  dbobjects). Records carry 100-byte names (dbobjects `mName`, calamity, civ power, events). These names are the
  natural seed for stable string keys.

### 2.3 Caps that block adding content

| Cap | Value | Headroom in AoC data | Where |
|---|---|---|---|
| Unit classes | 871 (`0x367`) | **23 free** (848 records, max type 847) | `include/ee/EEUnit.h:1029`, 57 `0x367` literals (EEPlayer.cpp 25, `mUnitClassUpgrades` `EEPlayer.h:1033`, `mHackUnitClass` `:1068`, EEComputerPlayer.cpp:391,949, EETechTree.cpp:162) |
| Event effects | 2000 | **file is exactly 2000 records**; highest effect index used 1565 | `EEEvent.h:39,47`, `EEEvent.cpp:110` (raw `Read(gEventEffects, sizeof)`) |
| Events | 1500 | 289 have effects | `EEEvent.h:58-62`, `EEEvent.cpp:111` (fixed loop of 1500) |
| Epochs | 15 | none (all used) | `EETechTree.cpp:133,148,210,269,294,444,447,588,605`; `EEPlayer.h:542 mEpochs[15]`; AI `EECPBuildManager.h:101`, `EECPUnitManager.h:88`, `EECPResearchManager.h:49`; `EEMusicObject.h:35-41`; the editor's `EESEPlayerControls.h:60-62`; 92 `[15]`/`<15`-style sites in src/EE + include/ee. **The epoch id = tech id** coupling means adding an epoch renumbers techs. |
| Civ-builder categories / levels | 35 / 60 (6-bit index) | | `EECivilizationManager.h:42,79`, `.cpp:356,368,379`; `.civ` encoding |
| Unit categories (→ C++ class) | 35 (`0x23`) | | `EEUnitClass.cpp:2914` |
| Families × attack kinds | 60 × 68 | | `EETerrainTypeTable.cpp:144,161,168` (loop unrolled 68×) |
| Weapon/armour kinds | 6 | | `gWeaponToHitFactors[6][6]`, `mArmor[6]` (`EEUnit.h:152,1449`); civ-builder case 4 |
| Terrain types | 17, in a 32-bit mask | | `EEEvent.cpp` kind 2 attribute 42 |
| Resources | 7 slots, 5 real | | `EEPlayer.h:316,1088,1115-1116`; AI `EECPResourceManager.h:64-77`, `EECPIntelligence.h:118`; `EEUnit.h:156 mGatherTimes[7]` |
| AI behaviour records | 129 | | `EECPBehavior.h` |
| Players | 16 + nature (17) | lobby shows 8 | `include/ee/RTSWorld.h:62`; `EEGameOptions.h:77`; `EEPlayer.h:1066,1076,1097` (`[17]`); network and save formats |
| Scenario variables | 10 per player, 3 per unit | | `EEPlayer.h:1074` |
| Difficulty | 3 | | record layouts; `EEGameOptions` < 3 |
| Map sizes / speeds | 6 / 4 | | `EEGameOptions.cpp:233,304-333` |

Triggers, conditions, effects, scenario objects, unit sets, civilisations and AI building counts are `std::vector`/`std::map`
and have no cap.

### 2.4 Save-game and multiplayer implications

- **Multiplayer is lockstep.**
  - Peers exchange commands and per-tick checksums (`EEClient.cpp:355`, `EEServer.cpp:625`).
  - Content must be identical on every peer. A content difference is a desync, not just a balance difference.
- **The install version check is weak and file-based.**
  - `EEInitializeNetwork` (`Empire Earth.cpp:1039-1084`) sums the exe, `Data\Db\*.dat` and
    `Data\Unit AI Scripts\*.tai` (loose files only), `Data\Models\*.cem`, plus each archive's `Timestamp` *tag*. It
    does not hash archive contents.
  - The low 13 bits are appended to the version string. The server rejects a mismatch with
    `kConnectionBadChecksum` (`EEServer.cpp:128`).
  - A mod system must replace this with a hash of the canonical merged content and the mod list. That is stronger
    and also lets the lobby show which mods differ.
- **Saves store integer ids.**
  - Unit classes, tech ids (researched/obsoleted lists), event state, scenario conditions and effects (fat structs
    with type ids) are all stored as integers.
  - `EEPChecksum` stores a world-state checksum chunk that also covers the tech tree (`EETechTree::GetChecksum`,
    `EETechTree.cpp:285`).
  - Once ids are assigned at run time from string keys, every save must carry a **key table** (runtime id → key) and
    the mod list, and remap on load. Original saves and scenarios map via the legacy ids (see 3.3).
- **Scenario and campaign files** reference unit and tech ids and embed triggers. The Scenario Editor writes them.
  Campaign `.ssa` archives join `gDataPath`, so a campaign can already ship db overrides: keep that as a "scenario
  mod".

---

## 3. Target architecture

### 3.1 Content layer and folder layout

```
<game>/content/                 # converted vanilla, read-only, shipped
  base/                         # base-game data the AoC install falls back to (gDataPath order)
  aoc/
    mod.yaml
    defines/world.yaml          # named dbworld constants + new named constants (combat, AI timers ...)
    units/*.yaml                # one entry per unit class (dbobjects), grouped by family or epoch
    techs/<epoch>.yaml          # techtree nodes; epochs/epochs.yaml
    effects/*.yaml              # events+effects, inlined per tech where single-use
    damage/families.yaml  damage/weapons.yaml
    civs/categories.yaml  civs/powers.yaml  civs/premade/*.yaml
    calamities/*.yaml  areaeffects/*.yaml  unitsets/*.yaml  upgrades/*.yaml
    ai/behavior.yaml  ai/targeting.yaml  ai/epoch_roles.yaml  ai/buildings.yaml  ai/difficulty.yaml
    gameoptions/*.yaml  victory/*.yaml  terrain/*.yaml  randommap/*.yaml
    media/{graphics,sounds,music,gfxeffects,buttons}.yaml
    ui/{forms,controls,hotkeys,...}.yaml
    unit_ai/*.tai               # unchanged text, moved into the VFS
    random_maps/*.rmv           # unchanged text, moved into the VFS
    localisation/<lang>/*.yaml  # key: text  (+ legacy numeric id map)
<user>/mods/<mod_id>/           # same layout; any subset
  mod.yaml                      # id, name, version, game_version, depends, load_after, conflicts, multiplayer: true
```

- **VFS.** Generalise `FSPath` into an ordered list of mount points: vanilla first, then mods in resolved order. All
  file access goes through it, including `.rmv` and the checksum code, which today bypass `gDataPath`
  (`EERandomMap.cpp:863,934`; `Empire Earth.cpp:1065-1071`). Media files (models, textures, sounds) override by path,
  last mount wins.
- **Definitions** do not override by file. Every YAML file contributes entries to typed *registries* (unit, tech,
  epoch, effect, ...), and the merge works per entry (3.2).

### 3.2 Entries, keys and merge semantics

- **Key shape.** Each entry is `registry/key`, for example `unit:citizen_male`, `tech:bronze_spearman`,
  `epoch:copper`. Keys are lowercase ASCII, generated by the converter from the record names (dbobjects `mName`,
  tech names, ...) with dedup suffixes. The legacy integer is stored as `legacy_id:`.
- **References are keys.** `projectile: unit:arrow_fire`, `prerequisites: [tech:x]`. The loader resolves them after
  every mod is merged. A dangling reference is a load error with file and line.
- **Merge per entry, in load order.**
  - A plain mapping in a later layer **deep-merges** into the existing entry: scalars replace, maps merge
    recursively, lists replace.
  - Explicit operators handle the rest. They are reserved keys, so the YAML stays valid and the semantics are
    language-neutral:
    - `!replace`, written `{$replace: {...}}` (whole entry);
    - `$delete: true`;
    - list `$append` / `$prepend` / `$remove` / `$insert_after: <key>`;
    - for numeric patches, `{ $add: 10 }` and `{ $mul: 1.1 }`.
  - Entries can be created in any mod. Overriding an entry that does not exist is a warning, or an error with
    `$must_exist`.
- **Load order.** A topological sort over `depends` and `load_after`, with ties broken by user order. The resolved
  order is part of the content hash. The tool `ee-content explain unit:citizen_male.hit_points` prints the
  provenance chain (which mod set what). Conflicts (two mods setting the same field without a dependency between
  them) are reported, the last one wins.
- **Runtime integer ids.**
  - After the merge, each registry assigns dense ids. Every entry with a `legacy_id` keeps it, so vanilla content
    keeps today's numbers and the existing code, saves and scenarios work unchanged.
  - New entries take ids above the highest legacy id, ordered by key. The assignment is deterministic, so every MP
    peer agrees.
  - Code constants (`kUnitTypeTownCenter` ...) become lookups by key at load time, or stay as legacy-id constants
    during the transition.
- **Validation.**
  - The schema is the single source of truth: one **JSON Schema (2020-12) per registry**, kept in
    `content/schema/`. JSON Schema is language-neutral: C++ (valijson / json-schema-validator), Rust (`jsonschema`
    crate), Python (converter, CI) and editor support (YAML language server gives completion for modders).
  - Generate the C++ record bindings and, later, the Rust `serde` structs from the schema, or one small IDL that
    emits both. Never hand-maintain two copies.
  - Semantic checks the schema cannot express run after the merge: reference resolution, range checks, epoch order
    and acyclic prerequisites.
- **Localisation.**
  - Files `localisation/<lang>/*.yaml` hold `key: "text"`. The converter emits keys like `unit.citizen_male.name`
    plus a `legacy_string_ids` map built from `Language.rc` (5008 lines, the string tables), so `GetString(9065)`
    keeps working.
  - A mod adds or overrides keys with the same per-key merge. The fallback chain is current language → English →
    key itself.
  - Text uses `{placeholder}` arguments instead of the current printf formats (convert in a later step).

### 3.3 Format choice

| Option | For | Against |
|---|---|---|
| **YAML 1.2 (restricted profile)** | Requested; readable; comments; multi-document; universal tooling; schema-aware editors; Python/C++/Rust parsers | Implicit typing traps (Norway `no`, octal, sexagesimal in 1.1), anchors/merge keys are not portable, indentation errors, mature Rust crates are in flux (`serde_yaml` archived; `serde_yml`/`saphyr`) |
| TOML | Strict, simple | Deep nested arrays of tables (effects, AI rules, UI trees) are awkward |
| JSON5 / JSONC | Strict, trivial parsers, comments | Noisier for humans; less familiar to modders |
| Paradox-style script (`key = { ... }`, `+=`) | Modders know it; merge-friendly; conditions read naturally | Custom grammar and parser in every language, no schema tooling |
| KDL / RON | Nice | RON is Rust-only; small ecosystems |

**Recommendation: YAML 1.2 with a strict profile**, enforced by our loader:
- Core schema only: strings must be quoted when ambiguous; booleans are `true`/`false` only.
- No anchors, aliases or merge keys. Layering is done by the mod merge, not by YAML.
- No tags except the reserved `$` operators.
- UTF-8, one entry map per file.

**The canonical form is JSON-compatible data**, so the validated, merged database can be written as JSON or a binary
cache and read by C++ and Rust alike. Modders get YAML, the engines get a stable JSON-shaped data model.

**Floats must round-trip exactly.** Lockstep determinism and the round-trip gate both depend on it. The converter
writes the shortest decimal that parses back to the same `float32`. The loader parses with a correctly rounded
parser (`std::from_chars`, Rust `f32::from_str`), never `atof` with locale or x87 double rounding. A test asserts
the bit identity of every float field.

The text formats that already exist (`.tai`, `.rmv`) keep their syntax, moved into the VFS. A YAML form of `.tai` is
optional and late.

### 3.4 Conditions, effects and triggers as data

Today there are three separate mini-languages: tech **events/effects** (`EEEvent.cpp`, 22 kinds × ~45 attributes),
**scenario triggers** (`EEScenarioIDs.h`: conditions, effects, selectors) and **civ-power / calamity switches** in
C++. They become one declarative, **deterministic** language, interpreted by the engine:

```yaml
# civs/powers.yaml
power:flaming_arrows:
  name: loc:civpower.flaming_arrows
  on_granted:                       # effect list
    - for_each_tech: [tech:simple_bowman, tech:composite_bow]
      if: { tech_researched: $this }
      do: [{ research: { tech: "$this.flaming_variant" } }]
# scenario trigger (converted from a scenario file)
trigger:defend_the_hill:
  when: { every: 500ms }            # or event: unit_killed / tech_researched / epoch_advanced / chat
  if:
    all:
      - { count: { units: { owner: player:2, set: unitset:military, in_area: area:hill } }, ">=": 10 }
      - not: { player_value: { player: player:1, value: population }, "<": 20 }
  then:
    - { delay: 5s, do: [{ message: loc:scn.hill_attack }] }
    - { random_one_of: [[{ spawn: {...} }], [{ calamity: calamity:plague }]] }
  repeat: false
```

Design rules:
- **Closed vocabulary, typed parameters.** Every condition and effect is a registered primitive with a JSON-schema
  parameter block. The initial primitives are exactly the existing ones: the 8 condition types, the 50-odd
  effect/action enums, the 22 event-effect kinds with their attributes, and the entity selector (`EESMObject`) and
  area (`EESMArea`). The converter can then translate every existing scenario and tech event 1:1, and the engine
  dispatch starts as the existing switches.
- **Composition**: `all`/`any`/`not` nesting (a strict superset of today's flat chain: the flat chain converts to
  `any[all[...]]`), `for_each`, `if/else`, `random_one_of` (exists today), `delay` (exists), scoped variables
  (per player, per unit and global; generalising `mScenarioVariables[10]`), and events in addition to polling.
- **A small expression sublanguage for numbers**:
  - Constants, references to `define:`, unit/player stats, `+ - * / min max clamp floor ceil`, and comparisons.
  - Exposed for formulas such as damage, morale and AI weights:
    `damage: "max(1, hit * attack * cloak_mult - armor[weapon])"`.
  - Parsed once into bytecode or an AST; no general loops; evaluated with a fixed float type and order (determinism).
  - The vanilla formula ships as data. The C++ fast path stays and is used when the formula is the default one
    (byte-compare the AST).
- **Why not embed Lua or Rhai now?**
  - Lockstep MP needs bit-identical evaluation on every peer, bounded cost per tick and safe sandboxing.
  - The language must also survive the move to Rust. Rhai is Rust-only, so it cannot run in today's C++ engine.
  - Lua (and Luau) is portable and has Rust bindings (`mlua`). But its doubles and table iteration order need
    discipline, and arbitrary scripts make desync hunting and save/load of script state hard.
  - A declarative tree language is portable by construction: the same YAML runs in the C++ and Rust interpreters,
    and it can be validated, diffed, converted and shown in editors.
- **Escape hatch, later.** A sandboxed Lua 5.4 / Luau layer for *custom primitives* (a primitive implemented as a
  script), with integer/fixed-point rules, a per-tick instruction budget and deterministic iteration. Keep it
  behind `multiplayer_safe: false` until the determinism tests cover it.
- **`.tai`.** The unit-AI state machines stay a separate, already data-driven layer. Their trigger and action
  vocabulary should be able to call shared conditions (`condition: {...}`) and effects later, so mods can add new
  unit behaviours without C++.

### 3.5 Compatibility path

1. **Converter, binary ⇄ text, lossless.** One tool (Python first: it is what the repo uses for tooling; Rust later)
   with a table description per db file: offsets, types, enums, references. It takes the field inventory from
   `db_formats.md`. Fields with unknown meaning are kept as `raw_0x84: 0`-style keys and padding as base64 blobs.
   Nothing is dropped.
   - Gate: `bin → yaml → bin` is **byte-identical** for all 41 AoC tables, the base-game tables, every `.civ` and
     `Language.rc` string table.
2. **Text load by compilation (engine changes minimal).** At startup the content system resolves the mods, merges,
   validates, then *compiles each registry back into the exact binary record stream* the existing loader expects.
   It hands that over as an `FSMemoryFile` through `gDataPath` (the `FindFile` seam). The loaders stay unchanged.
   - Gate: the compiled stream equals the shipped `.dat` bytes. The deterministic AI game is identical.
3. **Cache.** The merged, compiled database is written to `cache/<content-hash>.bin`, so startup does not re-parse
   about 20 MB of YAML on every launch.
4. **Then refactor the loaders** one table at a time to read the canonical data model directly, and lift each cap
   (vectors sized from content). From here the binary record layout is no longer the interface, so new fields,
   tags and expressions are possible.
5. **The legacy `.dat` path stays as an import** ("load a classic loose-file mod"): `Data\db\*.dat` found in a mod is
   converted on the fly. This keeps the existing EE mod scene usable.

The Rust rewrite reuses steps 1, 3 and the schemas unchanged. Only the engine side of step 4 is rewritten.

---

## 4. Phased plan

### Phase 0: gates and instrumentation (before any content change)

- **G0 — determinism baseline.** The fork's harness already plays seeded AI games
  (`tools/harness/game_session.py --exercise ai-game --seed N --pt-rate N --time-factor 32 --no-render`).
  - Port the per-PT differential sampler from the reconstruction. Its status is "not yet" in
    `tools/harness/README.md` (`differential/`, `compare_sessions.py`).
  - The gate is "same build, same seed, two runs: identical per-PT sim/AI hashes". Without it no later gate means
    anything.
- **G1 — content dump.** A test-profile hook (or a `--dump-content` command-line switch in a test build) writes
  every loaded table *after* the loaders have run as canonical JSON: unit class records, tech nodes, events/effects,
  world table, damage tables, civ data, behaviour table, UI tables, string table.
  - It is a cheap, exact gate that does not need a game: dump(original `.dat`) == dump(text-compiled).
- **G2 — game equivalence set.** Run 3-5 fixed seeds (2- and 4-AI, Small/Hard, different start epochs) to the time
  cap, plus the settings tour, plus a save/load round trip once `save_load.py` is ported.
  - Run it after every phase step that changes code paths, not only data.

### Phase 1: converter and schemas (no engine change)

1. Table descriptions for the 41 db files, from `db_formats.md` plus the record headers already in `include/ee/`.
   These headers carry `static_assert` sizes, for example `EETechTreeRecord.h`, `EEEvent.h`, `EECalamityClass.h`
   and `EECPBehavior.h`.
2. `tools/content/convert.py bin2yaml|yaml2bin`. The key generator uses record names. Enum maps:
   - unit categories, AI kinds, resource types;
   - event-effect kinds and attributes, from the `EEEvent.cpp` switches;
   - scenario enums, from `EEScenarioIDs.h`.
3. **Name the 95 `dbworld` entries and the 129 `dbcpbehavior` records** in the YAML, using their reader sites. This
   is also the cheapest modder-visible win.
4. `Language.rc` → `localisation/english/*.yaml` with legacy ids. `.civ` → YAML.
5. JSON Schemas generated from the table descriptions.
6. **Gate**: byte-identical round trip of everything (AoC and base). Converted output is checked in (or generated in
   CI) as `content/aoc`.

### Phase 2: engine reads text (behaviour identical)

1. Mount model: extend `FSPath` (`src/Low-Level Engine/File System/FSPath.cpp`) with ordered mod mounts. Route
   `.rmv` enumeration (`EERandomMap.cpp:863,934`) and the checksum through it.
2. Content system (new module, C++ now, written to be ported):
   - YAML parse (strict profile; `rapidyaml` or `yaml-cpp`) → merge → validate → resolve keys → compile to binary
     streams → serve them from `FindFile`;
   - `content/` replaces `data.ssa` for db files only (media stays in archives).
3. **Gates**: G1 dump identical; G2 games identical; startup-time budget (cache).

### Phase 3: mod layer

1. `mod.yaml` manifest, load-order resolution, merge operators, provenance and conflict report, and the
   `ee-content validate|explain|diff` CLI.
2. Runtime id assignment with legacy ids preserved; the key table written into saves (new persistent chunk,
   version-gated like the existing `FSPersistent` chunks, `EERegisterPersistents`, `Empire Earth.cpp:1088`); remap on
   load.
3. Replace the `gBinaryChecksum` data part (`Empire Earth.cpp:1065-1080`) with a hash of the canonical merged
   database plus the ordered mod list. Lobby messages carry the mod list for a readable mismatch error.
4. Localisation by key with fallback; string-key lookup next to `GetString(id)`.
5. **Gates**: vanilla with zero mods is G1/G2-identical. A sample balance mod (e.g. +10 citizen HP) shows up in
   the G1 dump and nowhere else. Two peers with different mod lists are refused.

### Phase 4: lift the caps (in order of content value)

Each item: replace the fixed array with a container sized from content, fix every literal bound (grep list in
§2.3), version the affected save fields, and run G1/G2.

1. **Unit classes 871** → dynamic. 57 `0x367` sites; `EEPlayer::mUnitClassUpgrades`; drop the "slot i == record i"
   link assumption (`EEUnitClass.cpp:3062`); `mHackUnitClass` sentinel.
2. **Event effects 2000 / events 1500** → vectors. Effects become inline lists per event. A file of 2000 effect
   records leaves no free slot for a mod today.
3. **Terrain types** (32-bit mask, 17), **families/attack kinds** (60/68), **civ categories/levels** (35/60, the
   `.civ` 6-bit encoding: add a keyed v2 `.civ`).
4. **Epochs 15**. This is the hardest. First decouple epoch ids from tech ids (an epoch registry; techs reference
   epochs by key). Then make every `[15]` structure dynamic (tech tree, player, AI managers, music, editor
   controls). The AI's epoch→unit tables must be data first (Phase 5), otherwise new epochs have no AI.
5. **Weapon/armour kinds 6, resources 7**: large surface (unit stats, AI, UI, network), best done in the Rust
   rewrite unless a concrete mod needs it.
6. **Players 16**: network messages and the save format. Leave it for the Rust rewrite.

### Phase 5: externalise hard-coded rules (ranked; each is an independent PR with G2)

1. `defines/world.yaml` named constants (B) + new `defines/combat.yaml`, `defines/ai.yaml`: move the literals listed
   in §1.2 C/G into named defines (morale 0.8/0.6, food 0.25, tick lengths, AI timers, scoring weights). Mechanical;
   biggest immediate value.
2. **Damage pipeline as data** (C): the formula as an expression with the vanilla default, elevation category list,
   difficulty-table selection rule.
3. **Civ-builder attribute map** (E): `categories.yaml` entry gets `applies: [{stat: attack, mode: add}]`
   replacing `EECivilizationManager.cpp:499-605`.
4. **Civ powers** (D) → effect lists in the new condition/effect language; **calamity kinds** (L) → data parameters
   + primitive registry (keep the C++ classes as primitives).
5. **AI data** (G): `ai/epoch_roles.yaml` (epoch → anti-air/fishing/gate/tower classes), `ai/buildings.yaml` (the 19
   kinds + caps), composition templates, N-level difficulty (J) as arrays per level (also extends the 3-field
   records and `EEGameOptions` limit).
6. **Unit tags/roles** (F): add `tags:` to unit entries, replace `kUnitType*` comparisons by tag checks hotspot by
   hotspot (Farm/Airport/NavyYard/Dock/TownCenter/Capitol/Granary first). Each replacement is behaviour-neutral
   because vanilla tags reproduce the old id sets.
7. **Game options** (K), **map sizes** (Q), **victory conditions** (I: register victory types; `EEVCWonder`
   becomes one data-configured type; conquest rule's "vital unit" via tag), **epoch media** (N).
8. **Scenario triggers** (H) → the unified language; converter for existing scenario files (keeps playing old
   scenarios by import); editor writes the new form.

### Phase 6: scripting and tooling

Event hooks (unit created/killed, tech researched, epoch advanced, building completed, timer), mod-callable effects
from `.tai`, optional sandboxed Lua custom primitives (§3.4), a mod browser in the UI (dbui forms already data),
hot-reload of content in single player for modders.

### First three concrete steps

1. **Port the per-PT differential sampler** into `tools/harness/differential/` and establish G0 (same seed twice →
   identical hashes) plus G1 (a content dump hook in the test profile).
2. **Write the lossless converter** for the four smallest high-value tables (`dbworld`, `dbcpbehavior`,
   `dbweapontohit`, `dbstartingresources`) with named entries, then grow to all 41 + `.civ` + `Language.rc`; gate =
   byte-identical round trip.
3. **Add the text-load seam**: content resolve → compile → `FSMemoryFile` behind `gDataPath->FindFile` for those
   tables; gate = G1 dump identical and the G2 game set identical; then publish the first moddable file
   (`defines/world.yaml` with names).

---

## 5. Risks and open questions

- **Determinism**: text floats must round-trip to identical bits; merge and id assignment must be order-stable
  (no hash-map iteration); new formula/expression evaluation must reproduce the C++ float order (x87 `/arch:IA32`
  today, SSE in a later x64/Rust build: the formula default fast path should remain the vanilla code until the
  engine's FP model is settled).
- **Unknown fields**: many record fields are unnamed (`mU84`, `mL18`, unnamed behaviour records). Lossless raw keys
  keep them safe, but modders cannot use them meaningfully until identified; `db_formats.md` coverage drives this.
- **Hidden coupling**: unit type == record index == slot (`EEUnitClass.cpp:3062`); epoch id == tech id; `.civ`
  6-bit level packing; literal ids in AI and civ-power code; the 2000-effect file with no count header. Lifting caps
  without first moving these to keys breaks silently, so every cap lift needs the grep list and G2.
- **Saves, scenarios, replays**: id remapping must be in place before the first non-legacy id exists. Decide
  whether original-format saves remain loadable (recommended: yes for vanilla content, via legacy ids).
- **Multiplayer**: compatibility with unmodified original clients ends as soon as the checksum changes; decide
  whether "vanilla-compatible" MP (same checksum as the original, no mods) is a requirement (WON is gone; likely
  not). Mods must declare `multiplayer_safe`.
- **Upstream merges**: the fork merges `upstream/fixes-all`. Deep edits in large files (EEPlayer.cpp, AI) conflict
  with upstream behaviour fixes; prefer new modules and narrow call-site edits, schedule big refactors (epochs,
  tags) after upstream activity settles or in the Rust rewrite.
- **Scope vs Rust rewrite**: Phases 0-3 and the schemas/converter carry over unchanged; Phase 4.4-4.6 (epochs,
  resources, armour kinds, players) may be cheaper to do only in Rust. Decide per item.
- **Startup cost and size**: dbobjects (1.6 MB binary, ~336 fields × 848) and dbuicontrols (2.9 MB) become large
  YAML; need the compiled cache and probably "only non-default fields" emission for units (with a template/
  inheritance key `$base: unit:template_infantry`, a merge-time feature).
- **Format decision** (owner): YAML strict profile is recommended; Paradox-style script is the alternative if the
  target audience is primarily Paradox modders. The data model (JSON-shaped + JSON Schema) is the same either way, so
  a second surface syntax could be added later.
- **`.tai`/`.rmv` vocabulary**: new triggers/actions/map commands still need code until the scripting phase.
- **Native behaviour classes**: total conversions that need new unit *categories* (new C++ behaviour) cannot be done
  in data; the unified effect language plus `.tai` covers most needs but not all.
