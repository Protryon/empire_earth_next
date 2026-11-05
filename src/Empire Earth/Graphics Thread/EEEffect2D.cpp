// C:\EEX\Empire Earth\Graphics Thread\EEEffect2D.cpp (no __FILE__; name from the RTTI class and its key
// "EEEffect2D-Data"). The base of the 2D effects (vtable 0x43f9a4): 0x10428c..0x1050fb (the key chain, the dtor ..
// Save, then the vector<EEEffect2DPart> template COMDATs and EEEffect2DPart's two inline ctors); EEEffect3D.cpp
// follows. The two EEEffect2D ctors and SetPosition are inline COMDATs emitted by EEE2DBattlefieldAmbient.cpp
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

#include <math.h>

// SYNTHETIC: EXE 0x50428c
// SYNTHETIC: EXE 0x504296
// SYNTHETIC: EXE 0x5042a7
// SYNTHETIC: EXE 0x5042b3
const UString EEEffect2D::sData("EEEffect2D-Data");

// FUNCTION: EXE 0x5042cf
// Destroys the 2D effect, unloading every part's media and releasing its model
EEEffect2D::~EEEffect2D()
{
    unsigned long theCount = this->mParts.size();
    for (unsigned long i = 0; i < theCount; i++)
        this->mParts[i].mGraphicObject->UnloadMedia();
    if (this->mModel) {
        this->mModel->Dereference();
        this->mModel = NULL;
    }
}

// FUNCTION: EXE 0x504332
// Returns 10 (skip) when the effect's tile is missing or unseen by the current player (unless the camera is free) or
// its bounding sphere lies outside the view on any side; else 0
long EEEffect2D::Queue(EEEffectQueue* inQueues)
{
    long theResult = EEEffect::Queue(inQueues);
    if (theResult != 0)
        return theResult;
    GE3DPoint theLocation(this->mPosition.GetX(), this->mPosition.GetY(), this->mPosition.GetZ());
    float theRadius = this->mBoundingRadius;
    GE3DPoint theCameraPoint;
    GE3DPoint thePoint;
    GE3DPoint theScreenPoint;
    long theX = UMFloor(theLocation.GetX());
    long theY = UMFloor(theLocation.GetY());
    bool theOrthographic = gOrthographicView;
    EEMapTile* theTile = gWorld.mMap->GetTile(theX, theY);
    if (theTile == NULL)
        return 10;
    if (!theTile->IsSeenBy(gWorld.GetCurrentPlayer()) && !gFreeCamera)
        return 10;
    gViewport.GetCameraInverse().Apply(theLocation, theCameraPoint);
    float theW;

    thePoint.SetX(theCameraPoint.GetX() + theRadius);
    thePoint.SetY(theCameraPoint.GetY());
    thePoint.SetZ(theCameraPoint.GetZ());
    if (!theOrthographic)
        gViewport.NormalizePerspectiveCameraPoint(thePoint, theScreenPoint, theW);
    else
        gViewport.NormalizeOrthographicCameraPoint(thePoint, theScreenPoint, theW);
    if (theW == 0.0f || theScreenPoint.GetX() / theW < -1.0f)
        return 10;

    thePoint.SetX(theCameraPoint.GetX() - theRadius);
    if (!theOrthographic)
        gViewport.NormalizePerspectiveCameraPoint(thePoint, theScreenPoint, theW);
    else
        gViewport.NormalizeOrthographicCameraPoint(thePoint, theScreenPoint, theW);
    if (theW == 0.0f || theScreenPoint.GetX() / theW > 1.0f)
        return 10;

    thePoint.SetX(theCameraPoint.GetX());
    thePoint.SetZ(theCameraPoint.GetZ() + theRadius);
    if (!theOrthographic)
        gViewport.NormalizePerspectiveCameraPoint(thePoint, theScreenPoint, theW);
    else
        gViewport.NormalizeOrthographicCameraPoint(thePoint, theScreenPoint, theW);
    if (theW == 0.0f || theScreenPoint.GetY() / theW < -gViewport.GetYClippingPlane())
        return 10;

    thePoint.SetZ(theCameraPoint.GetZ() - theRadius);
    if (!theOrthographic)
        gViewport.NormalizePerspectiveCameraPoint(thePoint, theScreenPoint, theW);
    else
        gViewport.NormalizeOrthographicCameraPoint(thePoint, theScreenPoint, theW);
    if (theW == 0.0f || theScreenPoint.GetY() / theW > (double)gViewport.GetYClippingPlane())  // owner-approved stand-in (2026-10-26): the original compares against the global loaded onto the x87 stack (fld; fcompp)
        return 10;

    thePoint.SetZ(theCameraPoint.GetZ());
    thePoint.SetY(theCameraPoint.GetY() + theRadius);
    if (!theOrthographic)
        gViewport.NormalizePerspectiveCameraPoint(thePoint, theScreenPoint, theW);
    else
        gViewport.NormalizeOrthographicCameraPoint(thePoint, theScreenPoint, theW);
    if (theW <= 0.0f || theScreenPoint.GetZ() / theW < 0.0f)
        return 10;

    thePoint.SetY(theCameraPoint.GetY() - theRadius);
    if (!theOrthographic)
        gViewport.NormalizePerspectiveCameraPoint(thePoint, theScreenPoint, theW);
    else
        gViewport.NormalizeOrthographicCameraPoint(thePoint, theScreenPoint, theW);
    if (theW >= 0.0f && (theW <= 0.0f || theScreenPoint.GetZ() / theW > 1.0f))
        return 10;
    return 0;
}

// NEAR: the floor conversions (y first, through a copy) and the register choice of the tag list
// FUNCTION: EXE 0x504623
// Creates one sprite part per tag point of the effect's kind on the unit (else one part at the unit, tag 10000),
// lifting a buried position by inOffset; returns the position and the height (farthest part + effect size)
void EEEffect2D::CreateParts(GE3DPoint& outPosition, float& outHeight, float inOffset)
{
    GE3DPoint theTagPoint;
    GETerrainMesh* theMesh = &gWorld.mMap->mTerrainMesh;
    GE3DPoint thePosition = this->GetUnitLocation();
    float theMaximum = 0.0f;
    float theHeight = 1.0f;
    EEUnit* theUnit = this->mUnit;
    if (theUnit) {
        EEUnitClassUpgrade* theStats = theUnit->mStats;
        EEComplexUnit* theComplexUnit = theUnit->mComplexUnit;
        EETagPointList* theList = theUnit->GetModel()->GetTagPoints(theComplexUnit ? theComplexUnit->mAnimation : 0,
                                                                this->mGraphicEffect->mKind);
        unsigned long theY = UMFloorUnsigned(thePosition.GetY());
        unsigned long theX = UMFloorUnsigned(thePosition.GetX());
        if (theMesh->GetIsValidPoint(theX, theY)) {
            float theZ = theMesh->GetMeshZ(thePosition.GetX(), thePosition.GetY());
            if (thePosition.GetZ() < theZ)
                thePosition.SetZ(theZ + inOffset);
        }
        if (theList && theList->mPoints.size() != 0) {
            GETransformation theTransformation(false);
            float theScale;
            unsigned long theFrame;
            if (theComplexUnit) {
                theTransformation = theComplexUnit->mPhysicalModel.GetLocation();
                theScale = theComplexUnit->mPhysicalModel.GetScale();
                theFrame = theComplexUnit->mPhysicalModel.GetFrame();
            } else {
                theTransformation.MakeIdentity();
                theTransformation.SetPosition(thePosition.GetX(), thePosition.GetY(), thePosition.GetZ());
                theFrame = 0;
                theScale = theStats->mScale;
            }
            EETagPointList::Scale(theTransformation, theScale);
            for (unsigned long i = 0; i < theList->mPoints.size(); i++) {
                EEGraphicObject* theObject = this->GetGraphicObject();
                theObject->LoadMedia();
                theList->mPoints[i]->GetPoint(theFrame, theTransformation, theTagPoint);
                this->mParts.push_back(EEEffect2DPart(theTagPoint, theObject, i));
                float theDistance = thePosition.DistanceToSquared(theTagPoint);
                if (theDistance > theMaximum)
                    theMaximum = theDistance;
            }
            theHeight = (float)sqrt(theMaximum) + this->mGraphicEffect->GetSize();
        } else {
            EEGraphicObject* theObject = this->GetGraphicObject();
            theObject->LoadMedia();
            this->mParts.push_back(EEEffect2DPart(thePosition, theObject, 10000));
            theHeight = this->mGraphicEffect->GetSize();
        }
    }
    outPosition = thePosition;
    outHeight = theHeight;
}

// FUNCTION: EXE 0x5048b1
// Unloads every part's media, drops the parts and the model, then releases the base effect's resources
void EEEffect2D::ReleaseResources()
{
    unsigned long theCount = this->mParts.size();
    for (unsigned long i = 0; i < theCount; i++)
        this->mParts[i].mGraphicObject->UnloadMedia();
    this->mParts.clear();
    if (this->mModel) {
        this->mModel->Dereference();
        this->mModel = NULL;
    }
    EEEffect::ReleaseResources();
}

// FUNCTION: EXE 0x50490e
// Resets a pooled 2D effect onto a new unit and graphic effect with the given model (referenced), with no parts
void EEEffect2D::Reinit(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, GEModel* inModel, bool inKeepsUnit)
{
    EEEffect::Reinit(inUnit, inGraphicEffect, inKeepsUnit);
    this->Clear();
    this->mModel = inModel;
    if (inModel)
        inModel->Reference();
}

// FUNCTION: EXE 0x50493b
// Resets a pooled 2D effect to the empty state with the given model (referenced), with no parts
void EEEffect2D::Reinit(GEModel* inModel, bool inKeepsUnit)
{
    EEEffect::Reinit(inKeepsUnit);
    this->Clear();
    this->mModel = inModel;
    if (inModel)
        inModel->Reference();
}

// FUNCTION: EXE 0x504960
// Drops the parts and the model pointer (without releasing it) and zeroes the position and bounding radius
void EEEffect2D::Clear()
{
    this->mParts.clear();
    this->mPosition.SetX(0.0f);
    this->mPosition.SetY(0.0f);
    this->mPosition.SetZ(0.0f);
    this->mModel = NULL;
    this->mBoundingRadius = 0.0f;
}

// FUNCTION: EXE 0x504985
// Reads the position, bounding radius and parts (position, tag, frame, time, done), reloading each part's media;
// returns the first error, stopping at a part whose media fails (the base class result is ignored)
long EEEffect2D::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect2DPart thePart;
    long theCount;
    EEEffect::Load(inMajorVersion, inMinorVersion, inFile);
    long theResult = this->mPosition.Load(inFile);
    if (theResult == 0) {
        theResult = inFile.Read(this->mBoundingRadius);
        if (theResult == 0) {
            theResult = inFile.Read(theCount);
            if (theResult == 0) {
                for (long i = 0; i < theCount && theResult == 0; i++) {
                    theResult = thePart.mPosition.Load(inFile);
                    if (theResult != 0)
                        continue;
                    theResult = inFile.Read(thePart.mTag);
                    if (theResult != 0)
                        continue;
                    theResult = inFile.Read(thePart.mFrame);
                    if (theResult != 0)
                        continue;
                    theResult = inFile.Read(thePart.mTime);
                    if (theResult != 0)
                        continue;
                    theResult = inFile.Read(thePart.mIsDone);
                    if (theResult != 0)
                        continue;
                    thePart.mGraphicObject = this->GetGraphicObject();
                    theResult = thePart.mGraphicObject->LoadMedia();
                    if (theResult == 0)
                        this->mParts.push_back(thePart);
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x504a8a
// Writes the position, bounding radius and parts (position, tag, frame, time, done); returns the first write error
// (the base class result is ignored)
long EEEffect2D::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect::Save(inMajorVersion, inMinorVersion, outFile);
    long theResult = this->mPosition.Save(outFile);
    if (theResult == 0) {
        theResult = outFile.Write(this->mBoundingRadius);
        if (theResult == 0) {
            theResult = this->mParts.size();
            theResult = outFile.Write(theResult);
            if (theResult == 0) {
                for (unsigned long i = 0; i < this->mParts.size() && theResult == 0; i++) {
                    theResult = this->mParts[i].mPosition.Save(outFile);
                    if (theResult != 0)
                        continue;
                    theResult = outFile.Write(this->mParts[i].mTag);
                    if (theResult != 0)
                        continue;
                    theResult = outFile.Write(this->mParts[i].mFrame);
                    if (theResult != 0)
                        continue;
                    theResult = outFile.Write(this->mParts[i].mTime);
                    if (theResult == 0)
                        theResult = outFile.Write(this->mParts[i].mIsDone);
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x504ba8
// Appends a part to a part list (std::vector<EEEffect2DPart>::push_back; first used by CreateParts)

// FUNCTION: EXE 0x504bd4
// Removes every part from a part list (std::vector<EEEffect2DPart>::clear)

// FUNCTION: EXE 0x504c0f
// Inserts one part before a position (std::vector<EEEffect2DPart>::insert(iterator, const T&))

// FUNCTION: EXE 0x504c69
// Erases a range of parts and returns the position after it (std::vector<EEEffect2DPart>::erase(iterator, iterator))

// FUNCTION: EXE 0x504cb3
// Inserts n copies of a part, growing the storage if needed (vector<EEEffect2DPart>::insert(it, n, const T&))

// FUNCTION: EXE 0x504f6a
// Copy-constructs n parts into raw storage (std::vector<EEEffect2DPart>::_Ufill)

// FUNCTION: EXE 0x504f97
// Copies a range of parts forwards (std::copy<EEEffect2DPart*, iterator>)

// FUNCTION: EXE 0x504fbb
// Copy-constructs a range of parts into raw storage (std::vector<EEEffect2DPart>::_Ucopy)

// FUNCTION: EXE 0x504fed
// Assigns one part to every element of a range (std::fill<iterator, EEEffect2DPart>)

// FUNCTION: EXE 0x50500a
// Copies a range of parts backwards (std::copy_backward<iterator, iterator>)

// FUNCTION: EXE 0x505036
// Copy-constructs one part in place (std::_Construct<EEEffect2DPart, EEEffect2DPart>)

// FUNCTION: EXE 0x50504c
// Copies a part (the implicit EEEffect2DPart::operator=)

// FUNCTION: EXE 0x505089
// Creates an empty part (no graphic object, frame 0, not done)
inline EEEffect2DPart::EEEffect2DPart()
    : mGraphicObject(NULL), mTag(0), mFrame(0), mDrawFrame(0), mTime(0.0f), mIsDone(false)
{
}

// FUNCTION: EXE 0x5050b0
// Creates a part at a position with a graphic object and tag, starting on the object's first start frame
inline EEEffect2DPart::EEEffect2DPart(const GE3DPoint& inPosition, EEGraphicObject* inGraphicObject,
                                      unsigned long inTag)
{
    this->mPosition = inPosition;
    this->mGraphicObject = inGraphicObject;
    this->mTag = inTag;
    this->mFrame = inGraphicObject->mStartFrames[0];
    this->mDrawFrame = 0;
    this->mTime = 0.0f;
    this->mIsDone = false;
}
