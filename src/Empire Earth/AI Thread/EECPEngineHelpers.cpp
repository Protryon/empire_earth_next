// Engine helpers the original emitted out of line in the computer player's combat units (EECPASG*, EECPAttackSubGroup,
// the prophet sub group units) after their first user there. Stage 5 deleted those units (docs/AI_RESEARCH.md
// §8.90); the rest of the game still calls these, so they live here. New unit (tools/units.py EXTRA_UNITS).
#include <windows.h>
#include <algorithm>
#include <deque>
#include <map>
#include <set>
#include <vector>
#include <math.h>
#include <string.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEViewport.h"
#include "lle/GETransformation.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GETerrainMesh.h"
#include "lle/GERectangle.h"
#include "lle/FSFile.h"
#include "lle/FSRestorable.h"
#include "lle/U2DPoint.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/USingleton.h"
#include "ee/EmpireEarth.h"
#include "ee/EEGlobalConstants.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EECPASGFlags.h"
#include "ee/EEGroupCluster.h"
#include "ee/EEUnitVectorItem.h"

// 0x43bf0c (.rdata; both neighbours are this unit's constants): declared extern in ee/EEGlobalConstants.h
const float kHeightEpsilon = 0.01f;

// FUNCTION: EXE 0x446fc0
// Returns true when the given unit is in this unit's cargo
bool EEComplexUnit::HasCargo(EEComplexUnit* inCargo)
{
    return std::find(this->mCargo.begin(), this->mCargo.end(), inCargo) != this->mCargo.end();
}

// FUNCTION: EXE 0x449b21
// Returns true when the player has explored the tile (visibility above 1) or sees the whole map
bool EETile::IsSeenBy(EEPlayer* inPlayer)
{
    return this->GetVisibility(inPlayer->mPlayerID) > 1 || inPlayer->mSeesAll;
}

// FUNCTION: EXE 0x45b96d
// Returns true for the four crusader unit types (short sword, long sword, bronze spear cavalry, knight)
// An EEUnitClass member (Virtual1C calls it with ecx = the unit's class); its body follows its first user here.
bool EEUnitClass::IsCrusader()
{
    return this->mUnitType == kUnitTypeInf04ShortSwordCrusader || this->mUnitType == kUnitTypeInf06LongSwordCrusader || this->mUnitType == kUnitTypeCav04BronzeSpearCavalryCrusader || this->mUnitType == kUnitTypeCav06KnightCrusader;
}

// FUNCTION: EXE 0x406ef9
// Tells whether the unit class is a land, naval, helicopter or building priest
bool EEUnitClass::IsPriest()
{
    long theCategory = this->mCategory;
    return theCategory == kUnitCategoryLandPriest || theCategory == kUnitCategoryNavalPriest || theCategory == kUnitCategoryHelicopterPriest || theCategory == kUnitCategoryBuildingPriest;
}

// FUNCTION: EXE 0x409d5e
// Returns the owner's tech tree node of the technology that makes the unit's class
EETechTreeNode* EEUnit::GetTechTreeNode()
{
    return this->GetPlayer()->GetTechTree()->Get(this->GetUnitClass()->GetTechnology());
}

// FUNCTION: EXE 0x40c5bb
// Returns the map tile at a tile location
EETile* EEMap::GetTileAtLocation(const EETileLocation& inLocation)
{
    return this->GetTile(inLocation.GetX(), inLocation.GetY());
}

// FUNCTION: EXE 0x44211d
// Returns the squared distance from the unit's position to a tile
float EEComplexUnit::DistanceSquaredToTileXY(long inTileX, long inTileY)
{
    float theDX = this->mPhysicalModel.GetLocation().GetX() - inTileX;
    float theDY = this->mPhysicalModel.GetLocation().GetY() - inTileY;
    return theDY * theDY + theDX * theDX;
}

// FUNCTION: EXE 0x45e407
// Returns the unit's top speed: its active group's value when it is in one, else its stats'
// Out of line after its first user here
float EEComplexUnit::GetMaxVelocity()
{
    if (this->mActiveGroup)
        return this->mActiveGroup->GetMaxVelocity(this);
    return this->mStats->mMaxVelocity;
}

// FUNCTION: EXE 0x408a3f
// Returns the squared tile distance from the stored centre tile to a given tile
long EETileDistanceLess::DistanceSquared(const EETileCoordinate& inTile) const
{
    long theDY = this->mCenter.mY - inTile.mY;
    long theDX = this->mCenter.GetX() - inTile.GetX();
    return theDX * theDX + theDY * theDY;
}

// FUNCTION: EXE 0x441da1
// Returns the number of goal queues the unit has
unsigned long EEComplexUnit::GetNumGoalQueues()
{
    return this->mGoalQueues.size();
}

// ---------------------------------------------------------------------------------------------------------------
// Engine members the deleted economy managers' units emitted (stage 6, docs/AI_LUA_VERBS.md)

// FUNCTION: EXE 0x4937dd (EECPUnitManager.cpp's range)
// Returns whether a technology is in the unit's build queue
bool EEComplexUnit::IsResearchQueued(long inTechnology)
{
    return std::find(this->mBuildQueue.begin(), this->mBuildQueue.end(), inTechnology) != this->mBuildQueue.end();
}

// FUNCTION: EXE 0x4708c9 (EECPCivilizationManager.cpp's range)
// Returns true for waypoint and path point units
bool EEUnit::IsPathMarker()
{
    return this->mUnitClass->mUnitType == kUnitTypeWaypoint || this->mUnitClass->mUnitType == kUnitTypePathPoint;
}

// RTSMapTileRun (ee/EEMap.h): its two out-of-line members sat among EECPWallManager.cpp's COMDATs (RTSMap.cpp calls them)
bool RTSMapTileRun::operator==(const RTSMapTileRun& inOther) const
{
    return inOther.mStart.mX == this->mStart.mX && inOther.mStart.mY == this->mStart.mY && inOther.mEnd.mX == this->mEnd.mX &&
        inOther.mEnd.mY == this->mEnd.mY;
}

// FUNCTION: EXE 0x4a016f (EECPWallManager.cpp's range)
// Creates a tile run from inStart to inEnd
RTSMapTileRun::RTSMapTileRun(const U2DPoint<long>& inStart, const U2DPoint<long>& inEnd)
    : mStart(inStart), mEnd(inEnd)
{
}
