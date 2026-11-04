// C:\EEX\Empire Earth\Graphics Thread\EEE2DShockwave.cpp (no __FILE__; name from the RTTI class and its key
// "EEE2DShockwave-Data"). 0xe614f..0xe6621: the key chain, three ctors .. Step; EEE2DSmokeMuzzleBlast.cpp
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
#include "ee/EEE2DShockwave.h"
#include "ee/EEEffectKindIDs.h"

// SYNTHETIC: EXE 0x4e614f
// SYNTHETIC: EXE 0x4e6159
// SYNTHETIC: EXE 0x4e616a
// SYNTHETIC: EXE 0x4e6176
const UString EEE2DShockwave::sData("EEE2DShockwave-Data");

// FUNCTION: EXE 0x4e6192
// Creates a shockwave on a unit at age 0, with no graphic yet
EEE2DShockwave::EEE2DShockwave(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect)
    : EEEffect2D(inUnit, inGraphicEffect, gU4E8DF4, kEffect2DShockwave, false), mGraphicObject(NULL), mAge(0.0f)
{
    this->mDrawAge = 0.0f;
}

// SYNTHETIC: EXE 0x4e61c6
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4e61e2
// Creates a shockwave at a point at age 0, with no unit or graphic yet
EEE2DShockwave::EEE2DShockwave(const GE3DPoint& inPosition, EEGraphicEffect* inGraphicEffect)
    : EEEffect2D(0, inGraphicEffect, gU4E8DF4, kEffect2DShockwave, false), mGraphicObject(NULL), mAge(0.0f), mDrawAge(0.0f)
{
    this->SetPosition(inPosition);
}

// FUNCTION: EXE 0x4e621f
// Creates an empty shockwave for loading from a saved game
EEE2DShockwave::EEE2DShockwave()
    : EEEffect2D(gU4E8DF4, kEffect2DShockwave, false)
{
}

// FUNCTION: EXE 0x4e623b
// Destroys the shockwave, unloading its graphic's media
EEE2DShockwave::~EEE2DShockwave()
{
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
}

long EEE2DShockwave::DrawPass2()
{
    return 0;
}

long EEE2DShockwave::DrawPass4()
{
    return 0;
}

long EEE2DShockwave::DrawPass5()
{
    return 0;
}

long EEE2DShockwave::Queue(EEEffectQueue* inQueues)
{
    long theResult = EEEffect2D::Queue(inQueues);
    if (theResult == 0) {
        this->mDrawAge = this->mAge;
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[4][theTexture].push_back(this);
    }
    return theResult;
}

void EEE2DShockwave::PrepareQueue()
{
}

// NEAR: begin() of the tag list is expanded, operator+ out of line in the original; register choice
// FUNCTION: EXE 0x4e6259
// Places a unit's shockwave on a random one of its type-4 tag points (at the unit if it has none), then
// loads its graphic; returns the load result
long EEE2DShockwave::SetUp()
{
    GE3DPoint thePosition;
    if (this->mUnit) {
        GETransformation theTransformation(false);
        theTransformation = this->mUnit->mComplexUnit->mPhysicalModel.GetLocation();
        EEComplexUnit* theUnit = this->mUnit->mComplexUnit;
        EETagPointList* theList = theUnit->GetModel()->GetTagPoints(theUnit->GetAnimation(), 4);
        float theScale = theUnit->mPhysicalModel.GetScale();
        unsigned long theFrame = theUnit->mPhysicalModel.GetFrame();
        if (theList && theList->mPoints.size() != 0) {
            long theIndex = UMFloatToLong(EEEffect::GetRandom() * theList->mPoints.size());
            if (theIndex == theList->mPoints.size())
                theIndex = theList->mPoints.size() - 1;
            theList->Scale(theTransformation, theScale);
            theList->mPoints[theIndex]->GetPoint(theFrame, theTransformation, thePosition);
        } else {
            // no type-4 tag points: at the unit itself, not at the map origin
            thePosition = GE3DPoint(theTransformation);
        }
        this->SetPosition(thePosition);
    }
    this->SetBoundingRadius(this->mGraphicEffect->GetSize());
    this->mGraphicObject = this->GetGraphicObject();
    return this->mGraphicObject->LoadMedia();
}

// NEAR: register choice (model ebx, screen in a frame slot) and the size copies
// FUNCTION: EXE 0x4e6383
// Draws the wave as an additive camera-facing sprite expanding with age (eased in or out by flag 1), fading out
// over its second half; returns the draw result
long EEE2DShockwave::DrawPass6()
{
    float theValue;
    GETransformation theTransformation(false);
    GEModel* theModel = this->mModel;
    GEMaterial* theMaterial = theModel->GetMaterial(0);
    unsigned long theScreen = gScreenIndex;
    if (this->mGraphicEffect->mGraphicObject)
        gRasterizer->SetAlphaTestingRefValue(this->mGraphicEffect->mGraphicObject->mAlphaReference, theScreen);
    else
        gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
    unsigned long theFog = gRasterizer->GetFog(theScreen);
    float theFade = (1.0f - this->mDrawAge) * (1.0f - this->mDrawAge);
    float theFactor = this->mGraphicEffect->mFlag1 ? theFade * theFade : 1.0f - theFade * theFade;
    gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
    gRasterizer->SetBlending(kBlendOne, kBlendOne, theScreen);
    gRasterizer->SetShadingColorSource(0, theScreen);
    Fn10F7C8();
    theMaterial->SetTexture(this->mGraphicObject->mTextures[0], theScreen);
    float theStartSize = this->mGraphicEffect->GetStartSize();
    float theEndSize = this->mGraphicEffect->GetSize();
    float theFrom = this->mGraphicEffect->GetStartSize();
    theTransformation.SetOrientationYPR(gWorld.GetCameraYaw(), 0.0f, 0.0f);
    const GE3DPoint& thePosition = this->mPosition;
    theTransformation.SetPosition(thePosition.GetX(), thePosition.GetY(), thePosition.GetZ());
    theTransformation.SetElement(3, 0, 0.0f);
    theTransformation.SetElement(3, 1, 0.0f);
    theTransformation.SetElement(3, 2, 0.0f);
    theTransformation.SetElement(3, 3, 1.0f);
    theModel->SetGraphicsPostModelTransform(theTransformation);
    theModel->SetGraphicsScale((theEndSize - theFrom) * theFactor + theStartSize);
    Fn11A130(thePosition, theFog, theValue);
    if (this->mDrawAge < 0.5f) {
        gRasterizer->SetAmbientColor(GEColor(theValue, theValue, theValue), theScreen);
    } else {
        float theLight = (1.0f - this->mDrawAge) * theValue * 2.0f;
        gRasterizer->SetAmbientColor(GEColor(theLight, theLight, theLight), theScreen);
    }
    theModel->PrepareTransforms();
    bool theSorted = gOrthographicView;
    long theResult = gRasterizer->Draw(theModel, theScreen, 1.0f, theSorted);
    gRasterizer->SetFog(theFog, theScreen);
    return theResult;
}

// FUNCTION: EXE 0x4e6585
// Reloads the shockwave's graphic and reads its age; returns the read result (base result ignored)
long EEE2DShockwave::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect2D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mGraphicObject = this->GetGraphicObject();
    this->mGraphicObject->LoadMedia();
    return inFile.Read(this->mAge);
}

// FUNCTION: EXE 0x4e65bc
// Writes the shockwave's age; returns the write result (the base class result is ignored)
long EEE2DShockwave::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect2D::Save(inMajorVersion, inMinorVersion, outFile);
    return outFile.Write(this->mAge);
}

// FUNCTION: EXE 0x4e65e2
// Ages the shockwave by the tick over the effect's duration; returns 7 (remove) once its life is over
long EEE2DShockwave::Step()
{
    long theResult = 0;
    this->mAge += (gPhysics->GetTickSeconds() * 1000.0f) / this->mGraphicEffect->mDuration;
    if (this->mAge > 1.0f)
        theResult = 7;
    return theResult;
}
