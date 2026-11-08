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

**Open.**
- `-ffp-contract=off` for both C++ builds (RUST_PLAN §2.1) before the Windows/macOS trace gate: today's float paths
  in the Lua VM are single operations, but nothing stops clang fusing them on arm64.
- The game side is not wired yet:
  - a reader over `Data\ai\` from the data root;
  - `print` into the AI trace (`EE_CP_TRACE`);
  - the memory in the save;
  - reload when the files change.
- The S2 gate (the S1 build order in Lua, identical traces) waits for S1.

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
2. **S0:** the per-seat switch and an idle stub.
3. **The S1 audit:** done, in [AI_LUA_AUDIT.md](AI_LUA_AUDIT.md): a table of every decision buried in the managers' executors, file by file,
   with where each one ends up (a tactic, a strategy order, or classic only).
4. **S1:** extract the executors in the order the boom opening needs them: gathering and claims, building and
   placement, production, research and the advance, then groups.
