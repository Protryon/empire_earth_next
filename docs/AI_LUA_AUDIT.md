# S1 audit: policy and execution in the classic computer player

The audit asked for by docs/AI_LUA_PLAN.md §8 step 3 (2026-11-08). It lists, file by file, every function that
makes a decision and where that decision goes in the new engine. Paths are under `src/Empire Earth/AI Thread/`, and
line numbers are function starts at main `513dca2`.

**Destinations:**
- **V** = a tactic verb (execution only)
- **S** = a strategy order (Lua decides)
- **R** = a reflex (automatic C++)
- **C** = classic only

**Triggers:** U = every AI tick (1 s); T(n) = a timer of n seconds; E = an event.

## Call graph from `EEComputerPlayer::Update` (EEComputerPlayer.cpp:5899)

- **First call:** `EnableManagers`, `SetEnableTime`, `SetNextAttackTime`, `SetNextExpansionTime`,
  `SetNextIslandExpansionTime`, `Regroup` (builds the resource centres), `ChooseCivilization`, the extra-resource
  cheat. In deathmatch it also runs `CheckBuildingCounts` and `DefenseCommander::BuildUp` x3.
- **Every tick:**
  - `UpdateGroups` (only when a regroup is pending);
  - the combat-state decay (60 s);
  - `EnableManagers` on its timer;
  - `WorkOutPopulation` T60;
  - `FlagGroup` on the expansion timer;
  - `EECPCivilizationManager::Update` (:1246):
    - the first update: the sentinel tech, then `AddCosts(citizen x GetWantedCitizens)`;
    - `UnitManager::Update` :1053, `ResourceManager::Update` :2777, `BuildManager::Update` :1740,
      `ResearchManager::Update` :577, `WallManager::Update` :1046;
    - `AttemptEpochAdvance` :239, guarded;
    - `CheckBuildingCounts` :381 T30;
    - `RepairBuildings` :935 T31;
    - `ChooseGarrison` :1332 and `ChooseFortress` :1403, each on its upgrade timer;
    - `QueueMoraleBuilding` :513 T59;
  - `DiplomacyManager::Update` (a stub);
  - `EECPCombatManager::Tick` (:1584):
    - `AttackCommander::Update` :437 (the groups' `EECPAttackGroup::Tick`);
    - `DefenseCommander::Update` :1210 T8-30;
    - `Personnel::Update` T30;
    - `CombatManager::Update` :332 (the attack choice);
    - `UpdateWonderRace` :1599;
  - `ExplorationManager::Update` :284 T30;
  - `KeepStandingArmy` :5874 every 30 s;
  - `EECPTraceStats`.
- **Then:** the units in `mUnitsToKill` are killed.

**Events:**

| Event | Handled by |
|---|---|
| `UnitPlaced` | `CivMgr::UnitAdded`, `CombatMgr::UnitAdded` |
| `UnitTaken` | `CivMgr::UnitLost` (then `UnitMgr::UnitLost` and `BuildMgr::BuilderLost`), `CombatMgr::UnitRemoved` (`NoteThreat`), `Exploration::UnitRemoved` |
| `UnitCompleted` | `ResourceCenter::AdoptDropSite`, `UnitMgr::BalanceProduction` |
| `UnitGoalDone` | `RM::TransportIdle`, `RM::AddIdleGatherer` |
| `DamageTaken` | `EECPRMNavalFlee`, `EECPRMNoteDanger` (then `EECPRMMilitia`) |
| `EpochReached` | `ResearchMgr::EpochReached`, `EnterEpoch` (the favourites, `RetireObsoleteUnits`, `CombatMgr::EpochReached`) |
| `WonderStarted` | `RespondToUnit` |
| `FlareReceived` | `Attack` |

## EEComputerPlayer.cpp

| Line | Function | Decision | Goes to |
|---|---|---|---|
| 257 | EnableManagers | which managers may act (combat-state priority, random shuffle, max enabled) | C |
| 215 | WorkOutPopulation | the pop cap pulled toward the human's (popband) | C |
| 1241 | **CanAfford** | the epoch reserve by pressure; boom citizens first; recovery below 12 workers; the prehistoric/stone army cap; goldbank, payback, threatspend, goldfix | split: the plain check is the tactic's refusal reason; the rest is S (`reserve`, `army_cap`) |
| 1048-1135 | EECPPendingEpoch / EpochPressure / ScheduleSpan / Booming | epoch pressure, the boom window | S |
| 1178 | EECPArmyThreat | enemy strength within 40 tiles against own; latched 240 s; **not fog-honest**; static arrays, not saved | the measurement is the view's `threats`; the decision is S |
| 1339 | CatchUpEpoch | catch-up cheat | C |
| 5874 | **KeepStandingArmy** | army = standing_army x workers, 5 at a time, the favourite land type | S (`train`) |
| 448-527 | ChooseFavorite*Type | a random favourite unit per epoch | S (the mix in `train`/`attack`) |
| 4910 | ChooseCivilization | a random civilization | C |
| 5170 | SetNextAttackTime | a random attack interval | S |
| 5208/5222 | SetNextExpansionTime / IslandExpansion | expansion timers | S (`claim`, island expansion) |
| 656 | FlagGroup | activate the first inactive centre off enemy-capitol continents | S (`claim`); SetActive is V |
| 581 / 5986 | Regroup / UpdateGroups | build the resource centres; activate within 6x the drop distance; fill gaps | the centre bookkeeping is V/view; the first activations are S |
| 555 | UnitCompleted | rebalance queues among producers | V |
| 5838 | UnitGoalDone | idle bookkeeping | V/R |
| 1367 | DamageTaken | danger zones, naval flee | R |

## EECPCivilizationManager.cpp

| Line | Function | Decision | Goes to |
|---|---|---|---|
| 1252-1271 | Update (the first) | sentinel tech; mNeeds seeded with the wanted citizens' cost | C |
| 239 | **AttemptEpochAdvance** | when to advance (timer, schedules, safe bonus); ResearchCheapest for missing SOL; drop obsolete requests | S (`advance()`); V: queue the epoch when it is paid |
| 1121 | SetNextEpochAdvanceTime | random epoch delay, rubber band | C |
| 381 | **CheckBuildingCounts** | wanted buildings from unit counts and caps; military minimum by difficulty; 2 docks with coast fishing | S (whitelist and caps; `build`) |
| 193 | AdjustUnitCount | the counts that drive buildings | C |
| 513 | QueueMoraleBuilding | a house where morale is not full | S (`build(house, near)`) |
| 1332 / 1403 | **ChooseGarrison / ChooseFortress** | settlement to town centre, town centre to capitol | S (`upgrade` / `garrison`); FillGarrison is V |
| 306 | FillGarrison | hire or send citizens in | V |
| 1197 | UnitAdded | new citizens to the garrison or fortress | V, gated by the S order |
| 935 | RepairBuildings | idle citizens repair under 80 % HP | R |
| 532-794 | Get*Class, ChooseTransportClass, GetMaxEpoch, IsBehindInEpoch | lookups | V helpers (IsBehindInEpoch C) |

## EECPResourceManager.cpp

| Line | Function | Decision | Goes to |
|---|---|---|---|
| 2777 | Update | timers: wanted T5, assign T10, prune T15 | split |
| 2217 | **UpdateWantedGatherers** | the needs-ledger split, mining-limit cap, surplus cut, low-stock floor, army floor | S (`gather(r, n)` sets the targets) |
| 2136 | **EECPRMSplitByDemand** | the 3-tier demand split | S |
| 1856 | EECPRMGetDemand | the demand tiers | S (library code in Lua) |
| 2042 | **EECPRMActivateNeeded** | keep 3/5 patches; claim the stone/gold/iron the epoch needs; patch_expand | S (`claim` / `release`) |
| 1946 | EECPRMSettlePatches | a settlement on each worked patch or mine without a drop site | S (`build(settlement, near=site)`); EECPRMPatchSite :656 is V |
| 2005 | EECPRMGuardSites | a tower per worked site | S (`build(tower, near=site)`) |
| 1664 | EECPRMRequestBoats | docks and boats | S (`fish(boats, docks)`) |
| 1618/1638 | EECPRMCoastFish / CoastBoatsWanted | the boat target | view (fish) / S (target) |
| 1776 | GetGatherCapacity | room per resource | view (`sites` room); the woodcap is S |
| 309 | **AssignGatherers** | rebalance, then the food order (hunt, farm, near animals, far farms, far animals, unfinished farms, fish), wood, mines, ferries | V (`gather`); the food order becomes a `prefer` parameter |
| 2326 | RebalanceGatherers | free stuck builders; move up to moves_per_cycle | V |
| 199 | AssignIdleGatherers | idle citizens to the most under-served resource | V |
| 233 | AssignIdleBoats | idle boats fish by their dock | V |
| 679 | BuildSettlementFor | a far gatherer builds a settlement | S (or an R toggle); placement V |
| 1118 / 1057 / 915 / 872 / 988 | FindResourceTarget, FindForestTarget, FindFarm, FindMines, FindFish | site finders | V |
| 1325 | SendToGather | the gather goal | V |
| 256 / 1460 / 1501 | RequestFerry / FerryGatherers / TransportIdle | ferries | V; asking for a transport is S |
| 2418 / 2579 / 2647 / 2734 | Militia / NavalFlee / NoteDanger / FleeRaids | fight or flee, danger zones, a few soldiers at a raider | R |
| 858/1547/1230/790 | AddCosts / RemoveCosts / Earn / Spend | the mNeeds ledger | C |

## EECPBuildManager.cpp

| Line | Function | Decision | Goes to |
|---|---|---|---|
| 1740 | Update | start the first startable request per epoch queue (T3); AssignBuilders T120 | V (the queue runner) |
| 1566 | **QueueBuilding** | hidden vetoes: houses in the Prehistoric, early_trainers, no duplicate while one is unfinished | queue V; the vetoes S |
| 1621 | QueueBuildingAt | at a tile | V |
| 278 | StartBuilding | site, builder; ClaimExpand when a farm, granary or settlement has no room | V; the ClaimExpand is S (report "no site") |
| 1353, 579-1432 | FindSite and the Find*Site functions | placement per class (FindTowerSite :1096: shore, centre, gate, choke) | V; the tower priority becomes `near` |
| 393/437/237/1637 | AssignBuilders, CancelStalledBuilds, BuilderLost, BuildingAbandoned | builder upkeep | R |
| 1675 | DropObsoleteRequests | on an advance | V |
| 1550 | HasLaterEpochRequests | the advance's gate | C |

## EECPUnitManager.cpp

| Line | Function | Decision | Goes to |
|---|---|---|---|
| 227 | **GetWantedCitizens** | the citizen target | S |
| 1053 | **Update** | the automatic citizen request; transports; the requests per epoch through CanAfford | auto-citizen S; the runner V (`train`) |
| 675 | RequestUnits | merge into the queue; **queues the cheapest producer when there is none** | V; the auto-producer is hidden policy (report "no producer") |
| 853 | StartProduction | producer choice, least busy, round-robin | V (`train(class, n, at)`) |
| 280 | BalanceProduction | spread over producers | V |
| 637 | RequestFishingBoats | one boat if none | V helper |
| 998 | UnitLost | re-request what a lost building made; reschedule a lost epoch | report it to S |
| 772 | DropObsoleteRequests | | V |
| 339-551 | queries (builders, idle citizens, counts, military units) | | V / view |

## EECPResearchManager.cpp

| Line | Function | Decision | Goes to |
|---|---|---|---|
| 577 | Update | start the first queued tech that can start; ChooseResearch on a random timer | the runner V; the choice S |
| 87 | **ChooseResearch** | citizen upgrades first, else a random candidate | S (`research`) |
| 256 | Research | queue in mEpochIDs; **orders the producer if missing** | V; the auto-producer as RequestUnits |
| 426 | TryResearch | start at an idle producer if affordable | V (`research` / `advance`) |
| 292 | ResearchCheapest | buildings for missing SOL | S (the view gives the SOL missing) |
| 394 / 538 | ReplaceUnaffordable / EpochReached | | C |
| 553 | IsWanted | the tech filter | V (validation) |

## Combat (EECPCombatManager, EECPAttackCommander, EECPDefenseCommander, EECPExplorationManager)

| File:line | Function | Decision | Goes to |
|---|---|---|---|
| Combat :332 | **Update** | when to attack and whom (front, nearest, weakest gatherer via mNeeds, damage; aggression) | S (`attack(target, mix)`) |
| Combat :561 | NoteThreat | after over 20 citizen losses in a region, send groups | S (a view threat event; `defend`) |
| Combat :1162 | Attack | nearest groups, a unit-limit share by difficulty | V dispatch; the share S |
| Combat :1653/1721/1784/1599 | AttackUnit, DefendUnit, RespondToUnit, UpdateWonderRace | wonders | C (S later) |
| Combat :1088 | EpochReached | WW1 attack, cruisers | C |
| Combat :757/777 | GetUnitsAvailable / GetUnitLimit | the military pop share | S (army cap) |
| Combat :1135/666 | ChooseTargetUnit / ChooseTask | | V |
| Attack :437 | Update | the groups' ticks | V (the state machines stay) |
| Attack :140 | **CreateGroup** | the default strategy; the prehistoric/stone peace veto at :126; **MakePlan, the UnitSelector and Personnel train units on their own** | V (`group`); the veto S; the sizing and auto-production are hidden policy |
| Defense :695/794/728 | PlanAid / AidOwnTowns / AidAllies | town guards | S (`defend(area)`); allies C |
| Defense :511/1446/575/829 | SendAid, AssignSubGroups, SendBestSubGroup, CounterAttack | | V |
| Defense :982 | ComputeTownThreat | the town threat 0..1 | view |
| Defense :1112 / :490 / :649 | GetUnitAllowance / CreateDefendGroup / BuildUp | | S |
| Defense :774/901 | CheckRaids / Retaliate | the citizens' last stand | C (S later) |
| Defense CheckNaval etc. | ship and space quotas, batteries, cloaking | | C (S5) |
| Defense :1808 | AssignSubGroupTask | guard posts | V |
| Exploration :284 / :85 | Update / RequestExplorer | explorer counts per domain | the counts S (`scout`); the request and goal V |
| Personnel :657, :842, :742, :586 | Update, KillUnitsForMilitary, RetireObsoleteUnits, FillRequest | culling; the retire cheat; the hidden production path | S/C; C; V |
| Wall manager :1046 | Update | walls from the Copper Age | C (later `build(wall, near)`) |
| UnitSelector :1081/958/1216 | ComputeGroupSize, ComputeDomainShares, WeighCounters | the attack size and mix | S (the mix); V (slot filling) |

## What the view can read cheaply

- **Resource centres:** `mResourceCenters`, ordered by distance from the town. For each centre: `mUnits`, `mUnit`
  (its drop site), `mIsActive`, `FindResourceUnit`, `FindBuildSite`, `GetMoraleFill`, `GetCenter`.
- **Gatherers:** `RM::mGatherers[7]` (index 0 is idle) and `mWantedGatherers`.
- **Resources by kind:** `mForests`, `mMines` (animals by distance), `mFish`.
- **Danger and claims:** `RM::mDangerZones`, `EECPRMResourceInDanger`, `BuildMgr::HasEnemyNear`, `EECPClaimRange`,
  `EECPRMInClaimRange`.
- **Queues and counts:**
  - the build queues `BuildMgr::mQueues[epoch]`;
  - the requests `UnitMgr::mRequests[epoch]` and `mNumInProduction`;
  - `GetUnits`, `GetCount`, `GetNumWorkers`, `CountMilitaryUnits`;
  - research `mEpochIDs` and `mCandidates`;
  - `mUnitCounts` are driver counts, not real counts.
- **The tech tree:** the current epoch, `GetRequiredSOL` and `GetCurrentSOL`, and costs through `GetCost` and
  `ScaleCost`.
- **Stock and income:** stock in `mResourceCaps`; income through the trace income hooks.
- **Intelligence:** `EECPIntelligence` files (armies, bounds, gatherers per resource, strengths). They are **not
  fog-honest** (walks `mFirstUnit`, EECPIntelligence.cpp:169), and neither is EECPArmyThreat. The view needs a
  seen filter (`IsSeenBy`, as CombatMgr::Update :526).
- **Map:** `DefenseCommander::ComputeTownThreat`, tile visibility (`IsVisibleTo` / `IsSeenBy`), `gContinentManager`.

## Hard dependencies

1. **CanAfford is a policy hub.** Production, TryResearch, ChooseGarrison/Fortress and FillGarrison all call it.
   The new engine needs a plain check plus a reserve the strategy sets. Branch at the call sites, so the classic one
   stays byte-identical.
2. **The mNeeds ledger.** QueueBuilding, RequestUnits and Research add to it, and the build runner removes from it.
   Both the classic split and the classic attack target read it.
3. **mWantedGatherers drives AssignGatherers, Rebalance and AssignIdleGatherers.** `gather` writes it, and the next
   engine skips the splits.
4. **The build queues per epoch:** StartBuilding only runs inside the queue walk. The demand split reads the first
   two items, and HasLaterEpochRequests gates the advance.
5. **The unit request queues.** RequestUnits (:712-748) and Research (:263) auto-request producers. UnitLost
   re-requests, and Personnel's FillRequest feeds requests from the attack groups.
6. **Research mEpochIDs** hold both the epoch advance and research; EECPPendingEpoch, the demand split and
   CanAfford all read them.
7. **The town centre tile** (`BuildMgr::mTownCenterTile`) is read by nearly every finder. It is set as a side
   effect in `UnitMgr::UnitAdded` :989.
8. **Groups train on their own:** CreateGroup → MakePlan → UnitSelector → Personnel → RequestUnits. The defence
   commander does the same.
9. **Threat state is scattered:**
   - CombatMgr's `mTileCounts`;
   - RM's `mDangerZones`;
   - EECPArmyThreat's static arrays;
   - the defence packages;
   - the intelligence armies.
10. **Unsaved per-player statics** (a lockstep and save risk):
    - `sClaimSteps` / `sClaimLast` (EECPTrace.cpp:257);
    - the `sLast` timers in SettlePatches, GuardSites and Militia;
    - boatrush2's `sBoats` / `sHold`;
    - ArmyThreat's `sChecked` / `sUntil`;
    - ScheduleStart's `sEpoch` / `sStart`;
    - `sClaimPass`.

    The claim range must become saved state before `claim` is a verb.
11. **EnableManagers flags** (`mProductionEnabled`, `mSettlementsEnabled`, `mBuildingCountsEnabled`, ...) gate
    execution paths.
12. **Hidden decisions inside executors:**
    - StartBuilding's ClaimExpand;
    - QueueBuilding's vetoes;
    - CheckBuildingCounts' dock cap;
    - AssignGatherers' boat and ferry requests;
    - RequestUnits' and Research's producer auto-queue.

## Extraction order for the boom opening

1. **Gathering and claims.**
   - `gather(r, n)` writes mWantedGatherers, and the next engine skips the splits and ActivateNeeded.
   - AssignGatherers, Rebalance, SendToGather and the finders stay as the executor, with the food preference as a
     parameter.
   - `claim` and `release` call SetActive, and the claim range becomes saved state.
   - The view gets the centres, the room and the danger zones. Militia and flee stay reflexes.
2. **Building and placement.**
   - `build(type, near)` uses QueueBuilding/At and the build runner.
   - The QueueBuilding vetoes and StartBuilding's ClaimExpand become classic-only.
   - The next engine never calls CheckBuildingCounts, QueueMoraleBuilding, SettlePatches or GuardSites. Their
     placement helpers become `near` resolvers.
   - `fish(boats, docks)` comes out of RequestBoats.
3. **Production.**
   - `train(class, n, at)` uses RequestUnits and the runner, with a plain affordability check.
   - The auto-citizen request, the auto-producer and KeepStandingArmy are off for the next engine.
   - UnitLost's re-request becomes an event.
4. **Research and the advance.**
   - `research` and `advance` go into mEpochIDs and run through TryResearch.
   - AttemptEpochAdvance, SetNextEpochAdvanceTime, ChooseResearch and ResearchCheapest are off for the next engine.
   - The reserve is an amount the strategy sets. Garrison and fortress choices become orders.
5. **Groups.**
   - `group(units, task)` wraps CreateGroupFromStrategy with an explicit mix and size. It bypasses CombatMgr::Update,
     SetNextAttackTime, the peace veto, PlanAid, BuildUp, CheckRaids and NoteThreat.
   - `scout` reuses RequestExplorer and EEUGExplore.
   - MakePlan's production is reported to the strategy, or turned off.
