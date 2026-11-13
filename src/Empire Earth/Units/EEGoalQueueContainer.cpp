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
#include "ee/EECalamityManager.h"

const UString EEUnitVectorItem::sData("EEGoalQueueContainer-Data");
// SYNTHETIC: EXE 0x60560c   (wrapper)
// SYNTHETIC: EXE 0x605616   (ctor)
// SYNTHETIC: EXE 0x605627   (registrar)
// SYNTHETIC: EXE 0x605633   (dtor)

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
void EEUnitVectorItem::Update()
{
    if (gPhysics->mGameTime >= this->mStartTime + this->mTime) {
        this->Clear();
        this->ResetTimer();
    }
}

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
    unsigned char theKind = 0;
    unsigned long theCount;
    EEUnitGoal* theGoal;
    this->Clear();
    long theResult = inFile.Read(theCount);
    if (theResult != kErrorOK)
        return theResult;
    for (unsigned long i = 0; i < theCount; i++) {
        if (theResult != kErrorOK)
            return theResult;
        theResult = inFile.Read(theKind);
        if (theResult != kErrorOK)
            continue;
        theGoal = EEUnitGoal::CreateGoal(theKind);
        theResult = theGoal->Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == kErrorOK)
            this->mGoals.push_back(theGoal);
        else {
            delete theGoal;
            theGoal = NULL;
        }
    }
    if (theResult == kErrorOK) {
        theResult = inFile.Read(this->mStartTime);
        if (theResult == kErrorOK) {
            theResult = inFile.Read(this->mTime);
            if (theResult == kErrorOK)
                theResult = inFile.Read(this->mStarted);
        }
    }
    return theResult;
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
    unsigned long theCount = this->mGoals.size();
    long theResult = outFile.Write(theCount);
    if (theResult != 0)
        return theResult;
    for (theCount = 0; theCount < this->mGoals.size(); theCount++) {
        if (theResult != 0)
            return theResult;
        EEUnitGoal* theGoal = this->mGoals[theCount];
        unsigned char theKind = theGoal->mKind;
        theResult = outFile.Write(theKind);
        if (theResult == 0)
            theResult = theGoal->Save(inMajorVersion, inMinorVersion, outFile);
    }
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mStartTime);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mTime);
    if (theResult == 0)
        theResult = outFile.Write(this->mStarted);
    return theResult;
}

// FUNCTION: EXE 0x605aad
// Stops and zeroes the queue's timer
void EEUnitVectorItem::ResetTimer()
{
    this->mStartTime = 0;
    this->mTime = 0;
    this->mStarted = false;
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
