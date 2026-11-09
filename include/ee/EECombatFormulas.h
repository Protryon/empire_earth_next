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
#include <math.h>

#include "ee/EESimCapture.h"

// ---- the records (the field order is the capture's layout)

// EEToHitTable::GetAttackFactor
struct EeCombatAttackFactorIn {
    float factor;                   // gDamageTable[the target class's family][the attack kind]
    int32_t attackable;             // the target class CanBeAttacked
    int32_t players_present;        // both players exist
    int32_t attacker_human;         // IsHuman and not player 0
    int32_t target_human;
    int32_t has_rule;               // the game's difficulty has a gameoptions.yaml rule
    float scale_human_target;       // that rule's scales (0 without one)
    float scale_human_attacker;
};

// EEUnitClass::ComputeDamage (kDamageDirect, kDamageSplash, kDamageTrample)
struct EeCombatDamageIn {
    int32_t form;
    float attack;
    float attack_factor;            // EEToHitTable::GetAttackFactor
    float weapon_factor;            // gWeaponToHitFactors[the target class's weapon][the attack's weapon]
    float armor;                    // the target's armour of the attack's weapon
    float elevation;                // GetElevationFactor (1 unless filled: not a trample, a positive hit)
    int32_t fanatic;                // a direct hit by a cloaked attacker (or owner)
    int32_t same_player;
    int32_t allied;                 // the attacker's player is not an enemy of the target's
    int32_t herd;                   // the attacker is a herd animal
    float friendly_factor;          // splash on the own player's units
    int32_t minimum_damage;         // combat.yaml
    float fanatic_factor;
    float elevation_step;
    float ally_factor;
    float herd_ally_factor;
    int32_t trample_damage;
    float trample_threshold;
};
struct EeCombatDamageOut {
    int32_t hit;                    // the hit does damage
    int32_t amount;
};

// EEUnitClass::GetElevationFactor
struct EeCombatElevationIn {
    int32_t applies;                // a combat.yaml elevation attacker against an elevation target
    float own_z;                    // the attacker's height (its launch height for a projectile)
    float target_z;
    float high_height;              // dbworld high_ground_height, low_ground_height (x 1/sqrt(6))
    float low_height;
    float high_bonus;               // the player's high ground bonus
    float high_factor;              // dbworld high_ground_attack_factor, low_ground_attack_factor
    float low_factor;
};

// EEComplexUnit::ApplyMorale
struct EeCombatMoraleIn {
    int32_t value;
    int32_t positive;               // the unit's positive and negative morale counts
    int32_t negative;
    float positive_share;           // combat.yaml
    float negative_share;
};

// EEUnit::TakeHit: the hit points
struct EeCombatHitPointsIn {
    int32_t hit_points;
    int32_t damage;
    int32_t max_hit_points;
};
struct EeCombatHitPointsOut {
    int32_t hit_points;             // at least 0
    int32_t badly_hurt;             // the hit crossed a tenth of the maximum
};

// EEUnit::TakeHit: whether the hit crosses the class's damage effect threshold
struct EeCombatDamageEffectIn {
    int32_t old_hit_points;
    int32_t hit_points;
    int32_t max_hit_points;
    float threshold;                // the effect's second parameter (a share of the maximum)
};

// EEUnitClass::Regenerate
struct EeCombatRegenerateIn {
    int32_t timer;
    int32_t tick_ms;
    int32_t hit_points;
    int32_t regeneration;
    int32_t max_hit_points;
    int32_t interval;               // combat.yaml regeneration_interval
};
struct EeCombatRegenerateOut {
    int32_t timer;
    int32_t hit_points;
    int32_t set;                    // the hit points change (a living unit)
};

// EEUnitClass::HealGarrison's timer
struct EeCombatTimerIn {
    int32_t timer;
    int32_t tick_ms;
    int32_t interval;
};
struct EeCombatTimerOut {
    int32_t timer;
    int32_t due;
};

// EEUnitClass::HealGarrison per unit, EEUGHealUnit::Work
struct EeCombatHealIn {
    int32_t hit_points;
    int32_t healing;
    int32_t max_hit_points;
};
struct EeCombatHealOut {
    int32_t hit_points;             // at most the maximum
    int32_t set;                    // a living unit
};

// EEUGHealUnit::Work's clock
struct EeCombatHealStepIn {
    uint32_t elapsed;
    int32_t tick_ms;
};
struct EeCombatHealStepOut {
    uint32_t elapsed;
    int32_t due;
};

// EEGetConversionCost
struct EeCombatConversionCostIn {
    int32_t cost;                   // dbworld conversion_faith_cost
    int32_t power;                  // the unit's maximum power
};

// EEUCBasicPriest::UpdateConversion, EEUCBuildingPriest::UpdateConversion
struct EeCombatConversionIn {
    int32_t work_timer;
    float tick_seconds;
    float resistance;               // the target's conversion resistance
    int32_t adaptation;             // the adaptation power lowers a town centre's full resistance (filled at 1 only)
    float rate;                     // the priest's conversion rate
    float factor;                   // the target player's conversion factor
};
struct EeCombatConversionOut {
    int32_t timer;                  // the work timer after this tick (no attempt)
    int32_t due;                    // a second has gone: one attempt
    float threshold;                // the attempt converts when the draw is at most this
    int32_t miss_timer;             // the work timer after a failed attempt
};

// EEGetBallisticPitch
struct EeCombatBallisticIn {
    float from_x;
    float from_y;
    float from_z;
    float to_x;
    float to_y;
    float to_z;
    float speed;                    // the launch speed
    float acceleration;             // the projectile class's gravity
    int32_t high;                   // the high arc (the class shoots high, or fires over walls high in animation 8)
};
struct EeCombatBallisticOut {
    int32_t reached;
    float angle;
};

// EEPMProjectile::Step0's arc
struct EeCombatArcIn {
    int32_t flight_time_ms;
    float gravity;
    float pitch;
    int32_t meteor;                 // flies straight at max_velocity, no gravity
    float max_velocity;
    float launcher_speed;           // the launcher class's launch speed (else the projectile's own)
    float own_speed;
};
struct EeCombatArcOut {
    float move_y;                   // the move from the launch point in the yaw's frame (x is 0)
    float move_z;
    float pitch;                    // the nose along the arc
};

// EEIsInRangeToHit's distance test
struct EeCombatRangeIn {
    float range;
    float heal_range;               // a crusader's range
    int32_t crusader;
    float distance_squared;         // EEGetHitDistanceSquared
    int32_t building;               // the target is a land building: half its inner sphere counts
    float inner_sphere;
};

// EECanReach, EEIsInWorkReach, EEIsInGatherReach
struct EeCombatReachIn {
    float distance_squared;
    float radius;
    float other_radius;
    float scale;                    // EECanReach's scale (0 for the work and gather tests)
    int32_t flag_a;                 // work: the target is a wall; gather: the target is a tree
    int32_t flag_b;                 // work: a robotic farm; gather: the unit is a naval citizen
};

// one row of the damage tables (EETerrainTypeTable::LoadDamageTables)
#define EE_COMBAT_DAMAGE_COLUMNS 68
struct EeCombatDamageRowIn {
    int32_t percent[EE_COMBAT_DAMAGE_COLUMNS];
    float scale;                    // percent to fraction (kHeightEpsilon)
    float twin1;                    // dbworld difficulty_damage_scale F, F2
    float twin2;
};
struct EeCombatDamageRowOut {
    float base[EE_COMBAT_DAMAGE_COLUMNS];
    float scaled1[EE_COMBAT_DAMAGE_COLUMNS];
    float scaled2[EE_COMBAT_DAMAGE_COLUMNS];
};

// ---- the C++ formulas (the golden capture's reference; replaced by ee_combat)

inline long EECombatCppCeil(float inValue)
{
    long theResult = (long)lrintf(inValue);
    if (theResult < inValue)
        theResult++;
    return theResult;
}

inline float EECombatCppAttackFactor(const EeCombatAttackFactorIn& in)
{
    float theResult = 0.0f;
    if (in.attackable) {
        if (!in.players_present)
            return 0.0f;
        bool theHuman = in.attacker_human != 0;
        bool theTargetHuman = in.target_human != 0;
        float theFactor = in.factor;
        if (theHuman == theTargetHuman)
            return theFactor;
        if (!in.has_rule)
            return theFactor;
        theResult = (theTargetHuman ? in.scale_human_target : in.scale_human_attacker) * theFactor;
    }
    return theResult;
}

inline float EECombatCppHitFactor(const EeCombatDamageIn& in)
{
    if (in.form == 0 && !(in.attack > 0.0f))
        return 0.0f;
    return in.attack_factor * in.weapon_factor;
}

inline EeCombatDamageOut EECombatCppDamage(const EeCombatDamageIn& in)
{
    EeCombatDamageOut theOut = {0, 0};
    float theHit = EECombatCppHitFactor(in);
    if (in.form == 2) {
        float theDamage = in.attack;
        if (theHit > 0.0f)
            theDamage = theDamage - lrintf(in.armor);
        if (!(theHit * theDamage > in.trample_threshold))
            return theOut;
        theOut.hit = 1;
        theOut.amount = in.trample_damage;
        return theOut;
    }
    if (theHit <= 0.0f)
        return theOut;
    float theDamage = theHit * in.attack;
    if (in.form == 0 && in.fanatic)
        theDamage = in.fanatic_factor * theDamage;
    theDamage = theDamage - in.armor;
    float theModifier = in.elevation;
    if (theModifier != 1.0f) {
        float theStep = in.elevation_step;
        if (theModifier > 1.0f) {
            theModifier = theModifier * theDamage;
            if (theModifier >= theDamage + theStep)
                theDamage = theModifier;
            else
                theDamage = theDamage + theStep;
        } else if (theModifier < 1.0f) {
            theModifier = theModifier * theDamage;
            if (theModifier <= theDamage - theStep)
                theDamage = theModifier;
            else
                theDamage = theDamage - theStep;
        }
    }
    if (in.form == 1 && in.same_player)
        theDamage = in.friendly_factor * theDamage;
    long theAmount = lrintf(theDamage);
    if (theAmount < in.minimum_damage)
        theAmount = in.minimum_damage;
    if (in.form == 0 && (in.same_player || in.allied))
        theAmount = EECombatCppCeil(theAmount * (in.herd ? in.herd_ally_factor : in.ally_factor));
    theOut.hit = 1;
    theOut.amount = (int32_t)theAmount;
    return theOut;
}

inline float EECombatCppElevation(const EeCombatElevationIn& in)
{
    static const float k447ADC = 0.40824828f;   // 1 / sqrt(6)
    if (!in.applies)
        return 1.0f;
    float theDifference = in.own_z - in.target_z;
    if (theDifference >= in.high_height * k447ADC)
        return in.high_bonus + in.high_factor;
    if (theDifference <= -(in.low_height * k447ADC))
        return in.low_factor;
    return 1.0f;
}

inline int32_t EECombatCppMorale(const EeCombatMoraleIn& in)
{
    long theValue = in.value;
    if (in.positive > 0)
        theValue -= lrintf(in.value * in.positive_share);
    if (in.negative > 0)
        theValue += lrintf(in.value * in.negative_share);
    return (int32_t)theValue;
}

inline EeCombatHitPointsOut EECombatCppHitPoints(const EeCombatHitPointsIn& in)
{
    EeCombatHitPointsOut theOut;
    long theHitPoints = in.hit_points;
    float theThreshold = (float)in.max_hit_points * 0.1f;
    theOut.badly_hurt = (float)theHitPoints > theThreshold && (float)(theHitPoints - in.damage) <= theThreshold;
    theHitPoints -= in.damage;
    if (theHitPoints < 0)
        theHitPoints = 0;
    theOut.hit_points = (int32_t)theHitPoints;
    return theOut;
}

inline int32_t EECombatCppDamageEffect(const EeCombatDamageEffectIn& in)
{
    float theInverseHealth = 1.0f / (float)in.max_hit_points;
    return (float)in.old_hit_points * theInverseHealth >= in.threshold &&
           in.threshold > (float)in.hit_points * theInverseHealth;
}

inline EeCombatRegenerateOut EECombatCppRegenerate(const EeCombatRegenerateIn& in)
{
    EeCombatRegenerateOut theOut = {0, in.hit_points, 0};
    long theTime = in.timer - in.tick_ms;
    if (theTime <= 0) {
        long theHitPoints = in.regeneration + in.hit_points;
        if (theHitPoints > in.max_hit_points)
            theHitPoints = in.max_hit_points;
        if (in.hit_points > 0 && theHitPoints > 0) {
            theOut.hit_points = (int32_t)theHitPoints;
            theOut.set = 1;
        }
        theTime = in.interval;
    }
    theOut.timer = (int32_t)theTime;
    return theOut;
}

inline EeCombatTimerOut EECombatCppTimer(const EeCombatTimerIn& in)
{
    EeCombatTimerOut theOut = {in.timer - in.tick_ms, 0};
    if (theOut.timer <= 0) {
        theOut.due = 1;
        theOut.timer = in.interval;
    }
    return theOut;
}

inline EeCombatHealOut EECombatCppHeal(const EeCombatHealIn& in)
{
    EeCombatHealOut theOut;
    long theHitPoints = in.healing + in.hit_points;
    if (theHitPoints > in.max_hit_points)
        theHitPoints = in.max_hit_points;
    theOut.hit_points = (int32_t)theHitPoints;
    theOut.set = in.hit_points > 0;
    return theOut;
}

inline EeCombatHealStepOut EECombatCppHealStep(const EeCombatHealStepIn& in)
{
    EeCombatHealStepOut theOut;
    uint32_t theElapsed = (uint32_t)in.tick_ms;
    if (theElapsed == 0)
        theElapsed = 1;
    theOut.elapsed = in.elapsed + theElapsed;
    theOut.due = theOut.elapsed > 1000;
    if (theOut.due)
        theOut.elapsed -= 1000;
    return theOut;
}

inline int32_t EECombatCppConversionCost(const EeCombatConversionCostIn& in)
{
    int32_t theCost = in.cost < in.power ? in.cost : in.power;
    return theCost > 0 ? theCost : 0;
}

inline EeCombatConversionOut EECombatCppConversion(const EeCombatConversionIn& in)
{
    EeCombatConversionOut theOut = {0, 0, 0.0f, 0};
    long theTime = in.work_timer - (long)(in.tick_seconds * -1000.0f);
    theOut.timer = (int32_t)theTime;
    if ((uint32_t)theTime >= 1000) {
        float theResistance = in.resistance;
        if (theResistance == 1.0f && in.adaptation)
            theResistance = 0.99f;
        theOut.due = 1;
        theOut.threshold = (1.0f - theResistance) * in.factor * in.rate;
        theOut.miss_timer = (int32_t)(theTime - 1000);
    }
    return theOut;
}

inline EeCombatBallisticOut EECombatCppBallistic(const EeCombatBallisticIn& in)
{
    EeCombatBallisticOut theOut;
    float theDistance = (float)sqrt((in.from_x - in.to_x) * (in.from_x - in.to_x) + (in.from_y - in.to_y) * (in.from_y - in.to_y));
    float theSpeed = in.speed;
    float theA = -(in.acceleration * (theDistance * theDistance)) / (theSpeed * theSpeed * 2.0f);
    float theDiscriminant = theDistance * theDistance - (theA - (in.to_z - in.from_z)) * theA * 4.0f;
    if (theDiscriminant >= 0.0f) {
        float theRoot = (float)sqrt(theDiscriminant);
        float theLow = UMArcTan((theRoot - theDistance) / (theA + theA));
        float theHigh = UMArcTan((-theDistance - theRoot) / (theA + theA));
        if (theLow > theHigh) {
            float theSwap = theLow;
            theLow = theHigh;
            theHigh = theSwap;
        }
        theOut.angle = in.high ? theHigh : theLow;
        theOut.reached = 1;
    } else {
        theOut.reached = 0;
        theOut.angle = 0.7853982f;
    }
    return theOut;
}

inline EeCombatArcOut EECombatCppArc(const EeCombatArcIn& in)
{
    EeCombatArcOut theOut;
    float theTime = in.flight_time_ms * 0.001f;
    float theGravity = in.gravity;
    float theCos = UMCos(in.pitch);
    float theSin = UMCos(1.5707964f - in.pitch);
    float theSpeed;
    if (in.meteor) {
        theSpeed = in.max_velocity;
        theGravity = 0.0f;
    } else {
        theSpeed = in.launcher_speed;
        if (!(theSpeed > 0.001f))
            theSpeed = in.own_speed;
    }
    float theHorizontal = theSpeed * theCos;
    theOut.move_y = theHorizontal * theTime;
    float theVertical = theSpeed * theSin;
    theOut.move_z = theVertical * theTime - (theTime * theTime) * theGravity * 0.5f;
    float theY = theHorizontal;
    float theZ = theVertical - theTime * theGravity;
    float theMagnitude = (float)sqrt(0.0f * 0.0f + theY * theY + theZ * theZ);
    if (theMagnitude >= -0.0001f && theMagnitude <= 0.0001f)
        theZ = 0.0f;
    else
        theZ = theZ * (1.0f / theMagnitude);
    theOut.pitch = UMArcCos(-theZ) - 1.5707964f;
    return theOut;
}

inline int32_t EECombatCppRange(const EeCombatRangeIn& in)
{
    float theRange = in.range * in.range;
    if (in.crusader)
        theRange = in.heal_range * in.heal_range;
    if (!in.building)
        return in.distance_squared <= theRange;
    float theHalf = in.inner_sphere * 0.5f;
    return -(theHalf * theHalf) + in.distance_squared <= theRange;
}

static const float kEECombatContactMargin = 1.42f;     // 0x449b7c
static const float kEECombatWorkReach = 1.5f;          // 0x449b80
static const float kEECombatGatherReach = 1.42f;       // 0x449b84

inline int32_t EECombatCppCanReach(const EeCombatReachIn& in)
{
    float theReach = (kEECombatContactMargin * 0.05f + in.radius + in.other_radius) * in.scale;
    return in.distance_squared * 4.0f < theReach * theReach;
}

inline int32_t EECombatCppWorkReach(const EeCombatReachIn& in)
{
    float theTargetRadius = in.other_radius;
    if (in.flag_a) {
        theTargetRadius = 1.0f;
    } else {
        if (in.flag_b)
            theTargetRadius = 2.0f;
        else if (theTargetRadius < 1.0f)
            theTargetRadius = 1.0f;
    }
    float theReach = (kEECombatContactMargin * 0.05f + in.radius + theTargetRadius) * kEECombatWorkReach;
    return in.distance_squared * 4.0f < theReach * theReach;
}

inline int32_t EECombatCppGatherReach(const EeCombatReachIn& in)
{
    float theRadius = in.radius;
    float theTargetRadius = in.other_radius;
    if (theTargetRadius < theRadius)
        theTargetRadius = theRadius;
    if (in.flag_a)
        theTargetRadius = 1.0f;
    if (in.flag_b)
        theRadius = 1.0f;
    float theReach = (kEECombatContactMargin * 0.05f + theTargetRadius + theRadius) * kEECombatGatherReach;
    return in.distance_squared * 4.0f < theReach * theReach;
}

inline int32_t EECombatCppWithinRange(float inDistanceSquared, float inRange)
{
    return inDistanceSquared <= inRange * inRange;
}

inline void EECombatCppDamageRow(const EeCombatDamageRowIn& in, EeCombatDamageRowOut& out)
{
    for (int j = 0; j < EE_COMBAT_DAMAGE_COLUMNS; j++)
        out.base[j] = in.percent[j] * in.scale;
    for (int j = 0; j < EE_COMBAT_DAMAGE_COLUMNS; j++) {
        out.scaled1[j] = in.twin1 * out.base[j];
        out.scaled2[j] = in.twin2 * out.base[j];
    }
}

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
    float theOut = EECombatCppAttackFactor(in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'A', 'F'), in, theOut);
    return theOut;
}
inline float EECombatHitFactor(const EeCombatDamageIn& in) { return EECombatCppHitFactor(in); }
inline EeCombatDamageOut EECombatDamage(const EeCombatDamageIn& in)
{
    EeCombatDamageOut theOut = EECombatCppDamage(in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'D', 'M'), in, theOut);
    return theOut;
}
inline float EECombatElevation(const EeCombatElevationIn& in)
{
    float theOut = EECombatCppElevation(in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'E', 'L'), in, theOut);
    return theOut;
}
inline int32_t EECombatMorale(const EeCombatMoraleIn& in)
{
    int32_t theOut = EECombatCppMorale(in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'M', 'O'), in, theOut);
    return theOut;
}
inline EeCombatHitPointsOut EECombatHitPoints(const EeCombatHitPointsIn& in)
{
    EeCombatHitPointsOut theOut = EECombatCppHitPoints(in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'T', 'H'), in, theOut);
    return theOut;
}
inline int32_t EECombatDamageEffect(const EeCombatDamageEffectIn& in)
{
    int32_t theOut = EECombatCppDamageEffect(in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'D', 'E'), in, theOut);
    return theOut;
}
inline EeCombatRegenerateOut EECombatRegenerate(const EeCombatRegenerateIn& in)
{
    EeCombatRegenerateOut theOut = EECombatCppRegenerate(in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'R', 'G'), in, theOut);
    return theOut;
}
inline EeCombatTimerOut EECombatTimer(const EeCombatTimerIn& in)
{
    EeCombatTimerOut theOut = EECombatCppTimer(in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'T', 'M'), in, theOut);
    return theOut;
}
inline EeCombatHealOut EECombatHeal(const EeCombatHealIn& in)
{
    EeCombatHealOut theOut = EECombatCppHeal(in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'H', 'L'), in, theOut);
    return theOut;
}
inline EeCombatHealStepOut EECombatHealStep(const EeCombatHealStepIn& in)
{
    EeCombatHealStepOut theOut = EECombatCppHealStep(in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'H', 'S'), in, theOut);
    return theOut;
}
inline int32_t EECombatConversionCost(const EeCombatConversionCostIn& in)
{
    int32_t theOut = EECombatCppConversionCost(in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'C', 'C'), in, theOut);
    return theOut;
}
inline EeCombatConversionOut EECombatConversion(const EeCombatConversionIn& in)
{
    EeCombatConversionOut theOut = EECombatCppConversion(in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'C', 'V'), in, theOut);
    return theOut;
}
inline EeCombatBallisticOut EECombatBallistic(const EeCombatBallisticIn& in)
{
    EeCombatBallisticOut theOut = EECombatCppBallistic(in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'B', 'P'), in, theOut);
    return theOut;
}
inline EeCombatArcOut EECombatArc(const EeCombatArcIn& in)
{
    EeCombatArcOut theOut = EECombatCppArc(in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'P', 'A'), in, theOut);
    return theOut;
}
inline int32_t EECombatRange(const EeCombatRangeIn& in)
{
    int32_t theOut = EECombatCppRange(in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'R', 'H'), in, theOut);
    return theOut;
}
inline int32_t EECombatCanReach(const EeCombatReachIn& in)
{
    int32_t theOut = EECombatCppCanReach(in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'R', 'C'), in, theOut);
    return theOut;
}
inline int32_t EECombatWorkReach(const EeCombatReachIn& in)
{
    int32_t theOut = EECombatCppWorkReach(in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'R', 'W'), in, theOut);
    return theOut;
}
inline int32_t EECombatGatherReach(const EeCombatReachIn& in)
{
    int32_t theOut = EECombatCppGatherReach(in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'R', 'T'), in, theOut);
    return theOut;
}
inline int32_t EECombatWithinRange(float inDistanceSquared, float inRange)
{
    int32_t theOut = EECombatCppWithinRange(inDistanceSquared, inRange);
    float theIn[2] = {inDistanceSquared, inRange};
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'H', 'R'), theIn, theOut);
    return theOut;
}
inline void EECombatDamageRow(const EeCombatDamageRowIn& in, EeCombatDamageRowOut& out)
{
    EECombatCppDamageRow(in, out);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'D', 'T'), in, out);
}

#endif
