// C:\EEX\Empire Earth\Units\EEGoalQueueContainer.cpp (file name inferred: no __FILE__; the key "EEGoalQueueContainer-Data").
// A unit's queue of goals (RTTI EEGoalQueueContainer, vtable 0x446f0c; declared as ee/EEUnitVectorItem.h's
// EEUnitVectorItem, the provisional name of the same class): 0x20560c..0x205eda. Unit flags: the exe's module set.
// Data: the key 0x563250 (guard byte 0x563248). New (units behaviour, round 3): the queue is a Rust object
// (ee_units goals.rs); here the typed FSFile calls, the goals' Save and CreateGoal + Load and the unit's refresh its
// record and per-tick update call.

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
#include "rust/sim/units.h"
#include "ee/EECalamityManager.h"

const UString EEUnitVectorItem::sData("EEGoalQueueContainer-Data");

namespace {

FSFile& File(void* inContext)
{
    return *static_cast<EEGoalQueueIO*>(inContext)->mFile;
}

int32_t QueueWriteU32(void* inContext, uint32_t inValue) { return (int32_t)File(inContext).Write(inValue); }
int32_t QueueWriteU8(void* inContext, uint8_t inValue) { return (int32_t)File(inContext).Write((unsigned char)inValue); }
int32_t QueueWriteBool(void* inContext, bool inValue) { return (int32_t)File(inContext).Write(inValue); }
int32_t QueueReadU32(void* inContext, uint32_t* ioValue) { return (int32_t)File(inContext).Read(*ioValue); }
int32_t QueueReadBool(void* inContext, bool* ioValue) { return (int32_t)File(inContext).Read(*ioValue); }

int32_t QueueReadU8(void* inContext, uint8_t* ioValue) { return (int32_t)File(inContext).Read(*(unsigned char*)ioValue); }

int32_t QueueSaveGoal(void* inContext, uint64_t inGoal)
{
    return (int32_t)EEGoalDeque::Goal(inGoal)->Save(static_cast<EEGoalQueueIO*>(inContext)->mMajor, static_cast<EEGoalQueueIO*>(inContext)->mMinor,
                                                    File(inContext));
}

// CreateGoal and its Load: the goal kept on success, deleted otherwise
int32_t QueueLoadGoal(void* inContext, uint8_t inKind, uint64_t* outGoal)
{
    EEGoalQueueIO* theIO = static_cast<EEGoalQueueIO*>(inContext);
    EEUnitGoal* theGoal = EEUnitGoal::CreateGoal(inKind);
    long theResult = theGoal->Load(theIO->mMajor, theIO->mMinor, *theIO->mFile);
    if (theResult != kErrorOK) {
        delete theGoal;
        theGoal = NULL;
    }
    *outGoal = EEGoalDeque::H(theGoal);
    return (int32_t)theResult;
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
    outHost = {&inIO, QueueWriteU32, QueueWriteU8, QueueWriteBool, QueueReadU32, QueueReadU8, QueueReadBool,
               QueueSaveGoal, QueueLoadGoal, QueueRefresh};
}

long EEUnitVectorItem::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEGoalQueueIO theIO = {&inFile, inMajorVersion, inMinorVersion, NULL};
    EeGoalQueueHost theHost;
    EEGoalQueueHostOf(theIO, theHost);
    return ee_units_gq_load(this->mGoals.mQueue, &theHost);
}

long EEUnitVectorItem::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEGoalQueueIO theIO = {&outFile, inMajorVersion, inMinorVersion, NULL};
    EeGoalQueueHost theHost;
    EEGoalQueueHostOf(theIO, theHost);
    return ee_units_gq_save(this->mGoals.mQueue, &theHost);
}
