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

#include "rust/sim/combat.h"
#include "ee/EESimCapture.h"

// The records (EeCombat*In, EeCombat*Out) and the formulas are ee_combat's (rust/crates/ee_sim/src/combat; the field order
// is the capture's layout). The C++ formulas they replace were deleted after the capture equalled theirs.

// ---- the calls (and the capture)

template <class In, class Out> inline void EECombatCapture(unsigned int inTag, const In& inInput, const Out& inOutput)
{
    ee_combat_capture(inTag, (const uint8_t*)&inInput, sizeof(In), (const uint8_t*)&inOutput, sizeof(Out));
}

#define EE_COMBAT_TAG(a, b, c, d) ((unsigned int)(a) | ((unsigned int)(b) << 8) | ((unsigned int)(c) << 16) | ((unsigned int)(d) << 24))

// The formula and its record are one Rust call (ee_sim combat captured.rs). The formulas only Rust calls (the heal
// step, the ballistic pitch, the arc, the range and reach tests) have no C++ name any more.
inline int32_t EECombatDamageEffect(const EeCombatDamageEffectIn& in) { return ee_combat_damage_effect_captured(&in); }
inline EeCombatRegenerateOut EECombatRegenerate(const EeCombatRegenerateIn& in) { return ee_combat_regenerate_captured(&in); }
inline EeCombatTimerOut EECombatTimer(const EeCombatTimerIn& in) { return ee_combat_timer_captured(&in); }
inline EeCombatHealOut EECombatHeal(const EeCombatHealIn& in) { return ee_combat_heal_captured(&in); }
inline int32_t EECombatConversionCost(const EeCombatConversionCostIn& in) { return ee_combat_conversion_cost_captured(&in); }

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
    ee_combat_chain_capture(inTag, (const uint8_t*)&inInput, sizeof(In), inChain.data(), inChain.size(), (const uint8_t*)&inOutput, sizeof(Out));
}

// ---- the impact tests and the effects' weapon points (round 5)

// CBGI (EEUCProjectile/EEUCMissile/EEUCTorpedo::CheckGroundImpact) and CBWI (EEUCProjectile/EEUCMissile::
// CheckWaterImpact): ee_combat's impact.rs decides over EeCombatImpactIn (the reads the C++ gathers; the lazy ones,
// written by the test with their flags, through a host table over inUnit, inClass's CanMoveOnTerrain); the record
// and the outcome are captured. Defined in EEUCMissile.cpp.
class EEUnitClass;
class EEComplexUnit;

// CBEW: an effect's start at the shooter's weapon tag point (EEE2DLinearProjectile, EEE2DMuzzleFlash,
// EEE2DSmokeMuzzleBlast, EEE3DPulseCannonBall, EEE3DTracers): the shooter's model data at its frame as
// EeCombatWeaponIn (EE_COMBAT_WEAPON_KIND_DEFAULT: no tag point list, the model's default point in `top` under the
// unscaled location), ee_combat's weapon_point over the model chain; captured with the site. Unit is EEComplexUnit,
// Model EEModel, List EETagPointList (templates: this header needs no model header)
template <class Unit, class Model, class List>
inline void EECombatEffectWeaponPoint(int32_t inSite, Unit* inUnit, Model* inModel, List* inList, bool inHasList,
                                      GE3DPoint& outStart)
{
    EeCombatWeaponIn in = {};
    std::vector<float> theChain;
    memcpy(in.location, inUnit->mPhysicalModel.GetLocation().mMatrix, sizeof(in.location));
    in.list_present = 1;
    in.scale = inUnit->mPhysicalModel.GetScale();
    if (!inHasList) {
        in.kind = EE_COMBAT_WEAPON_KIND_DEFAULT;
        in.top[0] = inModel->mDefaultPoint.GetX();
        in.top[1] = inModel->mDefaultPoint.GetY();
        in.top[2] = inModel->mDefaultPoint.GetZ();
    } else {
        auto* thePoint = inList->mPoints[inUnit->mWeaponTagPoint];
        if (inModel->GetNumParts() > 0) {
            in.kind = EE_COMBAT_WEAPON_KIND_TURRET;
            unsigned long thePart;
            if (ee_preload_find_turret_part(inModel, thePoint->mModel, &thePart) == 0) {
                in.turret = 1;
                in.yaw = inUnit->mTurretAngles[thePart];
            }
        } else
            in.kind = EE_COMBAT_WEAPON_KIND_POINT;
        EECombatTagPointChain(thePoint, inUnit->mPhysicalModel.GetFrame(), in.point, theChain, in.chain);
    }
    // the point and its CBEW record (the site, the record, the chain, the point): ee_sim combat captured.rs
    float theStart[3];
    ee_combat_effect_weapon_point(inSite, &in, theChain.empty() ? NULL : (const float(*)[16])&theChain[0], theStart);
    outStart = GE3DPoint(theStart[0], theStart[1], theStart[2]);
}

#endif
