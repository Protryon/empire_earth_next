// The new computer player's tactics layer (docs/AI_LUA_PLAN.md §2, stage S1; docs/AI_LUA_AUDIT.md). New in this port
// (no original address).
//
// Every computer player runs this engine (the original's decisions were deleted in stage 5): a Lua strategy (its
// AI's script, ee/EECPEngine.h) reads a view of what the player may know and drives the units through the per-unit
// interface (ee/EECPNextUnits.h). The engine is Rust (ee_sim::ai next_state.rs, next_value.rs, next_game.rs: the
// per-player state, the view, the army's scouting memory, the gather safety, the strategy step, the save); the C++
// keeps the computer player's own bookkeeping, the names, and one host (EECPNext.cpp, ee_ai_next_host) for what the
// world views do not hold.
#ifndef EE_EECPNEXT_H
#define EE_EECPNEXT_H

#include <string>
#include <vector>

class EEComputerPlayer;
class EEComplexUnit;
class EEUnit;
class EETechTreeNode;

// The next engine's tick (EEComputerPlayer::Update calls it instead of the managers' decisions)
void EECPNextUpdate(EEComputerPlayer* inPlayer, unsigned long inTime);

// ---------------------------------------------------------------------------------------------------------------
// Results (the script's view.results; ee_ai names.rs)

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
    kNextReasonNoTarget,        // a group verb: no place to go
    kNextReasonUnsafe,          // build: the site (or the way there from the town) is under an enemy tower or soldiers
    kNextReasonUnexplored,      // build near a site not explored yet
    kNextReasonNoDropoff,       // gather (queued): fewer places by a finished drop site than gatherers wanted
};

const char* EECPNextCodeName(long inCode);       // "done", "queued", "refused"
const char* EECPNextReasonName(long inReason);   // "-", "no_site", "no_producer", ...

// ---------------------------------------------------------------------------------------------------------------
// The gather safety (ee_ai next_game.rs; docs/AI_LUA_PLAN.md §3.2 "blocked"): what keeps gatherers off a resource is a
// real threat to them that the player sees or remembers: enemy soldiers (land military, not citizens), and enemy towers
// or armed buildings in range. The start resources by the town are home: blocked only while enemy soldiers are seen on
// them.

const long kNextResources = 5;

enum {                          // why the gatherer finders pass a resource over
    kNextBlockedNone = 0,
    kNextBlockedUnexplored = 1, // not visible
    kNextBlockedEnemyNear = 2,  // enemy soldiers seen within kNextThreatRange tiles in the last kNextThreatMs
    kNextBlockedDanger = 3,     // a recent danger zone (EECPRMResourceInDanger; not a home site)
    kNextBlockedArea = 4,       // within 10 tiles of a non-enemy player's area (gaia, an ally; not a home site)
    kNextBlockedTower = 5       // an enemy tower or armed building seen (and not seen destroyed) whose range covers it
};

const long kNextThreatRange = 9;            // tiles: seen enemy soldiers this near a resource keep gatherers off it
const long kNextThreatMs = 20000;           // ... remembered this long after they were last seen
const long kNextWoodSlack = 2;              // tiles past a drop site's reach: a tree's (the forest edge)
const long kNextDockReach = 30;             // tiles from the town centre: the fish a dock is built for (§8.35)

// the gatherers sent to a resource unit by the player: their gather goal names it (walking, waiting or working)
long EECPNextAssigned(EEComputerPlayer* inPlayer, EEUnit* inResource);
// whether a resource unit has room for one more of the player's gatherers (the mining limit; 6 to a forage patch)
bool EECPNextHasRoom(EEComputerPlayer* inPlayer, EEUnit* inResource);
// a finished drop site for the resource at hand (a town centre, capitol or settlement; inFood: a granary too) within
// reach of the tile, inSlack tiles further
bool EECPNextDropReady(EEComputerPlayer* inPlayer, long inX, long inY, bool inFood, long inSlack = 0);
// the same for a resource unit, a tile further (a mine or bush's own size)
bool EECPNextUnitDropReady(EEComputerPlayer* inPlayer, EEUnit* inResource);
// whether a building at a tile would put its builders in harm's way; 0: safe, else kNextBlocked*
long EECPNextBuildUnsafe(EEComputerPlayer* inPlayer, long inX, long inY);
// the unit id of the threat at a tile (0: none) and its kind; inNow: only soldiers in sight now, no towers
long EECPNextSiteThreat(EEComputerPlayer* inPlayer, long inX, long inY, bool inNow, long* outKind);
// why the next engine's gatherer finders pass a resource unit over (kNextBlocked*); outCause: the unit id that blocks it
long EECPNextSiteBlocked(EEComputerPlayer* inPlayer, EEUnit* inUnit, long* outCause);

// the citizens' losses in the AI trace (trace on only; nothing the game reads): a hit on a land citizen (inAttacker
// the attacker's player id) and a citizen lost: next.death and next.raid lines
void EECPNextTraceHit(EEComputerPlayer* inPlayer, long inAttacker, EEUnit* inUnit);
void EECPNextTraceDeath(EEComputerPlayer* inPlayer, EEUnit* inUnit);

// ---------------------------------------------------------------------------------------------------------------
// The per-player state (EEComputerPlayer::mNextState: an opaque Rust record, ee_ai next_state.rs; created on its first
// tick, or by a loaded game). Saved with the computer player from save version 0.252 (EECPNextSaveState).

enum EECPNextEventKind {
    kNextEventUnitLost = 0,         // a unit of type was lost (count)
    kNextEventProductionLost = 1,   // a lost building's queued unit of type
    kNextEventAdvanceLost = 2,      // the building researching the epoch advance was lost
    kNextEventNoSite = 3,           // a queued building of type found no site when its builder was free
    kNextEventGroupWants = 4,       // a group asked for a unit of type that does not exist
};

struct EECPNextState;                                                 // (Rust's)
EECPNextState* EECPNextGetState(EEComputerPlayer* inPlayer);        // created when missing
void EECPNextDeleteState(EEComputerPlayer* inPlayer);
std::string EECPNextAI(const EEComputerPlayer* inPlayer);           // the AI's id ("" none or no state)
void EECPNextSetAI(EEComputerPlayer* inPlayer, const std::string& inAI);
// the hooks of the classic code
bool EECPNextCanAfford(EEComputerPlayer* inPlayer, EETechTreeNode* inNode, long inUnitType, long inCount);
void EECPNextIncome(EEComputerPlayer* inPlayer, long inResource, long inAmount);
void EECPNextEventAdd(const EEComputerPlayer* inPlayer, long inKind, long inType, long inCount);

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
