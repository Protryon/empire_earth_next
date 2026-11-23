// C:\EEX\Empire Earth\Units\EEWheel.cpp (file name inferred: no __FILE__; the class name, as the codebase names its files).
// A unit model's wheels and some cdecl unit helpers: 0x23c83a..0x23ceca. Unit flags: the exe's module set.
// Data: the name key "wheel" 0x564488 (a plain global: its dtor is unguarded); vtable 0x449af0; the unit's named
// floats 0x449ae8.. (.rdata).
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/GEModel.h"

#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEWheel.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEMapDimensions.h"
#include "ee/EEGlobalConstants.h"
#include "ee/EEAIKindIDs.h"
#include "rust/sim/netmsg.h"   // ee_netmsg_trade_price
#include "rust/sim/units.h"



// (the wheel's construction, destruction and EEIsWheelModel: EEGraphicObject.cpp, which makes the wheels)

// Rolls the wheel back by the unit's travel this tick (speed or turn rate, whichever is the larger share of its top)
// over its scaled radius, in physics actions 0, 4 and 5 above a tenth of the top; wraps the angle by a full turn
void EEWheel::Update(EEComplexUnit* inUnit)
{
    this->mAngle = ee_units_cu_wheel_angle(EeUnitsWheelIn{this->mAngle, inUnit->mPhysicalModel.GetScale(), this->mRadius,
        (uint32_t)inUnit->mCurrentPhysicsAction, inUnit->mVelocity, inUnit->mAngularVelocity.mYaw, inUnit->mStats->GetInverseMaxVelocity(),
        inUnit->mStats->GetInverseMaxAngularVelocity(), inUnit->mStats->GetMaxVelocity(), gPhysics->GetTickSeconds()});   // cu_wheel.rs
}

// Returns the unit's attack animation set: 0xb for the special attack of Roman Legionnaires and capital ships,
// 8 when it fires high over walls, else 0
long EEGetAttackFrameSet(EEComplexUnit* inUnit)
{
    return ee_units_cu_attack_frame_set(inUnit->mIsSpecialAttack, (uint32_t)inUnit->mUnitClass->mUnitType, inUnit->mStats->mCanFireOverWalls,
                                        inUnit->mIsFiringHigh);   // cu_wheel.rs
}

// Replaces a target the unit can't reach: with its position when that is reachable, else with a reachable spot up to
// 9 tiles away, halved toward the target to within half a tile (the unit's position when none is found; Rust)
void EEFindLandingPoint(EEComplexUnit* inUnit, GE3DPoint* ioTarget)
{
    ee_units_find_landing_point((uint64_t)(uintptr_t)inUnit, (float(*)[3])&ioTarget->mX);   // ee_sim units landing.rs
}

// Returns a resource's market price (1..1000) from the world table's rate and the player's trade balance in it;
// 1 lower when inBuy is set, else 1 higher
long EEGetTradePrice(EEPlayer* inPlayer, long inResource, unsigned char inBuy)
{
    float theRate = 1.0f;
    long theTrade = inPlayer->mResourceTrade[inResource - 2];
    switch (inResource) {
    case 2: theRate = gWorldTable.GetFloatValue(kWorldTradeRateFoodWood); break;
    case 3: theRate = gWorldTable.GetFloatValue2(kWorldTradeRateFoodWood); break;
    case 4: theRate = gWorldTable.GetFloatValue(kWorldTradeRateStoneIron); break;
    case 6: theRate = gWorldTable.GetFloatValue2(kWorldTradeRateStoneIron); break;
    }
    return (long)ee_netmsg_trade_price(theRate, theTrade, inBuy);   // the price curve (ee_netmsg execute.rs)
}

// Picks a point 4..12 tiles from the unit along its goal-to-unit heading (+-20 degrees) on its land continent, up to
// 13 tries; returns whether found, else writes its position ((0, 0) when it has no goal)
// cdecl with a real GE2DPoint output: EEComplexUnit::FleeAfterCalamity and 0xbaab pass one (provisional name)
bool EEGetRandomPointAhead(EEComplexUnit* inUnit, GE2DPoint* outPoint)
{
    return ee_units_random_point_ahead((uint64_t)(uintptr_t)inUnit, (float(*)[2])&outPoint->mX);   // ee_sim units landing.rs
}

// Returns the unit type of the player's farms: 0x33a once the robotic farm upgrade is researched, else 0x2a
long EEGetFarmType(EEPlayer* inPlayer)
{
    return inPlayer->mTechnologies->Get(kTechRoboticFarmUpgrade)->mIsResearched ? 0x33a : 0x2a;
}
