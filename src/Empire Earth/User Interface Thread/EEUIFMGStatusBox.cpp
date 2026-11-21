// C:\EEX\Empire Earth\User Interface Thread\EEUIFMGStatusBox.cpp (__FILE__ 0x4cdcfc). The main group's status box
// (no vtable; ctor 0x26ada8): 0x269ca8..0x26de97 (the init chains of three GEColors and the two icon tables, the
// icon cell ctor, the ctor, the selection members, the icon textures, EEUIFMGStatusBar's SetIconBitmap after its first
// user, the exchange-button helpers 0x26ddb7/0x26de3b). Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/UIBackground.h"
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
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIBUpgrade.h"
#include "ee/EEUIFMGAttribute.h"
#include "ee/EEUICPortrait.h"
#include "lle/UIBitmap.h"
#include "lle/UIStatusBar.h"
#include "ee/EEUIFMGStatusBar.h"
#include "ee/EEUIFMGStatusBox.h"
#include "ee/EEStringIDs.h"
extern unsigned long& gTexturePixelDepth;

// one loaded icon (0x14 bytes) (provisional name)
struct EEUIFMGIconBitmap {
    GERectangle mRectangle;                 // +0x00 the texture rectangle
    GEBitmap* mBitmap;                      // +0x10
};

static GEColor kStatusColor1(0.0f, 0.0f, 1.0f);
static GEColor kStatusColor2(1.0f, 1.0f, 0.0f);
static GEColor kStatusColor3(0.0f, 1.0f, 0.5f);
EEUIFMGIconBitmap EEUIFMGStatusBox::sIconBitmaps[50];
// (the icon cells' table, 0x564890, is Rust's: ee_screens status_box.rs CELLS)
extern bool sIconsLoaded;                           // 0x5652ec; cross-TU flag (stored in EEGlobals.cpp)

// Creates the main panel's status box with no controls bound and no attributes shown
EEUIFMGStatusBox::EEUIFMGStatusBox(UIForm* inForm)
{
    ee_screens_status_box_init(this, inForm);   // (main_form_fields.rs)
}

// Existing pooled empty byte string, original reference26b6d9; no equal-content alias.
extern char gEmptyString[];

// The host's record of one status box call (ee_screens status_box.rs): the box, the unit (Fill, Refresh), the
// complex unit (UpdateQueuedCounts), the held text (the original's `theText`), LoadIcons' description and textures
struct EEUIFMGStatusBoxCall {
    EEUIFMGStatusBox* mBox;
    EEUnit* mUnit;
    EEComplexUnit* mComplex;
    UWideString mText;
    GETextureDescription mDescription;
    GETexture* mTextures[4];
};
static_assert(offsetof(EEUIFMGStatusBoxCall, mText) == 3 * sizeof(void*), "hud_direct.rs BoxCallHead");
static_assert(EE_SCREENS_BOX_IMPROVEMENT_FLIGHT_OR_POWER == kImprovementFlightTimeOrPower &&
                  EE_SCREENS_BOX_IMPROVEMENT_HIT_POINTS == kImprovementHitPoints &&
                  EE_SCREENS_BOX_IMPROVEMENT_TRANSPORT == kImprovementTransport &&
                  EE_SCREENS_BOX_IDS_GARRISON == IDS_IMPROVEMENT_GARRISON && kDisplayModeHidden == 0 &&
                  kDisplayModeShown == 1,
              "status_box.rs's constants");

// The control an EE_SCREENS_BOX_CTL_* names
UIControl* EEUIFMGStatusBox::GetBoxControl(int64_t inControl) const
{
    return static_cast<UIControl*>(ee_screens_status_box_control(this, inControl));   // (the record's slots)
}

// The call's unit (an EEUnit*) for the status box's view of it (hud_views.rs status_box_view, the original's
// EEStatusBoxView), and whether the box has its third and fourth bars
extern "C" int64_t ee_screenscpp_status_box_unit(void* inCall, bool* outTwoBars)
{
    EEUIFMGStatusBoxCall& theCall = *static_cast<EEUIFMGStatusBoxCall*>(inCall);
    *outTwoBars = theCall.mBox->mCA8 != NULL && theCall.mBox->mCAC != NULL;
    return (int64_t)theCall.mUnit;
}


// ---- the one-statement shims of ee_screens status_box_calls.rs (UI lane round 8; once HostOp's cases and
// EEUIFMGStatusBar::SetIconBitmap): each one call, no logic
#define SBC(x) (*reinterpret_cast<EEUIFMGStatusBoxCall*>(x))
#define SBX(x, i) SBC(x).mBox->GetBoxControl(i)
#define SBBAR(x, i) static_cast<EEUIFMGStatusBar*>(SBX(x, i))
static const GEColor* const kSBBarColors[3] = {&kStatusColor3, &kStatusColor1, &kStatusColor2};
static const char* const kSBTextureNames[4] = {"", "textures\\unit improvements_01", "textures\\unit improvements_02",
                                               "textures\\unit upgrades_03"};
static UWideString sSBSuffix;                 // the gate's lock state (the original's local)
static GETextureDescription sSBReload;        // ReloadIcons' description
extern "C" int64_t ee_screenscpp_sb_upgrade_queued(uintptr_t inCall, int64_t inCtl)
{ return static_cast<EEUIBUpgrade*>(SBX(inCall, inCtl))->mNumQueued; }
extern "C" void ee_screenscpp_sb_control_set_dirty(uintptr_t inCall, int64_t inCtl)
{ SBX(inCall, inCtl)->SetDirty(true); }
extern "C" void ee_screenscpp_sb_upgrade_set_queued(uintptr_t inCall, int64_t inCtl, int64_t inCount)
{ static_cast<EEUIBUpgrade*>(SBX(inCall, inCtl))->mNumQueued = (long)inCount; }
extern "C" int64_t ee_screenscpp_sb_upgrade_attribute(uintptr_t inCall, int64_t inCtl)
{ return static_cast<EEUIBUpgrade*>(SBX(inCall, inCtl))->mAttribute; }
extern "C" void ee_screenscpp_sb_upgrade_set_attribute(uintptr_t inCall, int64_t inCtl, int64_t inValue)
{ static_cast<EEUIBUpgrade*>(SBX(inCall, inCtl))->mAttribute = (long)inValue; }
extern "C" void ee_screenscpp_sb_upgrade_cost_label_clear(uintptr_t inCall, int64_t inCtl)
{ static_cast<EEUIBUpgrade*>(SBX(inCall, inCtl))->mCostLabel = NULL; }
extern "C" void ee_screenscpp_sb_upgrade_cost_label(uintptr_t inCall, int64_t inCtl, int64_t inLabel)
{ static_cast<EEUIBUpgrade*>(SBX(inCall, inCtl))->mCostLabel = static_cast<UITextLabel*>(SBX(inCall, inLabel)); }
extern "C" void ee_screenscpp_sb_upgrade_update_cost(uintptr_t inCall, int64_t inCtl)
{ static_cast<EEUIBUpgrade*>(SBX(inCall, inCtl))->UpdateCost(SBC(inCall).mText); }
extern "C" bool ee_screenscpp_sb_name_empty_fill(uintptr_t inCall)
{ return SBC(inCall).mUnit->mStats->mName == gEmptyString; }
extern "C" bool ee_screenscpp_sb_name_empty(uintptr_t inCall) { return SBC(inCall).mUnit->GetStats()->mName == ""; }
extern "C" void ee_screenscpp_sb_text_class_name(uintptr_t inCall)
{ gResource->GetString(SBC(inCall).mUnit->GetUnitClass()->mNameStringID, SBC(inCall).mText); }
extern "C" void ee_screenscpp_sb_text_stats_name(uintptr_t inCall)
{ SBC(inCall).mText = SBC(inCall).mUnit->GetStats()->mName; }
extern "C" bool ee_screenscpp_sb_is_gate_fill(uintptr_t inCall)
{ return SBC(inCall).mUnit->mUnitClass->mCategory == kUnitCategoryGate; }
extern "C" bool ee_screenscpp_sb_is_gate(uintptr_t inCall)
{ return SBC(inCall).mUnit->GetUnitClass()->mCategory == kUnitCategoryGate; }
extern "C" bool ee_screenscpp_sb_gate_locked(uintptr_t inCall) { return SBC(inCall).mUnit->mComplexUnit->mIsLocked; }
extern "C" void ee_screenscpp_sb_suffix_clear() { sSBSuffix = UWideString(); }
extern "C" void ee_screenscpp_sb_suffix_locked() { gResource->GetString(IDS_MSG_LOCKED, sSBSuffix); }
extern "C" void ee_screenscpp_sb_suffix_unlocked() { gResource->GetString(IDS_MSG_UNLOCKED, sSBSuffix); }
extern "C" void ee_screenscpp_sb_text_append_suffix(uintptr_t inCall) { SBC(inCall).mText += sSBSuffix; }
extern "C" void ee_screenscpp_sb_text_civ(uintptr_t inCall)
{ SBC(inCall).mText = SBC(inCall).mUnit->GetPlayer()->mCivilization.mName; }
extern "C" uintptr_t ee_screenscpp_sb_bar_control(uintptr_t inCall, int64_t inCtl)
{ return (uintptr_t)SBBAR(inCall, inCtl); }
extern "C" uintptr_t ee_screenscpp_sb_icon_rect(int64_t inIcon)
{ return (uintptr_t)&EEUIFMGStatusBox::sIconBitmaps[inIcon].mRectangle; }
extern "C" void ee_screenscpp_sb_bar_range(uintptr_t inCall, int64_t inCtl, int64_t inValue)
{ SBBAR(inCall, inCtl)->mBar->SetRange(0, (unsigned long)(long)inValue); }
extern "C" void ee_screenscpp_sb_bar_value(uintptr_t inCall, int64_t inCtl, int64_t inValue)
{ SBBAR(inCall, inCtl)->mBar->SetCurrentRangeValue((long)inValue); }
extern "C" void ee_screenscpp_sb_bar_color(uintptr_t inCall, int64_t inCtl, uint32_t inColor)
{ SBBAR(inCall, inCtl)->mBar->SetStatusBarColor(*kSBBarColors[inColor]); }
extern "C" void ee_screenscpp_sb_bar_enabled(uintptr_t inCall, int64_t inCtl, bool inEnabled)
{ SBBAR(inCall, inCtl)->mEnabled = inEnabled; }
extern "C" void ee_screenscpp_sb_stretch(uintptr_t inCall, int64_t inIcon, int64_t inRectangle, int64_t inCtl)
{ EEUIFMGStatusBox::sIconBitmaps[inIcon].mBitmap->ComputeStretchedCoordsAndScale(static_cast<EEUIFMGAttribute*>(SBX(inCall, inRectangle))->GetIconRectangle(), static_cast<EEUIFMGAttribute*>(SBX(inCall, inRectangle))->GetIconRectangle(), EEUIFMGStatusBox::sIconBitmaps[inIcon].mRectangle, SBX(inCall, inCtl)->GetScreenIndex()); }
extern "C" void ee_screenscpp_sb_button_set_normal(uintptr_t inCall, int64_t inCtl, int64_t inIcon)
{ static_cast<UIButton*>(SBX(inCall, inCtl))->SetNormalBitmap(EEUIFMGStatusBox::sIconBitmaps[inIcon].mBitmap); }
extern "C" void ee_screenscpp_sb_button_set_pressed(uintptr_t inCall, int64_t inCtl, int64_t inIcon)
{ static_cast<UIButton*>(SBX(inCall, inCtl))->SetPressedBitmap(EEUIFMGStatusBox::sIconBitmaps[inIcon].mBitmap); }
extern "C" bool ee_screenscpp_sb_button_normal_is(uintptr_t inCall, int64_t inCtl, int64_t inIcon)
{ return static_cast<UIButton*>(SBX(inCall, inCtl))->GetNormalBitmap() == EEUIFMGStatusBox::sIconBitmaps[inIcon].mBitmap; }
extern "C" bool ee_screenscpp_sb_button_pressed_is(uintptr_t inCall, int64_t inCtl, int64_t inIcon)
{ return static_cast<UIButton*>(SBX(inCall, inCtl))->GetPressedBitmap() == EEUIFMGStatusBox::sIconBitmaps[inIcon].mBitmap; }
extern "C" void ee_screenscpp_sb_portrait_set_unit(uintptr_t inCall)
{ static_cast<EEUICPortrait*>(SBC(inCall).mBox->mC8C)->SetUnit(SBC(inCall).mUnit); }
extern "C" void ee_screenscpp_sb_portrait_clear(uintptr_t inCall)
{ static_cast<EEUICPortrait*>(SBC(inCall).mBox->mC8C)->SetUnit(NULL); }
extern "C" void ee_screenscpp_sb_portrait_update(uintptr_t inCall)
{ static_cast<EEUICPortrait*>(SBC(inCall).mBox->mC8C)->Update(); }
extern "C" void ee_screenscpp_sb_verify_armor_count()
{ ESVerify(__FILE__, 367, "this->mArmorAttributeCount <= keeuifmgsbMaxAttributes"); }
extern "C" void ee_screenscpp_sb_verify_count()
{ ESVerify(__FILE__, 708, "this->mAttributeCount <= keeuifmgsbMaxAttributes"); }
extern "C" uintptr_t ee_screenscpp_sb_main_background(uint32_t inId)
{ return (uintptr_t)gUIManager->GetMainGameForm()->GetBackgroundByID(inId); }
extern "C" void ee_screenscpp_sb_background_set_visible(uintptr_t inBackground, bool inVisible)
{ reinterpret_cast<UIBackground*>(inBackground)->SetVisibility(inVisible); }
extern "C" bool ee_screenscpp_sb_value_differs(uintptr_t inCall, int64_t inCtl)
{ return SBC(inCall).mText != SBBAR(inCall, inCtl)->GetValueText(); }
extern "C" bool ee_screenscpp_sb_value2_differs(uintptr_t inCall, int64_t inCtl)
{ return SBC(inCall).mText != SBBAR(inCall, inCtl)->GetValue2Text(); }
extern "C" void ee_screenscpp_sb_set_value(uintptr_t inCall, int64_t inCtl)
{ SBBAR(inCall, inCtl)->mValue->SetText(SBC(inCall).mText); }
extern "C" void ee_screenscpp_sb_set_value2(uintptr_t inCall, int64_t inCtl)
{ SBBAR(inCall, inCtl)->mValue2->SetText(SBC(inCall).mText); }
extern "C" int64_t ee_screenscpp_sb_class_armor(uintptr_t inCall, int64_t inIndex)
{ return SBC(inCall).mUnit->GetUnitClass()->mArmor[inIndex]; }
extern "C" int64_t ee_screenscpp_sb_stats_class_armor(uintptr_t inCall, int64_t inIndex)
{ return SBC(inCall).mUnit->mStats->mUnitClass->mArmor[inIndex]; }
extern "C" float ee_screenscpp_sb_stats_armor(uintptr_t inCall, int64_t inIndex)
{ return SBC(inCall).mUnit->mStats->mArmor[inIndex]; }
extern "C" int64_t ee_screenscpp_sb_complex_unit_type(uintptr_t inCall)
{ return SBC(inCall).mComplex->GetUnitClass()->mUnitType; }
extern "C" int64_t ee_screenscpp_sb_count_queued(uintptr_t inCall, int64_t inImprovement, int64_t inKind)
{ return SBC(inCall).mComplex->GetPlayer()->CountQueuedImprovements((unsigned long)inImprovement, (long)inKind); }
extern "C" void ee_screenscpp_sb_new_bitmap(int64_t inIcon)
{ EEUIFMGStatusBox::sIconBitmaps[inIcon].mBitmap = new GEBitmap; }
extern "C" uint32_t ee_screenscpp_sb_primary_width()
{ return (uint32_t)gRasterizer->GetPrimarySurface(gScreenIndex)->GetWidth(); }
extern "C" void ee_screenscpp_sb_set_description(uintptr_t inCall, int32_t inDrop)
{ SBC(inCall).mDescription = GETextureDescription{1, 0, gTexturePixelDepth, inDrop, false, false, true, false}; }
extern "C" uintptr_t ee_screenscpp_sb_description(uintptr_t inCall) { return (uintptr_t)&SBC(inCall).mDescription; }
extern "C" void ee_screenscpp_sb_set_texture(uintptr_t inCall, int64_t inIndex, uintptr_t inTexture)
{ SBC(inCall).mTextures[inIndex] = reinterpret_cast<GETexture*>(inTexture); }
extern "C" void ee_screenscpp_sb_set_rect(int64_t inIcon, float inLeft, float inTop, float inRight, float inBottom)
{ EEUIFMGStatusBox::sIconBitmaps[inIcon].mRectangle = GERectangle(inLeft, inTop, inRight, inBottom); }
extern "C" uint32_t ee_screenscpp_sb_texture_width(uintptr_t inCall, int64_t inIndex)
{ return (uint32_t)SBC(inCall).mTextures[inIndex]->GetSurface()->GetWidth(); }
extern "C" uint32_t ee_screenscpp_sb_texture_height(uintptr_t inCall, int64_t inIndex)
{ return (uint32_t)SBC(inCall).mTextures[inIndex]->GetSurface()->GetHeight(); }
extern "C" void ee_screenscpp_sb_bitmap_set_texture(int64_t inIcon, uintptr_t inCall, int64_t inIndex)
{ EEUIFMGStatusBox::sIconBitmaps[inIcon].mBitmap->SetTexture(SBC(inCall).mTextures[inIndex], gScreenIndex); }
extern "C" void ee_screenscpp_sb_bitmap_set_coordinates(int64_t inIcon, float inU0, float inV0, float inU1, float inV1)
{ EEUIFMGStatusBox::sIconBitmaps[inIcon].mBitmap->SetTextureCoordinates(GETextureCoordinate(inU0, inV0), GETextureCoordinate(inU1, inV0), GETextureCoordinate(inU1, inV1), GETextureCoordinate(inU0, inV1)); }
extern "C" void ee_screenscpp_sb_bitmap_scale(int64_t inIcon, float inX, float inY)
{ EEUIFMGStatusBox::sIconBitmaps[inIcon].mBitmap->SetXYScale(inX, inY); }
extern "C" void ee_screenscpp_sb_icons_set_loaded(bool inLoaded) { sIconsLoaded = inLoaded; }
extern "C" uintptr_t ee_screenscpp_sb_bitmap(int64_t inIcon)
{ return (uintptr_t)EEUIFMGStatusBox::sIconBitmaps[inIcon].mBitmap; }
extern "C" void ee_screenscpp_sb_bitmap_dereference(int64_t inIcon)
{ EEUIFMGStatusBox::sIconBitmaps[inIcon].mBitmap->Dereference(); }
extern "C" void ee_screenscpp_sb_bitmap_clear(int64_t inIcon) { EEUIFMGStatusBox::sIconBitmaps[inIcon].mBitmap = NULL; }
extern "C" void ee_screenscpp_sb_set_reload_description(int32_t inDrop)
{ sSBReload = GETextureDescription{1, 0, gTexturePixelDepth, inDrop, false, false, true, false}; }
extern "C" int64_t ee_screenscpp_sb_reload(int64_t inIndex)
{ return gTextureManager->ReloadTexture(UString(kSBTextureNames[inIndex]), sSBReload); }
extern "C" void ee_screenscpp_sb_bar_bitmap(uintptr_t inBar, uintptr_t inBitmap)
{ reinterpret_cast<EEUIFMGStatusBar*>(inBar)->mIcon->SetBitmap(reinterpret_cast<GEBitmap*>(inBitmap)); }
extern "C" void ee_screenscpp_sb_bar_stretch(uintptr_t inBar, uintptr_t inBitmap, uintptr_t inRectangle)
{ reinterpret_cast<GEBitmap*>(inBitmap)->ComputeStretchedCoordsAndScale(reinterpret_cast<EEUIFMGStatusBar*>(inBar)->mIcon->GetRectangle(), reinterpret_cast<EEUIFMGStatusBar*>(inBar)->mIcon->GetRectangle(), *reinterpret_cast<const GERectangle*>(inRectangle), reinterpret_cast<EEUIFMGStatusBar*>(inBar)->GetScreenIndex()); }
extern "C" void ee_screenscpp_sb_bar_dirty(uintptr_t inBar)
{ reinterpret_cast<EEUIFMGStatusBar*>(inBar)->SetDirty(true); }

// (FillArmorAttributes 0x66ae16, UpdateArmorTexts 0x66c91d and GetAttributeText 0x66cb84 are Rust's: status_box.rs
// fill_armor_attributes, update_armor_texts; hud.rs attribute_text)

// Fills the status box for a unit: name (gate lock state), civilization, hit point / power / flight / garrison
// bars, the improvable attribute buttons and the background panels; clears it without a unit
void EEUIFMGStatusBox::Fill(EEUnit* inUnit)
{
    EEUIFMGStatusBoxCall theCall = {this, inUnit, NULL};
    ee_screens_status_box_fill(&theCall, inUnit != NULL);   // ee_screens status_box.rs fill
}

// Hides the status box's background panels (all but panel 0x7c)
void EEUIFMGStatusBox::HideBackgrounds()
{
    EEUIFMGStatusBoxCall theCall = {this, NULL, NULL};
    ee_screens_status_box_hide_backgrounds(&theCall);   // ee_screens status_box.rs hide_backgrounds
}

// Loads the three unit improvement textures and points the 50 attribute icons at their cells
void EEUIFMGStatusBox::LoadIcons()
{
    EEUIFMGStatusBoxCall theCall = {NULL, NULL, NULL};
    ee_screens_status_box_load_icons(&theCall);   // ee_screens status_box.rs load_icons (the cells, the texture choice)
}

// Releases the 50 attribute icon bitmaps
void EEUIFMGStatusBox::UnloadIcons()
{
    ee_screens_sb_unload_icons();   // (status_box_calls.rs)
}
// Refreshes the status box texts for the unit: name, civilization, hit points, attributes, cargo, flight time
// or power, garrison, and the resource it carries
void EEUIFMGStatusBox::Refresh(EEUnit* inUnit)
{
    EEUIFMGStatusBoxCall theCall = {this, inUnit, NULL};
    ee_screens_status_box_refresh(&theCall);   // ee_screens status_box.rs refresh
}

// Shows on each visible improvement button how many of its improvements the player has queued
void EEUIFMGStatusBox::UpdateQueuedCounts(EEComplexUnit* inUnit, long inImprovement)
{
    EEUIFMGStatusBoxCall theCall = {this, NULL, inUnit};
    ee_screens_status_box_update_queued(&theCall);   // ee_screens status_box.rs update_queued_counts
}

// Reloads the three unit improvement textures (after a device loss); returns the first error
long EEUIFMGStatusBox::ReloadIcons()
{
    return (long)ee_screens_sb_reload_icons();   // (status_box_calls.rs)
}

EEUIFMGStatusBox::~EEUIFMGStatusBox()
{
}

// the icons' load, unload and reload as the game's media calls them (Empire Earth.cpp: ee/EEGameUI.h)
#include "ee/EEGameUI.h"
static const bool sIconsRegistered = (gEEGameUI.mLoadIcons = EEUIFMGStatusBox::LoadIcons,
                                      gEEGameUI.mUnloadIcons = EEUIFMGStatusBox::UnloadIcons,
                                      gEEGameUI.mReloadIcons = EEUIFMGStatusBox::ReloadIcons, true);
