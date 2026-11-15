// The new computer player's tactics layer (docs/AI_LUA_PLAN.md §2, stage S1; docs/AI_LUA_AUDIT.md). New in this port
// (no original address).
//
// Every computer player runs this engine (the original's decisions were deleted in stage 5). It keeps the managers'
// executors (gatherer assignment, the build queue runner and placement, the production runner, the research runner,
// the attack group state machines) and the reflexes (militia, flee, naval flee, repairs, builder upkeep), and takes
// every decision from a Lua strategy through the verbs below. The strategy reads only the view (EECPNextView):
// integers, fog-honest.
//
// Every verb returns an EECPNextResult (done, queued or refused with a reason) and writes one trace line
// (EE_AI_TRACE, manager "next"): `next.order verb=build cls=... near=town result=refused reason=no_site`.
#ifndef EE_EECPNEXT_H
#define EE_EECPNEXT_H

#include <string>
#include <vector>

class EEComputerPlayer;
class EECPLuaState;
class EEComplexUnit;
class EEUnit;
class EETechTreeNode;

// The next engine's tick (EEComputerPlayer::Update calls it instead of the managers' decisions)
void EECPNextUpdate(EEComputerPlayer* inPlayer, unsigned long inTime);

// ---------------------------------------------------------------------------------------------------------------
// Results

enum EECPNextCode {
    kNextDone = 0,      // carried out now
    kNextQueued = 1,    // handed to a runner (the build, production or research queue)
    kNextRefused = 2,   // nothing happened; see the reason
};

enum EECPNextReason {
    kNextReasonNone = 0,
    kNextReasonNoSite,          // no place to build
    kNextReasonNoProducer,      // no building (or unit) that makes or researches it
    kNextReasonCannotAfford,    // the stock (less the reserve) does not cover it
    kNextReasonNotInEpoch,      // not available in this epoch (or obsolete)
    kNextReasonUnknownClass,    // no such unit class or technology
    kNextReasonBadArgument,     // a count, resource, site index or player out of range
    kNextReasonNoUnits,         // no units to form the group from
    kNextReasonRequirements,    // the epoch's SOL is not met, or the last epoch is reached
    kNextReasonBusy,            // already queued or under way
    kNextReasonNoTarget,        // a group verb: no place to go (no enemy town seen, nothing left to explore)
    kNextReasonUnsafe,          // build: the site (or the way there from the town) is under an enemy tower or soldiers
                                // seen there (EECPNextBuildUnsafe; docs/AI_RESEARCH.md §8.35)
    kNextReasonUnexplored,      // build near a site not explored yet: a citizen walks there first (§8.36), the order
                                // is to be given again once it is seen (the placement needs the tiles round it)
    kNextReasonNoDropoff,       // gather (queued): fewer places by a finished drop site than gatherers wanted; the rest
                                // wait (and help build the drop sites going up) until one stands (§8.36)
};

struct EECPNextResult {
    long mCode;                 // EECPNextCode
    long mReason;               // EECPNextReason
    long mCount;                // what was done or queued (units, buildings, gatherers)
};

const char* EECPNextCodeName(long inCode);       // "done", "queued", "refused"
const char* EECPNextReasonName(long inReason);   // "-", "no_site", "no_producer", ...

// ---------------------------------------------------------------------------------------------------------------
// The view: a fresh snapshot each step, integers only, fog-honest (only what the player sees now or has seen, unless
// the reveal cheat is on). Resources are indexed 0..4 = food, wood, stone, gold, iron (kResourceFood + i inside the
// game). Field names are the Lua table's (stage S2).

const long kNextResources = 5;

struct EECPNextSite {           // a resource centre (EEComputerPlayer::mResourceCenters, by distance from the town)
    long index;                 // the centre's index this step (claim/release/build near take it)
    long id;                    // a key that stays with the centre (its first unit id; minus the drop site id for the
                                // town centre's own centre): the Lua verbs key their claims by it
    long resource;              // 0..4: the resource most of its units hold (-1: none left)
    long units;                 // its resource units of that resource with some left (a mine's hit points drop to 0 at
                                // the first gather, EEUnit::TakeHit; it stays until its amount is gone)
    long amount;                // what they still hold
    long room;                  // gatherer slots left: the capacity less the workers
    long workers;               // gatherers sent to its units (EECPNextAssigned: working, walking or waiting there)
    long capacity;              // the gatherers it holds: a forage patch kNextPatchPlaces in all, a mine the mining
                                // limit per unit (§8.35: the owner's "strictly no more than 6 to a patch")
    long x, y;                  // the tile of its first resource unit with some left
    long distance;              // tiles from the town centre
    long claimed;               // 1: active (gathered from)
    long has_drop;              // 1: a drop site (town centre, settlement; granary for food) at hand: within the rings
                                //    build(settlement, site) places one in (kEECPPatchDropRange + 2), plus its half size
    long ready[kNextResources]; // §8.36, by resource: 1 when one of its units of it has a finished drop site at hand
                                // (EECPNextUnitDropReady: the finders send gatherers only there)
    long drop[kNextResources];  // ... a drop site at hand, finished or going up (a centre may hold gold and iron: its
                                // has_drop, from its first unit, said yes for the iron 6 tiles further)
    long has_tower;             // 1: one of the player's towers within 8 tiles
    long danger;                // the seen enemy strength within 12 tiles, plus 1 in a recent danger zone
    long patch;                 // 1: its food is a forage patch
    long explored;              // 1: its tile is visible to the player (EETile::IsVisibleTo)
    long blocked;               // kNextBlocked*: why the gatherer finders would pass its first unit over
    long holds[kNextResources]; // its resource units of each resource with some left (a centre may hold stone and
                                // gold: the finders take any of its units of the resource they look for)
    long home;                  // 1: one of the start resources by the town (EECPNextIsHome)
    long cause;                 // TEST ONLY (the site trace): the unit id that blocks it (enemy_near, tower; 0 none)
};

enum {                          // EECPNextSite::blocked
    kNextBlockedNone = 0,
    kNextBlockedUnexplored = 1, // not visible (the next engine's finders still send gatherers to a claimed site)
    kNextBlockedEnemyNear = 2,  // enemy soldiers seen within kNextThreatRange tiles in the last kNextThreatMs (a home
                                // site: seen there now); EECPNextSiteThreat
    kNextBlockedDanger = 3,     // a recent danger zone (EECPRMResourceInDanger; not a home site)
    kNextBlockedArea = 4,       // within 10 tiles of a non-enemy player's area (gaia, an ally; not a home site)
    kNextBlockedTower = 5       // an enemy tower or armed building seen (and not seen destroyed) whose range covers it
                                // (not a home site)
};

// The next engine's gather safety (EECPNextView.cpp; docs/AI_LUA_PLAN.md §3.2 "blocked"): what keeps gatherers off a
// resource is a real threat to them that the player sees or remembers: enemy soldiers (land military, not citizens),
// and enemy towers or armed buildings in range. The start resources by the town (2 gold, 2 iron, 1 stone, 2 forage
// patches: the nearest within kNextHomeRange tiles) are home: blocked only while enemy soldiers are seen on them.
const long kNextThreatRange = 9;            // tiles: seen enemy soldiers this near a resource keep gatherers off it
const long kNextThreatMs = 20000;           // ... remembered this long after they were last seen
const long kNextTowerMargin = 3;            // tiles past a tower's range (the gatherers stand round the mine)
const long kNextHomeRange = 20;             // tiles from the town centre: the start resources
const long kNextDropReach = 3;              // tiles from a drop site's edge (about): a site's drop is at hand
const long kNextDropSlack = 1;              // ... a resource unit's (EECPNextUnitDropReady, §8.36)
const long kNextExploreMs = 90000;          // a walker sent to see a site is kept there this long at most (§8.36)
const long kNextWoodSlack = 2;              // ... a tree's (the forest edge: the settlement stands a tile or two off)
const long kNextPatchPlaces = 6;            // gatherers a forage patch holds in all, however many bushes (§8.35)
const long kNextHuntRange = 24;             // tiles from the town centre: the game hunted at least (§8.35: hunters
                                            // 40+ tiles out); the claim range when it is larger (EECPNextHuntReach)
long EECPNextHuntReach(EEComputerPlayer* inPlayer);
const long kNextHuntersPerAnimal = 3;       // hunters sent to one animal at most (the finder takes the next one then)
const long kNextDockReach = 30;             // tiles from the town centre: the fish a dock is built for (§8.35)
// the gatherers sent to a resource unit by the player: their gather goal names it (walking, waiting or working;
// gWorld.GetResourceUsers counts only those working, so 9 were sent to one patch of 6)
long EECPNextAssigned(EEComputerPlayer* inPlayer, EEUnit* inResource);
// whether a resource unit has room for one more of the player's gatherers: the mining limit per unit and, for a forage
// bush, kNextPatchPlaces over its whole patch (the resource centre holding it)
bool EECPNextHasRoom(EEComputerPlayer* inPlayer, EEUnit* inResource);
// a finished drop site for the resource at hand (docs/AI_RESEARCH.md §8.36, the owner: "build a settlement before using
// a node"): a town centre, capitol or settlement (inFood: a granary too) whose centre lies within kNextDropReach tiles
// plus half its size of the tile (as the view's has_drop, which also counts one going up)
bool EECPNextDropReady(EEComputerPlayer* inPlayer, long inX, long inY, bool inFood, long inSlack = 0);
// the same for a resource unit, kNextDropSlack tiles further (a mine or bush's own size; the gatherers stand round it)
bool EECPNextUnitDropReady(EEComputerPlayer* inPlayer, EEUnit* inResource);
// the player's finished drop sites (inFood: granaries too): x, y (the centre tile) and half the size, three longs each
void EECPNextReadyDrops(EEComputerPlayer* inPlayer, bool inFood, std::vector<long>& outDrops);
// the wood target (the view's wood; the resource manager's, before stage 6): a tree with a finished drop site at hand,
// the forest point nearest the town centre first, else the one nearest each drop site; NULL: none
EEUnit* EECPNextWoodTarget(EEComputerPlayer* inPlayer);
// the tree of the forest nearest the town centre (FindForestTarget's); NULL: none
EEUnit* EECPNextForestTarget(EEComputerPlayer* inPlayer);
// whether a building at a tile would put its builders in harm's way: a seen enemy tower whose range covers the tile
// or the straight way to it from the town centre, enemy soldiers seen within kNextThreatRange of it in the last
// kNextThreatMs (away from the town), a danger zone (citizens hit there lately); 0: safe, else kNextBlocked*
long EECPNextBuildUnsafe(EEComputerPlayer* inPlayer, long inX, long inY);
// whether the straight way from the town centre to a tile crosses a seen enemy tower's reach (the tower's tile)
long EECPNextRouteTower(EEComputerPlayer* inPlayer, long inX, long inY);
// the unit id of the threat at a tile (0: none) and its kind (kNextBlockedEnemyNear, kNextBlockedTower); inNow: only
// soldiers in sight now, no memory, no towers (a home site)
long EECPNextSiteThreat(EEComputerPlayer* inPlayer, long inX, long inY, bool inNow, long* outKind);
// why the next engine's gatherer finders pass a resource unit over (kNextBlocked*; kNextBlockedNone: they take it);
// outCause: the unit id that blocks it
long EECPNextSiteBlocked(EEComputerPlayer* inPlayer, EEUnit* inUnit, long* outCause);
bool EECPNextIsHome(EEComputerPlayer* inPlayer, EEUnit* inUnit);

struct EECPNextBuildings {      // the player's buildings of one type
    long type;                  // the unit class
    long finished;
    long unfinished;            // foundations and buildings going up
    long queued;                // waiting in the build queues
    long busy;                  // finished ones with something in their production queue
    long queue;                 // entries in their production queues
};

struct EECPNextSoldiers {       // the player's living land soldiers of one class
    long type;
    long count;
    long strength;              // the sum of hit points x attack
};

struct EECPNextThreat {         // seen enemy soldiers within the claim range of the town centre, by player
    long player;
    long count;
    long strength;              // the sum of hit points x attack
    long x, y;                  // the nearest one's tile
    long distance;              // its distance from the town centre in tiles
};

struct EECPNextEnemy {          // what the player has seen of an enemy
    long player;
    long epoch;                 // the epoch it is in (shown in the game's diplomacy screen)
    long seen_army;             // the most soldiers seen at once at the last sighting
    long seen_strength;         // their hit points x attack
    long last_seen;             // the game ms of the last sighting of any of its units (0: never)
    long town_x, town_y;        // the last town centre seen (-1: none)
    // (the army side, docs/AI_LUA_PLAN.md §5 S4)
    long towers;                // its towers seen and not seen destroyed (EECPNextArmy's memory)
    long citizens_seen;         // its citizens and fishing boats in sight now
    long citizen_x, citizen_y;  // the latest place its gatherers were seen with no known tower near (-1: none)
    long army_x, army_y;        // where its soldiers were last seen (-1: never)
};

struct EECPNextClassCost {      // a class the player can make now (its tech is available and not obsolete)
    long type;
    long cost[kNextResources];
    long building;              // 1: a building (build it), 0: a unit (train it)
    long military;              // 1: a military class
    long producer;              // the first class that makes it (-1: none)
    long producers;             // finished producers the player has
    long requested;             // units waiting in the production requests
    long in_production;         // units paid and queued at the producers
    long counts_toward_epoch;   // 1: counts toward the next epoch's SOL
};

enum EECPNextEventKind {
    kNextEventUnitLost = 0,         // a unit of type was lost (count)
    kNextEventProductionLost = 1,   // a lost building's queued unit of type (the classic re-requested it)
    kNextEventAdvanceLost = 2,      // the building researching the epoch advance was lost
    kNextEventNoSite = 3,           // a queued building of type found no site when its builder was free
    kNextEventGroupWants = 4,       // a group asked for a unit of type that does not exist (not trained)
};

struct EECPNextEvent {
    long kind;                  // EECPNextEventKind
    long type;
    long count;
};

struct EECPNextView {
    long player;
    long time;                  // game ms
    long epoch;                 // the current epoch (0: Prehistoric)
    long epoch_start;           // the game ms it began (0 for the start epoch)
    long last_epoch;            // the game's last epoch
    long advancing;             // 1: the epoch advance is queued or under way
    long stock[kNextResources];
    long income[kNextResources];    // gathered over the last 60 s
    long income_total[kNextResources];   // gathered since the game's start (every AddResource: the classic ledger's Spend)
    long reserve[kNextResources];   // the reserve the strategy set
    long gatherers[kNextResources]; // land gatherers per resource (boats not counted)
    long wanted[kNextResources];    // the wanted land gatherers (gather)
    long idle;                  // idle citizens
    long citizens;              // living land citizens
    long boats;                 // fishing boats
    long population, population_cap;
    long town_x, town_y;        // the town centre tile (-1: none)
    long claim_range;           // tiles
    std::vector<EECPNextSite> sites;
    std::vector<std::vector<long> > site_units;   // by site: its resource units with some left, in the centre's order (ids)
    std::vector<EECPNextBuildings> buildings;
    std::vector<EECPNextSoldiers> soldiers;
    long army_count;
    long army_strength;
    std::vector<EECPNextThreat> threats;
    long threat_strength;       // the sum over threats
    std::vector<EECPNextEnemy> enemies;
    long next_epoch_cost[kNextResources];   // all 0 at the last epoch
    long epoch2_cost[kNextResources];       // the epoch after the next (the copper plan holds both before farms); 0 when none
    long sol_missing;           // the next epoch's SOL still missing
    std::vector<EECPNextClassCost> costs;
    std::vector<EECPNextEvent> events;      // since the last step
    long free_soldiers;         // land soldiers the script does not hold
    long free_strength;
    // §8.35
    long hunt;                  // huntable animals seen within EECPNextHuntReach of the town centre, not blocked
    long hunters;               // land citizens hunting (their gather goal is an animal)
    long wood_x, wood_y;        // the forest edge the wood gatherers go to (FindForestTarget; -1: none)
    long wood_distance;         // its distance from the town centre
    long wood_drop;             // 1: a drop site (town centre, settlement) within reach of it
    long wood_ready;            // 1: some forest edge has a finished drop site at hand (§8.36: the wood gatherers go
                                //    only there, EECPNextWoodTarget)
};

void EECPNextBuildView(EEComputerPlayer* inPlayer, EECPNextView& outView);
// lookups in a view (NULL: none)
const EECPNextBuildings* EECPNextFindBuildings(const EECPNextView& inView, long inType);
const EECPNextClassCost* EECPNextFindCost(const EECPNextView& inView, long inType);

// ---------------------------------------------------------------------------------------------------------------
// The verbs. Stage 6 (docs/AI_LUA_VERBS.md): the economy verbs (gather, claim, release, claim_range, build, fish, train,
// reserve, research, improve, advance) are data/ai/lib/verbs.lua's, on the per-unit commands; the group verbs follow.
// the army's step (EECPNextUpdate, before the view): the scouting memory (the enemies' units seen, their gather spots
// and towers, where their armies were)
void EECPNextArmyStep(EEComputerPlayer* inPlayer);
// the view's army fields: free_soldiers, free_strength and the enemies' army fields
void EECPNextArmyView(EEComputerPlayer* inPlayer, EECPNextView& ioView);
// the armed enemy ships the player sees now (x << 16 | y), and whether a tile lies within inRange of one of them: the
// fish finder leaves the fish near them alone (EECPResourceManager::FindFish, the next engine only)
void EECPNextSeenWarships(EEComputerPlayer* inPlayer, std::vector<long>& outTiles);
bool EECPNextNearTiles(const std::vector<long>& inTiles, long inX, long inY, long inRange);
// the citizens' losses in the AI trace (trace on only; nothing the game reads): a hit on a land citizen
// (EEComputerPlayer::DamageTaken; inAttacker the attacker's player id) and a citizen lost (UnitTaken): next.death and
// next.raid lines (EECPNextArmy.cpp)
void EECPNextTraceHit(EEComputerPlayer* inPlayer, long inAttacker, EEUnit* inUnit);
void EECPNextTraceDeath(EEComputerPlayer* inPlayer, EEUnit* inUnit);

// ---------------------------------------------------------------------------------------------------------------
// The per-player state of the next engine (EEComputerPlayer::mNextState; created on its first tick, or by a loaded
// game: EECPNextLoadState). Saved with the computer player from save version 0.252 (EECPNextSaveState).

// one order's result, for the script's next view (view.results)
struct EECPNextOrderResult {
    std::string mVerb;
    long mCode;                 // EECPNextCode
    long mReason;               // EECPNextReason
    long mCount;
};

// what the army remembers of an enemy's place: a gather spot (its citizens or fishing boats seen there) or a tower
struct EECPNextSpot {
    long mPlayer;
    long mX, mY;
    long mTime;                 // game ms last seen
    long mUnit;                 // a tower: its unit id (0: a gather spot)
};

enum EECPNextSeenKind {          // EECPNextSeen::mKind
    kSeenSoldier = 0,
    kSeenCitizen,
    kSeenBoat,
    kSeenTower,
    kSeenBuilding,
    kSeenShip,
};

// an enemy unit the player sees now (EECPNextArmyStep; not saved, rebuilt each step)
struct EECPNextSeen {
    long mUnit, mPlayer, mX, mY, mKind, mStrength, mHitPoints;
    unsigned short mContinent;
};

struct EECPNextState {
    EECPNextState();
    ~EECPNextState();
    EECPNextState(const EECPNextState&) = delete;
    EECPNextState& operator=(const EECPNextState&) = delete;
    long mEpoch;                            // the epoch at the last step, and since when
    unsigned long mEpochStart;
    long mIncomeTotal[kNextResources];      // gathered since the start
    long mIncomeRing[60][kNextResources];   // mIncomeTotal at each of the last 60 steps
    long mIncomeSteps;
    long mSeenArmy[64], mSeenStrength[64];  // the scouting, by player id
    unsigned long mLastSeen[64];
    long mSeenTownX[64], mSeenTownY[64];
    std::vector<EECPNextEvent> mEvents;     // since the last step
    // the army (EECPNextArmy.cpp; saved from the state block's version 2)
    std::vector<EECPNextSpot> mSpots;       // the enemies' gather spots (at most 8 a player)
    std::vector<EECPNextSpot> mTowers;      // the enemies' towers seen and not seen destroyed
    long mSeenCitizens[64];                 // citizens and fishing boats in sight at the last step, by player id
    long mArmyX[64], mArmyY[64];            // where its soldiers were last seen (-1 never)
    std::vector<EECPNextSeen> mSeen;        // (transient) the enemy units in sight this step
    // the gather safety (EECPNextView.cpp; saved from the state block's version 3)
    std::vector<EECPNextSpot> mThreats;     // enemy soldiers seen in the last kNextThreatMs (unit, player, x, y, time)
    long mThreatsTime;                      // game ms of the last scan into mThreats (its entries of that time: in sight)
    std::vector<long> mHome;                // (transient) the start resources' unit ids, sorted (EECPNextUpdateHome)
    std::vector<unsigned short> mForests;   // (transient) the forests nearest the town first (EECPNextForestTarget)
    long mForestsX = -1, mForestsY = -1;    // ... the town tile and the game ms they were sorted at
    unsigned long mForestsTime = 0;
    // the strategy (EECPNextLua.cpp; docs/AI_LUA_PLAN.md §3.2)
    EECPLuaState* mLua;                     // the script's Lua state (NULL until the first step)
    std::string mAI;                        // the AI's id (ee/EECPEngine.h: its manifest entry; saved from 0.253)
    std::string mScript;                    // the module it runs; "" not chosen
    bool mStarted;                          // init ran (a new game) or the memory came back (a loaded game)
    bool mFailed;                           // no AI, or the script did not load: the seat stands idle
    bool mRestore;                          // a loaded game: mRestoreBytes go back into the state at the first step
    std::string mRestoreBytes;              // the saved memory bytes (EECPLuaState::SaveMemory)
    std::vector<EECPNextOrderResult> mResults;   // the last step's orders' results (the next view's results)
    long mErrors;                           // steps that failed (budget, runtime error, bad orders)
    // TEST ONLY (EE_AI_SAVE_CHECK=1): a Lua state restored from the last save's bytes; the next step runs it beside the
    // live one and traces whether both give the same orders and memory (next.savecheck)
    EECPLuaState* mShadow;
};

EECPNextState* EECPNextGetState(EEComputerPlayer* inPlayer);        // created when missing
void EECPNextDeleteState(EEComputerPlayer* inPlayer);
// the hooks of the classic code
bool EECPNextCanAfford(EEComputerPlayer* inPlayer, EETechTreeNode* inNode, long inUnitType, long inCount);
void EECPNextIncome(EEComputerPlayer* inPlayer, long inResource, long inAmount);
void EECPNextEventAdd(const EEComputerPlayer* inPlayer, long inKind, long inType, long inCount);

// ---------------------------------------------------------------------------------------------------------------
// The strategy (EECPNextLua.cpp; docs/AI_LUA_PLAN.md §3.2 "S2 gate as built"): the seat's AI's Lua script
// (its entry in the AI manifest Data/ai/ais.json, ee/EECPEngine.h; Data/ai/<name>.lua) gets the view as a table and returns its orders, which the
// verbs above carry out; their results are in the next step's view. TEST ONLY: EE_AI_NEXT_SCRIPT names the script per
// player (a comma list by player id from player 1, as EE_AI_ENGINE).
void EECPNextStrategyStep(EEComputerPlayer* inPlayer, const EECPNextView& inView);
// a class or a tech by its data id (dbobjects / dbtechtree "id"; "dbobjects_<n>" when the data has no ids) -> -1: none
long EECPNextClassByName(const std::string& inName);
long EECPNextTechByName(const std::string& inName);
std::string EECPNextClassName(long inClass);
std::string EECPNextTechName(long inTech);
// the save (EEPPlayerHelper, from version 0.252): the state that must survive as bytes, and back. Load creates the
// state; a Lua script gets its memory back at the first step (LoadPersonality, RestoreMemory, never init)
bool EECPNextSaveState(EEComputerPlayer* inPlayer, std::string& outBytes);
bool EECPNextLoadState(EEComputerPlayer* inPlayer, const std::string& inBytes);

// the placement searches of the Lua verbs (ai.place; data/ai/lib/verbs.lua, docs/AI_LUA_VERBS.md): ring: the first
// tile in the rings inFirst..inLast round (x, y) where the class can be built (the build manager's CanBuildAt with its
// margin, on the land of (x, y)); tower: the same, none of the player's towers within its range; adjacent: the tile
// nearest the resource units inUnits (the town's land), then nearest the town, rings 1..inLast round (x, y) (outGap:
// its tiles from them); centre: the resource centre inSite's own site (EECPResourceCenter::FindBuildSite); classic: the
// build manager's FindSite for the class; dock: the shore by the nearest fish within kNextDockReach of the town that
// no dock of the player's serves (outGap: the fish's x * 65536 + y)
enum { kNextPlaceRing = 0, kNextPlaceTower, kNextPlaceAdjacent, kNextPlaceCentre, kNextPlaceClassic, kNextPlaceDock };
bool EECPNextPlaceSearch(EEComputerPlayer* inPlayer, long inType, long inMode, long inX, long inY, long inFirst,
    long inLast, const std::vector<long>& inUnits, long inSite, long& outX, long& outY, long& outGap);

#endif
