// C:\EEX\Empire Earth\AI Thread\EEInfluenceTable.cpp (file name inferred: no __FILE__; RTTI class EEInfluenceTable
// > RTSRegionTable<std::pair<unsigned long, long> >, vtable 0x43c41c). 0xa5589..0xa5c31: EEHumanPlayer.cpp ends at 0xa5589,
// EEPathManager.cpp's EEAutoPathRequest ctor starts at 0xa5c31; the RTSRegionTable COMDATs close the unit.
#include <windows.h>
#include <vector>
#include <utility>
#include <math.h>

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
#include "lle/FSFile.h"
#include "lle/U2DPoint.h"
#include "lle/U2DRectangle.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEInfluenceTable.h"

// FUNCTION: EXE 0x4a5589
// Creates the influence table over the map in 9x9-tile regions, with a zero entry per player and a total
EEInfluenceTable::EEInfluenceTable(EEMap* inMap, unsigned long inNumPlayers)
    : RTSRegionTable<std::pair<unsigned long, long> >(inMap, 9)
{
    this->mNumPlayers = inNumPlayers;
    this->Init();
}

// SYNTHETIC: EXE 0x4a55b1
// (the scalar deleting destructor)

// FUNCTION: EXE 0x4a55cd
// Destroys the influence table (the region table frees the regions)
EEInfluenceTable::~EEInfluenceTable()
{
}

// FUNCTION: EXE 0x4a55d8
// Gives every region a zero influence entry for each player plus a zero total entry
void EEInfluenceTable::Init()
{
    for (long x = this->mRegionsX - 1; x >= 0; x--) {
        for (long y = this->mRegionsY - 1; y >= 0; y--) {
            for (unsigned long i = 0; i < this->mNumPlayers; i++)
                this->Add(x, y, std::pair<unsigned long, long>(i, 0));
            this->Add(x, y, std::pair<unsigned long, long>(this->mNumPlayers, 0));
        }
    }
}

// NEAR: corner points built directly; the original copies them through the U2DPoint<long> copy ctor 0x4d28b
// (the U2DPoint<long> copy family) from inline corner accessors
// FUNCTION: EXE 0x4a5685
// Returns a player's influence summed over a rectangle of regions
long EEInfluenceTable::SumValue(unsigned long inPlayer, const U2DRectangle<long>& inRect)
{
    long theTotal = 0;
    for (long x = U2DPoint<long>(inRect.mLeft, inRect.mTop).mX; x <= U2DPoint<long>(inRect.mRight, inRect.mBottom).mX;
         x++) {
        for (long y = inRect.mTop; y <= inRect.mBottom; y++)
            theTotal += this->GetValue(inPlayer, x, y);
    }
    return theTotal;
}

// FUNCTION: EXE 0x4a56f6
// Returns the other players' influence summed over a rectangle of regions (start corner clipped at 0)
long EEInfluenceTable::SumOthersValue(unsigned long inPlayer, const U2DRectangle<long>& inRect)
{
    long theTotal = 0;
    long theMaxX = inRect.GetBottomRight().GetX();
    long theMaxY = inRect.GetBottomRight().GetY();
    for (long x = max(inRect.GetTopLeft().GetX(), 0); x <= theMaxX; x++) {
        for (long y = max(inRect.GetTopLeft().GetY(), 0); y <= theMaxY; y++)
            theTotal += this->GetOthersValue(inPlayer, x, y);
    }
    return theTotal;
}

// FUNCTION: EXE 0x4a57f9
// Returns the influence of all players but one in a region (the total minus that player's); 0 off the grid
long EEInfluenceTable::GetOthersValue(unsigned long inPlayer, unsigned long inX, unsigned long inY)
{
    std::vector<std::pair<unsigned long, long> >* theRegion = this->GetRegion(inX, inY);
    return theRegion != NULL ? (*theRegion)[this->mNumPlayers].second - (*theRegion)[inPlayer].second : 0;
}

// FUNCTION: EXE 0x4a5839
// Adds a unit's influence to its owner's and the total entry of the region holding a tile
void EEInfluenceTable::AddUnit(EEUnit* inUnit, long inX, long inY)
{
    long theValue = inUnit->mUnitClass->mInfluence;
    long theX = inX / this->mRegionSize;
    long theY = inY / this->mRegionSize;
    std::vector<std::pair<unsigned long, long> >* theRegion = this->GetRegion(theX, theY);
    if (theRegion != NULL) {
        (*theRegion)[inUnit->mPlayer->mPlayerID].second += theValue;
        (*theRegion)[this->mNumPlayers].second += theValue;
    }
}

// FUNCTION: EXE 0x4a58a7
// Subtracts a unit's influence from its owner's and the total entry of the region holding a tile, clamped at 0
void EEInfluenceTable::RemoveUnit(EEUnit* inUnit, long inX, long inY)
{
    long theX = inX / this->mRegionSize;
    long theY = inY / this->mRegionSize;
    std::vector<std::pair<unsigned long, long> >* theRegion = this->GetRegion(theX, theY);
    if (theRegion == NULL)
        return;
    std::pair<unsigned long, long>& thePlayer = (*theRegion)[inUnit->mPlayer->mPlayerID];
    thePlayer.second -= inUnit->mUnitClass->mInfluence;
    if (thePlayer.second < 0)
        thePlayer.second = 0;
    std::pair<unsigned long, long>& theTotal = (*theRegion)[this->mNumPlayers];
    theTotal.second -= inUnit->mUnitClass->mInfluence;
    if (theTotal.second < 0)
        theTotal.second = 0;
}

// FUNCTION: EXE 0x4a5920
// Returns the influence in a region of the players a player is at war with (player 0 only if inWithFirst)
long EEInfluenceTable::GetEnemyValue(unsigned long inPlayer, const U2DPoint<long>& inTile, bool inWithFirst)
{
    EEPlayer* thePlayer = gWorld.mPlayers[inPlayer];
    std::vector<std::pair<unsigned long, long> >* theRegion = this->GetRegion(inTile);
    long theTotal = 0;
    if (thePlayer != NULL && theRegion != NULL) {
        for (unsigned long i = !inWithFirst; i < this->mNumPlayers; i++) {
            if (gWorld.mPlayers[i] != thePlayer && thePlayer->mEnemies[gWorld.mPlayers[i]->mPlayerID] != 0)
                theTotal += (*theRegion)[i].second;
        }
    }
    return theTotal;
}

// FUNCTION: EXE 0x4a598d
// Returns the enemies' influence (player 0 only if inWithFirst) summed over a rectangle of regions
long EEInfluenceTable::SumEnemyValue(unsigned long inPlayer, const U2DRectangle<long>& inRect, bool inWithFirst)
{
    EEPlayer* thePlayer = gWorld.mPlayers[inPlayer];
    long theMaxX = inRect.GetBottomRight().GetX();
    long theMaxY = inRect.GetBottomRight().GetY();
    long theTotal = 0;
    if (thePlayer != NULL) {
        for (long x = max(inRect.GetTopLeft().GetX(), 0); x <= theMaxX; x++) {
            for (long y = max(inRect.GetTopLeft().GetY(), 0); y <= theMaxY; y++) {
                for (unsigned long i = !inWithFirst; i < this->mNumPlayers; i++) {
                    if (gWorld.mPlayers[i] != thePlayer && thePlayer->mEnemies[gWorld.mPlayers[i]->mPlayerID] != 0)
                        theTotal += this->GetValue(i, x, y);
                }
            }
        }
    }
    return theTotal;
}

// Compiler-generated for this unit (names in analysis/exe/names.txt):
// TEMPLATE: EXE 0x4a5b05
// TEMPLATE: EXE 0x4a5b1a
// TEMPLATE: EXE 0x4a5b36
// TEMPLATE: EXE 0x4a5b91
