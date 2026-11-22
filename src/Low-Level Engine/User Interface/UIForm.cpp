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
#define LLE_UIFORM_CPP      // UIForm::sTabKey and UIManager::sMouseWheel are visible here (see UIForm.h) (provisional name)
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

// unreferenced (its init chain 0x10070706 is the only user)
static const UString kUIFormKey("B7B2257DB7B2257DA49985B7B5A18DF4D49327D6B51165F42EB1A52ECC918DA616B16D26B41955F43C190DDE7EB1BD9624F92DDEFC610D9EB4B9559CB4210D84FC29"
                                "8596249907BE14CB25F6142B25B61C1165C624F9F59696B18D269C2357BEB51955B41C1165C624F9F59696B18D269EB16D26B431B5C4F4F935F6149907CEB7B34FF5");

ISEntity* UIForm::sTabKey;
ISEntity* UIForm::sShiftKey;

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
extern "C" void ee_uicpp_form_fetch_tab_key() { gISKeyboard->GetEntity(kKeyTab, UIForm::sTabKey); }
extern "C" void ee_uicpp_form_fetch_shift_key() { gISKeyboard->GetEntity(kKeyShift, UIForm::sShiftKey); }
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
intptr_t UIEntityGroup::HostOp(void*, EeUiOp inOp, intptr_t a, intptr_t b, intptr_t, intptr_t, float*)
{
    switch (inOp) {
    case EeUiOp::GrpIsButton: return reinterpret_cast<UIEntity*>(a)->GetType() == kControlTypeButton;
    case EeUiOp::GrpShown: return ENTITY_CONTROL->GetDisplayMode() == kDisplayModeShown;
    case EeUiOp::GrpOnCurrentForm: return ENTITY_CONTROL->GetForm() == ENTITY_CONTROL->GetForm()->GetManager()->GetCurrentForm();
    case EeUiOp::GrpAddDirty: ENTITY_CONTROL->GetForm()->GetManager()->AddDirtyRectangle(ENTITY_CONTROL->GetRectangle(), ENTITY_CONTROL->GetForm()); return 0;
    case EeUiOp::GrpHide: ENTITY_CONTROL->HideControl(); return 0;
    case EeUiOp::GrpSetDirty: ENTITY_CONTROL->SetDirty(true); return 0;
    case EeUiOp::GrpTabbing: return ENTITY_CONTROL->GetForm()->GetTabbingOn();
    case EeUiOp::GrpIsTabStop: return ENTITY_CONTROL->GetForm()->GetTabStop() == ENTITY_CONTROL->GetIndex();
    case EeUiOp::GrpTab: ENTITY_CONTROL->GetForm()->Tab(); return 0;
    case EeUiOp::GrpSetEnabled: static_cast<UIHotKey*>(reinterpret_cast<UIEntity*>(a))->SetEnabled(b != 0); return 0;
    default: return 0;
    }
}
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
extern "C" intptr_t ee_uicpp_form_host(EE_UI_HOST_ARGS) { return UIForm::HostOp(inObj, inOp, a, b, c, d, f); }
#define theForm static_cast<UIForm*>(inForm)
#define theControl reinterpret_cast<UIControl*>(a)
#define theGroup reinterpret_cast<UIEntityGroup*>(a)
#define theBackground reinterpret_cast<UIBackground*>(a)
#define theKey reinterpret_cast<UIHotKey*>(a)
#define theEvent reinterpret_cast<UIEvent*>(a)
#define theTable reinterpret_cast<UIControlsTable*>(a)
#define thePoll reinterpret_cast<TMPoll*>(a)
#define theRhombus reinterpret_cast<GERhombus*>(a)
intptr_t UIForm::HostOp(void* inForm, EeUiOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d, float* f)
{
    switch (inOp) {
    // the controls
    case EeUiOp::FormCtlType: return theControl->GetType();
    case EeUiOp::FormCtlEntityType: return theControl->UIEntity::GetType();
    case EeUiOp::FormCtlDisplayMode: return theControl->GetDisplayMode();
    case EeUiOp::FormCtlDirty: return theControl->GetIsDirty();
    case EeUiOp::FormCtlDrawCount: return theControl->GetDrawCount();
    case EeUiOp::FormCtlIndex: return theControl->GetIndex();
    case EeUiOp::FormCtlId: return theControl->GetId();
    case EeUiOp::FormCtlTabable: return theControl->GetTabable();
    case EeUiOp::FormCtlChecked: return static_cast<UIRadioButton*>(theControl)->GetIsChecked();
    case EeUiOp::FormCtlImeEnabled: return theControl->GetIMEEnabled();
    case EeUiOp::PartRect: UIObjHost::PutRect(f, theControl->GetRectangle()); return 0;
    case EeUiOp::PartCopiedDrawRect: UIObjHost::PutRect(f, theControl->GetCopiedDrawRectangle()); return 0;
    case EeUiOp::PartCopyData: return theControl->CopyData();
    case EeUiOp::PartDrawPolygons: return theControl->DrawPolygons();
    case EeUiOp::PartDrawText: return theControl->DrawTextA();
    case EeUiOp::PartSetLocation: theControl->SetLocation(GE2DPoint(f[0], f[1])); return 0;
    case EeUiOp::DeletePart: delete theControl; return 0;
    case EeUiOp::FormCtlSetDirty: theControl->SetDirty(b != 0); return 0;
    case EeUiOp::FormCtlSetDisplayMode: theControl->SetDisplayMode((unsigned long)b); return 0;
    case EeUiOp::FormCtlHide: theControl->HideControl(); return 0;
    case EeUiOp::FormCtlSetStringId: theControl->SetStringTableID((unsigned long)b); return 0;
    case EeUiOp::FormCtlTabbedFrom: theControl->TabbedFrom(); return 0;
    case EeUiOp::FormCtlTabbedTo: theControl->TabbedTo(); return 0;
    case EeUiOp::FormCtlUpdateKeyboard: return theControl->UpdateKeyboard();
    case EeUiOp::FormCtlUpdateMouse: return theControl->UpdateMouse(theForm->GetMouseLocation());
    case EeUiOp::FormCtlWheelTurned: return theControl->MouseWheelTurned((long)b);
    case EeUiOp::FormCtlFireTrigger: theControl->FireTriggerEvent((unsigned long)b); return 0;
    case EeUiOp::FormCtlReleaseMouse: theControl->ReleaseCapturedMouse(); return 0;
    case EeUiOp::FormCtlReleaseKeyboard: theControl->ReleaseCapturedKeyboard(); return 0;
    case EeUiOp::FormHelpSetText: static_cast<UITextLabel*>(theControl)->SetText(sUIFormHelpString, true); return 0;
    case EeUiOp::FormLabelFit: return static_cast<UITextLabel*>(theControl)->GetFitRectToText();
    // the groups
    case EeUiOp::FormGroupEnabled: return theGroup->GetIsEnabled();
    case EeUiOp::FormGroupCount: return theGroup->GetEntityCount();
    case EeUiOp::FormGroupEntity: return (intptr_t) static_cast<UIControl*>(theGroup->GetEntity((unsigned long)b));
    case EeUiOp::FormGroupNew: return (intptr_t) new UIEntityGroup;
    case EeUiOp::FormGroupDelete: delete theGroup; return 0;
    case EeUiOp::FormGroupAdd: theGroup->AddEntity(reinterpret_cast<UIControl*>(b)); return 0;
    case EeUiOp::FormGroupRemove: theGroup->RemoveEntity(reinterpret_cast<UIControl*>(b)); return 0;
    // the backgrounds
    case EeUiOp::FormBgPriority: return theBackground->GetDrawPriority();
    case EeUiOp::FormBgId: return theBackground->GetBackgroundID();
    case EeUiOp::FormBgVisible: return theBackground->GetIsVisible();
    case EeUiOp::FormBgRect: UIObjHost::PutRect(f, theBackground->GetRectangle()); return 0;
    case EeUiOp::FormBgOpen: theBackground->Open(); return 0;
    case EeUiOp::FormBgClose: theBackground->Close(); return 0;
    case EeUiOp::FormBgDelete: delete theBackground; return 0;
    case EeUiOp::FormBgDraw: return theBackground->DrawBackground(UIFormRect(f));
    case EeUiOp::FormBgLoad: theBackground->LoadBackground(); return 0;
    case EeUiOp::FormBgUnload: theBackground->UnloadBackground(); return 0;
    case EeUiOp::FormBgRelayout: theBackground->RelayoutBackground(); return 0;
    // the hot keys
    case EeUiOp::FormHotKeyAlways: return theKey->GetAlwaysFire();
    case EeUiOp::FormHotKeyKeys: return theKey->GetKeyCount();
    case EeUiOp::FormHotKeyKey: return (intptr_t)theKey->mKey;
    case EeUiOp::FormHotKeyMod0: return (intptr_t)theKey->mModifiers[0];
    case EeUiOp::FormHotKeyMod1: return (intptr_t)theKey->mModifiers[1];
    case EeUiOp::FormHotKeyMod2: return (intptr_t)theKey->mModifiers[2];
    case EeUiOp::FormHotKeyCopy: return (intptr_t) new UIHotKey(theForm, *theKey);
    case EeUiOp::FormHotKeyDelete: delete theKey; return 0;
    case EeUiOp::FormHotKeySetEnabled: theKey->mEnabled = b != 0; return 0;
    case EeUiOp::FormHotKeyFire: return theKey->FireEvents();
    case EeUiOp::FormHotKeyTemp: {   // (mForm is the record's: UIForm is the hot key's friend)
        UIHotKey* theHotKey = new UIHotKey;
        theHotKey->mForm = theForm;
        theHotKey->SetKey((unsigned long)a);
        return (intptr_t)theHotKey;
    }
    case EeUiOp::FormHotKeySetModifier: theKey->SetModifier((unsigned long)b, (unsigned char)c); return 0;
    case EeUiOp::FormHotKeySetAlways: theKey->mAlwaysFire = b != 0; return 0;
    case EeUiOp::FormKeyEntity: return (intptr_t)UIFormEntity(gISKeyboard, (unsigned long)a);
    case EeUiOp::FormKeysReady: return UIForm::sTabKey != NULL;
    case EeUiOp::FormTabJustPressed: return UIForm::sTabKey->JustPressed();
    case EeUiOp::FormShiftValue: return UIForm::sShiftKey->GetValue();
    // the events
    case EeUiOp::FormEventProcess: return theEvent->ProcessEvent(theForm, NULL);
    case EeUiOp::FormWheelEventNew: return (intptr_t) new UIEMouseWheelScroll;
    // the mouse
    case EeUiOp::FormMouseWheel: return UIManager::GetMouseWheel() != NULL;
    case EeUiOp::FormWheelAmount: return UIManager::GetMouseWheelScrollAmount();
    case EeUiOp::FormMouseEntity: return (intptr_t)UIFormEntity(gISMouse, (unsigned long)a);
    // the controls table
    case EeUiOp::FormTable: return (intptr_t)UIControlsTable::GetUIControlsTable((unsigned long)a);
    case EeUiOp::FormTableFormIndex: return theTable->GetFormIndex();
    case EeUiOp::FormTableGroup: return theTable->GetGroup();
    case EeUiOp::FormTableVisible: return theTable->GetIsVisible() == true;
    case EeUiOp::FormTableType: return theTable->GetControlType();
    case EeUiOp::FormTableStringId: return theTable->GetStringTableID();
    case EeUiOp::FormTableCreateControl: return (intptr_t)theTable->CreateControl(theForm->GetManager());
    case EeUiOp::FormTableCreateEvent: theTable->CreateControlEvent(theForm); return 0;
    case EeUiOp::FormTableRelayout: theTable->RelayoutControl(reinterpret_cast<UIControl*>(b), theForm->GetManager()); return 0;
    // the manager
    case EeUiOp::FormSurfaces: return theForm->GetManager()->GetNumberOfSurfaces();
    case EeUiOp::FormWindowWidth: return theForm->GetManager()->GetRasterizer()->GetWindow(theForm->GetManager()->GetScreen())->GetWidth();
    case EeUiOp::FormWindowHeight: return theForm->GetManager()->GetRasterizer()->GetWindow(theForm->GetManager()->GetScreen())->GetHeight();
    case EeUiOp::FormManagerProcessEvents: theForm->GetManager()->ProcessAllEventsForForm(theForm); return 0;
    case EeUiOp::FormManagerAddDirty: theForm->GetManager()->AddDirtyRectangle(UIFormRect(f), theForm); return 0;
    case EeUiOp::FormMusicStop: theForm->GetManager()->StopMusic(); return 0;
    case EeUiOp::FormMusicStart: theForm->GetManager()->StartMusic(UWideString((const char*)a)); return 0;
    case EeUiOp::FormCreateFormEvent: return (intptr_t)theForm->GetManager()->CreateFormEvent((unsigned long)a);
    case EeUiOp::FormManagerDelete: delete reinterpret_cast<UIFormManager*>(a); return 0;
    case EeUiOp::FormNow: return TMGetTime();
    // the draw-background lock, the help-text poll
    case EeUiOp::FormLockDelete: TSScheduler::DestroyCriticalSection(reinterpret_cast<TSCriticalSection*>(a)); return 0;
    case EeUiOp::FormLockEnter: reinterpret_cast<TSCriticalSection*>(a)->Enter(); return 0;
    case EeUiOp::FormLockLeave: reinterpret_cast<TSCriticalSection*>(a)->Leave(); return 0;
    case EeUiOp::PollCreate: return (intptr_t) new TMPoll((unsigned long)a, 0, false);
    case EeUiOp::PollDelete: delete thePoll; return 0;
    case EeUiOp::PollRunning: return thePoll->GetIsRunning();
    case EeUiOp::PollPoll: return thePoll->Poll();
    case EeUiOp::PollStart: thePoll->Start(); return 0;
    case EeUiOp::PollStop: thePoll->Stop(); return 0;
    case EeUiOp::PollSetDelay: thePoll->SetTimeDelay((unsigned long)b); return 0;
    // the form's own virtuals, so that a game form's override runs
    case EeUiOp::FormVirtHelpLocation: theForm->SetHelpTextPopupLocation(); return 0;
    case EeUiOp::FormVirtDrawBackground: return theForm->DrawBackground(UIFormRect(f));
    // the draws
    case EeUiOp::RhombusCreate: return (intptr_t) new GERhombus;
    case EeUiOp::RhombusDelete: delete theRhombus; return 0;
    case EeUiOp::FormRhombusLabelColor: theRhombus->SetColor(static_cast<UITextLabel*>(reinterpret_cast<UIControl*>(b))->GetGEText()->GetFgColor()); return 0;
    default: return 0;
    }
}
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
    UWideString theMusic(inData.mMusicFile);
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
