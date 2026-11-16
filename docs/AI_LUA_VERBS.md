# The coarse verbs: inventory and the move to Lua

Stage 6 of the computer player (2026-11-15; AI_RESEARCH §8.92). classic_lua gives unit-level commands only. boom,
rush, ageofai and the rb libraries still return the older order tables (`{verb = "gather", ...}`), which
EECPNextLua.cpp's `EECPNextRunOrder` hands to `EECPNext*` in EECPNextTactics.cpp. Those functions only fill the
C++ managers' queues and settings. The managers' runners do the work every tick in `EECPNextUpdate`
(EECPNext.cpp): `UnitManager`, `ResourceManager`, `BuildManager` and `ResearchManager::Update`, and `RepairBuildings`
every 31 s.

This document lists every verb, what it does in C++, and where each part goes.

## 1. The order verbs

| Verb | Who calls it | C++ (EECPNextTactics.cpp) | Executed by |
|---|---|---|---|
| `gather{resource, n, prefer = patch/hunt, exact, granary_farms}` | boom, rb_economy, ageofai, test_opening | sets `mWantedGatherers[r] = n` (food: + the boats on the lists), `mFoodPrefer`, `mFoodLand`, `mExact`, `mGranaryFarms`; result `queued/no_dropoff` when the ready places (patches 6, mines × mining limit, wood: one edge by a finished drop) are fewer than n | ResourceManager::AssignGatherers every 10 s (below) |
| `claim{site}` / `release{site}` | boom, rb_economy, ageofai | `EECPResourceCenter::SetActive` on view.sites[site] | the finders read `mIsActive` |
| `claim_range{range}` | same | `mClaimRange` (25..45) | the finders (patches and mines within it first), `view.hunt` (max(range, 24)) |
| `build{type, near = town/site/tile}` | all | placement (below), the unsafe test (`EECPNextBuildUnsafe`), then `BuildManager::QueueBuildingAt` | BuildManager::Update every 3 s, AssignBuilders every 2 min, BuilderLost, CancelStalledBuilds |
| `fish{boats, docks}` | boom, rb_economy, ageofai | a dock by the fish (`EECPNextDockByFish`) while fewer docks stand or are queued; then `RequestUnits(fishing boat, boats - have)` | UnitManager::Update, then AssignIdleBoats |
| `train{type, n, town}` | all | refused with no producer or not in the epoch; `UnitManager::RequestUnits` | UnitManager::Update every tick |
| `reserve{food..iron}` | boom, rb_economy, ageofai | `mReserve`, which `EECPNextCanAfford` (EEComputerPlayer::CanAfford) keeps off every cost but the epoch | every CanAfford of the runners (and of `ai.train` / `ai.build`) |
| `research{tech}` | boom, view.lua, ageofai | refusals (not wanted, researched, busy, no producer); `ResearchManager::Research` into `mEpochIDs` | ResearchManager::Update: TryResearch at an idle producer when affordable |
| `improve{type, what}` | boom | pays and applies an improvement level at once (no runner) | none |
| `advance` | boom, rb_economy, ageofai | SOL and affordability checks, `DropObsoleteRequests`, `Research(epoch)`, `mAdvancingEpoch` | ResearchManager::Update |
| `attack`, `raid`, `defend`, `scout`, `retreat`, `regroup`, `disband` | rb_army (rush), test_opening | EECPNextArmy.cpp's groups | EECPNextArmyStep (not a manager; out of this stage's scope) |

## 2. What the runners do with them

**UnitManager::Update** (every tick): the population room (cap - in production - population); per epoch FIFO, a
request starts at once when all of it is affordable and fits, else one unit when one is affordable (a non-citizen
waits while citizens are asked for and the workers are under half). `StartProduction`: the built producers of the
class on the request's town or the continent with most of them, round robin from the least busy
(`EECPUMQueueLess`), `QueueResearch` each. `RequestUnits` merges a class into its open request.

**ResourceManager::AssignGatherers** (every 10 s; RebalanceGatherers and EECPRMShedCrowds first):
- the lists: every citizen of the AI's on one list by resource, or idle; a gatherer the game moved on goes to the
  list of what it gathers;
- the idle pool: listed citizens with no goal (or waiting at a full resource), not building, not walking to see a
  site;
- per resource with fewer listed than wanted: food — hunt first (prefer hunt: the game within max(claim range, 24) of
  the town, 3 hunters an animal), the patches by a finished drop (6 to a patch, the claim range first), idle farms,
  the game near, any patch, the game far, unfinished farms, the boats to fish; wood — the forest edge nearest the
  town when a finished drop is at hand, else the edge nearest each drop; stone, gold, iron — the claimed centres from
  the farthest, a unit with room, explored or by a drop, not blocked (`EECPNextSiteBlocked`), by a finished drop,
  the claim range first;
- the rest: up to 4 builders to each settlement or granary going up (EECPRMHelpBuildDrops), then AssignIdleGatherers
  (the most under-served of gold, iron, stone, wood; none past its count with `exact = 2`);
- the idle boats to the fish nearest the nearest dock (none near a seen warship; away from other docks first);
- ShedCrowds: no more than the mining limit on a unit, 6 on a patch; a tree with no drop at hand left when an edge
  by a drop has trees;
- every 15 s: the depleted animals and fish dropped, idle citizens back on the idle list.

**BuildManager::Update** (every 3 s): up to 4 requests tried, the first that starts ends the update (a request not
producible goes to the back). StartBuilding: dropped when the site came under a tower or soldiers (event
`no_site`); an idle-or-gathering citizen nearest the site on its path area (`FindBuilderForTile`); the foundation
placed (`EEMakeBuilding`, paid then); a farm request takes an unbuilt farm foundation of a granary. AssignBuilders
(every 2 min): a builder for every foundation that has none; a foundation under a seen tower is cancelled and
refunded. BuilderLost: the nearest available citizen takes over, else the request is queued again.
CancelStalledBuilds: a builder waiting on a taken site, the foundation refunded and queued again.

**ResearchManager::Update** (every tick): the first queued tech of each epoch whose producer (an idle built
non-citizen) can afford it, `QueueResearch`.

**CivilizationManager::RepairBuildings** (every 31 s): idle-or-gathering citizens to the land buildings under the
repair ratio, the nearest on the same land first.

**The placement** (`EECPNextBuild`):
- `near = town`: BuildManager::FindSite, by class: granary (beyond a centre's drop-off distance), dock (shore),
  temple / settlement / university / hospital (FindCenterSite), towers (FindTowerSite: the worked centres' gold, iron
  and food, gates, choke points), production buildings (FindProductionSite), farms (a granary's unbuilt field);
  other classes no site; a dock is the dock by the fish; a tower with none, the rings 3..12 round the town;
- `near = site`: settlement and granary next to the centre's units with no drop yet (EECPNextAdjacentSite, gap 0
  first); a site not explored sends the nearest free citizen to look (90 s); others the rings 2..7; a settlement then
  the rings 6..8 or the centre's own; a tower with none goes to the town;
- `near = tile`: a settlement by the forest edge (EECPNextAdjacentSite), else the rings 0..6;
- every rule tests EECPBuildManager::CanBuildAt (the footprint with a one-tile margin, the town's land, no armed
  enemy near; strict: no choke point, hill, town centre within 3, gate within 4, a walkable ring).

## 3. The view fields the managers fill

`gatherers`, `idle` (the lists), `wanted` (`mWantedGatherers`, food `mFoodLand`), `claim_range`, `reserve`,
`advancing` (`mAdvancingEpoch` or an epoch in a queue), `hunt` (the animals list `mMines` within the hunt reach),
`hunters`, `wood` (FindForestTarget), `buildings[].queued` (the build queues), `costs[].requested`
(`mWaitingCounts`), `costs[].producers` (the unit lists), `sites[].claimed` (`mIsActive`), `sites[].workers` and
`room` (`EECPNextAssigned`: the lists), `town_x/y` (`BuildManager::mTownCenterTile`), the `no_site`,
`production_lost` and `advance_lost` events, `results`.

## 4. Where each part goes

- **data/ai/lib/verbs.lua** (new): `verbs.wrap(M)` wraps an AI module. Its step runs the runners in Lua before the
  script (as EECPNextUpdate does), fills the view fields of §3 from its own state, runs the script, carries out the
  economy verbs itself and passes the group verbs on to C++ (their results merged back in order). Its state is in
  `memory.verbs`. It holds the citizens and boats (`ai.take`), so no C++ runner touches them.
  - train / fish / research / advance / reserve / improve: request FIFOs, `ai.train` / `ai.research` at the least
    busy producer, the reserve checked in Lua before each;
  - build: the request queue every 3 s, the builders by `ai.units`, `ai.build`, the builder check every 2 min; the
    placement's rules ported to Lua over a C++ placement helper (CanBuildAt's legality as `ai.find_site` options);
  - gather / claim / release / claim_range: AssignGatherers, Rebalance, ShedCrowds, HelpBuildDrops, the finders and
    the boats in Lua over `ai.resources`, `ai.gatherers`, `ai.drop_site` and two helper queries over the view's
    threat memory (`ai.site_blocked`, `ai.build_unsafe`);
  - repairs every 31 s.
- **Kept in C++** (the engine and helper queries): the view's summary, the resource centres (EEComputerPlayer's
  grouping, a site id per centre), the threat memory and its tests, CanBuildAt's legality (moved out of the build
  manager), the reflexes (militia, danger zones and flight, naval flight: they act on the held units of a seat that
  asks with `ai.reflex{held = true}`), the wall utilities the bindings use (EEGetWallLine and friends).
- **Deleted once each AI is at parity:** the EECPNext* economy verbs and their order parsing, EECPUnitManager,
  EECPBuildManager, EECPResourceManager (but the reflexes), EECPResearchManager, EECPCivilizationManager and
  EECPWallManager (no Lua AI builds walls through it: the build verb refuses walls; ai.wall lays them), their
  EECPNextUpdate ticks, the manager hooks in EEComputerPlayer.

## 5. Decision code left in C++: the reflexes

The reflexes still decide in C++, on the damage events (EEComputerPlayer::DamageTaken), for every computer seat that
leaves them on (`ai.reflex`). They stay for now (owner, 2026-11-15):

| Reflex | Code (EECPResourceManager.cpp today) | What it decides | Who uses it |
|---|---|---|---|
| militia | EECPRMMilitia, ~165 lines | a citizen hit by soldiers: the odds of the citizens, soldiers and towers near against the raiders; fight (the soldiers, then the nearest citizens as the odds need) or run (to a tower, the town centre or the farthest drop site), the spot a danger zone | rush (the personality's `military.militia`); boom (lib.defence), ageofai (its militia) and classic_lua switch it off |
| danger zones and flight | EECPRMNoteDanger, EECPRMFleeRaids, EECPRMResourceInDanger, ~110 lines | a human's harassing raid (2 soldiers at most) marks a zone for 30 s; the gatherers within 8 tiles go home, the 4 nearest free soldiers attack the raider, the finders and the view's `blocked` / `danger` skip it | every seat against a human; against computers only with the experiment `flee` |
| naval flight | EECPRMNavalFlee, ~70 lines | a boat or ship hit by a warship it cannot answer leaves with the own units within 12 tiles | boom (unless lib.defence's `boats`), rush, ageofai |

With the Lua verbs a seat's citizens are held (`ai.take`), so the verbs library asks `ai.reflex{held = true}`: the
militia and the danger zones take held citizens too, as they took the runners' citizens before.

**What porting them would take:** the micro step (`micro`, 250 ms) already gets the `attacked` events; lib.defence is a
Lua militia and flight (boom's), and classic_lua's militia.lua ports EECPRMMilitia. A shared `lib/reflex.lua` would be
EECPRMMilitia's odds and orders on ai.seen / ai.strength / ai.attack / ai.move, the danger zones as script memory read
by the verbs' finders in place of `ai.site_blocked`'s `danger`, and the naval flight on the ships' events. Each
reflex is benched on its own (rush for the militia, an island map for the naval flight); then `ai.reflex` and the C++
reflexes go.

## 6. As built (stage 6)

**Lua** (data/ai/lib/verbs.lua): the eleven economy verbs and their runners, the view fields of §3, the held state
the script sees (who holds a citizen: the library holds them all in the engine, a script's ai.take and ai.release move
them between the two), the results merged in order. boom and ageofai return `require("lib.verbs").wrap(M)`;
classic_lua needs none of it.

**C++ kept, as helpers** (ee/EECPPlacement.h, ee/EECPReflexes.h, the view):
- EECPPlacement: the town centre tile (the unit manager's UnitAdded rule), the shore and space site tiles, the "unit
  near" tests, CanBuildAt, FindSite and its finders (centre, granary, production, shore, tower, wonder, airport,
  anti-air, space), the epoch's classes (tower, wall, gate, anti-air, fishing boat), and the build verb's ring,
  tower-ring, by-the-resource and dock-by-the-fish searches; Lua reaches them through `ai.place`;
- the reflexes (§5) with the danger zones on EEComputerPlayer;
- the view: the gatherers and the idle by their goals, the producers by a unit scan, the game near the town, the
  forest edge (the forests sorted by the town, EECPNextForestTarget / EECPNextWoodTarget), the scouting memory
  (EECPNextArmy.cpp's scan: the enemies' units seen, gather spots, towers);
- `ai.site_blocked`, `ai.build_unsafe`, `ai.techs{name}`, `view.sites[].id` and `.ids`, `ai.reflex{held}`.

**Deleted:** EECPUnitManager, EECPBuildManager (but the placement), EECPResourceManager (but the reflexes),
EECPResearchManager, EECPCivilizationManager, EECPWallManager, EECPNextTactics.cpp, the economy verbs' order parsing
and state (claim range, reserve, food preference, exact, granary farms, explore walkers, repairs), the group verbs
(attack, raid, defend, scout, retreat, regroup, disband) and their groups' step and save block (only rush, balanced and
test_opening called them: rush and balanced are gone with the remove-rush branch, test_opening with this stage), the
classic's dead epoch-pressure helpers, the scenario setters' mirror into the building counts, the managers' save
blocks (the state block is version 6; older saves do not load).
