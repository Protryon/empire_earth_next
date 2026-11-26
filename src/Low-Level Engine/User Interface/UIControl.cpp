// C:\EEX\Low-Level Engine\User Interface\UIControl.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10064ed6..0x10065f69: the U2D key init chains, the sDarkBorder/sLightBorder init chains, the ctor first, then the
// out-of-line members in alphabetical order, then the map<unsigned long, UIEvent*> COMDATs (~_Tree, clear, _Dec).
// UIControl's header-inline members are emitted first by UIBackground.cpp. UIControlsTable.cpp's inline block
// starts at 0x10065f69.
#include <windows.h>


#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/UResourceFile.h"
#include "lle/Input System.h"
#include "lle/UIControl.h"
#include "lle/UIEntityGroup.h"


// the record's rectangles are read as GERectangle (bottom, left, right, top)
static_assert(sizeof(EeUiRect) == sizeof(GERectangle), "EeUiRect is GERectangle's layout");

// The record's calls out (ee_ui controlobj.rs): the form's and manager's, the virtuals, the rhombi; the event's is
// Rust's (lle_control.rs, the host ee_ui_control_host)
#define ctl static_cast<UIControl*>(inObj)
#define theForm ctl->GetForm()
EE_UI_CASE(ctl_form_control) { return (intptr_t)theForm->GetControl((unsigned long)a); }
EE_UI_CASE(ctl_slot_left_down) { return theForm->GetControl((unsigned long)a)->mLeftButtonDown; }
EE_UI_CASE(ctl_slot_right_down) { return theForm->GetControl((unsigned long)a)->mRightButtonDown; }
EE_UI_CASE(ctl_group_enabled) { return theForm->GetEntityGroupById((unsigned long)a)->GetIsEnabled(); }
EE_UI_CASE(ctl_current_form) { return (intptr_t)theForm->GetManager()->GetCurrentForm(); }
EE_UI_CASE(ctl_keyboard_captured) { return (intptr_t)theForm->GetKeyboardCaptured(); }
EE_UI_CASE(ctl_mouse_captured) { return (intptr_t)theForm->GetMouseCaptured(); }
EE_UI_VOID(ctl_set_keyboard_captured) { theForm->SetKeyboardCaptured(reinterpret_cast<UIControl*>(a)); }
EE_UI_VOID(ctl_set_mouse_captured) { theForm->SetMouseCaptured(reinterpret_cast<UIControl*>(a)); }
EE_UI_VOID(ctl_hide) { ctl->HideControl(); }
EE_UI_VOID(ctl_add_dirty) { theForm->GetManager()->AddDirtyRectangle(UIObjHost::Rect(f), theForm); }
EE_UI_VOID(ctl_tab) { theForm->Tab(); }
EE_UI_VOID(ctl_add_event) { theForm->GetManager()->AddEventToTrigger(reinterpret_cast<UIEvent*>(a), theForm, ctl); }
EE_UI_VOID(ctl_help_string) { theForm->GetManager()->GetStringTable()->GetString((unsigned long)b, *reinterpret_cast<UWideString*>(a)); }
EE_UI_CASE(ctl_tab_stop) { return theForm->GetTabStop(); }
EE_UI_CASE(ctl_tabbing_on) { return theForm->GetTabbingOn(); }
EE_UI_CASE(ctl_form_surfaces) { return theForm->GetManager()->GetNumberOfSurfaces(); }
EE_UI_VOID(ctl_set_dirty) { ctl->SetDirty(a != 0); }
EE_UI_VOID(ctl_form_ctl_set_dirty) { reinterpret_cast<UIControl*>(a)->SetDirty(b != 0); }
EE_UI_VOID(ctl_virt_set_location) { ctl->SetLocation(GE2DPoint(f[0], f[1])); }
EE_UI_VOID(ctl_set_display_mode) { ctl->SetDisplayMode((unsigned long)a); }
EE_UI_VOID(ctl_release_mouse) { ctl->ReleaseCapturedMouse(); }
EE_UI_VOID(ctl_set_bg_opacity) { ctl->SetBgOpacity(f[0]); }
EE_UI_VOID(ctl_part_set_bg_color) { ctl->SetBgColor(GEColor(f[0], f[1], f[2])); }
EE_UI_VOID(ctl_bg_opacity) { f[0] = ctl->GetBgOpacity(); }
EE_UI_VOID(ctl_tabbed_to) { ctl->TabbedTo(); }
EE_UI_VOID(ctl_draw_copied_bg) { ctl->GetRasterizer()->Draw(ctl->CopiedBgRhombus(), ctl->GetScreenIndex()); }
EE_UI_CASE(ctl_virt_draw_border) { return ctl->DrawBorder(UIObjHost::Rect(f), (unsigned long)a); }
EE_UI_CASE(ctl_virt_mouse_down) { return ctl->MouseLButtonDown(); }
EE_UI_VOID(ctl_bg_rhombus_opacity) { ctl->BgRhombus()->GetOpacityPerVertex(f[0], f[0], f[0], f[0]); }
#undef theForm
#undef ctl

// The first control's key lookup (lle_control.rs): the statics in its order, the one-statement shims
#define CTL(x) reinterpret_cast<UIControl*>(x)
extern "C" bool ee_uicpp_control_keys_ready() { return UIControl::Key(EeUiKey::Space) != NULL; }
extern "C" void ee_uicpp_control_key_fetch(uint32_t inSlot, uint32_t inCode)
{ gISKeyboard->GetEntity(inCode, UIControl::Key((EeUiKey)inSlot)); }
extern "C" uintptr_t ee_uicpp_mouse_l_button() { return (uintptr_t)UIManager::GetLMouseButton(); }
extern "C" uintptr_t ee_uicpp_mouse_r_button() { return (uintptr_t)UIManager::GetRMouseButton(); }
extern "C" bool ee_uicpp_control_left_down(uintptr_t inControl) { return CTL(inControl)->mLeftButtonDown; }
extern "C" bool ee_uicpp_control_right_down(uintptr_t inControl) { return CTL(inControl)->mRightButtonDown; }
extern "C" void ee_uicpp_control_set_left_down(uintptr_t inControl, bool inDown)
{ CTL(inControl)->mLeftButtonDown = inDown; }
extern "C" void ee_uicpp_control_set_right_down(uintptr_t inControl, bool inDown)
{ CTL(inControl)->mRightButtonDown = inDown; }
extern "C" long ee_uicpp_control_mouse_left_double(uintptr_t inControl)
{ return CTL(inControl)->MouseLButtonDoublePressed(); }
extern "C" long ee_uicpp_control_mouse_left_up(uintptr_t inControl) { return CTL(inControl)->MouseLButtonUp(); }
extern "C" long ee_uicpp_control_mouse_left_down(uintptr_t inControl) { return CTL(inControl)->MouseLButtonDown(); }
extern "C" long ee_uicpp_control_mouse_right_up(uintptr_t inControl) { return CTL(inControl)->MouseRButtonUp(); }
extern "C" long ee_uicpp_control_mouse_right_down(uintptr_t inControl) { return CTL(inControl)->MouseRButtonDown(); }
extern "C" long ee_uicpp_control_mouse_move(uintptr_t inControl, uintptr_t inLocation)
{ return CTL(inControl)->MouseMove(*reinterpret_cast<const GE2DPoint*>(inLocation)); }



// the background rhombi's new and delete (ee_ui lle_control.rs: the constructor and destructor)
extern "C" void* ee_uicpp_rhombus_new() { return new GERhombus; }
extern "C" void ee_uicpp_rhombus_delete(void* inRhombus) { delete static_cast<GERhombus*>(inRhombus); }





// The ops every whole object's host shares (ee_ui obj.rs): UIControl's members on the object
#define ctl static_cast<UIControl*>(inObj)
EE_UI_CASE(obj_base_copy_data) { return ctl->UIControl::CopyData(); }
EE_UI_CASE(obj_base_draw_polygons) { return ctl->UIControl::DrawPolygons(); }
EE_UI_CASE(obj_base_draw_text) { return ctl->UIControl::DrawTextA(); }
EE_UI_VOID(obj_base_set_location) { ctl->UIControl::SetLocation(GE2DPoint(f[0], f[1])); }
EE_UI_VOID(obj_set_dirty) { ctl->SetDirty(a != 0); }
EE_UI_VOID(obj_rect) { UIObjHost::PutRect(f, ctl->GetRectangle()); }
EE_UI_VOID(obj_mouse_location) { *reinterpret_cast<GE2DPoint*>(f) = ctl->GetMouseLocation(); }
EE_UI_VOID(obj_capture_mouse) { ctl->CaptureMouse(); }
EE_UI_VOID(obj_release_mouse) { ctl->ReleaseCapturedMouse(); }
EE_UI_VOID(obj_fire_trigger) { ctl->FireTriggerEvent((unsigned long)a); }
EE_UI_CASE(obj_border_thickness) { return ctl->mBorderThickness; }
EE_UI_CASE(obj_width) { return ctl->GetWidth(); }
EE_UI_CASE(obj_index) { return ctl->mIndex; }
EE_UI_CASE(obj_border_type) { return ctl->mBorderType; }
EE_UI_VOID(obj_set_border_type) { ctl->mBorderType = (unsigned long)a; }
EE_UI_CASE(obj_tab_stop) { return ctl->GetForm()->GetTabStop(); }
EE_UI_CASE(obj_tabbing_on) { return ctl->GetForm()->GetTabbingOn(); }
EE_UI_VOID(obj_set_tab_stop) { ctl->GetForm()->SetTabStop((unsigned long)a); }
EE_UI_CASE(obj_tabable) { return ctl->GetTabable(); }
EE_UI_CASE(obj_has_mouse_capture) { return ctl->GetHasMouseCapture(); }
EE_UI_CASE(obj_right_button_down) { return ctl->GetRightMouseButtonDown(); }
EE_UI_VOID(obj_capture_keyboard) { ctl->CaptureKeyboard(); }
EE_UI_VOID(obj_release_keyboard) { ctl->ReleaseCapturedKeyboard(); }
EE_UI_CASE(obj_manager_bitmap) { return (intptr_t)ctl->GetForm()->GetManager()->GetBitmap(ee_ui_obj_manager_bitmap(a)); }
EE_UI_VOID(obj_copied_draw_rect) { UIObjHost::PutRect(f, ctl->GetCopiedDrawRectangle()); }
EE_UI_VOID(obj_base_set_rectangle) { ctl->UIControl::SetRectangle(UIObjHost::Rect(f)); }
EE_UI_CASE(obj_control_count) { return ctl->GetForm()->GetControlCount(); }
EE_UI_CASE(obj_always_dirty) { return ctl->GetAlwaysDirty(); }
EE_UI_VOID(obj_set_always_dirty) { ctl->SetAlwaysDirty(a != 0); }
EE_UI_VOID(obj_set_display_mode) { ctl->SetDisplayMode((unsigned long)a); }
EE_UI_VOID(obj_bg_opacity) { f[0] = ctl->GetBgOpacity(); }
EE_UI_VOID(obj_border_opacity) { f[0] = ctl->GetBorderOpacity(); }
EE_UI_VOID(obj_set_bg_opacity) { ctl->SetBgOpacity(f[0]); }
EE_UI_VOID(obj_set_border_opacity) { ctl->SetBorderOpacity(f[0]); }
#undef ctl
// The ops on the bitmaps and texts a whole object holds by address (ee_ui obj.rs): a the object, b an argument
#define ctl static_cast<UIControl*>(inObj)
#define theBitmap reinterpret_cast<GEBitmap*>(a)
#define theText reinterpret_cast<GEText*>(a)
EE_UI_VOID(obj_bitmap_reference) { theBitmap->Reference(); }
EE_UI_VOID(obj_bitmap_release) { theBitmap->Dereference(); }
EE_UI_CASE(obj_bitmap_texture_width) { return theBitmap->GetTexture(ctl->GetScreenIndex())->GetWidth(); }
EE_UI_CASE(obj_string_length) { return reinterpret_cast<const UWideString*>(a)->GetLength(); }
EE_UI_CASE(obj_text_create) { return (intptr_t) new GEText(reinterpret_cast<GEFont*>(a)); }
EE_UI_VOID(obj_text_delete) { delete theText; }
EE_UI_VOID(obj_text_set_font) { theText->SetFont(reinterpret_cast<GEFont*>(b)); }
EE_UI_CASE(obj_text_length) { return theText->GetText().GetLength(); }
EE_UI_VOID(obj_text_extent) { *reinterpret_cast<GERectangle*>(f) = theText->GetTextExtent(); }
EE_UI_VOID(obj_text_copy) { *theText = *reinterpret_cast<GEText*>(b); }
EE_UI_VOID(obj_text_set_text) { theText->SetText(*reinterpret_cast<const UWideString*>(b)); }
EE_UI_VOID(obj_text_set_location) { theText->SetLocation(GE2DPoint(f[0], f[1])); }
EE_UI_VOID(obj_text_set_fg_color) { theText->SetFgColor(GEColor(f[0], f[1], f[2])); }
EE_UI_CASE(obj_draw_clipped_text) { return ctl->GetForm()->DrawClippedText(UIObjHost::Rect(f), theText, ctl); }
#undef theText
#undef theBitmap
#undef ctl
// The ops on a part a whole object owns (ee_ui obj.rs): another control's virtuals, by address
#define part reinterpret_cast<UIControl*>(a)
EE_UI_VOID(obj_delete_part) { delete part; }
EE_UI_CASE(obj_part_copy_data) { return part->CopyData(); }
EE_UI_CASE(obj_part_draw_polygons) { return part->DrawPolygons(); }
EE_UI_CASE(obj_part_draw_text) { return part->DrawTextA(); }
EE_UI_VOID(obj_part_rect) { UIObjHost::PutRect(f, part->GetRectangle()); }
EE_UI_CASE(obj_part_mouse_down) { return part->MouseLButtonDown(); }
EE_UI_CASE(obj_part_mouse_up) { return part->MouseLButtonUp(); }
EE_UI_CASE(obj_part_mouse_move) { return part->MouseMove(GE2DPoint(f[0], f[1])); }
EE_UI_VOID(obj_part_set_location) { part->SetLocation(GE2DPoint(f[0], f[1])); }
EE_UI_VOID(obj_part_set_rectangle) { part->SetRectangle(UIObjHost::Rect(f)); }
EE_UI_VOID(obj_part_set_bg_color) { part->SetBgColor(GEColor(f[0], f[1], f[2])); }
EE_UI_VOID(obj_part_set_bg_opacity) { part->SetBgOpacity(f[0]); }
#undef part

// the key predicates' calls (lle_control.rs key_pressed)
extern "C" long ee_uicpp_entity_long_value(uintptr_t inEntity) { return reinterpret_cast<ISEntity*>(inEntity)->GetValue(); }
extern "C" bool ee_uicpp_num_lock_off() { return UIForm::GetNumLockOff(); }
