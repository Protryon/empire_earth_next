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

### 8.30 The boom script (S3, 2026-11-08, EE-SIM)

The next engine's boom personality as a Lua script (`ai/boom.lua`; docs/AI_LUA_PLAN.md §4.1 has the plan as built).
EE-SIM, Small, Medium, 2 AIs, no cheats, 2 hours, the boom against the classic engine (`balanced`), both seats
(`--engine next,classic` and `classic,next`), `--score mixed` where noted. Epoch times are the reports' `epoch_ms`.

**The way there** (seeds 26-29, 8 games, then 26-33, 16 games; the earlier rows as they ran):

| Version | Games | Stone | Copper (reached, median) | Copper after Stone | Defeated (before Copper) |
|---|---|---|---|---|---|
| the S2 test opening (`test_opening`) | 8 | 20.7 | 5/8, 47.3 | 29.2 | 8/8 (3) |
| v1: fixed shares, any enemy unit a threat | 8 | 26.6 | 3/8, 60.2 | 36.4 | 4/8 (4) |
| a threat only above a scout (2,500), food first, the start-up claims released | 8 | 24.4 | 7/8, 54.8 | 30.3 | 2/8 (1) |
| the sites' `explored`, the settlement fallback | 8 | 25.9 | 8/8, 56.2 | 31.8 | 4/8 (0) |
| gatherers by what each resource lacks | 8 | 21.8 | 8/8, 52.4 | 30.5 | 2/8 (0) |
| minerals limited to the places of reachable sites | 8 | 19.9 | 6/8, 62.9 | 44.2 | 3/8 (2) |
| claimed sites gathered before they are seen | 8 | 21.1 | 6/8, 52.7 | 32.0 | 1/8 (1) |
| a Prehistoric bank (gold 350, iron 400), mines out to 60 tiles, the light army before the reserve | 16 | 23.0 | 12/16, 50.9 | 28.8 | 2/16 (2) |
| `holds` (mixed centres), refused sites passed over, bank 250/300 | 16 | 22.5 | 15/16, 52.0 | 30.2 | 2/16 (1) |
| 30 per gatherer (was 50), the granary's claim growth | 16 | 22.3 | 15/16, 52.2 | 30.6 | 2/16 (0) |
| HasEnemyNear counts enemies only (as committed) | 16 | 22.3 | 15/16, 52.2 | 28.9 | 0/16 (0) |
| TEST: Stone Age citizens 72 (was 64) | 16 | identical to the row above | | | |

What each step found:
- **Scouts as threats.** The classic AI's early explorer (one unit, strength ~1,600) counted as a threat: the
  reserve came off, 12 clubmen were wanted in the Prehistoric, and the food went to soldiers (the Stone Age at 62
  min in one game).
- **Minerals out of reach.** Gold wanted, no gatherer on gold for 30 minutes: the mines in the view were never seen
  (the next engine has no explorer), or an enemy was near; the wanted miners wandered (1,520 moves a game).
- **The start-up claims.** Regroup's first activations hold mines 30-40 tiles out and the finder takes the farthest
  first: releasing them brought the miners home.
- **Mixed centres.** A centre labelled stone also held the town's nearest gold; releasing it starved gold.
- **Granaries.** In the Copper Age a granary found no room round the claimed land; with no granary there are no farm
  plots and so no farms: 60-98 idle citizens in one game. The claim now grows when the granary has no room.

**The benchmark** (seeds 26-49, both seats, 48 games, as committed):

| Measure | Boom | Classic | Owner's target |
|---|---|---|---|
| Stone Age (median) | 22.4 min, 47/48 reached, 42/48 by 25 | 25.0 | 20-26 |
| Copper Age (median) | 52.7, 44/48 | 66.3, 47/48 | 41-46 |
| Copper after Stone | 30.7 (6/48 within 25, 20/48 within 30) | 41 | about 20 |
| Bronze Age within 2 hours | 19/48 at 91.0 | 44/48 at 99.4 | 67-86 |
| Defeated before Copper / within 2 hours | 2/48 / 9/48 | 0 / 0 | |
| Citizens at 30 / 60 min | 44 / 76.5 | 35 at 30, 53 at 40 | 50-62 / 85-94 |
| Army at 30 / 40 min | 5 / 4 | 13 / 14 | |
| Most gatherers on gold / iron / wood, Stone Age | 10 / 11 / 14 | | at most 12 / 18; 10-23 |
| Fishing boats | 11 | | 8-10 |
| Towers by 40 min | 6 | | 4-7 |
| Mixed score, 2 hours | 2 wins, 37 losses, 9 close | | |

By seat (seat 0 = p2): Stone 22.4 / 22.4, Copper after Stone 30.9 / 30.4, defeated before Copper 0/24 and 2/24.
The Copper Age's three costs (1584 food, 845 gold, 845 iron) are each first in stock about 20 minutes into the Stone
Age (medians 21.5, 20.0, 19.3); iron comes last in 26 of 48 games. The Stone Age citizens never reach the plan's 64
before the reserve begins at 10 minutes, so a higher target changes nothing (the TEST row).

**Against a rush** (the classic `aggressive`: a large standing army, attacks from the Prehistoric; seeds 26-33, both
seats, 16 games): Stone 22.2, Copper 56.8 (15/16), Copper after Stone 33.2; 1/16 defeated before the Copper Age, 2/16
within 2 hours. The rush is slow (Stone 28.1, Copper 81.3) and holds 19-29 soldiers at 30-40 min against the boom's
4-5: the mixed score is 10-3 for it (3 close), as the owner expects of a pure rush against a boom; the towers and
the militia keep the town.

**Checks.** The classic engine's traces of seeds 2-5 (1800 game s, classic against classic) are identical before
and after the S3 changes (1842, 1822, 1746 and 1807 lines). The same seed twice gives the same trace (seeds 26 and 27,
3600 game s: 4795 and 4733 lines). No script errors in any run.

**The S3 gate:** Stone at 25 min at most: met (median 22.4, 42/48). Copper within about 25 min of Stone: not met
(30.7). Fewer than 1 in 10 defeated before the Copper Age: met (2/48 against the classic AI, 1/16 against the rush).
The owner's playtest is still to come.

### 8.31 The army and the rush and balanced scripts (S4, 2026-11-08, EE-SIM)

The next engine's army (docs/AI_LUA_PLAN.md §5 "S4 as built": groups that hold named soldiers, attack, raid,
defend, retreat, regroup, disband, scout) and two scripts: `ai/rush.lua` (aggressive) and `ai/balanced.lua` (tall for
now). Every bench: EE-SIM, Small, Medium, 2 AIs and the idle human, no cheats, 2 hours, seeds 26-49 in both seat
orders (48 games a matchup), `--score mixed`; "defeated" is a side with no citizens and no army at the end. The
classic engine's traces are unchanged (seeds 2-5, 1800 s, `ai_trace.py compare`: 1842, 1822, 1746 and 1807 lines
identical, after each C++ change). Two runs of the 576 failed to start (the lobby's "slot 0 human: index #0 outside
the combo's 0 lines", before any game code) and are left out.

**Round 1** (the first scripts; the pure boom is the S2 test opening, before S3 landed):

| Matchup (48 games) | Wins (mixed) | Defeated |
|---|---|---|
| rush vs classic | 6 - 38, 3 close | rush 27/47, classic 1 |
| balanced vs classic | 2 - 41, 4 close | balanced 25/47, classic 0 |
| rush vs test-opening boom | **24** - 6, 18 close | rush 1, boom 4 |
| balanced vs test-opening boom | **37** - 5, 6 close | balanced 0, boom 1 |
| balanced vs rush | **26** - 11, 11 close | balanced 3, rush 2 |

**Round 2** (main's S3 merged: the boom script with a scout and a defend guard, its army allowed to grow further, the
fog bypass reverted; the pure boom is now `ai/boom.lua`):

| Matchup (48 games) | Wins (mixed) | Defeated | Army at 20/30/40 min (script) |
|---|---|---|---|
| rush vs classic | 3 - 43, 2 close | rush 31, classic 0 | 5.0 / 6.3 / 7.9 |
| balanced vs classic | 5 - 43 | balanced 28, classic 0 | 4.0 / 6.3 / 7.6 |
| boom vs classic | 2 - 38, 8 close | boom 10, classic 0 | 3.9 / 4.8 / 3.6 |
| rush vs boom | 13 - **18**, 17 close | rush 0, boom 3 | 5.1 / 6.4 / 9.0 vs 3.0 / 3.8 / 3.3 |
| balanced vs boom | 10 - **17**, 21 close | none | 3.7 / 6.8 / 8.6 vs 3.5 / 5.5 / 7.0 |
| balanced vs rush | **20** - 14, 14 close | balanced 4, rush 1 | 3.2 / 4.8 / 7.1 vs 4.9 / 7.7 / 10.0 |

What the traces showed (and the coordinator's analysis of them):
- **The groups work**: soldiers stay in their groups (about 1.2 groups a side, nearly every soldier in one), defend
  groups fight on the claimed land and come back, scouts find the towns (`enemies[].town_x`) and gather spots, raids
  flee from towers and armies (`next.group flee`).
- **The rush split its army**: raids of 3, the guard and a wave at once; against the classic's 13-29 its army fell
  from about 9 at 30 minutes to 3 by 50. Its first wave in some games came only at 22 minutes: the scout died and
  the next one waited 2 minutes, and a wave needed the target's town seen.
- **The armies never reach their targets**: the epoch reserve holds what the soldiers would cost (saving lasts 10-15
  minutes in this sim's slow economy), and in the Stone Age the food income falls to 30-45 a minute with 15-20 food
  gatherers (the patches gone, the hunting far). Balanced reaches the Copper Age in 31 of 48 games against classic;
  the classic reaches the Bronze Age in 39.
- **Citizens die near the town from 30 minutes**: 50-110 lost in 2 hours against classic, which attacks from the
  Copper Age with 10-16 soldiers.

**Round 3** (the scripts as committed): one army (a raid only from a surplus of 6 over the guard; never a wave smaller
than the army seen at the target), scouting again every 30 s until the target's town is seen, a wave at the target's
gather spot while its town is unseen, the soldiers missing up to half the army wanted left out of the epoch's
reserve, the rush 30/60/80 citizens and waves of 10/14, balanced two barracks from 40 citizens. A 16-game check
(seeds 26-33) before the full bench: rush vs classic 3 - 11 (defeated 7/16, was 31/48), army 8.4 / 8.4 / 7.4 at 20 /
30 / 40 min (was 5.0 / 6.3 / 7.9); balanced vs classic 1 - 15 (defeated 7/16, was 28/48), army at 40 min 10.2 (was
7.6); rush vs boom 4 - 8, 4 close.

The full round 3 bench (48 games a matchup, 0 failed runs):

| Matchup (48 games) | Wins (mixed) | Defeated | Army at 20/30/40 min (script) | Stone / Copper (median min) |
|---|---|---|---|---|
| rush vs classic | 9 - 36, 3 close | rush **21** (was 31), classic 1 | 8.1 / 8.2 / 7.2 (was 5.0 / 6.3 / 7.9) | 32.6 / 75.4 |
| balanced vs classic | 3 - 44, 1 close | balanced 26 (was 28) | 3.8 / 5.9 / 8.2 | 26.7 / 63.7 (32/48) |
| rush vs boom | 12 - **19**, 17 close | boom 4 | 8.7 / 9.7 / 10.2 vs 3.3 / 3.7 / 3.4 | rush 33.3, boom 23.6 |
| balanced vs boom | 6 - **19**, 23 close | none | 3.6 / 7.0 / 10.7 vs 3.7 / 5.0 / 6.3 | balanced 25.9, boom 22.6 |
| balanced vs rush | 14 - **20**, 14 close | balanced 7 | 3.1 / 3.0 / 6.4 vs 8.5 / 8.6 / 10.7 | balanced 28.0, rush 32.0 |

The rush's Stone Age at 33 minutes is a bug of round 3: its army wanted is "the population", so the army floor
(half of it) left the whole epoch reserve out; the floor is now capped at 10 soldiers (`cfg.floor`; committed after the bench). Its 16-game check (seeds 26-33,
against the same seeds of round 3's first check): the rush's Stone Age at 29.1 minutes (was about 33), its army 6.9 /
6.8 / 8.5 at 20 / 30 / 40 min (was 8.4 / 8.4 / 7.4); rush vs classic 1 - 14, 1 close (defeated 9/16, was 7/16); rush
vs boom 3 - 5, 8 close (was 4 - 8, 4 close). The cap trades soldiers for the epoch; neither setting makes the rush beat
the boom.

**The gate** (AI_LUA_PLAN §5 S4):
- **Rush beats a pure boom: not met** against `ai/boom.lua` (round 3: 12 - 19, 17 close; it was met against the S2
  test opening in round 1, 24 - 6, 18 close). It never defeats the boom (0-4 booms defeated in 48).
- **A boom beats a failed rush: met.** The rush falls back in 45 of the 48 games (44 in round 2); in those the boom
  wins 18 - 10, 17 close (round 2: 17 - 10, 17 close); in the 3 where the rush held on it won 2 - 1.
- **Balanced holds its own** against the scripts (not lost in 29 of 48 against the boom, 28 of 48 against the rush;
  against the S2 test opening 37 - 5) but **not against classic** (3 - 44, defeated 26/48), which out-builds every
  next script from the Copper Age (39/48 classic Bronze Ages against 1/48).

Open:
- **The economy beyond the Stone Age** is the limit for all three scripts: food income collapses once the patches are
  gone and before farms (rb_economy builds farms in the Copper Age, too late), and the reserve stalls the army for
  10-15 minutes per epoch. The boom's economy work (S3) and S5 (farms, later epochs) are where the classic gap is.
- **Towers stay Prehistoric** (wrong, §8.32: the computer player gets each upgrade with its epoch): there is no tower upgrade verb; rb_economy counts only the current epoch's tower class
  (so it builds new ones each epoch), boom.lua counts every class toward its plan.
- **Rush micro**: clubmen waves of 10 still die to towers and a defending army; a rush wins when it hits early and
  kills gatherers, and it should pick fights by the seen strength near each target, not only at the wave.
- **The target**: `lib.army.pick_enemy` takes the largest economy seen; in EE-SIM the idle human (player 1) is an
  enemy too, and the first raids sometimes went there.

### 8.32 The next computer player's towers and the epochs (2026-11-08, EE-SIM)

§8.31 read from the scripts that the next engine's towers stay `b_guard_tower_paleo` all game. Measured, they do not.
The tower upgrades are techs (`copper_tower_upgrade`, `bronze_wall_and_tower_upgrade` ... `space_wall_and_tower_upgrade`;
dbtechtree kind 7, at the town centre or capitol, 50-300 of the fourth cost, each after the one before). Each one's
event converts every tower of the old class into the new one (dbeffects kind 19) and obsoletes the old class's
tech. They are `mResearchedWithEpoch`, and `EEComputerPlayer::EnterEpoch` researches an epoch's kind-7 techs the
moment the epoch is reached, for both engines, free. So the towers turn Copper on entering the Copper Age and Bronze on
entering the Bronze Age. The classic AI never researches them itself.

Bench: EE-SIM, Small, Medium, 2 AIs and the idle human, no cheats, 2 hours, seeds 26-33 in both seat orders (16
games a matchup), a hard-linked copy of the open tree. A TEST ONLY wrapper (`m_boom`, `m_rush`) printed the view's
`buildings[]` towers each minute. The table counts finished towers at each epoch's last minute, summed over the games
that reached that epoch (P paleo, C copper, B bronze). Six rush runs failed to start (the lobby's "slot 0 human"
fault, and a crash at the main menu on one slot); seeds 30-33 were run again for them.

| Matchup | Prehistoric | Stone | Copper | Bronze | Stone / Copper (median min) |
|---|---|---|---|---|---|
| boom vs classic, before | P 42 (16) | P 71 (16) | **C 72** (13) | **B 33** (5) | 24 / 54 |
| boom vs classic, after | P 42 (16) | P 71 (16) | C 72 (13) | B 33 (5) | 24 / 54 |
| rush vs classic, before | P 4 (16) | P 26 (15) | **C 30** (8) | **B 6** (1) | 30 / 65.5 |
| rush vs classic, after | P 4 (16) | P 26 (15) | C 30 (8) | B 6 (1) | 30 / 65.5 |

No old-class tower is left in any game past the Copper Age, before or after. The change (the `research` verb answers a
researched tech `done` instead of `busy`; `lib.view.tower_upgrade` orders each epoch's upgrade in boom, rush and
balanced) issues orders that come back `done`. Every trace is identical to the one before but for those lines (16
boom and 16 rush games), so the epoch pace is the same. The classic engine's traces are unchanged (seeds 2-5,
1800 s, `ai_trace.py compare`: 1842, 1822, 1746 and 1807 lines identical).

Open then: the **number** of towers, not their class, fell behind the plan. Boom has about 4.4 in the Stone Age (plan 7)
and 5.5 in the Copper Age (plan 10). Over the 16 boom games, `build near=site` got `no_site` for 76 Copper towers
(13 queued) and 26 Bronze towers (none queued). Paleo towers: 49 `no_site`, 87 queued. The newer towers find no room
in the rings round the worked sites. Also open: whether the computer should get the upgrades free (a human pays and
waits 45 s at the town centre). Without the free research, the scripts already research the upgrades.

**Tower placement.** A TEMP diagnostic of every failed ring search (seed 28, boom) showed why the later towers found
no room. By the Copper Age the only kept sites still unguarded were far or contested mines, 40-47 tiles from the
town. In their rings 2..7, 150-210 of the 216 tiles failed the block test. The first failing tile was out of sight
(`EECanPlaceUnitOnTile` places only on tiles visible now), under trees or the mine, or had another player's unit on
it. The rest failed `HasEnemyNear`. Boom then cycled over those 2-3 sites (each passed over for 5 minutes) and built
at the town only below 3 towers. The rush's `near=town` towers (the classic FindTowerSite) were refused 735 times:
FindTowerSite finds nothing once the gold, iron and worked food centres nearest the town are guarded.

The fix: `EECPNextBuild` sends a tower with no room at its site to the town (FindTowerSite, then the rings 3..12
round the town centre, at least the tower's range from the player's other towers; only tiles in sight, as before).
A `near=town` tower that FindTowerSite cannot place takes the same rings. Boom builds at the town whenever no kept
site is left unguarded (it stopped at 3). Same bench (16 games each, all runs started):

| Matchup | Stone | Copper | Bronze | tower no_site | Stone / Copper / Bronze (median min, games) | citizens lost / game (script, classic) |
|---|---|---|---|---|---|---|
| boom vs classic, before | 4.4 | 5.5 | 6.6 | 154 | 24 / 54 (13) / 90 (5) | 104, 49 |
| boom vs classic, after | 4.9 | **6.7** | **9.1** | 23 | 24 / 54 (15) / 86.5 (8) | 119, 68 |
| rush vs classic, before | 1.7 | 3.8 | 6 | 739 | 30 / 65.5 (8) / 116 (1) | 77, 39 |
| rush vs classic, after | 2.2 | 4.7 | 4.5 | 2 | 30 / 65 (7) / 110.5 (2) | 79, 31 |

(Towers: finished ones at each epoch's last minute, the mean over the games that reached the epoch.) The epoch pace
is unchanged; more boom games reach Copper (15 vs 13) and Bronze (8 vs 5). Citizens lost do not fall. Boom loses
more because more of its games run long into the later epochs, and the classic loses more too. Losses are the
citizen-defence work's concern. The boom's Stone Age is still below its plan of 7: there it is the stone, not the
room. Classic traces unchanged (seeds 2-5, 1800 s: 1842, 1822, 1746, 1807 lines identical). A boom game run twice
gives identical traces (9376 lines).

### 8.33 Why the next engine's citizens die (2026-11-08, EE-SIM)

The next engine lost 50-110 citizens per 2-hour game against the classic AI (§8.31), even though boom and balanced
guard the town with one defend group. Every bench below: EE-SIM, Small, Medium, 2 AIs and the idle human, no cheats,
2 hours. Boom vs classic is played from both seats (seeds 26-33, 16 games). Balanced vs classic is played from seat
p2 (8 games). "Citizens" are land citizens; fishing boats are counted apart. The scorecard's "citizens lost" counts
both.

**The measurement.** A new trace (trace on only; the game reads nothing of it: EECPNextArmy.cpp, from
EEComputerPlayer::DamageTaken and UnitTaken) writes two lines:
- `next.death`, for every citizen and boat lost:
  - where it died (tile, distance from the town centre) and the epoch;
  - the killer (player, class, kind);
  - what the citizen was doing at its first hit of the incident, and at its death;
  - the foes within 10 tiles, the armed animals within 4 and the own soldiers within 10;
  - the nearest group (distance, task, units), and the nearest own tower and drop site;
  - the time from the first hit and from the incident's start.
- `next.raid`, for every incident (hits on citizens within 20 tiles and 30 s of each other):
  - its length, hits, losses and most foes;
  - how far the nearest group was at its start, and how long a group took to come within 10 tiles.

The scripts that read them (deaths.py, militia.py, drain.py) were scratch tools and are not in the repository.

**Before** (main a84b92a with the trace, 16 boom games; balanced from §8.31's script):

| | Boom | Balanced |
|---|---|---|
| Land citizens / boats lost per game | 83.6 / 37.1 | 76.4 / 48.1 |
| Killed by soldiers / animals / warships / towers | 91% / 3% / 3% / 2% | 87% / 6% / 5% / 0% |
| Died within 15 tiles of the town centre | 58% | 77% |
| In the Copper Age / Bronze Age | 60% / 22% | 84% / - |
| Fighting when first hit (the militia) | 52% | 51-58% |
| Moving when first hit / at death | 17% / 40% | 10% / 29% |
| No own soldier within 10 tiles | 65% | 49% |
| No next-engine group at all | 51% | 37% |
| Losses in incidents of 8+ foes | 54% (675 of 1244) | 72% |

What it shows:
- **Armies in the town.** The classic attacks the town from the Copper Age with 8-17 soldiers; hunting, remote sites
  and animals are minor causes.
- **The militia fights armies.** It decided "fight" 1957 times and "run" 292. Its odds sum the towers' hit points
  (1000-1450 each) with the citizens', so 12 citizens and a tower against 8 rock throwers came out at 7.9. Fights
  against 5 or more raiders account for about half of the fighting deaths.
- **No army.** The boom's army was 4.1 / 3.5 soldiers at 30 / 40 minutes. Two causes:
  - the reserve, once the stock was below it, let no soldier be made: "reserve less the light army's cost" still
    leaves stock - cost < reserve, so one game had 0/8 soldiers for 50 minutes;
  - under a threat the town never scrambled: six towers anywhere counted 6 x 4,000 against a threat of 3,000.
- **Boats.** Frigates sank 37 boats a game, at fish the 30-second naval danger zones had already forgotten.

**Tried and dropped** (16 boom games each, seeds 26-33; TEST ONLY `EE_AI_EXP` switches, removed again):

| Variant | Land citizens lost / game | Copper reached | Why dropped |
|---|---|---|---|
| baseline (main 6a03d9b) | 71.7 | 13/16 | |
| `mrun`: the citizens fight only raids of 2 or fewer and run from anything larger to a refuge 15+ tiles from the attack | 77.2 | 9/16 | 70% now die running (spearmen and horsemen catch them); 25-29 citizens bounce between work and refuge every 4 s through the attack; the economy stops |
| `modds`: honest odds (towers add attack, not hit points; group soldiers count), no raider cap | 79.6 | 14/16 | no gain |
| rb_economy: the reserve cap below, for balanced and rush | 90 (balanced) | 7/8, Copper +8 min | slower and no larger army |
| a defend group that answers soldiers attacking the player's units first, up to 12 tiles past the claim (32 games, seeds 34-49) | 74.0 vs 77.6 | | within the noise |

Neither fighting nor running saves citizens from an army in the town; killing the army does. Game-to-game variance
is large (1 to 224 losses), so 16 games resolve only about 15 losses a game.

**Kept:**
- **The boom's army** (boom.lua):
  - the reserve never locks out the missing light army: at most the stock less what the missing soldiers cost, in
    steps of 100;
  - the town scrambles under a threat while its army is below the light army;
  - the stock targets count the soldiers of both trainers (the archers' wood);
  - a guard (from an owner playtest, where a raid's scramble spent the Copper Age's iron from 433 to 22, then the last
    safe iron mine ran out): a resource of the advance that is 70% paid, or that has no site left to gather from,
    keeps its reserve under a scramble and against the light army.
- **The boats:**
  - the fish finder skips fish within 12 tiles of an armed enemy ship the player sees now (C++, next engine; read
    from the world at the call, so fog-honest and save-safe);
  - the boom replaces no boat while 3 or more were sunk in the last 5 minutes.

**After** (the branch merged with main, same 16 seeds and seats):

| | Before | After |
|---|---|---|
| Land citizens lost / game (boom) | 83.6 | **65.5** |
| Fishing boats lost / game | 37.1 | **19.2** |
| All citizens lost (scorecard) | 118.9 | **82.8** |
| Army at 20 / 30 / 40 min | 3.6 / 4.1 / 3.5 | 4.0 / **7.9 / 7.2** |
| Citizens at 10 / 20 / 30 / 40 min | 22.4 / 26.8 / 40.9 / 57.3 | 22.4 / 29.1 / 40.1 / 58.9 |
| Stone / Copper / Bronze (median min, reached) | 23.5 / 53.4 (15) / 85.6 (8) | 24.3 / 51.7 (15) / 89.0 (12) |
| Per seed: Stone / Copper later by (median) | | +0.0 / +1.2 min |
| Wins, mixed score | 1 - 13, 2 close | 2 - 8, 6 close |
| Advance reserves of 600+ drained below 30% within 3 min | 2 (2 games) | **0** |
| Rush vs boom (16 games, both seats): wins | boom 5 - rush 1, 10 close | boom **8** - rush 1, 7 close |
| Rush vs boom: boom citizens lost | 29.5 | 14.5 |

The after-bench's causes: 86% soldiers, 50% within 15 tiles, 50% fighting when first hit, and 42% with no group near
(51% before). Balanced vs classic (8 games, its script unchanged, only the C++ fish check): 76.4 → 86.8 land citizens
and 48 boats, within the noise; balanced has no boat hold. Classic unchanged: seeds 2-5, 1800 s, classic against
classic, pre and post executables, `ai_trace.py compare` identical (1784, 1884, 1854 and 1829 lines). The same boom
game twice: identical (seeds 26 and 27, 3600 s: 4657 and 5864 lines).

**The owner's build order** (a separate step, after the above; boom.lua's plan table):
- **Prehistoric:** no soldiers; all the food to citizens (32-36: about 18 food, 5-10 wood, 4 iron, 2 gold, 2 stone);
  the barracks at 28 citizens; the advance as soon as it is paid.
- **Stone:** 20 soldiers first, spearmen preferred (`PREFERRED`), from two barracks, while the citizens wait at 32
  for at most 5 minutes. Then the citizens: 18 food (`food`, a minimum), 12 gold, 12-18 iron, 6 stone and 10-15
  wood, the boats on top. Then the Copper Age.
- **Copper:** 15-20 wood, 6 stone, 12-18 gold and iron, 3 granaries and 24 farms.
- **The give-way rule:** when food is short of its minimum, the resource with the most gatherers beyond what its
  lack needs gives one up. Before, iron always gave way first, while gold piled up past 1000.

Same 16 seeds against the after-bench above (v1 without the give-way rule):

| | After (above) | Build order v1 | Build order v2 |
|---|---|---|---|
| Stone (median, per seed vs after) | 24.3 | 23.1 (-0.7) | **22.1 (-1.6)** |
| Copper (median, reached; per seed vs after) | 51.7 (15) | 60.1 (14; +6.0) | 58.7 (13; +3.3) |
| Citizens at 10 / 20 / 30 / 40 min | 22.4 / 29.1 / 40.1 / 58.9 | 23.3 / 33.6 / 41.2 / 60.1 | 23.6 / **33.9** / 42.8 / **65.6** |
| Army at 20 / 30 / 40 min | 4.0 / 7.9 / 7.2 | 0 / 6.4 / 11.8 | 0 / 6.1 / **15.0** |
| Land citizens / boats lost per game | 65.5 / 19.2 | 103.8 / 25.4 | 81.8 / 26.8 |
| Wins vs classic (mixed) | 2 - 8, 6 close | 2 - 13, 1 close | 2 - 11, 3 close |
| Rush vs boom (boom - rush, close) | 8 - 1, 7 | 6 - 3, 7 | 8 - 6, 2 |
| Advance reserves drained | 0 | 0 | 0 |

The build order reaches Stone faster and has a larger economy and army by 40 minutes. The Copper Age comes later,
and more citizens die: the 20 soldiers and 18 food gatherers leave too few for iron at 36 citizens. Without
Prehistoric soldiers, the rush wins 6 of 16 (1 before).

**Left:**
- The town still loses to 8+ soldier armies (576 of the 922 incident losses after). The boom needs a larger standing
  army there, and its towers sit mostly at the sites.
- The militia's tower-inflated odds stay (better odds did not help by themselves).
- Balanced and rush have neither the army fixes nor the boat hold.
- The trace's `losses` line (EECPTrace.cpp EECPTraceUnitLost) lacks braces: `citizens_by_distance` and
  `citizens_by_killer` count every unit lost, and buildings and other losses only when there is no town centre. Not
  fixed here, because the fix changes the classic traces.

### 8.34 The resource sites: the false enemy_near and the vanished mine (2026-11-08, owner playtest and EE-SIM)

**The playtest** (Mac, boom, Medium, Continental Small, the owner against one next AI). The AI stayed in the Stone
Age. From 10 minutes on most mineral sites 32-55 tiles out were `enemy_near` (stone 14/16, gold 10/14, iron 13/17).
At about 50 minutes the only `ok` iron site (iron:12, d35, claimed, a drop site at hand) left `sites[]`. The iron
gatherers fell to 0-1 for the rest of the game, though Copper needed 845 iron and the AI had 52. The owner was not
near those sites until the end.

**The vanished site: the view lost every worked mine.** A gather is `EEUGGather::Work`. It hits a resource whose
hit points are not 0 for the gatherer's attack, as it hunts an animal. A mine has 1 hit point (dbobjects), and
`EEUnit::TakeHit` sets a resource's hit points to 0 instead of killing it. So the first swing at a mine leaves it at
0 hit points, still gatherable while its `mResourceAmount` lasts. Every mine holds 300000, so none is used up in a
game. The view counted only units with hit points above 0 (`holds`, `units`, the site's tile), and so did the
tactics' site tile. A mine dropped out of `sites[]` as soon as it was worked. In the playtest log the home gold
(d7, d9) and iron (d9, d15) were gone by 5 minutes, the stone at d9 by 30, and iron:12 by 55: in each case right
after its gatherers arrived. Boom then had no site for the mineral and set its gatherers to 0
(`want[r] = places`). The classic finders test only the amount and kept gathering, but the script no longer asked
for gatherers. Worked forage patches vanished the same way. Their claims never came back to the script: not
listed, so never released, and the claim range grew because "no patch was left". The `cp sites` trace line has the
same hit point test; it is left as it is, because changing it changes the classic traces.

**The false enemy_near: enemy citizens out of sight.** `enemy_near` was `EECPBuildManager::HasEnemyNear` (0x467e10):
the influence table's regions are 9 tiles, the box is the site ± 9 tiles, and the unit regions over it are taken
whole (up to 27 tiles across). It counts every armed unit not its own that is not an idle searcher. Citizens have an
attack of 5, towers count whatever their range, and the player need not see any of it. A TEST ONLY trace (`EE_AI_EXP
sitetrace`: once a minute each blocked site, the unit that blocked it, whether its tile is in sight) over the bench
below:

| Blocked site-minutes, 16 games | Before | After |
|---|---|---|
| `enemy_near` | 5977 | 1650 |
| ... by enemy citizens | 4602 (77%; 4562 out of sight) | 0 |
| ... by soldiers | 900 (390 in sight) | 1650 (1501 in sight, 149 remembered) |
| ... by towers (range not checked) / ships and boats | 309 / 128 | - |
| `tower` (range + 3 tiles covers the site) | - | 2749 |
| `danger` | 5 | 171 |
| home sites blocked (`enemy_near`, raids seen on them) | - | 526 |

Before, 91% of the blocking units were out of sight. On a 1v1 Continental Small the sites 40-55 tiles out lie by the
other town, whose gatherers block them. That matches the playtest: the owner's citizens near his own town.

**The fix** (next engine only; classic traces unchanged, seeds 2-5, 1800 s: 1784, 1884, 1854 and 1829 lines
identical; a boom game run twice: 5283 lines identical):
- The view counts a resource unit by its amount (`EECPNextResourceLeft`). The tactics' site tile does the same.
- `EECPNextSiteBlocked` (EECPNextView.cpp) is the check for the view, for the finders' next-engine branch
  (FindResourceTarget, FindFarm) and for next-engine `HasEnemyNear` (also used for building sites):
  - `enemy_near`: enemy soldiers (land military, not citizens) seen within 9 tiles now or in the last 20 s. The memory
    (`mThreats`) is filled from the army step's scan and saved in the state block (version 3).
  - `tower`: an enemy tower seen and not seen destroyed (the army's tower memory), or an armed building in sight,
    whose range + half its size + 3 tiles covers the site.
  - Then `danger` and `area` as before.
- **Home sites** (`sites[].home`, the owner's rule): the start resources every player gets are the nearest 2 gold,
  2 iron, 1 stone and 2 forage patches within 20 tiles of the town centre. Continental Small gives per player 1 gold,
  1 iron, 1 stone and 2 berries, plus `PlaceResourceInRing` gold 14-16 and iron 15-17. A home site is blocked only by
  soldiers seen on it now (a raid, which the defence answers): never `tower`, `danger` or `area`. In seed 26 the
  home sites are food d4 and d8, stone d4, gold d7 and d14, iron d4 and d7. The playtest's iron:12 (d35) was not a
  home site. Its home iron was at d9 and d15, and both left the view in the first 5 minutes.
- **Boom** (ai/boom.lua, claims only): the home sites come first in `sites_of`. A mineral the plan wants with no
  open site kept contests the nearest site that only soldiers block: it is kept and claimed, and an escort (a
  `defend` group of 2-6 from the home guard, radius 8) holds it. The escort is disbanded when the site opens or the
  town is threatened. Such a resource counts as `no_site` for the reserve (§8.33). There were 11 escorts in the 16
  games.

**Bench**: EE-SIM, Small, Medium, 2 AIs and the idle human, no cheats, 2 hours, seeds 26-33 in both seat orders (16
games), boom against the classic, `--data open`, main at 7a1155f (§8.33's build order) before and after.

| | Before | After |
|---|---|---|
| Gold / iron gatherers in the Stone Age (mean of 30 s samples) | 8.4 / 9.9 | 6.6 / 9.6 |
| ... samples with no iron gatherer | 13% | 14% |
| Stone (median min) | 22.1 | 23.1 |
| Copper reached, median min | 13/16, 58.7 | 11/16, 51.7 |
| Wins vs classic (mixed; close) | 2 - 11 (3) | 2 - 10 (4) |
| Citizens lost a game (boom) | 106.2 | 97.8 |

The blocked reasons are now true, and no mine leaves the view, but the economy against the classic engine is about
the same. Every difference above is within the §8.33 variance. The gold gatherers follow the plan's want now (about
5): before, extra ones beyond the want went on working mines the view no longer listed. Against the classic AI the
far sites stay blocked, now by its towers (2749 site-minutes) and soldiers rather than its citizens. The owner's
case differs: an opponent who leaves his far mines unguarded no longer blocks them, and a worked iron mine stays
claimed.

Tried and dropped (same bench): 4 Prehistoric patches instead of 3 changed nothing (no fourth patch in reach).
Growing the claim while the patch places are short of the food want: Copper 12/16 at 57.8, 109 citizens lost.

**Left:**
- Food: the Prehistoric food gatherers are 14.6 against a want of 19 (16.2 against 18 before, when the claims of
  the vanished patches stayed active on top of the plan's 3).
- `tower` memory: 1409 of the 2749 tower site-minutes are towers whose unit is gone from the world (the epoch's
  upgrade's conversion may give the tower a new unit, or it was destroyed out of sight; not checked). They are kept
  until their tile is seen empty, as the army's memory does.
- Home detection is "the nearest within 20 tiles", not the random map's own placement. A neutral mine nearer than
  the ring's second gold or iron can be taken for it.
- The `cp sites` trace line and the classic `EECPRMSettlePatches`/claim_needed still skip worked mines (hit points
  0), for both engines; changing them changes the classic.

### 8.35 The fifth playtest: builders under towers, crowded patches, far docks (2026-11-09, owner playtest and EE-SIM)

**The playtest** (`build/playtest_logs/4c115bc4-game-20261109-090006.log`; the owner against one boom AI, Continental
Small, difficulty 1). The AI reached the Stone Age at 24.8 minutes (the owner at 21.3), never the Copper Age, and
fell apart from about 49 minutes. The owner's chat lines and what the log shows:

- **"There are AI citizens 9 to one patch, waiting. Strictly no more than 6 to a patch."** From 12 minutes 16
  citizens were on two forage patches and the food income was 30-60 a 30 s (1-2 a second). The finders tested
  `gWorld.GetResourceUsers` against the mining limit (6 a bush): it counts the gatherers working a resource, not
  the ones sent there, so the walkers and the waiting were never counted. The 9 hunters' game ran out at 12
  minutes and they walked to the nearest bushes on their own. The 8 food gatherers the patches could not take went
  to wood (12 on wood against a want of 5, wood piled to 1200, iron to 600).
- **"Please get the settlements directly adjacent to all resource mines, not just randomly 3 tiles away."** The
  settlement took the first free tile in the rings 2..5 round the resource. A mine 8-10 tiles from the town centre
  counted the town centre as its drop site.
- **"He's pathfinding citizens into my base", "they built a settlement in my town", "it's like I'm being raided by
  citizens".** 119 citizens died: 78 were on a build job when first hit, 91 died to the owner's towers, 65 more
  than 40 tiles from home. A far iron mine (d52, never explored, by the owner's town) was claimed and released every
  10 s; in one of its claimed passes a settlement was queued there (x=85 y=67). The build queue kept it, and
  `EECPBuildManager::BuilderLost` sent a new builder for every one lost: up to 22 citizens at a time walked through
  the owner's town to it (`build.start` at 46, 65 and 76 minutes).
- **"A dock really, really far away from home", "they have fish at home! no dock at home".** The first dock stood
  59 tiles out: the SOL took the cheapest building that counts (a dock), and `FindShoreSite` takes the shore of
  the largest water body nearest the town, not the water by the town.
- **"Hunting really really far away from home".** The far hunting pass took game up to six drop-off distances out.
- **"They shouldn't respond to naval bombardment with melee units, only ranged".** The defend group's units not
  fighting closed in on the first target the group saw, a warship: 9 of 13 spearmen died on the shore in 20 s.
- **"Upgrading tower range is worth it for the AI".** No improvement could be bought at all on 64-bit Windows (below).
- **"No defence"** (46 min): the Stone Age army peaked at 13 of the 20 wanted; the food stock stayed under 50 for
  most of the Stone Age.

**The changes** (next engine and boom only; the classic engine is unchanged but for the improvement fix):
- **Build safety** (`EECPNextBuildUnsafe`, EECPNextView.cpp): a tile is unsafe when a seen enemy tower (remembered,
  or an armed building in sight) covers it (range + half + 3) or the straight way to it from the town centre (range
  + half + 1, every 2 tiles), or, more than 20 tiles from the town, enemy soldiers were seen within 9 tiles in the
  last 20 s or it is a danger zone. The build verb refuses `unsafe`; a queued building at an unsafe tile is
  dropped (`no_site` event); `BuilderLost` sends no new builder there; `AssignBuilders` sends none, takes its
  builders off, and gives up and refunds a foundation under a tower. The view's `blocked` is also `tower` when the
  way to a site crosses a tower's reach.
- **The town's side** (boom.lua `our_side`): a site is taken only when it is at most 85% as far from the town as
  from any enemy town seen, never within 25 tiles of one, and while no enemy town has been seen at most 38 tiles
  out (home sites always). Minerals are looked for to 45 tiles (was 60: on a Small map that is the other town).
- **Capacity** (the owner: "if there are only 12 spots for a resource, use only those 12"): the gatherers sent
  to a resource are counted by their gather goal (`EECPNextAssigned`), at most the mining limit a unit and 6 a
  forage patch in all (`EECPNextHasRoom`, used by FindFarm, FindResourceTarget and AssignGatherers; a unit of the
  centre with room is taken, not only the first). The extras (the game's own retargeting) are shed back to the
  idle list each assignment pass. `sites[].capacity`, `workers` (sent there) and `room`. Boom wants food only up to
  its places: the open patches kept (6 each), the game near the town (`view.hunt`, 3 hunters an animal, 6 hunters
  at most in the Prehistoric, 8 later) and the farms; the rest goes to the mines' places, then wood (to the plan's
  cap). In the Prehistoric no more citizens are trained than have work.
- **Hunting** within the claim range of the town centre (at least 24 tiles), 3 hunters an animal.
- **Drop sites by their resource** (`EECPNextAdjacentSite`): a settlement (or granary) at a site goes on the free
  tile nearest the centre's units (CanBuildAt keeps a one-tile margin, the gatherers' side), ties to the tile
  nearest the town. A drop site is "at hand" only within about 3 tiles (`kNextDropReach`), so the home mines 7-10
  tiles out get their own settlement. `view.wood` (the forest edge the wood gatherers go to): boom builds a
  settlement there when no site needs one and 4 or more are wanted on wood. Plan settlements 4/8/9 (were 3/6/7).
  In the bench 35 of 69 settlements stood at a gap of 1 tile, 7 at 2.
- **Docks by the home fish** (`EECPNextDockByFish`): the fish verb and `build(dock, near=town)` put a dock on the
  shore by the nearest fish within 30 tiles of the town that no dock of the player's serves (12 tiles), or none.
  The fish need not have been seen: they are the start's food as the home mines are (§8.34), and the AI never
  explores the water. Boom no longer builds a dock for the SOL.
- **Ships**: a group closes in on a land target, never a ship; with only ships to fight, its units not fighting
  walk to the group's place, or 12 tiles from the ship when the place is within 10 (`EECPNextAvoidShips`). Not
  measured: the sim's classic navy reaches the boom's town too seldom.
- **Tower range**: a new verb `improve(type, what)` (one level, paid and applied at once as the classic
  `GiveImprovements`, refused `cannot_afford` below the reserve, `requirements` with no level left). Boom buys its
  towers' range from the Stone Age once three towers stand; the data allows one level (+15%) a tower class.
- **Unit improvements on 64-bit Windows** (PORT_LEARNINGS): `EEUnitClassImprovementTable` read EEUCUpgradeTable 4
  bytes off (a 4-byte `long` for the 8-byte vtable pointer), so the maximum level was 0 and `CanBuy` refused every
  level for everyone, human or computer. macOS/Linux were right.

**Bench** (EE-SIM, Small, Medium, 2 AIs and the idle human, no cheats, 2 hours; boom against the classic engine,
seeds 26-41 in both seat orders, 32 games; the tree-rel data; before: this branch's base 66c3eb9):

| | Before | After |
|---|---|---|
| Stone Age (median min, reached) | 23.5 (32/32) | 24.5 (32/32) |
| Copper Age (median min, reached) | 56.5 (24/32) | 58.3 (26/32) |
| Prehistoric food gatherers / food a minute (after 5 min) | 17.8 / 96.6 | 14.8 / 91.9 |
| Citizens at the Stone Age | 33.8 | 32.5 |
| Stone Age food a minute | 121.9 | 128.5 |
| Citizens lost a game | 82.8 | 78.5 |
| ... on a build job when first hit | 4.2 | 2.4 |
| ... killed by towers | 5.8 | 1.1 |
| ... more than 30 tiles from the town | 5.6 | 1.3 |
| Fishing boats lost a game | 20.4 | 22.2 |
| Army at 30 / 40 min | 6.0 / 14.3 | 5.2 / 10.8 |
| Docks: distance from the town (median / most) | 11 / 66 | 10 / 29 |
| Tower range levels bought | 0 | 29 |

The sim's classic AI does not leave its far mines unguarded and towered the way the owner does, so the build and
tower losses are small in both columns; the playtest's 78 builder deaths came from one site the boom now never
claims (x=85 y=67, in the other town). Fewer Prehistoric food gatherers bring in about the same food (6.2 a
minute each, was 5.4): the citizens no longer wait at full patches, and the Stone Age comes about a minute later.
The Stone Age army is smaller at 40 minutes (10.8 against 14.3): the stock of wood rose (the food the patches could
not take went to wood first; sending it to the mines first is in the After column, 9.8 before that). Checks: the
same boom game twice gives the same trace (seeds 26 and 27, 3600 s: 4616 and 5043 lines); rush and balanced run
without script errors (seeds 26-27); the classic engine's traces (seeds 2-5, 1800 s, classic against classic)
are identical for seeds 2-4 and differ in seed 5 from 16 minutes, where a classic attack group buys improvements
(`GiveImprovements`, possible again on Windows; food 521 to 319, gold 220 to 18 at its creation).

Tried and dropped (16 games each, the same image): the sites anywhere out to 60 tiles again (Copper 58.5, 13/16,
the same within the noise; kept the owner's rule); no food cap in the script (the C++ capacity alone: Stone 24.0,
Copper 60.0, 15/16, 95 citizens lost a game).

**The Stone Age army** (a second step, after the table above; the owner: about 20 spearmen in the Stone Age to stop a
raid). The army did not fall between 30 and 40 minutes; it grew slowly (trained 9-10 in those 10 minutes, 2-4
lost). A spearman costs 30 food and 30 iron, and iron was the limit: in 31% of the Stone Age's minutes boom wanted no
iron gatherer at all (21% before §8.35), the iron stock stood at 10-25, and the spearmen came one per 30 iron
gathered. The give-way rule (food short of its minimum takes a gatherer from the resource with the most to spare)
counted the Copper Age's gold reserve as the larger lack, so iron gave way first, 12 gold gatherers against 0-1 on
iron. Now, while the army is short of what is wanted, iron gives way only when nothing else is left. Also past the
soldiers-first window, while the light army is short, the food gatherers fill their places and the soldiers' iron is
in stock, a citizen is trained only from the food above two soldiers' cost (alone this changed nothing: army
5.4 / 11.2 at 30 / 40 min). The same 32 games:

| | Before §8.35 | §8.35 above | With the army step |
|---|---|---|---|
| Army at 25 / 30 / 40 min | 2.6 / 6.0 / 14.3 | 2.1 / 5.2 / 10.8 | 2.3 / **8.4** / 13.5 |
| Stone / Copper (median min, Copper reached) | 23.5 / 56.5 (24/32) | 24.5 / 58.3 (26/32) | 24.5 / **55.0** (26/32) |
| Citizens lost a game (on a build job / by towers) | 82.8 (4.2 / 5.8) | 78.5 (2.4 / 1.1) | 79.1 (1.8 / 1.1) |

**Left:**
- The Stone Age army is still short of 20 at 30 minutes (8.4): two home iron mines hold 12 gatherers, and 20
  spearmen are 600 iron. A larger Prehistoric iron bank or a third iron site from the Stone Age's first minute would
  be the next step.
- The explore-and-fish mode for fishing boats: `FindFish` already takes the nearest fish with some left, seen or
  not, so drained boats sail on to far fish; a deliberate exploring boat (and keeping them off seen warships farther
  out) is not built.
- Far wood: `view.wood` is the forest point nearest the town centre (FindForestTarget), which moves as the edge is
  cut; the settlement follows only when it has no drop within 3 tiles.
- The improvement cost is taken at once; a human pays and waits at the building (queued improvements). The classic
  attack groups now buy improvements on Windows, as they did in the original.

### 8.36 Settlement first, and boom on the binary tree (2026-11-09, EE-SIM)

**The owner's rule:** "the first thing I do is build a settlement. require the AI to build a settlement before using a
node." No citizen gathers from a forage patch, mine or forest edge until a drop site stands by it; the first citizens
build the settlement, then gather there.

**Before** (main 300e36b): a site's drop was "at hand" within about 3 tiles (§8.35), but the finders sent gatherers to
any claimed site, drop or not. Counted per gatherer sample (the trips trace's new `nodrop=`: a gathered unit with no
finished drop site within 3 tiles plus the drop's half size plus 1; the before column used the first, looser version
of the count, any unit of the unit's resource centre, so it undercounts): 21% of the food, 81% of the wood and 52% of
the stone, gold and iron gatherer samples had no drop at hand.

**The changes** (next engine and boom only; the classic engine is unchanged):
- **The finders** (EECPResourceManager.cpp): FindFarm and FindResourceTarget take only a bush or mine with a finished
  drop site at hand (`EECPNextUnitDropReady`: a town centre, capitol or settlement, a granary for food; the unit of a
  centre with room *and* a drop is taken). The wood goes to the forest point nearest the town when it has one, else
  to the forest point nearest each finished drop site (`EECPNextWoodTarget`, 2 tiles more slack: the settlement
  stands off the edge). The hunters keep their rule (near home, §8.35): animals move, and the town centre is their
  drop. A wood gatherer the game moved on to a tree with no drop goes back to the idle list when an edge with a drop
  has trees (`EECPRMShedCrowds`). A resource by the player's own finished drop site skips the visibility test (below).
- **The citizens with no place help build** (`EECPRMHelpBuildDrops`): the idle ones the finders could not place go
  to the settlements and granaries going up, up to 4 builders a building, nearest first; when it stands they are idle
  by it and the finders send them there (`build.help` trace lines, 14 a game).
- **The verbs**: `gather` returns `queued` / `no_dropoff` (with `ready=` places) when fewer places have a finished
  drop than gatherers are wanted. `build(b_settlement, near="site")` places by the units of the centre that have no
  drop yet (a centre can hold gold and iron 6 tiles apart: its first unit's drop said yes for the iron). When the
  rings find no tile, or only one more than 2 tiles off, the nearest citizen not building walks to the site and stands
  there (`EECPNextState::mExplore`; AssignGatherers leaves it, 90 s at most): `queued` / `unexplored`, and the order
  comes again. The view: `sites[].ready` and `drop` per resource, `wood.ready`.
- **Why the walker**: `EETile::IsVisibleTo` is "in sight now", not "explored", and `EECanPlaceUnitOnTile` tests it
  for every tile of a footprint. A settlement ordered at a site nobody stood by went 5-6 tiles off (and never served
  its patch) or nowhere (`build.nosite_why`, TEST ONLY: 288 of 288 ring tiles refused). A home patch 7 tiles from the
  town centre read as unexplored all game, and the finders, which test it too, never sent a gatherer there (seed 31).
- **Boom**: each kept site the plan wants gatherers on whose resource has no drop (finished or going up), and the
  worked forest edge when no edge has one, gets its settlement: food first, then wood, then iron, gold, stone, nearest
  first, 2 going up at once, a spot not ordered again for 60 s (at once after `unexplored`); settlements 6/12/14 (were
  4/8/9). A patch or mine whose settlement found no room (`no_site`, 5 minutes) is passed over and the next one kept.
  With the patches short of the plan's 18 on food, hunting takes the rest (it was 6 hunters at most).

**Boom on the binary tree.** The binary tree's tables have no ids (every class is `dbobjects_<row>`): boom found no
`citizen`, so it never trained one, released every claimed site and asked for `copper_tower_upgrade`, refused as
unknown. The tree now carries `Data/db/dbobjects.ids` and `dbtechtree.ids` (`open_data.py build --binary` writes them,
`open_data.py ids TREE` adds them to an older tree), the ids the converter gives the text rows; `EEDbFindFile` reads
them with the `.dat`. Seeds 26 and 27 on both trees (boom against classic, 40 minutes): the boom traces are identical
line for line (701 and 656 `next` lines). The main checkout's `build/open_data/tree-binary` needs a rebuild (the
converter hash changed) or `open_data.py ids`.

**Bench** (EE-SIM, Small, Hard, 2 AIs and the idle human, no cheats, 2 hours; boom against classic, seeds 26-41 in
both seat orders, 32 games; the open tree; before: 300e36b with only the `nodrop` count added):

| | Before | After |
|---|---|---|
| Stone Age (median min, reached) | 26.0 (32/32) | 25.5 (32/32) |
| Copper Age (median min, reached) | 55.5 (23/32) | 57.5 (21/32) |
| Prehistoric food gatherers / food a minute (after 5 min) | 13.7 / 83.1 | 14.1 / 85.3 |
| Trip, resource to the nearest drop (tiles: food / wood / stone / gold / iron) | 7.6 / 5.4 / 5.6 / 5.7 / 6.3 | 6.3 / 4.4 / 3.3 / 3.4 / 3.2 |
| Gatherer samples with no drop at hand (food / wood / minerals) | 21% / 81% / 52% | 0.2% / 1.1% / 0.02% |
| Army at 25 / 30 / 40 min | 1.6 / 6.8 / 12.5 | 2.5 / 7.7 / 15.2 |
| Citizens lost a game | 102.3 | 93.6 |
| Stone Age gatherers on wood / iron (wanted) | 9.3 / 10.3 (7.1 / 10.9) | 15.1 / 8.8 (8.4 / 10.4) |
| Still in the Prehistoric at 30 min | 6 games | 7 games |
| Stone Age with no soldier | 1 game | 1 game |

The rule holds: what gathering is left without a drop at hand is the game's own retargeting (a gatherer taking the
next bush or tree) between two assignment passes. Every mineral trip is about half as long, and the Stone Age army is
larger (15.2 at 40 minutes). The epochs come at about the same time: the first 30-60 s go to the home patch's
settlement (on most maps the town centre is not "at hand" for the start patches), and the Stone Age's iron waits for
its settlements while the citizens it cannot take go to wood.

**The three questions:**
- **Food 18 in the Prehistoric**: 14.1 on average (13.7 before). Where it stays at 12 the third patch's settlement
  finds no room (`no_site`, every ring tile refused even with the walker there: patches 25-30 tiles out in rough
  ground) and too few animals are near. With one home patch (seed 30, the second 27 tiles out) the hunters now fill
  the rest.
- **Seed 27** (still in the Prehistoric at 30 minutes with 28 citizens): the boom seat p2 (27a) is raided by the
  classic Hard AI from minute 3 (citizens lost at 3-6 minutes, threats every few minutes); it reaches the Stone Age at
  35.5 minutes (34.5 before). The other seat (27b) reaches it at 21.5 (23.0 before). Not solved: a defence problem.
- **Stone Age with no soldier** (seed 30 before, 31b after): no iron site with a settlement (both home iron mines'
  settlements refused `no_site`, no other in reach), so no iron was wanted and no spearman could be paid. Not solved.

**Left:**
- The wood is over-gathered in the Stone Age (above): the citizens the iron's sites cannot take yet go to wood
  (`AssignIdleGatherers`). Holding them for the iron's settlement, or a third iron site, is the next step; so is a
  placement that can use tiles the walker sees but the site's rings do not reach (the far third patch, the iron of
  31b).
- Walkers to far sites can die; they are not counted separately.

### 8.37 The Age of AI port, first bench (2026-11-09, EE-SIM)

`ai/ageofai/` is a Lua port of Grille's *Empire Earth: Age of AI* (GPL-3.0, with the author's permission;
`ai/ageofai/UPSTREAM` pins upstream commit 0ac6495 and maps each upstream file to its module). What it keeps are the
decisions: the budget (an epoch reserve; a military account fed by 30% of the start stock and 40% of the income, 65%
from the Copper Age, with the hoard above reserve + 3000 / 200 free for the army), the worker targets by shortfall
over the measured rate per gatherer (food's shortfall counts the citizens' own food, wood the buildings', the
minerals a standing margin; a food floor before farms), the citizens wanted (the copper plan's workforce before the
saving, the mines' places from the Copper Age), two core towers by the town centre then one a minute at the front
(towards the nearest enemy building known) and the outposts, drop sites before gathering (the engine's rule, §8.36),
the production buildings and army_plan BUILD, granaries and fields from the Copper Age, the gathering upgrades, the
epoch as soon as it is paid (a deviation: upstream's "independent" timer is 50-70 minutes an epoch for games of
200-400 population a player). The army is one force on the per-unit interface (docs/AI_LUA_UNITS.md, lib.groups):
the home guard (6 + 4 x epoch, or 35% of the military room) of the strongest soldier the account pays for; the
force at its station pushes at the attack time (20-40 minutes, then every 20-25) when it beats the enemy known at
the nearest enemy building by 130%, along `ai.route` over the threat and lost layers to a staging point, is judged
every 4 s against the enemy at its middle and its destination and comes home as a convoy when losing twice running
or bled to half, takes the next building when the first is down, answers a raid at an own building with the whole
force when it beats what is known there, musters beyond the base when the base cannot be held, and sends a scout.
What upstream patched into the original exe stands in as C++ helpers: honest vision is the enemy memory, the danger
map the threat layer, DANGER_LOST the lost layer, `ar_route` is `ai.route`. Not ported yet: the navy, landings,
other islands, towns, siege, counterfire, garrison upgrades, diplomacy.

**Bench** (sim_bench, Small, Hard, no cheats, 45 minutes, seeds 2-17 in both seat orders: 32 games a pair; the
means with their standard errors; "army" is the land soldiers):

| | Stone Age (min, reached) | Copper | citizens 30 / 40 min | army 30 / 40 min | citizens / army at the end | wiped out |
|---|---|---|---|---|---|---|
| Age of AI against classic | 30.9 +- 0.6 (30/32) | 0 | 33.2 / 38.3 | 5.5 / 10.4 | 41.7 / 12.0 | 0 |
| classic Hard in those games | 26.1 +- 0.5 (32/32) | 0 | 37.4 / 58.3 | 10.7 / 10.2 | 70.1 / 3.8 | 0 |
| Age of AI against boom | 29.2 +- 0.5 (31/32) | 0 | 36.8 / 43.2 | 6.9 / 8.6 | 48.6 / 8.7 | 0 |
| boom in those games | 22.8 +- 0.5 (31/32) | 2 | 42.4 / 59.2 | 9.7 / 16.2 | 65.9 / 18.2 | 0 |
| boom against classic (§8.36's bench on this build, 32 games) | 27.4 +- 1.0 (32/32) | 2 | 33.5 / 43.3 | 5.3 / 11.6 | 49.3 / 11.8 | 0 |

No game was decided in 45 minutes (no wins, no losses, no script errors). The first port is behind boom in the early
economy: with upstream's copper plan the saving starts at 40 citizens or 18 minutes into the epoch (boom saves from
32), the Prehistoric home guard of 6 and the first barracks take food, iron and wood the citizens would have used,
and the worker plan's wood and mineral targets start earlier than boom's food-first order. The force does what it
should: the pushes wait at home while the enemy known at the target outweighs it (the classic Hard base is 10-20
times the force by 40 minutes), raids at the outposts are answered and the force comes home when losing; nothing
of upstream's late game (farms, the army's growth from the income share) shows in 45 minutes.

**The opening tuned** (seeds 2-17, one seat order, 16 games a pair; the deviations are marked in
`ai/ageofai/settings.lua`):
- *Variant 2*, the owner's opening as upstream's playtester's (`early_army`: no soldiers in the first epoch, the
  towers and the militia hold the town, the home guard from the Stone Age; the saving from 34 citizens instead of
  40; 60% of the workers on food before farms, up to 24): against boom the Stone Age at 23.1 +- 0.7 (boom 22.7), but
  with the saving starting again at once in the Stone Age (the copper plan holds the next epoch's cost from 34
  citizens on) the citizens stalled at 45 at 40 minutes (boom 63) and the army stayed at 0-1 all game (the Copper
  Age's gold and iron reserve left the account nothing to spend); against classic Stone 26.9, 34.6 citizens at 40.
- *Variant 3* (kept): the Stone Age grows to 60 citizens before the Copper Age is saved for (`stone_workforce`):

| 16 games each | Stone Age (min, reached) | citizens 30 / 40 min | army 30 / 40 min | citizens / army at the end | wiped out |
|---|---|---|---|---|---|
| Age of AI against boom | 23.1 +- 0.7 (15/16) | 47.3 / 59.2 | 8.9 / 11.2 | 59.5 / 9.2 | 0 |
| boom in those games | 22.7 +- 0.7 (16/16) | 42.0 / 60.9 | 8.8 / 14.4 | 68.8 / 17.1 | 0 |
| Age of AI against classic | 26.9 +- 1.0 (14/16) | 36.6 / 46.1 | 4.4 / 14.8 | 50.4 / 15.3 | 0 |
| classic Hard in those games | 25.2 +- 0.1 (16/16) | 38.2 / 61.4 | 11.6 / 11.7 | 76.2 / 4.5 | 0 |

The port is level with boom in the first 45 minutes (the Stone Age half a minute later, more citizens at 30
minutes, as many at 40, a smaller army at the end: the home guard's account is 40% of the income) and ahead of boom's
own numbers against the classic raider (Stone 26.9 against 27.4, citizens at 40 minutes 46 against 43, army 14.8
against 11.6). What upstream is strong at - the force's pushes, farms, the army's growth from 65% of the income -
lies past these 45 minutes; a longer bench (90 minutes, the Copper and Bronze Ages) is the next measure, with the
navy, towns and siege still to port.

**The tactics, the navy and the longer benches (2026-11-09, the second checkpoint).** Ported since the first bench
(`ai/ageofai/tactics.lua`, `navy.lua`, `ai/lib/navy.lua`; UPSTREAM's map): the siege controller, the settlement
upgrades (spare citizens only: a first draft on an island put the idle workforce into the settlements), counter-fire
in the micro step, and the navy (a sea scout over `ai.unexplored{sea = true}`, transports and a warship from the
account, the landing at the weakest beach when the force at home beats it by 1.4, the landed group on lib.groups).
The 45-minute numbers are unchanged (against boom Stone 23.1 against 22.7, citizens 47/58 against 42/61 at 30/40
min, army 8.9/11.0 against 8.8/14.2; against classic Stone 27.1, citizens 35/43, army 4.0/13.9; no defeats).

| 90 minutes, 16 games each | Stone (min, reached) | Copper (min, reached) | Bronze reached | citizens / army at the end | defeats (wiped) |
|---|---|---|---|---|---|
| Age of AI against boom | 24.6 +- 1.6 (16/16) | 63.5 +- 2.1 (14/16) | 9/16 (77-90 min) | 89.2 / 33.2 | 0 |
| boom in those games | 22.7 +- 0.7 (16/16) | 57.9 +- 2.6 (15/16) | 9/16 (60-88 min) | 107.3 / 17.7 | 0 |
| Age of AI against classic Hard | 29.1 +- 2.3 (15/16) | 64.0 +- 3.2 (7/16) | 0/16 | 28.2 / 6.2 | 4 (1) |
| classic Hard in those games | 25.2 +- 0.1 (16/16) | 63.3 +- 0.9 (15/16) | 5/16 (77-89 min) | 166.8 / 11.9 | 0 |
| Small Islands, Age of AI against classic (8 games) | 25.9 +- 1.0 (8/8) | 58.9 +- 2.2 (7/8) | - | 73.4 / 79.1 | 0 |
| classic Hard in those games | 47.1 +- 6.4 (7/8) | 75.0 (2/8) | - | 39.4 / 28.6 | 0 |

- **Against boom** the port keeps pace to the Bronze Age with twice the army at the end (33 against 18) and fewer
  citizens (89 against 107); boom's Copper comes 5-6 minutes earlier. The force pushed 10 times in 16 games (0.6 a
  game; upstream measures 1.6 on huge maps) and came home "losing" 37 times: the pushes meet boom's towers and army
  and turn round as the rule says; nothing was decided.
- **Against classic Hard** the port **loses 4 of 16 games in 90 minutes** (one seat wiped out), with 28 citizens
  left on average against the classic's 167. The force answered 112 raids and came home losing 88 times: the
  classic's late waves (Copper-Bronze armies of 30+) grind the home guard down at the outposts and the base, and
  the economy follows. Boom's own 90-minute result against classic is not measured yet (its 45-minute games have
  no defeats); the next steps for the port are the muster and the home defence against a wave the force cannot
  beat (upstream's hold_town, bunkers), fewer raid answers at the outposts (upstream's AR_RAID_BAND 3 holds, but
  our outposts are raided by whole waves), and towers at the fronts earlier.
- **On Small Islands** the port is far ahead of the classic AI (Stone 26 against 47 minutes, Copper 7/8 against
  2/8, 73 citizens and 79 soldiers against 39 and 29): the classic AI's economy stalls on islands. The landing
  worked in 2 of 8 games (40 boarded, 19-23 ashore each time; the landed group was beaten), was given up in 2 on
  the unload (the transports found no spot in 4 minutes: upstream's unload retries were engine fixes, still to do
  here) and never sailed in 4 (the force never 1.4 x the beach known). L6's gate (10/10 landings) is not met.
- **The save check:** with `EE_AI_SAVE_CHECK=1` the units block decodes and encodes to the same bytes at every
  autosave (`next.savecheck units=same`); the Lua shadow step is skipped for scripts on the per-unit interface
  (their commands cannot run twice) - a real load stays a manual test.

**Surviving the classic's waves (2026-11-09, the third checkpoint).** The 90-minute defeats against classic Hard
came apart in the traces: half the lost seats died in the *Prehistoric* - a soldierless town (the `early_army`
deviation) with two towers, raided from minute 3, its food income down to 15-30 a minute while the copper plan held
the Stone Age's 1122 food (seed 4 reached 1095 at minute 50 and died with it); the rest fell in the Stone Age when
the force answered raids at the outposts, came home "losing" and was ground down. Changes (settings.lua marks the
deviations): the recovery rule (no saving while the citizens are under 75% of their most, or while the food income
is under 60 a minute and the epoch not paid: recover_economy); hunting when the patches fail (food gatherers at work
under half the target and the income under 90); upstream's `early_soldiers` alternative made reactive (four
soldiers in the first epoch from minute 5 once enemy soldiers were seen: four from the start cost 2.5 minutes of
the Stone Age on quiet maps, 26.0 against 23.6); the force holds the town itself at 12 tiles under a threat instead
of the whole claimed land; raids answered only within 30 tiles of home (or a band of 5) when the force beats what
is known there by 1.3; the muster at 4 enemies and 1.5 x (upstream 8 and 2 x); front towers from 8 citizens, an
unsafe spot when no safe one.

| 90 minutes against classic Hard, 16 games | Stone | Copper | citizens 30 / 40 | army 30 / 40 | citizens / army at the end | defeats |
|---|---|---|---|---|---|---|
| Age of AI, second checkpoint | 29.1 (15/16) | 64.0 (7/16) | 35 / 43 | 4.0 / 13.9 | 28 / 6 | 4 (1 wiped) |
| Age of AI, four soldiers from minute 5 | 29.3 (15/16) | 67.8 (11/16) | 38 / 48 | 8.2 / 17.6 | 49 / 23 | 1 |
| Age of AI, the four once enemies are seen (kept) | 29.1 (14/16) | 65.3 (10/16) | 38 / 47 | 8.1 / 15.6 | 42 / 22 | 2 |
| boom (the reference) | 26.5 (16/16) | 59.1 (12/16) | 37 / 46 | 6.7 / 13.3 | 60 / 13 | 2 |
| classic Hard in boom's games | 25.5 | 61.9 (16/16) | 36 / 53 | 12.6 / 11.8 | 157 / 12 | 0 |

Against boom at 45 minutes the kept variant is level (Stone 23.3 against 22.7, citizens 47 / 60 against 43 / 62 at
30 / 40 minutes, army 10 / 17 against 11 / 17, no defeats). The target - no defeats in 16 and ahead of boom at 90
minutes - is not reached: the port now loses as often as boom (2 of 16) with a larger army and fewer citizens; the
classic Hard AI ends with 160+ citizens either way. What is left here is upstream's hold_town / bunkers (the
defenders drawn out one by one) and the citizens' shelter, and an economy that keeps growing under raids (the
finders refuse raided sites; the idle citizens then wait).

**Islands, the unload retries** (10 games, Small Islands, 90 min): a transport whose unload ended with cargo aboard
is ordered again at once at the next shore tile (upstream's engine fix, done in `lib/navy.lua` without an engine
change): the one landing that sailed put all 40 ashore (two had failed on the unload before). But only 2 of 10
games boarded at all: in 7 the sea scout never found an enemy building - `ai.unexplored{sea = true}` runs out of
points after a few (the grid's water body test; `ai.explore` on a boat then shows nothing) - and the landing's
judge waited in the rest. The economy on islands is far ahead of the classic AI's (Stone 27.5 against 50.5, Copper
10/10 against 2/10, 77 citizens and 82 soldiers against 31 and 32 at the end), with two faults seen: `b_farm` near
the town is refused `no_site` on the island maps (229 times in one game; the farms need a granary-side placement),
and 54 idle citizens at the end for want of food sites. L6's gate stays open.

**A learning:** `--data open` falls back to the main checkout's tree; a bench that runs while that tree is rebuilt
fails every game at start (`EETerrainColorTable.cpp (81): Verification failure`, no report). A worktree keeps its own
tree (`open_data.py build build/open_data/tree`, 5 minutes).

**Islands, farms and towns (2026-11-09, the fourth checkpoint).** Found and changed: a granary lays its eight
fields as foundations, so `build b_farm near=town` is right (the classic FindSite finishes them; rings round a
granary are refused) - the island games had no fields because they had no *granary*: the town's rings were full
and `near=town` refused it 50-180 times a game. A building refused `no_site` near the town now goes up through the
per-unit interface (`build_direct`: `ai.find_site` 4-20 tiles from the town centre, the three nearest citizens
build it through `ai.build` and stay the script's until it stands - released at once, the runners took them back
and 17 granary foundations stood empty in one game). The sea scout falls back to the unexplored tiles of a coarse
grid the boat can sail to (`ai.route{sea = true}`) when `ai.unexplored{sea = true}` runs dry. Towns (towns.c, a
light port): from the Copper Age the claim grows to its most while the claimed mines are full, and a far mineral
site's drop (30+ tiles) is a town centre when the wood allows.

| | Stone | Copper | citizens 30 / 40 | army 30 / 40 | end citizens / army | defeats |
|---|---|---|---|---|---|---|
| Small Islands, Age of AI against classic, 10 games, 90 min | 25.6 (10/10) | 59.3 (10/10) | 44 / 61 | 9.4 / 38.8 | 84 / 74 | 0 (classic lost 1) |
| classic in those games | 46.8 (9/10) | 80.8 (4/10) | 21 / 22 | 9.5 / 14.8 | 37 / 38 | 1 |
| Age of AI against classic Hard, 16 games, 90 min | 29.6 (15/16) | 65.8 (9/16) | 37 / 48 | 7.9 / 17.2 | 48 / 23 | 1 (1 wiped) |
| boom's reference (above) | 26.5 | 59.1 (12/16) | 37 / 46 | 6.7 / 13.3 | 60 / 13 | 2 |
| Age of AI against boom, 16 games, 45 min | 24.6 | - | 46 / 59 | 9.6 / 17.3 | 62 / 19 | 0 |
| boom in those games | 22.8 | 43.3 (3/16) | 43 / 60 | 11.4 / 17.1 | 67 / 20 | 0 |

- **Landings:** 6 of 10 island games boarded and all 6 put their soldiers ashore (30/30, 55/60, 38/40, 10/10,
  18/45, 30/30; the unload retry holds), and the classic AI lost one of those games - the port's first decided
  game. 4 games never boarded (no enemy building found across the water, or the force never 1.4 x the beach).
  Granaries stand in 8 of 10 island games now; 3 games still end with 25-41 idle citizens. L6's gate (10/10) is
  not met.
- **Against classic Hard at 90 minutes** the port loses 1 of 16 (boom's reference 2) with a larger army (23 against
  13) and fewer citizens (48 against 60); the 45-minute pace against boom is level. The target (no defeats, ahead of
  boom) is not reached: the classic Hard AI ends every game with 160+ citizens, and the port's economy under its
  waves still shrinks where boom's holds.
- **Left:** a directly placed foundation vanishes within 30 s in some games (seed 6: a new id at the same tile every
  order - something removes an unbuilt foundation the build manager did not queue); the idle citizens on islands
  (the finders have no food site for them until the fields stand); the 4 island games without a landing; hold_town
  and the citizens' shelter.

**The vanishing foundation and the fields (2026-11-09, the fifth checkpoint).** The directly placed granary
"vanished" because `ai.build`'s foundation is a *pending* unit of the player until a builder reaches it - not in
the world's unit lookup, so `ai.unit(id)` was nil, the script released its builders and ordered another at the
same tile (the pending one is not on the map, the placement test passed): the interface now finds pending units
(the record says `pending = true`). Found next: a granary placed by a human's build order comes without its eight
field foundations (the AI build manager lays them with its own granary), and `build b_farm` only finishes a
foundation that exists; `ai.replant(granary)` (the human's replant command) lays them. The direct granary is the
first one only, 8 tiles from any other. Also: the muster is upstream's hold_town now (6 tiles behind the base,
1.3 x), idle citizens for want of a food site send the gatherers hunting, two more boats a minute and the claim
wider.

| | Stone | Copper | citizens 30 / 40 | army 30 / 40 | end citizens / army | defeats |
|---|---|---|---|---|---|---|
| Small Islands, Age of AI against classic, 10 games, 90 min | 25.6 (10/10) | 59.0 (10/10) | 44 / 61 | 9.4 / 38.9 | 87 / 80 | 0 |
| classic in those games | 46.8 (9/10) | 78.8 (5/10) | 21 / 22 | 9.5 / 14.8 | 38 / 36 | 0 |
| Age of AI against classic Hard, 16 games, 90 min | 28.8 (15/16) | 68.7 (10/16) | 37 / 49 | 7.9 / 17.9 | 49 / 22 | 1 (boom 2) |
| Age of AI against boom, 16 games, 45 min | 24.6 | - | 46 / 58 | 9.6 / 17.4 | 62 / 19 | 0 (boom 22.8, 43 / 60, 67 / 20) |

Landings in 5 of 10 island games (30/30, 30/30, 55/60 and a second wave of 22, 23/40, 10/10 ashore); farmers 0-21 a
game at the end, but 4 games still end with 35-58 idle citizens (one granary of eight fields does not feed a
hundred citizens; a second granary placed by the first vanished before the 8-tile rule - more granaries and the
fish are the next step). Against classic Hard at 90 minutes: 1 defeat of 16 as before (boom 2), citizens 49 / army
22 at the end (boom 60 / 13); the 45-minute pace against boom level. The targets (L6's 10/10 landings; no defeats
and ahead of boom at 90 minutes) stay open.

**The sixth checkpoint (2026-11-09): the bait, a crash, the islands.** The games lost against classic Hard at 90
minutes lose their *citizens*, not their army: in seed 14 the force of 42 stood while 40 citizens died in 20
minutes to bands of 2-5 raiders at the outskirts (the force answered 40 raids and came home each time). Upstream's
`tower_bait` (early.c) ported into the micro step: a citizen hit by a seen enemy soldier walks to 2 tiles behind
the nearest own tower or town centre within 25 tiles and is the script's for 25 s (the runners would send it
straight back). A first version took every citizen within 6 tiles of the raider in every epoch: the two worst
seeds kept 57 citizens instead of 9-12, but the 16-game bench lost 4 (the economy stood still, Stone 13/16). As
upstream has it - the citizens hit, up to the Copper Age - it loses 2 of 16 (one wiped) with 52 citizens and 24
soldiers at the end (the round before, without the bait: 1 lost, 49 / 22; boom's reference 2 lost, 60 / 13). The
difference is one game either way; kept as upstream's.

A crash found and avoided: a granary placed through the human's build message (`ai.build`) lays its field
markers with it; where one of them cannot be placed (an island town) the granary stays a *pending* unit and the
citizen sent to build it faults in `EEUGBuildRepair::BeginRepair` -> `EEStepAsideFromSite` (Small Islands seed 11
at 49 minutes, twice; PORT_LEARNINGS). The port no longer places granaries that way (`direct_granary = false`,
and `replant_fields = false`: `ai.replant` from the AI's update showed the same removed markers); the engine guard
is still to do. The sea scout gets a warship from the Stone Age even before an enemy is known (the fishing boats
sent to look were sunk).

| | Stone | Copper | citizens 30 / 40 | army 30 / 40 | end citizens / army | defeats |
|---|---|---|---|---|---|---|
| Small Islands, Age of AI against classic, 10 games, 90 min | 25.6 (10/10) | 59.4 (10/10) | 44 / 61 | 9.0 / 38.4 | 84 / 80 | 0 |
| classic in those games | 41.0 (7/10) | 78.0 (2/10) | 21 / 22 | 9.7 / 14.8 | 30 / 34 | 0 |
| Age of AI against classic Hard, 16 games, 90 min | 30.1 (15/16) | 62.2 (8/16) | 38 / 52 | 8.5 / 17.5 | 52 / 24 | 2 (1 wiped) |
| Age of AI against boom, 16 games, 45 min | 24.4 | - | 47 / 61 | 9.8 / 16.4 | 64 / 18 | 0 (boom 22.8, 43 / 61, 65 / 21) |

Landings in 6 of 10 island games (one of them twice); 4 games never board; 2 games end with 35-49 idle citizens
(no granary where the town's rings are full, now that the direct one is off). Open: the engine guard for a pending
site's builder, granaries on crowded islands, enemy discovery in the 4 games, 0 defeats against classic Hard,
expand.c.

**The crash fixed (2026-11-09, the seventh checkpoint).** The fault of Small Islands seed 11 at 49 minutes is the
engine's: a building site whose footprint reaches off the map (a granary at the island's edge, placed through the
human's build message - `EECanPlaceOnTile` took the tile) makes `EEUGBuildRepair::BeginRepair` ->
`EEStepAsideFromSite` and `IsFootprintClear` -> `EEGetMovableTileUnits` dereference `GetTile` = NULL when the
builder arrives. A human's build order runs the same code: a game bug, not the AI's. Fixed in C++ (the three
functions skip a tile off the map; `EEStepAsideFromSite` a unit that is not valid, has no tile or no stats;
`BeginRepair` clears no footprint for a site not placed yet), reproduced with the seed-11 game before and after
(PORT_LEARNINGS has the command). With it the port's direct granary and `ai.replant` are on again.

| | Stone | Copper | citizens 30 / 40 | army 30 / 40 | end citizens / army | defeats |
|---|---|---|---|---|---|---|
| Small Islands, Age of AI against classic, 10 games, 90 min | 25.6 (10/10) | 59.4 (10/10) | 44 / 61 | 9.0 / 38.4 | 87 / 78 | 0 |
| classic in those games | 41.0 (7/10) | 77.3 (3/10) | 21 / 22 | 9.7 / 14.8 | 30 / 32 | 0 |
| Age of AI against classic Hard, 16 games, 90 min | 30.1 (15/16) | 62.2 (8/16) | 38 / 52 | 8.5 / 17.5 | 54 / 24 | 2 (1 wiped) |
| Age of AI against boom, 16 games, 45 min | 24.6 | - | 46 / 60 | 9.9 / 17.8 | 63 / 19 | 0 (boom 22.8, 43 / 61, 65 / 20) |

Islands: 10 of 10 games run (no fault), landings in 7 of 10, granaries in 8 of 10 with 2-17 farmers at the end; 3
games still end with 32-49 idle citizens (one of them with no granary at all). Against classic Hard 2 defeats of
16 as the round before (boom 2), 54 citizens and 24 soldiers at the end (boom 60 / 13). Open: the idle citizens of
the island games, enemy discovery in the 3 games without a landing, 0 defeats, expand.c.

**The eighth checkpoint (2026-11-09): the islands' idle citizens, a smaller step.** A directly placed foundation
that vanishes within a minute blocks its tile for ten minutes and a direct order comes at most every two (seed 11
had ordered a second granary at the same tile 68 times); a granary queued at the build manager for three minutes
with no foundation goes the direct way; the sea scout's fallback takes the far side of the map from the town
first. Small Islands (10 games, 90 min): 10/10 run, landings in 6 (one fewer than the round before: the judge),
0 defeats, 88 citizens and 82 soldiers at the end against the classic's 30 and 33; the direct granary churn is
gone but a second granary still rarely stands, and seed 2 still ends with 49 idle citizens (its granary orders
sit in the build manager's queue without a foundation, and the queue count the stall rule reads drops when the
manager gives up). Against boom at 45 minutes level as before (Stone 24.6 against 22.8, citizens 46 / 60 against
43 / 61, army 9.9 / 17.8 against 11.3 / 18.4, no defeats). Not advanced this round: 0 defeats against classic Hard
at 90 minutes (2 of 16, as boom), expand.c (other islands).

**The ninth checkpoint (2026-11-09): 120 minutes, and the bunkers.** The first 120-minute benches (Small, Hard, no
cheats, seeds 2-17; the games that started after the bunker port below include it):

| 120 minutes, 16 games | Stone | Copper | Bronze reached | end citizens / army | won / lost (wiped) |
|---|---|---|---|---|---|
| Age of AI against classic Hard | 30.1 (15/16) | 62.2 (8/16) | - | 38 / 19 | 0 / **7 (5)** |
| classic Hard in those games | 25.4 (16/16) | 59.4 (16/16) | - | 173 / 31 | 0 / 0 |
| Age of AI against boom | 24.6 (16/16) | 66.1 (16/16) | 12/16 (73-111 min) | **139 / 64** | 0 / 0 |
| boom in those games | 22.8 (16/16) | 53.7 (15/16) | 13/16 (65-104 min) | 102 / 25 | 0 / 0 |

- **Against boom the late game turns:** level to the Copper Age (boom's Copper 12 minutes earlier), the port ends
  with 139 citizens and 64 soldiers against boom's 102 and 25 - upstream's growth (the mines' places, 65% of the
  income to the army from the Copper Age) shows after 90 minutes. The force pushed 23 times in 16 games and
  finished nothing: every game ran to the cap.
- **Against classic Hard the port collapses between 90 and 120 minutes:** 7 of 16 lost (5 wiped out), 38 citizens
  on average at the end against the classic's 173 and an army of 31. The classic Hard AI's Copper-Bronze waves
  outgrow the home guard, the force never pushes (the classic base is 2-3 times the force at the nearest known
  building, by upstream's 130% rule), and the economy dies at the outskirts.
- **Bunkers** (bunker.c) ported: from the Copper Age a Fortress by the drop site nearest the base once citizens were
  hurt three times in two minutes; the citizens hit go into the nearest finished fortress within 15 tiles
  (`ai.load`: the fortress is a transport class, `IsTransport`), let out when it is quiet 8 s. The first bench found
  the fortress refused by the build verb and `ai.build` (`bad_argument`: `EEUCFortress` derives from
  `EEUCLandTransport`, `IsBuilding()` is false; `IsLandBuilding()` is what it answers): both accept it now, and
  on seed 5 the fortress stood and sheltered the citizens hit five times (one unload). Without a fortress the
  90-minute classic bench was byte-identical to the round before (2 lost of 16); with it not benched yet.
- **What finishes a game:** nothing of the port yet. Upstream's own measures count kills, losses and buildings
  razed, not games won; its pushes go at the nearest known building when the force beats what is there by 130%,
  and against a base that outgrows the force this never comes. To decide games the port needs what upstream has
  not got either at this population: a push rule that takes outposts and raids the economy when the main base is
  too strong (the mine raids of armies.c, siege at the walls), and the army share of the economy raised once the
  epoch pace is secured.

**The tenth checkpoint (2026-11-10): the whole of upstream ported, performance later.** The owner's direction changed
mid-round: port every remaining upstream file faithfully first, bench afterwards. Before that, the economy work of
the 120-minute analysis went in (commit 67acc22): the reserve from farms keeps only what the income cannot bring back
(econ.c spend_reserve), the saving holds once started, and the Prehistoric patch cap lifts while the patches' room is
short (seed 2 against classic Hard had sat 50 minutes with 18 food places for 32 gatherers wanted and never reached
the Stone Age; after the fix the Stone Age came at 23-26 minutes in both debug games). The reference bench of boom
against classic Hard at 120 minutes (16 games): boom won 0, lost 5 (4 wiped out), 47 citizens at the end against
classic's 169; the port with the fortress (aoa15, contaminated mid-run by a script edit): lost 8, 13 citizens at the
end. The cheating-classic reference benches the owner asked for were started and stopped with the direction change:
they run after the port is complete (`--set "gameoptions.difficulties.hard.ai_cheats=[cost, reveal, epochs, extra,
popband, catchup, retire]" --cheats none/none/all --personality balanced,balanced,classic`).

The engine side first: `ai.wall`, `ai.gate`, `ai.lock`, `ai.tribute`, `ai.trade`, `ai.diplomacy` (AI_LUA_UNITS §11;
the wall tech is never "available" to a computer player the production way, the classic wall manager never asks, so
the commands check the tech node and the cost only). Then the port, one upstream file (or group) per piece, each in
its own worktree, smoked (two runs of seeds 2 and 5, identical traces, no script errors) and merged `--no-ff`:
- expand.c and towns.c -> `towns.lua` (2,800 lines: resource towns, forward bases, the islands' outposts through
  transports with the fog-honest scouting of an island by its transport; `expand_islands = 1` is the one deviation,
  outposts on so the port settles free islands at all);
- attack.c -> `attack.lua` (the weak spots, the breaches through a known wall away from the towers, the siege's
  shelling target and firing position, wave_retarget, forces.c's enemy_objective; the leash, the aircraft and the
  rams' rules in lib.groups' scorer);
- navy.c, fleet.c, landing.c -> `navy.lua` rewritten and `fleet.lua` (navy_detect, the navy account, navy_pick's
  warship mix, the docks, the fleet led as one: massing under the towers while weaker, the strike at crit and
  1.25, the split at 1.5, hunters, raiders, patrol, the fishing boats guarded; the landing's plan, ships,
  escorts, bridgehead, waves and return). On Small Islands seed 11 the fleet massed, struck at ratio 30 and split;
  no landing came because the classic enemy's buildings were never sighted from the sea in 90 minutes;
- diplo.c -> `diplo.lua` (answer war, the opinion model, peace_score, the blocks and their veto, the vassals, the
  capitulation with its promise paid by ai.tribute, the surrender; the interface knows only my own stance, so a
  player turning hostile is read from its units hitting mine: AI-to-AI peace needs `hostile`/`allies` on
  `ai.players` and an offer channel);
- mass.c, armies.c's detachments, skirmish.c -> `mass.lua`, `detach.lua`, `skirmish.lua` (all off as upstream:
  army_mass 0, mine_raids false, skirmish 0; switched on in scratch runs they gathered reinforcements under the
  rally tower and released them as groups, ran mine raids and hit-and-run end to end);
- airstrike.c, stats.c -> `air.lua`, `stats.lua` (the bombers' wing of 8, trained while a position needs it, the
  run on the shooters' spot shared with skirmish.lua; `tests/ai_units/airwing.lua` spawns an airport, Gothas, a
  tower and an AA gun: the wing flew and took the tower to 75 hp, then its landed bombers could not be ordered out
  again: `ai.attack` refuses an aircraft inside its airport - an engine gap; st_ratio with what `ai.class` exposes:
  the damage-class and hit-factor tables are not);
- defence.c's walls -> `walls.lua` (the front wall 3 tiles before two front towers with the 8-direction snap, the
  gate, ENCLOSE's octagon and the island coast's ring, the dock pockets; `defence_walls` back to upstream's 1: on
  Small seed 5 a 9-piece copper wall went up at 75 minutes; with enclose on, 33 of 39 ring tiles of the seed-11
  island were walled in 8 minutes);
- metal.c, priority.c, fronts.c -> `metal.lua`, `priority.lua`, `fronts.lua` (the priority list is now the one
  place that says what each spender may touch: the epoch, farms, wants, landing, outpost, fortress, forest,
  upgrades, navy and special rows; metal_plan 0 and front_army off as upstream);
- the leftovers -> `packing.lua` (tight_build = 1 as upstream's ini has it), the bunkers' reserve and shelf,
  forces.c's walk-and-fight record, citizens_home part 2; spoils.c and early_militia are engine behaviour here
  (the defeat rule removes a player with nothing left; the militia reflex always runs).
`ai/ageofai/UPSTREAM` says PORTED / PARTLY (what is missing) / ENGINE / NOT STARTED per file: nothing is NOT STARTED
any more; the PARTLY lines list what the interface does not carry yet (AI_LUA_UNITS §11 "Not built yet"). The merged
whole: seeds 2 and 5 twice at 90 minutes and Small Islands seed 11 twice identical, no script errors (merged2 in
build/sim/bench). One open engine item: a Small Islands seed-11 pair diverged once in the units hash at 65 minutes
ten seconds after a direct granary (`ai.build`), and a second pair the same way at the same granary; the RNG and
AI hashes stay equal to the end and the final states are the same, so it is a hashed unit field the play never reads,
engine-side (PORT_LEARNINGS).

### 8.38 The Age of AI port's fidelity pass (2026-11-10, EE-SIM)

The owner's call after the tenth checkpoint (§8.37): make the port behave like upstream's C before any tuning. The
pass: every deviation in `settings.lua` back to upstream's default or its shipped ini, then the lost games traced
path by path against upstream's code, each difference fixed as a port bug or built as an engine gap. The bench:
Small, Hard, 2 AIs, 120 minutes, seeds 2-17, three matchups - the improved classic (no cheats, `balanced`), the
original classic with retail-Hard cheats (`classicX`: `--cheats none/none/all --personality balanced,balanced,classic
--set "gameoptions.difficulties.hard.ai_cheats=[cost, reveal, epochs, extra, popband, catchup, retire]"`), and boom.
Won / lost / close count a side with twice the other's citizens, or the other wiped out.

**Reverted to upstream (commit 4ae48a2 and after).** `min_workforce` 40 (34), the copper plan as shipped
(`copper_plan=on`: both early epochs' costs held from min_workforce, the citizens ignore the reserve below it, no
cap on them - the port had capped them at min_workforce), `food_floor` 18 (24) / `food_share` 50 (60),
`epoch_pacing` independent (rush: the first two epochs as soon as paid, from the Copper Age the 50-70 minute timer
with the game's catch-up when another player is ahead - `EEComputerPlayer::CatchUpEpoch`'s 1-12 minutes x Hard's
70%, the view's new `epoch2_cost`), no early army (the home guard of 6 + 4 x epoch from the first minute, the room
share counted in every epoch), the muster at 8 enemies / 2 x / 18 tiles (4 / 1.3 / 6), no tower cap, `ar_bleed_pct`
0 (50), `expand_islands` off, `attack_retarget` off, the vassal tribute off, `tower_bait` to the Bronze Age,
RECOVERY as econ.c (two thirds on food, the whole stock for citizens, no soldiers unless enemies are in the base),
the raid answer as armies.c (the raider nearest home seen twice running, `raid_answer` 100%; the port's cooldown
and band-5 rules gone), the attack times the game's own at Hard as upstream's `first_attack 0` takes them (6-12
minutes, then 13-15: `initial_attack_delay` 20-40 x 30%, `attack_frequency` 14-17 x 90%). Left as deviations with
their reason: the siege with the force (no waves from home), this engine's fishing-boat rule, `direct_granary` /
`replant_fields` (an engine difference).

**Port bugs and engine gaps found by tracing.**
- The engine's citizen militia reflex (`EECPRMMilitia`: citizens charge at 1.5 x odds counting the towers) ran for
  the port instead of upstream's CITIZEN MILITIA (3 x the enemies' strength within 6, 3 citizens an enemy, 6
  enemies, to the Bronze Age) - the reflex sent citizens to their deaths: seed 4 against the cheating classic lost
  15 citizens near the town in minutes 10-20 and was wiped out at 19; with `ai.reflex{militia = false}` (new) and
  the Lua militia (`tactics.lua`) it had 27 citizens at 25 minutes.
- attack.c `citizens_safety` was not ported (the engine's 30-second danger zones stood in): now `safety.lua` (the
  hurt spots for 2 minutes, 20 s with the attacker in sight; a site within 6 tiles of one - three for the minerals -
  or raided is dangerous unless the town holds it at `raid_held` 150%; its gatherers go back to the drop site and
  the claim is released; citizens near the enemy flee to the rally point while a muster is on).
- The home guard's room share (35% of the military room) applied only from the Stone Age; the port added a rule
  from the enemy army seen that upstream has not. Both fixed.
- The tower bait baited only the citizens hit (a measured deviation of the eighth checkpoint); now upstream's rule
  (a citizen with an enemy soldier within 5 tiles or attacking it, outside its own towers' reach, behind the
  nearest shooting building). The micro modules looked the town up as `b_town_center`: the start building is a
  Capitol, so the safety scanned the raiders round the map's corner; they take the view's town now.
- The safety's first version queried the engine once per citizen and blew the micro step's 300,000-instruction
  budget at 100+ citizens (1,900-8,900 aborted ticks a bench; PORT_LEARNINGS): one raider scan a second, the
  citizens in sixteenth slices a tick (each every 4 s as upstream).
- Engine gaps built: `ai.patrol`, `ai.players` `hostile` / `allies`, `ai.memory{category=}`, `ai.class` `category`
  / `family` / `cost`, `ai.reflex{militia=}`, the view's `epoch2_cost` (AI_LUA_UNITS §11).

**The benches.** c10 / c10b are the coordinator's from main aafe7d2 (the tenth checkpoint's settings).

| Port state | vs balanced W/L/C | Stone, Copper | cit 30 / 40 / end | vs classicX W/L/C | Stone | cit end | vs boom W/L/C | cit end |
|---|---|---|---|---|---|---|---|---|
| c10 (checkpoint 10) | 0 / 13 / 3 | 27.7, 10/16 at 63 | 38 / 53 / 33 | 2 / 11 / 3 | 13/16 at 38.7 | 20 | 6 / 2 / 8 | 140 vs 97 |
| f1: settings reverted | 0 / 14 / 2 | 33.5, 7/16 at 71 | 40 / 41 / 17 | 0 / 11 / 5 | 13/16 at 52 | 22 | 1 / 3 / 12 | 129 vs 108 |
| f2: + raid answer, home guard, no citizen cap, attack times | 0 / 13 / 3 | 33.9, 12/16 at 69 | 40 / 42 / 29 | 0 / 13 / 3 | 9/16 at 44 | 12 | 0 / 2 / 14 | 143 vs 111 |
| f4: + citizens' safety, the militia (f3, the same with the micro budget blown, is void) | 0 / 14 / 2 | 37.6, 7/16 at 68 | 39 / 41 / 19 | 0 / 14 / 2 | 8/16 at 46 | 10 | 1 / 2 / 13 | 135 vs 110 |
| f5: + the tower bait as upstream, the micro modules' town | 0 / 16 / 0 | 38.0, 5/16 at 78 | 40 / 41 / 19 | 0 / 12 / 4 | 5/16 at 46 | 14 | 1 / 2 / 13 | 136 vs 109 |
| f6: copper_plan next_epoch, the home guard's cap (166 / the own population) | 0 / 12 / 4 | 49.1 (8/16), 4/16 at 61 | 53 / 70 / 52 | 3 / 9 / 4 | 3/16 at 67 | 40 | 2 / 1 / 13 | 155 vs 109 |
| f7: + the forage places cap the food gatherers and the wild food, the accounts' income fractions carried (a bug: they truncated to 0 a step) | 0 / 10 / 6 | 53.0 (14/16), 11/16 at 63 | 50 / 61 / 58 | 2 / 11 / 3 | 8/16 at 57 | 26 | 1 / 2 / 13 | 141 vs 107 |
| f8: + the claims grow on the uncapped food want (up to 8 patches, a wider claim), the wild food as upstream's amounts | 0 / 11 / 5 | 48.5 (16/16), 11/16 at 64 | 50 / 60 / 57 | 2 / 9 / 5 | 9/16 at 57 | 36 | 2 / 2 / 12 | 148 vs 108 |
| f10 (tuning): epoch_share 60, early_army on (upstream's variant B; the army's food share 40 before farms) | 0 / 13 / 3 | 38.7 (15/16), 9/16 at 60 | 46 / 51 / 33 | 2 / 10 / 4 | 11/16 at 52 | 33 | 0 / 0 / 16 | 159 vs 110 |
| f11: + early_army's account pays out before farms, the excess gatherers to wood and stone | 0 / 12 / 4 | 38.3 (15/16), 6/16 at 80 | 47 / 50 / 41 | 1 / 10 / 5 | 10/16 at 50 | 26 | 1 / 5 / 10 | 129 vs 107 |

The classic reaches the Stone Age at 26, the Copper Age at 63 and 175 citizens; the cheating classic the Stone Age at
46 with 51 citizens all game and still wins the Prehistoric raids.

**What the traces say (f2, f4).** The safety and the militia change little in the counts: fewer wipe-outs against
the cheating classic (2 for 6) and a few more citizens at the end against boom, fewer against the classic. Against the improved classic the port's citizens stand at 40-42 from minute 25 to 45:
upstream's copper plan holds the Stone and Copper costs from 40 citizens on and the citizens wait on what is left
- the brake upstream's own notes call the most harmful feature and replaced with `copper_plan=next_epoch`, which is
not the shipped default. From the Copper Age (69 minutes) the timer pacing holds the Bronze reserve while the
classic's waves grow; the games are lost at 60-120 minutes to pushes that take the gatherers (seed 9: 62 near and
80 mid-range citizens lost in minutes 100-110 with 50 soldiers in the force). The raid answer at 100% with the
force out every 4 s is upstream's rule and costs: 80-135 raid answers a game against the cheating classic with 10-41
"losing" retreats (f1 with the port's 130% and cooldown: 1-39 and 0-14); seed 7 never left the Prehistoric in 120
minutes while the force grew to 44 soldiers from the military account's 40% of every income. The economy itself
gathers as boom does on the same seeds (seed 4: 45 food a minute from 9-10 gatherers at 8-12 minutes for both); the
7 settlements by minute 12 (one at every claimed site without a drop) are the port's own, upstream's `mine_fill` DROP
is off and the original game's AI built its settlements instead.

**Why the Stone Age comes 10-15 minutes later than at the tenth checkpoint (the diagnosis).** Seed 2 against the
improved classic, the 5-minute lines side by side: c10 saves from 34 citizens at minute 15 with the Stone cost alone
held (1,122 food), the workers 21 / 9 / 4 / 0 / 1 on food / wood / stone / gold / iron, the food stock 263 -> 818 in
five minutes, the Stone Age at 24; f1 saves from 40 citizens at minute 20 with both epochs' costs held (1,122 food,
then 1,584 food + 844 gold + 844 iron for the Copper Age), econ_targets' "all arrive at the same time" sends 13-21 of
41 workers to gold and iron (want 16 / 8 / 2 / 13 / 2, 23 / 3 / 0 / 7 / 7, 18 / 2 / 0 / 10 / 11), the food income
falls from 135 to 30-90 and the 1,122 food take 15 minutes (419 -> 719 -> 814 -> 999), the Stone Age at 38. That is
upstream's `copper_plan=on` to the letter (`budget_reserve`: "the full remaining cost of getting there", and the ini's
`[ai_hard]` section overrides nothing), the brake its own notes measured as the most harmful feature and replaced
with `copper_plan=next_epoch` - which it did not ship as the default. It costs more here because our Copper cost is
1,584 / 844 / 844 after the game variant's epoch-cost factor (the data's 1,200 / 640 / 640 x 1.32; upstream's notes
quote 750 / 400 / 400) and the mines lie 10-18 tiles out on Small maps: the gold and iron for an epoch two ages away
are gathered before the Stone Age's food. Second, from f2 on, the home guard's room share counts in every epoch as
upstream's does, but `unit_limit` is the lobby's 400 here (upstream's test games: a cap of 166 that follows the
human's population, 0.35 x 166 x 0.55 = 32 soldiers) - 77 soldiers wanted from the first minute with the military
account's 40% of every income: seed 2 (f4) trained 9 -> 19 soldiers between minutes 20 and 35 while the food stock
stood at 1-30 and 28-33 citizens starved; c10 had `early_army` (no account share in the first epoch) and 4-6
soldiers. Third, the attack times at Hard (6-12 minutes, then 13-15) send the first push of 8 at minute 6-12. The
rules are upstream's; the engine's lobby cap and the map's mine distances make two of them cost two to three times
what they cost upstream. The 1,900-8,900 aborted micro ticks a game (f3) were a real bug and are fixed in f4.

**f6, and the next-biggest cause.** With upstream's `copper_plan=next_epoch` (the epoch account of 40% of the
income, the citizens spend the rest) and the home guard's room from a cap of 166 (upstream's unit_limit follows the
human; the lobby's 400 here), the counts come back to the tenth checkpoint's against the improved classic (0/12/4
for 0/13/3) and past it against the cheating classic (3/9/4 for 2/11/3), with a different shape: 53 / 70 citizens
at 30 / 40 minutes (38 / 53 at c10), and the games split in two - half reach the Stone Age at 34-38 and the Copper
Age at 53-61 (faster than c10's 63) and end with 136-197 citizens, half never leave the Prehistoric in 120 minutes
and are wiped out (seed 7: 102 citizens in the first epoch with the account still short). The cause, from the f6
smoke's seed 2: the Small maps' forage runs out before the Stone Age is paid - at minute 40 the port has 68 citizens
and 12 of them on food (12 bush places left, hunting over, no fish before the Stone Age), food income 0, 26 on wood
with 875 wood idle; the next_epoch rule lets the citizens eat the 60% while the account creeps at 40% of a shrinking
income. Upstream's "the map's food runs out" clause (the sites' remaining amount within 20 tiles x 60% short of the
epoch's food + 500 -> the epoch's food first) is ported and does not bind: the patches still hold food, it is the
gatherable places that are gone. The 6 gatherers to a bush are the base game's rule, the same for upstream, whose
food_bush skips a full bush and whose game search forages farther out and hunts; the port's claims (3-5 patches)
were where its food gatherers could stand, and the gather verb queued the rest - wasted citizens, the wood piling
up while the food income stood at 0: a port bug in the worker plan's input, not a rule of this engine. The scaled-reserve alternative
(`copper_reserve_pct {76, 47}` with `copper_plan=on`, the reserve upstream's costs were) gave the Stone Age at
34-49 on four seeds: no better. The fix (f7): the food target capped at the claimed patches' places plus the game, the claims growing while the
places are short.

**f7.** The income shares of the epoch, military and navy accounts were accumulated a step at a time with an integer
division that truncates to 0 under 150 income a minute: the military account never grew past its start share in any
bench so far (`mil=240/225/240/30/30` in every trace), and next_epoch's account filled only on spikes - f6's
bimodality. With the fractions carried, the forage places capping the food gatherers (the excess to wood, stone,
gold, iron by the plan's floors; f7 also counted the wild food as places x a bush's food, reverted to upstream's
amounts afterwards since it never bound): 14 / 16 games reach the Stone
Age against the improved classic (at 53 minutes), 15 / 16 against boom, 8 / 16 against the cheating classic (3 wiped
out before 40 minutes, 5 alive in the Prehistoric at 120); W/L/C 0/10/6, 2/11/3, 1/2/13 against c10's 0/13/3, 2/11/3,
6/2/8; the end citizens against the classic [0 x7, 10, 20, 37, 104, 125, 129, 165, 167, 169]. The Stone Age at
48-57 minutes is next_epoch's own pace on these maps: 40% of a places-capped income of 60-100 a minute is 25-40 a
minute into the account from minute 18, and the military account now takes its 40% as upstream's does. The wild-food
clause does not bind on Small (the sites' amounts stay above the epoch's 1,122 less the account + 500 while the
places run out); more patches claimed while the places are short (up to 8 before farms) is the port's stand-in for
upstream's search farther out.

**f8.** The food target's cap at the places had also capped the claims' want (a circular rule): with the claims
growing on the uncapped want the patches go to 25 places by minute 30 (19 before), the account to 838 food at 45
minutes (513), and the Stone Age comes in 16 / 16 games against the improved classic (at 48.5 minutes; W/L/C
0/11/5 for c10's 0/13/3, the end citizens [0 x6, 4, 34, 47, 59, 78, 98, 128, 139, 140, 181]), 15 / 16 against boom
(2/2/12 for 6/2/8: the same losses, fewer wins, 148 citizens at the end against boom's 108), 9 / 16 against the
cheating classic (2/9/5 for 2/11/3; 5 games wiped out before the Stone Age could come, the end citizens [0 x5, 5,
14, 16, 22, 33, 37, 43, 47, 83, 132, 145]). What stands between f8 and the classic's 174 citizens at the end is the
Stone Age at 48 against 26 - next_epoch's 40% of a places-bound income - and the losses to the waves after.

**The tuning round (f9-f11), as marked deviations.** Checked first: the military account takes 40% of every
income from the first minute upstream too (hook_add_resource with `early_army` off); upstream's own notes give the
copper plan's Stone Age at 17-23 minutes on Continental (10,000-16,000 forage within 30 tiles) and call next_epoch
slower, with "epoch_share 60% and no cheat it does best" (docs/economy.md); docs/STRATEGY_RESEARCH.md §4.1-4.2 puts
the Standard Small target at 16-20 minutes from 25-30 citizens and asks for the next epoch only, no Copper metals
before the Stone Age, towers and fleeing as the Prehistoric defence. f9 (epoch_share 60, early_army with upstream's
10% food share; smoked only): the Stone Age at 41 on seeds 2 and 5, the army 0 all game. f10 (the army's food share
40 before farms, the owner's ~20 spearmen): the Stone Age at 38.7 in 15/16 games against the classic (48.5 at f8),
the Copper Age 9/16 at 60, but the Stone Age army still 0 at 30 and 40 minutes - the army's budget was the stock
above the whole Copper reserve, where upstream's variant B lets the account pay out whatever the reserve is - so
0/13/3 with 4 wiped out, and 0/0/16 against boom (159 citizens to 110, no doubling). f11: the account pays out
before farms as upstream's B does, and the food gatherers beyond the places go to wood and stone with a token to
gold and iron. f11: the Stone Age at 38 again but the Stone Age army still 0 at 30 minutes and 5 at 40 (the
account fills at 40% of a food income that the citizens' training leaves at 60-100 a minute), the Copper Age later
(6/16 at 80: the army now spends what the epoch account does not take), 0/12/4, 1/10/5 and 1/5/10 against boom -
worse than f8 in every matchup. The round's result: with next_epoch the epoch share buys the Stone Age minutes at the
army's cost on these maps, and early_army delays the Stone Age army past the waves. f8's settings stay (epoch_share
40, early_army off; the early_army code is in place and inactive). The structural gap to the classic's Stone Age at
26 is the forage: the classic gathers 65-105 food a minute from 18 gatherers at 40 minutes while the port's 12-19
places give 45-100 with 50 citizens; the model in STRATEGY_RESEARCH §4.1 reaches 16-20 minutes only from 25-30
citizens with the whole income going to the epoch - the tenth checkpoint's hold at min_workforce, which the owner
set aside for upstream's letter.

**Open.** The units-hash divergence after a direct granary on Small Islands (§8.37) was not looked into beyond the
build message's field assignment (PORT_LEARNINGS). The next round is tuning, reported as deviations from upstream
with their reason: the copper plan's brake (`next_epoch`), the raid answer's ratio and cooldown, the Bronze reserve
against the waves.

### 8.39 The classic computer player in Lua, first checkpoint (2026-11-10, EE-SIM)

The classic AI (the C++ managers of `src/Empire Earth/AI Thread/`, with this repository's changes behind the
personality switches) ported to the per-unit Lua interface as a baseline and a moddable start: `ai/classic_lua/`,
manifest id `classic_lua` ("Classic (Lua)", not random). `ai/classic_lua/ORIGIN` maps every C++ file to its module
(PORTED / PARTLY / ENGINE / NOT STARTED) with the deviations and the interface gaps. Both personalities run from the
rules data: `ai.tuning()` returns what EEComputerPlayer::Init and EECPPersonality give the C++ seat (the difficulty,
the cheats, the personality with EE_AI_PERSONALITY, every dbcpbehavior value).

**What the first checkpoint ports.** The economy, the builds and the epochs whole: the per-epoch request queues of the
unit, build and research managers, the citizens wanted, the resource centres and the needs ledger, both gatherer
splits (the original's needs split with the stock correction; `balanced`'s demand split with its claimed patches,
patch settlements and site towers), AssignGatherers' finders in order, the epoch timer with the personality's
pressure and the catchup cheat, AttemptEpochAdvance with ResearchCheapest, the building counts, the garrisons,
EEComputerPlayer::CanAfford's reserve, EnableManagers, the militia and the danger zones (militia.lua), the land
explorers. The army is a first stage (combat.lua): the attack timer and target, the group size of
ComputeGroupSize, one defend group at home, the groups on lib.groups.

**Cheats on a Lua seat.** cost, reveal, extra, popband, retire and the epochs' payment act in the engine for any
computer seat (ORIGIN lists where); catchup (the epoch timer's scale and CatchUpEpoch) and the free epoch's
affordability are decisions, ported.

**Problems met on the way** (PORT_LEARNINGS "The classic computer player in Lua"): a script's gatherers are invisible
to `ai.resource`'s `workers` (all 15 food gatherers went to one bush: `ai.gatherers()`); a class's tech is
`ai.class(t).tech`; the classic puts an epoch at the front of the town centre's queue (the port cancels and requeues
the citizens behind it); AttemptEpochAdvance runs only while a later epoch's request stands (the future research
sentinel); the engine's reflexes skip a script's units (militia.lua); a per-step cache keeps the step's charges
bounded; the fog at the start (the C++ classic's forest finder reads the whole map: the port's idle citizens explore
until something is known); a centre of the town centre alone (Regroup's), without which no tower or temple had a site.

**The bench** (EE-SIM, Small, Hard, 2 AIs, seeds 2-17, 90 minutes, `--data open`, the scripts from a copy of `ai/`;
`balanced` with no cheats; `classic` with the retail Hard cheats on both seats: `--set
"gameoptions.difficulties.hard.ai_cheats=[cost, reveal, epochs, extra, popband, catchup, retire]" --cheats
none/all/all`). Epochs: reached / median minute; citizens/army at 10/20/30/40 minutes; citizens lost by 20 minutes
and in all; the citizens at 90 minutes and the seats wiped out. "C++ vs C++" is the reference (both seats the C++
classic, 32 players).

| Matchup | AI | Stone | Copper | Citizens/army 10/20/30/40 min | Lost by 20 / all | Citizens at 90 min (wiped) | Won-lost |
|---|---|---|---|---|---|---|---|
| balanced: C++ vs C++ | classic | 32/32, 25.3 | 21/32, 66.3 | 24.9/2.2 / 25.4/2.0 / 29.1/4.3 / 42.8/3.9 | 1.7 / 46.4 | 103 (3/32) | - |
| balanced: Lua vs C++ | classic | 16/16, 25.2 | 15/16, 63.8 | 22.9/2.2 / 25.0/2.2 / 30.4/5.1 / 45.9/3.2 | 2.3 / 24.6 | 151 (0/16) | 12-1, 3 close |
| balanced: Lua vs C++ | classic_lua | 16/16, 24.9 | 9/16, 64.6 | 23.4/2.3 / 27.2/3.2 / 38.0/6.3 / 48.2/7.1 | 2.1 / 95.4 | 45 (2/16) | 1-12, 3 close |
| classic + cheats: C++ vs C++ | classic | 32/32, 46.5 | - | 24.3/6.7 / 47.3/5.6 / 49.8/6.5 / 49.4/5.9 | 15.0 / 91.4 | 50 (0/32) | - |
| classic + cheats: Lua vs C++ | classic | 16/16, 45.6 | - | 27.9/6.4 / 51.1/9.2 / 50.9/13.7 / 50.8/8.6 | 2.6 / 26.1 | 51 (0/16) | 12-1, 3 close |
| classic + cheats: Lua vs C++ | classic_lua | 16/16, 46.5 | - | 22.4/7.1 / 33.6/10.5 / 34.6/13.2 / 31.0/16.8 | 13.7 / 155.1 | 7 (4/16) | 1-12, 3 close |
| balanced: C++ vs boom | boom | 16/16, 28.0 | 9/16, 65.5 | 23.6/0.0 / 32.4/0.0 / 31.3/3.0 / 41.4/14.2 | 3.7 / 59.2 | 64 (0/16) | 1-13, 2 close |
| balanced: C++ vs boom | classic | 16/16, 25.0 | 16/16, 65.0 | 26.4/2.2 / 30.8/2.5 / 40.1/7.8 / 62.9/5.8 | 0.0 / 26.4 | 167 (0/16) | 13-1, 2 close |
| balanced: Lua vs boom | boom | 15/16, 26.9 | 13/16, 65.4 | 20.7/0.0 / 28.0/0.0 / 29.6/2.8 / 44.9/10.6 | 7.2 / 14.9 | 100 (1/16) | 9-1, 6 close |
| balanced: Lua vs boom | classic_lua | 15/16, 24.8 | 10/16, 61.3 | 24.2/2.4 / 30.0/2.9 / 38.8/6.8 / 51.6/8.4 | 0.1 / 63.9 | 62 (0/16) | 1-9, 6 close |
| balanced: C++ vs Age of AI | ageofai | 13/15, 52.6 | 5/15, 58.1 | 21.5/5.4 / 34.0/7.3 / 45.1/9.3 / 54.7/10.5 | 1.2 / 67.7 | 43 (1/15) | 0-14, 1 close |
| balanced: C++ vs Age of AI | classic | 15/15, 25.1 | 15/15, 61.8 | 26.0/2.3 / 29.1/1.5 / 35.5/4.1 / 56.0/2.8 | 1.1 / 22.5 | 172 (0/15) | 14-0, 1 close |
| balanced: Lua vs Age of AI | ageofai | 16/16, 52.3 | 13/16, 67.1 | 20.9/5.3 / 33.2/7.6 / 46.7/10.7 / 55.4/8.4 | 1.7 / 30.0 | 102 (0/16) | 8-3, 5 close |
| balanced: Lua vs Age of AI | classic_lua | 16/16, 25.0 | 9/16, 67.7 | 22.6/2.5 / 27.9/2.4 / 36.8/6.4 / 50.6/6.6 | 1.9 / 70.3 | 63 (0/16) | 3-8, 5 close |

**Readings.**
- **The economy, the builds and the first epoch match.** `balanced`: the Stone Age at 24.9 minutes (16/16) against the
  C++ classic's 25.2 in the same games and 25.3 in its own; the citizens at 10-40 minutes within a few of the C++
  (23.4/27.2/38.0/48.2 against 22.9/25.0/30.4/45.9). `classic` with the retail cheats: the Stone Age on the epoch timer
  at 46.5 minutes (the C++ 45.6 in the same games, 46.5 in its own). Against boom the port reaches the Stone Age
  earlier than boom (24.8 against 26.9) with more citizens to 40 minutes.
- **The gap is the army.** After 40 minutes the port loses three to six times the citizens of the C++, reaches the
  Copper Age in 9 of 16 games against 15 and loses most games; in the games it is not overrun it booms like the C++
  (seed 3 of the first round: 111 citizens and the Bronze Age at 88 minutes). The attack groups' tasks, the
  sub-groups, the aid packages' deployment, the counter units and the walls are the next stage (ORIGIN: PARTLY /
  NOT STARTED); the first army stage defends the town as one group (34 tiles: MilitaryAidPackage::Deploy's reach)
  and sends the attack groups on the C++ timer.
- **The same opponents** say the same: the C++ classic beats boom 13-1 and Age of AI 14-0 (one Age of AI run failed
  in the lobby); the port loses to boom 1-9 (6 close) and to Age of AI 3-8 (5 close), with the economy level with
  theirs to 40 minutes and the Stone Age first in both.
- **Determinism:** seeds 2 and 5, 60 minutes, `--repeat 2`: identical traces (5,982 and 4,855 lines); 0 script errors
  in every bench. **The other engines are unchanged** by the interface additions: boom and Age of AI against the
  classic, seed 2, 25 minutes, give the same traces on this build as on main's (1,887 and 2,541 lines).

### 8.40 The classic computer player in Lua, the army (2026-11-10, EE-SIM)

The second stage of §8.39: `ai/classic_lua/army.lua` replaces the first army stage with the classic's army.
- The attack (EECPCombatManager::Update on the attack timer: the target player, CreateGroup).
- The attack groups: the unit selector's group size, held, counter and favourite units; Personnel's per-class
  request slots with their 600 s deadline; the sub-groups' wait for units, gathering at the rally point, and the
  task (AssignLandTask's territory targets, else the defence ladder).
- The defence commander: an aid package per town (EEPlayer::AddUnitToTown's clusters), SendAid / SendBestSubGroup /
  CounterAttack, Gather, Deploy, Recall, PlanAid, CreateDefendGroup, GetUnitAllowance, ComputeTownThreat over the
  seen armies; NoteThreat on citizen deaths.
- lib.groups moves and fights the groups. ORIGIN has the deviations (fog: the target files are what ai.memory holds)
  and what is not started (special units, ChooseUnitsToKill, hills and choke points, naval, air, walls).

Three economy differences under the classic personality, found while chasing the army's numbers, mattered more than
the army itself:
- **The needs ledger counted the in-game cost, not the listed one.** EECPResourceManager::AddCosts counts a tech's
  listed cost (ScaleCost'ed); `ai.class`'s `cost` is GetCost's, whose epoch factor zeroes the future research
  sentinel (2000 wood, 1000 stone, 2000 iron) in the Prehistoric. The classic personality splits gatherers by the
  needs, so the C++ sent its first gatherers to stone and iron while the port's went to wood. `data_cost` (AI_LUA_UNITS
  §11) is the ledger's cost now.
- **BuildSettlementFor never ran.** The gather goal's storage search asks the C++ AI for a settlement by a far
  resource; the engine hook skips a script's seat. Without it the classic personality's mines had no drop site, so
  no tower site either (FindTowerSite works the centres with a drop site): 0-3 towers by 12 minutes against the
  C++'s 11-23, and the raids walked in. econ.lua ports it (every 4 s over the gatherers at work).
- **HasEnemyNear saw too little.** It took the enemies seen within 9 tiles; the C++ sums every enemy influence in
  the regions round a site (up to 27 tiles across). Lua citizens died far from home (386 of 917 deaths beyond 30
  tiles, 188 to towers). It now takes the armed enemies seen or remembered in the same box.

Bench: Small, Hard, 2 AIs (Lua against the C++ classic in the same game), seeds 2-17, 90 minutes, open data;
stage 1 (lua_bal4, lua_cls4) against this stage (a8_bal, a8_cls); classic with the retail Hard cheats on both AIs.

| Matchup | AI | Stone | Copper | Citizens/army 10/20/30/40 min | Lost by 20 / all | Citizens at 90 min (wiped) | Won-lost |
|---|---|---|---|---|---|---|---|
| balanced, stage-1 | classic | 16/16, 25.2 | 15/16, 63.8 | 22.9/2.2 / 25.0/2.2 / 30.4/5.1 / 45.9/3.2 | 2.3 / 24.6 | 151 (0/16) | 12-1, 3 close |
| balanced, stage-1 | classic_lua | 16/16, 24.9 | 9/16, 64.6 | 23.4/2.3 / 27.2/3.2 / 38.0/6.3 / 48.2/7.1 | 2.1 / 95.4 | 45 (2/16) | 1-12, 3 close |
| balanced, stage-2 | classic | 15/16, 25.1 | 13/16, 61.2 | 22.7/2.5 / 25.2/1.9 / 30.0/6.2 / 46.8/4.4 | 2.6 / 28.0 | 136 (1/16) | 11-3, 2 close |
| balanced, stage-2 | classic_lua | 16/16, 25.0 | 9/16, 65.9 | 23.6/2.8 / 26.7/3.3 / 33.9/7.2 / 43.4/9.3 | 2.0 / 66.4 | 50 (3/16) | 3-11, 2 close |
| classic+cheats, stage-1 | classic | 16/16, 45.6 | - | 27.9/6.4 / 51.1/9.2 / 50.9/13.7 / 50.8/8.6 | 2.6 / 26.1 | 51 (0/16) | 12-1, 3 close |
| classic+cheats, stage-1 | classic_lua | 16/16, 46.5 | - | 22.4/7.1 / 33.6/10.5 / 34.6/13.2 / 31.0/16.8 | 13.7 / 155.1 | 7 (4/16) | 1-12, 3 close |
| classic+cheats, stage-2 | classic | 16/16, 45.6 | - | 25.9/5.9 / 50.3/4.1 / 49.9/4.6 / 49.4/2.4 | 7.6 / 76.5 | 50 (0/16) | 0-6, 10 close |
| classic+cheats, stage-2 | classic_lua | 16/16, 46.4 | - | 22.2/7.9 / 37.2/14.3 / 44.0/39.6 / 45.9/42.3 | 9.9 / 68.2 | 56 (0/16) | 6-0, 10 close |

Readings:
- **Balanced:** the economy is level to 30 minutes and close at 40 (43.4 against 46.8 citizens); Lua keeps a larger
  army (7-9 against 4-6) and loses more citizens (66 against 28 in all), and loses 3-11 (2 close). The losses come
  later in the game, past the first epochs.
- **Classic with the cheats:** Lua's economy trails at 20 minutes (37 against 50 citizens) and is level by 40; its
  army grows to 40 while the C++'s falls to 2-5 (its attacks die at Lua's towers), and Lua wins 6-0 (10 close):
  from level-ish to ahead, so the remaining gap is in the other direction.
- An experiment that dropped the target file's floor at the own military (the suspected feedback that grows the
  army) did not lower the army and was not kept.
- Determinism: seeds 2 and 5, 60 minutes, `--repeat 2`: traces identical. 0 script errors.

Next: the balanced late-game losses and army size (the attack groups' slots against the room), the classic
personality's early citizens, then the special units, ChooseUnitsToKill and the walls.

### 8.41 Boom's defence and army on the per-unit interface (2026-11-10, EE-SIM)

The owner's brief: boom's raid answer, militia and Stone Age guard in Lua on the per-unit interface (AI_LUA_UNITS
§3, §11), with the improved classic's rules (`balanced`) and the research's (STRATEGY_RESEARCH §4), as library
modules other AIs can use. Built:
- `ai/lib/defence.lua`: the raid answer and the militia in the micro step, with the engine's militia and danger-zone
  reflexes off (`ai.reflex{militia = false, danger = false}`).
  - Raiders are clustered once a game second (one `ai.seen`). A cluster is a raid when it is stronger than boom's
    THREAT_MIN, or when one of its units has hit an own unit in the last 15 s.
  - Only the citizens with a raider within 6 tiles of them or of their work spot run. They go to the nearest tower or
    town centre that is not further into the raid.
  - They go back to their exact spot when it is clear. The return goes through a documented hook (`D.on_return`, for
    the economy's placement module) and `ai.release{gather = true}` (new, AI_LUA_UNITS §11).
  - The militia fights only melee raids of at most 6 raiders, at 3 x by the square law. The soldiers and the towers
    in reach count; at most 4 citizens join a raider. It breaks off below half those odds.
  - Warships are raids too: the citizens run from them, and none is sent back to a shore under fire.
  - Builders repair the towers hit (2 a tower, 3 towers at once).
- `ai/lib/guard.lua`: the home guard, as a manual lib.groups group.
  - Its post is under the own tower nearest the citizens' centre.
  - It fights the raids in the claim range at 0.8 x with the towers in reach, and always under a tower. It does not
    chase past that zone.
  - Focus fire: the raider nearest the citizens first. Units go on the next target in turn once enough fire is on
    one. The hurt (under 35%) are pulled back.
  - Under a warship's fire the ranged units shoot it and the rest leave its reach.
  - With no raid on, at most half of it hunts the enemy citizens in its zone that are out of their towers' reach.
  - The training follows a mix by the raiders seen (ai.memory). With nothing seen: 2/3 spearmen, 1/3 slingers.
    Spearmen go against shock infantry and cavalry, slingers against spearmen, clubmen against ranged units.
- `ai/boom.lua`: the plan's `army_per` / `army_floor` (balanced's standing army: a quarter of the citizens, at least
  10) in the Stone and Copper plans; the micro step; the calls.

**The bench.**
- Small, Hard, 2 AIs, 120 minutes, seeds 2-17, `--data open`. Boom plays from seat p2.
- Opponents: Age of AI; boom itself; the improved classic (`balanced`, no cheats); the original classic with
  retail-Hard cheats (classicX); the rush script (now in ais.json).
- Each row is 16 games. A few lobby start-up failures under the machine's load dropped one game from some rows.
- W/L/C: the scorecard's rule (1.5 x citizens and army at the end, or the other side wiped out).
- Lost: citizens and boats, by 20 minutes / in all. Per raid: the trace's next.raid incidents with a hit.

| Matchup | Variant | W/L/C | Defeats | Stone / Copper (median min) | Lost 20 min / all | Lost a raid | Citizens / army 40 min | End citizens / army |
|---|---|---|---|---|---|---|---|---|
| Age of AI | baseline (main) | 4/5/7 | 0 | 22.3 / 49.5 | 0.5 / 14.5 | 2.06 | 60 / 16 | 108 / 17 |
| | defence only | 4/5/7 | 0 | 22.3 / 51.7 | 0.6 / 10.5 | 1.18 | 60 / 14 | 111 / 19 |
| | + guard, fixed 20 | 0/4/12 | 0 | 22.3 / 52.1 | 0.6 / 13.7 | 1.39 | 58 / 17 | 111 / 31 |
| | + guard, scaled (final) | 4/4/8 | 0 | 22.3 / 51.6 | 0.6 / 10.4 | 1.04 | 62 / 12 | 112 / 30 |
| Mirror | baseline | 1/2/13 | 0 | 21.6 / 47.3 | 0.1 / 39.2 | 4.40 | 60 / 15 | 98 / 23 |
| | defence only | 0/0/16 | 0 | 21.6 / 46.8 | 0.1 / 12.6 | 1.71 | 62 / 18 | 111 / 20 |
| | fixed 20 | 1/1/14 | 0 | 21.6 / 51.2 | 0.1 / 18.8 | 2.13 | 67 / 16 | 107 / 23 |
| | scaled (final) | 0/0/16 | 0 | 21.6 / 51.7 | 0.1 / 4.7 | 0.70 | 68 / 11 | 111 / 28 |
| Improved classic | baseline | 1/11/4 | 6 | 24.7 / 56.2 (12) | 2.6 / 110.9 | 2.02 | 46 / 13 | 49 / 8 |
| | defence only | 0/13/3 | 5 | 22.9 / 61.6 (11) | 2.2 / 132.9 | 1.62 | 61 / 13 | 54 / 9 |
| | fixed 20 | 1/12/3 | 3 | 23.8 / 62.6 (12) | 2.0 / 111.1 | 1.13 | 60 / 15 | 65 / 11 |
| | scaled (final) | 0/12/4 | 3 | 23.8 / 60.8 (10) | 2.0 / 125.1 | 1.53 | 60 / 10 | 60 / 15 |
| Cheating classic | baseline | 5/9/2 | 6 | 27.7 (11) / 75.8 (6) | 9.4 / 84.0 | 2.29 | 20 / 4 | 41 / 9 |
| | defence only | 2/12/2 | 9 | 27.8 (10) / 77.3 (3) | 10.6 / 83.0 | 1.76 | 28 / 2 | 23 / 3 |
| | fixed 20 | 4/12/0 | 11 | 27.4 (11) / 77.1 (3) | 8.9 / 87.6 | 1.89 | 30 / 5 | 26 / 4 |
| | scaled (final) | 4/9/3 | 8 | 27.4 (11) / 66.3 (5) | 8.9 / 71.0 | 1.78 | 31 / 4 | 36 / 6 |
| Rush | baseline | 12/0/4 | 0 | 21.5 / 47.8 | 0.3 / 7.4 | 1.35 | 56 / 19 | 111 / 22 |
| | defence only | 10/0/6 | 0 | 21.5 / 51.8 | 0.4 / 9.2 | 1.69 | 62 / 17 | 110 / 22 |
| | fixed 20 | 9/0/6 | 0 | 21.5 / 51.8 | 0.5 / 15.0 | 2.37 | 61 / 16 | 107 / 22 |
| | scaled (final) | 11/0/4 | 0 | 21.5 / 51.1 | 0.5 / 4.6 | 0.72 | 68 / 12 | 110 / 26 |

(A count in brackets: the games that reached that epoch, where fewer than all 16 did.)

What it shows:
- **The mirror and the rush.**
  - Citizens lost fall from 39 to 5 a game in the mirror and from 7 to 5 against rush.
  - A raid costs 0.7 citizens (4.4 and 1.35 before).
  - The town has 68 citizens at 40 minutes (56-60 before). Rush is still beaten in 11 of 16.
- **The epochs.** The Stone Age comes at the same minute. The Copper Age comes 2-5 minutes later in every
  matchup, with more citizens and a smaller light army at 40 minutes (10-12 soldiers against 13-19), and a larger
  army at the end (26-30 against 17-23).
- **The improved classic still wins the late game.**
  - Boom is ahead at 40 minutes (60 citizens to 46) and has half the defeats (3 against 6).
  - It still loses 111-133 citizens a game, to the Copper Age waves and to warships. Ships killed 585-662 citizens
    and boats in 16 games, against 375 before: there are more boats and shore gatherers to sink.
  - The defence cannot answer an army, and boom has no army of its own beyond the light one.
- **The cheating classic.**
  - Its Prehistoric raids of 10-15 rock throwers and clubmen kill about 9 citizens by minute 20, whatever the
    answer. The citizens that run die running: 24 of 33 losses in seed 2 were moving. Before, the militia died
    fighting.
  - 8 defeats against 6 is within this matchup's noise: its W/L/C went 5/9/2, 2/12/2, 4/12/0 and 4/9/3 across
    variants of the same defence.
- **A fixed 20 against a quarter of the citizens.** The scaled army loses fewer citizens in all five matchups. The
  fixed 20 holds the citizens back in the Stone Age (`army_first`), and its guard sends more soldiers on the hunt.
- **The old home group raided.**
  - Boom's lib.groups defend group (radius: the claim range) attacked every enemy citizen in its claim. On Small maps
    the claims overlap, which is why the mirror lost 39 citizens a game (549 of them killed by soldiers).
  - Without it, Age of AI won 11 of 16 against boom (its economy grew to 171 citizens).
  - The guard's hunt of the citizens in its own zone restores that effect without the chase.
- **Soldiers lost to towers:** 1-3 a game against every opponent but Age of AI, against which it is 8-33 (the guard's
  hunt and the scouts near its fronts' towers).

**Determinism and errors.**
- Two runs of seed 2 against the improved classic (90 minutes) and two of seed 3 against rush (45 minutes) gave
  identical traces. These runs were with the scripts before the hunt and the scaled army: the gates' run should
  repeat the check.
- The defence's micro step overran its 300k budget twice in 64 games, each time sending about 60 citizens back at
  once. It now sends 12 a second, and there have been no script errors since.

**Engine findings.**
- A unit killed arrives as a `converted` event, never as `died` (PORT_LEARNINGS). Age of AI's `diplo.lua` counts its
  losses from `died`, so it has never seen one.
- `EECPTraceUnitLost`'s `if` without braces counts every lost unit in the citizens' by-distance and by-killer fields.
  This only affects the trace.

**32 seeds (2-33) against both classics, the final scripts with the died fix** (a few start-up failures under load
dropped 1-2 games a row):

| Matchup | Variant | Games | W/L/C | Defeats | Stone (n, median) | Copper (n, median) | Lost 20 min / all | Citizens 30 / 40 min | End citizens |
|---|---|---|---|---|---|---|---|---|---|
| Improved classic | baseline | 32 | 2/25/5 | 16 | 32, 26.6 | 19, 54.7 | 3.6 / 121.5 | 33 / 43 | 37 |
| | final | 30 | 0/22/8 | 11 | 28, 25.8 | 15, 59.9 | 4.0 / 114.4 | 36 / 52 | 46 |
| Cheating classic | baseline | 31 | 5/21/5 | 14 | 21, 30.6 | 6, 75.8 | 13.0 / 81.5 | 15 / 17 | 30 |
| | final | 31 | 4/23/4 | 20 | 21, 29.3 | 6, 62.1 | 10.0 / 71.5 | 20 / 27 | 23 |

- Against the improved classic the final scripts have fewer defeats (11 of 30 against 16 of 32), fewer citizens lost
  and more citizens at 40 minutes. Fewer games reach the Copper Age (15 against 19), and those that do get there 5
  minutes later.
- Against the cheating classic they have more defeats: 20 of 31 against 14 of 31. That is a real regression, not
  noise. Fewer citizens are lost and the town is larger at 30-40 minutes, but more towns are wiped out later. The
  flight keeps the citizens alive through the first raids; it does not stop the waves that follow.
- `--repeat 2` on the final scripts (seeds 4 and 9 against the classic and rush, 90 minutes): all four pairs
  identical.
- **Age of AI with the died fix** (against the improved classic, seeds 2-17): W/L/C 1/12/3 -> 0/11/4, defeats 6 -> 3,
  citizens lost 127 -> 133, end citizens 57 -> 61. Its play changes (diplo lines 396 -> 362 in 16 games). The traces
  first differ at the losses trace line, which the braces fix corrected.

**The waves (round three and four, 32 seeds each).**
- **What killed the towns.** The defeats where a5e2bea died and the baseline lived were mostly Prehistoric
  harassment. 1-3 rock throwers camped by a food patch for 10-20 minutes. The no-militia-against-ranged rule left them
  there, the citizens kept fleeing, and the food income fell to 0. The same pattern explains seeds 18 and 25, which
  never reached the Stone Age against the improved classic. The baseline's militia had cleared such camps, at the cost
  of a few citizens. One defeat (seed 30) was a 23-unit Prehistoric wave, which no Prehistoric defence survives.
- **The fix:** the militia also fights a ranged raid that has camped 40 s within 10 tiles of one spot.
- **The guard's size:** a quarter of the citizens with the owner's 20 as the floor (v2) against a floor of 10 (v1).
- **No garrison:** a town centre's or settlement's garrison is a counter (`EEComplexUnit::AddGarrison`). Its citizens
  are absorbed and never come out, so "the citizens garrison" is not available before the fortress.

| Variant | Improved classic: defeats, Stone reached | Cheating classic: defeats, Stone reached | Mirror: defeats, lost | Rush: W/L/C, defeats, lost | Age of AI: W/L/C, defeats, lost |
|---|---|---|---|---|---|
| baseline | 16/32, 32/32 | 14/31, 21/31 | 0/32, 39.4 | 20/2/8, 1/30, 7.4 | 7/11/14, 0/32, 18.3 |
| a5e2bea | 11/30, 28/30 | 20/31, 21/31 | | | |
| v1 (camp militia, floor 10) | 13/31, 30/31 | 14/32, 22/32 | | | |
| v2 (camp militia, floor 20) = b1b1312 | 6/32, 31/32 | 13/32, 22/32 | 0/32, 15.9 | 23/1/8, 0/32, 4.4 | 0/11/20, 0/31, 17.3 |
| v2 + the guard's fallback to the town centre | | 16/32, 22/32 | | | |
| v2 + the counter-attack after a failed wave (from the Stone Age) | | 15/31, 22/31 | | | |

- The one game of b1b1312 without the Stone Age (seed 7, against the improved classic) is a forage stall: 24-29
  citizens for 25 minutes on 5-55 food a minute, the reserve stuck at 1,020 of 1,122. It is boom-econ's.
- Every game reaches the Stone Age in the mirror, against rush and against Age of AI.
- The fallback and the counter-attack lose more games against the cheating classic. Both stay off (plan.attack,
  guard.opts.fallback). The counter-attack also wavers: 17 attacks, 150 retreat lines, 108 windows in 31 games.
- `--repeat 2` on b1b1312 (seeds 4 and 9, against the improved classic and rush, 90 minutes): all four pairs
  identical.

**The overnight experiments D1-D5 (2026-11-11).**
- The protocol:
  - one switch over main 1e422bd's boom, against main 1e422bd's scripts as the baseline;
  - 32 seeds (2-33), Small, Hard, 120 minutes, the same exe;
  - kept only if the defeats do not rise in any matchup and something gains;
  - the kept one is also checked against the mirror and Age of AI.
- Stone / Copper: the games that reached the epoch and its median minute. Lost: citizens and boats in all.

| Experiment | Matchup | Defeats | W/L/C | Stone | Copper | Lost |
|---|---|---|---|---|---|---|
| baseline (1e422bd) | improved classic | 6/32 | 0/27/5 | 31, 24.9 | 20, 61.6 | 111.8 |
| | cheating classic | 13/32 | 8/18/6 | 22, 28.0 | 8, 74.7 | 85.0 |
| | mirror | 0/32 | 1/1/30 | 32, 21.5 | 32, 51.9 | 15.9 |
| | Age of AI | 0/32 | 0/11/21 | 32, 22.4 | 32, 50.9 | 17.1 |
| D1 watcher + attack from Copper (`watch`, `attack`) | improved classic | 8/32 | 1/24/7 | 31, 24.9 | 22, 65.9 | 114.6 |
| | cheating classic | 16/32 | 8/19/5 | 22, 28.0 | 6, 90.5 | 86.6 |
| D2 wave flight (`wave_n = 8`): **kept** | improved classic | 4/32 | 0/30/2 | 31, 24.9 | 19, 62.8 | 119.7 |
| | cheating classic | 12/32 | 7/18/7 | 23, 29.4 | 7, 75.8 | 87.1 |
| | mirror | 0/32 | 1/0/31 | 31, 21.5 | 31, 51.6 | 15.4 |
| | Age of AI | 0/31 | 0/14/17 | 31, 22.3 | 31, 51.2 | 17.8 |
| D3 four Prehistoric towers from 6 citizens (`towers = 4`, `tower_at = 6`) | improved classic | 10/32 | 0/29/3 | 32, 24.6 | 21, 64.8 | 120.5 |
| | cheating classic | 17/32 | 12/18/2 | 26, 29.1 | 9, 81.7 | 74.4 |
| D4 tower bait (`bait`, Age of AI's early.c) | improved classic | 8/32 | 0/27/5 | 32, 24.3 | 15, 62.4 | 123.0 |
| | cheating classic | 15/30 | 8/17/5 | 21, 25.9 | 10, 78.2 | 87.4 |
| D5 the hurt spots (`hurt_safety`, Age of AI's attack.c citizens_safety) | improved classic | 9/32 | 0/27/5 | 32, 25.2 | 17, 57.3 | 109.2 |
| | cheating classic | 20/32 | 4/24/4 | 21, 29.8 | 3, 54.3 | 66.4 |

- **Kept:** D2 alone. It has the fewest defeats against both classics and no defeats against the mirror or Age of
  AI. The gain is small, though: 2 and 1 games, within a 32-seed bench's noise. Against Age of AI and the improved
  classic it has fewer close games (17 against 21, 2 against 5), and it loses a few more citizens against the
  classics. It is kept by the protocol's rule; a 64-seed check would settle it.
- **Off:**
  - D1 adds defeats against both classics and comes to the Copper Age later. The attacks pull soldiers out of a town
    the classics are about to hit.
  - D3 reaches the Stone Age in every game against the improved classic and in 26 of 32 against the cheating one
    (22 for the baseline). It wins 12 against 8 there and loses the fewest citizens. But it has 4 more defeats against
    each classic: the fourth tower's stone and the earlier towers come out of the Stone Age reserve.
  - D4 and D5 both move citizens more often and add defeats. D5 halves the Copper Age games against the cheating
    classic (3 against 8).

**D2 on 64 seeds; D3b and D3c (2026-11-11).** The same protocol. Seeds 34-65 were added to the baseline and to D2.
D3b and D3c are each one switch over D2, against D2's 32-seed rows above.

| Experiment | Matchup | Seeds | Defeats | W/L/C | Stone | Copper | Lost |
|---|---|---|---|---|---|---|---|
| baseline (1e422bd) | improved classic | 2-65 | 13/64 | 1/52/11 | 63, 23.8 | 40, 61.5 | 108.1 |
| | cheating classic | 2-65 | 28/64 | 17/38/9 | 46, 29.4 | 19, 71.8 | 76.1 |
| D2 wave flight | improved classic | 2-65 | 10/64 | 1/55/8 | 63, 23.8 | 40, 62.4 | 111.5 |
| | cheating classic | 2-65 | 28/64 | 18/36/10 | 46, 29.6 | 18, 72.3 | 77.1 |
| D3b D2 + the towers from 6 citizens (`tower_at = 6`) | improved classic | 2-33 | 7/32 | 0/26/6 | 32, 24.0 | 22, 62.3 | 113.0 |
| | cheating classic | 2-33 | 17/31 | 4/18/9 | 23, 29.1 | 7, 81.8 | 75.3 |
| D3c D2 + a fourth tower once the next epoch is reserved (`towers_saving = 4`) | improved classic | 2-33 | 9/32 | 0/27/5 | 32, 24.9 | 18, 62.4 | 115.2 |
| | cheating classic | 2-33 | 17/32 | 8/18/6 | 23, 29.3 | 11, 77.9 | 73.9 |

- **D2 holds on 64 seeds:** 10 defeats against 13 by the improved classic, 28 against 28 by the cheating one. Kept.
- **D3b and D3c add defeats against both classics** (7 and 9 against 4; 17 and 17 against 12). Both are off; no
  64-seed run.
  - Earlier towers and a fourth tower reach the Stone Age in every game against the improved classic. They also lose
    fewer citizens against the cheating classic.
  - But more towns are wiped out later. The tower spending, or the towers at the town that the plan builds first,
    does not stop the waves that kill the town.

**D6 and D7, the Prehistoric rule measured for the owner (2026-11-11).** Both are off by default; each is one switch
over D2. 32 seeds (2-33), against D2's rows.
- D6, `plan.emergency = N`: up to N soldiers, and a barracks, while a wave of 8 or more is seen in the claim range.
  Training stops 60 s after the wave is gone.
- D7, `tower_militia`: in the Prehistoric, a raid in a tower's reach is fought by the militia whatever its size or
  kind, when the citizens and the towers win by the square law at 3 x.

| Experiment | Matchup | Defeats | W/L/C | Stone | Copper | Citizens at 20 min | Lost |
|---|---|---|---|---|---|---|---|
| D2 | improved classic | 4/32 | 0/30/2 | 31, 24.9 | 19, 62.8 | 32.9 | 119.7 |
| | cheating classic | 12/32 | 7/18/7 | 23, 29.4 | 7, 75.8 | 28.6 | 87.1 |
| D6, N = 4 | improved classic | 5/32 | 0/30/2 | 31, 24.9 | 19, 62.8 | 32.9 | 115.5 |
| | cheating classic | 13/32 | 6/19/7 | 23, 29.3 | 6, 75.5 | 28.7 | 86.3 |
| D6, N = 8 | improved classic | 5/32 | 0/30/2 | 31, 24.9 | 19, 62.8 | 32.9 | 122.4 |
| | cheating classic | 12/32 | 6/18/8 | 23, 29.4 | 7, 75.8 | 28.7 | 88.4 |
| D7 | improved classic | 6/32 | 0/28/4 | 32, 24.3 | 21, 63.2 | 32.9 | 119.5 |
| | cheating classic | 16/31 | 8/20/3 | 21, 27.4 | 6, 73.9 | 28.4 | 76.1 |

- **Neither helps, and neither costs the economy anything measurable:** the Stone Age minute and the citizens at 20
  minutes are within 0.2 of D2's.
- **D6** changes almost nothing. The Prehistoric town has no barracks when a wave arrives (it stands from 28
  citizens), so the soldiers ordered come after the wave: 23 plan lines with soldiers in the 32 games against the
  cheating classic. An emergency army would have to be built before the wave is seen, which is the rule the owner
  set.
- **D7** adds defeats (6 and 16). Its militia rarely triggers: a Prehistoric tower reaches 4 tiles, and the waves
  stand outside that (938 militia lines against D2's 917).
- No 64-seed run: none is better than D2.

**Open.**
- The attack from the Copper Age and the permanent scout: `lib/attack.lua` and `lib/scout.lua` are written but not
  wired or benched.
- Raids routed around towers.
- The late-game answer to the classic's waves and warships.

### 8.42 The classic computer player in Lua, parity of the armies (2026-11-10, EE-SIM)

After §8.40 Lua beat the C++ classic under the classic personality with the cheats (a 40-strong army against 2-5)
and lost under balanced. The causes, each a port bug against the C++:
- **The attack picked a computer over the humans 100% of the time, not 10%.** mComputerAggression (10) is read as
  thousandths; the port compared a thousandth roll with it times 10. The C++ attacks the bench's human seat, which
  has no units, nine times in ten: one or two units, which then defend. The port's attacks went at the C++ every
  time, sized from a 30-40 strength, and waited ten minutes at home for their last slot.
- **The enemy list skipped players with no units** (the classic skips only observers), and the target file's mirror
  floor (§8.40: at least the own counts) inflated the strength of a seat never seen with anything; it now applies
  only to a player known to have a town or buildings.
- **Personnel's Cancel:** a request past its deadline now drops one production order per open slot.
- **QueueMoraleBuilding:** only the town centre (2 houses in its 10-tile square) and the capitol (4) carry the morale
  effect; the port built 4 houses at every drop site, settlements too: 514 houses in 32 games against 98.
- **The militia's soldiers** stay in the fight for its 20 s; the port's groups re-ordered them at once and the citizens
  fought alone (340 of the late Lua citizen deaths were "fight").

Bench: Small, Hard, 2 AIs (Lua against the C++ classic in the same game), seeds 2-33, 90 minutes, open data; before
(b1: §8.40 plus a deadline re-order, not kept) and after (b5).

| Matchup | AI | Stone | Copper | Citizens/army 10/20/30/40 min | Lost by 20 / all | Citizens at 90 min (wiped) | Won-lost |
|---|---|---|---|---|---|---|---|
| balanced, before | classic | 31/32, 25.0 | 25/32, 60.8 | 23.7/2.3 / 25.8/2.3 / 29.9/5.3 / 45.3/4.4 | 1.8 / 21.1 | 135 (1/32) | 21-6, 5 close |
| balanced, before | classic_lua | 31/32, 25.0 | 19/32, 61.6 | 23.3/2.8 / 26.0/3.1 / 32.9/7.4 / 42.4/8.6 | 2.3 / 61.1 | 57 (4/32) | 6-21, 5 close |
| balanced, after | classic | 29/31, 25.0 | 26/31, 65.2 | 23.3/2.4 / 25.2/2.3 / 28.6/4.6 / 41.9/3.7 | 2.1 / 30.5 | 132 (2/31) | 18-6, 7 close |
| balanced, after | classic_lua | 29/31, 24.9 | 20/31, 60.4 | 23.4/2.7 / 26.4/3.1 / 34.3/6.1 / 45.3/7.0 | 2.0 / 51.8 | 70 (2/31) | 6-18, 7 close |
| classic+cheats, before | classic | 31/31, 48.0 | - | 25.5/6.4 / 49.7/5.4 / 50.2/6.8 / 50.2/5.5 | 7.1 / 61.5 | 48 (0/31) | 3-14, 14 close |
| classic+cheats, before | classic_lua | 31/31, 45.2 | - | 22.0/8.2 / 36.1/17.0 / 40.5/39.4 / 43.6/36.4 | 11.0 / 88.5 | 55 (0/31) | 14-3, 14 close |
| classic+cheats, after | classic | 32/32, 47.9 | - | 25.8/6.0 / 50.8/6.5 / 50.9/9.7 / 50.5/5.4 | 5.1 / 36.0 | 50 (0/32) | 6-12, 14 close |
| classic+cheats, after | classic_lua | 32/32, 45.4 | - | 21.3/4.0 / 35.2/5.8 / 40.0/14.7 / 41.3/22.2 | 11.3 / 79.8 | 49 (1/32) | 12-6, 14 close |

Readings:
- **Classic with the cheats:** Lua's economy still trails (35 against 51 citizens at 20 minutes, 41 against 50 at
  40); the army gap fell from 39 against 7 to 15 against 10 at 30 minutes; Lua's citizens lost by 20
  minutes stay about twice the C++'s. Won-lost 12-6 (14 close): near, still on Lua's side.
- **Balanced:** level to 40 minutes (45.3 against 41.9 citizens), then the C++ grows to 132 citizens at 90 minutes and
  Lua to 70; Lua loses 52 citizens against 31. Won-lost 6-18. The late economy is the gap: in 32 games the C++ built
  33 farms (Lua none), 53 docks (Lua 15), 29 granaries (11), 30 hospitals (10).
- Determinism: seeds 2 and 5, 60 minutes, --repeat 2: identical. 0 script errors.

Next: the farms (CheckBuildingCounts' farm entry never queues one in the port), the dock's water sites, then
ChooseUnitsToKill, the special units, the guard posts and the navy; walls last (the C++ built none in 90 minutes).

### 8.43 Boom's economy: the second town centre, the stock correction, the saving ramp (2026-11-10, EE-SIM)

The improved classic AI's economy rules (`balanced` in aipersonalities.yaml, §8.10-8.29) tried in `ai/boom.lua`, each
behind a switch of its `ECON` table, the logic in new library modules: `lib.econ_units` (orders on single citizens),
`lib.demand` (the split's stock correction, the saving ramp), `lib.econ_stats` (the bench metrics: the `econ` trace
line each minute with the idle town-centre seconds, the patches with more than 6 gatherers sent, the town centres).

Bench: EE-SIM, Small, Hard, 2 AIs and the idle human, no cheats, 90 minutes, `--ai-scripts` (a copy of `ai/` with
`rush` registered and the baseline as `boom0`), the worktree's open tree, 16 games a row (seeds 2-17 unless noted;
a row with fewer seats lost runs to the lobby's "slot 0 human" start fault). Boom is seat p2. "Result" is the mixed
score (§8.29) against the other seat.

| Row (boom against classic `balanced`) | Stone | Copper (reached) | Bronze | Citizens 20 / 30 / 40 min | Lost | Result W-L-C (defeated) |
|---|---|---|---|---|---|---|
| baseline (main + the metrics; identical after the merge of main) | 24.7 | 56.2 (12/16) | 2/16 | 33.7 / 36.6 / 45.6 | 63.7 | 5-9-2 (3) |
| + second town centre (Stone, from 30 citizens) | 24.7 | 57.9 (13/16) | 5/16 | 33.4 / 36.2 / 49.5 | 63.2 | 4-6-6 (2) |
| + stock correction v1, unreachable, ramp (Prehistoric too) | 22.7 | 53.2 (9/16) | 4/16 | 27.9 / 36.2 / 49.9 | 63.6 | 2-8-6 (2) |
| the same without the ramp | 24.7 | 55.7 (10/16) | 5/16 | 33.4 / 36.5 / 49.9 | 64.1 | 3-8-5 (1) |
| the same without the stock correction | 22.7 | 56.9 (8/16) | 4/16 | 27.9 / 35.4 / 48.8 | 59.2 | 2-9-5 (1) |
| v1 + army slack, no ramp | 24.7 | 54.5 (10/16) | 1/16 | 33.4 / 36.4 / 47.8 | 65.1 | 3-11-2 (2) |
| v1 + army slack + Stone ramp | 24.7 | 55.2 (10/16) | 2/16 | 33.4 / 36.4 / 47.4 | 55.9 | 4-10-2 (1) |
| stock correction v2 (food gives to wood), no ramp | 23.7 | 61.5 (14/16) | 0/16 | 32.6 / 33.9 / 52.1 | 86.9 | 0-9-7 (0) |
| v2 + Stone ramp | 23.6 | 60.1 (11/13) | 1/13 | 32.4 / 33.5 / 51.1 | 78.3 | 0-8-5 (0) |
| **as committed** (second town centre + unreachable) | 24.5 | 56.5 (10/13) | 4/13 | 33.5 / 38.7 / 55.3 | 62.8 | 4-5-4 (0) |
| baseline, seeds 18-33 | 27.8 | 52.4 (7/16) | 3/16 | 31.4 / 30.1 / 39.6 | 65.4 | 2-11-3 (2) |
| as committed, seeds 18-33 | 27.8 | 58.9 (7/16) | 3/16 | 31.4 / 29.9 / 40.6 | 69.4 | 1-12-3 (5) |

| Other matchups | Stone | Copper (reached) | Bronze | Citizens 20 / 30 / 40 min | Lost | Result W-L-C |
|---|---|---|---|---|---|---|
| baseline against `rush` | 21.5 | 47.8 (16/16) | 14/16 | 33.9 / 43.2 / 56.1 | 3.1 | 16-0-0 |
| as committed against `rush` | 21.5 | 49.6 (16/16) | 13/16 | 32.8 / 43.1 / 60.8 | 7.9 | 16-0-0 |
| baseline against `ageofai` | 22.3 | 49.5 (16/16) | 8/16 | 33.7 / 42.4 / 60.4 | 6.4 | 9-0-7 |
| as committed against `ageofai` | 22.3 | 49.6 (14/16) | 8/16 | 33.2 / 41.8 / 62.6 | 10.6 | 6-2-8 |
| mirror: as committed (boom) against the baseline (boom0), seeds 2-9 both seats | 22.0 / 21.5 | 52.9 (13/14) / 51.7 (14/14) | 6 / 7 | 33.4 / 43.7 / 64.4 against 34.6 / 45.4 / 64.4 | 11.6 / 20.5 | 4-2-8 |

What the rows show:
- **The second town centre** (`lib.econ_units.town_centers`: the finished settlement nearest the town with no enemy
  soldier within 12 tiles is filled with 5 citizens, idle ones first, held until they are inside; only below 2 town
  centres and before the Bronze Age) buys citizens, not pace: 4-9 more at 40 minutes in every matchup, the Copper Age
  0-2 minutes later (5 citizens and the growth that follows come out of the saving). Against the classic AI it lost
  fewer games on seeds 2-17 (6 and 5 against 9) and more on 18-33 (12 against 11, 5 defeated against 2): within the
  noise of 16 games. Against `ageofai` 6-2 against the baseline's 9-0 (Copper reached 14 against 16). The idle town-centre seconds roughly double with two producers: food, not training, holds the
  town back from the Stone Age's tenth minute (the one producer is the limit only in the first 6-8 minutes).
- **The stock correction** (`lib.demand.correct`, balanced's surplus 1000 / cut 50% and low stock 150 / floor
  shares): v1 took the low resource's gatherers from gold, stone and iron only, and never fired in the Stone Age's
  wood famine (wood under 50 in half its minutes: food 18 and the army's iron were the whole town). v2 lets food give
  down to 4 under its minimum: the famine minutes fell from 263 to 211 of about 650, the Copper Age is reached more
  often (14/16) but later (61.5 against 56-58), the citizens at 30 minutes fall (33.9) and 87 citizens are lost
  against 63. **Off by default.**
- **The saving ramp** (`lib.demand.pressure`, balanced's epoch_pressure start/full: the reserve grows from 0 to the
  whole cost): in the Prehistoric it started the reserve before the owner's 32 citizens and held them at 28 (the
  Stone Age 2 minutes sooner, the Copper Age reached less often); from the Stone Age it changed little. **Off by
  default**; the Stone and Copper plans keep `save_ramp` fields for the switch.
- **No reserve for an unreachable advance** (balanced's claim_needed): the reserve is dropped while a resource of the
  advance is short, no reachable site holds it (`memory.no_site`) and nobody gathers it. On; no trace line counts its
  firings yet.
- **The army slack** (past the soldiers-first window the reserve waits for 3/4 of the light army only): a trace
  showed a town 2 soldiers short (iron-bound, 18 of 20) with a food reserve of 0 for 10 minutes while it saved for
  the Copper Age (the missing army's cap `stock - missing x cost` follows the stock to 0), and its citizens took the
  food. The slack lost more games (11 against 8). **Off.**
- **Already in place** (no change needed): the gatherers sent to a patch never pass 6 in the C++ runner (`crowd` 0-0.4
  samples a game, at most 7 sent for a moment); the busy gatherers moved per 10 s are balanced's moves_per_cycle 3 in
  the C++ Rebalance, which a next seat runs; fishing from the Stone Age, boats on top of the food citizens; at most 2
  barracks and 1-2 ranges before the Copper Age; the claim range grows when the patches cannot take the food wanted.
- **The rush script is no test:** boom won 32 of 32 against it and lost 3-8 citizens a game.
- **Determinism:** the `--repeat 2` run of the committed state was held back by the gates (not run); the baseline's 16 games after the merge of main (857f6aa, new C++ queries) equal the ones before
  it in every number above.

Open: the Stone Age's 15-minute stall (citizens 32-36 from minute 15 to 30: the Prehistoric reserve, then the army
first) is where the Copper Age's 10 minutes against the owner's 46 go; the food places (18 on food, about 11 boats)
bind from there. The per-unit gatherer placement, builders and rally points (items 6-8 of the economy plan) are not
built: the runner already keeps the patches at 6.

**The second round** (after the merge of boom's defence, cfc4c9e: the light army a quarter of the citizens, at least
10; 32 games a row, seeds 2-33 against the improved classic, Small, Hard, 90 min; "Copper by 60" counts the seats
in the Copper Age by minute 60; the median over all seats takes a seat that never got there as last):

| Row | Stone (reached) | Copper (reached, median) | Copper by 50 / 60 min | Citizens 20 / 30 / 40 | Lost | W-L-C (defeated) |
|---|---|---|---|---|---|---|
| main's boom (the defence, no economy rules) | 25.0 (30/32) | 17/32, 59.9 | 2 / 9 | 31.0 / 36.6 / 52.9 | 43.8 | 5-15-12 (3) |
| + second town centre, ratchet, full target, unreachable | 25.0 (30/32) | **20/32, 56.4** | 3 / **12** | 30.6 / 35.6 / 55.6 | 54.6 | 6-14-12 (4) |
| the same without the second town centre | 25.0 (28/30) | 14/30, 57.0 | 1 / 10 | 31.0 / 36.2 / 52.3 | 43.0 | 3-15-12 (3) |
| the same with the citizens beside the army (parallel) | 25.0 (30/32) | 22/32, 56.8 | 4 / 14 | 30.6 / 36.2 / 53.4 | 49.6 | 6-16-10 (**6**) |
| main's boom against `ageofai` (16) | 22.3 | 16/16, 48.4 | 9 / 14 | 33.7 / 43.6 / 60.9 | 10.1 | 3-2-11 |
| the economy rules against `ageofai` (16) | 22.3 | 15/16, 47.4 | 10 / 14 | 32.9 / 42.9 / 68.2 | 12.2 | 2-4-10 |

- **What holds the citizens from minute 15 to 30** (`plan` lines classified, 32 games): 79% of the minutes 15-20 the
  Prehistoric plan's 32 citizens (the saving for the Stone Age: by design); 20-25 the same and the Stone Age's
  soldiers-first window (15%); 25-40 below every cap with under 50 food (52-63%): the soldiers, the citizens and the
  boats share a food income bound by about 18 food places.
- **What holds the Copper Age:** 5 minutes before the advance the furthest resource from its cost was gold or iron in
  about 3 of 4 games (food in the rest). Two causes found in the traces:
  - **the iron reserve at 0**: the missing light army's cap took the iron reserve to 0, the stock stood above the bank,
    so `stock_targets` wanted no iron gatherer and the army and the Copper Age waited on iron for the rest of the game
    (a third of the saving minutes had a short metal no gatherer was wanted for; 11 of 32 games 10 minutes or more).
    The gatherers now aim at the advance's whole cost (`full_target`), and the reserve ratchets (`ratchet`: what came
    in stays saved; the army and the citizens share only the missing soldiers' margin above it);
  - **the mines' settlements unpaid**: with wood at 5-55 the settlements by the home gold and iron mines were queued
    unpaid for minutes (`afford=0`), so those mines had no drop site and no gatherer. `wood_floor` keeps wood at its
    plan floor in the give-way while it is under 150 (benched below once the gates allow).
- **The second town centre** now pays: with the defence's larger army it is the rule that moves the Copper Age (20
  against 14 reached; 12 against 10 by minute 60).
- **The citizens beside the army** (the owner's "in parallel"): more Copper Ages (22) but 6 defeated against 4: off.
- **`ai.units{type = {...}}`** now takes a list (C++, `tests/ai_units/types.lua`: a list's ids are the union of its
  names' ids). Age of AI's `citizens_home` (econ.lua) had never run: with the list working it called an undefined
  global `d2` and every step from about minute 1 aborted there (1,274 script errors in one game) - fixed with a local
  helper. Its traces differ from about line 60 on.

**The overnight experiments E1-E5** (2026-11-11; the coordinator's protocol). Each one switch of `ECON` over main
1e422bd's boom (the baseline: its own `boom.lua`; with every switch off this boom's traces equal it until the switch
first acts - checked seed by seed), seeds 2-33, Small, Hard, 2 AIs, 120 minutes, against the improved classic (`cl`:
`--cheats none --personality balanced`) and the cheating classic (`cx`: Hard's seven cheats for the classic seat). All
on this branch's EE-SIM (the `ai.units` type-list fix included). A row keeps its rule only if the defeats do not rise in
any matchup and it gains somewhere. Stone and Copper are the median minute of the seats that got there (reached);
"Copper by 60" counts seats in the Copper Age by minute 60; W-L-C the mixed score.

| Experiment | Matchup | Stone (reached) | Copper (reached) | Copper by 60 | Citizens 20 / 40 | Defeated | W-L-C |
|---|---|---|---|---|---|---|---|
| baseline (main's boom) | cl | 24.9 (31/32) | 61.6 (20/32) | 7 | 32.9 / 54.6 | 6 | 1-28-3 |
| baseline | cx | 28.0 (22/32) | 74.7 (8/32) | 1 | 28.8 / 27.5 | 13 | 1-29-2 |
| E1: reserve ratchet + gatherers aim at the whole cost (the stall fix) | cl | 24.9 (31/32) | 61.0 (19/32) | 9 | 32.9 / 54.8 | 11 | 5-26-1 |
| E1 | cx | 28.0 (22/32) | 72.9 (8/32) | 2 | 28.8 / 28.3 | 16 | 1-27-4 |
| E1 + the citizens beside the army (parallel) | cl | 24.9 (31/32) | 58.9 (20/32) | 11 | 32.9 / 58.2 | 9 | 1-27-4 |
| E1 + parallel | cx | 28.0 (22/32) | 68.2 (7/32) | 2 | 28.8 / 29.3 | 18 | 0-28-4 |
| E2: the forage-runs-dry claim (claim and hunting to 42 tiles) | cl | 24.3 (32/32) | 62.1 (23/32) | 9 | 33.0 / 55.1 | 7 | 2-26-4 |
| E2 | cx (31) | 29.2 (25/31) | 92.4 (7/31) | 1 | 27.7 / 30.6 | 14 | 0-27-4 |
| E3: the second town centre (alone; E1 lost) | cl | 24.9 (31/32) | 62.0 (21/32) | 9 | 32.8 / 59.5 | 8 | 2-23-7 |
| E3 | cx | 29.3 (22/32) | 75.4 (8/32) | 2 | 28.7 / 32.2 | 13 | 1-28-3 |
| E4: Age of AI's epoch account from Copper (50% of the income; aims at the cost) | cl | 24.9 (31/32) | 60.2 (18/32) | 9 | 32.9 / 54.8 | 12 | 2-26-4 |
| E4 | cx | 28.0 (22/32) | 68.8 (8/32) | 2 | 28.8 / 28.3 | 16 | 1-28-3 |
| E5: wood keeps its floor under 150 (packing does not apply to boom's verbs) | cl | 24.9 (31/32) | 65.1 (20/32) | 8 | 32.9 / 55.2 | 10 | 3-27-2 |
| E5 | cx | 28.0 (22/32) | 71.0 (4/32) | 1 | 28.8 / 27.5 | 16 | 1-30-1 |

(E3's `cl` row merges 27 games with the 5 seeds re-run after the lobby's "slot 0 human" start fault; E2's `cx` lost
seed 17 to it. The baseline's `cl` games show 6 defeats; the variants that act only after minute 20 range 6-12 on the
same seeds, so a single row's defeats move by several games from chance alone.)

- **Nothing is kept.** Every rule raised the defeats in at least one matchup. The closest:
  - E3, the second town centre: 4-5 more citizens at 40 minutes in both matchups, Copper reached about as often, the
    same 13 defeats under the cheats, 8 against 6 without;
  - E2, the forage fallback: Stone reached in every `cl` game and in 25 against 22 `cx` games, but one defeat more in
    each.
- **E1** (the stall fix the coordinator asked for) moves Copper by 60 minutes from 7 to 9 (11 with parallel citizens)
  and the citizens at 40 minutes up to 58, but the extra saving (and parallel citizens) cost the town 3-5 more
  defeats in both matchups. The defence's army floor of 20 needs the food the ratchet keeps.
- **E4** is not Age of AI's whole economy: only its epoch account in boom's reserve (the account implies aiming at the
  cost, so E4 also carries E1's `full_target`). The army share that let Age of AI out-grow boom after 90 minutes is
  the defence's training block, not reproduced here.
- **E5's packing** (Age of AI's `tight_build`): boom places its buildings through the C++ `build near=` verb, so the
  packing code has no seam; E5 benched the Stone Age wood floor instead (the mines' settlements waited on wood).
- **E3 against Age of AI** (32 seeds): 3-11-18 against the baseline's 10-9-13 (Copper 51.6 against 50.9, all 32
  reached in both; citizens at 40 minutes 60.7 against 59.6). The mirror (E3 against main's boom, seeds 2-17 both
  seats) stopped after 5 games when the disk filled up; not counted.
- **Checks:** with every switch off, boom's traces equal main 1e422bd's line for line (seeds 2-4, 45 minutes: 3908,
  3760 and 3743 lines); `--repeat 2` of that set (seeds 2-3, 45 minutes) and of E3 gives identical traces (4206 /
  4059 and 4322 / 3963 lines).
### 8.44 The classic computer player in Lua: farms, docks, fishing (2026-11-11, EE-SIM)

The "Lua built no farms" of §8.42 counted farm build orders. The C++ classic places its granary with EEMakeBuilding,
which plants the farm foundations at once, and CheckBuildingCounts queues a farm request per unbuilt foundation (48
in 32 games). A script's granary (`ai.build`, EEMBuildUnit) plants its farms when finished, and the builder builds
them itself (TendNextFarm), so the port never orders one. The farmers are level: 3.5 against 3.8 at 90 minutes, farms
in 13 games each, the first at 74 and 78 minutes.

Changes:
- **FindShoreSite** tries the water site tiles in turn (`ai.water_sites`, new: CollectSiteTiles(2), the largest water
  body's tiles by the town's land, nearest first), skipping those within sqrt(75) of a dock, unplaceable, threatened
  or without a builder (asked at the first 8 placeable sites). The port took the one shore nearest the town:
  docks built 16 -> 31 in 32 balanced games (the C++ 51).
- **AssignGatherers** takes the gatherers waiting for room at their resource as idle, as the classic's list does
  (the own units' new `waiting` field).

Bench: as §8.42 (seeds 2-33, 90 minutes, open data); before = §8.42's b5, after = c3 (one balanced game failed at
the lobby in each).

| Matchup | AI | Copper | Citizens/army 10/20/30/40 min | Lost by 20 / all | Fishers at 90 min | Won-lost |
|---|---|---|---|---|---|---|
| balanced, before | classic | 26/31, 65.2 | 23.3/2.4 / 25.2/2.3 / 28.6/4.6 / 41.9/3.7 | 2.1 / 30.5 | 20.8 | 18-6, 7 close |
| balanced, before | classic_lua | 20/31, 60.4 | 23.4/2.7 / 26.4/3.1 / 34.3/6.1 / 45.3/7.0 | 2.0 / 51.8 | 2.6 | 6-18, 7 close |
| balanced, after | classic | 25/31, 64.5 | 23.2/2.3 / 25.2/2.2 / 28.5/4.5 / 40.4/3.7 | 2.0 / 30.9 | 21.9 | 16-8, 7 close |
| balanced, after | classic_lua | 22/31, 58.3 | 23.2/2.7 / 27.0/3.3 / 35.6/6.5 / 48.1/6.8 | 1.5 / 52.3 | 2.4 | 8-16, 7 close |
| classic+cheats, before | classic | - | 25.8/6.0 / 50.8/6.5 / 50.9/9.7 / 50.5/5.4 | 5.1 / 36.0 | - | 6-12, 14 close |
| classic+cheats, before | classic_lua | - | 21.3/4.0 / 35.2/5.8 / 40.0/14.7 / 41.3/22.2 | 11.3 / 79.8 | - | 12-6, 14 close |
| classic+cheats, after | classic | - | 25.8/6.0 / 50.8/5.4 / 50.8/9.4 / 50.5/5.1 | 4.8 / 30.8 | - | 8-10, 14 close |
| classic+cheats, after | classic_lua | - | 21.4/4.2 / 35.4/4.7 / 39.9/14.1 / 42.2/26.4 | 11.3 / 91.6 | - | 10-8, 14 close |

Readings:
- Within noise of §8.42 on both personalities; the docks do not yet make fishers. **The fishing gap is the navy:**
  the C++ trains war rafts at its docks (its sea share) and they sink the port's fishing boats, 18-27 a game where
  the port fished (seeds 11, 12, 14, 18, 20 of the balanced bench); the port has no warships and its boats sit idle
  after the naval flight. Its fishers peak at 10-16 and fall; the C++'s grow to 22.
- **Classic with the cheats, the early economy:** with cost100=1 the citizens cost nothing and the town centre's
  training time sets the pace; the port loses 11 citizens by 20 minutes against 5, two in five of them builders far
  from home (killed by clubmen, rock throwers and the C++'s towers). Up to 20 minutes it starts 443 towers against
  308 and orders 212 settlements against 142 (BuildSettlementFor). The C++'s HasEnemyNear reads every player's
  influence, seen or not; the port's sees what it saw (the fog deviation of ORIGIN), so its builders walk into the
  C++'s soldiers.
- Hospitals 22 against 39 and temples 57 against 70 started (balanced, b5): CheckBuildingCounts runs less often in the
  port (manager 4 in few rounds) - not yet looked into.
- Determinism: seeds 2 and 5, 60 minutes, --repeat 2: identical. 0 script errors (the first version of the shore
  finder had 300 budget errors a bench under the reveal cheat).

Next: the navy (warships from the docks: the sea share of ComputeDomainShares), the builders' and settlements' danger
test, the building-count check's frequency, then ChooseUnitsToKill (only at the population limit: low effect at
these caps), the special units, guard posts, walls.

### 8.45 The classic computer player in Lua: the navy and the information cheat (2026-11-11, EE-SIM)

§8.44 found the C++'s war rafts and frigates sinking the port's fishing boats and shooting its citizens on the shore
(287 of the port's units in 32 balanced games); the port trained no warship. Ported:
- **The sea in the unit selector:** ComputeDomainShares' sea share (GetDomainShare: the water tiles' share of the map,
  the own sea strength against the target's, the air unit weight's pull) and Calculate, the sea's candidate classes,
  the held units and counter classes within each domain's count, ChooseFavoriteSeaType and the favourite sea units
  (none: the count goes to land). A group's warships form a sea sub-group with AssignSeaTask (a known unit of the
  target on its water a quarter of the time, else a random point of that water).
- **CheckNaval:** every ship_maintenance_timer minutes the warships are topped up to ships_to_maintain (Hard: 1) by a
  sea-only group; **the sea explorers** (num_sea_explorers: 1, the fastest warship class).
- New generic queries: `ai.area` / `ai.area_point` (the continent manager's areas: the sea task's random point) and
  `ai.tuning()`'s map tiles.

**The information cheat as a capability.** The C++ classic's HasEnemyNear reads every player's units through the fog;
the port's read what it saw, so under classic with the cheats its builders walked into the C++'s soldiers (157 of its
citizens died building in the first 20 minutes over 32 games). A manifest entry may now declare `"sees_all": true`
(only classic_lua does); the seat's trace start line ends in `sees_all=1`, and `ai.enemy_near(x, y)` is then the C++
test; for every other AI it is the fog-honest EECPNextSiteThreat (AI_LUA_UNITS §11). Builder deaths by 20 minutes:
157 -> 87.

**The building-count check's frequency:** the port's EnableManagers and the civilization manager's 30-second check
match the C++ line for line; manager 4 runs when wood and stone stand at enable_stock_build. The C++ queues more
temples, universities and hospitals because its stock is higher more often, not by a different rule.

Bench (as §8.42; before = §8.44's c3; navy = n8 without sees_all; after = s1 with both):

| Matchup | AI | Citizens/army 10/20/30/40 min | Lost by 20 / all | Fishers at 90 | Won-lost |
|---|---|---|---|---|---|
| balanced, before | classic | 23.2/2.3 / 25.2/2.2 / 28.5/4.5 / 40.4/3.7 | 2.0 / 30.9 | 21.9 | 16-8, 7 close |
| balanced, before | classic_lua | 23.2/2.7 / 27.0/3.3 / 35.6/6.5 / 48.1/6.8 | 1.5 / 52.3 | 2.4 | 8-16 |
| balanced, navy | classic | 23.5/2.1 / 25.7/2.5 / 30.0/4.8 / 46.7/3.7 | 1.8 / 26.8 | 22.3 | 19-6, 7 close |
| balanced, navy | classic_lua | 23.0/2.8 / 26.7/3.2 / 34.9/6.2 / 44.8/7.1 | 1.4 / 45.8 | 5.2 | 6-19 |
| balanced, after | classic | 23.9/2.4 / 25.8/2.7 / 30.6/6.0 / 45.7/3.3 | 1.6 / 23.4 | - | 20-6, 6 close |
| balanced, after | classic_lua | 23.2/2.8 / 26.4/3.3 / 34.6/6.2 / 44.4/6.4 | 1.4 / 58.8 | - | 6-20 |
| classic+cheats, before | classic | 25.8/6.0 / 50.8/5.4 / 50.8/9.4 / 50.5/5.1 | 4.8 / 30.8 | - | 8-10, 14 close |
| classic+cheats, before | classic_lua | 21.4/4.2 / 35.4/4.7 / 39.9/14.1 / 42.2/26.4 | 11.3 / 91.6 | - | 10-8 |
| classic+cheats, navy | classic | 26.4/6.2 / 50.7/5.6 / 51.2/10.5 / 50.5/6.3 | 3.5 / 24.5 | - | 6-10, 16 close |
| classic+cheats, navy | classic_lua | 22.7/9.8 / 36.7/16.1 / 40.8/30.0 / 44.2/33.8 | 10.4 / 80.5 | - | 10-6 |
| classic+cheats, after | classic | 26.4/6.4 / 51.1/6.3 / 51.1/12.2 / 50.9/6.5 | 3.6 / 17.7 | - | 3-11, 18 close |
| classic+cheats, after | classic_lua | 23.2/9.1 / 40.0/16.2 / 44.8/25.3 / 44.7/30.8 | 6.8 / 69.8 | - | 11-3 |

Readings:
- **Balanced:** the port's fishers 2.4 -> 5.2 (the C++ 22); its units sunk by the C++'s ships 287 -> 280: the C++ fleet
  still wins at sea. The won-lost moved within noise (8-16, 6-19, 6-20). The late economy (citizens at 90 minutes)
  remains the gap.
- **Classic with the cheats:** the port's early economy is nearer (40 against 51 citizens at 20 minutes, from 35), its
  losses by 20 minutes halved; its army is now 2-3 times the C++'s at 20-40 minutes (it was 1.5 times before the
  navy; not yet explained) - the port now outplays the C++ here (11-3), the opposite of the goal.
- Determinism: seeds 2 and 5 (balanced) and 3 (classic with the cheats), 60 minutes, --repeat 2: identical. 0
  script errors.

Next: why the port's army outgrows the C++'s under classic with the cheats (its groups' slots and the sea shares),
the C++ fleet's edge at sea, then ChooseUnitsToKill, the special units, guard posts, walls.

### 8.46 The classic computer player in Lua: the garrison cheat, and why its army outgrew the C++'s (2026-11-11, EE-SIM)

Under classic with the cheats the port's citizens trailed (35 against 51 at 20 minutes) and its army was 2-3 times
the C++'s. With the cost cheat (cost100=1) a citizen costs nothing; the town centre's 28 s per citizen sets the pace.
A trace of the C++'s StartProduction showed it training at a capitol and one, then two town centres from minute 5:
its FillGarrison sends the idle citizens into a settlement and **tops the rest of the garrison up with no
citizens** (EEComplexUnit::AddGarrison(count + room)), so a settlement becomes a town centre for 1-4 citizens every
town_center_upgrade_timer (5) minutes. The port could only garrison citizens it had, found few idle ones, and never
upgraded a settlement.

Ported as a second explicit capability, `"fills_garrison"` (AI_LUA_UNITS §11; classic_lua only, `fills_garrison=1` in
the start line): `ai.garrison_fill(id, n)` is the original's call. The port's FillGarrison takes the waiting gatherers
too, fills the rest once citizens are on their way, and sends new citizens to the garrison every 2 s (UnitAdded).

Bench (as §8.42; before = §8.45's s1, after = g2):

| Matchup | AI | Citizens/army 10/20/30/40 min | Lost by 20 / all | Won-lost |
|---|---|---|---|---|
| balanced, before | classic | 23.9/2.4 / 25.8/2.7 / 30.6/6.0 / 45.7/3.3 | 1.6 / 23.4 | 20-6, 6 close |
| balanced, before | classic_lua | 23.2/2.8 / 26.4/3.3 / 34.6/6.2 / 44.4/6.4 | 1.4 / 58.8 | 6-20 |
| balanced, after | classic | 23.8/2.4 / 25.6/2.7 / 29.8/5.0 / 44.5/3.1 | 1.8 / 30.1 | 16-9, 7 close |
| balanced, after | classic_lua | 23.2/2.7 / 26.3/3.3 / 35.0/5.8 / 45.0/5.7 | 1.7 / 53.0 | 9-16 |
| classic+cheats, before | classic | 26.4/6.4 / 51.1/6.3 / 51.1/12.2 / 50.9/6.5 | 3.6 / 17.7 | 3-11, 18 close |
| classic+cheats, before | classic_lua | 23.2/9.1 / 40.0/16.2 / 44.8/25.3 / 44.7/30.8 | 6.8 / 69.8 | 11-3 |
| classic+cheats, after | classic | 26.3/6.4 / 50.8/6.5 / 51.0/11.4 / 50.5/6.7 | 3.7 / 23.4 | 0-8, 24 close |
| classic+cheats, after | classic_lua | 23.2/9.4 / 50.0/13.2 / 49.6/24.5 / 49.7/25.9 | 11.2 / 97.0 | 8-0 |

Readings:
- **Classic with the cheats:** the citizens are level from 20 minutes (50 against 51); 24 games close of 32.
- **The army gap is losses, not production.** Over the 32 games the C++ lost 6270 military units (the trace's
  `cp losses` other) against the port's 3882 (balanced: 3450 against 1932). The C++ requests more soldiers than the
  port (381 against 93 in seed 14 by 40 minutes) but loses them faster. Both attack equally often (4 attacks in an
  hour in seeds 10, 14 and 18, nearly all at the empty human seat), so the losses come from the other groups' fights
  (the defence, counter-attack and ships-to-maintain groups) - where lib.groups' fighting stands in for the
  C++'s sub-group state machines (ORIGIN: EECPAttackSubGroup, the ASG* classes). Not yet traced to a cause: the trace
  has no per-unit deaths for a C++ seat.
- **Balanced:** level to 40 minutes; one script budget error (group_pop at 86 minutes, a large army).
- Determinism: seeds 3 (classic with the cheats), 2 and 5 (balanced), 60 minutes, --repeat 2: identical.

Next: a per-unit death trace for the C++ seat (where and to what its soldiers die), then the fight itself; the C++
fleet's edge under balanced.

### 8.47 The classic computer player in Lua: where the soldiers die, the towers, the fleet (2026-11-11, EE-SIM)

**A `unit.death` trace line** for every soldier, ship or other non-citizen unit a computer seat loses, the C++
classic's too (EECPTraceUnitLost, trace only: seed 3's 60-minute samples identical with it): class, tile, distance
from the town centre, goal, killer (player:class:kind) and its distance.

**Where the soldiers die (classic with the cheats, seeds 2-9, 60 minutes).** The C++ lost 821 against the port's 417
(path points left out); 600 of the C++'s died 35 tiles or more from its town, to the port's soldiers (299) and
towers (298), with an attack goal - from minute 7 on, in the port's land (its upgraded settlements' towns). The port's
died mostly 15-35 tiles out (306). Both seats attack equally often (§8.46); the C++'s soldiers go into the port's
territory and die there, the port's stay nearer home. Not traced further yet: which of the C++'s groups (defence
ladder, CheckNaval's, the counter-attacks) take them there.

**The towers.** The port's FindBuildSite(tower) took the first placeable tile of the rings 2 .. range and tested the
spacing only there; the C++ goes ring by ring below the range and tests HasEnemyNear (CanBuildAt) on each tile. Ported
(with ai.enemy_near): towers 194 -> 123 in those 8 games (the C++ 125-135); over 32 games 579 against 480 under classic
with the cheats, 318 against 261 under balanced. The C++'s remaining edge in losses did not move (790 against 461):
the towers were not the cause.

**The fleet (balanced, 32 games).** The port lost 26 ships and the C++ 73; ships killed 80 of the port's soldiers and
65 of the C++'s. Of the port's 1866 citizens and boats lost, 336 died to the C++'s ships (292 to frigates) and 1442 to
its land soldiers (spearmen 382, bowmen 320, macemen 199). 433 of the citizens died fighting (the militia), 54% of all
after 60 minutes. The late gap under balanced is the C++'s army against the port's citizens, more than its fleet.

**group_pop:** GetUnitAllowance's count of the groups' population now reads the units' classes in one ai.get (a budget
error at 86 minutes with a large army); the same numbers.

Bench (32 seeds; before = §8.46's g2, after = t1):

| Matchup | AI | Citizens/army 10/20/30/40 min | Lost by 20 / all | Won-lost |
|---|---|---|---|---|
| balanced, before | classic_lua | 23.2/2.7 / 26.3/3.3 / 35.0/5.8 / 45.0/5.7 | 1.7 / 53.0 | 9-16, 7 close |
| balanced, after | classic_lua | 23.1/2.7 / 26.3/3.3 / 35.1/7.3 / 43.0/7.0 | 1.7 / 57.8 | 8-20, 4 close |
| balanced, after | classic | 23.8/2.3 / 25.5/2.2 / 28.2/4.3 / 44.9/3.1 | 2.0 / 28.3 | 20-8 |
| classic+cheats, before | classic_lua | 23.2/9.4 / 50.0/13.2 / 49.6/24.5 / 49.7/25.9 | 11.2 / 97.0 | 8-0, 24 close |
| classic+cheats, after | classic_lua | 22.9/10.2 / 49.8/16.9 / 50.3/28.2 / 50.2/27.6 | 11.5 / 83.4 | 8-3, 21 close |
| classic+cheats, after | classic | 26.6/5.8 / 50.6/5.2 / 51.0/11.3 / 50.4/6.0 | 3.1 / 23.4 | 3-8 |

Determinism: seeds 2 and 5 (balanced) and 3 (classic with the cheats), 60 minutes, --repeat 2: identical. 0 script
errors.

Next: which C++ groups walk into the port's land (a group id on the death line), the militia's fights under balanced
(433 citizens), then ChooseUnitsToKill, the special units, guard posts, walls.

### 8.48 The classic computer player in Lua: which groups die, the forests, the militia's odds, priests (2026-11-11, EE-SIM)

**The death line names the group** (trace only; seed 3's samples identical): the classic's sub-group holding the unit
(id, mission, its task's type, mode and distance) and its group's task. Over seeds 2-9 (classic with the cheats, 60
minutes) the C++'s soldiers lost 35+ tiles from home were mostly in defence sub-groups (mission 1) on a location task
in defence mode (180 of ~700), sub-groups whose attack found no territory (mission 0, 105) and counter-attacks
(mission 3, 92); most died 12-30 or 30+ tiles short of their task: on the way. Their places come from
AssignSubGroupTask's ladder - the forest is the continent manager's forest whose hull comes within 35 tiles, and the
place a random point of the whole forest, which can lie far off; the port took a tree within 35 tiles.

Changes:
- **The ladder's forest** through a new generic query, `ai.forest_point(x, y, r, pick, seed)` (AI_LUA_UNITS §11):
  the port's soldiers now walk as far as the C++'s (48% of the soldiers it lost died 35+ tiles from home under classic
  with the cheats, from 36%; it lost 3920 against the C++'s 5386, from 3074 against 5143).
- **The militia's odds** count every raider, seen or not (`ai.enemy_force`, only with `sees_all`), as EECPRMMilitia:
  within noise (the citizens lost fighting under balanced 442 against 433).
- **The priests and prophets** of AllocateSpecialUnits (15% of the groups, 1-3 priests, a prophet; 2% and 20% of the
  population limit): within noise.

Bench (32 seeds, 90 minutes; the port's line; the C++ about as before):

| Matchup | stage | Citizens/army 10/20/30/40 min | Lost by 20 / all | Won-lost |
|---|---|---|---|---|
| balanced | §8.47 (t1) | 23.1/2.7 / 26.3/3.3 / 35.1/7.3 / 43.0/7.0 | 1.7 / 57.8 | 8-20, 4 close |
| balanced | militia (m1) | 23.1/2.8 / 26.2/3.1 / 34.0/7.0 / 42.9/6.7 | 1.8 / 54.3 | 8-17, 7 close |
| balanced | forests (f1) | 23.1/2.8 / 26.2/3.1 / 33.8/7.1 / 44.2/6.8 | 1.8 / 53.8 | 9-17, 6 close |
| balanced | priests (sp) | 23.3/2.8 / 26.4/3.0 / 33.2/7.2 / 41.7/6.3 | 1.9 / 57.6 | 7-18, 7 close |
| classic+cheats | §8.47 (t1) | 22.9/10.2 / 49.8/16.9 / 50.3/28.2 / 50.2/27.6 | 11.5 / 83.4 | 8-3, 21 close |
| classic+cheats | militia (m1) | identical (the classic personality has no militia) | | 8-3 |
| classic+cheats | forests (f1) | 22.9/9.2 / 50.2/13.6 / 49.8/29.4 / 49.8/28.7 | 10.7 / 89.1 | 11-1, 20 close |
| classic+cheats | priests (sp) | 24.0/9.4 / 49.2/12.6 / 49.7/25.6 / 49.7/29.3 | 11.7 / 83.6 | 10-2, 20 close |

Readings: under classic with the cheats the port's army is still 2-3 times the C++'s from 30 minutes; its soldiers now
walk as far and die more, but the C++'s die more still (5386 against 3920): the fights themselves (lib.groups against
the C++'s sub-group state machines) are the remaining suspect. Under balanced the C++'s land army against the port's
citizens late in the game is the gap (§8.47). 3 budget errors at 85-89 minutes in 2 balanced games (late, large
economies: the step's load, not one place). Determinism: seeds 2, 5 (balanced) and 3 (classic with the cheats), 60
minutes, --repeat 2: identical.

Next: a fight trace (who shoots whom, the targets' choice) to compare lib.groups with the C++'s sub-groups; the late
step's budget; ChooseUnitsToKill (only at the population limit); guard posts on hills and choke points; walls.

### 8.49 The classic computer player in Lua: the fights, the intelligence files, the late budget, ChooseUnitsToKill (2026-11-11, EE-SIM)

**The fight trace** (trace only, the samples identical): the `unit.death` line gives the killer's own goal and its
target's kind, and a classic seat's citizens get the line too. Classic with the cheats, seeds 2-9, 60 minutes:

| killer | soldiers killed | by soldiers | by towers | citizens killed |
|---|---|---|---|---|
| the port (p2) | the C++'s 765 | 488 | 257 | 103 |
| the C++ (p3) | the port's 603 | 360 | 232 | 338 |

Soldier against soldier the port won 488 to 360: its armies were picked against what it had seen of the C++, while the
C++'s against everything the port had (its files read every unit). The port's soldiers that killed the C++'s were
rock throwers (295) and clubmen (269); the C++'s mostly rock throwers (201) - the counter picks differed.

**The intelligence files through sees_all.** `ai.player_file(player)` (only with `sees_all`, AI_LUA_UNITS §11) is the
C++'s EECPIntelligencePlayerFile: every unit of a player - the gatherers, the classes, the military and strength per
domain. The port's group sizes, sea shares and counters now come from it (the remembered counts and their mirror
floor without `sees_all`). Classic with the cheats: **3-0, 29 close** (from 10-2, 20 close); the port's army at 30
minutes 15.7 against the C++'s 10.4 (from 25.6). Balanced 7-21 (from 7-18).

**The late budget.** `ai.used()` (new: the instructions a call has used, for a script's profiling) found
BuildSettlementFor testing each of 120 late gatherers' resources one by one: 1.7M of the step's 2M instructions. It now
tests each resource once a call; seed 15's game is the same to the minute where the old one ran out (85 minutes).

**ChooseUnitsToKill** (KillUnitsForMilitary every 30 s): the citizens over the citizen share of the population limit
deleted, the idle first; 44 in 32 games of each personality.

Bench (32 seeds; before = §8.48's sp; after = kl, with all of the above):

| Matchup | AI | Citizens/army 10/20/30/40 min | Lost by 20 / all | Won-lost |
|---|---|---|---|---|
| balanced, before | classic_lua | 23.3/2.8 / 26.4/3.0 / 33.2/7.2 / 41.7/6.3 | 1.9 / 57.6 | 7-18, 7 close |
| balanced, after | classic_lua | 23.1/2.8 / 25.8/3.2 / 32.8/5.7 / 42.6/5.6 | 2.1 / 55.2 | 7-21, 4 close |
| balanced, after | classic | 24.0/1.8 / 26.1/2.2 / 30.5/3.8 / 45.5/4.1 | 1.2 / 33.9 | 21-7 |
| classic+cheats, before | classic_lua | 24.0/9.4 / 49.2/12.6 / 49.7/25.6 / 49.7/29.3 | 11.7 / 83.6 | 10-2, 20 close |
| classic+cheats, after | classic_lua | 24.0/10.2 / 49.5/13.3 / 49.8/15.3 / 49.8/21.5 | 12.1 / 101.1 | 4-0, 28 close |
| classic+cheats, after | classic | 25.5/6.3 / 50.5/6.8 / 51.1/11.2 / 50.8/5.6 | 4.4 / 24.5 | 0-4 |

Readings: under classic with the cheats the port is now near level (28 of 32 close); its army is still larger at 40
minutes (21.5 against 5.6), and it loses far more citizens (101 against 25). Under balanced the C++ wins 21-7: the
port's late economy (Copper in 19 games of 32 against 27) and its citizens lost to the C++'s soldiers. 0 script errors.

Next: the port's citizen losses (who kills them and while doing what: the next.death lines), the balanced late
economy, heroes, guard posts on hills and choke points (a query for the continent manager's hills and choke points),
walls.

### 8.52 The classic computer player in Lua: the balanced collapse, CancelStalledBuilds (2026-11-11, EE-SIM)

Under balanced (§8.49's kl) the port reached Copper in 19 games of 32 against the C++'s 27, and 13 of its games ended
under 40 citizens. Those games collapse early: from about 20 minutes the port's citizens stand still or fall while
the C++'s grow (seed 3: 22-19 from minute 15 to 35; seed 24: 19 at 20 minutes, 15 at 40, against 26 and 53).

**Seed 3: 13 of 22 citizens building one settlement for 15 minutes.** The `cp food` goals showed gather 6-9 and build
12-14 from minute 18 to 36: the builders stood waiting for the foundation's site to clear while another unit held it,
and AssignBuilders added more every 2 minutes. The C++'s CancelStalledBuilds (every build manager update:
FindStalledBuilder - a builder in WaitForBuildSiteToClear whose site is taken) deletes the foundation, puts the
request back at the front without a site and sends the builder home; the port had not ported it. Ported, with a
`stalled` field on the own unit records (AI_LUA_UNITS §11). 8 builds cancelled in 32 games (the C++ 15).

**Seed 24: the epoch reserve and the raids.** Both seats save the Stone Age's food from about minute 9 (the same
pressure and reserve: §8.40's port). The C++ kept 23-28 citizens through it; the port lost 8 of 24 to a few C++
clubmen (its explorers on harass, and the first counter groups) between minutes 10 and 22, trained none while it
saved, and after the advance never recovered (13-16 citizens, 4-9 food gatherers). The losses to a handful of raiders
are the port's defence (militia and lib.groups); not changed yet.

Bench (balanced only, 32 seeds, 90 minutes, --jobs 2; before = §8.49's kl, after = cs):

| AI | Copper | Citizens/army 10/20/30/40 min | Lost by 20 / all | Won-lost |
|---|---|---|---|---|
| classic_lua, before | 19/32, 57.5 | 23.1/2.8 / 25.8/3.2 / 32.8/5.7 / 42.6/5.6 | 2.1 / 55.2 | 7-21, 4 close |
| classic_lua, after | 22/32, 57.3 | 23.1/2.8 / 25.8/3.2 / 33.2/6.0 / 43.2/5.8 | 2.1 / 56.6 | 10-18, 4 close |
| classic, after | 27/32, 67.2 | 24.0/1.8 / 26.1/2.2 / 30.1/3.9 / 45.3/3.6 | 1.2 / 36.6 | 18-10 |

Determinism: seeds 2 and 5, 60 minutes, --repeat 2: identical. 0 script errors.

Next: the port's answer to a few raiders before the Stone Age (seed 24's clubmen: what the C++'s militia and defence
groups do that the port's do not), then guard posts on hills and choke points, heroes, walls.

### 8.50 Boom's mid-game army, attacks and the owner's Stone raid (2026-11-11, EE-SIM)

The owner's direction: the cheating classic is a bad matchup to report, not a target. The focus is boom's army from
the Copper Age and its attacks, then (a later direction) the Stone raid of the owner's exact build order (memory
boom-build-order). Opponents: the improved classic (no cheats) and Age of AI. Small, Hard, 120 minutes. Stone /
Copper / Bronze: the games that reached the epoch and its median minute. "Under 30": towns with fewer than 30
citizens at 120 minutes (boom-econ's less noisy measure).

**The Copper army (mb1, merged in 1f6edf5).** From the Copper Age the light army is a third of the citizens (35%),
up to 60 (25%, 40 before). On 64 seeds (2-65):

| Variant | Opponent | Games | Defeats | W/L/C | Copper | Bronze |
|---|---|---|---|---|---|---|
| baseline | Age of AI | 63 | 0 | 6/25/32 | 61, 52.5 | 54, 79.2 |
| | improved classic | 63 | 10 | 1/54/8 | 39, 62.8 | 11, 94.6 |
| mb1 | Age of AI | 62 | 0 | 11/10/41 | 60, 52.6 | 51, 81.3 |
| | improved classic | 64 | 9 | 0/47/17 | 40, 62.4 | 11, 90.7 |

**Off (32 seeds each; the improved classic's defeats against mb1's 4 on the same exe):**
- the guard's sweep of lone harassers near own buildings: 8 alone, 9 with mb1;
- attacks (lib.attack) from the Copper Age with 8 soldiers kept home: 8;
- the same with 16 kept home: 6, then 7 on the fixed scripts;
- attacks only while the home guard holds 100% or 150% of the largest raid of the last 15 minutes: 6 of 29 and 7 of
  32 (the baseline on that exe: 6 of 29);
- Age of AI's W/L/C moves by 1-5 games either way in all of them.

The attacks pull soldiers out of a town that the improved classic hits harder from about minute 70, when it reaches
180 population and attacks with 42-48 soldiers while boom stands at 90-100 citizens and 22-35 soldiers (boom-econ's
analysis). An army scaled towards the population cap from the Bronze Age (L50: 50% up to 90; L70: 70% up to 120) was
started and stopped for the owner's new direction; its partial rows (22 and 12 games against the classic) are too
few to read.

**The micro step's budget (fixed in 2c477f26):** with 100+ citizens away from a large raid, lib.defence re-tested
every one against every raider each second and overran 300k instructions (boom-econ's seed 19, from minute 81; mb6's
17 script errors). It now looks at 40 a second in turn and skips a raid whose box is out of reach: 0 errors in the
re-run, seed 19 clean, `--repeat 2` identical on seeds 4 and 19 against both opponents.

**The owner's Stone raid (lib.raid, plan.raid, off).** Once the economy is developed (boom-econ's
`memory.phase == "stone_developed"`; here a stub, 8 minutes into the Stone Age, since the signal was on another branch),
~20 soldiers go in one raid on the nearest neighbour's gatherers along `ai.route`, and ~20 more are trained as the
home reserve on the dispatch. 32 seeds against main's scripts (lb0):

| Variant | Opponent | Defeats | Under 30 | W/L/C | Copper | Bronze |
|---|---|---|---|---|---|---|
| lb0 (Stone: a quarter of the citizens, at least 20; army_first) | Age of AI | 0 | 0 | 6/1/25 | 32, 51.1 | 29, 82.7 |
| | improved classic | 8 | 18 | 0/25/7 | 20, 65.8 | 5, 93.0 |
| rs0 (Stone: 8 soldiers, army_first off) | Age of AI | 0 | 0 | 6/3/23 | 32, 46.7 | 26, 76.3 |
| | improved classic | 8 | 13 | 1/21/10 | 20, 60.6 | 6, 91.2 |
| rs1 (rs0 + the raid) | Age of AI | 0 | 0 | 5/6/21 | 32, 47.7 | 23, 81.0 |
| | improved classic | 10 | 13 | 1/20/11 | 21, 64.6 | 7, 89.4 |

| The raids (rs1) | Sent | Units | Sent at (min) | Out (s) | Own losses | Killed (citizens / soldiers) |
|---|---|---|---|---|---|---|
| improved classic | 20 of 32 | 10.3 | 55 | 332 | 4.7 | 1.9 / 1.9 |
| Age of AI | 27 of 32 | 10.7 | 43 | 347 | 0.3 | 3.4 / 0.1 |

- **rs0 is the find:** the owner's 5-10 Stone soldiers in place of the 20-soldier army_first bring the Copper Age 4-5
  minutes sooner against both opponents and the Bronze Age 6 sooner against Age of AI, with the same defeats and 5
  fewer towns under 30 citizens against the classic.
- **The raid as built does not do the owner's job yet.** The force is never trained in time: the build waits 10
  minutes and then goes at half size, at minute 43-55, after the Copper save should have begun. It kills 2-3
  citizens, and costs two defeats and 1-5 minutes against the classic. Its timing depends on the economy: boom-econ's
  phase signal and its pause of the Copper save while the raid builds (on boom-econ's branch) are the parts it needs.

**The raid on boom-econ's real signal (boom-econ's f0fc8d79 merged into boom-defence; its order on).** Both variants
turn on boom-econ's build order (ECON.order), which carries rs0's 8 Stone soldiers in its phase tables. o1 adds the
raid with the training priority: 3 soldiers an order while it builds, the citizens yielding (memory.raid.priority),
and a 15-minute gate of three quarters of the force. Same exe (with boom-econ's release fix), 32 seeds, --cache.

| Variant | Opponent | Defeats | Under 30 | W/L/C | Stone | Copper | Bronze |
|---|---|---|---|---|---|---|---|
| o0, the order | Age of AI | 0/31 | 0 | 3/15/13 | 31, 22.6 | 26, 47.1 | 23, 73.5 |
| | improved classic | 15/32 | 18 | 2/28/2 | 26, 25.0 | 14, 46.0 | 10, 74.8 |
| o1, the order + the raid | Age of AI | 0/31 | 1 | 2/14/15 | 31, 22.6 | 26, 51.4 | 23, 75.8 |
| | improved classic | 16/32 | 19 | 1/26/5 | 26, 25.0 | 15, 50.1 | 11, 76.7 |

| The raids (o1) | Built from (min) | Sent | Units | Sent at (min) | Out (s) | Own losses | Killed (citizens / soldiers) |
|---|---|---|---|---|---|---|---|
| improved classic | 43.2 (26 games) | 16 | 17.6 | 54.3 | 339 | 4.7 | 3.6 / 4.2 |
| Age of AI | 38.7 (31 games) | 20 | 19.5 | 49.8 | 329 | 1.4 | 3.9 / 1.6 |

- **The priority works:** the raid now goes with 18-20 soldiers (10 before). It kills about 4 citizens and 2-4
  soldiers for 1-5 of its own.
- **It goes late:** `stone_developed` comes at minute 39-43, so the force builds through minutes 40-54. The raid
  costs the Copper Age 4 minutes against both opponents and one defeat more against the classic. Its timing is set
  by the phase signal.
- **The order itself** (o0, boom-econ's, still off on main) has 15 defeats against the classic, against rs0's 8 on
  the earlier exe. That is boom-econ's bench to read; the raid adds to it, not to rs0.

**The raid on the earlier latch (boom-econ's c2a6e309).** Order on in both; 32 seeds, the same exe.

| Variant | Opponent | Defeats | W/L/C | Stone | Copper | Bronze |
|---|---|---|---|---|---|---|
| o2, the order | Age of AI | 0/32 | 5/17/10 | 32, 22.4 | 28, 47.3 | 23, 78.0 |
| | improved classic | 15/32 | 1/29/2 | 26, 24.6 | 16, 48.6 | 10, 74.6 |
| o3, the order + the raid | Age of AI | 0/32 | 4/14/14 | 32, 22.4 | 24, 49.9 | 21, 74.4 |
| | improved classic | 17/32 | 1/28/3 | 26, 24.6 | 13, 50.0 | 7, 80.6 |

- The build now begins at minute 35-38. It still takes 13-15 minutes: about 1.3 soldiers a minute with the citizens
  yielding. So the raid goes at minute 50-52 with 17-18 soldiers. It kills 2-3 citizens and 2-5 soldiers for 1-5 of
  its own, and costs the Copper Age 1-3 minutes and two defeats.
- The order's 15 defeats against the classic did not move with the wood-bank fix on this exe: 26 of 32 games reach
  the Stone Age, against 30 for rs0 on the earlier exe.

**The raid with two barracks and a range (boom-econ's c8289c62: the producers at the Stone start, gold and iron
reserved only while saving, food unlimited, the exact gathering).** Order on in both, 32 seeds, the same exe.

| Variant | Opponent | Defeats | Under 30 | W/L/C | Stone | Copper | Bronze |
|---|---|---|---|---|---|---|---|
| o4, the order | Age of AI | 0/32 | 0 | 4/11/17 | 32, 21.7 | 31, 47.4 | 28, 72.5 |
| | improved classic | 13/32 | 15 | 0/28/4 | 31, 23.5 | 19, 53.6 | 8, 69.4 |
| o5, the order + the raid | Age of AI | 0/32 | 0 | 4/11/17 | 32, 21.7 | 31, 52.3 | 29, 79.0 |
| | improved classic | 14/32 | 15 | 0/25/7 | 31, 23.5 | 18, 57.0 | 11, 81.8 |

| The raids (o5) | Built from (min) | Sent | Units | Sent at (min) | Out (s) | Own losses | Killed (citizens / soldiers) |
|---|---|---|---|---|---|---|---|
| improved classic | 38.0 (30 games) | 22 | 20.0 | 42.9 | 320 | 2.5 | 2.1 / 1.1 |
| Age of AI | 34.7 (32 games) | 28 | 19.6 | 43.6 | 358 | 1.2 | 2.8 / 1.4 |

- **The raid now goes out as the owner describes:** 20 soldiers, 5 minutes after the economy is developed, at minute
  43, in most games.
- **It does little:** 2-3 citizens and about 1 soldier killed, 1-2.5 of its own lost. Against the classic it gives
  three fewer losses (7 close games for 4) and one defeat more.
- **It costs the Copper Age:** 5 minutes against Age of AI and 3.4 against the classic. The Bronze Age comes 6-12
  minutes later. The raid and the reserve of 20 trained after it spend the stock that the Copper save would have had.
- 5 budget errors in one game (seed 19 against Age of AI, minute 113, 100+ raiders) are fixed: a large raid's distance
  test now takes every n/16-th raider. The seed is clean after the fix.

**The owner's decisions (2026-11-11): the raid off by default, the reserve after the Copper advance.** With the raid off
(`plan.raid` unset, the code and switch kept), the Copper plan's floor of 20 (35% of the citizens, up to 60) already
trains the 20-soldier reserve from the Copper advance. The literal reading, the Stone Age's 8 plus 20 as a Copper floor
of 28, was benched against it (order on in both, 32 seeds):

| Variant | Opponent | Defeats | W/L/C | Copper | Bronze |
|---|---|---|---|---|---|
| o4b, Copper floor 20 (kept) | Age of AI | 0/32 | 5/12/15 | 31, 47.4 | 28, 72.5 |
| | improved classic | 14/32 | 0/26/6 | 20, 55.0 | 7, 83.1 |
| o6, Copper floor 28 | Age of AI | 0/32 | 3/10/19 | 31, 47.4 | 28, 73.3 |
| | improved classic | 15/32 | 0/28/4 | 20, 55.0 | 10, 89.5 |

The floor of 28 is no better, so the Copper plan stays as it is.

**The fishing boats (the owner's rule of 2026-11-11: protected from the middle of the Stone Age).**
- **Docks heal boats.** A dock's area effect `dock_healing_1_rng_6` heals the `dock_repair` set within 6 tiles, and that
  set holds the family `ship`, the fishing boats' family.
- **The rule as built** (`lib.defence` `boats`): a boat hit by an enemy, or within a warship's reach + 2, runs to the
  nearest own dock out of that reach. It waits until healed and goes back to the same fish. A boat whose dock comes
  under fire moves on: to a dock out of reach, else 8 tiles away. The engine's naval flee is off for the script.
- **The towers:** `memory.boat_towers` publishes the docks and the boats' fishing grounds, and boom-econ's order builds
  a tower at each from stone_grow.
- **The bench:** 32 seeds, boom-econ's order on in both, main a11066d's classic (it counts its own boats now).

| Variant | Opponent | Defeats | W/L/C | Copper | Bronze | Boats lost / game | Boats alive (min 30-90) |
|---|---|---|---|---|---|---|---|
| bt0n, the order | Age of AI | 0/32 | 1/8/23 | 29, 54.7 | 23, 88.7 | 1.4 | 9.6 |
| | improved classic | 11/32 | 2/27/3 | 17, 61.2 | 5, 77.2 | 18.0 | 5.4 |
| bt2, + the boats' flight and towers | Age of AI | 0/32 | 3/10/19 | 30, 52.7 | 25, 80.2 | 0.0 | 9.7 |
| | improved classic | 13/32 | 1/28/3 | 17, 58.6 | 9, 82.2 | 18.0 | 6.0 |

- **Against the improved classic, the boats die as often as before (18 a game).** Its frigates outrange the
  Stone-Age towers and follow the boats. A first version that waited at the dock lost more (22.4 against 19.0, 690
  of them idle at the dock); moving on from a dock under fire brought it back to even.
- **Against Age of AI, which has no warships near, nothing changes** but its Copper and Bronze minutes (2 and 8 sooner
  with the boats alive).
- **Off by default.** Against the classic's warships, the boats need ships of their own or towers that reach them.


**The answers to the frigates (the owner: "towers and sometimes a ship works for me").** Each is one switch on top of
bt2 (boom-econ's order, the boats' flight and towers). 32 seeds against the improved classic, main 998932c's exe.

| Variant | Defeats | Under 30 | W/L/C | Copper | Bronze | Boats lost | Boats alive (min 30-90) | End citizens / army |
|---|---|---|---|---|---|---|---|---|
| b0 = bt2 | 13 | 19 | 1/28/3 | 17, 58.6 | 9, 82.2 | 18.0 | 6.0 | 35.9 / 11.4 |
| B1 towers' range + attack from the first warship seen (`plan.tower_up_ships`) | 13 | 18 | 1/27/4 | 17, 58.6 | 9, 82.2 | 18.7 | 6.0 | 37.5 / 12.5 |
| B2 the boats' towers on the open-water side of the fish (`shore_towers`) | 14 | 19 | 2/27/3 | 17, 58.4 | 9, 85.7 | 16.8 | 6.3 | 36.2 / 10.9 |
| B3 two warships guarding the fishing grounds (`plan.boat_guard = 2`) | 13 | 16 | 2/27/3 | 19, 58.6 | 12, 87.2 | 16.4 | 6.3 | 42.4 / 13.1 |
| B4 = B1 + B3 | 13 | 16 | 2/26/4 | 19, 58.6 | 12, 87.2 | 15.6 | 6.3 | 43.4 / 13.5 |

- **B4 is the best:** 2.4 fewer boats lost a game, 3 fewer towns under 30 citizens, 7 more citizens at the end, and
  the same defeats. Two more games reach the Copper Age and three more the Bronze Age.
- **Against Age of AI B4 equals b0** (0 defeats, 3/10/19, Copper 52.7, Bronze 80.2): no warships come.
- **B1 alone changes nothing.** The towers' attack upgrade never completed (none available or affordable when the
  frigates came); the range upgrade was already researched.
- **Cost:** the two warships are bought from the first enemy warship seen, so they come late (about minute 70 on
  seed 3).
- **Kept off by default** until it merges with boom-econ's order.

**B4 in time.** Across b4's 32 games against the improved classic, the first enemy warship comes in 23: at minutes
34-91 in 18 of them and at 109-113 in 3; 9 games see none. B4's guard order went out 1-12 minutes after the first one,
unaffordable under the epoch reserve. The towers' attack upgrade was refused for cost in every game. Two fixes:
- `memory.priority_spend` (boom) carries what the answer still needs: the missing warships, and the upgrade's next level
  at its real cost. The cost comes from the new `ai.improve_cost` (AI_LUA_UNITS §11).
- boom-econ's reserve gives way by that much (b8fa3e89). Its order also moves up to 4 food gatherers to wood while the
  wood is short (91abc7d8).

32 seeds against the improved classic, boom-econ's order on, the same exe:

| Variant | Defeats | Under 30 | W/L/C | Copper | Bronze | Boats lost by 60 / 90 / all | Boats alive (30-90) |
|---|---|---|---|---|---|---|---|
| t0, B4 | 10 | 13 | 1/20/11 | 22, 57.3 | 10, 80.2 | 2.3 / 9.2 / 18.6 | 7.5 |
| t1, + the reserve gives way | 11 | 13 | 1/20/11 | 21, 55.8 | 9, 89.0 | 2.1 / 10.4 / 17.7 | 7.6 |
| t2, + up to 4 gatherers to wood | 11 | 14 | 2/18/12 | 24, 57.7 | 10, 92.0 | 1.7 / 7.3 / 16.5 | 7.9 |

- **t2 cuts the boats lost by minute 90 by a fifth** (7.3 against 9.2) and two more games reach the Copper Age. It
  has one defeat more (11 against 10, within the noise), and the Bronze Age comes later (92 against 80, 10 games).
- **The towers' attack upgrade cannot be bought after their range upgrade.** It was refused 1,392 times in t2, always
  at "level=1 max=1", whatever the stock. The game's units have a limited number of improvement points, and the
  range upgrade boom buys from the Stone Age takes the towers'. B1's attack half is dead weight, and its cost is
  dropped from the priority spend.

**The small raids (2026-11-12).** Against the improved classic, boom's Stone-Age food income was a fifth below its
income against Age of AI (143.5 against 177.4 a minute). The classic raided it 12 times in the Stone Age, against 2
for Age of AI, with 16 citizens lost against 8. 46% of those citizens died to 1-2 raiders, 44% while fleeing, and 42%
with no own soldier within 10 tiles. Three switches, on the boom_order setup (boom-econ's 5da1c67d), 32 seeds:

| Variant | Classic: defeats | Under 30 | W/L/C | Stone food a minute | Stone citizens lost | Age of AI: defeats, W/L/C |
|---|---|---|---|---|---|---|
| s0 | 9 | 13 | 2/20/10 | 154.4 | 14.1 | 0, 3/10/19 |
| S1 militia at 2 x and short flights against 1-2 raiders (`small_raid`) | 11 | 17 | 2/20/10 | 150.6 | 15.6 | 0, 2/14/15 |
| S2 the 3 nearest guard soldiers to raids of 1-3 (`detach_small`) | **5** | 8 | 6/23/3 | 153.2 | 11.6 | 0, 3/11/18 |
| S3 back to work sooner (`fast_return`) | **6** | 7 | 2/22/8 | 161.6 | 11.9 | 0, 5/12/14 |
| S1 + S2 + S3 | 11 | 13 | 5/21/6 | 154.1 | 14.9 | 0, 4/9/19 |
| S2 + S3 | 7 | 10 | 3/21/8 | 164.0 | 11.7 | 0, 5/13/14 |

On 64 seeds (2-65):

| Variant | Opponent | Defeats | Under 30 | W/L/C | Copper | Bronze | End citizens |
|---|---|---|---|---|---|---|---|
| s0 | improved classic | 19 | 26 | 3/43/18 | 46, 54.6 | 23, 78.9 | 53.7 |
| | Age of AI | 0 | 0 | 5/22/37 | 63, 50.2 | 52, 77.3 | 102.9 |
| S2 + S3 (on) | improved classic | **12** | **18** | 6/41/17 | 49, 57.1 | 25, 88.1 | 65.5 |
| | Age of AI | 0 | 0 | 9/20/35 | 63, 49.5 | 56, 79.8 | 101.6 |

- **S2 and S3 are on by default.** The Stone food is 10-15 a minute higher against the classic (164 and 149 against
  154 and 133 on the two halves of the seeds), defeats fall from 19 to 12 of 64, and the towns end with 12 more
  citizens.
- **S1 is off.** The militia against small raids loses citizens to the ranged raiders it now fights.
- **Benched only on boom_order.** The default boom shares lib.defence and lib.guard and gets the two switches too.

**The late game: iron (2026-11-12).** With S2 and S3 on, no town of the default boom falls after the Copper Age.
What holds it back is the Bronze advance: its save sits unpaid for 44 minutes on iron alone (stock 3-27, income 0-75
a minute), because the army is exempt from the reserve and spends the iron as it comes in. Two switches against
that, and B4 (warships from the dock) for the default boom, 32 seeds against the improved classic, the m13 exe:

| Variant | Defeats | Bronze (games, median min) | Dark | Save unpaid (min) | Boats lost after Copper |
|---|---|---|---|---|---|
| boom, base | 7 | 9, 93.7 | - | 44 | 24 |
| boom, `iron_free` (soldiers without iron while the iron is short) | 8 | 9, 90.2 | 5 | 42 | 21 |
| boom, `iron_free` + boom-econ's `iron_guard` | 8 | 8, 89.4 | 4 | 43 | 21 |
| boom, B4 (`boat_guard`, `tower_up_ships`) | 8 | 8, 90.1 | - | 39 | **14** |
| boom_order, base | 6 | 11, 79.5 | 7 | 58 | 13 |
| boom_order, `iron_free` | 6 | 14, 78.7 | 10 | 56 | 11 |
| boom_order, `iron_free` + `iron_guard` | 6 | **16**, 83.2 | 10 | 55 | 14 |

- **`iron_free` with `iron_guard` helps boom_order:** 16 games reach the Bronze Age against 11, at the same 6
  defeats. The save still waits about as long; the gain is in games that would never have paid it.
- **It does not help the default boom,** whose iron income is too low for the saving to matter. Off there.
- **B4 cuts the default boom's boats lost after the Copper Age by 40%** (14 against 24) and the save's wait by 5
  minutes, for one defeat more (within the noise).
- Both switches stay off by default here (`plan.iron_free`, `econ.iron_guard`): the choice is the owner's.

**The waves after Copper (2026-11-12).** On boom_order against the improved classic, with both iron switches on,
175 incidents of 3-13 foes after the Copper Age killed 592 citizens, against 337 lost to raids of 1-2. A wave is
typically 4 foes, lasts about 50 s, and comes about 15 tiles out. 84% of the dead were killed by land soldiers, at 1.6
tiles: bowmen, macemen, short swords, phalanxes, horsemen and war elephants. 54-60% were running when first hit. The
losses follow the army: 5.9 citizens a wave with 0-5 soldiers, 1.5 with 25 or more. With a temporary trace of the
guard's view (168 waves):
- in 85 waves the guard saw only clusters of 1-3, so S2 sent 3 soldiers to each and the rest waited at the post;
- in 47 it engaged, losing 4.6 citizens a wave;
- in 27 it had no soldiers (6.0 a wave);
- in 8 it held off on the odds.

The iron switches starved the army: under 10 soldiers in 28% of the minutes after Copper, against 9% without them.
iron_guard reserved the epoch's iron that iron_free only released while the stock was below the cost. With no archery
range, iron_free trained nothing. An unaffordable order sat queued and kept its class from being ordered again.

Fixes, 32 seeds (2-33), the m14 exe (the FindGatherSpot crash fixed):

| Variant | Defeats | Under 30 | Copper | Bronze | Army under 10 | Wave dead |
|---|---|---|---|---|---|---|
| A0 base (both switches off) | 6 | 10 | 22 | 11 | 9% | 476 |
| A1 iron switches fixed + `train_spare` | 7 | 10 | 22 | 14 | 20% | 1040 |
| A2 `train_spare` only | 7 | 10 | 22 | 11 | 13% | 663 |
| A3 iron switches fixed | 7 | 8 | 24 | 16 | 26% | 718 |
| B1 `detach_r` 12 | 6 | 6 | 25 | 13 | 10% | 597 |
| C1 `guard_refuge` | 9 | 12 | 21 | 11 | 4% | 447 |
| C2 `guard_refuge` from Copper | 7 | 11 | 22 | 12 | 8% | 475 |

On seeds 34-65, B1 had 8 defeats against A0's 5 (14 against 11 of 64).
- **None is better than the base.** The fixed iron switches still starve the army: a ranged-only guard cannot keep
  its numbers. Holding back an unaffordable order (`train_spare`) is worse than queueing it, because a queued order
  has the first claim on the income. All of them stay off (`plan.iron_free`, `plan.train_spare`, `detach_r = 0`,
  `guard_refuge`).
- The waves are an army-size problem: the guard engages them when it has the soldiers.

**The Bronze save (2026-11-12).** On boom_order (base, the m14 exe, 32 seeds against the improved classic), a
per-minute trace of the save found 914 minutes of saving for Bronze in 22 games:
- **Iron** (1320 for Bronze) was short in 849 minutes, and the only resource short in 450. Food was short in 389 and
  gold in 320. Stone is not in the cost.
- **Iron income:** about 12 gatherers against the want of 18, bringing in about 65 iron a minute. The soldier orders
  asked for about 41 of it. The slow saves had 6-10 iron gatherers and 33-52 iron a minute; the fast ones had 16-20
  and 95-107.

Two fixes:
- boom-econ's walker to claimed sites nobody has seen (1fe7a900, always on in the order) brought the unexplored iron
  mines into use, and its ECON.iron_push found nothing more to do (off).
- Here, `plan.iron_lean`: from the Copper Age while saving, this percent of the iron infantry's share is trained as
  ranged soldiers.

| Variant (with the walker) | Seeds | Defeats | Copper | Bronze | Dark | Wave dead | Army under 10 |
|---|---|---|---|---|---|---|---|
| P0 base | 2-33 | 5 | 23, 52.9 | 15, 75.1 | 8 | 374 | 9% |
| P2 `iron_lean` 50 | 2-33 | 5 | 23 | 17, 76.7 | 12 | 220 | 4% |
| P12 + `iron_push` | 2-33 | 5 | 23 | 17, 76.7 | 12 | 218 | 4% |
| P0 base | 34-65 | 3 | 26, 53.4 | 16, 79.9 | 11 | 573 | 5% |
| P2 `iron_lean` 50 | 34-65 | 3 | 26 | 18, 81.4 | 11 | 720 | 7% |

- **`iron_lean` 50 on 64 seeds:** Bronze in 35 games against 31 and Dark in 23 against 19, at the same 8 defeats. The
  wave dead are equal (940 against 947).
- **The save is no shorter** (about 60 minutes unpaid in both). What it waits on after the walker is not measured yet.

**The Bronze save with the walker (2026-11-12).** The same per-minute trace on the merged code (the walker, iron_lean
50; 32 seeds against the improved classic). 23 games saved for Bronze in Copper, 792 minutes in all:
- **What was short:** iron in 655 minutes (the only one in 176) and gold in 515 (76). Gold was short in 320 before;
  iron_lean's bowmen cost gold.
- **During the save:**

| Resource | Gatherers | Income a minute | Stock growth a minute |
|---|---|---|---|
| Iron | 14.7 | 79 | 51 |
| Gold | 13.3 | 68 | 44 |

- **The rate:** about 5.4 a gatherer a minute. boom-econ's deposit trace puts the engine's pace at 5.6-6.0, so drop
  sites are not the lever.
- **The fast saves** (12-20 minutes) had 17-19 on iron at 100-110 a minute. **The slow ones** (s2, s4, s28, s5: 54-66
  minutes, unpaid at the end) had 7-12 on iron at 27-65 a minute, all of it spent replacing soldiers lost to the
  waves.
- **The lever is the count of iron and gold gatherers** in the slow games. boom-econ's settlement caps (a03c6b9e) are
  one cause.

**The Stone soldiers paced (2026-11-12, `plan.soldier_pace`).** The owner trained his Stone soldiers about 1 a minute,
where boom_order trained its 8 at once. `soldier_pace = 60000` trains one a minute after the advance, and all at once
with enemy soldiers in sight or under a threat. 32 seeds, iron_lean 50 in both:

| Variant | Opponent | Stone food a minute | Citizens at +15 min | Citizens gained a minute | Stone lost | Defeats | Bronze |
|---|---|---|---|---|---|---|---|
| Q0 | improved classic | 162.6 | 65.1 | 2.26 | 13.0 | 5 | 17, 76.7 |
| Q1 paced | improved classic | 165.7 | 67.6 | 2.43 | 13.1 | 6 | 17, 82.9 |
| Q0 | Age of AI | 196.8 | 74.6 | 2.79 | 8.2 | 0 | 27 |
| Q1 paced | Age of AI | 200.1 | 76.3 | 2.91 | 8.6 | 0 | 28 |

About 2 citizens more by Stone +15 against both opponents, at the same losses; nothing clear beyond (Bronze later
against the classic). It stays off. Against the classic, enemy soldiers are in sight much of the Stone Age, so the
pace rarely holds the soldiers back.

### 8.51 Boom's economy after Stone; the owner's build order (2026-11-11, EE-SIM)

The owner's direction: boom's economy after Stone (epoch pace through Copper, Bronze and later). The first two rounds
(switches over boom; 32 seeds 2-33, Small, Hard, 120 min, improved classic `cl` and Age of AI `aoa`) were then
superseded by the owner's exact build order (memory boom-build-order.md), now `ai/lib/build_order.lua` behind
`ECON.order` (off by default until it beats the baseline).

**Round 1 and 2 (switches, all off):** what holds the epochs back. Against the improved classic the mines stay half
empty in Stone and Copper (2-3 on gold and iron against 7-13 wanted): the classic's lone slingers stand by them under
THREAT_MIN, and the reserve's missing-army clause sets gold's want to 0. In every matchup the citizens past the plan's
places go to wood (30-48 on it against 18-20 wanted, 2000-5000 in stock). Copper costs 1584 food, 844 gold and 844 iron
at Hard. Measured: `full_target` (more Copper Ages before 60 but 10 defeats against 4), `wood_surplus` plus the Bronze
reserve sooner (Gbc: against the improved classic Copper 24/32 against 20, towns under 30 citizens 12 against 18, the same
8 defeats; against Age of AI 10-9-12 against 15-5-12), `places`, `paid_go` (never fires) and `useful_cap`: none kept.
"Under 30 citizens at the end" is reported beside defeats since: the improved classic reaches 180 population by minute
70 and grinds boom down, so whether a collapsing town is at 0 by minute 120 is a race (Gbase-cl 8 defeats against mb1's
4: the same collapses, 4 of 5 extra were near 0 in both).

**The build order** (phases `pre`, `stone_start`, `stone_grow`, `stone_developed` latched, `copper`; `memory.phase` is
the army's signal, `memory.raid.state` its answer): each phase's gatherers filled in the owner's order as the citizens
come (`exact = 1` on the gather order keeps no gatherer of slack), the plan overlaid per phase (the army's soldier
fields: boom-defence's rs0), the Prehistoric wood bank while saving, a dock and 10 boats first in Stone, a second
barracks and a range, gold and iron reserved only while saving (the raid's spearmen), the Copper save at
`stone_developed`, 3 granaries in Copper, the town centre's gathering techs epoch by epoch (hafts, hand axe,
fine-flaked tools, metal working, hardened chisel, falconry, saw, amalgamation, shaft mining, ...), and the settlements
placed in Lua (`ai/lib/place.lua`: the tile touching the resource units, gap 0, `ai.can_place`, built with `ai.build`
at that tile; the C++ near=site placement is unchanged for the other scripts).

The acceptance check (`order_check.py`, the boom-econ scratchpad): per phase the true gatherer counts (the engine's
`cp stats`, land food without the boats), boats, buildings; income a minute against the owner's playtest with their
civ's free bonuses out (gold, iron and stone / 1.25, Expansionism's garrison of 5 at every drop site in
`EEDepositLoad`; stone / 1.2 more for their stone item); a gatherer's rate a minute; citizens gained a minute in Stone.

| Row (32 seeds) | Matchup | Stone | Copper (reached) | Bronze (reached) | Defeated | Under 30 | W-L-C | Gold / iron a minute, 30-45 |
|---|---|---|---|---|---|---|---|---|
| baseline (Gbase2) | cl | 25.1 (30) | 53.9 (21) | 81.5 (9) | 9 | 12 | 3-22-7 | |
| O1: the order | cl | 25.0 (26) | 46.0 (14) | 74.8 (10) | 15 | 18 | 3-23-6 | |
| O3: + food not capped at the patches, raid iron, barracks 2 + range, exact | cl | 23.5 (31) | 53.6 (19) | 74.4 (8) | 12 | 16 | 2-25-5 | 21 / 29 |
| O5: + gatherers only by a touching settlement | cl | 27.1 (30) | 60.0 (11) | 87.7 (2) | 14 | 22 | 1-28-3 | 12 / 12 |
| O8: O3 + touching settlements, techs, no Stone cap, food reach | cl | 26.7 (32) | 56.3 (18) | 84.3 (8) | 15 | 18 | 2-27-3 | 19 / 21 |
| baseline (Gbase2) | aoa | 22.2 (31) | 50.4 (31) | 80.9 (27) | 0 | 0 | 16-7-8 | |
| O1 | aoa | 22.5 (32) | 47.6 (27) | 73.5 (23) | 0 | 0 | 12-12-8 | |
| O3 | aoa | 21.7 (32) | 47.3 (31) | 72.7 (27) | 0 | 0 | | 44 / 50 |
| O5 | aoa | 21.5 (32) | 63.0 (26) | 103.7 (16) | 0 | 1 | 10-13-9 | |
| O8 | aoa | 21.9 (32) | 52.8 (32) | 82.5 (27) | 0 | 0 | 12-7-13 | 37 / 42 |

The requirement: Stone ~21, Copper ~45, Bronze ~66; gold 81 and iron 74 a minute in minutes 30-45.

- **O3 against Age of AI is the owner's pace** to Copper (47.3) and 8 minutes ahead of the baseline on Bronze; against
  the improved classic it costs 3 defeats. O8's additions slowed it again (Copper 52.8): not yet sorted out which.
- **The mines' rate is not the gap.** A deposit trace (TEST ONLY, `EE_AI_EXP=deposits` in `EEDepositLoad`; game samples
  identical with and without it) times a working gatherer's loads: gold 160 s, iron 151 s a load of 15 (5.6 and 6.0 a
  minute; the requirement 6.1, 148 s), food 112 s, wood 122 s. The owner's 7.6 is the same pace times their civ's
  garrison bonus (+25%, 15 a load becomes 19). Boom's gold and iron income is short by headcount: too few on the mines
  through Stone (the developed check fails in most games) and the mines' gatherers re-tasked (a probe: one gatherer 404
  of 820 s on gold, the rest on stone, wood and building).
- **The touching settlements** (lib.place) build at gap 0 on every smoke site; counting the mines' places only from
  touched units (O5) left the mines empty while the settlements waited on wood and cost 5 defeats: reverted.
- **The citizens** grow 1.2-1.9 a minute in Stone's first 15 minutes against the owner's 3.4 (his Expansionism trains
  at every drop site; boom has 2 town centres).
- **Engine fixes on the way** (both in AI_LUA_UNITS §11): a citizen released at work went onto no gatherer list (18 of
  33 in one game, the counts wrong for the rest of it), and `gather(exact = 1)`.

Open: the O3 to O8 slowdown; gold and iron 12-18 held through Stone; the citizens' growth (a third town centre or
earlier second one); the losses against the improved classic (boom-defence's o4b: in 12 of 14 the Stone forage ran out
before Copper was paid; O8's food reach did not yet move it).

**Later rounds (2026-11-12), to the playtest entry `boom_order`.** The order went to the owner's playtest as its own AI,
`boom_order` ("Boom (owner order)", `ai/boom_order.lua` through `boom.configure`; the default `boom` keeps the order
off). What changed on the way, each benched on 32 seeds against the improved classic and Age of AI:

- **The settlements touch from the start** (the owner's rule; lib.place's builders now 3 from within 20 tiles, which
  brought its pace up to the C++ verb's); the Prehistoric food bank at the advance (240 food) is off: it put the
  Stone Age 2 minutes later.
- **The Stone Age's town centres wait on food, then on the save.** A TEST ONLY spending trace (`EE_AI_EXP=spending`
  in `EEComputerPlayer::Pay`; game samples identical with it) shows Stone's food against Age of AI: 185 a minute in,
  89 on citizens (2.2 a minute), 20 on soldiers, 23 on the epoch, 1 on the techs (the techs cost nothing that matters).
  `stone_developed` now latches on the owner's targets only (12+ gold and iron, 6 stone, 10+ wood, 10 boats; 30 minutes
  as a safety); the Stone phases train to 76 citizens. Neither more town centres (C2) nor more mines (C7) moved gold
  and iron: the citizens, and behind them the food, are the limit.
- **The boats.** The owner's own pace is one dock at once and a boat every 30 s (10 by +7 minutes). A second dock
  (D1) and a larger wood bank (Wx) did not help. The late docks had wood but no dock site: the dock finder knew 9-18
  fish within 30 tiles and no shore tile in sight for them (a building goes only on seen tiles). The shore scout
  (`BO.OPT.shore_scout`, on in boom_order: a wood gatherer walks a ring 16 tiles round home while the Stone Age is
  saved for) brought "10 boats by +7" misses from 9 to 5 of 32 against Age of AI and Bronze from 26 to 29 games, the
  improved classic unchanged.
- **The Bronze save's iron.** boom-defence found the save unpaid 44-60 minutes on iron alone. The further iron mines
  stood claimed but unexplored for 30 minutes (gatherers go only to seen sites): lib.place now sends a walker to a
  claimed site nobody has seen (on in the order). On seed 17 that took the Copper Age's iron from 6 to 18 gatherers
  (Bronze at 63.1). `ECON.iron_push` (claim iron out to FAR_REACH and move food to iron while the save lacks iron)
  adds nothing on top of it and stays off; `ECON.iron_guard` stays off. boom-defence's `iron_lean` 50 (half the guard's
  iron infantry share to ranged during the Bronze save; 64 seeds: the same defeats, Bronze 35 games against 31) is on
  in boom_order.
- **boom_order against the improved classic is level with the default boom** (11 defeats each on main e8067a9's exe);
  against Age of AI 16-5-11 (boom's default, Gbase2: 16-7-8). P1 (boom_order with iron_push, boom-defence's exe):
  Stone 20.0, Copper 46.5, Bronze 70.8 (28/32), Dark 86.1 (22) against Age of AI, the Bronze save unpaid 27.6 minutes
  a game.

Still open: the owner's citizen growth (3.4 a minute against 1.6-2.1), gold and iron income (about half the
requirement), the Prehistoric's exact counts (one over or one short on the mines in most games).

**The gaps against the owner's spec (2026-11-12): the dock's wood and the granaries' fields.** G5 (as merged, 93c4ca7)
confirmed on 64 seeds (2-65) against Rel4: against the improved classic won-lost-close 28-16-20 against 20-29-15,
Copper in 56 games against 47, Bronze 87.1 in 44 against 79.6 in 37, 11 defeats against 9; against Age of AI 39-6-19
against 31-14-19, Bronze 66.3 (61) against 67.8 (60). The diagnosis of the four gaps, in the owner's order:
- **Gold and iron income** (31/38 a minute at minutes 30-45 against 81/74) is the gatherer count, not the rate: 3-6
  gold and 6 iron gatherers at minutes 30-35 against the owner's 12-18, at 4.7-5.3 a gatherer (6.1 wanted; the
  engine's pace, §8.50). The fill reaches 12/12 only at about 58 citizens, so it waits on the growth.
- **The Stone Age's growth** (1.8-2.3 a minute against 3.4): food stock about 20 and the town centres idle 26-67 s a
  minute in stone_start/grow, land food fixed at 18, so the boats carry the growth.
- **The dock** (10 boats by +7 missed in 25 of 63 games against the classic): not refused. The fish verb queued it
  in the Stone Age's first minute (afford=0), and it waited 12 minutes in the build queue while the Stone settlements
  (cap 14) took the wood each time it reached about 100 (seed 2: one patch's settlement placed three times).
  `BO.OPT.boat_wood` (the stone_start settlements wait for 10 boats) fixes it: H1.
- **The granaries** (3 within 10 minutes of Copper missed in 20 of 56): no script orders the farms. A standing
  granary lays its 8 fields as free foundations (EEComplexUnit::SetBuilt -> ReplantFarm, a game rule), and the engine's
  food runner sends idle food gatherers to build them, each paying a farm's price: Copper's wood went into the first
  granary's fields while granaries 2 and 3 waited queued (afford=0). More wood (W1: Copper's first wood step 15 -> 22)
  did not help (+2/+12/+9 minutes on 3 seeds against +6/+11/+11): the fields took it. The decision stays in Lua: the
  food `gather` order's `granary_farms = n` (AI_LUA_UNITS) stops the runner at n built farms a standing granary (0
  none). `BO.OPT.lua_farms` holds the fields (0) until the plan's 3 granaries stand, then 8; lib.place sites each
  granary (`granary_spot`: room for all 8 fields, clear of the other granaries' rings, standing or planned), one job at
  a time, ordered once its cost is in hand (a cost refusal waits 20 s, not 5 minutes). The C++ build verb had put a
  third granary on the second's laid field (build.stalled, the granary lost). Other field paths checked: the classic's
  building counts do not run for a next seat, RepairBuildings skips farms, a farm exhausted is only laid again (built
  by its farmer, after the hold), and boom orders no farms under the option. Smoke variants on seeds 12/13/16: L1
  (Lua orders the farms: a farm ordered where no field was laid waits forever), L2 (fields allowed once all are
  placed: +11/+13/+11), L3-L6 (the hold until they stand; the C++ verb's sites, then the reserve and the 5-minute cost
  wait) led to L7 (+4/+7/+3).
- **H2** (stone_start/grow land food 18 -> 21) raised growth against the classic (1.9 -> 2.2) but took the mines'
  citizens (29/37 and 53/61 a minute), Bronze 90.4 against 78.0 on seeds 2-33: dropped.

64 seeds (2-65; H1's first half on the old exe, identical game samples on the new one), G5 -> H1 -> L7 (L7 = H1 +
lua_farms):

| | G5 | H1 | L7 |
|---|---|---|---|
| classic: won-lost-close, defeated | 28-16-20, 11 | 29-21-14, 8 | 25-20-19, 9 |
| classic: Copper (games), Bronze (games) | 51.7 (56), 87.1 (44) | 51.6 (54), 83.4 (42) | 51.6 (54), 78.3 (42) |
| classic: docks / 10 boats by +7 missed | 9 / 25 | 4 / 21 | 4 / 21 |
| classic: 3 granaries by Copper +10 missed (median) | 20 of 56 (+9) | 21 of 54 (+10) | 6 of 54 (+3) |
| Age of AI: won-lost-close | 39-6-19 | 34-6-24 | 32-7-25 |
| Age of AI: Copper, Bronze (games) | 45.0, 66.3 (61) | 45.3, 65.1 (60) | 45.3, 64.4 (59) |
| Age of AI: 3 granaries missed (median) | 17 of 63 (+8) | 19 of 63 (+7) | 1 of 63 (+2) |

Both are on in boom_order. The boats' remaining misses are the pace (a dock at +1 trains its tenth boat at +6 to +7),
and the growth and the mines' income stay open.

**The Stone Age's growth (2026-11-13).** The town centres wait on food, not on a cap: in stone_start and stone_grow
they stood idle 26-57 s a minute with 21-47 food in stock, at the 76 cap 0-20% of the minutes (the population cap is
400). The spending trace (6 seeds against the improved classic): stone_start took in 131 food a minute and spent 91
on citizens, 38 on soldiers; stone_grow 162, 136 and 17. A boat brings 5.4-7.0 food a minute, a forager 6.9-7.5, a
hunter 5.1-7.3 (least squares over the Stone minutes; hunters average 1.3-2.2, so the hunting reach has little to
give). The 32-seed variants, against L7:
- **B1** (20 boats and 2 docks from stone_grow): growth 1.9 -> 2.1 and 2.2 -> 2.5, citizens at 40 minutes +7/+9, but
  Bronze later (86.9 in 19 against 82.9 in 22 against the classic), the second barracks and range short (the boats'
  wood), one more defeat: dropped.
- **S1** (`BO.OPT.soldiers_after_boats`: no new soldier in stone_start before 10 boats unless threatened or enemy
  soldiers are in sight): kept, on in boom_order. Against the classic the threat is usually there at the advance, so
  it acts mostly against Age of AI (stone_start's town centres idle 8 s a minute against 26).
- **F1** (`settle_afford`: a settlement only with its whole cost in hand, a cost refusal retried in 20 s, not 5
  minutes): Bronze against the classic in 16 games against 22: off.
- **B5** (`builder_pool` with `gather(exact = 2)`, `blocked_skip`, F1 and S1): the owner's ranges hold (no spill
  onto wood: the fill keeps its leftover out, the C++ idle runner sends no idle citizen to a resource that has its
  count, the claim reaches out for a short mine; the Stone citizens are the claimed places plus lib.place's builders,
  at most 86, none while 4 stand idle), but the town stops short: citizens at 40 minutes 61.7 against 70.1, Bronze
  91.2 against 82.9, 7 defeats against 5 (against the classic). The citizens held back were the ones on wood or food
  past the ranges, while a second mine waits for its settlement. Off; the end state once the mines can take them.
- **Builders against the fill:** 76 counts every land citizen; in stone_grow and stone_developed 4-12 (with boats)
  gathered nothing, 2-8 of them idle.

64 seeds (2-65), L7 -> S1: against the improved classic won-lost-close 25-20-19 -> 29-21-14, defeated 9 -> 5, Copper
51.6 (54) -> 51.3 (53), Bronze 78.3 (42) -> 77.5 (39), growth 1.9 -> 2.0; against Age of AI 32-7-25 -> 36-6-22,
Bronze 64.4 (59) -> 65.6 (61), no defeats.

**What delays the second and third mines** (3 games against Age of AI, the sites trace): the near-home gold and iron
mines (12-16 tiles, explored) are claimed only from stone_grow (stone_start's fill wants 2 of each, so the claim for
the phase target waits), about 5 minutes into the Stone Age, and their settlements stand 5-15 minutes after that:
boat_wood holds every Stone settlement until 10 boats, the settlement's stone runs short (cannot_afford, then
lib.place's 5-minute wait), and a wild animal on the tile had the same spot re-placed each minute (`blocked_skip`).
Next: claim and settle them from the advance, near home only, then the pool and exact = 2 on top.

### 8.53 The classic computer player in Lua: early raids, hills and choke points, heroes (2026-11-11, EE-SIM)

**Early raids are level.** Over seeds 2-17 (balanced, 25 minutes) the port lost 36 citizens and the C++ 41 (22 of the
C++'s with an attack goal: its militia's). Seed 24's collapse (§8.52) is not the common case; the port's citizen
losses come late (54% after 60 minutes, §8.47).

**The defence ladder's hills and choke points**, through two generic queries (AI_LUA_UNITS §11):
`ai.hill_point(x, y, r, pick, seed)` (a hill whose hull comes within r tiles, 35% of the time after the forest) and
`ai.choke_point(x, y, nearest, pick, seed)` (a choke point of the land, by guard_chokepoint_percentage: 68% in the
data). The port's defence tasks over 32 games: forest 203, town 158, choke 20, hill 9 (the sea's 81 apart).

**The heroes of AllocateSpecialUnits** (hero_chance 2%, one per group, a share of the limit; the game's hero limit is
not read): none was trained in the 32 games (no hero class in the balanced games' epochs and chances).

**Walls:** not ported; the C++ built none in 90-minute games (§8.42).

Bench (balanced, 32 seeds, 90 minutes, --jobs 2; before = §8.52's cs):

| stage | Lua won-lost | Lua Copper | Lua citizens lost by 20 / all | C++ citizens lost |
|---|---|---|---|---|
| §8.52 (cs) | 10-18, 4 close | 22/32 | 2.1 / 56.6 | 36.6 |
| hills, choke points (hc) | 9-19, 4 close | 22/32 | 2.0 / 57.5 | 37.9 |
| heroes (he, 2 lobby faults re-run) | 7-20, 5 close | 22/32 | 2.1 / 58.5 | 33.0 |

Within noise. Determinism: seeds 2 and 5, 60 minutes, --repeat 2: identical. 0 script errors.

Next: the balanced late game - the port's citizens lost to the C++'s soldiers after 60 minutes and its Copper Age
in 22 games against 27.

### 8.54 The classic computer player in Lua: the defence ladder against the C++'s own (2026-11-11, EE-SIM)

§8.53's port chose choke 20 and hill 9 of about 390 tasks; the data's guard_chokepoint_percentage is 68%. A trace-only
`dc.ladder` line on the C++'s AssignSubGroupTask (which step gave the task, and the sub-group's common move flags)
shows the C++'s own distribution on the same games. Seeds 2-9 (balanced, 90 minutes):

| step | the C++ | the port before | the port, forest gated | the port, re-tasked after fights (32 games) | the C++ (32 games) |
|---|---|---|---|---|---|
| choke | 31 | 8 | 12 | 76 | 166 |
| hill | 25 | 4 | 14 | 63 | 108 |
| forest | 12 | 33 | 2 | 20 | 86 |
| town | 15 | 4 | 2 | 16 | 51 |
| land | 3 | - | - | - | 23 |
| adjacent | 1 | - | - | - | 13 |

Two gaps, both closed:
- **The forest step's gate.** The C++ takes the forest only when every unit of the sub-group crosses forest
  (UpdateCommonMoveFlags over EEUnitClass::GetTerrainFlags); most of its groups mix units that cannot (flags 00), so
  the forest is rare and the hill and choke steps come next. The port took every soldier group for foot soldiers. The
  own unit records now have `crosses_forest` / `crosses_cliff` (AI_LUA_UNITS §11). (The C++'s forest tasks with flags
  11 are mostly empty sub-groups: an empty group passes every test.)
- **The re-tasking.** A C++ land sub-group whose fight is over goes idle and is given a task again (EECPASGLand: Attack
  -> Idle -> AssignTask); the port's stayed at its place. The port's ladder now runs 175 times in 32 games against the
  C++'s 449 (from 137); the rest is the C++'s other idle paths (a task no longer valid, the empty sub-groups).

Bench (balanced, 32 seeds; before = §8.53's he):

| stage | Lua won-lost | Lua Copper | the C++'s Copper | Lua citizens lost by 20 / all |
|---|---|---|---|---|
| §8.53 (he) | 7-20, 5 close | 22/32 | 27/32 | 2.1 / 58.5 |
| forest gated (tf, 2 lobby faults) | 9-16, 5 close | 21/30 | 21/30 | 2.3 / 60.0 |
| re-tasked after fights (rt) | 10-18, 4 close | 23/32 | 22/32 | 2.2 / 57.5 |

The port now reaches Copper as often as the C++ (23 against 22). Determinism: seeds 2 and 5, 60 minutes, --repeat 2:
identical. 0 script errors.

Next: the C++'s other re-task paths (an invalid task, the empty sub-groups' tasks), then the late balanced game - the
citizens lost to the C++'s soldiers after 60 minutes.

### 8.55 The classic computer player in Lua: the task checks, the docks, the C++'s uncounted boats (2026-11-11, EE-SIM)

**The task check (IsLocationTaskValid).** A C++ defence sub-group's task is looked at only once the group is at its
place and its next check time is past; then it is dropped half the time, else looked at again in 10-120 s. A new task
does not reset the check time, so a group that arrives late is looked at on arrival. The port looked at the task on a
timer that every new task reset. With the C++'s rule, the port's ladder runs 210 times in 32 games against the C++'s
386 (it ran 175). The remaining gap is mostly the C++'s empty sub-groups, which take a task like any other group and
pass every test (§8.54); they are not ported because the port makes no empty groups.

**The late game.** On 8 seeds, the port's citizens stopped growing after 40 minutes (70 at 89 minutes against the
C++'s 145), and from 60 minutes it lost 381 citizens against the C++'s 221. Two causes, both in the food supply:
- **No dock in 3 of 8 seeds.** A dock's water site lies on the shallows, which form a land mass of their own, so
  `ai.reach` from a citizen to the site failed and FindBuilder found no builder. The port also asked at only the
  first 8 placeable sites. The C++'s FindBuilder lets a citizen of another path area build a site class (dock, navy
  yard, the space classes) or a tower when its continent's shore takes in a tile of the footprint. The port now
  accepts a citizen that reaches a land mass by the footprint (its tiles and the ring round it, found with
  `ai.shore` and max 0), and asks at up to 48 placeable sites.
- **The C++ never counts its boats.** EECPRMRequestBoats counts with EECPUGatherSpecialUnitsCount built with
  inByClass false, which walks no units: only the boats requested or being made count. So the C++ asks for one more
  boat every 30 s while food lacks gatherers and fewer than 8 boats stand idle. It asked 13-47 times a game, against
  the 8 its wanted count names. The port counted the boats it had; now it counts as the C++ does.

Bench (balanced, 32 seeds; rt = §8.54's last stage):

| stage | Lua won-lost | Lua Copper / Bronze | the C++'s Copper / Bronze | citizens at 89 min, Lua / C++ | Lua boats / C++ fish gatherers at 70 min |
|---|---|---|---|---|---|
| rt (§8.54) | 10-18, 4 close | 23 / - | 22 / - | - | - |
| task check (lv) | 10-16, 6 close | 22 / - | 23 / - | - | - |
| docks (dk) | 11-15, 6 close | 24 / 11 | 22 / 4 | 93 / 116 | 9.1 / 15.2 |
| uncounted boats (bt) | 10-15, 7 close | 23 / 8 | 21 / 4 | 99 / 117 | 11.1 / 14.2 |

The citizen gap after 60 minutes narrowed from about half to 85%. The port still loses more citizens after
60 minutes (bt: 1142 against 729 in 32 games; at wood 353, fighting 278, hunting 112). It also loses far more
fishing boats: 427 against the C++'s 123. Determinism: seeds 2 and 5, 60 minutes, --repeat 2. 0 script errors.

Next: the port's boats lost to warships (its naval guard against the C++'s), then the woodcutters lost after
60 minutes.

### 8.56 The fishing boats counted, the boats that fled, the woodcutters the port deletes (2026-11-11, EE-SIM)

**The boat count (C++ and Lua).** §8.55 found the coast-fishing cap was never applied. EECPRMRequestBoats (our coast
fishing, 2026-11-07) and the original's RequestFishingBoats (0x49487b, the island rule) both build
EECPUGatherSpecialUnitsCount with inByClass false. That flag walks no units, so mUnits stays 0. The 8/16 cap
(kCoastBoats, kCoastBoatsLater) never applied, and the island rule's "unless the player already has one" never saw
its boats either. Both now count by class, and the port counts the same way. The C++ classic, balanced, against
the port (32 seeds; b0 before, b1 after):

| | before | after |
|---|---|---|
| C++ boat requests a game (mean, max) | 23.5, 55 | 14.0, 25 |
| C++ fish gatherers at 40 / 70 min | 3.7 / 14.2 | 3.7 / 10.4 |
| C++ food income at 40 / 70 min (a minute) | 135 / 160 | 135 / 145 |
| C++ food stock at 40 / 70 min | 244 / 625 | 244 / 680 |
| C++ citizens at 60 / 89 min | 79 / 117 | 77 / 103 |
| C++ Copper / Bronze | 21 / 4 | 21 / 5 |
| wins: C++ - Lua, close | 15-10, 7 | 15-13, 4 |
| Lua boats at 40 / 70 min | 2.1 / 11.1 | 2.1 / 8.1 |

Nothing changes before 40 minutes, and the epochs and the C++'s wins stay where they were. Later the C++ has about
12% fewer citizens and 9% less food income. The cap that binds is the forage patches' 16. A cap of 32 would match
the old boats in practice: 14.5 at 70 minutes against the old 14.7 (16 gives 11.0, 24 gives 13.4). This changes the
opponent of every bench against the C++ classic; benches from before need a new baseline.

The owner chose the 32-boat cap (kCoastBoatsLater, the forage patches' cap; kCoastBoats stays 8), in the C++ and
in the port, to keep the improved classic's strength. The bench (c32, 32 seeds) does not bring the boats all the
way back: the C++'s fish gatherers at 70 minutes are 11.3 (16: 10.4, uncounted: 14.2), with 15.5 boat requests a
game (max 32). The cap now binds in one seed only. The limit is the wanted count itself: two boats a fish within
30 tiles of the town (kCoastBoatsPerFish), which the uncounted cap had ignored. The rest of the bench: C++ food
income at 70 minutes 135 a minute, citizens at 89 minutes 102, Copper 21, wins C++ 14 - Lua 13, 5 close. The
remaining gap is in the per-fish count, not the cap. (The rules document's comment on forage_patches still says
"16 boats"; changing it would mean a new db release.)

**The boats that fled.** 186 of the port's 230 lost boats died idle. The engine's naval flight (EECPRMNavalFlee)
calls AddIdleGatherer on the C++'s own lists, which are empty for a Lua seat, so a fled boat stayed on the script's
food list. It sat idle at the far dock and was never sent to fish again. The port now moves a boat that is off its
fish back to its idle list. AssignIdleBoats takes every boat of that list, as the classic does. Boats lost: 170
against the C++'s 153 (from 230 against 113). Balanced 32 seeds: 14-14, 4 close.

**The woodcutters lost after 60 minutes.** 422 of the port's 1310 citizens lost after 60 minutes have no killer
(the C++'s: 801 in all). They die exactly 30 s apart, near the town, with no enemy near. These are the port's own
KillUnitsForMilitary: 407 citizens were deleted for the surplus over ceil(pop limit x 0.45) = 180. Wood scores
highest, so the woodcutters go first. The port trains past 180 because its wanted citizens rise to 199-278:
GetWantedCitizens' floor, the gather capacity up to max_citizen_share. The C++'s stays at 180 in every trace line
after 60 minutes. The port's capacity counts every centre as active (24 of 24 in seed 13) and its mineral sites
stay at 7/10/10. The C++'s centres' sites run down (3/0/2/4/4). Without the deletions the late losses are 903
against 801.

Determinism: seeds 2 and 5, 60 minutes, --repeat 2, identical apart from the timing lines. 0 script errors. The
builds link 0/0 (EE-SIM and the test profile), and the eedata check passes.

Next: the port's centres' activity and sites against the C++'s (GetGatherCapacity), so its wanted citizens stay at
the C++'s and it stops training citizens it then deletes.

### 8.57 The classic computer player in Lua: the untapped sites, the citizens it no longer deletes (2026-11-11, EE-SIM)

§8.56 found the port deleting about 400 citizens a bench, mostly woodcutters (KillUnitsForMilitary's surplus over
ceil(pop limit x 0.45) = 180). Its wanted citizens rose to 199-278, while the C++'s stayed at 180. The cause is not
the centres' activity: both mark every centre within the claim range active. It is the sites. A trace of the C++'s
centre units (seed 2, the gold) shows that a resource's hit points stand at 1 until anyone first gathers from it,
then fall to 0. The gold mines hold 300000, so they never run out. GetGatherCapacity, and the `sites` trace, count a
centre's units with mHitPoints > 0. So the C++ counts only the untapped sites, which is why its counts fall over the
game (gold 6 -> 2). Its capacity then stays under the 0.45 floor, and its wanted citizens stay at 180. The port
counted every mine, so its counts never fell (3/6/4 stone, gold and iron for 60 minutes). Resource records now have
`untapped` (AI_LUA_UNITS §11), and the port counts as the C++ does.

Bench (balanced, 32 seeds; c32 = §8.56's last stage):

| stage | Lua won-lost | Lua citizens lost (all) | the C++'s | citizens deleted | Lua Copper / the C++'s |
|---|---|---|---|---|---|
| c32 | 13-14, 5 close | 72.2 | 36.9 | about 400 | 26 / 21 |
| untapped (ut) | 12-13, 7 close | 47.0 | 32.0 | 43 | 22 / 25 |

After 60 minutes the port's wanted citizens are 180 in every line, as the C++'s. The port's citizens lost after 60
minutes: 786, of which 66 have no killer, against the C++'s 597. Citizens at 89 minutes: 97 against 109.
Determinism: seeds 2 and 5, 60 minutes, --repeat 2. 0 script errors.

### 8.58 The classic computer player in Lua: the untapped sites on new seeds, the late losses (2026-11-11, EE-SIM)

§8.57's untapped rule held on seeds 2-33 (12-13), but on new seeds (34-65) the port fell to 5-20, with Copper 18
against the C++'s 27. Without the rule, the same seeds gave 12-17 with Copper 25 against 21. The citizens at 40 minutes fell
from 47 to 38. The cause is the per-resource room. The port's centres hold the explored resources only (fog), so at
4 minutes they hold 3/1/3/3 food, stone, gold and iron sites against the C++'s whole-map 7/7/9/9. Counting only the
untapped ones on top of that left the demand split short of gold and food (gold wanted 2.0 against the C++'s 3.8 at
10 minutes). Two remedies were tried:
- **Every resource for the sees_all seat** (ai.resources and ai.unit through the fog, as the classic's grouping of
  the whole map): 5-25. The port sent its gatherers to far resources. Reverted.
- **The untapped rule for the citizen target only** (GetWantedCitizens), with every site counted for the per-resource
  room (a DEVIATION, in ORIGIN and econ.lua): kept.

| seeds | stage | Lua won-lost | Lua / C++ Copper | citizens deleted | Lua / C++ citizens lost after 60 min |
|---|---|---|---|---|---|
| 34-65 | before §8.57 | 12-17, 3 close | 25 / 21 | - | - |
| 34-65 | §8.57 (untapped everywhere) | 5-20, 7 close | 18 / 27 | - | 1155 / 576 |
| 34-65 | every resource (sees_all) | 5-25, 2 close | 17 / 27 | 2 | 1225 / 510 |
| 34-65 | untapped for the target only | 12-17, 3 close | 25 / 22 | 50 | 1138 / 773 |
| 2-33 | untapped for the target only | 13-14, 5 close | 26 / 21 | 33 | 1128 / 662 |

The Copper swing §8.57 saw (Lua 22 / C++ 25) was this regression: the port is back at 25-26 against 21-22.

**The late losses.** After 60 minutes the port still loses about 1.6 times the C++'s citizens (1128-1138 against
662-773 in 32 games). It loses 649 while fighting (the militia, with no guard near), 375 at gold, 304 at wood, 234
hunting and 192 at iron. Most die with fewer than 3 enemies near (1520 of 2266), so these are small raids rather than
the attack groups. Next: the port's militia and its flight from small raids against the C++'s (EECPRMNoteDanger,
the militia odds).

Determinism: seeds 2 and 5, 60 minutes, --repeat 2. 0 script errors.
**The militia against the C++'s (seeds 2-17, balanced, 90 minutes).** The port now traces its militia decisions in
the C++'s format (`classic_lua militia fight|run ...`; trace only):

| | the C++ | the port |
|---|---|---|
| militia decisions (hits answered) | 1307 | 1916 |
| fight / run | 1132 / 175 | 1739 / 177 |
| citizens sent per fight | 4.9 | 2.3 |
| soldiers per fight | 0.3 | 1.9 |
| raiders per fight | 2.5 | 1.8 |
| citizens dying in a fight (all game) | 282 | 211 |

The militia is not the gap: the port's fights cost fewer citizens than the C++'s, and it has more soldiers at hand.
The gap is the citizens killed while gathering. Over all game time in these 16 games, the port lost 932 citizens
against the C++'s 576. The C++'s die fighting (282) or moving (180), and only 86 while gathering. The port's die
hunting (162), at gold (147), at wood (93), foraging (70), at stone (64) and at iron (56). 548 of those 721 die
within 20 s of their first hit, and 362 with no soldier of their own near. The port is also hit half as often again
(1916 militia calls against 1307): the C++'s raids find it more. Next: what moves the C++'s hit citizens (their
`move` deaths), and where the port's raided gatherers stand against the C++'s (distance from the town and drop
sites).

**What moves the C++'s hit citizens.** The engine's unit AI (EEUnitAI: run from or retaliate against an attacker)
treats both seats alike. Its run trigger needs a goal that is not player-initiated, and both the classic and
`ai.gather` give gather goals that are. The C++'s `move` deaths are its own location goals: the militia's run, the
naval flight (land units under a warship's fire go to the town centre), and EECPRMFleeRaids, which moves the
gatherers out of the danger zones. The C++ fled 774 times in 16 games. Its zones come from the militia's runs and
from every naval flight. A Lua seat's naval flight is still the engine's: it moves the port's units, but it puts its
zone on the classic's own lists, which the port's gatherers are not on. Giving the port that zone (a zone at the
victim when a warship hits a citizen or a boat, once every 4 s) did not help, and was not kept. Balanced 32 seeds:
12-15, 5 close (13-14 before). Citizens lost: 64.8 (60.8 before), 1267 after 60 minutes (1128 before), and 161
killed by ships.

**Where the port's raided gatherers stand.** Seeds 2-17, the citizens killed by an enemy:

| tiles from the town | < 10 | 10-20 | 20-30 | 30-45 | 45+ |
|---|---|---|---|---|---|
| the C++ (502) | 107 | 198 | 134 | 51 | 12 |
| the port (789) | 114 | 274 | 148 | 185 | 68 |

The extra losses are mostly far out. 253 of the port's die 30 or more tiles from the town, against the C++'s 63.
The port's hunters die at a median of 35 tiles (137; 40 to enemy Prehistoric towers), its gold miners at 27 and its
stone miners at 29. The C++'s die near home: those killed fighting (which include its hunters) at a median of 15
tiles, those moving at 20. The port also loses 55 citizens to ships against the C++'s 7.
Next: the port's far gathering against the C++'s: the game it hunts, which lies beyond the explored land the C++
sees whole (fog); the mines it takes; and the enemy towers near both.

### 8.59 The classic computer player in Lua: the raids' flight (EECPRMFleeRaids), the gatherers killed by towers (2026-11-12, EE-SIM)

**The flight.** The port has EECPRMFleeRaids: the gatherers within 8 tiles of a danger zone (30 s) go home. Traced on
seeds 2-17, the C++'s flight fired 774 times (2790 units) and the port's only 37 times (45 units). The C++'s zones come
from 175 militia runs and from its naval flights: 101 for land units and 558 for boats, each zone made only where a
unit left. For the port, the engine runs the naval flight and moves the units, but it puts the zone on the classic's
own lists. The port now makes that zone itself: a hit on an own unit by a warship, once every 4 s, makes a zone if
any own unit within 12 tiles leaves (the boats round a boat; the citizens and melee soldiers round a land unit).
This is EECPRMNavalFlee's rule. §8.58's first try made a zone for every hit on a citizen or a boat.

| (32 seeds) | the C++ | the port before | the port now |
|---|---|---|---|
| flight fires (units) | 498 (2530) | about 74 | 705 (3021) |
| won-lost | | 13-14, 5 close | 12-14, 6 close |
| Lua citizens lost | | 60.8 | 61.8 |
| citizens killed by ships | 14 | about 110 | 116 |

The flight now fires about as often as the C++'s: more, because the port is hit more. It changes the outcome very
little. It is kept for fidelity.

**The towers.** The port's citizens killed by enemy towers: 145 in 32 games, against the C++'s 40. They die at gold
(52) and hunting (65). The C++'s rules for hunt and mine sites are the port's: FindMines takes game within 2 (near) or
6 (far) drop-off distances of the nearest town centre or settlement, and skips a site with HasEnemyNear. HasEnemyNear
is the classic's own test through sees_all: every armed unit in the influence box. It leaves out units whose target
search has been idle for more than 60 s, and so a quiet tower too. The difference is what the port can see. Its
centres and game are the explored ones, so it finds fewer near sites and takes the far ones. Giving it every resource
through the fog was tried in §8.58 and lost badly (5-25). Next: the hunting and mining sites against the enemy's
towers (EECPNextSiteThreat's remembered towers, which the fog-honest test has), and the game the port can see near
home.

Determinism: seeds 2 and 5, 60 minutes, --repeat 2. 0 script errors.

### 8.60 The classic computer player in Lua: the remembered towers on hunt and mine sites (tried, not kept) (2026-11-12, EE-SIM)

§8.59 proposed a second check on hunt and mine sites: an enemy tower the seat remembers (EECPNextSiteThreat's tower
test, offered as `ai.enemy_near(x, y, {towers = true})`) after HasEnemyNear. The bench (balanced, 32 seeds) came out
identical to §8.59's: 12-14, 6 close, citizens lost 61.8, and 145 citizens killed by towers against the C++'s 40.
A traced game (seed 2, 60 minutes) shows why. The finders called the check about 350 times, and it never found a
tower. The tower kills do not come from the sites the finders choose:
- most hunters die well into their work (112 of 145 more than 2 s after their first hit);
- they die far out (a median of 38 tiles), in bursts at one spot (seed 10: 57 kills, most of them in minutes 60-66
  at one herd 46 tiles out);
- the engine carries a hunter on to the next animal of its herd, under the tower, with no new order from the script.

The check and its binding were reverted. Next: what the C++ does with a hunter shot by a tower. Its militia ignores
towers (its raiders are soldiers), so the answer is likely its herds. A tower that kills hunters could make a danger
zone in the port, or end its hunt at that herd: a deviation, to be weighed against the C++'s own losses there.

### 8.61 The classic computer player in Lua: the herds given up (2026-11-12, EE-SIM)

**The C++'s hunters** (seeds 2-33, balanced). The C++ hunts little: 2.3 hunters at 30 minutes and 1.7 at 60 (its `cp
food` lines). Its citizens killed by enemy towers are 41: 23 gathering, 12 moving and only 2 hunting, at a median 28
tiles from its town. Its deaths while attacking (its hunters and its militia) lie at a median 15 tiles. Animals kill
16 of its citizens and 46 of the port's. The port's hunters die at a median 25 tiles (235 in 32 games). Its far herds
are the fog's: it sees fewer herds near home and goes on to the far ones it has explored (§8.59, §8.60).

**The herds given up.** This is a DEVIATION (fog), and sensible play: a hunter hit by an enemy tower that dies within
5 s gives that herd up. For 10 minutes no game within 12 tiles is hunted (FindMines skips it), and the hunters there
leave it for the town (`militia.lua` and `resource.lua` herd_banned; ORIGIN).

| (32 seeds) | before (§8.59) | herds given up |
|---|---|---|
| won-lost | 12-14, 6 close | 12-13, 7 close |
| Lua citizens lost | 61.8 | 58.6 |
| killed by enemy towers (of them hunting) | 145 (65) | 123 (30) |
| the C++'s killed by towers | 40 | 41 |
| Lua Copper / the C++'s | 23 / 21 | 24 / 21 |

There are 5 bans a bench. Each one ends a stream of hunters going to one herd. Kept. Determinism: seeds 2 and 5,
60 minutes, --repeat 2. 0 script errors.

### 8.62 The classic computer player in Lua: where it stands (2026-11-12)

The port (`ai/classic_lua`, manifest `classic_lua`) plays the C++ classic's decisions through the per-unit Lua
interface. It reads the same data and personality, and keeps the C++'s thresholds and timings. Sections §8.39-§8.61
record each stage. `ai/classic_lua/ORIGIN` maps every C++ file to its module, with what is ported, what stays in the
engine and what is not started.

**Parity.**
- **balanced, no cheats** (the bench since §8.49; 90 minutes, Small, Hard, 2 AIs, the port against the C++):
  - seeds 2-33: 12-13 with 7 close (§8.61);
  - seeds 34-65: 12-17 with 3 close (§8.58);
  - by our rule of thumb a 32-seed split this close is within noise (bench-noise note: decide on 32, confirm on 64).
  - The port reaches Copper about as often (24 and 25 of 32, against the C++'s 21 and 22) and Bronze more often
    (10-12 against 3-5).
  - It loses more citizens: 58.6 a game against the C++'s 32.6. The gap is after 60 minutes, in raids on its
    gatherers far from home (§8.58-§8.61). Its fights and its militia cost no more than the C++'s.
- **classic with the cheats** (classicX: the classic personality, Hard's seven cheats to both classic seats; seeds
  2-33, 90 minutes, on main 902a01c's exe), re-checked 2026-11-12: **2-1 with 29 close**, against §8.49's 4-0 with 28
  close. Parity holds.
  - Both sides reach the Stone Age in every game (the port at a median 45 minutes, the C++ at 48).
  - Both stand at the popband's 50 citizens from 20 minutes.
  - The port's army is larger (17.6 against 8.2 at 30 minutes), as at §8.49. The port loses more citizens (101.3
    against 25.2), also as at §8.49 (101.1 against 24.5).
  - 0 script errors. (The cheats must be set with `--set "gameoptions.difficulties.hard.ai_cheats=[cost, reveal,
    epochs, extra, popband, catchup, retire]"`: Hard keeps none in the data now. Without it, `--cheats all` turns
    nothing on.)

**The cheat capabilities** (manifest entries in ai/ais.json, declared only by classic_lua, shown in its trace start
line, AI_LUA_UNITS §11):
- `sees_all`, the classic's information cheat. With it, `ai.enemy_near` is HasEnemyNear (every armed unit, seen or
  not), `ai.enemy_force` gives the militia's odds, and `ai.player_file` gives the C++'s intelligence files.
- `fills_garrison`, the classic's garrison cheat (`ai.garrison_fill`, FillGarrison).
- The lobby's cheats (cost, reveal, extra, popband, epochs, retire) act in the engine as for a classic seat. The
  port runs catchup itself (ORIGIN).

**The documented deviations** (ORIGIN has each in place). They come from four causes.
- **The fog.** The port sees what is explored, where the C++ reads the whole map:
  - the resource centres, regrouped every 15 s;
  - the forest finder (the explored tree nearest the town);
  - the dock sites (`ai.water_sites`, explored);
  - the enemy armies, outside `ai.player_file` (those seen, counts as remembered);
  - the attack's "largest" enemy (the nearest known building) and no "weakest";
  - the humans' epochs (those the view's enemies carry);
  - an idle citizen explores until it knows a resource;
  - the untapped sites count for the citizen target only, while every site counts for the per-resource room (§8.58);
  - the herds given up under enemy towers (§8.61).
- **The interface.**
  - Placement goes through `ai.find_site` with the one-tile margin, without CanBuildAt's choke, hill, gate and
    walkable-ground rules.
  - FindBuilder's shore rule for docks and towers uses `ai.shore`.
  - The gatherers' room is the gatherers sent (`workers`).
  - FindResourceTarget has no area hull test.
  - The morale effect's fill is approximated (2 houses a town centre, 4 a capitol).
  - The production counts come from queue lengths.
  - The random draws are the script's stream.
  - A missing SOL building is logged (`sol none`), since a script cannot add to the next epoch's count.
  - ChooseFavoriteLandType draws among the trainable soldier classes.
- **The army.**
  - A sub-group does not split or merge.
  - Targets are what lib.groups picks within 12 tiles.
  - A town is a 16-tile cluster of the own buildings with its bounding box as hull.
  - A defence task reaches 12 tiles (18 at a threat point).
  - The target file's counts stand in for the C++'s files where `sees_all` does not reach.
- **The engine's lists.** The engine's reflexes act on the C++'s own lists, so the port ports their effects:
  - the militia and the danger zones (militia.lua);
  - the naval flight's danger zone (§8.59);
  - a fled boat's return to the idle list (§8.56).

**C++ fixes found by the port** (both sides changed): the boat count (EECPUGatherSpecialUnitsCount's `inByClass`,
§8.56), in our coast fishing and in the original's RequestFishingBoats; and the forage patches' boat cap of 32.

**What is left.**
- **Not started (ORIGIN):**
  - the walls (EECPWallManager);
  - the transports and ferries, and the island maps' landings;
  - air, space and submarine groups, carriers and paratroopers;
  - the wonder race;
  - the ICBM;
  - AidAllies, CheckRaids / Retaliate, BuildUp, the adjacent continents' step of the defence ladder;
  - BalanceProduction, ChooseTransportClass;
  - the robotic farm;
  - the island map's hired garrison citizens;
  - the chat taunts (no chat command).

  The Small balanced and classic benches never need these: they end before the Middle Ages, on land maps. Islands,
  late epochs and multi-ally games do.
- **Open gaps in play:**
  - the late raids on far gatherers (§8.58-§8.61: 1111 citizens lost after 60 minutes against the C++'s 599);
  - the empty sub-groups' tasks (§8.55).


### 8.63 Porting everything: the inventory, and the C++ classic on islands (2026-11-12, EE-SIM)

The owner's direction: port all of the C++ classic into Lua, so that its decision code can be deleted. The order:
1. the inventory (below);
2. the missing parts by value: transports and island play (2a), walls (2b), air, space and submarines (2c), the
   wonder race and the ICBM (2d), allies (2e), the rest (2f);
3. benches beyond Small land maps;
4. classic_lua as the lobby's classic for a soak;
5. then the deletion, keeping the helpers the Lua calls (EEComputerPlayer's engine glue split out).

**The inventory.**
- **Decision code: about 47,900 lines.**
  - Managers and commanders: 34,374. These are EEComputerPlayer, the civilization, unit, build, research and
    resource managers, the resource centres, the walls, the combat manager and its strategy, the attack
    commander/groups/sub-groups and formations, the unit selector, the personnel, the intelligence, the defence
    commander and aid packages, the exploration, the player interaction and EEAIFunctionObjects.
  - Sub-group and unit state machines (ASG*, ASGU*): 13,481.
- **What stays:** the Lua engine and its helpers (12,706) and the engine proper (pathfinding, continents, influence,
  towns, herds: about 15,800).
- **classic_lua today:** 7,324 lines, with lib.groups for the army's movement and fighting.
- **Not started:**
  - the walls;
  - the transports and ferries (RequestFerry / FerryGatherers / TransportIdle, ChooseTransportClass, the sea
    transport sub-groups, covered landings);
  - the air, space and submarine groups, carriers and paratroopers;
  - the wonder race and the ICBM;
  - the allies (AidAllies, AttackPlayer / DefendPlayer, CheckRaids / Retaliate);
  - BuildUp (deathmatch);
  - the prophets' and priests' micro (calamities), GiveImprovements, the WW1 groups, flares;
  - BalanceProduction, BuildingAbandoned / BuilderLost, CitizenLost's re-cost;
  - the robotic farm, the island map's hired garrison citizens, the chat taunts.

**The C++ classic on islands.** The first island bench (Small Islands, balanced, 8 seeds, 60 minutes) gave the port
7-0, 1 close. The C++ stood at 22 citizens from minute 10 and reached Copper in no game. Neither side used a
transport.
- **How it stalls.** A citizen request stood from minute 8 and never started: the epoch reserve held the food.
  Two bush gatherers fed it. The demand split wanted 2 on food with 1,108 food short.
- **Why.** The food room (GetGatherCapacity) counted only untapped sites. A resource's hit points fall from 1 when
  it is first gathered from (§8.57), and an island has little food outside its centres, so the room fell to about
  2 gatherers.
- **The fix, in both** (the owner's rule for such bugs). GetGatherCapacity counts the untapped sites for the
  citizen target only (GetWantedCitizens, inResource -1). A resource's own room counts every site with something
  left. The port did this already (§8.58's deviation), which is now gone: the port and the C++ agree.

| bench | before | after |
|---|---|---|
| Small Islands, 8 seeds: Lua won-lost | 7-0, 1 close | 6-0, 2 close |
| islands: the C++'s Stone / Copper | 6/8 at 35 min / 0/8 | 8/8 at 25 min / 2/8 at 55 min |
| islands: the C++'s citizens at 40 min | 22 | 40.5 |
| land (balanced, 32 seeds): Lua won-lost | 12-13, 7 close (§8.61) | 12-16, 4 close |
| land: the C++'s Copper / Bronze | 21 / 3 | 27 / 6 |
| land: the C++'s citizens at 30 / 40 min | 27.5 / 41.4 | 33.5 / 48.2 |
| land: Lua's Copper / Bronze | 24 / 12 | 20 / 11 |

**On land the C++ classic is stronger now.** It reaches Copper in 27 of 32 games, not 21, and it has 7 more
citizens at 40 minutes. This changes the opponent of every bench against the classic: the benches before this fix
need a new baseline. The port, the same code against a stronger opponent, now trails 12-16 (Copper 20 against 27).
That is near the edge of the noise, and it is to be watched as 2a goes on. On islands the C++ now plays, but the
port still wins 6-0 with 2 close (66.8 citizens at 40 minutes against 40.5). That gap is 2a's.
### 8.64 Porting everything, 2a: the ferries (2026-11-12, EE-SIM)

The classic's ferries (EECPResourceManager::RequestFerry / FerryGatherers / TransportIdle, IsIslandExpansionDue,
ChooseTransportClass's sea transports) are now `ai/classic_lua/ferry.lua`. On an island or planets map a mineral can
be wanted, or the island expansion due (its timer: island_expansion_timer minutes, started again after each ferry).
When the resource's first target lies on land the town's idle citizens cannot walk to, a free transport takes as
many as it holds to it. The crossing is lib.navy's landing: board, sail, unload. Ashore, the citizens are sent to
gather. With no free transport, one more is ordered while fewer than 4 exist and none is requested or being made.
The citizens on a trip are left alone by the gatherer assignment and the builders.

Small Islands, balanced, 60 minutes, 16 seeds:
- **Ferries:** 67 started, 50 landed, 6 aborted, 0 script errors. The C++ asked for 240 transports for its
  ferries and 35 for its attacks.
- **The result:** Lua 10-3, 3 close. The 8 seeds before the ferry gave 6-0, 2 close (§8.63), so the island gap is
  the economy's, not the ferry's.
- **Citizens:** the port has more from 10 minutes (23.8 against 21.0; 63.8 against 34.2 at 40 minutes).
- **Copper:** 12 of 16 games against 5.

Determinism on islands (seeds 3 and 5, 60 minutes, --repeat 2, ferries in both) is identical. Next in 2a: the
attack groups' crossings (the C++'s ASGSeaTransport: the transports for an attack across water), and why the port
outgrows the C++ on islands from the first minutes.

### 8.65 Porting everything, 2a: the attack crossings (2026-11-12, EE-SIM)

An attack sub-group whose target lies across water (on an island or planets map, the group's land not reaching
it) is now carried there, standing in for the C++'s EECPASGSeaTransport (army.lua `cross_step`).
- **The crossing.** The free transports (none held by a ferry or by another crossing) are taken until they hold the
  group, and lib.navy's landing carries it: board, sail, unload at the weakest beach of the target's land. Ashore,
  the group takes up its attack task again.
- **Too few transports.** One more of the epoch's sea transport is ordered at a time.
- **Giving up.** A crossing not under way within 5 minutes, or aborted, gives the group a defence task, and no
  crossing is tried again for 10 minutes.
- **The group's own orders** wait while it crosses (lib.groups `manual`).
- **DEVIATION.** The C++'s transport sub-group (its loading, escorts and covered landings) is lib.navy's landing.

Small Islands, balanced, 90 minutes, 8 seeds:
- 21 crossings started, all 21 boarded, 20 landed. 0 script errors.
- The C++ asked for 41 transports for its attacks.
- The result stays 6-0, 2 close: the island gap is still the economy's, which is next.

Determinism on islands (seeds 3 and 5, 90 minutes, --repeat 2, a landing in one of them) is identical. On land maps
nothing changes (no crossing is needed).

### 8.66 Porting everything, 2a: the island growth gap, the queues, the island garrisons (2026-11-13, EE-SIM)

**Where the gap starts.** On Small Islands (8 seeds) the two sides train their citizens in the same batches of 5
(at 30, 172, 314 and 456 s) for the first 8 minutes. They part from minute 6 (15.4 against 12.6 citizens), and by
20 minutes stand at 32.2 against 25.9. The epoch reserve and the epoch pressure are the same on both sides (a
traced game: 0.333 at 10 minutes on both, the Stone Age's 1,122 food reserved).

**Two differences, both ported:**
- **The queues (IsInProduction).** The C++ counts a class as in production while any producer is making it or has
  it queued (EEComplexUnit::IsResearchQueued). The port saw only the item being made, a documented deviation, now
  gone: own building records have `queued` (the techs behind the one being made; AI_LUA_UNITS §11), and the port's
  IsInProduction reads it.
- **The island garrisons (FillGarrison).** On island and planets maps the C++ fills a garrison with any available
  citizen, a gatherer too, not only the idle ones. A town centre hires its garrison: the citizens for the free room
  are trained at once when affordable, else requested. The upgrade then uses them up. In the C++, 73 citizens a
  bench die inside a garrison in the first 20 minutes (4.6 a game, at tile -10000, no killer); the port lost 0.4.
  The port now does both (civ.lua `fill_garrison`; the deviation in ORIGIN is gone).

| Small Islands, 16 seeds, 60 min | before | queues | queues + garrisons |
|---|---|---|---|
| Lua won-lost | 10-3, 3 close (§8.64) | 10-2, 4 close | 8-2, 6 close |
| citizens at 20 / 40 min, Lua | 31.1 / 63.8 | 31.1 / 63.2 | 27.9 / 58.9 |
| citizens at 20 / 40 min, the C++ | 24.2 / 34.2 | 24.2 / 34.9 | 24.1 / 34.4 |
| citizens lost by 20 min, Lua / C++ | 0.4 / 4.6 | 0.4 / 4.6 | 1.9 / 4.7 |

On land (balanced, 32 seeds) the queues give 11-17, 4 close (12-16, 4 close before), within noise. The garrisons
change only island and planets maps.

**Open in 2a.** The gap has narrowed but is not closed: 27.9 against 24.1 citizens at 20 minutes, and Copper 12 of
16 against 3. The C++ still loses more citizens in its first 20 minutes (4.7 against 1.9). The coast towers
(FindTowerSite's island coasts) go with the gates' and choke points' towers in 2b.

Determinism: islands (seeds 3 and 5) and land (seed 2), 60 minutes, --repeat 2, identical. 0 script errors.
### 8.67 Porting everything, 2a closed (2026-11-13, EE-SIM)

The close-out bench was Small Islands, balanced, 90 minutes, 32 seeds:
- **The result:** Lua 19-3, 10 close. Copper 29 of 32 against the C++'s 15; Bronze 17 against 0.
- **Citizens:** 54.1 against 36.4 at 40 minutes. Citizens lost: 14.7 against 14.4 in all, 1.8 against 3.6 by 20
  minutes.
- **The ferries:** 274 started, 214 landed, 43 aborted (3 found no plan).
- **The attack crossings:** 3 started, 2 landed. Few groups attacked across water in these games.
- 0 script errors.

The port plays islands better than the C++. That is accepted, and recorded in ORIGIN as a known deviation: the aim
is to replace the C++, not to copy its weaknesses. 2a is done: the ferries, the attack crossings, the queues, the
island garrisons. The coast towers move to 2b with the walls and the other tower sites.
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
