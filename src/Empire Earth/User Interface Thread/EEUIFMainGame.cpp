// C:\EEX\Empire Earth\User Interface Thread\EEUIFMainGame.cpp (__FILE__ 0x262e73). The in-game form: 0x26272b..0x26783d
// (the default player names' init chain first). Unit flags: the exe's module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GERasterizer.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIFMGMainGroup.h"
#include "ee/EEAreaUtilities.h"
#include "lle/UResourceFile.h"
#include "lle/UITLScrolling.h"
#include "lle/UIBitmap.h"
#include "lle/UIBitmapBackground.h"
#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/EEGraphicObject.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEButtonObject.h"
#include "ee/EEUnitPicker.h"
#include "ee/EEGhostUnit.h"
#include "lle/NESynchronousServer.h"
#include "ee/EEServer.h"
#include "ee/EEUserInterface.h"
#include "ee/EEPhysics.h"
#include "ee/EEMouse.h"
#include "ee/EEARust.h"
#include "lle/USingleton.h"
#include "ee/EECalamityManager.h"
#include "ee/EECalamityClass.h"
#include "ee/EEScreenEvent.h"
#include "ee/EEUICSelectUnits.h"
#include "ee/EEUICCommandUnits.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEUIShell.h"
#include "ee/EEUIFMGUnitHack.h"
#include "ee/EEStringIDs.h"
// the command buttons' part of the host (EEUIFMGStatusButton.cpp)
bool EEFindGateWall(const EETileCoordinate& inTile, long& ioDirection);  // 0x2116a2 (EEUIBuildGate.cpp's)
void EEPreviewWall(std::vector<EETileCoordinate>& inTiles, long inTechnology, long inPlayer);
void EEPreviewPlacement(const GE2DPoint& inLocation);  // 0x204aba (the calamity's ghost at the mouse) (provisional name)
// EESetCivBuilderButtonBlinking 0x27fcf0: ee/EmpireEarth.h (Open passes mIsButtonBlinking)
void EEPostGraphicsAll();                   // 0x10fc0c (Open) (provisional name)
extern float& gMasterVolume;                 // 0x530830 (the master volume Open restores)

// the information panel's message is the scenario's dialog message (EEUserInterface's message 0x422 hands over an
// EESMDialogMessage; ShowInformation 0x26533e read it through the 32-bit offsets of a stand-in class, whose fields
// lined up there and not on x64: the portrait's class was read from the duration, the scroll time from the unit id)

// The unit hack whole in Rust (ee_screens unit_hack.rs): the original's members 0x2628e5 (ctor: the sorted
// class names), 0x262a91 (dtor), 0x262aa5 PlaceUnit, 0x262c6f FillComboBox, 0x262cdc LoadUnitData, 0x262d66
// Update, 0x262e29 GetSelection, and the two message ctors' COMDATs 0x262c24 (EEMHackANewUnit) and 0x262d32
// (EEMLoadUnitData). The host answers its ops (EE_SCREENS_UH_OP_*): the class table, the object and player lists,
// the keys, the mouse, the messages.
#include "ee/EEScreensShims.h"
// the moved cases' shims (ee_screens unit_hack_calls.rs asks them first)
#define UH_OBJECTS(x) static_cast<UIComboBox*>(static_cast<EEUIFMainGame*>(reinterpret_cast<void*>(x))->GetControl(kMainGameTempObjectList))
static ISEntity* EEKey(unsigned long inKey);
extern "C" bool ee_screenscpp_uh_class_exists(int32_t inIndex) { return gUnitClassTable[inIndex] != NULL; }
extern "C" bool ee_screenscpp_uh_class_enabled(int32_t inIndex) { return gUnitClassTable[inIndex]->mIsEnabled; }
extern "C" bool ee_screenscpp_uh_class_placeable(int32_t inIndex)
{ return gUnitClassTable[inIndex]->mIsPlayerPlaceable; }
extern "C" void ee_screenscpp_uh_class_name(int32_t inIndex, void* inName)
{ EEShimOut(inName, UWideString((const char*)gUnitClassTable[inIndex]->mName)); }
extern "C" void ee_screenscpp_uh_combo_append(uintptr_t inForm, EeScreensShimText inLine)
{ UH_OBJECTS(inForm)->AppendLine(EEShimText(inLine)); }
extern "C" uintptr_t ee_screenscpp_uh_combo_trigger(uintptr_t inForm)
{ return (uintptr_t)UH_OBJECTS(inForm)->GetTriggerEvent(kControlTriggerSelectionChanged); }
extern "C" void ee_screenscpp_uh_combo_set_trigger(uintptr_t inForm, uintptr_t inEvent)
{ UH_OBJECTS(inForm)->SetTriggerEvent(kControlTriggerSelectionChanged, reinterpret_cast<UIEvent*>(inEvent)); }
extern "C" void ee_screenscpp_uh_combo_select_first(uintptr_t inForm) { UH_OBJECTS(inForm)->SetCurrentSelection(0); }
extern "C" void ee_screenscpp_event_reference(uintptr_t inEvent) { reinterpret_cast<UIEvent*>(inEvent)->Reference(); }
extern "C" void ee_screenscpp_uh_combo_selection(uintptr_t inForm, unsigned long* inSelection)
{ UH_OBJECTS(inForm)->GetCurrentSelection(*inSelection); }
extern "C" int32_t ee_screenscpp_uh_object_list_selection(uintptr_t inForm, unsigned long* inSelection)
{ return (int32_t)UH_OBJECTS(inForm)->GetListBox()->GetCurrentSelection(*inSelection); }
extern "C" int32_t ee_screenscpp_uh_player_list_selection(uintptr_t inForm, unsigned long* inSelection)
{ return (int32_t)static_cast<UIComboBox*>(static_cast<EEUIFMainGame*>(reinterpret_cast<void*>(inForm))->GetControl(kMainGameTempPlayerList))->GetListBox()->GetCurrentSelection(*inSelection); }
extern "C" int32_t ee_screenscpp_uh_control_value() { return (int32_t)EEKey(kKeyControl)->GetValue(); }
extern "C" void ee_screenscpp_uh_send_place(int32_t inUnitType, float inX, float inY, int32_t inFlag)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageHackANewUnit, {(long)inUnitType, GE2DPoint(inX, inY), (unsigned char)inFlag}), kSlotNone); }
extern "C" bool ee_screenscpp_uh_point(float inX, float inY)
{ return EEPointUnitOrTile(GE2DPoint(inX, inY), EEScratch<EETile*>(kScratchShimTile), EEScratch<GE3DPoint>(kScratchUHPoint), true); }
extern "C" float ee_screenscpp_uh_point_x() { return EEScratch<GE3DPoint>(kScratchUHPoint).GetX(); }
extern "C" float ee_screenscpp_uh_point_y() { return EEScratch<GE3DPoint>(kScratchUHPoint).GetY(); }
extern "C" void ee_screenscpp_uh_point_reset() { EEScratch<GE3DPoint>(kScratchUHPoint) = GE3DPoint(); }
// the host's one-call ops (unit_hack_calls.rs; the host's switch until UI lane round 16)
extern "C" int32_t ee_screenscpp_uh_class_count() { return (int32_t)EEUnitClassCount(); }
extern "C" int32_t ee_screenscpp_uh_class_category(int32_t inIndex) { return gUnitClassTable[inIndex]->mCategory; }
extern "C" int32_t ee_screenscpp_uh_class_type(int32_t inIndex) { return gUnitClassTable[inIndex]->mUnitType; }
extern "C" void ee_screenscpp_uh_combo_clear(uintptr_t inForm) { UH_OBJECTS(inForm)->DeleteAllEntries(); }
extern "C" bool ee_screenscpp_uh_tile(float inX, float inY) { return gWorld.mMap->GetTile((long)inX, (long)inY) != NULL; }
extern "C" bool ee_screenscpp_uh_player_skipped(int32_t inPlayer)
{ return gWorld.mPlayers[inPlayer] == NULL || gWorld.mPlayers[inPlayer] == gWorld.GetCurrentPlayer(); }
extern "C" int32_t ee_screenscpp_uh_local_player() { return (int32_t)gWorld.GetCurrentPlayer()->mPlayerID; }
extern "C" void ee_screenscpp_uh_send_load(int32_t inUnitType)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageLoadUnitData, {inUnitType, true}), kSlotNone); }
extern "C" bool ee_screenscpp_uh_in_view(float inX, float inY) { return gViewport.GetViewRectangle().Contains(GE2DPoint(inX, inY)); }

EEUIFMGUnitHack::EEUIFMGUnitHack(EEUIFMainGame* inForm)
    : EeScreensUnitHackFields{inForm, ee_screens_unit_hack_new(inForm)} {}

// (EEUIFMGFormCall, one call of the form's work in Rust: ee/EEUIFMainGame.h; its strings follow the Rust record as
// hud_direct.rs FormCallHead has them)
static_assert(offsetof(EEUIFMGFormCall, mText) == sizeof(EeScreensFormCallObjects), "hud_direct.rs FormCallHead");

// (BuildGate and BuildWall: the placements call the build action, ee/EEARust.h)

// (BuildUnit and the start modes StartGate, StartWall, StartCalamity, StartFlare, StartPatrol: inline in
// ee/EEUIFMainGame.h)

// Switches the interface mode, clearing ghosts and ending a wall or gate placement

#include "ee/EETeam.h"
#include "ee/EEScenarioManager.h"
#include "ee/EECombatGoals.h"

// (main_form.rs' sounds and strings and mouse_modes.rs' farms and keys equal the headers': hud_cpp_consts_tests.rs)

// ---- the mouse modes' ops (ee_screens mouse_modes.rs, EE_SCREENS_MF_OP_MD_*): the picked unit, the first selected
// unit and the pointed tile held in the call; io.f[0..2] the mouse location where an op takes one
// ---- (the mouse modes' ops end)

// (the cursor's reads for one UpdateCursorKind: the Rust record EeScreensCursorRead, hud_direct.rs, untyped)

// The world's answers to the cursor's ops (EE_SCREENS_CURSOR_OP_*): the target, the selection by index, an air
// base's cargo by index; the positions' floats into outF
#define CR(x) static_cast<EeScreensCursorRead*>(reinterpret_cast<void*>(x))
extern "C" uintptr_t ee_screenscpp_cur_mouse_over(uintptr_t inCtx)
{ return (uintptr_t)static_cast<EEUIFMainGame*>(CR(inCtx)->mForm)->GetMouseOverControl(); }
extern "C" uintptr_t ee_screenscpp_cur_command_units(uintptr_t inCtx)
{ return (uintptr_t)static_cast<EEUIFMainGame*>(CR(inCtx)->mForm)->GetControl(kMainGameCommandUnits); }
extern "C" void ee_screenscpp_cur_pick(uintptr_t inCtx, uint32_t inFlags)
{ CR(inCtx)->mTarget = gUnitPicker.PickUnit(gMouse->mPosition, inFlags); }
extern "C" bool ee_screenscpp_cur_has_target(uintptr_t inCtx) { return static_cast<EEUnit*>(CR(inCtx)->mTarget) != NULL; }
extern "C" bool ee_screenscpp_cur_t_on_tile(uintptr_t inCtx) { return static_cast<EEUnit*>(CR(inCtx)->mTarget)->mTileNode != NULL; }
extern "C" bool ee_screenscpp_cur_t_disabled(uintptr_t inCtx) { return static_cast<EEUnit*>(CR(inCtx)->mTarget)->mComplexUnit->mIsDisabled; }
extern "C" bool ee_screenscpp_cur_t_protected(uintptr_t inCtx)
{ return static_cast<EEUnit*>(CR(inCtx)->mTarget)->mComplexUnit->IsProtected(); }
extern "C" bool ee_screenscpp_cur_t_has_complex(uintptr_t inCtx)
{ return static_cast<EEUnit*>(CR(inCtx)->mTarget)->GetComplexUnit() != NULL; }
extern "C" bool ee_screenscpp_cur_t_has_model(uintptr_t inCtx)
{ return static_cast<EEUnit*>(CR(inCtx)->mTarget)->GetModel()->mModel != NULL; }
extern "C" float ee_screenscpp_cur_t_scale(uintptr_t inCtx) { return static_cast<EEUnit*>(CR(inCtx)->mTarget)->mStats->GetScale(); }
extern "C" void ee_screenscpp_cur_t_default_point(uintptr_t inCtx, float* inPoint)
{ *reinterpret_cast<GE3DPoint*>(inPoint) = static_cast<EEUnit*>(CR(inCtx)->mTarget)->GetModel()->mDefaultPoint; }
extern "C" void ee_screenscpp_cur_t_place(uintptr_t inCtx, float inX, float inY, float inZ, float* inAim)
{ static_cast<EEUnit*>(CR(inCtx)->mTarget)->GetComplexUnit()->mPhysicalModel.GetLocation().Apply(GE3DPoint(inX, inY, inZ), *reinterpret_cast<GE3DPoint*>(inAim)); }
extern "C" int32_t ee_screenscpp_cur_t_tile_x(uintptr_t inCtx) { return (int32_t)static_cast<EEUnit*>(CR(inCtx)->mTarget)->GetTileX(); }
extern "C" int32_t ee_screenscpp_cur_t_tile_y(uintptr_t inCtx) { return (int32_t)static_cast<EEUnit*>(CR(inCtx)->mTarget)->GetTileY(); }
extern "C" float ee_screenscpp_cur_t_tile_height(uintptr_t inCtx) { return static_cast<EEUnit*>(CR(inCtx)->mTarget)->mTile->GetHeight(); }
extern "C" bool ee_screenscpp_cur_can_gather(uintptr_t inCtx, uint32_t inFlags)
{ return static_cast<EESelectedGroup*>(CR(inCtx)->mGroup)->FindUnitWithFlags(inFlags)->CanGather(static_cast<EEUnit*>(CR(inCtx)->mTarget), static_cast<EEUnit*>(CR(inCtx)->mTarget)->GetTileX(), static_cast<EEUnit*>(CR(inCtx)->mTarget)->GetTileY(), false, false); }
extern "C" void ee_screenscpp_cur_set_shielded(bool inShielded) { EEScratch<bool>(kScratchCurShielded) = inShielded; }
extern "C" bool ee_screenscpp_cur_convert(uintptr_t inCtx, uint32_t inIndex)
{ return EECanConvert(static_cast<EESelectedGroup*>(CR(inCtx)->mGroup)->GetUnit(inIndex), static_cast<EEUnit*>(CR(inCtx)->mTarget)->mComplexUnit, EEScratch<bool>(kScratchCurShielded)); }
extern "C" bool ee_screenscpp_cur_shielded() { return EEScratch<bool>(kScratchCurShielded); }
// UpdateCursorKind's reads (cursor_ffi.rs ee_screens_update_cursor_kind) and its result
extern "C" EEPlayer* ee_screenscpp_cur_local_player() { return gWorld.GetCurrentPlayer(); }
extern "C" EESelectedGroup* ee_screenscpp_cur_selected_group(EEPlayer* inPlayer) { return inPlayer->mSelectedGroup; }
extern "C" EEComplexUnit* ee_screenscpp_cur_group_first(EESelectedGroup* inGroup) { return inGroup->GetFirstUnit(); }
extern "C" void ee_screenscpp_cur_set_pointer(int32_t inKind) { gMouse->mPointer = inKind; }
// the host's one-call ops (cursor_calls.rs; the host's switch until UI lane round 16)
#define CRU(x, i) static_cast<EESelectedGroup*>(CR(x)->mGroup)->GetUnit(i)
#define CRT(x) static_cast<EEUnit*>(CR(x)->mTarget)
extern "C" int32_t ee_screenscpp_cur_flags(uintptr_t inCtx) { return (int32_t)static_cast<EESelectedGroup*>(CR(inCtx)->mGroup)->mKindFlags; }
extern "C" bool ee_screenscpp_cur_in_view() { return gRasterizer->GetViewport(gScreenIndex).GetViewRectangle().Contains(gMouse->mPosition); }
extern "C" void ee_screenscpp_cur_release(uintptr_t inCtx) { CRT(inCtx)->Dereference(); }
extern "C" bool ee_screenscpp_cur_seen(uintptr_t inCtx) { return EECombatIsSeenBy(CRT(inCtx), static_cast<EEPlayer*>(CR(inCtx)->mPlayer), true); }
extern "C" bool ee_screenscpp_cur_t_targetable(uintptr_t inCtx) { return ee_units_uu_targetable(0, (uint64_t)(uintptr_t)CRT(inCtx)->mUnitClass, 0); }
extern "C" bool ee_screenscpp_cur_can_heal(uintptr_t inCtx) { return EECanHeal(static_cast<EEComplexUnit*>(CR(inCtx)->mFirst), CRT(inCtx)->mComplexUnit); }
extern "C" bool ee_screenscpp_cur_sel_air_base(uintptr_t inCtx, uint32_t inA) { return CRU(inCtx, inA)->GetUnitClass()->IsAirBase(); }
extern "C" bool ee_screenscpp_cur_sel_crusader(uintptr_t inCtx, uint32_t inA) { return CRU(inCtx, inA)->mUnitClass->IsCrusader(); }
extern "C" bool ee_screenscpp_cur_sel_priest(uintptr_t inCtx, uint32_t inA) { return CRU(inCtx, inA)->mUnitClass->IsPriest(); }
extern "C" bool ee_screenscpp_cur_sel_can_attack(uintptr_t inCtx, uint32_t inA)
{ return CRU(inCtx, inA)->GetComplexUnit()->CanAttackClass(CRT(inCtx)->GetUnitClass()); }
extern "C" bool ee_screenscpp_cur_cargo_can_attack(uintptr_t inCtx, uint32_t inA, uint32_t inB)
{ return CRU(inCtx, inA)->GetComplexUnit()->mCargo[inB]->CanAttackClass(CRT(inCtx)->GetUnitClass()); }

// Tracks the mouse in wall placement (ee_screens mouse_modes.rs wall_update): a left click sets the start, then
// builds each segment and chains from its end; a right click cancels; moving previews the wall ghosts
void EEUIBuildWallUpdate(EEUIBuildWall* inWall, const GE2DPoint& inLocation)
{
    ee_screens_wall_update(EEUIFMGFormCall(gUIManager->GetMainGameForm(), inWall).Ptr(), inWall, inLocation.GetX(), inLocation.GetY());
}

// Tracks the mouse in gate placement (ee_screens mouse_modes.rs gate_update): a gate ghost on the wall under the
// mouse, built on a left click, cancelled on a right click
void EEUIBuildGateUpdate(EEUIBuildGate* inGate, const GE2DPoint& inLocation)
{
    ee_screens_gate_update(EEUIFMGFormCall(gUIManager->GetMainGameForm(), inGate).Ptr(), inGate, inLocation.GetX(), inLocation.GetY());
}

// ---- the shims of the moved cases (UI lane round 7: ee_game screens main_form_calls.rs, game_cpp.rs), each one
// C++ statement on the host's call record or the world
static EEUIFMGFormCall* R(uintptr_t inRec) { return reinterpret_cast<EEUIFMGFormCall*>(inRec); }
static UIControl* C(uintptr_t inControl) { return reinterpret_cast<UIControl*>(inControl); }
// the keyboard's key (NULL when the keyboard has none)
static ISEntity* EEKey(unsigned long inKey)
{
    ISEntity* theKey = NULL;
    return gISKeyboard->GetEntity(inKey, theKey) != 0 ? NULL : theKey;
}
// shims5
extern "C" void ee_screenscpp_mf_set_ui_state(uintptr_t inRec, int32_t inState)
{ static_cast<EEUIFMainGame*>(R(inRec)->mForm)->SetUIState(inState); }
extern "C" bool ee_screenscpp_mf_has_wall(uintptr_t inRec) { return static_cast<EEUIBuildWall*>(R(inRec)->mWall) != NULL; }
extern "C" void ee_screenscpp_mf_wall_cancel(uintptr_t inRec) { ee_screens_wall_cancel(static_cast<EEUIBuildWall*>(R(inRec)->mWall)); }
extern "C" void ee_screenscpp_mf_gate_cancel(uintptr_t inRec) { ee_screens_gate_cancel(static_cast<EEUIBuildGate*>(R(inRec)->mGate)); }
extern "C" bool ee_screenscpp_mouse_left_just_pressed() { return UIManager::GetLMouseButton()->JustPressed() != 0; }
extern "C" bool ee_screenscpp_mouse_left_down() { return UIManager::GetLMouseButton()->GetValue() != 0; }
extern "C" bool ee_screenscpp_mouse_left_just_unpressed() { return UIManager::GetLMouseButton()->JustUnpressed() != 0; }
extern "C" bool ee_screenscpp_mouse_right_just_pressed() { return UIManager::GetRMouseButton()->JustPressed() != 0; }
extern "C" bool ee_screenscpp_mouse_right_down() { return UIManager::GetRMouseButton()->GetValue() != 0; }
extern "C" bool ee_screenscpp_mouse_right_just_unpressed()
{ return UIManager::GetRMouseButton()->JustUnpressed() != 0; }
extern "C" int32_t ee_screenscpp_key_shift_value() { return (int32_t)EEKey(kKeyShift)->GetValue(); }
extern "C" bool ee_screenscpp_key_exists(uint32_t inKey) { return EEKey(inKey) != NULL; }
extern "C" bool ee_screenscpp_key_value(uint32_t inKey) { return EEKey(inKey)->GetValue() != 0; }
extern "C" bool ee_screenscpp_key_just_pressed(uint32_t inKey) { return EEKey(inKey)->JustPressed() != 0; }
extern "C" bool ee_screenscpp_rasterizer_view_contains(float inX, float inY)
{ return gRasterizer->GetViewport(gScreenIndex).GetViewRectangle().Contains(GE2DPoint(inX, inY)); }
extern "C" bool ee_screenscpp_view_contains(float inX, float inY)
{ return gViewport.GetViewRectangle().Contains(GE2DPoint(inX, inY)); }
extern "C" bool ee_screenscpp_picked_owned_by_current(uintptr_t inRec)
{ return gWorld.GetCurrentPlayer() == static_cast<EEUnit*>(R(inRec)->mPicked)->mPlayer; }
extern "C" bool ee_screenscpp_picked_owner_hostile(uintptr_t inRec)
{ return static_cast<EEUnit*>(R(inRec)->mPicked)->mPlayer->mEnemies[gWorld.GetCurrentPlayer()->mPlayerID] != 0; }
extern "C" bool ee_screenscpp_picked_seen_by_current(uintptr_t inRec)
{ return EECombatIsSeenBy(static_cast<EEComplexUnit*>(static_cast<EEUnit*>(R(inRec)->mPicked)), gWorld.GetCurrentPlayer(), true); }
extern "C" bool ee_screenscpp_mf_first_from_group(uintptr_t inRec)
{ return (R(inRec)->mFirst = gWorld.GetCurrentPlayer()->mSelectedGroup->GetFirstUnit()) != NULL; }
extern "C" void ee_screenscpp_mf_first_clear(uintptr_t inRec) { R(inRec)->mFirst = NULL; }
extern "C" bool ee_screenscpp_mf_first_selected(uintptr_t inRec)
{ return EEGetFirstSelectedUnit(reinterpret_cast<EEComplexUnit*&>(R(inRec)->mFirst)) == true; }
extern "C" bool ee_screenscpp_mf_has_first(uintptr_t inRec) { return static_cast<EEComplexUnit*>(R(inRec)->mFirst) != NULL; }
extern "C" float ee_screenscpp_first_distance_squared(uintptr_t inRec, float inX, float inY, float inZ)
{ return static_cast<EEComplexUnit*>(R(inRec)->mFirst)->mComplexUnit->DistanceSquared3D(GE3DPoint(inX, inY, inZ)); }
extern "C" float ee_screenscpp_first_min_range(uintptr_t inRec) { return static_cast<EEComplexUnit*>(R(inRec)->mFirst)->GetUnitClass()->mMinRange; }
extern "C" void ee_screenscpp_player_add_event(int64_t inX, int64_t inY, int32_t inKind)
{ gWorld.GetCurrentPlayer()->mEvents.AddEvent(U2DPoint<long>((long)inX, (long)inY), 2, gWorld.GetCurrentPlayer(), inKind, false); }
extern "C" uint32_t ee_screenscpp_ghost_count() { return (uint32_t)gWorld.GetNumGhostUnits(); }
extern "C" bool ee_screenscpp_ghost_can_place(uint32_t inGhost)
{ return static_cast<EEGhostUnit*>(gWorld.GetGhostUnit(inGhost))->mCanPlace; }
extern "C" uint32_t ee_screenscpp_ghost_type(uint32_t inGhost)
{ return (uint32_t)static_cast<EEGhostUnit*>(gWorld.GetGhostUnit(inGhost))->mUnitClass->mUnitType; }
extern "C" int64_t ee_screenscpp_ghost_tile_x(uint32_t inGhost)
{ return (int64_t)static_cast<EEGhostUnit*>(gWorld.GetGhostUnit(inGhost))->mTileX; }
extern "C" int64_t ee_screenscpp_ghost_tile_y(uint32_t inGhost)
{ return (int64_t)static_cast<EEGhostUnit*>(gWorld.GetGhostUnit(inGhost))->mTileY; }
extern "C" void ee_screenscpp_build_set_queued(uintptr_t inRec, bool inQueued)
{ static_cast<EEABuild*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mBuild)->mIsQueued = inQueued; }
extern "C" void ee_screenscpp_build_at(uintptr_t inRec, float inX, float inY)
{ static_cast<EEABuild*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mBuild)->Build(GE2DPoint(inX, inY)); }
extern "C" void ee_screenscpp_camera_clear_unit_a() { gWorld.mCameraControl.ClearUnitA(); }
extern "C" void ee_screenscpp_camera_clear_unit_b() { gWorld.mCameraControl.ClearUnitB(); }
extern "C" void ee_screenscpp_hack_update(uintptr_t inRec, float inX, float inY)
{ static_cast<EEUIFMGUnitHack*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mUnitHack)->Update(GE2DPoint(inX, inY)); }
extern "C" void ee_screenscpp_hack_point(uintptr_t inRec, float inX, float inY)
{ static_cast<EEUIFMGUnitHack*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mUnitHack)->Fn157269(GE2DPoint(inX, inY)); }
extern "C" void ee_screenscpp_hack_place(uintptr_t inRec, float inX, float inY)
{ static_cast<EEUIFMGUnitHack*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mUnitHack)->PlaceUnit(GE2DPoint(inX, inY)); }
extern "C" float ee_screenscpp_mf_mouse_x(uintptr_t inRec) { return static_cast<EEUIFMainGame*>(R(inRec)->mForm)->GetMouseLocation().GetX(); }
extern "C" float ee_screenscpp_mf_mouse_y(uintptr_t inRec) { return static_cast<EEUIFMainGame*>(R(inRec)->mForm)->GetMouseLocation().GetY(); }
extern "C" int64_t ee_screenscpp_mf_mouse_mode(uintptr_t inRec)
{ return (int64_t)static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mMouseModes[static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mUIState]; }
extern "C" bool ee_screenscpp_cinematic_ending() { return gWorld.GetScenarioManager()->mIsEndingCinematic; }
extern "C" void ee_screenscpp_post_cinematic_screen_shot() { gUserInterface->PostScreenShot(-1, true, false); }
extern "C" uintptr_t ee_screenscpp_end_cinematic_event_new()
{ return (uintptr_t)static_cast<UIEvent*>(new EEScreenEvent(EE_SCREENS_EV_END_CINEMATIC)); }
extern "C" void ee_screenscpp_mf_start_selection(uintptr_t inRec)
{ static_cast<EEUICSelectUnits*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->GetControl(kMainGameSelectUnits))->StartSelection(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->GetMouseLocation()); }
extern "C" void ee_screenscpp_mf_start_command(uintptr_t inRec)
{ static_cast<EEUICCommandUnits*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->GetControl(kMainGameCommandUnits))->StartCommand(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->GetMouseLocation()); }
extern "C" void ee_screenscpp_mf_wall_build(uintptr_t inRec) { ee_screens_wall_build(static_cast<EEUIBuildWall*>(R(inRec)->mWall)); }
extern "C" void ee_screenscpp_mf_gate_build(uintptr_t inRec, float inX, float inY, int32_t inDirection)
{ ee_screens_gate_build(static_cast<EEUIBuildGate*>(R(inRec)->mGate), inX, inY, (int32_t)inDirection); }
extern "C" void ee_screenscpp_mf_gate_create_ghost(uintptr_t inRec)
{ EEGhostUnit::CreateGate(0.0f, static_cast<EEUnitClass*>(static_cast<EEUIBuildGate*>(R(inRec)->mGate)->mUnitClass), gWorld.GetPlayer(static_cast<EEUIBuildGate*>(R(inRec)->mGate)->mPlayer)); }
extern "C" void ee_screenscpp_gate_preview(float inX, float inY, long* inDirection)
{ EEGhostUnit::PreviewGate(GE2DPoint(inX, inY), *inDirection); }
extern "C" void ee_screenscpp_mf_wall_update(uintptr_t inRec)
{ EEUIBuildWallUpdate(&static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mBuildWall, static_cast<EEUIFMainGame*>(R(inRec)->mForm)->GetMouseLocation()); }
extern "C" void ee_screenscpp_mf_gate_update(uintptr_t inRec)
{ EEUIBuildGateUpdate(&static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mBuildGate, static_cast<EEUIFMainGame*>(R(inRec)->mForm)->GetMouseLocation()); }
extern "C" void ee_screenscpp_mf_error_lock_enter(uintptr_t inRec) { static_cast<TSCriticalSection*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mErrorCriticalSection)->Enter(); }
extern "C" void ee_screenscpp_mf_error_lock_leave(uintptr_t inRec) { static_cast<TSCriticalSection*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mErrorCriticalSection)->Leave(); }
extern "C" uint32_t ee_screenscpp_mf_current_group(uintptr_t inRec)
{ return (uint32_t)static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mCurrentGroup; }
extern "C" void ee_screenscpp_mf_group_reset(uintptr_t inRec) { static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mMainGroup->Reset(); }
extern "C" void ee_screenscpp_mf_group_refresh(uintptr_t inRec) { static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mMainGroup->Refresh(); }
extern "C" bool ee_screenscpp_mf_group_wants_refresh(uintptr_t inRec)
{ return static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mMainGroup->WantsRefresh(); }
extern "C" uintptr_t ee_screenscpp_mf_pick(uintptr_t inRec)
{ return (uintptr_t)(R(inRec)->mPicked = gUnitPicker.PickUnit(gMouse->mPosition, 0)); }
extern "C" bool ee_screenscpp_mf_picked_is_wall(uintptr_t inRec) { return static_cast<EEUnit*>(R(inRec)->mPicked)->mUnitClass->mIsWall; }
extern "C" int64_t ee_screenscpp_mf_picked_tile_x(uintptr_t inRec) { return (int64_t)static_cast<EEUnit*>(R(inRec)->mPicked)->mTileX; }
extern "C" int64_t ee_screenscpp_mf_picked_tile_y(uintptr_t inRec) { return (int64_t)static_cast<EEUnit*>(R(inRec)->mPicked)->mTileY; }
extern "C" void ee_screenscpp_mouse_tile(long* inX, long* inY)
{ EEPointUnitOrTileXY(gMouse->mPosition, EEScratch<EETile*>(kScratchShimTile), *inX, *inY, true); }
extern "C" void ee_screenscpp_mf_picked_dereference(uintptr_t inRec) { static_cast<EEUnit*>(R(inRec)->mPicked)->Dereference(); }
extern "C" void ee_screenscpp_mf_picked_clear(uintptr_t inRec) { R(inRec)->mPicked = NULL; }
extern "C" void ee_screenscpp_mf_form_gate_create_ghost(uintptr_t inRec)
{ EEGhostUnit::CreateGate(0.0f, static_cast<EEUnitClass*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mBuildGate.mUnitClass), gWorld.mPlayers[static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mBuildGate.mPlayer]); }
extern "C" void ee_screenscpp_gate_preview_at_mouse(long* inDirection)
{ EEGhostUnit::PreviewGate(gMouse->mPosition, *inDirection); }
extern "C" float ee_screenscpp_mf_wall_start_x(uintptr_t inRec) { return static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mBuildWall.mStart.x; }
extern "C" float ee_screenscpp_mf_wall_start_y(uintptr_t inRec) { return static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mBuildWall.mStart.y; }
extern "C" uintptr_t ee_screenscpp_team_wonder_countdown(uint32_t inTeam, unsigned long* inValue)
{ return (uintptr_t)gWorld.mTeams[inTeam].mVictoryCondition->GetWonderCountdown(*inValue); }
extern "C" void ee_screenscpp_mf_info_set_text(uintptr_t inRec)
{ static_cast<UITLScrolling*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->GetControl(kMainGameDialogMessage))->SetText(static_cast<const EESMDialogMessage*>(R(inRec)->mMessage)->mText); }
extern "C" uintptr_t ee_screenscpp_mf_control(uintptr_t inRec, uint32_t inControl)
{ return (uintptr_t)static_cast<EEUIFMainGame*>(R(inRec)->mForm)->GetControl(inControl); }
extern "C" uint32_t ee_screenscpp_mf_info_unit_type(uintptr_t inRec) { return (uint32_t)static_cast<const EESMDialogMessage*>(R(inRec)->mMessage)->mUnitType; }
extern "C" uint32_t ee_screenscpp_unit_class_count() { return (uint32_t)EEUnitClassCount(); }
extern "C" bool ee_screenscpp_unit_class_exists(uint32_t inKind) { return gUnitClassTable[inKind] != NULL; }
extern "C" bool ee_screenscpp_player_class_has_button(uint32_t inKind)
{ return gWorld.GetCurrentPlayer()->mUnitClassUpgrades[inKind]->mButton != NULL; }
extern "C" void ee_screenscpp_mf_info_set_face(uintptr_t inRec, uintptr_t inBitmap, uintptr_t inPortrait)
{ gWorld.GetCurrentPlayer()->mUnitClassUpgrades[static_cast<const EESMDialogMessage*>(R(inRec)->mMessage)->mUnitType]->mButton->SetFaceBitmap(reinterpret_cast<GEBitmap*>(inBitmap), C(inPortrait)->GetRectangle(), C(inPortrait)->GetScreenIndex()); }
extern "C" void ee_screenscpp_ui_bitmap_set_bitmap(uintptr_t inControl, uintptr_t inBitmap)
{ static_cast<UIBitmap*>(C(inControl))->SetBitmap(reinterpret_cast<GEBitmap*>(inBitmap)); }
extern "C" void ee_screenscpp_control_refresh_opacity(uintptr_t inControl)
{ C(inControl)->SetBgOpacity(C(inControl)->GetBgOpacity()); }
extern "C" void ee_screenscpp_mf_info_load_sound(uintptr_t inRec, unsigned long* inSample, unsigned long* inLength)
{ EELoadSound("Sounds\\" + UString((const char*)static_cast<const EESMDialogMessage*>(R(inRec)->mMessage)->mSound), 0x31, *inSample, *inLength, 1); }
extern "C" void ee_screenscpp_mf_poll_new(uintptr_t inRec, uint32_t inMilliseconds)
{ static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mPoll = new TMPoll((unsigned long)inMilliseconds, 0, false); }
extern "C" void ee_screenscpp_mf_poll_start(uintptr_t inRec) { static_cast<TMPoll*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mPoll)->Start(); }
extern "C" uint32_t ee_screenscpp_game_size_width() { return (uint32_t)gGameWindowWidth; }
extern "C" uint32_t ee_screenscpp_game_size_height() { return (uint32_t)gGameWindowHeight; }
extern "C" void ee_screenscpp_mf_scroll_new(uintptr_t inRec) { static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mScroll = new EEAScroll; }
extern "C" void ee_screenscpp_mf_scroll_set_edges(uintptr_t inRec, float inLeft, float inTop, float inRight, float inBottom)
{ static_cast<EEAScroll*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mScroll)->mEdges = GERectangle(inLeft, inTop, inRight, inBottom); }
extern "C" void ee_screenscpp_mf_mouse_click_new(uintptr_t inRec) { static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mMouseClick = new EEAMouseClick; }
extern "C" void ee_screenscpp_mf_lasso_new(uintptr_t inRec) { static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mLasso = new EEALasso; }
extern "C" void ee_screenscpp_mf_build_new(uintptr_t inRec) { static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mBuild = new EEABuild; }
extern "C" void ee_screenscpp_mf_calamity_new(uintptr_t inRec) { static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mCalamity = new EEACalamity; }
extern "C" void ee_screenscpp_mf_wheel_zoom_new(uintptr_t inRec, uint32_t inCode)
{ static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mWheelZoom = EECreateScreenEvent((unsigned long)inCode); }
extern "C" void ee_screenscpp_mf_set_wheel_event(uintptr_t inRec)
{ static_cast<EEUIFMainGame*>(R(inRec)->mForm)->SetMouseWheelMoveEvent(static_cast<UIEvent*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mWheelZoom)); }
extern "C" void ee_screenscpp_mf_grab_scroll_new(uintptr_t inRec, uint32_t inCode)
{ static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mGrabScroll = EECreateGrabScroll((unsigned long)inCode); }
extern "C" void ee_screenscpp_mf_set_middle_pressed_event(uintptr_t inRec)
{ static_cast<EEUIFMainGame*>(R(inRec)->mForm)->SetMouseMiddleButtonPressedEvent(static_cast<UIEvent*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mGrabScroll)); }
extern "C" void ee_screenscpp_mf_set_middle_unpressed_event(uintptr_t inRec)
{ static_cast<EEUIFMainGame*>(R(inRec)->mForm)->SetMouseMiddleButtonUnpressedEvent(static_cast<UIEvent*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mGrabScroll)); }
extern "C" void ee_screenscpp_mf_add_hot_key(uintptr_t inRec, uint32_t inKey, uint32_t inModifier1, uint32_t inModifier2, uint32_t inModifier3, uintptr_t inEvent1, uintptr_t inEvent2, uintptr_t inEvent3, uintptr_t inEvent4, uint32_t inId, bool inFlag)
{ static_cast<EEUIFMainGame*>(R(inRec)->mForm)->AddHotKey(inKey, inModifier1, inModifier2, inModifier3, reinterpret_cast<UIEvent*>(inEvent1), reinterpret_cast<UIEvent*>(inEvent2), reinterpret_cast<UIEvent*>(inEvent3), reinterpret_cast<UIEvent*>(inEvent4), (unsigned long)inId, inFlag); }
extern "C" void ee_screenscpp_mf_unit_load(uintptr_t inRec, uint32_t inUnit)
{ R(inRec)->mUnit = static_cast<EEUnit*>(gWorld.GetCurrentPlayer()->GetUnit((unsigned long)inUnit)); }
extern "C" uintptr_t ee_screenscpp_mf_unit(uintptr_t inRec) { return (uintptr_t)static_cast<EEUnit*>(R(inRec)->mUnit); }
extern "C" void ee_screenscpp_mf_ghost_of_node(uintptr_t inRec)
{ EEGhostUnit::Create(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->GetMouseLocation(), static_cast<EETechTreeNode*>(R(inRec)->mNode)->mData->mUnitClass, gWorld.GetCurrentPlayer(), 0.0f, false); }
extern "C" void ee_screenscpp_mf_ghost_of_calamity(uintptr_t inRec)
{ EEGhostUnit::Create(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->GetMouseLocation(), static_cast<EECalamityClass*>(R(inRec)->mCalamity)->mUnitClassIndex, gWorld.GetCurrentPlayer(), 0.0f, false); }
extern "C" void ee_screenscpp_mf_gate_set_technology(uintptr_t inRec, int32_t inTechnology)
{ static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mBuildGate.mTechnology = (long)inTechnology; }
extern "C" void ee_screenscpp_mf_gate_wall_type_from_node(uintptr_t inRec)
{ static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mBuildGate.mWallType = static_cast<EETechTreeNode*>(R(inRec)->mNode)->GetUnitClass(0); }
extern "C" void ee_screenscpp_mf_gate_class_from_node(uintptr_t inRec)
{ static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mBuildGate.mUnitClass = (gUnitClassTable[static_cast<EETechTreeNode*>(R(inRec)->mNode)->mData->mUnitClass]); }
extern "C" void ee_screenscpp_mf_gate_initialize(uintptr_t inRec)
{ ee_screens_gate_initialize(&static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mBuildGate, (int32_t)gWorld.GetCurrentPlayer()->mPlayerID); }
extern "C" void ee_screenscpp_mf_wall_set_technology(uintptr_t inRec, int32_t inTechnology)
{ static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mBuildWall.mTechnology = (long)inTechnology; }
extern "C" void ee_screenscpp_mf_wall_initialize(uintptr_t inRec)
{ ee_screens_wall_initialize(&static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mBuildWall, (int32_t)gWorld.GetCurrentPlayer()->mPlayerID); }
extern "C" uint32_t ee_screenscpp_selected_count()
{ return (uint32_t)gWorld.GetCurrentPlayer()->mSelectedGroup->GetNumUnits(); }
extern "C" void ee_screenscpp_mf_caster_first(uintptr_t inRec)
{ R(inRec)->mCaster = gWorld.GetCurrentPlayer()->mSelectedGroup->GetUnit(0); }
extern "C" void ee_screenscpp_mf_caster_clear(uintptr_t inRec) { R(inRec)->mCaster = NULL; }
extern "C" bool ee_screenscpp_mf_has_caster(uintptr_t inRec) { return static_cast<EEUnit*>(R(inRec)->mCaster) != NULL; }
extern "C" void ee_screenscpp_mf_calamity_set(uintptr_t inRec, int32_t inCalamity)
{ static_cast<EEACalamity*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mCalamity)->SetCalamity((long)inCalamity, static_cast<EECalamityClass*>(R(inRec)->mCalamity)); }
extern "C" void ee_screenscpp_mf_calamity_set_caster(uintptr_t inRec)
{ static_cast<EEACalamity*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mCalamity)->SetCasterID(static_cast<EEUnit*>(R(inRec)->mCaster)->mUnitID); }
extern "C" int64_t ee_screenscpp_mf_calamity_target_kind(uintptr_t inRec)
{ return (int64_t)static_cast<EECalamityClass*>(R(inRec)->mCalamity)->GetTargetKind(); }
extern "C" void ee_screenscpp_mouse_shift_off() { gMouse->mShiftMode = false; }
extern "C" void ee_screenscpp_mouse_set_pointer(int32_t inPointer) { gMouse->mPointer = (long)inPointer; }
extern "C" void ee_screenscpp_mouse_set_bounds(float inLeft, float inTop, float inRight, float inBottom)
{ gMouse->SetBounds(GERectangle(inLeft, inTop, inRight, inBottom)); }
extern "C" void ee_screenscpp_server_unpause(uintptr_t inServer)
{ reinterpret_cast<EEServer*>(inServer)->UnpauseGame(); }
extern "C" uintptr_t ee_screenscpp_graphics() { return (uintptr_t)gGraphics; }
extern "C" void ee_screenscpp_gfx_clear_render_objects() { gGraphics->PostClearRenderObjects(); }
extern "C" void ee_screenscpp_mf_cancel_button_up(uintptr_t inRec, uintptr_t inEvent)
{ static_cast<EEUIFMainGame*>(R(inRec)->mForm)->GetControl(kMainGameCancelUIMode)->SetTriggerEvent(kControlTriggerButtonUp, reinterpret_cast<UIEvent*>(inEvent)); }
extern "C" void ee_screenscpp_mf_cancel_right_button_up(uintptr_t inRec, uintptr_t inEvent)
{ static_cast<EEUIFMainGame*>(R(inRec)->mForm)->GetControl(kMainGameCancelUIMode)->SetTriggerEvent(kControlTriggerRightButtonUp, reinterpret_cast<UIEvent*>(inEvent)); }
extern "C" void ee_screenscpp_mf_main_group_open(uintptr_t inRec)
{ static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mMainGroup->Open(static_cast<EEUIFMainGame*>(R(inRec)->mForm)); }
extern "C" void ee_screenscpp_mf_main_group_clear_l20(uintptr_t inRec)
{ static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mMainGroup->mL20 = 0; }
extern "C" void ee_screenscpp_post_graphics_all() { EEPostGraphicsAll(); }
extern "C" void ee_screenscpp_gfx_fade_in(uint32_t inValue) { gGraphics->PostFadeIn(inValue); }
extern "C" void ee_screenscpp_mf_media_load(uintptr_t inRec) { static_cast<EEMedia*>(R(inRec)->mMedia)->LoadMedia(); }
extern "C" void ee_screenscpp_mf_old_media_unload(uintptr_t inRec) { static_cast<EEMedia*>(R(inRec)->mOldMedia)->UnloadMedia(); }
extern "C" void ee_screenscpp_shell_op(uintptr_t inControl, uint32_t inOp) { EEUIShell::Op(C(inControl), inOp); }
extern "C" uintptr_t ee_screenscpp_mf_background(uintptr_t inRec, uint32_t inId)
{ return (uintptr_t)static_cast<EEUIFMainGame*>(R(inRec)->mForm)->GetBackgroundByID((unsigned long)inId); }
extern "C" void ee_screenscpp_background_draw(uintptr_t inBackground)
{ reinterpret_cast<UIBackground*>(inBackground)->DrawBackground(reinterpret_cast<UIBackground*>(inBackground)->GetRectangle()); }
extern "C" float ee_screenscpp_mf_fps(uintptr_t inRec)
{ return gRasterizer->GetFramesPerSecond(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->GetManager()->GetScreen()); }
// end shims5

// the saved flags' and the error strings' shims (ee_screens hud_more.rs)
extern "C" int64_t ee_screenscpp_mff_read_bool(uintptr_t inFile, uintptr_t inField) { return reinterpret_cast<FSFile*>(inFile)->Read(*reinterpret_cast<bool*>(inField)); }
extern "C" int64_t ee_screenscpp_mff_write_bool(uintptr_t inFile, bool inValue) { return reinterpret_cast<FSFile*>(inFile)->Write(inValue); }
extern "C" void ee_screenscpp_mff_error_enter(uintptr_t inForm) { static_cast<TSCriticalSection*>(reinterpret_cast<EEUIFMainGame*>(inForm)->mErrorCriticalSection)->Enter(); }
extern "C" void ee_screenscpp_mff_error_set(uintptr_t inForm, uint32_t inIndex, uintptr_t inText) { reinterpret_cast<EEUIFMainGame*>(inForm)->mStrings[inIndex] = *reinterpret_cast<const UWideString*>(inText); }
extern "C" void ee_screenscpp_mff_strings_changed(uintptr_t inForm) { reinterpret_cast<EEUIFMainGame*>(inForm)->mStringsChanged = true; }
extern "C" void ee_screenscpp_mff_error_leave(uintptr_t inForm) { static_cast<TSCriticalSection*>(reinterpret_cast<EEUIFMainGame*>(inForm)->mErrorCriticalSection)->Leave(); }

// ---- the rest of the original host (EEUIFMainGame::HostOp, the mouse modes' EEMouseModeOp; UI lane round 9:
// main_form_calls.rs), each one statement on the call record, the form or the world, and the cases' locals
#define MF(inRec) static_cast<EEUIFMainGame*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm))
extern "C" int64_t ee_screenscpp_mf_gate_direction(uintptr_t inRec)
{ return (int64_t)MF(inRec)->mBuildGate.mDirection; }
extern "C" void ee_screenscpp_mf_set_gate_direction(uintptr_t inRec, int64_t inValue)
{ MF(inRec)->mBuildGate.mDirection = (decltype(MF(inRec)->mBuildGate.mDirection))inValue; }
extern "C" int64_t ee_screenscpp_mf_wall_state(uintptr_t inRec) { return (int64_t)MF(inRec)->mBuildWall.mState; }
extern "C" void ee_screenscpp_mf_set_wall_state(uintptr_t inRec, int64_t inValue)
{ MF(inRec)->mBuildWall.mState = (decltype(MF(inRec)->mBuildWall.mState))inValue; }
extern "C" bool ee_screenscpp_mf_tooltip_shadow(uintptr_t inRec) { return MF(inRec)->GetDrawToolTipShadow(); }
extern "C" void ee_screenscpp_mf_set_tooltip_shadow(uintptr_t inRec, bool inValue)
{ MF(inRec)->SetDrawToolTipShadow(inValue); }
extern "C" int64_t ee_screenscpp_mf_build_tech(uintptr_t inRec) { return (int64_t)static_cast<EEABuild*>(MF(inRec)->mBuild)->mTechnology; }
extern "C" void ee_screenscpp_mf_set_build_tech(uintptr_t inRec, int64_t inValue)
{ static_cast<EEABuild*>(MF(inRec)->mBuild)->mTechnology = (decltype(static_cast<EEABuild*>(MF(inRec)->mBuild)->mTechnology))inValue; }
extern "C" void ee_screenscpp_mf_log_line(uintptr_t inRec, int64_t inIndex)
{ static_cast<EEUIShellList*>(MF(inRec)->GetControl(kMainGameErrorLog))->SetLine((unsigned long)inIndex, MF(inRec)->mStrings[inIndex]); }
extern "C" int64_t ee_screenscpp_mf_poll(uintptr_t inRec) { return (int64_t)static_cast<TMPoll*>(MF(inRec)->mPoll)->Poll(); }
extern "C" void ee_screenscpp_mf_scroll(uintptr_t inRec) { static_cast<EEAScroll*>(MF(inRec)->mScroll)->Update(gMouse->mPosition); }
extern "C" void ee_screenscpp_mf_tile_x(int64_t inX) { EEScratch<EETileCoordinate>(kScratchMFTile).mX = (long)inX; }
extern "C" void ee_screenscpp_mf_tile_y(int64_t inY) { EEScratch<EETileCoordinate>(kScratchMFTile).mY = (long)inY; }
extern "C" bool ee_screenscpp_mf_find_gate_wall(long* inDirection) { return EEFindGateWall(EEScratch<EETileCoordinate>(kScratchMFTile), *inDirection); }
extern "C" void ee_screenscpp_mf_clear_ghosts() { gWorld.ClearGhostUnits(); }
extern "C" void ee_screenscpp_mf_tiles_clear() { EEScratch<std::vector<EETileCoordinate>>(kScratchMFTiles).clear(); }
extern "C" void ee_screenscpp_mf_tiles_add() { EEScratch<std::vector<EETileCoordinate>>(kScratchMFTiles).push_back(EEScratch<EETileCoordinate>(kScratchMFTile)); }
extern "C" void ee_screenscpp_mf_preview_wall_call(uintptr_t inRec)
{ EEPreviewWall(EEScratch<std::vector<EETileCoordinate>>(kScratchMFTiles), static_cast<EEUIBuildWall*>(R(inRec)->mWall)->mTechnology, static_cast<EEUIBuildWall*>(R(inRec)->mWall)->mPlayer); }
extern "C" void ee_screenscpp_mf_preview_wall_form(uintptr_t inRec)
{ EEPreviewWall(EEScratch<std::vector<EETileCoordinate>>(kScratchMFTiles), MF(inRec)->mBuildWall.mTechnology, MF(inRec)->mBuildWall.mPlayer); }
extern "C" void ee_screenscpp_mf_preview_placement_mouse() { EEPreviewPlacement(gMouse->mPosition); }
extern "C" int64_t ee_screenscpp_mf_population()
{ return (int64_t)gWorld.GetCurrentPlayer()->GetTotalPopulation(false, false); }
extern "C" int64_t ee_screenscpp_mf_population_limit()
{ return (int64_t)gWorld.GetCurrentPlayer()->GetPopulationLimit(false); }
extern "C" int64_t ee_screenscpp_mf_num_teams() { return (int64_t)gWorld.GetNumTeams(); }
extern "C" void ee_screenscpp_mf_label_player_color(uintptr_t inRec, int64_t inControl, int64_t inPlayer)
{ static_cast<UITextLabel*>(MF(inRec)->GetControl((unsigned long)inControl))->GetGEText()->SetFgColor(reinterpret_cast<EEPlayer*>(inPlayer)->mColor); }
extern "C" void ee_screenscpp_mf_wonder_format()
{ gResource->GetString(IDS_TIME_LEFT_UNTIL_WINS_WONDER_VICTORY, EEScratch<UWideString>(kScratchMFFormat)); }
extern "C" void ee_screenscpp_mf_wonder_print(uintptr_t inRec, int64_t inPlayer, int64_t inTime)
{ sprintf(R(inRec)->mText, EEScratch<UWideString>(kScratchMFFormat), (const char*)reinterpret_cast<EEPlayer*>(inPlayer)->mName, (unsigned long)inTime); }
extern "C" void ee_screenscpp_mf_text_time(uintptr_t inRec) { R(inRec)->mText = gPhysics->GetTimeString(); }
extern "C" bool ee_screenscpp_mf_comms_paused() { return gCommunications->GetIsGamePaused(); }
extern "C" bool ee_screenscpp_mf_has_teams() { return gWorld.mTeams.size() > 0; }
extern "C" void ee_screenscpp_mf_pause_text(uintptr_t inRec, int64_t inValue)
{ MF(inRec)->Rust(EE_SCREENS_MF_DO_PAUSE_TEXT, inValue); }
extern "C" int64_t ee_screenscpp_mf_lag_status() { return (int64_t)gWorld.mLagStatus; }
extern "C" void ee_screenscpp_mf_lag_color(uintptr_t inRec, int64_t inControl, int64_t inPlayer)
{ static_cast<UIBitmap*>(MF(inRec)->GetControl((unsigned long)inControl))->GetBitmap()->SetColor(gWorld.mPlayers[inPlayer]->mColor); }
extern "C" bool ee_screenscpp_mf_has_instructions()
{ return gWorld.mScenarioManager->mMediaStrings.GetSection(3).GetLength() > 0; }
extern "C" void ee_screenscpp_mf_update_cursor(uintptr_t inRec) { MF(inRec)->UpdateCursorKind(); }
extern "C" int64_t ee_screenscpp_mf_info_duration(uintptr_t inRec) { return (int64_t)static_cast<const EESMDialogMessage*>(R(inRec)->mMessage)->mDuration; }
extern "C" void ee_screenscpp_mf_chat_top(uintptr_t inRec, int64_t inValue)
{ MF(inRec)->Rust(EE_SCREENS_MF_DO_CHAT_TOP, inValue); }
extern "C" void ee_screenscpp_mf_hide_messages(uintptr_t inRec) { MF(inRec)->Rust(EE_SCREENS_MF_DO_HIDE_MESSAGES); }
extern "C" void ee_screenscpp_mf_create_error_lock(uintptr_t inRec)
{ ESVERIFY_AT(TSScheduler::CreateCriticalSection(reinterpret_cast<TSCriticalSection*&>(MF(inRec)->mErrorCriticalSection)) == keseOK, 633); }
extern "C" void ee_screenscpp_mf_create_group(uintptr_t inRec, int64_t inIndex)
{ MF(inRec)->CreateGroup((unsigned long)inIndex); }
extern "C" void ee_screenscpp_mf_new_unit_hack(uintptr_t inRec)
{ MF(inRec)->mUnitHack = new EEUIFMGUnitHack(MF(inRec)); }
extern "C" void ee_screenscpp_mf_wall_terminate(uintptr_t inRec)
{ ee_screens_wall_terminate(&MF(inRec)->mBuildWall); }
extern "C" void ee_screenscpp_mf_gate_terminate(uintptr_t inRec)
{ ee_screens_gate_terminate(&MF(inRec)->mBuildGate); }
extern "C" void ee_screenscpp_mf_node_load(uintptr_t inRec, int64_t inTechnology)
{ R(inRec)->mNode = gWorld.GetCurrentPlayer()->mTechnologies->Get((long)inTechnology); }
extern "C" bool ee_screenscpp_mf_has_node(uintptr_t inRec) { return static_cast<EETechTreeNode*>(R(inRec)->mNode) != NULL; }
extern "C" bool ee_screenscpp_mf_node_class_exists(uintptr_t inRec)
{ return gUnitClassTable[static_cast<EETechTreeNode*>(R(inRec)->mNode)->mData->mUnitClass] != NULL; }
extern "C" int64_t ee_screenscpp_mf_can_produce(uintptr_t inRec, int64_t inTechnology)
{ return (int64_t)gWorld.GetCurrentPlayer()->CanProduce((long)inTechnology, static_cast<EEUnit*>(R(inRec)->mUnit)->mComplexUnit->mUnitClass->GetUnitType(), 1, true, 3, &R(inRec)->mText); }
extern "C" void ee_screenscpp_mf_play_sound(int64_t inSound)
{ gSoundManager->Play(EESoundObject::Get((unsigned long)inSound)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL); }
extern "C" void ee_screenscpp_mf_chat_log_error(uintptr_t inRec)
{ gUserInterface->AddChatLogEntry(R(inRec)->mText, 0); }
extern "C" int64_t ee_screenscpp_mf_selected_count()
{ return (int64_t)gWorld.GetCurrentPlayer()->mSelectedGroup->GetNumUnits(); }
extern "C" int64_t ee_screenscpp_mf_node_calamity(uintptr_t inRec)
{ return (int64_t)static_cast<EETechTreeNode*>(R(inRec)->mNode)->mData->mCalamity; }
extern "C" void ee_screenscpp_mf_calamity_load(uintptr_t inRec, int64_t inCalamity)
{ R(inRec)->mCalamity = USingleton<EECalamityManager>::GetInstance()->GetCalamityClass((EECalamityEnum)inCalamity); }
extern "C" bool ee_screenscpp_mf_has_calamity(uintptr_t inRec) { return static_cast<EECalamityClass*>(R(inRec)->mCalamity) != NULL; }
extern "C" void ee_screenscpp_mf_calamity_cast(uintptr_t inRec)
{ static_cast<EEACalamity*>(MF(inRec)->mCalamity)->Cast(MF(inRec)->GetMouseLocation()); }
extern "C" bool ee_screenscpp_mf_user_paused() { return gWorld.mUserPaused; }
extern "C" void ee_screenscpp_mf_master_volume() { gSoundManager->SetMasterVolume(gMasterVolume); }
extern "C" void ee_screenscpp_mf_mouse_position(float inX, float inY) { gMouse->SetPosition(GE2DPoint(inX, inY)); }
extern "C" void ee_screenscpp_mf_fill_hack(uintptr_t inRec) { static_cast<EEUIFMGUnitHack*>(MF(inRec)->mUnitHack)->FillComboBox(); }
extern "C" bool ee_screenscpp_mf_has_techs() { return gWorld.GetCurrentPlayer()->mTechnologies != NULL; }
extern "C" void ee_screenscpp_mf_text_epoch_title(uintptr_t inRec)
{ gResource->GetString(EEEpochTitleID(gWorld.GetCurrentPlayer()->mTechnologies->mCurrentEpoch), R(inRec)->mText); }
extern "C" void ee_screenscpp_mf_unit_info_background(uintptr_t inRec)
{ MF(inRec)->mUnitInfoBackground = MF(inRec)->GetBackgroundByID(0x64); }
extern "C" void ee_screenscpp_mf_media_old_load(uintptr_t inRec)
{ R(inRec)->mOldMedia = EEMedia::Get(MF(inRec)->mMediaID); }
extern "C" bool ee_screenscpp_mf_has_media_old(uintptr_t inRec) { return static_cast<EEMedia*>(R(inRec)->mOldMedia) != NULL; }
extern "C" int64_t ee_screenscpp_mf_player_media_id()
{ return (int64_t)gWorld.GetCurrentPlayer()->mTechnologies->mMediaID; }
extern "C" bool ee_screenscpp_mf_using_carbon() { return gWorld.mUsingCarbon; }
extern "C" void ee_screenscpp_mf_media_get(uintptr_t inRec, int64_t inMedia)
{ R(inRec)->mMedia = EEMedia::Get((long)inMedia); }
extern "C" bool ee_screenscpp_mf_has_media(uintptr_t inRec) { return static_cast<EEMedia*>(R(inRec)->mMedia) != NULL; }
extern "C" void ee_screenscpp_mf_bg_texture(uintptr_t inRec, int64_t inBackground, int64_t inTexture)
{ static_cast<UIBitmapBackground*>(MF(inRec)->GetBackgroundByID((unsigned long)inBackground))->SetBitmapTexture(static_cast<EEGraphicObject*>(static_cast<EEMedia*>(R(inRec)->mMedia))->mTextures[inTexture]); }
extern "C" bool ee_screenscpp_md_captured(uintptr_t inRec) { return MF(inRec)->GetIsMouseCaptured(); }
extern "C" void ee_screenscpp_md_point_reset() { EEScratch<GE3DPoint>(kScratchMFPoint) = GE3DPoint(0.0f, 0.0f, 0.0f); }
extern "C" bool ee_screenscpp_md_point(uintptr_t inRec, float inX, float inY)
{ return EEPointUnitOrTile(GE2DPoint(inX, inY), reinterpret_cast<EETile*&>(R(inRec)->mTile), EEScratch<GE3DPoint>(kScratchMFPoint), true); }
extern "C" float ee_screenscpp_md_point_x() { return EEScratch<GE3DPoint>(kScratchMFPoint).GetX(); }
extern "C" float ee_screenscpp_md_point_y() { return EEScratch<GE3DPoint>(kScratchMFPoint).GetY(); }
extern "C" float ee_screenscpp_md_point_z() { return EEScratch<GE3DPoint>(kScratchMFPoint).GetZ(); }
extern "C" bool ee_screenscpp_md_point_xy(uintptr_t inRec, float inX, float inY, long* inTileX, long* inTileY)
{ return EEPointUnitOrTileXY(GE2DPoint(inX, inY), reinterpret_cast<EETile*&>(R(inRec)->mTile), *inTileX, *inTileY, true); }
extern "C" bool ee_screenscpp_md_tile_seen(uintptr_t inRec)
{ return static_cast<EETile*>(R(inRec)->mTile)->IsSeenBy(gWorld.GetCurrentPlayer()); }
extern "C" bool ee_screenscpp_md_pick(uintptr_t inRec, float inX, float inY)
{ return (R(inRec)->mPicked = gUnitPicker.PickUnit(GE2DPoint(inX, inY), 0)) != NULL; }
extern "C" bool ee_screenscpp_md_can_be_attacked(uintptr_t inRec)
{ return static_cast<EEUnit*>(R(inRec)->mPicked)->GetUnitClass()->CanBeAttacked(); }
extern "C" bool ee_screenscpp_md_has_picked(uintptr_t inRec) { return static_cast<EEUnit*>(R(inRec)->mPicked) != NULL; }
extern "C" bool ee_screenscpp_md_in_wonder_set(uintptr_t inRec)
{ return gUnitSetManager.Contains(static_cast<EEUnit*>(R(inRec)->mPicked)->mComplexUnit->GetUnitClass()->GetUnitType(), static_cast<EEComplexUnit*>(R(inRec)->mFirst)->mStats->mWonderUnitSet); }
extern "C" float ee_screenscpp_md_damage(int64_t inA, int64_t inB) { return EEDamageFactor((long)inA, (long)inB); }
extern "C" bool ee_screenscpp_md_is_caster(uintptr_t inRec)
{ return static_cast<EEComplexUnit*>(R(inRec)->mFirst)->mUnitID == static_cast<EEACalamity*>(MF(inRec)->mCalamity)->mCasterID; }
extern "C" void ee_screenscpp_md_in_range_set(bool inValue) { EEScratch<bool>(kScratchMFInRange) = inValue; }
extern "C" bool ee_screenscpp_md_can_convert(uintptr_t inRec, int64_t inUnit)
{ return EECanConvert(gWorld.GetCurrentPlayer()->mSelectedGroup->GetUnit((unsigned long)inUnit), static_cast<EEUnit*>(R(inRec)->mPicked)->GetComplexUnit(), EEScratch<bool>(kScratchMFInRange)); }
extern "C" bool ee_screenscpp_md_in_range() { return EEScratch<bool>(kScratchMFInRange); }
extern "C" void ee_screenscpp_md_send_load_garrison(uintptr_t inRec)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageLoadGarrison, {static_cast<EEUnit*>(R(inRec)->mPicked)->GetUnitID(), static_cast<EEComplexUnit*>(R(inRec)->mFirst)->mComplexUnit->mUnitID, 0}), kSlotNone); }
extern "C" void ee_screenscpp_md_send_unload(uintptr_t inRec, int64_t inX, int64_t inY)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageUnloadTransport, {static_cast<EEComplexUnit*>(R(inRec)->mFirst)->GetComplexUnit()->GetUnitID(), (long)inX, (long)inY}), kSlotNone); }
extern "C" void ee_screenscpp_md_send_heal(uintptr_t inRec)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageHealUnit, {static_cast<EEUnit*>(R(inRec)->mPicked)->mComplexUnit->mUnitID}), kSlotNone); }
extern "C" void ee_screenscpp_md_send_convert(uintptr_t inRec, bool inShielded)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageAttackUnit, {static_cast<EEUnit*>(R(inRec)->mPicked)->mComplexUnit->mUnitID, inShielded, true}), kSlotNone); }
extern "C" void ee_screenscpp_md_send_rally(uintptr_t inRec, float inX, float inY, int64_t inKind, int64_t inFlag)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageSetRallyGoal, {GE2DPoint(inX, inY), (long)inKind, static_cast<EEComplexUnit*>(R(inRec)->mFirst)->mComplexUnit->GetUnitID(), (unsigned char)inFlag}), kSlotNone); }
extern "C" void ee_screenscpp_md_order_at_none(float inX, float inY) { EEOrderAtCursor(GE2DPoint(inX, inY), NULL); }
extern "C" void ee_screenscpp_md_order_at_picked(uintptr_t inRec, float inX, float inY)
{ EEOrderAtCursor(GE2DPoint(inX, inY), static_cast<EEUnit*>(R(inRec)->mPicked)); }
extern "C" void ee_screenscpp_md_order_at_complex(uintptr_t inRec, float inX, float inY)
{ EEOrderAtCursor(GE2DPoint(inX, inY), static_cast<EEUnit*>(static_cast<EEUnit*>(R(inRec)->mPicked)->mComplexUnit)); }
extern "C" void ee_screenscpp_md_calamity_effect(uintptr_t inRec, float inX, float inY, float inZ)
{ EEMakeCalamityEffect(GE3DPoint(inX, inY, inZ), static_cast<EEUnit*>(R(inRec)->mPicked)); }
extern "C" void ee_screenscpp_md_preview(uintptr_t inRec) { EEPreviewPlacement(MF(inRec)->GetMouseLocation()); }
extern "C" int64_t ee_screenscpp_md_calamity_check(uintptr_t inRec, float inX, float inY)
{ return (int64_t)static_cast<EEACalamity*>(MF(inRec)->mCalamity)->Check(GE2DPoint(inX, inY)); }
extern "C" bool ee_screenscpp_md_in_game_view(uintptr_t inRec, float inX, float inY)
{ return MF(inRec)->IsInGameView(GE2DPoint(inX, inY)); }
extern "C" bool ee_screenscpp_md_entity_enabled(uintptr_t inRec, int64_t inGroup)
{ return MF(inRec)->GetEntityGroup((unsigned long)inGroup)->GetIsEnabled(); }
extern "C" void ee_screenscpp_md_set_entity_group(uintptr_t inRec, int64_t inGroup)
{ MF(inRec)->SetEntityGroup((long)inGroup); }
extern "C" void ee_screenscpp_md_gate_clear(uintptr_t inRec) { ee_screens_gate_clear_ghosts(static_cast<EEUIBuildGate*>(R(inRec)->mGate)); }

// the destructor's, CreateGroup's and IsInGameView's bodies: hud_bodies.rs over these (UI lane round 12)
#define MFB(f) ((EEUIFMainGame*)(f))
extern "C" void ee_screenscpp_mfb_group_main(uintptr_t inForm)
{ MFB(inForm)->mMainGroup = new EEUIFMGMainGroup(MFB(inForm)); }
extern "C" uintptr_t ee_screenscpp_mfb_unit_info(uintptr_t inForm)
{ return (uintptr_t)static_cast<UIBackground*>(MFB(inForm)->mUnitInfoBackground); }
extern "C" bool ee_screenscpp_mfb_background_visible(uintptr_t inBackground)
{ return ((UIBackground*)inBackground)->GetIsVisible(); }
extern "C" bool ee_screenscpp_mfb_background_contains(uintptr_t inBackground, float inX, float inY)
{ return ((UIBackground*)inBackground)->GetRectangle().Contains(GE2DPoint(inX, inY)); }
extern "C" bool ee_screenscpp_mfb_view_contains(float inX, float inY)
{ return gViewport.GetViewRectangle().Contains(GE2DPoint(inX, inY)); }
extern "C" void ee_screenscpp_mfb_grab_scroll_dereference(uintptr_t inForm) { static_cast<UIEvent*>(MFB(inForm)->mGrabScroll)->Dereference(); }
extern "C" void ee_screenscpp_mfb_release_grab_scroll() { EEReleaseGrabScroll(0); }
extern "C" void ee_screenscpp_mfb_wheel_zoom_dereference(uintptr_t inForm) { static_cast<UIEvent*>(MFB(inForm)->mWheelZoom)->Dereference(); }
extern "C" void ee_screenscpp_mfb_scroll_dereference(uintptr_t inForm) { static_cast<EEAScroll*>(MFB(inForm)->mScroll)->Dereference(); }
extern "C" void ee_screenscpp_mfb_mouse_click_dereference(uintptr_t inForm) { static_cast<EEAMouseClick*>(MFB(inForm)->mMouseClick)->Dereference(); }
extern "C" void ee_screenscpp_mfb_lasso_dereference(uintptr_t inForm) { static_cast<EEALasso*>(MFB(inForm)->mLasso)->Dereference(); }
extern "C" void ee_screenscpp_mfb_destroy_error_section(uintptr_t inForm)
{ TSScheduler::DestroyCriticalSection(static_cast<TSCriticalSection*>(MFB(inForm)->mErrorCriticalSection)); }
extern "C" void ee_screenscpp_mfb_group_delete(uintptr_t inForm) { delete MFB(inForm)->mMainGroup; }
extern "C" void ee_screenscpp_mfb_poll_delete(uintptr_t inForm) { delete static_cast<TMPoll*>(MFB(inForm)->mPoll); }
extern "C" void ee_screenscpp_mfb_build_dereference(uintptr_t inForm) { static_cast<EEABuild*>(MFB(inForm)->mBuild)->Dereference(); }
extern "C" void ee_screenscpp_mfb_calamity_dereference(uintptr_t inForm) { static_cast<EEACalamity*>(MFB(inForm)->mCalamity)->Dereference(); }
extern "C" void ee_screenscpp_mfb_unit_hack_delete(uintptr_t inForm) { delete static_cast<EEUIFMGUnitHack*>(MFB(inForm)->mUnitHack); }
