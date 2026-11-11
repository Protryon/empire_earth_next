// New (port), TEST ONLY: the tech tree's, the event effects' and the calamities' records of the simulation capture
// (ee/EESimCapture.h; tools/sim_capture.py; docs/RUST_PLAN.md R5b.2). With EE_SIM_CAPTURE set, the game appends:
//   TQRS  a research start (EEComplexUnit::QueueResearch): player, building type, tech, front, queue size before/after
//   TTRS  EETechTree::Research: player, id, the tree's epoch before/after, the next epoch's SOL before/after
//   TNRS  a node's ResearchNode / an epoch's Research: the node's state before, the result, the state after
//   TNPR  a node's SetPrerequisite: the state before, the prerequisite id, the state after
//   TSOL  an epoch's AddSOL / UpdateAvailability (kind 0 / 1): the state before and after
//   TEPO  SetEpoch / ResearchToEpoch / SetEpochRange (kind 0 / 1 / 2): the epoch, the tree's range before/after
//   TCST  a node's or epoch's GetCost: player, id, resource, unit class, scale, result
//   TSUM  SumCosts (the outermost call): player, id, unit class, the seven costs before and after
//   EVRN  EEEvent::RunEffect: effect, kind, attribute, player, id, loading, events enabled, result
//   EVAT  ApplyEventAttribute: per unit class the upgrade's numeric fields before and after; per call the player's
//   EVHP  one unit's hit points or flight time rescaled by an effect: what, unit, before, old and new maximum, after
//   CALC  a calamity created (EECalamityManager::Create*Calamity): kind, class, caster, target, x, y, player, result
//   CALS  AddCalamity: class, player, the start result
//   CALU  one calamity's Update: class, player, time alive before, the elapsed time, time alive after, the class's
//         lifetime, the result
//   TSWP  the sweep (every five game minutes): per player and node: availability, obsolescence, costs, SumCosts, the
//         prerequisite closure (DependsOn over every id), and the tree's checksum
//   CALW  one calamity's Update in full: class, the calamity's state before, the elapsed time, its state after, the
//         result, and the world calls the update made in order (EECalamityCaptureEvent: damage, effects, random
//         draws, the classes' world steps)
//   CCAS  a CanCastAt (kind 0: a game call, the outermost one; kind 1: the sweep every five game minutes over every
//         calamity class, each computer player's first living unit and real points and targets): class, caster,
//         location, tile, check, target, result
//   TRTE  ResearchToEpoch: the epoch, the tree's epochs, whether the player is the local one, the epoch steps in
//         order (unlock, research all, research self, research kind 7, show the name), the tree after
//   EVUN  the units an effect walks (line of sight, hit points, flight time, area effects, conversions): per unit its
//         view, whether the effect applies and the value written
// Off (EE_SIM_CAPTURE unset) each hook is one cached test.
#ifndef EE_EETECHCAPTURE_H
#define EE_EETECHCAPTURE_H

#include "ee/EESimCapture.h"

class EETechTreeNode;
class EETechTreeEpoch;
class EETechTree;
class EEUnitClassUpgrade;
class EEPlayer;

constexpr unsigned int EECaptureTag(const char (&inName)[5])
{
    return (unsigned int)(unsigned char)inName[0] | (unsigned int)(unsigned char)inName[1] << 8 |
           (unsigned int)(unsigned char)inName[2] << 16 | (unsigned int)(unsigned char)inName[3] << 24;
}

// a node's state: id, epoch, obsolete epoch, the flags (researched, obsolete, available, editor-obsoleted, allowed),
// the research delay, the prerequisite flags, the tree's current/start/end epoch; an epoch adds its SOL counters
void EETechCaptureNode(EESimCaptureRecord& ioRecord, EETechTreeNode* inNode);
void EETechCaptureEpoch(EESimCaptureRecord& ioRecord, EETechTreeEpoch* inEpoch);
void EETechCaptureTree(EESimCaptureRecord& ioRecord, EETechTree* inTree);
// the upgrade's numeric block (mAreaEffectScale .. the conversion flags; no pointers)
void EETechCaptureUpgrade(EESimCaptureRecord& ioRecord, EEUnitClassUpgrade* inUpgrade);
// the player's fields the effects change (population bonus, tribute fee, mBB44)
void EETechCapturePlayer(EESimCaptureRecord& ioRecord, EEPlayer* inPlayer);
long EETechCapturePlayerIndex(EETechTree* inTree);
// one TCST record (inKind 0: a node's GetCost, 1: an epoch's)
void EETechCaptureCost(EETechTreeNode* inNode, long inKind, long inResource, long inUnitClass, float inScale, long inCost);
// one EVHP record: what (0 hit points, 1 flight time), the unit, its value, the old and new maximum, the new value
void EETechCaptureRescale(long inWhat, unsigned long inUnitID, long inValue, unsigned long inOldMaximum, long inNewMaximum,
                          float inAmount);
// one CALC record: the create call (0 global, 1 location, 2 unit), class, caster, target, x, y, player, result
void EECalamityCapture(long inKind, unsigned long inClassEnum, unsigned long inCaster, unsigned long inTarget, long inX,
                       long inY, unsigned long inPlayer, bool inResult);
void EETechCaptureSweep();
// TEST ONLY (EE-SIM --calamities): the calamities' game. At its k-th call every calamity class whose row is k modulo
// 8 is cast by each computer player through EECalamityManager (as EEUGCastCalamity casts: the player's first living
// unit the caster; a unit class at the next player's first living unit, a location class at its tile), so that
// every class runs in a game the computer players alone never cast one in
void EETechCaptureCastCalamities(long inCall);

class EECalamity;
class EECalamityClass;
class EEComplexUnit;
class EEUnit;
class GE3DPoint;
// CALW: one world call of a calamity's update (an EECalamityCaptureOp, two integers, two floats), collected while
// EECalamityCaptureUpdate runs the update
enum EECalamityCaptureOp {
    kCalamityOpHelperUpdate = 1,    // the contagion helper's Update (id)
    kCalamityOpUnitsNear,           // EEGetUnitsNear (count)
    kCalamityOpTakeHit,             // TakeHit (unit id, damage; player, mode as floats)
    kCalamityOpImmune,              // the unit's immunity effect blocks it (unit id)
    kCalamityOpStrike,              // the hurricane's strike effect on a unit (unit id)
    kCalamityOpCracks,              // the earthquake's cracks
    kCalamityOpRandom,              // a physics random draw (its value)
    kCalamityOpShake,               // the camera shake (angle, amplitude)
    kCalamityOpCreateStorm,         // the hurricane's storm unit
    kCalamityOpLaunchMeteor,
    kCalamityOpCreateVolcano,       // (the effect manager's result)
    kCalamityOpWarpTint,            // the time warp's departure effect
    kCalamityOpWarpDepart,
    kCalamityOpWarpReturn,
    kCalamityOpWarpBlink,
    kCalamityOpHold,                // the teleport's hold
    kCalamityOpTeleport,
    kCalamityOpLogicBombMark,
    kCalamityOpLogicBombDisable,
    kCalamityOpCreateHelper,        // a contagion helper (id)
    kCalamityOpApplyEffect,         // an area effect class's ApplyEffect
    kCalamityOpPlacedEffect,        // a placed effect at the target (x, y as floats)
    kCalamityOpMorale,
    kCalamityOpSpecialAttack,
    kCalamityOpSpecialLocation,
    kCalamityOpDestroyEffect,       // (the handle)
};
void EECalamityCaptureEvent(long inOp, long inA = 0, long inB = 0, float inC = 0.0f, float inD = 0.0f);
// runs inClass->Update(inCalamity, inElapsedTime) and, capturing, writes its CALW record
long EECalamityCaptureUpdate(EECalamityClass* inClass, EECalamity* inCalamity, float inElapsedTime);
// CCAS kind 0: records an outermost CanCastAt's result (the scope counts the nesting of the overrides' calls)
struct EECalamityCanCastScope {
    EECalamityCanCastScope();
    ~EECalamityCanCastScope();
    long Record(EECalamityClass* inClass, EEComplexUnit* inUnit, const GE3DPoint& inLocation, unsigned long inX,
                unsigned long inY, bool inCheck, EEUnit* inTargetUnit, long inResult);
    bool mOutermost;
};
// CCAS kind 1: the sweep (every five game minutes)
void EECalamityCaptureSweep();
// TRTE: one step of ResearchToEpoch (0 unlock, 1 research all, 2 research self, 3 research kind 7, 4 show the name)
void EETechCaptureEpochStep(long inStep, long inEpoch);
// TRTE: the record of the ResearchToEpoch whose steps were collected
void EETechCaptureResearchToEpoch(EETechTree* inTree, long inEpoch, bool inLocal);
// EVUN: one unit an effect walks (collected), and the walk's record (inLoop: 0 sight off, 1 sight on, 2 hit points,
// 3 pending hit points, 4 flight time, 5 pending flight time, 6 area effects, 7 conversion)
void EETechCaptureUnitStep(EEUnit* inUnit, bool inApplies, long inValue);
void EETechCaptureUnitWalk(long inLoop, long inEffect, long inUnitType, long inA = 0, long inB = 0);

// CALT: a calamity's Start (kind 0) or Stop (kind 1): the class, the calamity's state, its target's and caster's
// unit state and the contagion manager's before, the world calls of the start or stop in order
// (EECalamityCaptureStartEvent), the same after, the result
enum EECalamityStartOp {
    kCalamityStartEffect = 1,       // an effect the start creates (its location's x, y as floats; z as the int)
    kCalamityStartSound,            // a sound started (volume, pan as floats)
    kCalamityStartHelper,           // a contagion helper created (id, whether it exists)
    kCalamityStopHelper,            // a contagion helper destroyed (id)
    kCalamityStartDestroyEffect,    // the calamity's effect destroyed (handle)
    kCalamityStartStopSound,        // StopSound / StopSecondarySound (0 / 1)
    kCalamityStartKill,             // the target unit killed (id)
    kCalamityStartStep,             // a class's own world step (its number)
};
void EECalamityCaptureStartEvent(long inOp, long inA = 0, long inB = 0, float inC = 0.0f, float inD = 0.0f);
// a kCalamityStartEffect event: which effect of the class, the location (z as the int's bits)
void EECalamityCaptureStartPoint(long inWhich, float inX, float inY, float inZ);
bool EECalamityCaptureStart(EECalamityClass* inClass, EECalamity* inCalamity);
void EECalamityCaptureStop(EECalamityClass* inClass, EECalamity* inCalamity);

// CONT: one contagion helper's Update: the helper's id, class, timers and units before, the elapsed time, the
// update's world calls in order (EECalamityCaptureContagionEvent: spread candidates with their answers and draws,
// units added, immune, cured, applied, the damage), the timers and units after
class EEContagionHelper;
enum EEContagionCaptureOp {
    kContagionSpread = 1,       // Spread (the elapsed time, the chance)
    kContagionUpdateUnits,      // the functor's UpdateUnits instead of a spread
    kContagionCandidate,        // a near unit (id, the draw or -1; the distance squared or -1)
    kContagionNew,              // a new unit (id; 1 added, 2 immune, 3 held or no complex)
    kContagionApply,            // Apply on a living unit (id)
    kContagionRemove,           // a dead unit removed and cured (id)
    kContagionDamage,           // a functor's TakeHit (unit id, damage)
    kContagionInfect,           // a functor's Infect (unit id)
    kContagionCure,             // a functor's Cure (unit id)
    kContagionFunctorUpdate,    // the functor's Update (its update count)
};
void EECalamityCaptureContagionEvent(long inOp, long inA = 0, long inB = 0, float inC = 0.0f, float inD = 0.0f);
void EECalamityCaptureContagion(EEContagionHelper* inHelper, float inElapsedTime);

// TNWK: one walk over an epoch's nodes or one tree decision (kind: 0 Unlock, 1 ResearchAll, 2 ResearchKind,
// 3 the epoch's ObsoleteOutdatedNodes, 4 the epoch's MakeObsolete, 5 a node's SetObsolete, 6 an epoch's
// GetBuildTime, 7 SetEpoch, 8 AdvanceEpoch, 9 SetNodeAvailable, 10 NeedsUnitMedia): the walk's arguments, per node
// the step (id, action), the node (or the tree's epoch) after, the result. Walks nest (an event a step fires may
// walk again): Begin opens one, the steps go to the innermost, Walk writes and closes it
void EETechCaptureWalkBegin();
void EETechCaptureWalkStep(long inID, long inAction);
void EETechCaptureWalk(long inKind, EETechTree* inTree, EETechTreeNode* inNode, long inA, long inB, long inResult);
#endif
