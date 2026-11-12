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

// TEST ONLY until the move (recorded from the unmodified C++ first): CBGI (EEUCProjectile/EEUCMissile/EEUCTorpedo::
// CheckGroundImpact) and CBWI (EEUCProjectile/EEUCMissile::CheckWaterImpact): the inputs the test reads (the lazy reads
// with their flags: the ground height, the target tiles, the same-tile test, the terrain test), the outcome
struct EECombatImpactRecordIn {
    int32_t kind;              // 0 projectile ground, 1 projectile water, 2 missile ground, 3 missile water, 4 torpedo ground
    int32_t bomb;
    int32_t flight_time;
    float start_z;
    float position[3];
    int32_t tile_present;
    float tile_height;
    int32_t has_goal;
    int32_t goal_kind;
    float goal_location[3];
    int32_t target_building;
    int32_t target_tile[2];
    int32_t target_size;
    int32_t unit_tile[2];
    int32_t shell_hole;
    int32_t launcher_cp_base;
    int32_t launcher_missile_base;
    int32_t ground_read;
    float ground_z;
    int32_t on_target_read;
    int32_t on_target;
    int32_t same_tile_read;
    int32_t same_tile;
    int32_t move_read;
    int32_t can_move;
};
struct EECombatImpactRecordOut {
    int32_t result;
    int32_t stage;
    int32_t goal;              // 0 none, 1 attack ground at the ground point, 2 attack ground at the position, z 0
    float goal_point[3];
    int32_t decal;
    int32_t place;
    float offset;
};

#endif
