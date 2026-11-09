// C:\EEX\Empire Earth\Graphics Thread\EEE2DMarchingIcons.cpp (no __FILE__; name from the RTTI class). Icons circling
// a unit while an area effect of a kind is on it: 0xe3954..0xe4529 (the five save-key chains, both ctors .. LayOutIcons,
// then the template/inline COMDATs; EEE2DMuzzleFlash.cpp follows). The vtable's ??_G is the ICF-kept copy 0xdf590 in
// EEE2DCircularMarchingIcons.cpp. Unit flags: the exe's module set.
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
#include "ee/EEAEManager.h"
#include "ee/EEAreaEffectTemplate.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEGraphicObject.h"
#include "ee/EERenderObject.h"
#include "ee/EEEffect.h"
#include "ee/EEEffect2D.h"
#include "ee/EEE2DMarchingIcons.h"

const UString EEE2DMarchingIcons::sUnitIDStringID("MarchingIconsUnitIDStringID");
const UString EEE2DMarchingIcons::sAngularOffsetStringID("MarchingIconsAngularOffsetStringID");
const UString EEE2DMarchingIcons::sRotationSpeedStringID("MarchingIconsRotationSpeedStringID");
const UString EEE2DMarchingIcons::sIconCountStringID("MarchingIconsIconCountStringID");
const UString EEE2DMarchingIcons::sAreaEffectTypeStringID("MarchingIconsAreaEffectTypeStringID");

// unit-local .rdata after the vtable (0x43fe2c), loaded from memory: a named const (a literal factor would fold)
const float kDegreesToPi = 1.0f / 180.0f;

// FUNCTION: EXE 0x4e3aa3
// Creates an empty marching-icons effect of the given kind for loading, with no icons and no area effect type
EEE2DMarchingIcons::EEE2DMarchingIcons(long inKind)
    : EEEffect2D(gEffect2DModel, inKind, false)
{
    this->mIconCount = 0;
    this->mAngularOffset = 0.0f;
    this->mAreaEffectType = 0;
    this->mRotationSpeed = 0.0f;
    this->mL98 = 0;
    this->mAreaEffectIconCount = 1;
}

// FUNCTION: EXE 0x4e3b1d
// Creates marching icons around a unit from the effect's icon count, spin speed (degrees/s) and area effect type,
// taking the icon count of the unit's matching area effect (1 if none)
EEE2DMarchingIcons::EEE2DMarchingIcons(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect2D(inUnit, inGraphicEffect, gEffect2DModel, inKind, false)
{
    this->mGraphicObject = NULL;
    this->mIconCount = inGraphicEffect->mIntParam1;
    this->mAngularOffset = 0.0f;
    this->mRotationSpeed = inGraphicEffect->mIntParam2 * kDegreesToPi * 3.1415927f;
    this->mAreaEffectType = inGraphicEffect->mIntParam3;
    this->mVisible = false;
    this->mL98 = 0;
    this->mAreaEffectIconCount = 1;
    for (std::vector<EEAreaEffect*>::iterator theIterator = this->mUnit->mComplexUnit->mAreaEffects.mItems.begin();
         theIterator != this->mUnit->mComplexUnit->mAreaEffects.mItems.end(); ++theIterator) {
        if ((*theIterator)->mTemplate->mKind == this->mAreaEffectType) {
            this->mAreaEffectIconCount = (*theIterator)->mTemplate->mID;
            break;
        }
    }
}

// FUNCTION: EXE 0x4e3bf6
// Destroys the icons, unloading their graphic's media
EEE2DMarchingIcons::~EEE2DMarchingIcons()
{
    if (this->mGraphicObject) {
        this->mGraphicObject->UnloadMedia();
        this->mGraphicObject = NULL;
    }
}

// FUNCTION: EXE 0x4e3c32
// Queues the icons in render queue 4 with a snapshot of the unit position and icon offsets while visible;
// returns 10 (skip) while hidden
long EEE2DMarchingIcons::Queue(EEEffectQueue* inQueues)
{
    EEComplexUnit* theUnit = this->mUnit->mComplexUnit;
    long theResult = EEEffect2D::Queue(inQueues);
    this->mShown.clear();
    if (theResult == 0) {
        if (this->mVisible == true) {
            theUnit->mPhysicalModel.GetLocation().GetPosition(this->mLocation);
            this->mShown = this->mOffsets;
            GETexture* theTexture;
            theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
            inQueues[4][theTexture].push_back(this);
        } else {
            theResult = 10;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x4e3cdf
// Returns whether an icon at an offset from the unit shows: the unit has an owner, and the spot is not already
// covered by the owner's area effect of this type (always with no type)
bool EEE2DMarchingIcons::IsIconShown(const GE3DPoint& inOffset)
{
    unsigned long thePlayerID = 0;
    float theRadius = 0.0f;
    bool theResult = false;
    EEUnit* theUnit = this->mUnit;
    if (theUnit) {
        EEComplexUnit* theComplexUnit = theUnit->mComplexUnit;
        if (theUnit->mUnitClass->IsLandBuilding())
            theRadius = theUnit->mUnitClass->mTileSize * 0.5f;
        if (this->GetPlayerID(thePlayerID)) {
            GE2DPoint theLocation(inOffset.GetX() + (theRadius + theComplexUnit->mPhysicalModel.GetLocation().GetX()),
                                  inOffset.GetY() + (theRadius + theComplexUnit->mPhysicalModel.GetLocation().GetY()));
            if (this->mAreaEffectType) {
                if (!gAEManager.HasEffect(this->mAreaEffectType, theLocation.GetX(), theLocation.GetY(), thePlayerID, false,
                                         &theComplexUnit->mAreaEffects))
                    theResult = true;
            } else {
                theResult = true;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x4e3d8e
// Returns true (end the icons) when the unit is dead, or when it has pending area effects, counting one down
// (the last one calls FnE123D)
bool EEE2DMarchingIcons::Virtual5C()
{
    bool theResult;
    EEComplexUnit* theUnit = this->mUnit->mComplexUnit;
    if (theUnit->mHitPoints <= 0) {
        theResult = true;
    } else if (theUnit->mNumAreaEffects > 0) {
        theResult = true;
        if (--theUnit->mNumAreaEffects == 0)
            theUnit->FnE123D();
    } else {
        theResult = false;
    }
    return theResult;
}

// FUNCTION: EXE 0x4e3dc9
// Returns the number of icons
unsigned long EEE2DMarchingIcons::GetIconCount()
{
    return this->mIconCount;
}

// FUNCTION: EXE 0x4e3dd0
// Returns the unit owner's player id through the argument; false without a unit or owner
bool EEE2DMarchingIcons::GetPlayerID(unsigned long& outPlayerID)
{
    bool theResult = false;
    if (!this->mUnit)
        return theResult;
    EEPlayer* thePlayer = this->mUnit->mPlayer;
    if (thePlayer) {
        outPlayerID = thePlayer->mPlayerID;
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x4e3df1
// Returns how high above the ground the icons are drawn (0.05)
float EEE2DMarchingIcons::GetHeightOffset()
{
    return 0.05f;
}

// the empty Virtual2C: ICF-folded into 0xe123d (no address of its own here)
void EEE2DMarchingIcons::PrepareQueue()
{
}

// FUNCTION: EXE 0x4e3df8
// Loads the icons' graphic; returns the load result
long EEE2DMarchingIcons::SetUp()
{
    this->mGraphicObject = this->GetGraphicObject();
    return this->mGraphicObject->LoadMedia();
}

// FUNCTION: EXE 0x4e3e0f
// Reloads the icons' graphic and reads the spin angle, spin speed, icon count and area effect type; returns the
// first error (base result ignored)
long EEE2DMarchingIcons::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    this->mAreaEffectType = 0;
    EEEffect2D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mGraphicObject = this->GetGraphicObject();
    long theResult = this->mGraphicObject->LoadMedia();
    if (theResult != 0)
        return theResult;
    theResult = inFile.Read(this->mAngularOffset);
    if (theResult != 0)
        return theResult;
    theResult = inFile.Read(this->mRotationSpeed);
    if (theResult != 0)
        return theResult;
    theResult = inFile.Read(this->mIconCount);
    if (theResult != 0)
        return theResult;
    unsigned long theType;
    theResult = inFile.Read(theType);
    if (theResult == 0)
        this->mAreaEffectType = theType;
    return theResult;
}

// FUNCTION: EXE 0x4e3e92
// Draws an opaque fog-lit icon on the ground at each shown offset that lies on a map tile; returns the last draw result
long EEE2DMarchingIcons::DrawPass6()
{
    float theValue;
    GE3DPoint theLocation;
    GE3DPoint theGround;
    GETransformation theTransformation;
    long theResult = 0;
    EEMap* theMap = gWorld.mMap;
    unsigned long theScreen = gScreenIndex;
    GEModel* theModel = this->mModel;
    GEMaterial* theMaterial = theModel->GetMaterial(0);
    float theScale = this->mGraphicEffect->mSize;
    if (this->mGraphicEffect->mGraphicObject)
        gRasterizer->SetAlphaTestingRefValue(this->mGraphicEffect->mGraphicObject->mAlphaReference, theScreen);
    else
        gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
    unsigned long theFog = gRasterizer->GetFog(theScreen);
    if (theScale <= 0.0f)
        theScale = 1.0f;
    gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
    gRasterizer->SetBlending(kBlendOne, kBlendZero, theScreen);
    SetWhiteLighting();
    theMaterial->SetTexture(this->mGraphicObject->mTextures[0], theScreen);
    for (std::vector<GE3DPoint>::iterator theIterator = this->mShown.begin(); theIterator != this->mShown.end(); ++theIterator) {
        theTransformation.SetOrientationYPR(gWorld.GetCameraYaw() * 0.5f, 0.0f, 0.0f);
        theLocation = this->mLocation;
        GE3DPoint theOffset = *theIterator;
        theLocation += theOffset;
        if (theMap->GetTile(UMFloor(theLocation.GetX()), UMFloor(theLocation.GetY()))) {
            theMap->GetGroundPoint(theLocation, theGround);
            theGround.SetZ(theGround.GetZ() + this->GetHeightOffset());
            GetFogFactor(theGround, theFog, theValue);
            gRasterizer->SetAmbientColor(GEColor(theValue, theValue, theValue), theScreen);
            theTransformation.SetPosition(theGround.GetX(), theGround.GetY(), theGround.GetZ());
            theModel->SetGraphicsPostModelTransform(theTransformation);
            theModel->SetGraphicsScale(theScale);
            theModel->PrepareTransforms();
            bool theSorted = gOrthographicView;
            theResult = gRasterizer->Draw(theModel, theScreen, 1.0f, theSorted);
        }
    }
    gRasterizer->SetFog(theFog, theScreen);
    return theResult;
}

// FUNCTION: EXE 0x4e4132
// Repairs the base effect's pointers after loading and retakes the icon count from the unit's matching area effect
long EEE2DMarchingIcons::RepairPointers()
{
    long theResult = EEEffect::RepairPointers();
    this->mAreaEffectIconCount = 1;
    if (this->mUnit) {
        for (std::vector<EEAreaEffect*>::iterator theIterator = this->mUnit->mComplexUnit->mAreaEffects.mItems.begin();
             theIterator != this->mUnit->mComplexUnit->mAreaEffects.mItems.end(); ++theIterator) {
            if ((*theIterator)->mTemplate->mKind == this->mAreaEffectType) {
                this->mAreaEffectIconCount = (*theIterator)->mTemplate->mID;
                break;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x4e4182
// Writes the spin angle, spin speed, icon count and area effect type after the base state; returns the first error
long EEE2DMarchingIcons::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = EEEffect2D::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mAngularOffset);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mRotationSpeed);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mIconCount);
    if (theResult == 0) {
        unsigned long theType = this->mAreaEffectType;
        theResult = outFile.Write(theType);
    }
    return theResult;
}

// FUNCTION: EXE 0x4e41ef
// Shows and spins the icons while the current player owns the built unit and has its type selected (or ghost units
// are placed); returns 7 (remove) when Virtual5C ends them
long EEE2DMarchingIcons::Step()
{
    long theResult = 0;
    EEComplexUnit* theUnit = this->mUnit->mComplexUnit;
    unsigned long theType = theUnit->mUnitClass->mUnitType;
    EESelectedGroup* theGroup = theUnit->mPlayer->mActiveGroup;
    bool theSelected;
    if (theGroup)
        theSelected = theGroup->HasUnitType(theType);
    else
        theSelected = false;
    // The town center/capitol test also needs an active group (it was read without the NULL check above).
    if (theGroup != NULL && (theType == kUnitTypeTownCenter || theType == kUnitTypeCapitol)) {
        if (theGroup->HasUnitType(0x23) || theGroup->HasUnitType(0x29))
            theSelected = true;
    }
    if (!this->Virtual5C()) {
        if (theUnit->mPlayer == gWorld.GetCurrentPlayer() && theUnit->mIsBuilt &&
            (theSelected == true || gWorld.GetNumGhostUnits() > 0)) {
            this->LayOutIcons();
            this->mVisible = true;
        } else {
            this->mVisible = false;
        }
    } else {
        theResult = 7;
        this->mVisible = false;
    }
    return theResult;
}

// FUNCTION: EXE 0x4e42b9
// Returns whether a group holds a unit of a type (EEGroup::HasUnitType, inline in ee/EEPlayer.h; emitted after Step)

// FUNCTION: EXE 0x4e42db
// Recomputes the icon offsets at the current spin angle, advances the spin by the tick and follows the unit
void EEE2DMarchingIcons::LayOutIcons()
{
    GE3DPoint theLocation;
    float theElapsed = gPhysics->mTickTime;
    EEComplexUnit* theUnit = this->mUnit->mComplexUnit;
    this->mOffsets.clear();
    this->ComputeOffsets(this->mAngularOffset, this->mOffsets);
    this->mAngularOffset += this->mRotationSpeed * (theElapsed * 0.001f);
    if (this->mAngularOffset > 6.2831855f)
        this->mAngularOffset -= 6.2831855f;
    else if (this->mAngularOffset < -6.2831855f)
        this->mAngularOffset += 6.2831855f;
    theUnit->mPhysicalModel.GetLocation().GetPosition(theLocation);
    this->SetPosition(theLocation);
    if (this->mL98 == 0)
        EEE2DMarchingIcons::PrepareQueue();
}

// TEMPLATE: EXE 0x4e43a6
// std::vector<GE3DPoint>::operator= (first used by Virtual24)

// FUNCTION: EXE 0x4e4510
// Returns the number of ghost units being placed (EEWorld::GetNumGhostUnits, inline in ee/EEWorld.h)
// (emitted at the end of the unit)

// SYNTHETIC: EXE 0x4e3954
// SYNTHETIC: EXE 0x4e395e
// SYNTHETIC: EXE 0x4e396f
// SYNTHETIC: EXE 0x4e397b
// SYNTHETIC: EXE 0x4e3997
// SYNTHETIC: EXE 0x4e39a1
// SYNTHETIC: EXE 0x4e39b2
// SYNTHETIC: EXE 0x4e39be
// SYNTHETIC: EXE 0x4e39da
// SYNTHETIC: EXE 0x4e39e4
// SYNTHETIC: EXE 0x4e39f5
// SYNTHETIC: EXE 0x4e3a01
// SYNTHETIC: EXE 0x4e3a1d
// SYNTHETIC: EXE 0x4e3a27
// SYNTHETIC: EXE 0x4e3a38
// SYNTHETIC: EXE 0x4e3a44
// SYNTHETIC: EXE 0x4e3a60
// SYNTHETIC: EXE 0x4e3a6a
// SYNTHETIC: EXE 0x4e3a7b
// SYNTHETIC: EXE 0x4e3a87
