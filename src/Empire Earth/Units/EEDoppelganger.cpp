// C:\EEX\Empire Earth\Units\EEDoppelganger.cpp (file name inferred: no __FILE__; the class name). What another
// player still sees of a unit: 0x201d85..0x201fd9. Unit flags: the exe's module set. Data: the vtable 0x446e7c (its
// ??_E 0x203433 is emitted by EEDoppelgangerManager.cpp); the trivial slots are ICF-folded elsewhere.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSRestorable.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEGraphicObject.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEDoppelganger.h"
#include "ee/EEDoppelgangerManager.h"

// Creates an empty doppelganger with no unit, viewing player or tile
EEDoppelganger::EEDoppelganger()
{
    this->Reset();
}

// Returns the remembered unit's model, as upgraded for the unit's owner
EEModel* EEDoppelganger::GetModel()
{
    long theType = this->mUnit->mUnitClass->mUnitType;
    EEPlayer* thePlayer = this->GetPlayer();
    return thePlayer->mUnitClassUpgrades[theType]->mModel;
}

// Returns the remembered unit's class scale
float EEDoppelganger::GetScale()
{
    return this->mUnit->mUnitClass->mScale;
}

// Returns -1: a doppelganger shows no hit points
long EEDoppelganger::GetHitPoints()
{
    return -1;
}

// Returns -10000 (off the map) as the tile X; GetTileY below is the same body, folded into it
long EEDoppelganger::GetTileX()
{
    return -10000;
}

long EEDoppelganger::GetTileY()
{
    return -10000;
}

// Destroys the doppelganger, releasing its unit and model media
EEDoppelganger::~EEDoppelganger()
{
    this->Clear();
}

// Forgets the unit, viewing player and tile without releasing anything
void EEDoppelganger::Reset()
{
    this->mUnit = NULL;
    this->mPlayer = NULL;
    this->mTile = NULL;
}

// Releases the remembered unit (and a simple doppelganger's model media), then resets
void EEDoppelganger::Clear()
{
    if (this->mUnit != NULL) {
        if (!this->IsComplex())
            this->mUnit->mStats->mModel->UnloadMedia();
        this->mUnit->Dereference();
    }
    this->Reset();
}

// Returns whether the remembered unit is still valid
bool EEDoppelganger::GetIsValid()
{
    return this->mUnit->mIsValid;
}

// Returns the remembered unit's owner
EEPlayer* EEDoppelganger::GetPlayer()
{
    return this->mUnit->mPlayer;
}

// Remembers a unit on a tile for a viewing player, referencing it (and loading a simple one's media)
void EEDoppelganger::Initialize(EEUnit* inUnit, EEPlayer* inPlayer, RTSTile* inTile)
{
    this->mPlayer = inPlayer;
    this->mUnit = inUnit;
    if (!this->IsComplex())
        this->mUnit->mStats->mModel->LoadMedia();
    this->mUnit->Reference();
    this->mTile = inTile;
}

// Copies another doppelganger's unit and tile for a new viewing player (the tile argument is unused)
void EEDoppelganger::Initialize(EEDoppelganger* inOther, EEPlayer* inPlayer, RTSTile* inTile)
{
    this->mPlayer = inPlayer;
    inOther->mUnit->Reference();
    this->mUnit = inOther->mUnit;
    this->mTile = inOther->mTile;
    if (!this->IsComplex())
        this->mUnit->mStats->mModel->LoadMedia();
}

// Reads the unit id and viewing player id from the saved game; returns the second read's result
long EEDoppelganger::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    unsigned long theUnitID;
    long thePlayer;
    inFile.Read(theUnitID);
    this->mUnit = reinterpret_cast<EEUnit*>(theUnitID);
    long theResult = inFile.Read(thePlayer);
    this->mPlayer = reinterpret_cast<EEPlayer*>(thePlayer);
    return theResult;
}

// Turns the loaded ids back into unit and player and puts a simple doppelganger on its tile; frees a
// simple one that lost either (returning 0), a complex one returns 9
long EEDoppelganger::RepairPointers(unsigned long inFirst, unsigned long inSecond)
{
    long theResult = 0;
    long thePlayer = (int32_t)USavedID(this->mPlayer);
    EEUnit* theUnit = static_cast<EEUnit*>(::gWorld.GetUnit(USavedID(this->mUnit)));
    if (theUnit != NULL) {
        this->mUnit = theUnit;
        theUnit->Reference();
    } else {
        this->mUnit = NULL;
        theResult = 9;
    }
    EEPlayer* theOwner = ::gWorld.mPlayers[thePlayer];
    if (theOwner != NULL)
        this->mPlayer = theOwner;
    else {
        this->mPlayer = NULL;
        theResult = 9;
    }
    if (theResult == 9) {
        if (!this->IsComplex()) {
            theResult = 0;
            if (this->mUnit != NULL)
                this->mUnit->mStats->mModel->LoadMedia();
            gDoppelgangerManager.Free(this);
        }
    } else if (!this->IsComplex()) {
        static_cast<EETile*>(this->mTile)->AddDoppelganger(this);
        this->mUnit->mStats->mModel->LoadMedia();
    }
    return theResult;
}

// Writes the complex flag, unit id and viewing player id if the unit is valid; returns the first write error
long EEDoppelganger::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    bool theFlag = this->IsComplex();
    long theResult = 0;
    if (this->GetIsValid() == true) {
        theResult = outFile.Write(theFlag);
        if (theResult == 0) {
            unsigned long theID = this->mUnit->mUnitID;
            theResult = outFile.Write(theID);
            if (theResult == 0) {
                theID = this->mPlayer->mPlayerID;
                theResult = outFile.Write(theID);
            }
        }
    }
    return theResult;
}

// The trivial slots, ICF-folded into other units' identical bodies: a simple doppelganger has no animation, frame
// or complex part, counts as built, is neither a foundation nor complex, has no transformation and keeps its tile:
long EEDoppelganger::GetAnimation()
{
    return 0;
}

EEComplexDoppelganger* EEDoppelganger::GetComplexDoppelganger()
{
    return NULL;
}

bool EEDoppelganger::IsBuilt()
{
    return true;
}

long EEDoppelganger::GetFrame()
{
    return 0;
}

bool EEDoppelganger::IsFoundation()
{
    return false;
}

bool EEDoppelganger::IsComplex()
{
    return false;
}

void EEDoppelganger::GetTransformation(GETransformation&)
{
}

RTSTile* EEDoppelganger::GetTile()
{
    return this->mTile;
}
