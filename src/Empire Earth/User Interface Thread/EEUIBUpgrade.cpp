// C:\EEX\Empire Earth\User Interface Thread\EEUIBUpgrade.cpp (file name inferred: no __FILE__; RTTI class EEUIBUpgrade).
// The in-game improvement button: 0x256845..0x257223 (ctor, the in-class mouse forwarders, ??_G, then the out-of-line
// members in alphabetical order; EEUICChatLine's ctor at 0x257223). Unit flags: the exe module set.
#include <windows.h>
#include <string>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GERhombus.h"
#include "lle/SESoundManager.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "lle/UResourceFile.h"
#include "lle/UITextLabel.h"
#include "ee/EmpireEarth.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEAction.h"
#include "ee/EEARust.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIBUpgrade.h"
#include "lle/UIBitmap.h"
#include "ee/EEUIFMGStatusBar.h"
#include "rust/game/screens.h"   // ee_screens_upgrade_* (ee_screens upgrade.rs)


// The button's host (upgrade.rs EE_SCREENS_UP_OP_*): units (as EEUnit*) and nodes as handles (the selection's reads
// are hud_views.rs')
// the host's one-statement ops (upgrade_ops.rs; EEUIBUpgrade::HostOp's switch until UI lane round 16), on the call
// record's button, the unit (a) and the button's improvement
#define UPX_CALL (*static_cast<EEUIBUpgradeCall*>(reinterpret_cast<void*>(inCall)))
#define UPX_BUTTON static_cast<EEUIBUpgrade*>(UPX_CALL.mButton)
#define UPX_UNIT reinterpret_cast<EEUnit*>(inA)
extern "C" int64_t ee_screenscpp_upx_allowed(uintptr_t inCall, int64_t inA, int64_t inB) { return (int64_t)(gUnitImprovementsAllowed == true); }
extern "C" int64_t ee_screenscpp_upx_can_buy(uintptr_t inCall, int64_t inA, int64_t inB) { return (int64_t)(UPX_UNIT->mPlayer->CanBuy(UPX_UNIT->mUnitClass->GetUnitType(), UPX_BUTTON->mAttribute, static_cast<UWideString*>(UPX_CALL.mOut))); }
extern "C" int64_t ee_screenscpp_upx_editor_unit(uintptr_t inCall, int64_t inA, int64_t inB) { return (int64_t)((int64_t) static_cast<EEUIFScenarioEditor*>(UPX_BUTTON->GetForm())->GetFirstSelectedUnit()); }
extern "C" int64_t ee_screenscpp_upx_can_raise(uintptr_t inCall, int64_t inA, int64_t inB) { return (int64_t)(UPX_UNIT->mStats->mImprovement.CanUpgrade(UPX_UNIT->mStats->mUnitClass, UPX_BUTTON->mAttribute) == true); }
extern "C" int64_t ee_screenscpp_upx_maxed(uintptr_t inCall, int64_t inA, int64_t inB) { return (int64_t)(UPX_UNIT->mPlayer->IsImprovementMaxed(UPX_UNIT->mUnitClass->GetUnitType(), UPX_BUTTON->mAttribute)); }
extern "C" int64_t ee_screenscpp_upx_level(uintptr_t inCall, int64_t inA, int64_t inB) { return (int64_t)(UPX_UNIT->GetPlayer()->GetImprovementLevel(UPX_UNIT->mUnitClass->GetUnitType(), UPX_BUTTON->mAttribute)); }
extern "C" void ee_screenscpp_upx_text_name(uintptr_t inCall, int64_t inA, int64_t inB) { UPX_UNIT->GetStats()->GetImprovementName(UPX_CALL.mText, UPX_BUTTON->mAttribute); }
extern "C" void ee_screenscpp_upx_text_gain(uintptr_t inCall, int64_t inA, int64_t inB) { UPX_UNIT->GetStats()->GetImprovementGainText(UPX_CALL.mText, UPX_BUTTON->mAttribute, (long)inB); }
extern "C" void ee_screenscpp_upx_text_string(uintptr_t inCall, int64_t inA, int64_t inB) { gResource->GetString((long)inA, UPX_CALL.mText); }
extern "C" void ee_screenscpp_upx_out_append(uintptr_t inCall, int64_t inA, int64_t inB) { *static_cast<UWideString*>(UPX_CALL.mOut) += UPX_CALL.mText; }
extern "C" void ee_screenscpp_upx_out_set(uintptr_t inCall, int64_t inA, int64_t inB) { *static_cast<UWideString*>(UPX_CALL.mOut) = UPX_CALL.mText; }
extern "C" void ee_screenscpp_upx_out_bytes(uintptr_t inCall, int64_t inA, int64_t inB) { *static_cast<UWideString*>(UPX_CALL.mOut) += std::string(reinterpret_cast<const char*>(inA), (size_t)inB).c_str(); }
extern "C" void ee_screenscpp_upx_base_help(uintptr_t inCall, int64_t inA, int64_t inB) { UPX_BUTTON->UIControl::GetHelpString(inA == 0 ? *static_cast<UWideString*>(UPX_CALL.mOut) : UPX_CALL.mText); }
extern "C" void ee_screenscpp_upx_upgrade(uintptr_t inCall, int64_t inA, int64_t inB) { (new EEAUpgradeImprovement(UPX_UNIT, UPX_BUTTON->mAttribute, inB != 0, !UPX_BUTTON->mScenarioEditor))->Dereference(); }
extern "C" int64_t ee_screenscpp_upx_out_length(uintptr_t inCall, int64_t inA, int64_t inB) { return (int64_t)(static_cast<UWideString*>(UPX_CALL.mOut)->GetLength()); }
extern "C" int64_t ee_screenscpp_upx_refusal_interval(uintptr_t inCall, int64_t inA, int64_t inB) { return (int64_t)(gWorldTable.GetLongValue(kWorldRefusalMessageInterval)); }
extern "C" int64_t ee_screenscpp_upx_time(uintptr_t inCall, int64_t inA, int64_t inB) { return (int64_t)(TMGetTime()); }
extern "C" void ee_screenscpp_upx_chat(uintptr_t inCall, int64_t inA, int64_t inB) { gUserInterface->AddChatLogEntry(*static_cast<UWideString*>(UPX_CALL.mOut), 0); }
extern "C" void ee_screenscpp_upx_update_help(uintptr_t inCall, int64_t inA, int64_t inB) { UPX_BUTTON->GetForm()->UpdateHelpString(); }
extern "C" int64_t ee_screenscpp_upx_pressed(uintptr_t inCall, int64_t inA, int64_t inB) { return (int64_t)(UPX_BUTTON->GetState() == kButtonStatePressed); }
extern "C" void ee_screenscpp_upx_base_button_up(uintptr_t inCall, int64_t inA, int64_t inB) { UPX_BUTTON->UIButton::ButtonUp(); }
extern "C" int64_t ee_screenscpp_upx_base_move(uintptr_t inCall, int64_t inA, int64_t inB) { return (int64_t)(UPX_BUTTON->UIButton::MouseMove(*static_cast<const GE2DPoint*>(UPX_CALL.mLocation))); }
extern "C" void ee_screenscpp_upx_help_text(uintptr_t inCall, int64_t inA, int64_t inB) { UPX_BUTTON->GetHelpString(UPX_CALL.mText); }
extern "C" int64_t ee_screenscpp_upx_contains(uintptr_t inCall, int64_t inA, int64_t inB) { return (int64_t)(UPX_BUTTON->GetRectangle().Contains(UPX_BUTTON->GetMouseLocation())); }
extern "C" void ee_screenscpp_upx_release_mouse(uintptr_t inCall, int64_t inA, int64_t inB) { UPX_BUTTON->ReleaseCapturedMouse(); }
extern "C" int64_t ee_screenscpp_upx_cost_differs(uintptr_t inCall, int64_t inA, int64_t inB) { return (int64_t)(static_cast<UITextLabel*>(UPX_BUTTON->mCostLabel) != NULL && *static_cast<UWideString*>(UPX_CALL.mOut) != static_cast<UITextLabel*>(UPX_BUTTON->mCostLabel)->GetText(0)); }
extern "C" void ee_screenscpp_upx_cost_set(uintptr_t inCall, int64_t inA, int64_t inB) { static_cast<UITextLabel*>(UPX_BUTTON->mCostLabel)->SetText(*static_cast<UWideString*>(UPX_CALL.mOut)); }
extern "C" void ee_screenscpp_upx_dirty(uintptr_t inCall, int64_t inA, int64_t inB) { UPX_BUTTON->SetDirty(true); }
extern "C" void ee_screenscpp_upx_border_thickness(uintptr_t inCall, int64_t inA, int64_t inB) { UPX_BUTTON->SetBorderThickness((unsigned long)inA); }
extern "C" void ee_screenscpp_upx_bar_set_pressed(uintptr_t inCall, int64_t inA, int64_t inB) { static_cast<EEUIFMGStatusBar*>(UPX_BUTTON)->mPressed = inA != 0; }
extern "C" int64_t ee_screenscpp_upx_bar_base_l_down(uintptr_t inCall) { return UPX_BUTTON->UIButton::MouseLButtonDown(); }
extern "C" int64_t ee_screenscpp_upx_bar_base_l_up(uintptr_t inCall) { return UPX_BUTTON->UIButton::MouseLButtonUp(); }
extern "C" int64_t ee_screenscpp_upx_bar_base_r_down(uintptr_t inCall) { return UPX_BUTTON->EEUIBUpgrade::MouseRButtonDown(); }
#undef UPX_CALL
#undef UPX_BUTTON
#undef UPX_UNIT

// ---- the host's multi-statement cases (UI lane round 9: ee_game screens upgrade_calls.rs), each one statement on
// the call record's button
#include "ee/EEScreensShims.h"
#define UPC(inCall) (*reinterpret_cast<EEUIBUpgradeCall*>(inCall))
#define UPB(inCall) static_cast<EEUIBUpgrade*>(UPC(inCall).mButton)
static const unsigned long kUpSounds[] = {kSoundButton2, kSoundButton1};
extern "C" int64_t ee_screenscpp_up_attribute(uintptr_t inCall) { return (int64_t)UPB(inCall)->mAttribute; }
extern "C" void ee_screenscpp_up_set_attribute(uintptr_t inCall, int64_t inValue)
{ UPB(inCall)->mAttribute = (long)inValue; }
extern "C" int64_t ee_screenscpp_up_level(uintptr_t inCall) { return (int64_t)UPB(inCall)->mLevel; }
extern "C" void ee_screenscpp_up_set_level(uintptr_t inCall, int64_t inValue) { UPB(inCall)->mLevel = (long)inValue; }
extern "C" int64_t ee_screenscpp_up_last_message(uintptr_t inCall)
{ return (int64_t)(long)UPB(inCall)->mLastMessageTime; }
extern "C" void ee_screenscpp_up_set_last_message(uintptr_t inCall, int64_t inValue)
{ UPB(inCall)->mLastMessageTime = (unsigned long)(long)inValue; }
extern "C" bool ee_screenscpp_up_editor(uintptr_t inCall) { return UPB(inCall)->mScenarioEditor; }
extern "C" bool ee_screenscpp_up_disabled(uintptr_t inCall) { return UPB(inCall)->mDisabled; }
extern "C" void ee_screenscpp_up_set_disabled(uintptr_t inCall, bool inValue) { UPB(inCall)->mDisabled = inValue; }
extern "C" uintptr_t ee_screenscpp_up_editor_complex(uintptr_t inCall)
{ return (uintptr_t)static_cast<EEUnit*>(static_cast<EEUIFScenarioEditor*>(UPB(inCall)->GetForm())->GetFirstSelectedUnit()->GetComplexUnit()); }
extern "C" float ee_screenscpp_up_step(uintptr_t inCall, uintptr_t inUnit)
{ return reinterpret_cast<EEUnit*>(inUnit)->mUnitClass->mImprovements->mFactors[UPB(inCall)->mAttribute].mOffset; }
extern "C" float ee_screenscpp_up_cost_factor(uintptr_t inCall, uintptr_t inUnit)
{ return reinterpret_cast<EEUnit*>(inUnit)->mUnitClass->mImprovements->mFactors[UPB(inCall)->mAttribute].mScale; }
extern "C" int64_t ee_screenscpp_up_unit_type(uintptr_t inUnit)
{ return (int64_t)reinterpret_cast<EEUnit*>(inUnit)->mUnitClass->GetUnitType(); }
extern "C" bool ee_screenscpp_up_class_exists(int64_t inUnitType) { return gUnitClassTable[(long)inUnitType] != NULL; }
extern "C" int64_t ee_screenscpp_up_class_technology(int64_t inUnitType)
{ return (int64_t)gUnitClassTable[(long)inUnitType]->GetTechnology(); }
extern "C" uintptr_t ee_screenscpp_up_node(int64_t inTechnology)
{ return (uintptr_t)gWorld.GetCurrentPlayer()->GetTechTree()->Get((long)inTechnology); }
extern "C" void ee_screenscpp_up_text_cost(uintptr_t inCall, uintptr_t inNode, uintptr_t inUnit, float inFactor)
{ reinterpret_cast<EETechTreeNode*>(inNode)->GetCostString(UPC(inCall).mText, reinterpret_cast<EEUnit*>(inUnit)->mUnitClass->GetUnitType(), inFactor); }
extern "C" void ee_screenscpp_up_sound(uint32_t inWhich)
{ gSoundManager->Play(EESoundObject::Get(kUpSounds[inWhich])->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL); }
extern "C" uintptr_t ee_screenscpp_up_help_label(uintptr_t inCall)
{ return (uintptr_t)UPB(inCall)->GetForm()->GetHelpTextLabel(); }
extern "C" void ee_screenscpp_up_help_label_set(uintptr_t inLabel, uintptr_t inCall)
{ reinterpret_cast<UITextLabel*>(inLabel)->SetText(UPC(inCall).mText); }
extern "C" void ee_screenscpp_up_bg_opacity(uintptr_t inCall, float inOpacity) { UPB(inCall)->SetBgOpacity(inOpacity); }
extern "C" void ee_screenscpp_up_border_opacity(uintptr_t inCall, float inOpacity)
{ UPB(inCall)->SetBorderOpacity(inOpacity); }
extern "C" bool ee_screenscpp_up_bar_enabled(uintptr_t inCall)
{ return static_cast<EEUIFMGStatusBar*>(UPB(inCall))->mEnabled; }
extern "C" bool ee_screenscpp_up_bar_pressed(uintptr_t inCall)
{ return static_cast<EEUIFMGStatusBar*>(UPB(inCall))->mPressed; }
extern "C" bool ee_screenscpp_up_has_capture(uintptr_t inCall) { return UPB(inCall)->GetHasMouseCapture(); }
extern "C" bool ee_screenscpp_up_inside_form_mouse(uintptr_t inCall)
{ return UPB(inCall)->GetRectangle().Contains(UPB(inCall)->GetForm()->GetMouseLocation()); }
extern "C" bool ee_screenscpp_up_inside_location(uintptr_t inCall)
{ return UPB(inCall)->GetRectangle().Contains(*static_cast<const GE2DPoint*>(UPC(inCall).mLocation)); }
extern "C" float ee_screenscpp_up_icon_left(uintptr_t inCall)
{ return static_cast<UIBitmap*>(static_cast<EEUIFMGStatusBar*>(UPB(inCall))->mIcon)->GetRectangle().GetTopLeft().GetX(); }
extern "C" float ee_screenscpp_up_icon_top(uintptr_t inCall)
{ return static_cast<UIBitmap*>(static_cast<EEUIFMGStatusBar*>(UPB(inCall))->mIcon)->GetRectangle().GetTopLeft().GetY(); }
extern "C" void ee_screenscpp_up_icon_set_location(uintptr_t inCall, float inX, float inY)
{ static_cast<UIBitmap*>(static_cast<EEUIFMGStatusBar*>(UPB(inCall))->mIcon)->SetLocation(GE2DPoint(inX, inY)); }
// the constructor's body and the draw, dirty, mouse and press overrides: hud_bodies.rs over these (UI lane round 12)
#define UPBB(b) ((EEUIBUpgrade*)(b))
extern "C" void ee_screenscpp_upb_set_num_queued(uintptr_t inButton, int64_t inValue)
{ UPBB(inButton)->mNumQueued = (long)inValue; }
extern "C" void ee_screenscpp_upb_set_level(uintptr_t inButton, int64_t inValue)
{ UPBB(inButton)->mLevel = (long)inValue; }
extern "C" void ee_screenscpp_upb_set_last_message(uintptr_t inButton, uint64_t inValue)
{ UPBB(inButton)->mLastMessageTime = (unsigned long)inValue; }
extern "C" void ee_screenscpp_upb_set_cost_label(uintptr_t inButton, uintptr_t inLabel)
{ UPBB(inButton)->mCostLabel = (UITextLabel*)inLabel; }
extern "C" void ee_screenscpp_upb_bg_opacity(uintptr_t inButton, float inOpacity)
{ UPBB(inButton)->SetBgOpacity(inOpacity); }
extern "C" void ee_screenscpp_upb_border_opacity(uintptr_t inButton, float inOpacity)
{ UPBB(inButton)->SetBorderOpacity(inOpacity); }
extern "C" void ee_screenscpp_upb_border_thickness(uintptr_t inButton, uint32_t inThickness)
{ UPBB(inButton)->SetBorderThickness(inThickness); }
extern "C" void ee_screenscpp_upb_set_disabled(uintptr_t inButton, bool inValue)
{ UPBB(inButton)->mDisabled = inValue; }
extern "C" bool ee_screenscpp_upb_disabled(uintptr_t inButton) { return UPBB(inButton)->GetDisabled(); }
extern "C" void ee_screenscpp_upb_base_button_down(uintptr_t inButton) { EEUIBUpgrade::BaseButtonDown(UPBB(inButton)); }
extern "C" void ee_screenscpp_upb_update_help(uintptr_t inButton) { UPBB(inButton)->GetForm()->UpdateHelpString(); }
extern "C" int64_t ee_screenscpp_upb_base_copy_data(uintptr_t inButton)
{ return (int64_t)UPBB(inButton)->UIButton::CopyData(); }
extern "C" uintptr_t ee_screenscpp_upb_cost_label(uintptr_t inButton) { return (uintptr_t)static_cast<UITextLabel*>(UPBB(inButton)->mCostLabel); }
extern "C" void ee_screenscpp_upb_label_dirty_rect(uintptr_t inButton, uintptr_t inLabel)
{ UPBB(inButton)->GetForm()->GetManager()->AddDirtyRectangle(((UITextLabel*)inLabel)->GetRectangle(), UPBB(inButton)->GetForm()); }
extern "C" int64_t ee_screenscpp_upb_label_copy_data(uintptr_t inLabel)
{ return (int64_t)((UITextLabel*)inLabel)->CopyData(); }
extern "C" int64_t ee_screenscpp_upb_base_draw_text(uintptr_t inButton)
{ return (int64_t)UPBB(inButton)->UIButton::DrawTextA(); }
extern "C" int64_t ee_screenscpp_upb_label_draw_text(uintptr_t inLabel)
{ return (int64_t)((UITextLabel*)inLabel)->DrawTextA(); }
extern "C" bool ee_screenscpp_upb_can_upgrade(uintptr_t inButton, uintptr_t* inUnit)
{ return UPBB(inButton)->GetCanUpgrade(*reinterpret_cast<EEUnit**>(inUnit), NULL); }
extern "C" void ee_screenscpp_upb_base_mouse_over(uintptr_t inButton) { EEUIBUpgrade::BaseMouseOver(UPBB(inButton)); }
extern "C" void ee_screenscpp_upb_capture(uintptr_t inButton) { EEUIBUpgrade::CaptureOf(UPBB(inButton)); }
extern "C" void ee_screenscpp_upb_button_down(uintptr_t inButton) { EEUIBUpgrade::ButtonDownOf(UPBB(inButton)); }
extern "C" void ee_screenscpp_upb_label_set_dirty(uintptr_t inLabel) { ((UITextLabel*)inLabel)->SetDirty(true); }
extern "C" void ee_screenscpp_upb_base_set_dirty(uintptr_t inButton, bool inDirty)
{ UPBB(inButton)->UIControl::SetDirty(inDirty); }
