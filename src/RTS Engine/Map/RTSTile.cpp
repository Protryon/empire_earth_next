// C:\EEX\RTS Engine\Map\RTSTile.cpp (no __FILE__: no asserts). A map tile's engine side (RTTI RTSTile, vtable
// 0x441dd0): 0x1245bb..0x1248f6 (the three "RTSTile-..." key chains, the class in definition order; RTSVisibleMap's
// key chains open the next unit). Unit flags: the exe's module set.
#include <windows.h>
#include <stdio.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFile.h"

#include "ee/EETileLocation.h"
#include "ee/EEPlayer.h"
#include "ee/EEMap.h"

// chain at 0x5245bb (wrapper)
//   object 0x929878  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
const UString RTSTile::sElevation("RTSTile-Elevation");
// SYNTHETIC: EXE 0x5245bb   (wrapper)
// SYNTHETIC: EXE 0x5245c5   (ctor)
// SYNTHETIC: EXE 0x5245d6   (registrar)
// SYNTHETIC: EXE 0x5245e2   (dtor)

// chain at 0x5245fe (wrapper)
//   object 0x929860  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
const UString RTSTile::sModelIndex("RTSTile-ModelIndex");
// SYNTHETIC: EXE 0x5245fe   (wrapper)
// SYNTHETIC: EXE 0x524608   (ctor)
// SYNTHETIC: EXE 0x524619   (registrar)
// SYNTHETIC: EXE 0x524625   (dtor)

// chain at 0x524641 (wrapper)
//   object 0x929850  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
const UString RTSTile::sTerrainIndex("RTSTile-TerrainIndex");
// SYNTHETIC: EXE 0x524641   (wrapper)
// SYNTHETIC: EXE 0x52464b   (ctor)
// SYNTHETIC: EXE 0x52465c   (registrar)
// SYNTHETIC: EXE 0x524668   (dtor)

// FUNCTION: EXE 0x524684
// Creates a tile at the given height with no units and terrain type 0
RTSTile::RTSTile(float inHeight)
{
    this->mHeight = inHeight;
    this->mUnits = NULL;
    this->mTerrainType = 0;
    this->mAIValue = false;
}

// SYNTHETIC: EXE 0x5246a1
// (the scalar deleting destructor, emitted with the vtable; slot RTSTile+0x0)

// FUNCTION: EXE 0x5246bd
// Destroys the tile
RTSTile::~RTSTile()
{
}

// FUNCTION: EXE 0x5246c4
// Drops one of the player's sight references on the tile; the last one leaves it explored
void RTSTile::RemoveSight(EEPlayer* inPlayer)
{
    if (this->GetVisibility(inPlayer->GetPlayerID()) > 1)
        this->mVisibility[inPlayer->GetPlayerID() >> 1] += -1 << ((inPlayer->GetPlayerID() & 1) << 4);
}

// FUNCTION: EXE 0x524708
// Frees a node of the tile's unit list
void RTSTile::FreeUnitNode(RTSTileUnitNode* inNode)
{
    delete inNode;
}

// FUNCTION: EXE 0x524719
// Returns the tile's checksum: its terrain type plus every player's visibility value
long RTSTile::GetChecksum(unsigned long inPlayerCount) const
{
    long theCount = 0;
    for (unsigned long i = 0; i < inPlayerCount; i++)
        theCount += this->GetVisibility(i);
    return this->mTerrainType + theCount;
}

// FUNCTION: EXE 0x524756
// Reads the tile's height and per-player visibility words; returns the first read error
long RTSTile::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile,
                   unsigned long inPlayerCount)
{
    long theResult = inFile.Read(this->mHeight);
    if (theResult == kErrorOK) {
        unsigned long theRead;
        theResult = inFile.Read(reinterpret_cast<char*>(this->mVisibility), ((inPlayerCount >> 1) + (inPlayerCount & 1)) * 4, theRead);
    }
    return theResult;
}

// FUNCTION: EXE 0x524793
// Prints each player's visibility value and the terrain type of the tile to an out-of-sync dump
void RTSTile::Dump(FILE* inFile, unsigned long inPlayerCount) const
{
    for (unsigned long i = 0; i < inPlayerCount; i++)
        fprintf(inFile, "Tile Vis Count For Player %ld has a value of %ld\n", i, this->GetVisibility(i));
    fprintf(inFile, "Tile Terrain Type has value %ld\n", this->mTerrainType);
}

// FUNCTION: EXE 0x5247ed
// Adds a sight reference of the player to the tile; an unexplored tile becomes seen
void RTSTile::AddSight(EEPlayer* inPlayer)
{
    if (this->GetVisibility(inPlayer->GetPlayerID()) != 0)
        this->mVisibility[inPlayer->GetPlayerID() >> 1] += 1 << ((inPlayer->GetPlayerID() & 1) << 4);
    else
        this->SetVisibility(2, inPlayer);
}

// FUNCTION: EXE 0x52483c
// Frees the tile's whole unit list
void RTSTile::RemoveAllUnits()
{
    RTSTileUnitNode* theNode = this->mUnits;
    while (theNode != NULL) {
        RTSTileUnitNode* theNext = theNode->mNext;
        this->FreeUnitNode(theNode);
        theNode = theNext;
    }
    this->mUnits = NULL;
}

// FUNCTION: EXE 0x52485f
// Removes a unit from the tile's unit list
void RTSTile::RemoveUnit(EEUnit* inUnit)
{
    RTSTileUnitNode* thePrevious = NULL;
    RTSTileUnitNode* theNode = this->mUnits;
    while (theNode != NULL) {
        if (theNode->mUnit == inUnit) {
            if (thePrevious != NULL)
                thePrevious->mNext = theNode->mNext;
            else
                this->mUnits = theNode->mNext;
            this->FreeUnitNode(theNode);
            return;
        }
        thePrevious = theNode;
        theNode = theNode->mNext;
    }
}

// FUNCTION: EXE 0x524895
// Writes the tile's height and per-player visibility words; returns the first write error
long RTSTile::Save(FSFile& outFile, unsigned long inPlayerCount)
{
    long theResult = outFile.Write(this->mHeight);
    if (theResult != kErrorOK)
        return theResult;
    unsigned long theWritten;
    theResult = outFile.Write(reinterpret_cast<const char*>(this->mVisibility), ((inPlayerCount >> 1) + (inPlayerCount & 1)) * 4,
                              theWritten);
    return theResult;
}

// FUNCTION: EXE 0x5248d5
// Returns whether a unit is in the tile's unit list
bool RTSTile::HasUnit(EEUnit* inUnit)
{
    bool theResult = false;
    RTSTileUnitNode* theNode = this->mUnits;
    while (theNode != NULL) {
        if (theNode->mUnit == inUnit) {
            theResult = true;
            break;
        }
        theNode = theNode->mNext;
    }
    return theResult;
}

// FUNCTION: EXE 0x5248ec
// Sets the tile's terrain type (the location and map are unused)
void RTSTile::SetTerrainType(long inValue, const EETileLocation&, RTSMap*)
{
    this->mTerrainType = inValue;
}
