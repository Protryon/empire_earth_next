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

#endif
