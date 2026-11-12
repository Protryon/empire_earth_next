// New (port): the combat and unit formulas as plain data (docs/RUST_PLAN.md R5b.2): the damage of a hit, the attack
// factor's difficulty scale, the elevation factor, morale, hit points and healing, conversion, the ballistic pitch,
// the projectile's arc and the reach and range tests. The C++ unit methods gather each formula's inputs from the
// world into a record and get the result back; nothing here reads the world.
//
// With EE_SIM_CAPTURE set (ee/EESimCapture.h, tools/sim_capture.py) every call appends one record: the tag, the
// input record's bytes, the output's bytes (every field 4 bytes, no padding). Requires lle/LowLevelEngine.h.
#ifndef EE_EECOMBATFORMULAS_H
#define EE_EECOMBATFORMULAS_H

#include <stdint.h>
#include <string.h>
#include <vector>

#include "rust/ee_rust.h"
#include "ee/EESimCapture.h"

// The records (EeCombat*In, EeCombat*Out) and the formulas are ee_combat's (rust/crates/ee_combat; the field order
// is the capture's layout). The C++ formulas they replace were deleted after the capture equalled theirs.

// ---- the calls (and the capture)

template <class In, class Out> inline void EECombatCapture(unsigned int inTag, const In& inInput, const Out& inOutput)
{
    if (!EESimCaptureRecord::Active())
        return;
    EESimCaptureRecord theRecord(inTag);
    theRecord.Bytes(&inInput, sizeof(In));
    theRecord.Bytes(&inOutput, sizeof(Out));
    theRecord.Commit();
}

#define EE_COMBAT_TAG(a, b, c, d) ((unsigned int)(a) | ((unsigned int)(b) << 8) | ((unsigned int)(c) << 16) | ((unsigned int)(d) << 24))

inline float EECombatAttackFactor(const EeCombatAttackFactorIn& in)
{
    float theOut = ee_combat_attack_factor(&in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'A', 'F'), in, theOut);
    return theOut;
}
inline float EECombatHitFactor(const EeCombatDamageIn& in) { return ee_combat_hit_factor(&in); }
inline EeCombatDamageOut EECombatDamage(const EeCombatDamageIn& in)
{
    EeCombatDamageOut theOut = ee_combat_damage(&in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'D', 'M'), in, theOut);
    return theOut;
}
inline float EECombatElevation(const EeCombatElevationIn& in)
{
    float theOut = ee_combat_elevation(&in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'E', 'L'), in, theOut);
    return theOut;
}
inline int32_t EECombatMorale(const EeCombatMoraleIn& in)
{
    int32_t theOut = ee_combat_morale(&in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'M', 'O'), in, theOut);
    return theOut;
}
inline EeCombatHitPointsOut EECombatHitPoints(const EeCombatHitPointsIn& in)
{
    EeCombatHitPointsOut theOut = ee_combat_hit_points(&in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'T', 'H'), in, theOut);
    return theOut;
}
inline int32_t EECombatDamageEffect(const EeCombatDamageEffectIn& in)
{
    int32_t theOut = ee_combat_damage_effect(&in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'D', 'E'), in, theOut);
    return theOut;
}
inline EeCombatRegenerateOut EECombatRegenerate(const EeCombatRegenerateIn& in)
{
    EeCombatRegenerateOut theOut = ee_combat_regenerate(&in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'R', 'G'), in, theOut);
    return theOut;
}
inline EeCombatTimerOut EECombatTimer(const EeCombatTimerIn& in)
{
    EeCombatTimerOut theOut = ee_combat_timer(&in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'T', 'M'), in, theOut);
    return theOut;
}
inline EeCombatHealOut EECombatHeal(const EeCombatHealIn& in)
{
    EeCombatHealOut theOut = ee_combat_heal(&in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'H', 'L'), in, theOut);
    return theOut;
}
inline EeCombatHealStepOut EECombatHealStep(const EeCombatHealStepIn& in)
{
    EeCombatHealStepOut theOut = ee_combat_heal_step(&in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'H', 'S'), in, theOut);
    return theOut;
}
inline int32_t EECombatConversionCost(const EeCombatConversionCostIn& in)
{
    int32_t theOut = ee_combat_conversion_cost(&in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'C', 'C'), in, theOut);
    return theOut;
}
inline EeCombatConversionOut EECombatConversion(const EeCombatConversionIn& in)
{
    EeCombatConversionOut theOut = ee_combat_conversion(&in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'C', 'V'), in, theOut);
    return theOut;
}
inline EeCombatBallisticOut EECombatBallistic(const EeCombatBallisticIn& in)
{
    EeCombatBallisticOut theOut = ee_combat_ballistic(&in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'B', 'P'), in, theOut);
    return theOut;
}
inline EeCombatArcOut EECombatArc(const EeCombatArcIn& in)
{
    EeCombatArcOut theOut = ee_combat_arc(&in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'P', 'A'), in, theOut);
    return theOut;
}
inline int32_t EECombatRange(const EeCombatRangeIn& in)
{
    int32_t theOut = ee_combat_range(&in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'R', 'H'), in, theOut);
    return theOut;
}
inline int32_t EECombatCanReach(const EeCombatReachIn& in)
{
    int32_t theOut = ee_combat_can_reach(&in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'R', 'C'), in, theOut);
    return theOut;
}
inline int32_t EECombatWorkReach(const EeCombatReachIn& in)
{
    int32_t theOut = ee_combat_work_reach(&in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'R', 'W'), in, theOut);
    return theOut;
}
inline int32_t EECombatGatherReach(const EeCombatReachIn& in)
{
    int32_t theOut = ee_combat_gather_reach(&in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'R', 'T'), in, theOut);
    return theOut;
}
inline int32_t EECombatWithinRange(float inDistanceSquared, float inRange)
{
    int32_t theOut = ee_combat_within_range(inDistanceSquared, inRange);
    float theIn[2] = {inDistanceSquared, inRange};
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'H', 'R'), theIn, theOut);
    return theOut;
}
inline void EECombatDamageRow(const EeCombatDamageRowIn& in, EeCombatDamageRowOut& out)
{
    ee_combat_damage_row(&in, &out);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'D', 'T'), in, out);
}

// ---- the weapon point's other users, the flight steps, the screens (round 4)

// The round 4 records (ee_combat launch.rs, flight.rs, screen.rs; recorded from the unmodified C++ first): CBLS
// (LaunchShot), CBLT (LaunchFromTurret), CBAP (EEApproachAttachPoint): EeCombatLaunchIn, the tag point's model chain
// (16 floats a link), EeCombatLaunchOut. CBPJ, CBPS (EEPMProjectile::Step0, Step2), CBMA, CBMH (EEPMMissile::Accelerate,
// Home): EeCombatFlightIn, EeCombatFlightOut. CBFS: a screen's EeCombatScreenIn and its result; CBHD:
// EEGetHitDistanceSquared's EeCombatHitDistanceIn and the distance.

// A tag point's local point at a frame and its model chain's internal transforms (the point's model first), as
// EETagPoint's walks take them (TagPoint is EETagPoint: ee/EEGraphicObject.h)
template <class TagPoint> inline void EECombatTagPointChain(TagPoint* inPoint, unsigned long inFrame, float* outPoint,
                                                           std::vector<float>& outChain, int32_t& outCount)
{
    const auto& theLocal = inPoint->mModel->GetTagPoint(inPoint->mIndex, inFrame);
    outPoint[0] = theLocal.GetX();
    outPoint[1] = theLocal.GetY();
    outPoint[2] = theLocal.GetZ();
    for (auto theLink = inPoint->mModel; theLink != NULL; theLink = theLink->GetParentModel()) {
        const auto& theStep = theLink->GetInternalTransform(inFrame);
        outChain.insert(outChain.end(), &theStep.mMatrix[0][0], &theStep.mMatrix[0][0] + 16);
        outCount++;
    }
}

template <class In, class Out>
inline void EECombatChainCapture(unsigned int inTag, const In& inInput, const std::vector<float>& inChain, const Out& inOutput)
{
    if (!EESimCaptureRecord::Active())
        return;
    EESimCaptureRecord theRecord(inTag);
    theRecord.Bytes(&inInput, sizeof(In));
    if (!inChain.empty())
        theRecord.F32s(&inChain[0], (unsigned int)inChain.size());
    theRecord.Bytes(&inOutput, sizeof(Out));
    theRecord.Commit();
}

// ---- the impact tests (round 5)

// CBGI (EEUCProjectile/EEUCMissile/EEUCTorpedo::CheckGroundImpact) and CBWI (EEUCProjectile/EEUCMissile::
// CheckWaterImpact): ee_combat's impact.rs decides over EeCombatImpactIn (the reads the C++ gathers; the lazy ones,
// written by the test with their flags, through a host table over inUnit, inClass's CanMoveOnTerrain); the record
// and the outcome are captured. Defined in EEUCMissile.cpp.
class EEUnitClass;
class EEComplexUnit;
EeCombatImpactOut EECombatImpact(EEUnitClass* inClass, EEComplexUnit* inUnit, EeCombatImpactIn& in);

// TEST ONLY until the move (recorded from the unmodified C++ first): CBEW, an effect's start at the shooter's weapon
// tag point (EEE2DLinearProjectile, EEE2DMuzzleFlash, EEE2DSmokeMuzzleBlast, EEE3DPulseCannonBall, EEE3DTracers): the
// site, the shooter's model data at its frame as EeCombatWeaponIn (kind 4: no tag point list, the model's default point
// in `top` under the unscaled location), the model chain, the start the effect computed. Unit is EEComplexUnit, Model
// EEModel, List EETagPointList (templates: this header needs no model header)
template <class Unit, class Model, class List>
inline void EECombatEffectPointCapture(int32_t inSite, Unit* inUnit, Model* inModel, List* inList, bool inHasList,
                                       const GE3DPoint& inStart)
{
    if (!EESimCaptureRecord::Active())
        return;
    EeCombatWeaponIn in = {};
    std::vector<float> theChain;
    memcpy(in.location, inUnit->mPhysicalModel.GetLocation().mMatrix, sizeof(in.location));
    in.list_present = 1;
    in.scale = inUnit->mPhysicalModel.GetScale();
    if (!inHasList) {
        in.kind = 4;
        in.top[0] = inModel->mDefaultPoint.GetX();
        in.top[1] = inModel->mDefaultPoint.GetY();
        in.top[2] = inModel->mDefaultPoint.GetZ();
    } else {
        auto* thePoint = inList->mPoints[inUnit->mWeaponTagPoint];
        if (inModel->GetNumParts() > 0) {
            in.kind = EE_COMBAT_WEAPON_KIND_TURRET;
            unsigned long thePart;
            if (inModel->FindTurretPart(thePoint->mModel, thePart) == 0) {
                in.turret = 1;
                in.yaw = inUnit->mTurretAngles[thePart];
            }
        } else
            in.kind = EE_COMBAT_WEAPON_KIND_POINT;
        EECombatTagPointChain(thePoint, inUnit->mPhysicalModel.GetFrame(), in.point, theChain, in.chain);
    }
    float theStart[3] = {inStart.GetX(), inStart.GetY(), inStart.GetZ()};
    EESimCaptureRecord theRecord(EE_COMBAT_TAG('C', 'B', 'E', 'W'));
    theRecord.I32(inSite);
    theRecord.Bytes(&in, sizeof(in));
    if (!theChain.empty())
        theRecord.F32s(&theChain[0], (unsigned int)theChain.size());
    theRecord.F32s(theStart, 3);
    theRecord.Commit();
}

#endif
