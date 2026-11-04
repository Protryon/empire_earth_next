// C:\EEX\Empire Earth\Random Map\Random Map Generators\Default\EEClumps.cpp (file name inferred; RTTI
// .?AVEEClumps@@ proves the class)
// Unit flags (hypothesis, docs/CALIBRATION.md): -O1 -G6 -MD -GR
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/U2DGeometry.h"
#include "ee/EERandomMap.h"
#include "EEClumps.h"

// Compiler-generated for this unit (names in analysis/default/names.txt):
// SYNTHETIC: DEFAULT 0x100079fa
// SYNTHETIC: DEFAULT 0x10007a04
// SYNTHETIC: DEFAULT 0x10007a15
// SYNTHETIC: DEFAULT 0x10007a21
// SYNTHETIC: DEFAULT 0x10007a2c
// SYNTHETIC: DEFAULT 0x10007a36
// SYNTHETIC: DEFAULT 0x10007a47
// SYNTHETIC: DEFAULT 0x10007a53
// SYNTHETIC: DEFAULT 0x10007a5e
// SYNTHETIC: DEFAULT 0x10007a68
// SYNTHETIC: DEFAULT 0x10007a79
// SYNTHETIC: DEFAULT 0x10007a85
// SYNTHETIC: DEFAULT 0x10007a90
// SYNTHETIC: DEFAULT 0x10007a9a
// SYNTHETIC: DEFAULT 0x10007aab
// SYNTHETIC: DEFAULT 0x10007ab7
// SYNTHETIC: DEFAULT 0x10007ac2
// SYNTHETIC: DEFAULT 0x10007acc
// SYNTHETIC: DEFAULT 0x10007add
// SYNTHETIC: DEFAULT 0x10007ae9
// SYNTHETIC: DEFAULT 0x10007af4
// SYNTHETIC: DEFAULT 0x10007afe
// SYNTHETIC: DEFAULT 0x10007b0f
// SYNTHETIC: DEFAULT 0x10007b1b
// SYNTHETIC: DEFAULT 0x10007b26
// SYNTHETIC: DEFAULT 0x10007b30
// SYNTHETIC: DEFAULT 0x10007b41
// SYNTHETIC: DEFAULT 0x10007b4d
// SYNTHETIC: DEFAULT 0x10008ee2
// SYNTHETIC: DEFAULT 0x10008eec
// SYNTHETIC: DEFAULT 0x10007c45
// TEMPLATE: DEFAULT 0x10008cc6
// TEMPLATE: DEFAULT 0x10008cf2
// TEMPLATE: DEFAULT 0x10008d8e
// TEMPLATE: DEFAULT 0x10008e34
// TEMPLATE: DEFAULT 0x10008e6d
// TEMPLATE: DEFAULT 0x10008e92
// TEMPLATE: DEFAULT 0x10008eb7
// TEMPLATE: DEFAULT 0x10008d53
// TEMPLATE: DEFAULT 0x10008da0
// TEMPLATE: DEFAULT 0x10008df0

// FUNCTION: DEFAULT 0x10007b58
// Creates a tile-claiming grower for one owner with a clump count, target size and spread, not started
EEClumps::EEClumps(EERandomMap* inMap, EECellGrid* inClaimed, float inSpread, unsigned long inClumpCount,
                   unsigned long inTargetSize, long inType, std::vector<unsigned long>* inDistances)
    : mType(inType), mClaimed(inClaimed), mAllowed(&inMap->mValidTiles), mDistances(inDistances),
      mRandom(&inMap->mRandom), mRandomIntegers(&inMap->mRandomIntegers), mMap(inMap), mSpread(inSpread)
{
    this->mMaximumY = inMap->mMaximumY;
    this->mMaximumX = inMap->mMaximumX;
    this->mMapSize = inMap->mGridSize;
    this->mIndex = 0;
    this->mTargetSize = inTargetSize;
    this->mClumpCount = inClumpCount;
    this->mGrowCount = inMap->mMaxIterationsClumpGrowth * inTargetSize;
    this->mStarted = false;
}

// FUNCTION: DEFAULT 0x10007c61
// Destroys the grower, freeing its centres, sizes, frontier and placed tiles
EEClumps::~EEClumps()
{
}

// FUNCTION: DEFAULT 0x10007c94
// Claims one valid tile next to a random frontier tile (0); drops a frontier tile with no valid
// neighbour (1); returns 2 when the frontier is empty
long EEClumps::Grow()
{
    bool theFound = false;
    unsigned long theTries = 0;
    long theResult = 0;
    long theX = 0;
    long theY = 0;
    if (this->mFrontier.size() != 0) {
        long theIndex = this->mRandomIntegers->Generate(0, this->mFrontier.size() - 1);
        long theDirection = this->mRandomIntegers->Generate(0, 3);
        do {
            switch (theDirection) {
            case 0:
                if (this->IsValidLocation(this->mFrontier[theIndex].mX, this->mFrontier[theIndex].mY + 1)) {
                    theFound = true;
                    theX = this->mFrontier[theIndex].mX;
                    theY = this->mFrontier[theIndex].mY + 1;
                }
                break;
            case 1:
                if (this->IsValidLocation(this->mFrontier[theIndex].mX + 1, this->mFrontier[theIndex].mY)) {
                    theFound = true;
                    theX = this->mFrontier[theIndex].mX + 1;
                    theY = this->mFrontier[theIndex].mY;
                }
                break;
            case 2:
                if (this->IsValidLocation(this->mFrontier[theIndex].mX, this->mFrontier[theIndex].mY - 1)) {
                    theFound = true;
                    theX = this->mFrontier[theIndex].mX;
                    theY = this->mFrontier[theIndex].mY - 1;
                }
                break;
            case 3:
                if (this->IsValidLocation(this->mFrontier[theIndex].mX - 1, this->mFrontier[theIndex].mY)) {
                    theFound = true;
                    theX = this->mFrontier[theIndex].mX - 1;
                    theY = this->mFrontier[theIndex].mY;
                }
                break;
            }
            theTries++;
            if (!theFound)
                theDirection = (theDirection + 1) & 3;
        } while (!theFound && theTries != 4);
        if (theFound)
            this->Claim(theX, theY, true);
        else {
            this->mFrontier.erase(this->mFrontier.begin() + theIndex);
            theResult = 1;
        }
    } else {
        unsigned long theSize = this->mSizes[this->mIndex];
        theResult = 2;
    }
    return theResult;
}

// FUNCTION: DEFAULT 0x10007fe9
// Empties the list of frontier tiles
void EEClumps::ClearFrontier()
{
    this->mFrontier.clear();
}

// FUNCTION: DEFAULT 0x10007ff1
// Starts the current clump at a location if its tile is valid: resets the frontier and claims it
bool EEClumps::AddLocation(const GE2DPoint& inLocation)
{
    if (this->IsValidLocation(UMFloor(inLocation.mX), UMFloor(inLocation.mY))) {
        this->ClearFrontier();
        this->mCenters[this->mIndex] = inLocation;
        this->Claim(inLocation, true);
        return true;
    }
    return false;
}

// FUNCTION: DEFAULT 0x10008074
// Claims the tile under a point (floored)
void EEClumps::Claim(const GE2DPoint& inLocation, bool inMark)
{
    this->Claim(UMFloor(inLocation.mX), UMFloor(inLocation.mY), inMark);
}

// FUNCTION: DEFAULT 0x100080c1
// Removes the first frontier entry at a tile
void EEClumps::RemoveFrontier(long inX, long inY)
{
    std::vector<U2DPoint<long> >::iterator theIterator = this->mFrontier.begin();
    while (theIterator != this->mFrontier.end()) {
        if (theIterator->mX == inX && theIterator->mY == inY) {
            this->mFrontier.erase(theIterator);
            theIterator = this->mFrontier.end();
        } else
            theIterator++;
    }
}

// FUNCTION: DEFAULT 0x10008103
// Gives every clump the grower's target size
void EEClumps::ResetTargets()
{
    for (unsigned long i = 0; i < this->mClumpCount; i++)
        this->mTargets[i] = this->mTargetSize;
}

// FUNCTION: DEFAULT 0x10008129
// Returns the number of tiles claimed by all clumps
unsigned long EEClumps::GetTotalSize() const
{
    unsigned long theTotal = 0;
    for (unsigned long i = 0; i < this->mClumpCount; i++)
        theTotal += this->mSizes[i];
    return theTotal;
}

// FUNCTION: DEFAULT 0x10008145
// Grows all clumps in one go: each grows to its target (a dead end adds a clump with the rest), then
// the next centre is picked; stops at the first clump that cannot start or find a next centre
void EEClumps::Generate()
{
    for (this->mIndex = 0; this->mIndex < this->mClumpCount; this->mIndex++) {
        if (!this->AddLocation(this->mCenters[this->mIndex]))
            return;
        bool theGrowing = true;
        for (unsigned long i = 0; i < this->mGrowCount && theGrowing; i++) {
            this->Grow();
            theGrowing = this->mSizes[this->mIndex] < this->mTargets[this->mIndex];
            if (this->mFrontier.size() == 0 && theGrowing) {
                this->AddClump();
                theGrowing = false;
            }
        }
        if (!(this->mIndex + 1 < this->mClumpCount))
            continue;
        bool theFailed = true;
        for (unsigned long j = 0; j < this->mMap->mMaxLoopCount && theFailed; j++) {
            theFailed = !this->PickDirection();
            if (theFailed)
                this->PickStart();
        }
        if (theFailed)
            return;
    }
}

// FUNCTION: DEFAULT 0x10008239
// Advances the current clump by one step (start it, move to the next clump, add a clump, or grow);
// returns false once all clumps are done
bool EEClumps::Step()
{
    bool theResult = true;
    if (this->mIndex == this->mClumpCount)
        theResult = false;
    else if (this->mSizes[this->mIndex] == 0) {
        if (!this->AddLocation(this->mCenters[this->mIndex]))
            this->mIndex = this->mClumpCount;
    } else if (this->mSizes[this->mIndex] == this->mTargets[this->mIndex]) {
        if (this->mIndex + 1 < this->mClumpCount) {
            bool theFailed = true;
            for (unsigned long j = 0; j < this->mMap->mMaxLoopCount && theFailed; j++) {
                theFailed = !this->PickDirection();
                if (theFailed)
                    this->PickStart();
            }
            if (theFailed)
                this->mIndex = this->mClumpCount;
            else {
                this->mIndex++;
                this->mSizes[this->mIndex] = 0;
            }
        } else
            this->mIndex = this->mClumpCount;
    } else if (this->mFrontier.size() == 0)
        this->AddClump();
    else {
        long theStatus = 1;
        for (long k = 0; k < this->mMap->mMaxIterationsClumpGrowth && theStatus == 1; k++)
            theStatus = this->Grow();
        if (theStatus != 0)
            this->mIndex = this->mClumpCount;
    }
    return theResult;
}

// FUNCTION: DEFAULT 0x100083b6
// Resets the grower, puts the first clump's centre at a start point and picks a growth direction
void EEClumps::Begin(const GE2DPoint& inStart)
{
    this->Reset();
    this->ResetTargets();
    this->mCenters[0] = inStart;
    this->PickStart();
    this->mStarted = true;
}

// FUNCTION: DEFAULT 0x100083e8
// Sizes the per-clump centres, sizes and targets to the clump count and zeroes them
void EEClumps::Reset()
{
    this->mCenters.resize(this->mClumpCount);
    this->mSizes.resize(this->mClumpCount, 0);
    this->mTargets.resize(this->mClumpCount, 0);
    for (unsigned long i = 0; i < this->mClumpCount; i++) {
        this->mCenters[i] = GE2DPoint();
        this->mSizes[i] = 0;
        this->mTargets[i] = 0;
    }
}

// FUNCTION: DEFAULT 0x1000844c
// Returns whether any of the four neighbours of a tile is free
bool EEClumps::HasFreeNeighbor(long inX, long inY)
{
    if (this->IsFree(inX, inY + 1) || this->IsFree(inX, inY - 1) || this->IsFree(inX + 1, inY) || this->IsFree(inX - 1, inY))
        return true;
    return false;
}

// FUNCTION: DEFAULT 0x1000849f
// Returns whether a tile is free (the base rule)
bool EEClumps::IsValidLocation(long inX, long inY)
{
    return this->IsFree(inX, inY);
}

// FUNCTION: DEFAULT 0x100084af
// Returns whether a tile is inside the allowed area and already claimed
bool EEClumps::IsClaimed(long inX, long inY)
{
    return this->IsInMap(inX, inY) && (*this->mClaimed)[inX][inY] != 0;
}

// FUNCTION: DEFAULT 0x100084e7
// Returns whether a tile is inside the allowed area and not yet claimed
bool EEClumps::IsFree(long inX, long inY)
{
    return this->IsInMap(inX, inY) && (*this->mClaimed)[inX][inY] == 0;
}

// FUNCTION: DEFAULT 0x1000851f
// Returns whether a tile is on the map and inside the allowed area
bool EEClumps::IsInMap(long inX, long inY)
{
    return inX < this->mMapSize && inY < this->mMapSize && (*this->mAllowed)[inX][inY] != 0;
}

// FUNCTION: DEFAULT 0x10008554
// Returns whether a tile is valid and has a free neighbour
bool EEClumps::IsFrontierLocation(long inX, long inY)
{
    return this->IsValidLocation(inX, inY) && this->HasFreeNeighbor(inX, inY);
}

// FUNCTION: DEFAULT 0x10008586
// Picks the next clump centre: turns the growth direction by a random angle scaled by the spread and
// walks from the current centre until a frontier tile; returns whether one was found
bool EEClumps::PickDirection()
{
    GETransformation theTransform;
    GEVector theDirection;
    bool theFound = false;
    GEVector theStep;
    long theX;
    long theY;
    long theStepX;
    long theStepY;
    this->mDirection.Scale((float)(*this->mDistances)[this->mIndex], this->mDirection);
    unsigned long theDistance = (*this->mDistances)[this->mIndex];
    for (unsigned long i = 0; i < this->mMap->mMaxLoopCount && !theFound; i++) {
        theFound = false;
        theDirection = this->mDirection;
        theTransform.SetOrientationYPR((this->mRandom->GenerateUnit() * 6.2831855f - 3.1415927f) * this->mSpread, 0.0f, 0.0f);
        theTransform.Apply(theDirection, theStep);
        theX = UMFloatToLong(this->mCenters[this->mIndex].mX + theStep.mX);
        theY = UMFloatToLong(theStep.mY + this->mCenters[this->mIndex].mY);
        theDirection = theStep;
        theStep.Normalize();
        theStep.Scale(1.99999f, theStep);
        theStepX = UMFloatToLong(theStep.mX);
        theStepY = UMFloatToLong(theStep.mY);
        if ((theStep.mX < 0.0f ? -theStep.mX : theStep.mX) > (theStep.mY < 0.0f ? -theStep.mY : theStep.mY))
            theStepY = 0;
        else
            theStepX = 0;
        for (unsigned long j = 0; j < this->mMap->mMaxLoopCount && !theFound; j++) {
            if (theX < 0 && theStepX < 0)
                break;
            if (theY < 0 && theStepY < 0)
                break;
            if (theX > this->mMaximumX && theStepX > 0)
                break;
            if (theY > this->mMaximumY && theStepY > 0)
                break;
            if (this->IsFrontierLocation(theX, theY)) {
                this->mCenters[this->mIndex + 1].mX = theX;
                theFound = true;
                this->mCenters[this->mIndex + 1].mY = theY;
            } else {
                theX += theStepX;
                theY += theStepY;
            }
        }
    }
    if (theFound) {
        this->mDirection = theDirection;
        this->mDirection.Normalize();
    }
    return theFound;
}

// FUNCTION: DEFAULT 0x1000889b
// Picks a random direction in which a tile one clump spacing away is valid with a free neighbour
bool EEClumps::PickStart()
{
    GE2DPoint theLocation;
    GETransformation theTransform;
    bool theFound = false;
    GEVector theUnit;
    unsigned long i;
    this->mDirection = GEVector();
    for (i = 0; !theFound && i < this->mMap->mMaxLoopCount; i++) {
        theUnit = GEVector(1.0f, 0.0f, 0.0f);
        theUnit.Scale((float)(*this->mDistances)[this->mIndex] + 1.0f, theUnit);
        theTransform.SetOrientationYPR(this->mRandom->GenerateUnit() * 6.2831855f - 3.1415927f, 0.0f, 0.0f);
        theTransform.Apply(theUnit, this->mDirection);
        theLocation.mX = UMFloor(this->mCenters[this->mIndex].mX + this->mDirection.mX);
        theLocation.mY = UMFloor(this->mCenters[this->mIndex].mY + this->mDirection.mY);
        theFound = this->IsValidLocation(UMFloor(theLocation.mX), UMFloor(theLocation.mY)) &&
                   this->HasFreeNeighbor(UMFloor(theLocation.mX), UMFloor(theLocation.mY));
    }
    if (theFound)
        this->mDirection.Normalize();
    return theFound;
}

// FUNCTION: DEFAULT 0x10008ae0
// Claims a tile for the current clump (marking the claim grid if asked) and updates the frontier
void EEClumps::Claim(long inX, long inY, bool inMark)
{
    GE2DPoint thePoint((float)inX, (float)inY);
    if (inMark)
        (*this->mClaimed)[inX][inY] = 1;
    this->mPlaced.push_back(thePoint);
    this->mSizes[this->mIndex]++;
    if (this->HasFreeNeighbor(inX, inY))
        this->mFrontier.push_back(U2DPoint<long>(inX, inY));
    this->UpdateFrontier(inX, inY);
}

// FUNCTION: DEFAULT 0x10008b60
// Drops the claimed neighbours of a tile from the frontier once they have no free neighbour left
void EEClumps::UpdateFrontier(long inX, long inY)
{
    if (this->IsClaimed(inX, inY + 1) && !this->HasFreeNeighbor(inX, inY + 1))
        this->RemoveFrontier(inX, inY + 1);
    if (this->IsClaimed(inX, inY - 1) && !this->HasFreeNeighbor(inX, inY - 1))
        this->RemoveFrontier(inX, inY - 1);
    if (this->IsClaimed(inX + 1, inY) && !this->HasFreeNeighbor(inX + 1, inY))
        this->RemoveFrontier(inX + 1, inY);
    if (this->IsClaimed(inX - 1, inY) && !this->HasFreeNeighbor(inX - 1, inY))
        this->RemoveFrontier(inX - 1, inY);
}

// FUNCTION: DEFAULT 0x10008c19
// Splits the current clump: a new clump takes the size it still lacks and the current one stops
void EEClumps::AddClump()
{
    GE2DPoint theCenter;
    unsigned long theRemaining = this->mTargets[this->mIndex] - this->mSizes[this->mIndex];
    this->mClumpCount++;
    this->mCenters.push_back(theCenter);
    this->mSizes.push_back(0);
    this->mTargets.push_back(theRemaining);
    this->mDistances->push_back((*this->mDistances)[this->mIndex]);
    this->mTargets[this->mIndex] = this->mSizes[this->mIndex];
}
