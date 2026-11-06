# Empire Earth AoC computer-player AI: research and modernisation plan

Status: stage 0 under way on the `new-ai` branch (§8: the trace, the cheat switches, the benchmark and the first
numbers). Started 2026-11-05.
`AI` stands for `src/Empire Earth/AI Thread/`. Method names come from the reconstruction and are often provisional:
AI has 86 `provisional` markers and about 115 unnamed members. **[inferred]** marks reasoning rather than reading.
⚠ marks external claims taken from forums or secondary sources only.

Companion documents: [MODDING_PLAN.md](MODDING_PLAN.md) covers the AI literals (§G), the scripting stance (§3.4) and
the determinism gates (Phase 0). [DATA_FORMATS.md](DATA_FORMATS.md) covers `dbcpbehavior`.

---

## 0. Summary

- **What EE's AI is.** About 60k lines of hand-written C++ in a fixed hierarchy:
  - `EEComputerPlayer`
  - Civilization manager (build, resource, research, unit and wall managers)
  - Combat manager (attack and defence commanders, personnel, unit selector, intelligence)
  - attack groups, sub-groups and per-unit controllers
  - the `.tai` unit state machines

  Tuning is ~120 `dbcpbehavior` values with Easy/Medium/Hard variants. It has no personality system. The
  diplomacy manager is empty.
- **How it wins today: it cheats.**
  - Map reveal at start.
  - Omniscient strategic intelligence.
  - Free epoch advances.
  - Population and epoch rubber-banding toward the human.
  - Possibly a large cost discount: §2.4 reads it as 1% of normal cost, which needs a runtime check.

  The fix is largely the same as the modernisation: honest perception, plus better decisions.
- **Where the field has converged** (§3):
  - Pathing, terrain analysis, threat maps and combat estimation stay native.
  - *Decisions* are data or scripts: AoE2 `.per`, OpenRA YAML, SupCom/FAF Lua, 0 A.D. JS.
  - The AI runs in the lockstep sim on every peer, at a fixed cadence.
  - Difficulty is handicapped decision-making rather than hidden bonuses.
  - ML is used offline for tuning. Live LLM/RL control is not used in lockstep games.
- **Lua is a good fit for the *strategic* layer, and a bad fit for everything below it.** The scenario-trigger AI
  effects already drive a narrow "external brain" seam: `CreateAIMilitaryGroup`, `SetAIVariable`, `SetFavoriteUnits`,
  `BuildObject` and others (§2.3). A script that drives this request layer keeps the 30k lines of working C++ tactics
  and pathing.
- **Recommended path** (§5), in five stages. Each stage stands on its own.

  | Stage | Work |
  |---|---|
  | 0 | Measure: decision trace, benchmark, honest no-cheat baseline |
  | 1 | Decisions and cheats as data, plus personalities |
  | 2 | A fair economy and honest perception in C++: the stage that matters most |
  | 3 | A Factorio-style deterministic Lua 5.4 strategist over a validated request API |
  | 4 | Self-play tuning and handicap-based difficulty |

  Replacing the tactical C++ with script is not recommended.

This revisits MODDING_PLAN §3.4, "Why not embed Lua now". That reasoning still holds for *content rules*. The AI is
a better first candidate for three reasons:

1. Its output is a small set of requests, not arbitrary state edits.
2. It already runs at about 1 Hz per player, so the script cost is bounded.
3. Retail multiplayer compatibility is no longer a constraint (owner decision, 2026-11-04).

Determinism between this port's own peers still is a constraint.

---

## 1. Goals

1. **A better opponent:** fair at every difficulty, varied, beatable in interesting ways, and strong at the top
   without cheats.
2. **Moddable AI:** personalities, civ- and epoch-specific openings, and scenario AIs, without recompiling.
3. **Determinism and saves preserved:** every peer runs every AI, and a save mid-game resumes identically.
4. **Survives the Rust port:** the script API is a narrow, C-shaped boundary (mlua covers Lua 5.4 and Luau).

Non-goals: live LLM/RL agents (§3.4), and rewriting pathfinding or the continent manager (shared with all units).

---

## 2. The current AI

### 2.1 Update loop

- **It runs in the physics tick, not on a thread.** `EEPhysics::Tick` calls `gArtificialIntelligence->Update()`
  (`src/Empire Earth/Physics Thread/EEPhysics.cpp:753`), after paths and units and before scenarios and game-over.
- **Round robin.** One computer player per PT, `mUpdateInterval = 1000 / N` ms
  (`AI/EEArtificialIntelligence.cpp:52-79`). Each CP therefore gets one full `Update` per game second.
- **`EEComputerPlayer::Update`** (`AI/EEComputerPlayer.cpp:5563-5631`) runs, in order:
  - civilization manager
  - diplomacy (empty)
  - combat manager
  - exploration
  - `EnableManagers` (:249-372): a priority list of manager ids, filtered by combat state and resources, with the
    "other" managers shuffled using the synced RNG; only `mMaxEnabledManagers + 1` run.
- **Sub-periods** (all effectively quantised to 1 s **[inferred]**):

  | System | Period | Source |
  |---|---|---|
  | Build | 3 s; builders reassigned every 2 min | `EECPBuildManager.cpp:1624` |
  | Exploration | 30 s | |
  | Walls | 30 s | |
  | Intelligence recount | 30 s | `EECPIntelligence.cpp:169` |
  | Personnel | 30 s | |
  | Sub-groups | 2-2.5 s | `EECPASGLand.cpp:1221` |
  | Per-unit controllers | 1 s | |

- **Path searches** are time-sliced at 32 steps per PT (`AI/EEPathManager.cpp:1683-1718`). They are shared with
  human-owned units.

### 2.2 Responsibilities and data flow

- **Economy.** Producers call three request APIs:
  - `BuildManager::QueueBuilding/QueueBuildingAt`
  - `UnitManager::RequestUnits(class, n, town, continent)`
  - `ResearchManager::Research*`

  Requests sit in **per-epoch FIFOs**. Priority comes only from ordering: blocked requests are rotated, obsolete ones
  dropped, cancelled builds pushed to the front. There is no scored queue and no resource budgeting across
  consumers, which 0 A.D.'s queueManager has.
- **Military.**
  1. `CombatManager::Update` picks a target player. It prefers humans, unless `GetRandom() < aggression/100`.
  2. It ranks targets by `EECPIntelligencePlayerFile` data (front overlap, distance, weak gatherers).
  3. It builds an `EECPAGTask` and calls `AttackCommander::CreateGroup`.
  4. `UnitSelector` picks classes using literal domain-share heuristics and favourite units.
  5. Personnel drafts units, or calls `RequestUnits`.
- **Events in.** `EEPlayer` virtuals: `UnitPlaced`, `UnitCompleted`, `UnitTaken`, `UnitGoalDone`, `DamageTaken`,
  `UnitStranded`, `EpochReached`, `WonderStarted`, `FlareReceived` (`include/ee/EEComputerPlayer.h:33-95`).

### 2.3 Output seam (the key finding)

**The AI does not use the command/message layer.** Human input arrives as `EEMessage` objects executed in
`EEPhysics::DrainInput` (`EEPhysics.cpp:859-940`). The AI instead mutates sim state directly, in-tick. Both paths
end in the same unit primitives:

| Action | AI | Human |
|---|---|---|
| Train / research | `EEComplexUnit::QueueResearch` (`EECPUnitManager.cpp:288`, `EECPResearchManager.cpp:458`) | `EEMResearchTechnology::Execute` (`EEMessage.cpp:6318`) |
| Place a building | `EEMakeBuilding` + `EEUGBuildRepair` goal (`EECPBuildManager.cpp:324-330`) | `EEMBuildUnit` (`EEMessage.cpp:1156`), which also runs `EECanPlaceOnTile` |
| Move / attack / gather / transport | `unit->SetGoal(new EEUG…)`: `EEUGLocation` ×41, `EEUGAttackUnit` ×9, transports ×9, … | matching messages |
| Other | `StartAction` (~45 sites), `ClearGoalQueue`, `Kill`, `SetAIState`, `AddResource` refunds | none: AI only |

The candidate seams, from lowest to highest level:

1. **Unit primitives.** Narrow, but a script here would have to re-implement all of the tactics, and these calls
   skip the validation that the messages do.
2. **Manager request layer** (recommended):
   - Economy: `QueueBuilding`, `RequestUnits`, `ResearchUnitClass`.
   - Military: `CombatManager::AddAttackTask/AddDefendTask/AttackUnit`, `AttackCommander::CreateGroupFromStrategy`.

   The scenario AI effects already drive this layer, so it is proven to work from outside the AI:
   - `EESMEffect` `BuildObject`, `CreateAIMilitaryGroup`, `ModifyAIMilitaryTarget`, `MaintainObject`,
     `SetAIVariable`, `SetFavoriteUnits`, `SetAIToggle` (enum `include/ee/EEScenarioIDs.h:7-26`).
3. **Behaviour values.** `SetAIVariable` can already rewrite any `dbcpbehavior` field per player at runtime.

### 2.4 Inputs and cheats

- **Map reveal.** `ExploreUnseen` on the first update (`EEComputerPlayer.cpp:5568`, `RTSMap.cpp:723`) explores the
  whole map.
- **Strategic omniscience.** Intelligence walks every player's `mFirstUnit` list with no fog check
  (`EECPIntelligence.cpp:186-226`). Fog checks (`IsSeenBy` and similar, about 26 sites) exist only tactically.
- **Cost scaling.** `mIsCheating = true` unconditionally (`EEComputerPlayer.cpp:1322`). The virtual `ScaleCost`
  override (:757) multiplies every cost by `GetUnitCostModifier()`, which is `value × 0.01`. The override
  `EEPlayer.h:770` is used by `Pay`, `CanAfford` and refunds (`EEPlayer.cpp:403-424, 1583-1616`).
  - The shipped `data/Data/db/dbcpbehavior.csv` has row 67 (Medium) float `1.0` and row 96 (Easy/Hard) int `1/1`,
    with sliders 0-500.
  - So the AI pays **1%** of costs. `UMFloatToLong` is `lrintf`, round to nearest
    (`include/lle/LowLevelEngineCore.h:142`):
    - any cost under 50 is free;
    - a 100-cost unit costs 1;
    - a 2,000-cost building costs 20.
  - This matches the game's reputation for a cheating AI that "doesn't pay". One runtime confirmation is still
    worth doing (Q1).
  - **Consequence: the AI economy has never worked under real scarcity.** The gather/assign logic, the per-epoch
    FIFOs and the affordability checks barely bind. Turning the discount off will expose economy weaknesses that
    nobody has seen before. That makes stage 2's resource arbitration a prerequisite for a fair AI, not polish.
- **Free epochs and starting resources.** Epoch advance is free (`EEComputerPlayer.cpp:5438`). The AI starts with
  +100 gold and iron (row 101, :5578).
- **Rubber-banding:**
  - The pop cap blends toward the largest human population (`WorkOutPopulation` :211-245).
  - `CatchUpEpoch` (:1048) advances the AI when it falls behind.
  - Obsolete units nobody can see are deleted (`EECPPersonnel.cpp:760-789`).
- The scenario `SetAIToggle` "Cheat" toggle is the only switch for `mIsCheating`.

### 2.5 Determinism

- **Every peer runs every AI.** There is no host check.
- **Randomness** comes only from `gPhysics->GetRandom` (207 sites) and `mRandom.Generate` (40), plus
  `EEAIRandomShuffle` (PORT_LEARNINGS.md:230). There is no `rand()` or wall clock.
  - **Any script RNG must be a separate seeded stream.** Drawing from the shared one shifts every later draw in the
    sim.
- **Floats** are used throughout, including the game clock. x87 and SSE2 builds diverge at about 256-290 s
  (PORT_LEARNINGS.md:211-216, 719-726).
- **One address-ordered `std::set`** (`EEAIAStarPathAreaGoal.cpp:282`).
- **A few reads of local or UI state:** `gGameScreenActive` (`EECPPersonnel.cpp:528`, `EECPCombatManager.cpp:1094`).
  These are latent desync risks in MP.

### 2.6 Difficulty and personality

- **Difficulty.** `Init` copies ~120 behaviour fields (:1121-1311). About a dozen accessors pick values per
  difficulty, with deathmatch variants. Difficulty only scales numbers. Every level runs the same logic, and every
  level cheats.
- **Variety.** A random premade civ from a hard-coded epoch table (:4602-4660), random favourite units per domain
  (:440-538), and `mComputerAggression`. There are no personalities, openings or plans.

### 2.7 `.tai` unit AI

- **Format.** 72 files, 3,356 lines, parsed by `EELoadScript` (`src/Empire Earth/Units/EEUnitAction.cpp:810-930`).
  Each state lists ordered triggers that lead to `true(State)` / `false(State)`.
- **Vocabulary.** 251 triggers and 168 actions (:140, :572). Conditions resolve through
  `EEUnitClass::EvaluateCondition` (`EEUnitAI.cpp:368`).
- **Relation to the strategic AI.** Above `.tai` sits a second, hard-coded C++ FSM layer for sub-groups and units
  (`EECPAGOrder.h:40-49`). The strategic AI drives units with goals and forced `StartAction` calls.
- **Plan:** keep `.tai` as the micro layer, and improve it with data only.

### 2.8 Size by layer (60,387 lines)

| Layer | Lines | Script candidate? |
|---|---|---|
| Pathfinding and map analysis (shared) | 12,802 | No: native forever |
| Strategic economy and core (Load/Save is ~3,650 of it) | 16,098 | **Yes**: decisions only |
| Strategic military | 9,251 | **Yes**: target and composition choice, timing |
| Tactical groups (`AttackGroup`, `ASG*`, formations) | 14,330 | No: keep, parameterise |
| Unit micro (`ASGU*`) | 5,302 | No: keep, parameterise |
| Herds, function objects | 2,604 | No |

### 2.9 Weaknesses, besides the cheats

- **No resource arbitration.** FIFO queues per epoch mean the economy has no strategy: no boom vs rush vs tech.
- **No opening books** and no plan-level reasoning. Behaviour is random within literal ranges.
- **No scouting model.** It doesn't need one, because it cheats. Removing the cheats requires building one.
- **Hard-coded literals:** ~300-500 real tuning constants (MODDING_PLAN §G).
- **Diplomacy is empty**, so team games have no tribute, alliance or aid logic beyond `EECPMilitaryAidPackage`.
- **Behaviour bugs already fixed:** FIXES.md #3, #5, #24-41, #51. The shipped AI was buggier than its design
  intended.

---

## 3. What modern RTS games do (survey)

### 3.1 Shipped and open games

| Game | AI language | Layer scripted | Determinism / save | Lesson |
|---|---|---|---|---|
| AoE II DE | `.per` rules + strategic numbers | Intent and ratios only; engine owns micro | Declarative, trivially safe | Most value from the simplest model. Community AIs (Barbarian, Promi) beat the default ⚠ |
| AoE III DE / AoM | XS (C-like) | Whole AI, rule activation | In-sim | |
| AoE IV | SCAR (Lua) for modes; **AI core closed** ⚠ | Only tuning XML | | Even a Lua-heavy engine kept the AI native |
| 0 A.D. | JavaScript (Petra) | Whole AI: HQ, queueManager, base, attack and defence managers | In-sim every peer; hand-written `Serialize` per manager; planned AI thread was never shipped | Script AIs pay for themselves in serialisation work |
| Spring / BAR | C++ Skirmish AI (BARb/Circuit), Lua (Shard/STAI), synced Lua gadgets | Varies | streflop floats; synced/unsynced split; mixing them caused desyncs | Keep an explicit synced boundary |
| SupCom / FAF | Lua in the 10 Hz sim | aiBrain + builders (priority/conditions) + platoons | Lockstep; AI cost slows everyone | Closest analogue. The strongest AI (M28) is non-cheating, with plausible-knowledge perception |
| OpenRA | C# modules + YAML | Pure data personalities | Safe | Data-driven personalities give most of the modding value |
| Warcraft III | JASS with cooperative threads | Whole AI | | `common.ai` unchanged for 15 years ⚠ |
| PA (TA heir) | JSON build conditions + small neural net | Data | | |
| EE "Age of AI" mod | C++ DLL hooking the retail exe; ini + `.eeai` personality files | Fixes and features inside the stock AI | n/a | The closest prior work on this very AI (§8.4) |

Sources:
- AoE2: https://airef.github.io/
- 0 A.D.: https://trac.wildfiregames.com/wiki/PetraBot , https://wildfiregames.com/forum/topic/15418-ai-and-saved-games
- Spring: https://springrts.com/wiki/Lua:Environments , https://springrts.com/wiki/Syncing_System
- SupCom lockstep: https://www.forrestthewoods.com/blog/synchronous_rts_engines_2_sync_harder/
- M28AI: https://www.moddb.com/mods/m28ai
- OpenRA: https://github.com/OpenRA/OpenRA/pull/21053/files
- AoE IV: https://forums.ageofempires.com/t/ai-modding-in-aoe4/230269 ⚠

### 3.2 Techniques worth adopting

- **Layering.** Commander → squad → unit (Killzone 2 HTN bots:
  https://www.guerrilla-games.com/media/News/Files/VUA07_Verweij_Hierarchically-Layered-MP-Bot_System.pdf). EE
  already has this shape. What it lacks is a real top layer.
- **Utility AI / Infinite Axis** (Dave Mark, https://www.gameai.com/manuals/index.php/Response_Curve). Score
  candidate goals by multiplying response curves over normalised inputs. Candidate goals include: boom, advance
  epoch, expand, raid, all-in, wonder, defend, tech up. It is data-friendly and auto-tunable. **Best fit for the
  strategist.**
- **Opening books per civ and epoch,** then a reactive planner. Build-order search such as BOSS
  (https://www.cs.mun.ca/~dchurchill/publications/pdf/aiide11-bo.pdf) is optional later. Replanning after
  disruption is the hard part.
- **Resource arbitration** (0 A.D. queueManager). Each queue gets a priority and a share of the budget. This
  replaces the FIFO-per-epoch scheme.
- **Influence/threat maps** for expansion siting, attack targets and retreat. `EEInfluenceTable` exists and should
  be exposed and extended.
- **Cheap combat estimation** before committing to a fight. Stardust uses a FAP-style simulator:
  https://github.com/bmnielsen/Stardust . Puppet Search and Stratified Strategy Selection search over *script choice
  points*, not raw actions (https://ojs.aaai.org/index.php/AIIDE/article/view/12779 ,
  https://www.ijcai.org/proceedings/2017/522).
- **Scouting and opponent modelling.** Classify the enemy plan from scouted units and buildings, then pick a
  counter.
- **Difficulty as handicaps** (M28, AlphaStar APM caps, Queller's "style per level",
  https://www.moddb.com/mods/queller-ai):
  - reaction delay
  - an APM cap
  - perception limits
  - weaker books
  - mistake probabilities drawn from the seeded RNG

  Resource bonuses become an explicit, labelled option.

### 3.3 Competitions: what actually wins

- **Hand-coded modular bots.** These still win StarCraft BW and SC2 bot competitions: BananaBrain and Stardust at
  AIIDE 2023 (https://www.satirist.org/ai/starcraft/blog/archives/1227-AIIDE-2023-results.html), and python-sc2
  frameworks.
- **Deep RL.** Only won microRTS, using ~70 GPU-days on a tiny game (https://arxiv.org/abs/2402.08112v1).
- **AlphaStar, TStarBot-X, ROA-Star** need thousands of accelerator-days and a frozen ruleset.

### 3.4 LLMs

- **Live LLM agents are too slow for real-time play.** TextStarCraft II (https://arxiv.org/abs/2312.11865v3) beat
  SC2's built-in Harder AI, but only with the game slowed or paused per decision. SwarmBrain
  (https://arxiv.org/pdf/2401.17749) reports more than 20 s per LLM call.
- **They can't run in lockstep at all.** Output is not reproducible across peers. The only safe placement is one
  host injecting results as player commands.
- **The useful role is offline:** authoring and reviewing Lua strategies and build books.

### 3.5 Embedding a script language deterministically

**Factorio is the reference design** (https://lua-api.factorio.com/latest/auxiliary/libraries.html ,
https://lua-api.factorio.com/latest/auxiliary/storage.html , https://wiki.factorio.com/Desync). It is modified
Lua 5.2:

- `pairs`/`next` iterate in **insertion order**.
- `math.random` is seeded from the map; `math.randomseed` is a no-op.
- Custom cross-platform trig, exp and log.
- `coroutine`, `io`, `os`, `loadfile` and binary chunks are removed.
- **Only the `storage` table persists**: no functions, and writing to it in `on_load` is an error.
- A "heavy mode" saves and reloads every tick to catch hidden state.

| Runtime | Determinism | Budget / interrupt | Sandbox | Rust path | Notes |
|---|---|---|---|---|---|
| **PUC Lua 5.4** | Good with patches: ordered `next`, own RNG and transcendentals; native int64 subtype | `lua_sethook` count hook (deterministic) | Manual whitelisting + custom allocator | mlua | Factorio-proven approach; ~25k lines of C, trivial to vendor |
| **Luau** | Good. No `__gc`, so less GC-observable behaviour. Iteration order needs checking (Q4) | Interrupt at calls and back-edges; drive it by a count, not a clock | Best out of the box (read-only builtins, no bytecode load) | mlua (`luau` feature) | Typed (gradual), friendlier for modders; C++ library |
| LuaJIT | Weaker: hooks ignored in compiled loops; stuck on 5.1 | Unreliable | Manual | mlua | Speed not needed at 1 Hz |
| wasmtime | Best: deterministic **fuel**, NaN canonicalisation | Fuel | Strong | Native | Heavy; poor for modders and hot reload |
| QuickJS / SpiderMonkey | OK | | | | 0 A.D. route; big dependency, hand serialisation |

Sources: https://luau.org/sandbox , https://luajit.org/faq.html ,
https://docs.wasmtime.dev/examples-deterministic-wasm-execution.html , https://docs.rs/crate/mlua/latest .
Persisting coroutines (Eris) only works for Lua 5.2/5.3 and needs care: https://github.com/fnuecke/eris .

---

## 4. Design options

| Option | What a script controls | Effort | Risk | Value |
|---|---|---|---|---|
| A. Data only | Build books, compositions, timings, utility curves, personalities as YAML through the eedata pipeline | L-M | Very low | Medium: fixes variety, enables modding |
| **B. Script strategist over the request layer** | Goals, budget split, target and composition choice, attack timing. C++ tactics execute | M | Low-medium | **High** |
| C. Script everything above `.tai` | Strategist + group tactics in Lua; C++ keeps pathing | H | High: re-implements 20k lines of tactics, perf at 16 players | High, but late |
| D. Full replacement via unit primitives | Everything | Very H | Very high | Not justified |
| E. Live ML/LLM | n/a | n/a | Breaks lockstep | No |

**Recommendation: A then B.** Leave C open per subsystem. Unit micro (`ASGU*`) stays C++, but its literals become
data.

### 4.1 Shape of option B

```
EEComputerPlayer::Update (1 Hz per CP, in the PT, every peer)
  ├─ perception snapshot (C++)   fog-honest unless `cheat.vision`; per-player; batched queries
  ├─ strategist.lua  step(ctx)   pure function of (snapshot, storage, rng) -> list of requests
  │                              instruction budget per step; errors -> deterministic fallback to C++ default
  └─ request executor (C++)      validates like EEMessage::Execute, then calls the existing manager APIs:
        build(class, near=town|pos)       train(class, n, town)      research(tech)
        set_budget(queue, share)          advance_epoch()
        attack(target_player|area, comp)  defend(area)  raid(...)    retreat(group)
        set_behavior(field, value)        set_favorites(domain, class)
```

Rules:

- **Requests, not unit commands.** Group FSMs, transports, formations and `.tai` stay C++. Every request is
  validated, and an invalid request is a logged no-op, never a crash.
- **Plain-data state.** All script state lives in `storage`, a tree of numbers, strings and tables, saved with the
  CP under a new save minor version. No coroutines and no closures in state. Hot reload means re-running the chunk
  and keeping `storage`.
- **Determinism.**
  - Integers for anything compared or stored. Floats are allowed for transient scoring only, and must be rounded to
    a fixed precision before being compared or stored.
  - Ordered iteration.
  - A per-player RNG stream seeded from the game seed and player id, not `gPhysics` (§2.5).
  - Count-based budget, never time-based.
- **Perception tiers** are the honest difficulty lever:
  - `fog` (default): only scouted, and remembered with age.
  - `explored`: today's map reveal.
  - `omniscient`: today's intelligence.

  These are the first things to switch off as part of B.
- **Personality** is a script plus a data table. Each one has an opening book per epoch range, utility weights, an
  aggression and risk curve, favoured domains and a mistake rate.
- **Default.** Port today's C++ decision logic into `default.lua` first, so that switching to scripts changes
  nothing measurable. Then improve it.

### 4.2 Runtime choice

**Lua 5.4 (PUC), patched the Factorio way**, is the default recommendation:

- It is small to vendor (it fits the `tools/units.py` `LIB_UNITS` pattern, like miniaudio).
- It has a deterministic count hook and native integers.
- mlua makes it portable to Rust.

**Luau** is the alternative if modder ergonomics (types, the sandbox) matter more than vendoring size. Decide after
the Q4 iteration-order check. LuaJIT and WASM are not recommended.

---

## 5. Staged plan

Every stage ends on the same gate: the harness plays the same seeded AI game twice, and the AI decision trace must
be identical. From stage 2 on, the gate also includes a behavioural benchmark result. Each stage delivers value on
its own, so the work can stop after any of them.

**Ordering.** The default order is 0 → 1 → 2 → 3 → 4.
- If the goal is a better opponent soon, run stage 2 before stage 1: it gives the fastest route to a fair, stronger
  AI.
- Stage 1 first is better for modding.
- Stage 3 depends on stage 2. A script strategist on top of a free economy and full map knowledge would learn
  nothing.

### 5.0 Working environment (read first on a new machine)

- **Build:** `python -I build.py`. The test profile for the harness is
  `python -I build.py --test-profile windowed-software`, which produces `build/clang-test/exe/EE-AOC.exe`.
- **The full harness is x86_64 Windows only** (`tools/harness/README.md`). It needs:
  - both original installs (Art of Conquest and Empire Earth), read only: `tools/open_data.py` builds the data tree
    from them;
  - MSYS2 `mingw-w64-x86_64-gcc` at `C:\msys64\mingw64`;
  - `pip install pefile capstone`.

  Self-test: `python -I tools/harness/selftest/selftest.py` (about 1 minute).
- **A seeded, accelerated, headless AI game:**
  ```
  python -I tools/harness/game_session.py --exercise ai-game --ai-players 2 --map-size Small --difficulty Hard \
         --max-game-seconds 600 --time-factor 32 --no-render --seed 2 --pt-rate 40 --timeout 2400000
  ```
- **Linux and macOS:** `tools/harness/posix_game.py --exercise ai-game` runs in real time only (no time shim), so
  it is too slow for a benchmark. Use it for smoke tests.
- **Not ported yet, and needed by this plan:**
  - `differential/`: per-PT hashes;
  - `game_batch.py`: batches of games, which the benchmark needs;
  - `save_load.py`: needed for the stage 3 save gate.

  All three still name i686 symbols and VC6 addresses.
- **AI trace output** goes through `ESDebugOut`. Existing gates already compare its epoch and research timing lines
  (PORT_LEARNINGS.md:719-721).
- **Where things are:**
  - AI code: `src/Empire Earth/AI Thread/`.
  - Tuning table: `data/Data/db/dbcpbehavior.csv`, with the record ids in `include/ee/EECPBehavior.h`.
  - `.tai` scripts: `data/Data/Unit AI Scripts/`.
  - Scenario AI effects: `src/Empire Earth/Scenario Manager/EESMEffect.cpp`.

### Stage 0: measure (no behaviour change)

**Goal:** know how good the AI really is, and be able to see why it does what it does. **Size:** small.

1. **Confirm the cheats at runtime (Q1).**
   - Log `ScaleCost(100)` per difficulty. The expected result is about 1.
   - Confirm the map reveal (`ExploreUnseen`, `EEComputerPlayer.cpp:5568`) and the free epochs
     (`EEComputerPlayer::Pay`, :5438).
   - Check whether the lobby or `SetAIVariable` ever rewrites rows 67/96.
2. **A structured decision trace.** One `ESDebugOut` line per AI decision:
   - CP id, game time, manager;
   - the request (build/train/research/attack/defend + class or target);
   - the reason (which rule or literal fired);
   - the resources at the time.

   Start with `EECPBuildManager::StartBuilding`, `EECPUnitManager::Update`, the `EECPResearchManager` queueing,
   `EECPCombatManager::Update` (target choice) and `AttackCommander::CreateGroup`. Keep it behind a test-profile
   switch so it costs nothing in normal builds.
3. **An AI-vs-AI benchmark.**
   - Port `game_batch.py`, or write a thin loop over `game_session.py --exercise ai-game`.
   - Run a matrix of seeds × map types × difficulties × player counts.
   - Per game, record:
     - the winner and the game length;
     - epoch reach times;
     - army value and worker count over time;
     - unspent resources over time;
     - idle workers;
     - population blocks.
   - Write CSV/JSON under `build/boot/` and add a small summary script.
4. **An honest baseline.** Rerun the benchmark with the cheats off:
   - rows 67/96 set to 100;
   - the map reveal and the free epochs disabled through a test-profile switch, or the scenario "Cheat" toggle.

   **This is the number that every later stage has to beat.** Expect it to be bad: the economy logic has never run
   under real scarcity (§2.4).
5. **Fix the latent sync risks:**
   - the `gGameScreenActive` reads (`EECPPersonnel.cpp:528, 551`, `EECPCombatManager.cpp:1094`);
   - the address-ordered `std::set<EEAIPathArea*>` (`EEAIAStarPathAreaGoal.cpp:282`), which should be keyed by an
     id.

   Each fix must keep the decision trace identical in single-player.
6. **Profile (Q3).** Measure the AI's share of a PT with 15 CPs, to size the script budgets in stage 3.

**Gate:** the trace is identical across two runs of the same seed. The benchmark and baseline numbers are recorded
in this document.

**Players notice:** nothing.

### Stage 1: decisions and cheats as data

**Goal:** change and vary the AI without recompiling. **Size:** medium.

1. **Move the literals into data** (`ai/*.yaml` through the eedata pipeline, MODDING_PLAN §3). The literals are
   inventoried in MODDING_PLAN §G:
   - epoch→unit tables (anti-air, fishing boat, gates, towers; `EECPCivilizationManager.cpp:543-660`);
   - the 19 building kinds and the building quotas (:90-146, :413-463);
   - the default favourite units (`EEComputerPlayer.cpp:1095-1098`);
   - the civ-by-epoch table (:4602-4660);
   - the unit selector domain shares and counter weights (`EECPUnitSelector.cpp:958-1074, 1214`);
   - the target-scoring weights (`EECPAttackSubGroupUnit.cpp:282-298`);
   - the manager timers.

   The C++ reads the data. The shipped data reproduces today's values exactly.
2. **Personalities.**
   - A personality record is a named set of overrides on `dbcpbehavior` plus the stage-1 tables. It covers
     aggression, preferred domains, expansion habit, wall use and wonder interest.
   - A personality can be selected in the lobby per AI slot, or chosen at random by default from the deterministic
     RNG.
   - Ship 4-6 to start, for example Boomer, Rusher, Turtle, Naval, Wonder, Balanced.
3. **One switch per cheat**, as data per difficulty or personality:
   - `cost_modifier`
   - `reveal_map`
   - `omniscient_intel`
   - `free_epochs`
   - `pop_rubber_band` (`WorkOutPopulation`)
   - `epoch_catch_up` (`CatchUpEpoch`)
   - `retire_unseen_obsolete`

   The defaults equal today's behaviour.

**Gate:** with the default data, the trace is identical to stage 0. The personalities measurably differ on the
benchmark: unit mix, timings, first-attack time.

**Players notice:** AIs that differ from each other. Modders can make AIs for the first time.

### Stage 2: a fair economy and honest perception (C++)

**Goal:** an AI that is competitive without cheats. **This is the stage that matters most.** **Size:** medium-large.

1. **Resource arbitration.**
   - A queue manager sits in front of the build, unit and research FIFOs, in the style of 0 A.D.'s queueManager.
   - Each queue gets a priority and a budget share that the strategy sets: boom favours workers and economy
     buildings, war favours military, tech favours research and the epoch.
   - Requests reserve resources, so they no longer race each other through `CanAfford`.
2. **Economy fixes found by the honest baseline.** Work through the stage 0 metrics:
   - worker allocation per resource (`EECPResourceManager`), and idle workers;
   - drop-off distance and new drop sites;
   - population/housing blocks;
   - expansion timing;
   - epoch advance timing now that epochs cost resources.
3. **A perception module.**
   - A per-CP memory of enemy units and buildings that the CP has actually seen, with last-seen time, position and
     class. It is fed by `IsSeenBy` checks during the CP update.
   - `EECPIntelligence` and the combat target choice read this memory instead of walking `mFirstUnit` lists when
     `omniscient_intel` is off.
   - Memory is saved with the CP under a new save minor version.
4. **Scouting.**
   - The exploration manager gets goals: find each enemy town, re-scout every N minutes, check the expansion sites.
   - Scouting results feed the perception module.
   - An early-scout unit is part of every opening.
5. **Threat assessment.** Extend `EEInfluenceTable` with remembered enemy strength, used for defence triggers,
   expansion siting and retreat decisions.

**Gate:** a Hard AI with every cheat off beats today's cheating Medium AI on the benchmark, at a recorded win rate.
The cheat switches still reproduce the old behaviour exactly.

**Players notice:** an AI that plays fair and is still a real opponent.

### Stage 3: Lua strategist

**Goal:** the top-level decisions become a script. C++ keeps tactics, pathfinding and unit control (§4.1).
**Size:** large.

1. **Runtime (§4.2).**
   - Vendor PUC Lua 5.4 under `src/ThirdParty/lua`, added like miniaudio through `LIB_UNITS`/`LIB_MEMBERS` in
     `tools/units.py`.
   - Patch it the Factorio way:
     - `next`/`pairs` iterate in insertion order;
     - `math.random` uses a per-player stream seeded from the game seed and player id, never `gPhysics`;
     - own `sin`/`cos`/`exp`/`log`/`sqrt`;
     - `os`, `io`, `coroutine`, `load` of binary chunks and `dofile` removed;
     - `string.format("%p")` and `tostring(table)` made address-free.
   - A count hook gives a fixed instruction budget per step, and a custom allocator gives a memory cap.
   - Answer Q4 first if Luau is still under consideration.
2. **The query API: a read-only snapshot**, built in C++ and batched once per CP update. It contains:
   - own economy: resources, workers by task, population, buildings, queues;
   - own army by class and group;
   - known enemies from the perception module (or omniscient when that cheat is on);
   - map summary: towns, continents, choke points, expansion sites;
   - epoch and tech state;
   - game settings.

   No live engine pointers ever go to Lua.
3. **The request API.**
   - `step(ctx)` returns a list of requests:
     - `build`, `train`, `research`, `advance_epoch`, `set_budget`;
     - `attack(target, composition)`, `defend(area)`, `raid`, `retreat(group)`;
     - `set_behavior(field, value)`, `set_favorites`.
   - C++ validates each request the way `EEMessage::Execute` does for human commands, then calls the existing
     manager APIs: `QueueBuilding`, `RequestUnits`, `ResearchUnitClass`, `AddAttackTask`,
     `CreateGroupFromStrategy`. These are the APIs the scenario AI effects use.
   - An invalid request is a logged no-op.
4. **State and saves.**
   - All script state lives in one `storage` table of plain data: numbers, strings and tables, with no functions.
   - It is saved with the CP under a new save minor version.
   - Hot reload re-runs the chunk and keeps `storage`.
5. **Failure handling.** A script error or a budget overrun hands that CP back to the C++ decision code
   deterministically, on every peer. The event is logged.
6. **`default.lua`** ports today's decisions. **Gate:** its benchmark statistics match the C++ path within noise.
7. **`utility.lua`:**
   - it scores goals (boom, rush, tech, expand, wonder, defend, raid) with response curves over the snapshot;
   - it uses opening books per civ and epoch range;
   - personalities become curve and weight sets.

   **Gate:** it beats `default.lua` and the stage 2 C++ AI on the benchmark.
8. **Harness additions:**
   - port `save_load.py`, then save and reload every N game seconds and require an identical trace;
   - a fuzz test that loads broken or hostile scripts and requires a clean fallback;
   - a two-run determinism gate with scripts on.
9. **Scenario hooks.** Scenario authors can attach a strategist script per player. The existing AI effects are
   also callable from Lua.

**Players notice:** stronger and more varied AIs. Modders can rewrite strategy entirely.

### Stage 4: tuning and difficulty

**Goal:** strong at the top and fair at every level. **Size:** medium, then ongoing.

1. **Self-play tuning.**
   - Run CMA-ES or a GA over the personality and utility parameters on the headless benchmark (Windows harness,
     `--time-factor 32 --no-render`).
   - Ship the results as data. There is no runtime ML.
2. **Difficulty by handicap.** Each level is a set of:
   - reaction delay (requests applied N seconds late);
   - a requests-per-minute cap;
   - a perception tier (`fog` / `explored` / `omniscient`);
   - opening book quality;
   - a seeded mistake rate (bad target choice, late epoch, under-defended expansion).

   Cheating levels stay available under an explicit "Cheating" label.
3. **Optional: a fight estimator.** `estimate_fight(our_group, known_enemy)` in C++ (in the style of Stardust's FAP
   simulator), exposed to scripts, so the AI attacks only when it should win and retreats otherwise.
4. **Optional: offline LLM help.** Use an LLM to draft and review personality scripts and opening books. The
   benchmark stays the judge.

**Gate:** a monotonic win-rate ladder across difficulties on the benchmark. The top non-cheating level beats the
original Hard AI.

### Out of scope

- Rewriting pathfinding or the continent manager: they are shared with all units.
- Moving the group tactics (`AttackGroup`/`ASG*`/`ASGU*`) into Lua. Revisit per subsystem after stage 3.
- Live LLM or RL control: it cannot be deterministic across peers (§3.4).

## 6. Open questions

- **Q1.** Confirm the ~1% cost at runtime: log `ScaleCost(100)` per difficulty. Also check whether the lobby or
  `SetAIVariable` ever rewrites rows 67/96. The code and data agree on 1%, which matches the game's reputation.
  - Also measure how a no-discount AI performs (rows 67/96 set to 100, or the "Cheat" toggle off) on the benchmark.
    That gives the honest baseline that stage 2 has to beat.
- **Q2.** Does the out-of-sync checksum cover the AI-only state (manager queues)? If not, add an AI hash to the
  per-PT differential.
- **Q3.** How much of each PT does the AI take with 15 CPs? Profile it before setting script budgets.
- **Q4.** Is Luau's table iteration order deterministic across runs and platforms? Read `lvmexecute`/`ltable` or
  test it. It decides between Luau and PUC Lua.
- **Q5.** Should the script be allowed to issue unit-level commands for special cases (heroes, prophets,
  calamities), or should those stay C++ with exposed parameters? The recommendation is the latter until stage 3 is
  stable.
- **Q6.** Is multiplayer with AIs between peers of this port in scope soon? If yes, stage 0 step 5 and the save-reload
  harness move earlier.

## 7. Sources

External sources are inline in §3 and §8.4. The code references were gathered on 2026-11-05 against `main` at
`6cd4a34`.

## 8. Stage 0 results

### 8.1 What exists

- **Decision trace** (`include/ee/EECPTrace.h`, `src/Empire Earth/AI Thread/EECPTrace.cpp`). `EE_AI_TRACE=1` in the
  environment turns it on; it costs one bool test per site when off. One `EE-AI` debug string per decision:
  ```
  EE-AI <game ms> p<player> <manager> <event> key=value ... | f= w= s= g= i=
  ```
  - events: `start` (difficulty, cheats, `ScaleCost(100)`, extra resources, aggression), `build.queue`,
    `build.start`, `train.request`, `research.request`, `research.start`, `epoch.advance`, `attack.group`, and
    `stats` every 30 game s (epoch, population, pop cap, game limit, citizens, idle citizens, buildings, other
    units);
  - `<manager>` is the manager running when the decision was made (`civ`, `unit`, `build`, `research`,
    `resource`, `wall`, `combat`, `explore`, `cp`);
  - `EE-AIPROF` lines hold the real time per CP update (every 60 game s); they vary per run and are never compared.
- **Cheat switches.** `EE_AI_CHEATS` lists the cheats that stay on: `cost`, `reveal`, `epochs`, `extra`, `popband`,
  `catchup`, `retire`, or `none`. Unset means all of them, which is the original behaviour. This is TEST ONLY: an
  environment variable is per machine, so stage 1 moves the switches into the rules data. `omniscient_intel` has no
  switch yet, because turning it off needs the stage 2 perception module.
- **Tools:**
  - `tools/harness/ai_trace.py extract|compare|summary`: `compare` cuts both traces at the earlier run's end;
  - `tools/harness/ai_bench.py`: a matrix of seeded headless games with the trace on, writing
    `build/boot/ai_bench/<name>/results.{json,csv}`; `--repeat 2` compares the traces.

### 8.2 Checks done

- **Q1, the cost cheat: confirmed at runtime.** The `start` line on Hard prints `cheating=1 cost100=1`, and the first
  building costs the AI 2 wood. With `EE_AI_CHEATS=none` it prints `cost100=100`.
- **Determinism gate: passes.** Seeds 2 and 3, Hard, Small, 2 AIs, cheats `all` and `none`, two runs each: all four
  pairs print identical traces (144-262 lines in the compared window).
- **Latent sync risks (stage 0 step 5): neither is a risk, so the code is unchanged.**
  - `gGameScreenActive` is world state. It is set true under the world write lock when loading starts
    (`EEUserInterface::HandleMessage` 0x407, `TestScenario`, `SaveAndTestScenario`, `RestartGame`), before any PT
    runs, and false only by `EEWorld` construction and the leave-map clear. Within a game it is constant on every
    peer. In the scenario editor it is what keeps the AI from drafting units.
  - The address-ordered `std::set<EEAIPathArea*>` in `EEAIAStarPathAreaGoal::AssignKindIDs` only decides the order in
    which continents receive *the same* `mNextKindID`. Each continent's id list comes out the same in any order.
- **Q3, profile: first numbers.** On this machine, one CP update takes a mean of 12-21 ms with the cheats on and
  38-42 ms with them off. The first update takes 0.8-8.4 s, which is the map analysis. **[inferred]** The no-cheat
  cost is probably exploration and placement failing over unexplored map. With 15 CPs, 40 ms each comes to 0.6 s
  of real time per game second, so a script budget has to be small next to the C++ that is already there.

### 8.3 First benchmark (2026-11-05)

Seeds 2 and 3, Hard, Small, Continental, 2 AIs plus the idle human. The games end at the harness time cap, at about
860-905 game s. Values are at the end of the run, per AI:

| Cheats | Citizens | Other units | Buildings | Unspent | Pop cap | Epoch advances |
|---|---|---|---|---|---|---|
| all | 36-52 | 5-17 | 9-21 | 3,100-3,600 | 112-115 | 0 |
| none | 28-49 | 4-6 | 8 | 820-930 | 560 | 0 |

Readings:
- **No AI advances an epoch in 15 minutes, cheats on or off.** Row 28 (`epoch_frequency`) sets the timer to 50-70
  minutes, scaled by difficulty (§8.4). The 15-minute gate game therefore measures only the first epoch. **The
  benchmark needs games of 60 minutes or more** (§8.5).
- **The pop-cap rubber band works against the AI in harness games.** The idle human has a handful of units, and
  rows 92/93 (weights 2.4 for the human, 0.6 for the AI's own limit) pull the cap down to about 112. Without
  `popband` the cap is 400 × 1.4 (Hard, row 98) = 560, which is not clamped to the game limit. Every AI-vs-AI result
  with an idle human is shaped by this, so the benchmark should run with `popband` off, or later with an observer
  slot.
- **Without the cheats the economy stalls.** Buildings stay at 8 (against 9-21), the military stays at 4-6 units,
  and 4 builds start in 15 minutes (against 13-17). Citizens are about the same. This is the scarcity weakness
  predicted in §2.4.

### 8.4 Prior work: the "Age of AI" mod

https://github.com/Grille/Empire-Earth-Age-of-AI (GPL-3.0, active, last update 2026-11-05). It is a DLL that hooks
the retail 1.04 exe in memory, and since 2026-11-05 also this port's `EE-AOC.exe`. It is configured by
`config/ageofai.ini` per difficulty and has `.eeai` personality files. Its notes (`docs/ai-analysis.md`,
`docs/engine-notes.md`, `docs/economy.md`, `docs/testing.md`, `docs/experiments.md`) are the best existing analysis
of this AI. **Use:** facts and ideas only. Its code and text are GPL-3.0 and are not copied here.

It agrees with this document on:
- the 1% cost cheat (stock 1 / 1 / 1.0, active only while the Cheat toggle is on);
- the map knowledge: every human unit has visibility floor 1, so the AI sees everything;
- the pop-cap formula (and its unclamped row 98 multiplier);
- the empty diplomacy manager.

Facts it adds, to check against this code as each stage gets to them:
- **Epoch timing.** Row 28 gives 50-70 minutes × difficulty, doubled when the AI is ahead of the humans and halved
  when it is behind. Shortening it hides the economy problems. They reverted their own row 28 change for that reason.
- **The economy without the cheat.** The AI spends about 14.5k food against 1.3k gathered in 20 minutes, never
  budgets finite wild food, and stalls in the Stone Age.
- **Food search.** It rejects berries as "enemy near" and aborts at the first unexplored site.
- **Mines.** Leftover citizens go to any mine on the map, including mines under enemy towers.
- **The build queue is blocked for the whole game** by an unplaceable "Single Player Settlement" (type 395), with
  about 1,000 failed placements per game.
- **The epoch timer decides, not the economy.** A requirement-tech request is never retried, and the fallback
  requests unplaceable statues.
- **Economy buildings.** Gathering upgrades are rarely researched, one granary is used all game, and fields are not
  rebuilt.
- **Military.**
  - No home army between attacks; siege is never taken along.
  - Routing ignores walls, walks through tower lines, and counts influence from unseen units.
  - Attacks continue on a player after peace or alliance.
- **Other.** Phantom "in production" units eat the pop cap; defeated AIs linger for 20-40 minutes; the scenario
  `Set AI Variable` case for row 96 writes the epoch-frequency fields.
- **Testing lessons.** Use many fixed-seed repeats with standard errors, and test both seats. A short epoch timer
  hides economy bugs. AI-vs-AI cannot test tower handling.
- **Their results.** On Hard, 60-minute 1v1 games:
  - their AI won 0 games against the cheating stock AI;
  - against the stock AI with the cheat off, it won Continental 22-6;
  - home guard: 87 → 272 soldiers trained per AI;
  - build-queue fix: 43 → 97 soldiers trained;
  - Stone Age reached at minutes 17-23 instead of 52-71.

  Two experiment results stand out: more soldiers alone gave more losses, and decisions per unit beat decisions per
  army.

Their fix list is effectively a ready-made backlog for stage 2 (§5). Each item should first be confirmed in this
source with the trace.

### 8.6 First 60-minute pair (seed 2, Hard, Small, 2 AIs)

| Cheats | End | Epoch advance requested (game s) | Epoch at end | Pop / cap at end | Unspent at end |
|---|---|---|---|---|---|
| all | time cap, 3,882 s | 2,771 and 3,093 | 1 and 1 | 51/112 and 54/112 | 6,773 and 7,572 |
| none | game end, 3,425 s: the idle human was defeated | 2,771 and 3,093 | 0 and 0 | 32/560 and 115/560 | 1,365 and 3,055 |

Readings:
- **The epoch timer, not the economy, sets the epoch time.** Both runs request the advance at exactly the same game
  times. With the cheats on, the advance is free and happens at 46 and 52 minutes. With them off, neither AI
  manages to pay for it: one is at food 0 when the game ends.
- **The cheating AI lives at about 50 population for the whole hour.** The rubber band holds the cap at 112, and
  its stock grows unspent to 7k. Without `popband`, one honest AI reaches 104 citizens.
- **A stuck build loop (stage 2 backlog).** In the cheating run, p2 orders a House at the same tile (20, 63) every
  4 s from 460 s to the end: 681 `build.start` events, never finished. Each order pays 1% of the cost again. This
  matches the class of bug that §8.4 reports for type 395. Next step: trace why that site never completes
  (`StartBuilding` → `EEMakeBuilding` → `EEUGBuildRepair`).
- **The idle human is not a neutral observer.** The AIs prefer human targets (§2.2), so the honest run ends when
  they defeat it. Benchmarks need `--no-human-defeat`, or an observer slot.

### 8.7 Stage 2 fixes so far (2026-11-05)

Benchmark: `ai_bench.py --jobs 6` (six 60-minute games in about 3 minutes), Hard, Small, 2 AIs, the idle human
kept in (`--no-human-defeat`). Each fix is its own commit on `new-ai`.

| Fix | Evidence | Effect |
|---|---|---|
| **Stuck build loop.** A stalled build is requeued without its site (`CancelStalledBuilds`). | `build.stalled ... blocker=37 owner=<self>`: 1,426 of 1,466 stalls were the AI's own dying foundation. | Stalls per AI 237 → 4 with the cheats on; buildings at 60 min 31 → 34 with the cheats, 15 → 18.5 without. |
| **Wasted build slots.** Up to 4 producible requests are tried per 3 s update, until one starts. | `build.nosite cls=47`: the Paleolithic guard tower found no site about 2,500 times per AI per hour, and used up the update each time. | Honest AIs: buildings 18.5 → 21.5, build starts 12 → 14. With the cheats on: unchanged. |
| **Honest epoch advances.** An unaffordable research stays queued (`TryResearch`); the epoch's cost counts in the needs; a pending epoch reserves its cost (`EEComputerPlayer::CanAfford`). | The original treated an unaffordable epoch research as started and dropped it, and `mAdvancingEpoch` then blocked every later attempt. Stone Age costs 1,122 food, and food never rose above about 70. | Honest AIs reaching the Stone Age within 60 minutes: 0 of 24 → 9 of 24. They reach it 6-12 minutes after the epoch timer asks (at 37-52 minutes). |
| **Pop cap clamp.** The AI's pop cap never exceeds the game's population limit. | Row 98 on Hard is 140%: 560 of 400 without `popband`. | Owner request. |

Also:
- **Cheats as rules data.** `gameoptions.yaml` difficulties take `ai_cheats` (DATA_FORMATS.md). A fair Hard is now a
  data row, and it is multiplayer-safe.
- **Q2 done.** The out-of-sync checksum now includes every player's resources and the AI's state.

**Determinism.** The trace has a "sim hash" line every 10 game s. Within 20 game minutes, same-seed pairs are
identical, including 4 parallel games. Over 40-60 minutes, about half of the pairs diverge at 25-27 game minutes,
also on the trace-only build. That is pre-existing, and the units, the RNG and the AI all diverge in the same 10 s.
Owner decision (2026-11-05): lockstep stays. Keep the current determinism, and fix old desyncs only when it is easy.

**Root causes found for the honest economy, still open:**
- **No farms before the Copper Age.** Farms and granaries are Copper Age, two epochs after the start. Until then,
  food comes only from wild sources, which run out. No honest AI queued a single granary in 60 minutes.
- **The needs ledger only grows.** The gatherer split follows `mNeeds`, which is the queued costs minus what has been
  gathered or started. It ignores the stock and counts costs twice. Food need goes from 12.6k to 21.6k over the
  game, so the split wants 85-90% of citizens on food.
- **Busy gatherers never move.** `AssignGatherers` places idle citizens only. Leftovers go to gold → iron → stone →
  wood in a fixed order, never to food. After an hour: iron 3,300 and gold 1,500 unused, food 15.
- **The epoch timer, not the economy, decides when to advance.** Row 28 sets it to 50-70 minutes.

### 8.8 Design input from the owner (2026-11-05)

- **Use what you hold.** The AI should always fully work its claimed and defended resources: granaries, farm plots,
  mines, woods near drop sites. The worker target should come from the capacity of those sites, not from pop cap ×
  citizen percentage (`GetWantedCitizens`). Food is sometimes needed for something more urgent than citizens, but
  saturation is the standing goal.
- **A maximum pop cap.** The AI never goes above the game's population limit (done, §8.7).
- **Personality as competing pressures.** Three pressures compete for resources: epoch up (tall), resource gathering
  and expansion (wide), and military buildup.
  - Each personality weights them.
  - **Epoch pressure rises with time**: since the last advance, and against the opponents' epochs.
  - This replaces the fixed epoch timer (row 28).
  - The epoch reserve of §8.7 is the first instance: how much of the reserve is enforced should come from the epoch
    pressure.
  - These are the utility weights of §4.1 and stage 3. They go in as data from stage 1 on.

### 8.10 Economy and epoch experiments (2026-11-05)

Each row is 6 to 12 sixty-minute games (seeds 2-7, Hard, 2 AIs), compared at fixed game times. End-of-game numbers
mislead here: in most honest games one AI wipes out the other.

| Build | Honest citizens at 20 / 30 / 40 min | Honest AIs in the Stone Age |
|---|---|---|
| epoch fix (§8.7), original gatherer split | 32 / 44 / 54 | 7 of 12, at 53 min |
| gatherer split by base shares and the epoch reserve | 26 / 31 / 34 | 3 of 12 |
| gatherer split by pending-cost shortfall | 29 / 31 / 41 | 6 of 12 |
| original split + capacity citizen target, rebalancing, leftovers to the most-needed resource (kept) | 33 / 46 / 55 | 7 of 12, at 55 min |
| + epoch pressure, full reserve from 30% of the timer | 26 / 28 / 34 | 9 of 12, at 28 min |
| + epoch pressure, reserve growing 30% → 80% (kept) | 30 / 36 / 40 | 8 of 12, at 37 min |

Readings:
- **The original food-heavy gatherer split is right for the first epochs.** Food is citizens, and citizens are
  growth. Both replacements were slower and are not used. A better split has to model the spending rate, not only
  the queues.
- **The capacity target, rebalancing and the leftover order are neutral today.** Food income binds first. They are
  the groundwork for "use what you hold" once farms exist (Copper Age).
- **The epoch timer is a tall/wide slider.** Two thresholds, where the request starts and where the reserve is full,
  trade growth against epoch speed. They become personality weights (§8.8).
- **Food income is the next bottleneck.** About 15 foragers make roughly 110 food per minute, and the Stone Age
  costs 1,122. Candidates: hunting and fishing, forage site choice (§8.4: berries rejected as "enemy near"), and
  drop-site placement.

### 8.9 Next

Done since §8.5: 60-minute multi-seed benchmarks (`--jobs`), the gatherer, placement and stall trace, Q2.

1. **Economy by capacity (§8.8, owner).**
   - The worker target is the capacity of the held resource sites.
   - The gatherer split is by shortfall against the stock and the reserves, not by the needs ledger.
   - A few busy gatherers move each cycle, so the split can rebalance.
   - Leftover citizens go to the most-needed resource, not the fixed gold → iron → stone → wood order.
   - Wild food is budgeted until farms exist.
2. **Epoch pressure instead of the fixed timer** (§8.8), with personality weights as data (stage 1).
3. **Combat:** the target choice trace in `EECPCombatManager::Update`, and why the honest games are so lopsided:
   one AI usually wipes out the other, and p2 wins far more often than p3.
4. **Still to port:** `save_load.py` (save gate) and `game_batch.py`, or use `ai_bench`.
5. **Before any merge:** `tools/gates.sh`, and the macOS/Linux builds.
