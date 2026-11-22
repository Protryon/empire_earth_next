// C:\EEX\Low-Level Engine\User Interface\UIForm.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1007042f..0x10075330: the U2D key init chains, UIIMECandidate's implicit copy members and ??_E (UIForm.cpp is the
// first includer of UIIMECandidate.h), the init chain of an unreferenced file-scope UString, then the ctors, the dtor
// and the members in the original's order (roughly alphabetical), then the template COMDATs (hot key vectors, sort,
// the dirty-rectangle deque). UIFullBackground.cpp starts at 0x10075330.
//
// The class is whole in Rust (rust/crates/ee_ui/src/formobj.rs, R6 ui round 16): the methods are forwarders to the
// Rust form (UIForm::Call), the ctors and dtor make and free it, and UIForm::HostOp answers its calls out (the
// controls', groups', backgrounds', hot keys' and events' members and virtuals, the manager, the input keys).
#define LLE_UIFORM_CPP      // UIControl::Key(EeUiKey::Tab) and UIManager::sMouseWheel are visible here (see UIForm.h) (provisional name)
#include <windows.h>
#include <mbstring.h>

#include "rust/ui/formobj.h"
#include "rust/ui/obj.h"
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/U2DRectangle.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/GEText.h"
#include "lle/FSFileSpec.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIControl.h"
#include "lle/UIButton.h"
#include "lle/UIRadioButton.h"
#include "lle/UITextLabel.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIHotKey.h"
#include "lle/UIFormManager.h"
#include "lle/UIForm.h"
#include "lle/UIControlsTable.h"
#include "lle/UIManager.h"
#include "lle/UIEMouseWheelScroll.h"
#include "lle/UIShims.h"
#include "lle/UIObjHost.h"
#include "rust/ui/lle_form.h"
#include "rust/ui/form_state.h"
#include "rust/ui/lle_hosts.h"

// unreferenced (its init chain 0x10070706 is the only user)
static const UString kUIFormKey("B7B2257DB7B2257DA49985B7B5A18DF4D49327D6B51165F42EB1A52ECC918DA616B16D26B41955F43C190DDE7EB1BD9624F92DDEFC610D9EB4B9559CB4210D84FC29"
                                "8596249907BE14CB25F6142B25B61C1165C624F9F59696B18D269C2357BEB51955B41C1165C624F9F59696B18D269EB16D26B431B5C4F4F935F6149907CEB7B34FF5");


static GERectangle UIFormRect(const float* inF) { return GERectangle(inF[1], inF[3], inF[2], inF[0]); }

// The form's calls out of more than one call are Rust's (ee_ui lle_form.rs, the host ee_ui_form_host), over the
// one-statement shims below (and UIManager.cpp's)
#define FORM(x) reinterpret_cast<UIForm*>(x)
#define MGR(x) reinterpret_cast<UIManager*>(x)
#define CTL(x) reinterpret_cast<UIControl*>(x)
#define HOTKEY(x) reinterpret_cast<UIHotKey*>(x)
#define EVENT(x) reinterpret_cast<UIEvent*>(x)
static UWideString sUIFormHelpString;   // FormHelpGet's string, which FormHelpSetText sets (the UI thread's)
extern "C" void ee_uicpp_form_help_clear() { sUIFormHelpString = ""; }
extern "C" bool ee_uicpp_control_help_string(uintptr_t inControl)
{ return CTL(inControl)->GetHelpString(sUIFormHelpString); }
extern "C" void ee_uicpp_hot_key_set_just_pressed(uintptr_t inKey, uintptr_t inEvent)
{ HOTKEY(inKey)->SetJustPressedEvent(EVENT(inEvent)); }
extern "C" void ee_uicpp_hot_key_set_pressed(uintptr_t inKey, uintptr_t inEvent)
{ HOTKEY(inKey)->SetPressedEvent(EVENT(inEvent)); }
extern "C" void ee_uicpp_hot_key_set_just_unpressed(uintptr_t inKey, uintptr_t inEvent)
{ HOTKEY(inKey)->SetJustUnpressedEvent(EVENT(inEvent)); }
extern "C" void ee_uicpp_hot_key_set_double_pressed(uintptr_t inKey, uintptr_t inEvent)
{ HOTKEY(inKey)->SetDoublePressedEvent(EVENT(inEvent)); }
extern "C" void ee_uicpp_form_fetch_tab_key() { gISKeyboard->GetEntity(kKeyTab, UIControl::Key(EeUiKey::Tab)); }
extern "C" void ee_uicpp_form_fetch_shift_key() { gISKeyboard->GetEntity(kKeyShift, UIControl::Key(EeUiKey::Shift)); }
extern "C" void ee_uicpp_event_uninitialize(uintptr_t inEvent) { EVENT(inEvent)->Uninitialize(); }
extern "C" void ee_uicpp_event_initialize(uintptr_t inEvent) { EVENT(inEvent)->Initialize(); }
extern "C" uint32_t ee_uicpp_entity_value(uintptr_t inEntity)
{ return (uint32_t)reinterpret_cast<ISEntity*>(inEntity)->GetValue(); }
extern "C" bool ee_uicpp_entity_just_pressed(uintptr_t inEntity)
{ return reinterpret_cast<ISEntity*>(inEntity)->JustPressed(); }
extern "C" bool ee_uicpp_entity_just_unpressed(uintptr_t inEntity)
{ return reinterpret_cast<ISEntity*>(inEntity)->JustUnpressed(); }
extern "C" bool ee_uicpp_entity_just_double_pressed(uintptr_t inEntity)
{ return reinterpret_cast<ISEntity*>(inEntity)->JustDoublePressed(); }
extern "C" uint32_t ee_uicpp_form_screen(uintptr_t inForm) { return FORM(inForm)->GetManager()->GetScreen(); }
extern "C" uintptr_t ee_uicpp_form_keyboard_captured(uintptr_t inForm)
{ return (uintptr_t)FORM(inForm)->GetKeyboardCaptured(); }
extern "C" void ee_uicpp_control_ime_result(uintptr_t inControl, uintptr_t inText)
{ CTL(inControl)->AddIMEResultString(*reinterpret_cast<const UWideString*>(inText)); }
extern "C" long ee_uicpp_control_ime_change(uintptr_t inControl, uintptr_t inText, uintptr_t inAttributes, uint32_t inCursor)
{ return CTL(inControl)->ChangeIMECompositionString(*reinterpret_cast<const UWideString*>(inText), *reinterpret_cast<const UString*>(inAttributes), inCursor); }
extern "C" long ee_uicpp_control_ime_close(uintptr_t inControl) { return CTL(inControl)->CloseIMECompositionString(); }
extern "C" long ee_uicpp_control_ime_open(uintptr_t inControl) { return CTL(inControl)->OpenIMECompositionString(); }
extern "C" long ee_uicpp_control_translated_key(uintptr_t inControl, uintptr_t inKey, int32_t inRepeat)
{ return CTL(inControl)->UpdateTranslatedKey(reinterpret_cast<const char*>(inKey), (short)inRepeat); }
extern "C" uintptr_t ee_uicpp_mgr_create_form(uintptr_t inM, EeUiShimText inData)
{ return (uintptr_t)MGR(inM)->CreateForm(*reinterpret_cast<const UIDbDataForms*>(inData.ptr)); }
extern "C" void ee_uicpp_mgr_add_form(uintptr_t inM, uintptr_t inForm) { MGR(inM)->AddForm(FORM(inForm)); }
extern "C" void ee_uicpp_form_enable_group0(uintptr_t inForm) { FORM(inForm)->GetEntityGroup(0)->Enable(); }

// UIEntityGroup is whole in Rust (rust/crates/ee_ui/src/groupobj.rs, R6 ui round 21; the original unit
// UIEntityGroup.cpp, 0x1006e304..0x1006e5a8): the calls out of Disable/Enable on entity a, a button or a hot key
#define ENTITY_CONTROL static_cast<UIControl*>(reinterpret_cast<UIEntity*>(a))
EE_UI_CASE(group_is_button) { return reinterpret_cast<UIEntity*>(a)->GetType() == kControlTypeButton; }
EE_UI_CASE(group_shown) { return ENTITY_CONTROL->GetDisplayMode() == kDisplayModeShown; }
EE_UI_CASE(group_on_current_form) { return ENTITY_CONTROL->GetForm() == ENTITY_CONTROL->GetForm()->GetManager()->GetCurrentForm(); }
EE_UI_VOID(group_add_dirty) { ENTITY_CONTROL->GetForm()->GetManager()->AddDirtyRectangle(ENTITY_CONTROL->GetRectangle(), ENTITY_CONTROL->GetForm()); }
EE_UI_VOID(group_hide) { ENTITY_CONTROL->HideControl(); }
EE_UI_VOID(group_set_dirty) { ENTITY_CONTROL->SetDirty(true); }
EE_UI_CASE(group_tabbing) { return ENTITY_CONTROL->GetForm()->GetTabbingOn(); }
EE_UI_CASE(group_is_tab_stop) { return ENTITY_CONTROL->GetForm()->GetTabStop() == ENTITY_CONTROL->GetIndex(); }
EE_UI_VOID(group_tab) { ENTITY_CONTROL->GetForm()->Tab(); }
EE_UI_VOID(group_set_enabled) { static_cast<UIHotKey*>(reinterpret_cast<UIEntity*>(a))->SetEnabled(b != 0); }
#undef ENTITY_CONTROL

// a call with a rectangle (GERectangle is the buffer's order: bottom, left, right, top)
intptr_t UIForm::CallRect(EeUiFormCall inCall, intptr_t inA, intptr_t inB, const GERectangle& inRectangle) const
{
    float f[4] = {inRectangle.mBottom, inRectangle.mLeft, inRectangle.mRight, inRectangle.mTop};
    return this->Call(inCall, inA, inB, f);
}

// an input entity by key or button
template <class D> static ISEntity* UIFormEntity(D* inDevice, unsigned long inKey)
{
    ISEntity* theEntity = NULL;
    inDevice->GetEntity(inKey, theEntity);
    return theEntity;
}

// The Rust form's calls out (EeUiOp, obj.rs): a is the control, group, background, hot key, event, table or other
// object the op names; rectangles and points travel in f
#define theForm static_cast<UIForm*>(inObj)
#define theControl reinterpret_cast<UIControl*>(a)
#define theGroup reinterpret_cast<UIEntityGroup*>(a)
#define theBackground reinterpret_cast<UIBackground*>(a)
#define theKey reinterpret_cast<UIHotKey*>(a)
#define theEvent reinterpret_cast<UIEvent*>(a)
#define theTable reinterpret_cast<UIControlsTable*>(a)
#define thePoll reinterpret_cast<TMPoll*>(a)
#define theRhombus reinterpret_cast<GERhombus*>(a)
EE_UI_CASE(form_ctl_type) { return theControl->GetType(); }
EE_UI_CASE(form_ctl_entity_type) { return theControl->UIEntity::GetType(); }
EE_UI_CASE(form_ctl_display_mode) { return theControl->GetDisplayMode(); }
EE_UI_CASE(form_ctl_dirty) { return theControl->GetIsDirty(); }
EE_UI_CASE(form_ctl_draw_count) { return theControl->GetDrawCount(); }
EE_UI_CASE(form_ctl_index) { return theControl->GetIndex(); }
EE_UI_CASE(form_ctl_id) { return theControl->GetId(); }
EE_UI_CASE(form_ctl_tabable) { return theControl->GetTabable(); }
EE_UI_CASE(form_ctl_checked) { return static_cast<UIRadioButton*>(theControl)->GetIsChecked(); }
EE_UI_CASE(form_ctl_ime_enabled) { return theControl->GetIMEEnabled(); }
EE_UI_VOID(form_part_rect) { UIObjHost::PutRect(f, theControl->GetRectangle()); }
EE_UI_VOID(form_part_copied_draw_rect) { UIObjHost::PutRect(f, theControl->GetCopiedDrawRectangle()); }
EE_UI_CASE(form_part_copy_data) { return theControl->CopyData(); }
EE_UI_CASE(form_part_draw_polygons) { return theControl->DrawPolygons(); }
EE_UI_CASE(form_part_draw_text) { return theControl->DrawTextA(); }
EE_UI_VOID(form_part_set_location) { theControl->SetLocation(GE2DPoint(f[0], f[1])); }
EE_UI_VOID(form_delete_part) { delete theControl; }
EE_UI_VOID(form_ctl_set_dirty) { theControl->SetDirty(b != 0); }
EE_UI_VOID(form_ctl_set_display_mode) { theControl->SetDisplayMode((unsigned long)b); }
EE_UI_VOID(form_ctl_hide) { theControl->HideControl(); }
EE_UI_VOID(form_ctl_set_string_id) { theControl->SetStringTableID((unsigned long)b); }
EE_UI_VOID(form_ctl_tabbed_from) { theControl->TabbedFrom(); }
EE_UI_VOID(form_ctl_tabbed_to) { theControl->TabbedTo(); }
EE_UI_CASE(form_ctl_update_keyboard) { return theControl->UpdateKeyboard(); }
EE_UI_CASE(form_ctl_update_mouse) { return theControl->UpdateMouse(theForm->GetMouseLocation()); }
EE_UI_CASE(form_ctl_wheel_turned) { return theControl->MouseWheelTurned((long)b); }
EE_UI_VOID(form_ctl_fire_trigger) { theControl->FireTriggerEvent((unsigned long)b); }
EE_UI_VOID(form_ctl_release_mouse) { theControl->ReleaseCapturedMouse(); }
EE_UI_VOID(form_ctl_release_keyboard) { theControl->ReleaseCapturedKeyboard(); }
EE_UI_VOID(form_help_set_text) { static_cast<UITextLabel*>(theControl)->SetText(sUIFormHelpString, true); }
EE_UI_CASE(form_label_fit) { return static_cast<UITextLabel*>(theControl)->GetFitRectToText(); }
EE_UI_CASE(form_group_enabled) { return theGroup->GetIsEnabled(); }
EE_UI_CASE(form_group_count) { return theGroup->GetEntityCount(); }
EE_UI_CASE(form_group_entity) { return (intptr_t) static_cast<UIControl*>(theGroup->GetEntity((unsigned long)b)); }
EE_UI_CASE(form_group_new) { return (intptr_t) new UIEntityGroup; }
EE_UI_VOID(form_group_delete) { delete theGroup; }
EE_UI_VOID(form_group_add) { theGroup->AddEntity(reinterpret_cast<UIControl*>(b)); }
EE_UI_VOID(form_group_remove) { theGroup->RemoveEntity(reinterpret_cast<UIControl*>(b)); }
EE_UI_CASE(form_bg_priority) { return theBackground->GetDrawPriority(); }
EE_UI_CASE(form_bg_id) { return theBackground->GetBackgroundID(); }
EE_UI_CASE(form_bg_visible) { return theBackground->GetIsVisible(); }
EE_UI_VOID(form_bg_rect) { UIObjHost::PutRect(f, theBackground->GetRectangle()); }
EE_UI_VOID(form_bg_open) { theBackground->Open(); }
EE_UI_VOID(form_bg_close) { theBackground->Close(); }
EE_UI_VOID(form_bg_delete) { delete theBackground; }
EE_UI_CASE(form_bg_draw) { return theBackground->DrawBackground(UIFormRect(f)); }
EE_UI_VOID(form_bg_load) { theBackground->LoadBackground(); }
EE_UI_VOID(form_bg_unload) { theBackground->UnloadBackground(); }
EE_UI_VOID(form_bg_relayout) { theBackground->RelayoutBackground(); }
EE_UI_CASE(form_hot_key_always) { return theKey->GetAlwaysFire(); }
EE_UI_CASE(form_hot_key_keys) { return theKey->GetKeyCount(); }
EE_UI_CASE(form_hot_key_key) { return (intptr_t)theKey->mKey; }
EE_UI_CASE(form_hot_key_mod0) { return (intptr_t)theKey->mModifiers[0]; }
EE_UI_CASE(form_hot_key_mod1) { return (intptr_t)theKey->mModifiers[1]; }
EE_UI_CASE(form_hot_key_mod2) { return (intptr_t)theKey->mModifiers[2]; }
EE_UI_CASE(form_hot_key_copy) { return (intptr_t) new UIHotKey(theForm, *theKey); }
EE_UI_VOID(form_hot_key_delete) { delete theKey; }
EE_UI_VOID(form_hot_key_set_enabled) { theKey->mEnabled = b != 0; }
EE_UI_CASE(form_hot_key_fire) { return theKey->FireEvents(); }
EE_UI_CASE(form_hot_key_temp)
{
    UIHotKey* theHotKey = new UIHotKey;
    theHotKey->mForm = theForm;
    theHotKey->SetKey((unsigned long)a);
    return (intptr_t)theHotKey;
}
EE_UI_VOID(form_hot_key_set_modifier) { theKey->SetModifier((unsigned long)b, (unsigned char)c); }
EE_UI_VOID(form_hot_key_set_always) { theKey->mAlwaysFire = b != 0; }
EE_UI_CASE(form_key_entity) { return (intptr_t)UIFormEntity(gISKeyboard, (unsigned long)a); }
EE_UI_CASE(form_keys_ready) { return UIControl::Key(EeUiKey::Tab) != NULL; }
EE_UI_CASE(form_tab_just_pressed) { return UIControl::Key(EeUiKey::Tab)->JustPressed(); }
EE_UI_CASE(form_shift_value) { return UIControl::Key(EeUiKey::Shift)->GetValue(); }
EE_UI_CASE(form_event_process) { return theEvent->ProcessEvent(theForm, NULL); }
EE_UI_CASE(form_wheel_event_new) { return (intptr_t) new UIEMouseWheelScroll; }
EE_UI_CASE(form_mouse_wheel) { return UIManager::GetMouseWheel() != NULL; }
EE_UI_CASE(form_wheel_amount) { return UIManager::GetMouseWheelScrollAmount(); }
EE_UI_CASE(form_mouse_entity) { return (intptr_t)UIFormEntity(gISMouse, (unsigned long)a); }
EE_UI_CASE(form_table) { return (intptr_t)UIControlsTable::GetUIControlsTable((unsigned long)a); }
EE_UI_CASE(form_table_form_index) { return theTable->GetFormIndex(); }
EE_UI_CASE(form_table_group) { return theTable->GetGroup(); }
EE_UI_CASE(form_table_visible) { return theTable->GetIsVisible() == true; }
EE_UI_CASE(form_table_type) { return theTable->GetControlType(); }
EE_UI_CASE(form_table_string_id) { return theTable->GetStringTableID(); }
EE_UI_CASE(form_table_create_control) { return (intptr_t)theTable->CreateControl(theForm->GetManager()); }
EE_UI_VOID(form_table_create_event) { theTable->CreateControlEvent(theForm); }
EE_UI_VOID(form_table_relayout) { theTable->RelayoutControl(reinterpret_cast<UIControl*>(b), theForm->GetManager()); }
EE_UI_CASE(form_surfaces) { return theForm->GetManager()->GetNumberOfSurfaces(); }
EE_UI_CASE(form_window_width) { return theForm->GetManager()->GetRasterizer()->GetWindow(theForm->GetManager()->GetScreen())->GetWidth(); }
EE_UI_CASE(form_window_height) { return theForm->GetManager()->GetRasterizer()->GetWindow(theForm->GetManager()->GetScreen())->GetHeight(); }
EE_UI_VOID(form_manager_process_events) { theForm->GetManager()->ProcessAllEventsForForm(theForm); }
EE_UI_VOID(form_manager_add_dirty) { theForm->GetManager()->AddDirtyRectangle(UIFormRect(f), theForm); }
EE_UI_VOID(form_music_stop) { theForm->GetManager()->StopMusic(); }
EE_UI_VOID(form_music_start) { theForm->GetManager()->StartMusic(UWideString((const char*)a)); }
EE_UI_CASE(form_create_form_event) { return (intptr_t)theForm->GetManager()->CreateFormEvent((unsigned long)a); }
EE_UI_VOID(form_manager_delete) { delete reinterpret_cast<UIFormManager*>(a); }
EE_UI_CASE(form_now) { return TMGetTime(); }
EE_UI_VOID(form_lock_delete) { TSScheduler::DestroyCriticalSection(reinterpret_cast<TSCriticalSection*>(a)); }
EE_UI_VOID(form_lock_enter) { reinterpret_cast<TSCriticalSection*>(a)->Enter(); }
EE_UI_VOID(form_lock_leave) { reinterpret_cast<TSCriticalSection*>(a)->Leave(); }
EE_UI_CASE(form_poll_create) { return (intptr_t) new TMPoll((unsigned long)a, 0, false); }
EE_UI_VOID(form_poll_delete) { delete thePoll; }
EE_UI_CASE(form_poll_running) { return thePoll->GetIsRunning(); }
EE_UI_CASE(form_poll_poll) { return thePoll->Poll(); }
EE_UI_VOID(form_poll_start) { thePoll->Start(); }
EE_UI_VOID(form_poll_stop) { thePoll->Stop(); }
EE_UI_VOID(form_poll_set_delay) { thePoll->SetTimeDelay((unsigned long)b); }
EE_UI_VOID(form_virt_help_location) { theForm->SetHelpTextPopupLocation(); }
EE_UI_CASE(form_virt_draw_background) { return theForm->DrawBackground(UIFormRect(f)); }
EE_UI_CASE(form_rhombus_create) { return (intptr_t) new GERhombus; }
EE_UI_VOID(form_rhombus_delete) { delete theRhombus; }
EE_UI_VOID(form_rhombus_label_color) { theRhombus->SetColor(static_cast<UITextLabel*>(reinterpret_cast<UIControl*>(b))->GetGEText()->GetFgColor()); }
#undef theForm
#undef theControl
#undef theGroup
#undef theBackground
#undef theKey
#undef theEvent
#undef theTable
#undef thePoll
#undef theRhombus

// Creates an empty form with the given control and group counts, tool tips and tabbing off
UIForm::UIForm(unsigned long inControlCount, unsigned long inControlGroupCount, UIManager* inManager,
               unsigned long inFormID, bool inUseGDIForText)
    : mRust(NULL)
{
    this->mRust = ee_ui_form_new(this, ee_ui_form_host, inControlCount, inControlGroupCount, inManager, inFormID,
                                 inUseGDIForText);
}

// Creates a form from its UI database record: counts, layout, tool-tip settings, music file (lower-cased), tabbing
UIForm::UIForm(UIManager* inManager, const UIDbDataForms& inData, bool inUseGDIForText)
    : mRust(NULL)
{
    UWideString theMusic(reinterpret_cast<const char*>(inData.mMusicFile));
    PlatMbsLwr((unsigned char*)(const char*)theMusic);
    this->mRust = ee_ui_form_new_db(this, ee_ui_form_host, inManager, &inData, (const char*)theMusic, inUseGDIForText);
}

// Destroys the form, releasing its events, hot keys, groups, backgrounds, database-created controls and draw lock
UIForm::~UIForm()
{
    ee_ui_form_free(this->mRust);
    this->mRust = NULL;
}

// Builds a hot key from a key, up to three modifiers and four events, then adds it to the given group
void UIForm::AddHotKey(unsigned long inKey, unsigned long inModifier1, unsigned long inModifier2,
                       unsigned long inModifier3, UIEvent* inJustPressedEvent, UIEvent* inPressedEvent,
                       UIEvent* inJustUnpressedEvent, UIEvent* inDoublePressedEvent, unsigned long inGroup,
                       bool inAlwaysFire)
{
    this->Call(EeUiFormCall::AddHotKeyEvents, (intptr_t)inJustPressedEvent, (intptr_t)inPressedEvent);
    this->Call(EeUiFormCall::AddHotKeyEvents2, (intptr_t)inJustUnpressedEvent, (intptr_t)inDoublePressedEvent);
    this->Call(EeUiFormCall::AddHotKeyBuild, ee_ui_form_pack_keys(inKey, inModifier1, inModifier2, inModifier3),
               (intptr_t)((uint64_t)(uint32_t)inGroup | (uint64_t)inAlwaysFire << 32));
}

// Draws a bitmap tiled over a tile area, clipped to the rectangle, one piece per tile
long UIForm::DrawBackground(GEBitmap* inBitmap, const GERectangle& inRectangle, const GERectangle& inDrawRectangle,
                            const GERectangle& inTileRectangle)
{
    this->CallRect(EeUiFormCall::StageRect, 0, 0, inRectangle);
    this->CallRect(EeUiFormCall::StageRect, 1, 0, inDrawRectangle);
    this->CallRect(EeUiFormCall::StageRect, 2, 0, inTileRectangle);
    return (long)this->Call(EeUiFormCall::DrawTiles, (intptr_t)inBitmap);
}


// Records the mouse location, finds the control under it, hides a tool tip the mouse enters, updates the mouse-over
void UIForm::UpdateMouseLocation(const GE2DPoint& inLocation)
{
    float f[4] = {inLocation.GetX(), inLocation.GetY(), 0.0f, 0.0f};
    this->Call(EeUiFormCall::UpdateMouseLocation, 0, 0, f);
}

float UIForm::GetFadeOpacity()
{
    float f[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    this->Call(EeUiFormCall::FadeOpacity, 0, 0, f);
    return f[0];
}
void UIForm::SetLayout(UILayoutUtility& inLayout)
{
    float f[4] = {inLayout.GetLeftOffset(), inLayout.GetRightOffset(), inLayout.GetTopOffset(), inLayout.GetBottomOffset()};
    this->Call(EeUiFormCall::SetLayout, ee_ui_form_pack_types(inLayout.GetLeftOffsetType(), inLayout.GetRightOffsetType(), inLayout.GetTopOffsetType(), inLayout.GetBottomOffsetType()), 0, f);
}

UITextLabel* UIForm::GetHelpTextLabel() const { return static_cast<UITextLabel*>(reinterpret_cast<UIControl*>(this->Call(EeUiFormCall::HelpTextLabel))); }

void UIForm::SetHelpTextLabel(UITextLabel* inLabel) { this->Call(EeUiFormCall::SetHelpTextLabel, (intptr_t) static_cast<UIControl*>(inLabel)); }
