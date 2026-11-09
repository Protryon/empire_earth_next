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
#include "ee/EESMIDMap.h"               // (the remap slots +0x20..+0x40)

class EESMArea;
class FSFileSpec;
class EESMCondition;
class EESMEffect;
class EESMObject;
class EESMTrigger;
// the +0x164 vector's 0xc-byte elements (_Buy 0x150afb; +0 indexes mEffects; 0x1b005b builds one and stable-sorts the
// vector by mTime; provisional names)
struct EESMTimedEffect {
    EESMTimedEffect(long inEffect, unsigned long inTime, bool inIsInCinematic) { this->mEffectIndex = inEffect; this->mTime = inTime; this->mIsInCinematic = inIsInCinematic; this->mHasRun = false; }
    unsigned long mEffectIndex;             // +0x00 (an mEffects index; Load/Save: unsigned long)
    unsigned long mTime;                    // +0x04 (the sort key)
    bool mIsInCinematic;                    // +0x08
    bool mHasRun;                           // +0x09
};

// One element of the +0xd4 vector (8 bytes): a player and a unit class whose media the scenario loaded; the dtor
// 0x1afe66 and 0x1b28c0 unload each through gWorld.mPlayers[mPlayer]->mUnitClassUpgrades[mUnitClass] (0x23c24f); 0x1b2b43 appends
// one (provisional names)
struct EESMUnitClassMedia {
    long mPlayer;                           // +0x00
    long mUnitClass;                        // +0x04
};

class EEScenarioManager : public FSPersistent {   // RTTI base; vtable 0x445fd8, sizeof 0x23c
public:
    EEScenarioManager();                    // 0x1afa90
    // FSPersistent's slots (vtable 0x445fd8; EEPTPRestorable<EEScenarioManager> in Empire Earth.cpp needs the class
    // concrete)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x04 0x1b404a
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x0c 0x1b4d1a
    virtual ~EEScenarioManager();           // +0x10 (??_G 0x1afb95)
    virtual FSPersistent* CreateDefault() const;   // +0x14 0x1b0982
    // own slots +0x1c..+0x48 (after FSPersistent::GetTagID +0x18): broadcast hooks that
    // forward to the element classes' remap (EESMIDMap&) / renumber (old, new) slots. Names by offset (provisional
    // name).
    virtual void Prepare();                 // +0x1c 0x1b28c0 (unloads mUnitClassMedia's media, reloads the effects' media)
    virtual void RemapAreas(EESMIDMap& inMap);  // +0x20 0x1b313b (mEffects/mObjects/mPendingEffects +0x08)
    virtual void RenumberAreas(unsigned long inOld, unsigned long inNew);  // +0x24 0x1b3084 (mEffects/mObjects/mPendingEffects +0x0c)
    virtual void RemapConditions(EESMIDMap& inMap);  // +0x28 0x1b2d21 (mTriggers +0x04)
    virtual void RenumberConditions(unsigned long inOld, unsigned long inNew);  // +0x2c 0x1b2cdc (mTriggers +0x08)
    virtual void RemapEffects(EESMIDMap& inMap);  // +0x30 0x1b2e33 (mTriggers +0x0c, mTimedEffects ids)
    virtual void RenumberEffects(unsigned long inOld, unsigned long inNew);  // +0x34 0x1b2d62 (mTriggers +0x10, mTimedEffects)
    virtual void RemapObjects(EESMIDMap& inMap);  // +0x38 0x1b2fa1 (conditions +0x04, effects/objects/mPendingEffects +0x10)
    virtual void RenumberObjects(unsigned long inOld, unsigned long inNew);  // +0x3c 0x1b2eb2
    virtual void RemapTriggers(EESMIDMap& inMap);  // +0x40 0x1b2c2f (conditions +0x0c, effects/mPendingEffects +0x18)
    virtual void RenumberTriggers(unsigned long inOld, unsigned long inNew);  // +0x44 0x1b2b78
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
    void GetAreaNames(std::vector<UWideString>& outNames);
    void GetObjectNames(std::vector<UWideString>& outNames);
    void GetTriggerNames(std::vector<UWideString>& outNames);   // 0x1b3a2a (the +0xe8 entries' names; EESETSCTTriggerAttribute; provisional name)
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
    void SortConditions();                  // 0x1b01a6 (EESETSCondition::Sort)
    EESMCondition* NewCondition();          // 0x1b07d2 (EESETSCondition::New; returns the new condition)
    void MoveConditionDown(unsigned long inIndex);  // 0x1b0c45 (Virtual0C)
    bool DeleteCondition(unsigned long inIndex);  // 0x1b0f3f (Virtual10)
    EESMCondition* DuplicateCondition(unsigned long inIndex);  // 0x1b1358 (Virtual18; duplicates condition inIndex)
    void GetUserConditionNames(std::vector<UWideString>& outNames);  // 0x1b3656 (Virtual1C)
    void MoveConditionUp(unsigned long inIndex);  // 0x1b3ea9 (Virtual30)
    // the effect screen's slots (provisional names by address)
    void SortEffects();                     // 0x1b02f6 (EESETSEffect::Sort)
    EESMEffect* NewEffect();                // 0x1b099c (EESETSEffect::New; returns the new effect)
    void MoveEffectDown(unsigned long inIndex);  // 0x1b0cde (EESETSEffect::MoveDown)
    bool DeleteEffect(unsigned long inIndex);   // 0x1b0fb5 (EESETSEffect::Delete; deletes effect inIndex)
    EESMEffect* DuplicateEffect(unsigned long inIndex);  // 0x1b15a7 (EESETSEffect::Duplicate; duplicates effect inIndex)
    void GetUserEffectNames(std::vector<UWideString>& outNames);    // 0x1b3726 (EESETSEffect::GetNames)
    void MoveEffectUp(unsigned long inIndex);   // 0x1b3edd (EESETSEffect::MoveUp)

    // EEScenarioManager.cpp's own members (provisional names unless stated)
    void Reset();                           // 0x1afbb1 (the ctor and Load 0x1b4081)
    void AddMessage(const EESMDialogMessage& inMessage);    // 0x1afffb
    void AddV134Effect(const EESMEffect& inEffect);   // 0x1b000d (not while mB234)
    void AddTimedEffect(long inEffect, unsigned long inTime, bool inIsInCinematic);  // 0x1b005b (EESMTrigger 0x1c9c84)
    void SortAreas();                       // 0x1b00c1 (by name, after the two default areas)
    void SortObjects();                     // 0x1b041c (and the object conditions)
    void SortTriggers();                    // 0x1b0674
    EESMArea* NewArea();                    // 0x1b074c ("<Area %d>")
    EESMObject* NewObject();                // 0x1b0a28 (and its object condition)
    EESMTrigger* NewTrigger();              // 0x1b0b65 ("<Trigger %d>")
    void MoveAreaDown(unsigned long inIndex);   // 0x1b0beb
    void MoveObjectDown(unsigned long inIndex); // 0x1b0d37
    void MoveTriggerDown(unsigned long inIndex);    // 0x1b0e71
    bool DeleteArea(unsigned long inIndex);     // 0x1b0ec6
    bool DeleteObject(unsigned long inIndex);   // 0x1b1033
    bool DeleteTrigger(unsigned long inIndex);  // 0x1b10ed
    void RemoveTimedEffects(long inEffect);  // 0x1b115a
    void EraseTimedEffect(unsigned long inIndex);  // 0x1b11d1
    EESMArea* DuplicateArea(unsigned long inIndex);     // 0x1b120d
    EESMObject* DuplicateObject(unsigned long inIndex); // 0x1b1719
    EESMTrigger* DuplicateTrigger(unsigned long inIndex);   // 0x1b1978
    // 0x1b260a: a scenario player choice (0 nature, 1/3 humans, 2/4 computers, 5/6 random, 7.. player n - 6) as one
    // player (the result, -1 if none) and the candidate list (provisional name)
    long ChoosePlayer(long inType, std::vector<long>* outPlayers, bool inSingle);
    EESMCondition* FindConditionByID(long inID);    // 0x1b322d (mID)
    EESMEffect* FindEffectByID(long inID);  // 0x1b3269 (mID)
    EESMObject* FindObjectByID(long inID);  // 0x1b32a8 (mL64)
    long GetObjectIndex(EESMObject* inObject);  // 0x1b32e4 (-1 if none; EESMObject::SetName)
    // the editor's name lists ("[id]" prefixes while mShowDebugIDs; provisional names)
    void GetUserAreaNames(std::vector<UWideString>& outNames);  // 0x1b3317 (from area 2)
    void GetAllConditionNames(std::vector<UWideString>& outNames);  // 0x1b3496 (renames the object conditions)
    void GetAllEffectNames(std::vector<UWideString>& outNames); // 0x1b37e9
    void GetUserObjectNames(std::vector<UWideString>& outNames);    // 0x1b38ab (from object 1)
    void MoveAreaUp(unsigned long inIndex);     // 0x1b3e75 (EESETSArea 0x17d1f3)
    void MoveObjectUp(unsigned long inIndex);   // 0x1b3f11
    void MoveTriggerUp(unsigned long inIndex);  // 0x1b4017
    void SetTimedEffectHasRun(EESMEffect* inEffect, bool inFlag);  // 0x1b45cb (EESMEffect 0x1bd860)
    // the trigger import's line parsers (0x1b4a95 / 0x1b52bb call them; 9 when the key is missing; provisional names)
    long ParseConditionID(const UString& inLine, unsigned long inKind, long* outID, UWideString& outRest);   // 0x1b463b
    long ParseEffectID(const UString& inLine, unsigned long inKind, long* outID, UWideString& outRest);  // 0x1b46bf
    long ParsePlayerName(const UString& inLine, unsigned long inKind, long* outID, UWideString& outRest);    // 0x1b4796
    EESMEffect* GetTimedEffect(unsigned long inIndex);     // 0x1b9f8f (mEffects[mTimedEffects[i].mEffectIndex], 0 past the end)
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
    std::vector<EESMTimedEffect> mTimedEffects;  // +0x164
    EESMMediaStrings mMediaStrings;         // +0x174 (sizeof 0x8c)
    long mGameResult;                       // +0x200
    float mSavedZoom;                       // +0x204 (the camera zoom StartCinematic saves; fld/fstp)
    float mStartX;                          // +0x208 (saved)
    float mStartY;                          // +0x20c
    float mTime;                            // +0x210
    float mTriggerTimer;                    // +0x214
    float mCinematicStartTime;              // +0x218 (Reset: fldz)
    long mSavedCameraMode;                  // +0x21c
    unsigned long mNextTriggerID;           // +0x220 (saved)
    unsigned long mNextConditionID;         // +0x224
    unsigned long mNextEffectID;            // +0x228
    unsigned long mNextObjectID;            // +0x22c
    unsigned long mNextAreaID;              // +0x230
    bool mB234;                             // +0x234
    bool mIsMovingCamera;                   // +0x235 (saved)
    bool mShowDebugIDs;                     // +0x236 (saved; EESEEShowDebugIDs toggles it; provisional name)
    bool mIsShowingDialog;                  // +0x237
    bool mIsEndingCinematic;                // +0x238 (Reset: and byte)
};

#endif
