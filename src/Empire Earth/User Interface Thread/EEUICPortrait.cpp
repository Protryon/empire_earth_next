// C:\EEX\Empire Earth\User Interface Thread\EEUICPortrait.cpp (file name inferred: no __FILE__; RTTI class
// EEUICPortrait). The main game form's selected-unit portrait: 0x25861c..0x25952f (ctor, ??_G, dtor, then the members
// in alphabetical order with EEPlayer's two inline lookups after their first users; EEUICSelectUnits' init chains at
// 0x25952f). Unit flags: the exe module set.
#include <windows.h>
#include <string>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GEScreen.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/SESoundManager.h"
#include "lle/UIControl.h"
#include "lle/UIFontTable.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "lle/UResourceFile.h"
#include "ee/EmpireEarth.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEButtonObject.h"
#include "ee/EEUICPortrait.h"
#include "ee/EEScreensShims.h"
#include "ee/EEStringIDs.h"
// Creates the selected-unit portrait with its research (green) and improvement (yellow) progress bars over a
// red background, and a level text

// the tooltip being built (the UI thread's)

extern "C" void ee_screenscpp_pt_help_target(UWideString* inOut) { EEScratch<UWideString*>(kScratchPortraitHelp) = inOut; }
extern "C" void ee_screenscpp_pt_help_clear() { *EEScratch<UWideString*>(kScratchPortraitHelp) = UWideString(); }

// The portrait's own ops (portrait.rs EE_SCREENS_PT_OP_*: units and nodes as handles), its constructor's,
// destructor's, DrawTextA's and MouseMove's bodies: portrait_calls.rs over the one-statement shims below (UI lane
// round 11)
#define PT(p) ((EEUICPortrait*)(p))
#define PT_CU(u) ((EEComplexUnit*)(u))
#define PT_N(n) ((EETechTreeNode*)(n))

extern "C" bool ee_screenscpp_pt_has_unit(uintptr_t inP) { return PT(inP)->mUnit != NULL; }
extern "C" void ee_screenscpp_pt_button_release(uintptr_t inP) { static_cast<EEButtonObject*>(PT(inP)->mButton)->Release(); }
extern "C" void ee_screenscpp_pt_unit_release(uintptr_t inP) { PT(inP)->mUnit->Dereference(); }
extern "C" void ee_screenscpp_pt_set_unit(uintptr_t inP, uintptr_t inUnit)
{ PT(inP)->mUnit = reinterpret_cast<EEUnit*>(inUnit); }
extern "C" void ee_screenscpp_pt_unit_acquire(uintptr_t inP) { PT(inP)->mUnit->Reference(); }
extern "C" void ee_screenscpp_pt_button_take(uintptr_t inP) { PT(inP)->mButton = static_cast<EEButtonObject*>(PT(inP)->mUnit->mStats->mButton); }
extern "C" void ee_screenscpp_pt_button_acquire(uintptr_t inP) { static_cast<EEButtonObject*>(PT(inP)->mButton)->Acquire(); }
extern "C" void ee_screenscpp_pt_face_texture(uintptr_t inP)
{ static_cast<GEBitmap*>(PT(inP)->mPortrait)->SetTexture(static_cast<EEButtonObject*>(PT(inP)->mButton)->GetTexture(), gScreenIndex); }
extern "C" void ee_screenscpp_pt_face_bitmap(uintptr_t inP)
{ static_cast<EEButtonObject*>(PT(inP)->mButton)->SetFaceBitmap(static_cast<GEBitmap*>(PT(inP)->mPortrait), PT(inP)->GetRectangle(), gScreenIndex); }
extern "C" uint64_t ee_screenscpp_pt_level(uintptr_t inP) { return (uint64_t)PT(inP)->mLevel; }
extern "C" bool ee_screenscpp_pt_first_bar(uintptr_t inP) { return PT(inP)->mShowFirstBar; }
extern "C" bool ee_screenscpp_pt_second_bar(uintptr_t inP) { return PT(inP)->mShowSecondBar; }
extern "C" void ee_screenscpp_pt_set_level(uintptr_t inP, uint64_t inV) { PT(inP)->mLevel = (unsigned long)inV; }
extern "C" void ee_screenscpp_pt_set_first_bar(uintptr_t inP, bool inV) { PT(inP)->mShowFirstBar = inV; }
extern "C" void ee_screenscpp_pt_set_second_bar(uintptr_t inP, bool inV) { PT(inP)->mShowSecondBar = inV; }
extern "C" void ee_screenscpp_pt_set_show_level(uintptr_t inP, bool inV) { PT(inP)->mShowLevel = inV; }
extern "C" uintptr_t ee_screenscpp_pt_unit_complex(uintptr_t inP)
{ return (uintptr_t)PT(inP)->mUnit->GetComplexUnit(); }
extern "C" int64_t ee_screenscpp_pt_researched(uintptr_t inUnit)
{ return PT_CU(inUnit)->GetPlayer()->GetResearchedImprovement(PT_CU(inUnit)->GetUnitClass()->GetUnitType()); }
extern "C" bool ee_screenscpp_pt_node_makes_unit(uintptr_t inNode) { return PT_N(inNode)->mData->mMakesUnit; }
extern "C" uintptr_t ee_screenscpp_pt_node_upgrade(uintptr_t inP, uintptr_t inNode)
{ return (uintptr_t)PT(inP)->mUnit->GetPlayer()->mUnitClassUpgrades[PT_N(inNode)->mData->mUnitClass]; }
extern "C" float ee_screenscpp_pt_upgrade_build_scale(uintptr_t inUp)
{ return ((EEUnitClassUpgrade*)inUp)->mBuildTimeScale; }
extern "C" int64_t ee_screenscpp_pt_node_build_time(uintptr_t inNode) { return PT_N(inNode)->GetBuildTime(); }
extern "C" int64_t ee_screenscpp_pt_improvement_level(uintptr_t inUnit, int64_t inKind)
{ return PT_CU(inUnit)->GetStats()->mImprovement.GetLevel((long)inKind); }
extern "C" int64_t ee_screenscpp_pt_improvement_time(int64_t inKind)
{ return gWorldTable.GetLongValue(EEWorldImprovementTime((long)inKind)); }
extern "C" int64_t ee_screenscpp_pt_improvement_time2(int64_t inKind)
{ return gWorldTable.GetLongValue2(EEWorldImprovementTime((long)inKind)); }
extern "C" float ee_screenscpp_pt_progress(uintptr_t inUnit)
{ return PT_CU(inUnit)->GetPlayer()->GetImprovementProgress(PT_CU(inUnit)->GetUnitClass()->GetUnitType()); }
extern "C" int64_t ee_screenscpp_pt_total_cost(uintptr_t inUnit)
{ return PT_CU(inUnit)->GetStats()->mImprovement.mTotalCost; }
extern "C" int64_t ee_screenscpp_pt_points_max() { return gWorldTable.GetLongValue(kWorldUnitImprovementPointsMax); }
extern "C" void ee_screenscpp_pt_text(uintptr_t inP, EeScreensShimText inText)
{ static_cast<GEText*>(PT(inP)->mText)->SetText(EEShimText(inText)); }
extern "C" void ee_screenscpp_pt_update_help(uintptr_t inP) { PT(inP)->GetForm()->UpdateHelpString(); }
extern "C" bool ee_screenscpp_pt_contains(uintptr_t inP, float inLeft, float inTop, float inRight, float inBottom)
{ return GERectangle(inLeft, inTop, inRight, inBottom).Contains(PT(inP)->GetMouseLocation()); }
extern "C" bool ee_screenscpp_pt_unit_building(uintptr_t inP) { return PT(inP)->mUnit->GetUnitClass()->IsBuilding(); }
extern "C" void ee_screenscpp_pt_strings_clear() { EEScratch<UWideString>(kScratchPTFormat) = UWideString(); }
extern "C" void ee_screenscpp_pt_name_clear() { EEScratch<UWideString>(kScratchPTName) = UWideString(); }
extern "C" void ee_screenscpp_pt_format_research()
{ gResource->GetString(IDS_LEFT_RIGHT_CLICK_TO_CANCEL_RESEARCH, EEScratch<UWideString>(kScratchPTFormat)); }
extern "C" void ee_screenscpp_pt_format_improvement()
{ gResource->GetString(IDS_LEFT_RIGHT_CLICK_TO_CANCEL_IMPROVEMENT, EEScratch<UWideString>(kScratchPTFormat)); }
extern "C" void ee_screenscpp_pt_name_unit_class(uintptr_t inNode)
{ gResource->GetString(gUnitClassTable[PT_N(inNode)->mData->mUnitClass]->mNameStringID, EEScratch<UWideString>(kScratchPTName)); }
extern "C" void ee_screenscpp_pt_name_node(uintptr_t inNode)
{ gResource->GetString(PT_N(inNode)->mData->mNameID, EEScratch<UWideString>(kScratchPTName)); }
extern "C" void ee_screenscpp_pt_name_improvement(uintptr_t inP)
{ PT(inP)->mUnit->mStats->GetImprovementName(EEScratch<UWideString>(kScratchPTName), PT(inP)->mUnit->GetPlayer()->GetResearchedImprovement(PT(inP)->mUnit->GetUnitClass()->GetUnitType())); }
extern "C" void ee_screenscpp_pt_help_print() { sprintf(*EEScratch<UWideString*>(kScratchPortraitHelp), EEScratch<UWideString>(kScratchPTFormat), (const char*)EEScratch<UWideString>(kScratchPTName)); }
extern "C" bool ee_screenscpp_pt_unit_hp_positive(uintptr_t inP) { return PT(inP)->mUnit->mHitPoints > 0; }
extern "C" bool ee_screenscpp_pt_unit_on_tile(uintptr_t inP) { return PT(inP)->mUnit->mTileNode != NULL; }
extern "C" uintptr_t ee_screenscpp_pt_unit_complex_field(uintptr_t inP)
{ return (uintptr_t)(EEComplexUnit*)PT(inP)->mUnit->mComplexUnit; }
extern "C" void ee_screenscpp_pt_sound()
{ gSoundManager->Play(EESoundObject::Get(kSoundButton1)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL); }
extern "C" void ee_screenscpp_pt_new_bitmap(uintptr_t inP) { PT(inP)->mPortrait = new GEBitmap; }
extern "C" void ee_screenscpp_pt_bitmap_location(uintptr_t inP, float inX, float inY)
{ static_cast<GEBitmap*>(PT(inP)->mPortrait)->SetLocation(GE2DPoint(inX, inY)); }
extern "C" void ee_screenscpp_pt_draw_init(uintptr_t inP, float* inRect)
{ ee_screens_portrait_draw_init(PT(inP), static_cast<GEBitmap*>(PT(inP)->mPortrait), inRect); }
extern "C" void ee_screenscpp_pt_new_text(uintptr_t inP, float inHeight)
{ PT(inP)->mText = new GEText(UIFontTable::CreateClosestFont(0x36, inHeight)); }
extern "C" void ee_screenscpp_pt_text_align_right(uintptr_t inP) { static_cast<GEText*>(PT(inP)->mText)->SetAlignment(kTextAlignRight); }
extern "C" void ee_screenscpp_pt_text_transparent(uintptr_t inP) { static_cast<GEText*>(PT(inP)->mText)->SetIsTransparent(true); }
extern "C" void ee_screenscpp_pt_text_color(uintptr_t inP) { static_cast<GEText*>(PT(inP)->mText)->SetFgColor(GEColor()); }
extern "C" void ee_screenscpp_pt_text_location(uintptr_t inP, float inX, float inY)
{ static_cast<GEText*>(PT(inP)->mText)->SetLocation(GE2DPoint(inX, inY)); }
extern "C" void ee_screenscpp_pt_delete_text(uintptr_t inP) { delete static_cast<GEText*>(PT(inP)->mText); }
extern "C" void ee_screenscpp_pt_bitmap_release(uintptr_t inP) { static_cast<GEBitmap*>(PT(inP)->mPortrait)->Dereference(); }
extern "C" void ee_screenscpp_pt_draw_text_base(uintptr_t inP) { PT(inP)->UIControl::DrawTextA(); }
extern "C" int32_t ee_screenscpp_pt_draw_level(uintptr_t inP)
{ return PT(inP)->GetForm()->DrawClippedText(PT(inP)->GetCopiedDrawRectangle(), static_cast<GEText*>(PT(inP)->mText), PT(inP)); }
