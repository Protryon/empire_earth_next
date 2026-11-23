// C:\EEX\Empire Earth\User Interface Thread\EEUIFMGStatusButton.cpp (__FILE__ 0x4cde4c, ESVERIFY line 195). The
// main game form's command buttons (RTTI EEUIFMGStatusButton > UIButton; vtable 0x44ad98): 0x26de97..0x271319 (ctor,
// ??_G, dtor, the overrides and members, the EEM* inline ctors after their first users, EEUIManager's two main-group
// setters, EEUIEndCinematicModeEvent::ProcessEvent, the main game form's button tests 0x26fc22..0x27115c and its
// UpdateKeyboard/UpdateMouse). Unit flags: the exe module set.
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
#include "lle/GEBitmap.h"
#include "lle/Input System.h"
#include "lle/UIHotKey.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/UIControlsTable.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "lle/UResourceFile.h"
#include "lle/GEText.h"
#include "ee/EmpireEarth.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIFMGMainGroup.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EEButtonObject.h"
#include "ee/EEUnitSetManager.h"
#include "lle/USingleton.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEPhysics.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEUIFMGStatusButton.h"
#include "ee/EEStringIDs.h"
#include "rust/game/screens.h"   // ee_screens_button_*, ee_screens_command_*: the command panel (ee_screens command_panel.rs)
extern float& gSavedGameSpeed;               // 0x5318b0 (the cinematic's end time) (provisional name)
extern unsigned short gFortressUnloadCost;   // 0x4c8fa4 (= 100, a word: EEUCFortress.cpp defines it)
extern long& gGameResources;                 // 0x53123c (4: a game kind that delays JIT manufacturing) (provisional name)
// 0x20d571 / 0x20da86: EEGetConversionCost / EECanConvert (ee/EEAreaUtilities.h; EEUCBasicPriest.cpp)

long EEGetTradePrice(EEPlayer* inPlayer, long inResource, unsigned char inBuy);  // 0x23cc42 (EEMessage.cpp's)
// Creates a main game command button with its icon, dither and queue count text
EEUIFMGStatusButton::EEUIFMGStatusButton(UIManager* inManager, const GERectangle& inRectangle, UIControlsTable& inTable)
    : UIButton(inTable.GetID(), NULL, NULL, NULL, inRectangle, inManager->GetFormByID(inTable.GetFormID()),
               inTable.GetFormIndex(), 0),
      EeScreensStatusButtonFields(), EeScreensStatusButtonObjects()
{
    ee_screens_status_button_fields(this, static_cast<EeScreensStatusButtonFields*>(this));
    ee_screens_sbt_construct((uintptr_t)this, (uintptr_t)inManager, (uintptr_t)&inRectangle, (uintptr_t)&inTable);   // (status_button_calls.rs)
}

// (the command buttons' part of the main game form's host, the original's EEStatusButtonOp, is Rust's over the
// shims below: ee_screens status_button_calls.rs)
// (status_button.rs' constants equal the headers': hud_cpp_consts_tests.rs)

// Builds the button's help text: market trade prices, command help (with conversion cost for priests) and
// hot key, or a tech's name, cost and description (ee_screens status_button.rs help_string)
bool EEUIFMGStatusButton::GetHelpString(UWideString& outString) const
{
    return static_cast<EEUIFMainGame*>(this->GetForm())->ButtonCall(const_cast<EEUIFMGStatusButton*>(this), EE_SCREENS_SB_DO_HELP, &outString) != 0;
}

// Runs a command button on a right release, or cancels one queued tech (Shift: five, for units only)
// (ee_screens status_button.rs)
long EEUIFMGStatusButton::MouseRButtonUp()
{
    return (long)static_cast<EEUIFMainGame*>(this->GetForm())->ButtonCall(this, EE_SCREENS_SB_DO_RIGHT_UP);   // (0)
}

// the right press's and tab's shims (ee_screens hud_more.rs)
#define SBM(b) reinterpret_cast<EEUIFMGStatusButton*>(b)
extern "C" bool ee_screenscpp_sbm_is_command(uintptr_t inButton) { return SBM(inButton)->mIsCommand; }
extern "C" void ee_screenscpp_sbm_capture_mouse(uintptr_t inButton) { SBM(inButton)->BaseCaptureMouse(); }
extern "C" void ee_screenscpp_sbm_base_button_down(uintptr_t inButton) { SBM(inButton)->BaseButtonDown(); }
extern "C" uint32_t ee_screenscpp_sbm_state(uintptr_t inButton) { return (uint32_t)SBM(inButton)->GetState(); }
extern "C" void ee_screenscpp_sbm_hot_key_up(uintptr_t inButton) { SBM(inButton)->HotKeyUp(); }
extern "C" void ee_screenscpp_sbm_base_tabbed_from(uintptr_t inButton) { SBM(inButton)->BaseTabbedFrom(); }

// Ends the cinematic at its saved end time (sends the end-cinematic message)
// (EEMEndCinematic takes the end time as a float)
long EEUIEndCinematicModeEvent::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageEndCinematic, {gSavedGameSpeed}), kSlotNone);
    return kErrorOK;
}

// ---- the one-statement shims of ee_screens status_button_calls.rs (UI lane round 8): each one call, no logic
#include "ee/EEScreensShims.h"
#define SBR(x) reinterpret_cast<EEUIFMGFormCall*>(x)
#define SBRB(x) SBR(x)->mButton
#define SBRF(x) static_cast<EEUIFMainGame*>(SBR(x)->mButton->GetForm())
#define SBB(x) reinterpret_cast<EEUIFMGStatusButton*>(x)
#define SBU(x) reinterpret_cast<EEComplexUnit*>(x)
#define SBN(x) reinterpret_cast<EETechTreeNode*>(x)
#define SBK(x) reinterpret_cast<EEUnitClass*>(x)
#define SBM(x) reinterpret_cast<UIManager*>(x)
#define SBT(x) (*reinterpret_cast<UIControlsTable*>(x))
extern "C" uintptr_t ee_screenscpp_sbt_unit(int64_t inIndex)
{ return (uintptr_t)gWorld.GetCurrentPlayer()->mSelectedGroup->GetUnit(inIndex); }
extern "C" int64_t ee_screenscpp_sbt_unit_get_power(uintptr_t inUnit) { return SBU(inUnit)->GetPower(); }
extern "C" int64_t ee_screenscpp_sbt_unit_power(uintptr_t inUnit) { return (long)SBU(inUnit)->mPower; }
extern "C" bool ee_screenscpp_sbt_unit_cloaked(uintptr_t inUnit) { return SBU(inUnit)->IsCloaked(); }
extern "C" bool ee_screenscpp_sbt_unit_is_priest(uintptr_t inUnit) { return SBU(inUnit)->GetUnitClass()->IsPriest(); }
extern "C" int64_t ee_screenscpp_sbt_conversion_cost(uintptr_t inUnit)
{ return EEGetConversionCost(SBU(inUnit)->mComplexUnit); }
extern "C" int64_t ee_screenscpp_sbt_unload_cost() { return gFortressUnloadCost; }
extern "C" int64_t ee_screenscpp_sbt_first_category(uintptr_t inRec)
{ return SBR(inRec)->mFirst->GetUnitClass()->mCategory; }
extern "C" int64_t ee_screenscpp_sbt_first_type(uintptr_t inRec)
{ return SBR(inRec)->mFirst->GetUnitClass()->mUnitType; }
extern "C" void ee_screenscpp_sbt_send_unload(uintptr_t inRec)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageUnloadTransport, {SBR(inRec)->mFirst->mUnitID, SBR(inRec)->mFirst->GetTileX(), SBR(inRec)->mFirst->GetTileY()}), kSlotNone); }
extern "C" void ee_screenscpp_sbt_send_research(uintptr_t inRec, bool inShift)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageResearchTechnology, {SBRB(inRec)->mUnitID, SBRB(inRec)->mTechnology, inShift}), kSlotNone); }
extern "C" void ee_screenscpp_sbt_send_remove(uintptr_t inRec, bool inShift)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageRemoveTechnology, {SBRB(inRec)->mUnitID, SBRB(inRec)->mTechnology, false, inShift}), kSlotNone); }
extern "C" void ee_screenscpp_sbt_start_patrol(uintptr_t inRec) { SBRF(inRec)->StartPatrol(); }
extern "C" void ee_screenscpp_sbt_start_flare(uintptr_t inRec) { SBRF(inRec)->StartFlare(); }
extern "C" void ee_screenscpp_sbt_build_unit(uintptr_t inRec)
{ SBRF(inRec)->BuildUnit(SBRB(inRec)->mUnitID, SBRB(inRec)->mTechnology); }
extern "C" void ee_screenscpp_sbt_start_wall(uintptr_t inRec) { SBRF(inRec)->StartWall(SBRB(inRec)->mTechnology); }
extern "C" void ee_screenscpp_sbt_start_gate(uintptr_t inRec) { SBRF(inRec)->StartGate(SBRB(inRec)->mTechnology); }
extern "C" void ee_screenscpp_sbt_start_calamity(uintptr_t inRec)
{ SBRF(inRec)->StartCalamity(SBRB(inRec)->mTechnology); }
extern "C" int64_t ee_screenscpp_sbt_game_resources() { return gGameResources; }
extern "C" float ee_screenscpp_sbt_game_clock() { return gPhysics->GetGameClock(); }
extern "C" int64_t ee_screenscpp_sbt_world_long(int64_t inConstant)
{ return gWorldTable.GetLongValue((EEWorldConstant)inConstant); }
extern "C" void ee_screenscpp_sbt_string_clear() { EEScratch<UWideString>(kScratchSBTString) = UWideString(); }
extern "C" void ee_screenscpp_sbt_string_load(int64_t inId) { gResource->GetString(inId, EEScratch<UWideString>(kScratchSBTString)); }
extern "C" void ee_screenscpp_sbt_chat_add_string() { gUserInterface->AddChatLogEntry(EEScratch<UWideString>(kScratchSBTString), 0); }
extern "C" bool ee_screenscpp_sbt_shift() { return UIControl::ShiftKeyPressed(); }
extern "C" bool ee_screenscpp_sbt_is_command(uintptr_t inRec) { return SBRB(inRec)->mIsCommand; }
extern "C" uintptr_t ee_screenscpp_sbt_node(uintptr_t inRec)
{ return (uintptr_t)gWorld.GetCurrentPlayer()->GetTechTree()->Get(SBRB(inRec)->mTechnology); }
extern "C" int64_t ee_screenscpp_sbt_node_kind(uintptr_t inNode) { return SBN(inNode)->mData->mKind; }
extern "C" int64_t ee_screenscpp_sbt_node_class(uintptr_t inNode) { return SBN(inNode)->mData->mUnitClass; }
extern "C" int64_t ee_screenscpp_sbt_node_makes_unit(uintptr_t inNode) { return SBN(inNode)->mData->mMakesUnit; }
extern "C" int64_t ee_screenscpp_sbt_node_calamity(uintptr_t inNode) { return SBN(inNode)->mData->GetCalamity(); }
extern "C" int64_t ee_screenscpp_sbt_node_name_id(uintptr_t inNode) { return SBN(inNode)->mData->mNameID; }
extern "C" bool ee_screenscpp_sbt_node_contains(uintptr_t inNode, int64_t inUnit)
{ return SBN(inNode)->Contains(inUnit); }
extern "C" uintptr_t ee_screenscpp_sbt_node_calamity_class(uintptr_t inNode)
{ return (uintptr_t)USingleton<EECalamityManager>::GetInstance()->GetCalamityClass((EECalamityEnum)SBN(inNode)->mData->mCalamity); }
extern "C" uintptr_t ee_screenscpp_sbt_calamity_class(int64_t inCalamity)
{ return (uintptr_t)USingleton<EECalamityManager>::GetInstance()->GetCalamityClass((EECalamityEnum)inCalamity); }
extern "C" int64_t ee_screenscpp_sbt_calamity_cost(uintptr_t inCalamity)
{ return reinterpret_cast<EECalamityClass*>(inCalamity)->GetCost(); }
extern "C" uintptr_t ee_screenscpp_sbt_class(int64_t inIndex) { return (uintptr_t)gUnitClassTable[inIndex]; }
extern "C" bool ee_screenscpp_sbt_class_is_land_building(uintptr_t inClass) { return SBK(inClass)->IsLandBuilding(); }
extern "C" bool ee_screenscpp_sbt_class_naval_building(uintptr_t inClass)
{ return SBK(inClass)->mCategory == kUnitCategoryNavalBuilding; }
extern "C" bool ee_screenscpp_sbt_class_upgrade_named(uintptr_t inClass)
{ return gWorld.GetCurrentPlayer()->mUnitClassUpgrades[SBK(inClass)->mUnitType]->mName.GetLength() != 0; }
extern "C" int64_t ee_screenscpp_sbt_class_population(uintptr_t inClass) { return SBK(inClass)->GetPopulationCost(); }
extern "C" bool ee_screenscpp_sbt_can_produce(uintptr_t inRec, int64_t inCount)
{ return gWorld.GetCurrentPlayer()->CanProduce(SBRB(inRec)->mTechnology, SBRB(inRec)->mUnitType, inCount, true, 3, &SBR(inRec)->mText) == true; }
extern "C" bool ee_screenscpp_sbt_can_produce_quiet(uintptr_t inRec)
{ return gWorld.GetCurrentPlayer()->CanProduce(SBRB(inRec)->mTechnology, SBRB(inRec)->mUnitType, 1, true, 3, NULL) == true; }
extern "C" void ee_screenscpp_sbt_button_up(uintptr_t inRec) { SBRB(inRec)->ButtonUp(); }
extern "C" bool ee_screenscpp_sbt_mouse_inside(uintptr_t inRec) { return SBRB(inRec)->IsMouseInside(); }
extern "C" void ee_screenscpp_sbt_count_text(uintptr_t inRec, EeScreensShimText inText)
{ static_cast<GEText*>(SBRB(inRec)->mBuildQueue)->SetText(EEShimText(inText)); }
extern "C" bool ee_screenscpp_sbt_in_fanatic_set(int64_t inUnit)
{ return gUnitSetManager.GetSet(0x2f)->Contains((unsigned long)inUnit); }
extern "C" void ee_screenscpp_sbt_text_string(uintptr_t inRec, int64_t inId)
{ gResource->GetString((unsigned long)inId, SBR(inRec)->mText); }
extern "C" uintptr_t ee_screenscpp_sbt_button_object_of(uintptr_t inRec)
{ return (uintptr_t)EEButtonObject::Get(SBRB(inRec)->mButton); }
extern "C" uintptr_t ee_screenscpp_sbt_object_hot_key(uintptr_t inObject)
{ return (uintptr_t)reinterpret_cast<EEButtonObject*>(inObject)->GetHotKey(); }
extern "C" uintptr_t ee_screenscpp_sbt_node_hot_key(uintptr_t inNode) { return (uintptr_t)SBN(inNode)->GetHotKey(); }
extern "C" void ee_screenscpp_sbt_hot_key_name(uintptr_t inRec, uintptr_t inHotKey)
{ reinterpret_cast<UIHotKey*>(inHotKey)->GetHotKeyName(SBR(inRec)->mText); }
extern "C" void ee_screenscpp_sbt_text_upgrade_name(uintptr_t inRec, int64_t inClass)
{ SBR(inRec)->mText = gWorld.GetCurrentPlayer()->mUnitClassUpgrades[gUnitClassTable[inClass]->mUnitType]->mName; }
extern "C" void ee_screenscpp_sbt_text_node_cost(uintptr_t inRec, uintptr_t inNode)
{ SBN(inNode)->GetCostString(SBR(inRec)->mText, SBRB(inRec)->mUnitType, 1.0f); }
extern "C" int64_t ee_screenscpp_sbt_text_length(uintptr_t inRec) { return (int64_t)strlen(SBR(inRec)->mText); }
extern "C" int64_t ee_screenscpp_sbt_trade_price(int64_t inResource, bool inBuy)
{ return EEGetTradePrice(gWorld.GetCurrentPlayer(), (long)inResource, inBuy); }
extern "C" bool ee_screenscpp_sbt_has_object(uintptr_t inRec) { return static_cast<EEButtonObject*>(SBRB(inRec)->mButtonObject) != NULL; }
extern "C" void ee_screenscpp_sbt_set_draw_icon(uintptr_t inRec, bool inDraw) { SBRB(inRec)->mDrawIcon = inDraw; }
extern "C" void ee_screenscpp_sbt_object_pressed_icon(uintptr_t inRec)
{ static_cast<EEButtonObject*>(SBRB(inRec)->mButtonObject)->SetPressedBitmap(static_cast<GEBitmap*>(SBRB(inRec)->mIcon), SBRB(inRec)->GetRectangle(), SBRB(inRec)->GetScreenIndex()); }
extern "C" void ee_screenscpp_sbt_object_face_icon(uintptr_t inRec)
{ static_cast<EEButtonObject*>(SBRB(inRec)->mButtonObject)->SetFaceBitmap(static_cast<GEBitmap*>(SBRB(inRec)->mIcon), SBRB(inRec)->GetRectangle(), SBRB(inRec)->GetScreenIndex()); }
extern "C" void ee_screenscpp_sbt_main_group_l1c(int64_t inValue)
{ static_cast<EEUIFMGMainGroup*>(gUIManager->GetMainGameForm()->mGroups[0])->mL1C = (long)inValue; }
extern "C" void ee_screenscpp_sbt_main_group_reset()
{ static_cast<EEUIFMGMainGroup*>(gUIManager->GetMainGameForm()->mGroups[0])->mReset = true; }
extern "C" uintptr_t ee_screenscpp_sbt_object(uintptr_t inButton) { return (uintptr_t)static_cast<EEButtonObject*>(SBB(inButton)->mButtonObject); }
extern "C" void ee_screenscpp_sbt_object_release(uintptr_t inButton) { static_cast<EEButtonObject*>(SBB(inButton)->mButtonObject)->Release(); }
extern "C" void ee_screenscpp_sbt_object_clear(uintptr_t inButton) { SBB(inButton)->mButtonObject = NULL; }
extern "C" void ee_screenscpp_sbt_object_acquire(uintptr_t inButton) { static_cast<EEButtonObject*>(SBB(inButton)->mButtonObject)->Acquire(); }
extern "C" void ee_screenscpp_sbt_set_object(uintptr_t inButton, uint32_t inIndex)
{ SBB(inButton)->mButtonObject = EEButtonObject::Get(inIndex); }
extern "C" void ee_screenscpp_sbt_set_node_object(uintptr_t inButton, int64_t inTechnology)
{ SBB(inButton)->mButtonObject = gWorld.GetCurrentPlayer()->GetTechTree()->Get((long)inTechnology)->mButton; }
extern "C" void ee_screenscpp_sbt_set_count(uintptr_t inButton, int64_t inCount)
{ SBB(inButton)->mCount = (long)inCount; }
extern "C" void ee_screenscpp_sbt_set_command(uintptr_t inButton, bool inCommand)
{ SBB(inButton)->mIsCommand = inCommand; }
extern "C" void ee_screenscpp_sbt_set_button(uintptr_t inButton, uint32_t inIndex) { SBB(inButton)->mButton = inIndex; }
extern "C" void ee_screenscpp_sbt_set_technology(uintptr_t inButton, int64_t inTechnology)
{ SBB(inButton)->mTechnology = (long)inTechnology; }
extern "C" void ee_screenscpp_sbt_set_unit_type(uintptr_t inButton, uint32_t inKind)
{ SBB(inButton)->mUnitType = inKind; }
extern "C" void ee_screenscpp_sbt_set_unit_id(uintptr_t inButton, uint32_t inId) { SBB(inButton)->mUnitID = inId; }
extern "C" void ee_screenscpp_sbt_icon_dereference(uintptr_t inButton) { static_cast<GEBitmap*>(SBB(inButton)->mIcon)->Dereference(); }
extern "C" void ee_screenscpp_sbt_queue_delete(uintptr_t inButton) { delete static_cast<GEText*>(SBB(inButton)->mBuildQueue); }
extern "C" void ee_screenscpp_sbt_dither_dereference(uintptr_t inButton) { static_cast<GEBitmap*>(SBB(inButton)->mDither)->Dereference(); }
extern "C" bool ee_screenscpp_sbt_draw_icon(uintptr_t inButton) { return SBB(inButton)->mDrawIcon; }
extern "C" void ee_screenscpp_sbt_icon_white(uintptr_t inButton)
{ static_cast<GEBitmap*>(SBB(inButton)->mIcon)->SetColor(GEColor(1.0f, 1.0f, 1.0f)); }
extern "C" void ee_screenscpp_sbt_draw_icon_bitmap(uintptr_t inButton)
{ SBB(inButton)->GetRasterizer()->Draw(static_cast<GEBitmap*>(SBB(inButton)->mIcon), SBB(inButton)->GetScreenIndex()); }
extern "C" bool ee_screenscpp_sbt_available(uintptr_t inButton) { return SBB(inButton)->mAvailable; }
extern "C" void ee_screenscpp_sbt_dither_at_drawn(uintptr_t inButton)
{ static_cast<GEBitmap*>(SBB(inButton)->mDither)->SetLocation(SBB(inButton)->GetCopiedDrawRectangle().GetTopLeft()); }
extern "C" void ee_screenscpp_sbt_draw_dither(uintptr_t inButton)
{ SBB(inButton)->GetRasterizer()->Draw(static_cast<GEBitmap*>(SBB(inButton)->mDither), SBB(inButton)->GetScreenIndex()); }
extern "C" int64_t ee_screenscpp_sbt_base_draw_polygons(uintptr_t inButton)
{ return SBB(inButton)->UIButton::DrawPolygons(); }
extern "C" bool ee_screenscpp_sbt_show_count(uintptr_t inButton) { return SBB(inButton)->mShowCount; }
extern "C" void ee_screenscpp_sbt_draw_count(uintptr_t inButton)
{ SBB(inButton)->GetForm()->DrawClippedText(SBB(inButton)->GetRectangle(), static_cast<GEText*>(SBB(inButton)->mBuildQueue), SBB(inButton)); }
extern "C" int64_t ee_screenscpp_sbt_base_draw_text(uintptr_t inButton) { return SBB(inButton)->UIButton::DrawTextA(); }
extern "C" bool ee_screenscpp_sbt_pressed(uintptr_t inButton)
{ return SBB(inButton)->GetState() == kButtonStatePressed; }
extern "C" void ee_screenscpp_sbt_up(uintptr_t inButton) { SBB(inButton)->ButtonUp(); }
extern "C" void ee_screenscpp_sbt_icon_at(uintptr_t inButton, float inX, float inY)
{ static_cast<GEBitmap*>(SBB(inButton)->mIcon)->SetLocation(GE2DPoint(inX, inY)); }
extern "C" void ee_screenscpp_sbt_queue_at(uintptr_t inButton, float inX, float inY)
{ static_cast<GEText*>(SBB(inButton)->mBuildQueue)->SetLocation(GE2DPoint(inX, inY)); }
extern "C" void ee_screenscpp_sbt_base_set_location(uintptr_t inButton, float inX, float inY)
{ SBB(inButton)->UIButton::SetLocation(GE2DPoint(inX, inY)); }
extern "C" uintptr_t ee_screenscpp_sbt_normal_bitmap(uintptr_t inButton, uintptr_t inManager, uintptr_t inTable)
{ return (uintptr_t)SBT(inTable).CreateNormalBitmap(SBM(inManager), SBB(inButton)->GetRectangle()); }
extern "C" uintptr_t ee_screenscpp_sbt_pressed_bitmap(uintptr_t inButton, uintptr_t inManager, uintptr_t inTable)
{ return (uintptr_t)SBT(inTable).CreatePressedBitmap(SBM(inManager), SBB(inButton)->GetRectangle()); }
extern "C" void ee_screenscpp_sbt_set_pressed_bitmap(uintptr_t inButton, uintptr_t inBitmap)
{ SBB(inButton)->SetPressedBitmap(reinterpret_cast<GEBitmap*>(inBitmap)); }
extern "C" void ee_screenscpp_sbt_set_normal_bitmap(uintptr_t inButton, uintptr_t inBitmap)
{ SBB(inButton)->SetNormalBitmap(reinterpret_cast<GEBitmap*>(inBitmap)); }
extern "C" void ee_screenscpp_sbt_set_f13x(uintptr_t inButton, float inValue)
{ SBB(inButton)->mF138 = SBB(inButton)->mF134 = inValue; }
extern "C" void ee_screenscpp_sbt_icon_new(uintptr_t inButton) { SBB(inButton)->mIcon = new GEBitmap; }
extern "C" float ee_screenscpp_sbt_left(uintptr_t inButton) { return SBB(inButton)->GetRectangle().GetLeft(); }
extern "C" float ee_screenscpp_sbt_top(uintptr_t inButton) { return SBB(inButton)->GetRectangle().GetTop(); }
extern "C" void ee_screenscpp_sbt_queue_new(uintptr_t inButton, uintptr_t inTable)
{ SBB(inButton)->mBuildQueue = new GEText(SBT(inTable).GetFont()); }
extern "C" void ee_screenscpp_sbt_queue_verify(uintptr_t inButton) { ESVERIFY_AT(static_cast<GEText*>(SBB(inButton)->mBuildQueue), 195); }
extern "C" void ee_screenscpp_sbt_queue_setup(uintptr_t inButton, uintptr_t inTable)
{ SBT(inTable).SetupGEText(static_cast<GEText*>(SBB(inButton)->mBuildQueue)); }
extern "C" uintptr_t ee_screenscpp_sbt_load_dither(uintptr_t inManager)
{ return (uintptr_t)SBM(inManager)->GetRasterizer()->GetTextureManager(SBM(inManager)->GetScreen())->LoadTexture("textures\\Dither", GETextureDescription{1, 0, SBM(inManager)->GetUITextureBitDepth(), 0, false, false, true, true}); }
extern "C" void ee_screenscpp_sbt_dither_new(uintptr_t inButton) { SBB(inButton)->mDither = new GEBitmap; }
extern "C" void ee_screenscpp_sbt_dither_texture(uintptr_t inButton, uintptr_t inTexture, uintptr_t inManager)
{ static_cast<GEBitmap*>(SBB(inButton)->mDither)->SetTexture(reinterpret_cast<GETexture*>(inTexture), SBM(inManager)->GetScreen()); }
extern "C" float ee_screenscpp_sbt_texture_width(uintptr_t inTexture)
{ return (float)reinterpret_cast<GETexture*>(inTexture)->GetSurface()->GetWidth(); }
extern "C" float ee_screenscpp_sbt_texture_height(uintptr_t inTexture)
{ return (float)reinterpret_cast<GETexture*>(inTexture)->GetSurface()->GetHeight(); }
extern "C" void ee_screenscpp_sbt_dither_stretch(uintptr_t inButton, uintptr_t inRectangle, float inWidth, float inHeight, uintptr_t inManager)
{ static_cast<GEBitmap*>(SBB(inButton)->mDither)->ComputeStretchedCoordsAndScale(*reinterpret_cast<const GERectangle*>(inRectangle), *reinterpret_cast<const GERectangle*>(inRectangle), GERectangle(0.0f, 0.0f, inWidth, inHeight), SBM(inManager)->GetScreen()); }

void EEUIFMGStatusButton::ButtonUp()
{
    static_cast<EEUIFMainGame*>(this->GetForm())->ButtonCall(this, EE_SCREENS_SB_DO_UP);
}

long EEUIFMGStatusButton::CopyData()
{
    return (long)static_cast<EEUIFMainGame*>(this->GetForm())->ButtonCall(this, EE_SCREENS_SB_DO_COPY_DATA);
}
