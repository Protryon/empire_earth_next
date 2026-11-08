// C:\EEX\Empire Earth\Graphics Thread\EEE2DWaterRipple.cpp (no __FILE__; name from the RTTI class and its key
// "EEE2DWaterRipple-Data"). 0xe7d67..0xe8158: the key chain, three ctors .. Virtual4C; EEE2DWinterBreath.cpp follows.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEOrientation.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GEMaterial.h"
#include "lle/GEModel.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/EEUnit.h"
#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/RTSGraphics.h"
#include "ee/EEGraphics.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEGraphicObject.h"
#include "ee/EERenderObject.h"
#include "ee/EEEffect.h"
#include "ee/EEEffect2D.h"
#include "ee/EEE2DWaterRipple.h"
#include "ee/EEEffectKindIDs.h"

// SYNTHETIC: EXE 0x4e7d67
// SYNTHETIC: EXE 0x4e7d71
// SYNTHETIC: EXE 0x4e7d82
// SYNTHETIC: EXE 0x4e7d8e
const UString EEE2DWaterRipple::sData("EEE2DWaterRipple-Data");

// FUNCTION: EXE 0x4e7daa
// Creates a water ripple at a point, flattened to height 0, with age 0 and no graphic loaded yet
EEE2DWaterRipple::EEE2DWaterRipple(const GE3DPoint& inPosition, EEGraphicEffect* inGraphicEffect)
    : EEEffect2D(0, inGraphicEffect, gU4E8DF4, kEffect2DWaterRipple, false)
{
    this->mTransformation.SetPosition(inPosition.GetX(), inPosition.GetY(), inPosition.GetZ());
    this->mTransformation.SetZ(0.0f);
    this->mGraphicObject = NULL;
    this->mAge = 0.0f;
    this->mDrawAge = 0.0f;
}

// SYNTHETIC: EXE 0x4e7e0f
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4e7e2b
// Creates an empty water ripple for loading from a saved game
EEE2DWaterRipple::EEE2DWaterRipple()
    : EEEffect2D(gU4E8DF4, kEffect2DWaterRipple, false)
{
}

// FUNCTION: EXE 0x4e7e50
// Creates a water ripple at a transformation, flattened to height 0, with age 0 and no graphic loaded yet
EEE2DWaterRipple::EEE2DWaterRipple(const GETransformation& inTransformation, EEGraphicEffect* inGraphicEffect)
    : EEEffect2D(0, inGraphicEffect, gU4E8DF4, kEffect2DWaterRipple, false)
{
    this->mTransformation = inTransformation;
    this->mTransformation.SetZ(0.0f);
    this->mGraphicObject = NULL;
    this->mAge = 0.0f;
    this->mDrawAge = 0.0f;
}

// FUNCTION: EXE 0x4e7eac
// Destroys the ripple, unloading its graphic's media
EEE2DWaterRipple::~EEE2DWaterRipple()
{
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
}

long EEE2DWaterRipple::DrawPass2()
{
    return 0;
}

long EEE2DWaterRipple::DrawPass4()
{
    return 0;
}

long EEE2DWaterRipple::DrawPass6()
{
    return 0;
}

// FUNCTION: EXE 0x4e7ecd
// Queues the ripple in render queue 3 under its effect's texture for this screen, snapshotting its age to draw
long EEE2DWaterRipple::Queue(EEEffectQueue* inQueues)
{
    long theResult = EEEffect2D::Queue(inQueues);
    if (theResult == 0) {
        this->mDrawAge = this->mAge;
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[3][theTexture].push_back(this);
    }
    return theResult;
}

void EEE2DWaterRipple::PrepareQueue()
{
}

// FUNCTION: EXE 0x4e7f34
// Loads the ripple's graphic and sets its position and bounding radius (the effect's size); returns the load result
long EEE2DWaterRipple::SetUp()
{
    GE3DPoint thePosition;
    this->mGraphicObject = this->GetGraphicObject();
    long theResult = this->mGraphicObject->LoadMedia();
    this->mTransformation.GetPosition(thePosition);
    this->SetPosition(thePosition);
    this->SetBoundingRadius(this->mGraphicEffect->GetSize());
    return theResult;
}

// FUNCTION: EXE 0x4e7f90
// Draws the ripple as a flat sprite growing from the start size to the full size, fading out over its second half
long EEE2DWaterRipple::DrawPass5()
{
    float theReference = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mAlphaReference : 0.0f;
    float theAlpha = this->mDrawAge < 0.5f ? 1.0f : (1.0f - this->mDrawAge) * 2.0f;
    GE3DPoint thePosition(this->mTransformation);
    float theStartSize = this->mGraphicEffect->GetStartSize();
    float theEndSize = this->mGraphicEffect->GetSize();
    float theFrom = this->mGraphicEffect->GetStartSize();
    GEColor theColor;
    AddGroundSprite(thePosition, theColor, 0.0f, (theEndSize - theFrom) * this->mDrawAge + theStartSize, theAlpha, 1.0f,
             theReference);
    return 0;
}

// FUNCTION: EXE 0x4e803e
// Reloads the ripple's graphic and reads its transformation and age; returns the first read error (base result ignored)
long EEE2DWaterRipple::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect2D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mGraphicObject = this->GetGraphicObject();
    this->mGraphicObject->LoadMedia();
    long theResult = this->mTransformation.Load(inFile);
    if (theResult == 0)
        theResult = inFile.Read(this->mAge);
    return theResult;
}

// FUNCTION: EXE 0x4e8089
// Writes the ripple's transformation and age; returns the first write error (the base class result is ignored)
long EEE2DWaterRipple::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect2D::Save(inMajorVersion, inMinorVersion, outFile);
    long theResult = this->mTransformation.Save(outFile);
    if (theResult == 0)
        theResult = outFile.Write(this->mAge);
    return theResult;
}

// FUNCTION: EXE 0x4e80c0
// Ages the ripple by the tick over the effect's duration; returns 7 (remove) once it is over or its tile is not blocked
long EEE2DWaterRipple::Step()
{
    long theResult = 0;
    this->mAge += (gPhysics->GetTickSeconds() * 1000.0f) / this->mGraphicEffect->mDuration;
    if (this->mAge <= 1.0f) {
        this->mTransformation.SetZ(0.0f);
        long theY = UMFloor(this->mTransformation.GetY());
        long theX = UMFloor(this->mTransformation.GetX());
        if (!gWorld.IsTileBlocked(theX, theY))
            theResult = 7;
    } else {
        theResult = 7;
    }
    return theResult;
}
