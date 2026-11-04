// C:\EEX\Empire Earth\Graphics Thread\EEE3DDamageVent.cpp (no __FILE__; name from the RTTI class and the key).
// 0xea9e2..0xebbba: the key chain, ctors .. Virtual4C, the jet's ctor/Aim, then vector<EEE3DDamageVentPart>'s members;
// EEE3DDestructionSmoke.cpp follows.
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
#include "lle/SESoundManager.h"

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
#include "ee/EEGameSound.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEGraphicObject.h"
#include "ee/EERenderObject.h"
#include "ee/EEEffect.h"
#include "ee/EEEffect2D.h"
#include "ee/EEEffect3D.h"
#include "ee/EEEffectManager.h"
#include "ee/EEE3DDamageVent.h"
#include "ee/EEEffectKindIDs.h"

// SYNTHETIC: EXE 0x4ea9e2
// SYNTHETIC: EXE 0x4ea9ec
// SYNTHETIC: EXE 0x4ea9fd
// SYNTHETIC: EXE 0x4eaa09
const UString EEE3DDamageVent::sData("EEE3DDamageVentData");

// the jets' base orientation (0x440514: x stays, y -> z, z -> -y)
static const float kVentRotation[4][4] = {
    {1.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 1.0f, 0.0f},
    {0.0f, -1.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 1.0f}
};

// FUNCTION: EXE 0x4eaa25
// Creates a damage vent (fire or steam jets) on a unit, with no jets yet
EEE3DDamageVent::EEE3DDamageVent(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect)
    : EEEffect3D(inUnit, inGraphicEffect, kEffect3DDamageVent)
{
    this->mGraphicObject = NULL;
    this->mNextCountCheckTime = 0.0f;
}

// SYNTHETIC: EXE 0x4eaa76
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4eaa92
// Creates an empty damage vent for loading from a saved game
EEE3DDamageVent::EEE3DDamageVent()
    : EEEffect3D(kEffect3DDamageVent)
{
    this->mNextCountCheckTime = 0.0f;
}

// FUNCTION: EXE 0x4eaac0
// Destroys the vent, unloading its graphic's media
EEE3DDamageVent::~EEE3DDamageVent()
{
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
}

long EEE3DDamageVent::DrawPass2()
{
    return 0;
}

long EEE3DDamageVent::DrawPass4()
{
    return 0;
}

long EEE3DDamageVent::DrawPass5()
{
    return 0;
}

// FUNCTION: EXE 0x4eaaec
// Queues the vent in render queue 4 under its texture, snapshotting each jet's length, frame and transforms
long EEE3DDamageVent::Queue(EEEffectQueue* inQueues)
{
    long theResult = EEEffect3D::Queue(inQueues);
    if (theResult == 0) {
        for (std::vector<EEE3DDamageVentPart>::iterator thePart = this->mParts.begin(); thePart != this->mParts.end(); thePart++) {
            thePart->mDrawLength = thePart->mLength;
            thePart->mDrawFrame = thePart->mFrame;
            thePart->mDrawTransformation = thePart->mTransformation;
            thePart->mDrawOrientation = thePart->mOrientation;
        }
        this->mDrawCount = this->mParts.size();
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[4][theTexture].push_back(this);
    }
    return theResult;
}

// the empty Virtual2C: ICF-folded into 0xe123d
void EEE3DDamageVent::PrepareQueue()
{
}

// FUNCTION: EXE 0x4eabb5
// Loads the vent's graphic; its bounding radius is the unit class's scale plus the effect's size
long EEE3DDamageVent::SetUp()
{
    this->mGraphicObject = this->GetGraphicObject();
    long theResult = this->mGraphicObject->LoadMedia();
    this->mBoundingRadius = this->mUnit->GetClass()->GetScale() + this->mGraphicEffect->GetSize();
    return theResult;
}

// FUNCTION: EXE 0x4eabe0
// Draws each snapshotted jet's model scaled by its length and at its animation frame, additive or alpha blended
// by the effect's flag 2; restores the fog and returns the last draw result
long EEE3DDamageVent::DrawPass6()
{
    long theResult = 0;
    unsigned long theScreen = gScreenIndex;
    GEModel* theModel = this->mGraphicObject->mModel;
    bool theAdditive = this->mGraphicEffect->mFlag2;
    float theValue;
    if (this->mGraphicEffect->mGraphicObject)
        gRasterizer->SetAlphaTestingRefValue(this->mGraphicEffect->mGraphicObject->mAlphaReference, theScreen);
    else
        gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
    unsigned long theFog = gRasterizer->GetFog(theScreen);
    gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
    gRasterizer->SetShadingColorSource(0, theScreen);
    if (theAdditive)
        gRasterizer->SetBlending(kBlendOne, kBlendOne, theScreen);
    else
        gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
    Fn10F7C8();
    theModel->SetTexture(this->mGraphicObject->mTextures[0], theScreen);
    if (theAdditive) {
        GETransformation theTransformation(this->mTransformation);
        GE3DPoint theLocation(theTransformation.GetX(), theTransformation.GetY(), theTransformation.GetZ());
        Fn11A130(theLocation, theFog, theValue);
    }
    for (long i = 0; i < (long)this->mDrawCount && (unsigned long)i < this->mParts.size(); i++) {
        EEE3DDamageVentPart& thePart = this->mParts[i];
        theModel->SetGraphicsPreModelTransform(thePart.mDrawOrientation);
        GETransformation theScale;
        theScale.SetScale(thePart.mDrawLength, thePart.mDrawLength, thePart.mDrawLength);
        GETransformation theTransformation;
        GETransformation theLocation(thePart.mDrawTransformation);
        theLocation.Apply(theScale, theTransformation);
        theModel->SetGraphicsPostModelTransform(theTransformation);
        this->mGraphicObject->mTextures[0]->SetFrame(thePart.mDrawFrame);
        theModel->PrepareTransforms();
        bool theSorted = gOrthographicView;
        theResult = gRasterizer->Draw(theModel, theScreen, 1.0f, theSorted);
    }
    gRasterizer->SetFog(theFog, theScreen);
    return theResult;
}

// FUNCTION: EXE 0x4eae0a
// Reads the base 3D effect (result ignored) and reloads the graphic; returns the load result
long EEE3DDamageVent::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect3D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mGraphicObject = this->GetGraphicObject();
    return this->mGraphicObject->LoadMedia();
}

// Save (writes the base 3D effect): ICF-folded into EEE3DAdditivePass's 0xe8e01
long EEE3DDamageVent::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    return EEEffect3D::Save(inMajorVersion, inMinorVersion, outFile);
}

// NEAR: tag-list begin()/_Ptrit::operator+ out of line in the original, SetPosition(MMM) imported, frame slots
// FUNCTION: EXE 0x4eae33
// Every 2 s matches the number of jets to the unit's damage (one per vent tag point at most, new ones on free tags);
// grows or shrinks each jet, animates it and emits linked sub-effects; returns 7 when the unit dies or heals
long EEE3DDamageVent::Step()
{
    EEUnit* theUnit = this->mUnit;
    if (!theUnit)
        return 7;
    EEComplexUnit* theComplex = theUnit->mComplexUnit;
    EETagPointList* theFromList = theUnit->GetModel()->GetTagPoints(theComplex->GetAnimation(), 5);
    EETagPointList* theToList = theUnit->GetModel()->GetTagPoints(theComplex->GetAnimation(), 0x38);
    float theHealth = (float)this->mUnit->mHitPoints;
    theHealth /= this->mUnit->mStats->mMaxHitPoints;
    EEGraphicEffect* theEffect = this->mGraphicEffect;
    if ((theComplex->mCommand ? theComplex->mCommand->mType : 0) == kActionDead)
        return 7;
    if (theHealth >= theEffect->GetParam2())
        return 7;
    GE3DPoint thePosition;
    theUnit->GetPosition(thePosition);
    GETransformation theTransformation;
    theTransformation.SetPosition(thePosition);
    theTransformation.SetOrientationYPR(0.0f, 0.0f, 0.0f);
    theTransformation.SetElement(3, 0, 0.0f);
    theTransformation.SetElement(3, 1, 0.0f);
    theTransformation.SetElement(3, 2, 0.0f);
    theTransformation.SetElement(3, 3, 1.0f);
    this->SetTransformation(theTransformation);
    float theElapsed = gPhysics->GetTickTime();
    this->mNextCountCheckTime -= theElapsed;
    if (this->mNextCountCheckTime <= 0.0f) {
        this->mNextCountCheckTime = 2000.0f;
        unsigned long theCount = theFromList->GetNumTagPoints();
        unsigned long theParts = this->mParts.size();
        unsigned long theWanted = UMFloor((theEffect->GetParam2() - theHealth) * theCount / theEffect->GetParam2()) + 1;
        if (theWanted > theCount)
            theWanted = theCount;
        if (theParts > theWanted) {
            for (long i = theParts - theWanted; i > 0; i--)
                this->mParts.pop_back();
        } else if (theParts < theWanted) {
            long theTag = Fn102F57(0, theCount - 1);
            std::vector<EEE3DDamageVentPart>::iterator theOther = this->mParts.begin();
            while (theOther != this->mParts.end()) {
                if (theTag != theOther->mTag) {
                    theOther++;
                } else {
                    theTag++;
                    if (theTag == (long)theCount)
                        theTag = 0;
                    theOther = this->mParts.begin();
                }
            }
            EEE3DDamageVentPart thePart;
            thePart.mTag = theTag;
            thePart.mSpeed = Fn102F3D(theEffect->GetParam3(), theEffect->GetParam4());
            thePart.mFrame = 0;
            thePart.mLength = 0.0f;
            thePart.mTime = 0.0f;
            thePart.mF54 = 0.0f;
            thePart.mNextSubEffectTime = 0.0f;
            if (theEffect->mFlag1 == false)
                thePart.Aim(theFromList, theToList);
            this->mParts.push_back(thePart);
        }
    }
    if (this->mParts.size() > 0) {
        float theScale = (theEffect->GetParam2() - theHealth) / theEffect->GetParam2()
                         * (theEffect->GetSize() - theEffect->GetStartSize()) + theEffect->GetStartSize();
        for (std::vector<EEE3DDamageVentPart>::iterator thePart = this->mParts.begin(); thePart != this->mParts.end(); thePart++) {
            float theTarget = theScale * thePart->mSpeed;
            if (theTarget > thePart->mLength) {
                thePart->mLength += theEffect->GetParam5() * theElapsed / 1000.0f;
                if (thePart->mLength > theTarget)
                    thePart->mLength = theTarget;
            } else if (theTarget < thePart->mLength) {
                thePart->mLength -= theEffect->GetParam5() * theElapsed / 1000.0f;
                if (thePart->mLength < theTarget)
                    thePart->mLength = theTarget;
            }
            thePart->mTime -= theElapsed;
            if (thePart->mTime <= 0.0f) {
                thePart->mFrame++;
                if (thePart->mFrame >= this->mGraphicObject->mFrameCounts[0])
                    thePart->mFrame = 0;
                thePart->mTime = (float)this->mGraphicObject->mFrameTimes[0];
            }
            if (theEffect->mFlag1)
                thePart->Aim(theFromList, theToList);
            const GETransformation& theLocation = theComplex->mPhysicalModel.GetLocation();
            GETransformation theUnitTransformation(theLocation);
            theFromList->Scale(theUnitTransformation, theComplex->mStats->GetScale());
            GE3DPoint theFrom;
            GE3DPoint theTo;
            (theFromList->GetTagPoint(thePart->mTag))->GetPoint(0, theUnitTransformation, theFrom);
            (theToList->GetTagPoint(thePart->mTag))->GetPoint(0, theUnitTransformation, theTo);
            GEVector theDirection(theTo.GetX() - theFrom.GetX(), theTo.GetY() - theFrom.GetY(),
                                  theTo.GetZ() - theFrom.GetZ());
            theDirection.Normalize();
            theDirection.SetX(theDirection.GetX() * thePart->mLength);
            theDirection.SetY(theDirection.GetY() * thePart->mLength);
            theDirection.SetZ(theDirection.GetZ() * thePart->mLength);
            thePart->mTransformation = theLocation;
            GEModel* theModel = theComplex->GetModel()->mModels[0];
            GE3DPoint theOffset;
            (*(theFromList->mPoints.begin() + thePart->mTag))->GetOffset(0, theOffset);
            GE3DPoint theCenter = theModel->GetCenterPoint();
            GEVector theTranslation;
            theUnitTransformation.Apply(GEVector(theOffset.GetX() - theCenter.GetX(),
                                                 theOffset.GetY() - theCenter.GetY(),
                                                 theOffset.GetZ() - theCenter.GetZ()), theTranslation);
            thePart->mTransformation.Translate(theDirection);
            thePart->mTransformation.Translate(theTranslation);
            if (!theEffect->mLinkedEffect1)
                continue;
            thePart->mNextSubEffectTime -= theElapsed;
            if (!(thePart->mNextSubEffectTime <= 0.0f))
                continue;
            thePart->mNextSubEffectTime = Fn102F3D(theEffect->GetParam6(), theEffect->GetParam7());
            float theSpread = theEffect->mParam8;
            GE3DPoint thePlace((0.5f - EEEffect::GetRandom()) * theSpread + theFrom.GetX(),
                               (0.5f - EEEffect::GetRandom()) * theSpread + theFrom.GetY(),
                               (0.5f - EEEffect::GetRandom()) * theSpread + theFrom.GetZ());
            gEffectManager->CreatePlacedEffect(thePlace, theEffect->mLinkedEffect1, NULL);
        }
    }
    return 0;
    return 7;
}

// FUNCTION: EXE 0x4eb4c0
// Creates an empty vent jet (the implicit default ctor)

// FUNCTION: EXE 0x4eb4e9
// Points the jet from its tag point in the first list towards the same tag in the second (rotated to the jet axis)
void EEE3DDamageVentPart::Aim(const EETagPointList* inFrom, const EETagPointList* inTo)
{
    GE3DPoint theFrom;
    GE3DPoint theTo;
    inFrom->mPoints[this->mTag]->GetOffset(0, theFrom);
    inTo->mPoints[this->mTag]->GetOffset(0, theTo);
    GETransformation theRotation(kVentRotation);
    GETransformation theLook(false);
    theLook.MakeLookAtTransform(theFrom, theTo);
    theLook.Apply(theRotation, theLook);
    this->mOrientation = theLook;
}

// FUNCTION: EXE 0x4eb58e
// FUNCTION: EXE 0x4eb5ba
// FUNCTION: EXE 0x4eb5ee
// FUNCTION: EXE 0x4eb64b
// FUNCTION: EXE 0x4eb6ae
// FUNCTION: EXE 0x4eb6e8
// FUNCTION: EXE 0x4eb6fd
// FUNCTION: EXE 0x4eb714
// FUNCTION: EXE 0x4eb9fc
// FUNCTION: EXE 0x4eba77
// FUNCTION: EXE 0x4ebaa8
// FUNCTION: EXE 0x4ebadc
// FUNCTION: EXE 0x4ebafc
// FUNCTION: EXE 0x4ebb2d
// FUNCTION: EXE 0x4ebb3f
// Implement vector<EEE3DDamageVentPart>'s members (COMDATs from <vector>)
