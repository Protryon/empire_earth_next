# Lua units: human-level queries and commands for the computer player

An implementation spec (2026-11-09). The owner skipped the review: every open point below is decided (§10 lists
the choices). **Stage 1** is L0-L3 of §8 (the primitives, beside the old verbs). **Stage 2** (L4, the coarse verbs
into `ai/lib`) goes only as far as boom needs. After stage 1 the main target is a Lua port of Age of AI on this
API (§8, L5-L6 first: per-unit army control, the danger map and danger-aware routes, garrisons and the capitol
upgrades, transports and landings, the navy), then a classic-like Lua AI.

**The owner's decision:** (nearly) every decision of the next computer player moves into Lua. Lua gets what a human
player has: it can look at single units and give them the orders a human gives ("send unit 7 to attack unit 9").
C++ keeps the engine, plus the searches that are too expensive for a script, offered as helper functions.

Today's coarse verbs (`gather`, `claim`, `build near=`, `fish`, the army groups) become a Lua library, `ai/lib`,
built on the new primitives. Boom keeps working, and a modder can write against either layer.

**Rules that do not change:**
- fog-honest: the player sees only what it has seen;
- deterministic for lockstep;
- the AI's state is saved.

Read with:
- docs/AI_LUA_PLAN.md:
  - §2.1: the C++ tactics as built;
  - §3.1: the runtime, its budget and the memory encoding;
  - §3.2: today's view and verbs;
  - §5 "S4 as built": the groups;
- docs/AI_LUA_AUDIT.md: the policy hidden in the managers;
- docs/RUST_PLAN.md D2 (option c: Lua first, which survives the port);
- docs/AI_RESEARCH.md §8.30-8.35: what the next engine has learned so far.

## 0. Summary

```
                Lua   ai/<personality>.lua         plans: boom, rush, balanced, mods
                Lua   ai/lib/*.lua                 today's verbs re-made: gather, claim, build near=, fish, groups,
                                                   raid, retreat, scout; the economy planner
   ---------- the primitives (this document) -----------------------------------------------------------
                C++   queries   ai.units{...}, ai.unit(id), ai.seen{...}, ai.resources{...}, ai.tile(x, y) ...
                C++   commands  ai.move(ids, x, y), ai.attack(ids, target), ai.gather(ids, res), ai.build(ids, ...)
                C++   helpers   ai.find_site, ai.route, ai.reach, ai.danger, ai.drop_site, ai.can_place ...
   ---------- the engine ---------------------------------------------------------------------------------
                C++   pathfinding, movement, formations, combat, the units' own goals (the gather/return loop,
                      auto-acquire by stance), production and research queues, the order code a human's
                      messages run (EEMessage.cpp)
```

**The key choices:**

1. **Commands take the human path.** Each human order is an `EEM*` message, and its `Execute` runs against the
   sender's selection (EEMessage.cpp, e.g. `EEMAttackUnit::Execute` :956, `EEMBuildUnit::Execute` :1140).
   - The AI builds the same message, puts the units into the computer player's own selection (`mActiveGroup`,
     which nothing else of a computer player uses), and calls the message's `Execute` directly (§3.2).
   - `EEMessage.cpp` itself does not change. Extracting order functions out of the `Execute` bodies is left for
     when one is needed.
   - The AI runs on every peer inside the physics tick, so its orders are never sent over the network. They obey
     the same rules as a human's: `CanProduce`, `EECanPlaceOnTile`, `QueueResearch` → `CanAfford`/`Pay`, the
     population cap.
   - The AI path adds the checks that the UI makes for a human (§3.2), for example that the target is seen.
2. **Queries are lazy and charged.**
   - The per-step view stays, as summary numbers only.
   - Units are read on demand: a filter returns ids, and a record or column call returns fields.
   - Every C call charges instruction-equivalents to the step's 2M budget, so the hook's blind spot for C
     functions is closed (§2.1).
3. **Commands run at once and return their result at once** (§3.4). There is no `results[]` round trip for the
   primitives. A unit ordered earlier in the step is already busy when the next query asks.
4. **Ticks.**
   - **The 1 Hz strategy step stays.** Events (attacked, died, idle, built, trained, swapped) are collected in C++
     and delivered to the next step.
   - **An optional micro step,** `micro(events, memory)`, runs every 250 game ms with a 300k budget, for kiting and
     retreats.
   - **No callbacks inside the engine's damage or death handlers** (§5).
5. **Ids are the engine's.**
   - An id is `player << 24 | counter` (RTSPlayer.cpp:71). The counter is 24 bits, so an id is in practice
     never reused.
   - A swap (an upgrade, pack/unpack, the epoch tower upgrade) or a conversion makes a new id. C++ keeps a saved
     forwarding map and reports a `swapped` event (§6).
6. **Ownership is explicit.**
   - Every own unit is either the engine's (the classic runners: gatherer assignment, builders, militia, flee) or
     Lua's.
   - `ai.take` / `ai.release` move a unit between the two, and a command takes the unit implicitly. This extends
     today's `DetachUnitFromAI` / `EECPNextHoldsUnit` (§7).
   - The end state switches the runners off for the seat altogether.
7. **The enemy memory is in C++** (saved, fog-honest, shared with the danger map). Lua may keep its own on top.
8. **The danger-aware ways come from a route helper first.**
   - `ai.route` runs a coarse A* over danger costs and returns waypoints, which Lua gives as queued moves.
   - A cost hook in the engine's pathfinder (Age of AI's `shiproute.c`) is a later stage, and only if the waypoints
     prove too coarse (§4.3).
9. **The migration ports one verb at a time** (§8). Each port is gated by "boom on the lib = boom on the C++ verbs"
   in `sim_bench` numbers (32 games), and each C++ verb is deleted once its lib version passes.

## 1. Principles and layering

| Layer | Where | What | Why there |
|---|---|---|---|
| Engine | C++ (later Rust, RUST_PLAN R5) | pathfinding (EEPathManager, the continent and sector A*), movement and collisions, formations and group movement, combat resolution, the unit goals (`EEUGGather`'s gather/return loop, `EEUGAttackUnit`, `EEUGBuildRepair`, `EEUGLoadTransport`...), auto-acquire by stance, the production/research queues, conversions, swaps | per-tick work on every unit; the same code serves humans; it must be fast and is already deterministic |
| Order functions | C++, new (extracted from `EEM*::Execute`) | one function per human order, on a unit list | one rule set for humans and AIs |
| Helpers | C++, new or extracted from the managers | site search, reachability, route with danger, danger/influence maps, nearest drop site, enemy memory, capacity counts | map-wide scans and searches: 10-1000 times cheaper in C++, and a script cannot do them within the budget |
| Primitives | the Lua bindings (`EECPLuaUnits.cpp`, new) | the queries, commands and helpers as Lua functions; fog filters; charging; trace lines | the contract, shaped like C (integers, ids, arrays) so that mlua binds it unchanged |
| Library | `ai/lib/*.lua` | today's verbs and their state machines (gather split, claims, build near=, fish, groups, raid, retreat, scout), the economy planner | policy belongs in Lua, where modders can change it |
| Scripts | `ai/<name>.lua` | personalities | |

**What stays in C++ and why:**
- **Per-tick behaviour.**
  - The units' goals: the gather loop walks to the drop site, returns and picks the next tree.
  - Auto-acquire: a soldier on aggressive stance attacks what enters its sight.
  - Projectile and melee resolution.

  A script at 1 Hz (or 4 Hz) cannot replace these, and a human does not do them by hand either.
- **Searches over the map:**
  - a building site (EECanPlaceOnTile over rings, `FindSite`, `EECPNextAdjacentSite`);
  - path length and reachability (continents, path sectors);
  - the danger grid;
  - the nearest drop site;
  - the gatherers sent to a resource (`EECPNextAssigned`, §8.35).

  Each is O(map) or O(units) per call. As helpers they cost one charged call.
- **Reflexes, until Lua can do them.** The militia, the citizens' flight to towers, the naval flee and repairs hang
  off `EEComputerPlayer::DamageTaken` (EEComputerPlayer.cpp:1376). They stay as switchable C++ reflexes
  (`ai.reflex{flee=false}`) until the micro step does them better (§5, §7).
- **The enemy memory** (§2.5).

**What leaves C++** (§8 has the stages):
- the `EECPNext*` verbs and their policy;
- the next engine's use of the classic runners (`UnitManager`, `ResourceManager`, `BuildManager`,
  `ResearchManager::Update`, `RepairBuildings`, the attack commander);
- `EECPNextArmy.cpp`.

The classic engine itself stays as it is.

**Rules every primitive follows:**
- **Integers only.** Positions are in centi-tiles (`px = floor(x * 100)`) and tiles; times are game ms; hit points
  are rounded down. A float read from the engine is converted once in C++. Floats are bit-identical on every peer
  (`-ffp-contract=off`, AI_LUA_PLAN §3.2), and truncation is exact.
- **Deterministic order.** Every list is sorted by id, or by (distance, id) where a `near` is given. Never by
  pointer or hash order.
- **Fog-honest by construction** (§2.4). The reveal cheat lifts it, as it does today.
- **No raise while C++ holds a destructor** (AI_LUA_PLAN §3.1, "C++ and longjmp"). Every binding works in three
  steps:
  1. read and check its arguments into plain locals (this may raise);
  2. run the C++ part with no Lua calls;
  3. push the results (the pushes may raise only out of memory, and by then the C++ objects are gone).

## 2. The query API

### 2.1 Cost model

| Quantity | Value | Source |
|---|---|---|
| Computer players | up to 15 | 16 slots, round robin: `EEArtificialIntelligence::Update` updates one CP a PT, every `1000 / N` ms (EEArtificialIntelligence.cpp, CollectComputerPlayers) |
| Strategy steps | 1 a game second for each CP | the same |
| Physics ticks | 40 a second at normal speed (PORT_LEARNINGS:1523), 25 ms each | one CP step lands in one PT: a long step is a visible hitch for every peer (lockstep) |
| Own units, late epochs | 150-400 at the usual population limit (citizens 100, army 40-100, buildings 40-100); about 1,000 at Age of AI's high-population target | AI_LUA_PLAN §6, Age of AI docs/ai-features.md |
| Seen enemy units | 0-300 | |
| Lua speed | about 3-10 ns a VM instruction (Lua 5.4, x86-64 and arm64) | so the 2M budget is about 6-20 ms |
| Full snapshot of everything, every step | about 400 x 25 + 300 x 10 = 13k field sets and 700 tables: about 1-2 ms in C, plus about 150 KB of garbage a step | it grows with the population, whether the script uses it or not |

**The decision:** keep the summary view (today's `view` without the per-unit parts), and make the per-unit data lazy.

- A filter call returns ids: one integer per unit, from C++ lists that are already sorted.
- Records are built only for the ids asked for.
- A column call, `ai.get(ids, {"hp", "x", "y"})`, returns parallel arrays: no table per unit.

A typical boom step reads under a hundred records. A late-game army step reads a few hundred.

**Charging.** The count hook does not see C functions (AI_LUA_PLAN §3.1).
- Each binding adds a charge to a per-call counter `mCharged`, in instruction-equivalents: a base cost per call
  plus a cost per item returned or per unit ordered.
- The hook (every 1,000 instructions) and every binding compare `instructions + mCharged` with the budget. Past the
  budget the step aborts as today (`budget_exceeded`).
- Charges depend only on the arguments and on the result sizes, never on time, so they are the same on every peer.
- The starting weights are below. Stage L0 calibrates them from `EE-AIPROF` so that a charge is about as long as
  the same number of VM instructions.

| Kind | Base | Per item |
|---|---|---|
| a filter (`ai.units`, `ai.seen`, `ai.resources`) | 200 | 20 an id returned; 5 a unit tested when the filter cannot use an index |
| `ai.unit(id)` | 300 | |
| `ai.get(ids, fields)` | 100 | 15 an id x field |
| `ai.tile` | 50 | |
| a command | 300 | 100 a unit ordered |
| a helper | its own fixed cost, sized by its work, e.g. `ai.find_site` 20k, `ai.route` 5k + 10 a cell expanded | |

**The budgets:**
- the step keeps 2M as the hard cap;
- the trace prints each step's `instructions + charged`;
- the soft target is about 300k for a typical step (about 1-3 ms), so that 15 computer players cost under 5% of a
  game second.

### 2.2 Functions

Names are proposals. Every call takes integers, strings and tables of them.

| Call | Returns | Notes |
|---|---|---|
| `ai.me()` | the player id | |
| `ai.time()` | game ms | as `view.time` |
| `ai.players()` | an array: `id`, `team`, `ally`, `enemy`, `alive`, `civ`, `epoch` (only where the game's UI shows it to a human), `human` | |
| `ai.map()` | `width`, `height`, `water` (bool), the start tile, `epoch_max` | |
| `ai.units{...}` | ids, own only | filter keys: `type` (a class id or a list), `category` (`citizen`, `soldier`, `ship`, `boat`, `building`, `transport`, `siege`, `air`, `hero`, `priest`), `near = {x, y, r}` (tiles; sorted by distance, then id), `idle = true` (no goal, or an idle action), `action` (`gather`, `build`, `move`, `attack`, `garrisoned`, `carried`, `produce`...), `held` (`lua` / `engine`), `built` (buildings: finished or not), `inside = id`, `limit = n` |
| `ai.unit(id)` | a record, or nil (dead, gone, or not seen) | own and allied units: every field; others: the fields of §2.3 that are marked seen |
| `ai.get(ids, fields)` | a table of arrays, `{hp = {...}, x = {...}}`; nil where an id is invalid | the cheap way to read many units |
| `ai.seen{...}` | ids of other players' units seen now | keys: `player`, `enemy = true`, `category`, `type`, `near`, `armed = true`, `limit` |
| `ai.memory{...}` | the enemy memory's records (§2.5) | keys as `ai.seen`, plus `since` (ms) |
| `ai.resources{...}` | ids of resource units (trees, mines, bushes, animals, fish) on explored tiles | `kind` (`food`, `wood`, `stone`, `gold`, `iron`), `source` (`bush`, `hunt`, `tree`, `mine`, `fish`, `farm`), `near`, `has_room = true` (the gatherers sent to it are fewer than its places: `EECPNextHasRoom`), `limit` |
| `ai.resource(id)` | `id`, `kind`, `source`, `amount` (as last seen), `x`, `y`, `places` (the gatherers it holds), `workers` (sent: `EECPNextAssigned`), `drop` (the nearest own drop site id), `drop_distance` | |
| `ai.tile(x, y)` | `explored`, `visible`, `land`, `water`, `shore`, `continent` (land id), `sea` (water id), `height`, `forest`, `danger` (§4.4) | nil off the map |
| `ai.class(type)` | static class data: `category`, `cost` (per resource), `pop`, `hp`, `attack`, `range`, `sight`, `speed`, `armor`, `damage_kind`, `producer` (class ids), `epoch`, `capacity` (transport, garrison), `size` | from the db tables, never fogged; it may be cached per game |
| `ai.damage(attacker_type, target_type)` | the hit factor (percent, an integer) | combat.yaml, which a human can read too |
| `ai.stock()`, `ai.pop()` | the per-resource map; `{used, cap}` | also in the view |
| `ai.queue(building)` | the building's queue: `{tech, class, progress_pct}`..., and its rally point | own buildings |
| `ai.techs{...}` | what can be researched or trained now, and where | `costs[]` today, on demand |

**The view after the port** keeps only the summaries a step reads every time:
- time, epoch, stock, income, population;
- the per-category counts;
- `events` (§5).

`sites[]`, `soldiers[]`, `threats[]` and `groups[]` move into `ai/lib`, built from the queries. `sites[]` may stay a
C++ summary if its cost says so (§8, L4).

### 2.3 Fields of a unit record

| Field | Own and allied | Others, seen now | Notes |
|---|---|---|---|
| `id`, `type` (class id), `player`, `category` | yes | yes | |
| `x`, `y` (tile), `px`, `py` (centi-tiles) | yes | yes | |
| `hp`, `hp_max` | yes | yes | a human sees an enemy's health bar |
| `built` (buildings), `progress_pct` (going up) | yes | `built` only | |
| `action` (`idle`, `move`, `attack`, `gather`, `build`, `repair`, `garrisoned`, `carried`, `produce`, `heal`, `convert`, `flee`) | yes | `move`, `attack`, `idle` only (what the animation shows) | |
| `goal` (the goal kind), `target` (id), `tx`, `ty` (its place) | yes | no | an enemy's target is what Age of AI's counterfire read (counterfire.c:71-75), and a human cannot see it |
| `carrying` (resource, amount) | yes | no | |
| `inside` (the container id), `cargo` (count), `room` | yes | no | |
| `stance` | yes | no | |
| `held` (`lua` / `engine`), `group` (the engine's active group, if any) | own | no | |
| `attacked` (ms), `attacker` (id) | own | | `attacker` only while that unit is seen |
| `queue_len`, `rally_x`, `rally_y` | own buildings | | |
| `speed`, `range`, `sight` (with the upgrades) | yes | yes | from the class and the player's improvements, which a human can read off the unit |

### 2.4 Fog rules

- **Own and allied units:** always visible.
- **Other players' units:** returned only while `EEIsSeenBy(unit, me)` (EEAreaUtilities.h:23); otherwise
  `ai.unit(id)` is nil and they are left out of `ai.seen`. What the player saw before comes from `ai.memory`.
  Buildings in the fog are memory too: a human sees a fogged building as it last saw it.
- **Resources:** only on explored tiles. The amount is the live one. A human sees a resource's amount when they
  select it; open question 2 asks whether the amount should freeze at the last time the tile was seen.
- **Tiles:** `explored` and `visible` always; terrain only where explored; `danger` comes from what was seen.
- **Commands:** a command target must be seen now. The one exception is a building in the memory: a human can
  click a fogged building to attack it. A build or a move to an unexplored tile is allowed (a human can do it), but
  `ai.find_site` and `ai.route` search explored tiles only.
- **The reveal cheat** (`EEMMakeWorldVisible`) lifts all of these, as it lifts the view's fog today.
- **The gap from before:** Age of AI's honest mode still had leaks: `cv_enemy_def` read every enemy defence, and
  `expand.c` took island mines from the zone list (see §9). The bindings are the only door into the engine, so each
  check sits in one place, and a luatest probe checks it (§8, L1).

### 2.5 The enemy memory (C++)

**Why in C++:**
- it is updated from sight, on every one of the player's steps, over every seen enemy unit: O(enemy units), cheap in
  C++;
- the danger map and `ai.route` need it;
- its save format is fixed;
- it grows from today's scouting memory (AI_LUA_PLAN §5 "S4 as built": the gather spots, the towers, the army last
  seen).

**What it holds:** one record per enemy unit seen: `id`, `type`, `player`, `x`, `y`, `hp`, `seen` (ms).
- **Buildings** stay until their tile is seen without them, or they are seen destroyed.
- **Mobile units** stay for `ai.config{memory_ms = 180000}` (Age of AI's `fight_memory`: 3 minutes), or until they
  are seen elsewhere, which updates the record.
- **The cap** is 4,000 records per player; past it, the oldest mobile records go first.
- **Saved** in the next state (a new state block version).

**Lua** reads it with `ai.memory{...}` and may keep any aggregate it likes in its own memory (an army estimate, say).

## 3. The command API

### 3.1 How a human's command reaches the game today

1. The UI turns a click into a message.
2. The message goes into the lockstep queue, with the sender's selection id (`mFastSelectionID`).
3. Every peer runs `EEMessage::ExecuteMessage` (EEMessage.cpp:169). It runs `Execute` only when the selection id
   matches the sender's last selection message, otherwise it resets the selection.
4. `Execute` reads the sender's `mActiveGroup` (EEPlayer.h:1083).
5. Most orders then make `EEActiveGroup`s split by formation category (`Fill(group, 1, i)` for i 1..5), spread the
   units round the target, set an `EEUG*` goal, `Plan()`, and push the group into `mPendingGroups`.

Two more effects come with it:
- **Display and sound:** `EESendGroupCommand` (EEClient.cpp:738: the confirmation markers) and
  `PlayAcknowledgement` (EEPlayer.cpp:2745: voice, timed by `TMGetTime`, real time). Both are display only.
- **The synced RNG:** `gPhysics->GetRandom` is drawn for the acknowledgement's unit and the granary farms.

| Human order | Message (EEMessageID.h) | Execute | What it does |
|---|---|---|---|
| move | `EEMPathTo` 0x4d | :5054 | one group per formation slot; `mIsForcedMove`, `mIsQueued`, facing, `mUseGroupMovement` |
| attack a unit or building (convert: the same message) | `EEMAttackUnit` 0x4c | :956 | per category, `SpreadAroundUnit`, `EEUGAttackUnit` |
| attack ground | `EEMAttackGround` 0x9f | :821 | ranged and siege units |
| patrol | `EEMAddPatrolPoint` 0x72, `EEMPatrol` 0x73 | :559, :5179 | |
| explore | `EEMExplore` 0x79 | :2694 | `EEUGExplore` per category |
| gather | `EEMGatherResource` 0x51 | :3132 | `SpreadAroundResource`, `EEUGGather` |
| drop off | `EEMDropoffResource` 0x61 | :2134 | |
| farm | `EEMSmartFarmers` 0x8f, `EEMReplantFarms` 0x81 | :7917, :5966 | |
| build | `EEMBuildUnit` 0x56 | :1140 | `CanProduce`, `EECanPlaceOnTile`, `EEMakeBuilding`, then `OrderRepair` or `AddBuildTarget` (queued); a granary also takes its free farms |
| wall, gate, lock | `EEMBuildWall` 0x59, `EEMBuildGate` 0x86, `EEMLockUnlock` 0x8c | :1263, :1047, :4740 | |
| repair | `EEMRepairUnit` 0x57 | :5829 | |
| heal | `EEMHealUnit` 0x92 | :3434 | |
| garrison | `EEMLoadGarrison` 0x7b | :4490 | |
| load a transport | `EEMLoadTransport` 0x67 | :4609 | |
| unload (transport or building) | `EEMUnloadTransport` 0x68, `EEMImmediateCargoUnload` 0x95 | :8479, :3556 | |
| train, research (an epoch too) | `EEMResearchTechnology` 0x52 | :6289 | `QueueResearch` at the least busy selected building of the clicked one's type; `QueueResearch` checks `CanAfford` and pays (EEComplexUnit.cpp:380, :401) |
| cancel | `EEMRemoveTechnology` 0x53 | :6199 | |
| improve a class | `EEMImproveUnit` 0x77 | :8650 | |
| stance | `EEMSetUnitBehavior` 0x9e | :8764 | `EEUnitBehavior::sTable[n]` |
| formation | `EEMChangeFormation` 0x78 | :1528 | |
| stop | `EEMHaltSelectedGroup` 0x58, `EEMStopActiveGroup` 0x8a | :3378, :8273 | |
| delete | `EEMKillUnit` 0x5c | :3890 | refunds an unfinished building |
| disband (refund) | `EEMDisband` 0xa4 | :2111 | |
| rally point | `EEMSetRallyGoal` 0x66 | :7677 | gather, attack, repair, patrol, garrison or move |
| pack / unpack | `EEMPackUnpack` 0xab | :5015 | |
| powers, calamities | `EEMCastCalamity` 0x75, `EEMGoFanatic` 0xac | :1398, :3252 | |
| aircraft | `EEMSetAirportCommand` 0x80, `EEMScrambleAircraft` 0xa0, `EEMReturnToBase` 0x90, `EEMClearAirportCommands` 0x87 | :7428, :6743, :6411, :2034 | later epochs |
| tribute, market, diplomacy | `EEMPayTribute` 0x44, `EEMExchangeResources` 0xae, `EEMSetDiplomaticStance` 0x99 | :5332, :2618, :7582 | |

**How the classic AI orders today.** It never goes through these messages. Its managers build goals and give them
straight to units:
- `EEComplexUnit::SetGoal` / `QueueGoal` with `new EEUGLocation`, `EEUGAttackUnit`, `EEUGGather`, `EEUGBuildRepair`,
  `EEUGLoadTransport`...: about 50 call sites under `AI Thread/`;
- production through `QueueResearch`.

The next engine's army does the same (EECPNextArmy.cpp:188-206, `EECPNextGiveAttack` and `EECPNextGiveMove`). This
route skips:
- the per-category split;
- the spread round the target;
- the formation;
- `CanProduce` and `EECanPlaceOnTile` as a human meets them (the build manager places through its own `CanBuildAt`
  rules).

### 3.2 The AI's path: order functions shared with the messages

- **The decision (stage 1): no refactor.** `EEMessage.cpp` is unchanged, and the AI runs the messages' own
  `Execute`. Each `Execute` finds its player by `GetPlayerBySlot(GetOriginatingSlot())` (EEWorld.cpp:833) and its
  units in that player's `mActiveGroup`.
- **The AI's call** (`EECPUnitsOrder`, EECPNextUnits.cpp).
  1. Clear the computer player's own `mActiveGroup` and fill it with the checked units (`EESelectedGroup::Add`).
     Nothing else of a computer player reads its selection.
  2. Build the message with its normal constructor, `SetOriginatingSlot(player->mSlot)`, and call `Execute()`.
  3. Clear the selection again.

  The `EEActiveGroup`s that the order made live on in `mPendingGroups` / `mRunningGroups`, as for a human.
- **Display side effects** are inert for a computer player:
  - `EESendGroupCommand`'s markers are local-only effects for that player id (EEClient.cpp:738);
  - `PlayAcknowledgement` plays only for the local player (`EEPlayForLocalPlayer`);
  - `mChangeTime` and the acknowledgement time read real time, but nothing in the simulation reads them.
- **Extraction later.** If an order ever needs a result the message cannot report, its body moves into an
  `EEOrder<Name>` function that both callers use. The gate for that is the classic traces plus the tours.
- **No network.** Every peer runs the AI at the same PT, so the order is executed directly. The orders draw the
  synced RNG exactly as a human's do, so the engine's RNG stream depends on the AI's orders. That was already true
  of the classic AI's goals.
- **The AI-side checks** (what the UI enforces for a human and the message never checks):
  - each id is the player's own, alive (`hp > 0`), on the map (or `inside` for an unload), not disabled, and able
    to do the order (`mCanChangeBehavior`, a citizen for a build, a transport for a load);
  - a target is seen (§2.4);
  - a tile is on the map;
  - a class or tech is known by name;
  - a count is in range.

  A unit that fails is left out. When none passes, the command is refused with the first reason.
- **Rules:** exactly those of the order functions.
  - **Costs:** `CanProduce`, `CanAfford`, `Pay`.
  - **Population cap:** checked in the production queue.
  - **Requirements:** the tech tree.
  - **Placement:** `EECanPlaceOnTile`.
  - **Difficulty modifiers:** `EEComputerPlayer::CanProduce/CanAfford/ScaleCost` are virtual. For a next seat they
    answer as `EEPlayer` does, plus the difficulty levers the owner keeps (AI_LUA_PLAN §1: skill first, modifiers
    second).
  - **The reserve:** today's `EECPNextCanAfford` applies the strategy's reserve inside the engine. The primitives
    do not, and the lib checks it before it orders (open question 1).

### 3.3 Commands

`ids` is an id or an array of ids. `opts` is an optional table. Every command returns `code, reason, n, id`.

| Lua | Order function (from) | Arguments, checks | Result |
|---|---|---|---|
| `ai.move(ids, x, y, {queue, forced, formation, facing})` | PathTo | tile on the map; `forced = false` (the default) fights what it meets on the way, as the next army's legs do; `formation = true` is `mUseGroupMovement` (the fast wait for the slow) | n = units moved |
| `ai.attack(ids, target, {queue, convert})` | AttackUnit | target seen, or a building in the memory; not own or allied | |
| `ai.attack_ground(ids, x, y)` | AttackGround | units with a ranged attack | |
| `ai.patrol(ids, points)` | AddPatrolPoint + Patrol | 2-8 tiles | |
| `ai.explore(ids)` | Explore | | |
| `ai.gather(ids, resource, {queue})` | GatherResource | resource on an explored tile, with an amount left; citizens and boats | n = units sent. Room is not enforced: the lib checks `has_room`, as a human may crowd a bush |
| `ai.dropoff(ids, building)` | DropoffResource | own drop site for the resource carried | |
| `ai.farm(ids, granary)`, `ai.replant(granary)` | SmartFarmers, ReplantFarms | | |
| `ai.build(ids, type, x, y, {queue})` | BuildUnit | citizens; `CanProduce`; `EECanPlaceOnTile` | id = the new foundation; reasons `cannot_place`, `cannot_afford`, `not_in_epoch`, `requirements` |
| `ai.wall(ids, type, x1, y1, x2, y2)`, `ai.gate(type, x, y)`, `ai.lock(gate, on)` | BuildWall, BuildGate, LockUnlock | | |
| `ai.repair(ids, target)`, `ai.heal(ids, target)` | RepairUnit, HealUnit | own or allied target | |
| `ai.garrison(ids, building)`, `ai.load(ids, transport)` | LoadGarrison, LoadTransport | room | n = units taken in |
| `ai.unload(container, x, y)`, `ai.unload_one(container, id)` | UnloadTransport, ImmediateCargoUnload | the shore or the building's own tile | |
| `ai.train(building, type, n)` | ResearchTechnology (the unit's tech) | a built own producer of the class; n 1..5 (the queue's own limit) | n queued; `cannot_afford` (nothing queued), `pop_cap`, `busy` (queue full) |
| `ai.research(building, tech)` | ResearchTechnology | the epoch is a tech at the town centre or capitol | |
| `ai.cancel(building, tech)` | RemoveTechnology | | refunded |
| `ai.improve(type, what)` | ImproveUnit | `range`, `attack`, `hit_points`, `speed`, `armor`... | |
| `ai.stance(ids, stance)` | SetUnitBehavior | `aggressive`, `defensive`, `stand`, `passive` (the behaviour table's names) | |
| `ai.formation(ids, kind)` | ChangeFormation | | |
| `ai.stop(ids)` | HaltSelectedGroup | | |
| `ai.delete(ids)`, `ai.disband(ids)` | KillUnit, Disband | | |
| `ai.rally(building, x, y)` / `ai.rally(building, target)` | SetRallyGoal | | |
| `ai.pack(ids, on)` | PackUnpack | | |
| `ai.cast(ids, power, target or x, y)` | CastCalamity | | |
| `ai.tribute(player, amounts)`, `ai.trade(resource, buy, n)`, `ai.diplomacy(player, stance)` | PayTribute, ExchangeResources, SetDiplomaticStance | | |
| `ai.air(...)` | the airport messages | stage L6 or later | |

**The ownership calls (§7):**
- `ai.take(ids)`, `ai.release(ids)`;
- `ai.reflex{militia=, flee=, naval_flee=, repair=}`;
- `ai.runners{gather=, build=, produce=, research=}`, for the migration (§8).

**Result codes:**
- `code`: `ok` or `refused`;
- `reason`: `none`, `bad_argument`, `not_own`, `dead`, `not_seen`, `unexplored`, `cannot_place`, `cannot_afford`,
  `not_in_epoch`, `requirements`, `pop_cap`, `busy`, `full`, `cannot_do` (no unit of the list can), `limit`
  (§3.4);
- `n`: the units the order reached;
- `id`: a new foundation's id, or nil.

These are today's reasons (AI_LUA_PLAN §3.2), plus the per-unit ones.

### 3.4 Execution, batching, limits, trace

- **When it runs:** at once, inside the call (§0, choice 3). The step runs inside `EEComputerPlayer::Update`, inside
  the physics tick, where the classic AI gives its goals too.
  - The effects are visible to the rest of the step: the units have their goal, `idle` is false, and the stock is
    paid.
  - Two orders to one unit in one step: the second replaces the first, or queues with `queue = true`, as for a
    human.
- **Batching:** a command takes many ids. The order function makes one active group per formation category, as a
  human's box selection does. There is no further batching.
- **Limits per strategy step:**
  - 400 commands and 4,000 unit-orders (a unit counted once per command);
  - in the micro step (§5), 100 commands and 800 unit-orders;
  - past either, the command is refused `limit`;
  - the charges (§2.1) bound the cost before the limits do.
- **The trace:** one line per command while the trace is on, e.g.
  `next.cmd attack n=12 target=50331702 result=ok`, with the ids hashed (FNV) and the first 4 listed. The 30 s
  `next.view` line gets the counts of commands, unit-orders, queries and charges. Lines are written only when the
  trace is on, as today.
- **Today's verbs:** they keep `results[]` until they are deleted. The primitives never use it.

## 4. Helpers

Each helper is a C++ function behind one Lua call. Each has a fixed charge, and its result is integers.

| Helper | What | From | Cost |
|---|---|---|---|
| `ai.find_site(type, near = {x, y} or a resource id, {rings, safe, adjacent})` | a buildable tile for the class: EECanPlaceOnTile over rings; `adjacent` = the free tile nearest a resource's units (EECPNextAdjacentSite); `safe` = not `EECPNextBuildUnsafe` | EECPNextTactics.cpp (build), EECPBuildManager `FindSite`, `FindShoreSite`, `FindTowerSite` | 5-30k |
| `ai.can_place(type, x, y)` | bool + reason | EECanPlaceOnTile (EEAreaUtilities.h:20) | 300 |
| `ai.reach(a, b, {kind})` | same land continent / sea; a unit's continent | `gContinentManager->GetContinentID` (EEAIContinentManager.h) | 100 |
| `ai.path_len(a, b, {kind})` | path length in tiles, or nil when there is none | the sector A* (`EEAIAStarMachine` over path sectors): coarse, cached per step | 2-10k |
| `ai.route(a, b, {avoid = "threat" or "lost", kind})` | waypoints (at most 16) on an 8-tile-cell A* with the danger costs (§4.3) | new; Age of AI `ar_route` (armies.c:595) is the model | 5k + 10 a cell |
| `ai.danger(x, y)`, `ai.danger_box(x1, y1, x2, y2)` | the danger layers' values (§4.4) | new | 50 / 5 a cell |
| `ai.drop_site(resource or x, y, kind)` | the nearest own finished drop site for a resource kind, and its distance | the finders' own (EECPResourceManager) | 500 |
| `ai.assigned(resource)` | the own gatherers sent to it (walking, waiting, working) | EECPNextAssigned (§8.35) | 200 |
| `ai.strength(ids)` / `ai.strength_box(player, x1, y1, x2, y2)` | hit points x attack, as the view counts it | EECPNextView | 5 a unit |
| `ai.shore(x, y, sea)` | the nearest landable shore tile of a sea, for docks and landings | FindShoreSite, Age of AI `ld_site` (landing.c:373) | 5-20k |
| `ai.unexplored(x, y, {continent})` | the nearest unexplored grid point (8 tiles) | EECPNextArmy's scout | 2k |

### 4.1 Formations and group movement

They stay in the engine: `ai.move{formation = true}` is the human's group movement, and `ai.formation` picks the
shape. A Lua "group" is just a list of ids in the memory (§6). Leg-by-leg marching (EECPNextArmy's 14-tile legs) and
staging (Age of AI's `stage.c`) are lib code on top of `ai.move`.

### 4.2 Build-site search

The new engine's placement code is in `EECPNextBuild` and its helpers (EECPNextAdjacentSite, the tower rings, the
dock by fish). It becomes `ai.find_site`, with the policy (which near, which ring, the side of the town) left to
the lib. Age of AI's landing base search (landing.c:373, 16 angles x radii 6-30 round a building) is `ai.shore` plus
`ai.find_site`.

### 4.3 Danger-aware routes

Age of AI changes the pathfinder itself: `patch_path_request` replaces the game's path request with its own tile A*
(+40 per threat on a tile, shiproute.c:283). Citizens and unarmed units use the THREATS layer, soldiers on a move use
LOST (shiproute.c:642-655), and `hook_route_cost` adds threat to the attack planner (attack.c:110).

**The plan, in two steps:**
1. **Waypoints (stage L5).** `ai.route` runs an A* on 8-tile cells: passable from the continent and the reach data,
   the cost being the distance plus the danger. It returns at most 16 waypoints, which the lib gives as queued moves
   (`ai.move(ids, wx, wy, {queue = true})`).
   - No engine change. The engine's own pathfinder walks each leg.
   - This is what Age of AI's army does (`ar_route`). Its citizens' routing needed the tile-level hook only because
     its citizens' paths came from the game's managers.
2. **A cost overlay (later, only if (1) loses units).** A per-player cost grid that the engine's sector A* reads
   for the units the player flags (`ai.config{route_overlay = {"citizen", "boat", "transport"}}`).
   - This touches EEPathManager's cost function: every path the flagged units ask for pays a lookup.
   - It changes the engine's paths, so the classic engine must stay off it (a per-player null grid).
   - It is an owner decision (open question 4).

### 4.4 Danger and influence maps

These are per player, in C++, rebuilt on that player's step. Cells are 4 tiles: 64 x 64 cells on a 256-tile map,
one byte per layer, about 4 KB a layer.
- **threat**: seen enemy defences at range + 2; seen armed units at range + 1; the memory's soldiers seen in the
  last `memory_ms` (weighted down with age); spots where own citizens were hurt in the last 2 minutes (radius 6,
  from DamageTaken: a real event, where Age of AI polled hit points every 4 s, danger.c).
- **lost**: cells where own units traded badly in the last 20 minutes: the hit points lost over the hit points
  dealt, from the damage events. Age of AI's DANGER_LOST (cover.c `cv_zones`).
- **influence**: own minus enemy strength per cell, spread over 3 cells, for fronts and staging.
- **marks**: Lua may add its own decaying marks, `ai.mark(x, y, r, value, ms)` (at most 64 live), saved.

The layers and marks are saved, so a loaded game keeps them. Otherwise they would have to be rebuilt from memory;
open question: is rebuilding enough?

## 5. Tick model, events, budgets

| Hook | When | Budget | Gets | May |
|---|---|---|---|---|
| `init(view, memory)` | a new game's first step | 2M | the view | everything |
| `step(view, memory)` | 1 a game second (the round robin, unchanged) | 2M hard, 300k target | the view, `view.events` since the last step or micro | everything |
| `micro(events, memory)` (optional) | every 250 game ms, offset per player by its round-robin slot; never in the same PT as its `step` | 300k hard | the events since the last call | queries and commands on its own units; no builds, no training, no research (they wait for the step) |

**Why no callbacks in the handlers.**
- `DamageTaken`, `UnitLost` and the death code run in the middle of combat resolution.
- A Lua call there would issue orders while the engine walks its own unit lists (re-entrancy), and its cost would
  scale with the number of hits.
- The handlers only append an event to a C++ queue in the player's next state:
  - bounded at 512 events, coalesced: one `attacked` per unit per delivery, with the latest attacker and the total
    damage;
  - in the order they happened, which is deterministic.

**Events** (`kind`, then the fields):
- `attacked` `{unit, attacker (if seen), damage}`;
- `died` `{unit, type, killer (if seen)}`;
- `idle` `{unit}` (from UnitGoalDone, EEComputerPlayer.cpp:5853, when the last goal ends);
- `built` `{unit, type}` (UnitCompleted :562);
- `trained` `{unit, type, at}`;
- `swapped` `{old, new}` (UnitsSwapped :5016);
- `converted` `{unit, to}` (a unit lost to a priest; a unit gained is `trained` with `at = 0`);
- `enemy_seen` `{player, x, y, count}` (the first sight of a player's units in a 20 s window);
- `research_done` `{tech}`;
- `epoch` `{epoch}`;
- today's `unit_lost`, `production_lost`, `advance_lost`, `no_site`, `group_wants`, until their verbs go.

**Determinism.**
- The micro times come from game time: `next_micro` advances by 250 from the player's first step, and it runs when
  the game time reaches it, as the round robin does.
- Both hooks share one Lua state and one memory.
- An error or a budget overrun in `micro` or `step` does not undo the orders given before it: they ran at once
  (§3.4), just as a failed step's memory writes stay today. The trace says so, and the next call runs normally.

**Cost.** The worst case is 15 players x (1 x 2M + 4 x 300k) = 48M a game second. That is a bound, not a target.
The trace's per-step totals (§3.4) and `EE-AIPROF` measure the real cost. A script that does not define `micro`
pays nothing for it.

## 6. Unit ids in the script's memory

- **Form.** An id is a Lua integer, `player << 24 | n`, assigned by `RTSPlayer::AddUnit` (RTSPlayer.cpp:68-77,
  `mNextUniqueID` 24 bits, saved, in the sync checksum).
  - **Reuse:** none in practice. A player would have to make 16.7M units before an id wraps.
  - **Lookup:** `RTSWorld::GetUnit` → the player's 128-bucket hash (RTSWorld.cpp:122), O(units / 128).
- **Validity.** An id is valid while `gWorld.GetUnit(id)` returns a unit with `hp > 0`. A dying unit (`hp <= 0`,
  still listed until `RemoveKilledUnits`, RTSPlayer.cpp:81) counts as dead. Every call that takes ids checks them
  and skips the invalid ones. A record's `nil` is how a script learns that a unit is gone, and the `died` event
  tells it once.
- **Swaps and conversions** give a new id: `EEPhysics::SwapUnit` (EEPhysics.cpp:822, `ReplaceUnit(old, new)` :836)
  and an owner change (`AddUnit` without keepID).
  - **Forwarding:** C++ keeps a forwarding map `old → new` for the player's own units (saved; entries older than 10
    minutes dropped). Every call that takes an id follows it, so a group in the memory keeps working through a
    tower's epoch upgrade.
  - **The event:** `swapped` lets the lib rewrite its lists.
- **Saves.** Units keep their ids across a save (the players chunk saves the units with their ids and
  `mNextUniqueID`; L0 checks it with the save check: the ids of a group before and after a save and load must be
  equal). The script's memory holds only integers, so the memory bytes (AI_LUA_PLAN §3.1) are unchanged.
- **Groups.**
  - A group is Lua data: `mem.groups[k] = {task = "raid", units = {ids...}, x =, y =, state =, ...}`.
  - The C++ groups of EECPNextArmy.cpp (`mGroups` in the state) go away in stage L5.
  - The engine's own `EEActiveGroup`s (made by every move and attack) are an engine detail. A record's `group`
    field shows one, but Lua never names it.

## 7. Ownership between Lua and the classic machinery

**Today, for a next seat** (EECPNextUpdate, EECPNext.cpp:186-284), all of this still runs:
- `UnitManager::Update` (the production runner);
- `ResourceManager::Update` (AssignGatherers, Rebalance, the finders, the idle pickup);
- `BuildManager::Update` (the build queue, the builders, BuilderLost);
- `ResearchManager::Update`;
- `RepairBuildings` (every 31 s);
- `AttackCommander::Update`;
- `EECPNextArmyStep`;
- the reflexes on DamageTaken: militia, flee, naval flee.

**How a unit is taken today:** `EEComputerPlayer::DetachUnitFromAI` (EEComputerPlayer.cpp:389) sets
`mIsDetachedFromAI` and takes the unit out of the combat manager. `UnitGoalDone` (:5853) gives it back when its goals
run out, unless `EECPNextHoldsUnit` (EECPNextArmy.cpp:218) says a next group holds it.

**The design:**
- **The held set.** A sorted set of ids in the next state (saved) replaces the group scan in `EECPNextHoldsUnit`.
  `ai.take(ids)` adds them and calls `DetachUnitFromAI`. `ai.release(ids)` removes them and hands them to the
  combat manager (`CombatManager::UnitAdded`) or, for a citizen, to the resource manager's idle list.
- **Implicit take:** a command on a unit that is not held takes it (`ai.config{auto_take = true}`, the default).
  Without it, a lib verb ordering 3 builders would fight the build runner the next second.
- **Every runner skips held units.** Each check is one `EECPNextHoldsUnit` test:
  - `ResourceManager`: AssignGatherers, Rebalance, AddIdleGatherer (UnitGoalDone), the shed of extras;
  - `BuildManager`: AssignBuilders, BuilderLost;
  - `RepairBuildings`;
  - the militia and flee reflexes (unless `ai.reflex` keeps them for held units too);
  - the combat manager (already: detached).
- **Runners per seat:**
  - `ai.runners{gather = false}` stops a runner from taking any unit;
  - `ai.reflex{flee = false}` turns a reflex off;
  - the end state (stage L7) is every runner off and every unit held: the seat is all Lua plus the engine.
- **What always runs:** the engine's unit behaviour, whoever holds the unit.
  - the gather/return loop;
  - auto-acquire by stance;
  - a builder going on to the next foundation it was queued;
  - the production queue.

  A held citizen given `ai.gather` keeps gathering until told otherwise or until its resource runs out (then
  `idle`), exactly as a human's does.

## 8. Migration

Each stage stands alone, and the work can stop after any of them. "Bench" means
`sim_bench.py --engine boom,classic --seeds 2-33 --max-game-seconds 2700` (32 games, Small, Medium, as §8.35) with
these measures:
- Stone and Copper minutes;
- citizens at 30 and 40 minutes;
- the army at 30 and 40 minutes;
- defeats;
- script errors.

"Parity" means every mean within 1 standard error of the reference, or within 5%, whichever is wider, and no more
defeats. Trace identity is not asked for: an order function draws the RNG differently from today's direct goals.
Where it is cheap (L0), it is.

| Stage | Work | Gate | Deleted | Effort |
|---|---|---|---|---|
| **L0, plumbing** | the order path through the messages' `Execute` (§3.2); the charge counter in the hook; the id checks and the swap forwarding map; the held set; the event queue; a state block version | classic traces identical for seeds 2-5 (`ai_trace.py compare`), boom's traces identical (nothing calls the new code yet); luatest: the charge budget aborts as the instruction budget does | nothing | 4-6 days |
| **L1, queries** | §2: `ai.units`, `ai.unit`, `ai.get`, `ai.seen`, `ai.memory`, `ai.resources`, `ai.resource`, `ai.tile`, `ai.class`, `ai.players`, `ai.map`; the enemy memory; the new events | a probe script (`ai/test/probe.lua`) whose counts equal the view's (citizens, gatherers, soldiers, buildings, sites) every step for 1 hour; a fog probe: `ai.seen` never returns a unit whose tile `IsSeenBy` is false (a C++ check at each call, TEST ONLY); twice-run traces identical; charges logged; boom's bench unchanged (it does not call them yet) | nothing | 5-7 days |
| **L2, commands** | §3.3 for land units: move, attack, attack_ground, patrol, explore, gather, dropoff, build, repair, garrison, train, research, cancel, stance, stop, delete, rally, take, release, reflex | **micro test scripts** in EE-SIM (a normal start; the test seat skips the economy and is given its units by the `EE_AI_TEST_UNITS` TEST ONLY spawn at 2 minutes): (a) kiting: 8 archers against 8 clubmen, archers that step back when a melee unit is within 2 tiles kill more and lose fewer than archers left on aggressive stance, over 10 seeds; (b) a raid of 6 that attacks citizens and retreats home when soldiers come, losing under half; (c) a build of each class through `ai.build` + `ai.find_site` placed and finished. Plus twice-run identity and the save check (`EE_AI_SAVE_CHECK`, the held set and the memory the same after a restore) | nothing | 5-7 days |
| **L3, micro step** | `micro` (§5), its budget and limits | the kiting test (a) re-run with the kiting in `micro`: better than at 1 Hz; the cost in the trace under 5% of a game second with 4 seats | nothing | 2-3 days |
| **L4, the economy verbs to ai/lib** | one at a time, each `lib/verbs/<verb>.lua` with the same order table as today (`{verb = "build", ...}`), so boom does not change. Order: `train` / `research` / `advance` / `reserve` / `improve` (simple; the reserve becomes the lib's check before `ai.train`); `build` (`ai.find_site`, the unsafe test, the builders by `ai.units{category = "citizen", near = ...}`); `fish`; `claim` / `release` / `claim_range` and `gather` (the largest: the gatherer split, the capacity counts, idle pickup, hunting near home; `sites[]` built in Lua or kept as a C++ summary, whichever the bench says). The runner for the ported verb is switched off (`ai.runners`) when the lib takes over | **bench parity per verb** (lib vs C++), then the owner's next playtest on the lib build | per verb once its parity holds: `EECPNextTrain`, `Research`, `Advance`, `Reserve` + `EECPNextCanAfford`; `EECPNextBuild` and its site code (moved into `ai.find_site`); `EECPNextFish`; `EECPNextGather`, `Claim`, `Release`, `ClaimRange`, the next-only gates in EECPResourceManager / EECPBuildManager / EECPUnitManager / EECPResearchManager (AI_LUA_PLAN §2.1 "Classic-only gates") | 2-3 weeks |
| **L5, the army** | `lib/army.lua` takes EECPNextArmy's groups: forming, legs, targets (soldiers, citizens, towers, buildings), raid flight, retreat, defend, scout, the scouting memory's use; the danger layers and `ai.route` (§4.3-4.4); melee never against warships (§8.35) | bench parity for boom (defend + scout) and rush/balanced (the S4 matrix, AI_RESEARCH §8.31: rush beats a pure boom as often as before ± 1 se); the raid test (b) with `ai.route` loses fewer citizens than without | `EECPNextArmy.cpp` (1,636 lines), `EECPNextDefend` / `Attack` / `Raid` / `Scout` / `Retreat` / `Regroup` / `Disband`; the attack commander's update for next seats | 1-2 weeks |
| **L6, navy and transports** | warships (guard, hunt, escort, flee), fishing boats under threat, transports and landings in lib, on `ai.load` / `ai.unload` / `ai.shore` / `ai.reach`; the engine's own unload bugs fixed in C++ where found (Age of AI's unload retry and idle-unload-shore hooks were engine fixes) | an island map bench (2 AIs, Islands Small): the next seat lands on the other island and builds there in 10/10 seeds; boats sunk per game no worse than today | the naval flee reflex for next seats; `EECPNextSeenWarships` (in lib) | 2-3 weeks |
| **L7, the runners off** | every runner off for next seats; `EECPNextUpdate` reduces to: the bookkeeping, the event flush, the memory/danger update, `step` / `micro` | bench parity with L6; the full gates with a next seat (AI_LUA_PLAN §6) | the next engine's calls into `UnitManager` / `ResourceManager` / `BuildManager` / `ResearchManager::Update`, `RepairBuildings` (lib), `EECPNextTactics.cpp`, most of `EECPNextView.cpp` (the summary view stays), the classic-only gates that only the next engine needed | 3-5 days |

**Order after stage 1** (the owner, 2026-11-09): the Age of AI port is the main target. L5 and L6 come before L4,
and L4 is done only as far as boom needs it to keep working. Then come the Age of AI port (`ai/ageofai/`, pinned to
an upstream commit) and a classic-like Lua AI.

The C++ test opening (`EECPNextTest.cpp`, the S2 reference) is deleted at L4, together with the first verb it
calls. The Lua `test_opening.lua` is ported with boom.

## 9. Coverage

### 9.1 Age of AI's subsystems

What Age of AI's code reads and orders (a read-only review of `dll/ai/*.c`, `dll/game/*.c`, `dll/engine/shiproute.c`):
- **Reads, per unit:**
  - tile and position, owner, id, type, class;
  - hp, max hp, attack, range, reload, missile delay, damage class and the hit table, speed, sight;
  - transport capacity and room, the cargo, the container;
  - the goal, its kind, target and place;
  - the AI state (idle), held, frozen;
  - built, the production queue;
  - a resource's amount and its gatherers;
  - the last attacker;
  - the sight counts and the explored flag.
- **Orders:** move, attack-move (a non-forced move), attack, group move, load (with the passengers' parts), unload,
  build/repair, gather (through the resource manager), train, research, hold/release, and editing a goal's place.
- **It runs** every 2 s (500 ms while a landing sails), with map scans every 2-4 s.

| Subsystem | Needs | The API | Gap |
|---|---|---|---|
| armies.c: one land force, attack time, march, push, retreat as a group, raid answer | own soldiers and their goals/idle; seen + remembered enemies; strength; danger route (`ar_route`); group move; hold/release | `ai.units`, `ai.get`, `ai.seen`, `ai.memory`, `ai.strength*`, `ai.route`, `ai.move{formation}`, `ai.attack`, `ai.take` | none. Its `cv_enemy_def` read every enemy defence, seen or not: the API cannot, by design |
| stage.c: one staging point, push when winning | danger layers, the reach, seen + memory, ranges; re-pointing a march | `ai.danger_box`, `ai.reach`, `ai.class`; re-issue `ai.move` (no goal edit needed: the step owns the march) | Lua does at 1 Hz what it did every 2 s |
| cover.c: hospitals, out of lost fights, the LOST layer | hp/max, ranges, finished buildings, seen per cell, memory | `ai.get`, `ai.units{type = hospital}`, `ai.danger` lost layer, `ai.move` | none |
| counterfire.c: strike shooters that out-range own units | the shooter's target (an enemy goal) | `attacked` events with `attacker` while seen; `ai.seen{armed}`, ranges | **by design:** an enemy's target is not readable; the events tell which unit was hit and by whom, which a human sees |
| landing.c: landings, forward base, escort, bombardment | building classes, capacities, weapons; goals of own transports; aboard; reach regions; site search; load/unload; build; train | `ai.class`, `ai.unit` (inside, cargo, goal), `ai.reach`, `ai.shore`, `ai.find_site`, `ai.load`, `ai.unload`, `ai.build`, `ai.train`; `micro` for the 500 ms re-unload | the stalled-unload retry is an engine fix (L6), not a script job |
| fleet.c: warships | hp, attack, reload, range, hit table, sea ids, seen + memory | `ai.class`, `ai.damage`, `ai.tile().sea`, `ai.seen`, `ai.memory`, `ai.move`, `ai.attack` | its straight-line safety test every 2 tiles: `ai.danger_box` or a `ai.route{kind = "sea"}` |
| transports.c: stuck .tai transports; load/unload splitting | AI state, pending passengers | `ai.unit` (action, cargo), `ai.load` per transport, `ai.unload` | no .tai: a held transport only does what Lua orders |
| econ.c: budget, reserves, gatherer rebalance, research, citizens home | stock, techs, gatherers per resource, queues, site amounts, explored | the view, `ai.techs`, `ai.units{action = "gather"}`, `ai.queue`, `ai.resources`, `ai.gather`, `ai.research` | its `catch_up_ais` read other AIs' epochs: allowed only where the UI shows them |
| expand.c: islands, ferries, outposts, stranded citizens | resources, continents, intel | `ai.resources` (explored only), `ai.reach`, `ai.load`/`ai.unload`, `ai.build` | its island mines from the zone list: the API gives only explored resources (a scout first) |
| danger.c | seen defences, soldiers, remembered soldiers, hurt spots | §4.4 threat layer | none |
| shiproute.c: A* with danger for every path | the pathfinder's cost | `ai.route` waypoints (§4.3, step 1); the overlay (step 2) if needed | the tile-level routing per path request only comes with step 2 |
| reachmap.c: walkability with gates | regions, the open gates | `ai.reach`, `ai.path_len` | gates opening to the player: the continent data must count own gates (check in L1) |
| bunker.c, garrison.c, citizens.c, towns.c | garrison, settlement upgrades | `ai.garrison`, `ai.unload` | none |

### 9.2 A classic-like AI

A script that does what the original managers do (AI_RESEARCH §2):

| Classic part | The API |
|---|---|
| resource manager: centres, finders, splits, idle pickup | `ai.resources`, `ai.resource`, `ai.assigned`, `ai.drop_site`, `ai.gather`, `idle` events |
| build manager: queues, placement, builders, repairs, walls | `ai.find_site`, `ai.build`, `ai.repair`, `ai.wall` / `ai.gate` (a wall planner in lib; open: a `ai.wall_line` helper if the lib's is too slow) |
| unit manager: requests, producers | `ai.train`, `ai.queue`, `ai.techs` |
| research manager | `ai.research`, `ai.improve`, `research_done` events |
| attack and defence commanders, sub-groups, .tai | lib groups on `ai.move`, `ai.attack`, `ai.attack_ground`, `ai.stance`, `ai.formation` |
| exploration | `ai.explore` or lib scouting with `ai.unexplored` |
| diplomacy, tribute, trade | `ai.diplomacy`, `ai.tribute`, `ai.trade` |
| civilization powers, calamities, wonders | `ai.cast`, `ai.build` |
| air (airports, carriers) | `ai.air` (after L6) |

## 10. Risks, open questions, effort

**Risks:**
- **Cost in lockstep.** The slowest peer sets the pace, and a step's cost lands in one 25 ms PT.
  - Mitigations: the charges, the 300k target, the per-step totals in the trace, `EE-AIPROF` in every bench.
  - Late-game armies of 100+ units at 4 Hz are the case to watch.
- **The computer player's selection.** The orders borrow `mActiveGroup`. A future feature that reads a computer
  player's selection (an observer view, say) has to know that it is filled only during an order.
- **Double control** while runners and Lua share a seat (L2-L6): a unit ordered by Lua and taken back by a runner.
  - Mitigations: the held set, implicit take, and a TEST ONLY check that writes `next.conflict` when a runner gives
    a held unit a goal.
- **Fog leaks** through a field nobody thought of. Mitigations: one binding layer, and the L1 fog probe.
- **Boom regressions** during L4. Mitigations: parity per verb, the C++ verb kept until parity, the owner's
  playtest between verbs.
- **The Rust port.** The API is C-shaped (ids, integers, arrays), so it binds through mlua unchanged (RUST_PLAN D2
  option c). The order functions are the seam that RUST_PLAN R5b.4's command queue needs anyway.

**Decisions** (the owner skipped the review, so these are made here; any can be revisited):
1. **The reserve.** Primitives spend from the plain stock (`EEPlayer::CanAfford`). The lib checks the reserve
   before it orders, as a human keeps one in their head. Today's verbs keep `EECPNextCanAfford` until they are
   ported.
2. **What a seen enemy shows:** hp, position, type and a coarse action, never its goal or target. Resource amounts
   are live on explored tiles: a human can click any explored resource and read it.
3. **The micro step** is built in stage 1 (L3), every 250 game ms, so the kiting test can use it. A script without
   `micro` pays nothing.
4. **Danger-aware paths:** waypoints from `ai.route` first. An engine pathfinder overlay (§4.3, step 2) comes only
   if the waypoints keep losing citizens in the Age of AI port.
5. **Reflexes:** militia, flee and naval flee stay switchable C++ reflexes (`ai.reflex`), on by default. A Lua AI
   that does them itself turns them off.
6. **The Age of AI port** is pinned to an upstream commit, recorded in its folder's README line and in its
   manifest entry's description, so that it can follow upstream changes.

**Effort:** L0-L3 (the primitives, with boom unchanged) take about 3-4 weeks. L4-L7 (the port and the deletions)
take 6-9 weeks with sim rounds and two owner playtests. That is about 10-13 weeks in all, and the work can stop
after any stage.

## 11. Stage 1 as built (2026-11-09)

Code:
- `include/ee/EECPNextUnits.h`, `src/Empire Earth/AI Thread/EECPNextUnits.cpp`: the bindings, the held set, the
  events, the enemy memory, the micro step, the save block;
- the runtime (`ee/EECPLua.h`):
  - `EECPLuaConfig::mOpen` adds functions to `ai` while the state is set up;
  - `EECPLuaState::Charge` adds a C function's cost to the call's count (the budget aborts it as it aborts
    instructions, and a `pcall` cannot swallow it);
  - `CallOptional` runs another entry of the personality (`micro`) with its own budget.

  luatest: 136 checks (11 new: the extension, charges against the budget, CallOptional).
- the hooks, each a test of `mUnitsState` (NULL for the classic engine and for any seat whose Lua state is not open,
  so the classic engine is untouched):
  - `EEComputerPlayer` `DamageTaken`, `UnitPlaced`, `UnitCompleted`, `UnitTaken`, `UnitGoalDone`, `UnitsSwapped` →
    events;
  - `EEArtificialIntelligence::Update` → `EECPUnitsMicroTick` every physics tick.
- the takeover:
  - `EECPResourceManager::AddGatherer` (every list of the gatherer runner);
  - `EECPUnitManager::GetIdleCitizens` (the builders);
  - `EECPBuildManager::AssignBuilders` (a script's foundation gets no runner builder);
  - `EEComputerPlayer::UnitGoalDone` (no runner takes a held unit back; the transport idling skips it);
  - `EECPNextFreeSoldiers` and the view's free soldiers (the coarse groups never take a held unit);
  - the danger-zone reflex skips held citizens;
  - `ai.reflex` switches the naval flee and the danger zones (with the militia) off.
- the save, version 0.254 (`EEPPlayerHelper`):
  - after the next engine's block comes a `u32` length and the `EENU` block: the held set, the pending events, the
    forwarding map, the enemy memory, the reflex switches and the micro timer;
  - older saves load with an empty one.

**What changed from the spec while building:**
- **The order path** (§3.2). The computer player's own `mActiveGroup` holds the units while the message's `Execute`
  runs. The player answers to slot `0xfd` meanwhile, because computer players have slot `0xff` ("none", shared by
  all of them), which `GetPlayerBySlot` cannot tell apart. `EEMessage.cpp` is unchanged.
- **Events** come from `ai.events()`, which hands over what was queued since the last call (step or micro), not from
  `view.events`. The coarse view is untouched.
- **The enemy memory** is updated once a game second from `EECPUnitsMicroTick`, not from the strategy step.
- **`ai.stance(ids, n)`** takes the behaviour table's row: 0 scout, 1 stand_ground, 2 guard, 3 harass, 4 cautious,
  5 aggressive (`dbunitbehavior.csv`).
- **TEST ONLY, with `EE_AI_TEST_SPAWN=1`:** `ai.test_spawn(type, x, y, player)` (the cheat's `EEMHackANewUnit`) and
  `ai.test_town(player)` (a town centre or capitol, fog ignored).

**Built** (§2-§6): `me`, `time`, `players`, `map`, `units`, `unit`, `get`, `seen`, `memory`, `resources`, `tile`,
`class`, `events`, `can_place`, `reach`, `strength`, `move`, `attack`, `attack_ground`, `gather`, `build` (returns the
foundation's id, which the script holds), `repair`, `garrison`, `load`, `unload`, `train`, `research`, `stance`,
`stop`, `explore`, `delete`, `take`, `release`, `reflex`; `micro(info, memory)` every 250 game ms with 300k.

**Added after stage 1 (2026-11-09, the Age of AI port's first needs):**
- **The threat layer** (§4.4), per player, in 4-tile cells, rebuilt every game second after the enemy memory. Each
  remembered enemy unit with an attack adds that attack (its owner's class with improvements) over its range + 1
  tiles, a building over range + 2. A mobile unit's share falls linearly to 0 at `memory_ms`. Each own citizen or
  boat hurt by a player in the last 2 minutes adds 5 within 6 tiles. `ai.mark(x, y, r, value, ms)` adds the script's
  own (at most 64). The hurt spots and the marks are in the save (`EENU` version 2). The lost layer and the influence
  layer are not built.
- **The queries:** `ai.danger(x, y)` and `ai.danger_box(x1, y1, x2, y2)` -> the highest and the sum.
- **`ai.route(x1, y1, x2, y2, {avoid = 4, sea = false})`** -> at most 16 waypoints `{x, y}` (the last is the goal)
  and the cost; nil when the goal is not on the start's land mass (or water body).
  - It is an A* over the 4-tile cells: 10 a straight step, 14 a diagonal, plus `avoid` times the threat of the cell
    entered.
  - A cell is open when one of five of its tiles is on the area.
  - A start or goal tile on no area (a tree, a building) takes the nearest area within 3 tiles; `ai.reach` does the
    same.
  - `lib.units.move_route(ids, x, y, opts)` gives the waypoints as queued moves.
- **`ai.find_site(type, x, y, {min = 0, max = 12, safe = false})`** -> the nearest explored tile, ring by ring, where
  `EECanPlaceOnTile` takes the class. With `safe`, only a tile where the threat is 0.
- **Commands:** `heal`, `rally` (a tile or a unit), `cancel`, `improve(unit, n)`, `formation`, `pack`, `dropoff`,
  `farm` and `cast`, each through its human message.
- **`ai/lib/units.lua`:** `STANCE`, `d2`, `alive`, `centre`, `nearest`, `move_route`.
- **The helpers test** (`tests/ai_units/helpers.lua`, seeds 2-5):
  - a mark of 50 twelve tiles out, between the town and a point 24 tiles out: the straight route (`avoid = 0`, cost
    60) crosses it, and the `avoid = 8` route goes round it (cost 68-76). In 3 seeds the highest danger on the
    straight legs between its waypoints is 0. In the fourth, one leg cuts a corner of the marked area: the A* avoids
    whole cells, and the legs between the corners are straight lines;
  - `ai.find_site` and `ai.build` with 3 held citizens put up a barracks, and its `built` event arrived 32-48 s
    later in all 4 seeds;
  - two runs of each seed give identical traces.
- **Boom after the batch:** seeds 2-9 as above. The traces are identical to main's but for one line: the AI folder
  hash (`EE-AI engine ... folder hash`) changed, because `ai/lib/units.lua` is new.

**The second batch of helpers (2026-11-09, the Age of AI port's needs):**
- `ai.path_len(x1, y1, x2, y2, {sea})` -> tiles along the cell A* without danger (the straight distance at least),
  nil when no way; `ai.drop_site(x, y[, "food"])` -> the nearest finished drop site's id, distance and centre;
  `ai.shore(x, y, {sea, land, max = 20})` -> the nearest explored land tile by water (of a sea), its sea and land
  ids; `ai.unexplored(x, y, {land, grid = 8, away = {x, y}, skip = {{x, y}...}})` -> the scout's grid point;
  `ai.config{memory_ms}`.
- **The lost layer** (§4.4): per cell and minute the hit points own units lost to enemies and dealt to them
  (`EEUnit::TakeHit` tells the attacker's seat what it dealt, `EECPUnitsOnDealt`); 20 minutes; where lost exceeds
  dealt the difference over 10 stands on the cell and half of it round it. **The influence layer:** own strength
  less the remembered enemies' (weighed down with age), spread over 3 cells. `ai.danger(x, y, "lost" | "influence")`,
  `ai.danger_box(..., layer)`, `ai.route{avoid_lost = n}`. The trades are in the save (EENU version 3).
- **Projectiles** (a hunter's spear, bombs, missiles, torpedoes) are no units of the interface (they were "soldiers").
- The helpers test prints them (`helpers.more`): seeds 2-5 give a path length of 20 (nil where the point is off
  the land), the town centre as the drop site 12 tiles off, a shore in one seed within 30 rings, a grid point, the
  influence at the town 1200-1300.

**Stage L5 as built (2026-11-09): `ai/lib/groups.lua`.** EECPNextArmy's groups as Lua data on the primitives:
`G.create{task, role, x, y, radius, target}`, `G.add` / `G.recruit` (free soldiers: `ai.units{category = "soldier",
held = "engine"}`, nearest a tile first, or out of another group), `G.disband` (released to the engine), `G.step`
(every group: the dead dropped, forming at the nearest soldier for 20 s, legs of 14 tiles given again after 4 tiles
or 10 s, per-unit targets by priority and distance through `ai.attack` batched by target, a unit keeps its aim while
it is still a candidate, melee never against ships and boats, a group with only ships to fight walks out of their
reach; attack / raid / defend / retreat / scout as the C++ tasks, the raid's flight at 1.5 x its strength, the scout
over `ai.unexplored` with its skips), the enemies' gather spots (`mem.gspots`), `G.retreat`, `G.pick_enemy`. Boom's
home guard, scouts and escorts run on it; the C++ verbs (`defend`, `scout`, ...) and `EECPNextArmy.cpp` stay for
rush and balanced until those are ported (the deletion the table above names is left).
- **Parity** (sim_bench, boom against classic, seeds 2-33, Small, Hard, 45 min; lib against the C++ groups): Stone
  Age 27.4 +- 1.0 min in both (32/32 reached), Copper 2 against 1 games, citizens at 30 / 40 min 33.5 / 43.3 against
  33.7 / 43.6, army 5.3 / 11.6 against 5.3 / 11.8, citizens at the end 49.3 against 50.6, no defeats, no script
  errors.

**The Age of AI port** (2026-11-09): `ai/ageofai/` (GPL-3.0, pinned in `UPSTREAM` to upstream 0ac6495 with a per-file
map), `ais.json` entry `ageofai` ("Age of AI", random false). The economy runs on the coarse verbs, as upstream
steered the game's managers (the budget's accounts and the worker targets by shortfall and rate, the citizens by
epoch and the mines' places, drop sites before gathering, the core towers then the fronts' and outposts', the
production and SOL buildings, farms from the Copper Age, the gathering upgrades, the epoch when paid); the army on
the primitives with lib.groups (the home guard and the strongest affordable soldier, one force at the station,
pushes at the attack time at 130% of the enemy known at the target along `ai.route` to a staging point, judged every
4 s and home as a convoy when losing or bled, the next building, the raid answer, the muster, scouts). docs/AI_RESEARCH.md
§8.37 has the bench. Not ported: the navy, landings, other islands, towns, siege, counterfire, garrison upgrades,
diplomacy (UPSTREAM lists them).

**L6, the navy in Lua (2026-11-09): `ai/lib/navy.lua`.** `N.transports()`, `N.dock_sea()`, `N.plan(target, soldiers,
transports)` (the site: the shore of the target's land mass on the dock's sea nearest a known enemy building, the
least dangerous of nine; the boarding shore nearest the soldiers and the water in front of both, `ai.shore`), and
`N.landing_step(L)`: board (the transports to the boarding water, the soldiers loaded in shares with `ai.load`,
again every 30 s; sails when all are aboard, 9 in 10 after a minute, or after 3 minutes with half at least), sail (a
sea route `ai.route{sea = true}` in formation), unload (each transport its own shore tile, `ai.unload` again every
2 s while anyone is aboard), ashore (the soldiers handed back; the caller makes a lib.groups attack of them).
`N.fleet_step(mem)`: the warships hold the dock's water, attack the enemy ships, transports and boats seen within
reach (ships first) and fall back when the seen enemy warships are 1.5 x their strength. The engine's own unload
bugs (upstream's retries) are not touched yet: the step re-orders the unload instead. The Age of AI port's
`navy.lua` uses it (a landing at the attack time when the force at home beats the beach by 1.4, transports and a
warship or two from the account).

**The port's tactics (2026-11-09): `ai/ageofai/tactics.lua`.** The siege controller (2 + a quarter of the known
enemy defences, at most 4, from a Siege Factory from the Bronze Age, a 10-minute pause after 3 lost in 10 minutes),
the settlement upgrades (every 20 s up to 10 spare citizens - idle, or wood cutters over 5000 wood - into the drop
site with the most metal mines within reach, a Settlement first, through `ai.garrison`; the record's new
`garrisoned` field counts them), and counter-fire in the **micro step** (the attacked events every 250 ms: a shooter
of 5+ tiles range hitting from beyond the victim's reach + 1; shooters within 6 tiles are one target; the own
soldiers within 24 tiles join nearest first until 1.5 x the enemy fighters seen within 10 of the shooter, at most 60,
committed 45 s: `lib.groups` leaves `G.committed` units alone). The step keeps the attacked events the micro step
took (`memory.hits`) for the raid answer.

**The EENU save check (2026-11-09):** with `EE_AI_SAVE_CHECK=1` every save of the units block decodes into a fresh
state and encodes it again; `next.savecheck units=same|DIFFERENT bytes= held= events= memory= trades=` in the trace.

**Foundations and fields (2026-11-09):** `ai.build`'s foundation lives in the player's pending list until a builder
reaches it (not in the world's unit lookup): `ai.unit(id)` now finds it there (the record says `pending = true`,
`built = false`), so a script can wait for its building instead of ordering it again (17 empty granary
foundations in one island game). `ai.replant(granary)` (the human's replant command) lays a granary's missing field
foundations: a granary placed by a human's build order comes without them, and `build b_farm` (the classic FindSite)
finishes only foundations that exist.

**Walls, gates and diplomacy (2026-11-10):** `ai.wall(ids, class, x1, y1, x2, y2)` lays a straight wall as a human's
drag (EEMBuildWall: EEPlanWall along the tiles or exactly diagonal, EEBuildWall with the citizens); `class` is a wall
class name or `"wall"` for the epoch's wall as the classic wall manager picks it (EECPCivilizationManager::GetWallClass:
nothing before the Copper Age, so `"wall"` is `bad_argument` there; a Prehistoric palisade goes by name). A wall tech is
never "available" the production way (`EEPlayer::CanProduce` says no for every computer player, which the classic wall
manager never asks), so the command checks the tech node and `CanAfford` only; `not_in_epoch` is a new reason. `ai.gate(class,
x, y)` turns the five finished own pieces round a wall tile into a gate at once (EEMBuildGate: the side is the first
`EEDoesWallGateFitOnUnit` accepts, as the classic manager picks it; `cannot_place` without such a run); `ai.lock(ids, on)`
is the gates' lock. `ai.tribute(player, {food=, ...})` moves the resources with EEMPayTribute's ledger (without its chat
line); `ai.trade(resource, "buy"|"sell", 1|5)` is EEMExchangeResources through the borrowed slot; `ai.diplomacy(player,
"ally"|"enemy")` is `EEWorld::SetDiplomaticStance` (0 the ally; the alliance forms once both stand allied). The test
`tests/ai_units/commands.lua` (seeds 2-3, 300 s) exercises them: the wall `ok n=3`, the tribute and the stances as
ordered, the trade both ways, the gate `cannot_place` while no run of five finished pieces stands.

**The fidelity pass (2026-11-10):** `ai.patrol(ids, {{x, y}, ...})` (2..8 points: EEMAddPatrolPoint for each, as the
human's patrol clicks; the message got a constructor from a point); `ai.players()` records carry `hostile` (the other
player's stance towards me, `EEPlayer::GetEnemy`) and `allies` (the other players of the world's teams it is in);
`ai.memory{category=}` filters the records by their class's category; `ai.class` has `category`, `family` (the
game's family number) and `cost` (the tech's cost as the player pays it, by resource; 0 for a class its tree has no
node for). The view has `epoch2_cost` (the epoch after the next: the copper plan holds both before farms).

**The release of a working citizen (2026-11-10):** `ai.release(ids, {gather = true})` hands a citizen or boat that
gathers (an `EEUGGather` goal on a live resource, e.g. just given by `ai.gather`) to the engine's gatherer list of
that resource, so the runners count it where it works. Without the option (as before) only an idle unit rejoins
them, on the idle list, and a released gatherer was on no list until its resource ran out. `lib.defence` sends the
citizens that fled a raid back to their spot this way (AI_RESEARCH §8.41).

**The classic port's queries (2026-11-10, `ai/classic_lua/`, its ORIGIN map):** what the classic computer player reads
from data, so that a script can make its decisions with the same numbers:
- `ai.tuning()` -> `difficulty` (0 easy .. 2 hard), `deathmatch`, `all_managers` (the resource setting 5),
  `epoch_start` / `epoch_end`, the map's `island` / `planets` / `waterless` / `space`, `population_limit` (the game's;
  the computer player's cap with popband is the view's `population_cap`); `cheats` (cost, reveal, epochs, extra,
  popband, catchup, retire: whether each is on for the seat); `personality` (aipersonalities.yaml as EECPPersonality
  picks it, with EE_AI_PERSONALITY and the TEST ONLY EE_AI_EXP switches folded in); `behavior` (every dbcpbehavior
  value EEComputerPlayer::Init copies, by a snake_case name, plus the population cap weights, the epoch advance scale
  and the mining limit). Floats are thousandths, under keys ending in `_k`.
- `ai.techs{epoch = n, id = tech}` -> the tech tree's nodes of epochs n..0 (the current epoch by default), latest
  epoch first as GetEpochNodeIDs collects them: `id`, `name`, `epoch`, `kind`, `makes` (the class), `counts_toward_epoch`,
  `researched`, `available`, `obsolete`, `obsolete_epoch`, `producers` (class names), `plain_producer` (a producer that
  is no citizen and whose own tech is no epoch: the classic research manager's IsWanted), `cost` (as the player pays
  it), `producible` (CanProduce without the cost), `affordable` (with it), `citizen_upgrade` (the researched event
  improves a citizen's gathering). Never fogged: a human reads the tech tree.
- `ai.epoch(n)` -> epoch n's node: `id`, `name`, `required_sol`, `current_sol`, `cost`, `available`, `researched`.
- `ai.gatherers()` -> `{[resource id] = n}`: the own citizens and boats whose gather goal is that resource, whoever
  holds them (`ai.resource`'s `workers` counts the classic runners' lists, which never hold a script's units).
- `ai.find_site` takes `margin = true` (the footprint grown by a tile is free too: the classic's CanBuildAt inMargin)
  and `far = true` (the rings from `max` inward).
- `ai.class` has `tech` (the class's own tech id, what `ai.train` orders).
- `ai.class` has the class's military data, which the classic unit selector reads (the db's, never fogged):
  `ai_kind` (the AI kind: the counter families), `counters` (the three kinds it counters, 0 for none), `attack_unit`,
  `defense_unit` (the attack and defence group roles), `medic`, `converts` (a priest's or calamity powers),
  `is_transport`, `mine`, `influence` (its weight in the classic's strength sums), `epoch`, `pop` (its population cost)
  and `domain` (`land`, `sea`, `air` or `space`).
- `ai.class` and the `ai.techs` records have `data_cost`: the tech's listed cost as the cost cheat scales it, without
  the epoch and upgrade factors of `cost` (what the classic's needs ledger counts: EECPResourceManager::AddCosts).
- Own and allied unit records have `target` (the goal's target unit: the resource gathered, the building built or
  repaired, the unit attacked) and `gathers` (a gatherer's resource kind).
- The view has `income_total` (gathered since the start, per resource: the classic ledger's Spend).
- `ai.water_sites(x, y, {max = 64})` -> `{{x, y}, ...}`: the dock sites, nearest (x, y) first, at most `max` (512):
  the largest water body's shore tiles next to the land mass at (x, y) and the water tiles round them, explored ones
  only (EECPBuildManager::CollectSiteTiles(2), which reads the whole map); empty when that water does not touch the
  land. The classic's FindShoreSite tries them in turn.

**Not built yet:**
- the engine pathfinder overlay (§4.3, step 2);
- what the port's modules asked for (UPSTREAM's interface gaps): a tribute-received event, an offer channel between
  AIs, `ai.resign`, `ai.class`'s damage class / hit factors, aircraft launched from their
  airport by `ai.attack`, `ai.resource(id)` (documented, not bound), `ai.tile`'s continent under the fog;
- the per-step command limits are counted per game ms, so a step and a micro in the same tick share them;
- the deletion of `EECPNextArmy.cpp` once rush and balanced run on lib.groups; the engine's unload fixes (L6's
  gate on an Islands map is in AI_RESEARCH §8.37 once run).

**Gates:**
- **Boom unchanged.** sim_bench seeds 2-9 (Small, Hard, `--engine boom,classic`, 2700 game s, `--ai-scripts ai`,
  open data): `ai_trace.py compare` against main's build gives identical traces for all 8 seeds (3738, 3589, 3674,
  3632, 3371, 3464, 3382 and 3416 lines).
- **The micro tests** (`tests/ai_units/`; `--ai-scripts tests/ai_units --env EE_AI_TEST_SPAWN=1 --env-vary
  EE_AI_NEXT_SCRIPT=kite,nokite,raid`, seeds 2-7, 230 game s). They run, and two runs of each seed give identical
  traces.
  - **Raid** (6 clubmen at the idle human's town; 10 of its spearmen appear 20 s later): the raid retreats in all 6
    seeds, 20-25 s after the start (seed 2 at 59 s), and all 6 raiders reach home every time.
  - **Kite** (8 bowmen against 8 clubmen) at 120 s after the setup:

    | 6 seeds, 48 bowmen against 48 clubmen | Bowmen lost | Bowmen hp lost | Clubmen still seen alive |
    |---|---|---|---|
    | kite (each micro step: 4 tiles away from a clubman within 3 tiles, then shoot again) | 2 | 410 | 28 |
    | stand and shoot | 3 | 750 | 0 |

    The bowmen start with about 4,750 hp in all.

    Kiting works as an order loop (micro step, a forced move away, then the attack again). In this matchup it trades
    killing for safety: bowmen out-shoot clubmen anyway, and the 1.5 s flee leaves them too little time to shoot. It
    shows the API, not a tuned tactic.
- **Builds:** `build.py --sim` and `--test-profile windowed-software` link 0/0; luatest 136 checks, 0 failed.
