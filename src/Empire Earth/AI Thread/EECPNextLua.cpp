// The next engine's strategy: the personality's Lua script behind the verbs (ee/EECPNext.h EECPNextStrategyStep;
// docs/AI_LUA_PLAN.md §3.2 "S2 gate as built"). Each step the view goes to the script as a table (EECPLuaValue), the
// script's orders come back as one, and each order is checked and handed to its verb; the verbs' results are in the
// next step's view. The Lua state is per computer player (EECPNextState::mLua), seeded from the game's seed and the
// player id, reads Data\ai through the data root (TEST ONLY: EE_AI_SCRIPTS first) and prints into the AI trace. Its
// memory and the state the tactics keep go into the save (EECPNextSaveState / EECPNextLoadState). New in this port:
// no original address.
#include <windows.h>
#include <vector>
#include <deque>
#include <map>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GERectangle.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEViewport.h"
#include "lle/GETransformation.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFile.h"
#include "lle/UResourceFile.h"
#include "lle/U2DPoint.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/USingleton.h"

#include "ee/EmpireEarth.h"
#include "ee/EEUserInterface.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EERules.h"
#include "ee/EECPTrace.h"
#include "ee/EEGameLog.h"
#include "ee/EECPLua.h"
#include "ee/EECPNext.h"
#include "EEDataText.h"
#include "platform/PlatFile.h"
#include "platform/PlatSim.h"

static const char* const kNextResourceNames[kNextResources] = {"food", "wood", "stone", "gold", "iron"};
static const unsigned long kNextMaxOrders = 200;     // orders past this many in one step are dropped (traced)

// ---------------------------------------------------------------------------------------------------------------
// Names: classes and techs by their data ids (dbobjects / dbtechtree "id": "citizen", "b_barracks", "inf01_clubman")

struct EECPNextNames {
    std::map<long, std::string> mNames;
    std::map<std::string, long> mKeys;
    std::string mPrefix;        // "dbobjects_": the name of a key without an id (a binary tree), as tools/eedata ids.py
};

static EECPNextNames& EECPNextNamesOf(const char* inTable)
{
    static EECPNextNames sClasses, sTechs;
    static bool sRead = false;
    if (!sRead) {
        sRead = true;
        EEDbRowIds("dbobjects", sClasses.mNames);
        EEDbRowIds("dbtechtree", sTechs.mNames);
        sClasses.mPrefix = "dbobjects_";
        sTechs.mPrefix = "dbtechtree_";
        for (std::map<long, std::string>::iterator it = sClasses.mNames.begin(); it != sClasses.mNames.end(); ++it)
            sClasses.mKeys[it->second] = it->first;
        for (std::map<long, std::string>::iterator it = sTechs.mNames.begin(); it != sTechs.mNames.end(); ++it)
            sTechs.mKeys[it->second] = it->first;
    }
    return strcmp(inTable, "dbobjects") == 0 ? sClasses : sTechs;
}

static std::string EECPNextNameOf(EECPNextNames& inNames, long inKey)
{
    std::map<long, std::string>::iterator it = inNames.mNames.find(inKey);
    if (it != inNames.mNames.end())
        return it->second;
    char theText[32];
    snprintf(theText, sizeof(theText), "%ld", inKey);
    return inNames.mPrefix + theText;
}

static long EECPNextKeyOf(EECPNextNames& inNames, const std::string& inName)
{
    std::map<std::string, long>::iterator it = inNames.mKeys.find(inName);
    if (it != inNames.mKeys.end())
        return it->second;
    // "<table>_<key>": a row without an id in the data (or the binary tree's names)
    if (inName.size() > inNames.mPrefix.size() && inName.compare(0, inNames.mPrefix.size(), inNames.mPrefix) == 0) {
        long theKey = 0;
        for (size_t i = inNames.mPrefix.size(); i < inName.size(); i++) {
            if (inName[i] < '0' || inName[i] > '9' || theKey > 100000000)
                return -1;
            theKey = theKey * 10 + (inName[i] - '0');
        }
        return inNames.mNames.count(theKey) ? -1 : theKey;   // a key with an id goes by its id only
    }
    return -1;
}

long EECPNextClassByName(const std::string& inName)
{
    long theClass = EECPNextKeyOf(EECPNextNamesOf("dbobjects"), inName);
    return theClass >= 0 && (unsigned long)theClass < EEUnitClassCount() && gUnitClassTable[theClass] != NULL ? theClass : -1;
}

long EECPNextTechByName(const std::string& inName)
{
    return EECPNextKeyOf(EECPNextNamesOf("dbtechtree"), inName);
}

std::string EECPNextClassName(long inClass)
{
    return EECPNextNameOf(EECPNextNamesOf("dbobjects"), inClass);
}

std::string EECPNextTechName(long inTech)
{
    return EECPNextNameOf(EECPNextNamesOf("dbtechtree"), inTech);
}

static const char* EECPNextEventKindName(long inKind)
{
    switch (inKind) {
    case kNextEventUnitLost: return "unit_lost";
    case kNextEventProductionLost: return "production_lost";
    case kNextEventAdvanceLost: return "advance_lost";
    case kNextEventNoSite: return "no_site";
    case kNextEventGroupWants: return "group_wants";
    default: return "unknown";
    }
}

// ---------------------------------------------------------------------------------------------------------------
// The view as the script sees it (snake_case; arrays 1-based; per-resource values as maps by resource name; the 0/1
// flags as booleans; classes by name)

static EECPLuaValue EECPNextInt(long inValue)
{
    return EECPLuaValue::Int((int64_t)inValue);
}

static EECPLuaValue EECPNextResources(const long* inValues)
{
    EECPLuaValue theMap = EECPLuaValue::NewMap();
    for (long i = 0; i < kNextResources; i++)
        theMap.Set(kNextResourceNames[i], EECPNextInt(inValues[i]));
    return theMap;
}

static const char* EECPNextResultReasonName(long inReason)
{
    return inReason == kNextReasonNone ? "none" : EECPNextReasonName(inReason);
}

static void EECPNextViewValue(const EECPNextView& inView, const std::vector<EECPNextOrderResult>& inResults,
    EECPLuaValue& outValue)
{
    EECPLuaValue v = EECPLuaValue::NewMap();
    v.Set("player", EECPNextInt(inView.player));
    v.Set("time", EECPNextInt(inView.time));
    v.Set("epoch", EECPNextInt(inView.epoch));
    v.Set("epoch_start", EECPNextInt(inView.epoch_start));
    v.Set("last_epoch", EECPNextInt(inView.last_epoch));
    v.Set("advancing", EECPLuaValue::Bool(inView.advancing != 0));
    v.Set("stock", EECPNextResources(inView.stock));
    v.Set("income", EECPNextResources(inView.income));
    v.Set("reserve", EECPNextResources(inView.reserve));
    v.Set("gatherers", EECPNextResources(inView.gatherers));
    v.Set("wanted", EECPNextResources(inView.wanted));
    v.Set("idle", EECPNextInt(inView.idle));
    v.Set("citizens", EECPNextInt(inView.citizens));
    v.Set("boats", EECPNextInt(inView.boats));
    v.Set("population", EECPNextInt(inView.population));
    v.Set("population_cap", EECPNextInt(inView.population_cap));
    v.Set("town_x", EECPNextInt(inView.town_x));
    v.Set("town_y", EECPNextInt(inView.town_y));
    v.Set("claim_range", EECPNextInt(inView.claim_range));
    v.Set("claim_range_min", EECPNextInt(kEECPClaimRange));       // what claim_range takes (EECPTrace.h)
    v.Set("claim_range_max", EECPNextInt(kEECPClaimRangeMax));
    v.Set("claim_range_step", EECPNextInt(kEECPClaimRangeStep));
    EECPLuaValue theSites = EECPLuaValue::NewArray();
    for (unsigned long k = 0; k < inView.sites.size(); k++) {
        const EECPNextSite& s = inView.sites[k];
        EECPLuaValue e = EECPLuaValue::NewMap();
        e.Set("index", EECPNextInt(s.index + 1));   // its place in view.sites: claim{site = index}
        if (s.resource >= 0 && s.resource < kNextResources)
            e.Set("resource", EECPLuaValue::Str(kNextResourceNames[s.resource]));   // nil: nothing left
        e.Set("units", EECPNextInt(s.units));
        e.Set("amount", EECPNextInt(s.amount));
        e.Set("room", EECPNextInt(s.room));
        e.Set("workers", EECPNextInt(s.workers));
        e.Set("x", EECPNextInt(s.x));
        e.Set("y", EECPNextInt(s.y));
        e.Set("distance", EECPNextInt(s.distance));
        e.Set("claimed", EECPLuaValue::Bool(s.claimed != 0));
        e.Set("has_drop", EECPLuaValue::Bool(s.has_drop != 0));
        e.Set("has_tower", EECPLuaValue::Bool(s.has_tower != 0));
        e.Set("danger", EECPNextInt(s.danger));
        e.Set("patch", EECPLuaValue::Bool(s.patch != 0));
        theSites.Push(e);
    }
    v.Set("sites", theSites);
    EECPLuaValue theBuildings = EECPLuaValue::NewArray();
    for (unsigned long k = 0; k < inView.buildings.size(); k++) {
        const EECPNextBuildings& b = inView.buildings[k];
        EECPLuaValue e = EECPLuaValue::NewMap();
        e.Set("type", EECPLuaValue::Str(EECPNextClassName(b.type)));
        e.Set("finished", EECPNextInt(b.finished));
        e.Set("unfinished", EECPNextInt(b.unfinished));
        e.Set("queued", EECPNextInt(b.queued));
        e.Set("busy", EECPNextInt(b.busy));
        e.Set("queue", EECPNextInt(b.queue));
        theBuildings.Push(e);
    }
    v.Set("buildings", theBuildings);
    EECPLuaValue theSoldiers = EECPLuaValue::NewArray();
    for (unsigned long k = 0; k < inView.soldiers.size(); k++) {
        EECPLuaValue e = EECPLuaValue::NewMap();
        e.Set("type", EECPLuaValue::Str(EECPNextClassName(inView.soldiers[k].type)));
        e.Set("count", EECPNextInt(inView.soldiers[k].count));
        e.Set("strength", EECPNextInt(inView.soldiers[k].strength));
        theSoldiers.Push(e);
    }
    v.Set("soldiers", theSoldiers);
    v.Set("army_count", EECPNextInt(inView.army_count));
    v.Set("army_strength", EECPNextInt(inView.army_strength));
    EECPLuaValue theThreats = EECPLuaValue::NewArray();
    for (unsigned long k = 0; k < inView.threats.size(); k++) {
        const EECPNextThreat& t = inView.threats[k];
        EECPLuaValue e = EECPLuaValue::NewMap();
        e.Set("player", EECPNextInt(t.player));
        e.Set("count", EECPNextInt(t.count));
        e.Set("strength", EECPNextInt(t.strength));
        e.Set("x", EECPNextInt(t.x));
        e.Set("y", EECPNextInt(t.y));
        e.Set("distance", EECPNextInt(t.distance));
        theThreats.Push(e);
    }
    v.Set("threats", theThreats);
    v.Set("threat_strength", EECPNextInt(inView.threat_strength));
    EECPLuaValue theEnemies = EECPLuaValue::NewArray();
    for (unsigned long k = 0; k < inView.enemies.size(); k++) {
        const EECPNextEnemy& n = inView.enemies[k];
        EECPLuaValue e = EECPLuaValue::NewMap();
        e.Set("player", EECPNextInt(n.player));
        e.Set("epoch", EECPNextInt(n.epoch));
        e.Set("seen_army", EECPNextInt(n.seen_army));
        e.Set("seen_strength", EECPNextInt(n.seen_strength));
        e.Set("last_seen", EECPNextInt(n.last_seen));
        e.Set("town_x", EECPNextInt(n.town_x));
        e.Set("town_y", EECPNextInt(n.town_y));
        e.Set("towers", EECPNextInt(n.towers));
        e.Set("citizens_seen", EECPNextInt(n.citizens_seen));
        e.Set("citizen_x", EECPNextInt(n.citizen_x));
        e.Set("citizen_y", EECPNextInt(n.citizen_y));
        e.Set("army_x", EECPNextInt(n.army_x));
        e.Set("army_y", EECPNextInt(n.army_y));
        theEnemies.Push(e);
    }
    v.Set("enemies", theEnemies);
    v.Set("next_epoch_cost", EECPNextResources(inView.next_epoch_cost));
    v.Set("sol_missing", EECPNextInt(inView.sol_missing));
    EECPLuaValue theCosts = EECPLuaValue::NewArray();
    for (unsigned long k = 0; k < inView.costs.size(); k++) {
        const EECPNextClassCost& c = inView.costs[k];
        EECPLuaValue e = EECPLuaValue::NewMap();
        e.Set("type", EECPLuaValue::Str(EECPNextClassName(c.type)));
        e.Set("cost", EECPNextResources(c.cost));
        e.Set("building", EECPLuaValue::Bool(c.building != 0));
        e.Set("military", EECPLuaValue::Bool(c.military != 0));
        if (c.producer >= 0)
            e.Set("producer", EECPLuaValue::Str(EECPNextClassName(c.producer)));   // nil: none
        e.Set("producers", EECPNextInt(c.producers));
        e.Set("requested", EECPNextInt(c.requested));
        e.Set("in_production", EECPNextInt(c.in_production));
        e.Set("counts_toward_epoch", EECPLuaValue::Bool(c.counts_toward_epoch != 0));
        theCosts.Push(e);
    }
    v.Set("costs", theCosts);
    EECPLuaValue theEvents = EECPLuaValue::NewArray();
    for (unsigned long k = 0; k < inView.events.size(); k++) {
        EECPLuaValue e = EECPLuaValue::NewMap();
        e.Set("kind", EECPLuaValue::Str(EECPNextEventKindName(inView.events[k].kind)));
        e.Set("type", EECPLuaValue::Str(EECPNextClassName(inView.events[k].type)));
        e.Set("count", EECPNextInt(inView.events[k].count));
        theEvents.Push(e);
    }
    v.Set("events", theEvents);
    EECPLuaValue theGroups = EECPLuaValue::NewArray();
    for (unsigned long k = 0; k < inView.groups.size(); k++) {
        const EECPNextGroupView& g = inView.groups[k];
        EECPLuaValue e = EECPLuaValue::NewMap();
        e.Set("id", EECPNextInt(g.id));
        e.Set("task", EECPLuaValue::Str(EECPNextTaskName(g.task)));
        e.Set("state", EECPLuaValue::Str(EECPNextGroupStateName(g.state)));
        e.Set("units", EECPNextInt(g.units));
        e.Set("strength", EECPNextInt(g.strength));
        e.Set("x", EECPNextInt(g.x));
        e.Set("y", EECPNextInt(g.y));
        e.Set("target", EECPNextInt(g.target));
        e.Set("tx", EECPNextInt(g.tx));
        e.Set("ty", EECPNextInt(g.ty));
        e.Set("enemy_count", EECPNextInt(g.enemy_count));
        e.Set("enemy_strength", EECPNextInt(g.enemy_strength));
        e.Set("targets", EECPNextInt(g.targets));
        e.Set("started", EECPNextInt(g.started));
        e.Set("lost", EECPNextInt(g.lost));
        e.Set("created", EECPNextInt(g.created));
        theGroups.Push(e);
    }
    v.Set("groups", theGroups);
    v.Set("free_soldiers", EECPNextInt(inView.free_soldiers));
    v.Set("free_strength", EECPNextInt(inView.free_strength));
    EECPLuaValue theResults = EECPLuaValue::NewArray();
    for (unsigned long k = 0; k < inResults.size(); k++) {
        EECPLuaValue e = EECPLuaValue::NewMap();
        e.Set("verb", EECPLuaValue::Str(inResults[k].mVerb));
        e.Set("code", EECPLuaValue::Str(EECPNextCodeName(inResults[k].mCode)));
        e.Set("reason", EECPLuaValue::Str(EECPNextResultReasonName(inResults[k].mReason)));
        e.Set("count", EECPNextInt(inResults[k].mCount));
        if (inResults[k].mGroup != 0)
            e.Set("group", EECPNextInt(inResults[k].mGroup));   // the group verbs
        theResults.Push(e);
    }
    v.Set("results", theResults);
    outValue = v;
}

// ---------------------------------------------------------------------------------------------------------------
// The orders: an array of maps {verb = "...", ...}; each is checked here (names, types, unknown keys) and handed to
// its verb. A malformed order is refused as bad_argument with a next.order trace line like the verbs' own.

struct EECPNextOrderReader {
    const EECPLuaValue& mOrder;
    std::vector<std::string> mUsed;
    std::string mProblem;       // the first problem ("" none)

    explicit EECPNextOrderReader(const EECPLuaValue& inOrder) : mOrder(inOrder) { this->mUsed.push_back("verb"); }

    const EECPLuaValue* Get(const char* inKey)
    {
        this->mUsed.push_back(inKey);
        return this->mOrder.Find(inKey);
    }
    // an integer field; inDefault when absent (inRequired: a problem)
    long Int(const char* inKey, long inDefault, bool inRequired = false)
    {
        const EECPLuaValue* v = this->Get(inKey);
        if (v == NULL || v->IsNil()) {
            if (inRequired && this->mProblem.empty())
                this->mProblem = std::string(inKey) + " missing";
            return inDefault;
        }
        if (v->GetType() != EECPLuaValue::kInt) {
            if (this->mProblem.empty())
                this->mProblem = std::string(inKey) + " not an integer";
            return inDefault;
        }
        int64_t n = v->AsInt();
        if (n < -1000000000LL || n > 1000000000LL) {
            if (this->mProblem.empty())
                this->mProblem = std::string(inKey) + " out of range";
            return inDefault;
        }
        return (long)n;
    }
    std::string Str(const char* inKey, const char* inDefault, bool inRequired = false)
    {
        const EECPLuaValue* v = this->Get(inKey);
        if (v == NULL || v->IsNil()) {
            if (inRequired && this->mProblem.empty())
                this->mProblem = std::string(inKey) + " missing";
            return inDefault;
        }
        if (v->GetType() != EECPLuaValue::kString) {
            if (this->mProblem.empty())
                this->mProblem = std::string(inKey) + " not a string";
            return inDefault;
        }
        return v->AsString();
    }
    // an array of class names (absent or {}: empty); an unknown name is a problem
    std::vector<long> Classes(const char* inKey)
    {
        std::vector<long> theClasses;
        const EECPLuaValue* v = this->Get(inKey);
        if (v == NULL || v->IsNil() || (v->GetType() == EECPLuaValue::kMap && v->FieldCount() == 0))
            return theClasses;
        if (v->GetType() != EECPLuaValue::kArray) {
            if (this->mProblem.empty())
                this->mProblem = std::string(inKey) + " not an array of class names";
            return theClasses;
        }
        for (size_t i = 0; i < v->Size(); i++) {
            const EECPLuaValue& theName = v->At(i);
            long theClass = theName.GetType() == EECPLuaValue::kString ? EECPNextClassByName(theName.AsString()) : -1;
            if (theClass < 0) {
                if (this->mProblem.empty())
                    this->mProblem = std::string(inKey) + ": unknown class "
                        + (theName.GetType() == EECPLuaValue::kString ? theName.AsString() : std::string("(not a string)"));
                return theClasses;
            }
            theClasses.push_back(theClass);
        }
        return theClasses;
    }
    // after the fields: a key nobody read is a problem (a typo stays visible)
    bool Done()
    {
        for (size_t i = 0; i < this->mOrder.FieldCount() && this->mProblem.empty(); i++) {
            const EECPLuaKey& theKey = this->mOrder.KeyAt(i);
            if (!theKey.mIsInt && std::find(this->mUsed.begin(), this->mUsed.end(), theKey.mString) != this->mUsed.end())
                continue;
            this->mProblem = theKey.mIsInt ? "an integer key" : "unknown key " + theKey.mString;
        }
        return this->mProblem.empty();
    }
};

static EECPNextResult EECPNextRefuse(EEComputerPlayer* inPlayer, const std::string& inVerb, const std::string& inWhy)
{
    EECPNextResult theResult = { kNextRefused, kNextReasonBadArgument, 0 };
    EE_CP_TRACE(inPlayer, "next.order", "verb=%s (%s) result=refused reason=bad_argument n=0", inVerb.c_str(), inWhy.c_str());
    return theResult;
}

static long EECPNextResourceIndex(const std::string& inName)
{
    for (long i = 0; i < kNextResources; i++)
        if (inName == kNextResourceNames[i])
            return i;
    return -1;
}

static EECPNextResult EECPNextRunOrder(EEComputerPlayer* inPlayer, const EECPLuaValue& inOrder, std::string& outVerb)
{
    outVerb = "?";
    if (inOrder.GetType() != EECPLuaValue::kMap)
        return EECPNextRefuse(inPlayer, outVerb, "an order is a table {verb = ...}");
    const EECPLuaValue* theVerb = inOrder.Find("verb");
    if (theVerb == NULL || theVerb->GetType() != EECPLuaValue::kString)
        return EECPNextRefuse(inPlayer, outVerb, "no verb");
    outVerb = theVerb->AsString();
    const std::string& v = outVerb;
    EECPNextOrderReader r(inOrder);
    if (v == "gather") {
        std::string theResource = r.Str("resource", "", true);
        long n = r.Int("n", 0, true);
        std::string thePrefer = r.Str("prefer", "patch");
        if (!r.Done())
            return EECPNextRefuse(inPlayer, v, r.mProblem);
        long theIndex = EECPNextResourceIndex(theResource);
        long thePreferKind = thePrefer == "patch" ? kNextPreferPatch : thePrefer == "hunt" ? kNextPreferHunt : -1;
        if (theIndex < 0 || thePreferKind < 0)
            return EECPNextRefuse(inPlayer, v, theIndex < 0 ? "resource " + theResource : "prefer " + thePrefer);
        return EECPNextGather(inPlayer, theIndex, n, thePreferKind);
    }
    if (v == "claim" || v == "release") {
        long theSite = r.Int("site", 0, true);
        if (!r.Done())
            return EECPNextRefuse(inPlayer, v, r.mProblem);
        return v == "claim" ? EECPNextClaim(inPlayer, theSite - 1) : EECPNextRelease(inPlayer, theSite - 1);
    }
    if (v == "claim_range") {
        long theRange = r.Int("range", 0, true);
        if (!r.Done())
            return EECPNextRefuse(inPlayer, v, r.mProblem);
        return EECPNextClaimRange(inPlayer, theRange);
    }
    if (v == "build") {
        std::string theType = r.Str("type", "", true);
        std::string theNear = r.Str("near", "town");
        long theNearKind = theNear == "town" ? kNextNearTown : theNear == "site" ? kNextNearSite : theNear == "tile" ? kNextNearTile : -1;
        long a1 = 0, a2 = 0;
        if (theNearKind == kNextNearSite)
            a1 = r.Int("site", 0, true) - 1;
        else if (theNearKind == kNextNearTile) {
            a1 = r.Int("x", 0, true);
            a2 = r.Int("y", 0, true);
        }
        if (!r.Done())
            return EECPNextRefuse(inPlayer, v, r.mProblem);
        if (theNearKind < 0)
            return EECPNextRefuse(inPlayer, v, "near " + theNear);
        long theClass = EECPNextClassByName(theType);
        if (theClass < 0)
            return EECPNextRefuse(inPlayer, v, "unknown type " + theType);
        return EECPNextBuild(inPlayer, theClass, theNearKind, a1, a2);
    }
    if (v == "fish") {
        long theBoats = r.Int("boats", 0, true);
        long theDocks = r.Int("docks", 1);
        if (!r.Done())
            return EECPNextRefuse(inPlayer, v, r.mProblem);
        return EECPNextFish(inPlayer, theBoats, theDocks);
    }
    if (v == "train") {
        std::string theType = r.Str("type", "", true);
        long n = r.Int("n", 0, true);
        long theTown = r.Int("town", 0);
        if (!r.Done())
            return EECPNextRefuse(inPlayer, v, r.mProblem);
        long theClass = EECPNextClassByName(theType);
        if (theClass < 0)
            return EECPNextRefuse(inPlayer, v, "unknown type " + theType);
        return EECPNextTrain(inPlayer, theClass, n, theTown);
    }
    if (v == "reserve") {
        long theAmounts[kNextResources];
        for (long i = 0; i < kNextResources; i++)
            theAmounts[i] = r.Int(kNextResourceNames[i], 0);
        if (!r.Done())
            return EECPNextRefuse(inPlayer, v, r.mProblem);
        return EECPNextReserve(inPlayer, theAmounts[0], theAmounts[1], theAmounts[2], theAmounts[3], theAmounts[4]);
    }
    if (v == "research") {
        std::string theTech = r.Str("tech", "", true);
        if (!r.Done())
            return EECPNextRefuse(inPlayer, v, r.mProblem);
        long theID = EECPNextTechByName(theTech);
        if (theID < 0)
            return EECPNextRefuse(inPlayer, v, "unknown tech " + theTech);
        return EECPNextResearch(inPlayer, theID);
    }
    if (v == "advance") {
        if (!r.Done())
            return EECPNextRefuse(inPlayer, v, r.mProblem);
        return EECPNextAdvance(inPlayer);
    }
    if (v == "attack" || v == "raid" || v == "defend" || v == "scout") {
        // {verb=, n=, group=, from=, classes={...}, player= (attack, raid), x=, y=, radius= (defend)}
        EECPNextGroupOrder theOrder;
        theOrder.mGroup = r.Int("group", 0);
        theOrder.mFrom = r.Int("from", 0);
        theOrder.mCount = r.Int("n", 0, theOrder.mGroup == 0);
        theOrder.mClasses = r.Classes("classes");
        theOrder.mPlayer = v == "attack" || v == "raid" ? r.Int("player", 0) : 0;
        theOrder.mX = v != "scout" ? r.Int("x", -1) : -1;
        theOrder.mY = v != "scout" ? r.Int("y", -1) : -1;
        theOrder.mRadius = v == "defend" ? r.Int("radius", 0) : 0;
        if (!r.Done())
            return EECPNextRefuse(inPlayer, v, r.mProblem);
        long theTask = v == "attack" ? kNextTaskAttack : v == "raid" ? kNextTaskRaid : v == "defend" ? kNextTaskDefend : kNextTaskScout;
        return EECPNextGroupTask(inPlayer, theTask, theOrder);
    }
    if (v == "retreat" || v == "regroup") {
        long theGroup = r.Int("group", 0, v == "regroup");
        long x = r.Int("x", -1), y = r.Int("y", -1);
        if (!r.Done())
            return EECPNextRefuse(inPlayer, v, r.mProblem);
        return v == "retreat" ? EECPNextRetreat(inPlayer, theGroup, x, y) : EECPNextRegroup(inPlayer, theGroup, x, y);
    }
    if (v == "disband") {
        long theGroup = r.Int("group", 0, true);
        if (!r.Done())
            return EECPNextRefuse(inPlayer, v, r.mProblem);
        return EECPNextDisband(inPlayer, theGroup);
    }
    std::string theUnknown = outVerb;
    outVerb = "?";
    return EECPNextRefuse(inPlayer, outVerb, "unknown verb " + theUnknown);
}

// ---------------------------------------------------------------------------------------------------------------
// The Lua state of a player: its reader, its print, its errors

// TEST ONLY: EE_AI_SCRIPTS=<dir> is read before Data\ai (dir stands for Data\ai; absolute: the game runs in its data
// root), so a simulation can play a script being edited without a new data tree
static const std::string& EECPNextScriptsOverride()
{
    static std::string sDir;
    static bool sRead = false;
    if (!sRead) {
        sRead = true;
        const char* theValue = getenv("EE_AI_SCRIPTS");
        if (theValue != NULL && theValue[0] != '\0') {
            sDir = theValue;
            if (sDir[sDir.size() - 1] != '\\' && sDir[sDir.size() - 1] != '/')
                sDir += '\\';
        }
    }
    return sDir;
}

// the reader: inPath is "ai\<module path>.lua"
static bool EECPNextReadScript(void*, const char* inPath, std::string& outText)
{
    const std::string& theOverride = EECPNextScriptsOverride();
    if (!theOverride.empty() && strncmp(inPath, "ai\\", 3) == 0) {
        std::string thePath = theOverride + (inPath + 3);
#ifndef _WIN32
        std::replace(thePath.begin(), thePath.end(), '\\', '/');
#endif
        FILE* f = PlatFOpen(thePath.c_str(), "rb");
        if (f != NULL) {
            fclose(f);
            return EECPLuaReadFromFolder((void*)"", thePath.c_str(), outText);
        }
    }
    return EECPLuaReadFromFolder((void*)"Data\\", inPath, outText);
}

// one line of trace text: no line breaks or tabs, at most 400 bytes
static std::string EECPNextOneLine(const std::string& inText)
{
    std::string theLine;
    for (size_t i = 0; i < inText.size() && theLine.size() < 400; i++) {
        char c = inText[i];
        if (c == '\r')
            continue;
        if (c == '\n')
            theLine += " | ";
        else
            theLine += c == '\t' ? ' ' : c;
    }
    return theLine;
}

// print(...) from a script: a next.print trace line
static void EECPNextPrint(void* inContext, const char* inText)
{
    EEComputerPlayer* thePlayer = (EEComputerPlayer*)inContext;
    EE_CP_TRACE(thePlayer, "next.print", "%s", EECPNextOneLine(inText != NULL ? inText : "").c_str());
}

// a failed call: a next.error line in the AI trace and the game log; EE-SIM also writes it to stderr (sim_bench
// counts them). The seat issues no orders this step; the reflexes go on.
static void EECPNextError(EEComputerPlayer* inPlayer, const char* inWhat, EECPLuaStatus inStatus, const std::string& inMessage)
{
    std::string theStatus = EECPLuaStatusName(inStatus);
    std::replace(theStatus.begin(), theStatus.end(), ' ', '_');
    std::string theMessage = EECPNextOneLine(inMessage);
    if (inPlayer->mNextState != NULL)
        inPlayer->mNextState->mErrors++;
    if (gCPTraceOn) {
        EECPTraceScope theScope("next");
        EECPTraceOut(inPlayer, "next.error", "in=%s status=%s msg=%s", inWhat, theStatus.c_str(), theMessage.c_str());
    } else if (EEGameLogOn()) {
        char theLine[700];
        snprintf(theLine, sizeof(theLine), "EE-AI %lu p%lu next next.error in=%s status=%s msg=%s\n", gPhysics->mGameTime,
            inPlayer->mPlayerID, inWhat, theStatus.c_str(), theMessage.c_str());
        EEGameLogLine(theLine);
    }
    if (gPlatSimMode) {
        fprintf(stderr, "SIM next.error p%lu t=%lu in=%s status=%s msg=%s\n", inPlayer->mPlayerID, gPhysics->mGameTime,
            inWhat, theStatus.c_str(), theMessage.c_str());
        fflush(stderr);
    }
}

// TEST ONLY: EE_AI_NEXT_SCRIPT, the script per player (a comma list by player id from player 1; "" unset)
static std::string EECPNextScriptOverride(const EEComputerPlayer* inPlayer)
{
    static std::vector<std::string> sScripts;
    static bool sRead = false;
    if (!sRead) {
        sRead = true;
        const char* theValue = getenv("EE_AI_NEXT_SCRIPT");
        std::string theList = theValue != NULL ? theValue : "";
        size_t theStart = 0;
        while (!theList.empty() && theStart <= theList.size()) {
            size_t theEnd = theList.find(',', theStart);
            if (theEnd == std::string::npos)
                theEnd = theList.size();
            sScripts.push_back(theList.substr(theStart, theEnd - theStart));
            theStart = theEnd + 1;
        }
    }
    if (sScripts.empty())
        return std::string();
    unsigned long theID = inPlayer->mPlayerID;
    return sScripts[(theID > 0 ? theID - 1 : 0) % sScripts.size()];
}

// the Lua state, created and loaded at the first step (a loaded game gets its memory back here, never init)
static bool EECPNextOpen(EEComputerPlayer* inPlayer, EECPNextState* inState)
{
    EECPLuaConfig theConfig;
    theConfig.mSeed = EECPLuaSeed((uint32_t)gUserInterface->mRandomSeed, (uint32_t)inPlayer->mPlayerID);
    theConfig.mRead = EECPNextReadScript;
    theConfig.mWrite = EECPNextPrint;
    theConfig.mWriteContext = inPlayer;
    theConfig.mCheckMemory = true;   // a memory the save cannot hold fails the step that made it
    inState->mLua = new EECPLuaState(theConfig);
    if (!inState->mLua->LoadPersonality(inState->mScript.c_str())) {
        EECPNextError(inPlayer, "load", inState->mLua->LastStatus(), inState->mLua->LastError());
        return false;
    }
    if (inState->mRestore) {
        inState->mRestore = false;
        bool theOK = inState->mLua->RestoreMemory(inState->mRestoreBytes);
        inState->mRestoreBytes.clear();
        if (!theOK) {
            EECPNextError(inPlayer, "restore", inState->mLua->LastStatus(), inState->mLua->LastError());
            return false;
        }
        inState->mStarted = true;
        EE_CP_TRACE(inPlayer, "next.script", "name=%s restored=1", inState->mScript.c_str());
    } else
        EE_CP_TRACE(inPlayer, "next.script", "name=%s", inState->mScript.c_str());
    return true;
}

static void EECPNextRunOrders(EEComputerPlayer* inPlayer, EECPNextState* inState, const EECPLuaValue& inOrders,
    const char* inWhat)
{
    if (inOrders.IsNil() || (inOrders.GetType() == EECPLuaValue::kMap && inOrders.FieldCount() == 0))
        return;   // no orders
    if (inOrders.GetType() != EECPLuaValue::kArray) {
        EECPNextError(inPlayer, inWhat, kEECPLuaBadValue, "the orders must be an array of tables {verb = ...}");
        return;
    }
    unsigned long theCount = (unsigned long)inOrders.Size();
    if (theCount > kNextMaxOrders) {
        EE_CP_TRACE(inPlayer, "next.orders", "dropped=%lu (at most %lu a step)", theCount - kNextMaxOrders, kNextMaxOrders);
        theCount = kNextMaxOrders;
    }
    for (unsigned long k = 0; k < theCount; k++) {
        EECPNextOrderResult theEntry;
        EECPNextResult theResult = EECPNextRunOrder(inPlayer, inOrders.At(k), theEntry.mVerb);
        theEntry.mCode = theResult.mCode;
        theEntry.mReason = theResult.mReason;
        theEntry.mCount = theResult.mCount;
        theEntry.mGroup = theResult.mGroup;
        inState->mResults.push_back(theEntry);
    }
}

static void EECPNextShadowStep(EEComputerPlayer* inPlayer, EECPNextState* s, const EECPLuaValue& inView,
    const EECPLuaValue& inOrders);

void EECPNextStrategyStep(EEComputerPlayer* inPlayer, const EECPNextView& inView)
{
    EECPNextState* theState = EECPNextGetState(inPlayer);
    if (theState->mScript.empty()) {
        theState->mScript = EECPNextScriptOverride(inPlayer);
        if (theState->mScript.empty())
            theState->mScript = EECPPersonality(inPlayer).mNextScript;
        if (theState->mScript == "c++")
            EE_CP_TRACE(inPlayer, "next.script", "name=c++ (TEST ONLY: the C++ test opening)");
    }
    if (theState->mScript == "c++") {
        EECPNextTestStep(inPlayer, inView);
        return;
    }
    if (theState->mFailed)
        return;
    if (theState->mLua == NULL && !EECPNextOpen(inPlayer, theState)) {
        theState->mFailed = true;
        return;
    }
    EECPLuaValue theView, theOrders;
    EECPNextViewValue(inView, theState->mResults, theView);
    theState->mResults.clear();
    if (!theState->mStarted) {
        theState->mStarted = true;
        if (!theState->mLua->Init(theView, theOrders))
            EECPNextError(inPlayer, "init", theState->mLua->LastStatus(), theState->mLua->LastError());
        else
            EECPNextRunOrders(inPlayer, theState, theOrders, "init");
    }
    if (!theState->mLua->Step(theView, theOrders)) {
        EECPNextError(inPlayer, "step", theState->mLua->LastStatus(), theState->mLua->LastError());
        return;
    }
    if (theState->mShadow != NULL)
        EECPNextShadowStep(inPlayer, theState, theView, theOrders);   // TEST ONLY (EE_AI_SAVE_CHECK)
    EECPNextRunOrders(inPlayer, theState, theOrders, "step");
}

// ---------------------------------------------------------------------------------------------------------------
// The save (EEPPlayerHelper, from version 0.252): little-endian int32 fields, strings as a uint32 length and bytes.
//   "EENS", uint32 version 2 (1: the same without the army block below)
//   the tactics' state: claim range, reserve[5], food preference, land food gatherers, epoch, epoch start, next repair
//     time, income totals[5], the income ring[60][5], its step count; per player id 0..63: seen army, seen strength,
//     last seen, seen town x, y; the events (count, then kind, type, count each); the C++ test opening's memory (4)
//   the strategy: the script name; started, failed (one byte each); the last step's results (count, then verb, code,
//     reason, count each); the Lua memory bytes (EECPLuaState::SaveMemory; length 0: no Lua state)
// Not saved (rebuilt): the Lua state itself (the modules run again from scratch), the view, the class name tables.

static void EECPNextPut(std::string& ioBytes, long inValue)
{
    uint32_t v = (uint32_t)(int32_t)inValue;
    for (int i = 0; i < 4; i++)
        ioBytes += (char)((v >> (8 * i)) & 0xff);
}

static void EECPNextPutString(std::string& ioBytes, const std::string& inText)
{
    EECPNextPut(ioBytes, (long)inText.size());
    ioBytes += inText;
}

struct EECPNextGetter {
    const std::string& mBytes;
    size_t mOffset;
    bool mOK;
    explicit EECPNextGetter(const std::string& inBytes) : mBytes(inBytes), mOffset(0), mOK(true) {}
    long Get()
    {
        if (!this->mOK || this->mOffset + 4 > this->mBytes.size()) {
            this->mOK = false;
            return 0;
        }
        uint32_t v = 0;
        for (int i = 0; i < 4; i++)
            v |= (uint32_t)(unsigned char)this->mBytes[this->mOffset + i] << (8 * i);
        this->mOffset += 4;
        return (long)(int32_t)v;
    }
    std::string GetString()
    {
        long n = this->Get();
        if (!this->mOK || n < 0 || this->mOffset + (size_t)n > this->mBytes.size()) {
            this->mOK = false;
            return std::string();
        }
        std::string theText = this->mBytes.substr(this->mOffset, (size_t)n);
        this->mOffset += (size_t)n;
        return theText;
    }
    bool GetByte()
    {
        if (!this->mOK || this->mOffset >= this->mBytes.size()) {
            this->mOK = false;
            return false;
        }
        return this->mBytes[this->mOffset++] != 0;
    }
};

// the memory bytes of a state: the restore bytes when it was loaded and has not stepped since, else its Lua memory
// ("" when there is none)
static bool EECPNextMemoryBytes(EEComputerPlayer* inPlayer, EECPNextState* s, std::string& outMemory)
{
    outMemory.clear();
    if (s->mRestore) {
        outMemory = s->mRestoreBytes;   // loaded and saved again before its first step
        return true;
    }
    if (s->mLua != NULL && s->mStarted && !s->mFailed && !s->mLua->SaveMemory(outMemory)) {
        EECPNextError(inPlayer, "save", s->mLua->LastStatus(), s->mLua->LastError());
        outMemory.clear();
        return false;
    }
    return true;
}

// version 2 (S4): the army after the Lua memory: the next group id; the groups (count, then id, task, state, target,
// x, y, radius, created, state time, way x, y, time, started, lost, the units (count, then id and aim each), the
// skips (count, then each)); the gather spots and the towers (count, then player, x, y, time, unit each); per player
// id 0..63: citizens seen, army x, y; the last step's results' group ids (count, then each)
static void EECPNextPutSpots(std::string& ioBytes, const std::vector<EECPNextSpot>& inSpots)
{
    EECPNextPut(ioBytes, (long)inSpots.size());
    for (unsigned long k = 0; k < inSpots.size(); k++) {
        EECPNextPut(ioBytes, inSpots[k].mPlayer);
        EECPNextPut(ioBytes, inSpots[k].mX);
        EECPNextPut(ioBytes, inSpots[k].mY);
        EECPNextPut(ioBytes, inSpots[k].mTime);
        EECPNextPut(ioBytes, inSpots[k].mUnit);
    }
}

static void EECPNextEncodeArmy(const EECPNextState* s, std::string& outBytes)
{
    EECPNextPut(outBytes, s->mNextGroup);
    EECPNextPut(outBytes, (long)s->mGroups.size());
    for (unsigned long g = 0; g < s->mGroups.size(); g++) {
        const EECPNextGroup& theGroup = s->mGroups[g];
        const long theFields[] = {theGroup.mID, theGroup.mTask, theGroup.mState, theGroup.mTarget, theGroup.mX, theGroup.mY,
            theGroup.mRadius, theGroup.mCreated, theGroup.mStateTime, theGroup.mWayX, theGroup.mWayY, theGroup.mWayTime,
            theGroup.mStarted, theGroup.mLost};
        for (unsigned long f = 0; f < sizeof(theFields) / sizeof(theFields[0]); f++)
            EECPNextPut(outBytes, theFields[f]);
        EECPNextPut(outBytes, (long)theGroup.mUnits.size());
        for (unsigned long k = 0; k < theGroup.mUnits.size(); k++) {
            EECPNextPut(outBytes, theGroup.mUnits[k]);
            EECPNextPut(outBytes, theGroup.mAims[k]);
        }
        EECPNextPut(outBytes, (long)theGroup.mSkips.size());
        for (unsigned long k = 0; k < theGroup.mSkips.size(); k++)
            EECPNextPut(outBytes, theGroup.mSkips[k]);
    }
    EECPNextPutSpots(outBytes, s->mSpots);
    EECPNextPutSpots(outBytes, s->mTowers);
    for (long p = 0; p < 64; p++) {
        EECPNextPut(outBytes, s->mSeenCitizens[p]);
        EECPNextPut(outBytes, s->mArmyX[p]);
        EECPNextPut(outBytes, s->mArmyY[p]);
    }
    EECPNextPut(outBytes, (long)s->mResults.size());
    for (unsigned long k = 0; k < s->mResults.size(); k++)
        EECPNextPut(outBytes, s->mResults[k].mGroup);
}

static void EECPNextGetSpots(EECPNextGetter& g, std::vector<EECPNextSpot>& outSpots)
{
    long theCount = g.Get();
    outSpots.clear();
    for (long k = 0; g.mOK && k < theCount; k++) {
        EECPNextSpot theSpot;
        theSpot.mPlayer = g.Get();
        theSpot.mX = g.Get();
        theSpot.mY = g.Get();
        theSpot.mTime = g.Get();
        theSpot.mUnit = g.Get();
        if (k < 64 * 32)
            outSpots.push_back(theSpot);
    }
}

static void EECPNextDecodeArmy(EECPNextState* s, EECPNextGetter& g)
{
    s->mNextGroup = g.Get();
    long theGroups = g.Get();
    s->mGroups.clear();
    for (long n = 0; g.mOK && n < theGroups; n++) {
        EECPNextGroup theGroup;
        long* const theFields[] = {&theGroup.mID, &theGroup.mTask, &theGroup.mState, &theGroup.mTarget, &theGroup.mX,
            &theGroup.mY, &theGroup.mRadius, &theGroup.mCreated, &theGroup.mStateTime, &theGroup.mWayX, &theGroup.mWayY,
            &theGroup.mWayTime, &theGroup.mStarted, &theGroup.mLost};
        for (unsigned long f = 0; f < sizeof(theFields) / sizeof(theFields[0]); f++)
            *theFields[f] = g.Get();
        long theUnits = g.Get();
        for (long k = 0; g.mOK && k < theUnits; k++) {
            theGroup.mUnits.push_back(g.Get());
            theGroup.mAims.push_back(g.Get());
        }
        long theSkips = g.Get();
        for (long k = 0; g.mOK && k < theSkips; k++)
            theGroup.mSkips.push_back(g.Get());
        if (n < 1000)
            s->mGroups.push_back(theGroup);
    }
    EECPNextGetSpots(g, s->mSpots);
    EECPNextGetSpots(g, s->mTowers);
    for (long p = 0; p < 64; p++) {
        s->mSeenCitizens[p] = g.Get();
        s->mArmyX[p] = g.Get();
        s->mArmyY[p] = g.Get();
    }
    long theResults = g.Get();
    for (long k = 0; g.mOK && k < theResults; k++) {
        long theGroup = g.Get();
        if ((unsigned long)k < s->mResults.size())
            s->mResults[k].mGroup = theGroup;
    }
}

static void EECPNextEncodeState(const EECPNextState* s, const std::string& inMemory, std::string& outBytes)
{
    outBytes.clear();
    outBytes.append("EENS", 4);
    EECPNextPut(outBytes, 2);
    EECPNextPut(outBytes, s->mClaimRange);
    for (long i = 0; i < kNextResources; i++)
        EECPNextPut(outBytes, s->mReserve[i]);
    EECPNextPut(outBytes, s->mFoodPrefer);
    EECPNextPut(outBytes, s->mFoodLand);
    EECPNextPut(outBytes, s->mEpoch);
    EECPNextPut(outBytes, (long)s->mEpochStart);
    EECPNextPut(outBytes, (long)s->mNextRepairTime);
    for (long i = 0; i < kNextResources; i++)
        EECPNextPut(outBytes, s->mIncomeTotal[i]);
    for (long k = 0; k < 60; k++)
        for (long i = 0; i < kNextResources; i++)
            EECPNextPut(outBytes, s->mIncomeRing[k][i]);
    EECPNextPut(outBytes, s->mIncomeSteps);
    for (long p = 0; p < 64; p++) {
        EECPNextPut(outBytes, s->mSeenArmy[p]);
        EECPNextPut(outBytes, s->mSeenStrength[p]);
        EECPNextPut(outBytes, (long)s->mLastSeen[p]);
        EECPNextPut(outBytes, s->mSeenTownX[p]);
        EECPNextPut(outBytes, s->mSeenTownY[p]);
    }
    EECPNextPut(outBytes, (long)s->mEvents.size());
    for (unsigned long k = 0; k < s->mEvents.size(); k++) {
        EECPNextPut(outBytes, s->mEvents[k].kind);
        EECPNextPut(outBytes, s->mEvents[k].type);
        EECPNextPut(outBytes, s->mEvents[k].count);
    }
    EECPNextPut(outBytes, s->mTest.mLastClaim);
    EECPNextPut(outBytes, s->mTest.mDefended);
    EECPNextPut(outBytes, s->mTest.mAttacked);
    EECPNextPut(outBytes, s->mTest.mStep);
    EECPNextPutString(outBytes, s->mScript);
    outBytes += (char)(s->mStarted ? 1 : 0);
    outBytes += (char)(s->mFailed ? 1 : 0);
    EECPNextPut(outBytes, (long)s->mResults.size());
    for (unsigned long k = 0; k < s->mResults.size(); k++) {
        EECPNextPutString(outBytes, s->mResults[k].mVerb);
        EECPNextPut(outBytes, s->mResults[k].mCode);
        EECPNextPut(outBytes, s->mResults[k].mReason);
        EECPNextPut(outBytes, s->mResults[k].mCount);
    }
    EECPNextPutString(outBytes, inMemory);
    EECPNextEncodeArmy(s, outBytes);
}

// the state from its bytes (false: not a version 1 block, or bytes left over); the Lua memory goes to mRestoreBytes,
// restored at the first step
static bool EECPNextDecodeState(EECPNextState* s, const std::string& inBytes)
{
    if (inBytes.size() < 8 || inBytes.compare(0, 4, "EENS") != 0)
        return false;
    EECPNextGetter g(inBytes);
    g.mOffset = 4;
    long theVersion = g.Get();
    if (theVersion != 1 && theVersion != 2)
        return false;
    s->mClaimRange = g.Get();
    for (long i = 0; i < kNextResources; i++)
        s->mReserve[i] = g.Get();
    s->mFoodPrefer = g.Get();
    s->mFoodLand = g.Get();
    s->mEpoch = g.Get();
    s->mEpochStart = (unsigned long)(uint32_t)g.Get();
    s->mNextRepairTime = (unsigned long)(uint32_t)g.Get();
    for (long i = 0; i < kNextResources; i++)
        s->mIncomeTotal[i] = g.Get();
    for (long k = 0; k < 60; k++)
        for (long i = 0; i < kNextResources; i++)
            s->mIncomeRing[k][i] = g.Get();
    s->mIncomeSteps = g.Get();
    for (long p = 0; p < 64; p++) {
        s->mSeenArmy[p] = g.Get();
        s->mSeenStrength[p] = g.Get();
        s->mLastSeen[p] = (unsigned long)(uint32_t)g.Get();
        s->mSeenTownX[p] = g.Get();
        s->mSeenTownY[p] = g.Get();
    }
    long theEvents = g.Get();
    s->mEvents.clear();
    for (long k = 0; g.mOK && k < theEvents; k++) {
        EECPNextEvent theEvent;
        theEvent.kind = g.Get();
        theEvent.type = g.Get();
        theEvent.count = g.Get();
        if (k < 256)
            s->mEvents.push_back(theEvent);
    }
    s->mTest.mLastClaim = g.Get();
    s->mTest.mDefended = g.Get();
    s->mTest.mAttacked = g.Get();
    s->mTest.mStep = g.Get();
    s->mScript = g.GetString();
    s->mStarted = g.GetByte();
    s->mFailed = g.GetByte();
    long theResults = g.Get();
    s->mResults.clear();
    for (long k = 0; g.mOK && k < theResults; k++) {
        EECPNextOrderResult theEntry;
        theEntry.mVerb = g.GetString();
        theEntry.mCode = g.Get();
        theEntry.mReason = g.Get();
        theEntry.mCount = g.Get();
        theEntry.mGroup = 0;   // (version 2: from the army block)
        if (k < (long)kNextMaxOrders)
            s->mResults.push_back(theEntry);
    }
    std::string theMemory = g.GetString();
    if (theVersion >= 2)
        EECPNextDecodeArmy(s, g);   // (a version 1 block: no groups, no army memory)
    if (!g.mOK || g.mOffset != inBytes.size())
        return false;
    delete s->mLua;
    s->mLua = NULL;
    s->mRestore = !theMemory.empty();
    s->mRestoreBytes = theMemory;
    return true;
}

static uint32_t EECPNextHash(const std::string& inBytes)   // FNV-1a: the save line's hash of the memory bytes
{
    uint32_t theHash = 2166136261u;
    for (size_t i = 0; i < inBytes.size(); i++)
        theHash = (theHash ^ (unsigned char)inBytes[i]) * 16777619u;
    return theHash;
}

// TEST ONLY (EE_AI_SAVE_CHECK=1): what a load of these bytes would resume with. The bytes decode into a fresh state
// that encodes to the same bytes, and its script, loaded and given the memory back (LoadPersonality, RestoreMemory,
// as a loaded game does), becomes the shadow the next step runs beside the live state (EECPNextShadowStep)
static void EECPNextSaveCheck(EEComputerPlayer* inPlayer, EECPNextState* s, const std::string& inBytes)
{
    static int sOn = -1;
    if (sOn < 0) {
        const char* theValue = getenv("EE_AI_SAVE_CHECK");
        sOn = theValue != NULL && theValue[0] == '1' ? 1 : 0;
    }
    if (!sOn)
        return;
    EECPNextState theCopy;
    std::string theAgain;
    bool theDecoded = EECPNextDecodeState(&theCopy, inBytes);
    if (theDecoded)
        EECPNextEncodeState(&theCopy, theCopy.mRestoreBytes, theAgain);
    bool theRestored = false;
    delete s->mShadow;
    s->mShadow = NULL;
    if (theDecoded && theCopy.mRestore && theCopy.mScript != "c++") {
        EECPLuaConfig theConfig;
        theConfig.mRead = EECPNextReadScript;   // no print: the live state prints
        theConfig.mCheckMemory = true;
        s->mShadow = new EECPLuaState(theConfig);
        theRestored = s->mShadow->LoadPersonality(theCopy.mScript.c_str()) && s->mShadow->RestoreMemory(theCopy.mRestoreBytes);
        if (!theRestored) {
            EECPNextError(inPlayer, "savecheck", s->mShadow->LastStatus(), s->mShadow->LastError());
            delete s->mShadow;
            s->mShadow = NULL;
        }
    }
    EE_CP_TRACE(inPlayer, "next.savecheck", "state=%s restored=%d", theDecoded && theAgain == inBytes ? "same" : "DIFFERENT",
        theRestored ? 1 : 0);
}

// TEST ONLY: the shadow's step on the live step's view: the orders and the memory after it must equal the live ones
static void EECPNextShadowStep(EEComputerPlayer* inPlayer, EECPNextState* s, const EECPLuaValue& inView,
    const EECPLuaValue& inOrders)
{
    EECPLuaState* theShadow = s->mShadow;
    s->mShadow = NULL;
    EECPLuaValue theOrders;
    std::string theLive, theRestored;
    bool theOK = theShadow->Step(inView, theOrders) && theShadow->SaveMemory(theRestored) && s->mLua->SaveMemory(theLive);
    EE_CP_TRACE(inPlayer, "next.savecheck", "step orders=%s memory=%s hash=%08x/%08x", theOK && theOrders == inOrders ? "same" : "DIFFERENT",
        theOK && theLive == theRestored ? "same" : "DIFFERENT", EECPNextHash(theLive), EECPNextHash(theRestored));
    delete theShadow;
}

bool EECPNextSaveState(EEComputerPlayer* inPlayer, std::string& outBytes)
{
    outBytes.clear();
    EECPNextState* s = inPlayer->mNextState;
    if (s == NULL)
        return true;   // nothing yet (the first tick creates it)
    std::string theMemory;
    bool theOK = EECPNextMemoryBytes(inPlayer, s, theMemory);
    EECPNextEncodeState(s, theMemory, outBytes);
    if (gCPTraceOn) {
        EECPTraceScope theScope("next");
        EECPTraceOut(inPlayer, "next.save", "bytes=%lu memory=%lu hash=%08x", (unsigned long)outBytes.size(),
            (unsigned long)theMemory.size(), EECPNextHash(theMemory));
    }
    EECPNextSaveCheck(inPlayer, s, outBytes);
    return theOK;
}

bool EECPNextLoadState(EEComputerPlayer* inPlayer, const std::string& inBytes)
{
    if (inBytes.empty())
        return true;
    if (!EECPNextDecodeState(EECPNextGetState(inPlayer), inBytes))
        return false;
    EECPNextState* s = inPlayer->mNextState;
    if (gCPTraceOn) {
        EECPTraceScope theScope("next");
        EECPTraceOut(inPlayer, "next.load", "bytes=%lu memory=%lu hash=%08x script=%s", (unsigned long)inBytes.size(),
            (unsigned long)s->mRestoreBytes.size(), EECPNextHash(s->mRestoreBytes), s->mScript.c_str());
    }
    return true;
}
