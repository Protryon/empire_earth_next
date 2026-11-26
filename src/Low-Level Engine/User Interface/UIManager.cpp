// C:\EEX\Low-Level Engine\User Interface\UIManager.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10078c81..0x1007ba37: the U2D key init chains, the implicit copy members and ??_E of UIStretchedTiledBackground
// and UITBTiledBackground (UIManager.cpp was their first includer; both are UITiledBackground's tilings now), the
// ctor, the dtor and the out-of-line members in
// the original's order, then the vector/deque COMDATs. UIMovie.cpp starts at 0x1007ba37.
#define LLE_UIMANAGER_CPP   // UIControl::Key(EeUiKey::MouseWheel)/sMouseLeftHanded are visible here (see UIManager.h)
#include <windows.h>
#include <mbstring.h>
#include <string.h>
#include <utility>

#include "lle/LowLevelEngine.h"
#include "EEDataText.h"   // EEDbFindFile: the db tables' text forms
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/UIControl.h"
#include "lle/UIBackgroundTable.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIControlsTable.h"

// The Rust manager's calls out (EeUiOp, obj.rs): a is the form, event, bitmap, texture, lock, sound instance or
// other object the op names; rectangles and points travel in f. The cases of more than one call are Rust's (ee_ui
// lle_manager.rs, the host ee_ui_manager_host), over the one-statement shims below.
static GERectangle UIManagerRect(const float* inF) { return GERectangle(inF[1], inF[3], inF[2], inF[0]); }
#define MGR(x) reinterpret_cast<UIManager*>(x)
#define TEX(x) reinterpret_cast<GETexture*>(x)
#define BMP(x) reinterpret_cast<GEBitmap*>(x)
#define UVS(x) reinterpret_cast<GETextureCoordinate*>(x)
#define FILE_SPEC(x) reinterpret_cast<FSFileSpec*>(x)
static UWideString sUIManagerKey;   // UpdateTranslatedKey's character (the UI thread's)
extern "C" uint32_t ee_uicpp_mgr_screen(uintptr_t inM) { return MGR(inM)->GetScreen(); }
extern "C" void ee_uicpp_mgr_rasterizer_screen(uintptr_t inM)
{ MGR(inM)->GetRasterizer()->GetScreen(MGR(inM)->GetScreen()); }
extern "C" uintptr_t ee_uicpp_mgr_window(uintptr_t inM)
{ return (uintptr_t)MGR(inM)->GetRasterizer()->GetWindow(MGR(inM)->GetScreen()); }
extern "C" float ee_uicpp_window_origin_x(uintptr_t inW)
{ return reinterpret_cast<GEWindow*>(inW)->GetClientOrigin().GetX(); }
extern "C" float ee_uicpp_window_origin_y(uintptr_t inW)
{ return reinterpret_cast<GEWindow*>(inW)->GetClientOrigin().GetY(); }
extern "C" uint32_t ee_uicpp_window_width(uintptr_t inW)
{ return (uint32_t)reinterpret_cast<GEWindow*>(inW)->GetWidth(); }
extern "C" uint32_t ee_uicpp_window_height(uintptr_t inW)
{ return (uint32_t)reinterpret_cast<GEWindow*>(inW)->GetHeight(); }
extern "C" void ee_uicpp_mgr_blending(uintptr_t inM, unsigned long* inSource, unsigned long* inDestination)
{ MGR(inM)->GetRasterizer()->GetBlending(*inSource, *inDestination, MGR(inM)->GetScreen()); }
extern "C" void ee_uicpp_key_copy(uintptr_t inKey)
{ sUIManagerKey.CopyString(reinterpret_cast<const char*>(inKey), 1); }
extern "C" long ee_uicpp_form_translated_key(uintptr_t inForm, int32_t inRepeat)
{ return reinterpret_cast<UIForm*>(inForm)->UpdateTranslatedKey(sUIManagerKey, (short)inRepeat); }
extern "C" void ee_uicpp_event_reference(uintptr_t inEvent) { reinterpret_cast<UIEvent*>(inEvent)->Reference(); }
extern "C" void ee_uicpp_event_dereference(uintptr_t inEvent) { reinterpret_cast<UIEvent*>(inEvent)->Dereference(); }
extern "C" uintptr_t ee_uicpp_callback_manager(uintptr_t inCallback, uintptr_t inM, uint32_t inId)
{ return (uintptr_t)reinterpret_cast<UIEvent* (*)(UIManager*, unsigned long)>(inCallback)(MGR(inM), inId); }
extern "C" uintptr_t ee_uicpp_callback_form(uintptr_t inCallback, uintptr_t inForm, uint32_t inId)
{ return (uintptr_t)reinterpret_cast<UIEvent* (*)(UIForm*, unsigned long)>(inCallback)(reinterpret_cast<UIForm*>(inForm), inId); }
extern "C" uintptr_t ee_uicpp_mgr_texture_manager(uintptr_t inM)
{ return (uintptr_t)MGR(inM)->GetRasterizer()->GetScreen(MGR(inM)->GetScreen())->GetTextureManager(); }
extern "C" uint32_t ee_uicpp_mgr_ui_depth(uintptr_t inM) { return (uint32_t)MGR(inM)->GetUITextureBitDepth(); }
extern "C" uintptr_t ee_uicpp_texture_load_ui(uintptr_t inManager, EeUiShimText inName, uint32_t inDepth, bool inAlpha)
{ return (uintptr_t)reinterpret_cast<GETextureManager*>(inManager)->LoadTexture(UIShimString(inName), GETextureDescription{1, 0, inDepth, 0, false, false, inAlpha, true}); }
extern "C" void ee_uicpp_texture_reference(uintptr_t inT) { TEX(inT)->Reference(); }
extern "C" void ee_uicpp_texture_stage_alpha(uintptr_t inT, uint32_t inMode)
{ TEX(inT)->SetStageAlphaMode(inMode, -1); }
extern "C" void ee_uicpp_texture_stage_color(uintptr_t inT, uint32_t inMode)
{ TEX(inT)->SetStageColorMode(inMode, -1); }
extern "C" void ee_uicpp_texture_stage_min(uintptr_t inT, uint32_t inMode)
{ TEX(inT)->SetStageMinFilterMode(inMode, -1); }
extern "C" void ee_uicpp_texture_stage_mag(uintptr_t inT, uint32_t inMode)
{ TEX(inT)->SetStageMagFilterMode(inMode, -1); }
extern "C" void ee_uicpp_texture_stage_mip(uintptr_t inT, uint32_t inMode)
{ TEX(inT)->SetStageMipFilterMode(inMode, -1); }
extern "C" void ee_uicpp_texture_stage_bias(uintptr_t inT, float inBias) { TEX(inT)->SetStageMipMapBias(inBias, -1); }
extern "C" void ee_uicpp_texture_stage_address(uintptr_t inT, uint32_t inMode)
{ TEX(inT)->SetStageTextureAddressingMode(inMode, -1); }
extern "C" uint32_t ee_uicpp_texture_width(uintptr_t inT) { return (uint32_t)TEX(inT)->GetWidth(); }
extern "C" uint32_t ee_uicpp_texture_height(uintptr_t inT) { return (uint32_t)TEX(inT)->GetHeight(); }
extern "C" uintptr_t ee_uicpp_bitmap_new() { return (uintptr_t) new GEBitmap; }
extern "C" void ee_uicpp_bitmap_set_texture(uintptr_t inB, uintptr_t inT, uint32_t inScreen)
{ BMP(inB)->SetTexture(TEX(inT), inScreen); }
extern "C" uintptr_t ee_uicpp_bitmap_texture(uintptr_t inB, uint32_t inScreen)
{ return (uintptr_t)BMP(inB)->GetTexture(inScreen); }
extern "C" void ee_uicpp_bitmap_set_opacity(uintptr_t inB, float inOpacity) { BMP(inB)->SetOpacity(inOpacity); }
extern "C" void ee_uicpp_bitmap_uvs(uintptr_t inB, float* inUvs)
{ BMP(inB)->GetTextureCoordinatesPerVertex(UVS(inUvs)[0], UVS(inUvs)[1], UVS(inUvs)[2], UVS(inUvs)[3]); }
extern "C" void ee_uicpp_bitmap_set_uvs(uintptr_t inB, float inU0, float inV0, float inU1, float inV1, float inU2, float inV2, float inU3, float inV3)
{ BMP(inB)->SetTextureCoordinates(GETextureCoordinate(inU0, inV0), GETextureCoordinate(inU1, inV1), GETextureCoordinate(inU2, inV2), GETextureCoordinate(inU3, inV3)); }
extern "C" bool ee_uicpp_lock_create(uintptr_t* inLock)
{ return TSScheduler::CreateCriticalSection(*reinterpret_cast<TSCriticalSection**>(inLock)) == keseOK; }
extern "C" void ee_uicpp_es_verify(uint32_t inLine, EeUiShimText inExpression)
{ ESVerify(__FILE__, inLine, UIShimStd(inExpression).c_str()); }
extern "C" void ee_uicpp_instance_reference(uintptr_t inInstance)
{ reinterpret_cast<SESMInstanceNode*>(inInstance)->Reference(); }
extern "C" void ee_uicpp_instance_dereference(uintptr_t inInstance)
{ reinterpret_cast<SESMInstanceNode*>(inInstance)->Dereference(); }
extern "C" uintptr_t ee_uicpp_table_get(uint32_t inIndex)
{ return (uintptr_t)UIControlsTable::GetUIControlsTable(inIndex); }
extern "C" uintptr_t ee_uicpp_mgr_form_by_id(uintptr_t inM, uint32_t inId)
{ return (uintptr_t)MGR(inM)->GetFormByID(inId); }
extern "C" void ee_uicpp_table_add_event(uintptr_t inTable, uint32_t inId, uint32_t inKind)
{ reinterpret_cast<UIControlsTable*>(inTable)->AddEvent(inId, inKind); }
extern "C" void ee_uicpp_form_add_event(uintptr_t inForm, uint32_t inId, uint32_t inKind)
{ reinterpret_cast<UIForm*>(inForm)->AddEvent(inId, inKind); }
extern "C" long ee_uicpp_db_find(uintptr_t inPath, uintptr_t inDirectory, EeUiShimText inName, uintptr_t* inFile)
{ return EEDbFindFile(reinterpret_cast<FSPath*>(inPath), *reinterpret_cast<const UString*>(inDirectory) + UIShimString(inName), *reinterpret_cast<FSFileSpec**>(inFile)); }
extern "C" long ee_uicpp_db_fonts(uintptr_t inFile, uintptr_t inM)
{ return UIFontTable::Create(*FILE_SPEC(inFile), MGR(inM)); }
extern "C" long ee_uicpp_db_forms(uintptr_t inFile, uintptr_t inM)
{ return UIForm::Create(*FILE_SPEC(inFile), MGR(inM)); }
extern "C" long ee_uicpp_db_backgrounds(uintptr_t inFile) { return UIBackgroundTable::Create(*FILE_SPEC(inFile)); }
extern "C" void ee_uicpp_db_create_backgrounds(uintptr_t inM) { ee_ui_bgtable_create_backgrounds(MGR(inM), ee_ui_bg_table_host); }
extern "C" void ee_uicpp_db_controls(uintptr_t inFile, uintptr_t inM)
{ UIControlsTable::Create(*FILE_SPEC(inFile), MGR(inM)); }
extern "C" void ee_uicpp_db_control_events(uintptr_t inFile) { UIEvent::CreateControlEvents(*FILE_SPEC(inFile)); }
extern "C" void ee_uicpp_db_form_events(uintptr_t inFile, uintptr_t inM)
{ UIEvent::CreateFormEvents(*FILE_SPEC(inFile), MGR(inM)); }
extern "C" void ee_uicpp_db_hot_keys(uintptr_t inFile) { UIHotKey::CreateHotKeys(*FILE_SPEC(inFile)); }
extern "C" void ee_uicpp_file_delete(uintptr_t inFile) { delete FILE_SPEC(inFile); }
extern "C" void ee_uicpp_db_remove_controls() { UIControlsTable::DeleteAll(); }
extern "C" void ee_uicpp_db_remove_backgrounds() { UIBackgroundTable::DeleteAll(); }
extern "C" void ee_uicpp_db_remove_fonts() { UIFontTable::DeleteAll(); }
extern "C" bool ee_uicpp_mouse_buttons_ready() { return UIControl::Key(EeUiKey::LeftMouse) != NULL; }
// the shared mouse button inButton into the key slot inWhich after the left button's (lle_manager.rs)
extern "C" void ee_uicpp_mouse_button_fetch(uint32_t inWhich, uint32_t inButton)
{ gISMouse->GetEntity(inButton, UIControl::Key((EeUiKey)((uint32_t)EeUiKey::LeftMouse + inWhich))); }
extern "C" void ee_uicpp_mouse_swap_buttons() { std::swap(UIControl::Key(EeUiKey::LeftMouse), UIControl::Key(EeUiKey::RightMouse)); }

// a call with a rectangle (GERectangle is the buffer's order: bottom, left, right, top) or two floats
#define FORM reinterpret_cast<UIForm*>(a)
#define INSTANCE reinterpret_cast<SESMInstanceNode*>(a)
#define SOUNDS static_cast<UIManager*>(inObj)->GetSoundManager()
EE_UI_VOID(mgr_form_rect) { UIObjHost::PutRect(f, FORM->GetRectangle()); }
EE_UI_CASE(mgr_form_id) { return FORM->GetFormID(); }
EE_UI_CASE(mgr_form_use_gdi) { return FORM->GetUseGDIForText(); }
EE_UI_CASE(mgr_form_keyboard) { return (intptr_t)FORM->GetKeyboardCaptured(); }
EE_UI_CASE(mgr_form_mouse) { return (intptr_t)FORM->GetMouseCaptured(); }
// the form calls (lle_hosts.rs mgr_form_call, mgr_form_rect_call, mgr_form_ime: b the call)
EE_UI_VOID(mgr_form_open) { FORM->Open(); }
EE_UI_VOID(mgr_form_close) { FORM->Close(); }
EE_UI_VOID(mgr_form_activate) { FORM->Activate(); }
EE_UI_VOID(mgr_form_deactivate) { FORM->Deactivate(); }
EE_UI_VOID(mgr_form_clear_clips) { FORM->ClearAllClipRectangles(); }
EE_UI_VOID(mgr_form_all_dirty) { FORM->SetAllControlsDirty(); }
EE_UI_CASE(mgr_form_copy) { return FORM->CopyData(); }
EE_UI_CASE(mgr_form_draw_polygons) { return FORM->DrawPolygons(); }
EE_UI_CASE(mgr_form_draw_text) { return FORM->DrawTextA(); }
EE_UI_CASE(mgr_form_update_keyboard) { return FORM->UpdateKeyboard(); }
EE_UI_CASE(mgr_form_update_mouse) { return FORM->UpdateMouse(); }
EE_UI_VOID(mgr_form_delete) { delete FORM; }
EE_UI_VOID(mgr_form_add_dirty) { FORM->AddDirtyRectangle(UIManagerRect(f)); }
EE_UI_VOID(mgr_form_redraw) { FORM->RedrawIntersectControls(UIManagerRect(f)); }
EE_UI_VOID(mgr_form_add_clip) { FORM->AddClipRectangle(UIManagerRect(f)); }
EE_UI_VOID(mgr_form_relayout) { FORM->RelayoutForm(UIManagerRect(f)); }
EE_UI_VOID(mgr_form_mouse_location) { FORM->UpdateMouseLocation(GE2DPoint(f[0], f[1])); }
#define COMPOSITION (*reinterpret_cast<const EeUiImeComposition*>(c))
EE_UI_VOID(mgr_form_ime_result) { FORM->AddIMEResultString(*reinterpret_cast<const UWideString*>(c)); }
EE_UI_CASE(mgr_form_ime_change) { return FORM->ChangeIMECompositionString(*static_cast<const UWideString*>(COMPOSITION.string), *static_cast<const UString*>(COMPOSITION.attributes), COMPOSITION.cursor); }
EE_UI_CASE(mgr_form_ime_close) { return FORM->CloseIMECompositionString(); }
EE_UI_CASE(mgr_form_ime_open) { return FORM->OpenIMECompositionString(); }
#undef COMPOSITION
EE_UI_CASE(mgr_ctl_ime) { return reinterpret_cast<UIControl*>(a)->GetIMEEnabled(); }
EE_UI_VOID(mgr_part_rect) { UIObjHost::PutRect(f, reinterpret_cast<UIControl*>(a)->GetRectangle()); }
EE_UI_VOID(mgr_event_process) { reinterpret_cast<UIEvent*>(a)->ProcessEvent(reinterpret_cast<UIForm*>(b), reinterpret_cast<UIEntity*>(c)); }
EE_UI_CASE(mgr_keyboard_should_update) { return gISKeyboard->GetShouldUpdate(); }
EE_UI_CASE(mgr_mouse_should_update) { return gISMouse->GetShouldUpdate(); }
// the hot keys' pressed poll (the original's UIHotKey::sPressedPoll: 50 ms), polled once per update
static TMPoll sHotKeyPressedPoll(50, 0, true);
EE_UI_VOID(mgr_hot_key_poll) { sHotKeyPressedPoll.Poll(); }
EE_UI_VOID(mgr_hot_keys_delete) { UIHotKey::DeleteHotKeys(); }
EE_UI_CASE(mgr_virt_update_keyboard) { return static_cast<UIManager*>(inObj)->UpdateKeyboard(); }
EE_UI_CASE(mgr_virt_update_mouse) { return static_cast<UIManager*>(inObj)->UpdateMouse(GE2DPoint(f[0], f[1])); }
EE_UI_VOID(mgr_set_blending) { static_cast<UIManager*>(inObj)->GetRasterizer()->SetBlending((unsigned long)a, (unsigned long)b, static_cast<UIManager*>(inObj)->GetScreen()); }
EE_UI_VOID(mgr_texture_release) { reinterpret_cast<GETexture*>(a)->Dereference(); }
EE_UI_VOID(mgr_bitmap_release) { reinterpret_cast<GEBitmap*>(a)->Dereference(); }
EE_UI_VOID(mgr_lock_delete) { TSScheduler::DestroyCriticalSection(reinterpret_cast<TSCriticalSection*>(a)); }
EE_UI_VOID(mgr_form_lock_enter) { reinterpret_cast<TSCriticalSection*>(a)->Enter(); }
EE_UI_VOID(mgr_form_lock_leave) { reinterpret_cast<TSCriticalSection*>(a)->Leave(); }
EE_UI_VOID(mgr_sound_stop) { SOUNDS->Stop(INSTANCE, false); }
EE_UI_VOID(mgr_sound_unload) { SOUNDS->Unload((long)a); }
EE_UI_CASE(mgr_sound_load) { return SOUNDS->Load(reinterpret_cast<const char*>(a), 0, 1); }
EE_UI_CASE(mgr_sound_play) { return (intptr_t)SOUNDS->PlayAndReturnReferencedNode((long)a, f[0], 0.0f, 1.0f, true, NULL); }
EE_UI_VOID(mgr_sound_parameters) { SOUNDS->SetSoundParameters(INSTANCE, f[0], 0.0f, 1.0f); }
#undef FORM
#undef INSTANCE
#undef SOUNDS



// UIEvent is whole in Rust (rust/crates/ee_ui/src/eventobj.rs, R6 ui round 21; the original unit UIEvent.cpp,
// 0x1006f278..0x1006f4ef): Initialize/Uninitialize over the record, the two loaders over the events file. The calls
// out: the event's virtuals on the record, the file's reads (UIHotKey.cpp's ee_uicpp_file_*) and the owners'
EE_UI_CASE(event_ev_initialize) { return static_cast<UIEvent*>(static_cast<EeUiEventFields*>(inObj))->InitializeEvent(); }
EE_UI_CASE(event_ev_uninitialize) { return static_cast<UIEvent*>(static_cast<EeUiEventFields*>(inObj))->UninitializeEvent(); }
EE_UI_VOID(event_bgt_file_open) { ee_uicpp_file_open(inObj); }
EE_UI_CASE(event_bgt_file_read) { return ee_uicpp_file_read(inObj, reinterpret_cast<unsigned char*>(b), (unsigned long)c); }
EE_UI_VOID(event_bgt_file_close) { ee_uicpp_file_close(inObj); }
// TEST ONLY: the harness's presentation bridge (tools/harness/presentation_bridge.c) finds these by name in the map
// (the manager's and forms' state is Rust's: ee_ui managerobj.rs, formobj.rs): the UI lock it try-enters, the form
// stack, and a form's id and controls. Kept by the linker for it.
extern "C" void* EETestUILock(UIManager* inManager) { return inManager->GetUILock(); }
extern "C" unsigned long EETestUIFormCount(UIManager* inManager) { return inManager->GetFormCount(); }
extern "C" void* EETestUIForm(UIManager* inManager, unsigned long inIndex) { return inManager->GetStackedForm(inIndex); }
extern "C" unsigned long EETestUIFormID(UIForm* inForm) { return inForm->GetFormID(); }
extern "C" unsigned long EETestUIFormControlCount(UIForm* inForm) { return inForm->GetControlCount(); }
extern "C" void* EETestUIFormControl(UIForm* inForm, unsigned long inIndex) { return inForm->GetControl(inIndex); }
#if defined(_MSC_VER)
#pragma comment(linker, "/include:EETestUILock")
#pragma comment(linker, "/include:EETestUIFormCount")
#pragma comment(linker, "/include:EETestUIForm")
#pragma comment(linker, "/include:EETestUIFormID")
#pragma comment(linker, "/include:EETestUIFormControlCount")
#pragma comment(linker, "/include:EETestUIFormControl")
#endif


