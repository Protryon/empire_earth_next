// C:\EEX\Empire Earth\Scenario Manager\EEScenarioManager.cpp (file name inferred: no __FILE__): 0x1afa90..0x1ba02f,
// with the functions of EESMDialogMessage.cpp folded in. Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/UWideString.h"
#include "ee/EESMDialogMessage.h"
#include "lle/FSFile.h"
#include <vector>
#include "ee/EEAreaUtilities.h"
#include "ee/EEScenarioManager.h"
#include "ee/EESMCondition.h"
#include <algorithm>
#include <mbstring.h>
#include <stdlib.h>
#include "ee/EESMEffect.h"
#include "ee/EESMObject.h"
#include "ee/EESMArea.h"
#include "ee/EESMTrigger.h"
#include "ee/EEDeleteVectorContents.h"
#include "lle/UResourceFile.h"
#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEPhysics.h"
#include "ee/EEAIRandom.h"
#include "ee/EEDebugTrace.h"
#include "LostDebug.h"
#include "lle/SESoundManager.h"
#include "lle/FSFileSpec.h"
#include "GEWindow.h"
#include "ee/EECameraControl.h"
#include "ee/EEGraphics.h"
#include "ee/EEMouse.h"
#include "ee/EEUserInterface.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEBitmap.h"
#include "lle/GEVertexArray.h"
#include "lle/GERhombus.h"
#include "lle/ISEntity.h"
#include "lle/UIEntity.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GERasterizer.h"
#include "lle/UIManager.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEGameSound.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "ee/EECommunications.h"
#include "ee/EEStringIDs.h"
#include "ee/EEPersistentTag.h"
// the vector Load/Save helpers (cdecl template COMDATs 0x1b5faf / 0x1b5fff: the count, then each element's slot)
template<class T> long EESMLoadVector(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile,
                                      std::vector<T>& outVector)
{
    unsigned long theCount;
    long theResult = inFile.Read(theCount);
    outVector.resize(theCount);
    for (std::vector<T>::iterator theIterator = outVector.begin(); theIterator != outVector.end() && theResult == kErrorOK;
         theIterator++)
        theResult = theIterator->Load(inMajorVersion, inMinorVersion, inFile);
    return theResult;
}
template<class T> long EESMSaveVector(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile,
                                      std::vector<T>& inVector)
{
    unsigned long theCount = inVector.size();
    long theResult = outFile.Write(theCount);
    for (std::vector<T>::iterator theIterator = inVector.begin(); theIterator != inVector.end() && theResult == 0;
         theIterator++)
        theResult = theIterator->Save(inMajorVersion, inMinorVersion, outFile);
    return theResult;
}

static const float kTriggerInterval = 500.0f;  // 0x445fcc (trigger interval, ms: Update, ApplyPendingEffects; unit .rdata; provisional name)
static const float kSMZero = 0.0f;           // 0x445fd0 (Update's mTriggerTimer test while mIsMovingCamera; unit .rdata; provisional name)
extern float gF4BE374;                       // 0x4be374 (= 1.0f; Update ends the cinematic when it is 1; provisional name)
extern float gSideScrollSpeed;               // 0x530828 (MoveCameraToStart's camera step scale; provisional name)

// the stable_sort predicates (empty function objects passed by value: 0x1b5e77..0x1b5f7b; provisional names)
struct EESMTimedEffectLess {
    bool operator()(EESMTimedEffect inA, EESMTimedEffect inB) const { return inA.mTime < inB.mTime; }
};
struct EESMConditionLess {
    bool operator()(EESMCondition* inA, EESMCondition* inB) const { return inA->mName < inB->mName; }
};
struct EESMEffectLess {
    bool operator()(EESMEffect* inA, EESMEffect* inB) const { return inA->mName < inB->mName; }
};
struct EESMObjectLess {
    bool operator()(EESMObject* inA, EESMObject* inB) const { return inA->mName < inB->mName; }
};
struct EESMAreaLess {
    bool operator()(EESMArea* inA, EESMArea* inB) const { return inA->mName < inB->mName; }
};
struct EESMTriggerLess {
    bool operator()(EESMTrigger* inA, EESMTrigger* inB) const { return inA->mName < inB->mName; }
};

// FUNCTION: EXE 0x5afa90
// Creates a scenario holding only the default entries (see Reset)
EEScenarioManager::EEScenarioManager()
    : FSPersistent(kPersistentScenario)
{
    this->Reset();
}

// SYNTHETIC: EXE 0x5afb95
// (the scalar deleting destructor, emitted with the vtable; slot EEScenarioManager+0x10)

// FUNCTION: EXE 0x5afbb1
// Empties the scenario back to its defaults (an Always True condition, a Do Nothing effect, a None object, the
// Off Map and Previous Location areas); zeroes the clocks and ID counters, clears the timed and pending effects
void EEScenarioManager::Reset()
{
    UWideString theName;

    this->mIsMovingCamera = false;
    this->mTime = 0.0f;
    this->mShowDebugIDs = false;
    this->mTriggerTimer = 0.0f;
    this->mCinematicStartTime = 0.0f;
    this->mNextTriggerID = 0;
    this->mNextConditionID = 0;
    this->mNextEffectID = 0;
    this->mStartX = 0.0f;
    this->mNextObjectID = 0;
    this->mNextAreaID = 0;
    this->mGameResult = 0;
    this->mStartY = 0.0f;

    DeleteVectorContents(this->mTriggers);
    DeleteVectorContents(this->mConditions);
    EESMCondition* theCondition = new EESMCondition;
    gResource->GetString(IDS_ALWAYS_TRUE, theName);
    theCondition->mName = theName;
    theCondition->mType = kSMConditionAlwaysTrue;
    this->mConditions.push_back(theCondition);

    DeleteVectorContents(this->mEffects);
    EESMEffect* theEffect = new EESMEffect;
    gResource->GetString(IDS_DO_NOTHING, theName);
    theEffect->mName = theName;
    theEffect->mType = kSMEffectNone;
    this->mEffects.push_back(theEffect);

    DeleteVectorContents(this->mObjects);
    EESMObject* theObject = new EESMObject;
    gResource->GetString(IDS_NONE, theName);
    theObject->SetName(theName);
    this->mObjects.push_back(theObject);

    DeleteVectorContents(this->mAreas);
    this->mAreas.clear();
    EESMArea* theArea = new EESMArea;
    gResource->GetString(IDS_OFF_MAP, theName);
    theArea->mName = theName;
    theArea->SetAnchor(U2DPoint<long>(-10000, -10000));
    this->mAreas.push_back(theArea);
    theArea = new EESMArea;
    gResource->GetString(IDS_PREVIOUS_LOCATION, theName);
    theArea->mName = theName;
    theArea->SetAnchor(U2DPoint<long>(-1, -1));
    this->mAreas.push_back(theArea);

    this->mTimedEffects.clear();
    this->mPendingEffects.clear();
    this->mShowDebugIDs = false;
    this->mIsShowingDialog = false;
    this->mIsEndingCinematic = false;
    this->mB234 = true;
}

// FUNCTION: EXE 0x5afe66
// Destroys the scenario: unloads the unit class media Prepare loaded and deletes every trigger, condition, effect,
// object, area and pending effect
EEScenarioManager::~EEScenarioManager()
{
    // LOST-DEBUG: none: exact only (tools lostdebug_search, n=1 at START)
    EE_LOST_DEBUG(1);
    for (unsigned long i = 0; i < this->mUnitClassMedia.size(); i++) {
        EEPlayer* thePlayer = gWorld.GetPlayer(this->mUnitClassMedia[i].mPlayer);
        thePlayer->GetUpgrade(this->mUnitClassMedia[i].mUnitClass)->UnloadMedia(thePlayer);
        // LOST-DEBUG: a third mUnitClassMedia[i] whose result is discarded (0x1afe66); in Virtual1C 0x1b28c0 the same
        // expression feeds the trace call 0x9ba76, compiled away here (owner-approved stand-in, 2026-10-25)
        EE_LOST_TRACE(this->mUnitClassMedia[i]);
    }
    DeleteVectorContents(this->mTriggers);
    DeleteVectorContents(this->mConditions);
    DeleteVectorContents(this->mEffects);
    DeleteVectorContents(this->mObjects);
    DeleteVectorContents(this->mAreas);
    this->mTimedEffects.clear();
    DeleteVectorContents(this->mPendingEffects);
}

// FUNCTION: EXE 0x5afffb
// Queues a dialog message for ShowDialogMessage
void EEScenarioManager::AddMessage(const EESMDialogMessage& inMessage)
{
    this->mMessages.push_back(inMessage);
}

// FUNCTION: EXE 0x5b000d
// Records a copy of an applied effect whose change a saved game must replay on load (ignored until
// ApplyPendingEffects has run)
void EEScenarioManager::AddV134Effect(const EESMEffect& inEffect)
{
    if (this->mB234)
        return;
    EESMEffect* theEffect = new EESMEffect;
    theEffect->Copy(inEffect);
    this->mPendingEffects.push_back(theEffect);
}

// FUNCTION: EXE 0x5b005b
// Schedules an effect (by index) to run at the given scenario time, keeping the timed effects sorted by time
void EEScenarioManager::AddTimedEffect(long inEffect, unsigned long inTime, bool inIsInCinematic)
{
    EESMTimedEffect theEntry(inEffect, inTime, inIsInCinematic);
    this->mTimedEffects.push_back(theEntry);
    std::stable_sort(this->mTimedEffects.begin(), this->mTimedEffects.end(), EESMTimedEffectLess());
}

// FUNCTION: EXE 0x5b00c1
// Sorts the user areas by name (the two built-in areas stay first) and remaps every area reference
void EEScenarioManager::SortAreas()
{
    std::vector<EESMArea*> theOldAreas(this->mAreas);
    EESMIDMap theMap;
    std::stable_sort(this->mAreas.begin() + 2, this->mAreas.end(), EESMAreaLess());
    for (unsigned long i = 0; i < this->mAreas.size(); i++) {
        for (unsigned long j = 0; j < theOldAreas.size(); j++) {
            if (theOldAreas[j] == this->mAreas[i]) {
                theMap.insert(EESMIDMap::value_type(j, i));
                break;
            }
        }
    }
    this->RemapAreas(theMap);
}

// FUNCTION: EXE 0x5b01a6
// Sorts the user conditions by name (Always True stays first, the object conditions last) and remaps every
// condition reference
void EEScenarioManager::SortConditions()
{
    std::vector<EESMCondition*> theOldConditions(this->mConditions);
    EESMIDMap theMap;
    unsigned long i;
    for (i = 0; i < this->mConditions.size(); i++)
        if (this->mConditions[i]->GetType() == kSMConditionObject)
            break;
    std::stable_sort(this->mConditions.begin() + 1, this->mConditions.end() - (this->mConditions.size() - i), EESMConditionLess());
    for (unsigned long theNew = 0; theNew < this->mConditions.size(); theNew++) {
        for (unsigned long theOld = 0; theOld < theOldConditions.size(); theOld++) {
            if (theOldConditions[theOld] == this->mConditions[theNew]) {
                theMap.insert(EESMIDMap::value_type(theOld, theNew));
                break;
            }
        }
    }
    this->RemapConditions(theMap);
}

// FUNCTION: EXE 0x5b02f6
// Drops every timed effect, sorts the user effects by name (Do Nothing stays first) and remaps effect references
void EEScenarioManager::SortEffects()
{
    std::vector<EESMEffect*> theOldEffects(this->mEffects);
    EESMIDMap theMap;
    this->mTimedEffects.clear();
    std::stable_sort(this->mEffects.begin() + 1, this->mEffects.end(), EESMEffectLess());
    for (unsigned long theNew = 1; theNew < this->mEffects.size(); theNew++) {
        for (unsigned long theOld = 0; theOld < theOldEffects.size(); theOld++) {
            if (theOldEffects[theOld] == this->mEffects[theNew]) {
                theMap.insert(EESMIDMap::value_type(theOld, theNew));
                break;
            }
        }
    }
    this->RemapEffects(theMap);
}

// FUNCTION: EXE 0x5b041c
// Sorts the user objects (None stays first) and the object conditions by name, then remaps object and condition
// references
void EEScenarioManager::SortObjects()
{
    std::vector<EESMObject*> theOldObjects(this->mObjects);
    std::vector<EESMCondition*> theOldConditions(this->mConditions);
    EESMIDMap theMap;
    std::stable_sort(this->mObjects.begin() + 1, this->mObjects.end(), EESMObjectLess());
    unsigned long i;
    for (i = 0; i < this->mConditions.size(); i++)
        if (this->mConditions[i]->mType == kSMConditionObject)
            break;
    std::stable_sort(this->mConditions.begin() + i, this->mConditions.end(), EESMConditionLess());
    for (unsigned long theNew = 1; theNew < this->mObjects.size(); theNew++) {
        for (unsigned long theOld = 0; theOld < theOldObjects.size(); theOld++) {
            if (theOldObjects[theOld] == this->mObjects[theNew]) {
                theMap.insert(EESMIDMap::value_type(theOld, theNew));
                break;
            }
        }
    }
    this->RemapObjects(theMap);
    theMap.clear();
    for (unsigned long theNewCondition = 1; theNewCondition < this->mConditions.size(); theNewCondition++) {
        for (unsigned long theOldCondition = 0; theOldCondition < theOldConditions.size(); theOldCondition++) {
            if (theOldConditions[theOldCondition] == this->mConditions[theNewCondition]) {
                theMap.insert(EESMIDMap::value_type(theOldCondition, theNewCondition));
                break;
            }
        }
    }
    this->RemapConditions(theMap);
}

// FUNCTION: EXE 0x5b0674
// Sorts the triggers by name and remaps every trigger reference
void EEScenarioManager::SortTriggers()
{
    std::vector<EESMTrigger*> theOldTriggers(this->mTriggers);
    EESMIDMap theMap;
    std::stable_sort(this->mTriggers.begin(), this->mTriggers.end(), EESMTriggerLess());
    for (unsigned long i = 0; i < this->mTriggers.size(); i++) {
        for (unsigned long j = 0; j < theOldTriggers.size(); j++) {
            if (theOldTriggers[j] == this->mTriggers[i]) {
                theMap.insert(EESMIDMap::value_type(j, i));
                break;
            }
        }
    }
    this->RemapTriggers(theMap);
}

// FUNCTION: EXE 0x5b074c
// Appends a new area named <AreaN> and returns it (its ID is the incremented counter, N + 1)
EESMArea* EEScenarioManager::NewArea()
{
    UWideString theName;
    sprintf(theName, "<Area%d>", this->mNextAreaID++);
    EESMArea* theArea = new EESMArea;
    theArea->mName = theName;
    theArea->mID = this->mNextAreaID;
    this->mAreas.push_back(theArea);
    return theArea;
}

// FUNCTION: EXE 0x5b07d2
// Inserts a new condition named <ConditionN> before the object conditions, remaps condition references and
// returns it (its ID is N + 1)
EESMCondition* EEScenarioManager::NewCondition()
{
    // LOST-DEBUG: none: exact only (tools lostdebug_search, n=3 at START; the original refuses the _Tree ctor inline)
    EE_LOST_DEBUG(3);
    std::vector<EESMCondition*> theOldConditions(this->mConditions);
    EESMIDMap theMap;
    UWideString theName;
    unsigned long i;
    for (i = 0; i < this->mConditions.size(); i++)
        if (this->mConditions[i]->mType == kSMConditionObject)
            break;
    sprintf(theName, "<Condition%d>", this->mNextConditionID++);
    EESMCondition* theCondition = new EESMCondition;
    theCondition->mName = theName;
    theCondition->mID = this->mNextConditionID;
    this->mConditions.insert(this->mConditions.begin() + i, theCondition);
    for (unsigned long theNew = 0; theNew < this->mConditions.size(); theNew++) {
        for (unsigned long theOld = 0; theOld < theOldConditions.size(); theOld++) {
            if (theOldConditions[theOld] == this->mConditions[theNew]) {
                theMap.insert(EESMIDMap::value_type(theOld, theNew));
                break;
            }
        }
    }
    this->RemapConditions(theMap);
    return theCondition;
}

// FUNCTION: EXE 0x5b0982
// Creates a new, default scenario manager (the persistent object factory)
FSPersistent* EEScenarioManager::CreateDefault() const
{
    return new EEScenarioManager;
}

// FUNCTION: EXE 0x5b099c
// Appends a new effect named <EffectN> and returns it (its ID is the incremented counter, N + 1)
EESMEffect* EEScenarioManager::NewEffect()
{
    UWideString theName;
    sprintf(theName, "<Effect%d>", this->mNextEffectID++);
    EESMEffect* theEffect = new EESMEffect;
    theEffect->mName = theName;
    theEffect->mID = this->mNextEffectID;
    this->mEffects.push_back(theEffect);
    return theEffect;
}

// FUNCTION: EXE 0x5b0a28
// Appends a new scenario object with the next object ID and an Exists condition for it, named from the object's
// default name (<id:None Exists>); returns the object
EESMObject* EEScenarioManager::NewObject()
{
    UWideString theName;
    UWideString theText;
    EESMObject* theObject = new EESMObject;
    theObject->mL64 = ++this->mNextObjectID;
    this->mObjects.push_back(theObject);
    if (this->mObjects.size() <= 1)
        return theObject;
    EESMCondition* theCondition = new EESMCondition;
    this->mConditions.push_back(theCondition);
    ++this->mNextConditionID;
    gResource->GetString(IDS_EXISTS, theText);
    sprintf(theName, "<%d:%s %s>", this->mNextConditionID, (const char*)theObject->mName, (const char*)theText);
    theCondition->mName = theName;
    theCondition->mID = this->mNextConditionID;
    theCondition->mType = kSMConditionObject;
    theCondition->mObjectIndex = this->mObjects.size() - 1;
    return theObject;
}

// FUNCTION: EXE 0x5b0b65
// Appends a new trigger named <TriggerN> and returns it (its ID is the incremented counter, N + 1)
EESMTrigger* EEScenarioManager::NewTrigger()
{
    UWideString theName;
    sprintf(theName, "<Trigger%d>", this->mNextTriggerID++);
    EESMTrigger* theTrigger = new EESMTrigger;
    theTrigger->mName = theName;
    theTrigger->mID = this->mNextTriggerID;
    this->mTriggers.push_back(theTrigger);
    return theTrigger;
}

// FUNCTION: EXE 0x5b0beb
// Swaps a user area with the next one and renumbers area references (not a built-in area or the last one)
void EEScenarioManager::MoveAreaDown(unsigned long inIndex)
{
    EESMArea* theArea = this->mAreas[inIndex];
    if (inIndex <= 1 || inIndex == this->mAreas.size() - 1)
        return;
    this->mAreas[inIndex] = this->mAreas[inIndex + 1];
    this->mAreas[inIndex + 1] = theArea;
    this->RenumberAreas(inIndex, inIndex + 1);
}

// FUNCTION: EXE 0x5b0c45
// Swaps a user condition with the next one and renumbers references; the last user condition swaps with the
// first object condition
void EEScenarioManager::MoveConditionDown(unsigned long inIndex)
{
    EESMCondition* theCondition = this->mConditions[inIndex];
    unsigned long theCount;
    for (theCount = 0; theCount < this->mConditions.size() && this->mConditions[theCount]->mType != kSMConditionObject; theCount++)
        ;
    if (inIndex <= 0 || inIndex >= theCount || !(inIndex < this->mConditions.size() - 1))
        return;
    this->mConditions[inIndex] = this->mConditions[inIndex + 1];
    this->mConditions[inIndex + 1] = theCondition;
    this->RenumberConditions(inIndex, inIndex + 1);
}

// FUNCTION: EXE 0x5b0cde
// Swaps a user effect with the next one and renumbers effect references (not Do Nothing or the last effect)
void EEScenarioManager::MoveEffectDown(unsigned long inIndex)
{
    EESMEffect* theEffect = this->mEffects[inIndex];
    if (inIndex <= 0 || inIndex == this->mEffects.size() - 1)
        return;
    this->mEffects[inIndex] = this->mEffects[inIndex + 1];
    this->mEffects[inIndex + 1] = theEffect;
    this->RenumberEffects(inIndex, inIndex + 1);
}

// FUNCTION: EXE 0x5b0d37
// Swaps a user object with the next one, moves its Exists condition down one place too and renumbers references
void EEScenarioManager::MoveObjectDown(unsigned long inIndex)
{
    EESMObject* theObject = this->mObjects[inIndex];
    if (inIndex <= 0 || inIndex == this->mObjects.size() - 1)
        return;
    this->mObjects[inIndex] = this->mObjects[inIndex + 1];
    this->mObjects[inIndex + 1] = theObject;
    for (unsigned long i = 1; i < this->mConditions.size() - 1; i++) {
        EESMCondition* theCondition = this->mConditions[i];
        if (theCondition->GetType() != kSMConditionObject || theCondition->GetObjectIndex() != inIndex)
            continue;
        this->mConditions[i] = this->mConditions[i + 1];
        this->mConditions[i + 1] = theCondition;
        this->RenumberConditions(i, i + 1);
        break;
    }
    this->RenumberObjects(inIndex, inIndex + 1);
}

// FUNCTION: EXE 0x5b0e71
// Swaps a trigger with the next one and renumbers trigger references (not the last trigger)
void EEScenarioManager::MoveTriggerDown(unsigned long inIndex)
{
    EESMTrigger* theTrigger = this->mTriggers[inIndex];
    if (inIndex == this->mTriggers.size() - 1)
        return;
    this->mTriggers[inIndex] = this->mTriggers[inIndex + 1];
    this->mTriggers[inIndex + 1] = theTrigger;
    this->RenumberTriggers(inIndex, inIndex + 1);
}

// FUNCTION: EXE 0x5b0ec6
// Deletes a user area and renumbers area references (references to it become Off Map); false for a built-in area
bool EEScenarioManager::DeleteArea(unsigned long inIndex)
{
    if (inIndex >= 2) {
        EESMArea* theArea = this->GetArea(inIndex);
        this->mAreas.erase(this->mAreas.begin() + inIndex);
        delete theArea;
        this->RenumberAreas(inIndex, (unsigned long)-1);
        return true;
    }
    return false;
}

// FUNCTION: EXE 0x5b0f3f
// Deletes a condition (not Always True) and renumbers the triggers' condition references; returns whether it did
bool EEScenarioManager::DeleteCondition(unsigned long inIndex)
{
    bool theResult = false;
    if (inIndex != 0) {
        EESMCondition* theCondition = this->GetCondition(inIndex);
        this->mConditions.erase(this->mConditions.begin() + inIndex);
        delete theCondition;
        this->RenumberConditions(inIndex, (unsigned long)-1);
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x5b0fb5
// Deletes an effect (not Do Nothing) with its timed entries and renumbers effect references; returns whether it did
bool EEScenarioManager::DeleteEffect(unsigned long inIndex)
{
    bool theResult = false;
    if (inIndex != 0) {
        EESMEffect* theEffect = this->GetEffect(inIndex);
        this->RemoveTimedEffects(inIndex);
        this->mEffects.erase(this->mEffects.begin() + inIndex);
        delete theEffect;
        this->RenumberEffects(inIndex, (unsigned long)-1);
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x5b1033
// Deletes a scenario object (not None) and its Exists condition, then renumbers object references; returns whether
// it did
bool EEScenarioManager::DeleteObject(unsigned long inIndex)
{
    bool theResult = true;
    if (inIndex > 0) {
        EESMObject* theObject = this->GetScenarioObject(inIndex);
        this->mObjects.erase(this->mObjects.begin() + inIndex);
        delete theObject;
        for (unsigned long i = 1; i < this->mConditions.size(); i++) {
            EESMCondition* theCondition = this->mConditions[i];
            if (theCondition->mType == kSMConditionObject && theCondition->mObjectIndex == inIndex) {
                this->DeleteCondition(i);
                break;
            }
        }
        this->RenumberObjects(inIndex, (unsigned long)-1);
    } else
        theResult = false;
    return theResult;
}

// FUNCTION: EXE 0x5b10ed
// Deletes a trigger and renumbers trigger references; always returns true
bool EEScenarioManager::DeleteTrigger(unsigned long inIndex)
{
    EESMTrigger* theTrigger = this->GetTrigger(inIndex);
    this->mTriggers.erase(this->mTriggers.begin() + inIndex);
    delete theTrigger;
    this->RenumberTriggers(inIndex, (unsigned long)-1);
    return true;
}

// FUNCTION: EXE 0x5b115a
// Drops every timed entry of the given effect
void EEScenarioManager::RemoveTimedEffects(long inEffect)
{
    for (unsigned long i = 0; i < this->mTimedEffects.size(); i++) {
        if (this->mTimedEffects[i].mEffectIndex == inEffect) {
            this->mTimedEffects.erase(this->mTimedEffects.begin() + i);
            i--;
        }
    }
}

// FUNCTION: EXE 0x5b11d1
// Drops one timed effect entry by position
void EEScenarioManager::EraseTimedEffect(unsigned long inIndex)
{
    this->mTimedEffects.erase(this->mTimedEffects.begin() + inIndex);
}

// FUNCTION: EXE 0x5b120d
// Appends a copy of an area named <name_Copy> with the next area ID and returns it
EESMArea* EEScenarioManager::DuplicateArea(unsigned long inIndex)
{
    UWideString theNewName;
    UWideString theName;
    theName = this->mAreas[inIndex]->GetName();
    if (theName.GetLength() > 2) {
        if (theName[0] == '<') {
            theName = (const char*)_mbsninc(reinterpret_cast<const unsigned char*>((const char*)theName), 1);
            if (theName[(int)theName.GetLength() - 1] == '>')
                theName.CopyString(theName, theName.GetLength() - 1);
        }
    }
    UWideString theCopyOf;
    gResource->GetString(IDS_COPY, theCopyOf);
    sprintf(theNewName, "<%s_%s>", (const char*)theName, (const char*)theCopyOf);
    EESMArea* theArea = new EESMArea;
    theArea->Copy(*this->mAreas[inIndex]);
    theArea->mName = theNewName;
    long theID = ++this->mNextAreaID;   // one new ID per area, as in NewArea
    theArea->mID = theID;
    this->mAreas.push_back(theArea);
    return theArea;
}

// FUNCTION: EXE 0x5b1358
// Inserts a copy of a condition named <name_Copy> before the object conditions, remaps condition references and
// returns it
EESMCondition* EEScenarioManager::DuplicateCondition(unsigned long inIndex)
{
    // LOST-DEBUG: none: exact only (tools lostdebug_search)
    EE_LOST_DEBUG(6);
    std::vector<EESMCondition*> theOldConditions(this->mConditions);
    EESMIDMap theMap;
    UWideString theNewName;
    UWideString theName;
    unsigned long i;
    for (i = 0; i < this->mConditions.size(); i++)
        if (this->mConditions[i]->mType == kSMConditionObject)
            break;
    theName = this->mConditions[inIndex]->mName;
    if (theName.GetLength() > 2) {
        if (theName[0] == '<') {
            theName = (const char*)_mbsninc(reinterpret_cast<const unsigned char*>((const char*)theName), 1);
            if (theName[(int)theName.GetLength() - 1] == '>')
                theName.CopyString(theName, theName.GetLength() - 1);
        }
    }
    UWideString theCopyOf;
    gResource->GetString(IDS_COPY, theCopyOf);
    sprintf(theNewName, "<%s_%s>", (const char*)theName, (const char*)theCopyOf);
    EESMCondition* theCondition = new EESMCondition;
    theCondition->Copy(*this->mConditions[inIndex]);
    theCondition->mName = theNewName;
    theCondition->mID = ++this->mNextConditionID;
    this->mConditions.insert(this->mConditions.begin() + i, theCondition);
    for (unsigned long theNew = 0; theNew < this->mConditions.size(); theNew++) {
        for (unsigned long theOld = 0; theOld < theOldConditions.size(); theOld++) {
            if (theOldConditions[theOld] == this->mConditions[theNew]) {
                theMap.insert(EESMIDMap::value_type(theOld, theNew));
                break;
            }
        }
    }
    this->RemapConditions(theMap);
    return theCondition;
}

// FUNCTION: EXE 0x5b15a7
// Appends a copy of an effect named <name_Copy> with the next effect ID and returns it
EESMEffect* EEScenarioManager::DuplicateEffect(unsigned long inIndex)
{
    UWideString theNewName;
    UWideString theName;
    theName = this->mEffects[inIndex]->mName;
    theName = this->mEffects[inIndex]->mName;
    if (theName.GetLength() > 2) {
        if (theName[0] == '<') {
            theName = (const char*)_mbsninc(reinterpret_cast<const unsigned char*>((const char*)theName), 1);
            if (theName[(int)theName.GetLength() - 1] == '>')
                theName.CopyString(theName, theName.GetLength() - 1);
        }
    }
    UWideString theCopyOf;
    gResource->GetString(IDS_COPY, theCopyOf);
    sprintf(theNewName, "<%s_%s>", (const char*)theName, (const char*)theCopyOf);
    EESMEffect* theEffect = new EESMEffect;
    theEffect->Copy(*this->mEffects[inIndex]);
    theEffect->mName = theNewName;
    theEffect->mID = ++this->mNextEffectID;
    this->mEffects.push_back(theEffect);
    return theEffect;
}

// FUNCTION: EXE 0x5b1719
// Appends a copy of a scenario object named <name_Copy> (name_Copy for an unbracketed name) with its own
// "name Exists" condition; returns the copy
EESMObject* EEScenarioManager::DuplicateObject(unsigned long inIndex)
{
    UWideString theNewName;
    UWideString theName;
    UWideString theStripped;
    theName = this->mObjects[inIndex]->mName;
    theName = this->mObjects[inIndex]->mName;
    UWideString theCopyOf;
    gResource->GetString(IDS_COPY, theCopyOf);
    if (theName.GetLength() > 2 && theName[0] == '<') {
        theStripped = (const char*)_mbsninc(reinterpret_cast<const unsigned char*>((const char*)theName), 1);
        if (theStripped[(int)theStripped.GetLength() - 1] == '>')
            theName.CopyString(theStripped, theStripped.GetLength() - 1);
        sprintf(theNewName, "<%s_%s>", (const char*)theName, (const char*)theCopyOf);
    } else
        sprintf(theNewName, "%s_%s", (const char*)theName, (const char*)theCopyOf);
    EESMObject* theObject = new EESMObject;
    theObject->Copy(*this->mObjects[inIndex]);
    theObject->SetName(theNewName);
    theObject->mL64 = ++this->mNextObjectID;
    this->mObjects.push_back(theObject);
    EESMCondition* theCondition = new EESMCondition;
    this->mConditions.push_back(theCondition);
    UWideString theText;
    gResource->GetString(IDS_EXISTS, theText);
    theCondition->mName = theObject->mName + " " + theText;
    theCondition->mType = kSMConditionObject;
    theCondition->mObjectIndex = this->mObjects.size() - 1;
    theCondition->mID = ++this->mNextConditionID;
    return theObject;
}

// FUNCTION: EXE 0x5b1978
// Appends a copy of a trigger named <name_Copy> with the next trigger ID and returns it
EESMTrigger* EEScenarioManager::DuplicateTrigger(unsigned long inIndex)
{
    UWideString theNewName;
    UWideString theName;
    theName = this->mTriggers[inIndex]->mName;
    theName = this->mTriggers[inIndex]->mName;
    if (theName.GetLength() > 2) {
        if (theName[0] == '<') {
            theName = (const char*)_mbsninc(reinterpret_cast<const unsigned char*>((const char*)theName), 1);
            if (theName[(int)theName.GetLength() - 1] == '>')
                theName.CopyString(theName, theName.GetLength() - 1);
        }
    }
    UWideString theCopyOf;
    gResource->GetString(IDS_COPY, theCopyOf);
    sprintf(theNewName, "<%s_%s>", (const char*)theName, (const char*)theCopyOf);
    EESMTrigger* theTrigger = new EESMTrigger;
    theTrigger->Copy(*this->mTriggers[inIndex]);
    theTrigger->mName = theNewName;
    theTrigger->mID = ++this->mNextTriggerID;
    this->mTriggers.push_back(theTrigger);
    return theTrigger;
}

// FUNCTION: EXE 0x5b1ae4
// Ends a cinematic: delays the other timed effects and trigger timers by its length + 3 s, drops its unrun effects
// and dialog messages, delays the wonder timers and restores the camera saved by StartCinematic
// Called by EEMEndCinematic::Execute 0xbdf7d
// NEAR: the two discarded expressions per pending entry (a compiled-out trace) and the inline budget of begin()
void EEScenarioManager::EndCinematic()
{
    float theElapsed = this->mTime - this->mCinematicStartTime;
    if (!gWorld.mInCinematic || this->mIsEndingCinematic)
        return;
    gSoundManager->SetLatency(100);
    unsigned long i;
    for (i = 0; i < this->mTimedEffects.size(); i++) {
        if (!this->mTimedEffects[i].mIsInCinematic) {
            this->mTimedEffects[i].mTime += (long)theElapsed + 3000;
            // LOST-DEBUG: the original evaluates `begin() + i` and GetEffect(mEffectIndex) and discards both (a trace)


        } else if (!this->mTimedEffects[i].mHasRun) {
            // LOST-DEBUG: GetEffect's result is discarded (a trace)

            this->mTimedEffects[i].mHasRun = true;
        }
    }
    std::stable_sort(this->mTimedEffects.begin(), this->mTimedEffects.end(), EESMTimedEffectLess());
    unsigned long t;
    for (t = 0; t < this->GetNumTriggers(); t++) {
        if (!this->GetTrigger(t)->mIsInCinematic && this->GetTrigger(t)->mConditionDuration > 0)
            this->GetTrigger(t)->mConditionStartTime = this->GetTrigger(t)->mConditionStartTime + (long)theElapsed + 3000;
    }
    gUserInterface->PostHideMessages();
    if (this->mMessages.size() > 0) {
        for (long j = this->mMessages.size() - 1; j >= 0; j--) {
            if (this->mMessages[j].mIsCinematic == true)
                this->mMessages.erase(this->mMessages.begin() + j);
        }
    }
    for (i = 0; i < gWorld.mNumPlayers; i++) {
        if (gWorld.GetPlayer(i) != NULL)
            gWorld.GetPlayer(i)->DelayWonderTimers((long)theElapsed);
    }
    this->mCinematicStartTime = 0.0f;
    gGraphics->PostLetterboxOut(3000);
    this->mIsEndingCinematic = true;
    gWorld.mCameraControl.ResetPath();
    gWorld.mCameraControl.ClearUnitB();
    gWorld.mCameraControl.ClearUnitA();
    gWorld.mCameraControl.mTurnYaw = 0.0f;
    gWorld.mCameraControl.SetMode(this->mSavedCameraMode);
    gWorld.mCameraControl.SetZoom(this->mSavedZoom);
    gWorld.mCameraControl.SetOffset(this->mK008.mOffset, true);
    gWorld.mCameraControl.SetLocation(this->mK008.mFocus, true, false);
    gWorld.mCameraControl.SetViewTurn(this->mK008.mT00, true);
    gWorld.mCameraControl.SetOffsetTurn(this->mK008.mT40, true);
    gWorld.mCameraControl.SetChanged();
    gWorld.mCameraControl.Update(0.0f);
}

// FUNCTION: EXE 0x5b1e47
// Leaves the cinematic once its letterbox is gone (Update): restores the game speed, the main game form, the
// mouse, fades in and stops the one-shot sounds
void EEScenarioManager::ExitCinematic()
{
    gWorld.mInCinematic = false;
    gWorld.SetGameSpeed(gWorld.mSavedGameSpeed);
    gUIManager->mMainGameForm->SetUIState(0);
    gUIManager->mMainGameForm->mIsCursorLocked = false;
    gMouse->mPointer = 0;
    gMouse->SetPosition(GE2DPoint((float)(gGameWindow->GetWidth() / 2), (float)(gGameWindow->GetHeight() / 2)));
    gGraphics->PostFadeIn(0);
    gSoundManager->StopAllOneShotSounds();
}

// FUNCTION: EXE 0x5b1eee
// Starts a cinematic (EESMEffect 0x1bd86c): saves the game speed and camera, closes the main game form, locks the
// cursor, unselects every player's units, drops the non-cinematic dialog messages and letterboxes the screen
void EEScenarioManager::StartCinematic()
{
    if (gWorld.mInCinematic)
        return;
    gSoundManager->SetLatency(300);
    this->mCinematicStartTime = this->mTime;
    gWorld.mInCinematic = true;
    gWorld.mSavedGameSpeed = gWorld.GetGameSpeed();
    gUserInterface->CloseForm(gUIManager->mMainGameForm);
    gSoundManager->StopAllOneShotSounds();
    gWorld.mCameraControl.GetOffset(gWorld.mCameraControl.mFraction, this->mK008.mOffset);
    gWorld.mCameraControl.GetLocation(gWorld.mCameraControl.mFraction, this->mK008.mFocus);
    gWorld.mCameraControl.GetViewTurn(gWorld.mCameraControl.mFraction, this->mK008.mT00);
    gWorld.mCameraControl.GetOffsetTurn(gWorld.mCameraControl.mFraction, this->mK008.mT40);
    this->mSavedCameraMode = gWorld.mCameraControl.mMode;
    this->mSavedZoom = gWorld.mCameraControl.GetZoom();
    gWorld.mCameraControl.ResetPath();
    gWorld.mCameraControl.ClearUnitB();
    gWorld.mCameraControl.ClearUnitA();
    gUIManager->mMainGameForm->SetUIState(0x11);
    gUIManager->mMainGameForm->mIsCursorLocked = true;
    gMouse->mPointer = 6;
    for (unsigned long i = 1; i < gWorld.mNumPlayers; i++) {
        gWorld.GetPlayer(i)->UnselectAllUnits();
        gWorld.GetPlayer(i)->mSelectedGroup->CopySelection(gWorld.GetPlayer(i)->mActiveGroup);
    }
    gUserInterface->PostHideMessages();
    if (this->mMessages.size() > 0) {
        for (long j = this->mMessages.size() - 1; j >= 0; j--) {
            if (!this->mMessages[j].mIsCinematic)
                this->mMessages.erase(this->mMessages.begin() + j);
        }
    }
    gGraphics->PostLetterboxIn(3000);
}

// FUNCTION: EXE 0x5b20e6
// Runs a scenario tick (EEPhysics 0x153265): shows queued dialog, advances the clocks (tick / game speed in a
// cinematic); while a human plays, every 500 ms evaluates the triggers and runs the due timed effects
void EEScenarioManager::Update()
{
    float theSpeed = gWorld.mGameSpeed;
    bool theHumanPlaying = false;
    float theTick = gPhysics->GetTickSeconds() * 1000.0f;
    if (this->mMessages.size() > 0 && !this->mIsShowingDialog)
        this->ShowDialogMessage();
    for (long i = 0; i < (long)gWorld.mNumPlayers; i++) {
        if (gWorld.GetPlayer(i) != NULL && gWorld.GetPlayer(i)->IsHuman() && !gWorld.GetPlayer(i)->mObserverMode)
            theHumanPlaying = true;
    }
    if (gWorld.mInCinematic)
        theTick /= theSpeed;
    this->mTime += theTick;
    this->mTriggerTimer += theTick;
    if (theHumanPlaying) {
        if (this->mIsMovingCamera && this->mTriggerTimer >= kSMZero)
            this->MoveCameraToStart();
        if (this->mTriggerTimer >= kTriggerInterval) {
            long theCount = this->mTriggers.size();
            for (long t = 0; t < theCount; t++)
                this->GetTrigger(t)->Evaluate(this->mTime);
            for (int e = 0; e < (int)this->mTimedEffects.size(); e++) {
                if (!((this->mTimedEffects.begin() + e)->mTime <= this->mTime))
                    continue;
                EESMEffect* theEffect = this->GetTimedEffect(e);
                if (theEffect != NULL && !this->mTimedEffects[e].mHasRun
                    && (!gWorld.mInCinematic || this->mTimedEffects[e].mIsInCinematic)) {
                    (this->mTimedEffects.begin() + e)->mHasRun = true;
                    theEffect->Execute((this->mTimedEffects.begin() + e)->mIsInCinematic);
                }
            }
            for (int k = (int)this->mTimedEffects.size() - 1; k >= 0; k--) {
                if ((this->mTimedEffects.begin() + k)->mHasRun) {
                    // LOST-DEBUG: the original computes mEffects.begin() + mEffectIndex and discards it (a trace argument)

                    this->EraseTimedEffect(k);
                }
            }
            this->mTriggerTimer = 0.0f;
            this->mLastChatText.Clear();
        }
    }
    if (this->mIsEndingCinematic && gF4BE374 == 1.0f) {
        this->mIsEndingCinematic = false;
        this->ExitCinematic();
    }
}

// FUNCTION: EXE 0x5b23dc
// Writes the trigger text file (EESEIAExport): the media strings, then the effect, condition and player name
// entries; 9 on a write error
// Called by EESEIAExport 0x16ffd0
long EEScenarioManager::ExportText(FSFileSpec& inFile)
{
    UWideString theText;
    long theResult = this->mMediaStrings.Export(inFile);
    if (theResult == 0) {
        unsigned long i;
        for (i = 0; i < this->mEffects.size() && theResult == 0; i++) {
            EESMEffect* theEffect = this->mEffects[i];
            if (theEffect == NULL)
                continue;
            theText = "";
            if (theEffect->GetType() == kSMEffectMedia) {
                switch (theEffect->mMediaAction) {
                case kSMMediaActionChangeText:
                    theText = theEffect->mDescription;
                    break;
                case kSMMediaActionSendDialogue:
                case kSMMediaActionSendChatMessage:
                    theText = theEffect->mMessage;
                    break;
                }
            } else if (theEffect->GetType() == kSMEffectObject && theEffect->mObjectAction == kSMObjectActionClassName) {
                theText = theEffect->mNewName;
            }
            if (!(theText.GetLength() > 0))
                continue;
            if (fprintf(inFile, "{%d_Effect:%s}\n", theEffect->GetID(), (const char*)theEffect->GetName()) < 0
                || fprintf(inFile, "%s\n\n", (const char*)theText) < 0)
                theResult = 9;
        }
        for (i = 0; i < this->mConditions.size() && theResult == 0; i++) {
            EESMCondition* theCondition = this->mConditions[i];
            if (theCondition == NULL)
                continue;
            theText = "";
            if (theCondition->GetType() == kSMConditionChatMessageContains)

                theText = theCondition->mChatText;
            if (!(theText.GetLength() > 0))
                continue;
            if (fprintf(inFile, "{%d_Condition:%s}\n", theCondition->GetID(),
                        (const char*)theCondition->GetName()) < 0
                || fprintf(inFile, "%s\n\n", (const char*)theText) < 0)
                theResult = 9;
        }
        for (unsigned long p = 1; p < gWorld.mNumPlayers; p++) {
            if (gWorld.GetPlayer(p) != NULL) {
                if (fprintf(inFile, "{%d_Player Name:%s}\n", p, (const char*)gWorld.GetPlayer(p)->mName) < 0)
                    theResult = 9;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5b260a
// Resolves a scenario player choice (0 nature, 1/3 a random/every human, 2/4 a random/every computer, 5/6 a random
// player with/without nature, 7+ player n-6) into a list; returns the chosen player, -1 for none or a group
long EEScenarioManager::ChoosePlayer(long inType, std::vector<long>* outPlayers, bool inSingle)
{
    unsigned long theNumPlayers = gWorld.mNumPlayers;
    int theResult = -1;
    std::vector<long> thePlayers;
    long i;
    switch (inType) {
    case 0:
        theResult = 0;
        thePlayers.push_back(theResult);
        break;
    case 1:
    case 3:
        for (i = 1; i < theNumPlayers; i++)
            if (gWorld.GetPlayer(i)->mHumanPlayer != NULL)
                thePlayers.push_back(i);
        if (thePlayers.size() != 0 && inType == 1) {
            EEAIRandomShuffle(thePlayers.begin(), thePlayers.end());
            theResult = thePlayers[0];
            if (inSingle) {
                thePlayers.clear();
                thePlayers.push_back(theResult);
            }
        }
        break;
    case 2:
    case 4:
        for (i = 1; i < theNumPlayers; i++)
            if (gWorld.GetPlayer(i)->mComputerPlayer != NULL)
                thePlayers.push_back(i);
        if (thePlayers.size() != 0 && inType == 2) {
            EEAIRandomShuffle(thePlayers.begin(), thePlayers.end());
            theResult = thePlayers[0];
            if (inSingle) {
                thePlayers.clear();
                thePlayers.push_back(theResult);
            }
        }
        break;
    case 5:
        for (i = 0; i < theNumPlayers; i++)
            thePlayers.push_back(i);
        theResult = gPhysics->GetRandom(0L, (long)(theNumPlayers - 1));
        if (inSingle) {
            thePlayers.clear();
            thePlayers.push_back(theResult);
        }
        break;
    case 6:
        for (i = 1; i < theNumPlayers; i++)
            thePlayers.push_back(i);
        theResult = gPhysics->GetRandom(1L, (long)(theNumPlayers - 1));
        if (inSingle) {
            thePlayers.clear();
            thePlayers.push_back(theResult);
        }
        break;
    default:
        theResult = inType - 6;
        if (theResult < (long)theNumPlayers)
            thePlayers.push_back(theResult);
        else
            theResult = -1;
        break;
    }
    if (outPlayers != NULL)
        *outPlayers = thePlayers;
    return theResult;
}

// FUNCTION: EXE 0x5b286d
// Replays every pending effect of a loaded game, makes the triggers evaluate on the next tick and starts recording
// new pending effects
void EEScenarioManager::ApplyPendingEffects()
{
    this->mTriggerTimer = kTriggerInterval;
    unsigned long theCount = this->mPendingEffects.size();
    for (unsigned long i = 0; i < theCount; i++)
        if (this->mPendingEffects[i] != NULL)
            this->mPendingEffects[i]->Execute(0);
    this->mB234 = false;
}

// FUNCTION: EXE 0x5b28c0
// Unloads the unit class media loaded before, then preloads the media of each unit class a Create Object effect
// makes for one fixed player (objects not selected on the map)
void EEScenarioManager::Prepare()
{
    std::vector<long> thePlayers;
    unsigned long i;
    if (this->mUnitClassMedia.size() != 0) {
        for (i = 0; i < this->mUnitClassMedia.size(); i++) {
            EEPlayer* thePlayer = gWorld.GetPlayer(this->mUnitClassMedia[i].mPlayer);
            EEUnitClassUpgrade* theUpgrade = thePlayer->mUnitClassUpgrades[this->mUnitClassMedia[i].mUnitClass];
            theUpgrade->UnloadMedia(thePlayer);
            EEDebugTrace("Scenario:: Unloading Media for %S for player %d", theUpgrade->GetUnitClass()->GetName(),
                         this->mUnitClassMedia[i].mPlayer);
        }
    }
    this->mUnitClassMedia.clear();
    for (unsigned long theTrigger = 0; theTrigger < this->mTriggers.size(); theTrigger++) {
        EESMTrigger* theSMTrigger = this->mTriggers[theTrigger];
        for (unsigned long theWrapper = 0; theWrapper < theSMTrigger->mEffects.size(); theWrapper++) {
            EESMEffect* theEffect = this->mEffects[theSMTrigger->GetEffect(theWrapper)->mEffectIndex];
            if (theEffect->mType != kSMEffectObject || theEffect->mObjectAction != kSMObjectActionCreate)
                continue;
            EESMObject* theObject = this->mObjects[theEffect->mObject];
            if (theObject->mIsSelectOnMap)
                continue;
            this->ChoosePlayer(theObject->mOwner, &thePlayers, false);
            if (thePlayers.size() == 1) {
                long thePlayerNumber = thePlayers[0];
                long theUnitClass = theObject->mClass;
                if (theUnitClass != 0 && gUnitClassTable[theUnitClass] != NULL) {
                    EEPlayer* thePlayer = gWorld.GetPlayer(thePlayerNumber);
                    thePlayer->mUnitClassUpgrades[theUnitClass]->LoadMedia(thePlayer, 0x7f);
                    EESMUnitClassMedia theMedia;
                    theMedia.mPlayer = thePlayerNumber;
                    theMedia.mUnitClass = theUnitClass;
                    this->mUnitClassMedia.push_back(theMedia);
                }
            }
        }
    }
}

// FUNCTION: EXE 0x5b2b78
// Passes a trigger index change (move or delete) to every condition, effect and pending effect
void EEScenarioManager::RenumberTriggers(unsigned long inOld, unsigned long inNew)
{
    unsigned long theCount = this->mConditions.size();
    unsigned long i;
    for (i = 0; i < theCount; i++)
        this->mConditions[i]->RenumberTriggers(inOld, inNew);
    theCount = this->mEffects.size();
    for (i = 0; i < theCount; i++)
        this->mEffects[i]->RenumberTriggers(inOld, inNew);
    theCount = this->mPendingEffects.size();
    for (i = 0; i < theCount; i++)
        this->mPendingEffects[i]->RenumberTriggers(inOld, inNew);
}

// FUNCTION: EXE 0x5b2c2f
// Applies an old-to-new trigger index map to every condition, effect and pending effect
void EEScenarioManager::RemapTriggers(EESMIDMap& inMap)
{
    unsigned long theCount = this->mConditions.size();
    unsigned long i;
    for (i = 0; i < theCount; i++)
        this->mConditions[i]->RemapTriggers(inMap);
    theCount = this->mEffects.size();
    for (i = 0; i < theCount; i++)
        this->mEffects[i]->RemapTriggers(inMap);
    theCount = this->mPendingEffects.size();
    for (i = 0; i < theCount; i++)
        this->mPendingEffects[i]->RemapTriggers(inMap);
}

// FUNCTION: EXE 0x5b2cdc
// Passes a condition index change (move or delete) to every trigger
void EEScenarioManager::RenumberConditions(unsigned long inOld, unsigned long inNew)
{
    unsigned long theCount = this->mTriggers.size();
    for (unsigned long i = 0; i < theCount; i++)
        this->mTriggers[i]->RenumberConditions(inOld, inNew);
}

// FUNCTION: EXE 0x5b2d21
// Applies an old-to-new condition index map to every trigger
void EEScenarioManager::RemapConditions(EESMIDMap& inMap)
{
    unsigned long theCount = this->mTriggers.size();
    for (unsigned long i = 0; i < theCount; i++)
        this->mTriggers[i]->RemapConditions(inMap);
}

// FUNCTION: EXE 0x5b2d62
// Passes an effect index change to every trigger; timed entries of the old index follow it (or go on a delete),
// but entries of the swapped or later effects are not renumbered
void EEScenarioManager::RenumberEffects(unsigned long inOld, unsigned long inNew)
{
    unsigned long theCount = this->mTriggers.size();
    unsigned long i;
    for (i = 0; i < theCount; i++)
        this->mTriggers[i]->RenumberEffects(inOld, inNew);
    // Bounded by the current size: after an erase the cached count read a stale slot past the end.
    for (i = 0; i < this->mTimedEffects.size(); i++) {
        if (this->mTimedEffects[i].mEffectIndex != inOld)
            continue;
        if (inNew == (unsigned long)-1) {
            this->mTimedEffects.erase(this->mTimedEffects.begin() + i);
            i--;
        } else
            this->mTimedEffects[i].mEffectIndex = inNew;
    }
}

// FUNCTION: EXE 0x5b2e33
// Applies an old-to-new effect index map to every trigger and timed entry (unmapped entries become Do Nothing)
void EEScenarioManager::RemapEffects(EESMIDMap& inMap)
{
    unsigned long theCount = this->mTriggers.size();
    unsigned long i;
    for (i = 0; i < theCount; i++)
        this->mTriggers[i]->RemapEffects(inMap);
    theCount = this->mTimedEffects.size();
    for (i = 0; i < theCount; i++)
        EERemapID(this->mTimedEffects[i].mEffectIndex, inMap);
}

// FUNCTION: EXE 0x5b2eb2
// Passes an object index change (move or delete) to every condition, effect, object and pending effect
void EEScenarioManager::RenumberObjects(unsigned long inOld, unsigned long inNew)
{
    unsigned long theCount = this->mConditions.size();
    unsigned long i;
    for (i = 0; i < theCount; i++)
        this->mConditions[i]->RenumberObjects(inOld, inNew);
    theCount = this->mEffects.size();
    for (i = 0; i < theCount; i++)
        this->mEffects[i]->RenumberObjects(inOld, inNew);
    theCount = this->mObjects.size();
    for (i = 0; i < theCount; i++)
        this->mObjects[i]->RenumberObjects(inOld, inNew);
    theCount = this->mPendingEffects.size();
    for (i = 0; i < theCount; i++)
        this->mPendingEffects[i]->RenumberObjects(inOld, inNew);
}

// FUNCTION: EXE 0x5b2fa1
// Applies an old-to-new object index map to every condition, effect, object and pending effect
void EEScenarioManager::RemapObjects(EESMIDMap& inMap)
{
    unsigned long theCount = this->mConditions.size();
    unsigned long i;
    for (i = 0; i < theCount; i++)
        this->mConditions[i]->RemapObjects(inMap);
    theCount = this->mEffects.size();
    for (i = 0; i < theCount; i++)
        this->mEffects[i]->RemapObjects(inMap);
    theCount = this->mObjects.size();
    for (i = 0; i < theCount; i++)
        this->mObjects[i]->RemapObjects(inMap);
    theCount = this->mPendingEffects.size();
    for (i = 0; i < theCount; i++)
        this->mPendingEffects[i]->RemapObjects(inMap);
}

// FUNCTION: EXE 0x5b3084
// Passes an area index change (move or delete) to every effect, object and pending effect
void EEScenarioManager::RenumberAreas(unsigned long inOld, unsigned long inNew)
{
    unsigned long theCount = this->mEffects.size();
    unsigned long i;
    for (i = 0; i < theCount; i++)
        this->mEffects[i]->RenumberAreas(inOld, inNew);
    theCount = this->mObjects.size();
    for (i = 0; i < theCount; i++)
        this->mObjects[i]->RenumberAreas(inOld, inNew);
    theCount = this->mPendingEffects.size();
    for (i = 0; i < theCount; i++)
        this->mPendingEffects[i]->RenumberAreas(inOld, inNew);
}

// FUNCTION: EXE 0x5b313b
// Applies an old-to-new area index map to every effect, object and pending effect
void EEScenarioManager::RemapAreas(EESMIDMap& inMap)
{
    unsigned long theCount = this->mEffects.size();
    unsigned long i;
    for (i = 0; i < theCount; i++)
        this->mEffects[i]->RemapAreas(inMap);
    theCount = this->mObjects.size();
    for (i = 0; i < theCount; i++)
        this->mObjects[i]->RemapAreas(inMap);
    theCount = this->mPendingEffects.size();
    for (i = 0; i < theCount; i++)
        this->mPendingEffects[i]->RemapAreas(inMap);
}

// FUNCTION: EXE 0x5b31e8
// Replaces a unit ID with another in every scenario object
void EEScenarioManager::ReplaceUnit(long inOldUnit, long inNewUnit)
{
    unsigned long theCount = this->mObjects.size();
    for (unsigned long i = 0; i < theCount; i++)
        this->mObjects[i]->ReplaceUnit(inOldUnit, inNewUnit);
}

// FUNCTION: EXE 0x5b322d
// Returns the condition with the given ID, or NULL
EESMCondition* EEScenarioManager::FindConditionByID(long inID)
{
    EESMCondition* theResult = NULL;
    const std::vector<EESMCondition*>& theConditions = this->mConditions;
    for (unsigned long i = 0; i < this->mConditions.size(); i++)
        if (theConditions[i]->mID == inID) { theResult = theConditions[i]; break; }

    return theResult;
}

// FUNCTION: EXE 0x5b3269
// Returns the effect with the given ID, or NULL
EESMEffect* EEScenarioManager::FindEffectByID(long inID)
{
    EESMEffect* theResult = NULL;
    const std::vector<EESMEffect*>& theEffects = this->mEffects;
    for (unsigned long i = 0; i < this->mEffects.size(); i++)
        if (theEffects[i]->mID == inID) { theResult = theEffects[i]; break; }

    return theResult;
}

// FUNCTION: EXE 0x5b32a8
// Returns the scenario object with the given object ID, or NULL
EESMObject* EEScenarioManager::FindObjectByID(long inID)
{
    EESMObject* theResult = NULL;
    const std::vector<EESMObject*>& theObjects = this->mObjects;
    for (unsigned long i = 0; i < this->mObjects.size(); i++)
        if (theObjects[i]->mL64 == inID) { theResult = theObjects[i]; break; }

    return theResult;
}

// FUNCTION: EXE 0x5b32e4
// Returns a scenario object's index in the object list, or -1
long EEScenarioManager::GetObjectIndex(EESMObject* inObject)
{
    for (unsigned long i = 0, theCount = this->mObjects.size(); i < theCount; i++)
        if (this->mObjects[i] == inObject)
            return i;
    return -1;
}

// FUNCTION: EXE 0x5b3317
// Appends the user areas' names to a list ("[id]" prefixed while debug IDs are shown)
void EEScenarioManager::GetUserAreaNames(std::vector<UWideString>& outNames)
{
    UWideString theName;
    UString theID;
    unsigned long theCount = this->mAreas.size();
    for (unsigned long i = 2; i < theCount; i++) {
        if (this->mShowDebugIDs)
            sprintf(theID, "[%d]", this->mAreas[i]->mID);
        theName = (const char*)(theID + (const char*)this->mAreas[i]->mName);
        outNames.push_back(theName);
    }
}

// FUNCTION: EXE 0x5b33d7
// Appends every area's name, the built-in ones included ("[id]" prefixed while debug IDs are shown)
void EEScenarioManager::GetAreaNames(std::vector<UWideString>& outNames)
{
    UWideString theName;
    UString theID;
    unsigned long theCount = this->mAreas.size();
    for (unsigned long i = 0; i < theCount; i++) {
        if (this->mShowDebugIDs)
            sprintf(theID, "[%d]", this->mAreas[i]->mID);
        theName = (const char*)(theID + (const char*)this->mAreas[i]->mName);
        outNames.push_back(theName);
    }
}

// FUNCTION: EXE 0x5b3496
// Appends every condition's name, first renaming each object condition "<object name Exists>" (English text)
void EEScenarioManager::GetAllConditionNames(std::vector<UWideString>& outNames)
{
    UWideString theName;
    UString theID;
    unsigned long theCount = this->mConditions.size();
    for (unsigned long i = 0; i < theCount; i++) {
        if (this->mShowDebugIDs)
            sprintf(theID, "[%d]", this->mConditions[i]->mID);
        if (this->mConditions[i]->GetType() == kSMConditionObject) {
            UWideString thePrefix("<");
            this->mConditions[i]->mName = thePrefix + this->GetScenarioObject(this->mConditions[i]->GetObjectIndex())->mName + " Exists>";
        }
        theName = (const char*)(theID + (const char*)this->mConditions[i]->GetName());
        outNames.push_back(theName);
    }
}

// FUNCTION: EXE 0x5b3656
// Appends the user conditions' names, without Always True and the object conditions ("[id]" prefixed while
// debug IDs are shown)
void EEScenarioManager::GetUserConditionNames(std::vector<UWideString>& outNames)
{
    UWideString theName;
    UString theID;
    unsigned long theCount = this->mConditions.size();
    for (unsigned long i = 1; i < theCount; i++) {
        EESMCondition* theCondition = this->mConditions[i];
        if (theCondition->mType == kSMConditionObject)
            continue;
        if (this->mShowDebugIDs)
            sprintf(theID, "[%d]", theCondition->mID);
        theName = (const char*)(theID + (const char*)this->mConditions[i]->mName);
        outNames.push_back(theName);
    }
}

// FUNCTION: EXE 0x5b3726
// Appends the user effects' names, without Do Nothing ("[id]" prefixed while debug IDs are shown)
void EEScenarioManager::GetUserEffectNames(std::vector<UWideString>& outNames)
{
    UWideString theName;
    UString theID;
    unsigned long theCount = this->mEffects.size();
    for (unsigned long i = 1; i < theCount; i++) {
        if (this->mShowDebugIDs)
            sprintf(theID, "[%d]", this->mEffects[i]->mID);
        theName = (const char*)(theID + (const char*)this->mEffects[i]->mName);
        outNames.push_back(theName);
    }
}

// FUNCTION: EXE 0x5b37e9
// Appends every effect's name, Do Nothing included ("[id]" prefixed while debug IDs are shown)
void EEScenarioManager::GetAllEffectNames(std::vector<UWideString>& outNames)
{
    UWideString theName;
    UString theID;
    unsigned long theCount = this->mEffects.size();
    for (unsigned long i = 0; i < theCount; i++) {
        if (this->mShowDebugIDs)
            sprintf(theID, "[%d]", this->mEffects[i]->mID);
        theName = (const char*)(theID + (const char*)this->mEffects[i]->mName);
        outNames.push_back(theName);
    }
}

// FUNCTION: EXE 0x5b38ab
// Appends the user scenario objects' names, without None ("[id]" prefixed while debug IDs are shown)
void EEScenarioManager::GetUserObjectNames(std::vector<UWideString>& outNames)
{
    UWideString theName;
    UString theID;
    unsigned long theCount = this->mObjects.size();
    for (unsigned long i = 1; i < theCount; i++) {
        if (this->mShowDebugIDs)
            sprintf(theID, "[%d]", this->mObjects[i]->mL64);
        theName = (const char*)(theID + (const char*)this->mObjects[i]->mName);
        outNames.push_back(theName);
    }
}

// FUNCTION: EXE 0x5b396b
// Appends every scenario object's name, None included ("[id]" prefixed while debug IDs are shown)
void EEScenarioManager::GetObjectNames(std::vector<UWideString>& outNames)
{
    UWideString theName;
    UString theID;
    unsigned long theCount = this->mObjects.size();
    for (unsigned long i = 0; i < theCount; i++) {
        if (this->mShowDebugIDs)
            sprintf(theID, "[%d]", this->mObjects[i]->mL64);
        theName = (const char*)(theID + (const char*)this->mObjects[i]->mName);
        outNames.push_back(theName);
    }
}

// FUNCTION: EXE 0x5b3a2a
// Appends every trigger's name ("[id]" prefixed while debug IDs are shown)
void EEScenarioManager::GetTriggerNames(std::vector<UWideString>& outNames)
{
    UWideString theName;
    UString theID;
    unsigned long theCount = this->mTriggers.size();
    for (unsigned long i = 0; i < theCount; i++) {
        if (this->mShowDebugIDs)
            sprintf(theID, "[%d]", this->mTriggers[i]->mID);
        theName = (const char*)(theID + (const char*)this->mTriggers[i]->mName);
        outNames.push_back(theName);
    }
}

// FUNCTION: EXE 0x5b3ae9
// Reads a trigger text file (EESEIAImport) of "{id_Kind:name}" lines, each followed by its text; with inValidate
// checks the entries first (ValidateText), then applies them (ImportText); 9 on a malformed file
// Called by EESEIAImport 0x170439
long EEScenarioManager::ImportTriggerText(FSFileSpec& inFile, unsigned long inKind, bool inValidate)
{
    EESMMediaStrings theMediaStrings;
    std::vector<UString> theTokens;
    UWideString theText;
    std::vector<UString> theLines;
    std::vector<UWideString> theTexts;
    UString theLine;
    UString theSeparators("{}");
    char theBuffer[0x200];
    long theResult = 0;
    bool theInEntry = false;
    while (!feof((FILE*)inFile) && theResult == 0) {
        char* theRead = fgets(theBuffer, 0x200, inFile);
        if (theRead != NULL) {
            theLine = theRead;
            theLine.Tokenize(theSeparators, theTokens);
            if (theTokens.size() == 2) {
                if (theInEntry == true) {
                    if (theText.GetLength() > 0 && theText[(int)(theText.GetLength() - 1)] == '\n') {
                        if (theText.GetLength() > 1)
                            theText = theText.GetIndexedSubstring(0, theText.GetLength() - 2);
                        else
                            theText = "";
                    }
                    if (theText.GetLength() > 0 && theText[(int)(theText.GetLength() - 1)] == '\n') {
                        if (theText.GetLength() > 1)
                            theText = theText.GetIndexedSubstring(0, theText.GetLength() - 2);
                        else
                            theText = "";
                    }
                    theTexts.push_back(theText);
                    theResult = theTexts.size() != theLines.size() ? 9 : 0;
                }
                theLines.push_back(theTokens[0]);
                theText = "";
                theInEntry = true;
            } else if (theTokens.size() <= 2) {
                if (theInEntry == true)
                    theText += theRead;
            }
        } else {
            if (theInEntry == true) {
                if (theText.GetLength() > 0 && theText[(int)(theText.GetLength() - 1)] == '\n') {
                    if (theText.GetLength() > 1)
                        theText = theText.GetIndexedSubstring(0, theText.GetLength() - 2);
                    else
                        theText = "";
                }
                if (theText.GetLength() > 0 && theText[(int)(theText.GetLength() - 1)] == '\n') {
                    if (theText.GetLength() > 1)
                        theText = theText.GetIndexedSubstring(0, theText.GetLength() - 2);
                    else
                        theText = "";
                }
                theTexts.push_back(theText);
            }
            theResult = theTexts.size() != theLines.size() ? 9 : 0;
        }
    }
    if (theResult == 0) {
        if (inValidate == true)
            theResult = this->ValidateText(theLines, inKind, theTexts);
        if (theResult == 0)
            theResult = this->ImportText(theLines, inKind, theTexts);
    }
    return theResult;
}

// FUNCTION: EXE 0x5b3e75
// Swaps a user area with the previous one and renumbers area references (not the first user area)
void EEScenarioManager::MoveAreaUp(unsigned long inIndex)
{
    EESMArea* theElement = this->mAreas[inIndex];
    if (inIndex <= 2)
        return;
    this->mAreas[inIndex] = this->mAreas[inIndex - 1];
    this->mAreas[inIndex - 1] = theElement;
    this->RenumberAreas(inIndex, inIndex - 1);
}

// FUNCTION: EXE 0x5b3ea9
// Swaps a condition with the previous one and renumbers condition references (not the first user condition)
void EEScenarioManager::MoveConditionUp(unsigned long inIndex)
{
    EESMCondition* theElement = this->mConditions[inIndex];
    if (inIndex <= 1)
        return;
    this->mConditions[inIndex] = this->mConditions[inIndex - 1];
    this->mConditions[inIndex - 1] = theElement;
    this->RenumberConditions(inIndex, inIndex - 1);
}

// FUNCTION: EXE 0x5b3edd
// Swaps an effect with the previous one and renumbers effect references (not the first user effect)
void EEScenarioManager::MoveEffectUp(unsigned long inIndex)
{
    EESMEffect* theElement = this->mEffects[inIndex];
    if (inIndex <= 1)
        return;
    this->mEffects[inIndex] = this->mEffects[inIndex - 1];
    this->mEffects[inIndex - 1] = theElement;
    this->RenumberEffects(inIndex, inIndex - 1);
}

// FUNCTION: EXE 0x5b3f11
// Swaps a user object with the previous one, moves its Exists condition up one place too and renumbers references
void EEScenarioManager::MoveObjectUp(unsigned long inIndex)
{
    EESMObject* theObject = this->mObjects[inIndex];
    if (inIndex <= 1)
        return;
    this->mObjects[inIndex] = this->mObjects[inIndex - 1];
    this->mObjects[inIndex - 1] = theObject;
    for (unsigned long i = 1; i < this->mConditions.size(); i++) {
        EESMCondition* theCondition = this->mConditions[i];
        if (theCondition->GetType() != kSMConditionObject || theCondition->GetObjectIndex() != inIndex)
            continue;
        this->mConditions[i] = this->mConditions[i - 1];
        this->mConditions[i - 1] = theCondition;
        this->RenumberConditions(i, i - 1);
        break;
    }
    this->RenumberObjects(inIndex, inIndex - 1);
}

// FUNCTION: EXE 0x5b4017
// Swaps a trigger with the previous one and renumbers trigger references (not the first trigger)
void EEScenarioManager::MoveTriggerUp(unsigned long inIndex)
{
    EESMTrigger* theElement = this->mTriggers[inIndex];
    if (inIndex == 0)
        return;
    this->mTriggers[inIndex] = this->mTriggers[inIndex - 1];
    this->mTriggers[inIndex - 1] = theElement;
    this->RenumberTriggers(inIndex, inIndex - 1);
}

// FUNCTION: EXE 0x5b404a
// Resets, then reads triggers, conditions, effects, objects, areas, timed and pending effects (dropped when loading
// a scenario), ID counters, clocks, media strings, start camera and dialog messages; returns the first error
long EEScenarioManager::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    std::vector<long> theUnused1;
    std::vector<long> theUnused2;
    unsigned long theCount;
    unsigned long i;
    bool theIsInCinematic = false;
    unsigned long theEffect = 0;
    unsigned long theTime = 0;
    EESMTrigger* theTrigger;
    EESMCondition* theCondition;
    EESMEffect* theSMEffect;
    EESMObject* theObject;
    EESMArea* theArea;
    this->Reset();
    long theResult = inFile.Read(theCount);
    if (theResult == kErrorOK) {
        for (i = 0; i < theCount; i++) {
            theTrigger = new EESMTrigger;
            theResult = theTrigger->Load(inMajorVersion, inMinorVersion, inFile);
            if (theResult != kErrorOK) {
                delete theTrigger;
                break;
            }
            this->mTriggers.push_back(theTrigger);
        }
        if (theResult == kErrorOK)
            theResult = inFile.Read(theCount);
        if (theResult == kErrorOK) {
            for (i = 0; i < theCount; i++) {
                theCondition = new EESMCondition;
                theResult = theCondition->Load(inMajorVersion, inMinorVersion, inFile);
                if (theResult != kErrorOK) {
                    delete theCondition;
                    break;
                }
                if (theCondition->mType != kSMConditionObject || theCondition->mObjectIndex != 0)
                    this->mConditions.push_back(theCondition);
                else
                    delete theCondition;    // a dropped "object 0" condition is freed (it leaked)
            }
        }
        if (theResult == kErrorOK)
            theResult = inFile.Read(theCount);
        if (theResult == kErrorOK) {
            for (i = 0; i < theCount; i++) {
                theSMEffect = new EESMEffect;
                theResult = theSMEffect->Load(inMajorVersion, inMinorVersion, inFile);
                if (theResult != kErrorOK) {
                    delete theSMEffect;
                    break;
                }
                this->mEffects.push_back(theSMEffect);
            }
        }
        if (theResult == kErrorOK)
            theResult = inFile.Read(theCount);
        if (theResult == kErrorOK) {
            for (i = 0; i < theCount; i++) {
                theObject = new EESMObject;
                theResult = theObject->Load(inMajorVersion, inMinorVersion, inFile);
                if (theResult != kErrorOK) {
                    delete theObject;
                    break;
                }
                this->mObjects.push_back(theObject);
            }
        }
        if (theResult == kErrorOK)
            theResult = inFile.Read(theCount);
        if (theResult == kErrorOK) {
            for (i = 0; i < theCount; i++) {
                theArea = new EESMArea;
                theResult = theArea->Load(inMajorVersion, inMinorVersion, inFile);
                if (theResult != kErrorOK) {
                    delete theArea;
                    break;
                }
                this->mAreas.push_back(theArea);
            }
        }
        if (theResult == kErrorOK)
            theResult = inFile.Read(theCount);
        if (theResult == kErrorOK) {
            for (i = 0; i < theCount; i++) {
                theEffect = 0;
                theTime = 0;
                theIsInCinematic = false;
                theResult = inFile.Read(theEffect);
                if (theResult != kErrorOK)
                    return theResult;
                theResult = inFile.Read(theTime);
                if (theResult != kErrorOK)
                    return theResult;
                theResult = inFile.Read(theIsInCinematic);
                if (theResult != kErrorOK)
                    return theResult;
                this->mTimedEffects.push_back(EESMTimedEffect(theEffect, theTime, theIsInCinematic));
            }
        }
        if (theResult == kErrorOK)
            theResult = inFile.Read(theCount);
        if (theResult == kErrorOK) {
            for (i = 0; i < theCount; i++) {
                theSMEffect = new EESMEffect;
                theResult = theSMEffect->Load(inMajorVersion, inMinorVersion, inFile);
                if (theResult != kErrorOK) {
                    delete theSMEffect;
                    break;
                }
                if (!Fn14A09E())
                    this->mPendingEffects.push_back(theSMEffect);
                else
                    delete theSMEffect;
            }
        }
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNextTriggerID);
    }
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mNextConditionID);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mNextEffectID);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mNextObjectID);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mNextAreaID);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mTime);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mTriggerTimer);
    if (theResult == kErrorOK)
        theResult = this->mMediaStrings.Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mIsMovingCamera);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mStartX);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mStartY);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mShowDebugIDs);
    if (theResult == kErrorOK)
        theResult = EESMLoadVector(inMajorVersion, inMinorVersion, inFile, this->mMessages);
    if (theResult == kErrorOK)
        theResult = this->mExportFileName.Load(inFile);
    if (theResult == kErrorOK)
        theResult = this->mLastChatText.Load(inFile);
    return theResult;
}

// FUNCTION: EXE 0x5b45cb
// Sets the run flag of the first timed entry of an effect whose flag differs
void EEScenarioManager::SetTimedEffectHasRun(EESMEffect* inEffect, bool inFlag)
{
    for (unsigned long i = 0; i < this->mTimedEffects.size(); i++) {
        if (this->GetEffect(this->mTimedEffects[i].mEffectIndex) == inEffect && this->mTimedEffects[i].mHasRun != inFlag) {
            this->mTimedEffects[i].mHasRun = inFlag;
            return;
        }
    }
}

// FUNCTION: EXE 0x5b463b
// Parses "<id>_Condition:<name>" into the ID and the name; 9 when the line is not a condition entry
long EEScenarioManager::ParseConditionID(const UString& inLine, unsigned long inKind, long* outID, UWideString& outRest)
{
    UWideString theNumber;
    long theResult;
    unsigned long theIndex = inLine.Find("_Condition:", 0);
    if (theIndex <= 0 || theIndex >= inLine.GetLength())
        theResult = 9;
    else {
        theNumber.CopyString((const char*)inLine, theIndex);
        *outID = atoi(theNumber);
        if (theIndex + 1 < inLine.GetLength())
            outRest = (const char*)_mbsninc(reinterpret_cast<const unsigned char*>((const char*)inLine), theIndex + 11);
        theResult = 0;
    }
    return theResult;
}

// FUNCTION: EXE 0x5b46bf
// Parses "<id>_Effect:<name>" (kind 0: "<id>_<name>") into the ID and the name; 9 when it is not an effect entry
long EEScenarioManager::ParseEffectID(const UString& inLine, unsigned long inKind, long* outID, UWideString& outRest)
{
    UWideString theNumber;
    unsigned long theIndex;
    if (inKind <= 0) {
        theIndex = inLine.Find("_", 0);
        if (theIndex <= 0 || theIndex >= inLine.GetLength())
            return 9;
        theNumber.CopyString((const char*)inLine, theIndex);
        *outID = atoi(theNumber);
        if (theIndex + 1 < inLine.GetLength())
            outRest = (const char*)_mbsninc(reinterpret_cast<const unsigned char*>((const char*)inLine), theIndex + 1);
    } else {
        theIndex = inLine.Find("_Effect:", 0);
        if (theIndex <= 0 || theIndex >= inLine.GetLength())
            return 9;
        theNumber.CopyString((const char*)inLine, theIndex);
        *outID = atoi(theNumber);
        if (theIndex + 1 < inLine.GetLength())
            outRest = (const char*)_mbsninc(reinterpret_cast<const unsigned char*>((const char*)inLine), theIndex + 8);
    }
    return 0;
}

// FUNCTION: EXE 0x5b4796
// Parses "<id>_Player Name:<name>" into the player number and the name; 9 when it is not a player entry
long EEScenarioManager::ParsePlayerName(const UString& inLine, unsigned long inKind, long* outID, UWideString& outRest)
{
    UWideString theNumber;
    long theResult;
    unsigned long theIndex = inLine.Find("_Player Name:", 0);
    if (theIndex <= 0 || theIndex >= inLine.GetLength())
        theResult = 9;
    else {
        theNumber.CopyString((const char*)inLine, theIndex);
        *outID = atoi(theNumber);
        if (theIndex + 1 < inLine.GetLength())
            outRest = (const char*)_mbsninc(reinterpret_cast<const unsigned char*>((const char*)inLine), theIndex + 13);
        theResult = 0;
    }
    return theResult;
}

// FUNCTION: EXE 0x5b481a
// Shows the next queued dialog message (in a cinematic the next cinematic one), stopping one-shot sounds for a
// voiced one and moving the camera to its unit; none left: hides the messages; nothing while paused
void EEScenarioManager::ShowDialogMessage()
{
    this->mIsShowingDialog = false;
    if (gCommunications == NULL || gCommunications->GetIsGamePaused())
        return;
    if (this->mMessages.size() > 0) {
        long theIndex;
        if (gWorld.mInCinematic) {
            theIndex = -1;
            for (unsigned long i = 0; i < this->mMessages.size(); i++) {
                if (this->mMessages[i].mIsCinematic) {
                    theIndex = i;
                    break;
                }
            }
        } else
            theIndex = 0;
        if (theIndex != -1) {
            if (this->mMessages[theIndex].mSound.GetLength() > 0)
                gSoundManager->StopAllOneShotSounds();
            gUserInterface->PostDialogMessage(this->mMessages[theIndex]);
            EEUnit* theUnit = gWorld.GetUnit(this->mMessages[theIndex].mUnitID);
            if (theUnit != NULL && this->mMessages[theIndex].mScrollSpeed >= 0) {
                gWorld.mCameraControl.ClearUnitA();
                gWorld.mCameraControl.ClearUnitB();
                GE3DPoint theOffset(-10000.0f, 0.0f, 0.0f);
                gWorld.mCameraControl.SetMode(0);
                float theAngle;
                if (this->mMessages[theIndex].mScrollSpeed == 0)
                    theAngle = 3.1415927f;
                else
                    theAngle = 0.0f;
                EESMDialogMessage& theMessage = this->mMessages[theIndex];
                EESMDialogMessage& theOther = this->mMessages[theIndex];
                gWorld.mCameraControl.MoveTo(theUnit, theOffset, true, (float)theOther.mScrollSpeed,
                                               theMessage.mScrollSpeed == 0 ? theUnit : NULL, theOffset, false, 0.1f,
                                               theAngle, 0.0f);
            }
            this->mMessages.erase(this->mMessages.begin() + theIndex);
            this->mIsShowingDialog = true;
            return;
        }
    }
    gUserInterface->PostHideMessages();
    gSoundManager->StopAllOneShotSounds();
}

// FUNCTION: EXE 0x5b4a95
// Applies the trigger text entries: each goes to the media strings, else renames the effect / condition / player
// it names and sets its text; stops with 9 at an unknown ID or an entry whose kind takes no text
// The trigger text import's second pass (after ValidateText)
long EEScenarioManager::ImportText(const std::vector<UString>& inLines, unsigned long inKind,
                                   const std::vector<UWideString>& inTexts)
{
    UWideString theName;
    long theResult = 0;
    long theID;
    for (unsigned long i = 0; i < inLines.size() && theResult == 0; i++) {
        theResult = this->mMediaStrings.Import(*(inLines.begin() + i), inKind, inTexts[i]);
        if (theResult == 0)
            continue;
        theResult = this->ParseEffectID(inLines[i], inKind, &theID, theName);
        if (theResult == 0) {
            EESMEffect* theEffect = this->FindEffectByID(theID);
            if (theEffect == NULL) {
                theResult = 9;
                continue;
            }
            theEffect->mName = theName;
            if (theEffect->mType == kSMEffectMedia) {
                switch (theEffect->mMediaAction) {
                case kSMMediaActionChangeText:
                    theEffect->mDescription = *(inTexts.begin() + i);
                    break;
                case kSMMediaActionSendDialogue:
                case kSMMediaActionSendChatMessage:
                    theEffect->mMessage = *(inTexts.begin() + i);
                    break;
                default:
                    theResult = 9;
                    break;
                }
            } else if (theEffect->mType == kSMEffectObject) {
                if (theEffect->mObjectAction == kSMObjectActionClassName)
                    theEffect->mNewName = UWideString(*(inTexts.begin() + i));
                else
                    theResult = 9;
            }
            continue;
        }
        theResult = this->ParseConditionID(*(inLines.begin() + i), inKind, &theID, theName);
        if (theResult == 0) {
            EESMCondition* theCondition = this->FindConditionByID(theID);
            if (theCondition != NULL && theCondition->mType == kSMConditionChatMessageContains) {
                theCondition->mName = theName;
                theCondition->mChatText = inTexts[i];
            } else {
                theResult = 9;
            }
            continue;
        }
        theResult = this->ParsePlayerName(inLines[i], inKind, &theID, theName);
        if (theResult != 0 || (unsigned long)theID <= 0 || !((unsigned long)theID < gWorld.mNumPlayers))
            continue;
        if (gWorld.GetPlayer(theID) != NULL)
            gWorld.GetPlayer(theID)->mName = theName;
        else
            theResult = 9;
    }
    return theResult;
}

// FUNCTION: EXE 0x5b4d1a
// Writes triggers, conditions, effects, objects and areas (not the built-ins), timed and pending effects, ID
// counters, clocks, media strings, start camera and dialog messages; returns the first write error
long EEScenarioManager::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    unsigned long i = 0;
    unsigned long theCount = this->mTriggers.size();
    long theResult = outFile.Write(theCount);
    // Each section's count is written unconditionally, which used to replace the previous section's error:
    // the first error is kept and returned (the bytes written are unchanged).
    long theFirstError = kErrorOK;
    if (theResult == 0) {
        for (; i < theCount; i++) {
            EESMTrigger*& theTrigger = this->mTriggers[i];
            theResult = theTrigger->Save(inMajorVersion, inMinorVersion, outFile);
            if (theResult != 0)
                break;
        }
    }
    if (theFirstError == kErrorOK)
        theFirstError = theResult;
    theCount = this->mConditions.size() - 1;
    theResult = outFile.Write(theCount);
    if (theResult == 0) {
        for (i = 1; i < theCount + 1; i++) {
            EESMCondition*& theCondition = this->mConditions[i];
            theResult = theCondition->Save(inMajorVersion, inMinorVersion, outFile);
            if (theResult != 0)
                break;
        }
    }
    if (theFirstError == kErrorOK)
        theFirstError = theResult;
    theCount = this->mEffects.size() - 1;
    theResult = outFile.Write(theCount);
    if (theResult == 0) {
        for (i = 1; i < theCount + 1; i++) {
            EESMEffect*& theEffect = this->mEffects[i];
            theResult = theEffect->Save(inMajorVersion, inMinorVersion, outFile);
            if (theResult != 0)
                break;
        }
    }
    if (theFirstError == kErrorOK)
        theFirstError = theResult;
    theCount = this->mObjects.size() - 1;
    theResult = outFile.Write(theCount);
    if (theResult == 0) {
        for (i = 1; i < theCount + 1; i++) {
            EESMObject*& theObject = this->mObjects[i];
            theResult = theObject->Save(inMajorVersion, inMinorVersion, outFile);
            if (theResult != 0)
                break;
        }
    }
    if (theFirstError == kErrorOK)
        theFirstError = theResult;
    theCount = this->mAreas.size() - 2;
    theResult = outFile.Write(theCount);
    if (theResult == 0) {
        for (i = 2; i < theCount + 2; i++) {
            EESMArea*& theArea = this->mAreas[i];
            theResult = theArea->Save(inMajorVersion, inMinorVersion, outFile);
            if (theResult != 0)
                break;
        }
    }
    if (theFirstError == kErrorOK)
        theFirstError = theResult;
    theCount = this->mTimedEffects.size();
    if (theResult == 0)
        theResult = outFile.Write(theCount);
    if (theResult == 0) {
        for (unsigned long j = 0; j < theCount; j++) {
            if ((theResult = outFile.Write((this->mTimedEffects.begin() + j)->mEffectIndex)) != 0 || (theResult = outFile.Write(this->mTimedEffects[j].mTime)) != 0
                || (theResult = outFile.Write(this->mTimedEffects[j].mIsInCinematic)) != 0)
                break;
        }
    }
    if (theFirstError == kErrorOK)
        theFirstError = theResult;
    theCount = this->mPendingEffects.size();
    theResult = outFile.Write(theCount);
    if (theResult == 0) {
        for (unsigned long k = 0; k < theCount; k++) {
            EESMEffect*& theEffect = this->mPendingEffects[k];
            theResult = theEffect->Save(inMajorVersion, inMinorVersion, outFile);
            if (theResult != 0)
                break;
        }
        if (theResult == 0 && (theResult = outFile.Write(this->mNextTriggerID)) == 0 && (theResult = outFile.Write(this->mNextConditionID)) == 0
            && (theResult = outFile.Write(this->mNextEffectID)) == 0 && (theResult = outFile.Write(this->mNextObjectID)) == 0
            && (theResult = outFile.Write(this->mNextAreaID)) == 0 && (theResult = outFile.Write(this->mTime)) == 0
            && (theResult = outFile.Write(this->mTriggerTimer)) == 0
            && (theResult = this->mMediaStrings.Save(inMajorVersion, inMinorVersion, outFile)) == 0) {
            if (inMinorVersion >= 0x14) {
                if ((theResult = outFile.Write(this->mIsMovingCamera)) == 0 && (theResult = outFile.Write(this->mStartX)) == 0)
                    theResult = outFile.Write(this->mStartY);
            }
            if (theResult == 0 && (theResult = outFile.Write(this->mShowDebugIDs)) == 0
                && (theResult = EESMSaveVector(inMajorVersion, inMinorVersion, outFile, this->mMessages)) == 0
                && (theResult = this->mExportFileName.Save(outFile)) == 0)
                theResult = this->mLastChatText.Save(outFile);    // its result was dropped
        }
    }
    return theFirstError != kErrorOK ? theFirstError : theResult;
}

// FUNCTION: EXE 0x5b5173
// Moves the camera one zoom-scaled step towards the scenario's start location; stops moving once it is there
void EEScenarioManager::MoveCameraToStart()
{
    GE3DPoint theLocation = gWorld.mLine.GetStart();
    if (theLocation.GetX() == this->mStartX && theLocation.GetY() == this->mStartY) {
        this->mIsMovingCamera = false;
        return;
    }
    float theStep = (gWorld.mCameraControl.mZoom + 0.5f) * gSideScrollSpeed;
    float theDX = theLocation.GetX() - this->mStartX;
    float theAbsDX = UMAbs(theDX);
    float theDY = theLocation.GetY() - this->mStartY;
    float theAbsDY = UMAbs(theDY);
    float theTotal = theAbsDY + theAbsDX;
    float theStepX = theAbsDX * theStep / theTotal;
    float theStepY = theAbsDY * theStep / theTotal;
    if (theStepX > UMAbs(theDX))
        theStepX = UMAbs(theDX);
    if (theLocation.GetX() > this->mStartX)
        theStepX = -theStepX;
    if (theStepY > UMAbs(theDY))
        theStepY = UMAbs(theDY);
    if (theLocation.GetY() > this->mStartY)
        theStepY = -theStepY;
    theLocation.SetX(theLocation.GetX() + theStepX);
    theLocation.SetY(theLocation.GetY() + theStepY);
    gWorld.mCameraControl.SetLocation(theLocation, false, true);
}

// FUNCTION: EXE 0x5b52bb
// Checks the trigger text entries: lists an error (mImportErrors) for each unknown token, ID or player number,
// wrong effect / condition kind or name that differs from the scenario's; 9 when there are any
// The trigger text import's first pass
long EEScenarioManager::ValidateText(std::vector<UString>& inLines, unsigned long inKind,
                                     const std::vector<UWideString>& inTexts)
{
    UWideString theFormat;
    UWideString theMessage;
    UWideString theName;
    long theID;
    this->mImportErrors.clear();
    for (unsigned long i = 0; i < inLines.size(); i++) {
        if (this->mMediaStrings.Validate(inLines[i], inKind) == 0)
            continue;
        if (this->ParseEffectID(inLines[i], inKind, &theID, theName) == 0) {
            EESMEffect* theEffect = this->FindEffectByID(theID);
            if (theEffect == NULL) {
                gResource->GetString(IDS_IS_INVALID_EFFECT_ID_FOR_TOKEN, theFormat);
                sprintf(theMessage, theFormat, theID, (const char*)inLines[i]);
            } else if (theEffect->GetType() == kSMEffectMedia
                       && (theEffect->mMediaAction == kSMMediaActionChangeText || theEffect->mMediaAction == kSMMediaActionSendDialogue || theEffect->mMediaAction == kSMMediaActionSendChatMessage)
                       || theEffect->GetType() == kSMEffectObject && theEffect->mObjectAction == kSMObjectActionClassName) {
                if (!(theName != theEffect->GetName()))
                    continue;
                gResource->GetString(IDS_TEXT_FILE_EFFECT_NAME_DOES_NOT_MATCH_SCENARIOS, theFormat);
                sprintf(theMessage, theFormat, (const char*)inLines[i], (const char*)theEffect->GetName());
            } else {
                gResource->GetString(IDS_IS_INVALID_EFFECT_TYPE_TO_BE_TOKEN, theFormat);
                sprintf(theMessage, theFormat, (const char*)inLines[i]);
            }
        } else if (this->ParseConditionID(inLines[i], inKind, &theID, theName) == 0) {
            EESMCondition* theCondition = this->FindConditionByID(theID);
            if (theCondition == NULL) {
                gResource->GetString(IDS_IS_INVALID_CONDITION_ID_FOR_TOKEN, theFormat);
                sprintf(theMessage, theFormat, theID, (const char*)inLines[i]);
            } else if (theCondition->GetType() == kSMConditionChatMessageContains) {
                if (!(theName != theCondition->GetName()))
                    continue;
                gResource->GetString(IDS_TEXT_FILE_EFFECT_NAME_DOES_NOT_MATCH_SCENARIOS_2747, theFormat);
                sprintf(theMessage, theFormat, (const char*)inLines[i], (const char*)theCondition->GetName());
            } else {
                gResource->GetString(IDS_IS_INVALID_CONDITION_TYPE_TO_BE_TOKEN, theFormat);
                sprintf(theMessage, theFormat, (const char*)inLines[i]);
            }
        } else if (this->ParsePlayerName(inLines[i], inKind, &theID, theName) == 0) {
            if (theID != 0 && (unsigned long)theID < gWorld.mNumPlayers)
                continue;
            gResource->GetString(IDS_INVALID_PLAYER_NUMBER_FOR_THIS_SCENARIO, theFormat);
            sprintf(theMessage, theFormat, theID);
        } else {
            gResource->GetString(IDS_IS_INVALID_TOKEN, theFormat);
            sprintf(theMessage, theFormat, (const char*)inLines[i]);
        }
        this->mImportErrors.push_back(theMessage);
    }
    return this->mImportErrors.size() > 0 ? 9 : 0;
}

// FUNCTION: EXE 0x5b5652
// Empties a vector of 8-byte entries (vector<T>::clear; one body for every such vector)

// FUNCTION: EXE 0x5b568d
// Appends a trigger to its list (std::vector<EESMTrigger*>::push_back)

// FUNCTION: EXE 0x5b56b9
// Appends a condition to its list (std::vector<EESMCondition*>::push_back)

// FUNCTION: EXE 0x5b56e5
// Appends an effect to its list (std::vector<EESMEffect*>::push_back)

// FUNCTION: EXE 0x5b5711
// Erases one entry of a pointer list (vector<T*>::erase; one body for every pointer vector)

// FUNCTION: EXE 0x5b5772
// Empties a pointer list (vector<T*>::clear; one body for every pointer vector)

// FUNCTION: EXE 0x5b57ad
// Appends a scenario object to its list (std::vector<EESMObject*>::push_back)

// FUNCTION: EXE 0x5b57d9
// Appends an area to its list (std::vector<EESMArea*>::push_back)

// FUNCTION: EXE 0x5b5805
// Appends a dialog message to its list (std::vector<EESMDialogMessage>::push_back)

// FUNCTION: EXE 0x5b5831
// Erases one dialog message from the list (std::vector<EESMDialogMessage>::erase)

// TEMPLATE: EXE 0x5b58a6
// ??H?$_Ptrit@VEESMDialogMessage@@HPAV1@AAV1@PAV1@AAV1@@std@@QBE?AV01@H@Z (Dinkum COMDAT)

// FUNCTION: EXE 0x5b58b8
// Appends a timed effect entry to its list (std::vector<EESMTimedEffect>::push_back)

// FUNCTION: EXE 0x5b58e4
// Erases one timed effect entry from the list (std::vector<EESMTimedEffect>::erase)

// FUNCTION: EXE 0x5b5945
// Empties a list of timed effects (std::vector<EESMTimedEffect>::clear)

// FUNCTION: EXE 0x5b5980
// Inserts a dialog message at a position in its list (std::vector<EESMDialogMessage>::insert)

// TEMPLATE: EXE 0x5b59d0
// ?_Buy@?$vector@VEESMDialogMessage@@V?$allocator@VEESMDialogMessage@@@std@@@std@@IAE_NI@Z (Dinkum COMDAT)

// TEMPLATE: EXE 0x5b5a09
// ?_Tidy@?$vector@VEESMDialogMessage@@V?$allocator@VEESMDialogMessage@@@std@@@std@@IAEXXZ (Dinkum COMDAT)

// FUNCTION: EXE 0x5b5a43
// Inserts n copies of a dialog message at a position in its list (std::vector<EESMDialogMessage>::insert)

// FUNCTION: EXE 0x5b5cfd
// Destroys a dialog message's text and sound strings
// (EESMDialogMessage's implicit dtor: destroys +0x18, +0x04, no vptr store; emitted for vector<EESMDialogMessage>)

// TEMPLATE: EXE 0x5b5d14
// ?_Ufill@?$vector@VEESMDialogMessage@@V?$allocator@VEESMDialogMessage@@@std@@@std@@IAEPAVEESMDialogMessage@@PAV3@IABV3@@Z (Dinkum COMDAT)

// TEMPLATE: EXE 0x5b5d3f
// ??G?$_Ptrit@VEESMDialogMessage@@HPAV1@AAV1@PAV1@AAV1@@std@@QBE?AV01@H@Z (Dinkum COMDAT)

// FUNCTION: EXE 0x5b5d53
// Deletes every trigger in a list and empties it (DeleteVectorContents<EESMTrigger*>)

// FUNCTION: EXE 0x5b5d90
// Deletes every condition in a list and empties it (DeleteVectorContents<EESMCondition*>)

// FUNCTION: EXE 0x5b5dcd
// Deletes every effect in a list and empties it (DeleteVectorContents<EESMEffect*>)

// FUNCTION: EXE 0x5b5e0a
// Deletes every scenario object in a list and empties it (DeleteVectorContents<EESMObject*>)

// FUNCTION: EXE 0x5b5e3a
// Deletes every area in a list and empties it (DeleteVectorContents<EESMArea*>)

// FUNCTION: EXE 0x5b5e77
// Sorts the timed effects by due time, keeping the order of equal ones (std::stable_sort)

// FUNCTION: EXE 0x5b5eab
// Sorts the areas by name, keeping the order of equal ones (std::stable_sort)

// FUNCTION: EXE 0x5b5edf
// Sorts the conditions by name, keeping the order of equal ones (std::stable_sort)

// FUNCTION: EXE 0x5b5f13
// Sorts the effects by name, keeping the order of equal ones (std::stable_sort)

// FUNCTION: EXE 0x5b5f47
// Sorts the scenario objects by name, keeping the order of equal ones (std::stable_sort)

// FUNCTION: EXE 0x5b5f7b
// Sorts the triggers by name, keeping the order of equal ones (std::stable_sort)

// FUNCTION: EXE 0x5b5faf
// Reads the dialog message list: the count, then each message; returns the first read error (EESMLoadVector)

// FUNCTION: EXE 0x5b5fff
// Writes the dialog message list: the count, then each message; returns the first write error (EESMSaveVector)

// FUNCTION: EXE 0x5b6054
// Copies a range of dialog messages (std::copy)

// TEMPLATE: EXE 0x5b607d
// ?_Ucopy@?$vector@VEESMDialogMessage@@V?$allocator@VEESMDialogMessage@@@std@@@std@@IAEPAVEESMDialogMessage@@V?$_Ptrit@VEESMDialogMessage@@HPAV1@AAV1@PAV1@AAV1@@2@0PAV3@@Z (Dinkum COMDAT)

// TEMPLATE: EXE 0x5b60a9
// ?fill@std@@YAXV?$_Ptrit@VEESMDialogMessage@@HPAV1@AAV1@PAV1@AAV1@@1@0ABVEESMDialogMessage@@@Z (Dinkum COMDAT)

// TEMPLATE: EXE 0x5b60c6
// ?copy_backward@std@@YA?AV?$_Ptrit@VEESMDialogMessage@@HPAV1@AAV1@PAV1@AAV1@@1@V21@00@Z (Dinkum COMDAT)

// TEMPLATE: EXE 0x5b60f2
// ?fill@std@@YAXV?$_Ptrit@UEESMTimedEffect@@HPAU1@AAU1@PAU1@AAU1@@1@0ABUEESMEntry164@@@Z (Dinkum COMDAT)

// TEMPLATE: EXE 0x5b6113
// ?_Construct@std@@YAXPAVEESMDialogMessage@@ABV2@@Z (Dinkum COMDAT)

// FUNCTION: EXE 0x5b6125
// Destroys a dialog message and frees it when asked (EESMDialogMessage scalar deleting destructor)

// FUNCTION: EXE 0x5b614e
// Resizes the dialog message list, padding with empty messages (vector<EESMDialogMessage>::resize)

// FUNCTION: EXE 0x5b616a
// Resizes the dialog message list, padding with copies of a message (vector<EESMDialogMessage>::resize)

// FUNCTION: EXE 0x5b6225
// Erases a range of dialog messages from the list (std::vector<EESMDialogMessage>::erase)

// FUNCTION: EXE 0x5b6290
// Gets a temporary buffer and runs the stable sort of the timed effects by due time (std::_Stable_sort)

// FUNCTION: EXE 0x5b62fd
// Gets a temporary buffer and runs the stable sort of the areas by name (std::_Stable_sort)

// FUNCTION: EXE 0x5b636a
// Gets a temporary buffer and runs the stable sort of the conditions by name (std::_Stable_sort)

// FUNCTION: EXE 0x5b63d7
// Gets a temporary buffer and runs the stable sort of the effects by name (std::_Stable_sort)

// FUNCTION: EXE 0x5b6444
// Gets a temporary buffer and runs the stable sort of the scenario objects by name (std::_Stable_sort)

// FUNCTION: EXE 0x5b64b1
// Gets a temporary buffer and runs the stable sort of the triggers by name (std::_Stable_sort)

// FUNCTION: EXE 0x5b651e
// Copies a range of dialog messages (std::copy)

// TEMPLATE: EXE 0x5b6542
// ??0?$_Temp_iterator@UEESMTimedEffect@@@std@@QAE@H@Z (Dinkum COMDAT)

// TEMPLATE: EXE 0x5b658a
// ??0?$_Temp_iterator@PAVEESMArea@@@std@@QAE@H@Z (Dinkum COMDAT)

// FUNCTION: EXE 0x5b65d1
// Counts the entries in a range of the timed effects (std::_Distance)

// FUNCTION: EXE 0x5b65fc
// Stable-sorts the timed effects with the temporary buffer, merging in place when it is short (std::_Stable_sort)

// FUNCTION: EXE 0x5b6715
// Stable-sorts the areas with the temporary buffer, merging in place when it is short (std::_Stable_sort)

// FUNCTION: EXE 0x5b682b
// Stable-sorts the conditions with the temporary buffer, merging in place when it is short (std::_Stable_sort)

// FUNCTION: EXE 0x5b6941
// Stable-sorts the effects with the temporary buffer, merging in place when it is short (std::_Stable_sort)

// FUNCTION: EXE 0x5b6a57
// Stable-sorts the scenario objects with the temporary buffer, merging in place when it is short (std::_Stable_sort)

// FUNCTION: EXE 0x5b6b6d
// Stable-sorts the triggers with the temporary buffer, merging in place when it is short (std::_Stable_sort)

// TEMPLATE: EXE 0x5b6c83
// ?_Distance@std@@YAXV?$_Ptrit@UEESMTimedEffect@@HPAU1@AAU1@PAU1@AAU1@@1@0AAHUrandom_access_iterator_tag@1@@Z (Dinkum COMDAT)

// FUNCTION: EXE 0x5b6c98
// Insertion-sorts a short run of the timed effects by due time (std::_Insertion_sort)

// FUNCTION: EXE 0x5b6cc1
// Merge-sorts the timed effects by due time through the temporary buffer (std::_Buffered_merge_sort)

// FUNCTION: EXE 0x5b6da8
// Merges two sorted runs of the timed effects, through the buffer when they fit (std::_Buffered_merge)

// TEMPLATE: EXE 0x5b70cf
// ?_Distance@std@@YAXV?$_Ptrit@PAVEESMArea@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0AAHUrandom_access_iterator_tag@1@@Z (Dinkum COMDAT)

// FUNCTION: EXE 0x5b70e1
// Merges two sorted runs of the areas, through the buffer when they fit (std::_Buffered_merge)

// FUNCTION: EXE 0x5b7407
// Merges two sorted runs of the conditions, through the buffer when they fit (std::_Buffered_merge)

// FUNCTION: EXE 0x5b772d
// Insertion-sorts a short run of the effects by name (std::_Insertion_sort)

// FUNCTION: EXE 0x5b7756
// Merge-sorts the effects by name through the temporary buffer (std::_Buffered_merge_sort)

// FUNCTION: EXE 0x5b783d
// Merges two sorted runs of the effects, through the buffer when they fit (std::_Buffered_merge)

// FUNCTION: EXE 0x5b7b63
// Merges two sorted runs of the scenario objects, through the buffer when they fit (std::_Buffered_merge)

// FUNCTION: EXE 0x5b7e89
// Insertion-sorts a short run of the areas by name (std::_Insertion_sort)

// FUNCTION: EXE 0x5b7eb2
// Merge-sorts the areas by name through the temporary buffer (std::_Buffered_merge_sort)

// FUNCTION: EXE 0x5b7f99
// Merges two sorted runs of the triggers, through the buffer when they fit (std::_Buffered_merge)

// TEMPLATE: EXE 0x5b82bf
// ?_First@?$_Temp_iterator@UEESMTimedEffect@@@std@@QBEPAUEESMEntry164@@XZ (Dinkum COMDAT)

// TEMPLATE: EXE 0x5b82c5
// ?_Last@?$_Temp_iterator@UEESMTimedEffect@@@std@@QBEPAUEESMEntry164@@XZ (Dinkum COMDAT)

// TEMPLATE: EXE 0x5b82cc
// ?_Init@?$_Temp_iterator@UEESMTimedEffect@@@std@@QAEAAV12@XZ (Dinkum COMDAT)

// FUNCTION: EXE 0x5b82d7
// Insertion-sorts a run of the timed effects by due time (std::_Insertion_sort_1)

// TEMPLATE: EXE 0x5b8381
// ??E?$_Ptrit@UEESMTimedEffect@@HPAU1@AAU1@PAU1@AAU1@@std@@QAEAAV01@XZ (Dinkum COMDAT)

// FUNCTION: EXE 0x5b8387
// Merges adjacent sorted chunks of the timed effects between list and buffer (std::_Chunked_merge)

// FUNCTION: EXE 0x5b84cb
// Merges adjacent sorted chunks of the timed effects between list and buffer (std::_Chunked_merge)

// TEMPLATE: EXE 0x5b8578
// ?copy@std@@YA?AV?$_Temp_iterator@UEESMTimedEffect@@@1@V?$_Ptrit@UEESMTimedEffect@@HPAU1@AAU1@PAU1@AAU1@@1@0V21@@Z (Dinkum COMDAT)

// FUNCTION: EXE 0x5b85b5
// Merges two sorted runs of the timed effects into a destination (std::merge)

// FUNCTION: EXE 0x5b8652
// Merges two sorted runs of the timed effects from the back (std::_Merge_backward)

// FUNCTION: EXE 0x5b8700
// Finds the first of the timed effects not ordered before a value (std::lower_bound)

// FUNCTION: EXE 0x5b8738
// Finds the first of the timed effects ordered after a value (std::upper_bound)

// FUNCTION: EXE 0x5b8770
// Rotates a range of the timed effects, through the buffer when it fits (std::_Buffered_rotate)

// FUNCTION: EXE 0x5b8915
// Insertion-sorts a run of the areas by name (std::_Insertion_sort_1)

// FUNCTION: EXE 0x5b89c1
// Merges adjacent sorted chunks of the areas between list and buffer (std::_Chunked_merge)

// TEMPLATE: EXE 0x5b8a59
// ?copy@std@@YA?AV?$_Temp_iterator@PAVEESMArea@@@1@V?$_Ptrit@PAVEESMArea@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0V21@@Z (Dinkum COMDAT)

// FUNCTION: EXE 0x5b8a96
// Merges two sorted runs of the areas from the back (std::_Merge_backward)

// FUNCTION: EXE 0x5b8b41
// Insertion-sorts a run of the effects by name (std::_Insertion_sort_1)

// FUNCTION: EXE 0x5b8bed
// Merges adjacent sorted chunks of the effects between list and buffer (std::_Chunked_merge)

// FUNCTION: EXE 0x5b8d29
// Merges adjacent sorted chunks of the effects between list and buffer (std::_Chunked_merge)

// FUNCTION: EXE 0x5b8dc1
// Merges two sorted runs of the effects into a destination (std::merge)

// FUNCTION: EXE 0x5b8e57
// Merges two sorted runs of the effects from the back (std::_Merge_backward)

// FUNCTION: EXE 0x5b8f02
// Finds the first of the effects not ordered before a value (std::lower_bound)

// FUNCTION: EXE 0x5b8f3a
// Finds the first of the effects ordered after a value (std::upper_bound)

// FUNCTION: EXE 0x5b8f72
// Merges adjacent sorted chunks of the areas between list and buffer (std::_Chunked_merge)

// FUNCTION: EXE 0x5b90ae
// Rotates a range of the areas, through the buffer when it fits (std::_Buffered_rotate)

// FUNCTION: EXE 0x5b9250
// Merges two sorted runs of the areas into a destination (std::merge)

// FUNCTION: EXE 0x5b92e6
// Finds the first of the areas not ordered before a value (std::lower_bound)

// FUNCTION: EXE 0x5b931e
// Finds the first of the areas ordered after a value (std::upper_bound)

// TEMPLATE: EXE 0x5b9356
// ??4?$_Temp_iterator@UEESMTimedEffect@@@std@@QAEAAV01@ABUEESMEntry164@@@Z (Dinkum COMDAT)

// TEMPLATE: EXE 0x5b9395
// ??4?$_Temp_iterator@PAVEESMArea@@@std@@QAEAAV01@ABQAVEESMArea@@@Z (Dinkum COMDAT)

// FUNCTION: EXE 0x5b93d2
// Moves one of the timed effects back to its sorted place (std::_Unguarded_insert)

// FUNCTION: EXE 0x5b9412
// Merges two sorted runs of the timed effects into the temporary buffer (std::merge)

// FUNCTION: EXE 0x5b94f2
// Merges two sorted runs of the timed effects into a destination (std::merge)

// TEMPLATE: EXE 0x5b957e
// ?copy@std@@YA?AV?$_Ptrit@UEESMTimedEffect@@HPAU1@AAU1@PAU1@AAU1@@1@V21@00@Z (Dinkum COMDAT)

// TEMPLATE: EXE 0x5b95ab
// ?_Iter_swap@std@@YAXV?$_Ptrit@UEESMTimedEffect@@HPAU1@AAU1@PAU1@AAU1@@1@0PAUEESMEntry164@@@Z (Dinkum COMDAT)

// FUNCTION: EXE 0x5b95d2
// Finds the first of the timed effects not ordered before a value (std::_Lower_bound)

// FUNCTION: EXE 0x5b9657
// Finds the first of the timed effects ordered after a value (std::_Upper_bound)

// FUNCTION: EXE 0x5b96dc
// Rotates a range of the timed effects (std::rotate)

// FUNCTION: EXE 0x5b971d
// Finds the first of the areas ordered after a value (std::_Upper_bound)

// FUNCTION: EXE 0x5b979d
// Rotates a range of the areas (std::rotate)

// FUNCTION: EXE 0x5b97de
// Finds the first of the areas not ordered before a value (std::_Lower_bound)

// TEMPLATE: EXE 0x5b985f
// ?_Unguarded_insert@std@@YAXV?$_Ptrit@PAVEESMEffect@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@PAVEESMEffect@@UEESMEffectLess@@@Z (Dinkum COMDAT)

// FUNCTION: EXE 0x5b98aa
// Merges two sorted runs of the effects into the temporary buffer (std::merge)

// FUNCTION: EXE 0x5b998c
// Merges two sorted runs of the effects into a destination (std::merge)

// FUNCTION: EXE 0x5b9a0c
// Finds the first of the effects not ordered before a value (std::_Lower_bound)

// FUNCTION: EXE 0x5b9a8d
// Finds the first of the effects ordered after a value (std::_Upper_bound)

// FUNCTION: EXE 0x5b9b0d
// Merges two sorted runs of the areas into the temporary buffer (std::merge)

// FUNCTION: EXE 0x5b9bef
// Merges two sorted runs of the areas into a destination (std::merge)

// TEMPLATE: EXE 0x5b9c6f
// ?copy@std@@YA?AV?$_Ptrit@PAVEESMTrigger@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@V21@00@Z (Dinkum COMDAT)

// TEMPLATE: EXE 0x5b9c93
// ?_Unguarded_insert@std@@YAXV?$_Ptrit@PAVEESMArea@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@PAVEESMArea@@UEESMAreaLess@@@Z (Dinkum COMDAT)

// FUNCTION: EXE 0x5b9cde
// Rotates a range of the timed effects by element swaps (std::_Rotate)

// FUNCTION: EXE 0x5b9d12
// Rotates a range of the areas by element swaps (std::_Rotate)

// TEMPLATE: EXE 0x5b9d46
// ?_Rotate@std@@YAXV?$_Ptrit@UEESMTimedEffect@@HPAU1@AAU1@PAU1@AAU1@@1@00PAHPAUEESMEntry164@@@Z (Dinkum COMDAT)

// TEMPLATE: EXE 0x5b9e20
// ?_Rotate@std@@YAXV?$_Ptrit@PAVEESMArea@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@00PAHPAPAVEESMArea@@@Z (Dinkum COMDAT)

// FUNCTION: EXE 0x5b9eb8
// Creates an empty dialog message: no unit or duration, no camera move (scroll speed -1), not for a cinematic
EESMDialogMessage::EESMDialogMessage()
{
    this->mScrollSpeed = -1;
    this->mUnitType = 0;
    this->mDuration = 0;
    this->mUnitID = 0;
    this->mIsCinematic = false;
}

// FUNCTION: EXE 0x5b9ee9
// Creates a copy of a dialog message
EESMDialogMessage::EESMDialogMessage(const EESMDialogMessage& inMessage)
{
    this->mText = inMessage.mText;
    this->mUnitType = inMessage.mUnitType;
    this->mSound = inMessage.mSound;
    this->mDuration = inMessage.mDuration;
    this->mUnitID = inMessage.mUnitID;
    this->mScrollSpeed = inMessage.mScrollSpeed;
    this->mIsCinematic = inMessage.mIsCinematic;
}

// FUNCTION: EXE 0x5b9f4a
// Copies another dialog message's text, unit, sound, duration, scroll speed and cinematic flag
void EESMDialogMessage::operator=(const EESMDialogMessage& inMessage)
{
    this->mText = inMessage.mText;
    this->mUnitType = inMessage.mUnitType;
    this->mSound = inMessage.mSound;
    this->mDuration = inMessage.mDuration;
    this->mUnitID = inMessage.mUnitID;
    this->mScrollSpeed = inMessage.mScrollSpeed;
    this->mIsCinematic = inMessage.mIsCinematic;
}

// FUNCTION: EXE 0x5b9f8f
// Returns the effect of the timed entry at an index, or NULL past the end
EESMEffect* EEScenarioManager::GetTimedEffect(unsigned long inIndex)
{
    EESMEffect* theEffect = NULL;
    if (inIndex < this->mTimedEffects.size())
        theEffect = this->mEffects[this->mTimedEffects[inIndex].mEffectIndex];
    return theEffect;
}

// FUNCTION: EXE 0x5b9fcf
// Maps an index through an old-to-new index map; an index the map lacks becomes 0
void EERemapID(unsigned long& ioID, EESMIDMap& inMap)
{
    if (inMap.find(ioID) == inMap.end())
        ioID = 0;
    else
        ioID = inMap[ioID];
}

// TEMPLATE: EXE 0x5ba01a
// ??H?$_Ptrit@UEESMTimedEffect@@HPAU1@AAU1@PAU1@AAU1@@std@@QBE?AV01@H@Z (Dinkum COMDAT)
