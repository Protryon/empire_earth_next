# A new computer player: C++ tactics, Lua strategy

A plan for writing the computer player again from scratch, as a second engine beside today's:
- the **tactics**, the code that carries out decisions, come out of the original into a C++ API;
- a new **strategy** layer in Lua decides, every game second, what those tactics do.

The original AI stays in the tree and playable. The new one is a swap, chosen per computer seat.

Read with:
- docs/AI_RESEARCH.md: §2 is the original AI, §4 the design options (this is option B), §8 the experiments;
- docs/MODDING_PLAN.md: its rules on determinism and scripting;
- docs/RUST_PLAN.md: R5, the computer player, and mlua.

## 0. Summary

**Why a rewrite.** Four playtests and §8.26-§8.29 of AI_RESEARCH patched the original's decisions. Every patch had
to fight the same structure:
- the epoch timer and its reserve ramp decide when the AI saves;
- `CanAfford` is both "can I pay" and the saving policy;
- the gatherer split mixes what a site can hold with what the AI wants;
- the build queues per epoch come from dbcpbehavior's desired counts (7 barracks, 6 ranges, temples, hospitals);
- timers drive the upgrades and the attacks.

A Stone Age strategist bolted on top (§8.29) failed for the same reason: it overrode some of these layers and
deadlocked against the others.

The fifth playtest (log `19872e3b`, 88 minutes) shows the result:

| | Owner | AI |
|---|---|---|
| Stone Age | 20.4 min | 24.7 min |
| Copper Age | 41.0 min | never |
| Citizens at 75 min | 100 | 217 |
| Woodcutters at 75 min | 23 | 87 |

The AI's Copper reserve was asked for at 35 minutes. It reset when an attack came at 50, and was nearly full again
at 76, just before the AI was broken.

**What.** Two layers with a narrow contract between them:

```
EEComputerPlayer::Update (1 Hz per computer player, inside the physics tick, on every peer)
  engine "classic": today's managers, unchanged
  engine "next":
    view      (C++)  a fog-honest snapshot: own economy, sites, buildings, army, threats, scouting, costs
    strategy  (Lua)  step(view, memory) -> orders          personality scripts in Data/ai/*.lua
    tactics   (C++)  validate each order, then carry it out with the code taken from today's managers
```

**How long.** Weeks, in stages that each stand on their own (§5). The first playable result is the boom opening
from the Prehistoric to the Copper Age (stage 3). Until a stage gate passes, the classic engine stays the default.

## 1. Goals and non-goals

Goals:
- **The owner's difficulty targets:** Medium is an even match, Hard usually wins, Easy is a handicapped Medium. Skill
  comes first, modifiers second.
- **Personalities as scripts:** boom, rush and balanced each with its own plan per epoch. A rush beats a pure boom; a
  boom with towers locks a rush out and wins later.
- **Lockstep safe:** every peer computes the same orders. Save and load keep the strategy's memory.
- **Mods:** a mod can add a personality or change a plan without C++.
- **Observable:** every order and every refused order is in the AI trace and the game log, with the reason.
- **Measured** against the owner's playtest numbers (§6), not only head to head against the classic AI.

Not goals:
- rewriting unit micro (`.tai`, the attack sub-group units), pathfinding or transport internals;
- a learned or LLM opponent (it breaks lockstep);
- matching the classic AI's behaviour.

## 2. The tactics layer (C++)

Tactics are the verbs. Each one is a request that is validated like `EEMessage::Execute`: an invalid request is a
logged no-op, never a crash. A verb reports what happened (done, queued, refused: no site, no producer, too
expensive, not in this epoch), so the strategy sees why something did not happen. That is exactly what the §8.29
strategist could not see.

| Verb | Today's code it takes over (extracted, not rewritten) |
|---|---|
| `gather(resource, n)` | gatherer assignment: `FindResourceTarget`, `FindForestTarget`, `FindFarm`, `FindMines`, `FindFish`, `SendToGather`, idle pickup (EECPResourceManager.cpp) |
| `claim(site)`, `release(site)` | resource centres `SetActive`; the claim range and `EECPRMInClaimRange` |
| `build(type, near)` (town, a site, a point) | `EECPBuildManager` placement (`FindSite`, `FindTowerSite`, `EECPRMPatchSite`, wall plans), builder assignment, repairs |
| `train(class, n, at)` | `EECPUnitManager` production: `StartProduction`, producer choice, `BalanceProduction` |
| `research(tech)`, `advance()` | `EECPResearchManager`; the epoch research itself (it occupies the town centre) |
| `fish(boats, docks)` | the coast fishing, dock and boat requests |
| `group(units, task)`: `defend(area)`, `attack(target, mix)`, `raid(kind)`, `retreat`, `garrison`, `scout(area)` | `EECPAttackCommander`, `EECPDefenseCommander`, `EECPExplorationManager`; the group state machines stay |

**Reflexes** stay tactical and always on: the militia, citizens fleeing to towers, naval flee, the drafted-unit
handover and repairs. The strategy can tune them; they don't wait for it.

**The view** is built fresh each step, as integers:
- **time:** the game time, the current epoch and when it began;
- **stock:** stock and income per resource over the last minute;
- **gatherers:** the count per resource, and boats separately;
- **sites:**
  - claimed and free patches, mines, forests and fish;
  - distance and room for each;
  - the danger near each;
- **buildings:** counts by type, finished and in construction, and what each producer is doing;
- **army:** soldiers by class, with strength as hit points times attack;
- **threats:** enemy strength within the claim range, and where it is;
- **scouting:** what the AI has seen of each enemy (epoch, army estimate, when last seen). Fog-honest unless the
  `reveal` cheat is on;
- **costs:** the next epoch's cost, and the cost of each class the AI can make.

**What must come out of the tactics.** Today's executors carry policy inside them. The extraction has to separate
these, each one an audit item in stage 1:
- `EEComputerPlayer::CanAfford`: the epoch reserve, the prehistoric army cap, the recovery rule;
- `GetWantedCitizens` and `UpdateWantedGatherers`: the targets;
- `CheckBuildingCounts` and the per-epoch build queues: what to build;
- `AttemptEpochAdvance`: when to advance;
- the military pressure (`KeepStandingArmy`): army size;
- the attack timers;
- `ChooseGarrison` and `ChooseFortress`: the town centre and capitol upgrades;
- the island expansion;
- the trade and diplomacy timers.

The classic engine keeps calling these. The next engine never does.

### 2.1 S1 as built (2026-11-08)

Code: `include/ee/EECPNext.h`; `src/Empire Earth/AI Thread/EECPNext.cpp` (switch, state, tick), `EECPNextTactics.cpp`
(verbs), `EECPNextView.cpp` (view), `EECPNextTest.cpp` (TEST ONLY: the C++ test opening).

**The switch.** `bool EECPNextEngine(const EEComputerPlayer*)` is the one test every gate asks:
`EECPEngine(player) == kEECPEngineNext` (S0's seat choice; EE-SIM `--engine next,classic`, sim_bench `--engine`).
`EEComputerPlayer::Update` branches once, at S0's branch point, to `EECPNextUpdate(player, time)`. The first-call
start-up (Regroup and its first centre activations, the timers, ChooseCivilization) still runs for both engines;
the next engine's tick then does the regroup, the combat-state decay and WorkOutPopulation (the pop cap) itself, and
never runs EnableManagers, FlagGroup, `EECPCivilizationManager::Update`, the diplomacy, combat and exploration
updates or KeepStandingArmy.

**The tick** (`EECPNextUpdate`): the runners (`UnitManager::Update`, `ResourceManager::Update`,
`BuildManager::Update`, `ResearchManager::Update`), the reflexes (`RepairBuildings` every 31 s, the attack groups'
state machines through `AttackCommander::Update`; militia, flee and naval flee run on the damage events as before),
then one strategy step on a fresh view. Not run: the wall manager, the defence commander, Personnel's upkeep (its
culling), CombatManager::Update and the wonder race.

**The verbs.** Each returns `EECPNextResult {code, reason, count}`: code `done`, `queued` or `refused`; reason
`no_site`, `no_producer`, `cannot_afford`, `not_in_epoch`, `unknown_class`, `bad_argument`, `no_units`,
`requirements` (SOL or last epoch), `busy`. Each writes one trace line in the manager `next`:
`next.order verb=build cls=47 near=site arg=3/0 x=84 y=68 afford=1 result=queued reason=- n=1`.

| Verb | Signature | Carried out by | Results |
|---|---|---|---|
| gather | `EECPNextGather(p, resource 0..4, n, prefer = patch\|hunt)` | `mWantedGatherers` (food: n land gatherers plus the boats fishing); AssignGatherers, Rebalance, the finders, SendToGather; `hunt` puts all n food gatherers on the game near home first | done; bad_argument |
| claim / release | `EECPNextClaim(p, site)`, `EECPNextRelease(p, site)` | `EECPResourceCenter::SetActive` | done (n=0 when it already was); bad_argument |
| claim_range | `EECPNextClaimRange(p, tiles 25..45)` | the per-player `mClaimRange` (`EECPClaimRange` returns it for the next engine; ClaimExpand does nothing) | done; bad_argument |
| build | `EECPNextBuild(p, type, near = town\|site\|tile, a1, a2)` | the site is found at order time: town = `FindSite` (the classic placement per class: shore, centre, tower, production site), site = the rings 2..5 round its resource (a settlement, as EECPRMPatchSite) or 2..7 (anything else), tile = the rings 0..6; then `QueueBuildingAt` and the build runner | queued (afford=0/1 in the trace); no_site, not_in_epoch, unknown_class, bad_argument (a unit or a wall) |
| fish | `EECPNextFish(p, boats, docks)` | a dock (build near town) while fewer stand or are queued, then `RequestUnits(fishing boat)` up to the count | queued, done; no_producer (no dock built yet), the dock's refusal |
| train | `EECPNextTrain(p, type, n, town = 0)` | `RequestUnits` and the production runner (`StartProduction`: the least busy producer) | queued (afford in the trace); no_producer, not_in_epoch, unknown_class, bad_argument (a building, n out of 1..100) |
| reserve | `EECPNextReserve(p, f, w, s, g, i)` | `EECPNextCanAfford`: the plain `EEPlayer::CanAfford`, then nothing but the epoch advance may spend below the reserve | done; bad_argument |
| research | `EECPNextResearch(p, tech)` | `ResearchManager::Research` into mEpochIDs; TryResearch starts it when affordable | queued; no_producer, busy (researched or queued), not_in_epoch, bad_argument (an epoch or a unit), unknown_class |
| advance | `EECPNextAdvance(p)` | only when the SOL is met and the cost is paid: DropObsoleteRequests, `Research(epoch)`, TryResearch at the town centre | queued; requirements, busy, cannot_afford, no_producer |
| defend | `EECPNextDefend(p, x, y, n)` (x = y = -1: the town centre) | `CreateGroupFromStrategy`: a defence task at the tile (as CreateDefendGroup), "already defined" requirements of the soldiers there are, up to n | done (n = the units in the group); no_units, bad_argument |
| attack | `EECPNextAttack(p, player, n)` | `CreateGroupFromStrategy` with a task naming the player (as CombatManager::Update's attack), up to n soldiers | done; no_units, bad_argument |

Not built yet (S4 added scout, raid, retreat and groups that name their soldiers: §5 "S4 as built"): `upgrade(centre)` (the garrison and fortress choices simply never run), `scout`, `group(units,
task)` with a mix, `raid`, `retreat`, walls.

**The view** (`EECPNextView`, snake_case fields as the Lua table will have them; resources 0..4 = food, wood,
stone, gold, iron):
- `player`, `time`, `epoch`, `epoch_start`, `last_epoch`, `advancing`;
- `stock[5]`, `income[5]` (gathered in the last 60 steps), `reserve[5]`;
- `gatherers[5]` (land), `wanted[5]`, `idle`, `citizens`, `boats`, `population`, `population_cap`;
- `town_x`, `town_y`, `claim_range`;
- `sites[]`: `index`, `resource`, `units`, `amount`, `room` (units x the mining limit less the workers),
  `workers`, `x`, `y`, `distance`, `claimed`, `has_drop`, `has_tower`, `danger` (seen enemy strength within 12
  tiles, +1 in a danger zone), `patch`;
- `buildings[]`: `type`, `finished`, `unfinished`, `queued` (build queues), `busy`, `queue` (production entries);
- `soldiers[]` (`type`, `count`, `strength`), `army_count`, `army_strength` (hit points x attack);
- `threats[]` (by player: `count`, `strength`, nearest `x`, `y`, `distance`) within the claim range, and
  `threat_strength`;
- `enemies[]`: `player`, `epoch`, `seen_army`, `seen_strength`, `last_seen`, `town_x`, `town_y`;
- `next_epoch_cost[5]`, `sol_missing`;
- `costs[]`: every class the player can make now: `type`, `cost[5]`, `building`, `military`, `producer`,
  `producers`, `requested`, `in_production`, `counts_toward_epoch`;
- `events[]` since the last step: `unit_lost`, `production_lost` (the classic re-requested it), `advance_lost`,
  `no_site` (a queued building found no site when its builder was free: dropped), `group_wants` (a group asked for a
  unit that is not there; nothing is trained);
- `groups`.

Fog: other players' units count only while their tile `IsSeenBy` the player (the reveal cheat sees all); the view
never reads EECPArmyThreat or EECPIntelligence. Every 30 s a `next.view` line and every 60 s a `next.sites` line
(the claimed sites) go to the trace.

**Classic-only gates** (each `!EECPNextEngine(player)`; the classic path is unchanged):
- EEComputerPlayer: `CanAfford` (the next engine takes `EECPNextCanAfford`), the deathmatch start-up's
  CheckBuildingCounts and BuildUp;
- EECPResourceManager: `UpdateWantedGatherers` (so the demand split, ActivateNeeded, SettlePatches, GuardSites and
  the split's boat requests), AssignGatherers' two `EECPRMRequestBoats`, `BuildSettlementFor`; the hunt6 test is the
  `hunt` preference instead;
- EECPBuildManager: QueueBuilding's two vetoes (no Prehistoric houses, early_trainers); StartBuilding's
  ClaimExpand: the next engine drops the request and sends `no_site`;
- EECPUnitManager: the automatic citizen request; RequestUnits' cheapest-producer queue (traced `train.noproducer`);
  UnitLost's re-requests and epoch rescheduling (events instead);
- EECPResearchManager: ChooseResearch on its timer; TryResearch's producer queue;
- EECPPersonnel: `RequestProduction` (a group trains nothing; `group_wants`);
- EECPAttackCommander: the Prehistoric and Stone peace veto;
- EECPCombatManager: `NoteThreat`'s groups;
- EECPTrace: `EECPClaimRange` / `EECPClaimExpand` (the per-player `mClaimRange`).

**State.** `EEComputerPlayer::mNextState` (`EECPNextState`, created on the first next-engine tick): the claim range,
the reserve, the food preference, the epoch start, the income ring, the scouting per player, the events, the test
opening's memory. It is a member, not a static array; from save version 0.252 it is in the save with the
strategy's memory (§3.2).

**Gates** (EE-SIM, Small, Medium, 2 AIs, no cheats, balanced; `tools/sim_bench.py`):
- classic unchanged: seeds 2-5, 1800 game s, before (main `593314d`, before S0 and S2) and after (S1 merged with
  them); `ai_trace.py compare` on the `.trace.txt` files (every EE-AI line, the sim hash lines included) after
  dropping S0's ` engine=classic` from the start lines: identical (1842, 1822, 1746 and 1807 lines);
- next: `--engine next,classic --repeat 2`, 3600 game s, seeds 2 and 3: the two runs of each seed print identical
  traces (4432 and 4225 lines). The test opening (p2) reached the Stone Age by 19.5 and 21.5 minutes and the Copper
  Age by 43.0 and 53.0 (the classic p3: Stone by 25.0 and 27.5, no Copper within the hour). Times are the first
  30-second stats line in the epoch.

**Known gaps for S3** (what the boom script will still need):
- `defend` and `attack` form their group (the commander keeps it) but report 0 units placed at creation: whether the
  "already defined" requirements fill from the free soldiers later is not verified; a `group(units, task)` that names
  the units is the robust form;
- the Stone Age needs SOL (2 at its start in the runs): the test meets it by building the cheapest building that counts
  and is not built yet; the view could name the qualifying classes directly;
- `upgrade(centre)`, `scout` (no exploration runs for the next engine, so `enemies` are only what the town sees),
  walls, the farm preference of `gather`, island ferries as an order;
- site indexes are valid for one step only (the centres are re-sorted when new ones form);
- (done in S2, §3.2: the state and the script's memory are in the save);
- the claim's first activations still come from the shared start-up (Regroup); the strategy may release them;
- the classic bookkeeping still runs inside the executors: the mNeeds ledger (AddCosts/RemoveCosts) and the
  EnableManagers flags, which the next engine never sets (its improvements stay off).

## 3. The strategy layer (Lua)

**Runtime: Lua 5.4, vendored** (MIT) under `src/ThirdParty/lua`, built as plain C units for Windows, macOS and Linux
(`tools/units.py` LIB_UNITS, like miniaudio). In the Rust port mlua binds it unchanged. Luau is the alternative if
types and the sandbox matter more than vendoring size (§7).

**Determinism**, the rules from MODDING_PLAN §3.4 and RUST_PLAN §2.1:
- **No `math.random` and no `os` time;** each player has its own RNG, seeded from the game seed and the player id.
- **Integers for everything stored or compared.** Floats only for transient scoring, rounded before any comparison.
- **The `math` library** is limited to integer and IEEE-exact operations (no `sin`, `pow`, `log`). Any needed
  function is supplied in fixed point.
- **Table iteration:**
  - the string hash seed is fixed, so iteration order is the same on every peer and every platform;
  - strategy code iterates through sorted helpers (`each(t)`), never raw `pairs` over mixed keys.
- **An instruction budget per step,** enforced by a count hook. Running over it is deterministic: the step ends, no
  orders are issued, and the trace says so.
- **The gate:** the same seeded EE-SIM game twice on one machine gives identical AI traces, and Windows against
  macOS gives identical traces too.

**State.**
- **The Lua state:** one per computer player.
- **The script's memory:** a plain table of integers, strings and nested tables, with no functions and no
  metatables.
- **Saving:** the memory goes into the save under a new minor version, with keys sorted. On load the scripts run
  again from scratch and get their memory back.

**Sandbox.**
- **No libraries:** no `io`, `os`, `debug`, no `load` from strings, no `require` outside `Data/ai`.
- **On a Lua error,** the seat's orders stop (the tactics' reflexes keep the town alive), the error goes to the
  game log, and EE-SIM marks the run failed.

**Files** (shipped in the db data part, next to the rules documents):

```
Data/ai/lib/*.lua          shared: the economy planner, build orders, the threat model, helpers
Data/ai/boom.lua           personalities: one script each, plus a data table of its numbers
Data/ai/rush.lua
Data/ai/balanced.lua
Data/db/aipersonalities.yaml   which script each personality runs, its numbers, the difficulty levers
```

**Developer loop.** EE-SIM and debug builds reload the scripts when they change, keeping the memory.

### 3.1 Runtime as built (S2)

`include/ee/EECPLua.h` and `src/Empire Earth/AI Thread/EECPLua.cpp`. The runtime knows nothing about the managers:
the caller passes the view in as a value tree and gets the orders back as one.

**API.**
- `EECPLuaValue`: nil, bool, integer, string, array (keys 1..n), map (integer and string keys, kept sorted).
  `EECPLuaKey` orders integers ascending, then strings by bytes. An empty Lua table converts to an empty map.
- `EECPLuaState(const EECPLuaConfig&)`: one per computer player. The config holds:
  - `mSeed`: `EECPLuaSeed(game seed, player)`, which is `seed << 32 | player`;
  - `mMemoryLimit` (32 MB) and `mBudget` (2,000,000 VM instructions a call);
  - `mRead` (the script reader) and `mWrite` (where `print` goes);
  - `mCheckMemory`: check after every step that the memory can be saved.
- `LoadPersonality(module)`: `require(module)` must return a table with `step` and, optionally, `init`.
- `Init(view, orders)`: a new game. `Step(view, orders)`: `step(view, memory)`.
- `Run(source, name, result)`: for tests and a console.
- `Reload()`: the developer loop; the memory and the RNG stay.
- `SaveMemory(bytes)` / `RestoreMemory(bytes)`: a loaded game calls `LoadPersonality` and then `RestoreMemory`,
  never `Init`.
- `GetMemory(value)`.
- `LastStatus()` returns one of: ok, no state, not loaded, no file, syntax, runtime, budget exceeded, out of
  memory, bad value. `LastError()` gives the message (with a Lua traceback). `LastInstructions()`, `MemoryUsed()` and
  `MemoryPeak()` report what a call cost.
- `EECPLuaReadFromFolder`: the default reader. Its context is the folder that stands for `Data`, and it reads
  through `PlatFOpen`.
- Every entry point runs inside `lua_pcall` and returns false on any script error. A failed call keeps whatever it
  wrote to the memory before the error.

**What a script sees.**
- **base:** without `dofile`, `loadfile`, `load`, `collectgarbage` and `warn`. Three functions are replaced:
  - `print` goes to `mWrite`;
  - `tostring` gives a table, function or userdata without `__tostring` as its type's name, never an address;
  - `setmetatable` refuses `__gc` and `__mode`, because finalisers and weak tables depend on when the collector runs.
- **table, utf8.**
- **string:** without `dump`, `pack`, `packsize` and `unpack` (`l` is 4 bytes on Windows, 8 on POSIX). `format`
  refuses these, because they print differently by platform:
  - `%p`, which prints an address;
  - `%a`, and `%q` of a float, which print hexadecimal floats;
  - a NaN or an infinity in a float conversion.

  `%s` of a table prints its type's name. Floats may be formatted for display, but nothing stored in the memory may
  be a float.
- **math:** these are kept:
  - `abs`, `ceil`, `floor`, `fmod`, `modf`, `max`, `min`, `tointeger`, `type`, `ult`: integer or exact operations;
  - `sqrt`: IEEE 754 requires it to be correctly rounded, and every target has a hardware instruction for it;
  - `huge`, `pi`, `maxinteger`, `mininteger`.

  `random`, `randomseed`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `exp` and `log` are removed: libm does not
  round them correctly, and its results differ between UCRT, glibc and Apple.
- **`require(name)`:** loads `Data/ai/<name, with dots as folders>.lua`, once per state.
  - A name is letters, digits and `_`, in segments separated by dots. Nothing else can name a file, so `..`, `/`,
    `\` and drive letters never reach the reader.
  - Text only (mode `"t"`, so bytecode is refused). The chunk name is `@ai/<path>.lua`, so errors read
    `ai/boom.lua:12: ...`.
- **`each(t)`:** iterates over the keys in the total order (integers, then strings). The keys are taken when the
  loop starts, and a key removed during the loop is skipped. A float, boolean or table key is an error.
  `ai.keys(t)` returns the sorted key array.
- **`ai.random(n)` / `ai.random(lo, hi)`:** SplitMix64 (64-bit state, golden-gamma step, the reference mix), drawn
  uniformly by rejection sampling. Its state goes into the memory bytes.

**Determinism in the vendored Lua** (`ee_lua.c` sets macros Lua leaves open; Lua's own files stay unmodified):
- **The string hash seed** is fixed.
- **`table.sort`** never randomises its pivot: the original reads `clock()` and `time()`.
- **`^`** is square-and-multiply for integral exponents (`|y| < 2^53`). Any other exponent is an error, and a
  constant like `2^0.5` already fails to compile.
- **Float text** (`tostring`, `..`, `print`) spells NaN and the infinities `nan`, `inf` and `-inf`; UCRT prints
  `-nan(ind)`. Finite floats keep `%.14g`, which every C library we build with rounds correctly.
- **Character classes and case mapping** in lauxlib, base and string are ASCII only. The game sets a Windows locale
  whose ctype knows Latin-1 letters, while POSIX stays in the C locale.

**Budget and memory.**
- **The budget:** a count hook runs every 1,000 instructions. Past the budget it raises an error, then sets the
  hook to every instruction and raises again at each one, so a script's `pcall` cannot swallow the abort. The same
  code costs the same count on every peer. C functions are not counted (a pathological string pattern can still be
  slow); the memory limit bounds `string.rep` and `table.concat`.
- **The memory limit:** a counting allocator refuses a block past the limit, and the call then fails as out of
  memory.
  - The count uses the sizes Lua asks for. Lua's structs have the same layout on Win64 and LP64 because none of
    them uses `long`, so the collector runs at the same points on every 64-bit target.

**Memory bytes** (`EECPLuaEncode`/`Decode`): canonical, the same bytes for the same content everywhere.
- **The header:** `"EELM"`, version 1, then the RNG state as uint64 LE.
- **The value:**
  - tag `0` nil, `1` false, `2` true;
  - `3` int64 LE;
  - `4` string (uint32 LE length, then the bytes);
  - `5` table (uint32 LE count, then the key/value pairs in key order).
- **Decoding** refuses keys out of order, trailing bytes and nesting deeper than 64.
- **Not storable:** floats (including integral ones like `2.0`), functions, userdata, metatables and cycles. They
  are errors with a path, e.g. `memory.bad: a float`. The orders follow the same rules.

**C++ and longjmp.** Lua is C and raises its errors with longjmp. No function Lua calls into holds a C++ object with
a destructor while Lua can raise. The value tree is pushed inside the protected call and read back outside it.

**Tests:** `python -I tools/xbuild.py luatest` (Windows, Linux, macOS) builds `tests/platform/lua_test.cpp` with
`ee_lua.c`, `EECPLua.cpp` and the platform layer. It runs the 125 checks against the scripts in
`tests/platform/lua/ai/`, which cover:
- the sandbox;
- the budget, including a `pcall` around an endless loop;
- the memory limit;
- RNG reproducibility, pinned to SplitMix64's reference output;
- sorted iteration;
- the memory round trip, byte-identical and pinned against hand-written bytes;
- `require` staying inside `ai/`;
- the value tree round trip;
- floats refused in the orders and the memory;
- syntax, runtime and missing-module errors.

**Open** (after §3.2): reload when the files change (the developer loop: `Reload` exists, nothing calls it yet); the
Windows/macOS trace gate.

### 3.2 S2 gate as built (2026-11-08)

Code: `src/Empire Earth/AI Thread/EECPNextLua.cpp` (the glue: view to table, orders to verbs, the state's save),
`ai/test_opening.lua` and `ai/lib/view.lua` (the scripts), `include/ee/EECPNext.h` (`EECPNextStrategyStep`, the
state's new fields, the save functions).

**Per computer player.** `EECPNextState::mLua`, created at the next engine's first strategy step:
- seed `EECPLuaSeed(gUserInterface->mRandomSeed, player id)` (the seed every peer shares; EE-SIM pins it with
  `--seed`, through `gPlatSimSeed`);
- the reader: `EE_AI_SCRIPTS` first (TEST ONLY), then `Data\ai\` from the data root through `PlatFOpen`
  (`EECPLuaReadFromFolder`);
- `print` -> a `next.print` trace line (manager `next`; line breaks become ` | `);
- `mCheckMemory` on: a step that leaves something unsaveable in the memory fails like an error;
- the script: the personality's `next_script` (aipersonalities.yaml, default `test_opening`). TEST ONLY:
  `EE_AI_NEXT_SCRIPT` names it per player (a comma list by player id from player 1, as `EE_AI_ENGINE`); `c++` there
  runs the C++ test opening instead (the gate's reference). A `next.script name=...` line records the choice.
- A new game calls `init(view, memory)` (optional) and then `step(view, memory)` in the first tick; every later tick
  calls `step` once. The orders of both are carried out in order.

**The script contract.** `require("x")` returns a table with `step` (and optionally `init`); each returns an array of
orders. The view (a fresh table each step; integers, strings, booleans; arrays 1-based; per-resource values are maps
keyed `food`, `wood`, `stone`, `gold`, `iron`):

| Field | What |
|---|---|
| `player`, `time` (game ms), `epoch` (0 Prehistoric), `epoch_start` (ms), `last_epoch` | |
| `advancing` | boolean: the epoch advance is queued or under way |
| `stock`, `income` (last 60 s), `reserve`, `gatherers` (land), `wanted`, `next_epoch_cost` | per-resource maps |
| `idle`, `citizens`, `boats`, `population`, `population_cap` | |
| `town_x`, `town_y` (-1: no town centre), `claim_range`, `claim_range_min`, `claim_range_max`, `claim_range_step` | tiles (25, 45, 5 today) |
| `sites[]` | `index` (its place in `sites`, what `claim`/`release`/`build near="site"` take; valid this step only), `resource` (name; nil when nothing is left), `units`, `amount`, `room`, `workers`, `x`, `y`, `distance`, `danger`; booleans `claimed`, `has_drop`, `has_tower`, `patch`, `explored` (S3: the tile is visible to the player); `holds` (S3: the living units of each resource, a per-resource map: a centre may hold stone and gold); `blocked` (S3: why the classic gatherer finders would pass it over, `unexplored`, `enemy_near`, `danger` or `area`; nil when they would not) |
| `buildings[]` | `type`, `finished`, `unfinished`, `queued`, `busy`, `queue` |
| `soldiers[]` | `type`, `count`, `strength`; and `army_count`, `army_strength` |
| `threats[]` | `player`, `count`, `strength`, `x`, `y`, `distance`; and `threat_strength` |
| `enemies[]` | `player`, `epoch`, `seen_army`, `seen_strength`, `last_seen` (ms, 0 never), `town_x`, `town_y` |
| `sol_missing` | the next epoch's SOL still missing |
| `costs[]` | every class the player can make now: `type`, `cost` (per-resource map), `producer` (nil: none), `producers`, `requested`, `in_production`; booleans `building`, `military`, `counts_toward_epoch` |
| `events[]` | `kind` (`unit_lost`, `production_lost`, `advance_lost`, `no_site`, `group_wants`), `type`, `count` |
| `groups` | (S4: the next engine's groups, §5 "S4 as built"; it was the attack commander's group count) |
| `results[]` | the previous step's orders, index for index: `verb`, `code` (`done`, `queued`, `refused`), `reason` (`none`, `no_site`, `no_producer`, `cannot_afford`, `not_in_epoch`, `unknown_class`, `bad_argument`, `no_units`, `requirements`, `busy`), `count` |

**Classes and techs are named by their data ids:** dbobjects' `id` (`citizen`, `b_barracks`, `b_settlement`,
`b_guard_tower_paleo`, `inf01_clubman`, `s02_fishing_boat_stone`) and dbtechtree's `id` for `research`. The id is
the row's identity in the text tables (DATA_FORMATS §8.6), so a script keeps working when rows move, and a mod
that adds a row gives its scripts a name for it. A data tree without the tables' text (the binary tree) has no ids:
every class is then `dbobjects_<row>` (a tech `dbtechtree_<id>`), the form tools/eedata gives a row without a name.
`EECPNextClassByName` / `EECPNextClassName` (EECPNextLua.cpp) do the lookups, from the ids EEDbFindFile kept when
it compiled the tables. `ai/lib/view.lua` has the helpers: `resources`, `cost(view, type)`, `buildings(view, type)`,
`total(entry)`, `sum(amounts)`.

**The orders** (maps; a missing optional field takes the default shown; an unknown key, a wrong type, an unknown
name or verb is refused `bad_argument` with a `next.order verb=... (why) result=refused` trace line):

| Order | Verb |
|---|---|
| `{verb="gather", resource="food", n=12, prefer="patch"}` (prefer `patch` or `hunt`) | `EECPNextGather` |
| `{verb="claim", site=3}`, `{verb="release", site=3}` (a `sites[].index`) | `EECPNextClaim` / `Release` |
| `{verb="claim_range", range=30}` | `EECPNextClaimRange` |
| `{verb="build", type="b_barracks", near="town"}`; `near="site", site=3`; `near="tile", x=, y=` | `EECPNextBuild` |
| `{verb="fish", boats=6, docks=1}` | `EECPNextFish` |
| `{verb="train", type="citizen", n=2, town=0}` | `EECPNextTrain` |
| `{verb="reserve", food=, wood=, stone=, gold=, iron=}` (each 0 when missing) | `EECPNextReserve` |
| `{verb="research", tech="<dbtechtree id>"}` | `EECPNextResearch` |
| `{verb="advance"}` | `EECPNextAdvance` |
| `{verb="defend", n=4, x=-1, y=-1}` (-1: the town centre) | `EECPNextDefend` |
| `{verb="attack", player=3, n=4}` | `EECPNextAttack` |

At most 200 orders a step (the rest are dropped, a `next.orders dropped=` line). No orders: return `{}` or nil.

**Memory.** `memory` is the same table every step: integers, strings, booleans and nested tables (no floats; §3.1).
The save keeps it, with the RNG (below).

**Errors.** A load, init or step that fails (syntax, runtime, budget, out of memory, a bad value, orders that are not
an array) issues no orders for that step; the tactics' runners and reflexes go on. It writes
`next.error in=step status=runtime msg=ai/x.lua:12: ...` (status with `_` for spaces: `budget_exceeded`) to the AI
trace and the game log; EE-SIM also prints `SIM next.error p2 t=... ...` to stderr, and sim_bench counts those per
run (`lua_errors` in results.csv, "N script errors" on the run's line). A script that does not load stays off for
the game (one error line).

**Where the scripts live.** The repository's `ai/` (new code, not retail data). `tools/open_data.py build` copies it
into the tree's `Data/ai/`; `open_data.py ai TREE` refreshes only `Data/ai` (seconds) and records the scripts' hash as
`ai` in `open_data.json`; the harness (`exe_boot.data_tree`) refreshes its own trees when that hash differs (the
converters' hash leaves the scripts out, so a script change never rebuilds the 4-minute tree); `tools/sim.py` warns.
`open_data.py pack --part db` ships `Data/ai` with `Data/db`. TEST ONLY: `EE_AI_SCRIPTS=<absolute dir>` (the dir
stands for `Data\ai`) is read first; `tools/sim.py --ai-scripts DIR` and `tools/sim_bench.py --ai-scripts DIR` set
it (made absolute: the game runs in its data root).

**The save** (version 0.252, `kSaveGameMinorVersion`): after each computer player's engine byte a `u32` length and
the bytes of `EECPNextSaveState` (0: no next state). Saved: the claim range, the reserve, the food preference and
the land food gatherers, the epoch and its start, the repair timer, the income totals and the 60-step ring, the
scouting per player id (seen army and strength, last seen, the town seen), the pending events, the C++ test opening's
memory, the script's name, whether init ran, whether the script failed, the last step's results (the next view's
`results`) and the Lua memory bytes (`SaveMemory`: the memory table and the RNG state). Not saved: the Lua state
itself (a loaded game runs the modules from scratch, `LoadPersonality` then `RestoreMemory`, never `init`), the
class-name tables (rebuilt), the view (built each step). The classic managers' own state (resource centres, queues,
groups) is the original's save as before. A `next.save bytes= memory= hash=` trace line (FNV-1a of the memory bytes)
records each save.

**fp contraction.** `/clang:-ffp-contract=off` in every module's flags (tools/ee.py MODULE_FLAGS: all of the Windows
build) and `-ffp-contract=off` in tools/xbuild.py (the POSIX game, the platform tests, luatest). On Windows x86_64 it changes nothing: the classic engine's traces
of seeds 2-5 (1800 game s, Medium, `ai_trace.py compare`) are identical before and after the flag (1842, 1822, 1746
and 1807 lines), as expected, since the x86_64 baseline has no FMA instruction to fuse into. It matters on arm64,
where clang fuses `a * b + c` by default; the Windows/macOS trace comparison still has to be run on the Mac.

**Gate** (EE-SIM, Small, Medium, 2 AIs, no cheats, balanced; `--engine next,classic`, 3600 game s, seeds 2 and 3,
`sim_bench.py --env-vary EE_AI_NEXT_SCRIPT=c++,test_opening --repeat 2 --ai-scripts ai`):
- **Lua = C++:** the trace of the Lua opening equals the C++ opening's after dropping the `next.script` lines (the only
  lines that name the script): 4432 and 4225 lines identical, with 487 and 334 `next.order` lines (every order, every
  result). p2 asked for the Stone Age at 18.4 and 20.0 minutes and the Copper Age at 41.7 and 51.9 (the advance
  orders).
- **Lua twice:** the two runs of each seed are identical (4433 and 4226 lines; the C++ ones too).
- **Save:** EE-SIM cannot load a game, so `EE_AI_SAVE_CHECK=1` with `--autosave-ms 600000` (seed 2): both autosaves
  (game 25 and 50 min) decode to a state that re-encodes to the same bytes, the restored script's next step gives the
  same orders and the same memory as the live one (`next.savecheck state=same restored=1`, `step orders=same
  memory=same hash=41919145/41919145`), and the run's trace (save lines dropped) equals the run without saves. The
  save files are version 0.252 with the `EENS`/`EELM` blocks in the players chunk (`tools/harness/savefile.py`
  parses them). A real load is a manual test: save a game with a Next seat, load it from the Load Game screen with
  `EE_AI_TRACE=1`; the trace shows `next.load ... hash=H script=...` (H the `next.save` hash), then `next.script
  name=... restored=1` at the first tick, and no `init`.
- **Errors:** a script that raises, loops (the budget) or returns bad orders writes `next.error` (trace, game log,
  `SIM next.error` on stderr) and the seat goes on with its reflexes; refused orders trace their reason.

**Open after S2:**
- the scripts are not in the multiplayer checksum (`gBinaryChecksum` covers the db, .tai and .cem files): peers with
  different `Data/ai` would diverge silently;
- the Windows/macOS trace gate (with `-ffp-contract=off` now on both);
- reload on change (the developer loop) is not wired;
- a real save-load-resume run of EE-SIM (it has no load path; the shadow check stands in).

## 4. The first scripts

**Boom**, the owner's own play. Each epoch is a plan table, and a short planner runs it. The numbers come from the
playtests (AI_RESEARCH §8.26-§8.29):

| Epoch | Plan |
|---|---|
| Prehistoric | food first (patches, 6 per patch, settlement on the patch, the rest hunting); 2-3 on gold and 6 on iron; 4 clubmen as a mobile defence unless threatened; towers; no houses; no attacks; Stone at about 20-25 min |
| Stone | boats and two docks at once; wood, stone, iron and gold raised (at most 12 gold, 18 iron, about 10-23 wood); food never wasted (finite until farms); 2 barracks, 2 archery ranges, at most 1 temple, university and hospital; towers at the town and the worked sites; a light army that grows with the scouted threat; saving for Copper from Stone's start, with citizens and boats still trained; Copper about 20 min after Stone |
| Copper and later | farms and granaries; then the army that wins (the owner swarms a Copper army with Bronze phalanxes): extended in stage 5 |

**Rush:**
- an early army and early attacks;
- raids that target citizens and fishing boats;
- if the rush fails, a plan to fall back to a boom.

**Balanced:** between the two, reacting to what it scouts.

**Difficulty:** the same scripts at every level, with levers in the data:
- how quickly it reacts to a threat;
- how many orders it may issue a minute;
- a rate of deliberate mistakes;
- the cheats (`ai_cheats`), as today.

Hard should win by playing better first, with modifiers second.

**Lessons from §8.29 built in.**
- **Check producers first.** The planner asks the view whether a producer exists before it counts on soldiers.
- **Never block citizens for the army.** Citizens and boats are never held back while the army waits.
- **Ask for the epoch only when it's paid.** Asking early occupies the town centre.
- **Threats are measured on the claimed land, not by a radius.** A radius reached the enemy's town on small maps.
- **Shares grow with the town.** Wood, stone, gold and iron get their shares as the town grows, never at the cost
  of food.
- **The building list is a whitelist with caps,** not desired counts.

### 4.1 S3 as built (2026-11-08)

Code: `ai/boom.lua` (the script; `lib/view.lua` unchanged), the personalities `balanced` and `wide` run it
(`aipersonalities.yaml` `next_script: boom`; rules.py and EERules.cpp); the tactics changes below. Benchmarks in
AI_RESEARCH.md §8.30.

**The planner.** One plan table per epoch (Prehistoric, Stone, Copper and later) and one pass of the same planner
every step; the claims, gatherers and buildings every 10 s, citizens, the reserve, the advance and soldiers every
step. A `plan` line each minute and a `sites` line every five (`next.print` in the AI trace).

| | Prehistoric | Stone | Copper and later (holding until S5) |
|---|---|---|---|
| Citizens: trained freely / from spare food | 28 / 34 | 64 / 90 | 90 / 100 |
| Reserve the next epoch from | 26 citizens or 18 min | 60 citizens or 10 min (gold and iron from the start) | 95 citizens or 25 min |
| Food share at least | 55 % | 40 % (25 % once the advance's food is paid) | 40 % |
| Caps on wood / stone / gold / iron | 6 / 3 / 3 / 6 | 23 / 6 / 12 / 18 | 26 / 6 / 14 / 20 |
| Bank (kept for the next epoch) | wood 400, gold 250, iron 300 | wood 150, gold 100, iron 100 | wood 300, gold 200, iron 200 |
| Forage patches | 3 | 5 | 5 |
| Soldiers (light / under a threat at most) | 4 / 12 | 8 / 24 | 16 / 40 |
| Towers | 3 | 7 | 10 |
| Settlements | 3 | 6 | 7 |
| Trainers | 1 barracks | 2 barracks, 2 archery ranges | the same |
| Fishing | none | 10 boats, 2 docks | the same |
| Farms | | | a granary, 16 farms, a second granary from 8 |

**What it does, in order.**
- **Refusals.** An order refused for want of a site or producer is not repeated for a minute; a building refused at
  a site passes that site over for five (the next settlement or tower goes to the next site).
- **The threat:** enemy soldiers on the claimed land (the view's `threats`) of at least 2,500 strength (a lone scout
  is left to the militia and the towers), remembered 90 s. The army wanted is the light army plus a third of the
  largest army seen in the last five minutes, and under a threat as many more as it takes for the army and the
  towers (4,000 each) to reach 1.5x the threat. When short under a threat the town scrambles: the reserve is off
  (stone excepted) and soldiers are trained 3 at a time.
- **The reserve and the advance.** The epoch is saved for once the town has its citizens or the epoch has run long:
  the reserve is the advance's cost less what the missing light army costs (the light army comes first), and from
  the Stone Age the Copper Age's gold and iron are reserved from the start. The advance is asked for only when it is
  paid and the SOL is met; the SOL is met by building the cheapest building that counts and is not built yet (no
  town centre, capitol, settlement, farm, granary or house).
- **Citizens** are never held back: two at a time per town centre up to the plan, then from the food above the
  reserve up to the plan's maximum ("food never wasted").
- **Gatherers.** Wood, stone, gold and iron each get `ceil(lack / 30)` gatherers within the plan's floor and cap,
  where the lack is the stock the plan will spend soon less the stock: the reserve, the next barracks and range,
  two settlements, the docks, the next four boats, two towers, three farms, the soldiers still wanted, and the
  bank. Stone is gathered only for towers (the start's 800 pay for four). If those leave food less than its share,
  the largest give way one at a time. Food takes the rest (patches first, then hunting: the `patch` preference).
- **Claims.** The food gatherers' forage patches inside the claim range (6 places each; the claim grows only when
  none is left in it), and for each mineral the nearest open sites out to 60 tiles whose places hold its
  gatherers (2-3 sites; a centre's `holds` counts, so a mixed stone-and-gold centre serves both). Every other claimed
  patch or mine is released: the start-up (Regroup) claims sites 30-40 tiles out, and the classic finder takes
  the farthest claimed site first. A mineral with no reachable site gets no gatherers: they would otherwise be moved
  between resources every 10 s (seen in the first versions: 1,520 moves a game).
- **Settlements** at the kept patches and mines with no drop site at hand, the nearest first, one at a time.
- **Towers:** the first at the town (and a second under a threat), then at the kept sites without one, one at a
  time.
- **Fishing** from the Stone Age's first pass: `fish(10, 2)`.
- **Trainers:** a barracks at 10 citizens; in the Stone Age an archery range, a second barracks at 55 citizens and
  a second range at 65, either sooner under a threat. Soldiers: the cheapest of each trainer, two at a time.
- **Copper and later:** a granary near the town (the claim grows 5 tiles whenever it finds no room, as the classic
  ClaimExpand did), farms (FindSite takes the granary's plots) while the patches cannot hold the food gatherers, a
  second granary from 8 farms; the Bronze Age is saved for from 95 citizens or 25 minutes.
- **No attacks, no defend groups** (see the army side below).

**Tactics changes (economy side).**
- **The view's sites** (EECPNextView.cpp, EECPNextLua.cpp): `holds` (units per resource), `explored` and
  `blocked`, the checks FindResourceTarget and FindFarm make in their order: not visible, an enemy near
  (HasEnemyNear), a danger zone, another non-enemy player's area within 10 tiles.
- **The finders** (EECPResourceManager.cpp): for the next engine, FindResourceTarget and FindFarm send gatherers to
  a claimed site that is not visible. The next engine runs no explorers, and the larger mines 35-60 tiles out were
  never seen, so never gathered from. The view lists every resource centre and the strategy claims the ones it
  wants; walking there explores them. This is a small map-knowledge step past the fog (the original's Regroup
  already builds centres from every resource on the map).
- **HasEnemyNear** (EECPBuildManager.cpp): for the next engine only an enemy's armed units count, not gaia's or an
  ally's.
- **build(settlement, near=site)** (EECPNextTactics.cpp): with no room in the rings 2-5 round the resource, the rings
  6-8, then the centre's own placement within 9 tiles (BuildSettlementFor's fallback).
- Classic unchanged: every change is behind `EECPNextEngine`, and the classic traces are identical (§8.30).

**What the army side must provide for the boom** (the defend/attack/raid/scout groups are not the boom's code):
- `defend` does not work for a boom yet: the group is created with 0 units (`attack.group ... units=0`), its task
  names the first enemy player (in EE-SIM player 1, the idle human), and nothing joins it later; a defend order a
  minute builds up groups. The boom issues none; its soldiers stand where they were trained and fight what comes
  into sight. Wanted: a defend group that takes the named idle soldiers and holds a point (the town, a worked site,
  the threat's tile) and returns when the threat is gone, so the boom can meet raiders at its far mines.
- `scout`: one cheap unit that explores the land round the town and the next mines (the boom now gathers from
  claimed unseen sites; an explorer would make that fog-honest again and show the enemy's army for the
  `seen_army` rule, which today only counts what reaches the town).
- A rush-aware army mix: the boom's light army is 4-5 soldiers at 30-40 min against the aggressive classic's 19-29,
  and it loses the mixed score to it (it survives: §8.30).

**Results** (EE-SIM, Small, Medium, 2 AIs, no cheats, 2 hours; `--engine next,classic` and `classic,next`, seeds
26-49, 48 games; details in AI_RESEARCH.md §8.30):

| Measure (48 games) | Boom | Classic opponent | Owner's target |
|---|---|---|---|
| Stone Age (median) | 22.4 min, 47/48 reached, 42/48 by 25 | 25.0 | 20-26 |
| Copper Age (median) | 52.7 min, 44/48 | 66.3 | 41-46 |
| Copper after Stone (median) | 30.7 min (6/48 within 25, 20/48 within 30) | 41 | about 20 (gate: about 25) |
| Defeated before the Copper Age | 2/48 (4 %) | 0 | gate: under 1 in 10 |
| Defeated within 2 hours | 9/48 | 0 | |
| Citizens at 30 / 60 min | 44 / 76.5 | 35 at 30, 53 at 40 | 50-62 / 85-94 |
| Most on gold / iron / wood in the Stone Age | 10 / 11 / 14 | | at most 12 / 18; 10-23 |
| Fishing boats from the Stone Age | 11 | | 8-10 |
| Towers by 40 min | 6 | | 4-7 |

Against the classic `aggressive` (the rush personality), 16 games (seeds 26-33, both seats): Stone 22.2, Copper
56.8 (15/16), 1/16 defeated before the Copper Age, 2/16 within 2 hours. The mixed score goes to the classic AI
(37-2 with 9 close; against aggressive 10-3 with 3 close): its army is 13-29 at 30-40 min against the boom's 4-5,
and from the Copper Age on the boom's economy shrinks (no Bronze army plan yet). Classic unchanged: the classic
engine's traces of seeds 2-5 (1800 game s) are identical before and after (1842, 1822, 1746 and 1807 lines). Twice
the same seed: identical (seeds 26 and 27, 3600 game s: 4795 and 4733 lines).

**Open after S3:**
- **the gate's second item is not met:** Copper comes 30.7 min after Stone (median; 6/48 within 25). Food, gold and
  iron each reach the Copper Age's cost about 20 min into the Stone Age, and iron is the last in 26 of 48 games. The
  gold and iron gatherers are 5-7 on average, not 12 and 18: the open mines within 60 tiles hold one or two sites'
  places (most mines in the view are `unexplored` or `enemy_near`). A higher Stone Age citizen target changed
  nothing (the town does not reach it before the reserve starts at 10 min). Next levers: a scout, towers and
  settlements that make the far mines safe, citizens at 30 min (44 against the owner's 50-62);
- the army (above); the post-Copper hold (S5);
- the owner playtest (the S3 gate's third item).

## 5. Stages

Each stage ends on a gate. Each one is useful on its own, and the work can stop after any of them.

**S0, the switch** (1-2 days).
- **Engine per seat:** `ai_engine: classic | next` for each computer seat, set in `gameoptions.yaml` per difficulty,
  with a lobby choice later. `EE_AI_ENGINE` sets it per seat in EE-SIM, like `EE_AI_PERSONALITY`.
- **The next engine** starts as a stub that issues no orders; the tactics' reflexes still run.
- **Gate:**
  - the classic engine's traces are byte-identical to today's;
  - the next engine's town stands idle;
  - the full gates pass.

*S0 as built* (2026-11-08). The owner's decision 2 replaced the `gameoptions.yaml` setting: the choice is the lobby's.
- **The choice:** `EEGameOptionsPlayer::mEngine` per lobby slot, 0 classic (the default, and every reset of a slot),
  1 next, 2 random (`ee/EECPEngine.h` `EECPEngineKind`).
- **The lobby:** the engine column sits in the ping column, which a computer row leaves empty, widened to 0.428-0.520
  of the window. The controls are not in dbuicontrols; `EEUIManager`'s ctor adds them with
  `UIControlsTable::AddControlLike` (form 26):
  - 274: the header, a copy of the ping label (2507) shown in its place: "Ping / AI", or "AI" in single player;
  - 275 + slot: the combo box, a copy of the citizens combo (2629; lines Classic, Next, Random = strings 33170-33172);
  - 291 + slot: the label, a copy of the citizens label (2645);
  - control ids 90100-90132; help string 33175.
  `LobbyShowEngine` (flags 0, 6 and 8 of `LobbyShowPlayer`) shows them on computer rows only: the box to the host
  until it is ready on a new game, like the citizens box, else the label.
- **The network:** `EEUIELEChangeEngine` sends `EEMPlayerInformation` field 6 (`kPlayerInfoEngine`).
  `EEMFullPlayerInformation` carries the engine too (4 more payload bytes), for the join sync and the slot resets.
  A client's own full record keeps the host's engine for its slot.
- **Random:** resolved in `EEGameState::Create`, when the computer player is made, by `EECPEngineResolve`. It is a
  32-bit hash of `gUserInterface->mRandomSeed` (every peer's) and the slot, not one of the game's generators, so the
  random start epoch and the map draw as before. The resolved engine goes to `EEComputerPlayer::mEngine` (a byte in
  the class's padding: the size stays 0xf58) and into the game's options.
- **TEST ONLY:** `EE_AI_ENGINE` (classic/next/random, a comma list in turn by player id from player 1, as
  `EE_AI_PERSONALITY`) overrides the lobby at that point.
- **Save games:** version 0.251 (`kSaveGameMinorVersion`; the original wrote 0.250). It adds a `u32` engine after
  each game options slot and a `u8` engine after each computer player in the players chunk (`EEPPlayerHelper`), so a
  loaded game keeps its engines. 0.250 saves and scenarios load as classic. The scenario editor's computer players
  are classic.
- **The branch point:** `EEComputerPlayer::Update`. The start-up block runs for both engines. Then a next player
  calls `EECPNextUpdate` (`EECPEngine.cpp`) instead of the original's block (regroup, enable timers, population,
  expansion, the civilization, diplomacy, combat and exploration managers, the standing army). The unit kill list
  after it runs for both. The stub only writes the 30 s stats line. S1-S3 replace it with view -> Lua strategy ->
  tactics.
- **Classic:** takes the original's path unchanged. Its only new output is ` engine=classic` at the end of the
  trace's `cp start` line. The game log's player line has `engine=` (tools/game_log.py shows it), and
  tools/ai_scorecard.py can tell the players apart by it (`--key engine`).
- **EE-SIM:** `--engine LIST` (one engine for every seat, or a comma list by seat; seat 1 is player 2) selects the
  engine boxes 275 + seat through the lobby, as `--lobby 276=next` would. The report's setup records it.
  `sim_bench.py --engine` takes `;` between variants.

**S1, the tactics API** (the largest stage: 1-2 weeks).
- **Extraction:** take the executors out of the managers (§2), and make the policy cuts listed there.
- **Proof:** a C++ test strategy with a fixed build order shows that every verb works.
- **Gate:**
  - the C++ build order reaches the Stone Age in EE-SIM, and its traces are identical across two runs;
  - the classic engine is unchanged, by trace comparison on a fixed set of seeds.

**S2, the Lua runtime** (about a week).
- **Runtime:** vendor Lua, then add the sandbox, the budget, the RNG, sorted iteration, memory in the save, and
  the error path.
- **Determinism tests:** in `tests/platform`.
- **Gate:**
  - the S1 build order rewritten in Lua plays the same game as the C++ one, with identical traces;
  - the Windows and macOS traces of one seed are identical;
  - a save and load in the middle of the game resumes with the same memory.

**S3, the boom script, Prehistoric to Copper** (1-2 weeks with sim rounds).
- **Gate,** against the targets in §6:
  - Stone at 25 minutes at most, and Copper within about 25 minutes of Stone;
  - it survives a classic rush (fewer than 1 in 10 boom AIs defeated before Copper);
  - an owner playtest.

**S4, rush and balanced.**
- **The personality matrix:** boom, rush, balanced and classic against each other.
- **Difficulty:** the levers.
- **Gate:**
  - rush beats a pure boom, boom beats a failed rush;
  - balanced holds its own;
  - the owner's Medium is even.

*S4 as built* (2026-11-08; the army side and the rush and balanced scripts; the boom script is S3's).

**The army** (`src/Empire Earth/AI Thread/EECPNextArmy.cpp`, new; `ee/EECPNext.h`). S1's `defend` and `attack` handed
a strategy to `CreateGroupFromStrategy`: the UnitSelector sized it its own way, the sub-groups asked Personnel for the
units, and nothing arrived (0 units placed, and the group's production is off for the next engine). The next engine
now owns its groups:
- **Taking soldiers.** A group verb takes up to `n` of the free land soldiers (in none of the next engine's groups),
  only the `classes` named when there are any, or with `from = id` out of another group: the nearest to the town
  first, a raid's and a scout's the fastest first. Each leaves the classic attack commander through
  `EEComputerPlayer::DetachUnitFromAI` (out of Personnel and the area groups, `mIsDetachedFromAI`).
  `EEComputerPlayer::UnitGoalDone` gives a detached unit back to the commander when its goals run out; it now asks
  `EECPNextHoldsUnit` first (classic never has a next state: unchanged). `disband` gives them back
  (`CombatManager::UnitAdded`). Soldiers no group took stay with the classic commander as before.
- **The step** (`EECPNextArmyStep`, every tick after the commander's update, before the view). It drops the lost
  units, then gives each group's units their goals itself: `EEUGAttackUnit` on a seen enemy unit, `EEUGLocation`
  towards its place (a leg of 14 tiles ahead of the group's centre, given again when the leg moves 4 tiles or after
  10 s; non-forced, so they fight what they meet, forced for retreat and scout). A new group forms first at the
  soldier nearest its centre (20 s at most, or 80% there). The targets are the enemy units whose tile the player sees
  now (`IsSeenBy`), so the army is as fog-honest as the view; each unit keeps its target while it is still the best
  kind it can reach, else takes the nearest (one already taken counts a little further; buildings: the weakest first;
  boats and ships only for ranged units).
- **Tasks:** `attack` (soldiers within 12 tiles, then citizens within 20, towers, buildings; at its place it looks for
  the target's units within 30 tiles, then its town; nothing: `arrived`); `raid` (citizens, then fishing boats, and
  soldiers only within 6 tiles; nothing within 9 tiles of a known tower; at its place the next gather spot; it flees
  home when the seen soldiers and towers within 10 tiles are 1.5 times its strength); `defend` (the enemies within
  its radius of the place, or 6 tiles of the group; strays come back); `retreat` (forced home, then it defends the
  town); `scout` (forced moves to the unexplored grid point (8 tiles) on its continent nearest it and furthest from
  the town; skips a place it does not reach in 45 s; `done` when nothing is left).
- **The scouting memory** (in the state, saved): the enemies' gather spots (citizens or fishing boats seen; one spot
  per 8 tiles, 8 per player; forgotten when seen empty for a minute) and towers (forgotten when their tile is seen
  without them), the citizens seen now, where their soldiers were last seen.

**Verbs** (Lua; every group verb's result carries `group`, the id):

| Order | What |
|---|---|
| `{verb="attack", player=3, n=8, classes={...}, group=id, from=id, x=, y=}` | a new group (or `group` takes the task and `n` more); the place: x, y, else the target's town seen; `no_target` when none is seen |
| `{verb="raid", player=3, n=4, ...}` | the place: x, y, else its latest gather spot with no known tower near, else its town |
| `{verb="defend", n=4, x=-1, y=-1, radius=0, group=, from=, classes=}` | -1: the town, radius 0: the claim range (12 at a tile) |
| `{verb="scout", n=1, classes=, from=}` | |
| `{verb="retreat", group=id, x=, y=}` | group 0 or none: every group but the scouts; home by default |
| `{verb="regroup", group=id, x=, y=}` | hold a place (default: its centre) as a defence of 8 tiles |
| `{verb="disband", group=id}` | the soldiers back to the classic commander |

The same task at the same place only adds soldiers (the group goes on as it was). The C++ test opening's
`EECPNextDefend` and `EECPNextAttack` make these groups too.

**View** (new fields): `groups[]` (it was the commander's group count): `id`, `task`, `state` (`forming`, `moving`,
`fighting`, `arrived`, `done`), `units`, `strength`, `x`, `y` (its centre), `target`, `tx`, `ty` (its place),
`enemy_count`, `enemy_strength` (seen soldiers within 12 tiles), `targets` (what it would fight now), `started` (its
most units), `lost`, `created`; `free_soldiers`, `free_strength`; per enemy `towers`, `citizens_seen`, `citizen_x`,
`citizen_y` (its latest gather spot with no known tower near), `army_x`, `army_y`. Results: `group`. Reason:
`no_target`. Save: the state block is version 2, the army after the Lua memory (groups, spots, towers, result group
ids); version 1 still loads.

**Scripts.** `ai/lib/army.lua` (generic: groups by role from the results, `pick_enemy` (the largest economy seen),
the classes a producer makes); `ai/lib/rb_economy.lua` and `ai/lib/rb_army.lua` (the rush and balanced economy and
army, numbers from a plan table); `ai/rush.lua`; `ai/balanced.lua`. Mapping for now: `aggressive` runs `rush`,
`tall` runs `balanced` (S3 gives `balanced` and `wide` the boom; the final mapping is the owner's). TEST ONLY:
`EE_AI_NEXT_SCRIPT` also takes `/` between players (sim_bench's `--env-vary` splits on commas).
- **Economy** (both): citizens two at a time and never held back for the army (soldiers wait while the town is short
  of citizens, unless the army is under half its target); the gatherer split in per-resource bounds with food at
  least 40-45%; while saving, every mineral the advance does not need goes to its least; forage patches claimed, the
  claim grows and the food gatherers hunt when none is left; settlements at patches; towers at the worked sites;
  docks and boats from the Stone Age; the plan's buildings (a whitelist with counts); the SOL by the cheapest
  building; the reserve and the advance only when paid. Farms from the Copper Age.
- **Army** (both): a scout (one soldier); the home guard (`defend` at the town, the claim range) takes every free
  soldier; waves `attack ... from = home`; reinforcements; raids `from = home`; a group out retreats from a stronger
  enemy, and every group out is recalled when the threat at the town outweighs the home guard; no wave while the
  town is threatened; a group back home is folded into the guard; a wave lost (or back with half its units lost)
  counts as failed.
- **Rush:** 24/55/75 citizens, iron-heavy (30% in the Prehistoric), two barracks at 7 and 11 citizens, no army cap but
  the population (held at its size while 4+ citizens short with 8 soldiers), the scout at 1 minute, raids of 3 every
  2.5 minutes, waves of 6 in the Prehistoric and 10 later (more than the target was seen with), a wave as the target
  reaches a new epoch, Stone saved for from 12 minutes. **Falls back** to a boom (the BOOM plan: 30/80/95 citizens,
  towers 2/4/6, two docks, an army of 6/14/20, attacks only from 1.6 times the target's seen strength) after two
  failed waves, from 25 minutes against a target with 4+ towers and a stronger army, or still Prehistoric at 30.
- **Balanced:** 32/88/100 citizens, gold at most 3/12/14, iron 6/18/20, wood up to 23, one barracks, a range from
  40 citizens, towers 2/4/6, 10-12 boats in two docks; an army of 6/16/26 (or half as many again as the most seen,
  at most a third of the pop cap); the scout at 2.5 minutes, again after 8 minutes without news; a wave (keeping 4
  home) only when 1.5 times the target's seen strength (towers counted) with 8/14/20 soldiers; raids of 4 every 6
  minutes on targets with 3 towers at most.

**Results:** docs/AI_RESEARCH.md §8.31 (the matrix).

**S5, later epochs.**
- **Coverage:** Copper to Bronze (farms, the phalanx swarm), then the Middle Ages and on.
- **New verbs** in the tactics as the scripts need them: siege, navy, air.

**S6, the default.**
- **When:** the next engine becomes the default when the owner's playtests say so.
- **Classic stays selectable;** deleting it is the owner's call.

**S7, documentation and Rust.**
- **Modding:** the scripting reference in DATA_FORMATS.md and a modding guide.
- **Rust:** mlua when RUST_PLAN reaches R5.

## 6. Measuring

- **Targets from the owner's playtests,** in a new `tools/ai_targets.py` that reads EE-SIM runs and game logs. The
  owner's last game:

  | Measure | Owner's value |
  |---|---|
  | Stone | 20-26 min |
  | Copper | 41-46 min |
  | Bronze | 67-86 min |
  | Citizens at 30 / 60 min | 50-62 / 85-94 |
  | On gold, Stone Age | 12 at most |
  | On iron, Stone Age | 18 at most |
  | Wood gatherers | 10-23 |
  | Fishing boats from Stone | 8-10 |
  | Towers by 40 min | 4-7 |

- **Matchups,** not a single win count. A boom is judged by its pace and whether it survives; a rush by how often it
  breaks a boom (AI_RESEARCH §8.29).
- **Scores:** `--score mixed` (economy, military, tech), and the defeat rate.
- **Traces:**
  - every step writes a `plan` line: the phase, the targets and the orders issued or refused, with reasons;
  - `tools/game_log.py` shows them for the owner's playtests.
- **In `gates.sh`:** one next-engine AI game per tree, at the time cap with both alive, after S3.

## 7. Risks and decisions

Risks:
- **Hidden policy in the tactics** (§2): the S1 audit has to find all of it. The safeguard is the classic engine's
  trace staying identical through the whole extraction.
- **Lockstep:** floats, hashing and iteration order. The S2 cross-platform trace gate catches these.
- **Cost:** up to 15 computer players run once a second each. The instruction budget and an integer view keep it
  small.
- **Scope:** weeks before the first playtest (S3). S0-S2 bring no gameplay change.
- **Measuring:** a sim opponent unlike the owner. The answer is the targets in §6 and the owner's playtests, with
  the classic AI as one opponent among several.

Decisions (the owner, 2026-11-08):
1. **Lua 5.4.**
2. **A lobby choice from the start** for each computer seat: classic, next, or random. EE-SIM and `EE_AI_ENGINE`
   choose it too.
3. **The scripts ship in the db data part** (`Data/ai`, next to the rules documents).
4. **Fog of war is on:** the view holds only what the AI has seen, unless the `reveal` cheat is on.
5. **On `main`, behind the switch.** The work runs through S4.

## 8. First steps

1. The owner's decisions: done (§7).
2. **S0:** the per-seat switch and an idle stub: built (§5, "S0 as built").
3. **The S1 audit:** done, in [AI_LUA_AUDIT.md](AI_LUA_AUDIT.md): a table of every decision buried in the managers' executors, file by file,
   with where each one ends up (a tactic, a strategy order, or classic only).
4. **S1:** extract the executors in the order the boom opening needs them: gathering and claims, building and
   placement, production, research and the advance, then groups.
