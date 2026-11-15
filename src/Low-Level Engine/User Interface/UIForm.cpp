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

#include "rust/ee_ui.h"
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
#include "lle/UIEMouseWheelScroll.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIFormManager.h"
#include "lle/UIForm.h"
#include "lle/UIControlsTable.h"
#include "lle/UIManager.h"

// unreferenced (its init chain 0x10070706 is the only user)
static const UString kUIFormKey("B7B2257DB7B2257DA49985B7B5A18DF4D49327D6B51165F42EB1A52ECC918DA616B16D26B41955F43C190DDE7EB1BD9624F92DDEFC610D9EB4B9559CB4210D84FC29"
                                "8596249907BE14CB25F6142B25B61C1165C624F9F59696B18D269C2357BEB51955B41C1165C624F9F59696B18D269EB16D26B431B5C4F4F935F6149907CEB7B34FF5");

ISEntity* UIForm::sTabKey;
ISEntity* UIForm::sShiftKey;

static GERectangle UIFormRect(const float* inF) { return GERectangle(inF[1], inF[3], inF[2], inF[0]); }
static void UIFormPutRect(float* outF, const GERectangle& inRectangle)
{
    outF[0] = inRectangle.mBottom;
    outF[1] = inRectangle.mLeft;
    outF[2] = inRectangle.mRight;
    outF[3] = inRectangle.mTop;
}

// UIEntityGroup is whole in Rust (rust/crates/ee_ui/src/groupobj.rs, R6 ui round 21; the original unit
// UIEntityGroup.cpp, 0x1006e304..0x1006e5a8): the calls out of Disable/Enable on entity a, a button or a hot key
intptr_t UIEntityGroup::HostOp(void*, EeUiOp inOp, intptr_t a, intptr_t b, intptr_t, intptr_t, float*)
{
    UIEntity* theEntity = reinterpret_cast<UIEntity*>(a);
    UIControl* theControl = static_cast<UIControl*>(theEntity);
    switch (inOp) {
    case EeUiOp::GrpIsButton: return theEntity->GetType() == kControlTypeButton;
    case EeUiOp::GrpShown: return theControl->GetDisplayMode() == kDisplayModeShown;
    case EeUiOp::GrpOnCurrentForm: return theControl->GetForm() == theControl->GetForm()->GetManager()->GetCurrentForm();
    case EeUiOp::GrpAddDirty:
        theControl->GetForm()->GetManager()->AddDirtyRectangle(theControl->GetRectangle(), theControl->GetForm());
        break;
    case EeUiOp::GrpHide: theControl->HideControl(); break;
    case EeUiOp::GrpSetDirty: theControl->SetDirty(true); break;
    case EeUiOp::GrpTabbing: return theControl->GetForm()->GetTabbingOn();
    case EeUiOp::GrpIsTabStop: return theControl->GetForm()->GetTabStop() == theControl->GetIndex();
    case EeUiOp::GrpTab: theControl->GetForm()->Tab(); break;
    case EeUiOp::GrpSetEnabled: static_cast<UIHotKey*>(theEntity)->SetEnabled(b != 0); break;
    default: break;
    }
    return 0;
}

intptr_t UIForm::Call(EeUiFormCall inCall, intptr_t inA, intptr_t inB, float* ioF) const
{
    return ee_ui_form_call(this->mRust, inCall, inA, inB, ioF);
}

// The Rust form's calls out (EeUiOp, obj.rs): a is the control, group, background, hot key, event, table or other
// object the op names; rectangles and points travel in f
intptr_t UIForm::HostOp(void* inForm, EeUiOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d, float* f)
{
    static UWideString sHelpString;   // FormHelpGet's string, which FormHelpSetText sets (the UI thread's)
    UIForm* theForm = static_cast<UIForm*>(inForm);
    UIControl* theControl = reinterpret_cast<UIControl*>(a);
    UIEntityGroup* theGroup = reinterpret_cast<UIEntityGroup*>(a);
    UIBackground* theBackground = reinterpret_cast<UIBackground*>(a);
    UIHotKey* theKey = reinterpret_cast<UIHotKey*>(a);
    UIEvent* theEvent = reinterpret_cast<UIEvent*>(a);
    UIControlsTable* theTable = reinterpret_cast<UIControlsTable*>(a);
    GEBitmap* theBitmap = reinterpret_cast<GEBitmap*>(a);
    TMPoll* thePoll = reinterpret_cast<TMPoll*>(a);
    GERhombus* theRhombus = reinterpret_cast<GERhombus*>(a);
    ISEntity* theEntity = NULL;
    (void)d;
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
    case EeUiOp::PartRect: UIFormPutRect(f, theControl->GetRectangle()); return 0;
    case EeUiOp::PartCopiedDrawRect: UIFormPutRect(f, theControl->GetCopiedDrawRectangle()); return 0;
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
    case EeUiOp::FormHelpGet:
        sHelpString = "";
        return theControl->GetHelpString(sHelpString);
    case EeUiOp::FormHelpSetText: static_cast<UITextLabel*>(theControl)->SetText(sHelpString, true); return 0;
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
    case EeUiOp::FormBgRect: UIFormPutRect(f, theBackground->GetRectangle()); return 0;
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
    case EeUiOp::FormHotKeyTemp: {
        UIHotKey* theHotKey = new UIHotKey;
        theHotKey->mForm = theForm;
        theHotKey->SetKey((unsigned long)a);
        return (intptr_t)theHotKey;
    }
    case EeUiOp::FormHotKeySetModifier: theKey->SetModifier((unsigned long)b, (unsigned char)c); return 0;
    case EeUiOp::FormHotKeySetEvent: {
        UIEvent* theSet = reinterpret_cast<UIEvent*>(c);
        if (b == 0)
            theKey->SetJustPressedEvent(theSet);
        else if (b == 1)
            theKey->SetPressedEvent(theSet);
        else if (b == 2)
            theKey->SetJustUnpressedEvent(theSet);
        else
            theKey->SetDoublePressedEvent(theSet);
        return 0;
    }
    case EeUiOp::FormHotKeySetAlways: theKey->mAlwaysFire = b != 0; return 0;
    case EeUiOp::FormKeyEntity: gISKeyboard->GetEntity((unsigned long)a, theEntity); return (intptr_t)theEntity;
    case EeUiOp::FormKeysReady: return UIForm::sTabKey != NULL;
    case EeUiOp::FormKeysFetch:
        gISKeyboard->GetEntity(kKeyTab, UIForm::sTabKey);
        gISKeyboard->GetEntity(kKeyShift, UIForm::sShiftKey);
        return 0;
    case EeUiOp::FormTabJustPressed: return UIForm::sTabKey->JustPressed();
    case EeUiOp::FormShiftValue: return UIForm::sShiftKey->GetValue();
    // the events
    case EeUiOp::FormEventCall:
        if (b == 0)
            theEvent->Uninitialize();
        else if (b == 1)
            theEvent->Dereference();
        else if (b == 2)
            theEvent->Reference();
        else
            theEvent->Initialize();
        return 0;
    case EeUiOp::FormEventProcess: return theEvent->ProcessEvent(theForm, NULL);
    case EeUiOp::FormWheelEventNew: return (intptr_t) new UIEMouseWheelScroll;
    // the mouse
    case EeUiOp::FormMouseWheel: return UIManager::GetMouseWheel() != NULL;
    case EeUiOp::FormWheelAmount: return UIManager::GetMouseWheelScrollAmount();
    case EeUiOp::FormMouseEntity: gISMouse->GetEntity((unsigned long)a, theEntity); return (intptr_t)theEntity;
    case EeUiOp::FormMouseAsk: {
        ISEntity* theAsked = reinterpret_cast<ISEntity*>(a);
        if (b == 0)
            return theAsked->GetValue() != 0;
        if (b == 1)
            return theAsked->JustPressed();
        if (b == 2)
            return theAsked->JustUnpressed();
        return theAsked->JustDoublePressed();
    }
    // the controls table
    case EeUiOp::FormTable: return (intptr_t)UIControlsTable::GetUIControlsTable((unsigned long)a);
    case EeUiOp::FormTableFormIndex: return theTable->GetFormIndex();
    case EeUiOp::FormTableGroup: return theTable->GetGroup();
    case EeUiOp::FormTableVisible: return theTable->GetIsVisible() == true;
    case EeUiOp::FormTableType: return theTable->GetControlType();
    case EeUiOp::FormTableStringId: return theTable->GetStringTableID();
    case EeUiOp::FormTableCreateControl: return (intptr_t)theTable->CreateControl(theForm->GetManager());
    case EeUiOp::FormTableCreateEvent: theTable->CreateControlEvent(theForm); return 0;
    case EeUiOp::FormTableRelayout:
        theTable->RelayoutControl(reinterpret_cast<UIControl*>(b), theForm->GetManager());
        return 0;
    // the manager
    case EeUiOp::FormSurfaces: return theForm->GetManager()->GetNumberOfSurfaces();
    case EeUiOp::FormWindowWidth:
        return theForm->GetManager()->GetRasterizer()->GetWindow(theForm->GetManager()->GetScreen())->GetWidth();
    case EeUiOp::FormWindowHeight:
        return theForm->GetManager()->GetRasterizer()->GetWindow(theForm->GetManager()->GetScreen())->GetHeight();
    case EeUiOp::FormCandidateShown: return theForm->GetManager()->GetDrawCandidateWindow();
    case EeUiOp::FormCandidateRect:
        UIFormPutRect(f, theForm->GetManager()->GetCandidateWindow()->GetIMECopyCandidateRectangle());
        return 0;
    case EeUiOp::FormManagerProcessEvents: theForm->GetManager()->ProcessAllEventsForForm(theForm); return 0;
    case EeUiOp::FormManagerAddDirty: theForm->GetManager()->AddDirtyRectangle(UIFormRect(f), theForm); return 0;
    case EeUiOp::FormMusicStop: theForm->GetManager()->StopMusic(); return 0;
    case EeUiOp::FormMusicStart: theForm->GetManager()->StartMusic(UWideString((const char*)a)); return 0;
    case EeUiOp::FormCreateFormEvent: return (intptr_t)theForm->GetManager()->CreateFormEvent((unsigned long)a);
    case EeUiOp::FormManagerDelete: delete reinterpret_cast<UIFormManager*>(a); return 0;
    case EeUiOp::FormNow: return TMGetTime();
    // the draw-background lock, the help-text poll
    case EeUiOp::FormLockNew: {
        TSCriticalSection* theLock = NULL;
        ESVERIFY_AT(TSScheduler::CreateCriticalSection(theLock) == keseOK, 1743);
        return (intptr_t)theLock;
    }
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
    case EeUiOp::FormBitmapTexSize:
        f[0] = (float)theBitmap->GetTexture(theForm->GetManager()->GetScreen())->GetWidth();
        f[1] = (float)theBitmap->GetTexture(theForm->GetManager()->GetScreen())->GetHeight();
        return 0;
    case EeUiOp::BitmapSetLocation: theBitmap->SetLocation(GE2DPoint(f[0], f[1])); return 0;
    case EeUiOp::BitmapSetScale: theBitmap->SetXYScale(f[0], f[1]); return 0;
    case EeUiOp::FormBitmapSetTexCoords: {
        GETextureCoordinate theTopLeft;
        GETextureCoordinate theBottomRight;
        GETextureCoordinate theTopRight;
        GETextureCoordinate theBottomLeft;
        theTopLeft.SetU(f[0]);
        theTopLeft.SetV(f[1]);
        theBottomRight.SetU(f[2]);
        theBottomRight.SetV(f[3]);
        theTopRight.SetU(theBottomRight.GetU());
        theTopRight.SetV(theTopLeft.GetV());
        theBottomLeft.SetU(theTopLeft.GetU());
        theBottomLeft.SetV(theBottomRight.GetV());
        theBitmap->SetTextureCoordinates(theTopLeft, theTopRight, theBottomLeft, theBottomRight);
        return 0;
    }
    case EeUiOp::FormBitmapDraw:
        return theForm->GetManager()->GetRasterizer()->Draw(theBitmap, theForm->GetManager()->GetScreen());
    case EeUiOp::FormTextDraw:
        return theForm->GetManager()->GetRasterizer()->Draw(reinterpret_cast<GEText*>(a), UIFormRect(f),
                                                            theForm->GetManager()->GetDrawingSurface());
    case EeUiOp::RhombusCreate: return (intptr_t) new GERhombus;
    case EeUiOp::RhombusDelete: delete theRhombus; return 0;
    case EeUiOp::RhombusSetOpacity: theRhombus->SetOpacity(f[0]); return 0;
    case EeUiOp::RhombusSetRect: theRhombus->SetRectangle(UIFormRect(f)); return 0;
    case EeUiOp::FormRhombusLabelColor:
        theRhombus->SetColor(static_cast<UITextLabel*>(reinterpret_cast<UIControl*>(b))->GetGEText()->GetFgColor());
        return 0;
    case EeUiOp::FormRhombusDraw:
        return theForm->GetManager()->GetRasterizer()->Draw(theRhombus, theForm->GetManager()->GetScreen());
    default:
        break;
    }
    return 0;
}

// Creates an empty form with the given control and group counts, tool tips and tabbing off
UIForm::UIForm(unsigned long inControlCount, unsigned long inControlGroupCount, UIManager* inManager,
               unsigned long inFormID, bool inUseGDIForText)
    : mRust(NULL)
{
    this->mRust = ee_ui_form_new(this, UIForm::HostOp, inControlCount, inControlGroupCount, inManager, inFormID,
                                 inUseGDIForText);
}

// Creates a form from its UI database record: counts, layout, tool-tip settings, music file (lower-cased), tabbing
UIForm::UIForm(UIManager* inManager, const UIDbDataForms& inData, bool inUseGDIForText)
    : mRust(NULL)
{
    UWideString theMusic(inData.mMusicFile);
    PlatMbsLwr((unsigned char*)(const char*)theMusic);
    this->mRust = ee_ui_form_new_db(this, UIForm::HostOp, inManager, &inData, (const char*)theMusic, inUseGDIForText);
}

// Destroys the form, releasing its events, hot keys, groups, backgrounds, database-created controls and draw lock
UIForm::~UIForm()
{
    ee_ui_form_free(this->mRust);
    this->mRust = NULL;
}

// Starts the form's music (or stops the music for "none"), restores the tab stop and fires the activate event
void UIForm::Activate() { this->Call(EeUiFormCall::Activate); }

// Inserts a background before the first one with an equal or higher draw priority
void UIForm::AddBackground(UIBackground* inBackground) { this->Call(EeUiFormCall::AddBackground, (intptr_t)inBackground); }

// Adds a rectangle that text drawing must keep out of
void UIForm::AddClipRectangle(const GERectangle& inRectangle)
{
    float f[4];
    UIFormPutRect(f, inRectangle);
    this->Call(EeUiFormCall::AddClipRectangle, 0, 0, f);
}

// Puts a control into its form slot (when the slot is empty) and adds it to the given group
void UIForm::AddControl(UIControl* inControl, unsigned long inGroup)
{
    this->Call(EeUiFormCall::AddControl, (intptr_t)inControl, (intptr_t)inGroup);
}

// Adds a rectangle to the background-redraw list, splitting it around listed rectangles so none overlap
void UIForm::AddCopyDirtyRectangle(const GERectangle& inRectangle)
{
    float f[4];
    UIFormPutRect(f, inRectangle);
    this->Call(EeUiFormCall::AddCopyDirtyRectangle, 0, 0, f);
}

void UIForm::AddDatabaseControl(const UICDatabase& inControl)
{
    this->Call(EeUiFormCall::AddDatabaseControl, (intptr_t)inControl.mTableIndex, (intptr_t)inControl.mControl);
}

// Lists the control in the given slot for drawing this frame
void UIForm::AddDrawControl(unsigned long inIndex) { this->Call(EeUiFormCall::AddDrawControl, (intptr_t)inIndex); }

// Queues a dirty rectangle for every drawing surface, dropping queued ones it covers; marks an overlapped popup dirty
void UIForm::AddDirtyRectangle(const GERectangle& inRectangle)
{
    float f[4];
    UIFormPutRect(f, inRectangle);
    this->Call(EeUiFormCall::AddDirtyRectangle, 0, 0, f);
}

void UIForm::AddEvent(const UIEventData& inEvent)
{
    this->Call(EeUiFormCall::AddEvent, (intptr_t)inEvent.mEventID, (intptr_t)inEvent.mEventType);
}

// Copies a hot key into the given group and the form's list, keeping both sorted by key count, always-fire keys last
void UIForm::AddHotKey(const UIHotKey& inHotKey, unsigned long inGroup)
{
    this->Call(EeUiFormCall::AddHotKey, (intptr_t)&inHotKey, (intptr_t)inGroup);
}

// The key and the three modifiers, 16 bits each (a code past 0xffff is no key, as kKeyNone 0x7fff is)
static intptr_t UIFormKeys(unsigned long inKey, unsigned long inModifier1, unsigned long inModifier2,
                           unsigned long inModifier3)
{
    uint64_t theKeys[4] = {inKey, inModifier1, inModifier2, inModifier3};
    uint64_t thePacked = 0;
    for (int i = 0; i < 4; i++)
        thePacked |= (theKeys[i] < 0xffff ? theKeys[i] : 0xffff) << (16 * i);
    return (intptr_t)thePacked;
}

// Builds a hot key from a key, up to three modifiers and four events, then adds it to the given group
void UIForm::AddHotKey(unsigned long inKey, unsigned long inModifier1, unsigned long inModifier2,
                       unsigned long inModifier3, UIEvent* inJustPressedEvent, UIEvent* inPressedEvent,
                       UIEvent* inJustUnpressedEvent, UIEvent* inDoublePressedEvent, unsigned long inGroup,
                       bool inAlwaysFire)
{
    this->Call(EeUiFormCall::AddHotKeyEvents, (intptr_t)inJustPressedEvent, (intptr_t)inPressedEvent);
    this->Call(EeUiFormCall::AddHotKeyEvents2, (intptr_t)inJustUnpressedEvent, (intptr_t)inDoublePressedEvent);
    this->Call(EeUiFormCall::AddHotKeyBuild, UIFormKeys(inKey, inModifier1, inModifier2, inModifier3),
               (intptr_t)((uint64_t)(uint32_t)inGroup | (uint64_t)inAlwaysFire << 32));
}

// Passes an IME result string to the control holding the keyboard
void UIForm::AddIMEResultString(const UWideString& inString)
{
    if (UIControl* theControl = this->GetKeyboardCaptured())
        theControl->AddIMEResultString(inString);
}

// Passes the IME composition string to the control holding the keyboard; kErrorOK when none does
long UIForm::ChangeIMECompositionString(const UWideString& inString, const UString& inAttributes,
                                        unsigned long inCursor)
{
    UIControl* theControl = this->GetKeyboardCaptured();
    return theControl ? theControl->ChangeIMECompositionString(inString, inAttributes, inCursor) : kErrorOK;
}

// Moves a radio-button index to the checked button of its run of radio buttons (unchanged if it is checked)
void UIForm::CheckedRadioButton(long& ioIndex) { ioIndex = (long)this->Call(EeUiFormCall::CheckedRadioButton, ioIndex); }

void UIForm::ClearAllClipRectangles() { this->Call(EeUiFormCall::ClearAllClipRectangles); }

// Closes the form: fires the close event, clears the draw list and popup, closes the backgrounds, flushes its events
// and deletes the controls created from the database
void UIForm::Close() { this->Call(EeUiFormCall::Close); }

// Closes the IME composition string of the control holding the keyboard; kErrorOK when none does
long UIForm::CloseIMECompositionString()
{
    UIControl* theControl = this->GetKeyboardCaptured();
    return theControl ? theControl->CloseIMECompositionString() : kErrorOK;
}

// Reads every form record from the UI database file and creates, registers and enables each form with an ID above 0
long UIForm::Create(FSFileSpec& inFile, UIManager* inManager)
{
    unsigned long theCount = 0;
    unsigned long theRead = 0;
    long theResult;
    UIDbDataForms theData;
    inFile.Open(kFileAccessRead);
    theResult = inFile.Read(theCount);
    if (theResult == kErrorOK) {
        for (unsigned long i = 0; i < theCount; i++) {
            theResult = inFile.Read(reinterpret_cast<unsigned char*>(&theData), sizeof(theData), theRead);
            if (!(theData.mFormID > 0))
                continue;
            UIForm* theForm = inManager->CreateForm(theData);
            inManager->AddForm(theForm);
            theForm->GetEntityGroup(0)->Enable();
        }
    }
    inFile.Close();
    return theResult;
}

// Fires the deactivate event, hides the popup and help text, and releases the captured mouse and keyboard
void UIForm::Deactivate() { this->Call(EeUiFormCall::Deactivate); }

// Snapshots the form for drawing: fires the update event, copies every dirty shown control, lists the controls to draw,
// ages the dirty rectangles and updates the help text
long UIForm::CopyData() { return (long)this->Call(EeUiFormCall::CopyData); }

// Redraws every visible background that overlaps the rectangle; returns the last draw result
long UIForm::DrawBackground(const GERectangle& inRectangle)
{
    float f[4];
    UIFormPutRect(f, inRectangle);
    return (long)this->Call(EeUiFormCall::DrawBackground, 0, 0, f);
}

// Draws the part of a bitmap's texture under a rectangle at a screen location
long UIForm::DrawBackground(GEBitmap* inBitmap, const GERectangle& inRectangle, const GE2DPoint& inLocation)
{
    float f[4];
    UIFormPutRect(f, inRectangle);
    float theX = inLocation.GetX(), theY = inLocation.GetY();
    uint32_t theBits[2];
    memcpy(&theBits[0], &theX, 4);
    memcpy(&theBits[1], &theY, 4);
    return (long)this->Call(EeUiFormCall::DrawBitmap, (intptr_t)inBitmap,
                            (intptr_t)((uint64_t)theBits[0] | (uint64_t)theBits[1] << 32), f);
}

// Draws a bitmap tiled over a tile area, clipped to the rectangle, one piece per tile
long UIForm::DrawBackground(GEBitmap* inBitmap, const GERectangle& inRectangle, const GERectangle& inDrawRectangle,
                            const GERectangle& inTileRectangle)
{
    const GERectangle* theRectangles[3] = {&inRectangle, &inDrawRectangle, &inTileRectangle};
    for (int i = 0; i < 3; i++) {
        float f[4];
        UIFormPutRect(f, *theRectangles[i]);
        this->Call(EeUiFormCall::StageRect, i, 0, f);
    }
    return (long)this->Call(EeUiFormCall::DrawTiles, (intptr_t)inBitmap);
}

// Draws text clipped around the form's clip rectangles, the copied popup or the IME candidate window
long UIForm::DrawClippedText(const GERectangle& inRectangle, GEText* inText, UIControl* inControl)
{
    float f[4];
    UIFormPutRect(f, inRectangle);
    return (long)this->Call(EeUiFormCall::DrawClippedText, (intptr_t)inText, (intptr_t)inControl, f);
}

// Redraws the queued background rectangles, then every listed control and the popup (with a shadow under a tool tip)
long UIForm::DrawPolygons() { return (long)this->Call(EeUiFormCall::DrawPolygons); }

// Draws the text of every listed control, then the popup's, and empties the draw list
long UIForm::DrawTextA() { return (long)this->Call(EeUiFormCall::DrawText); }

// Returns the background with the given ID, or NULL
UIBackground* UIForm::GetBackgroundByID(unsigned long inID) const
{
    return reinterpret_cast<UIBackground*>(this->Call(EeUiFormCall::BackgroundById, (intptr_t)inID));
}

// Returns the control with the given ID (NULL: none)
UIControl* UIForm::GetControlById(unsigned long inId)
{
    return reinterpret_cast<UIControl*>(this->Call(EeUiFormCall::ControlById, (intptr_t)inId));
}

// Returns the group holding the control with the given ID; falls back to group 0 when no group holds it
UIEntityGroup* UIForm::GetEntityGroupById(unsigned long inId) const
{
    return reinterpret_cast<UIEntityGroup*>(this->Call(EeUiFormCall::EntityGroupById, (intptr_t)inId));
}

// Returns false: forms never fade in
bool UIForm::GetShouldFadeIn() { return this->Call(EeUiFormCall::ShouldFadeIn) != 0; }

// Loads every background's texture
void UIForm::LoadBackgrounds() { this->Call(EeUiFormCall::LoadBackgrounds); }

// Opens the form: lays it out on the screen, opens the backgrounds, creates the database controls and their events,
// fires the open event and marks every control dirty
void UIForm::Open() { this->Call(EeUiFormCall::Open); }

// Opens an IME composition string in the control holding the keyboard; kErrorFailed when none does
long UIForm::OpenIMECompositionString()
{
    UIControl* theControl = this->GetKeyboardCaptured();
    return theControl ? theControl->OpenIMECompositionString() : kErrorFailed;
}

// Deletes every hot key of every group
void UIForm::RemoveAllHotKeys() { this->Call(EeUiFormCall::RemoveAllHotKeys); }

// Deletes every hot key of one group
void UIForm::RemoveAllHotKeysForAGroup(unsigned long inGroup)
{
    this->Call(EeUiFormCall::RemoveAllHotKeysForAGroup, (intptr_t)inGroup);
}

// Deletes the first hot key of a group that matches the key and the three modifiers
void UIForm::RemoveHotKey(unsigned long inKey, unsigned long inModifier1, unsigned long inModifier2,
                          unsigned long inModifier3, unsigned long inGroup)
{
    this->Call(EeUiFormCall::RemoveHotKeyKeys, UIFormKeys(inKey, inModifier1, inModifier2, inModifier3), (intptr_t)inGroup);
}

// Deletes the hot key at an index in a group
void UIForm::RemoveHotKey(unsigned long inIndex, unsigned long inGroup)
{
    this->Call(EeUiFormCall::RemoveHotKeyAt, (intptr_t)inIndex, (intptr_t)inGroup);
}

// Removes and deletes a background found by its ID (nothing happens when it is not listed)
void UIForm::RemoveBackground(UIBackground* inBackground)
{
    this->Call(EeUiFormCall::RemoveBackground, (intptr_t)inBackground);
}

// (exported 0x100596d9) Empties the control's form slot and takes it out of the group
void UIForm::RemoveControl(UIControl* inControl, unsigned long inGroup)
{
    this->Call(EeUiFormCall::RemoveControl, (intptr_t)inControl, (intptr_t)inGroup);
}

// Marks dirty every visible control of an enabled group that overlaps the rectangle
void UIForm::RedrawIntersectControls(const GERectangle& inRectangle)
{
    float f[4];
    UIFormPutRect(f, inRectangle);
    this->Call(EeUiFormCall::RedrawIntersectControls, 0, 0, f);
}

// Lays the form out again inside a new rectangle, with its backgrounds and database controls
void UIForm::RelayoutForm(const GERectangle& inRectangle)
{
    float f[4];
    UIFormPutRect(f, inRectangle);
    this->Call(EeUiFormCall::RelayoutForm, 0, 0, f);
}

// Marks every control of the form dirty
void UIForm::SetAllControlsDirty() { this->Call(EeUiFormCall::SetAllControlsDirty); }

// one (the event's index in the Rust form: formobj.rs EV_*)
void UIForm::SetFormOpenEvent(UIEvent* inEvent) { this->Call(EeUiFormCall::SetEvent, 0, (intptr_t)inEvent); }
void UIForm::SetFormCloseEvent(UIEvent* inEvent) { this->Call(EeUiFormCall::SetEvent, 1, (intptr_t)inEvent); }
void UIForm::SetMouseMiddleButtonPressedEvent(UIEvent* inEvent) { this->Call(EeUiFormCall::SetEvent, 5, (intptr_t)inEvent); }
void UIForm::SetMouseMiddleButtonUnpressedEvent(UIEvent* inEvent) { this->Call(EeUiFormCall::SetEvent, 6, (intptr_t)inEvent); }
void UIForm::SetMouseMoveEvent(UIEvent* inEvent) { this->Call(EeUiFormCall::SetEvent, 11, (intptr_t)inEvent); }
void UIForm::SetMouseWheelMoveEvent(UIEvent* inEvent) { this->Call(EeUiFormCall::SetEvent, 12, (intptr_t)inEvent); }
void UIForm::SetFormUpdateEvent(UIEvent* inEvent) { this->Call(EeUiFormCall::SetEvent, 13, (intptr_t)inEvent); }
void UIForm::SetFormActivateEvent(UIEvent* inEvent) { this->Call(EeUiFormCall::SetEvent, 14, (intptr_t)inEvent); }
void UIForm::SetFormDeactivateEvent(UIEvent* inEvent) { this->Call(EeUiFormCall::SetEvent, 15, (intptr_t)inEvent); }
void UIForm::SetFormKeyboardCapturedEvent(UIEvent* inEvent) { this->Call(EeUiFormCall::SetEvent, 16, (intptr_t)inEvent); }

// Sets the form event of the given type (open, close, mouse buttons, move, wheel); other types are ignored
void UIForm::SetFormEvent(unsigned long inType, UIEvent* inEvent)
{
    this->Call(EeUiFormCall::SetFormEvent, (intptr_t)inType, (intptr_t)inEvent);
}

// Replaces the form manager, deleting the old one
void UIForm::SetFormManager(UIFormManager* inFormManager)
{
    this->Call(EeUiFormCall::SetFormManager, (intptr_t)inFormManager);
}

// Enables or disables every hot key of a group
void UIForm::SetHotKeyGroupEnabled(unsigned long inGroup, bool inEnabled)
{
    this->Call(EeUiFormCall::SetHotKeyGroupEnabled, (intptr_t)inGroup, inEnabled);
}

// Gives the keyboard to a control and fires the keyboard-captured event
void UIForm::SetKeyboardCaptured(UIControl* inControl)
{
    this->Call(EeUiFormCall::SetKeyboardCaptured, (intptr_t)inControl);
}

void UIForm::SetMouseCaptured(UIControl* inControl) { this->Call(EeUiFormCall::SetMouseCaptured, (intptr_t)inControl); }

// Changes the control under the mouse, firing mouse-out/over triggers, and restarts the help-text timer with its help
void UIForm::SetMouseOverControl(UIControl* inControl)
{
    this->Call(EeUiFormCall::SetMouseOverControl, (intptr_t)inControl);
}

// Replaces the popup control, hiding the old one and redrawing what it (and a tool-tip shadow) covered
void UIForm::SetPopup(UIControl* inControl) { this->Call(EeUiFormCall::SetPopup, (intptr_t)inControl); }

// Turns keyboard tabbing on or off; turning it on moves the tab stop to the first tabbable control
void UIForm::SetTabbing(bool inTabbing) { this->Call(EeUiFormCall::SetTabbing, inTabbing); }

// Moves the tab stop to a control if it exists and is tabbable; otherwise keeps the old stop and returns false
bool UIForm::SetTabStop(unsigned long inTabStop) { return this->Call(EeUiFormCall::SetTabStop, (intptr_t)inTabStop) != 0; }

// Moves the tab stop back to the previous shown, enabled, tabbable control, skipping radio-button runs, wrapping round
void UIForm::ShiftTab() { this->Call(EeUiFormCall::ShiftTab); }

// Moves the tab stop on to the next shown, enabled, tabbable control, skipping radio-button runs, wrapping round
void UIForm::Tab() { this->Call(EeUiFormCall::Tab); }

// Unloads every background's texture
void UIForm::UnloadBackgrounds() { this->Call(EeUiFormCall::UnloadBackgrounds); }

// Refreshes the help text from the control under the mouse, re-placing a tool tip that sizes itself to its text
void UIForm::UpdateHelpString() { this->Call(EeUiFormCall::UpdateHelpString); }

// Places the tool tip by the mouse inside the form: shifted left at the right edge, above the cursor near the bottom
void UIForm::SetHelpTextPopupLocation() { this->Call(EeUiFormCall::SetHelpTextPopupLocation); }

// Shows the help text once its delay has passed (or when forced), as a popup tool tip when the form uses tool tips
void UIForm::UpdateHelpText(bool inForce) { this->Call(EeUiFormCall::UpdateHelpText, inForce); }

// Fires hot keys (higher groups first; one non-always-fire key per frame), then Tab/Shift+Tab, else passes the
// keyboard to the capturing control or the tab stop
long UIForm::UpdateKeyboard() { return (long)this->Call(EeUiFormCall::UpdateKeyboard); }

// Passes the mouse to the captured or mouse-over control (wheel included), then fires the form's own mouse events
long UIForm::UpdateMouse() { return (long)this->Call(EeUiFormCall::UpdateMouse); }

// Records the mouse location, finds the control under it, hides a tool tip the mouse enters, updates the mouse-over
void UIForm::UpdateMouseLocation(const GE2DPoint& inLocation)
{
    float f[4] = {inLocation.GetX(), inLocation.GetY(), 0.0f, 0.0f};
    this->Call(EeUiFormCall::UpdateMouseLocation, 0, 0, f);
}

// Passes a typed character to the control holding the keyboard
long UIForm::UpdateTranslatedKey(const char* inKey, short inRepeat)
{
    UIControl* theControl = this->GetKeyboardCaptured();
    return theControl ? theControl->UpdateTranslatedKey(inKey, inRepeat) : kErrorOK;
}

// The header's members (inline in the original, emitted first by UIBackground.cpp 0x10059559..0x10059795)
void UIForm::SetRectangle(const GERectangle& inRectangle)
{
    float f[4];
    UIFormPutRect(f, inRectangle);
    this->Call(EeUiFormCall::SetRectangle, 0, 0, f);
}
UIControl* UIForm::GetControl(unsigned long inIndex) const
{
    return reinterpret_cast<UIControl*>(this->Call(EeUiFormCall::Control, (intptr_t)inIndex));
}
unsigned long UIForm::GetControlCount() const { return (unsigned long)this->Call(EeUiFormCall::ControlCount); }
unsigned long UIForm::GetControlGroupCount() const { return (unsigned long)this->Call(EeUiFormCall::ControlGroupCount); }
unsigned long UIForm::GetDesignedHeight() const { return (unsigned long)this->Call(EeUiFormCall::DesignedHeight); }
unsigned long UIForm::GetDesignedWidth() const { return (unsigned long)this->Call(EeUiFormCall::DesignedWidth); }
UIEntityGroup* UIForm::GetEntityGroup(unsigned long inIndex) const
{
    return reinterpret_cast<UIEntityGroup*>(this->Call(EeUiFormCall::EntityGroup, (intptr_t)inIndex));
}
float UIForm::GetFadeOpacity()
{
    float f[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    this->Call(EeUiFormCall::FadeOpacity, 0, 0, f);
    return f[0];
}
UIEvent* UIForm::GetFormUpdateEvent() const { return reinterpret_cast<UIEvent*>(this->Call(EeUiFormCall::Event, 13)); }
UIEvent* UIForm::GetMouseMoveEvent() const { return reinterpret_cast<UIEvent*>(this->Call(EeUiFormCall::Event, 11)); }
unsigned long UIForm::GetFormID() const { return (unsigned long)this->Call(EeUiFormCall::FormId); }
UIFormManager* UIForm::GetFormManager() const { return reinterpret_cast<UIFormManager*>(this->Call(EeUiFormCall::FormManager)); }
UITextLabel* UIForm::GetHelpTextLabel() const
{
    return static_cast<UITextLabel*>(reinterpret_cast<UIControl*>(this->Call(EeUiFormCall::HelpTextLabel)));
}
bool UIForm::GetUseExclusiveCoords() const { return this->Call(EeUiFormCall::UseExclusiveCoords) != 0; }
UIManager* UIForm::GetManager() const { return reinterpret_cast<UIManager*>(this->Call(EeUiFormCall::Manager)); }
const GE2DPoint& UIForm::GetMouseLocation() const
{
    return *reinterpret_cast<const GE2DPoint*>(this->Call(EeUiFormCall::MouseLocation));
}
UIControl* UIForm::GetMouseOverControl() const { return reinterpret_cast<UIControl*>(this->Call(EeUiFormCall::MouseOverControl)); }
UWideString UIForm::GetMusicFileName() const
{
    return UWideString(reinterpret_cast<const char*>(this->Call(EeUiFormCall::MusicFileName)));
}
const GERectangle& UIForm::GetRectangle() const
{
    return *reinterpret_cast<const GERectangle*>(this->Call(EeUiFormCall::Rectangle));
}
bool UIForm::GetTabbingOn() const { return this->Call(EeUiFormCall::TabbingOn) != 0; }
unsigned long UIForm::GetTabStop() const { return (unsigned long)this->Call(EeUiFormCall::TabStop); }
bool UIForm::GetUseGDIForText() const { return this->Call(EeUiFormCall::UseGdiForText) != 0; }
void UIForm::LockDrawBackground() { this->Call(EeUiFormCall::LockDraw, 1); }
void UIForm::UnlockDrawBackground() { this->Call(EeUiFormCall::LockDraw, 0); }
void UIForm::SetHelpTextLabel(UITextLabel* inLabel)
{
    this->Call(EeUiFormCall::SetHelpTextLabel, (intptr_t) static_cast<UIControl*>(inLabel));
}
void UIForm::SetHelpTextTimeDelay(unsigned long inTimeDelay) { this->Call(EeUiFormCall::SetHelpTextTimeDelay, (intptr_t)inTimeDelay); }
void UIForm::SetLayout(UILayoutUtility& inLayout)
{
    unsigned long theTypes[4] = {inLayout.GetLeftOffsetType(), inLayout.GetRightOffsetType(), inLayout.GetTopOffsetType(),
                                 inLayout.GetBottomOffsetType()};
    float f[4] = {inLayout.GetLeftOffset(), inLayout.GetRightOffset(), inLayout.GetTopOffset(), inLayout.GetBottomOffset()};
    uint64_t thePacked = 0;
    for (int i = 0; i < 4; i++)
        thePacked |= (uint64_t)(theTypes[i] & 0xffff) << (16 * i);
    this->Call(EeUiFormCall::SetLayout, (intptr_t)thePacked, 0, f);
}
void UIForm::SetMusicFileName(const UWideString& inName) { this->Call(EeUiFormCall::SetMusicFileName, (intptr_t)(const char*)inName); }
void UIForm::SetUseToolTips(bool inUseToolTips) { this->Call(EeUiFormCall::SetUseToolTips, inUseToolTips); }
bool UIForm::GetDrawToolTipShadow() const { return this->Call(EeUiFormCall::DrawToolTipShadow) != 0; }
void UIForm::SetDrawToolTipShadow(bool inDraw) { this->Call(EeUiFormCall::SetDrawToolTipShadow, inDraw); }
UIControl* UIForm::GetKeyboardCaptured() const { return reinterpret_cast<UIControl*>(this->Call(EeUiFormCall::KeyboardCaptured)); }
UIControl* UIForm::GetMouseCaptured() const { return reinterpret_cast<UIControl*>(this->Call(EeUiFormCall::MouseCaptured)); }

// port: see the header
void UIForm::ReserveControlIndex(unsigned long inIndex) { this->Call(EeUiFormCall::ReserveControlIndex, (intptr_t)inIndex); }
