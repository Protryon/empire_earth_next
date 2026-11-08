// The new computer player's tactics layer (docs/AI_LUA_PLAN.md §2, stage S1; docs/AI_LUA_AUDIT.md). New in this port
// (no original address).
//
// A computer player runs one of two engines. "classic" is the original's managers. "next" keeps the managers'
// executors (gatherer assignment, the build queue runner and placement, the production runner, the research runner,
// the attack group state machines) and the reflexes (militia, flee, naval flee, repairs, builder upkeep), and takes
// every decision from a strategy through the verbs below. The strategy reads only the view (EECPNextView): integers,
// fog-honest. Stage S1's strategy is the C++ test opening in EECPNextTest.cpp; stage S2 puts Lua behind the same verbs.
//
// Every verb returns an EECPNextResult (done, queued or refused with a reason) and writes one trace line
// (EE_AI_TRACE, manager "next"): `next.order verb=build cls=... near=town result=refused reason=no_site`.
#ifndef EE_EECPNEXT_H
#define EE_EECPNEXT_H

#include <vector>

class EEComputerPlayer;
class EEComplexUnit;
class EEUnit;
class EETechTreeNode;

// The engine switch: the seat's engine (ee/EECPEngine.h, stage S0: the lobby, EE-SIM --engine "next,classic" for p2
// and p3). Every classic-only gate asks this one helper.
bool EECPNextEngine(const EEComputerPlayer* inPlayer);
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
    long resource;              // 0..4: the resource most of its units hold (-1: none left)
    long units;                 // its living resource units of that resource
    long amount;                // what they still hold
    long room;                  // gatherer slots left: units x the mining limit less the gatherers there
    long workers;               // gatherers working its units now
    long x, y;                  // the tile of its first living resource unit
    long distance;              // tiles from the town centre
    long claimed;               // 1: active (gathered from)
    long has_drop;              // 1: a drop site (town centre, settlement; granary for food) at hand: within the rings
                                //    build(settlement, site) places one in (kEECPPatchDropRange + 2), plus its half size
    long has_tower;             // 1: one of the player's towers within 8 tiles
    long danger;                // the seen enemy strength within 12 tiles, plus 1 in a recent danger zone
    long patch;                 // 1: its food is a forage patch
};

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
    std::vector<EECPNextBuildings> buildings;
    std::vector<EECPNextSoldiers> soldiers;
    long army_count;
    long army_strength;
    std::vector<EECPNextThreat> threats;
    long threat_strength;       // the sum over threats
    std::vector<EECPNextEnemy> enemies;
    long next_epoch_cost[kNextResources];   // all 0 at the last epoch
    long sol_missing;           // the next epoch's SOL still missing
    std::vector<EECPNextClassCost> costs;
    std::vector<EECPNextEvent> events;      // since the last step
    long groups;                // the attack commander's groups
};

void EECPNextBuildView(EEComputerPlayer* inPlayer, EECPNextView& outView);
// lookups in a view (NULL: none)
const EECPNextBuildings* EECPNextFindBuildings(const EECPNextView& inView, long inType);
const EECPNextClassCost* EECPNextFindCost(const EECPNextView& inView, long inType);

// ---------------------------------------------------------------------------------------------------------------
// The verbs (the tactics). Each validates its arguments like EEMessage::Execute: a bad order is a logged no-op.

// Where to build: the town (the classic placement, FindSite), a resource centre (the ring round its resource unit;
// a settlement at a forage patch takes EECPRMPatchSite's rings) or a tile (the first free site in the rings round it)
enum EECPNextNear {
    kNextNearTown = 0,
    kNextNearSite = 1,          // inArg1: the site index
    kNextNearTile = 2,          // inArg1, inArg2: the tile
};

// the food order of the gatherer assignment (AssignGatherers)
enum EECPNextFoodPrefer {
    kNextPreferPatch = 0,       // the original order: farms and forage patches, the game near home, then far
    kNextPreferHunt = 1,        // the game near home first, for all the wanted food gatherers
};

// gather(resource, n[, prefer]): n land gatherers wanted on resource 0..4 (food: plus the boats fishing)
EECPNextResult EECPNextGather(EEComputerPlayer* inPlayer, long inResource, long inCount, long inPrefer = kNextPreferPatch);
// claim(site) / release(site): gather from a resource centre or stop (EECPResourceCenter::SetActive)
EECPNextResult EECPNextClaim(EEComputerPlayer* inPlayer, long inSite);
EECPNextResult EECPNextRelease(EEComputerPlayer* inPlayer, long inSite);
// claim_range(tiles): the land the finders and the patch settlements keep to (kEECPClaimRange .. kEECPClaimRangeMax)
EECPNextResult EECPNextClaimRange(EEComputerPlayer* inPlayer, long inTiles);
// build(type, near): one building queued (QueueBuilding / QueueBuildingAt, the build runner places and builds it)
EECPNextResult EECPNextBuild(EEComputerPlayer* inPlayer, long inType, long inNear = kNextNearTown, long inArg1 = 0,
    long inArg2 = 0);
// fish(boats, docks): docks built or queued up to inDocks (at the shore), fishing boats up to inBoats
EECPNextResult EECPNextFish(EEComputerPlayer* inPlayer, long inBoats, long inDocks);
// train(type, n[, town]): n units requested (RequestUnits; the production runner starts them when affordable)
EECPNextResult EECPNextTrain(EEComputerPlayer* inPlayer, long inType, long inCount, long inTown = 0);
// reserve(food, wood, stone, gold, iron): what nothing but the epoch advance may spend
EECPNextResult EECPNextReserve(EEComputerPlayer* inPlayer, long inFood, long inWood, long inStone, long inGold, long inIron);
// research(tech): queued at its producer (TryResearch starts it when affordable)
EECPNextResult EECPNextResearch(EEComputerPlayer* inPlayer, long inTech);
// advance(): the next epoch, only when its SOL is met and it is paid (the town centre researches it at once)
EECPNextResult EECPNextAdvance(EEComputerPlayer* inPlayer);
// defend(x, y, n): a group of up to n soldiers to a tile (the town centre's: x = y = -1)
EECPNextResult EECPNextDefend(EEComputerPlayer* inPlayer, long inX, long inY, long inCount);
// attack(player, n): a group of up to n soldiers against a player
EECPNextResult EECPNextAttack(EEComputerPlayer* inPlayer, long inTarget, long inCount);

// ---------------------------------------------------------------------------------------------------------------
// The per-player state of the next engine (EEComputerPlayer::mNextState; created on its first tick). Not saved yet:
// stage S2 saves it with the strategy's memory.

struct EECPNextTestMemory {     // the C++ test opening's memory (EECPNextTest.cpp)
    long mLastClaim;            // game ms of the last claim pass
    long mDefended;             // 1: the defend group was sent
    long mAttacked;             // 1: the attack group was sent
    long mStep;
};

struct EECPNextState {
    EECPNextState();
    long mClaimRange;                       // tiles (EECPClaimRange)
    long mReserve[kNextResources];
    long mFoodPrefer;                       // EECPNextFoodPrefer
    long mFoodLand;                         // the land food gatherers asked for (gather adds the boats)
    long mEpoch;                            // the epoch at the last step, and since when
    unsigned long mEpochStart;
    unsigned long mNextRepairTime;
    long mIncomeTotal[kNextResources];      // gathered since the start
    long mIncomeRing[60][kNextResources];   // mIncomeTotal at each of the last 60 steps
    long mIncomeSteps;
    long mSeenArmy[64], mSeenStrength[64];  // the scouting, by player id
    unsigned long mLastSeen[64];
    long mSeenTownX[64], mSeenTownY[64];
    std::vector<EECPNextEvent> mEvents;     // since the last step
    EECPNextTestMemory mTest;
};

EECPNextState* EECPNextGetState(EEComputerPlayer* inPlayer);        // created when missing
void EECPNextDeleteState(EEComputerPlayer* inPlayer);
// the hooks of the classic code
bool EECPNextCanAfford(EEComputerPlayer* inPlayer, EETechTreeNode* inNode, long inUnitType, long inCount);
void EECPNextIncome(EEComputerPlayer* inPlayer, long inResource, long inAmount);
void EECPNextEventAdd(const EEComputerPlayer* inPlayer, long inKind, long inType, long inCount);
long EECPNextFoodPreference(const EEComputerPlayer* inPlayer);
long EECPNextClaimRangeOf(const EEComputerPlayer* inPlayer);

// the test strategy's step (EECPNextTest.cpp)
void EECPNextTestStep(EEComputerPlayer* inPlayer, const EECPNextView& inView);

#endif
