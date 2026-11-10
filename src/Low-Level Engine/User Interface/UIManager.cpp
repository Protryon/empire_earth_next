// C:\EEX\Low-Level Engine\User Interface\UIManager.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10078c81..0x1007ba37: the U2D key init chains, the implicit copy members and ??_E of UIStretchedTiledBackground
// and UITBTiledBackground (UIManager.cpp is their first includer), the ctor, the dtor and the out-of-line members in
// the original's order, then the vector/deque COMDATs. UIMovie.cpp starts at 0x1007ba37.
#define LLE_UIMANAGER_CPP   // UIManager::sMouseWheel/sMouseLeftHanded are visible here (see UIManager.h)
#include <windows.h>
#include <mbstring.h>
#include <algorithm>

#include "rust/ee_rust.h"
#include "lle/LowLevelEngine.h"
#include "EEDataText.h"   // EEDbFindFile: the db tables' text forms
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
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIBackgroundTable.h"
#include "lle/UITextureBackground.h"
#include "lle/UIBFull.h"
#include "lle/UIBitmapBackground.h"
#include "lle/UITiledBackground.h"
#include "lle/UILRTiledBackground.h"
#include "lle/UITBTiledBackground.h"
#include "lle/UIStretchedTiledBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIControlsTable.h"
#include "lle/UIManager.h"

ISEntity* UIManager::sLMouseButton;
ISEntity* UIManager::sRMouseButton;
ISEntity* UIManager::sMouseWheel;
bool UIManager::sMouseLeftHanded;

// The Rust form stack's view of the manager (rust/crates/ee_ui/src/manager.rs, docs/RUST_PLAN.md R6): its lists a
// slot at a time, the forms as opaque handles, the calls on a form. A friend of UIManager and UIForm.
struct UIManagerRust {
    // 0 the stack, 1 the visible forms, 2 every form
    static std::vector<UIForm*>& List(void* inManager, int inList)
    {
        UIManager* theManager = static_cast<UIManager*>(inManager);
        switch (inList) {
        case 0:
            return theManager->mFormStack;
        case 1:
            return theManager->mVisibleForms;
        default:
            return theManager->mForms;
        }
    }
    static UIManager* Manager(void* inManager) { return static_cast<UIManager*>(inManager); }
    static std::vector<UIForm*>& Copied(void* inManager) { return Manager(inManager)->mCopiedForms; }
    static std::deque<UIEventTrigger>& Events(void* inManager) { return Manager(inManager)->mEventsToTrigger; }
    static UIForm* Form(void* inForm) { return static_cast<UIForm*>(inForm); }
    static EeUiRect Rect(const GERectangle& inRectangle)
    {
        return EeUiRect{inRectangle.mBottom, inRectangle.mLeft, inRectangle.mRight, inRectangle.mTop};
    }
    static GERectangle Rect(const EeUiRect& inRectangle)
    {
        return GERectangle(inRectangle.left, inRectangle.top, inRectangle.right, inRectangle.bottom);
    }
    static void FormCall(UIForm* inForm, int inCall)
    {
        switch (inCall) {
        case 0:
            inForm->Open();
            break;
        case 1:
            inForm->Close();
            break;
        case 2:
            inForm->Activate();
            break;
        case 3:
            inForm->Deactivate();
            break;
        case 4:
            inForm->ClearAllClipRectangles();
            break;
        default:
            inForm->SetAllControlsDirty();
            break;
        }
    }
    static void FormRectCall(UIForm* inForm, int inCall, const GERectangle& inRectangle)
    {
        switch (inCall) {
        case 0:
            inForm->AddDirtyRectangle(inRectangle);
            break;
        case 1:
            inForm->RedrawIntersectControls(inRectangle);
            break;
        case 2:
            inForm->AddClipRectangle(inRectangle);
            break;
        default:
            inForm->RelayoutForm(inRectangle);
            break;
        }
    }
};

extern "C" unsigned long ee_uicpp_manager_len(void* inManager, int inList)
{
    return (unsigned long)UIManagerRust::List(inManager, inList).size();
}
extern "C" void* ee_uicpp_manager_at(void* inManager, int inList, unsigned long inIndex)
{
    return UIManagerRust::List(inManager, inList)[inIndex];
}
extern "C" void ee_uicpp_manager_push(void* inManager, int inList, void* inForm)
{
    UIManagerRust::List(inManager, inList).push_back(UIManagerRust::Form(inForm));
}
extern "C" void ee_uicpp_manager_pop(void* inManager, int inList)
{
    UIManagerRust::List(inManager, inList).pop_back();
}
extern "C" void ee_uicpp_manager_insert(void* inManager, int inList, unsigned long inIndex, void* inForm)
{
    std::vector<UIForm*>& theList = UIManagerRust::List(inManager, inList);
    theList.insert(theList.begin() + inIndex, UIManagerRust::Form(inForm));
}
extern "C" void ee_uicpp_manager_erase(void* inManager, int inList, unsigned long inIndex)
{
    std::vector<UIForm*>& theList = UIManagerRust::List(inManager, inList);
    theList.erase(theList.begin() + inIndex);
}
extern "C" void ee_uicpp_manager_copied_clear(void* inManager)
{
    UIManagerRust::Copied(inManager).clear();
}
extern "C" EeUiRect ee_uicpp_manager_form_rect(void* inForm)
{
    return UIManagerRust::Rect(UIManagerRust::Form(inForm)->GetRectangle());
}
extern "C" unsigned long ee_uicpp_manager_form_id(void* inForm)
{
    return UIManagerRust::Form(inForm)->GetFormID();
}
extern "C" void ee_uicpp_manager_form_call(void* inForm, int inCall)
{
    UIManagerRust::FormCall(UIManagerRust::Form(inForm), inCall);
}
extern "C" void ee_uicpp_manager_form_rect_call(void* inForm, int inCall, const EeUiRect* inRectangle)
{
    UIManagerRust::FormRectCall(UIManagerRust::Form(inForm), inCall, UIManagerRust::Rect(*inRectangle));
}
extern "C" bool ee_uicpp_manager_event_take(void* inManager, EeUiTrigger* outTrigger)
{
    std::deque<UIEventTrigger>& theQueue = UIManagerRust::Events(inManager);
    if (theQueue.empty())
        return false;
    UIEventTrigger theTrigger = theQueue.front();
    theQueue.pop_front();
    outTrigger->event = theTrigger.mEvent;
    outTrigger->form = theTrigger.mForm;
    outTrigger->entity = theTrigger.mEntity;
    return true;
}
extern "C" void ee_uicpp_manager_event_process(const EeUiTrigger* inTrigger)
{
    UIEvent* theEvent = static_cast<UIEvent*>(inTrigger->event);
    theEvent->ProcessEvent(static_cast<UIForm*>(inTrigger->form), static_cast<UIEntity*>(inTrigger->entity));
    theEvent->Dereference();
}
extern "C" void ee_uicpp_manager_event_push(void* inManager, const EeUiTrigger* inTrigger)
{
    UIEventTrigger theTrigger;
    theTrigger.mEvent = static_cast<UIEvent*>(inTrigger->event);
    theTrigger.mForm = static_cast<UIForm*>(inTrigger->form);
    theTrigger.mEntity = static_cast<UIEntity*>(inTrigger->entity);
    UIManagerRust::Events(inManager).push_back(theTrigger);
}

// FUNCTION: LLE 0x10078ee4
// Creates the UI manager: its locks, the UIControls texture and the check box, radio, sort and scroll-bar
// bitmaps cut from it; looks up the shared mouse buttons and wheel on first use; no music playing
UIManager::UIManager(GERasterizer* inRasterizer, SESoundManager* inSoundManager, unsigned long inNumberOfSurfaces,
                     unsigned long inUITextureBitDepth, UResourceFile* inStringTable, unsigned long inScreen)
{
    FSFileSpec theDirectory(gStartupVolume + gStartupDirectory);
    this->mRasterizer = inRasterizer;
    this->mScreen = inScreen;
    this->mNumberOfSurfaces = inNumberOfSurfaces;
    this->mUITextureBitDepth = inUITextureBitDepth;
    this->mStringTable = inStringTable;
    this->mSoundManager = inSoundManager;
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mCriticalSection) == keseOK, 81);
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mDrawCriticalSection) == keseOK, 82);
    this->mRasterizer->GetScreen(inScreen);
    this->LoadTexture(UString("textures\\UIControls"), this->mUITexture);
    float theWidth = (float)this->mUITexture->GetWidth();
    float theHeight = (float)this->mUITexture->GetHeight();
    this->mDragBarBitmap = new GEBitmap;
    this->mDragBarBitmap->SetTexture(this->mUITexture, inScreen);
    this->CreateBitmap(this->mDragBarBitmap, GETextureCoordinate(80.0f / theWidth, 40.0f / theHeight),
                 GETextureCoordinate(100.0f / theWidth, 40.0f / theHeight),
                 GETextureCoordinate(80.0f / theWidth, 60.0f / theHeight),
                 GETextureCoordinate(100.0f / theWidth, 60.0f / theHeight));
    this->mCheckOn = new GEBitmap;
    this->mCheckOn->SetTexture(this->mUITexture, inScreen);
    this->CreateBitmap(this->mCheckOn, GETextureCoordinate(0.0f / theWidth, 0.0f / theHeight),
                 GETextureCoordinate(20.0f / theWidth, 0.0f / theHeight),
                 GETextureCoordinate(0.0f / theWidth, 20.0f / theHeight),
                 GETextureCoordinate(20.0f / theWidth, 20.0f / theHeight));
    this->mCheckDownOn = new GEBitmap;
    this->mCheckDownOn->SetTexture(this->mUITexture, inScreen);
    this->CreateBitmap(this->mCheckDownOn, GETextureCoordinate(20.0f / theWidth, 0.0f / theHeight),
                 GETextureCoordinate(40.0f / theWidth, 0.0f / theHeight),
                 GETextureCoordinate(20.0f / theWidth, 20.0f / theHeight),
                 GETextureCoordinate(40.0f / theWidth, 20.0f / theHeight));
    this->mCheckNot = new GEBitmap;
    this->mCheckNot->SetTexture(this->mUITexture, inScreen);
    this->CreateBitmap(this->mCheckNot, GETextureCoordinate(0.0f / theWidth, 60.0f / theHeight),
                 GETextureCoordinate(20.0f / theWidth, 60.0f / theHeight),
                 GETextureCoordinate(0.0f / theWidth, 80.0f / theHeight),
                 GETextureCoordinate(20.0f / theWidth, 80.0f / theHeight));
    this->mCheckDownNot = new GEBitmap;
    this->mCheckDownNot->SetTexture(this->mUITexture, inScreen);
    this->CreateBitmap(this->mCheckDownNot, GETextureCoordinate(20.0f / theWidth, 60.0f / theHeight),
                 GETextureCoordinate(40.0f / theWidth, 60.0f / theHeight),
                 GETextureCoordinate(20.0f / theWidth, 80.0f / theHeight),
                 GETextureCoordinate(40.0f / theWidth, 80.0f / theHeight));
    this->mAlphaSortUpBitmap = new GEBitmap;
    this->mAlphaSortUpBitmap->SetTexture(this->mUITexture, inScreen);
    this->CreateBitmap(this->mAlphaSortUpBitmap, GETextureCoordinate(40.0f / theWidth, 60.0f / theHeight),
                 GETextureCoordinate(60.0f / theWidth, 60.0f / theHeight),
                 GETextureCoordinate(40.0f / theWidth, 80.0f / theHeight),
                 GETextureCoordinate(60.0f / theWidth, 80.0f / theHeight));
    this->mAlphaSortDownBitmap = new GEBitmap;
    this->mAlphaSortDownBitmap->SetTexture(this->mUITexture, inScreen);
    this->CreateBitmap(this->mAlphaSortDownBitmap, GETextureCoordinate(60.0f / theWidth, 60.0f / theHeight),
                 GETextureCoordinate(80.0f / theWidth, 60.0f / theHeight),
                 GETextureCoordinate(60.0f / theWidth, 80.0f / theHeight),
                 GETextureCoordinate(80.0f / theWidth, 80.0f / theHeight));
    this->mCheckOff = new GEBitmap;
    this->mCheckOff->SetTexture(this->mUITexture, inScreen);
    this->CreateBitmap(this->mCheckOff, GETextureCoordinate(40.0f / theWidth, 0.0f / theHeight),
                 GETextureCoordinate(60.0f / theWidth, 0.0f / theHeight),
                 GETextureCoordinate(40.0f / theWidth, 20.0f / theHeight),
                 GETextureCoordinate(60.0f / theWidth, 20.0f / theHeight));
    this->mCheckDownOff = new GEBitmap;
    this->mCheckDownOff->SetTexture(this->mUITexture, inScreen);
    this->CreateBitmap(this->mCheckDownOff, GETextureCoordinate(60.0f / theWidth, 0.0f / theHeight),
                 GETextureCoordinate(80.0f / theWidth, 0.0f / theHeight),
                 GETextureCoordinate(60.0f / theWidth, 20.0f / theHeight),
                 GETextureCoordinate(80.0f / theWidth, 20.0f / theHeight));
    this->mRadioOn = new GEBitmap;
    this->mRadioOn->SetTexture(this->mUITexture, inScreen);
    this->CreateBitmap(this->mRadioOn, GETextureCoordinate(80.0f / theWidth, 0.0f / theHeight),
                 GETextureCoordinate(100.0f / theWidth, 0.0f / theHeight),
                 GETextureCoordinate(80.0f / theWidth, 20.0f / theHeight),
                 GETextureCoordinate(100.0f / theWidth, 20.0f / theHeight));
    this->mRadioDownOn = new GEBitmap;
    this->mRadioDownOn->SetTexture(this->mUITexture, inScreen);
    this->CreateBitmap(this->mRadioDownOn, GETextureCoordinate(100.0f / theWidth, 0.0f / theHeight),
                 GETextureCoordinate(120.0f / theWidth, 0.0f / theHeight),
                 GETextureCoordinate(100.0f / theWidth, 20.0f / theHeight),
                 GETextureCoordinate(120.0f / theWidth, 20.0f / theHeight));
    this->mRadioOff = new GEBitmap;
    this->mRadioOff->SetTexture(this->mUITexture, inScreen);
    this->CreateBitmap(this->mRadioOff, GETextureCoordinate(0.0f / theWidth, 20.0f / theHeight),
                 GETextureCoordinate(20.0f / theWidth, 20.0f / theHeight),
                 GETextureCoordinate(0.0f / theWidth, 40.0f / theHeight),
                 GETextureCoordinate(20.0f / theWidth, 40.0f / theHeight));
    this->mRadioDownOff = new GEBitmap;
    this->mRadioDownOff->SetTexture(this->mUITexture, inScreen);
    this->CreateBitmap(this->mRadioDownOff, GETextureCoordinate(20.0f / theWidth, 20.0f / theHeight),
                 GETextureCoordinate(40.0f / theWidth, 20.0f / theHeight),
                 GETextureCoordinate(20.0f / theWidth, 40.0f / theHeight),
                 GETextureCoordinate(40.0f / theWidth, 40.0f / theHeight));
    this->mSBBitmapUpNormal = new GEBitmap;
    this->mSBBitmapUpNormal->SetTexture(this->mUITexture, inScreen);
    this->CreateBitmap(this->mSBBitmapUpNormal, GETextureCoordinate(40.0f / theWidth, 20.0f / theHeight),
                 GETextureCoordinate(60.0f / theWidth, 20.0f / theHeight),
                 GETextureCoordinate(40.0f / theWidth, 40.0f / theHeight),
                 GETextureCoordinate(60.0f / theWidth, 40.0f / theHeight));
    this->mSBBitmapUpPressed = new GEBitmap;
    this->mSBBitmapUpPressed->SetTexture(this->mUITexture, inScreen);
    this->CreateBitmap(this->mSBBitmapUpPressed, GETextureCoordinate(60.0f / theWidth, 20.0f / theHeight),
                 GETextureCoordinate(80.0f / theWidth, 20.0f / theHeight),
                 GETextureCoordinate(60.0f / theWidth, 40.0f / theHeight),
                 GETextureCoordinate(80.0f / theWidth, 40.0f / theHeight));
    this->mSBBitmapDownPressed = new GEBitmap;
    this->mSBBitmapDownPressed->SetTexture(this->mUITexture, inScreen);
    this->CreateBitmap(this->mSBBitmapDownPressed, GETextureCoordinate(80.0f / theWidth, 20.0f / theHeight),
                 GETextureCoordinate(100.0f / theWidth, 20.0f / theHeight),
                 GETextureCoordinate(80.0f / theWidth, 40.0f / theHeight),
                 GETextureCoordinate(100.0f / theWidth, 40.0f / theHeight));
    this->mSBBitmapDownNormal = new GEBitmap;
    this->mSBBitmapDownNormal->SetTexture(this->mUITexture, inScreen);
    this->CreateBitmap(this->mSBBitmapDownNormal, GETextureCoordinate(100.0f / theWidth, 20.0f / theHeight),
                 GETextureCoordinate(120.0f / theWidth, 20.0f / theHeight),
                 GETextureCoordinate(100.0f / theWidth, 40.0f / theHeight),
                 GETextureCoordinate(120.0f / theWidth, 40.0f / theHeight));
    this->mSBBitmapRightNormal = new GEBitmap;
    this->mSBBitmapRightNormal->SetTexture(this->mUITexture, inScreen);
    this->CreateBitmap(this->mSBBitmapRightNormal, GETextureCoordinate(0.0f / theWidth, 40.0f / theHeight),
                 GETextureCoordinate(20.0f / theWidth, 40.0f / theHeight),
                 GETextureCoordinate(0.0f / theWidth, 60.0f / theHeight),
                 GETextureCoordinate(20.0f / theWidth, 60.0f / theHeight));
    this->mSBBitmapRightPressed = new GEBitmap;
    this->mSBBitmapRightPressed->SetTexture(this->mUITexture, inScreen);
    this->CreateBitmap(this->mSBBitmapRightPressed, GETextureCoordinate(20.0f / theWidth, 40.0f / theHeight),
                 GETextureCoordinate(40.0f / theWidth, 40.0f / theHeight),
                 GETextureCoordinate(20.0f / theWidth, 60.0f / theHeight),
                 GETextureCoordinate(40.0f / theWidth, 60.0f / theHeight));
    this->mSBBitmapLeftPressed = new GEBitmap;
    this->mSBBitmapLeftPressed->SetTexture(this->mUITexture, inScreen);
    this->CreateBitmap(this->mSBBitmapLeftPressed, GETextureCoordinate(40.0f / theWidth, 40.0f / theHeight),
                 GETextureCoordinate(60.0f / theWidth, 40.0f / theHeight),
                 GETextureCoordinate(40.0f / theWidth, 60.0f / theHeight),
                 GETextureCoordinate(60.0f / theWidth, 60.0f / theHeight));
    this->mSBBitmapLeftNormal = new GEBitmap;
    this->mSBBitmapLeftNormal->SetTexture(this->mUITexture, inScreen);
    this->CreateBitmap(this->mSBBitmapLeftNormal, GETextureCoordinate(60.0f / theWidth, 40.0f / theHeight),
                 GETextureCoordinate(80.0f / theWidth, 40.0f / theHeight),
                 GETextureCoordinate(60.0f / theWidth, 60.0f / theHeight),
                 GETextureCoordinate(80.0f / theWidth, 60.0f / theHeight));
    if (UIManager::sLMouseButton == NULL) {
        gISMouse->GetEntity(kMouseLeftButton, UIManager::sLMouseButton);
        gISMouse->GetEntity(kMouseRightButton, UIManager::sRMouseButton);
        gISMouse->GetEntity(kMouseZAxis, UIManager::sMouseWheel);
    }
    this->mCandidateWindowOpen = false;
    this->mMusicVolume = 1.0f;
    this->mDrawCandidateWindow = false;
    this->mMusicID = -1;
    this->mStartMusic = false;
    this->mStopMusic = false;
    this->mForceUIUpdate = false;
    this->mCandidateWindow = NULL;
    this->mMusicInstance = NULL;
}

// FUNCTION: LLE 0x10079a18
// Destroys the UI manager: stops the music, frees the locks, texture, bitmaps, forms, hot keys, IME window and database
UIManager::~UIManager()
{
    this->RemoveAndStopSoundInstance();
    TSScheduler::DestroyCriticalSection(this->mCriticalSection);
    TSScheduler::DestroyCriticalSection(this->mDrawCriticalSection);
    this->mUITexture->Dereference();
    this->mSBBitmapDownNormal->Dereference();
    this->mSBBitmapDownPressed->Dereference();
    this->mSBBitmapLeftNormal->Dereference();
    this->mSBBitmapLeftPressed->Dereference();
    this->mSBBitmapRightNormal->Dereference();
    this->mSBBitmapRightPressed->Dereference();
    this->mSBBitmapUpNormal->Dereference();
    this->mSBBitmapUpPressed->Dereference();
    this->mDragBarBitmap->Dereference();
    this->mCheckOff->Dereference();
    this->mCheckDownOff->Dereference();
    this->mCheckNot->Dereference();
    this->mCheckDownNot->Dereference();
    this->mCheckOn->Dereference();
    this->mCheckDownOn->Dereference();
    this->mRadioOff->Dereference();
    this->mRadioDownOff->Dereference();
    this->mRadioOn->Dereference();
    this->mRadioDownOn->Dereference();
    this->mAlphaSortUpBitmap->Dereference();
    this->mAlphaSortDownBitmap->Dereference();
    for (unsigned long i = 0; i < this->mForms.size(); i++) {
        delete this->mForms[i];
        this->mForms[i] = NULL;
    }
    UIHotKey::DeleteHotKeys();
    this->mForms.clear();
    this->mVisibleForms.clear();
    delete this->mCandidateWindow;
    this->RemoveDatabase();
}

// FUNCTION: LLE 0x10079be5 (AddClippedForms) and 0x10079e1f (AddToVisibleForms): the bodies are Rust (ee_ui
// manager.rs, inside Pop, SwapForm and Push); the members stay declared, with no C++ definition.

// FUNCTION: LLE 0x10079f7f
// Marks a rectangle dirty: on the top form directly, else on that form and every visible form above that overlaps it
void UIManager::AddDirtyRectangle(const GERectangle& inRectangle, UIForm* inForm)
{
    EeUiRect theRectangle = UIManagerRust::Rect(inRectangle);
    ee_ui_manager_add_dirty_rectangle(this, &theRectangle, inForm);
}

// FUNCTION: LLE 0x1007a03d
// Passes an IME result string to the top form, under the UI lock
void UIManager::AddIMEResultString(const UWideString& inString)
{
    this->mCriticalSection->Enter();
    if (this->mFormStack.size() > 0)
        this->mFormStack.back()->AddIMEResultString(inString);
    this->mCriticalSection->Leave();
}

// FUNCTION: LLE 0x1007a074
// Updates the IME candidate list of the focused IME-enabled control: hides the window for a style-2 list of at
// most one candidate, else shows it at the control; returns OK
long UIManager::ChangeIMECandidateWindow(const UIIMECandidateList& inList)
{
    this->mCriticalSection->Enter();
    if (this->GetCurrentForm() && this->GetCurrentForm()->GetKeyboardCaptured() &&
        this->GetCurrentForm()->GetKeyboardCaptured()->GetIMEEnabled() == true) {
        if ((inList.mStyle & 2) && inList.mCount <= 1)
            this->mCandidateWindowOpen = false;
        else {
            this->mCandidateWindowOpen = true;
            this->mCandidateWindow->SetCandidateList(inList,
                                               this->GetCurrentForm()->GetKeyboardCaptured()->GetRectangle().GetLeft());
        }
    }
    this->mCriticalSection->Leave();
    return kErrorOK;
}

// FUNCTION: LLE 0x1007a102
// Passes the IME composition string, attributes and cursor to the top form under the UI lock; returns its result
long UIManager::ChangeIMECompositionString(const UWideString& inString, const UString& inAttributes,
                                           unsigned long inCursor)
{
    long theResult = kErrorOK;
    this->mCriticalSection->Enter();
    if (this->mFormStack.size() > 0)
        theResult = this->mFormStack.back()->ChangeIMECompositionString(inString, inAttributes, inCursor);
    this->mCriticalSection->Leave();
    return theResult;
}

// FUNCTION: LLE 0x1007a149
// Closes the IME candidate window; returns OK
long UIManager::CloseIMECandidateWindow()
{
    this->mCandidateWindow->CloseCandidateWindow();
    this->mCandidateWindowOpen = false;
    return kErrorOK;
}

// FUNCTION: LLE 0x1007a162
// Tells the top form the IME composition string closed, under the UI lock; returns its result
long UIManager::CloseIMECompositionString()
{
    long theResult = kErrorOK;
    this->mCriticalSection->Enter();
    if (this->mFormStack.size() > 0)
        theResult = this->mFormStack.back()->CloseIMECompositionString();
    this->mCriticalSection->Leave();
    return theResult;
}

// FUNCTION: LLE 0x1007a19b
// Snapshots every visible form (and the candidate window when open) for drawing; returns the last copy result
long UIManager::CopyData()
{
    long theResult = kErrorOK;
    this->mCopiedForms.clear();
    for (long i = 0; i < (long)this->mVisibleForms.size(); i++) {
        theResult = this->mVisibleForms[i]->CopyData();
        this->mCopiedForms.push_back(this->mVisibleForms[i]);
    }
    this->mDrawCandidateWindow = this->mCandidateWindowOpen;
    if (this->mDrawCandidateWindow == true)
        theResult = this->mCandidateWindow->CopyData();
    return theResult;
}

// FUNCTION: LLE 0x1007a216
// Creates every background in the UI background database on its form, by type (others via CreateUserDefinedBackground)
// tiled backgrounds get the row's ID where every other type gets its background ID
void UIManager::CreateBackgrounds()
{
    GETextureDescription theDescription;
    theDescription.mPixelDepth = 16;
    theDescription.mNumMipLevelsToDrop = 0;
    theDescription.mSurfaceVolatility = 1;
    theDescription.mFrameFormat = 0;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mHasAlpha = true;
    theDescription.mCreateMIPMaps = true;
    unsigned long theCount = UIBackgroundTable::GetArraySize();
    theDescription.mPixelDepth = this->mUITextureBitDepth;
    for (unsigned long i = 0; i < theCount; i++) {
        UIBackgroundTable* theTable = UIBackgroundTable::GetUIBackgroundTable(i);
        if (!theTable)
            continue;
        theDescription.mHasAlpha = theTable->GetHasAlpha();
        UIForm* theForm = this->GetFormByID(theTable->GetFormID());
        if (!theForm)
            continue;   // no such form (GetFormByID used to return a garbage pointer here)
        switch (theTable->GetBackgroundType()) {
        case kBackgroundBitmap:
            new UIBitmapBackground(theTable->GetBackgroundID(), theTable->GetTextureFile(), theDescription,
                                   theTable->GetTextureRect(), theTable->GetLayout(), theForm,
                                   theTable->GetDrawPriority(), theTable->GetVisible());
            break;
        case kBackgroundTiled:
            // Fixed: passed the row ID (GetID) where every other case passes the background ID
            new UITiledBackground(theTable->GetBackgroundID(), theTable->GetTextureFile(), theDescription,
                                  theTable->GetTextureRect(), theTable->GetLayout(), theForm,
                                  theTable->GetDrawPriority(), theTable->GetVisible());
            break;
        case kBackgroundFull:
            new UIBFull(theTable->GetBackgroundID(), theTable->GetTextureFile(), theDescription,
                        theTable->GetTextureRect(), theTable->GetLayout(), theForm, theTable->GetDrawPriority(),
                        theTable->GetNumCols(), theTable->GetNumRows(), theTable->GetVisible());
            break;
        case kBackgroundLRTiled:
            new UILRTiledBackground(theTable->GetBackgroundID(), theTable->GetTextureFile(), theDescription,
                                    theTable->GetTextureRect(), theTable->GetLayout(), theForm,
                                    theTable->GetDrawPriority(), theTable->GetVisible());
            break;
        case kBackgroundTBTiled:
            new UITBTiledBackground(theTable->GetBackgroundID(), theTable->GetTextureFile(), theDescription,
                                    theTable->GetTextureRect(), theTable->GetLayout(), theForm,
                                    theTable->GetDrawPriority(), theTable->GetVisible());
            break;
        case kBackgroundStretchedTiled:
            new UIStretchedTiledBackground(theTable->GetBackgroundID(), theTable->GetTextureFile(), theDescription,
                                           theTable->GetTextureRect(), theTable->GetTileWidthPercent(),
                                           theTable->GetTileHeightPercent(), theTable->GetLayout(), theForm,
                                           theTable->GetDrawPriority(), theTable->GetVisible());
            break;
        default:
            this->CreateUserDefinedBackground(*theTable);
            break;
        }
    }
}

// FUNCTION: LLE 0x1007a438
// Points a bitmap at a texture region given by its four corner coordinates, opaque, scaled to the region
void UIManager::CreateBitmap(GEBitmap* inBitmap, GETextureCoordinate& inTopLeft, GETextureCoordinate& inTopRight,
                             GETextureCoordinate& inBottomLeft, GETextureCoordinate& inBottomRight)
{
    inBitmap->SetOpacity(1.0f);
    inBitmap->SetTextureCoordinates(inTopLeft, inTopRight, inBottomLeft, inBottomRight);
    inBitmap->SetXYScale(inTopRight.GetU() - inTopLeft.GetU(), inBottomLeft.GetV() - inTopLeft.GetV());
}

// FUNCTION: LLE 0x1007a48e
// Creates a copy of a bitmap's texture region scaled to fill a control rectangle (20-pixel source)
GEBitmap* UIManager::CreateBitmapForScaledControl(GEBitmap* inBitmap, const GERectangle& inRectangle)
{
    GETextureCoordinate theTopLeft;
    GETextureCoordinate theTopRight;
    GETextureCoordinate theBottomLeft;
    GETextureCoordinate theBottomRight;
    inBitmap->GetTextureCoordinatesPerVertex(theTopLeft, theTopRight, theBottomLeft, theBottomRight);
    GEBitmap* theBitmap = new GEBitmap;
    theBitmap->SetTexture(inBitmap->GetTexture(this->mScreen), this->mScreen);
    theBitmap->SetTextureCoordinates(theTopLeft, theTopRight, theBottomLeft, theBottomRight);
    theBitmap->SetXYScale(inRectangle.GetWidth() * inBitmap->GetXScale() * 0.05f,
                          inRectangle.GetHeight() * inBitmap->GetYScale() * 0.05f);
    return theBitmap;
}

// FUNCTION: LLE 0x1007a559
// Asks each registered control-event factory in turn for a form's event; returns the first one made, or NULL
UIEvent* UIManager::CreateControlEvent(UIForm* inForm, unsigned long inEventID)
{
    UIEvent* theEvent = NULL;
    for (unsigned long i = 0; i < mControlEventCallbacks.size() && theEvent == NULL; i++)
        theEvent = mControlEventCallbacks[i](inForm, inEventID);
    return theEvent;
}

// FUNCTION: LLE 0x1007a58f
// Asks each registered form-event factory in turn for an event; returns the first one made, or NULL
UIEvent* UIManager::CreateFormEvent(unsigned long inEventID)
{
    UIEvent* theEvent = NULL;
    for (unsigned long i = 0; i < mFormEventCallbacks.size() && theEvent == NULL; i++)
        theEvent = mFormEventCallbacks[i](this, inEventID);
    return theEvent;
}

// FUNCTION: LLE 0x1007a5c2
// Creates the IME candidate window with the given font
void UIManager::CreateIMECandidateWindow(GEFont* inFont)
{
    this->mCandidateWindow = new UIIMECandidate(this, 0.2f, 0.35f, inFont);
}

// FUNCTION: LLE 0x1007a602
// Draws the snapshotted forms with alpha blending (their text too, unless drawn by GDI) and the candidate window
long UIManager::DrawPolygons()
{
    unsigned long theSource;
    unsigned long theDestination;
    long theResult = kErrorOK;
    this->mRasterizer->GetBlending(theSource, theDestination, this->mScreen);
    this->mRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, this->mScreen);
    this->mDrawCriticalSection->Enter();
    for (unsigned long i = 0; i < this->mCopiedForms.size(); i++) {
        theResult = this->mCopiedForms[i]->DrawPolygons();
        if (!this->mCopiedForms[i]->GetUseGDIForText())
            theResult = this->mCopiedForms[i]->DrawTextA();
    }
    if (this->mDrawCandidateWindow == true)
        theResult = this->mCandidateWindow->DrawPolygons();
    this->mDrawCriticalSection->Leave();
    this->mRasterizer->SetBlending(theSource, theDestination, this->mScreen);
    return theResult;
}

// FUNCTION: LLE 0x1007a6be
// Draws the GDI text of the snapshotted forms and the candidate window, under the draw lock
long UIManager::DrawTextA()
{
    long theResult = kErrorOK;
    this->mDrawCriticalSection->Enter();
    if (this->mCopiedForms.size() > 0)
        for (unsigned long i = 0; i < this->mCopiedForms.size(); i++)
            if (this->mCopiedForms[i]->GetUseGDIForText() == true)
                theResult = this->mCopiedForms[i]->DrawTextA();
    if (this->mDrawCandidateWindow == true)
        theResult = this->mCandidateWindow->DrawTextA();
    this->mDrawCriticalSection->Leave();
    return theResult;
}

// FUNCTION: LLE 0x1007a72d
// Returns the form with the given ID (NULL: none)
UIForm* UIManager::GetFormByID(unsigned long inID) const
{
    // an unknown ID gives NULL (the original returned the word one past the end of the form list)
    return static_cast<UIForm*>(ee_ui_manager_form_by_id(const_cast<UIManager*>(this), inID));
}

// FUNCTION: LLE 0x1007a766
// Returns the mouse wheel's scroll step: -1 when rolled forward, 1 when rolled back, 0 when still
long UIManager::GetMouseWheelScrollAmount()
{
    return ee_ui_manager_wheel_scroll_amount(UIManager::sMouseWheel->GetNormalizedValue());
}

// FUNCTION: LLE 0x1007a78a
// Loads the UI database (fonts, forms, backgrounds, controls, control and form events, hot keys) from a directory
void UIManager::LoadDatabase(FSPath* inPath, const UString& inDirectory)
{
    FSFileSpec* theDbFile = NULL;
    long theResult;

    theResult = EEDbFindFile(inPath, inDirectory + gDbUIFontsFileName, theDbFile);
    ESVERIFY_AT(theResult == keseOK, 1081);
    ESVERIFY_AT(theDbFile != NULL, 1082);
    theResult = UIFontTable::Create(*theDbFile, this);
    ESVERIFY_AT(theResult == keseOK, 1084);
    delete theDbFile;

    theResult = EEDbFindFile(inPath, inDirectory + gDbUIFormsFileName, theDbFile);
    ESVERIFY_AT(theResult == keseOK, 1089);
    ESVERIFY_AT(theDbFile != NULL, 1090);
    theResult = UIForm::Create(*theDbFile, this);
    ESVERIFY_AT(theResult == keseOK, 1092);
    delete theDbFile;

    theResult = EEDbFindFile(inPath, inDirectory + gDbUIBackgroundFileName, theDbFile);
    ESVERIFY_AT(theResult == keseOK, 1097);
    ESVERIFY_AT(theDbFile != NULL, 1098);
    theResult = UIBackgroundTable::Create(*theDbFile);
    ESVERIFY_AT(theResult == keseOK, 1100);
    this->CreateBackgrounds();
    delete theDbFile;

    theResult = EEDbFindFile(inPath, inDirectory + gDbUIControlsFileName, theDbFile);
    ESVERIFY_AT(theResult == keseOK, 1106);
    ESVERIFY_AT(theDbFile != NULL, 1107);
    UIControlsTable::Create(*theDbFile, this);
    delete theDbFile;

    theResult = EEDbFindFile(inPath, inDirectory + gDbUIControlEventsFileName, theDbFile);
    ESVERIFY_AT(theResult == keseOK, 1113);
    ESVERIFY_AT(theDbFile != NULL, 1114);
    UIEvent::CreateControlEvents(*theDbFile);
    delete theDbFile;

    theResult = EEDbFindFile(inPath, inDirectory + gDbUIFormEventsFileName, theDbFile);
    ESVERIFY_AT(theResult == keseOK, 1120);
    ESVERIFY_AT(theDbFile != NULL, 1121);
    UIEvent::CreateFormEvents(*theDbFile, this);
    delete theDbFile;

    theResult = EEDbFindFile(inPath, inDirectory + gDbUIHotKeyFileName, theDbFile);
    ESVERIFY_AT(theResult == keseOK, 1127);
    ESVERIFY_AT(theDbFile != NULL, 1128);
    UIHotKey::CreateHotKeys(*theDbFile);
    delete theDbFile;
}

// FUNCTION: LLE 0x1007ab03
// Loads a UI texture with the UI bit depth, alpha, mipmaps and clamped, modulated stages; keeps a reference
void UIManager::LoadTexture(const UString& inName, GETexture*& outTexture)
{
    GETextureManager* theManager = this->mRasterizer->GetScreen(this->mScreen)->GetTextureManager();
    GETextureDescription theDescription;
    theDescription.mNumMipLevelsToDrop = 0;
    theDescription.mFrameFormat = 0;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mSurfaceVolatility = 1;
    theDescription.mPixelDepth = this->mUITextureBitDepth;
    theDescription.mHasAlpha = true;
    theDescription.mCreateMIPMaps = true;
    outTexture = theManager->LoadTexture(inName, theDescription);
    outTexture->Reference();
    outTexture->SetStageAlphaMode(kStageOpModulate, -1);
    outTexture->SetStageColorMode(kStageOpModulate, -1);
    outTexture->SetStageMinFilterMode(1, -1);
    outTexture->SetStageMagFilterMode(1, -1);
    outTexture->SetStageMipFilterMode(kMipFilterPoint, -1);
    outTexture->SetStageMipMapBias(-0.75f, -1);
    outTexture->SetStageTextureAddressingMode(kAddressingClamp, -1);
}

// FUNCTION: LLE 0x1007abb9
// Opens the IME candidate window over the client area for the focused IME-enabled control; returns OK
long UIManager::OpenIMECandidateWindow()
{
    GERectangle theRectangle;
    this->mCriticalSection->Enter();
    if (this->mFormStack.size() > 0) {
        UIControl* theControl = this->mFormStack.back()->GetKeyboardCaptured();
        if (theControl && theControl->GetIMEEnabled() == true) {
            GEWindow* theWindow = this->mRasterizer->GetWindow(this->mScreen);
            if (theWindow) {
                this->mCandidateWindowOpen = true;
                theRectangle.SetLeft(theWindow->GetClientOrigin().GetX());
                theRectangle.SetRight(theWindow->GetWidth() + theRectangle.GetLeft());
                theRectangle.SetTop(theWindow->GetClientOrigin().GetY());
                theRectangle.SetBottom(theWindow->GetHeight() + theRectangle.GetTop());
                this->mCandidateWindow->OpenCandidateWindow(theRectangle, theControl->GetRectangle());
            }
        }
    }
    this->mCriticalSection->Leave();
    return kErrorOK;
}

// FUNCTION: LLE 0x1007ac9b
// Tells the top form an IME composition string opened, under the UI lock; returns its result
long UIManager::OpenIMECompositionString()
{
    long theResult = kErrorOK;
    this->mCriticalSection->Enter();
    if (this->mFormStack.size() > 0)
        theResult = this->mFormStack.back()->OpenIMECompositionString();
    this->mCriticalSection->Leave();
    return theResult;
}

// FUNCTION: LLE 0x1007acd4
// Closes the top form, re-shows the forms it covered, redraws its area and activates the form below; returns OK
long UIManager::Pop()
{
    this->mCriticalSection->Enter();
    this->mDrawCriticalSection->Enter();
    ee_ui_manager_pop(this);
    this->mDrawCriticalSection->Leave();
    this->mCriticalSection->Leave();
    return kErrorOK;
}

// FUNCTION: LLE 0x1007adbe
// Pops forms until the given one is on top, when it is on the stack; returns OK (each Pop does)
long UIManager::PopTo(UIForm* inForm)
{
    this->mCriticalSection->Enter();
    this->mDrawCriticalSection->Enter();
    ee_ui_manager_pop_to(this, inForm);
    this->mDrawCriticalSection->Leave();
    this->mCriticalSection->Leave();
    return kErrorOK;
}

// FUNCTION: LLE 0x1007ae53
// Opens a form on top of the stack (unless already stacked), deactivating the old top; returns OK
long UIManager::Push(UIForm* inForm)
{
    this->mCriticalSection->Enter();
    this->mDrawCriticalSection->Enter();
    ee_ui_manager_push(this, inForm);
    this->mDrawCriticalSection->Leave();
    this->mCriticalSection->Leave();
    return kErrorOK;
}

// FUNCTION: LLE 0x1007af47
// Marks every visible form and all its controls dirty
void UIManager::RedrawAll()
{
    this->mCriticalSection->Enter();
    ee_ui_manager_redraw_all(this);
    this->mCriticalSection->Leave();
}

// FUNCTION: LLE 0x1007af90 (RemoveClippedForms): the body is Rust (ee_ui manager.rs, inside Push and SwapForm); the
// member stays declared, with no C++ definition.

// FUNCTION: LLE 0x1007b147
// Rebuilds each visible form's clip rectangles from the visible forms above it
void UIManager::ResetClippedForms()
{
    ee_ui_manager_reset_clipped_forms(this);
}

// FUNCTION: LLE 0x1007b1a2
// Re-lays out every stacked form to the window size and rebuilds the clip rectangles
void UIManager::RelayoutAllForms()
{
    GEWindow* theWindow = this->mRasterizer->GetWindow(this->mScreen);
    ee_ui_manager_relayout_all_forms(this, theWindow->GetWidth(), theWindow->GetHeight());
}

// FUNCTION: LLE 0x1007b254
// Runs and releases the queued events of one form; the other forms' events stay queued in order
void UIManager::ProcessAllEventsForForm(UIForm* inForm)
{
    ee_ui_manager_process_all_events_for_form(this, inForm);
}

// FUNCTION: LLE 0x1007b2ec
// Stops the playing music and releases its instance
void UIManager::RemoveAndStopSoundInstance()
{
    if (this->mMusicInstance) {
        this->mSoundManager->Stop(this->mMusicInstance, false);
        this->RemoveSoundInstance();
    }
}

// FUNCTION: LLE 0x1007b310
// Releases the music instance
void UIManager::RemoveSoundInstance()
{
    if (this->mMusicInstance)
        this->mMusicInstance->Dereference();
    this->mMusicInstance = NULL;
}

// FUNCTION: LLE 0x1007b338
// Deletes the UI database tables (controls, backgrounds, fonts)
void UIManager::RemoveDatabase()
{
    UIControlsTable::DeleteAll();
    UIBackgroundTable::DeleteAll();
    UIFontTable::DeleteAll();
}

// FUNCTION: LLE 0x1007b347
// Replaces the music instance, moving the reference to the new one
void UIManager::SetSoundInstance(SESMInstanceNode* inInstance)
{
    if (inInstance == this->mMusicInstance)
        return;
    this->RemoveSoundInstance();
    this->mMusicInstance = inInstance;
    if (this->mMusicInstance)
        this->mMusicInstance->Reference();
}

// FUNCTION: LLE 0x1007b36e
// Replaces the top form with another (unless already stacked), re-showing what the old one covered; returns OK
long UIManager::SwapForm(UIForm* inForm)
{
    this->mCriticalSection->Enter();
    this->mDrawCriticalSection->Enter();
    ee_ui_manager_swap_form(this, inForm);
    this->mDrawCriticalSection->Leave();
    this->mCriticalSection->Leave();
    return kErrorOK;
}

// FUNCTION: LLE 0x1007b511
// Swaps the left and right mouse buttons when the left-handed setting changes
void UIManager::SetMouseLeftHanded(bool inLeftHanded)
{
    ISEntity* theButton = UIManager::sLMouseButton;
    if (inLeftHanded == UIManager::sMouseLeftHanded)
        return;
    UIManager::sLMouseButton = UIManager::sRMouseButton;
    UIManager::sRMouseButton = theButton;
    UIManager::sMouseLeftHanded = inLeftHanded;
}

// FUNCTION: LLE 0x1007b53a
// Sets the music volume; stops the music at 0 and restarts the last track (or the intro) when raised from 0
void UIManager::SetMusicVolume(float inVolume)
{
    if (this->mMusicVolume > 0.0f && inVolume <= 0.0f)
        this->StopMusic();
    else if (this->mMusicVolume <= 0.0f && inVolume > 0.0f) {
        if (this->mMusicFileName == "") {
            UWideString theIntro("sounds\\eeintro.mp3");
            this->StartMusic(theIntro);
        } else {
            UWideString theFileName(this->mMusicFileName);
            this->mMusicFileName = "";
            this->StartMusic(theFileName);
        }
    }
    this->mMusicVolume = inVolume;
    if (this->mMusicInstance)
        this->mSoundManager->SetSoundParameters(this->mMusicInstance, inVolume, 0.0f, 1.0f);
}

// FUNCTION: LLE 0x1007b616
// Loads and starts a music track unless it is the current one (names compared lower-cased)
void UIManager::StartMusic(const UWideString& inFileName)
{
    UWideString theFileName(inFileName);
    if (!this->mSoundManager)
        return;
    PlatMbsLwr((unsigned char*)(const char*)theFileName);
    if (this->mMusicFileName != theFileName) {
        this->mStopMusic = true;
        if (this->mMusicID != -1) {
            this->RemoveAndStopSoundInstance();
            this->mSoundManager->Unload(this->mMusicID);
        }
        this->mMusicID = this->mSoundManager->Load((const char*)inFileName, 0, 1);
        if (this->mMusicID != -1) {
            // Fixed: stored the name as given while the check above compares lower-cased names, so restarting the
            // current track with a mixed-case name reloaded it
            this->mMusicFileName = theFileName;
            this->mStartMusic = true;
            this->mCriticalSection->Enter();
            if (this->mMusicInstance == NULL) {
                this->SetSoundInstance(
                    this->mSoundManager->PlayAndReturnReferencedNode(this->mMusicID, this->mMusicVolume, 0.0f, 1.0f, true, NULL));
                if (this->mMusicInstance)
                    this->mMusicInstance->Dereference();
            }
            this->mCriticalSection->Leave();
        }
    }
}

// FUNCTION: LLE 0x1007b743
// Stops the music and unloads the track
void UIManager::StopMusic()
{
    this->RemoveAndStopSoundInstance();
    if (this->mMusicID == -1)
        return;
    this->mSoundManager->Unload(this->mMusicID);
    this->mMusicID = -1;
    this->mMusicFileName = "";
    this->mStopMusic = true;
}

// FUNCTION: LLE 0x1007b786
// Passes keyboard input to the top form when the keyboard changed, a control holds it, or an update is forced
long UIManager::UpdateKeyboard()
{
    long theResult = kErrorOK;
    this->mCriticalSection->Enter();
    if (this->mFormStack.size() > 0 && (gISKeyboard->GetShouldUpdate() || this->mFormStack.back()->GetKeyboardCaptured() ||
                                   this->mForceUIUpdate == true)) {
        UIHotKey::UpdatePressedPollAmount();
        theResult = this->mFormStack.back()->UpdateKeyboard();
    }
    this->mCriticalSection->Leave();
    return theResult;
}

// FUNCTION: LLE 0x1007b7f4
// Passes the mouse location and buttons to the top form when the mouse changed, it is captured, or an update is forced
long UIManager::UpdateMouse(const GE2DPoint& inMouseLocation)
{
    long theResult = kErrorOK;
    this->mCriticalSection->Enter();
    if (this->mFormStack.size() > 0 && (gISMouse->GetShouldUpdate() || this->mFormStack.back()->GetMouseCaptured() ||
                                   this->mForceUIUpdate == true)) {
        this->mFormStack.back()->UpdateMouseLocation(inMouseLocation);
        theResult = this->mFormStack.back()->UpdateMouse();
    }
    this->mCriticalSection->Leave();
    return theResult;
}

// FUNCTION: LLE 0x1007b864
// Passes a translated character to the top form unless its repeat count is 16 or more; returns its result
long UIManager::UpdateTranslatedKey(const char* inKey, short inRepeat)
{
    static UWideString sKey;
    long theResult = kErrorOK;
    this->mCriticalSection->Enter();
    if (this->mFormStack.size() > 0 && inRepeat < 0x10) {
        sKey.CopyString(inKey, 1);
        theResult = this->mFormStack.back()->UpdateTranslatedKey(sKey, inRepeat);
    }
    this->mCriticalSection->Leave();
    return theResult;
}

// FUNCTION: LLE 0x1007b8f1
// Runs one UI update: keyboard, mouse, queued events, then any pending music stop or start; returns the mouse result
long UIManager::UpdateUI(const GE2DPoint& inMouseLocation)
{
    this->UpdateKeyboard();
    long theResult = this->UpdateMouse(inMouseLocation);
    this->mForceUIUpdate = false;
    this->mCriticalSection->Enter();
    ee_ui_manager_process_all_events(this);
    if (this->mStopMusic == true) {
        this->RemoveAndStopSoundInstance();
        this->mStopMusic = false;
    }
    if (this->mStartMusic == true) {
        if (this->mMusicInstance == NULL) {
            this->SetSoundInstance(this->mSoundManager->PlayAndReturnReferencedNode(this->mMusicID, this->mMusicVolume, 0.0f, 1.0f, true, NULL));
            if (this->mMusicInstance)
                this->mMusicInstance->Dereference();
        }
        this->mStartMusic = false;
    }
    this->mCriticalSection->Leave();
    return theResult;
}

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// TEMPLATE: LLE 0x1007ba04
// TEMPLATE: LLE 0x1007ba0b
// SYNTHETIC: LLE 0x10078c81
// SYNTHETIC: LLE 0x10078c8b
// SYNTHETIC: LLE 0x10078c9b
// SYNTHETIC: LLE 0x10078ca7
// SYNTHETIC: LLE 0x10078cb1
// SYNTHETIC: LLE 0x10078cbb
// SYNTHETIC: LLE 0x10078ccb
// SYNTHETIC: LLE 0x10078cd7
// SYNTHETIC: LLE 0x10078ce1
// SYNTHETIC: LLE 0x10078ceb
// SYNTHETIC: LLE 0x10078cfb
// SYNTHETIC: LLE 0x10078d07
// SYNTHETIC: LLE 0x10078d11
// SYNTHETIC: LLE 0x10078d1b
// SYNTHETIC: LLE 0x10078d2b
// SYNTHETIC: LLE 0x10078d37
// SYNTHETIC: LLE 0x10078d41
// SYNTHETIC: LLE 0x10078d4b
// SYNTHETIC: LLE 0x10078d5b
// SYNTHETIC: LLE 0x10078d67
// SYNTHETIC: LLE 0x10078d71
// SYNTHETIC: LLE 0x10078d7b
// SYNTHETIC: LLE 0x10078d8b
// SYNTHETIC: LLE 0x10078d97
// SYNTHETIC: LLE 0x10078da1
// SYNTHETIC: LLE 0x10078dda
// SYNTHETIC: LLE 0x10078e07
// SYNTHETIC: LLE 0x10078e68
// SYNTHETIC: LLE 0x10078e89
// SYNTHETIC: LLE 0x1007b8e7
