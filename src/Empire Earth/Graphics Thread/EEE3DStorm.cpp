// C:\EEX\Empire Earth\Graphics Thread\EEE3DStorm.cpp (no __FILE__; name from the RTTI class). 0xfb088..0xfb9f6: the
// init chain of a file-scope map, the ctors .. Save, then the map's _Tree COMDATs.
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
#include <map>
#include "lle/U2DPoint.h"
#include "ee/EETileCoordinate.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEE3DStorm.h"

// SYNTHETIC: EXE 0x4fb088
// SYNTHETIC: EXE 0x4fb092
// SYNTHETIC: EXE 0x4fb0b2
// SYNTHETIC: EXE 0x4fb0be
// 0x4e8ce8: a map of tile lists by an unsigned short key (unguarded dtor: a plain file-scope object; no user in this
// unit's code) (provisional name)
static std::map<unsigned short, std::vector<EETileCoordinate> > sStormTiles;

// FUNCTION: EXE 0x4fb0c8
// Creates a weather storm of the given kind over a copy of the tiles, 5 s long at full intensity, centred on them
EEE3DStorm::EEE3DStorm(long inKind, const std::vector<EETileCoordinate>& inTiles, EEGraphicEffect* inGraphicEffect)
    : EEEffect3D(0, inGraphicEffect, inKind)
{
    this->mTiles = inTiles;
    this->mElapsedTime = 0.0f;
    this->mL74 = 0;
    this->mDensity = 0.0f;
    this->mModel = NULL;
    this->mIntensity = 1.0f;
    this->mDuration = 5000;
    this->mIsRestored = false;
    this->mIsSustained = false;
    this->mFallSpeed = 0.0f;
    this->mSpawnHeight = 0.0f;
    this->GetCenter();
}

// SYNTHETIC: EXE 0x4fb175
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4fb191
// Creates an empty storm of the given kind (for loading)
EEE3DStorm::EEE3DStorm(long inKind)
    : EEEffect3D(inKind)
{
    this->mElapsedTime = 0.0f;
    this->mDensity = 0.0f;
    this->mL74 = 0;
    this->mModel = NULL;
    this->mIntensity = 1.0f;
    this->mIsRestored = false;
    this->mIsSustained = false;
    this->mFallSpeed = 0.0f;
    this->mDuration = 5000;
    this->mSpawnHeight = 0.0f;
}

// FUNCTION: EXE 0x4fb217
// Destroys the storm, releasing its model
EEE3DStorm::~EEE3DStorm()
{
    if (this->mModel) {
        this->mModel->Dereference();
        this->mModel = NULL;
    }
}

// NEAR: the original calls begin() and _Ptrit::operator+ out of line at every mTiles[i] (Dinkum budget family),
// so SetTransformation still expands there; ours expands [i] and refuses SetTransformation
// FUNCTION: EXE 0x4fb252
// Steps every storm tile one tile towards its place around the new centre (a tile that would leave the map stays),
// recentres the storm and returns the tiles' bounds; the radius becomes half the longer side
void EEE3DStorm::Move(const U2DPoint<long>& inCenter, U2DRectangle<long>& outBounds)
{
    GETransformation theTransformation;
    EETileCoordinate theTile;
    long theSumX;
    long theSumY;
    theTile.mX = 0;
    theTile.mY = 0;
    theSumX = 0;
    theSumY = 0;
    if (this->mTiles.size() <= 0)
        return;
    outBounds.mTop = this->mCenter.mY;
    outBounds.mLeft = this->mCenter.GetX();
    outBounds.mBottom = this->mCenter.mY;
    outBounds.mRight = this->mCenter.GetX();
    for (long i = this->mTiles.size() - 1; i >= 0; i--) {
        theTile.mY = inCenter.mY + (this->mTiles[i].GetY() - this->mCenter.mY);
        theTile.mX = inCenter.GetX() + (this->mTiles[i].GetX() - this->mCenter.GetX());
        long theDirection = EEGetTileDirection(this->mTiles[i].GetX(), this->mTiles[i].GetY(), theTile.GetX(), theTile.mY);
        EEGetNextTile(this->mTiles[i], theTile, theDirection, 1);
        if (gWorld.mMap->GetTile(theTile.GetX(), theTile.mY))
            this->mTiles[i] = theTile;
        theSumX += this->mTiles[i].GetX();
        theSumY += this->mTiles[i].GetY();
        outBounds.Extend(this->mTiles[i]);
    }
    this->mCenter.mY = (unsigned long)theSumY / this->mTiles.size();
    this->mCenter.mX = (unsigned long)theSumX / this->mTiles.size();
    theTransformation.SetPosition((float)this->mCenter.GetX(), (float)this->mCenter.mY,
                                  gWorld.mMap->mTerrainMesh.GetZ(this->mCenter.GetX(), this->mCenter.mY));
    this->SetTransformation(theTransformation);
    long theRadius = max((outBounds.mRight - outBounds.mLeft) / 2, (outBounds.mBottom - outBounds.mTop) / 2);
    this->SetBoundingRadius((float)theRadius);
}

// NEAR: the original calls back() and begin() out of line (inline budget); bounds/sum slots differ
// FUNCTION: EXE 0x4fb500
// Recomputes the storm centre as the mean of its tiles, moves the effect there on the terrain and returns it (the
// tile bounds it also computes are unused)
U2DPoint<long>& EEE3DStorm::GetCenter()
{
    long i;
    GETransformation theTransformation;
    EEAIAStarBounds theBounds;
    U2DPoint<long> theSum;
    if (this->mTiles.size() > 0) {
        theBounds.mBottom = this->mTiles.back().GetY();
        theBounds.mRight = this->mTiles.back().GetX();
        theBounds.mTop = this->mTiles.back().GetY();
        theBounds.mLeft = this->mTiles.back().GetX();
        for (i = this->mTiles.size() - 2; i >= 0; i--)
            theBounds.Extend(this->mTiles[i]);
    }
    if (this->mTiles.size() > 0) {
        for (i = this->mTiles.size() - 1; i >= 0; i--) {
            theSum.mX += this->mTiles[i].GetX();
            theSum.mY += this->mTiles[i].GetY();
        }
        this->mCenter.mY = (unsigned long)theSum.GetY() / this->mTiles.size();
        this->mCenter.mX = (unsigned long)theSum.GetX() / this->mTiles.size();
        theTransformation.SetPosition((float)this->mCenter.GetX(), (float)this->mCenter.GetY(),
                                      gWorld.mMap->mTerrainMesh.GetZ(this->mCenter.GetX(), this->mCenter.GetY()));
        this->SetTransformation(theTransformation);
    }
    return this->mCenter;
}

// FUNCTION: EXE 0x4fb6b4
// Returns the bounding rectangle of the storm's tiles (unchanged when it has none)
void EEE3DStorm::GetBounds(U2DRectangle<long>& outBounds)
{
    if (this->mTiles.size() <= 0)
        return;
    const U2DPoint<long>& theBottomRight = this->mTiles.back();
    outBounds.mBottom = theBottomRight.mY;
    outBounds.mRight = theBottomRight.GetX();
    const U2DPoint<long>& theTopLeft = this->mTiles.back();
    outBounds.mTop = theTopLeft.mY;
    outBounds.mLeft = theTopLeft.GetX();
    for (long i = this->mTiles.size() - 2; i >= 0; i--)
        outBounds.Extend(this->mTiles[i]);
}

// FUNCTION: EXE 0x4fb72e
// Reads fall speed, spawn height, density and duration from the storm's graphic effect
void EEE3DStorm::Virtual64()
{
    EEGraphicEffect* theEffect = this->mGraphicEffect;
    this->mFallSpeed = theEffect->GetParam1();
    this->mSpawnHeight = theEffect->GetParam2();
    this->mDensity = theEffect->GetParam3();
    this->mDuration = theEffect->mDuration;
}

// FUNCTION: EXE 0x4fb756
// Reads the storm's elapsed time, tiles, centre, intensity and sustained flag; returns the first read error
long EEE3DStorm::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    unsigned long theCount = 0;
    long theResult = EEEffect3D::Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult == 0)
        theResult = inFile.Read(this->mElapsedTime);
    if (theResult == 0)
        theResult = inFile.Read(theCount);
    if (theResult == 0) {
        for (unsigned long i = 0; i < theCount && theResult == 0; i++) {
            long theX;
            long theY;
            theResult = inFile.Read(theX);
            if (theResult != 0)
                continue;
            theResult = inFile.Read(theY);
            if (theResult != 0)
                continue;
            EETileCoordinate theTile;
            theTile.mX = theX;
            theTile.mY = theY;
            this->mTiles.push_back(theTile);
        }
    }
    if (theResult == 0)
        theResult = this->mCenter.Load(inFile);
    if (theResult == 0)
        theResult = inFile.Read(this->mIntensity);
    if (theResult == 0)
        theResult = inFile.Read(this->mIsSustained);
    return theResult;
}

// ICF-folded into EEE3DBlastWave's 0xe93e0
long EEE3DStorm::RepairPointers()
{
    return EEEffect::RepairPointers();
}

// FUNCTION: EXE 0x4fb83f
// Writes the base effect, elapsed time, tiles, centre, intensity and sustained flag;
// returns the first write error
long EEE3DStorm::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = EEEffect3D::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult == 0)
        theResult = outFile.Write(this->mElapsedTime);
    if (theResult == 0) {
        unsigned long theCount = this->mTiles.size();
        theResult = outFile.Write(theCount);
    }
    if (theResult == 0) {
        for (std::vector<EETileCoordinate>::iterator theTile = this->mTiles.begin(); theTile != this->mTiles.end() && theResult == 0;
             theTile++) {
            long theValue = theTile->mX;
            theResult = outFile.Write(theValue);
            if (theResult == 0) {
                theValue = theTile->mY;
                theResult = outFile.Write(theValue);
            }
        }
    }
    if (theResult == 0)
        theResult = this->mCenter.Save(outFile);
    if (theResult == 0)
        theResult = outFile.Write(this->mIntensity);
    if (theResult == 0)
        theResult = outFile.Write(this->mIsSustained);
    return theResult;
}

// FUNCTION: EXE 0x4fb920
// FUNCTION: EXE 0x4fb94b
// FUNCTION: EXE 0x4fb9bc
// The file-scope map's _Tree members (template COMDATs from <map>)
