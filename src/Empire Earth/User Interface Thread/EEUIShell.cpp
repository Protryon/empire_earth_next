// New (port; UI cluster round 3): the generic C++ shells' base calls and their host (ee/EEUIShell.h; ee_game screens
// shells.rs): the game's C++ each kind's Rust calls, one case per step of the deleted classes' code.
#include <windows.h>
#include <stdio.h>
#include "lle/LowLevelEngine.h"
#include "lle/GERhombus.h"
#include "lle/UResourceFile.h"
#include "lle/UIControl.h"
#include "lle/UIFontTable.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUIManager.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "lle/GESurface.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GERasterizer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUserInterface.h"
#include "ee/EESEARust.h"
#include "lle/Input System.h"
#include "ee/EEGhostUnit.h"
#include "ee/EEMap.h"
#include "ee/EEMouse.h"
#include "lle/SESoundManager.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEButtonObject.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "lle/UIControlsTable.h"
#include <set>
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "ee/EEUIShell.h"

// The icon bar kind's own ops (the old RustOp: a unit's class flags and goals, the selection message, the sounds,
// the keys, the mouse, the icons and the frame) are icon_bar_ops.rs' over the one-statement shims below (UI lane
// round 11)
#define IB(p) static_cast<EEUIShellControl*>((EEUIRustControl*)(p))
#define IB_CU(u) ((EEComplexUnit*)(u))
extern "C" bool ee_screenscpp_ib_class_transport(uintptr_t inUnit)
{ return IB_CU(inUnit)->GetUnitClass()->IsTransport(); }
extern "C" bool ee_screenscpp_ib_class_air_base(uintptr_t inUnit) { return IB_CU(inUnit)->GetUnitClass()->IsAirBase(); }
extern "C" bool ee_screenscpp_ib_class_paratrooper_bomber(uintptr_t inUnit)
{ return IB_CU(inUnit)->GetUnitClass()->mIsParatrooperBomber; }
extern "C" int64_t ee_screenscpp_ib_class_family(uintptr_t inUnit)
{ return (int64_t)IB_CU(inUnit)->GetUnitClass()->mFamily; }
extern "C" bool ee_screenscpp_ib_queued_goal(uintptr_t inUnit, uint64_t inGoal)
{ return IB_CU(inUnit)->GetQueuedGoal((unsigned long)inGoal) != NULL; }
extern "C" bool ee_screenscpp_ib_goals_empty(uintptr_t inUnit) { return IB_CU(inUnit)->GetQueueGoals(1).size() == 0; }
extern "C" bool ee_screenscpp_ib_goal_front_none(uintptr_t inUnit)
{ return IB_CU(inUnit)->mGoalQueues[1].GetFront() == NULL; }
extern "C" uintptr_t ee_screenscpp_ib_select_these(uintptr_t inIds, uint64_t inCount)
{ return (uintptr_t) new EEMRecord(kMessageSelectTheseUnits, {std::vector<long>((const int32_t*)inIds, (const int32_t*)inIds + inCount)}); }
extern "C" void ee_screenscpp_ib_send(uintptr_t inMessage)
{ gCommunications->ProcessLocalUserInput((NEMessage*)inMessage, kSlotNone); }
extern "C" void ee_screenscpp_ib_sound(uintptr_t inP, uint32_t inSlot)
{ gSoundManager->Play((unsigned long)(uintptr_t)IB(inP)->mSlots[inSlot], 1.0f, 0.0f, 1.0f, false, NULL); }
extern "C" bool ee_screenscpp_ib_ctrl() { return UIControl::ControlKeyPressed(); }
extern "C" bool ee_screenscpp_ib_shift() { return UIControl::ShiftKeyPressed(); }
extern "C" float ee_screenscpp_ib_mouse_x(uintptr_t inP) { return IB(inP)->GetMouseLocation().GetX(); }
extern "C" bool ee_screenscpp_ib_contains(uintptr_t inP)
{ return IB(inP)->GetRectangle().Contains(IB(inP)->GetMouseLocation()); }
extern "C" void ee_screenscpp_ib_button_acquire(uintptr_t inButton) { ((EEButtonObject*)inButton)->Acquire(); }
extern "C" void ee_screenscpp_ib_button_release(uintptr_t inButton) { ((EEButtonObject*)inButton)->Release(); }
extern "C" void ee_screenscpp_ib_icon_texture(uintptr_t inP, uintptr_t inButton)
{ static_cast<GEBitmap*>(IB(inP)->mSlots[0])->SetTexture(((EEButtonObject*)inButton)->GetTexture(), IB(inP)->GetScreenIndex()); }
extern "C" void ee_screenscpp_ib_icon_face(uintptr_t inP, uintptr_t inButton, float inLeft, float inTop, float inRight, float inBottom)
{ ((EEButtonObject*)inButton)->SetFaceBitmap(static_cast<GEBitmap*>(IB(inP)->mSlots[0]), GERectangle(inLeft, inTop, inRight, inBottom), gScreenIndex); }
extern "C" void ee_screenscpp_ib_slot_location(uintptr_t inP, uint32_t inSlot, float inX, float inY)
{ static_cast<GEBitmap*>(IB(inP)->mSlots[inSlot])->SetLocation(GE2DPoint(inX, inY)); }
extern "C" int64_t ee_screenscpp_ib_slot_draw(uintptr_t inP, uint32_t inSlot)
{ return IB(inP)->GetRasterizer()->Draw(static_cast<GEBitmap*>(IB(inP)->mSlots[inSlot]), IB(inP)->GetScreenIndex()); }

// A new file combo box (EEUIFileComboBox: the editor's map, sound and elevation lists): the path referenced, the
// list filled by the kind (shells_files.rs)
UIComboBox* EEUIShellNewFileCombo(FSPath* inPath, const UString& inPattern, unsigned long inId, GEFont* inFont,
                                  const GE2DPoint& inLocation, unsigned long inWidth, UIForm* inForm, unsigned long inIndex)
{
    inPath->Reference();
    return new EEUIShellCombo(EE_SCREENS_SK_FILE_COMBO, {(int64_t)inPath, (int64_t)(const char*)inPattern}, inId, 1, {},
                              inFont, GERectangle(inLocation, (float)inWidth, 20.0f), 400, inForm, inIndex, 0, 2);
}

static EEUIFScenarioEditor* EEShellTriggers() { return gUIManager->GetScenarioEditorForm(); }

// a shell's base class virtual (a form shell's: its host object is the UIForm*) and a form shell's own virtual
extern "C" int64_t ee_screenscpp_shell_base(void* inControl, uint32_t inVirtual, int64_t a, int64_t b, float* f)
{ return EEUIShell::Of(static_cast<UIControl*>(inControl))->BaseCall(inVirtual, a, b, f); }
extern "C" int64_t ee_screenscpp_shell_form_base(void* inForm, uint32_t inVirtual, int64_t a, int64_t b, float* f)
{ return static_cast<EEUIShellForm*>(static_cast<UIForm*>(inForm))->BaseCall(inVirtual, a, b, f); }
extern "C" int64_t ee_screenscpp_shell_form_self(void* inForm, uint32_t inVirtual)
{ return static_cast<EEUIShellForm*>(static_cast<UIForm*>(inForm))->SelfCall(inVirtual); }

extern "C" int64_t ee_screenscpp_shell(void* inControl, uint32_t inOp, int64_t a, int64_t b, float* f)
{
    UIControl* c = static_cast<UIControl*>(inControl);
    switch (inOp) {
    case EE_SCREENS_SH_BG_OPACITY: c->SetBgOpacity(f[0]); break;
    case EE_SCREENS_SH_BG_COLOR: c->SetBgColor(GEColor(f[0], f[1], f[2])); break;
    case EE_SCREENS_SH_BG_PLAYER_COLOR: c->SetBgColor(EEGetPlayerColor((unsigned long)a)); break;
    case EE_SCREENS_SH_SELECTED_COLOR: static_cast<UILBString*>(c)->GetSelectedBackground()->SetColor(GEColor(f[0], f[1], f[2])); break;
    case EE_SCREENS_SH_SELECTED_OPACITY: static_cast<UILBString*>(c)->GetSelectedBackground()->SetOpacity(f[0]); break;
    case EE_SCREENS_SH_SELECTION_INDEX: return (int64_t)(uint32_t)static_cast<EEUIShellCombo*>(c)->Selection();
    case EE_SCREENS_SH_SET_SELECTION: static_cast<UIComboBox*>(c)->SetCurrentSelection((unsigned long)a); break;
    case EE_SCREENS_SH_FILE_PATH_RELEASE: reinterpret_cast<FSPath*>(a)->Dereference(); break;
    case EE_SCREENS_SH_DELETE_ALL: static_cast<UIComboBox*>(c)->DeleteAllEntries(); break;
    case EE_SCREENS_SH_SCROLL_CAPTURED: return static_cast<UILBString*>(c)->GetScrollBarCaptured();
    case EE_SCREENS_SH_RELEASE_MOUSE: c->ReleaseCapturedMouse(); break;
    case EE_SCREENS_SH_FIRE: c->FireTriggerEvent((unsigned long)a); break;
    case EE_SCREENS_SH_SLIDER_VALUE: return (int64_t)(uint32_t)static_cast<UISlider*>(c)->GetCurrentRangeValue();
    case EE_SCREENS_SH_SLIDER_MAX: return static_cast<UISlider*>(c)->GetMaxRange();
    case EE_SCREENS_SH_LABEL_NUMBER: static_cast<UITextLabel*>(c->GetForm()->GetControlById((unsigned long)a))->SetText((long)b); break;
    case EE_SCREENS_SH_BUTTON_TEXT_BYTES:
        static_cast<UIButton*>(c)->SetText(UWideString(reinterpret_cast<const char*>(a)), reinterpret_cast<GEFont*>(b));
        break;
    case EE_SCREENS_SH_LIST_COUNT: return static_cast<UIListBox<UWideString>*>(c->GetForm()->GetControlById((unsigned long)a))->GetLineCount();
    case EE_SCREENS_SH_PLAYER_PAGE: static_cast<EEUIFScenarioEditor*>(c->GetForm())->PlayerCall(EE_SCREENS_SEPL_SELECT_PAGE, (uint32_t)a); break;
    case EE_SCREENS_SH_FILL_TECH_LISTS: gUIManager->GetScenarioEditorForm()->PlayerCall(EE_SCREENS_SEPL_FILL_TECH_LISTS); break;
    case EE_SCREENS_SH_SET_PLAYER_COUNT: static_cast<EEUIFScenarioEditor*>(c->GetForm())->PlayerCall(EE_SCREENS_SEPL_SET_PLAYER_COUNT, (uint32_t)a); break;
    case EE_SCREENS_SH_BITMAP_SCALE: reinterpret_cast<GEBitmap*>(a)->SetXYScale(f[0], f[1]); break;
    case EE_SCREENS_SH_BITMAP_RELEASE: reinterpret_cast<GEBitmap*>(a)->Dereference(); break;
    case EE_SCREENS_SH_SET_NORMAL: static_cast<UIButton*>(c)->SetNormalBitmap(reinterpret_cast<GEBitmap*>(a)); break;
    case EE_SCREENS_SH_SET_PRESSED: static_cast<UIButton*>(c)->SetPressedBitmap(reinterpret_cast<GEBitmap*>(a)); break;
    case EE_SCREENS_SH_BITMAP_OFFSET: static_cast<UIButton*>(c)->SetBitmapDrawOffset(GE2DPoint(f[0], f[1])); break;
    // the tech lists' (EESEUITechListBox)
    case EE_SCREENS_SH_LIST_VIEW: return static_cast<EEUIShellList*>(c)->View();
    case EE_SCREENS_SH_LIST_DELETE_ALL: static_cast<UILBString*>(c)->DeleteAllEntries(); break;
    case EE_SCREENS_SH_LIST_APPEND_BYTES: static_cast<UILBString*>(c)->AppendLine(UWideString(reinterpret_cast<const char*>(a))); break;
    case EE_SCREENS_SH_LIST_LINE_COUNT: return static_cast<UILBString*>(c)->GetLineCount();
    case EE_SCREENS_SH_LIST_START_LINE: static_cast<UILBString*>(c)->SetStartingDisplayLine((unsigned long)a); break;
    case EE_SCREENS_SH_LIST_SELECT_SCROLL: static_cast<UILBString*>(c)->ChangeSelectionIndex((unsigned long)a, false, true); break;
    case EE_SCREENS_SH_FORM_CONTROL: return (int64_t)c->GetForm()->GetControlById((unsigned long)a);
    // the unit tab's (EEUIFSELBUnitList, EEUIFSELBCargoList, EEUIFSEBRotation)
    case EE_SCREENS_SH_UNIT_SELECT_TYPE: static_cast<EEUIFScenarioEditor*>(c->GetForm())->UnitCall(EE_SCREENS_SEUN_SELECT); break;
    case EE_SCREENS_SH_ENTER_KEY: return EEUIShellList::EnterKey();
    case EE_SCREENS_SH_DELETE_UNIT: (new EESEARust(EE_SCREENS_SEW_DELETE_UNIT, NULL, (int32_t)a))->Dereference(); break;
    case EE_SCREENS_SH_BITMAP_ROTATION: reinterpret_cast<GEBitmap*>(a)->SetRotation(f[0]); break;
    case EE_SCREENS_SH_SELF: return EEUIShell::Of(c)->SelfCall((uint32_t)a);
    case EE_SCREENS_SH_KEYBOARD: EEUIShell::Of(c)->Keyboard(a != 0); break;
    case EE_SCREENS_SH_ALWAYS_DIRTY: c->SetAlwaysDirty(a != 0); break;
    case EE_SCREENS_SH_EDITOR_TEXTURE: return (int64_t) static_cast<EEUIFScenarioEditor*>(c->GetForm())->GetButtonTexture();
    // the message box's buttons' (shells_dialog.rs)
    case EE_SCREENS_SH_FOCUSED_TEXT_COLOR: static_cast<UIButton*>(c)->SetFocusedTextColor(GEColor(f[0], f[1], f[2])); break;
    case EE_SCREENS_SH_CLOSE_FORM: gUserInterface->CloseForm(reinterpret_cast<UIForm*>(a)); break;
    // the saved-group buttons' (shells_saved.rs)
    case EE_SCREENS_SH_SG_NEW_PORTRAIT: return (int64_t) new GEBitmap;
    case EE_SCREENS_SH_SG_PLAY: gSoundManager->Play((unsigned long)a, 1.0f, 0.0f, 1.0f, false, NULL); break;
    case EE_SCREENS_SH_SG_PLAYER: return (int64_t)gWorld.GetCurrentPlayer();
    case EE_SCREENS_SH_SG_SHIFT: return UIControl::ShiftKeyPressed();
    case EE_SCREENS_SH_SG_INDEX: return (int64_t)c->GetIndex();
    case EE_SCREENS_SH_SG_SET_TEXTURE: reinterpret_cast<GEBitmap*>(a)->SetTexture(reinterpret_cast<GETexture*>(b), c->GetScreenIndex()); break;
    // the icon bar's (shells_icon_bar.rs): the slots let go as its destructor did
    // the chat line's (shells_chat.rs): its texts deleted, as its destructor did
    // the paint areas' (shells_paint.rs)
    case EE_SCREENS_SH_MAP_HALF: f[0] = gWorld.GetMap()->GetF1955F4(); break;
    case EE_SCREENS_SH_WINDOW_SIZE: return (int64_t)gGameWindow->GetWidth() | (int64_t)gGameWindow->GetHeight() << 32;
    case EE_SCREENS_SH_MOUSE_BOUNDS: gMouse->SetBounds(GERectangle(f[0], f[1], f[2], f[3])); break;
    case EE_SCREENS_SH_SCROLL_EDGES: reinterpret_cast<EEAScroll*>(a)->mEdges = GERectangle(f[0], f[1], f[2], f[3]); break;
    case EE_SCREENS_SH_CAPTURE_MOUSE: static_cast<EEUIShellControl*>(c)->Capture(); break;
    case EE_SCREENS_SH_PAINT_NEW: return (int64_t) static_cast<EESEAPaint*>(new EESEAPaintRust((uint32_t)a, (char)b));
    case EE_SCREENS_SH_ACTION_POST: gUserInterface->PostAction(reinterpret_cast<EESEAPaint*>(a)); break;
    case EE_SCREENS_SH_ACTION_RELEASE: reinterpret_cast<EESEAPaint*>(a)->Dereference(); break;
    // the scenario editor lane's (0xa0..): the diplomacy buttons' state, an action's post and release
    case EE_SCREENS_SH_SE_DIPLOMACY: gEEUIShellDiplomacyShow(static_cast<UIButton*>(c), (unsigned long)a, b != 0); break;
    case EE_SCREENS_SH_SE_ACTION_POST: gUserInterface->PostAction(reinterpret_cast<EEAction*>(a)); break;
    case EE_SCREENS_SH_SE_ACTION_RELEASE: reinterpret_cast<EEAction*>(a)->Dereference(); break;
    case EE_SCREENS_SH_BRUSH_HIGHLIGHT: gUIManager->GetScenarioEditorForm()->GetBrushGroup()->SetHighlight(f[0], f[1], a != 0); break;
    case EE_SCREENS_SH_BRUSH_GROUP_SIZE: return reinterpret_cast<EEUIFSEBrushGroup*>(a)->mBrushSize;
    case EE_SCREENS_SH_TERRAIN_RUST: return (int64_t)ee_screens_segr_tab(reinterpret_cast<EeScreensSeGroup*>(a));
    // the game's
    case EE_SCREENS_SH_TRIGGER_REFRESH: EEShellTriggers()->TriggerSubSlot((long)a, EE_SCREENS_SETS_SCREEN_REFRESH); break;
    case EE_SCREENS_SH_TRIGGER_REFRESH_SCREEN: EEShellTriggers()->TriggerCall(EE_SCREENS_SETS_REFRESH_SCREEN); break;
    case EE_SCREENS_SH_TRIGGER_JUMP: EESETriggerJump((unsigned long)a, (unsigned long)b); break;
    case EE_SCREENS_SH_OBJECT_UPDATE_SELECTION: EEShellTriggers()->TriggerSubOp(EE_SCREENS_SE_TRIGGER_SCREEN_OBJECT, EE_SCREENS_SEOB_UPDATE_SELECTION); break;
    case EE_SCREENS_SH_OBJECT_STORE: EEShellTriggers()->TriggerSubSlot(EE_SCREENS_SE_TRIGGER_SCREEN_OBJECT, EE_SCREENS_SETS_SCREEN_STORE); break;
    case EE_SCREENS_SH_COPY_FIRST_TO_ALL: static_cast<EEUIFScenarioEditor*>(c->GetForm())->PlayerCall(EE_SCREENS_SEPL_COPY_FIRST_TO_ALL); break;
    case EE_SCREENS_SH_BRUSH_SIZE: reinterpret_cast<EEUIFSEBrushGroup*>(a)->mBrushSize = (long)b; break;
    }
    return 0;
}

// ---- the shims of the moved cases (UI lane round 7: ee_game screens shell_calls.rs, game_cpp.rs), each one C++
// statement on the shell's control or the world
#include "ee/EEScreensShims.h"
static UIControl* C(uintptr_t inControl) { return reinterpret_cast<UIControl*>(inControl); }
// shims6
extern "C" void ee_screenscpp_shell_text_fg(uintptr_t inControl, uint32_t inWhich, float inRed, float inGreen, float inBlue)
{ EEUIShell::Of(C(inControl))->Text(inWhich)->SetFgColor(GEColor(inRed, inGreen, inBlue)); }
extern "C" void ee_screenscpp_shell_text_transparent(uintptr_t inControl, uint32_t inWhich)
{ EEUIShell::Of(C(inControl))->Text(inWhich)->SetIsTransparent(true); }
extern "C" void ee_screenscpp_shell_button_set_text(uintptr_t inControl, EeScreensShimText inText, uintptr_t inFont)
{ static_cast<UIButton*>(C(inControl))->SetText(EEShimText(inText), reinterpret_cast<GEFont*>(inFont)); }
extern "C" void ee_screenscpp_shell_combo_append(uintptr_t inControl, EeScreensShimText inText)
{ static_cast<UIComboBox*>(C(inControl))->AppendLine(EEShimText(inText)); }
extern "C" uint32_t ee_screenscpp_shell_combo_line_count(uintptr_t inControl)
{ return (uint32_t)static_cast<UIComboBox*>(C(inControl))->GetListBox()->GetLineCount(); }
extern "C" bool ee_screenscpp_shell_combo_line_is(uintptr_t inControl, uint32_t inLine, uintptr_t inText)
{ return static_cast<UIComboBox*>(C(inControl))->GetListBox()->GetData(inLine) == *reinterpret_cast<const UWideString*>(inText); }
extern "C" void ee_screenscpp_sound_unload_media(uintptr_t inSound)
{ reinterpret_cast<EESoundObject*>(inSound)->UnloadMedia(); }
extern "C" uint32_t ee_screenscpp_sound_sample_id(uintptr_t inSound)
{ return (uint32_t)reinterpret_cast<EESoundObject*>(inSound)->GetSampleID(); }
extern "C" uintptr_t ee_screenscpp_selected_first_unit()
{ return (uintptr_t)gWorld.GetCurrentPlayer()->mSelectedGroup->GetFirstUnit(); }
extern "C" uintptr_t ee_screenscpp_player_group(uint32_t inGroup)
{ return (uintptr_t)gWorld.GetCurrentPlayer()->mGroups[inGroup]; }
extern "C" uint32_t ee_screenscpp_group_count(uintptr_t inGroup)
{ return (uint32_t)reinterpret_cast<EEGroup*>(inGroup)->GetNumUnits(); }
extern "C" uintptr_t ee_screenscpp_group_first_unit(uintptr_t inGroup)
{ return (uintptr_t)reinterpret_cast<EEGroup*>(inGroup)->GetFirstUnit(); }
extern "C" void ee_screenscpp_send_saved_group_select(uint32_t inGroup, bool inAdd)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageSavedGroupSelect, {(unsigned long)inGroup, inAdd}), kSlotNone); }
extern "C" void ee_screenscpp_send_saved_group_add_selection(uint32_t inGroup)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageSavedGroupAddSelection, {(unsigned long)inGroup}), kSlotNone); }
extern "C" void ee_screenscpp_send_saved_group_create(uint32_t inGroup)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageSavedGroupCreate, {(unsigned long)inGroup}), kSlotNone); }
extern "C" void ee_screenscpp_send_saved_group_clear(uint32_t inGroup)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageSavedGroupClear, {(unsigned long)inGroup}), kSlotNone); }
extern "C" uintptr_t ee_screenscpp_unit_stats_button(uintptr_t inUnit)
{ return (uintptr_t)reinterpret_cast<EEComplexUnit*>(inUnit)->mStats->mButton; }
extern "C" void ee_screenscpp_button_set_face(uintptr_t inButton, uintptr_t inBitmap, uintptr_t inControl)
{ reinterpret_cast<EEButtonObject*>(inButton)->SetFaceBitmap(reinterpret_cast<GEBitmap*>(inBitmap), C(inControl)->GetRectangle(), gScreenIndex); }
extern "C" float ee_screenscpp_button_cell_u(uintptr_t inButton)
{ return reinterpret_cast<EEButtonObject*>(inButton)->GetCellU(); }
extern "C" float ee_screenscpp_button_cell_v(uintptr_t inButton)
{ return reinterpret_cast<EEButtonObject*>(inButton)->GetCellV(); }
extern "C" uintptr_t ee_screenscpp_button_texture(uintptr_t inButton)
{ return (uintptr_t)reinterpret_cast<EEButtonObject*>(inButton)->GetTexture(); }
extern "C" void ee_screenscpp_texture_reference(uintptr_t inTexture)
{ reinterpret_cast<GETexture*>(inTexture)->Reference(); }
extern "C" void ee_screenscpp_texture_dereference(uintptr_t inTexture)
{ reinterpret_cast<GETexture*>(inTexture)->Dereference(); }
extern "C" void ee_screenscpp_bitmap_at_button(uintptr_t inBitmap, uintptr_t inControl)
{ reinterpret_cast<GEBitmap*>(inBitmap)->SetLocation(static_cast<UIButton*>(C(inControl))->GetBitmapLocation()); }
extern "C" void ee_screenscpp_shell_draw_bitmap(uintptr_t inControl, uintptr_t inBitmap)
{ C(inControl)->GetRasterizer()->Draw(reinterpret_cast<GEBitmap*>(inBitmap), C(inControl)->GetScreenIndex()); }
extern "C" uintptr_t ee_screenscpp_shell_slot(uintptr_t inControl, uint32_t inSlot)
{ return (uintptr_t)static_cast<EEUIShellControl*>(C(inControl))->mSlots[inSlot]; }
extern "C" void ee_screenscpp_shell_clear_slot(uintptr_t inControl, uint32_t inSlot)
{ static_cast<EEUIShellControl*>(C(inControl))->mSlots[inSlot] = NULL; }
extern "C" void ee_screenscpp_text_delete(uintptr_t inText) { delete reinterpret_cast<GEText*>(inText); }
extern "C" float ee_screenscpp_control_width(uintptr_t inControl) { return C(inControl)->GetRectangle().GetWidth(); }
extern "C" float ee_screenscpp_control_height(uintptr_t inControl) { return C(inControl)->GetRectangle().GetHeight(); }
extern "C" void ee_screenscpp_shim_tile_clear() { EEScratch<EETile*>(kScratchUIShellShimTile) = NULL; }
extern "C" void ee_screenscpp_shim_point_clear() { EEScratch<GE3DPoint>(kScratchShimPoint) = GE3DPoint(); }
extern "C" bool ee_screenscpp_pick_tile_xy(float inX, float inY, long* inTileX, long* inTileY)
{ return EEPointUnitOrTileXY(GE2DPoint(inX, inY), EEScratch<EETile*>(kScratchUIShellShimTile), *inTileX, *inTileY, false); }
extern "C" bool ee_screenscpp_shim_tile_exists() { return EEScratch<EETile*>(kScratchUIShellShimTile) != NULL; }
extern "C" float ee_screenscpp_shim_tile_height() { return EEScratch<EETile*>(kScratchUIShellShimTile)->GetHeight(); }
extern "C" void ee_screenscpp_pick_map_location(float inX, float inY)
{ EEScratch<EETile*>(kScratchUIShellShimTile) = gWorld.GetMap()->GetLocationAt(GE2DPoint(inX, inY), EEScratch<GE3DPoint>(kScratchShimPoint), false); }
extern "C" void ee_screenscpp_pick_unit_or_tile(float inX, float inY)
{ EEPointUnitOrTile(GE2DPoint(inX, inY), EEScratch<EETile*>(kScratchUIShellShimTile), EEScratch<GE3DPoint>(kScratchShimPoint), false); }
extern "C" float ee_screenscpp_shim_point_x() { return EEScratch<GE3DPoint>(kScratchShimPoint).GetX(); }
extern "C" float ee_screenscpp_shim_point_y() { return EEScratch<GE3DPoint>(kScratchShimPoint).GetY(); }
extern "C" float ee_screenscpp_shim_point_z() { return EEScratch<GE3DPoint>(kScratchShimPoint).GetZ(); }
extern "C" void ee_screenscpp_map_tile_load(uint32_t inX, uint32_t inY)
{ EEScratch<EEMapTile*>(kScratchShimMapTile) = gWorld.mMap->GetTile((unsigned long)inX, (unsigned long)inY); }
extern "C" bool ee_screenscpp_shim_map_tile_exists() { return EEScratch<EEMapTile*>(kScratchShimMapTile) != NULL; }
extern "C" float ee_screenscpp_shim_map_tile_height() { return EEScratch<EEMapTile*>(kScratchShimMapTile)->GetHeight(); }
extern "C" float ee_screenscpp_mouse_x() { return gMouse->mPosition.GetX(); }
extern "C" float ee_screenscpp_mouse_y() { return gMouse->mPosition.GetY(); }
extern "C" float ee_screenscpp_shell_mouse_x(uintptr_t inControl)
{ return static_cast<EEUIShellControl*>(C(inControl))->MouseLocation().GetX(); }
extern "C" float ee_screenscpp_shell_mouse_y(uintptr_t inControl)
{ return static_cast<EEUIShellControl*>(C(inControl))->MouseLocation().GetY(); }
extern "C" float ee_screenscpp_view_left() { return gViewport.GetViewRectangle().GetLeft(); }
extern "C" float ee_screenscpp_view_top() { return gViewport.GetViewRectangle().GetTop(); }
extern "C" float ee_screenscpp_view_right() { return gViewport.GetViewRectangle().GetRight(); }
extern "C" float ee_screenscpp_view_bottom() { return gViewport.GetViewRectangle().GetBottom(); }
// end shims6

// ---- the rest of the multi-statement cases (UI lane round 9: shell_calls.rs), each one statement, and the
// cases' locals (EEShellTree and EEShellRotate are Rust's)
#define TREE(inTree) reinterpret_cast<EETechTree*>(inTree)
static const unsigned long kShKeys[] = {kKeyComma, kKeyPeriod, kKeySpace};
extern "C" void ee_screenscpp_sh_files_clear() { EEScratch<std::set<UString>>(kScratchShFiles).clear(); }
extern "C" void ee_screenscpp_sh_files_list(uintptr_t inPath, uintptr_t inPattern)
{ reinterpret_cast<FSPath*>(inPath)->ListFiles(UString(reinterpret_cast<const char*>(inPattern)), EEScratch<std::set<UString>>(kScratchShFiles)); }
extern "C" void ee_screenscpp_sh_files_snapshot() { EEScratch<std::vector<UString>>(kScratchShLines).assign(EEScratch<std::set<UString>>(kScratchShFiles).begin(), EEScratch<std::set<UString>>(kScratchShFiles).end()); }
extern "C" uint32_t ee_screenscpp_sh_files_count() { return (uint32_t)EEScratch<std::vector<UString>>(kScratchShLines).size(); }
extern "C" void ee_screenscpp_sh_text_from_file(uint32_t inIndex) { EEScratch<UWideString>(kScratchShText) = EEScratch<std::vector<UString>>(kScratchShLines)[inIndex]; }
extern "C" void ee_screenscpp_sh_text_trim4() { EEScratch<UWideString>(kScratchShText).InsertString("", EEScratch<UWideString>(kScratchShText).GetLength() - 4, true); }
extern "C" void ee_screenscpp_sh_combo_append_text(uintptr_t inControl)
{ static_cast<UIComboBox*>(C(inControl))->AppendLine(EEScratch<UWideString>(kScratchShText)); }
extern "C" void ee_screenscpp_sh_numbered_format(int64_t inId) { gResource->GetString((unsigned long)inId, EEScratch<UWideString>(kScratchShFormat)); }
extern "C" void ee_screenscpp_sh_numbered_print(uint32_t inNumber)
{ sprintf(EEScratch<UWideString>(kScratchShText), EEScratch<UWideString>(kScratchShFormat), (unsigned long)inNumber); }
extern "C" void ee_screenscpp_sh_radio_clear_down_checked(uintptr_t inControl)
{ static_cast<UIRadioButton*>(C(inControl))->SetDownCheckedBitmap(NULL); }
extern "C" void ee_screenscpp_sh_radio_clear_up_checked(uintptr_t inControl)
{ static_cast<UIRadioButton*>(C(inControl))->SetUpCheckedBitmap(NULL); }
extern "C" void ee_screenscpp_sh_radio_clear_down_unchecked(uintptr_t inControl)
{ static_cast<UIRadioButton*>(C(inControl))->SetDownUncheckedBitmap(NULL); }
extern "C" void ee_screenscpp_sh_radio_clear_up_unchecked(uintptr_t inControl)
{ static_cast<UIRadioButton*>(C(inControl))->SetUpUncheckedBitmap(NULL); }
extern "C" int64_t ee_screenscpp_sh_form_list_selection(uintptr_t inControl, int64_t inId, unsigned long* inSelection)
{ return (int64_t)static_cast<UIListBox<UWideString>*>(C(inControl)->GetForm()->GetControlById((unsigned long)inId))->GetCurrentSelection(*inSelection); }
extern "C" void ee_screenscpp_sh_editor_player(unsigned long* inSelection)
{ gUIManager->GetScenarioEditorForm()->GetPlayerComboBox()->GetCurrentSelection(*inSelection); }
extern "C" void ee_screenscpp_sh_tech_tree_move(int32_t inA, int32_t inB, int32_t inC, bool inFlag)
{ (new EESEARust(EE_SCREENS_SEW_TECH_TREE, NULL, inA, inB, inC, inFlag))->Dereference(); }
extern "C" void ee_screenscpp_sh_combo_list_selection(uintptr_t inControl, unsigned long* inSelection)
{ const_cast<UILBString*>(static_cast<UIComboBox*>(C(inControl))->GetListBox())->GetCurrentSelection(*inSelection); }
extern "C" uintptr_t ee_screenscpp_sh_bitmap_new() { return (uintptr_t)new GEBitmap; }
extern "C" void ee_screenscpp_sh_bitmap_texture_form(uintptr_t inBitmap, uintptr_t inTexture, uintptr_t inControl)
{ reinterpret_cast<GEBitmap*>(inBitmap)->SetTexture(reinterpret_cast<GETexture*>(inTexture), C(inControl)->GetForm()->GetManager()->GetScreen()); }
extern "C" void ee_screenscpp_sh_bitmap_texture_global(uintptr_t inBitmap, uintptr_t inTexture)
{ reinterpret_cast<GEBitmap*>(inBitmap)->SetTexture(reinterpret_cast<GETexture*>(inTexture), gScreenIndex); }
extern "C" void ee_screenscpp_sh_bitmap_coords(uintptr_t inBitmap, float inLeft, float inTop, float inRight, float inBottom)
{ reinterpret_cast<GEBitmap*>(inBitmap)->SetTextureCoordinates(GETextureCoordinate(inLeft, inTop), GETextureCoordinate(inRight, inTop), GETextureCoordinate(inLeft, inBottom), GETextureCoordinate(inRight, inBottom)); }
extern "C" float ee_screenscpp_sh_texture_width(uintptr_t inTexture)
{ return (float)reinterpret_cast<GETexture*>(inTexture)->GetWidth(); }
extern "C" float ee_screenscpp_sh_texture_height(uintptr_t inTexture)
{ return (float)reinterpret_cast<GETexture*>(inTexture)->GetHeight(); }
extern "C" bool ee_screenscpp_sh_has_user_interface() { return gUserInterface != NULL; }
extern "C" void ee_screenscpp_sh_post_select_tab(uint8_t inTab, int32_t inPage)
{ gUserInterface->PostSelectEditorTab((unsigned char)inTab, (long)inPage); }
extern "C" void ee_screenscpp_sh_select_tab(uintptr_t inControl, uint8_t inTab, int32_t inPage)
{ static_cast<EEUIFScenarioEditor*>(C(inControl)->GetForm())->SelectTab((unsigned char)inTab, (long)inPage); }
extern "C" int64_t ee_screenscpp_sh_list_current(uintptr_t inControl, unsigned long* inSelection)
{ return (int64_t)static_cast<UILBString*>(C(inControl))->GetCurrentSelection(*inSelection); }
extern "C" uintptr_t ee_screenscpp_sh_form_control(uintptr_t inControl, int64_t inId)
{ return (uintptr_t)C(inControl)->GetForm()->GetControlById((unsigned long)inId); }
extern "C" bool ee_screenscpp_sh_is_radio(uintptr_t inControl)
{ return dynamic_cast<UIRadioButton*>(C(inControl)) != NULL; }
extern "C" bool ee_screenscpp_sh_radio_checked(uintptr_t inControl)
{ return dynamic_cast<UIRadioButton*>(C(inControl))->GetIsChecked(); }
extern "C" bool ee_screenscpp_sh_check_checked(uintptr_t inControl)
{ return static_cast<UICheckBox*>(C(inControl))->GetIsChecked(); }
extern "C" int64_t ee_screenscpp_sh_tree_depends(uintptr_t inTree, int64_t inTechnology, int64_t inOn)
{ return (int64_t)TREE(inTree)->Get((long)inTechnology)->DependsOn((long)inOn); }
extern "C" void ee_screenscpp_sh_tree_set_obsoleted(uintptr_t inTree, int64_t inTechnology, bool inObsoleted)
{ TREE(inTree)->Get((long)inTechnology)->SetScenEditorObsoleted(inObsoleted, true); }
extern "C" bool ee_screenscpp_sh_tree_epoch_reached(uintptr_t inTree, int64_t inTechnology)
{ return TREE(inTree)->Get((long)inTechnology)->mEpoch <= TREE(inTree)->mCurrentEpoch; }
extern "C" int64_t ee_screenscpp_sh_tree_obsoleted(uintptr_t inTree, int64_t inTechnology)
{ return (int64_t)TREE(inTree)->Get((long)inTechnology)->mScenEditorObsoleted; }
extern "C" void ee_screenscpp_sh_tree_mark_player(uintptr_t inTree)
{ gUIManager->GetScenarioEditorForm()->PlayerCall(EE_SCREENS_SEPL_MARK_CHANGED, TREE(inTree)->GetPlayer()->mPlayerID, false); }
extern "C" int64_t ee_screenscpp_sh_class_name_id(int64_t inUnitClass)
{ return (int64_t)gUnitClassTable[(long)inUnitClass]->mNameStringID; }
extern "C" int64_t ee_screenscpp_sh_class_epoch(int64_t inUnitClass)
{ return (int64_t)gUnitClassTable[(long)inUnitClass]->mEpoch; }
extern "C" int64_t ee_screenscpp_sh_class_family(int64_t inUnitClass)
{ return (int64_t)gUnitClassTable[(long)inUnitClass]->mFamily; }
extern "C" void ee_screenscpp_sh_list_insert(uintptr_t inControl, uintptr_t inText, int64_t inIndex)
{ static_cast<UILBString*>(C(inControl))->InsertLine(UWideString(reinterpret_cast<const char*>(inText)), (unsigned long)inIndex, EEScratch<UILBNoSort<UWideString>>(kScratchShNoSort)); }
extern "C" void ee_screenscpp_sh_list_data(uintptr_t inControl, int64_t inIndex, void* inOut)
{ EEShimOutC(inOut, (const char*)static_cast<UILBString*>(C(inControl))->GetData((unsigned long)inIndex)); }
extern "C" int64_t ee_screenscpp_sh_unit_name_id(uintptr_t inUnit)
{ return (int64_t)reinterpret_cast<EEUnit*>(inUnit)->mUnitClass->mNameStringID; }
extern "C" uint32_t ee_screenscpp_sh_unit_id(uintptr_t inUnit)
{ return (uint32_t)reinterpret_cast<EEUnit*>(inUnit)->GetUnitID(); }
extern "C" uint32_t ee_screenscpp_sh_ghost_count() { return (uint32_t)gWorld.GetNumGhostUnits(); }
extern "C" void ee_screenscpp_sh_ghosts_enter() { gWorld.mGhostUnitsLock->Enter(); }
extern "C" void ee_screenscpp_sh_ghosts_leave() { gWorld.mGhostUnitsLock->Leave(); }
extern "C" bool ee_screenscpp_sh_ghost_turns(uint32_t inIndex)
{ return gUnitClassTable[static_cast<EEGhostUnit*>(gWorld.GetGhostUnit(inIndex))->mUnitClass->mUnitType]->mAngularAcceleration > 0.0f; }
extern "C" void ee_screenscpp_sh_tr_from_ghost(uint32_t inIndex)
{ EEScratch<GETransformation>(kScratchShTransformation) = static_cast<EEGhostUnit*>(gWorld.GetGhostUnit(inIndex))->mTransformation; }
extern "C" void ee_screenscpp_sh_tr_orient(float inAngle) { EEScratch<GETransformation>(kScratchShTransformation).SetOrientationYPR(inAngle, 0.0f, 0.0f); }
extern "C" void ee_screenscpp_sh_tr_to_ghost(uint32_t inIndex)
{ static_cast<EEGhostUnit*>(gWorld.GetGhostUnit(inIndex))->mTransformation = EEScratch<GETransformation>(kScratchShTransformation); }
extern "C" uint32_t ee_screenscpp_sh_editor_selected_count(uintptr_t inControl)
{ return (uint32_t)static_cast<EEUIFScenarioEditor*>(C(inControl)->GetForm())->GetSelectedUnitCount(); }
extern "C" uintptr_t ee_screenscpp_sh_editor_selected(uintptr_t inControl, uint32_t inIndex)
{ return (uintptr_t)static_cast<EEUIFScenarioEditor*>(C(inControl)->GetForm())->GetSelectedUnit(inIndex); }
extern "C" bool ee_screenscpp_sh_unit_has_complex(uintptr_t inUnit)
{ return reinterpret_cast<EEUnit*>(inUnit)->mComplexUnit != NULL; }
extern "C" bool ee_screenscpp_sh_unit_turns(uintptr_t inUnit)
{ return reinterpret_cast<EEUnit*>(inUnit)->mUnitClass->mAngularAcceleration > 0.0f; }
extern "C" void ee_screenscpp_sh_tr_from_unit(uintptr_t inUnit)
{ EEScratch<GETransformation>(kScratchShTransformation) = reinterpret_cast<EEUnit*>(inUnit)->mComplexUnit->mPhysicalModel.GetLocation(); }
extern "C" void ee_screenscpp_sh_tr_to_unit(uintptr_t inUnit)
{ reinterpret_cast<EEUnit*>(inUnit)->mComplexUnit->SetTransformation(EEScratch<GETransformation>(kScratchShTransformation), true); }
extern "C" int64_t ee_screenscpp_sh_key_load(uint32_t inWhich)
{ return (int64_t)gISKeyboard->GetEntity(kShKeys[inWhich], EEScratch<ISEntity*>(kScratchShKey)); }
extern "C" bool ee_screenscpp_sh_key_just_pressed() { return EEScratch<ISEntity*>(kScratchShKey)->JustPressed() != 0; }
extern "C" bool ee_screenscpp_sh_key_just_unpressed() { return EEScratch<ISEntity*>(kScratchShKey)->JustUnpressed() != 0; }
extern "C" int64_t ee_screenscpp_sh_key_value() { return (int64_t)EEScratch<ISEntity*>(kScratchShKey)->GetValue(); }
extern "C" void ee_screenscpp_sh_screen_line(float inX, float inY)
{ gWorld.mMap->GetScreenLine(GE2DPoint(inX, inY), EEScratch<GE3DLine>(kScratchShLine)); }
extern "C" float ee_screenscpp_sh_line_start_x() { return EEScratch<GE3DLine>(kScratchShLine).GetStart().GetX(); }
extern "C" float ee_screenscpp_sh_line_start_y() { return EEScratch<GE3DLine>(kScratchShLine).GetStart().GetY(); }
extern "C" float ee_screenscpp_sh_line_start_z() { return EEScratch<GE3DLine>(kScratchShLine).GetStart().GetZ(); }
extern "C" float ee_screenscpp_sh_line_direction_x() { return EEScratch<GE3DLine>(kScratchShLine).GetDirection().GetX(); }
extern "C" float ee_screenscpp_sh_line_direction_y() { return EEScratch<GE3DLine>(kScratchShLine).GetDirection().GetY(); }
extern "C" float ee_screenscpp_sh_line_direction_z() { return EEScratch<GE3DLine>(kScratchShLine).GetDirection().GetZ(); }
extern "C" bool ee_screenscpp_sh_right_down(uintptr_t inControl) { return C(inControl)->GetRightMouseButtonDown(); }
extern "C" bool ee_screenscpp_sh_left_down(uintptr_t inControl) { return C(inControl)->GetLeftMouseButtonDown(); }
extern "C" bool ee_screenscpp_sh_has_capture(uintptr_t inControl) { return C(inControl)->GetHasMouseCapture(); }
extern "C" int64_t ee_screenscpp_sh_paint_area(uintptr_t inPaint, int32_t inLeft, int32_t inTop, int32_t inRight, int32_t inBottom)
{ return (int64_t)reinterpret_cast<EESEAPaint*>(inPaint)->SetArea(inLeft, inTop, inRight, inBottom); }
extern "C" int64_t ee_screenscpp_sh_slider_max(uintptr_t inControl, int64_t inId)
{ return (int64_t)static_cast<UISlider*>(C(inControl)->GetForm()->GetControlById((unsigned long)inId))->GetMaxRange(); }
extern "C" uint32_t ee_screenscpp_sh_slider_current(uintptr_t inControl, int64_t inId)
{ return (uint32_t)static_cast<UISlider*>(C(inControl)->GetForm()->GetControlById((unsigned long)inId))->GetCurrentRangeValue(); }
extern "C" uintptr_t ee_screenscpp_sh_rasterizer(uintptr_t inControl)
{ return (uintptr_t)C(inControl)->GetRasterizer(); }
extern "C" uint32_t ee_screenscpp_sh_screen_index(uintptr_t inControl)
{ return (uint32_t)C(inControl)->GetScreenIndex(); }
extern "C" uintptr_t ee_screenscpp_sh_copied_draw_rectangle(uintptr_t inControl)
{ return (uintptr_t)&C(inControl)->GetCopiedDrawRectangle(); }

// the icon bar's and chat line's factories and the tech list's bars: shell_factories.rs over these (UI lane round 12)
#define SF(c) static_cast<EEUIShellControl*>(reinterpret_cast<UIControl*>(c))
#define SFT(t) reinterpret_cast<UIControlsTable*>(t)
extern "C" int64_t ee_screenscpp_sf_shell_op(uintptr_t inControl, uint32_t inOp, int64_t inA, int64_t inB, float* inF)
{ return EEUIShell::Op(reinterpret_cast<UIControl*>(inControl), inOp, inA, inB, inF); }
extern "C" void ee_screenscpp_sf_slot_set(uintptr_t inControl, uint32_t inSlot, uintptr_t inValue)
{ SF(inControl)->mSlots[inSlot] = (void*)inValue; }
extern "C" uintptr_t ee_screenscpp_sf_slots(uintptr_t inControl) { return (uintptr_t)SF(inControl)->mSlots; }
extern "C" uintptr_t ee_screenscpp_sf_bitmap_new() { return (uintptr_t)new GEBitmap; }
extern "C" uintptr_t ee_screenscpp_sf_normal_bitmap(uintptr_t inTable, uintptr_t inForm, float inLeft, float inTop, float inRight, float inBottom)
{ return (uintptr_t)SFT(inTable)->CreateNormalBitmap(((UIForm*)inForm)->GetManager(), GERectangle(inLeft, inTop, inRight, inBottom)); }
extern "C" uintptr_t ee_screenscpp_sf_sound(uint32_t inId) { return (uintptr_t)EESoundObject::Get(inId); }
extern "C" void ee_screenscpp_sf_sound_load(uintptr_t inSound) { ((EESoundObject*)inSound)->LoadMedia(); }
extern "C" uintptr_t ee_screenscpp_sf_sound_sample(uintptr_t inSound)
{ return (uintptr_t)((EESoundObject*)inSound)->GetSampleID(); }
extern "C" float ee_screenscpp_sf_table_bg_opacity(uintptr_t inTable) { return SFT(inTable)->GetBackgroundOpacity(); }
extern "C" bool ee_screenscpp_sf_table_always_dirty(uintptr_t inTable) { return SFT(inTable)->GetAlwaysDirty(); }
extern "C" uintptr_t ee_screenscpp_sf_table_font(uintptr_t inTable) { return (uintptr_t)SFT(inTable)->GetFont(); }
extern "C" void ee_screenscpp_sf_set_always_dirty(uintptr_t inControl, bool inOn)
{ SF(inControl)->SetAlwaysDirty(inOn); }
extern "C" void ee_screenscpp_sf_bg_color_of_table(uintptr_t inControl, uintptr_t inTable)
{ SF(inControl)->SetBgColor(SFT(inTable)->GetBackground()); }
extern "C" void ee_screenscpp_sf_bg_opacity(uintptr_t inControl, float inOpacity)
{ SF(inControl)->SetBgOpacity(inOpacity); }
extern "C" uintptr_t ee_screenscpp_sf_bold_font(uintptr_t inFont)
{ return (uintptr_t)UIFontTable::FindOrCreateFont(((GEFont*)inFont)->GetFontName(), ((GEFont*)inFont)->GetFontStyle() | kFontStyleBold, ((GEFont*)inFont)->GetFontQuality(), ((GEFont*)inFont)->GetFontSize()); }
extern "C" uintptr_t ee_screenscpp_sf_text_new(uintptr_t inFont) { return (uintptr_t)new GEText((GEFont*)inFont); }
extern "C" void ee_screenscpp_sf_table_setup_text(uintptr_t inTable, uintptr_t inText)
{ SFT(inTable)->SetupGEText((GEText*)inText); }
extern "C" void ee_screenscpp_sf_text_at_top_left(uintptr_t inText, uintptr_t inControl)
{ ((GEText*)inText)->SetLocation(SF(inControl)->GetRectangle().GetTopLeft()); }
extern "C" uintptr_t ee_screenscpp_bars_rhombus_new() { return (uintptr_t)new GERhombus; }
extern "C" void ee_screenscpp_bars_rhombus_free(uintptr_t inRhombus) { delete (GERhombus*)inRhombus; }
extern "C" void ee_screenscpp_bars_rhombus_color(uintptr_t inRhombus, float inRed, float inGreen, float inBlue)
{ ((GERhombus*)inRhombus)->SetColor(GEColor(inRed, inGreen, inBlue)); }
extern "C" void ee_screenscpp_bars_copy_text_rect(uintptr_t inControl, float* inOut)
{ UIObjHost::PutRect(inOut, static_cast<EEUIShellList*>(reinterpret_cast<UIControl*>(inControl))->GetCopyTextRectangle()); }
extern "C" uint64_t ee_screenscpp_bars_line_height(uintptr_t inControl)
{ return (uint64_t)static_cast<EEUIShellList*>(reinterpret_cast<UIControl*>(inControl))->GetLineHeight(); }
extern "C" void ee_screenscpp_bars_rhombus_rect(uintptr_t inRhombus, float inLeft, float inTop, float inRight, float inBottom)
{ ((GERhombus*)inRhombus)->SetRectangle(GERectangle(inLeft, inTop, inRight, inBottom)); }
extern "C" int64_t ee_screenscpp_bars_draw(uintptr_t inControl, uintptr_t inRhombus)
{ return (int64_t)reinterpret_cast<UIControl*>(inControl)->GetRasterizer()->Draw((GERhombus*)inRhombus, reinterpret_cast<UIControl*>(inControl)->GetScreenIndex()); }
