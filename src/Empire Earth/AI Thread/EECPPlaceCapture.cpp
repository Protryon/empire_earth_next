// New (port), TEST ONLY: PLCE, the placement and resource centres' capture record (ee/EECPPlaceCapture.h;
// tools/sim_capture.py). Reads the objects only.
#include <windows.h>
#include <string.h>
#include <stdint.h>
#include <vector>
#include <deque>
#include <map>
#include <set>
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
#include "lle/USingleton.h"

#include "ee/EEAIFOUnitScreeners.h"
#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EECPPlacement.h"
#include "ee/EECPResourceCenter.h"
#include "ee/EESimCapture.h"
#include "ee/EETechCapture.h"
#include "ee/EECPPlaceCapture.h"

namespace {

thread_local int sPlaceDepth = 0;

void Fnv(uint64_t& ioHash, long inValue)
{
    uint32_t theWord = (uint32_t)inValue;
    const unsigned char* theBytes = (const unsigned char*)&theWord;
    for (int i = 0; i < 4; i++)
        ioHash = (ioHash ^ theBytes[i]) * 0x100000001b3ull;
}

}   // namespace

EECPPlaceScope::EECPPlaceScope(unsigned int inMethod, const EEComputerPlayer* inPlayer)
    : mOuter(sPlaceDepth++ == 0), mOn(false), mMethod(inMethod),
      mPlayer(inPlayer != NULL ? (unsigned int)inPlayer->mPlayerID : 0xffffffffu)
{
    this->mOn = this->mOuter && EESimCaptureRecord::Active();
}

EECPPlaceScope::~EECPPlaceScope()
{
    sPlaceDepth--;
    if (!this->mOn)
        return;
    EESimCaptureRecord r(EECaptureTag("PLCE"));
    r.U32(this->mMethod);
    r.U32(this->mPlayer);
    r.U32((unsigned int)this->mWords.size());
    if (!this->mWords.empty())
        r.Bytes(&this->mWords[0], (unsigned int)(this->mWords.size() * 4));
    r.Commit();
}

EECPPlaceScope& EECPPlaceScope::F(float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, 4);
    return this->I((long)theBits);
}

EECPPlaceScope& EECPPlaceScope::U(const EEUnit* inUnit)
{
    return this->I(inUnit != NULL ? (long)((const RTSUnit*)inUnit)->mUnitID : (long)0xfffffffe);
}

EECPPlaceScope& EECPPlaceScope::CU(const EEComplexUnit* inUnit)
{
    return this->U((const EEUnit*)inUnit);
}

EECPPlaceScope& EECPPlaceScope::State(const EECPPlacement* inPlacement)
{
    if (!this->mOn || inPlacement == NULL)
        return *this;
    this->I(inPlacement->mTownCenterTile.mX).I(inPlacement->mTownCenterTile.mY);
    const std::vector<U2DPoint<long> >* theLists[2] = {&inPlacement->mWaterSites, &inPlacement->mSpaceSites};
    for (int k = 0; k < 2; k++) {
        uint64_t theHash = 0xcbf29ce484222325ull;
        for (size_t i = 0; i < theLists[k]->size(); i++) {
            Fnv(theHash, (*theLists[k])[i].mX);
            Fnv(theHash, (*theLists[k])[i].mY);
        }
        this->I((long)theLists[k]->size()).I((long)(uint32_t)theHash).I((long)(uint32_t)(theHash >> 32));
    }
    this->I(inPlacement->mSiteTilesCollected ? 1 : 0).I((long)inPlacement->mTypesSeen.size());
    return *this;
}

EECPPlaceScope& EECPPlaceScope::State(const EECPResourceCenter* inCenter)
{
    if (!this->mOn || inCenter == NULL)
        return *this;
    this->I((long)inCenter->mUnits.size());
    for (size_t i = 0; i < inCenter->mUnits.size(); i++)
        this->I((long)inCenter->mUnits[i].GetSavedID());
    // (not mIsActive: a new centre's is uninitialized until its first SetActive; SetActive's record has the flag)
    this->I((long)inCenter->mUnit.GetSavedID());
    return *this;
}

EECPPlaceScope& EECPPlaceScope::Centers(const EEComputerPlayer* inPlayer)
{
    if (!this->mOn || inPlayer == NULL)
        return *this;
    uint64_t theHash = 0xcbf29ce484222325ull;
    for (size_t k = 0; k < inPlayer->mResourceCenters.size(); k++) {
        const EECPResourceCenter* theCenter = inPlayer->mResourceCenters[k];
        if (theCenter == NULL) {
            Fnv(theHash, -3);
            continue;
        }
        Fnv(theHash, (long)theCenter->mUnits.size());
        for (size_t i = 0; i < theCenter->mUnits.size(); i++)
            Fnv(theHash, (long)theCenter->mUnits[i].GetSavedID());
        Fnv(theHash, (long)theCenter->mUnit.GetSavedID());
    }
    return this->I((long)inPlayer->mResourceCenters.size()).I((long)(uint32_t)theHash).I((long)(uint32_t)(theHash >> 32));
}
