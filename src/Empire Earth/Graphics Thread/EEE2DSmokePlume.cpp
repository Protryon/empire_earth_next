// C:\EEX\Empire Earth\Graphics Thread\EEE2DSmokePlume.cpp (no __FILE__; name from the RTTI class and its key
// "EEE2DSmokePlume-Data"). 0xe6ef2..0xe72fa: the key chain, both ctors .. Step; EEE2DSquareMarchingIcons.cpp
// follows.
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
#include "ee/EEE2DSmokePlume.h"

// SYNTHETIC: EXE 0x4e6ef2
// SYNTHETIC: EXE 0x4e6efc
// SYNTHETIC: EXE 0x4e6f0d
// SYNTHETIC: EXE 0x4e6f19
const UString EEE2DSmokePlume::sData("EEE2DSmokePlume-Data");

// FUNCTION: EXE 0x4e6f35
// Creates a smoke puff of the given kind at a point with a random rotation and a random brightness in
// parameter 1 +- 2, clamped to 0..1
EEE2DSmokePlume::EEE2DSmokePlume(const GE3DPoint& inPosition, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect2D(0, inGraphicEffect, gEffect2DModel, inKind, false)
{
    float theMinimum = this->mGraphicEffect->GetParam1() - this->mGraphicEffect->GetParam2();
    float theMaximum = this->mGraphicEffect->GetParam1() + this->mGraphicEffect->GetParam2();
    this->mStart = inPosition;
    this->mAge = 0.0f;
    this->mGraphicObject = NULL;
    this->mDrawAge = 0.0f;
    this->mRotation = EEEffect::GetRandom() * 6.2831855f;
    this->mBrightness = EEEffect::GetRandom() * (theMaximum - theMinimum) + theMinimum;
    this->mBrightness = max(this->mBrightness, 0.0f);
    this->mBrightness = min(this->mBrightness, 1.0f);
}

// SYNTHETIC: EXE 0x4e7004
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4e7020
// Creates an empty smoke puff of the given kind for loading from a saved game
EEE2DSmokePlume::EEE2DSmokePlume(long inKind)
    : EEEffect2D(gEffect2DModel, inKind, false)
{
}

// FUNCTION: EXE 0x4e705e
// Destroys the puff, unloading its graphic's media
EEE2DSmokePlume::~EEE2DSmokePlume()
{
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
}

long EEE2DSmokePlume::DrawPass2()
{
    return 0;
}

long EEE2DSmokePlume::DrawPass4()
{
    return 0;
}

long EEE2DSmokePlume::DrawPass5()
{
    return 0;
}

// FUNCTION: EXE 0x4e707c
// Queues the puff in render queue 4 under its texture for this screen, snapshotting its age and drifting point
long EEE2DSmokePlume::Queue(EEEffectQueue* inQueues)
{
    long theResult = EEEffect2D::Queue(inQueues);
    if (theResult == 0) {
        this->mDrawAge = this->mAge;
        this->mDrawPosition = this->mStart;
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[4][theTexture].push_back(this);
    }
    return theResult;
}

void EEE2DSmokePlume::PrepareQueue()
{
}

// FUNCTION: EXE 0x4e70ef
// Loads the puff's graphic and sets its position and bounding radius (the effect's size); returns the load result
long EEE2DSmokePlume::SetUp()
{
    this->mGraphicObject = this->GetGraphicObject();
    long theResult = this->mGraphicObject->LoadMedia();
    this->SetPosition(this->mStart);
    this->SetBoundingRadius(this->mGraphicEffect->GetSize());
    return theResult;
}

// FUNCTION: EXE 0x4e711d
// Draws the puff as a smoke sprite growing from the start size and fading out with age; returns 0
long EEE2DSmokePlume::DrawPass6()
{
    float theReference = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mAlphaReference : 0.0f;
    GEColor theColor;
    AddBlendedSprite(this->mDrawPosition, theColor, this->mRotation,
             (this->mGraphicEffect->GetSize() - this->mGraphicEffect->GetStartSize()) * this->mDrawAge + this->mGraphicEffect->GetStartSize(),
             (1.0f - this->mDrawAge) * this->mGraphicEffect->GetAlpha(), this->mBrightness, theReference);
    return 0;
}

// FUNCTION: EXE 0x4e718e
// Reloads the puff's graphic and reads its point, age, rotation and brightness; returns the first read error
// (the base class result is ignored)
long EEE2DSmokePlume::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect2D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mGraphicObject = this->GetGraphicObject();
    this->mGraphicObject->LoadMedia();
    long theResult = this->mStart.Load(inFile);
    if (theResult != 0)
        return theResult;
    theResult = inFile.Read(this->mAge);
    if (theResult != 0)
        return theResult;
    theResult = inFile.Read(this->mRotation);
    if (theResult == 0)
        theResult = inFile.Read(this->mBrightness);
    return theResult;
}

// FUNCTION: EXE 0x4e71f1
// Writes the puff's point, age, rotation and brightness; returns the first write error (base result ignored)
long EEE2DSmokePlume::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect2D::Save(inMajorVersion, inMinorVersion, outFile);
    long theResult = this->mStart.Save(outFile);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mAge);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mRotation);
    if (theResult == 0)
        theResult = outFile.Write(this->mBrightness);
    return theResult;
}

// FUNCTION: EXE 0x4e7243
// Ages the puff and, unless flag 1 is set, drifts its drawn point with the wind and rises (the effect position
// itself is not moved); returns 7 (remove) once its life is over
long EEE2DSmokePlume::Step()
{
    long theResult = 0;
    float theAge = 0.0f;
    float theElapsed = gPhysics->GetTickSeconds() * 1000.0f;
    theAge = this->mAge += theElapsed / this->mGraphicEffect->mDuration;
    if (this->mAge <= 1.0f) {
        if (!this->mGraphicEffect->mFlag1) {
            GEVector theWind = gWorld.mWindDirection;
            float theStep = theElapsed * 0.25f;
            this->mStart.SetX(this->mStart.GetX() + theAge * theWind.GetX() * theStep);
            this->mStart.SetY(this->mStart.GetY() + theWind.GetY() * this->mAge * theStep);
            this->mStart.SetZ(this->mStart.GetZ() + ((1.0f - this->mAge) * this->mGraphicEffect->GetParam3() * theElapsed
                                         + theWind.GetZ() * this->mAge * theStep));
        }
    } else
        theResult = 7;
    return theResult;
}
