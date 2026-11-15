// C:\EEX\Empire Earth\Units\EEGoalQueueContainer.cpp (file name inferred: no __FILE__; the key "EEGoalQueueContainer-Data").
// A unit's queue of goals (RTTI EEGoalQueueContainer, vtable 0x446f0c; declared as ee/EEUnitVectorItem.h's
// EEUnitVectorItem, the provisional name of the same class): 0x20560c..0x205eda. Unit flags: the exe's module set.
// Data: the key 0x563250 (guard byte 0x563248). The tail holds the deque<EEUnitGoal*> COMDATs this unit instantiates
// first (resize 0x205b7b, the iterator's operator- 0x205bc0, insert(P, M, X) 0x205bd5: in include/dinkum/deque)
// and DeleteVectorContents<deque<EEUnitGoal*> > 0x205e8c.

#include <windows.h>
#include <stdio.h>
#include <deque>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/USingleton.h"

#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEUnitStore.h"
#include "rust/ee_units.h"
#include "ee/EECalamityManager.h"

const UString EEUnitVectorItem::sData("EEGoalQueueContainer-Data");
// SYNTHETIC: EXE 0x60560c   (wrapper)
// SYNTHETIC: EXE 0x605616   (ctor)
// SYNTHETIC: EXE 0x605627   (registrar)
// SYNTHETIC: EXE 0x605633   (dtor)

// New (R5b.4, the sixteenth round): the host of the queues' record and per-tick update in Rust (ee_units goals.rs):
// typed FSFile reads and writes, the goals' own Save and CreateGoal + Load, the container's fields; the update's
// expiry and refresh (EEUnitUpdate.cpp hands a unit, no file)
namespace {

int32_t QueueWriteU32(void* inContext, uint32_t inValue)
{
    return (int32_t)static_cast<EEGoalQueueIO*>(inContext)->mFile->Write(inValue);
}

int32_t QueueWriteU8(void* inContext, uint8_t inValue)
{
    unsigned char theValue = inValue;
    return (int32_t)static_cast<EEGoalQueueIO*>(inContext)->mFile->Write(theValue);
}

int32_t QueueWriteBool(void* inContext, bool inValue)
{
    return (int32_t)static_cast<EEGoalQueueIO*>(inContext)->mFile->Write(inValue);
}

int32_t QueueReadU32(void* inContext, uint32_t* outValue)
{
    return (int32_t)static_cast<EEGoalQueueIO*>(inContext)->mFile->Read(*outValue);
}

int32_t QueueReadU8(void* inContext, uint8_t* outValue)
{
    unsigned char theValue = *outValue;
    long theResult = static_cast<EEGoalQueueIO*>(inContext)->mFile->Read(theValue);
    *outValue = theValue;
    return (int32_t)theResult;
}

int32_t QueueSaveGoal(void* inContext, uint64_t inGoal)
{
    EEGoalQueueIO* theIO = static_cast<EEGoalQueueIO*>(inContext);
    EEUnitGoal* theGoal = reinterpret_cast<EEUnitGoal*>(static_cast<uintptr_t>(inGoal));
    return (int32_t)theGoal->Save(theIO->mMajor, theIO->mMinor, *theIO->mFile);
}

int32_t QueueLoadGoal(void* inContext, uint8_t inKind)
{
    EEGoalQueueIO* theIO = static_cast<EEGoalQueueIO*>(inContext);
    EEUnitGoal* theGoal = EEUnitGoal::CreateGoal(inKind);
    long theResult = theGoal->Load(theIO->mMajor, theIO->mMinor, *theIO->mFile);
    if (theResult == kErrorOK)
        theIO->mQueue->mGoals.push_back(theGoal);
    else
        delete theGoal;
    return (int32_t)theResult;
}

void QueueClear(void* inContext)
{
    static_cast<EEGoalQueueIO*>(inContext)->mQueue->Clear();
}

int32_t QueueReadTimer(void* inContext, uint32_t inField)
{
    EEGoalQueueIO* theIO = static_cast<EEGoalQueueIO*>(inContext);
    EEUnitVectorItem* theQueue = theIO->mQueue;
    long theResult;
    if (inField == 0)
        theResult = theIO->mFile->Read(theQueue->mStartTime);
    else if (inField == 1)
        theResult = theIO->mFile->Read(theQueue->mTime);
    else
        theResult = theIO->mFile->Read(theQueue->mStarted);
    EEGoalQueueReport(theQueue);
    return (int32_t)theResult;
}

void QueueExpire(void* inContext, uint32_t inIndex)
{
    EEUnitVectorItem& theQueue = static_cast<EEGoalQueueIO*>(inContext)->mUnit->mGoalQueues[inIndex];
    theQueue.Clear();
    theQueue.ResetTimer();
}

void QueueRefresh(void* inContext)
{
    EEComplexUnit* theUnit = static_cast<EEGoalQueueIO*>(inContext)->mUnit;
    theUnit->RefreshGoals();
    theUnit->RefreshActiveGoal();
}

}  // namespace

void EEGoalQueueHostOf(EEGoalQueueIO& inIO, EeGoalQueueHost& outHost)
{
    outHost.context = &inIO;
    outHost.write_u32 = QueueWriteU32;
    outHost.write_u8 = QueueWriteU8;
    outHost.write_bool = QueueWriteBool;
    outHost.read_u32 = QueueReadU32;
    outHost.read_u8 = QueueReadU8;
    outHost.save_goal = QueueSaveGoal;
    outHost.load_goal = QueueLoadGoal;
    outHost.clear = QueueClear;
    outHost.read_timer = QueueReadTimer;
    outHost.expire = QueueExpire;
    outHost.refresh = QueueRefresh;
}

// FUNCTION: EXE 0x60564f
// Creates an empty goal queue for a unit, with its timer stopped
EEUnitVectorItem::EEUnitVectorItem(EEComplexUnit* inUnit)
{
    this->mUnit = inUnit;
    this->ResetTimer();
}

// SYNTHETIC: EXE 0x605681
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x60569d
// Creates a copy of another queue: clones its goals for the same unit and copies the timer
EEUnitVectorItem::EEUnitVectorItem(const EEUnitVectorItem& inItem)
{
    this->mUnit = inItem.mUnit;
    this->Clear();
    this->mGoals.resize(inItem.mGoals.size(), NULL);
    for (long i = inItem.mGoals.size() - 1; i >= 0; i--) {
        this->mGoals.Set(i, inItem.mGoals[i]->Clone());
        this->mGoals[i]->SetOwner(this->mUnit);
    }
    this->mStartTime = inItem.mStartTime;
    this->mTime = inItem.mTime;
    this->mStarted = inItem.mStarted;
    EEGoalQueueReport(this);   // New (R5b.4, the sixteenth round): the mirror's timer
}

// FUNCTION: EXE 0x605768
// Destroys the queue, deleting its goals
EEUnitVectorItem::~EEUnitVectorItem()
{
    this->Clear();
    EEGoalQueueDropped(this);   // New (R5b.4, the fifteenth round): out of the Rust mirror (ee/EEUnitStore.h)
}

// FUNCTION: EXE 0x605780
// Empties the queue once its timer has run out

// FUNCTION: EXE 0x6057a7
// Tells each goal it left the queue, deletes them all and stops the timer
void EEUnitVectorItem::Clear()
{
    for (unsigned long i = 0; i < this->mGoals.size(); i++)
        this->mGoals[i]->RemovedFromQueue(this->mUnit);
    DeleteVectorContents(this->mGoals);
    this->ResetTimer();
}

// FUNCTION: EXE 0x6057fa
// Returns whether the unit's player is queuing orders (shift mode)
bool EEUnitVectorItem::IsShiftMode()
{
    return this->mUnit->mPlayer->mShiftMode;
}

// FUNCTION: EXE 0x605807
// Returns the game time left on the queue's timer, 0 if it is not running
unsigned long EEUnitVectorItem::GetTimeLeft()
{
    unsigned long theResult = 0;
    if (this->mStarted)
        theResult = this->mTime - gPhysics->mGameTime + this->mStartTime;
    return theResult;
}

// FUNCTION: EXE 0x605821
// Adds time to the queue's timer, starting it now if it is not running
void EEUnitVectorItem::AddTime(long inTime)
{
    if (!this->mStarted) {
        this->mStarted = true;
        this->mStartTime = gPhysics->mGameTime;
    }
    this->mTime += inTime;
    EEGoalQueueReport(this);   // New (R5b.4, the sixteenth round): the mirror's timer (ee/EEUnitStore.h)
}

// FUNCTION: EXE 0x605843
// Inserts a goal at a position (appending past the end) and makes the unit its owner
void EEUnitVectorItem::Insert(unsigned long inIndex, EEUnitGoal* inGoal)
{
    if (inIndex < this->mGoals.size())
        this->mGoals.insert(this->mGoals.begin() + inIndex, inGoal);
    else
        this->mGoals.push_back(inGoal);
    inGoal->SetOwner(this->mUnit);
}

// FUNCTION: EXE 0x60588b
// Replaces the goals with those read from the saved game (kind, then data; a goal that fails is deleted)
// and reads the timer; returns the first read error
long EEUnitVectorItem::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    // New (R5b.4, the sixteenth round): the record's sequence is Rust (ee_units goals.rs load), the reads, the
    // goals' CreateGoal and Load and the container's fields this host's
    EEGoalQueueIO theIO = {this, &inFile, inMajorVersion, inMinorVersion, NULL};
    EeGoalQueueHost theHost;
    EEGoalQueueHostOf(theIO, theHost);
    return ee_units_goal_queue_load(&theHost);
}

// FUNCTION: EXE 0x605960
// Removes and deletes the last goal
void EEUnitVectorItem::PopBack()
{
    this->mGoals.back()->RemovedFromQueue(this->mUnit);
    delete this->mGoals.back();
    this->mGoals.pop_back();
}

// FUNCTION: EXE 0x605999
// Removes and deletes the first goal
void EEUnitVectorItem::PopFront()
{
    this->mGoals.front()->RemovedFromQueue(this->mUnit);
    delete this->mGoals.front();
    this->mGoals.pop_front();
}

// FUNCTION: EXE 0x6059ea
// Writes the goals (kind, then data) and the timer to the saved game; returns the first write error
long EEUnitVectorItem::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    // New (R5b.4, the sixteenth round): written from the Rust mirror (ee_units goals.rs save): the goals' kinds and
    // the timer from it, each goal's own record by this host
    EEGoalQueueIO theIO = {this, &outFile, inMajorVersion, inMinorVersion, NULL};
    EeGoalQueueHost theHost;
    EEGoalQueueHostOf(theIO, theHost);
    return ee_units_goal_queue_save(reinterpret_cast<uintptr_t>(this), &theHost);
}

// FUNCTION: EXE 0x605aad
// Stops and zeroes the queue's timer
void EEUnitVectorItem::ResetTimer()
{
    this->mStartTime = 0;
    this->mTime = 0;
    this->mStarted = false;
    EEGoalQueueReport(this);   // New (R5b.4, the sixteenth round): the mirror's timer
}

// FUNCTION: EXE 0x605ab9
// Copies another queue: clones its goals for the same unit and copies the timer (self-assignment does nothing)
EEUnitVectorItem& EEUnitVectorItem::operator=(const EEUnitVectorItem& inItem)
{
    if (&inItem != this) {
        this->mUnit = inItem.mUnit;
        this->Clear();
        this->mGoals.resize(inItem.mGoals.size(), NULL);
        for (long i = inItem.mGoals.size() - 1; i >= 0; i--) {
            this->mGoals.Set(i, inItem.mGoals[i]->Clone());
            this->mGoals[i]->SetOwner(this->mUnit);
        }
        this->mStartTime = inItem.mStartTime;
        this->mTime = inItem.mTime;
        this->mStarted = inItem.mStarted;
        EEGoalQueueReport(this);   // New (R5b.4, the sixteenth round): the mirror's timer
    }
    return *this;
}

// TEMPLATE: EXE 0x605b7b
// deque<EEUnitGoal*>::resize(n, x)

// TEMPLATE: EXE 0x605bc0
// deque<EEUnitGoal*>::iterator::operator-(difference_type)

// TEMPLATE: EXE 0x605bd5
// deque<EEUnitGoal*>::insert(iterator, size_type, const T&) (NEAR: Dinkum instruction scheduling differs)

// TEMPLATE: EXE 0x605e8c
// DeleteVectorContents<std::deque<EEUnitGoal*> > (ee/EECalamityManager.h), first used by Clear
