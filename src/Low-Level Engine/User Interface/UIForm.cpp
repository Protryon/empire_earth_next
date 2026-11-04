// C:\EEX\Low-Level Engine\User Interface\UIForm.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1007042f..0x10075330: the U2D key init chains, UIIMECandidate's implicit copy members and ??_E (UIForm.cpp is the
// first includer of UIIMECandidate.h), the init chain of an unreferenced file-scope UString, then the ctors, the dtor
// and the members in the original's order (roughly alphabetical), then the template COMDATs (hot key vectors, sort,
// the dirty-rectangle deque). UIFullBackground.cpp starts at 0x10075330.
#define LLE_UIFORM_CPP      // UIForm::sTabKey and UIManager::sMouseWheel are visible here (see UIForm.h) (provisional name)
#include <windows.h>
#include <mbstring.h>
#include <algorithm>

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

const float kToolTipShadowOffset = 3.0f;
const float kToolTipShadowOpacity = 0.4f;
const float kFadeInRate = 1.0f / 700.0f;
const float kHelpTextOffset = 32.0f;

// Orders the hot keys so that combinations with more keys are tried first (vtable 0x100999d8: ??_G only)
class UIHotKeySorter {
public:
    virtual ~UIHotKeySorter() {}
    bool operator()(UIHotKey* const& inLeft, UIHotKey* const& inRight) const
    {
        if (inLeft->GetAlwaysFire() && !inRight->GetAlwaysFire())
            return false;
        if (inRight->GetAlwaysFire() && !inLeft->GetAlwaysFire())
            return true;
        return inLeft->GetKeyCount() < inRight->GetKeyCount() ? true : false;
    }
};

// FUNCTION: LLE 0x1007072c
// Creates an empty form with the given control and group counts, tool tips and tabbing off
UIForm::UIForm(unsigned long inControlCount, unsigned long inControlGroupCount, UIManager* inManager,
               unsigned long inFormID, bool inUseGDIForText)
    : mManager(inManager)
{
    this->mControlCount = inControlCount;
    this->mControlGroupCount = inControlGroupCount;
    this->mFormID = inFormID;
    this->mTabbingOn = false;
    this->mUseToolTips = false;
    this->mDrawToolTipShadow = false;
    this->mHelpTextTimeDelay = 0;
    this->mOpenTime = 0;
    this->mUseGDIForText = inUseGDIForText;
    this->mUseExclusiveCoords = false;
    this->Init();
}

// FUNCTION: LLE 0x10070879
// Creates a form from its UI database record: counts, layout, tool-tip settings, music file (lower-cased), tabbing
UIForm::UIForm(UIManager* inManager, const UIDbDataForms& inData, bool inUseGDIForText)
{
    this->mManager = inManager;
    this->mControlCount = inData.mControlCount;
    this->mControlGroupCount = inData.mControlGroupCount;
    this->mFormID = inData.mFormID;
    this->mDrawToolTipShadow = this->mUseToolTips = inData.mUseToolTips;
    this->mHelpTextTimeDelay = inData.mHelpTextTimeDelay;
    this->mMusicFileName = inData.mMusicFile;
    _mbslwr((unsigned char*)(const char*)this->mMusicFileName);
    this->Init();
    this->mLayout.SetLeft(inData.mLeftOffsetType, inData.mLeftOffset);
    this->mLayout.SetTop(inData.mTopOffsetType, inData.mTopOffset);
    this->mLayout.SetRight(inData.mRightOffsetType, inData.mRightOffset);
    this->mLayout.SetBottom(inData.mBottomOffsetType, inData.mBottomOffset);
    this->mDesignedHeight = inData.mDesignedHeight;
    this->mDesignedWidth = inData.mDesignedWidth;
    this->mUseGDIForText = inUseGDIForText;
    this->mUseExclusiveCoords = inData.mUseExclusiveCoords;
    this->SetTabbing(inData.mTabbingOn);
}

// FUNCTION: LLE 0x10070a67
// Destroys the form, releasing its events, hot keys, groups, backgrounds, database-created controls and draw lock
UIForm::~UIForm()
{
    long i;
    this->SetFormCloseEvent(NULL);
    this->SetFormOpenEvent(NULL);
    this->SetFormKeyboardCapturedEvent(NULL);
    this->SetFormUpdateEvent(NULL);
    this->SetMouseLeftButtonDoublePressedEvent(NULL);
    this->SetMouseLeftButtonPressedEvent(NULL);
    this->SetMouseLeftButtonUnpressedEvent(NULL);
    this->SetMouseMiddleButtonDoublePressedEvent(NULL);
    this->SetMouseMiddleButtonPressedEvent(NULL);
    this->SetMouseMiddleButtonUnpressedEvent(NULL);
    this->SetMouseMoveEvent(NULL);
    this->SetMouseRightButtonDoublePressedEvent(NULL);
    this->SetMouseRightButtonPressedEvent(NULL);
    this->SetMouseRightButtonUnpressedEvent(NULL);
    this->SetMouseWheelMoveEvent(NULL);
    this->SetFormActivateEvent(NULL);
    this->SetFormDeactivateEvent(NULL);
    this->SetFormOpenEvent(NULL);
    this->SetFormCloseEvent(NULL);
    this->mMouseWheelScrollEvent->Dereference();
    this->RemoveAllHotKeys();
    for (i = this->mControlGroupCount - 1; i >= 0; i--) {
        delete this->mEntityGroups[i];
        this->mEntityGroups[i] = NULL;
    }
    delete[] this->mEntityGroups;
    for (i = 0; i < (long)this->mBackgrounds.size(); i++) {
        delete this->mBackgrounds[i];
        this->mBackgrounds[i] = NULL;
    }
    for (i = this->mDatabaseControls.size() - 1; i >= 0; i--) {
        UIControlsTable* theTable = UIControlsTable::GetUIControlsTable(this->mDatabaseControls[i].mTableIndex);
        if (theTable && theTable->GetFormIndex() < this->mControlCount)
            delete this->mControls[theTable->GetFormIndex()];
    }
    if (this->mHelpTextPoll)
        delete this->mHelpTextPoll;
    delete[] this->mControls;
    delete[] this->mDrawControls;
    delete this->mFormManager;
    if (this->mText) {
        delete this->mText;
        this->mText = NULL;
    }
    TSScheduler::DestroyCriticalSection(this->mDrawBackgroundLock);
}

// FUNCTION: LLE 0x10070cca
// Starts the form's music (or stops the music for "none"), restores the tab stop and fires the activate event
void UIForm::Activate()
{
    if (this->mMusicFileName.GetLength() > 0) {
        if (this->mMusicFileName == "none")
            this->mManager->StopMusic();
        else
            this->mManager->StartMusic(this->mMusicFileName);
    }
    this->SetTabStop(this->mTabStop);
    if (this->mFormActivateEvent)
        this->mFormActivateEvent->ProcessEvent(this, NULL);
}

// FUNCTION: LLE 0x10070d29
// Inserts a background before the first one with an equal or higher draw priority
void UIForm::AddBackground(UIBackground* inBackground)
{
    unsigned long i;
    unsigned long thePriority = inBackground->GetDrawPriority();
    for (i = 0; i < this->mBackgrounds.size(); i++)
        if (thePriority <= this->mBackgrounds[i]->GetDrawPriority())
            break;
    this->mBackgrounds.insert(this->mBackgrounds.begin() + i, inBackground);
}

// FUNCTION: LLE 0x10070d94
// Adds a rectangle that text drawing must keep out of
void UIForm::AddClipRectangle(const GERectangle& inRectangle)
{
    this->mClipRectangles.push_back(inRectangle);
}

// FUNCTION: LLE 0x10070da3
// Puts a control into its form slot (when the slot is empty) and adds it to the given group
void UIForm::AddControl(UIControl* inControl, unsigned long inGroup)
{
    if (this->mControls[inControl->GetIndex()] == NULL)
        this->mControls[inControl->GetIndex()] = inControl;
    this->mEntityGroups[inGroup]->AddEntity(inControl);
}

// FUNCTION: LLE 0x10070ddb
// Adds a rectangle to the background-redraw list, splitting it around listed rectangles so none overlap
void UIForm::AddCopyDirtyRectangle(const GERectangle& inRectangle)
{
    bool theAdd = true;
    for (unsigned long i = 0; i < this->mCopyDirtyRectangles.size(); i++) {
        if (inRectangle.Contains(this->mCopyDirtyRectangles[i])) {
            this->mCopyDirtyRectangles.erase(this->mCopyDirtyRectangles.begin() + i);
            this->AddCopyDirtyRectangle(inRectangle);
            theAdd = false;
            break;
        }
        if (this->mCopyDirtyRectangles[i].Contains(inRectangle)) {
            theAdd = false;
            break;
        }
        if (!this->mCopyDirtyRectangles[i].Intersects(inRectangle))
            continue;
        if (inRectangle.GetRight() > this->mCopyDirtyRectangles[i].GetRight() + 1.0f)
            this->AddCopyDirtyRectangle(GERectangle(this->mCopyDirtyRectangles[i].GetRight() + 1.0f, inRectangle.GetTop(),
                                              inRectangle.GetRight(), inRectangle.GetBottom()));
        if (inRectangle.GetLeft() < this->mCopyDirtyRectangles[i].GetLeft() - 1.0f)
            this->AddCopyDirtyRectangle(GERectangle(inRectangle.GetLeft(), inRectangle.GetTop(),
                                              this->mCopyDirtyRectangles[i].GetLeft() - 1.0f, inRectangle.GetBottom()));
        if (inRectangle.GetTop() < this->mCopyDirtyRectangles[i].GetTop() - 1.0f)
            this->AddCopyDirtyRectangle(GERectangle(inRectangle.GetLeft(), inRectangle.GetTop(), inRectangle.GetRight(),
                                              this->mCopyDirtyRectangles[i].GetTop() - 1.0f));
        if (inRectangle.GetBottom() > this->mCopyDirtyRectangles[i].GetBottom() + 1.0f)
            this->AddCopyDirtyRectangle(GERectangle(inRectangle.GetLeft(), this->mCopyDirtyRectangles[i].GetBottom() + 1.0f,
                                              inRectangle.GetRight(), inRectangle.GetBottom()));
        theAdd = false;
        break;
    }
    if (theAdd)
        this->mCopyDirtyRectangles.push_back(inRectangle);
}

// FUNCTION: LLE 0x1007110f
// Lists the control in the given slot for drawing this frame
void UIForm::AddDrawControl(unsigned long inIndex)
{
    this->mDrawControls[inIndex] = this->mControls[inIndex];
}

// FUNCTION: LLE 0x1007112b
// Queues a dirty rectangle for every drawing surface, dropping queued ones it covers; marks an overlapped popup dirty
void UIForm::AddDirtyRectangle(const GERectangle& inRectangle)
{
    UIDirtyRectangle theNew;
    UIDirtyRectangle theOld;
    bool theAdd = true;
    if (this->mPopup && inRectangle.Intersects(this->mPopup->GetRectangle()))
        this->mPopup->SetDirty(true);
    theNew.mSurfaceCount = this->mManager->GetNumberOfSurfaces();
    theNew.mRectangle = inRectangle;
    unsigned long i = this->mDirtyRectangles.size();
    while (i--) {
        theOld = this->mDirtyRectangles.front();
        this->mDirtyRectangles.pop_front();
        if (theOld.mRectangle.Contains(theNew.mRectangle) && theOld.mSurfaceCount == theNew.mSurfaceCount) {
            this->mDirtyRectangles.push_back(theOld);
            theAdd = false;
        } else if (!theNew.mRectangle.Contains(theOld.mRectangle))
            this->mDirtyRectangles.push_back(theOld);
    }
    if (theAdd)
        this->mDirtyRectangles.push_back(theNew);
}

// FUNCTION: LLE 0x100712ab
// Copies a hot key into the given group and the form's list, keeping both sorted by key count, always-fire keys last
void UIForm::AddHotKey(const UIHotKey& inHotKey, unsigned long inGroup)
{
    UIHotKey* theHotKey = new UIHotKey(this, inHotKey);
    if (this->mHotKeys.size() <= inGroup)
        this->mHotKeys.resize(inGroup + 1);
    this->mHotKeys[inGroup].push_back(theHotKey);
    std::sort(this->mHotKeys[inGroup].begin(), this->mHotKeys[inGroup].end(), UIHotKeySorter());
    this->mHotKeyList.push_back(theHotKey);
    std::sort(this->mHotKeyList.begin(), this->mHotKeyList.end(), UIHotKeySorter());
}

// FUNCTION: LLE 0x100713cf
// Builds a hot key from a key, up to three modifiers and four events, then adds it to the given group
void UIForm::AddHotKey(unsigned long inKey, unsigned long inModifier1, unsigned long inModifier2,
                       unsigned long inModifier3, UIEvent* inJustPressedEvent, UIEvent* inPressedEvent,
                       UIEvent* inJustUnpressedEvent, UIEvent* inDoublePressedEvent, unsigned long inGroup,
                       bool inAlwaysFire)
{
    UIHotKey theHotKey;
    theHotKey.mForm = this;
    gISKeyboard->GetEntity(inKey, theHotKey.mKey);
    theHotKey.CalculateKeyCount();
    if (inModifier1 < kKeyNone)
        theHotKey.SetModifier(inModifier1, 0);
    if (inModifier2 < kKeyNone)
        theHotKey.SetModifier(inModifier2, 1);
    if (inModifier3 < kKeyNone)
        theHotKey.SetModifier(inModifier3, 2);
    if (inJustPressedEvent) {
        theHotKey.SetJustPressedEvent(inJustPressedEvent);
        inJustPressedEvent->Dereference();
    }
    if (inPressedEvent) {
        theHotKey.SetPressedEvent(inPressedEvent);
        inPressedEvent->Dereference();
    }
    if (inJustUnpressedEvent) {
        theHotKey.SetJustUnpressedEvent(inJustUnpressedEvent);
        inJustUnpressedEvent->Dereference();
    }
    if (inDoublePressedEvent) {
        theHotKey.SetDoublePressedEvent(inDoublePressedEvent);
        inDoublePressedEvent->Dereference();
    }
    theHotKey.mAlwaysFire = inAlwaysFire;
    this->AddHotKey(theHotKey, inGroup);
}

// FUNCTION: LLE 0x100714bd
// Passes an IME result string to the control holding the keyboard
void UIForm::AddIMEResultString(const UWideString& inString)
{
    if (this->mKeyboardCaptured)
        this->mKeyboardCaptured->AddIMEResultString(inString);
}

// FUNCTION: LLE 0x100714d3
// Passes the IME composition string to the control holding the keyboard; kErrorOK when none does
long UIForm::ChangeIMECompositionString(const UWideString& inString, const UString& inAttributes,
                                        unsigned long inCursor)
{
    long theResult = kErrorOK;
    if (this->mKeyboardCaptured)
        theResult = this->mKeyboardCaptured->ChangeIMECompositionString(inString, inAttributes, inCursor);
    return theResult;
}

// FUNCTION: LLE 0x100714f3
// Moves a radio-button index to the checked button of its run of radio buttons (unchanged if it is checked)
void UIForm::CheckedRadioButton(long& ioIndex)
{
    long i = ioIndex;
    if (this->mControls[i]->GetType() != kControlTypeRadioButton || static_cast<UIRadioButton*>(this->mControls[i])->GetIsChecked())
        return;
    for (i--; i >= 0 && this->mControls[i] && this->mControls[i]->GetType() == kControlTypeRadioButton; i--)
        if (static_cast<UIRadioButton*>(this->mControls[i])->GetIsChecked()) {
            ioIndex = i;
            break;
        }
    if (i != ioIndex)
        for (i = ioIndex + 1; i < this->mControlCount && this->mControls[i] && this->mControls[i]->GetType() == kControlTypeRadioButton; i++)
            if (static_cast<UIRadioButton*>(this->mControls[i])->GetIsChecked()) {
                ioIndex = i;
                break;
            }
}

// FUNCTION: LLE 0x10071592
// Closes the form: fires the close event, clears the draw list and popup, closes the backgrounds, flushes its events
// and deletes the controls created from the database
void UIForm::Close()
{
    unsigned long i;
    if (this->mFormCloseEvent)
        this->mFormCloseEvent->ProcessEvent(this, NULL);
    this->SetAllControlsDirty();
    for (i = 0; i < this->mControlCount; i++)
        this->mDrawControls[i] = NULL;
    this->mCopiedPopup = NULL;
    for (i = 0; i < this->mBackgrounds.size(); i++)
        this->mBackgrounds[i]->Close();
    this->mManager->ProcessAllEventsForForm(this);
    for (i = 0; i < this->mDatabaseControls.size(); i++) {
        UIControlsTable* theTable = UIControlsTable::GetUIControlsTable(this->mDatabaseControls[i].mTableIndex);
        if (theTable && this->mDatabaseControls[i].mControl)
            this->RemoveControl(this->mDatabaseControls[i].mControl, theTable->GetGroup());
        delete this->mDatabaseControls[i].mControl;
        this->mDatabaseControls[i].mControl = NULL;
    }
}

// FUNCTION: LLE 0x100716ae
// Closes the IME composition string of the control holding the keyboard; kErrorOK when none does
long UIForm::CloseIMECompositionString()
{
    long theResult = kErrorOK;
    if (this->mKeyboardCaptured)
        theResult = this->mKeyboardCaptured->CloseIMECompositionString();
    return theResult;
}

// FUNCTION: LLE 0x100716c0
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
            theForm->mEntityGroups[0]->Enable();
        }
    }
    inFile.Close();
    return theResult;
}

// FUNCTION: LLE 0x10071764
// Fires the deactivate event, hides the popup and help text, and releases the captured mouse and keyboard
void UIForm::Deactivate()
{
    if (this->mFormDeactivateEvent)
        this->mFormDeactivateEvent->ProcessEvent(this, NULL);
    this->SetPopup(NULL);
    if (this->mHelpTextLabel)
        this->mHelpTextLabel->SetDisplayMode(kDisplayModeHidden);
    this->mHelpTextPoll->Stop();
    if (this->mMouseCaptured) {
        this->mMouseCaptured->ReleaseCapturedMouse();
        this->mMouseCaptured = NULL;
    }
    if (this->mKeyboardCaptured) {
        this->mKeyboardCaptured->ReleaseCapturedKeyboard();
        this->mKeyboardCaptured = NULL;
    }
    this->mMouseOverControl = NULL;
}

// FUNCTION: LLE 0x100717d1
// Snapshots the form for drawing: fires the update event, copies every dirty shown control, lists the controls to draw,
// ages the dirty rectangles and updates the help text
long UIForm::CopyData()
{
    UIDirtyRectangle theDirty;
    long theResult = kErrorOK;
    long i;
    if (this->mFormUpdateEvent)
        theResult = this->mFormUpdateEvent->ProcessEvent(this, NULL);
    if (this->GetShouldFadeIn()) {
        this->mFadeOpacity = (float)(TMGetTime() - this->mOpenTime) * kFadeInRate;
        this->mFadeOpacity = max(min(this->mFadeOpacity, 1.0f), 0.0f);
        this->SetAllControlsDirty();
        this->mManager->AddDirtyRectangle(this->mRectangle, this);
    } else
        this->mFadeOpacity = 1.0f;
    for (i = this->mControlGroupCount - 1; i >= 0; i--) {
        UIEntityGroup* theGroup = this->mEntityGroups[i];
        if (theGroup->GetIsEnabled())
            for (long j = theGroup->GetEntityCount() - 1; j >= 0; j--) {
                UIControl* theControl = static_cast<UIControl*>(theGroup->GetEntity(j));
                if (theControl->GetDisplayMode() != kDisplayModeShown)
                    continue;
                if (theControl->GetIsDirty()) {
                    this->mManager->AddDirtyRectangle(theControl->GetRectangle(), this);
                    if (this->mPopup &&
                        (theControl == this->mPopup || theControl->GetRectangle().Intersects(this->mPopup->GetRectangle()))) {
                        this->mCopiedPopupRectangle = this->mPopup->GetRectangle();
                        this->mPopupDrawCount = this->mManager->GetNumberOfSurfaces();
                    }
                    theResult = theControl->CopyData();
                }
                if (theControl->GetDrawCount() > 0 && theControl != this->mPopup)
                    this->AddDrawControl(theControl->GetIndex());
            }
    }
    if (this->mPopup && this->mPopupDrawCount > 0)
        this->mCopiedPopup = this->mPopup;
    unsigned long k = this->mDirtyRectangles.size();
    while (k--) {
        theDirty = this->mDirtyRectangles.front();
        this->mDirtyRectangles.pop_front();
        this->AddCopyDirtyRectangle(theDirty.mRectangle);
        if (--theDirty.mSurfaceCount)
            this->mDirtyRectangles.push_back(theDirty);
    }
    if (this->mHelpTextLabel && this->mMouseOverControl)
        this->UpdateHelpText(false);
    return theResult;
}

// FUNCTION: LLE 0x10071a3e
// Redraws every visible background that overlaps the rectangle; returns the last draw result
long UIForm::DrawBackground(const GERectangle& inRectangle)
{
    long theResult = kErrorOK;
    for (unsigned long i = 0; i < this->mBackgrounds.size(); i++) {
        UIBackground* theBackground = this->mBackgrounds[i];
        if (theBackground->GetIsVisible() && inRectangle.Intersects(theBackground->GetRectangle()))
            theResult = theBackground->DrawBackground(inRectangle);
    }
    return theResult;
}

// FUNCTION: LLE 0x10071aa5
// Draws the part of a bitmap's texture under a rectangle at a screen location
long UIForm::DrawBackground(GEBitmap* inBitmap, const GERectangle& inRectangle, const GE2DPoint& inLocation)
{
    GETextureCoordinate theTopLeft;
    GETextureCoordinate theBottomRight;
    GETextureCoordinate theTopRight;
    GETextureCoordinate theBottomLeft;
    float theWidth = (float)inBitmap->GetTexture(this->mManager->GetScreen())->GetWidth();
    float theHeight = (float)inBitmap->GetTexture(this->mManager->GetScreen())->GetHeight();
    GERectangle theRectangle;
    theRectangle = inRectangle;
    theRectangle.SetRight(theRectangle.GetRight() + 1.0f);
    theRectangle.SetBottom(theRectangle.GetBottom() + 1.0f);
    theTopLeft.SetU(theRectangle.GetLeft() / theWidth);
    theTopLeft.SetV(theRectangle.GetTop() / theHeight);
    theBottomRight.SetU(theRectangle.GetRight() / theWidth);
    theBottomRight.SetV(theRectangle.GetBottom() / theHeight);
    theTopRight.SetU(theBottomRight.GetU());
    theTopRight.SetV(theTopLeft.GetV());
    theBottomLeft.SetU(theTopLeft.GetU());
    theBottomLeft.SetV(theBottomRight.GetV());
    inBitmap->SetLocation(inLocation);
    inBitmap->SetXYScale(theRectangle.GetWidth() / theWidth, theRectangle.GetHeight() / theHeight);
    inBitmap->SetTextureCoordinates(theTopLeft, theTopRight, theBottomLeft, theBottomRight);
    return this->mManager->GetRasterizer()->Draw(inBitmap, this->mManager->GetScreen());
}

// FUNCTION: LLE 0x10071bee
// Draws a bitmap tiled over a tile area, clipped to the rectangle, one piece per tile
long UIForm::DrawBackground(GEBitmap* inBitmap, const GERectangle& inRectangle, const GERectangle& inDrawRectangle,
                            const GERectangle& inTileRectangle)
{
    GERectangle theTile;
    GERectangle theClipped;
    GE2DPoint theLocation;
    if (inRectangle.And(inTileRectangle, theClipped)) {
        unsigned long theTileWidth = UMFloor(inDrawRectangle.GetWidth());
        unsigned long theTileHeight = UMFloor(inDrawRectangle.GetHeight());
        unsigned long theFirstRow = UMFloor((theClipped.GetTop() - inTileRectangle.GetTop()) / theTileHeight);
        unsigned long theLastRow = UMFloor((theClipped.GetBottom() - inTileRectangle.GetTop()) / theTileHeight);
        unsigned long theFirstColumn = UMFloor((theClipped.GetLeft() - inTileRectangle.GetLeft()) / theTileWidth);
        unsigned long theLastColumn = UMFloor((theClipped.GetRight() - inTileRectangle.GetLeft()) / theTileWidth);
        for (unsigned long theRow = theFirstRow; theRow <= theLastRow; theRow++)
            for (unsigned long theColumn = theFirstColumn; theColumn <= theLastColumn; theColumn++) {
                GE2DPoint thePosition(inTileRectangle.GetLeft() + theColumn * theTileWidth,
                                      inTileRectangle.GetTop() + theRow * theTileHeight);
                theTile = GERectangle(thePosition, GE2DPoint(thePosition.GetX() + theTileWidth,
                                                             thePosition.GetY() + theTileHeight));
                if (!theTile.And(theClipped) || !(theTile.GetWidth() > 0.0f) || !(theTile.GetHeight() > 0.0f))
                    continue;
                theLocation = theTile.GetTopLeft();
                GE2DPoint theOrigin(inDrawRectangle.GetTopLeft());
                theTile.SetLeft(theTile.GetLeft() + theOrigin.GetX() - thePosition.GetX());
                theTile.SetRight(theTile.GetRight() + theOrigin.GetX() - thePosition.GetX());
                theTile.SetBottom(theOrigin.GetY() + theTile.GetBottom() - thePosition.GetY());
                theTile.SetTop(theTile.GetTop() + theOrigin.GetY() - thePosition.GetY());
                this->DrawBackground(inBitmap, theTile, theLocation);
            }
    }
    return kErrorOK;
}

// FUNCTION: LLE 0x10071e99
// Draws text clipped around the IME candidate window, the copied popup or the form's clip rectangles
long UIForm::DrawClippedText(const GERectangle& inRectangle, GEText* inText, UIControl* inControl)
{
    std::vector<GERectangle> theRectangles;
    std::vector<GERectangle> theRemaining;
    long theResult = kErrorOK;
    if (this->mClipRectangles.size() == 0) {
        if (this->mCopiedPopup == NULL) {
            if (!this->GetManager()->GetDrawCandidateWindow())
                theResult = this->GetManager()->GetRasterizer()->Draw(inText, inRectangle, this->GetManager()->GetDrawingSurface());
            else {
                if (!inRectangle.Intersects(this->GetManager()->GetCandidateWindow()->GetIMECopyCandidateRectangle()))
                    theResult = this->GetManager()->GetRasterizer()->Draw(inText, inRectangle,
                                                                    this->GetManager()->GetDrawingSurface());
                else {
                    inRectangle.RemoveIntersectingRectangle(
                        this->GetManager()->GetCandidateWindow()->GetIMECopyCandidateRectangle(), theRectangles);
                    for (unsigned long i = 0; i < theRectangles.size() && theResult == kErrorOK; i++)
                        theResult = this->GetManager()->GetRasterizer()->Draw(inText, theRectangles[i],
                                                                    this->GetManager()->GetDrawingSurface());
                }
            }
        } else if (inControl && this->mCopiedPopup == this->mControls[inControl->GetIndex()] ||
                   !inRectangle.Intersects(this->mCopiedPopup->GetRectangle()))
            theResult = this->GetManager()->GetRasterizer()->Draw(inText, inRectangle, this->GetManager()->GetDrawingSurface());
        else {
            inRectangle.RemoveIntersectingRectangle(this->mCopiedPopup->GetCopiedDrawRectangle(), theRectangles);
            for (unsigned long i = 0; i < theRectangles.size() && theResult == kErrorOK; i++)
                theResult = this->GetManager()->GetRasterizer()->Draw(inText, theRectangles[i],
                                                                this->GetManager()->GetDrawingSurface());
        }
    } else {
        bool theClipped = false;
        for (unsigned long i = 0; i < this->mClipRectangles.size(); i++)
            if (inRectangle.Intersects(this->mClipRectangles[i])) {
                if (!theClipped) {
                    theRectangles.push_back(inRectangle);
                    theClipped = true;
                }
                theRemaining.clear();
                for (unsigned long j = 0; j < theRectangles.size(); j++)
                    theRectangles[j].RemoveIntersectingRectangle(this->mClipRectangles[i], theRemaining);
                theRectangles.clear();
                theRectangles = theRemaining;
            }
        if (!theClipped)
            theResult = this->GetManager()->GetRasterizer()->Draw(inText, inRectangle, this->GetManager()->GetDrawingSurface());
        else
            for (unsigned long i = 0; i < theRectangles.size() && theResult == kErrorOK; i++)
                theResult = this->GetManager()->GetRasterizer()->Draw(inText, theRectangles[i],
                                                                this->GetManager()->GetDrawingSurface());
    }
    return theResult;
}

// FUNCTION: LLE 0x1007229e
// Redraws the queued background rectangles, then every listed control and the popup (with a shadow under a tool tip)
long UIForm::DrawPolygons()
{
    GERectangle theShadowRectangle;
    GERhombus theShadow;
    long theResult = kErrorOK;
    long i;
    this->LockDrawBackground();
    for (i = 0; i < (long)this->mCopyDirtyRectangles.size(); i++)
        this->DrawBackground(this->mCopyDirtyRectangles[i]);
    this->mCopyDirtyRectangles.clear();
    this->UnlockDrawBackground();
    long theCount = this->mControlCount;
    for (i = 0; i < theCount; i++)
        if (this->mDrawControls[i])
            theResult = this->mDrawControls[i]->DrawPolygons();
    if (this->mCopiedPopup) {
        if (this->mDrawToolTipShadow && this->mHelpTextLabel == this->mCopiedPopup) {
            this->GetToolTipShadowRectangle(this->mCopiedPopup->GetRectangle(), theShadowRectangle);
            theShadow.SetColor(static_cast<UITextLabel*>(this->mCopiedPopup)->GetGEText()->GetFgColor());
            theShadow.SetOpacity(kToolTipShadowOpacity);
            theShadow.SetRectangle(theShadowRectangle);
            this->mManager->GetRasterizer()->Draw(&theShadow, this->mManager->GetScreen());
        }
        theResult = this->mCopiedPopup->DrawPolygons();
    }
    return theResult;
}

// FUNCTION: LLE 0x100723dd
// Draws the text of every listed control, then the popup's, and empties the draw list
long UIForm::DrawTextA()
{
    long theResult = kErrorOK;
    long i;
    if (this->mCopiedPopup) {
        for (i = this->mControlCount - 1; i >= 0; i--)
            if (this->mDrawControls[i]) {
                theResult = this->mDrawControls[i]->DrawTextA();
                this->mDrawControls[i] = NULL;
            }
        if (this->mCopiedPopup) {
            theResult = this->mCopiedPopup->DrawTextA();
            this->mPopupDrawCount--;
            this->mCopiedPopup = NULL;
        }
    } else
        for (i = this->mControlCount - 1; i >= 0; i--)
            if (this->mDrawControls[i]) {
                theResult = this->mDrawControls[i]->DrawTextA();
                this->mDrawControls[i] = NULL;
            }
    return theResult;
}

// FUNCTION: LLE 0x10072452
// Finds the highest empty control slot and returns the slot after it; false when none is empty or it is the last slot
bool UIForm::GetAvailableIndex(unsigned long& outIndex) const
{
    bool theResult = false;
    for (long i = this->mControlCount - 1; i >= 0; i--)
        if (this->mControls[i] == NULL) {
            if (i + 1 != this->mControlCount) {
                theResult = true;
                outIndex = i + 1;
            }
            break;
        }
    return theResult;
}

// FUNCTION: LLE 0x1007248c
// Returns the background with the given ID, or NULL
UIBackground* UIForm::GetBackgroundByID(unsigned long inID) const
{
    UIBackground* theResult = NULL;
    for (unsigned long i = 0; i < this->mBackgrounds.size(); i++)
        if (this->mBackgrounds[i]->GetBackgroundID() == inID) {
            theResult = this->mBackgrounds[i];
            break;
        }
    return theResult;
}

// FUNCTION: LLE 0x100724c6
// Returns the control with the given ID (NULL: none)
UIControl* UIForm::GetControlById(unsigned long inId)
{
    long i;
    for (i = this->mControlCount - 1; i >= 0; i--)
        if (this->mControls[i] && this->mControls[i]->GetId() == inId)
            break;
    if (i < 0)
        return NULL;   // Fixed: with no match it used to return mControls[-1], the word before the array
    return this->mControls[i];
}

// FUNCTION: LLE 0x10072500
// Returns the group holding the control with the given ID; falls back to group 0 when no group holds it
UIEntityGroup* UIForm::GetEntityGroupById(unsigned long inId) const
{
    long theResult = kErrorFailed;
    UIEntityGroup* theGroup = NULL;
    for (long i = this->mControlGroupCount - 1; i >= 0 && theResult != kErrorOK; i--) {
        theGroup = this->mEntityGroups[i];
        for (long j = theGroup->GetEntityCount() - 1; j >= 0; j--) {
            UIEntity* theEntity = theGroup->GetEntity(j);
            if (theEntity->GetType() == kControlTypeButton && static_cast<UIControl*>(theEntity)->GetId() == inId) {
                theResult = kErrorOK;
                break;
            }
        }
        if (theResult == kErrorOK)
            break;
    }
    return theGroup;
}

// FUNCTION: LLE 0x10072571
// Returns false: forms never fade in
bool UIForm::GetShouldFadeIn()
{
    return false;
}

// FUNCTION: LLE 0x10072574
// Returns the tool-tip shadow rectangle: the given rectangle moved 3 pixels right and down
void UIForm::GetToolTipShadowRectangle(const GERectangle& inRectangle, GERectangle& outRectangle)
{
    GE2DPoint theTopLeft = inRectangle.GetTopLeft();
    GE2DPoint theBottomRight = inRectangle.GetBottomRight();
    GE2DPoint theOffset(kToolTipShadowOffset, kToolTipShadowOffset);
    theTopLeft += theOffset;
    theBottomRight += theOffset;
    outRectangle = GERectangle(theTopLeft, theBottomRight);
}

// FUNCTION: LLE 0x100725fd
// Loads every background's texture
void UIForm::LoadBackgrounds()
{
    for (unsigned long i = 0; i < this->mBackgrounds.size(); i++)
        this->mBackgrounds[i]->LoadBackground();
}

// FUNCTION: LLE 0x10072624
// Initializes the form: clears events and captures, creates the wheel-scroll event, help-text timer, control slots,
// groups and draw lock
void UIForm::Init()
{
    long i;
    this->mMouseCaptured = NULL;
    this->mKeyboardCaptured = NULL;
    this->mTabStop = 0;
    this->mPopup = NULL;
    this->mCopiedPopup = NULL;
    this->mUnknown19A = false;
    this->mFormCloseEvent = NULL;
    this->mFormOpenEvent = NULL;
    this->mFormUpdateEvent = NULL;
    this->mFormManager = NULL;
    this->mMouseLeftButtonPressedEvent = NULL;
    this->mMouseLeftButtonUnpressedEvent = NULL;
    this->mMouseLeftButtonDoublePressedEvent = NULL;
    this->mMouseMiddleButtonPressedEvent = NULL;
    this->mMouseMiddleButtonUnpressedEvent = NULL;
    this->mMouseMiddleButtonDoublePressedEvent = NULL;
    this->mMouseMoveEvent = NULL;
    this->mMouseRightButtonPressedEvent = NULL;
    this->mMouseRightButtonUnpressedEvent = NULL;
    this->mMouseRightButtonDoublePressedEvent = NULL;
    this->mMouseWheelMoveEvent = NULL;
    this->mFormActivateEvent = NULL;
    this->mFormDeactivateEvent = NULL;
    this->mFormKeyboardCapturedEvent = NULL;
    this->mMouseWheelScrollEvent = new UIEMouseWheelScroll;
    this->SetMouseWheelMoveEvent(this->mMouseWheelScrollEvent);
    this->mHelpTextLabel = NULL;
    this->mMouseOverControl = NULL;
    this->mDesignedHeight = 0;
    this->mDesignedWidth = 0;
    this->mPopupDrawCount = 0;
    if (UIForm::sTabKey == NULL) {
        gISKeyboard->GetEntity(kKeyTab, UIForm::sTabKey);
        gISKeyboard->GetEntity(kKeyShift, UIForm::sShiftKey);
    }
    if (this->mHelpTextTimeDelay == 0)
        this->mHelpTextTimeDelay = 1;
    this->mHelpTextPoll = new TMPoll(this->mHelpTextTimeDelay, 0, false);
    this->mControls = new UIControl*[this->mControlCount];
    this->mDrawControls = new UIControl*[this->mControlCount];
    for (i = this->mControlCount - 1; i >= 0; i--) {
        this->mControls[i] = NULL;
        this->mDrawControls[i] = NULL;
    }
    this->mEntityGroups = new UIEntityGroup*[this->mControlGroupCount];
    for (i = this->mControlGroupCount - 1; i >= 0; i--)
        this->mEntityGroups[i] = new UIEntityGroup;
    this->mText = NULL;
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mDrawBackgroundLock) == keseOK, 1743);
}

// FUNCTION: LLE 0x10072834
// Opens the form: lays it out on the screen, opens the backgrounds, creates the database controls and their events,
// fires the open event and marks every control dirty
void UIForm::Open()
{
    GERectangle theScreenRectangle;
    unsigned long i;
    unsigned long j;
    GEWindow* theWindow = this->mManager->GetRasterizer()->GetWindow(this->mManager->GetScreen());
    theScreenRectangle.SetLeft(0.0f);
    theScreenRectangle.SetTop(0.0f);
    theScreenRectangle.SetRight((float)theWindow->GetWidth() - 1.0f);
    theScreenRectangle.SetBottom((float)theWindow->GetHeight() - 1.0f);
    this->mLayout.CalculateRectangle(theScreenRectangle, this->mRectangle, false);
    for (j = 0; j < this->mBackgrounds.size(); j++)
        this->mBackgrounds[j]->Open();
    for (i = 0; i < this->mDatabaseControls.size(); i++) {
        UIControlsTable* theTable = UIControlsTable::GetUIControlsTable(this->mDatabaseControls[i].mTableIndex);
        if (theTable) {
            this->mDatabaseControls[i].mControl = theTable->CreateControl(this->mManager);
            if (this->mDatabaseControls[i].mControl) {
                this->AddControl(this->mDatabaseControls[i].mControl, theTable->GetGroup());
                this->mDatabaseControls[i].mControl->SetDisplayMode(theTable->GetIsVisible() == true);
                if ((theTable->GetControlType() == kDbControlHelpTextLabel || theTable->GetControlType() == kDbControlMultiLineHelpTextLabel) &&
                    theTable->GetIsVisible() == true) {
                    this->mHelpTextLabel = static_cast<UITextLabel*>(this->mDatabaseControls[i].mControl);
                    this->mDatabaseControls[i].mControl->SetDisplayMode(kDisplayModeHidden);
                }
                if (theTable->GetStringTableID() > 0)
                    this->mDatabaseControls[i].mControl->SetStringTableID(theTable->GetStringTableID());
            }
        }
        this->SetTabbing(this->mTabbingOn);
    }
    for (j = 0; j < this->mDatabaseControls.size(); j++) {
        UIControlsTable* theTable = UIControlsTable::GetUIControlsTable(this->mDatabaseControls[j].mTableIndex);
        if (theTable)
            theTable->CreateControlEvent(this);
    }
    for (i = 0; i < this->mEventData.size(); i++) {
        UIEvent* theEvent = this->mManager->CreateFormEvent(this->mEventData[i].mEventID);
        if (theEvent) {
            this->SetFormEvent(this->mEventData[i].mEventType, theEvent);
            theEvent->Dereference();
        }
    }
    if (this->mFormOpenEvent)
        this->mFormOpenEvent->ProcessEvent(this, NULL);
    this->mOpenTime = TMGetTime();
    this->SetAllControlsDirty();
}

// FUNCTION: LLE 0x10072b37
// Opens an IME composition string in the control holding the keyboard; kErrorFailed when none does
long UIForm::OpenIMECompositionString()
{
    long theResult = kErrorFailed;
    if (this->mKeyboardCaptured)
        theResult = this->mKeyboardCaptured->OpenIMECompositionString();
    return theResult;
}

// FUNCTION: LLE 0x10072b4a
// Deletes every hot key of every group
void UIForm::RemoveAllHotKeys()
{
    for (unsigned long i = 0; i < this->mHotKeys.size(); i++) {
        for (unsigned long j = 0; j < this->mHotKeys[i].size(); j++)
            delete this->mHotKeys[i][j];
        this->mHotKeys[i].clear();
    }
    this->mHotKeys.clear();
    this->mHotKeyList.clear();
}

// FUNCTION: LLE 0x10072bc4
// Deletes every hot key of one group
void UIForm::RemoveAllHotKeysForAGroup(unsigned long inGroup)
{
    if (inGroup >= this->mHotKeys.size())
        return;
    std::vector<UIHotKey*>::iterator theIterator = this->mHotKeys[inGroup].begin();
    while (theIterator != this->mHotKeys[inGroup].end()) {
        std::vector<UIHotKey*>::iterator theListIterator = this->mHotKeyList.begin();
        while (theListIterator != this->mHotKeyList.end())
            if (*theIterator == *theListIterator)
                this->mHotKeyList.erase(theListIterator);
            else
                theListIterator++;
        delete *theIterator;
        this->mHotKeys[inGroup].erase(theIterator);
    }
    this->mHotKeys[inGroup].clear();
}

// FUNCTION: LLE 0x10072c82
// Deletes the first hot key of a group that matches the key and the three modifiers
void UIForm::RemoveHotKey(unsigned long inKey, unsigned long inModifier1, unsigned long inModifier2,
                          unsigned long inModifier3, unsigned long inGroup)
{
    ISEntity* theKey;
    ISEntity* theModifiers[3];
    gISKeyboard->GetEntity(inKey, theKey);
    gISKeyboard->GetEntity(inModifier1, theModifiers[0]);
    gISKeyboard->GetEntity(inModifier2, theModifiers[1]);
    gISKeyboard->GetEntity(inModifier3, theModifiers[2]);
    if (inGroup >= this->mHotKeys.size())
        return;
    std::vector<UIHotKey*>::iterator theIterator;
    for (theIterator = this->mHotKeys[inGroup].begin(); theIterator != this->mHotKeys[inGroup].end(); theIterator++) {
        UIHotKey* theHotKey = *theIterator;
        if (theHotKey->mKey != theKey || theHotKey->mModifiers[0] != theModifiers[0] ||
            theHotKey->mModifiers[1] != theModifiers[1] || theHotKey->mModifiers[2] != theModifiers[2])
            continue;
        std::vector<UIHotKey*>::iterator theListIterator = this->mHotKeyList.begin();
        while (theListIterator != this->mHotKeyList.end())
            if (*theIterator == *theListIterator)
                this->mHotKeyList.erase(theListIterator);
            else
                theListIterator++;
        delete *theIterator;
        this->mHotKeys[inGroup].erase(theIterator);
        return;
    }
}

// FUNCTION: LLE 0x10072da2
// Deletes the hot key at an index in a group
void UIForm::RemoveHotKey(unsigned long inIndex, unsigned long inGroup)
{
    UIHotKey* theHotKey = this->mHotKeys[inGroup][inIndex];
    this->mHotKeys[inGroup].erase(this->mHotKeys[inGroup].begin() + inIndex);
    for (long i = this->mHotKeyList.size() - 1; i >= 0; i--)
        if (theHotKey == this->mHotKeyList[i])
            this->mHotKeyList.erase(this->mHotKeyList.begin() + i);
    delete theHotKey;
}

// FUNCTION: LLE 0x10072e8a
// Removes and deletes a background found by its ID (nothing happens when it is not listed)
void UIForm::RemoveBackground(UIBackground* inBackground)
{
    unsigned long theID = inBackground->GetBackgroundID();
    for (unsigned long i = 0; i < this->mBackgrounds.size(); i++)
        if (theID == this->mBackgrounds[i]->GetBackgroundID()) {
            this->mBackgrounds.erase(this->mBackgrounds.begin() + i);
            delete inBackground;
            break;
        }
}

// FUNCTION: LLE 0x10072efc
// Marks dirty every visible control of an enabled group that overlaps the rectangle
void UIForm::RedrawIntersectControls(const GERectangle& inRectangle)
{
    for (long i = this->mControlGroupCount - 1; i >= 0; i--) {
        UIEntityGroup* theGroup = this->mEntityGroups[i];
        if (theGroup->GetIsEnabled())
            for (long j = theGroup->GetEntityCount() - 1; j >= 0; j--) {
                UIEntity* theEntity = theGroup->GetEntity(j);
                if (theEntity->GetType() == kControlTypeButton && static_cast<UIControl*>(theEntity)->GetDisplayMode() != kDisplayModeHidden &&
                    static_cast<UIControl*>(theEntity)->GetRectangle().Intersects(inRectangle))
                    (static_cast<UIControl*>(theEntity))->SetDirty(true);
            }
    }
}

// FUNCTION: LLE 0x10072f8e
// Lays the form out again inside a new rectangle, with its backgrounds and database controls, and drops the cached text
void UIForm::RelayoutForm(const GERectangle& inRectangle)
{
    unsigned long i;
    this->mLayout.CalculateRectangle(inRectangle, this->mRectangle, false);
    for (i = 0; i < this->mBackgrounds.size(); i++)
        this->mBackgrounds[i]->RelayoutBackground();
    for (i = 0; i < this->mDatabaseControls.size(); i++) {
        UIControlsTable* theTable = UIControlsTable::GetUIControlsTable(this->mDatabaseControls[i].mTableIndex);
        if (theTable)
            theTable->RelayoutControl(this->mDatabaseControls[i].mControl, this->mManager);
    }
    if (this->mText) {
        delete this->mText;
        this->mText = NULL;
    }
}

// FUNCTION: LLE 0x10073022
// Marks every control of the form dirty
void UIForm::SetAllControlsDirty()
{
    for (long i = this->mControlCount - 1; i >= 0; i--)
        if (this->mControls[i])
            this->mControls[i]->SetDirty(true);
}

// FUNCTION: LLE 0x10073048
// Replaces the form-activate event, releasing the old one and initializing the new one
void UIForm::SetFormActivateEvent(UIEvent* inEvent)
{
    if (this->mFormActivateEvent) {
        this->mFormActivateEvent->Uninitialize();
        this->mFormActivateEvent->Dereference();
    }
    this->mFormActivateEvent = inEvent;
    if (this->mFormActivateEvent) {
        this->mFormActivateEvent->Reference();
        this->mFormActivateEvent->Initialize();
    }
}

// FUNCTION: LLE 0x10073085
// Replaces the form-close event, releasing the old one and initializing the new one
void UIForm::SetFormCloseEvent(UIEvent* inEvent)
{
    if (this->mFormCloseEvent) {
        this->mFormCloseEvent->Uninitialize();
        this->mFormCloseEvent->Dereference();
    }
    this->mFormCloseEvent = inEvent;
    if (this->mFormCloseEvent) {
        this->mFormCloseEvent->Reference();
        this->mFormCloseEvent->Initialize();
    }
}

// FUNCTION: LLE 0x100730c2
// Replaces the form-deactivate event, releasing the old one and initializing the new one
void UIForm::SetFormDeactivateEvent(UIEvent* inEvent)
{
    if (this->mFormDeactivateEvent) {
        this->mFormDeactivateEvent->Uninitialize();
        this->mFormDeactivateEvent->Dereference();
    }
    this->mFormDeactivateEvent = inEvent;
    if (this->mFormDeactivateEvent) {
        this->mFormDeactivateEvent->Reference();
        this->mFormDeactivateEvent->Initialize();
    }
}

// FUNCTION: LLE 0x100730ff
// Sets the form event of the given type (open, close, mouse buttons, move, wheel); other types are ignored
void UIForm::SetFormEvent(unsigned long inType, UIEvent* inEvent)
{
    switch (inType) {
    case kUIEventFormOpen:
        this->SetFormOpenEvent(inEvent);
        break;
    case kUIEventFormClose:
        this->SetFormCloseEvent(inEvent);
        break;
    case kUIEventMouseLeftButtonPressed:
        this->SetMouseLeftButtonPressedEvent(inEvent);
        break;
    case kUIEventMouseLeftButtonUnpressed:
        this->SetMouseLeftButtonUnpressedEvent(inEvent);
        break;
    case kUIEventMouseLeftButtonDoublePressed:
        this->SetMouseLeftButtonDoublePressedEvent(inEvent);
        break;
    case kUIEventMouseMiddleButtonPressed:
        this->SetMouseMiddleButtonPressedEvent(inEvent);
        break;
    case kUIEventMouseMiddleButtonUnpressed:
        this->SetMouseMiddleButtonUnpressedEvent(inEvent);
        break;
    case kUIEventMouseMiddleButtonDoublePressed:
        this->SetMouseMiddleButtonDoublePressedEvent(inEvent);
        break;
    case kUIEventMouseRightButtonPressed:
        this->SetMouseRightButtonPressedEvent(inEvent);
        break;
    case kUIEventMouseRightButtonUnpressed:
        this->SetMouseRightButtonUnpressedEvent(inEvent);
        break;
    case kUIEventMouseRightButtonDoublePressed:
        this->SetMouseRightButtonDoublePressedEvent(inEvent);
        break;
    case kUIEventMouseMove:
        this->SetMouseMoveEvent(inEvent);
        break;
    case kUIEventMouseWheelMove:
        this->SetMouseWheelMoveEvent(inEvent);
        break;
    }
}

// FUNCTION: LLE 0x100731cd
// Replaces the keyboard-captured event, releasing the old one and initializing the new one
void UIForm::SetFormKeyboardCapturedEvent(UIEvent* inEvent)
{
    if (this->mFormKeyboardCapturedEvent) {
        this->mFormKeyboardCapturedEvent->Uninitialize();
        this->mFormKeyboardCapturedEvent->Dereference();
    }
    this->mFormKeyboardCapturedEvent = inEvent;
    if (this->mFormKeyboardCapturedEvent) {
        this->mFormKeyboardCapturedEvent->Reference();
        this->mFormKeyboardCapturedEvent->Initialize();
    }
}

// FUNCTION: LLE 0x1007320a
// Replaces the form-open event, releasing the old one and initializing the new one
void UIForm::SetFormOpenEvent(UIEvent* inEvent)
{
    if (this->mFormOpenEvent) {
        this->mFormOpenEvent->Uninitialize();
        this->mFormOpenEvent->Dereference();
    }
    this->mFormOpenEvent = inEvent;
    if (this->mFormOpenEvent) {
        this->mFormOpenEvent->Reference();
        this->mFormOpenEvent->Initialize();
    }
}

// FUNCTION: LLE 0x10073247
// Replaces the form manager, deleting the old one
void UIForm::SetFormManager(UIFormManager* inFormManager)
{
    delete this->mFormManager;
    this->mFormManager = inFormManager;
}

// FUNCTION: LLE 0x10073264
// Replaces the form-update event, releasing the old one and initializing the new one
void UIForm::SetFormUpdateEvent(UIEvent* inEvent)
{
    if (this->mFormUpdateEvent) {
        this->mFormUpdateEvent->Uninitialize();
        this->mFormUpdateEvent->Dereference();
    }
    this->mFormUpdateEvent = inEvent;
    if (this->mFormUpdateEvent) {
        this->mFormUpdateEvent->Reference();
        this->mFormUpdateEvent->Initialize();
    }
}

// FUNCTION: LLE 0x100732a1
// Enables or disables every hot key of a group
void UIForm::SetHotKeyGroupEnabled(unsigned long inGroup, bool inEnabled)
{
    if (inGroup < this->mHotKeys.size())
        for (unsigned long i = 0; i < this->mHotKeys[inGroup].size(); i++)
            this->mHotKeys[inGroup][i]->mEnabled = inEnabled;
}

// FUNCTION: LLE 0x100732f0
// Gives the keyboard to a control and fires the keyboard-captured event
void UIForm::SetKeyboardCaptured(UIControl* inControl)
{
    this->mKeyboardCaptured = inControl;
    if (this->mFormKeyboardCapturedEvent)
        this->mFormKeyboardCapturedEvent->ProcessEvent(this, NULL);
}

// FUNCTION: LLE 0x10073311
// Replaces the left-button-pressed event, releasing the old one and initializing the new one
void UIForm::SetMouseLeftButtonPressedEvent(UIEvent* inEvent)
{
    if (this->mMouseLeftButtonPressedEvent) {
        this->mMouseLeftButtonPressedEvent->Uninitialize();
        this->mMouseLeftButtonPressedEvent->Dereference();
    }
    this->mMouseLeftButtonPressedEvent = inEvent;
    if (this->mMouseLeftButtonPressedEvent) {
        this->mMouseLeftButtonPressedEvent->Reference();
        this->mMouseLeftButtonPressedEvent->Initialize();
    }
}

// FUNCTION: LLE 0x1007334e
// Replaces the left-button-released event, releasing the old one and initializing the new one
void UIForm::SetMouseLeftButtonUnpressedEvent(UIEvent* inEvent)
{
    if (this->mMouseLeftButtonUnpressedEvent) {
        this->mMouseLeftButtonUnpressedEvent->Uninitialize();
        this->mMouseLeftButtonUnpressedEvent->Dereference();
    }
    this->mMouseLeftButtonUnpressedEvent = inEvent;
    if (this->mMouseLeftButtonUnpressedEvent) {
        this->mMouseLeftButtonUnpressedEvent->Reference();
        this->mMouseLeftButtonUnpressedEvent->Initialize();
    }
}

// FUNCTION: LLE 0x1007338b
// Replaces the left-button double-click event, releasing the old one and initializing the new one
void UIForm::SetMouseLeftButtonDoublePressedEvent(UIEvent* inEvent)
{
    if (this->mMouseLeftButtonDoublePressedEvent) {
        this->mMouseLeftButtonDoublePressedEvent->Uninitialize();
        this->mMouseLeftButtonDoublePressedEvent->Dereference();
    }
    this->mMouseLeftButtonDoublePressedEvent = inEvent;
    if (this->mMouseLeftButtonDoublePressedEvent) {
        this->mMouseLeftButtonDoublePressedEvent->Reference();
        this->mMouseLeftButtonDoublePressedEvent->Initialize();
    }
}

// FUNCTION: LLE 0x100733c8
// Replaces the middle-button-pressed event, releasing the old one and initializing the new one
void UIForm::SetMouseMiddleButtonPressedEvent(UIEvent* inEvent)
{
    if (this->mMouseMiddleButtonPressedEvent) {
        this->mMouseMiddleButtonPressedEvent->Uninitialize();
        this->mMouseMiddleButtonPressedEvent->Dereference();
    }
    this->mMouseMiddleButtonPressedEvent = inEvent;
    if (this->mMouseMiddleButtonPressedEvent) {
        this->mMouseMiddleButtonPressedEvent->Reference();
        this->mMouseMiddleButtonPressedEvent->Initialize();
    }
}

// FUNCTION: LLE 0x10073405
// Replaces the middle-button-released event, releasing the old one and initializing the new one
void UIForm::SetMouseMiddleButtonUnpressedEvent(UIEvent* inEvent)
{
    if (this->mMouseMiddleButtonUnpressedEvent) {
        this->mMouseMiddleButtonUnpressedEvent->Uninitialize();
        this->mMouseMiddleButtonUnpressedEvent->Dereference();
    }
    this->mMouseMiddleButtonUnpressedEvent = inEvent;
    if (this->mMouseMiddleButtonUnpressedEvent) {
        this->mMouseMiddleButtonUnpressedEvent->Reference();
        this->mMouseMiddleButtonUnpressedEvent->Initialize();
    }
}

// FUNCTION: LLE 0x10073442
// Replaces the middle-button double-click event, releasing the old one and initializing the new one
void UIForm::SetMouseMiddleButtonDoublePressedEvent(UIEvent* inEvent)
{
    if (this->mMouseMiddleButtonDoublePressedEvent) {
        this->mMouseMiddleButtonDoublePressedEvent->Uninitialize();
        this->mMouseMiddleButtonDoublePressedEvent->Dereference();
    }
    this->mMouseMiddleButtonDoublePressedEvent = inEvent;
    if (this->mMouseMiddleButtonDoublePressedEvent) {
        this->mMouseMiddleButtonDoublePressedEvent->Reference();
        this->mMouseMiddleButtonDoublePressedEvent->Initialize();
    }
}

// FUNCTION: LLE 0x1007347f
// Replaces the mouse-move event, releasing the old one and initializing the new one
void UIForm::SetMouseMoveEvent(UIEvent* inEvent)
{
    if (this->mMouseMoveEvent) {
        this->mMouseMoveEvent->Uninitialize();
        this->mMouseMoveEvent->Dereference();
    }
    this->mMouseMoveEvent = inEvent;
    if (this->mMouseMoveEvent) {
        this->mMouseMoveEvent->Reference();
        this->mMouseMoveEvent->Initialize();
    }
}

// FUNCTION: LLE 0x100734bc
// Changes the control under the mouse, firing mouse-out/over triggers, and restarts the help-text timer with its help
void UIForm::SetMouseOverControl(UIControl* inControl)
{
    UWideString theHelpString;
    if (this->mMouseOverControl)
        this->mMouseOverControl->FireTriggerEvent(kControlTriggerMouseOut);
    this->mMouseOverControl = inControl;
    if (this->mMouseOverControl)
        this->mMouseOverControl->FireTriggerEvent(kControlTriggerMouseOver);
    if (!this->mHelpTextLabel)
        return;
    this->mHelpTextPoll->Stop();
    if (this->mUseToolTips && this->mPopup == this->mHelpTextLabel)
        this->SetPopup(NULL);
    this->mHelpTextLabel->SetDisplayMode(kDisplayModeHidden);
    if (this->mMouseOverControl && this->mMouseOverControl->GetHelpString(theHelpString) == true) {
        this->mHelpTextPoll->Start();
        this->mHelpTextLabel->SetText(theHelpString, true);
    }
}

// FUNCTION: LLE 0x10073577
// Replaces the right-button-pressed event, releasing the old one and initializing the new one
void UIForm::SetMouseRightButtonPressedEvent(UIEvent* inEvent)
{
    if (this->mMouseRightButtonPressedEvent) {
        this->mMouseRightButtonPressedEvent->Uninitialize();
        this->mMouseRightButtonPressedEvent->Dereference();
    }
    this->mMouseRightButtonPressedEvent = inEvent;
    if (this->mMouseRightButtonPressedEvent) {
        this->mMouseRightButtonPressedEvent->Reference();
        this->mMouseRightButtonPressedEvent->Initialize();
    }
}

// FUNCTION: LLE 0x100735b4
// Replaces the right-button-released event, releasing the old one and initializing the new one
void UIForm::SetMouseRightButtonUnpressedEvent(UIEvent* inEvent)
{
    if (this->mMouseRightButtonUnpressedEvent) {
        this->mMouseRightButtonUnpressedEvent->Uninitialize();
        this->mMouseRightButtonUnpressedEvent->Dereference();
    }
    this->mMouseRightButtonUnpressedEvent = inEvent;
    if (this->mMouseRightButtonUnpressedEvent) {
        this->mMouseRightButtonUnpressedEvent->Reference();
        this->mMouseRightButtonUnpressedEvent->Initialize();
    }
}

// FUNCTION: LLE 0x100735f1
// Replaces the right-button double-click event, releasing the old one and initializing the new one
void UIForm::SetMouseRightButtonDoublePressedEvent(UIEvent* inEvent)
{
    if (this->mMouseRightButtonDoublePressedEvent) {
        this->mMouseRightButtonDoublePressedEvent->Uninitialize();
        this->mMouseRightButtonDoublePressedEvent->Dereference();
    }
    this->mMouseRightButtonDoublePressedEvent = inEvent;
    if (this->mMouseRightButtonDoublePressedEvent) {
        this->mMouseRightButtonDoublePressedEvent->Reference();
        this->mMouseRightButtonDoublePressedEvent->Initialize();
    }
}

// FUNCTION: LLE 0x1007362e
// Replaces the mouse-wheel event, releasing the old one and initializing the new one
void UIForm::SetMouseWheelMoveEvent(UIEvent* inEvent)
{
    if (this->mMouseWheelMoveEvent) {
        this->mMouseWheelMoveEvent->Uninitialize();
        this->mMouseWheelMoveEvent->Dereference();
    }
    this->mMouseWheelMoveEvent = inEvent;
    if (this->mMouseWheelMoveEvent) {
        this->mMouseWheelMoveEvent->Reference();
        this->mMouseWheelMoveEvent->Initialize();
    }
}

// FUNCTION: LLE 0x1007366b
// Replaces the popup control, hiding the old one and redrawing what it (and a tool-tip shadow) covered
void UIForm::SetPopup(UIControl* inControl)
{
    GERectangle theRectangle;
    if (this->mPopup) {
        theRectangle = this->mPopup->GetRectangle();
        if (inControl == NULL) {
            if (this->mDrawToolTipShadow && this->mPopup == this->mHelpTextLabel) {
                this->GetToolTipShadowRectangle(this->mPopup->GetRectangle(), theRectangle);
                this->mManager->AddDirtyRectangle(theRectangle, this);
                theRectangle.And(this->mPopup->GetRectangle());
            }
            this->RedrawIntersectControls(theRectangle);
        } else if (this->mPopup != inControl && this->mPopup == this->mHelpTextLabel) {
            if (this->mDrawToolTipShadow) {
                this->GetToolTipShadowRectangle(this->mPopup->GetRectangle(), theRectangle);
                this->mManager->AddDirtyRectangle(theRectangle, this);
                theRectangle.And(this->mPopup->GetRectangle());
            }
            this->RedrawIntersectControls(theRectangle);
            this->mHelpTextLabel->SetDisplayMode(kDisplayModeHidden);
        }
        this->mPopup->HideControl();
    }
    this->mPopup = inControl;
    if (inControl)
        inControl->SetDirty(true);
}

// FUNCTION: LLE 0x1007377d
// Turns keyboard tabbing on or off; turning it on moves the tab stop to the first tabbable control
void UIForm::SetTabbing(bool inTabbing)
{
    this->mTabbingOn = inTabbing;
    if (this->mControls[this->mTabStop])
        this->mControls[this->mTabStop]->TabbedFrom();
    if (inTabbing)
        for (unsigned long i = 0; i < this->mControlCount; i++)
            if (this->mControls[i] && this->mControls[i]->GetTabable()) {
                this->mTabStop = i;
                this->mControls[i]->TabbedTo();
                break;
            }
}

// FUNCTION: LLE 0x100737ed
// Moves the tab stop to a control if it exists and is tabbable; otherwise keeps the old stop and returns false
bool UIForm::SetTabStop(unsigned long inTabStop)
{
    bool theResult = false;
    if (this->mControls[this->mTabStop])
        this->mControls[this->mTabStop]->TabbedFrom();
    if (this->mControls[inTabStop] && this->mControls[inTabStop]->GetTabable()) {
        this->mControls[inTabStop]->TabbedTo();
        this->mTabStop = inTabStop;
        theResult = true;
    } else if (this->mControls[this->mTabStop])
        this->mControls[this->mTabStop]->TabbedTo();
    return theResult;
}

// FUNCTION: LLE 0x1007385e
// Moves the tab stop back to the previous shown, enabled, tabbable control, skipping radio-button runs, wrapping round
void UIForm::ShiftTab()
{
    if (!this->mTabbingOn)
        return;
    unsigned long theTabStop = this->mTabStop;
    long theIndex = theTabStop;
    if (this->mControls[theIndex]->GetType() == kControlTypeRadioButton)
        for (theIndex--; theIndex > 0 && this->mControls[theIndex] && this->mControls[theIndex]->GetType() == kControlTypeRadioButton; theIndex--)
            ;
    else
        theIndex--;
    while (true) {
        if (theIndex < 0)
            theIndex = this->mControlCount - 1;
        if (theIndex == (long)theTabStop)
            break;
        UIControl* theControl = this->mControls[theIndex];
        if (theControl != NULL) {
            if (theControl->GetDisplayMode() == kDisplayModeShown && this->GetEntityGroupById(theControl->GetId())->GetIsEnabled()) {
                this->CheckedRadioButton(theIndex);
                if (theControl->GetTabable()) {
                    this->SetTabStop(theIndex);
                    break;
                }
            }
            if (theControl->GetType() == kControlTypeRadioButton)
                for (theIndex--; theIndex > 0 && this->mControls[theIndex] && this->mControls[theIndex]->GetType() == kControlTypeRadioButton;
                     theIndex--)
                    ;
            else
                theIndex--;
        } else
            theIndex--;
    }
}

// FUNCTION: LLE 0x1007395a
// Moves the tab stop on to the next shown, enabled, tabbable control, skipping radio-button runs, wrapping round
void UIForm::Tab()
{
    if (!this->mTabbingOn)
        return;
    long theTabStop = this->mTabStop;
    long theIndex = theTabStop;
    if (this->mControls[theIndex]->GetType() == kControlTypeRadioButton)
        do
            theIndex++;
        while (theIndex < this->mControlCount && this->mControls[theIndex] && this->mControls[theIndex]->GetType() == kControlTypeRadioButton);
    else
        theIndex++;
    while (true) {
        if (theIndex == this->mControlCount)
            theIndex = 0;
        if (theIndex == theTabStop)
            break;
        UIControl* theControl = this->mControls[theIndex];
        if (theControl == NULL)
            theIndex++;
        else {
            if (theControl->GetDisplayMode() == kDisplayModeShown && this->GetEntityGroupById(theControl->GetId())->GetIsEnabled()) {
                this->CheckedRadioButton(theIndex);
                if (theControl->GetTabable()) {
                    this->SetTabStop(theIndex);
                    break;
                }
            }
            if (theControl->GetType() == kControlTypeRadioButton)
                do
                    theIndex++;
                while (theIndex < this->mControlCount && this->mControls[theIndex] && this->mControls[theIndex]->GetType() == kControlTypeRadioButton);
            else
                theIndex++;
        }
    }
}

// FUNCTION: LLE 0x10073a61
// Unloads every background's texture
void UIForm::UnloadBackgrounds()
{
    for (unsigned long i = 0; i < this->mBackgrounds.size(); i++)
        this->mBackgrounds[i]->UnloadBackground();
}

// FUNCTION: LLE 0x10073a88
// Refreshes the help text from the control under the mouse, re-placing a tool tip that sizes itself to its text
void UIForm::UpdateHelpString()
{
    UWideString theHelpString;
    if (!this->mHelpTextLabel || !this->mMouseOverControl)
        return;
    this->mMouseOverControl->GetHelpString(theHelpString);
    if (this->mUseToolTips && this->mPopup == this->mHelpTextLabel && this->mHelpTextLabel->GetFitRectToText() == true) {
        this->mHelpTextLabel->SetDisplayMode(kDisplayModeHidden);
        this->SetPopup(NULL);
        this->mHelpTextLabel->SetText(theHelpString, true);
        this->UpdateHelpText(true);
    } else
        this->mHelpTextLabel->SetText(theHelpString, true);
}

// FUNCTION: LLE 0x10073b1d
// Places the tool tip by the mouse inside the form: shifted left at the right edge, above the cursor near the bottom
void UIForm::SetHelpTextPopupLocation()
{
    GE2DPoint theLocation;
    theLocation = this->mMouseLocation;
    GERectangle theRectangle = this->mHelpTextLabel->GetRectangle();
    float theWidth = theRectangle.GetWidth();
    if (this->mMouseLocation.GetX() + theWidth > this->mRectangle.GetRight())
        theLocation.SetX(UMFloor(theLocation.GetX()) -
                         (UMFloor(this->mMouseLocation.GetX()) + theWidth - this->mRectangle.GetRight() + 1.0f));
    if (theRectangle.GetHeight() + kHelpTextOffset > this->mRectangle.GetBottom() - this->mMouseLocation.GetY())
        theLocation.SetY(UMFloor(theLocation.GetY()) - theRectangle.GetHeight());
    else
        theLocation.SetY(UMFloor(theLocation.GetY()) + kHelpTextOffset);
    theLocation.SetX(UMFloor(theLocation.GetX() + 0.5f));
    theLocation.SetY(UMFloor(theLocation.GetY() + 0.5f));
    this->mHelpTextLabel->SetLocation(theLocation);
}

// FUNCTION: LLE 0x10073c7d
// Shows the help text once its delay has passed (or when forced), as a popup tool tip when the form uses tool tips
void UIForm::UpdateHelpText(bool inForce)
{
    if (!(inForce == true || this->mHelpTextPoll->GetIsRunning() == true && this->mHelpTextPoll->Poll() > 0))
        return;
    if (this->mUseToolTips) {
        if (this->mPopup == NULL) {
            this->SetHelpTextPopupLocation();
            this->mHelpTextPoll->Stop();
            this->mHelpTextLabel->SetDisplayMode(kDisplayModeShown);
            this->SetPopup(this->mHelpTextLabel);
        }
    } else {
        this->mHelpTextPoll->Stop();
        this->mHelpTextLabel->SetDisplayMode(kDisplayModeShown);
    }
}

// FUNCTION: LLE 0x10073cfa
// Fires hot keys (higher groups first; one non-always-fire key per frame), then Tab/Shift+Tab, else passes the
// keyboard to the capturing control or the tab stop
long UIForm::UpdateKeyboard()
{
    long theResult = kErrorOK;
    std::vector<unsigned char> theGroupFired;
    bool theFired = false;
    bool theSkipGroup = false;
    bool theHandled = false;
    theGroupFired.resize(this->mHotKeys.size(), 0);
    if (this->mKeyboardCaptured == NULL || !this->mKeyboardCaptured->GetIMEEnabled()) {
        for (long i = this->mHotKeys.size() - 1; i >= 0; i--) {
            theGroupFired[i] = 0;
            if (i != this->mHotKeys.size() - 1 && (theSkipGroup || theGroupFired[i + 1]))
                theSkipGroup = true;
            else
                theSkipGroup = false;
            for (long j = this->mHotKeys[i].size() - 1; j >= 0 && !theSkipGroup; j--) {
                UIHotKey* theHotKey = this->mHotKeys[i][j];
                if ((theFired && !theHotKey->GetAlwaysFire()) || !theHotKey->FireEvents())
                    continue;
                theGroupFired[i] = 1;
                if (!theHotKey->GetAlwaysFire())
                    theFired = true;
            }
        }
        if (theFired)
            theHandled = true;
        else if (this->mTabbingOn) {
            if (UIForm::sTabKey->JustPressed() && UIForm::sShiftKey->GetValue()) {
                this->ShiftTab();
                theHandled = true;
            } else if (UIForm::sTabKey->JustPressed()) {
                this->Tab();
                theHandled = true;
            }
        }
    }
    if (!theHandled) {
        if (this->mKeyboardCaptured)
            theResult = this->mKeyboardCaptured->UpdateKeyboard();
        else if (this->mTabbingOn)
            theResult = this->mControls[this->mTabStop]->UpdateKeyboard();
    }
    return theResult;
}

// FUNCTION: LLE 0x10073efa
// Passes the mouse to the captured or mouse-over control (wheel included), then fires the form's own mouse events
long UIForm::UpdateMouse()
{
    long theResult = kErrorOK;
    UIControl* theControl = NULL;
    ISEntity* theXAxis;
    ISEntity* theYAxis;
    ISEntity* theWheel;
    ISEntity* theLeftButton;
    ISEntity* theRightButton;
    ISEntity* theMiddleButton;
    if (this->mMouseCaptured)
        theResult = this->mMouseCaptured->UpdateMouse(this->mMouseLocation);
    else {
        theControl = this->mMouseOverControl;
        if (theControl) {
            theResult = theControl->UpdateMouse(this->mMouseLocation);
            if (theResult == kErrorOK && UIManager::GetMouseWheel()) {
                long theAmount = UIManager::GetMouseWheelScrollAmount();
                if (theAmount) {
                    theResult = theControl->MouseWheelTurned(theAmount);
                    if (theResult != kErrorOK) {
                        theResult = kErrorOK;
                        theControl = NULL;
                    }
                }
            }
        }
    }
    if (this->mMouseCaptured != NULL || theResult != kErrorOK)
        return theResult;
    gISMouse->GetEntity(kMouseXAxis, theXAxis);
    gISMouse->GetEntity(kMouseYAxis, theYAxis);
    gISMouse->GetEntity(kMouseZAxis, theWheel);
    gISMouse->GetEntity(kMouseLeftButton, theLeftButton);
    gISMouse->GetEntity(kMouseRightButton, theRightButton);
    gISMouse->GetEntity(kMouseMiddleButton, theMiddleButton);
    if (this->mMouseMoveEvent && (theXAxis->GetValue() || theYAxis->GetValue()))
        theResult = this->mMouseMoveEvent->ProcessEvent(this, NULL);
    if (this->mMouseWheelMoveEvent && theControl == NULL && theWheel && theWheel->GetValue())
        theResult = this->mMouseWheelMoveEvent->ProcessEvent(this, NULL);
    if (this->mMouseLeftButtonPressedEvent && theLeftButton->JustPressed())
        theResult = this->mMouseLeftButtonPressedEvent->ProcessEvent(this, NULL);
    if (this->mMouseLeftButtonUnpressedEvent && theLeftButton->JustUnpressed())
        theResult = this->mMouseLeftButtonUnpressedEvent->ProcessEvent(this, NULL);
    if (this->mMouseLeftButtonDoublePressedEvent && theLeftButton->JustDoublePressed())
        theResult = this->mMouseLeftButtonDoublePressedEvent->ProcessEvent(this, NULL);
    if (theMiddleButton) {
        if (this->mMouseMiddleButtonPressedEvent && theMiddleButton->JustPressed())
            theResult = this->mMouseMiddleButtonPressedEvent->ProcessEvent(this, NULL);
        if (this->mMouseMiddleButtonUnpressedEvent && theMiddleButton->JustUnpressed())
            theResult = this->mMouseMiddleButtonUnpressedEvent->ProcessEvent(this, NULL);
        if (this->mMouseMiddleButtonDoublePressedEvent && theMiddleButton->JustDoublePressed())
            theResult = this->mMouseMiddleButtonDoublePressedEvent->ProcessEvent(this, NULL);
    }
    if (this->mMouseRightButtonPressedEvent && theRightButton->JustPressed())
        theResult = this->mMouseRightButtonPressedEvent->ProcessEvent(this, NULL);
    if (this->mMouseRightButtonUnpressedEvent && theRightButton->JustUnpressed())
        theResult = this->mMouseRightButtonUnpressedEvent->ProcessEvent(this, NULL);
    if (this->mMouseRightButtonDoublePressedEvent && theRightButton->JustDoublePressed())
        theResult = this->mMouseRightButtonDoublePressedEvent->ProcessEvent(this, NULL);
    return theResult;
}

// FUNCTION: LLE 0x1007419d
// Records the mouse location, finds the control under it, hides a tool tip the mouse enters, updates the mouse-over
void UIForm::UpdateMouseLocation(const GE2DPoint& inLocation)
{
    UIControl* theControl = NULL;
    bool theFound = false;
    this->mMouseLocation = inLocation;
    if (this->mMouseCaptured && this->mMouseCaptured->GetRectangle().Contains(inLocation))
        theControl = this->mMouseCaptured;
    else {
        for (long i = this->mControlGroupCount - 1; i >= 0 && !theFound; i--) {
            UIEntityGroup* theGroup = this->mEntityGroups[i];
            if (theGroup->GetIsEnabled())
                for (unsigned long j = 0; j < theGroup->GetEntityCount() && !theFound; j++) {
                    UIControl* theEntity = static_cast<UIControl*>(theGroup->GetEntity(j));
                    if (theEntity->GetDisplayMode() != kDisplayModeHidden && theEntity->GetRectangle().Contains(inLocation)) {
                        theControl = theEntity;
                        theFound = true;
                    }
                }
        }
        if (this->mUseToolTips && this->GetHelpTextLabel() && this->mPopup == this->GetHelpTextLabel() &&
            this->GetHelpTextLabel()->GetRectangle().Contains(this->mMouseLocation)) {
            this->mHelpTextPoll->Stop();
            this->mHelpTextLabel->SetDisplayMode(kDisplayModeHidden);
            this->SetPopup(NULL);
        }
    }
    if (this->mMouseOverControl != theControl)
        this->SetMouseOverControl(theControl);
}

// FUNCTION: LLE 0x100742c7
// Passes a typed character to the control holding the keyboard
long UIForm::UpdateTranslatedKey(const char* inKey, short inRepeat)
{
    long theResult = kErrorOK;
    if (this->mKeyboardCaptured)
        theResult = this->mKeyboardCaptured->UpdateTranslatedKey(inKey, inRepeat);
    return theResult;
}

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// TEMPLATE: LLE 0x10074e05
// TEMPLATE: LLE 0x100750fe
// TEMPLATE: LLE 0x1007521f
// TEMPLATE: LLE 0x1001c11b
// TEMPLATE: LLE 0x1001bec7
// TEMPLATE: LLE 0x1001c050
// TEMPLATE: LLE 0x1005ab0f
// TEMPLATE: LLE 0x1005abb3
// TEMPLATE: LLE 0x1005b0c2
// TEMPLATE: LLE 0x1005b19e
// TEMPLATE: LLE 0x1005b9b2
// TEMPLATE: LLE 0x10064015
// TEMPLATE: LLE 0x1007430c
// TEMPLATE: LLE 0x10074380
// TEMPLATE: LLE 0x10074497
// TEMPLATE: LLE 0x10074adb
// TEMPLATE: LLE 0x10074bbb
// TEMPLATE: LLE 0x10074bc9
// SYNTHETIC: LLE 0x10074cde
// TEMPLATE: LLE 0x10074d1b
// TEMPLATE: LLE 0x10074f33
// TEMPLATE: LLE 0x10074f76
// TEMPLATE: LLE 0x100752f2
// TEMPLATE: LLE 0x1007456d
// TEMPLATE: LLE 0x10074bff
// TEMPLATE: LLE 0x10074c23
// TEMPLATE: LLE 0x10074c4f
// TEMPLATE: LLE 0x10074ef2
// TEMPLATE: LLE 0x10075187
// TEMPLATE: LLE 0x100752a1
// TEMPLATE: LLE 0x1007b9f7
// SYNTHETIC: LLE 0x1007042f
// SYNTHETIC: LLE 0x10070439
// SYNTHETIC: LLE 0x10070449
// SYNTHETIC: LLE 0x10070455
// SYNTHETIC: LLE 0x1007045f
// SYNTHETIC: LLE 0x10070469
// SYNTHETIC: LLE 0x10070479
// SYNTHETIC: LLE 0x10070485
// SYNTHETIC: LLE 0x1007048f
// SYNTHETIC: LLE 0x10070499
// SYNTHETIC: LLE 0x100704a9
// SYNTHETIC: LLE 0x100704b5
// SYNTHETIC: LLE 0x100704bf
// SYNTHETIC: LLE 0x100704c9
// SYNTHETIC: LLE 0x100704d9
// SYNTHETIC: LLE 0x100704e5
// SYNTHETIC: LLE 0x100704ef
// SYNTHETIC: LLE 0x100704f9
// SYNTHETIC: LLE 0x10070509
// SYNTHETIC: LLE 0x10070515
// SYNTHETIC: LLE 0x1007051f
// SYNTHETIC: LLE 0x10070529
// SYNTHETIC: LLE 0x10070539
// SYNTHETIC: LLE 0x10070545
// SYNTHETIC: LLE 0x1007054f
// SYNTHETIC: LLE 0x100705ec
// SYNTHETIC: LLE 0x1007069d
// SYNTHETIC: LLE 0x100706fc
// SYNTHETIC: LLE 0x10070706
// SYNTHETIC: LLE 0x10070716
// SYNTHETIC: LLE 0x10070722
// SYNTHETIC: LLE 0x100713b2

// Library COMDATs first emitted here (vector<vector<UIHotKey*>>, vector<unsigned char>, the UIHotKeySorter sort
// internals; the unnamed ones are left for the <algorithm> rebuild)
// TEMPLATE: LLE 0x100742e6
// TEMPLATE: LLE 0x10074319
// TEMPLATE: LLE 0x10074354
// TEMPLATE: LLE 0x100743e4
// TEMPLATE: LLE 0x10074539
// TEMPLATE: LLE 0x10074821
// TEMPLATE: LLE 0x10074a88
// TEMPLATE: LLE 0x10074acc
// TEMPLATE: LLE 0x10074c7b
// TEMPLATE: LLE 0x10074ca0
// TEMPLATE: LLE 0x10074cc1
// TEMPLATE: LLE 0x10074ce9
// TEMPLATE: LLE 0x10074e41
// TEMPLATE: LLE 0x10074f2d
// TEMPLATE: LLE 0x10074fbd
// TEMPLATE: LLE 0x100750ce
// TEMPLATE: LLE 0x10075155
// TEMPLATE: LLE 0x100751ac
// TEMPLATE: LLE 0x1007528e
