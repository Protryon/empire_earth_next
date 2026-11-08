// C:\EEX\Empire Earth\Graphics Thread\EEE3DBlastWave.cpp (no __FILE__; name from the RTTI class and its key
// "EEE3DBlastWave-Data"). 0xe8ed8..0xe9474: the key chain, the three ctors .. Virtual4C; EEE3DCitizenBeam.cpp's key
// chain follows. Unit flags: the exe's module set.
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
#include "ee/EEEffect3D.h"
#include "ee/EEE3DBlastWave.h"

// SYNTHETIC: EXE 0x4e8ed8
// SYNTHETIC: EXE 0x4e8ee2
// SYNTHETIC: EXE 0x4e8ef3
// SYNTHETIC: EXE 0x4e8eff
const UString EEE3DBlastWave::sData("EEE3DBlastWave-Data");

// FUNCTION: EXE 0x4e8f1b
// Creates a blast wave attached to a unit, aging from 0, sized by the effect
EEE3DBlastWave::EEE3DBlastWave(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect3D(inUnit, inGraphicEffect, inKind), mGraphicObject(NULL), mAge(0.0f)
{
    this->mIsAging = true;
    this->mDrawAge = 0.0f;
    this->mBoundingRadius = this->mGraphicEffect->GetSize();
}

// SYNTHETIC: EXE 0x4e8f69
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4e8f85
// Creates a blast wave at a fixed point, aging from 0, sized by the effect
EEE3DBlastWave::EEE3DBlastWave(const GE3DPoint& inLocation, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect3D(0, inGraphicEffect, inKind)
{
    GETransformation theTransformation;
    theTransformation.SetPosition(inLocation.GetX(), inLocation.GetY(), inLocation.GetZ());
    this->mGraphicObject = NULL;
    this->mIsAging = true;
    this->mAge = 0.0f;
    this->mDrawAge = 0.0f;
    this->mBoundingRadius = this->mGraphicEffect->GetSize();
    this->SetTransformation(theTransformation);
}

// FUNCTION: EXE 0x4e900e
// Creates an empty blast wave of a kind for loading from a saved game
EEE3DBlastWave::EEE3DBlastWave(long inKind)
    : EEEffect3D(inKind)
{
}

// FUNCTION: EXE 0x4e9026
// Destroys the blast wave, unloading its graphic's media
EEE3DBlastWave::~EEE3DBlastWave()
{
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
}

long EEE3DBlastWave::DrawPass2()
{
    return 0;
}

long EEE3DBlastWave::DrawPass4()
{
    return 0;
}

long EEE3DBlastWave::DrawPass5()
{
    return 0;
}

// FUNCTION: EXE 0x4e904a
// Moves the wave to the unit's first tag point of type 0x12 (keeping the unit's orientation), else to the unit
// (EEE3DShield's identical copy is ICF-folded into this one)
void EEE3DBlastWave::FollowUnit()
{
    GE3DPoint thePoint;
    GETransformation theLocation(this->mUnit->mComplexUnit->mPhysicalModel.GetLocation());
    float theScale = this->mUnit->mComplexUnit->mPhysicalModel.GetScale();
    unsigned long theFrame = this->mUnit->mComplexUnit->mPhysicalModel.GetFrame();
    EETagPointList* theList = this->mUnit->mComplexUnit->GetModel()->GetTagPoints(this->mUnit->mComplexUnit->GetAnimation(), 0x12);
    if (theList != NULL && !theList->mPoints.empty()) {
        EETagPointList::Scale(theLocation, theScale);
        const std::vector<EETagPoint*>& thePoints = theList->mPoints;
        thePoints.front()->GetPoint(theFrame, theLocation, thePoint);
        GETransformation theTransformation(false);
        theTransformation = theLocation;
        theTransformation.SetPosition(thePoint.GetX(), thePoint.GetY(), thePoint.GetZ());
        this->SetTransformation(theTransformation);
    } else {
        this->SetTransformation(theLocation);
    }
}

// FUNCTION: EXE 0x4e9145
// Loads the wave's graphic and moves it to its unit if it has one; returns the load result
long EEE3DBlastWave::SetUp()
{
    this->mGraphicObject = this->GetGraphicObject();
    long theResult = this->mGraphicObject->LoadMedia();
    if (this->mUnit != NULL)
        this->FollowUnit();
    return theResult;
}

// FUNCTION: EXE 0x4e916c
// Draws the wave's model additively, shrinking (flag 1) or growing with age, lit by the terrain light and dimming
// over its second half; restores the fog and returns the draw result
// NEAR: the original expands GERasterizer::GetFog (ours calls the import), keeps the fade on the x87 stack across
// the clamps (fld 0.1; fcomp st1) and copies the location twice through temporaries
long EEE3DBlastWave::DrawPass6()
{
    GETransformation theTransformation(this->mTransformation);
    float theValue;
    GEModel* theModel = this->mGraphicObject->mModel;
    GEMaterial* theMaterial = theModel->GetMaterial(0);
    unsigned long theScreen = gScreenIndex;
    float theScale = this->mGraphicEffect->mSize;
    if (this->mGraphicEffect->mGraphicObject)
        gRasterizer->SetAlphaTestingRefValue(this->mGraphicEffect->mGraphicObject->mAlphaReference, theScreen);
    else
        gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
    unsigned long theFog = gRasterizer->GetFog(theScreen);
    float theFade = 1.0f - this->mDrawAge;
    theFade = theFade * theFade;
    if (this->mGraphicEffect->mFlag1) {
        theScale = theFade * theFade * theScale;
        if (theScale < 0.1f)
            theScale = 0.1f;
    } else {
        theScale = max(0.1f, 1.0f - theFade * theFade) * theScale;
    }
    gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
    gRasterizer->SetBlending(kBlendOne, kBlendOne, theScreen);
    gRasterizer->SetShadingColorSource(0, theScreen);
    SetWhiteLighting();
    theMaterial->SetTexture(this->mGraphicObject->mTextures[0], theScreen);
    GE3DPoint theLocation(theTransformation.GetX(), theTransformation.GetY(), theTransformation.GetZ());
    GetFogFactor(theLocation, theFog, theValue);
    theLocation.SetY(theTransformation.GetY());
    theLocation.SetZ(theTransformation.GetZ());
    theTransformation.SetPosition(theTransformation.GetX(), theLocation.GetY(), theLocation.GetZ());
    theModel->SetGraphicsPostModelTransform(theTransformation);
    theModel->SetGraphicsScale(theScale);
    if (this->mDrawAge >= 0.5f) {
        float theLight = (1.0f - this->mDrawAge) * theValue * 2.0f;
        gRasterizer->SetAmbientColor(GEColor(theLight, theLight, theLight), theScreen);
    } else {
        gRasterizer->SetAmbientColor(GEColor(theValue, theValue, theValue), theScreen);
    }
    theModel->PrepareTransforms();
    bool theSorted = gOrthographicView;
    long theResult = gRasterizer->Draw(theModel, theScreen, 1.0f, theSorted);
    gRasterizer->SetFog(theFog, theScreen);
    return theResult;
}

// ReplaceUnit (forwards to EEEffect::ReplaceUnit): ICF-folded into 0xf717b
void EEE3DBlastWave::ReplaceUnit(EEUnit* inOldUnit, EEUnit* inNewUnit)
{
    EEEffect::ReplaceUnit(inOldUnit, inNewUnit);
}

// Queue (render queue 4 under the effect's texture, snapshotting the age): ICF-folded into EEE3DScreenFlash's
// 0xf8db9
long EEE3DBlastWave::Queue(EEEffectQueue* inQueues)
{
    long theResult = EEEffect3D::Queue(inQueues);
    if (theResult == 0) {
        this->mDrawAge = this->mAge;
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[4][theTexture].push_back(this);
    }
    return theResult;
}

// the empty Virtual2C: ICF-folded into 0xe123d
void EEE3DBlastWave::PrepareQueue()
{
}

// FUNCTION: EXE 0x4e9393
// Reloads the wave's graphic and reads its age and aging flag; returns the first error (the base result is ignored)
long EEE3DBlastWave::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect3D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mGraphicObject = this->GetGraphicObject();
    long theResult = this->mGraphicObject->LoadMedia();
    if (theResult == 0)
        theResult = inFile.Read(this->mAge);
    if (theResult == 0)
        theResult = inFile.Read(this->mIsAging);
    return theResult;
}

// FUNCTION: EXE 0x4e93e0
// Relinks the effect's pointers after a game load
long EEE3DBlastWave::RepairPointers()
{
    return EEEffect::RepairPointers();
}

// FUNCTION: EXE 0x4e93e5
// Writes the base 3D effect, the wave's age and aging flag; returns the first write error
long EEE3DBlastWave::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = EEEffect3D::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult == 0)
        theResult = outFile.Write(this->mAge);
    if (theResult == 0)
        theResult = outFile.Write(this->mIsAging);
    return theResult;
}

// FUNCTION: EXE 0x4e9421
// Follows the unit and, while aging, ages the wave by the tick over its duration; returns 7 (remove) past age 1
long EEE3DBlastWave::Step()
{
    long theResult = 0;
    if (this->mUnit != NULL)
        this->FollowUnit();
    if (this->mIsAging) {
        this->mAge += gPhysics->GetTickTime() / this->mGraphicEffect->mDuration;
        if (this->mAge > 1.0f)
            theResult = 7;
    }
    return theResult;
}
