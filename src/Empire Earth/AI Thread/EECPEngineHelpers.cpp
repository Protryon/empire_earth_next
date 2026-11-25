// Engine helpers the original emitted out of line in the computer player's combat units (EECPASG*, EECPAttackSubGroup,
// the prophet sub group units) after their first user there. Stage 5 deleted those units (docs/AI_RESEARCH.md
// §8.90); the rest of the game still calls these, so they live here. New unit (tools/units.py EXTRA_UNITS).
// EEUnit::GetTechTreeNode is EETechTree.cpp's and EEComplexUnit::GetMaxVelocity EEActiveGroup.cpp's: the only two
// that called other units (tools/cpp_deps.py), so the rest is a leaf.
#include <windows.h>
#include <algorithm>
#include <deque>
#include <map>
#include <set>
#include <vector>
#include <math.h>
#include <string.h>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEGlobalConstants.h"   // kHeightEpsilon's extern declaration (its definition here)

// 0x43bf0c (.rdata; both neighbours are this unit's constants): declared extern in ee/EEGlobalConstants.h
const float kHeightEpsilon = 0.01f;

// Returns true when the player has explored the tile (visibility above 1) or sees the whole map
bool EETile::IsSeenBy(EEPlayer* inPlayer)
{
    return this->GetVisibility(inPlayer->mPlayerID) > 1 || inPlayer->mSeesAll;
}

// Returns the squared tile distance from the stored centre tile to a given tile
long EETileDistanceLess::DistanceSquared(const EETileCoordinate& inTile) const
{
    long theDY = this->mCenter.mY - inTile.mY;
    long theDX = this->mCenter.GetX() - inTile.GetX();
    return theDX * theDX + theDY * theDY;
}

// ---------------------------------------------------------------------------------------------------------------
// Engine members the deleted economy managers' units emitted (stage 6, docs/AI_LUA_VERBS.md)

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
