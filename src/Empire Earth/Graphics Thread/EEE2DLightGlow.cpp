// C:\EEX\Empire Earth\Graphics Thread\EEE2DLightGlow.cpp (no __FILE__; name from the RTTI class and its key
// "EEE2DLightGlow-Data"). 0xe1a37..0xe28be: the key chain, both ctors .. PlaceGlows; EEE2DLightning.cpp follows.
#include <windows.h>
#include <stdio.h>

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
#include "lle/GEModelManager.h"
#include "lle/GEPolygonArray.h"

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
#include "ee/EEE2DLightGlow.h"
#include "ee/EEEffectKindIDs.h"

// SYNTHETIC: EXE 0x4e1a37
// SYNTHETIC: EXE 0x4e1a41
// SYNTHETIC: EXE 0x4e1a52
// SYNTHETIC: EXE 0x4e1a5e
const UString EEE2DLightGlow::sData("EEE2DLightGlow-Data");
GEModel* EEE2DLightGlow::sModel = 0;
// the colour components of a light tag point name are 0..255: `/ 255.0f` (VC6 multiplies by the unrounded reciprocal,
// the pooled __real at 0x43f7e8, also read by 0x4db3fa)

// FUNCTION: EXE 0x4e1a7a
// Creates a light glow effect on a unit with no glow spots yet, shown and at age 0
EEE2DLightGlow::EEE2DLightGlow(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, bool inKeepsUnit)
    : EEEffect2D(inUnit, inGraphicEffect, gEffect2DModel, kEffect2DLightGlow, inKeepsUnit)
{
    this->mAge = 0.0f;
    this->mGraphicObject = NULL;
    this->mSpots = NULL;
    this->mSpotCount = 0;
    this->mHidden = false;
}

// SYNTHETIC: EXE 0x4e1ab5
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4e1ad1
// Creates an empty light glow for loading from a saved game, building the shared glow model on first use
EEE2DLightGlow::EEE2DLightGlow()
    : EEEffect2D(gEffect2DModel, kEffect2DLightGlow, false)
{
    this->mGraphicObject = NULL;
    this->mSpots = NULL;
    if (EEE2DLightGlow::sModel == NULL)
        this->CreateModel();
}

// FUNCTION: EXE 0x4e1b05
// Destroys the glow, unloading its graphic's media and freeing its spots
EEE2DLightGlow::~EEE2DLightGlow()
{
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
    if (this->mSpots)
        delete[] this->mSpots;
    this->mSpots = NULL;
}

// FUNCTION: EXE 0x4e1b35
// Builds the shared "light glow" model (two nested textured quads) and registers its release at shutdown
void EEE2DLightGlow::CreateModel()
{
    GEColor theColor;
    GETextureCoordinate theCoordinate0(0.0f, 0.0f);
    GETextureCoordinate theCoordinate1(1.0f, 0.0f);
    GETextureCoordinate theCoordinate2(0.0f, 1.0f);
    GETextureCoordinate theCoordinate3(1.0f, 1.0f);
    GEVector theNormal(0.0f, -1.0f, 0.0f);
    EEE2DLightGlow::sModel = gModelManager->CreateModel(4, 8, 1, 0, 1, 0, "light glow", 1);
    EEE2DLightGlow::sModel->GetVertexArray(0)->SetPoint(GE3DPoint(-0.5f, 0.0f, 0.5f), 0);
    EEE2DLightGlow::sModel->GetVertexArray(0)->SetPoint(GE3DPoint(0.5f, 0.0f, 0.5f), 1);
    EEE2DLightGlow::sModel->GetVertexArray(0)->SetPoint(GE3DPoint(-0.5f, 0.0f, -0.5f), 2);
    EEE2DLightGlow::sModel->GetVertexArray(0)->SetPoint(GE3DPoint(0.5f, 0.0f, -0.5f), 3);
    EEE2DLightGlow::sModel->GetVertexArray(0)->SetPoint(GE3DPoint(-0.25f, 0.0f, 0.25f), 4);
    EEE2DLightGlow::sModel->GetVertexArray(0)->SetPoint(GE3DPoint(0.25f, 0.0f, 0.25f), 5);
    EEE2DLightGlow::sModel->GetVertexArray(0)->SetPoint(GE3DPoint(-0.25f, 0.0f, -0.25f), 6);
    EEE2DLightGlow::sModel->GetVertexArray(0)->SetPoint(GE3DPoint(0.25f, 0.0f, -0.25f), 7);
    for (unsigned long i = 0; i < 8; i++) {
        EEE2DLightGlow::sModel->GetVertexArray(0)->SetNormal(theNormal, i);
        EEE2DLightGlow::sModel->GetVertexArray(0)->SetColor(theColor, i);
        EEE2DLightGlow::sModel->GetVertexArray(0)->SetOpacity(1.0f, i);
    }
    EEE2DLightGlow::sModel->GetMaterial(0)->SetPolygonStartingIndex(0, 0);
    EEE2DLightGlow::sModel->GetMaterial(0)->SetPolygonRangeLength(4, 0);
    EEE2DLightGlow::sModel->GetMaterial(0)->SetVertexStartingIndex(0);
    EEE2DLightGlow::sModel->GetMaterial(0)->SetVertexRangeLength(8);
    EEE2DLightGlow::sModel->GetPolygonArray(0)->SetVertexIndex1(0, 0);
    EEE2DLightGlow::sModel->GetPolygonArray(0)->SetVertexIndex2(0, 1);
    EEE2DLightGlow::sModel->GetPolygonArray(0)->SetVertexIndex3(0, 2);
    EEE2DLightGlow::sModel->GetPolygonArray(0)->SetVertexIndex1(1, 2);
    EEE2DLightGlow::sModel->GetPolygonArray(0)->SetVertexIndex2(1, 1);
    EEE2DLightGlow::sModel->GetPolygonArray(0)->SetVertexIndex3(1, 3);
    EEE2DLightGlow::sModel->GetPolygonArray(0)->SetVertexIndex1(2, 4);
    EEE2DLightGlow::sModel->GetPolygonArray(0)->SetVertexIndex2(2, 5);
    EEE2DLightGlow::sModel->GetPolygonArray(0)->SetVertexIndex3(2, 6);
    EEE2DLightGlow::sModel->GetPolygonArray(0)->SetVertexIndex1(3, 6);
    EEE2DLightGlow::sModel->GetPolygonArray(0)->SetVertexIndex2(3, 5);
    EEE2DLightGlow::sModel->GetPolygonArray(0)->SetVertexIndex3(3, 7);
    EEE2DLightGlow::sModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate0, 0);
    EEE2DLightGlow::sModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate1, 1);
    EEE2DLightGlow::sModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate2, 2);
    EEE2DLightGlow::sModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate3, 3);
    EEE2DLightGlow::sModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate0, 4);
    EEE2DLightGlow::sModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate1, 5);
    EEE2DLightGlow::sModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate2, 6);
    EEE2DLightGlow::sModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate3, 7);
    EEE2DLightGlow::sModel->ComputeBoundingVolumes();
    URegisterShutdown(EEE2DLightGlow::ReleaseModel);
}

long EEE2DLightGlow::DrawPass2()
{
    return 0;
}

long EEE2DLightGlow::DrawPass4()
{
    return 0;
}

long EEE2DLightGlow::DrawPass5()
{
    return 0;
}

// FUNCTION: EXE 0x4e1f89
// Queues the glow in render queue 4 under its texture, snapshotting its fade-in and each spot's visibility and
// position; returns 9 (skip) while the unit is off screen
long EEE2DLightGlow::Queue(EEEffectQueue* inQueues)
{
    long theResult = EEEffect2D::Queue(inQueues);
    if (theResult == 0) {
        if (this->mHidden)
            theResult = this->Hidden();
        if (theResult == 0) {
            this->mDrawAge = this->mAge / this->mGraphicEffect->mTime1;
            if (this->mDrawAge > 1.0f)
                this->mDrawAge = 1.0f;
            for (unsigned long i = 0; i < this->mSpotCount; i++) {
                if (this->mSpots[i].mVisible) {
                    this->mSpots[i].mDrawVisible = true;
                    this->mSpots[i].mDrawPosition = this->mSpots[i].mPosition;
                } else {
                    this->mSpots[i].mDrawVisible = false;
                }
            }
            GETexture* theTexture;
            theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
            inQueues[4][theTexture].push_back(this);
        }
    }
    return theResult;
}

void EEE2DLightGlow::PrepareQueue()
{
}

// FUNCTION: EXE 0x4e2067
// Loads the glow's graphic and creates one spot per light tag point, parsing colour, on time, period and start
// from the tag name (one default spot without tags); returns the load result
long EEE2DLightGlow::SetUp()
{
    UString theName;
    UString theField;
    GE3DPoint thePosition;
    EEUnit* theUnit = this->mUnit;
    EEComplexUnit* theComplex = theUnit->mComplexUnit;
    this->mGraphicObject = this->GetGraphicObject();
    long theResult = this->mGraphicObject->LoadMedia();
    if (EEE2DLightGlow::sModel == NULL)
        this->CreateModel();
    EETagPointList* theTags;
    if (theComplex != NULL) {
        theTags = theUnit->GetModel()->GetTagPoints(theComplex->GetAnimation(), 0xd);
        const GETransformation& theLocation = theComplex->mPhysicalModel.GetLocation();
        thePosition.SetX(theLocation.GetX());
        thePosition.SetY(theLocation.GetY());
        thePosition.SetZ(theLocation.GetZ());
    } else {
        theTags = theUnit->GetModel()->GetTagPoints(0, 0xd);
        thePosition.SetX(theUnit->mUnitClass->mTileSize * 0.5f + theUnit->mTileX);
        thePosition.SetY(theUnit->mUnitClass->mTileSize * 0.5f + theUnit->mTileY);
        thePosition.SetZ(gWorld.GetMap()->mTerrainMesh.GetMeshZ(thePosition.GetX(), thePosition.GetY()));
    }
    if (theTags != NULL && theTags->GetNumTagPoints() != 0) {
        this->mSpotCount = theTags->GetNumTagPoints();
        this->mSpots = new EEE2DLightGlowSpot[this->mSpotCount];
        for (unsigned long i = 0; i < theTags->GetNumTagPoints(); i++) {
            unsigned long theIndex = theTags->mPoints[i]->mIndex;
            theName = theTags->mPoints[i]->mModel->GetTagPointName(theIndex);
            float theRed;
            float theGreen;
            float theBlue;
            unsigned long theStart = theName.Find("_", 0) + 1;
            unsigned long theEnd = theName.Find("_", theStart);
            if (theStart > 0 && theEnd < theName.GetLength()) {
                theField.Copy(theName, theStart, theEnd - theStart + 1, 0, false);
                sscanf(theField, "%f", &theRed);
            } else {
                theRed = 1.0f;
            }
            theStart = theEnd + 1;
            theEnd = theName.Find("_", theStart);
            if (theEnd < theName.GetLength()) {
                theField.Copy(theName, theStart, theEnd - theStart + 1, 0, false);
                sscanf(theField, "%f", &theGreen);
            } else {
                theGreen = 1.0f;
            }
            theStart = theEnd + 1;
            theEnd = theName.Find("_", theStart);
            if (theEnd < theName.GetLength()) {
                theField.Copy(theName, theStart, theEnd - theStart + 1, 0, false);
                sscanf(theField, "%f", &theBlue);
            } else {
                theBlue = 1.0f;
            }
            theStart = theEnd + 1;
            theEnd = theName.Find("_", theStart);
            if (theEnd < theName.GetLength()) {
                theField.Copy(theName, theStart, theEnd - theStart + 1, 0, false);
                sscanf(theField, "%ld", &this->mSpots[i].mOnTime);
            } else {
                this->mSpots[i].mOnTime = 500;
            }
            theStart = theEnd + 1;
            theEnd = theName.Find("_", theStart);
            if (theEnd < theName.GetLength()) {
                theField.Copy(theName, theStart, theEnd - theStart + 1, 0, false);
                sscanf(theField, "%ld", &this->mSpots[i].mPeriod);
            } else {
                this->mSpots[i].mPeriod = this->mSpots[i].mOnTime;
            }
            theStart = theEnd + 1;
            if (theStart < theName.GetLength()) {
                theField.Copy(theName, theStart, theName.GetLength() - theStart, 0, false);
                sscanf(theField, "%ld", &this->mSpots[i].mStart);
            } else {
                this->mSpots[i].mStart = 0;
            }
            this->mSpots[i].mColor.SetRed(theRed / 255.0f);
            this->mSpots[i].mColor.SetGreen(theGreen / 255.0f);
            this->mSpots[i].mColor.SetBlue(theBlue / 255.0f);
        }
    } else {
        this->mSpotCount = 1;
        this->mSpots = new EEE2DLightGlowSpot[1];
        this->mSpots->mOnTime = 500;
        this->mSpots->mPeriod = 500;
        this->mSpots->mStart = 0;
    }
    this->SetPosition(thePosition);
    this->mBoundingRadius = this->mGraphicEffect->GetSize();
    return theResult;
}

// FUNCTION: EXE 0x4e24cb
// Creates a glow spot (its colour default-constructed, the rest left unset)
EEE2DLightGlowSpot::EEE2DLightGlowSpot()
{
}

// FUNCTION: EXE 0x4e24fa
// Releases the shared glow model at shutdown
void EEE2DLightGlow::ReleaseModel(bool inShutdown)
{
    if (EEE2DLightGlow::sModel) {
        EEE2DLightGlow::sModel->Dereference();
        EEE2DLightGlow::sModel = NULL;
    }
}

// FUNCTION: EXE 0x4e2512
// Draws a coloured sprite at every visible spot; returns 0
long EEE2DLightGlow::DrawPass6()
{
    for (unsigned long i = 0; i < this->mSpotCount; i++) {
        if (this->mSpots[i].mDrawVisible)
            AddGlowSprite(this->mSpots[i].mDrawPosition, this->mSpots[i].mColor, 0.0f, this->mGraphicEffect->GetSize(), 1.0f, 1.0f,
                     this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mAlphaReference : 0.0f);
    }
    return 0;
}

// FUNCTION: EXE 0x4e257b
// Reads the glow's age; returns the read result (the base class result is ignored)
long EEE2DLightGlow::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect2D::Load(inMajorVersion, inMinorVersion, inFile);
    return inFile.Read(this->mAge);
}

// FUNCTION: EXE 0x4e25a1
// Repairs the base effect's pointers after loading, then sets the glow up again; returns the first error
long EEE2DLightGlow::RepairPointers()
{
    long theResult = EEEffect::RepairPointers();
    if (theResult == 0)
        theResult = this->SetUp();
    return theResult;
}

// FUNCTION: EXE 0x4e25b6
// Writes the glow's age; returns the write result (the base class result is ignored)
long EEE2DLightGlow::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect2D::Save(inMajorVersion, inMinorVersion, outFile);
    return outFile.Write(this->mAge);
}

// FUNCTION: EXE 0x4e25dc
// Returns 7 (remove) once the unit has no hit points; else places the glows while the unit is within two tiles
// of the visible terrain, and hides the glow otherwise
long EEE2DLightGlow::Step()
{
    GETerrainMesh& theMesh = gWorld.GetMap()->mTerrainMesh;
    long theResult = 0;
    unsigned long theX, theY, theWidth, theHeight;
    theMesh.GetTerrainVisibleBottomLeftIndices(theX, theY);
    theMesh.GetTerrainVisibleTileDimensions(theWidth, theHeight);
    U2DRectangle<long> theArea;
    theArea.mTop = theY - 2;
    theArea.mLeft = theX - 2;
    theArea.mRight = theWidth + theX + 2;
    theArea.mBottom = theHeight + theY + 2;
    if (this->mUnit->mHitPoints == 0) {
        theResult = 7;
    } else if (theMesh.GetNumVisibleTileRows() != 0 && theArea.Contains(this->mUnit->mTileX, this->mUnit->mTileY)) {
        this->mHidden = false;
        this->PlaceGlows();
    } else {
        this->mHidden = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x4e266f
// Ages the glow and blinks each spot by its start, period and on time, placing lit spots on their tag points; hides
// all spots for an enemy's cloaked unit or an unfinished building; returns 7 (remove) if the unit has no hit points
long EEE2DLightGlow::PlaceGlows()
{
    GE3DPoint thePosition;
    GETransformation theTransformation(false);
    EEUnit* theUnit = this->mUnit;
    long theResult = 0;
    bool theShow = true;
    if (theUnit->mHitPoints == 0) {
        theResult = 7;
    } else {
        EETagPointList* theTags;
        float theScale;
        unsigned long theFrame;
        EEComplexUnit* theComplex = theUnit->mComplexUnit;
        if (theComplex != NULL) {
            bool theCloaked = theComplex->mIsCloakable ? theComplex->mIsCloaked : false;
            if (theCloaked && gWorld.GetCurrentPlayer() != theComplex->mPlayer)
                theShow = false;
            if (theComplex->mUnitClass->IsLandBuilding() && !theComplex->mIsBuilt)
                theShow = false;
            theTags = theUnit->GetModel()->GetTagPoints(theComplex->GetAnimation(), 0xd);
            theScale = theComplex->mPhysicalModel.GetScale();
            theFrame = theComplex->mPhysicalModel.GetFrame();
            theTransformation = theComplex->mPhysicalModel.GetLocation();
            thePosition.SetX(theTransformation.GetX());
            thePosition.SetY(theTransformation.GetY());
            thePosition.SetZ(theTransformation.GetZ());
        } else {
            EEUnitClassUpgrade* theStats = theUnit->mStats;
            theTags = theUnit->GetModel()->GetTagPoints(0, 0xd);
            theScale = theStats->mScale;
            theFrame = 0;
            thePosition.SetX(theUnit->mUnitClass->mTileSize * 0.5f + theUnit->mTileX);
            thePosition.SetY(theUnit->mUnitClass->mTileSize * 0.5f + theUnit->mTileY);
            thePosition.SetZ(gWorld.GetMap()->mTerrainMesh.GetMeshZ(thePosition.GetX(), thePosition.GetY()));
            theTransformation.MakeIdentity();
            theTransformation.SetX(thePosition.GetX());
            theTransformation.SetY(thePosition.GetY());
            theTransformation.SetZ(thePosition.GetZ());
        }
        this->SetPosition(thePosition);
        this->mAge += gPhysics->mTickTime;
        if (theTags != NULL)
            EETagPointList::Scale(theTransformation, theScale);
        for (unsigned long i = 0; i < this->mSpotCount; i++) {
            if (theShow && this->mSpots[i].mStart <= this->mAge) {
                if (((unsigned long)this->mAge - this->mSpots[i].mStart) % this->mSpots[i].mPeriod > this->mSpots[i].mOnTime) {
                    this->mSpots[i].mVisible = false;
                } else {
                    this->mSpots[i].mVisible = true;
                    if (theTags != NULL) {
                        const std::vector<EETagPoint*>& thePoints = theTags->mPoints;
                        thePoints[i]->GetPoint(theFrame, theTransformation, this->mSpots[i].mPosition);
                    }
                    else
                        this->mSpots[i].mPosition = thePosition;
                }
            } else {
                this->mSpots[i].mVisible = false;
            }
        }
    }
    return theResult;
}

// the hidden effect's Virtual24 result (ICF-folded into 0x228735: `push 9; pop eax`)
long EEE2DLightGlow::Hidden()
{
    return 9;
}
