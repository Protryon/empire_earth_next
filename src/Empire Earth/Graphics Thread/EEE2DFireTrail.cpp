// C:\EEX\Empire Earth\Graphics Thread\EEE2DFireTrail.cpp (no __FILE__; name from the RTTI class and its key
// "EEE2DFireTrail-Data"). 0xe0cf5..0xe0ffa: the key chain, both ctors .. Virtual4C; its Virtual24 is the ICF-shared
// copy 0xe5804 (EEE2DPlasmaPulse.cpp); EEE2DImpact.cpp follows.
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
#include "ee/EEE2DFireTrail.h"
#include "ee/EEEffectKindIDs.h"

// SYNTHETIC: EXE 0x4e0cf5
// SYNTHETIC: EXE 0x4e0cff
// SYNTHETIC: EXE 0x4e0d10
// SYNTHETIC: EXE 0x4e0d1c
const UString EEE2DFireTrail::sData("EEE2DFireTrail-Data");

// unit-local .rdata after the vtable (0x43fc38), loaded from memory: a named const
const float kFireTrailRise = 0.00020000001f;

// FUNCTION: EXE 0x4e0d38
// Creates a fire trail puff for a unit with a random rotation and age 0
EEE2DFireTrail::EEE2DFireTrail(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect)
    : EEEffect2D(inUnit, inGraphicEffect, gEffect2DModel, kEffect2DFireTrail, false), mGraphicObject(NULL), mAge(0.0f)
{
    this->mDrawAge = 0.0f;
    this->mRotation = EEEffect::GetRandom() * 6.2831855f;
}

// SYNTHETIC: EXE 0x4e0d89
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4e0da5
// Creates an empty fire trail puff for loading from a saved game (graphic and age are set by Load)
EEE2DFireTrail::EEE2DFireTrail()
    : EEEffect2D(gEffect2DModel, kEffect2DFireTrail, false)
{
}

// FUNCTION: EXE 0x4e0dd0
// Destroys the puff, unloading its graphic's media
EEE2DFireTrail::~EEE2DFireTrail()
{
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
}

long EEE2DFireTrail::DrawPass2()
{
    return 0;
}

long EEE2DFireTrail::DrawPass4()
{
    return 0;
}

long EEE2DFireTrail::DrawPass5()
{
    return 0;
}

void EEE2DFireTrail::PrepareQueue()
{
}

// FUNCTION: EXE 0x4e0dee
// Places the puff at its unit with the start size as radius (if the unit is complex), then loads its graphic
long EEE2DFireTrail::SetUp()
{
    GE3DPoint thePosition;
    EEComplexUnit* theUnit = this->mUnit->mComplexUnit;
    if (theUnit) {
        theUnit->mPhysicalModel.GetLocation().GetPosition(thePosition);
        this->SetPosition(thePosition);
        this->SetBoundingRadius(this->mGraphicEffect->GetStartSize());
    }
    this->mGraphicObject = this->GetGraphicObject();
    return this->mGraphicObject->LoadMedia();
}

// FUNCTION: EXE 0x4e0e50
// Draws the puff as a sprite growing from the start size to the full size and fading out with age; returns 0
long EEE2DFireTrail::DrawPass6()
{
    EEGraphicEffect* theEffect = this->mGraphicEffect;
    float theReference = theEffect->mGraphicObject ? theEffect->mGraphicObject->mAlphaReference : 0.0f;
    float theStartSize = theEffect->GetStartSize();
    float theEndSize = theEffect->GetSize();
    float theFrom = theEffect->GetStartSize();
    GEColor theColor;
    AddGlowSprite(this->mDrawPosition, theColor, this->mRotation, (theEndSize - theFrom) * this->mDrawAge + theStartSize, 1.0f - this->mDrawAge, 1.0f, theReference);
    return 0;
}

long EEE2DFireTrail::Queue(EEEffectQueue* inQueues)
{
    long theResult = EEEffect2D::Queue(inQueues);
    if (theResult == 0) {
        this->mDrawAge = this->mAge;
        this->mDrawPosition = this->mPosition;
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[4][theTexture].push_back(this);
    }
    return theResult;
}

// FUNCTION: EXE 0x4e0ec9
// Reloads the puff's graphic and reads its age and rotation; returns the first read error (base result ignored)
long EEE2DFireTrail::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect2D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mGraphicObject = this->GetGraphicObject();
    this->mGraphicObject->LoadMedia();
    long theResult = inFile.Read(this->mAge);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mRotation);
    return theResult;
}

// FUNCTION: EXE 0x4e0f0f
// Writes the puff's age and rotation; returns the first write error (the base class result is ignored)
long EEE2DFireTrail::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect2D::Save(inMajorVersion, inMinorVersion, outFile);
    long theResult = outFile.Write(this->mAge);
    if (theResult == 0)
        theResult = outFile.Write(this->mRotation);
    return theResult;
}

// NEAR: the wind's z stays on the FP stack and the result lives in esi in the original (GEVector local shape)
// FUNCTION: EXE 0x4e0f44
// Ages the puff and drifts it with the wind while rising and growing; returns 7 (remove) once its life is over
long EEE2DFireTrail::Step()
{
    GE3DPoint thePosition = this->mPosition;
    GEVector theWind = gWorld.mWindDirection;
    float theElapsed = gPhysics->GetTickTime();
    long theResult = 0;
    this->mAge += theElapsed / this->mGraphicEffect->mDuration;
    if (this->mAge <= 1.0f) {
        thePosition.SetX(thePosition.GetX() + theWind.mX);
        thePosition.SetY(thePosition.GetY() + theWind.GetY());
        thePosition.SetZ(thePosition.GetZ() + (theWind.GetZ() + theElapsed * kFireTrailRise));
        this->SetPosition(thePosition);
        this->SetBoundingRadius(((this->mGraphicEffect->GetSize() - this->mGraphicEffect->GetStartSize()) * this->mAge + this->mGraphicEffect->GetStartSize()) * 0.5f);
    } else {
        theResult = 7;
    }
    return theResult;
}
