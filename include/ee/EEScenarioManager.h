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

class EESMArea;
class FSFileSpec;
class EESMCondition;
class EESMEffect;
class EESMObject;
class EESMTrigger;
// One element of the +0xd4 vector (8 bytes): a player and a unit class whose media the scenario loaded; the dtor
// 0x1afe66 and 0x1b28c0 unload each through gWorld.mPlayers[mPlayer]->mUnitClassUpgrades[mUnitClass] (0x23c24f); 0x1b2b43 appends
// one (provisional names; the record is Rust's, ee_scenario manager.rs)
typedef EeScenarioUnitClassMedia EESMUnitClassMedia;

// element inIndex of a list, NULL past the end (the inline getters below, each the original's own COMDAT of this shape)
template<class T> T* EESMAt(const std::vector<T*>& inList, unsigned long inIndex) { return inIndex < inList.size() ? inList[inIndex] : NULL; }

// TEST ONLY (EE-SIM --scenario): the driver ends each shown dialog message at the next communications tick (as a
// player who reads every one through), and the UI's end of a message (EEPhysics' message 0x508) is ignored: the
// scenario's dialogs, and the cinematics that wait for them, then follow the game's ticks, not the wall clock
extern bool gScenarioSimDialogs;

// The numbers (+0x200..+0x238: the clocks, id counters, cinematic and dialog flags) are the Rust record
// EeScenarioManagerFields (ee_scenario manager.rs), with the members' names and C++ types
class EEScenarioManager : public FSPersistent, public EeScenarioManagerFields {   // RTTI base; vtable 0x445fd8, sizeof 0x23c
public:
    EEScenarioManager();
    // FSPersistent's slots (vtable 0x445fd8; EEPTPRestorable<EEScenarioManager> in Empire Earth.cpp needs the class
    // concrete)
    // Resets, then reads triggers, conditions, effects, objects, areas, timed and pending effects (dropped when loading
    // a scenario), ID counters, clocks, media strings, start camera and dialog messages; returns the first error
    // (after Reset, ee_scenario manager.rs load over the stream, the elements created and kept here: EEScenarioHost.cpp's
    // stream callback; entry.rs adds the simulation capture's SMSV record)
    // +0x04 0x1b404a
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile) { return ee_scenario_en_manager_load(this, &inFile, inMajorVersion, inMinorVersion); }
    // Writes triggers, conditions, effects, objects and areas (not the built-ins), timed and pending effects, ID
    // counters, clocks, media strings, start camera and dialog messages; returns the first write error (ee_scenario
    // manager.rs save over the stream)
    // +0x0c 0x1b4d1a
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile) { return ee_scenario_en_manager_save(this, &outFile, inMajorVersion, inMinorVersion); }
    // Destroys the scenario: unloads the unit class media Prepare loaded and deletes every trigger, condition, effect,
    // object, area and pending effect
    // +0x10 (??_G 0x1afb95)
    virtual ~EEScenarioManager() { ee_scenario_en_round(this, EE_SCENARIO_MN_DESTROY, NULL, 0, 0); }
    // Creates a new, default scenario manager (the persistent object factory)
    // +0x14 0x1b0982
    virtual FSPersistent* CreateDefault() const { return new EEScenarioManager; }
    // own slots +0x1c..+0x48 (after FSPersistent::GetTagID +0x18); the remap and renumber slots +0x20..+0x44 are
    // ee_scenario lists.rs
    // Unloads the unit class media loaded before, then preloads the media of each unit class a Create Object effect
    // makes for one fixed player (objects not selected on the map; which: ee_scenario lists.rs prepare_media)
    // +0x1c 0x1b28c0 (unloads mUnitClassMedia's media, reloads the effects' media)
    virtual void Prepare() { ee_scenario_en_manager(this, EE_SCENARIO_MN_PREPARE); }
    // Replaces a unit ID with another in every scenario object
    // +0x48 0x1b31e8 (mObjects +0x18; EEPhysics 0x153430)
    virtual void ReplaceUnit(long inOldUnit, long inNewUnit) { ee_scenario_en_round(this, EE_SCENARIO_MN_REPLACE_UNIT, NULL, inOldUnit, inNewUnit); }
    // Shows the next queued dialog message (in a cinematic the next cinematic one), stopping one-shot sounds for a
    // voiced one and moving the camera to its unit; none left: hides the messages; nothing while paused (ee_scenario
    // round.rs show_dialog)
    // 0x1b481a (no arguments, `ret`; EEPhysics::HandleMessage's message 0x508; provisional name)
    void ShowDialogMessage() { ee_scenario_en_manager(this, EE_SCENARIO_MN_SHOW_DIALOG); }
    // the cinematic family (provisional names)
    // Runs a scenario tick (EEPhysics 0x153265): shows queued dialog, advances the clocks (tick / game speed in a
    // cinematic); while a human plays, every 500 ms evaluates the triggers and runs the due timed effects (ee_scenario
    // round.rs update)
    // 0x1b20e6 (EEPhysics 0x153265: the per-tick clock and trigger update)
    void Update() { ee_scenario_en_manager(this, EE_SCENARIO_MN_UPDATE); }
    // 0x1b33d7 / 0x1b396b: fill outNames with the scenario's area / object names (EESEEvents' combo helper;
    // provisional names)
    // inline (COMDAT 0x17cadf, emitted by EESETriggerScreen.cpp): condition inIndex, 0 past the end
    EESMCondition* GetCondition(unsigned long inIndex) { return EESMAt(this->mConditions, inIndex); }
    // inline (COMDAT 0x182d82, emitted by EESETSEffect.cpp; GetCondition's shape): effect inIndex, 0 past the end
    EESMEffect* GetEffect(unsigned long inIndex) { return EESMAt(this->mEffects, inIndex); }
    // the condition screen's slots call these with its +0x04 + 1 (provisional names by address)
    // the effect screen's slots (provisional names by address)

    // EEScenarioManager.cpp's own members (provisional names unless stated)
    // Empties the scenario back to its defaults (an Always True condition, a Do Nothing effect, a None object, the
    // Off Map and Previous Location areas); zeroes the clocks and ID counters, clears the timed and pending effects
    // (ee_scenario entry.rs: the numbers and the timed effects, manager.rs reset; the lists and their built-ins, lists.rs
    // reset)
    // 0x1afbb1 (the ctor and Load 0x1b4081)
    void Reset() { ee_scenario_en_manager(this, EE_SCENARIO_MN_RESET); }
    void* NewEntry(unsigned long inList) { return (void*)(intptr_t)ee_scenario_en_list(this, EE_SCENARIO_LS_NEW, inList, 0, 0, NULL); }
    bool DeleteEntry(unsigned long inList, unsigned long inIndex) { return ee_scenario_en_list(this, EE_SCENARIO_LS_DELETE, inList, inIndex, 0, NULL) != 0; }
    void* DuplicateEntry(unsigned long inList, unsigned long inIndex) { return (void*)(intptr_t)ee_scenario_en_list(this, EE_SCENARIO_LS_DUPLICATE, inList, inIndex, 0, NULL); }
    // Schedules an effect (by index) to run at the given scenario time, keeping the timed effects sorted by time
    // 0x1b005b (EESMTrigger 0x1c9c84)
    void AddTimedEffect(long inEffect, unsigned long inTime, bool inIsInCinematic) { ee_scenario_timed_add(this->mTimedEffects, inEffect, inTime, inIsInCinematic); }
    // 0x1b260a: a scenario player choice (0 nature, 1/3 humans, 2/4 computers, 5/6 random, 7.. player n - 6) as one
    // player (the result, -1 if none) and the candidate list (provisional name)
    // 0x1b322d (mID)
    EESMCondition* FindConditionByID(long inID) { return (EESMCondition*)(intptr_t)ee_scenario_en_list(this, EE_SCENARIO_LS_FIND, EE_SCENARIO_LIST_CONDITIONS, inID, 0, NULL); }
    // 0x1b3269 (mID)
    EESMEffect* FindEffectByID(long inID) { return (EESMEffect*)(intptr_t)ee_scenario_en_list(this, EE_SCENARIO_LS_FIND, EE_SCENARIO_LIST_EFFECTS, inID, 0, NULL); }
    // 0x1b32a8 (mL64)
    EESMObject* FindObjectByID(long inID) { return (EESMObject*)(intptr_t)ee_scenario_en_list(this, EE_SCENARIO_LS_FIND, EE_SCENARIO_LIST_OBJECTS, inID, 0, NULL); }
    // the editor's name lists ("[id]" prefixes while mShowDebugIDs; provisional names)
    unsigned long GetNumTimedEffects() { return (unsigned long)ee_scenario_timed_count(this->mTimedEffects); }
    EeScenarioTimedEntry GetTimedEntry(unsigned long inIndex) { return ee_scenario_timed_at(this->mTimedEffects, inIndex); }
    // inline (COMDATs 0x17da08 / 0x188edd / 0x18ef8a, emitted by the editor's area / object / trigger screens;
    // GetCondition's shape; provisional names)
    EESMArea* GetArea(unsigned long inIndex) { return EESMAt(this->mAreas, inIndex); }
    EESMObject* GetScenarioObject(unsigned long inIndex) { return EESMAt(this->mObjects, inIndex); }
    EESMTrigger* GetTrigger(unsigned long inIndex) { return EESMAt(this->mTriggers, inIndex); }
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
    std::vector<EESMDialogMessage> mMessages;    // +0x144 (0x40-byte elements by value)
    std::vector<UWideString> mImportErrors;  // +0x154 (_Buy 0x6ba94)
    EeScenarioTimedQueue* mTimedEffects;    // +0x164 (the timed effects, sorted by time: owned by the Rust queue)
    EESMMediaStrings mMediaStrings;         // +0x174 (sizeof 0x8c)
    // +0x200..+0x238: EeScenarioManagerFields (mGameResult, mSavedZoom, mStartX, mStartY, mTime, mTriggerTimer,
    // mCinematicStartTime, mSavedCameraMode, mNext{Trigger,Condition,Effect,Object,Area}ID, mB234, mIsMovingCamera,
    // mShowDebugIDs, mIsShowingDialog, mIsEndingCinematic)
};

#endif
