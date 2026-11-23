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
#include "rust/sim/units.h"

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
    ee_units_dm_dop(EE_UNITS_DM_DG_DO_RESET, this, NULL, NULL, NULL);   // dm_dop.rs
}

// Releases the remembered unit (and a simple doppelganger's model media), then resets
void EEDoppelganger::Clear()
{
    ee_units_dm_dop(EE_UNITS_DM_DG_DO_CLEAR, this, NULL, NULL, NULL);   // dm_dop.rs
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
    ee_units_dm_dop(EE_UNITS_DM_DG_DO_INITIALIZE, this, inUnit, inPlayer, inTile);   // dm_dop.rs
}

// Copies another doppelganger's unit and tile for a new viewing player (the tile argument is unused)
void EEDoppelganger::Initialize(EEDoppelganger* inOther, EEPlayer* inPlayer, RTSTile* inTile)
{
    ee_units_dm_dop(EE_UNITS_DM_DG_DO_INITIALIZE_COPY, this, inOther, inPlayer, inTile);   // dm_dop.rs
}

// Reads the unit id and viewing player id from the saved game; returns the second read's result
long EEDoppelganger::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return (long)ee_units_dm_dop(EE_UNITS_DM_DG_DO_LOAD, this, &inFile, NULL, NULL);   // dm_dop.rs
}

// Turns the loaded ids back into unit and player and puts a simple doppelganger on its tile; frees a
// simple one that lost either (returning 0), a complex one returns 9
long EEDoppelganger::RepairPointers(unsigned long inFirst, unsigned long inSecond)
{
    return (long)ee_units_dm_dop(EE_UNITS_DM_DG_DO_REPAIR_POINTERS, this, NULL, NULL, NULL);   // dm_dop.rs
}

// Writes the complex flag, unit id and viewing player id if the unit is valid; returns the first write error
long EEDoppelganger::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    return (long)ee_units_dm_dop(EE_UNITS_DM_DG_DO_SAVE, this, &outFile, NULL, NULL);   // dm_dop.rs
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
