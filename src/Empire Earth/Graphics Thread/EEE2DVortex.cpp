// C:\EEX\Empire Earth\Graphics Thread\EEE2DVortex.cpp (no __FILE__; name from the RTTI class and its key
// "EEE2DVortex-Data"). 0xe773c..0xe7d67: the key chain, both ctors .. Step, then vector<long>::_Buy (the
// vectors' ICF-kept copy); EEE2DWaterRipple.cpp follows.
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
#include "ee/EEE2DVortex.h"
#include "LostDebug.h"
#include "ee/EEEffectKindIDs.h"

// SYNTHETIC: EXE 0x4e773c
// SYNTHETIC: EXE 0x4e7746
// SYNTHETIC: EXE 0x4e7757
// SYNTHETIC: EXE 0x4e7763
const UString EEE2DVortex::sData("EEE2DVortex-Data");

// FUNCTION: EXE 0x4e777f
// Creates a vortex at a point with no orbitals and no graphic
EEE2DVortex::EEE2DVortex(const GE3DPoint& inPosition, EEGraphicEffect* inGraphicEffect)
    : EEEffect2D(0, inGraphicEffect, gU4E8DF4, kEffect2DVortex, false),
      mAge(0.0f), mFrame(0), mOrbitalCount(0)
{
    // A new vortex has no orbitals until Load copies them: the count must start at zero.
    this->mGraphicObject = NULL;
    this->SetPosition(inPosition);
}

// SYNTHETIC: EXE 0x4e77e3
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4e77ff
// Creates an empty vortex for loading from a saved game
EEE2DVortex::EEE2DVortex()
    : EEEffect2D(gU4E8DF4, kEffect2DVortex, false)
{
}

// FUNCTION: EXE 0x4e784d
// Destroys the vortex, unloading its graphic's media
EEE2DVortex::~EEE2DVortex()
{
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
}

long EEE2DVortex::DrawPass2()
{
    return 0;
}

long EEE2DVortex::DrawPass4()
{
    return 0;
}

long EEE2DVortex::DrawPass6()
{
    return 0;
}

// FUNCTION: EXE 0x4e787e
// Queues the vortex in render queue 3 under its texture, snapshotting its frame, age, spin and position
long EEE2DVortex::Queue(EEEffectQueue* inQueues)
{
    long theResult = EEEffect2D::Queue(inQueues);
    if (theResult == 0) {
        this->mDrawFrame = this->mFrame;
        this->mDrawAge = this->mAge;
        this->mDrawSpin = this->mSpin;
        this->mDrawPosition = this->mPosition;
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[3][theTexture].push_back(this);
    }
    return theResult;
}

void EEE2DVortex::PrepareQueue()
{
}

// FUNCTION: EXE 0x4e790f
// Loads the vortex's graphic and starts it at age 0 and spin 0.1; returns the load result
long EEE2DVortex::SetUp()
{
    this->mGraphicObject = this->GetGraphicObject();
    long theResult = this->mGraphicObject->LoadMedia();
    this->mAge = 0.0f;
    this->mDrawSpin = this->mSpin = 0.1f;
    this->SetBoundingRadius(this->mGraphicEffect->GetSize());
    return theResult;
}

// NEAR: the transformation pointer is pushed after the z sum in the original
// FUNCTION: EXE 0x4e7949
// Poses the vortex model and its orbitals 0.2 above its point, draws it, fading out over the last 10%
// of its life; returns the draw result
long EEE2DVortex::DrawPass5()
{
    // LOST-DEBUG: none independent: as EEE3DDisease::DrawPass5, the budget that makes mAngles[i] and
    // mOrbitals[i] call operator+ out of line

    std::vector<long> theUnused;
    GETransformation theTransformation;
    GETransformation theOrientation;
    GEModel* theModel = this->mGraphicObject->mModel;
    float theScale = this->mGraphicEffect->mSize;
    unsigned long theScreen = gScreenIndex;
    long theResult = 0;
    if (this->mGraphicEffect->mGraphicObject)
        gRasterizer->SetAlphaTestingRefValue(this->mGraphicEffect->mGraphicObject->mAlphaReference, theScreen);
    else
        gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
    gRasterizer->SetZBuffer(kZBufferTestAndWrite, theScreen);
    gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
    gRasterizer->SetShadingColorSource(0, theScreen);
    Fn10FA01();
    theTransformation.SetX(this->mDrawPosition.GetX());
    theTransformation.SetY(this->mDrawPosition.GetY());
    theTransformation.SetZ(this->mDrawPosition.GetZ() + 0.2f);
    theModel->SetGraphicsPostModelTransform(theTransformation);
    theModel->SetGraphicsCurrentFrame(this->mDrawFrame);
    theModel->SetGraphicsScale(theScale);
    for (long i = 0; i < this->mOrbitalCount; i++) {
        theOrientation.SetOrientationYPR(this->mAngles[i], 0.0f, 0.0f);
        this->mOrbitals[i]->mModel->SetGraphicsPreModelTransform(theOrientation);
    }
    theModel->PrepareTransforms();
    theModel->GetMaterial(0)->SetTexture(this->mGraphicObject->mTextures[0], theScreen);
    // The vortex is drawn for its whole life and fades out over its last 10%.
    if (this->mDrawAge >= 0.9f)
        theModel->SetOpacity((1.0f - this->mDrawAge) * 10.0f);
    bool theSorted = gOrthographicView;
    theResult = gRasterizer->Draw(theModel, theScreen, 1.0f, theSorted);
    theModel->SetOpacity(1.0f);
    return theResult;
}

// FUNCTION: EXE 0x4e7b50
// Reloads the vortex's graphic, copies its orbitals and reads age and spin; returns the first read error (base
// result ignored)
long EEE2DVortex::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect2D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mGraphicObject = this->GetGraphicObject();
    this->mGraphicObject->LoadMedia();
    this->mOrbitalCount = this->mGraphicObject->GetOrbitalCount();
    this->mAngles.resize(this->mOrbitalCount, 0.0f);
    for (long i = 0; i < this->mOrbitalCount; i++)
        this->mOrbitals.push_back(new EEOrbital(*this->mGraphicObject->GetOrbital(i)));
    long theResult = inFile.Read(this->mAge);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mSpin);
    return theResult;
}

// FUNCTION: EXE 0x4e7c1c
// Writes the vortex's age and spin; returns the first write error (the base class result is ignored)
long EEE2DVortex::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect2D::Save(inMajorVersion, inMinorVersion, outFile);
    long theResult = outFile.Write(this->mAge);
    if (theResult == 0)
        theResult = outFile.Write(this->mSpin);
    return theResult;
}

// FUNCTION: EXE 0x4e7c57
// Ages and spins the vortex, follows its unit if any and updates its orbitals; returns 7 (remove) once its life
// is over
long EEE2DVortex::Step()
{
    long theResult = 0;
    this->mAge += gPhysics->GetTickTime() / this->mGraphicEffect->mDuration;
    this->mSpin += this->mGraphicEffect->GetParam1() * gPhysics->GetTickTime();
    if (this->mSpin > 1.0f)
        this->mSpin -= 1.0f;
    if (this->mAge <= 1.0f) {
        if (this->mUnit && this->mUnit->mComplexUnit) {
            GE3DPoint thePosition(this->mUnit->mComplexUnit->mPhysicalModel.GetLocation());
            this->SetPosition(thePosition);
        }
        for (long i = 0; i < this->mOrbitalCount; i++)
            this->mOrbitals[i]->Update();
    } else {
        theResult = 7;
    }
    return theResult;
}

// FUNCTION: EXE 0x4e7d2e
// Allocates room for n elements (std::vector<long>::_Buy; the ICF-kept copy for the pointer/float vectors)
// (first used by the ctors)
