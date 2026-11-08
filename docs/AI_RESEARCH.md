Status: stages 0 and 1 done, and stage 2 under way on the `new-ai` branch. §8 covers the trace and benchmark, the
fixes and personalities as data, and the experiments, kept and dropped. Started 2026-11-05.
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

### 8.11 Lost builders and the stock correction (2026-11-05)

- **Lost builders.**
  - The trace (`food`, `builders` lines) showed 11 of 30 honest citizens at 20 minutes sitting on a finished building.
    They had a stale build goal and were waiting in the citizen script's `TaskSmartCitizen` state (action 4028).
  - The AI only takes citizens with no goal, or with a gather goal, so it never used them again.
  - Fix: `RebalanceGatherers` frees them every 10 s.
  - Effect: honest citizens at 20/30/40 min went from 30/36/40 to 35/39/45, and buildings at 40 min from 12 to 16.
- **Stock correction of the original split.**
  - A resource with more than 1,000 in stock gets half its share.
  - A resource with less than 150 gets at least a floor share: wood 25%, food 10%, the others 5%.
  - Effect: honest citizens went to 38/46/49, and the Stone Age went from 10 of 12 to 12 of 12 (at 37 min).

### 8.12 Personalities as data (stage 1, 2026-11-05)

**`aipersonalities.yaml`** is a new rules document (DATA_FORMATS.md).
- Per personality, it holds the epoch pressure (`start`, `full`) and the economy tuning of §8.10-8.11.
- `assign` names the personality each computer player takes.
- The defaults are `balanced` (today's tuning, assigned to everyone), `tall` and `wide`.
- `EE_AI_PERSONALITY` overrides the assignment (TEST ONLY), and `ai_bench --personality tall+wide` sets it.

Six 60-minute honest games per set (seeds 2-7, Hard):

| Personality | Citizens at 20 / 40 min | Stone Age | Copper Age | Head-to-head |
|---|---|---|---|---|
| tall (0.15 / 0.5) | 24 / 29 | 11 of 12, at 25.5 min | 2 of 12 | loses to wide 10 of 12 |
| balanced (0.3 / 0.8) | 38 / 49 | 12 of 12, at 36.7 min | 0 | |
| wide (0.6 / 1.0, citizens 80%) | 42 / 53 | 9 of 12, at 45.8 min | 0 | beats tall from either seat |

Readings:
- **The stage 1 gate is met:** the personalities differ measurably.
- **On a Small map, the tall rush does not pay.** Reaching the Stone Age 20 minutes earlier on half the workforce
  loses to the boom. Military pressure, the third slider, does not exist yet; that is the obvious next weight.
- **Seat asymmetry.** Over 89 honest games where both AIs had the same personality, p2 dominated in 39, p3 in 24,
  and 26 were close. Candidates: the round-robin update order (p2 always first) and the start positions. Head-to-head
  tests have to swap the seats.

### 8.13 Military pressure, first version (2026-11-05)

What the trace shows (`losses` lines, six honest 60-minute games):
- **Armies stay tiny.** Both AIs keep 10-30 military units while iron piles up to 1,400-2,200.
- **The loser bleeds citizens.** It loses 11-19 citizens per 5 minutes to raids of a few units, from about minute 20.
  The winner barely loses any.
- **Military units come only from attack groups and defense build-ups.** Nothing turns the stock into an army.

The new `military.standing_army` (per worker) is the third personality pressure. Every 30 game s the AI requests its
favourite land unit up to that target, within the original's military share of the pop cap.

| Set | Military at 20 / 30 / 40 min (balanced) | Result |
|---|---|---|
| before (pers-60m) | 7.3 / 11.2 / 12.6 | |
| standing army 0.25 | 7.8 / 11.7 / 15.4 | aggressive (0.5) against balanced: 6-6 over both seats |

**The effect is modest, because the army competes with citizens for food.** The unit manager trains requests in
order, and only when they are affordable, and a citizen request is almost always outstanding. Later in the game, once
iron and gold pile up, the standing army does use them; one AI had 97 units at 60 minutes. A real military pressure
needs arbitration between citizens, army and epoch: stage 2's queue manager (§5), with the personality weights
setting the shares.

### 8.14 Arbitration and defense: tried, not kept (2026-11-06)

Each experiment is 18 honest 60-minute games: seeds 2-7, sets `balanced`, `aggressive+balanced` and
`balanced+aggressive`. The code of both is in the session notes, not on the branch.

- **Citizen/army arbitration in `CanAfford`.** When both are below their targets, the side further behind keeps one
  unit's cost in reserve against the other.
  - Result: no change. Balanced armies at 20/30/40 min were 7.8/10.8/15.3, against 7.8/11.7/15.4.
  - Reason: food was not what blocked the army. Iron was (§8.16).
- **A defense reflex.** Military units near the town center attack raiders within 15 tiles (version A). Version B
  also covers the last attackers of citizens and buildings.
  - Result: citizens lost were the same (444 against 440). The balanced economy got worse: citizens at 40 min 38
    against 60.
  - Reason: pulling soldiers out of their groups weakens the AI's own attacks, and a home army of 8-10 units cannot
    beat a raiding group.
- **What the trace showed:**
  - Most citizen losses are far from the town center: more than 30 tiles for about 55% of them, under 15 tiles for
    about 25%.
  - Other players' units kill nearly all of them; wild animals kill 0-5 per game.

### 8.15 The army waits on iron (2026-11-06)

The `pending` trace line (every 30 s: the non-citizen requests, with count, affordable now, built producers, and the
production room) showed the cause for an aggressive AI:
- 33 clubmen pending at 30 minutes.
- Never affordable, with 450 food, 300 wood and 235 gold in stock.
- The clubman costs 30 food and 30 iron, and iron sat at 10-25.

The gatherer split gave iron about 17%, while food took about 70%. That share comes from food's need in the ledger:
- `EECPCivilizationManager::Update` adds every wanted citizen's cost once at the start (180 × 50 food).
- Each citizen request then adds its own cost again.

**Removing that lump collapsed the honest economy.** Citizens stuck at about 19, against 37-57. The double-counted
food is in effect the AI's citizen-growth drive, so it stays (reverted).

`in_production` (the phantom-unit theory of §8.4) stayed at 0-12 the whole game, so no phantom units build up here.

### 8.16 Military pressure, part 2: the army floor (2026-11-06)

`military.army_floor` (per personality): while the standing army is short of its target, the gatherer floor on what
its unit costs rises by up to this share. It scales with the shortfall and is split by the unit's cost.

| Set | Army at 20 / 30 / 40 min | Citizens at 20 / 30 / 40 min |
|---|---|---|
| balanced, no floor (36 AIs, seeds 2-19, 45 min) | 8.3 / 11.7 / 14.9 | 34.7 / 41.4 / 58.4 |
| balanced, floor 0.2 (same seeds) | 9.8 / 13.0 / 18.2 | 35.7 / 37.8 / 52.6 |

- **It is a pressure, not a free gain.** About 20% more army costs about 10% fewer citizens. Epoch reach and survival
  are within noise.
- **So it is a personality weight:** balanced, tall and wide 0.1, aggressive 0.3.
- **Aggressive against balanced** (seeds 2-13, both seats): aggressive wins 16, balanced 7, 1 close. Before the
  floor it was 6-6.

### 8.17 Seat asymmetry: not the update order (2026-11-06)

Balanced against balanced, seeds 2-19, 45 minutes, on the build of §8.16:

| Round robin | p2 wins | p3 wins | Close |
|---|---|---|---|
| normal order (p2 updates first) | 7 | 7 | 4 |
| reversed (p3 first; a TEST ONLY switch, not kept) | 8 | 5 | 5 |

- **The update order does not explain the seat advantage.** Reversing it did not move the advantage to p3.
- **There is no measurable seat effect left.** The 39-24 of §8.12 came from older builds, over several sets.
- **Head-to-head tests still swap the seats.**

### 8.18 Gatherers out of enemy land: tried, not kept (2026-11-06)

`FindResourceTarget` rejects sites within 10 tiles of another player's area, but its loop skips enemies. So it keeps
citizens off allied and neutral land and not off enemy land. Adding enemy areas, with a 15-tile margin, changed
nothing measurable.

| Build (balanced against balanced, seeds 2-19, 45 min) | Citizens lost per AI | Citizens at 40 min | Army at 40 min |
|---|---|---|---|
| before | 20.0 | 58.6 | 17.4 |
| with the enemy margin | 20.1 | 55.2 | 19.1 |

**Not kept.** The far deaths are raids that reach the AI's own outer resource centers, not trips into enemy land.

### 8.19 Honest against cheating (2026-11-06)

`EE_AI_CHEATS` now takes one set per player (`/` between sets, in turn from player 1, the idle human: `none/all` makes
p2 cheat and p3 honest; TEST ONLY). Balanced against balanced, Hard, Small, seeds 2-7, both seats, 45 minutes:

| Cheater's set | Cheater wins | Honest wins | Close | Notes |
|---|---|---|---|---|
| all (Hard's default) | 12 | 0 | 0 | every game over at 22-35 min; the honest AI stops gathering by 15 min |
| reveal, extra, popband, catchup, retire (no `cost`, no `epochs`) | 6 | 5 | 1 | all to the time cap; p2 won 7, p3 4 |

- **`cost` and `epochs` decide it.** On Hard, `cost` makes everything cost 1% (`cost100=1`), and `epochs` makes epoch
  advances free. The other five cheats together are worth about nothing against the current honest AI.
- **A fair Hard is close.** The honest AI holds even with a cheater that only sees the map and gets the resource
  bonus. Beating the full cheater would need about a hundredfold economy: not a goal. The goal is a fair Hard
  (`ai_cheats` without `cost` and `epochs`, or none) that plays as well as today's cheating Hard does against a human.
- **Both are slow.** At 45 minutes every AI in the second set was still in the Prehistoric or Stone Age (epoch 0
  or 1).

### 8.20 Settlements by the resources (2026-11-06)

The trace's `trips` line (every 30 s) gives, per resource, the mean distance from each gatherer's resource to the
nearest built drop site that takes it, and how many are past the drop-off distance (6 here), where
`BuildSettlementFor` orders a settlement. Honest balanced AIs, seeds 2-7, 45 minutes (12 AIs):

| Game min | Food | Wood | Stone | Gold | Iron | Settlements | Gatherers |
|---|---|---|---|---|---|---|---|
| 10 | 7.2 (39% far) | 4.3 (23%) | 8.6 (43%) | 8.8 (51%) | 7.5 (30%) | 2.9 | 29 |
| 20 | 8.0 (41%) | 3.9 (5%) | 4.5 (7%) | 7.4 (49%) | 8.3 (40%) | 4.3 | 37 |
| 40 | 8.8 (40%) | 4.4 (18%) | 5.1 (15%) | 7.1 (50%) | 8.2 (40%) | 5.6 | 46 |

- **The AI does build settlements by its resources.** There were 80 `settlement.order` events (about 7 per AI, all
  five resources), and about 5 settlements stand per AI by 30 minutes. `BuildSettlementFor` orders them straight from
  the gather goal, outside the build queue, so `build.queue` never shows them.
- **Wood and stone are close** (about 4-5, 5-15% far).
- **Food, gold and iron are not: 40-55% work past the drop-off distance.** The trips are not long (a mean of 7-9
  against 6). Candidates:
  - hunting is excluded;
  - one queued settlement per resource at a time;
  - no settlement within 0.75 of the radius of a town center.
- **No granary** was ever built: the AI did not reach the farm epochs.

### 8.21 The demand split (2026-11-06)

`EE_AI_DEMAND=1` (TEST ONLY; per player in turn with `/`, as `EE_AI_CHEATS`) replaces the needs ledger's gatherer
split (`EECPRMSplitByDemand`, `EECPResourceManager.cpp`). The AI's wants come in tiers, each a cost per resource:

1. **Growth:** the next 15 missing citizens, the standing army's shortfall (up to 15 of its favourite land unit), and
   the first two buildings of each epoch's build queue.
2. **The pending epoch advance,** by its pressure (as the reserve in `CanAfford`).
3. **The rest** (counted at half): the other unit requests, the queued research, and the rest of the build queues.

Each resource's shortfall is its demand less its stock. Each resource first gets half of the personality's floor
share. The rest of the gatherers go by shortfall, plus a quarter of the demand, capped by what the claimed resource
units hold, with the overflow passed on to resources that have room.

| Version (seeds 2-7, both AIs on it) | Citizens at 20 / 30 / 40 min | Stone Age |
|---|---|---|
| ledger (today) | 35.8 / 39.6 / 42.1 | 9/12 |
| v1: no floors, next 5 citizens | 26.2 / 24.4 / 29.8 | 8/12 |
| v2: full floors | 27.0 / 25.8 / 22.8 | 11/12 |
| v3: half floors, next 15 citizens | 30.3 / 23.8 / 25.2 | 9/12 |

- **v1 starved wood.** Nothing asks for wood: buildings outside the queues and upkeep never reach the demand. The
  floors bring back that base income.
- **Symmetric games mislead.** One side usually gets wiped out, so the means mix economy with combat.

**Head to head** (v3 against the ledger, balanced, honest, Hard, seeds 2-13, both seats; 24 games):

| | Demand | Ledger |
|---|---|---|
| Wins (close: 2) | **16** | 6 |
| Stone Age | 20/24 at 36 min | 13/24 at 38 min |
| Citizens / army at 20 min | 31.3 / 12.0 | 32.1 / 7.9 |
| Citizens / army at 40 min | 41.0 / 19.5 | 28.5 / 11.2 |
| Citizens lost by 20 min | 4.0 | 6.1 |

- **Same economy, more army, earlier epoch.** The demand split matches the ledger's citizens and is ahead on army and
  epoch at 20 minutes. The army is in tier 1, while the ledger let the army wait on iron (§8.15).
- **Seats were even:** p2 won 12 and p3 10.
- **Next:**
  - make it the default as data (an `economy` field in `aipersonalities.yaml`), with the tier weights per
    personality;
  - check it against `aggressive` (also an army push) and on the cheat sets;
  - look at the wanted gatherers that are never filled: iron wanted 6-7 with 9 sites listed and 0 gathering (seed 3).

### 8.22 Epoch pace, towers and raids (2026-11-06, EE-SIM)

The scorecard (`tools/ai_scorecard.py` over `tools/sim_bench.py` runs) now gives time to every epoch, from the sim's
`epoch_ms`. Test-only experiments go in behind `EE_AI_EXP` (per player, as `EE_AI_CHEATS`) and are tried head to
head with `--env-vary EE_AI_EXP=x/-,-/x`. Honest balanced AIs on the demand split, Hard, Small, 60 minutes: 18/24
reach the Stone Age at a median of 37 minutes, and none the Copper Age.

**Income.** The trace's `income` line counts what `EEComputerPlayer::AddResource` brings in, per resource.
- About 6 per gatherer per minute: food 6, wood 6.5, stone 4-5, gold and iron 4-5.
- `gatheract`: gatherers spend 80% in the gather action and 18% walking (`Advance`).
- The citizen's data (forage 5.2 s per unit, 15 carried) allows about 11.5 per minute at the bush. So, walking
  included, the rate is about what the game intends.
- What is small is the workforce: about 25-30 gatherers from 15 to 50 minutes.

**The epoch reserve.** Head to head, 48 games each:

| Experiment | Result | What happened |
|---|---|---|
| citizens exempt from the reserve | lost 7-37 | 2/48 reach the Stone Age; the food goes to citizens; armies collapse |
| exempt below 30 / 40 / 50 workers | lost 14-18 / 12-20 / 7-25 | more citizens, smaller armies, 2-3x the citizens lost |

The reserve is the right trade under AI-vs-AI pressure.

**Pressure timing.** A `--vary` sweep of `epoch_pressure` start 0 / 0.15 / 0.3 and full 0.4 / 0.6 / 0.8 (8 seeds):
- An earlier full brings the Stone Age forward: 21 minutes at start 0 / full 0.4, against 37 minutes.
- The cost is economy: citizens stay about 17 instead of about 30.
- The Copper Age stays out of reach in every cell (0-3/16).
- Its cost is 1,584 food, 844 gold and 844 iron, with 3-4 gatherers on each of gold and iron.

**Next for epoch pace:** gold and iron income before the Copper Age, through the demand model's tier 2 (the epoch
advance's cost) as soon as the Stone Age is in.

**Towers** (playtest: they stood too far out). `FindTowerSite` took the resource centres in list order and placed
by gold or iron only. Now:
- worked centres come first, nearest the town centre first;
- a worked centre's food counts too.

Head to head 19-22 (7 close). Median distance from the town centre was 10 tiles before and 9 after. In the sim,
44/58 towers stood within 15 tiles before and 51/61 after, so the playtest complaint may be about tower range rather
than distance.

**Raids** (playtest: citizens stayed on a patch a raider camped on). The experiment "flee":
- Trigger: an enemy player's unit hits a citizen.
- Effect: a 30 s danger zone. Gatherers within 8 tiles run home, and none are assigned within 10 tiles.

Head to head:

| Version | Result |
|---|---|
| v1: flee from any enemy soldier within 8 tiles | lost 12-33 |
| v2: zones from hits | lost 12-36 |
| v3: not within 12 tiles of the town centre | lost 17-28 |
| v4: at most 2 raiders | lost 17-28 |
| v5: v4 plus up to 4 free soldiers within 30 tiles go for the raider | 20-24 (3 close) |

- **Shipped:** v5, for raids by human players only. Against an AI attack group, running costs more work than it
  saves lives.
- **Testing:** the experiment "flee" turns it on against computer players.
- **Not measurable here:** the human case cannot be measured AI-vs-AI.

**Retaliation** (playtest: a clubman walking to its target took hits without answering). `Generic Attack.tai`'s
`UnderAttack` retaliates only while `UnitIdleOrAttackingDefenselessBuilding` holds. That condition now also holds
for a unit whose attack target is still out of reach when another unit hits it. Determinism holds (12/12 repeat
pairs).

### 8.23 Economy experiments: defence is the bottleneck (2026-11-06, EE-SIM)

Head to head, 48 games each, both seats (`--env-vary EE_AI_EXP=x/-,-/x`; `+` joins experiments in one set):

| Experiment | Result | Notes |
|---|---|---|
| `boom25` / `boom35`: until 25 workers or 12 min (35 or 15), citizens skip the epoch reserve and no standing army | lost 16-58 (both) | the same citizens at 10 minutes, then fewer soldiers and more citizens lost |
| `epochplan`: the next epoch's wood, stone, gold and iron count whole in tier 2 from the start of an epoch | 22-25, 90 min: 22-26 | at 40 min 6.1 gold and 5.9 iron gatherers against 5.2 and 3.4; Copper 18/48 at 73 min against 19/48 at 75 min |
| `payback`: citizens skip the reserve while the epoch timer has more than 8 minutes to run | lost 11-35 | citizens 37 against 33 at 20 min, then about 1 soldier, 63 citizens lost against 24, Stone Age at 47 min |

- **Early growth is capped by training.** Citizens train only at the Town Center or Capitol, 28 s each. The
  Prehistoric allows one of them, and the queue holds 6-8 for the first 8 minutes.
- **The stall comes at 15-20 minutes.** The AI holds 600-1,000 food with no citizen in training while it fills the
  Stone Age's 1,122.
- **Freeing that food for citizens loses.** It starves the army, and the opponent's raids then take more citizens
  than the extra ones are worth.
- **The Copper Age:** 19-29 of 48 AIs reach it, at about 75 minutes, about 40 minutes after the Stone Age.
- **`noattack`** (no attack groups of its own) is not a clean economy race. Retaliation still sends units: 40
  attack groups formed in a game with both sides on it.

**Next.** Make defence cheaper, so that a smaller army keeps the economy safe:
- towers and the raid response against computer players too (v5 of §8.22 was 20-24);
- a home army that guards the worked resource centres.

Then retry `payback` and the boom on top. A sparring opponent that does not rush (a complete `noattack`) and the
owner's playtests judge the economy changes the rush meta hides.

### 8.24 First playtest with the game log (2026-11-06)

The owner against one Medium AI (balanced, no cheats) on Continental, 51-91 minutes logged (the game log, `tools/game_log.py`):

- **The AI was half the owner's size before any fighting.** At 56 min: 48 citizens against 89. It reached the
  Stone Age at 57.9 min; the owner reached the Copper Age at 59.3 min and the Bronze Age at 83.1 min.
- **Food.** The advance took the AI's food, and the owner's raids on its food gatherers kept it near 0. It had no
  farms: they are Copper Age. Every soldier it queued stayed unaffordable, and its attack groups formed with 0 units.
- **No raid response.** Most of its citizens were lost within 15 tiles of the main town centre, to groups of more
  than 2. Those are exactly the hits that the §8.22 flee and raid response leave to the town defence, and the town
  had no soldiers.
- **Owner's notes:**
  - "Guard stance doesn't respond to being attacked."
  - "All buildings and citizens have to be destroyed for an AI to be out." At 90 min the AI had 0 citizens,
    0 soldiers and 3 buildings.
  - "Random barracks and temples, undefended, far away." The trace has barracks 33-35 tiles from the main town
    centre and a temple at 36.

Fixes, checked in EE-SIM (Small Hard, 2 AIs, seeds 2-13, 45 and 90 min, against the commit before):

- **Guard:** a unit on guard closes on the unit that hit it, then returns (`EEUnitAI.cpp` CanIMoveFreely). Guard's
  ranged units (`mRangedCanMoveFreely` 0) stood still under fire from beyond their range.
- **Production sites:** barracks, ranges and the like try the active resource centres nearest the main town centre
  first, not round robin. Ranges more than 20 tiles out: 6 of 25 in 90 minutes before, 0 of 25 after. Temples fill
  the centres within 20 tiles first. They still spread out once those centres are taken, because the original
  rule wants one per centre.
- **Defeat:** `gameoptions.yaml` `defeat_without_recovery` (default on). A player is out when it has no citizens,
  no other units, and no finished town centre or capitol with a citizen's cost in stock. Random maps only.
- **The beaten AI's dead end.** With few citizens left it spent its last food on macemen, then lost the citizens.
  With 10-15 food it could train nothing, and its town centres stood empty for 40-50 minutes. Below 12 workers it
  now trains citizens whatever the epoch reserve, and nothing else may spend a citizen's cost.
- **Strength at 45 min:** the same. Citizens 32.6 against 32.2 per AI, army 7.0 against 5.9. AIs collapsed to 0
  citizens: 4 against 7.

**Next:** the town defends itself without an army. Garrison the citizens, or pull them back, when an attack group
reaches the base.

### 8.25 The militia (2026-11-06, EE-SIM)

Garrisoning is no hiding place: `EEComplexUnit::GarrisonUnit` uses the citizen up. The town centre has no attack
of its own. A citizen is about a third of a clubman: 65 hit points and attack 5, against 135 and 8. So the town
defends itself with its citizens.

**The militia** (`EECPRMMilitia`; `aipersonalities.yaml` `military.militia`, on by default). When enemy soldiers
hit a citizen, the AI acts at most once every 4 s:
- it takes the attackers within 10 tiles;
- it calls the soldiers within 30 tiles that are not attacking already;
- it adds the nearest citizens within 10 tiles, as many as needed for 1.5 times the attackers' strength
  (strength = summed hit points x summed attack);
- if that much can be raised, they fight, each defender taking the nearest attacker;
- if not, the citizens run: to the town centre, or, when the attack is there, to the drop site farthest from it.
  The spot becomes a §8.22 danger zone.

It replaces the §8.22 flee and raid response, and it acts against computer attackers too.
`EE_AI_EXP=nomilitia` turns it off.

Head to head, both seats:

| Version | Seeds | Result | Citizens lost (all / by 20 min) |
|---|---|---|---|
| v1: every citizen within 10 tiles fights | 2-25 | lost 18-25, 5 close | 13.5 against 17.6 / 1.5 against 4.8 |
| v2: soldiers first, then only the citizens the odds need | 2-25 | won 29-16, 3 close | 14.3 against 24.5 / 2.0 against 5.9 |
| v2 | 26-49 | won 25-14, 9 close | 11.9 against 23.6 / 1.9 against 4.5 |

- **v1 fought 2,151 times in 48 games:** whole work crews chased single scouts and lost the gathering time.
- **v2 at 40 min:** 42.7-45.0 citizens and 9.8-11.2 soldiers, against 29.4-33.4 and 4.6-6.1.
- **v2 reaches the Stone Age more often:** 36 of 48 and 41 of 48 players, against 31 and 31.

Defence was the bottleneck (§8.23). Next: retry `payback` and the boom with the militia on.

### 8.26 The boom (2026-11-07, full-game playtest and EE-SIM)

**The playtest** (Continental, Medium, Standard - High resources, 104 minutes, logged from minute 0). The AI
(Carthage) led for half an hour: 60 citizens and 27 soldiers at 30 min, against the owner's 38 and 6. It then
starved:
- **No fish.** `mUsesFishing` holds only on island maps, so on a continent its food before the Copper Age farms was
  bushes and the first animals: 16-24 citizens on food at most, then 1 once the bushes near home were gone or under
  the owner's towers. Food per gatherer was the same as the owner's.
- **31 minutes lost on the Stone Age.** It asked for the advance at 26 min with 19 food and paid for it at 57.
  Meanwhile the food went to citizens and soldiers, and the citizens it could not put on food went to wood and gold
  (17 on wood where it wanted 8; 1,370 gold unspent).
- **The militia ran at home.** 22-36 citizens met 13 raiders at odds 0.7-1.3, short of 1.5. Every check said run,
  past the raiders to the far drop site: 17 citizens died in 2 minutes.
- **The army melted away.** 27 soldiers were down to 0 by minute 42; attack plans went on with `units=0`, and new
  soldiers left one by one.

The owner's notes on play:
- Prehistoric and Stone live on food, with wood and iron behind; gold serves the two ranged units and the Copper Age
  (2-3 citizens on gold, about 6 on iron in the Prehistoric).
- The Prehistoric army is a small mobile defence force, sacrificed when the Stone Age units come.
- A boom player who scouts a rush scrambles and builds an army as needed; rush beats a boom taken too far.
- Towers are strong until the Bronze Age (no siege), so a boom needs towers, and raiders avoid crowded towers.
- One or two docks.

**Three rules** (`aipersonalities.yaml`; on by default, off in `aggressive` for the Prehistoric army):
- `economy.coast_fishing` (`EECPRMCoastBoatsWanted`, `EECPRMRequestBoats`): from the Stone Age (the first fishing
  boat), the fish within 30 tiles of the town centre count 2 boats each, 8 at most, in the food capacity. A dock is
  queued while there is no built one; boats are requested one at a time up to the count; farms stay (the island rule
  gives them up); the build lists' docks are capped at 2 (they built up to 6). `EE_AI_EXP=nofish` turns it off.
- `military.militia_towers` (`EECPRMMilitia`): the built towers within 12 tiles of the attack count in the militia's
  odds, and citizens that cannot fight run to the nearest tower within 30 tiles. `nomtower`.
- `military.prehistoric_army` (8; `EEComputerPlayer::CanAfford`): in the Prehistoric, no soldier is affordable while
  the player has that many, unless enemy soldiers within 40 tiles of the town centre are stronger (hit points x
  attack) than its army; then the cap is off for 4 minutes (trace `threat`). `noprearmy`.

`EECPUnitManager::CountMilitaryUnits(false, false)` returns 0 (it misses the units the combat manager holds), so the
cap counts the soldiers directly. The demand split's standing-army shortfall uses that same count, so it always plans
for the whole target; left as it is for now.

Head to head against the AI without them, seeds 2-13, both seats, Medium, Small:

| Variant | Length | Result | Stone (median) | Copper | Citizens / army at 40 min |
|---|---|---|---|---|---|
| the citizen cap alone (30 Prehistoric, 50 Stone) | 90 min | lost 3-17 | 45 min | 3/24 | 31 / 16 |
| fishing | 90 min | won 14-8 | 50 min | 5/24 | 57 / 17 |
| the tower militia | 90 min | 9-10 | 50 min | 2/24 | 53 / 16 |
| the army cap | 90 min | 9-9 | 49 min | 1/24 | 60 / 9 |
| all three (the boom) | 90 min | **won 12-7** | 50 min | 3/24 | 56 / 9 |
| the boom + citizen cap (Prehistoric 30) | 90 min | 10-11 | **36 min** | 11/24 | 35 / 9 |
| the boom | 150 min | **won 17-7**, none close | 50 min | 17/24 | Bronze 10/24 against 2/24 |
| the boom + citizen cap (Prehistoric 30) | 150 min | 10-11 | 36 min | 17/24 at 83 min | Bronze 10/24 at 123 min |

- **The citizen cap** (experiments `citcap`, `citpre`, `citpre40`, TEST ONLY) reaches the epochs much earlier but
  stalls after them (the next epoch's reserve holds the food), and breaks even at best. Left out.
- **"Citizens lost" counts fishing boats** with the citizens: 65-130 in the fishing variants.

All three are on by default; the game log of the next playtest decides what comes next. Open:
- the gold and iron supply for the Copper Age (a capped AI sat on 3,000 food with no gold coming in);
- the standing-army count above;
- the Stone Age pace (the citizen cap shows 13 minutes are there).

### 8.27 Food, gold and the Stone Age (2026-11-07, second full-game playtest and EE-SIM)

**The playtest** (Continental, Medium, the AI Babylon, 106 min). The AI kept pace for 16 minutes (31 citizens and 7
soldiers against the owner's 30 and 5), fished, and its citizens' response to raids was "perfect". It still reached
the Stone Age at 55 min (the owner: 21) and never left it:
- **Food capacity.** `GetGatherCapacity` added every food gatherer to the food capacity (meant for farm and fish
  workers, which are not resource-centre units), so the capacity grew with each one sent: 23-28 citizens on 1-2
  forage patches of about 6 places each, queueing or walking to far bushes (mean trip 18 tiles). Food came in at
  about 2 a gatherer per 30 s, the owner's at 3.6-4.2.
- **Gold.** It settled gold once, at 7 min. From 34 min no active centre held gold, so it wanted no gold gatherers
  and none ever ordered a settlement: gold stayed at 11-26. The Copper reserve then held the food and iron for an
  advance it could not pay, and it trained almost no soldiers (0-8): "no military response".
- **Warships.** The owner's single battleship sank its fishing; boats and units under fire stayed.

The owner's play, as the target: a settlement right on the patch, 6 citizens a patch, the rest hunting, then many
fishing boats; in the Prehistoric only citizens, about 4 clubmen unless attacked and towers, no attacks. Food per
30 s (the game log's `got`; the gatherer counts include boats):

| Minutes | Owner | Per gatherer | AI (playtest) | Per gatherer |
|---|---|---|---|---|
| 5-10 | 48 | 3.0 | 30 | 2.6 |
| 10-20 | 65-68 | 3.6-3.8 | 37 | 1.7-2.5 |
| 30-60 | 101-109 | 3.7-4.0 | 35-59 | 1.7-2.2 |

**The rules** (`aipersonalities.yaml`, on by default; EE_AI_EXP `no...` turns each off):
- `economy.forage_patches` (`EECPRMFoodOutsideCenters`, `EECPRMPatchSite`, `EECPRMActivateNeeded`): the food capacity
  is the patches' places plus the workers on farms, fish and game, plus a hunter for each animal within 45 tiles
  (12 at most); a patch with no drop site within 4 tiles gets a settlement in the rings round it; 3 patches stay
  claimed in the Prehistoric and 5 later (the nearest inactive centre with a forage bush); up to 16 fishing boats.
- `economy.claim_needed`: the next epoch's stone, gold or iron short with no active centre holding any claims the
  nearest centre with it; an advance with a short resource that nobody gathers reserves nothing.
- `military.naval_flee` (`EECPRMNavalFlee`): hit by a warship, the boats within 12 tiles go to the own dock farthest
  from it, the citizens and melee units to the town centre; the spot is a danger zone for 30 s (fish there skipped).
- `military.prehistoric_attacks: false` (`EECPACPrehistoricPeace`): no attack group against an enemy in the
  Prehistoric (the defence still forms); `military.prehistoric_army` 8 -> 4. `aggressive` keeps both.
- `epoch_pressure.stone_span: 30` (`EECPEpochPressure`, `AttemptEpochAdvance`): in the Prehistoric the pressure is the
  game time over 30 minutes instead of the epoch timer (50-70 min on Medium), so the Stone Age is asked for from
  9 min and wholly reserved from 24. The later epochs keep the timer.

Head to head against the AI without them (seeds 2-13, both seats, Medium, Small; food per 30 s at 10-30 / 40-60 min):

| Variant | Length | Result | Stone | Copper | Food |
|---|---|---|---|---|---|
| forage patches, first (no patch claiming) | 90 min | lost 1-19 | 51.5 min | 4/24 | |
| forage patches | 90 min | 5-7, 12 close | 49.7 | 3/24 | 57-65 / 58-65 |
| claim_needed alone | 90 min | 4-7, 13 close (no effect: gold near home) | 49.5 | 7/24 | |
| naval_flee alone | 90 min | 7-7 (sim AIs rarely field warships) | 49.5 | 8/24 | |
| + Prehistoric peace, 4 clubmen | 90 min | 4-8, 12 close | 49.3 | 4/24 | 46-64 / 54-62 |
| + Stone span 22 | 90 min | 4-9 | 20.8 | 16/24 | 37-45 / 70-77 |
| + Stone span 22 + citizen cap 30 | 90 min | 2-12 | 21.2 | 16/24 | |
| **+ Stone span 30** | 90 min | **7-7**, 10 close | **25.0** | **20/24** | 45-50 / 78-87 |
| + Stone span 22 | 150 min | lost 5-17 | 20.8 | 17/23 | Bronze 6/23 |
| **+ Stone span 30** | 150 min | **11-10** | 25.0 | 21/24 | Bronze 14/24 against 9/24, Dark 3/24 |
| the rules as shipped | 90 min | 5-5, 14 close | 25.0 | 19/24 | |

- **Patches run out.** The near land holds about 2 patches; the claimed ones lie 49-54 tiles out. The owner's extra
  food is hunting and fish; the AI keeps 1-4 hunters. Still open, with the Copper farms.
- **A Stone Age at 21 min costs too much** (span 22): the reserve stops the citizens at 20 for the advance. At 30 the
  AI reaches it at 25 and holds even.
- **A crash** in a 150-minute game: an AI transport's unload line ran off the map (PORT_LEARNINGS.md "A transport's
  unload line off the map").

### 8.28 The original as a benchmark, the Copper Age, and the claim range (2026-11-07, third playtest and EE-SIM)

**The original AI as a benchmark** (TEMPORARY until the new AI beats it): the `classic` personality holds every rule
at the original's setting; `--set "gameoptions.difficulties.medium.ai_cheats=[...]"` (a list value replaces a list)
brings back the cheats, `EE_AI_CHEATS` narrows them per seat (e.g. `all/none`). The switchless fixes (the guard, the
nearest-home sites, the citizen recovery, the defeat rule) stay on in it. 3-hour Medium games against the current AI:

| classic | Result |
|---|---|
| honest | lost 0-24 (the Stone Age in 2 games: the original lived on the free-epochs cheat) |
| all seven cheats | lost 1-22 (not meaningful: `catchup` holds it back behind the idle sim human) |
| cost, reveal, epochs, extra, retire | **won 5-0, 19 close**: 155 citizens at 30 min (units at 1%), the current AI out-techs it (Bronze 21/24 against Copper at 122 min, Bronze 8/24) |

**Why the Copper Age takes long** (3-hour games, the current AI: Stone 25, Copper 81 min). The timer: the advance is
asked for about 23 min after the Stone Age (0.3 of a ~60-min timer) and its reserve fills over the next 30 (to 0.8).
All three resources reach their costs together, at the reserve's pace; the income (90 food, 42 gold, 38 iron per 30 s)
would pay it in about 11 minutes. But schedules that ask sooner lose: the reserve takes the food and iron soldiers cost.

| Experiment (TEST ONLY) | 90 min | 3 hours | Copper | Army at 40 min |
|---|---|---|---|---|
| `span20` / `span25` / `span30` (every later epoch on a schedule from its start) | 0-20 / 2-18 / 2-18 | 0-22 / 2-22 / 3-20 | 16-20/24 at 63 min | 9-12 (against 14) |
| `boatrush` (wood for 2 docks and 4 boats banked before Stone, a second dock, boats 4 at a time) | 1-15 | 3-19 | 17/24 at 75 | 10 |
| `boatrush` + `span25` / `span30` | 1-18 / 2-14 | 2-22 / 3-20 | 14-16/24 at 59-64 | 6-8 |
| `threatspend` (no reserve on soldiers while a stronger enemy is near) | 6-13 | 6-15 | 20-21/24 at 73 | 14 |
| `boatrush` + `span30` + `threatspend` | 1-16 | 1-22 | 15/24 at 62 | 9 |

`boatrush` reaches the owner's food (106-114 per 30 s at 40-60 min) but the timer still sets the Copper Age; with a
schedule the savers fall behind in soldiers and a third never reach Copper. In AI-against-AI games every investment
loses to the opponent's standing army; the owner's playtests are the better judge of the boom.

**The third playtest** (Continental, Medium, Babylon; a crash at 74 min, then a saved game to the Middle Ages). The
Stone Age at 24.9 min (the owner 24.2), Copper at 73.1 (54.6). The owner: citizens sent to far patches and a far iron
mine with no settlement ("every patch should have 0 or 1 tile distance to a settlement"); land claimed that it cannot
defend, granaries far out; no towers at the resources; houses in the first minutes; more than 4 soldiers in the
Prehistoric; "the ai did a good job rebuilding from raids". The log: 14-17 of 25-27 food trips far, iron trips of 9-16
tiles; 4 houses in minutes 1-4; 6 soldiers in the Prehistoric (the cap counted only the living ones).

**Rules** (under `economy.forage_patches`; TEST ONLY switches `oldpatches`, `nosettle`, `noguard`):
- a claim range of 25 tiles round the town centre for the patch and gold claims, the mines (within it first, the
  original's search when none is left) and the granaries; 5 more each time a farm, granary or settlement finds no room
  (45 at most, one step per 2 minutes);
- a settlement on each claimed patch and worked mine without a drop site within 3 tiles (`EECPRMSettlePatches`);
- a tower at each worked site within the claim range without one within 8 tiles (`EECPRMGuardSites`);
- no houses in the Prehistoric; the Prehistoric army cap counts the soldiers in training.

| Against `oldpatches` | 90 min | 3 hours | Copper | Food trips 10-30 min |
|---|---|---|---|---|
| with the drop rule (no gathering at a patch without a drop site) | lost 4-17 | lost 3-20 | 11-14/24 | 6.5-8.6 |
| without it (as shipped) | **won 10-7** | **won 11-8** | **69.4 min** (75.3) | **4.3-5.6** (5.9-7.0) |

Without the drop rule (the experiment `droprule` keeps it): the towers alone are even (11-11), the settlements carry it.
Dark Age 12/24 against 7/24 in 3 hours.

### 8.29 The Copper Age experiments, and a strategist that failed (2026-11-08, fourth playtest and EE-SIM)

**The fourth playtest** (Continental, Medium, three logs: 0-60, 55-91 and 91-121 min from autosaves). The AI reached
the Stone Age first (24.6 min against the owner's 26.2) and never the Copper Age: 135 citizens at 60 min (the owner
85), 8 soldiers (20), 15-42 on wood (9-13), its first settlement made a town centre at minute 5 and 4 by 71. From 76
min it had no food gatherer for 45 minutes: farms come with the Copper Age, the patches in its claim range were gone,
the owner's warships had sunk its boats, and it held 1399 of the Copper Age's 1584 food. The Copper Age costs 1584
food, 845 gold, 845 iron (750/400/400 x the standard variant's 2.64 x 0.8).

**Why the experiments lost head to head.** The epoch reserve fills on the timer (asked for at 0.3, whole at 0.8):
income above the growing reserve is spent, so an economy idea alone cannot bring the Copper Age sooner, and a faster
schedule takes the food and iron the soldiers cost. `boatrush` lost its army through the wood bank before the Stone
Age and 29-45 boats rebought by 60 min. Most of the "losses" by the body count were 1.5x citizens, not defeats; the
EE-SIM samples now carry `strength` (army hit points x attack) and `value` (the cost of every live unit and
building), and `ai_scorecard.py --score mixed` judges by economy, military and tech shares (0.4/0.4/0.2).

| Experiment (TEST ONLY), 48 games, 2 hours, mixed score unless noted | Result | Copper (median) |
|---|---|---|
| `noop` (both sides unchanged) | 23-23, 21-21 | 74 |
| `noupgrade` | **26-20** | 75 |
| `patchexpand` (as first written: grow with fewer than the wanted patches) | 23-22 alone, 5-7 games worse in combinations | |
| `patchexpand` narrowed (grow only with no patch left) | never fires in the sim; no cost | |
| `fewbarracks` | 20-21 | 74 |
| `stonecit` (90 citizens in the Stone Age) | 22-24 alone, **12-30** with the rest | |
| `safedash` + `stonepeace` | 21-27 | **65.5** (75); Bronze 19/48 at 94 (11/48 at 113) |
| `noupgrade` + `safedash` + `stonepeace` | 19-26 | 71 |
| `stonecap`, `towerdef`, `goldbank`, `hunt6`, `epochplan`, `boatrush2` (24 games, body count) | 10-11, 11-10, 4-15, 8-11, 7-13, 1-17 | |

**Rules** (aipersonalities.yaml; aggressive keeps its trainers and attacks, classic every one off):
- `economy.early_town_centers: 2`: before the Bronze Age a settlement becomes a town centre only below two, and no
  capitol;
- `economy.early_trainers: 2`: before the Copper Age at most 2 barracks and 2 archery ranges;
- `economy.patch_expand: true`: before the Copper Age the claim range grows when no forage patch is left in it;
- `epoch_pressure.safe_bonus: 0.25`: from the Stone Age, the pressure 0.25 higher while no stronger enemy is near;
- `military.stone_attacks: false`: no attack group against an enemy in the Stone Age.

TEST ONLY switches: `noupgrade`/`nonoupgrade`, `fewbarracks`/`manybarracks`, `patchexpand`/`nopatchexpand`,
`safedash`/`nosafedash`, `stonepeace`/`nostonepeace`.

**A strategist, tried and dropped.** One Stone Age plan owning the citizen target (90), the gatherer split (gold and
iron by the Copper Age cost, 10 wood, 5 stone, 12/18 at most on gold/iron), the spending rule (the next epoch's food,
gold and iron held from the epoch's start), the building whitelist and caps, a light army of 10 (20 under threat),
and the advance asked for once paid. Nine 48-game batches against the shipped AI: the Stone-to-Copper time fell to
29-38 min (the shipped AI's 46-50), but 18-36 of 48 strategists were destroyed and fewer than half reached the
Copper Age; it never kept an army (2-3 soldiers at 40 min against 13-15). Found on the way: the split starved food
at a small town; the epoch research asked for early blocks the town centre's citizens; the 40-tile threat reaches the
enemy's town on small maps; wood starved the archery range of the favourite slingers; temples, universities,
hospitals and the next epochs' towers clogged the build queue (70-200 retries each); holding citizens for the light
army deadlocked when no soldier could be made. The owner: a pure rush beats a boom, a boom with towers locks a rush
out and wins later, hence several personalities.

**EE-SIM notes.** Two benches at once pick the same ports: `Publish` fails, `NESynchronousServer` keeps a NULL
recorder and the first input faults in `NERecordPlayback::Process` (the original's path). Run names past the Windows
path limit fail as no-report (long `+` experiment lists).

### 8.9 Next

Done since §8.5: 60-minute multi-seed benchmarks (`--jobs`), the gatherer, placement and stall trace, Q2, epoch
pressure, the stock correction, the lost builders, and personalities as data.

1. **Raids on gatherers.** Most citizen deaths happen more than 30 tiles from the town center (§8.14). Candidates:
   - keep gatherers on sites the AI can defend (distance, towers);
   - let citizens flee to or garrison in the nearest building;
   - escort the far resource centers.

   A defense reflex that pulled soldiers out of their groups did not help.
2. **Food before farms.** Hunting and fishing, forage site choice (§8.4), and drop sites. Then the Copper Age farm
   economy, where "use what you hold" (granaries, farm plots) starts to bind.
3. **Seat asymmetry:** resolved; nothing measurable on the current build (§8.17).
4. **Lobby:** a personality per AI slot, and a fair-Hard difficulty row (`ai_cheats: []`) shipped as data.
5. **Still to port:** `save_load.py` (save gate) and `game_batch.py`, or use `ai_bench`.
6. **Before any merge:** `tools/gates.sh`, the macOS/Linux builds, and republishing the data bundle (new rules
   document).
