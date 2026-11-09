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

#include <string>
#include <vector>

class EEComputerPlayer;
class EECPLuaState;
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
    kNextReasonNoTarget,        // a group verb: no place to go (no enemy town seen, nothing left to explore)
};

struct EECPNextResult {
    long mCode;                 // EECPNextCode
    long mReason;               // EECPNextReason
    long mCount;                // what was done or queued (units, buildings, gatherers)
    long mGroup;                // the group verbs: the group's id (0: none)
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
    long explored;              // 1: its tile is visible to the player (EETile::IsVisibleTo)
    long blocked;               // kNextBlocked*: why the classic gatherer finders would pass its first unit over
    long holds[kNextResources]; // its living resource units of each resource (a centre may hold stone and gold: the
                                // finders take any of its units of the resource they look for)
};

enum {                          // EECPNextSite::blocked
    kNextBlockedNone = 0,
    kNextBlockedUnexplored = 1, // not visible (the next engine's finders still send gatherers to a claimed site)
    kNextBlockedEnemyNear = 2,  // an enemy unit near it (BuildManager::HasEnemyNear)
    kNextBlockedDanger = 3,     // a recent danger zone (EECPRMResourceInDanger)
    kNextBlockedArea = 4        // within 10 tiles of a non-enemy player's area (gaia, an ally)
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
    // (the army side, docs/AI_LUA_PLAN.md §5 S4)
    long towers;                // its towers seen and not seen destroyed (EECPNextArmy's memory)
    long citizens_seen;         // its citizens and fishing boats in sight now
    long citizen_x, citizen_y;  // the latest place its gatherers were seen with no known tower near (-1: none)
    long army_x, army_y;        // where its soldiers were last seen (-1: never)
};

// a group of the next engine (EECPNextArmy.cpp): soldiers the strategy took out of the classic attack commander
enum EECPNextTask {
    kNextTaskAttack = 1,        // fight its way to a place (an enemy's town): soldiers, then citizens, towers, buildings
    kNextTaskRaid = 2,          // the enemy's gatherers and fishing boats; away from its towers; no buildings
    kNextTaskDefend = 3,        // hold a place: the enemies within its radius
    kNextTaskRetreat = 4,       // back to the town without fighting; then it defends the town
    kNextTaskScout = 5,         // walk to the unexplored land without fighting (the view's enemies fill from it)
};

enum EECPNextGroupState {
    kNextGroupForming = 0,      // its soldiers gather (at most 20 s)
    kNextGroupMoving = 1,       // on its way
    kNextGroupFighting = 2,     // attacking what it sees
    kNextGroupArrived = 3,      // at its place with nothing to fight (attack, raid, defend)
    kNextGroupDone = 4,         // a scout with nothing left to explore that it can reach
};

struct EECPNextGroupView {
    long id;
    long task;                  // EECPNextTask
    long state;                 // EECPNextGroupState
    long units;
    long strength;              // hit points x attack
    long x, y;                  // the tile at its centre
    long target;                // the player it attacks or raids (0: none)
    long tx, ty;                // its place
    long enemy_count;           // seen enemy soldiers within 12 tiles of its centre
    long enemy_strength;
    long targets;               // seen enemy units it would fight now
    long started;               // the units it had at its largest
    long lost;                  // its units lost
    long created;               // game ms
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
    std::vector<EECPNextGroupView> groups;  // the next engine's groups (EECPNextArmy.cpp)
    long free_soldiers;         // land soldiers in none of them (the classic attack commander keeps these)
    long free_strength;
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
// The groups (EECPNextArmy.cpp; docs/AI_LUA_PLAN.md "S4 as built"). A group verb takes up to n of the free land
// soldiers (in none of the next engine's groups; only the classes named, when any are), the nearest to its place
// first (a raid and a scout: the fastest first), out of the classic attack commander (DetachUnitFromAI), into a new
// group, or into group inGroup when it names one (which then takes the new task). Its units are its own until it is
// disbanded or empty: every step the army moves them and picks their targets from what the player sees.
struct EECPNextGroupOrder {
    long mGroup;                // 0: a new group; else the group to add to and give the task
    long mCount;                // the free soldiers to add (0: none, for a group named)
    long mFrom;                 // 0: the free soldiers; else the soldiers are taken out of this group
    std::vector<long> mClasses; // only these classes (empty: any)
    long mPlayer;               // attack, raid: the target player (0: the first enemy)
    long mX, mY;                // the place (-1: the task's default: the town, the target's town, a gather spot)
    long mRadius;               // defend: the tiles round the place it holds (0: the claim range at the town, else 12)
};
// attack, raid, defend or scout (EECPNextTask)
EECPNextResult EECPNextGroupTask(EEComputerPlayer* inPlayer, long inTask, const EECPNextGroupOrder& inOrder);
// retreat(group): back to the town (or x, y), not fighting on the way; group 0: every group but the scouts
EECPNextResult EECPNextRetreat(EEComputerPlayer* inPlayer, long inGroup, long inX, long inY);
// regroup(group[, x, y]): gather at a place (default: the group's own centre) and hold it (a defence of 8 tiles)
EECPNextResult EECPNextRegroup(EEComputerPlayer* inPlayer, long inGroup, long inX, long inY);
// disband(group): its soldiers go back to the classic attack commander
EECPNextResult EECPNextDisband(EEComputerPlayer* inPlayer, long inGroup);
// defend(x, y, n): a group of up to n soldiers to a tile (the town centre's: x = y = -1)
EECPNextResult EECPNextDefend(EEComputerPlayer* inPlayer, long inX, long inY, long inCount);
// attack(player, n): a group of up to n soldiers against a player
EECPNextResult EECPNextAttack(EEComputerPlayer* inPlayer, long inTarget, long inCount);
// the army's step (EECPNextUpdate, before the view): drops lost units, fights and moves the groups, keeps the
// scouting memory (the enemies' gather spots and towers)
void EECPNextArmyStep(EEComputerPlayer* inPlayer);
// the view's army fields: groups, free_soldiers, free_strength and the enemies' army fields
void EECPNextArmyView(EEComputerPlayer* inPlayer, EECPNextView& ioView);
// the citizens' losses in the AI trace (trace on only; nothing the game reads): a hit on a land citizen
// (EEComputerPlayer::DamageTaken; inAttacker the attacker's player id) and a citizen lost (UnitTaken): next.death and
// next.raid lines (EECPNextArmy.cpp)
void EECPNextTraceHit(EEComputerPlayer* inPlayer, long inAttacker, EEUnit* inUnit);
void EECPNextTraceDeath(EEComputerPlayer* inPlayer, EEUnit* inUnit);
// a unit in one of the next engine's groups (EEComputerPlayer::UnitGoalDone keeps it detached from the classic AI)
bool EECPNextHoldsUnit(const EEComputerPlayer* inPlayer, const EEComplexUnit* inUnit);
const char* EECPNextTaskName(long inTask);         // "attack", "raid", "defend", "retreat", "scout"
const char* EECPNextGroupStateName(long inState);  // "forming", "moving", "fighting", "arrived", "done"

// ---------------------------------------------------------------------------------------------------------------
// The per-player state of the next engine (EEComputerPlayer::mNextState; created on its first tick, or by a loaded
// game: EECPNextLoadState). Saved with the computer player from save version 0.252 (EECPNextSaveState).

struct EECPNextTestMemory {     // the C++ test opening's memory (EECPNextTest.cpp)
    long mLastClaim;            // game ms of the last claim pass
    long mDefended;             // 1: the defend group was sent
    long mAttacked;             // 1: the attack group was sent
    long mStep;
};

// one order's result, for the script's next view (view.results)
struct EECPNextOrderResult {
    std::string mVerb;
    long mCode;                 // EECPNextCode
    long mReason;               // EECPNextReason
    long mCount;
    long mGroup;                // the group verbs: the group's id
};

// a group of the next engine (EECPNextArmy.cpp)
struct EECPNextGroup {
    long mID;
    long mTask;                 // EECPNextTask
    long mState;                // EECPNextGroupState
    long mTarget;               // attack, raid: the target player
    long mX, mY;                // its place
    long mRadius;               // defend: the tiles it holds round its place
    long mCreated, mStateTime;  // game ms: made, and since its state
    long mWayX, mWayY, mWayTime;   // the move it gave last, and when
    long mStarted, mLost;       // its largest unit count, its units lost
    std::vector<long> mUnits;   // unit ids
    std::vector<long> mAims;    // each unit's target (a unit id; 0 none)
    std::vector<long> mSkips;   // a scout: the places it could not reach (x << 16 | y)
};

// what the army remembers of an enemy's place: a gather spot (its citizens or fishing boats seen there) or a tower
struct EECPNextSpot {
    long mPlayer;
    long mX, mY;
    long mTime;                 // game ms last seen
    long mUnit;                 // a tower: its unit id (0: a gather spot)
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
    // the army (EECPNextArmy.cpp; saved from the state block's version 2)
    std::vector<EECPNextGroup> mGroups;
    long mNextGroup;                        // the next group's id
    std::vector<EECPNextSpot> mSpots;       // the enemies' gather spots (at most 8 a player)
    std::vector<EECPNextSpot> mTowers;      // the enemies' towers seen and not seen destroyed
    long mSeenCitizens[64];                 // citizens and fishing boats in sight at the last step, by player id
    long mArmyX[64], mArmyY[64];            // where its soldiers were last seen (-1 never)
    std::vector<EECPNextSeen> mSeen;        // (transient) the enemy units in sight this step
    // the strategy (EECPNextLua.cpp; docs/AI_LUA_PLAN.md §3.2)
    EECPLuaState* mLua;                     // the script's Lua state (NULL until the first step)
    std::string mScript;                    // the module it runs ("c++": the C++ test opening, TEST ONLY); "" not chosen
    bool mStarted;                          // init ran (a new game) or the memory came back (a loaded game)
    bool mFailed;                           // the script did not load: the seat issues no orders
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
long EECPNextFoodPreference(const EEComputerPlayer* inPlayer);
long EECPNextClaimRangeOf(const EEComputerPlayer* inPlayer);

// the test strategy's step (EECPNextTest.cpp)
void EECPNextTestStep(EEComputerPlayer* inPlayer, const EECPNextView& inView);

// ---------------------------------------------------------------------------------------------------------------
// The strategy (EECPNextLua.cpp; docs/AI_LUA_PLAN.md §3.2 "S2 gate as built"): the personality's Lua script
// (aipersonalities.yaml next_script, Data/ai/<name>.lua) gets the view as a table and returns its orders, which the
// verbs above carry out; their results are in the next step's view. TEST ONLY: EE_AI_NEXT_SCRIPT names the script per
// player (a comma list by player id from player 1, as EE_AI_ENGINE); "c++" there runs the C++ test opening.
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

#endif
