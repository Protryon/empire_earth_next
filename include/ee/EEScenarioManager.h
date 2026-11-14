// C:\EEX\Empire Earth\Scenario Manager\EEScenarioManager.h (RECONSTRUCTION; file name inferred). The scenario
// manager the world owns (gWorld.mScenarioManager, +0xb24; new 0x23c bytes, ctor 0x1afa90 at 0x1a5940). Only
// what decompiled units call so far; provisional names. Layout not decoded yet: no
// members declared. Layout
// below. Requires <vector>, lle/UWideString.h, ee/EECameraKey.h, ee/EEAreaUtilities.h, ee/EESMMediaStrings.h.
#ifndef EE_EESCENARIOMANAGER_H
#define EE_EESCENARIOMANAGER_H

#include "lle/FSRestorable.h"
#include "lle/FSPersistent.h"
#include "ee/EECameraKey.h"          // (the layout below needs the complete types)
#include "ee/EESMMediaStrings.h"

#include "ee/EESMDialogMessage.h"
#include "rust/ee_scenario.h"           // EeScenarioManagerFields, the timed effects' queue

class EESMArea;
class FSFileSpec;
class EESMCondition;
class EESMEffect;
class EESMObject;
class EESMTrigger;
// One element of the +0xd4 vector (8 bytes): a player and a unit class whose media the scenario loaded; the dtor
// 0x1afe66 and 0x1b28c0 unload each through gWorld.mPlayers[mPlayer]->mUnitClassUpgrades[mUnitClass] (0x23c24f); 0x1b2b43 appends
// one (provisional names)
struct EESMUnitClassMedia {
    long mPlayer;                           // +0x00
    long mUnitClass;                        // +0x04
};

// TEST ONLY (EE-SIM --scenario): the driver ends each shown dialog message at the next communications tick (as a
// player who reads every one through), and the UI's end of a message (EEPhysics' message 0x508) is ignored: the
// scenario's dialogs, and the cinematics that wait for them, then follow the game's ticks, not the wall clock
extern bool gScenarioSimDialogs;

// The numbers (+0x200..+0x238: the clocks, id counters, cinematic and dialog flags) are the Rust record
// EeScenarioManagerFields (ee_scenario manager.rs), with the members' names and C++ types
class EEScenarioManager : public FSPersistent, public EeScenarioManagerFields {   // RTTI base; vtable 0x445fd8, sizeof 0x23c
public:
    EEScenarioManager();                    // 0x1afa90
    // FSPersistent's slots (vtable 0x445fd8; EEPTPRestorable<EEScenarioManager> in Empire Earth.cpp needs the class
    // concrete)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x04 0x1b404a
    long LoadUncaptured(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);  // (+ SMSV)
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x0c 0x1b4d1a
    virtual ~EEScenarioManager();           // +0x10 (??_G 0x1afb95)
    virtual FSPersistent* CreateDefault() const;   // +0x14 0x1b0982
    // own slots +0x1c..+0x48 (after FSPersistent::GetTagID +0x18); the remap and renumber slots +0x20..+0x44 are
    // ee_scenario lists.rs
    virtual void Prepare();                 // +0x1c 0x1b28c0 (unloads mUnitClassMedia's media, reloads the effects' media)
    virtual void ReplaceUnit(long inOldUnit, long inNewUnit);  // +0x48 0x1b31e8 (mObjects +0x18; EEPhysics 0x153430)
    void EndCinematic();                    // 0x1b1ae4 (no arguments; EEMEndCinematic::Execute 0xbdf7d)
    void ShowDialogMessage();               // 0x1b481a (no arguments, `ret`; EEPhysics::HandleMessage's message 0x508; provisional name)
    // the cinematic family (provisional names)
    void ExitCinematic();                   // 0x1b1e47 (Update, once the end-of-cinematic fade is done)
    void StartCinematic();                  // 0x1b1eee (EESMEffect 0x1bd86c)
    void Update();                          // 0x1b20e6 (EEPhysics 0x153265: the per-tick clock and trigger update)
    long ExportText(FSFileSpec& inFile);    // 0x1b23dc (EESEIAExport 0x16ffd0; `ret 4`)
    void MoveCameraToStart();               // 0x1b5173 (Update, while mIsMovingCamera; `ret`)
    // the trigger text import (EESEIAImport 0x170439; provisional names; `ret 0xc` each)
    long ImportTriggerText(FSFileSpec& inFile, unsigned long inKind, bool inValidate);   // 0x1b3ae9
    long ImportText(const std::vector<UString>& inLines, unsigned long inKind, const std::vector<UWideString>& inTexts);  // 0x1b4a95
    long ValidateText(std::vector<UString>& inLines, unsigned long inKind, const std::vector<UWideString>& inTexts);    // 0x1b52bb (errors into mImportErrors)
    // 0x1b33d7 / 0x1b396b: fill outNames with the scenario's area / object names (EESEEvents' combo helper;
    // provisional names)
    // inline (COMDAT 0x17cadf, emitted by EESETriggerScreen.cpp): condition inIndex, 0 past the end
    EESMCondition* GetCondition(unsigned long inIndex)
    {
        EESMCondition* theCondition = NULL;
        if (inIndex < this->mConditions.size())
            theCondition = this->mConditions[inIndex];
        return theCondition;
    }
    // inline (COMDAT 0x182d82, emitted by EESETSEffect.cpp; GetCondition's shape): effect inIndex, 0 past the end
    EESMEffect* GetEffect(unsigned long inIndex)
    {
        EESMEffect* theEffect = NULL;
        if (inIndex < this->mEffects.size())
            theEffect = this->mEffects[inIndex];
        return theEffect;
    }
    // the condition screen's slots call these with its +0x04 + 1 (provisional names by address)
    // the effect screen's slots (provisional names by address)

    // EEScenarioManager.cpp's own members (provisional names unless stated)
    void Reset();                           // 0x1afbb1 (the ctor and Load 0x1b4081)
    // the editor's list operations and name lists (EE_SCENARIO_LIST_*, EE_SCENARIO_NAMES_*; ee_scenario lists.rs)
    void SortList(unsigned long inList);
    void* NewEntry(unsigned long inList);
    void MoveEntry(unsigned long inList, unsigned long inIndex, bool inUp);
    bool DeleteEntry(unsigned long inList, unsigned long inIndex);
    void* DuplicateEntry(unsigned long inList, unsigned long inIndex);
    void GetNames(unsigned long inWhich, std::vector<UWideString>& outNames);
    void AddMessage(const EESMDialogMessage& inMessage);    // 0x1afffb
    void AddV134Effect(const EESMEffect& inEffect);   // 0x1b000d (not while mB234)
    void AddTimedEffect(long inEffect, unsigned long inTime, bool inIsInCinematic);  // 0x1b005b (EESMTrigger 0x1c9c84)
    // 0x1b260a: a scenario player choice (0 nature, 1/3 humans, 2/4 computers, 5/6 random, 7.. player n - 6) as one
    // player (the result, -1 if none) and the candidate list (provisional name)
    long ChoosePlayer(long inType, std::vector<long>* outPlayers, bool inSingle);
    EESMCondition* FindConditionByID(long inID);    // 0x1b322d (mID)
    EESMEffect* FindEffectByID(long inID);  // 0x1b3269 (mID)
    EESMObject* FindObjectByID(long inID);  // 0x1b32a8 (mL64)
    long GetObjectIndex(EESMObject* inObject);  // 0x1b32e4 (-1 if none; EESMObject::SetName)
    // the editor's name lists ("[id]" prefixes while mShowDebugIDs; provisional names)
    void SetTimedEffectHasRun(EESMEffect* inEffect, bool inFlag);  // 0x1b45cb (EESMEffect 0x1bd860)
    // the trigger import's line parsers (0x1b4a95 / 0x1b52bb call them; 9 when the key is missing; provisional names)
    long ParseConditionID(const UString& inLine, unsigned long inKind, long* outID, UWideString& outRest);   // 0x1b463b
    long ParseEffectID(const UString& inLine, unsigned long inKind, long* outID, UWideString& outRest);  // 0x1b46bf
    long ParsePlayerName(const UString& inLine, unsigned long inKind, long* outID, UWideString& outRest);    // 0x1b4796
    EESMEffect* GetTimedEffect(unsigned long inIndex);     // 0x1b9f8f (mEffects[mTimedEffects[i].mEffectIndex], 0 past the end)
    unsigned long GetNumTimedEffects() { return (unsigned long)ee_scenario_timed_count(this->mTimedEffects); }
    EeScenarioTimedEntry GetTimedEntry(unsigned long inIndex) { return ee_scenario_timed_at(this->mTimedEffects, inIndex); }
    void ApplyPendingEffects();             // 0x1b286d (EEWorld 0x13a0e6; runs every mPendingEffects effect, clears mB234)
    // inline (COMDATs 0x17da08 / 0x188edd / 0x18ef8a, emitted by the editor's area / object / trigger screens;
    // GetCondition's shape; provisional names)
    EESMArea* GetArea(unsigned long inIndex)
    {
        EESMArea* theArea = NULL;
        if (inIndex < this->mAreas.size())
            theArea = this->mAreas[inIndex];
        return theArea;
    }
    EESMObject* GetScenarioObject(unsigned long inIndex)
    {
        EESMObject* theObject = NULL;
        if (inIndex < this->mObjects.size())
            theObject = this->mObjects[inIndex];
        return theObject;
    }
    EESMTrigger* GetTrigger(unsigned long inIndex)
    {
        EESMTrigger* theTrigger = NULL;
        if (inIndex < this->mTriggers.size())
            theTrigger = this->mTriggers[inIndex];
        return theTrigger;
    }
    // inline (out-of-line COMDAT 0x1c5152, emitted by EESMEffect.cpp: Execute's inline budget refuses it)
    // (provisional name)
    unsigned long GetNumMessages() { return this->mMessages.size(); }
    unsigned long GetNumObjects() { return this->mObjects.size(); }  // (0x59ccc0, 0x5b1ae4)
    unsigned long GetNumConditions() { return this->mConditions.size(); }
    unsigned long GetNumEffects() { return this->mEffects.size(); }
    unsigned long GetNumTriggers() { return this->mTriggers.size(); }
    // Layout: the ctor 0x1afa90 (member ctors in order) and the Save/Load field census of
    // Vector element types are unproven unless stated (4-byte elements: `_Buy` ICF-folded with vector<long>'s);
    // member names by offset.
    EECameraKey mK008;                      // +0x008 (ctor 0x12883b)
    UWideString mExportFileName;            // +0x0ac
    UWideString mLastChatText;              // +0x0c0
    std::vector<EESMUnitClassMedia> mUnitClassMedia;  // +0x0d4 (_Buy 0x14823b, ICF)
    std::vector<EESMTrigger*> mTriggers;    // +0x0e4 (Reset 0x1afbb1: deleted through the dtor 0x1c98dd)
    std::vector<EESMCondition*> mConditions;    // +0x0f4 (GetCondition)
    std::vector<EESMEffect*> mEffects;      // +0x104 (GetEffect; 0x1b9f8f returns mEffects[mTimedEffects[i] +0])
    std::vector<EESMObject*> mObjects;      // +0x114 (virtual delete; Reset's default object 0x32c9)
    std::vector<EESMArea*> mAreas;          // +0x124 (Reset's two default areas 0x32ce/0x32cf)
    std::vector<EESMEffect*> mPendingEffects;  // +0x134 (0x1b000d appends new effects)
    EESMDialogMessage& GetDialogMessage(unsigned long inIndex) { return this->mMessages[inIndex]; }
    std::vector<EESMDialogMessage> mMessages;    // +0x144 (0x40-byte elements by value)
    std::vector<UWideString> mImportErrors;  // +0x154 (_Buy 0x6ba94)
    EeScenarioTimedQueue* mTimedEffects;    // +0x164 (the timed effects, sorted by time: owned by the Rust queue)
    EESMMediaStrings mMediaStrings;         // +0x174 (sizeof 0x8c)
    // +0x200..+0x238: EeScenarioManagerFields (mGameResult, mSavedZoom, mStartX, mStartY, mTime, mTriggerTimer,
    // mCinematicStartTime, mSavedCameraMode, mNext{Trigger,Condition,Effect,Object,Area}ID, mB234, mIsMovingCamera,
    // mShowDebugIDs, mIsShowingDialog, mIsEndingCinematic)
};

#endif
