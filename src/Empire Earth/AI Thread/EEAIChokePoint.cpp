// C:\EEX\Empire Earth\AI Thread\EEAIChokePoint.cpp (file name inferred: no __FILE__; RTTI class EEAIChokePoint,
// vtable 0x439d3c). 0x1c740..0x1d846 (EEAIAStarTerrainStorage.cpp ends at 0x1c740; EEAIContinent's ctor starts at
// 0x1d846). The dtor, AddAreaSerials, then the choke point pass EEAIBuildChokePoints (EEAIContinentManager's terrain pass):
// a per-tile area serial grid (continents and big forests are 0x36 "kinds"), smoothed inPasses - 1 times so that the
// tiles between two areas get a flag, then a flood fill (EEAIAStarMachine<EEAIAStarChokePointStorage,
// EEAIAStarChokePointGoal, EEMap>) that turns each run of flagged tiles into an EEAIChokePoint of gContinentManager.
// The machine's COMDATs follow (ctor, dtor, the inline slots, Run and its steps, EEAIAStarMachineBase's inline ctor).
// File globals in .bss 0x4e5c44..0x4e5c54 (later definitions get lower addresses).
#include <windows.h>
#include <string.h>
#include <vector>
#include <map>
#include <math.h>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEViewport.h"
#include "lle/GETransformation.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFile.h"
#include "lle/U2DPoint.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"

#include "ee/EmpireEarth.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAreaUtilities.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIAStarFastStorage.h"
#include "ee/EEAIAStarTerrainStorage.h"
#include "ee/EEAIAStarChokePointStorage.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIForest.h"
#include "ee/EEAIChokePoint.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIAStarGoalBase.h"
#include "ee/EEAIAStarChokePointGoal.h"
#include "ee/EEAIAStarMachine.h"
#include "ee/EEAIAStarMachineSteps.h"

typedef EEAIAStarMachine<EEAIAStarChokePointStorage, EEAIAStarChokePointGoal, EEMap> EEAIChokePointMachine;

// EEAIContinentManager.cpp's inline (out of line here: 0x29ced)
long EEAIHasResourceUnit(EETile* inTile);

long gEEAIChokeHeight;
long gEEAIChokeWidth;
unsigned char* gEEAIChokeTileFlags;
unsigned long* gEEAIChokeTileSerials;
long* gEEAIChokeTileKinds;

static void EEAIChokeSmooth(long* inKinds, long* ioKinds, long inWidth, long inHeight, bool inFirst);
static void EEAIChokeGetKinds(EEMap* inMap, long* outKinds);
static void EEAIChokeCreate(EEMap* inMap);
static bool EEAIChokeGetTileSerial(EETile* inTile, long inX, long inY, long* outSerial);
static void EEAIChokeDone();

// FUNCTION: EXE 0x41c740
// Destroys the choke point (members only)
EEAIChokePoint::~EEAIChokePoint()
{
}

// FUNCTION: EXE 0x41c75a
// Appends the serials of the areas the choke point joins
void EEAIChokePoint::AddAreaSerials(const std::vector<unsigned long>& inSerials)
{
    this->mAreaSerials.insert(this->mAreaSerials.end(), inSerials.begin(), inSerials.end());
}

// FUNCTION: EXE 0x41c78c
// Runs one smoothing pass over the inner tiles: averages each non-area tile's kind over its 3x3 neighbourhood,
// spreads a neighbour's area serial into it, and flags tiles that touch two different areas (or a flagged one)
static void EEAIChokeSmooth(long* inKinds, long* ioKinds, long inWidth, long inHeight, bool inFirst)
{
    long theSerial = 0;
    long theSize = inWidth * inHeight;
    unsigned char* theFlags = new unsigned char[theSize];
    memcpy(theFlags, gEEAIChokeTileFlags, theSize);
    for (long i = 1; i < inWidth - 1; i++) {
        for (long j = 1; j < inHeight - 1; j++) {
            bool theBorder = false;
            long theIndex = i * inWidth + j;
            if (ioKinds[theIndex] == 0x36)
                continue;
            long theSum = 0;
            long theCount = 0;
            for (long x = i - 1; x <= i + 1; x++) {
                for (long y = j - 1; y <= j + 1; y++) {
                    long theOther = x * inWidth + y;
                    long theKind = inKinds[theOther];
                    if (theKind == 0x36 && !inFirst)
                        continue;
                    theSum += theKind;
                    theCount++;
                    if (theKind && (theSerial = gEEAIChokeTileSerials[theOther]) != 0 &&
                        gEEAIChokeTileSerials[theIndex] &&
                        (gEEAIChokeTileFlags[theOther] || theSerial != gEEAIChokeTileSerials[theIndex]))
                        theBorder = true;
                }
            }
            if (theBorder)
                theFlags[theIndex] = 1;
            if (theSum && gEEAIChokeTileSerials[theIndex] == 0)
                gEEAIChokeTileSerials[theIndex] = theSerial;
            ioKinds[theIndex] = theCount ? theSum / theCount : 0;
        }
    }
    memcpy(gEEAIChokeTileFlags, theFlags, theSize);
    delete[] theFlags;
}

// NEAR: the passes loop keeps its test at the top (jmp back); ours is rotated
// FUNCTION: EXE 0x41c949
// Builds the continent manager's choke points: classifies every tile by area serial, smooths inPasses - 1
// times to flag the tiles between areas, flood-fills the flagged runs into choke points and frees the grids
void EEAIBuildChokePoints(EEMap* inMap, long inPasses)
{
    long theSize = inMap->mSize;
    long theCount = theSize * theSize;
    long theBytes = theCount * sizeof(long);
    long* theKinds = new long[theCount];
    long* theSmoothed = new long[theCount];
    delete[] gEEAIChokeTileKinds;
    gEEAIChokeTileKinds = NULL;
    delete[] gEEAIChokeTileSerials;
    gEEAIChokeTileSerials = new unsigned long[theCount];
    memset(gEEAIChokeTileSerials, 0, theBytes);
    delete[] gEEAIChokeTileFlags;
    gEEAIChokeTileFlags = new unsigned char[theCount];
    memset(gEEAIChokeTileFlags, 0, theCount);
    EEAIChokeGetKinds(inMap, theSmoothed);
    long theLast = inPasses - 1;
    inPasses = theLast;
    while (true) {
        if (inPasses <= 0)
            break;
        EEAIChokeGetKinds(inMap, theKinds);
        EEAIChokeSmooth(theSmoothed, theKinds, theSize, theSize, inPasses == theLast);
        memcpy(theSmoothed, theKinds, theBytes);
        inPasses--;
    }
    gEEAIChokeWidth = theSize;
    gEEAIChokeHeight = theSize;
    EEAIChokeCreate(inMap);
    delete[] theSmoothed;
    delete[] theKinds;
    delete[] gEEAIChokeTileKinds;
    gEEAIChokeTileKinds = NULL;
    delete[] gEEAIChokeTileSerials;
    gEEAIChokeTileSerials = NULL;
    delete[] gEEAIChokeTileFlags;
    gEEAIChokeTileFlags = NULL;
    EEAIChokeDone();
}

// FUNCTION: EXE 0x41ca7c
// Copies the per-tile kind grid (0x36 for tiles in a water/space continent or big forest, else 0) into outKinds,
// building it and the tile serials on first use
static void EEAIChokeGetKinds(EEMap* inMap, long* outKinds)
{
    long theWidth = inMap->mSize;
    long theHeight = inMap->mSize;
    if (gEEAIChokeTileKinds == NULL) {
        gEEAIChokeTileKinds = new long[theWidth * theHeight];
        for (long x = theWidth - 1; x >= 0; x--) {
            for (long y = theHeight - 1; y >= 0; y--) {
                long theSerial;
                long theIndex = x * theHeight + y;
                if (EEAIChokeGetTileSerial(inMap->GetTile(x, y), x, y, &theSerial)) {
                    gEEAIChokeTileSerials[theIndex] = theSerial;
                    gEEAIChokeTileKinds[theIndex] = 0x36;
                } else {
                    gEEAIChokeTileSerials[theIndex] = 0;
                    gEEAIChokeTileKinds[theIndex] = 0;
                }
            }
        }
    }
    memcpy(outKinds, gEEAIChokeTileKinds, theHeight * theWidth * sizeof(long));
}

// NEAR: the original calls the storage's Virtual0C through the vtable (an inline helper or a pointer: unknown) and
// default-constructs the iterator at the top ([ebp-8]); declared there, the slots swap
// FUNCTION: EXE 0x41cb7e
// Flood-fills every flagged non-area tile not yet in a choke point into a new choke point, then gives each one
// a serial and registers it with the continent manager
static void EEAIChokeCreate(EEMap* inMap)
{
    EEAIChokePointMachine theMachine(9, inMap);
    long theSize = inMap->mSize;
    EEAIChokePointMachine* theStorage = &theMachine;   // the storage's virtual through the machine (no null check)
    theStorage->Init(theSize, theSize);
    theMachine.mCheckCorners = false;
    for (long x = theSize - 1; x >= 0; x--) {
        for (long y = theSize - 1; y >= 0; y--) {
            long theIndex = x * theSize + y;
            if (!gEEAIChokeTileFlags[theIndex] || gEEAIChokeTileKinds[theIndex] == 0x36 ||
                theMachine.mChokePointIDs[theMachine.mHeight * x + y] != 0)
                continue;
            theMachine.mGoal.mStartTile.mX = x;
            theMachine.mGoal.mStartTile.mY = y;
            theMachine.mGoal.mGoalTile.mX = inMap->mSize;
            theMachine.mGoal.mGoalTile.mY = inMap->mSize;
            theMachine.Run();
            theMachine.mGoal.CollectTiles();
            theMachine.mGoal.CreateChokePoint();
        }
    }
    gContinentManager.mChokePoints.resize(theMachine.mGoal.mNextIndex);
    for (std::map<long, EEAIChokePoint*>::iterator theIt = theMachine.mGoal.mChokePoints.begin();
         theIt != theMachine.mGoal.mChokePoints.end(); theIt++) {
        EEAIChokePoint* theChokePoint = theIt->second;
        theChokePoint->mSerial = ++gContinentManager.mLastSerial;
        gContinentManager.AddChokePoint(theChokePoint);
    }
}

// NEAR: the original calls _Ptrit::operator+ out of line for mContinents[id] (Dinkum depth), which moves the registers
// FUNCTION: EXE 0x41ccd0
// Returns whether a tile belongs to a fixed area and its serial: a water/space/edge continent's, a forest's over
// 6 tiles (0 for smaller ones), or the land continent's for resources outside a forest; no tile gives -1
static bool EEAIChokeGetTileSerial(EETile* inTile, long inX, long inY, long* outSerial)
{
    bool theResult = inTile == NULL;
    *outSerial = 0;
    if (!theResult) {
        EEAIContinent* theContinent = gContinentManager.GetContinentFromID(gContinentManager.GetContinentID(inX, inY));
        if (theContinent->GetCategory() != kContinentCategoryLand && theContinent->GetCategory() != kContinentCategoryCliff) {
            theResult = true;
            *outSerial = theContinent->mSerial;
        } else {
            theResult = false;
            if (EEAIHasResourceUnit(inTile)) {
                EETileLocation theTile(inX, inY);
                EEAIForest* theForest = gContinentManager.GetForestAt(theTile);
                unsigned short theID = theForest ? theForest->mID : 0;
                const std::vector<EEAIForest*>& theForests = gContinentManager.mForests;
                theForest = theForests[theID];
                if (theForest) {
                    if (theForest->mTiles.Size() > 6)
                        *outSerial = theForest->mSerial;
                    else
                        *outSerial = 0;
                } else
                    *outSerial = theContinent->mSerial;
                theResult = true;
            }
        }
    } else
        *outSerial = -1;
    return theResult;
}

// an empty function (ICF-folded into 0xe123d; called last by EEAIBuildChokePoints) (provisional name)
static void EEAIChokeDone()
{
}

// Compiler-generated for this unit (names in analysis/exe/names.txt): the machine's ctor/dtor/inline slots/??_G, the
// vector<long> insert helpers, Run and its steps (above), the inline FastStorage lookups 0x1d74b/0x1d773 (AddNode,
// their only caller, is ICF-kept at 0x28fc9), EEAIAStarMachineBase's inline ctor/Virtual34/??_G, vector resize.
// TEMPLATE: EXE 0x41cdae
// TEMPLATE: EXE 0x41cdfb
// TEMPLATE: EXE 0x41ce2f
// TEMPLATE: EXE 0x41ce36
// TEMPLATE: EXE 0x41ce3d
// TEMPLATE: EXE 0x41ce4a
// TEMPLATE: EXE 0x41ce57
// TEMPLATE: EXE 0x41ce6e
// TEMPLATE: EXE 0x41ce8a
// TEMPLATE: EXE 0x41ceb1
// TEMPLATE: EXE 0x41cf34
// TEMPLATE: EXE 0x41d184
// TEMPLATE: EXE 0x41d1fb
// TEMPLATE: EXE 0x41d527
// TEMPLATE: EXE 0x41d63b
// TEMPLATE: EXE 0x41d6b1
// TEMPLATE: EXE 0x41d6f8
// TEMPLATE: EXE 0x41d700
// TEMPLATE: EXE 0x41d729
// TEMPLATE: EXE 0x41d72e
// TEMPLATE: EXE 0x41d74b
// TEMPLATE: EXE 0x41d773
// TEMPLATE: EXE 0x41d79b
