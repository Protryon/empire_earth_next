// C:\EEX\Empire Earth\Graphics Thread\EEEffect3D.cpp (no __FILE__; name from the RTTI class and its key
// "EEEffect3D-Data"). The base of the 3D effects (vtable 0x440334): 0x1050fb..0x1054ec (the key chain, Virtual24 ..
// Save). The load-time ctor EEEffect3D(long), the Virtual3C forwarder and ??_G are inline COMDATs emitted by the
// first user, EEE3DAdditivePass.cpp (0xe8e76..0xe8ed8). Unit flags: the exe's module set.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GEOrientation.h"
#include "lle/GETransformation.h"
#include "lle/GEViewport.h"
#include "lle/GETerrainMesh.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/EEUnit.h"
#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/RTSGraphics.h"
#include "ee/EEGraphics.h"
#include "ee/EEEffect.h"
#include "ee/EEEffect3D.h"
#include "rust/ee_ge.h"   // ee_gfx view.rs: the frustum test (RUST_PLAN R4.4)
#include "rust/ee_gfx.h"

// SYNTHETIC: EXE 0x5050fb
// SYNTHETIC: EXE 0x505105
// SYNTHETIC: EXE 0x505116
// SYNTHETIC: EXE 0x505122
const UString EEEffect3D::sData("EEEffect3D-Data");

// FUNCTION: EXE 0x50513e
// Returns 10 (skip) when the effect's tile is missing or not visible (IsTileVisible; unless the camera is free) or its
// bounding sphere lies outside the view on any side; else 0
long EEEffect3D::Queue(EEEffectQueue* inQueues)
{
    long theResult = EEEffect::Queue(inQueues);
    if (theResult != 0)
        return theResult;
    GE3DPoint theLocation(this->mTransformation.GetX(), this->mTransformation.GetY(), this->mTransformation.GetZ());
    float theRadius = this->mBoundingRadius;
    long theX = UMFloor(theLocation.GetX());
    long theY = UMFloor(theLocation.GetY());
    bool theOrthographic = gOrthographicView;
    EEMapTile* theTile = gWorld.mMap->GetTile(theX, theY);
    if (theTile == NULL)
        return 10;
    if (this->IsTileVisible(theTile) != true && gFreeCamera == false)
        return 10;
    if (!ee_gfx_effect_in_view(reinterpret_cast<const EeGeViewport*>(&gViewport), theOrthographic,
                               reinterpret_cast<const EeGe3DPoint*>(&theLocation), theRadius))
        return 10;
    return 0;
}

// FUNCTION: EXE 0x505424
// Releases the base effect's resources
void EEEffect3D::ReleaseResources()
{
    EEEffect::ReleaseResources();
}

// FUNCTION: EXE 0x505429
// Returns whether the current player sees the tile
bool EEEffect3D::IsTileVisible(EEMapTile* inTile)
{
    return inTile->IsSeenBy(gWorld.GetCurrentPlayer());
}

// FUNCTION: EXE 0x505441
// Resets a pooled 3D effect onto a new unit and graphic effect, with an identity transformation
void EEEffect3D::Reinit(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect)
{
    EEEffect::Reinit(inUnit, inGraphicEffect, false);
    this->InitTransformation();
}

// FUNCTION: EXE 0x50545e
// Resets a pooled 3D effect to the empty state with an identity transformation
void EEEffect3D::Reinit()
{
    EEEffect::Reinit(false);
    this->InitTransformation();
}

// FUNCTION: EXE 0x505471
// Resets the transformation to identity and the bounding radius to 0
void EEEffect3D::InitTransformation()
{
    this->mTransformation.MakeIdentity();
    this->mBoundingRadius = 0.0f;
}

// FUNCTION: EXE 0x505484
// Reads the transformation and bounding radius; returns the first read error (the base class result is ignored)
long EEEffect3D::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = EEEffect::Load(inMajorVersion, inMinorVersion, inFile);
    theResult = this->mTransformation.Load(inFile);
    if (theResult == 0)
        theResult = inFile.Read(this->mBoundingRadius);
    return theResult;
}

// FUNCTION: EXE 0x5054b8
// Writes the transformation and bounding radius; returns the first write error (the base class result is ignored)
long EEEffect3D::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = EEEffect::Save(inMajorVersion, inMinorVersion, outFile);
    theResult = this->mTransformation.Save(outFile);
    if (theResult == 0)
        theResult = outFile.Write(this->mBoundingRadius);
    return theResult;
}
