// C:\EEX\Empire Earth\User Interface Thread\EEUIFMGMainGroup.cpp (__FILE__ 0x4cdc20, ESVERIFY lines 415/462). The
// main game form's main group (RTTI EEUIFMGMainGroup > EEUIFMGGroup; vtable 0x44ac84): 0x267ec8..0x2691b2 (ctor,
// Virtual10 (inline), ??_G, dtor, Virtual04, the selection members, EEUIEMGHotKeyStatusButton's ??_G after its first
// user, Virtual14, then the inline COMDATs EETechTreeNode::GetHotKey, EEComplexUnit::GetCitizenBuildTarget and
// EEButtonObject::GetHotKey; the two GetHotKeys are their classes' units' now: the command buttons named this unit
// for them). Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GERhombus.h"
#include "lle/Input System.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIHotKey.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "lle/UITextLabel.h"
#include "lle/UResourceFile.h"
#include "ee/EmpireEarth.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIFMGMainGroup.h"
#include "rust/game/screens.h"   // ee_screens_panel_*: the command panel's layout (ee_screens command_panel.rs)
#include "ee/EEUIManager.h"
#include "ee/EEButtonObject.h"
#include "ee/EEUnitBehavior.h"
#include "ee/EEFormation.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEPhysics.h"
#include "ee/EEUIShell.h"
#include "ee/EEUIFMGStatusBox.h"
#include "ee/EEUIFMGStatusButton.h"
#include "ee/EEUIEMGHotKeyStatusButton.h"

// Presses or releases a status button by hot key (pressing also resets the saved-group state). The original (0x6601da)
// lay among the main game form's events (EEUIEMainGameEvents.cpp, now Rust's: ee_screens main_game.rs); this one
// holds a C++ button, so it stays with the panel that makes it.
long EEUIEMGHotKeyStatusButton::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    ee_screens_mg_hot_key_event((uintptr_t)this);   // (main_group_calls.rs)
    return kErrorOK;
}

// Creates the main game form's main panel group with its status box, no unit shown
EEUIFMGMainGroup::EEUIFMGMainGroup(EEUIFMainGame* inOwner)
    : EEUIFMGGroup(inOwner), EeScreensMainGroupFields()
{
    ee_screens_main_group_fields(this, static_cast<EeScreensMainGroupFields*>(this));   // (the record zeroed)
    this->mStatusBox = new EEUIFMGStatusBox(inOwner);
}

// Destroys the main panel group and its status box
EEUIFMGMainGroup::~EEUIFMGMainGroup()
{
    delete this->mStatusBox;
}

// (EETechTreeNode::GetHotKey and EEButtonObject::GetHotKey, the inline COMDATs the original emitted here: their
// classes' units, Tech Tree/EETechTreeNode.cpp and Database/EEButtonObject.cpp)

// Returns the citizen's queued build target at an index
EEUnit* EEComplexUnit::GetCitizenBuildTarget(unsigned long inIndex)
{
    return this->mCitizenBuildQueue[inIndex];
}

// Returns the epoch the tech tree shows to the player (two past the start, at most the end; at least the Bronze
// Age's): the statistics group's "wonders available in" (inline in the original EEUIFMGGStatistics.h)
long EETechTree::GetShownEpoch() const
{
    long theEpoch = this->mStartEpoch + 2 > this->mEndEpoch ? this->mEndEpoch : this->mStartEpoch + 2;
    return theEpoch > kTechEpochBronze ? theEpoch : 3;
}

// The panel groups' host (ee_screens panel_groups.rs): a value (EE_SCREENS_GROUP_VALUE_*), or a label's text
// printed from the format with the resource strings[0] and strings[1] (when not -1) or strings[0] and the two values
extern "C" int32_t ee_screenscpp_group(void* inGroup, bool inPrint, uint32_t inWhich, uint32_t inControl,
                                       bool inIfChanged, const char* inFormat, const int32_t* inStrings,
                                       const int32_t* inValues)
{
    EEUIFMGGRust* theGroup = static_cast<EEUIFMGGRust*>(inGroup);
    if (!inPrint) {
        switch (inWhich) {
        case EE_SCREENS_GROUP_VALUE_CONVERSIONS:
            return gWorld.GetCurrentPlayer()->mSavedGroups.GetLastValue(5);
        case EE_SCREENS_GROUP_VALUE_CONVERSIONS_MAX:
            return gWorldTable.GetLongValue(kWorldConversionsStatisticMax);
        case EE_SCREENS_GROUP_VALUE_WONDERS_TO_WIN:
            return gWorld.mNumberOfWondersToWin;
        case EE_SCREENS_GROUP_VALUE_EPOCH_TITLE:
            return EEEpochTitleID(gWorld.GetCurrentPlayer()->GetTechTree()->GetShownEpoch());
        case EE_SCREENS_GROUP_VALUE_HAS_PLAYER:
            return gWorld.GetCurrentPlayer() != NULL;
        }
        return 0;
    }
    UWideString theText, theFirst, theSecond;
    gResource->GetString(inStrings[0], theFirst);
    if (inStrings[1] != -1) {
        gResource->GetString(inStrings[1], theSecond);
        sprintf(theText, inFormat, (const char*)theFirst, (const char*)theSecond);
    } else {
        sprintf(theText, inFormat, (const char*)theFirst, (long)inValues[0], (long)inValues[1]);
    }
    UITextLabel* theLabel = static_cast<UITextLabel*>(theGroup->mOwner->GetControl(inControl));
    if (!inIfChanged || theLabel->GetText(0) != theText)
        theLabel->SetText(theText);
    return 0;
}

// ---- the one-statement shims of ee_screens main_group_calls.rs (UI lane round 8: the group's host, ShowCommands,
// the status buttons' hot keys): each one call, no logic
#include "ee/EEScreensShims.h"
#define MG(x) reinterpret_cast<EEUIFMGMainGroup*>(x)
#define MGU(x) reinterpret_cast<EEUnit*>(x)
#define MGC(x) reinterpret_cast<EEComplexUnit*>(x)
#define MGP(x) reinterpret_cast<EEPlayer*>(x)
#define MGK(x) reinterpret_cast<UIHotKey*>(x)
#define SBN(x) reinterpret_cast<EETechTreeNode*>(x)
#define SBB(x) reinterpret_cast<EEUIFMGStatusButton*>(x)
// the status box's control slots EE_SCREENS_MGR_OP_BOX_BIND binds (main_group.rs' order)
static void* EEUIFMGStatusBox::* const kMGBoxSlots[8] = {
    &EEUIFMGStatusBox::mC8C, &EEUIFMGStatusBox::mC94, &EEUIFMGStatusBox::mC90, &EEUIFMGStatusBox::mC84,
    &EEUIFMGStatusBox::mC88, &EEUIFMGStatusBox::mC98, &EEUIFMGStatusBox::mCAC, &EEUIFMGStatusBox::mCA8};
extern "C" uintptr_t ee_screenscpp_mg_owner(uintptr_t inGroup) { return (uintptr_t)MG(inGroup)->mOwner; }
extern "C" uintptr_t ee_screenscpp_mg_main_form() { return (uintptr_t)gUIManager->GetMainGameForm(); }
extern "C" void ee_screenscpp_mg_remove_hot_keys(uintptr_t inGroup)
{ MG(inGroup)->mOwner->RemoveAllHotKeysForAGroup(2); }
extern "C" void ee_screenscpp_mg_hide(uintptr_t inGroup, uint32_t inControl)
{ MG(inGroup)->mOwner->GetControl(inControl)->SetDisplayMode(kDisplayModeHidden); }
extern "C" void ee_screenscpp_mg_show(uintptr_t inGroup, uint32_t inControl)
{ MG(inGroup)->mOwner->GetControl(inControl)->SetDisplayMode(kDisplayModeShown); }
extern "C" uintptr_t ee_screenscpp_mg_button(uintptr_t inGroup, uint32_t inControl)
{ return (uintptr_t)static_cast<EEUIFMGStatusButton*>(MG(inGroup)->mOwner->GetControl(inControl)); }
extern "C" uintptr_t ee_screenscpp_mg_control(uintptr_t inGroup, uint32_t inControl)
{ return (uintptr_t)MG(inGroup)->mOwner->GetControl(inControl); }
extern "C" bool ee_screenscpp_mg_cancel_hidden(uintptr_t inGroup)
{ return MG(inGroup)->mOwner->GetControl(kMainGameCancelUIMode)->GetDisplayMode() == kDisplayModeHidden; }
extern "C" int64_t ee_screenscpp_mg_mode(uintptr_t inGroup) { return MG(inGroup)->mMode; }
extern "C" uintptr_t ee_screenscpp_mg_unit_complex(uintptr_t inUnit) { return (uintptr_t)MGU(inUnit)->mComplexUnit; }
extern "C" bool ee_screenscpp_mg_unit_alive(uintptr_t inUnit) { return MGU(inUnit)->mHitPoints != 0; }
extern "C" uintptr_t ee_screenscpp_mg_unit_player(uintptr_t inUnit) { return (uintptr_t)MGU(inUnit)->mPlayer; }
extern "C" bool ee_screenscpp_mg_unit_in_fanatic_set(uintptr_t inUnit)
{ return gUnitSetManager.GetSet(0x2f)->Contains(MGU(inUnit)->mUnitClass->mUnitType); }
extern "C" uint32_t ee_screenscpp_mg_unit_class_type(uintptr_t inUnit)
{ return (uint32_t)MGU(inUnit)->mUnitClass->mUnitType; }
extern "C" uint32_t ee_screenscpp_mg_unit_id(uintptr_t inUnit) { return (uint32_t)MGU(inUnit)->mUnitID; }
extern "C" uint64_t ee_screenscpp_mg_class_command(uintptr_t inUnit, uint32_t inIndex)
{ return MGU(inUnit)->mUnitClass->mCommands[inIndex]; }
extern "C" int64_t ee_screenscpp_mg_research_count(uintptr_t inUnit)
{ return MGU(inUnit)->mUnitClass->GetNumResearchTechnologies(); }
extern "C" int64_t ee_screenscpp_mg_research_tech(uintptr_t inUnit, int64_t inIndex)
{ return MGU(inUnit)->mUnitClass->mResearchTechnologies[inIndex]; }
extern "C" bool ee_screenscpp_mg_complex_built(uintptr_t inComplex) { return MGC(inComplex)->mIsBuilt; }
extern "C" bool ee_screenscpp_mg_complex_commandable(uintptr_t inComplex) { return MGC(inComplex)->mIsCommandable; }
extern "C" bool ee_screenscpp_mg_complex_rally(uintptr_t inComplex)
{ return MGC(inComplex)->mUnitClass->HasRallyPoint(); }
extern "C" bool ee_screenscpp_mg_complex_airport(uintptr_t inComplex)
{ return MGC(inComplex)->mUnitClass->mUnitType == kUnitTypeAirport; }
extern "C" bool ee_screenscpp_mg_research_queued(uintptr_t inComplex, uintptr_t inNode)
{ return MGC(inComplex)->IsResearchQueued(SBN(inNode)->mData->mID); }
extern "C" uintptr_t ee_screenscpp_mg_current_player() { return (uintptr_t)gWorld.GetCurrentPlayer(); }
extern "C" bool ee_screenscpp_mg_player_observer(uintptr_t inPlayer) { return MGP(inPlayer)->mObserverMode; }
extern "C" bool ee_screenscpp_mg_player_human(uintptr_t inPlayer) { return MGP(inPlayer)->IsHuman() == true; }
extern "C" bool ee_screenscpp_mg_player_fanaticism(uintptr_t inPlayer)
{ return gCivPowersManager.HasPower(MGP(inPlayer), kCivPowerFanaticism); }
extern "C" bool ee_screenscpp_mg_player_jit(uintptr_t inPlayer)
{ return gCivPowersManager.HasPower(MGP(inPlayer), kCivPowerJustInTimeManufacturing); }
extern "C" bool ee_screenscpp_mg_player_jit_on(uintptr_t inPlayer) { return MGP(inPlayer)->mJITManufacturing; }
extern "C" uintptr_t ee_screenscpp_mg_player_node(uintptr_t inPlayer, int64_t inTechnology)
{ return (uintptr_t)MGP(inPlayer)->GetTechTree()->Get(inTechnology); }
extern "C" uintptr_t ee_screenscpp_mg_button_object(uint32_t inIndex)
{ return (uintptr_t)EEButtonObject::Get(inIndex); }
extern "C" int64_t ee_screenscpp_mg_object_position(uintptr_t inObject)
{ return reinterpret_cast<EEButtonObject*>(inObject)->mPosition; }
extern "C" int64_t ee_screenscpp_mg_object_id(uintptr_t inObject)
{ return reinterpret_cast<EEButtonObject*>(inObject)->mID; }
extern "C" uintptr_t ee_screenscpp_mg_node_object(uintptr_t inNode) { return (uintptr_t)SBN(inNode)->mButton; }
extern "C" bool ee_screenscpp_mg_node_available(uintptr_t inNode) { return SBN(inNode)->GetIsAvailable(); }
extern "C" bool ee_screenscpp_mg_node_allowed(uintptr_t inNode) { return SBN(inNode)->mIsAllowed; }
extern "C" void ee_screenscpp_mg_hot_key_always_fire(uintptr_t inKey) { MGK(inKey)->SetAlwaysFire(true); }
extern "C" void ee_screenscpp_mg_add_hot_key(uintptr_t inGroup, uintptr_t inKey)
{ MG(inGroup)->mOwner->AddHotKey(*MGK(inKey), 2); }
extern "C" uintptr_t ee_screenscpp_mg_hot_key_copy(uintptr_t inGroup, uintptr_t inKey)
{ return (uintptr_t)new UIHotKey(MG(inGroup)->mOwner, *MGK(inKey)); }
extern "C" void ee_screenscpp_mg_hot_key_shift(uintptr_t inKey) { MGK(inKey)->SetModifier(kKeyShift, 0); }
extern "C" void ee_screenscpp_mg_hot_key_delete(uintptr_t inKey) { delete MGK(inKey); }
extern "C" uintptr_t ee_screenscpp_mg_hot_key_event_new(uintptr_t inButton, bool inPressed)
{ return (uintptr_t)static_cast<UIEvent*>(new EEUIEMGHotKeyStatusButton(SBB(inButton), inPressed)); }
extern "C" void ee_screenscpp_mg_hot_key_pressed_event(uintptr_t inKey, uintptr_t inEvent)
{ MGK(inKey)->SetJustPressedEvent(reinterpret_cast<UIEvent*>(inEvent)); }
extern "C" void ee_screenscpp_mg_hot_key_unpressed_event(uintptr_t inKey, uintptr_t inEvent)
{ MGK(inKey)->SetJustUnpressedEvent(reinterpret_cast<UIEvent*>(inEvent)); }
extern "C" uintptr_t ee_screenscpp_mg_event_button(uintptr_t inEvent)
{ return (uintptr_t)reinterpret_cast<EEUIEMGHotKeyStatusButton*>(inEvent)->mButton; }
extern "C" bool ee_screenscpp_mg_event_pressed(uintptr_t inEvent)
{ return reinterpret_cast<EEUIEMGHotKeyStatusButton*>(inEvent)->mPressed; }
extern "C" void ee_screenscpp_mg_button_hot_key_down(uintptr_t inButton) { SBB(inButton)->HotKeyDown(); }
extern "C" void ee_screenscpp_mg_saved_groups_reset() { gWorld.GetCurrentPlayer()->mSavedGroups.Reset(0x14, 1); }
extern "C" void ee_screenscpp_mg_remove_cloak(uintptr_t inUnit)
{ EERemoveCloakEffect(static_cast<EEComplexUnit*>(reinterpret_cast<RTSUnit*>(inUnit))); }
extern "C" void ee_screenscpp_mg_add_cloak(uintptr_t inUnit) { EEAddCloakEffect(MGU(inUnit)); }
extern "C" void ee_screenscpp_mg_box_fill(uintptr_t inGroup, uintptr_t inUnit)
{ MG(inGroup)->mStatusBox->Fill(MGU(inUnit)); }
extern "C" void ee_screenscpp_mg_box_refresh(uintptr_t inGroup, uintptr_t inUnit)
{ MG(inGroup)->mStatusBox->Refresh(MGU(inUnit)); }
extern "C" void ee_screenscpp_mg_box_hide(uintptr_t inGroup) { MG(inGroup)->mStatusBox->HideBackgrounds(); }
extern "C" void ee_screenscpp_mg_box_queued(uintptr_t inGroup, uintptr_t inUnit)
{ MG(inGroup)->mStatusBox->UpdateQueuedCounts(MGU(inUnit)->GetComplexUnit(), MGU(inUnit)->GetComplexUnit()->GetPlayer()->GetResearchedImprovement(MGU(inUnit)->GetComplexUnit()->GetClass()->GetUnitType())); }
extern "C" void ee_screenscpp_mg_box_bind(uintptr_t inGroup, uint32_t inSlot, uint32_t inControl)
{ MG(inGroup)->mStatusBox->*kMGBoxSlots[inSlot] = MG(inGroup)->mOwner->GetControl(inControl); }
extern "C" void ee_screenscpp_mg_box_pair(uintptr_t inGroup, uint32_t inIndex)
{ MG(inGroup)->mStatusBox->SetControls((long)inIndex, MG(inGroup)->mOwner->GetControl(inIndex + 0x45), MG(inGroup)->mOwner->GetControl(inIndex + 0x20)); }
extern "C" void ee_screenscpp_mg_icon_bar_update(uintptr_t inControl)
{ EEUIShell::Op(reinterpret_cast<UIControl*>(inControl), EE_SCREENS_SKOP_IB_UPDATE); }
extern "C" bool ee_screenscpp_mg_unit_behavior_is(uintptr_t inUnit, int64_t inBehavior)
{ return MGU(inUnit)->GetComplexUnit()->GetBehavior() == EEUnitBehavior::Get(inBehavior); }
extern "C" bool ee_screenscpp_mg_object_has_hot_key(uint32_t inIndex)
{ return EEButtonObject::Get(inIndex)->GetHotKey() != NULL; }
extern "C" uintptr_t ee_screenscpp_mg_object_hot_key(uint32_t inIndex)
{ return (uintptr_t)EEButtonObject::Get(inIndex)->GetHotKey(); }
extern "C" void ee_screenscpp_mg_minutes_seconds(uint32_t inTime, unsigned long* inMinutes, unsigned long* inSeconds)
{ gPhysics->GetMinutesAndSeconds(inMinutes, inSeconds, inTime); }
extern "C" void ee_screenscpp_mg_timer_text(uintptr_t inGroup, uint32_t inControl, EeScreensShimText inText)
{ static_cast<UIButton*>(MG(inGroup)->mOwner->GetControl(inControl))->SetText(EEShimText(inText), gU531930); }
extern "C" uintptr_t ee_screenscpp_mg_world_unit(uint32_t inId)
{ return (uintptr_t)gWorld.GetUnit((unsigned long)inId); }
extern "C" uintptr_t ee_screenscpp_mg_player_unit(uint32_t inId)
{ return (uintptr_t)gWorld.GetCurrentPlayer()->GetUnit((unsigned long)inId); }
extern "C" int64_t ee_screenscpp_mg_formation_type()
{ return gWorld.GetCurrentPlayer()->GetSelectedGroup()->GetFormation()->GetType(); }
extern "C" void ee_screenscpp_mg_verify(int64_t inLine) { ESVERIFY_AT(false, (long)inLine); }
extern "C" int64_t ee_screenscpp_mg_time_left(uintptr_t inUnit, int64_t inQueue)
{ return MGU(inUnit)->mComplexUnit->mGoalQueues[inQueue].GetTimeLeft(); }
extern "C" int64_t ee_screenscpp_mg_queue_size(uintptr_t inUnit)
{ return (int64_t)MGU(inUnit)->GetComplexUnit()->mCitizenBuildQueue.size(); }
extern "C" int64_t ee_screenscpp_mg_queue_key(uintptr_t inUnit, uint32_t inIndex)
{ return MGU(inUnit)->GetComplexUnit()->GetCitizenBuildTarget(inIndex)->GetClass()->GetTechnology(); }

// the command buttons as the rest of the game shows them (the saved group's selection: ee/EEGameUI.h)
#include "ee/EEGameUI.h"
static const bool sShowCommandsRegistered =
    (gEEGameUI.mShowCommands = [](EEUIFMGMainGroup* inGroup, EEUnit* inUnit) { inGroup->ShowCommands(inUnit); }, true);
