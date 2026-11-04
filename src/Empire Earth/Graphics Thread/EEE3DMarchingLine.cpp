// C:\EEX\Empire Earth\Graphics Thread\EEE3DMarchingLine.cpp (no __FILE__, no key chain; name from the RTTI class).
// 0xf3580..0xf4247: ??_G, both ctors, the dtor, the virtuals, the region helpers, then the COMDATs
// vector<EEEffect*>::push_back 0xf40ac and the tile walk 0xf40d8. Unit flags: the exe's module set.
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
#include <algorithm>
#include "lle/GELine.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEE3DIcon.h"
#include "ee/EEE3DGoal.h"
#include "ee/EERenderObject.h"
#include "ee/EEE3DMarchingLine.h"
#include "LostDebug.h"
#include "ee/EEEffectKindIDs.h"

// the step along the line in tiles (.rdata 0x440994, loaded from memory: a named constant)
const float kMarchingLineStep = 1.0f;

// 0xf40d8 is U2DPoint<long>::GetLineTiles (lle/U2DPoint.h, exe view): its COMDAT is first emitted here

// SYNTHETIC: EXE 0x4f3580
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4f359c
// Creates a marching line (waypoint line) between two points, attached to no units
EEE3DMarchingLine::EEE3DMarchingLine(GE3DPoint& inFrom, GE3DPoint& inTo, EEGraphicEffect* inGraphicEffect)
    : EEEffect3D(0, inGraphicEffect, kEffect3DMarchingLine)
{
    this->mFrom = inFrom;
    this->mTo = inTo;
    this->mFromUnit = NULL;
    this->mToUnit = NULL;
}

// FUNCTION: EXE 0x4f3693
// Creates an empty marching line for loading from a saved game
EEE3DMarchingLine::EEE3DMarchingLine()
    : EEEffect3D(kEffect3DMarchingLine), mFromUnit(NULL), mToUnit(NULL)
{
    this->Reset();
}

// FUNCTION: EXE 0x4f3736
// Destroys the line: leaves its map regions, unloads its graphic and releases both end units
EEE3DMarchingLine::~EEE3DMarchingLine()
{
    this->RemoveRegions();
    if (this->mGraphicObject) {
        this->mGraphicObject->UnloadMedia();
        this->mGraphicObject = NULL;
    }
    if (this->mFromUnit)
        this->mFromUnit->Dereference();
    if (this->mToUnit)
        this->mToUnit->Dereference();
}

// FUNCTION: EXE 0x4f3793
// Queues the line in render queue 2, snapshotting its ends (at the end units if any) and the camera position
long EEE3DMarchingLine::Queue(EEEffectQueue* inQueues)
{
    long theResult = EEEffect::Queue(inQueues);
    if (theResult == 0) {
        if (this->mFromUnit)
            this->mFromUnit->GetPosition(this->mDrawFrom);
        else
            this->mDrawFrom = this->mFrom;
        if (this->mToUnit)
            this->mToUnit->GetPosition(this->mDrawTo);
        else
            this->mDrawTo = this->mTo;
        ::gWorld.mTransformation.GetPosition(this->mCamera);
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[2][theTexture].push_back(this);
    }
    return theResult;
}

// FUNCTION: EXE 0x4f3882
// Leaves the line's map regions, then releases the 3D effect's resources
void EEE3DMarchingLine::ReleaseResources()
{
    this->RemoveRegions();
    EEEffect3D::ReleaseResources();
}

// FUNCTION: EXE 0x4f3893
// Resets the line's time and its mCC value to 0
void EEE3DMarchingLine::Reset()
{
    this->mCC = 0.0f;
    this->mTime = 0.0f;
}

// FUNCTION: EXE 0x4f38a4
// Resets the line and loads its graphic; returns 0
long EEE3DMarchingLine::SetUp()
{
    EEGraphicEffect* theEffect = this->mGraphicEffect;
    this->Reset();
    this->mGraphicObject = theEffect->mGraphicObject;
    this->mGraphicObject->LoadMedia();
    return 0;
}

// FUNCTION: EXE 0x4f38c3
// Registers the line in each effect region it crosses between two points, except the end regions and its own
void EEE3DMarchingLine::AddRegions(const GE3DPoint& inFrom, const GE3DPoint& inTo)
{
    std::vector<U2DPoint<long> > theTiles;
    long theFromY = UMFloor(inFrom.GetY());
    long theFromX = UMFloor(inFrom.GetX());
    long theToY = UMFloor(inTo.GetY());
    long theToX = UMFloor(inTo.GetX());
    U2DPoint<long> theTile(this->mTileX, this->mTileY);
    U2DPoint<long> theStart;
    U2DPoint<long> theEnd;
    RTSRegionTable<EEEffect*>* theTable = ::gWorld.mMap->mEffectRegions;
    theStart = U2DPoint<long>(theFromX / theTable->mRegionSize, theFromY / theTable->mRegionSize);
    theEnd = U2DPoint<long>(theToX / theTable->mRegionSize, theToY / theTable->mRegionSize);
    theStart.GetLineTiles(theEnd, kMarchingLineStep, theTiles);
    for (long i = theTiles.size() - 1; i >= 0; i--) {
        if (theTiles[i] == theStart || theTiles[i] == theEnd || theTiles[i] == theTile)
            continue;
        if (std::find(this->mRegions.begin(), this->mRegions.end(), theTiles[i]) != this->mRegions.end())
            continue;
        this->mRegions.push_back(theTiles[i]);
        EEEffect* theEffect = this;
        std::vector<EEEffect*>* theRegion = theTable->GetRegion(theTiles[i]);
        if (theRegion)
            theRegion->push_back(theEffect);
    }
}

// FUNCTION: EXE 0x4f3b66
// Reads the base 3D effect, time, mCC and the ids of the end units (relinked later); returns the first error
long EEE3DMarchingLine::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = EEEffect3D::Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult != 0)
        return theResult;
    theResult = inFile.Read(this->mTime);
    if (theResult != 0)
        return theResult;
    theResult = inFile.Read(this->mCC);
    if (theResult != 0)
        return theResult;
    bool theHasUnit;
    unsigned long theID;
    theResult = inFile.Read(theHasUnit);
    if (theResult != 0)
        return theResult;
    if (theHasUnit) {
        theResult = inFile.Read(theID);
        if (theResult != 0)
            return theResult;
        this->mFromUnit = reinterpret_cast<EEUnit*>(theID);
    }
    theResult = inFile.Read(theHasUnit);
    if (theResult != 0 || !theHasUnit)
        return theResult;
    theResult = inFile.Read(theID);
    if (theResult == 0)
        this->mToUnit = reinterpret_cast<EEUnit*>(theID);
    return theResult;
}

// FUNCTION: EXE 0x4f3c02
// Removes the line from every effect region it was registered in and forgets them
void EEE3DMarchingLine::RemoveRegions()
{
    RTSRegionTable<EEEffect*>* theTable;
    if (::gWorld.mMap && (theTable = ::gWorld.mMap->mEffectRegions) != 0) {
        for (long i = this->mRegions.size() - 1; i >= 0; i--) {
            EEEffect* theEffect = this;
            RTSRegion<EEEffect*>* theRegion = theTable->GetRegion(this->mRegions[i]);
            if (theRegion)
                theRegion->Remove(theEffect);
        }
    }
    this->mRegions.clear();
}

// FUNCTION: EXE 0x4f3c68
// Draws the line on screen between its projected ends, fading from opaque to 0.3, without fog; returns the draw result
long EEE3DMarchingLine::DrawPass4()
{
    // LOST-DEBUG: none: exact only (tools lostdebug_search)
    EE_LOST_DEBUG(4);
    GELine theLine;
    GE2DPoint theStart;
    GE2DPoint theEnd;
    unsigned long theScreen = gScreenIndex;
    if (this->mGraphicEffect->mGraphicObject)
        gRasterizer->SetAlphaTestingRefValue(this->mGraphicEffect->mGraphicObject->mAlphaReference, theScreen);
    else
        gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
    Fn118676(this->mDrawFrom, theStart);
    Fn118676(this->mDrawTo, theEnd);
    theLine.SetStartPoint(theStart);
    theLine.SetEndPoint(theEnd);
    theLine.SetOpacityPerVertex(1.0f, 0.3f);
    unsigned long theFog = gRasterizer->GetFog(theScreen);
    unsigned long theZBuffer = gRasterizer->GetZBuffer(theScreen);
    unsigned long theZCompare = gRasterizer->GetZCompare(theScreen);
    gRasterizer->SetFog(0, theScreen);
    gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
    gRasterizer->SetZCompare(kCompareLess, theScreen);
    gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
    gRasterizer->SetShadingColorSource(1, theScreen);
    Fn10F7C8();
    long theResult = gRasterizer->Draw(&theLine, 1, theScreen);
    gRasterizer->SetFog(theFog, theScreen);
    gRasterizer->SetZBuffer(theZBuffer, theScreen);
    gRasterizer->SetZCompare(theZCompare, theScreen);
    return theResult;
}

long EEE3DMarchingLine::DrawPass6()
{
    return 0;
}

// the empty Virtual2C: ICF-folded into 0xe123d
void EEE3DMarchingLine::PrepareQueue()
{
}

// FUNCTION: EXE 0x4f3e29
// Moves the line's start to a point and re-registers its map regions
void EEE3DMarchingLine::MoveToPoint(const GE3DPoint& inLocation)
{
    this->RemoveRegions();
    EEEffect::MoveToPoint(inLocation);
    this->AddRegions(inLocation, this->mTo);
}

// FUNCTION: EXE 0x4f3e52
// Moves the line's start to a transformation's position and re-registers its map regions
void EEE3DMarchingLine::MoveToTransformation(const GETransformation& inTransformation)
{
    GE3DPoint theLocation(inTransformation.GetX(), inTransformation.GetY(), inTransformation.GetZ());
    this->RemoveRegions();
    EEEffect::MoveToTransformation(inTransformation);
    this->AddRegions(theLocation, this->mTo);
}

// FUNCTION: EXE 0x4f3e96
// Sets the start point unless the line starts at a unit; returns whether it was set
bool EEE3DMarchingLine::SetFrom(const GE3DPoint& inPoint)
{
    bool theResult = false;
    if (this->mFromUnit == NULL) {
        this->mFrom = inPoint;
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x4f3ebf
// Sets the end point unless the line ends at a unit; returns whether it was set
bool EEE3DMarchingLine::SetTo(const GE3DPoint& inPoint)
{
    bool theResult = false;
    if (this->mToUnit == NULL) {
        this->mTo = inPoint;
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x4f3ee8
// After a load, turns the saved unit ids back into units (without referencing them) and reloads the graphic
long EEE3DMarchingLine::RepairPointers()
{
    EEGraphicEffect* theEffect = this->mGraphicEffect;
    long theResult = EEEffect::RepairPointers();
    if (theResult == 0) {
        if (reinterpret_cast<unsigned long>(this->mFromUnit) > 0)
            this->mFromUnit = static_cast<EEUnit*>(::gWorld.GetUnit(reinterpret_cast<unsigned long>(this->mFromUnit)));
        if (reinterpret_cast<unsigned long>(this->mToUnit) > 0)
            this->mToUnit = static_cast<EEUnit*>(::gWorld.GetUnit(reinterpret_cast<unsigned long>(this->mToUnit)));
        this->mGraphicObject = theEffect->mGraphicObject;
        this->mGraphicObject->LoadMedia();
    }
    return theResult;
}

// FUNCTION: EXE 0x4f3f56
// Writes the base 3D effect, time, mCC and the end units' ids
long EEE3DMarchingLine::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = EEEffect3D::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mTime);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mCC);
    if (theResult != 0)
        return theResult;
    bool theHasUnit = this->mFromUnit != NULL;
    unsigned long theID;
    theResult = outFile.Write(theHasUnit);
    if (theResult != 0)
        return theResult;
    if (theHasUnit) {
        theID = this->mFromUnit->GetUnitID();
        theResult = outFile.Write(theID);
        if (theResult != 0)
            return theResult;
    }
    theHasUnit = this->mToUnit != NULL;
    theResult = outFile.Write(theHasUnit);
    if (theResult == 0 && theHasUnit) {
        theID = this->mToUnit->GetUnitID();
        theResult = outFile.Write(theID);
    }
    return theResult;
}

// FUNCTION: EXE 0x4f4017
// Swaps a replaced unit for its replacement at either end of the line, moving the references
void EEE3DMarchingLine::ReplaceUnit(EEUnit* inOldUnit, EEUnit* inNewUnit)
{
    EEEffect::ReplaceUnit(inOldUnit, inNewUnit);
    if (inOldUnit == this->mFromUnit) {
        this->mFromUnit->Dereference();
        this->mFromUnit = inNewUnit;
        inNewUnit->Reference();
    }
    if (inOldUnit != this->mToUnit)
        return;
    this->mToUnit->Dereference();
    this->mToUnit = inNewUnit;
    inNewUnit->Reference();
}

// FUNCTION: EXE 0x4f4068
// Ages the line; returns 7 (remove) once a set duration is up
long EEE3DMarchingLine::Step()
{
    unsigned long theDuration = this->mGraphicEffect->mDuration;
    long theResult = 0;
    this->mTime += gPhysics->GetTickTime();
    if (this->mTime >= theDuration && theDuration != 0)
        theResult = 7;
    return theResult;
}

// FUNCTION: EXE 0x4f40ac
// Appends an effect to a vector<EEEffect*> (a template COMDAT)

// TEMPLATE: EXE 0x4f40d8
// U2DPoint<long>::GetLineTiles (lle/U2DPoint.h exe view)
