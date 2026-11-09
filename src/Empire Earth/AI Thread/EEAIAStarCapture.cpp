// New (port), TEST ONLY: the capture of the tile A* searches (ee/EEAIAStarCapture.h).
#include <cstdlib>
#include <vector>

#include "ee/EESimCapture.h"
#include "ee/EEAIAStarCapture.h"

thread_local EEAIAStarCaptureRun* gEEAIAStarCaptureRun = NULL;

static const unsigned long long kFnvBasis = 0xcbf29ce484222325ULL;
static const unsigned long long kFnvPrime = 0x100000001b3ULL;

static inline unsigned long long EEAStarFnv(unsigned long long inHash, const unsigned char* inData, size_t inSize)
{
    for (size_t i = 0; i < inSize; i++)
        inHash = (inHash ^ inData[i]) * kFnvPrime;
    return inHash;
}

static bool EEAStarKeepEvents()
{
    static const bool sKeep = [] {
        const char* theValue = std::getenv("EE_SIM_CAPTURE_ASTAR_EVENTS");
        return theValue != NULL && theValue[0] == '1';
    }();
    return sKeep;
}

bool EEAIAStarCaptureRun::Active()
{
    return EESimCaptureRecord::Active();
}

EEAIAStarCaptureRun::EEAIAStarCaptureRun()
    : mEventHash(kFnvBasis), mEventCount(0), mStorageHash(kFnvBasis), mOn(false), mOuter(NULL)
{
    if (!EESimCaptureRecord::Active())
        return;
    this->mOn = true;
    this->mOuter = gEEAIAStarCaptureRun;
    gEEAIAStarCaptureRun = this;
}

EEAIAStarCaptureRun::~EEAIAStarCaptureRun()
{
    if (this->mOn)
        gEEAIAStarCaptureRun = this->mOuter;
}

void EEAIAStarCaptureRun::Event(unsigned char inKind, long inA, long inB, long inC, long inD, long inE, long inAnswer)
{
    unsigned char theBytes[25];
    long theValues[6] = {inA, inB, inC, inD, inE, inAnswer};
    theBytes[0] = inKind;
    for (int i = 0; i < 6; i++) {
        unsigned int theValue = (unsigned int)theValues[i];
        theBytes[1 + i * 4] = (unsigned char)theValue;
        theBytes[2 + i * 4] = (unsigned char)(theValue >> 8);
        theBytes[3 + i * 4] = (unsigned char)(theValue >> 16);
        theBytes[4 + i * 4] = (unsigned char)(theValue >> 24);
    }
    this->mEventHash = EEAStarFnv(this->mEventHash, theBytes, sizeof(theBytes));
    this->mEventCount++;
    if (EEAStarKeepEvents())
        this->mEvents.insert(this->mEvents.end(), theBytes, theBytes + sizeof(theBytes));
}

void EEAIAStarCaptureRun::I32(long inValue)
{
    unsigned int theValue = (unsigned int)inValue;
    for (int i = 0; i < 4; i++)
        this->mData.push_back((unsigned char)(theValue >> (8 * i)));
}

void EEAIAStarCaptureRun::Node(const EEAIAStarNodeView& inNode)
{
    this->I32(inNode.mX);
    this->I32(inNode.mY);
    this->I32((long)inNode.mCost);
    this->I32(inNode.mPathCost);
    this->I32(inNode.mEstimate);
    this->U8(inNode.mHasParent);
    this->I32(inNode.mParentX);
    this->I32(inNode.mParentY);
}

void EEAIAStarCaptureRun::Nodes(const std::vector<EEAIAStarNodeView>& inNodes)
{
    // into the storage digest: the count, then each node's fields
    std::vector<unsigned char> theSaved;
    theSaved.swap(this->mData);
    this->I32((long)inNodes.size());
    for (size_t i = 0; i < inNodes.size(); i++)
        this->Node(inNodes[i]);
    this->mStorageHash = EEAStarFnv(this->mStorageHash, this->mData.data(), this->mData.size());
    theSaved.swap(this->mData);
}

void EEAIAStarCaptureRun::Commit()
{
    EESimCaptureRecord theRecord(0x4e525341);   // "ASRN"
    theRecord.Bytes(this->mData.data(), (unsigned int)this->mData.size());
    unsigned int theCount = (unsigned int)this->mEventCount;
    theRecord.U32(theCount);
    theRecord.Bytes(&this->mEventHash, 8);
    theRecord.Bytes(&this->mStorageHash, 8);
    if (!this->mEvents.empty())
        theRecord.Bytes(this->mEvents.data(), (unsigned int)this->mEvents.size());
    theRecord.Commit();
}
