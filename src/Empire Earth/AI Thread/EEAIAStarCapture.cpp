// New (port), TEST ONLY: the capture of the tile A* searches (ee/EEAIAStarCapture.h).
#include <cstdlib>
#include <vector>

#include "ee/EESimCapture.h"
#include "ee/EEAIAStarCapture.h"
#include "rust/sim/astar.h"   // EeAStarNodeView

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

void EEAIAStarCaptureRun::Commit(unsigned long inTag)
{
    EESimCaptureRecord theRecord(inTag);
    theRecord.Bytes(this->mData.data(), (unsigned int)this->mData.size());
    unsigned int theCount = (unsigned int)this->mEventCount;
    theRecord.U32(theCount);
    theRecord.Bytes(&this->mEventHash, 8);
    theRecord.Bytes(&this->mStorageHash, 8);
    if (!this->mEvents.empty())
        theRecord.Bytes(this->mEvents.data(), (unsigned int)this->mEvents.size());
    theRecord.Commit();
}

// The capture's side of the Rust searches (ee_sim::astar search_note.rs): a note, and a run's record (null while the
// capture is off)
extern "C" void ee_astar_capture_note(uint8_t inKind, int32_t inA, int32_t inB, int32_t inC, int32_t inD, int32_t inE,
                                      int32_t inAnswer)
{
    EEAIAStarNote(inKind, inA, inB, inC, inD, inE, inAnswer);
}

extern "C" void* ee_astar_capture_begin()
{
    EEAIAStarCaptureRun* theRun = new EEAIAStarCaptureRun;
    if (theRun->IsOn())
        return theRun;
    delete theRun;
    return NULL;
}

extern "C" void ee_astar_capture_bytes(void* inRun, const uint8_t* inData, uint32_t inLength)
{
    EEAIAStarCaptureRun* theRun = (EEAIAStarCaptureRun*)inRun;
    theRun->mData.insert(theRun->mData.end(), inData, inData + inLength);
}

extern "C" void ee_astar_capture_nodes(void* inRun, const EeAStarNodeView* inNodes, uint32_t inCount)
{
    std::vector<EEAIAStarNodeView> theNodes(inCount);
    for (uint32_t i = 0; i < inCount; i++) {
        theNodes[i].mX = inNodes[i].x;
        theNodes[i].mY = inNodes[i].y;
        theNodes[i].mCost = inNodes[i].cost;
        theNodes[i].mPathCost = inNodes[i].path_cost;
        theNodes[i].mEstimate = inNodes[i].estimate;
        theNodes[i].mHasParent = inNodes[i].has_parent;
        theNodes[i].mParentX = inNodes[i].parent_x;
        theNodes[i].mParentY = inNodes[i].parent_y;
    }
    ((EEAIAStarCaptureRun*)inRun)->Nodes(theNodes);
}

extern "C" void ee_astar_capture_end(void* inRun, uint32_t inTag)
{
    EEAIAStarCaptureRun* theRun = (EEAIAStarCaptureRun*)inRun;
    theRun->Commit(inTag);
    delete theRun;
}
