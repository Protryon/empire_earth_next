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
#include "LostDebug.h"  // NOT original source: EE_LOST_DEBUG filler (Pop, Push; docs/LOST_DEBUG.md)

ISEntity* UIManager::sLMouseButton;
ISEntity* UIManager::sRMouseButton;
ISEntity* UIManager::sMouseWheel;
bool UIManager::sMouseLeftHanded;

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
    EE_LOST_DEBUG(1);  // LOST-DEBUG: none: exact only (the original calls both mForms[i] out of line; window 1..2)
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

// FUNCTION: LLE 0x10079be5
// Makes visible again every stacked form below the given visible form that only that form was covering
void UIManager::AddClippedForms(unsigned long inIndex)
{
    std::vector<UIForm*> theForms;
    unsigned long i;
    for (i = 0; this->mFormStack[i] != this->mVisibleForms[inIndex]; i++)
        if (this->mVisibleForms[inIndex]->GetRectangle().Contains(this->mFormStack[i]->GetRectangle())) {
            unsigned long j;
            for (j = i + 1; ; j++) {
                if (this->mFormStack[j] == this->mVisibleForms[inIndex]) {
                    theForms.push_back(this->mFormStack[i]);
                    break;
                }
                if (this->mFormStack[j]->GetRectangle().Contains(this->mFormStack[i]->GetRectangle()))
                    break;
            }
        }
    for (unsigned long k = 0; k < theForms.size(); k++)
        this->AddToVisibleForms(theForms[k]);
}

// FUNCTION: LLE 0x10079e1f
// Inserts a stacked form into the visible list in stack order and opens it; returns its index (forms not on the
// stack are not added)
unsigned long UIManager::AddToVisibleForms(UIForm* inForm)
{
    unsigned long theIndex = 0;
    if (this->mVisibleForms.size() == 0 && inForm == this->mFormStack.back()) {
        // Fixed: compared mVisibleForms.back() of the empty list with the form here (a read before the list's
        // start); an empty list never holds the form, so it is simply added below
    } else {
        unsigned long i;
        for (i = 0; i < this->mFormStack.size(); i++) {
            if (this->mFormStack[i] == inForm) {
                if (this->mVisibleForms[theIndex] != inForm) {
                    this->mVisibleForms.insert(this->mVisibleForms.begin() + theIndex, inForm);
                    inForm->Open();
                }
                return theIndex;
            }
            if (this->mFormStack[i] != this->mVisibleForms[theIndex])
                continue;
            theIndex++;
            if (theIndex == this->mVisibleForms.size())
                break;
        }
        if (i == this->mFormStack.size())
            return theIndex;
    }
    theIndex = this->mVisibleForms.size();
    this->mVisibleForms.push_back(inForm);
    inForm->Open();
    return theIndex;
}

// FUNCTION: LLE 0x10079f7f
// Marks a rectangle dirty: on the top form directly, else on that form and every visible form above that overlaps it
void UIManager::AddDirtyRectangle(const GERectangle& inRectangle, UIForm* inForm)
{
    GERectangle theRectangle;
    if (this->mFormStack.size() <= 0)
        return;
    if (inForm == this->mFormStack.back())
        inForm->AddDirtyRectangle(inRectangle);
    else {
        for (unsigned long i = 0; i < this->mVisibleForms.size(); i++)
            if (this->mVisibleForms[i] == inForm) {
                for (; i < this->mVisibleForms.size(); i++)
                    if (this->mVisibleForms[i]->GetRectangle().And(inRectangle, theRectangle) == true) {
                        this->mVisibleForms[i]->AddDirtyRectangle(theRectangle);
                        this->mVisibleForms[i]->RedrawIntersectControls(theRectangle);
                    }
                break;
            }
    }
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
            new UITiledBackground(theTable->GetID(), theTable->GetTextureFile(), theDescription,
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
    unsigned long i;
    for (i = 0; i < this->mForms.size(); i++)
        if (this->mForms[i]->GetFormID() == inID)
            break;
    if (i == this->mForms.size())
        return NULL;   // Fixed: an unknown ID used to return the word one past the end of the form list
    return this->mForms[i];
}

// FUNCTION: LLE 0x1007a766
// Returns the mouse wheel's scroll step: -1 when rolled forward, 1 when rolled back, 0 when still
long UIManager::GetMouseWheelScrollAmount()
{
    long theResult = 0;
    long theValue = UIManager::sMouseWheel->GetNormalizedValue();
    if (theValue)
        theResult = theValue > 0 ? -1 : 1;
    return theResult;
}

// FUNCTION: LLE 0x1007a78a
// Loads the UI database (fonts, forms, backgrounds, controls, control and form events, hot keys) from a directory
void UIManager::LoadDatabase(FSPath* inPath, const UString& inDirectory)
{
    FSFileSpec* theDbFile = NULL;
    long theResult;

    theResult = inPath->FindFile(inDirectory + gDbUIFontsFileName, theDbFile);
    ESVERIFY_AT(theResult == keseOK, 1081);
    ESVERIFY_AT(theDbFile != NULL, 1082);
    theResult = UIFontTable::Create(*theDbFile, this);
    ESVERIFY_AT(theResult == keseOK, 1084);
    delete theDbFile;

    theResult = inPath->FindFile(inDirectory + gDbUIFormsFileName, theDbFile);
    ESVERIFY_AT(theResult == keseOK, 1089);
    ESVERIFY_AT(theDbFile != NULL, 1090);
    theResult = UIForm::Create(*theDbFile, this);
    ESVERIFY_AT(theResult == keseOK, 1092);
    delete theDbFile;

    theResult = inPath->FindFile(inDirectory + gDbUIBackgroundFileName, theDbFile);
    ESVERIFY_AT(theResult == keseOK, 1097);
    ESVERIFY_AT(theDbFile != NULL, 1098);
    theResult = UIBackgroundTable::Create(*theDbFile);
    ESVERIFY_AT(theResult == keseOK, 1100);
    this->CreateBackgrounds();
    delete theDbFile;

    theResult = inPath->FindFile(inDirectory + gDbUIControlsFileName, theDbFile);
    ESVERIFY_AT(theResult == keseOK, 1106);
    ESVERIFY_AT(theDbFile != NULL, 1107);
    UIControlsTable::Create(*theDbFile, this);
    delete theDbFile;

    theResult = inPath->FindFile(inDirectory + gDbUIControlEventsFileName, theDbFile);
    ESVERIFY_AT(theResult == keseOK, 1113);
    ESVERIFY_AT(theDbFile != NULL, 1114);
    UIEvent::CreateControlEvents(*theDbFile);
    delete theDbFile;

    theResult = inPath->FindFile(inDirectory + gDbUIFormEventsFileName, theDbFile);
    ESVERIFY_AT(theResult == keseOK, 1120);
    ESVERIFY_AT(theDbFile != NULL, 1121);
    UIEvent::CreateFormEvents(*theDbFile, this);
    delete theDbFile;

    theResult = inPath->FindFile(inDirectory + gDbUIHotKeyFileName, theDbFile);
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
    EE_LOST_DEBUG(1);  // LOST-DEBUG (PROVISIONAL, owner experiment 2026-10-24): no independent evidence; base-game LLE identical (exact window 1..7)
    this->mCriticalSection->Enter();
    this->mDrawCriticalSection->Enter();
    if (this->mFormStack.size() > 0) {
        UIForm* theForm = this->mFormStack[this->mFormStack.size() - 1];
        this->mCopiedForms.clear();
        theForm->Deactivate();
        theForm->Close();
        this->AddClippedForms(this->mVisibleForms.size() - 1);
        this->AddDirtyRectangle(theForm->GetRectangle(), this->mVisibleForms.front());
        this->mFormStack.pop_back();
        this->mVisibleForms.pop_back();
        if (this->mFormStack.size() > 0)
            this->mFormStack.back()->Activate();
    }
    this->ResetClippedForms();
    this->mDrawCriticalSection->Leave();
    this->mCriticalSection->Leave();
    return kErrorOK;
}

// FUNCTION: LLE 0x1007adbe
// Pops forms until the given one is on top, when it is on the stack; returns the first failure
long UIManager::PopTo(UIForm* inForm)
{
    long theResult = kErrorOK;
    this->mCriticalSection->Enter();
    this->mDrawCriticalSection->Enter();
    if (std::find(this->mFormStack.begin(), this->mFormStack.end(), inForm) != this->mFormStack.end())
        while (this->GetCurrentForm() != inForm) {
            theResult = this->Pop();
            if (theResult != kErrorOK)
                break;
        }
    this->mDrawCriticalSection->Leave();
    this->mCriticalSection->Leave();
    return theResult;
}

// FUNCTION: LLE 0x1007ae53
// Opens a form on top of the stack (unless already stacked), deactivating the old top; returns OK
long UIManager::Push(UIForm* inForm)
{
    EE_LOST_DEBUG(5);  // LOST-DEBUG (PROVISIONAL, owner experiment 2026-10-24): no independent evidence; base-game LLE identical (exact window 5..12)
    bool theFound = false;
    this->mCriticalSection->Enter();
    this->mDrawCriticalSection->Enter();
    for (unsigned long i = 0; i < this->mFormStack.size(); i++)
        if (this->mFormStack[i] == inForm) {
            theFound = true;
            break;
        }
    if (!theFound) {
        this->mCopiedForms.clear();
        if (this->mFormStack.size() > 0)
            this->mFormStack.back()->Deactivate();
        this->mFormStack.push_back(inForm);
        this->mVisibleForms.push_back(inForm);
        inForm->Open();
        inForm->Activate();
        this->RemoveClippedForms(this->mVisibleForms.size() - 1);
        this->AddDirtyRectangle(inForm->GetRectangle(), inForm);
        this->ResetClippedForms();
    }
    this->mDrawCriticalSection->Leave();
    this->mCriticalSection->Leave();
    return kErrorOK;
}

// FUNCTION: LLE 0x1007af47
// Marks every visible form and all its controls dirty
void UIManager::RedrawAll()
{
    this->mCriticalSection->Enter();
    for (unsigned long i = 0; i < this->mVisibleForms.size(); i++) {
        this->mVisibleForms[i]->AddDirtyRectangle(this->mVisibleForms[i]->GetRectangle());
        this->mVisibleForms[i]->SetAllControlsDirty();
    }
    this->mCriticalSection->Leave();
}

// FUNCTION: LLE 0x1007af90
// Closes the visible forms below the given one that it fully covers, and the first form above that contains it
// (when there is none, redraws the given form's area)
void UIManager::RemoveClippedForms(unsigned long inIndex)
{
    GERectangle theRectangle = this->mVisibleForms[inIndex]->GetRectangle();
    bool theRemoved = false;
    long i;
    for (i = inIndex + 1; i < (long)this->mVisibleForms.size(); i++)
        if (this->mVisibleForms[i]->GetRectangle().Contains(theRectangle)) {
            this->mVisibleForms[i]->Close();
            this->mVisibleForms.erase(this->mVisibleForms.begin() + i);
            theRemoved = true;
            break;
        }
    if (!theRemoved)
        this->AddDirtyRectangle(theRectangle, this->mVisibleForms[inIndex]);
    for (i = inIndex - 1; i >= 0; i--)
        if (theRectangle.Contains(this->mVisibleForms[i]->GetRectangle())) {
            this->mVisibleForms[i]->Close();
            this->mVisibleForms.erase(this->mVisibleForms.begin() + i);
        }
}

// FUNCTION: LLE 0x1007b147
// Rebuilds each visible form's clip rectangles from the visible forms above it
void UIManager::ResetClippedForms()
{
    for (unsigned long i = 0; i < this->mVisibleForms.size(); i++) {
        this->mVisibleForms[i]->ClearAllClipRectangles();
        for (unsigned long j = i + 1; j < this->mVisibleForms.size(); j++)
            this->mVisibleForms[i]->AddClipRectangle(this->mVisibleForms[j]->GetRectangle());
    }
}

// FUNCTION: LLE 0x1007b1a2
// Re-lays out every stacked form to the window size and rebuilds the clip rectangles
void UIManager::RelayoutAllForms()
{
    GERectangle theRectangle(0.0f, 0.0f, (float)this->mRasterizer->GetWindow(this->mScreen)->GetWidth() - 1.0f,
                             (float)this->mRasterizer->GetWindow(this->mScreen)->GetHeight() - 1.0f);
    for (unsigned long i = 0; i < this->mFormStack.size(); i++)
        this->mFormStack[i]->RelayoutForm(theRectangle);
    this->ResetClippedForms();
}

// FUNCTION: LLE 0x1007b254
// Runs and releases the queued events of one form; the other forms' events stay queued in order
void UIManager::ProcessAllEventsForForm(UIForm* inForm)
{
    std::deque<UIEventTrigger> theOtherEvents;
    while (this->mEventsToTrigger.size() != 0) {
        UIEventTrigger theTrigger = this->mEventsToTrigger.front();
        this->mEventsToTrigger.pop_front();
        if (theTrigger.mForm != inForm)
            theOtherEvents.push_back(theTrigger);
        else {
            theTrigger.mEvent->ProcessEvent(theTrigger.mForm, theTrigger.mEntity);
            theTrigger.mEvent->Dereference();
        }
    }
    this->mEventsToTrigger = theOtherEvents;
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
    bool theFound = false;
    this->mCriticalSection->Enter();
    this->mDrawCriticalSection->Enter();
    for (unsigned long i = 0; i < this->mFormStack.size(); i++)
        if (this->mFormStack[i] == inForm) {
            theFound = true;
            break;
        }
    if (!theFound) {
        this->mCopiedForms.clear();
        if (this->mFormStack.size() > 0) {
            UIForm* theForm = this->mFormStack.back();
            theForm->Deactivate();
            theForm->Close();
            inForm->Open();
            inForm->Activate();
            if (!inForm->GetRectangle().Contains(this->mVisibleForms.back()->GetRectangle())) {
                this->AddClippedForms(this->mVisibleForms.size() - 1);
                this->AddDirtyRectangle(theForm->GetRectangle(), this->mVisibleForms.front());
            }
            this->mFormStack.pop_back();
            this->mVisibleForms.pop_back();
        } else {
            inForm->Open();
            inForm->Activate();
        }
        this->mFormStack.push_back(inForm);
        this->mVisibleForms.push_back(inForm);
        this->RemoveClippedForms(this->mVisibleForms.size() - 1);
        this->AddDirtyRectangle(inForm->GetRectangle(), inForm);
        this->ResetClippedForms();
    }
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
// Loads and starts a music track unless it is the current one; the check lower-cases the name but the track
// name is stored as given, so a mixed-case name always reloads
void UIManager::StartMusic(const UWideString& inFileName)
{
    UWideString theFileName(inFileName);
    if (!this->mSoundManager)
        return;
    _mbslwr((unsigned char*)(const char*)theFileName);
    if (this->mMusicFileName != theFileName) {
        this->mStopMusic = true;
        if (this->mMusicID != -1) {
            this->RemoveAndStopSoundInstance();
            this->mSoundManager->Unload(this->mMusicID);
        }
        this->mMusicID = this->mSoundManager->Load((const char*)inFileName, 0, 1);
        if (this->mMusicID != -1) {
            this->mMusicFileName = inFileName;
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
    while (this->mEventsToTrigger.size() != 0) {
        UIEventTrigger theTrigger = this->mEventsToTrigger.front();
        this->mEventsToTrigger.pop_front();
        theTrigger.mEvent->ProcessEvent(theTrigger.mForm, theTrigger.mEntity);
        theTrigger.mEvent->Dereference();
    }
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
