// C:\EEX\Low-Level Engine\User Interface\UIControlsTable.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10065f69..0x10069cc7: UIControlsTable's header-inline block (0x10065f69, declaration order; the getters ICF
// folded into earlier units are missing), its implicit copy members and ??_E, the U2D key init chains (0x1006672c),
// then the header-inline/implicit copy members and ??_E of every control class this unit includes first
// (UIDualButton, UIFileComboBox, UIFileListBox, UIGraph, UILineGraph, UIMovie, UITLScrolling, UIStatusBar,
// UIFontTable), the sUIControlsArray init chain (0x10067792), the ctors, the dtor and the members in alphabetical
// order, then the vector<UString> COMDATs (0x10069ad4..). UIDualButton.cpp starts at 0x10069cc7.
#include <windows.h>

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
#include "lle/GEFont.h"
#include "lle/GEFontBitmap.h"
#include "lle/GEText.h"
#include "lle/GELine.h"
#include "lle/GEMovie.h"
#include "lle/FSFileSpec.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/UResourceFile.h"
#include "lle/UGraph.h"
#include "lle/UILayoutUtility.h"
#include "lle/UIEntity.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIControl.h"
#include "lle/UIFormManager.h"
#include "lle/UIForm.h"
#include "lle/UIControlsTable.h"
#include "lle/UIBitmap.h"
#include "lle/UIButton.h"
#include "lle/UIBlinkingButton.h"
#include "lle/UISlider.h"
#include "lle/UIScrollButton.h"
#include "lle/UIScrollBar.h"
#include "lle/UITextLabel.h"
#include "lle/UICheckBox.h"
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/UILBString.h"
#include "lle/UIComboBox.h"
#include "lle/UIDualButton.h"
#include "lle/UIFileComboBox.h"
#include "lle/UIFileListBox.h"
#include "lle/UIGraph.h"
#include "lle/UILineGraph.h"
#include "lle/UIMovie.h"
#include "lle/UIRadioButton.h"
#include "lle/UIScalableButton.h"
#include "lle/UITLScrolling.h"
#include "lle/UIStatusBar.h"
#include "lle/UIFontTable.h"
#include "lle/UIManager.h"
#include "LostDebug.h"  // NOT original source: EE_LOST_DEBUG filler (CreateBitmap; docs/LOST_DEBUG.md)

// One record of the controls database file (Create reads them with FSFileSpec::Read, 0x35c bytes each). The
// offsets are proven by the UIDbControl ctor 0x1006796f (provisional field names).
struct UIDbControl {
    char mName[100];                        // +0x000
    char mNormalFileName[100];              // +0x064
    char mPressedFileName[100];             // +0x0c8
    char mFocusedFileName[100];             // +0x12c
    char mButtonDownSound[100];             // +0x190
    char mButtonUpSound[100];               // +0x1f4
    float mBackgroundOpacity;               // +0x258
    float mTextBoxOpacity;                  // +0x25c
    float mScrollBarBgOpacity;              // +0x260
    unsigned long mUnknown264[5];           // +0x264
    float mLeftOffset;                      // +0x278
    float mRightOffset;                     // +0x27c
    float mBottomOffset;                    // +0x280
    float mTopOffset;                       // +0x284
    long mUnknown288;                       // +0x288
    long mID;                               // +0x28c
    long mFormID;                           // +0x290
    unsigned long mGroup;                   // +0x294
    unsigned long mControlType;             // +0x298
    long mFormIndex;                        // +0x29c
    long mBorderThickness;                  // +0x2a0
    unsigned long mLeftOffsetType;          // +0x2a4
    unsigned long mRightOffsetType;         // +0x2a8
    unsigned long mTopOffsetType;           // +0x2ac
    unsigned long mBottomOffsetType;        // +0x2b0
    unsigned long mBackgroundRed;           // +0x2b4
    unsigned long mBackgroundGreen;         // +0x2b8
    unsigned long mBackgroundBlue;          // +0x2bc
    long mStringTableBegin;                 // +0x2c0
    long mStringTableEnd;                   // +0x2c4
    unsigned long mFontTableIndex;          // +0x2c8
    unsigned long mTextForegroundRed;       // +0x2cc
    unsigned long mTextForegroundGreen;     // +0x2d0
    unsigned long mTextForegroundBlue;      // +0x2d4
    long mTextAlignment;                    // +0x2d8
    long mNormalLeft;                       // +0x2dc
    long mNormalRight;                      // +0x2e0
    long mNormalTop;                        // +0x2e4
    long mNormalBottom;                     // +0x2e8
    long mPressedLeft;                      // +0x2ec
    long mPressedRight;                     // +0x2f0
    long mPressedTop;                       // +0x2f4
    long mPressedBottom;                    // +0x2f8
    long mFocusedLeft;                      // +0x2fc
    long mFocusedRight;                     // +0x300
    long mFocusedTop;                       // +0x304
    long mFocusedBottom;                    // +0x308
    long mMaxChars;                         // +0x30c
    unsigned long mSelectedListBoxRed;      // +0x310
    unsigned long mSelectedListBoxGreen;    // +0x314
    unsigned long mSelectedListBoxBlue;     // +0x318
    long mStartCurrentSelection;            // +0x31c
    unsigned long mFocusedLineRed;          // +0x320
    unsigned long mFocusedLineGreen;        // +0x324
    unsigned long mFocusedLineBlue;         // +0x328
    long mStartRange;                       // +0x32c
    long mEndRange;                         // +0x330
    long mCurrentRange;                     // +0x334
    long mPageSize;                         // +0x338
    long mStringTableID;                    // +0x33c
    long mBlinkRate;                        // +0x340
    unsigned long mUnknown344[3];           // +0x344
    bool mIsVisible;                        // +0x350
    bool mStartChecked;                     // +0x351
    bool mListBoxAlwaysDirty;               // +0x352
    bool mSortListBox;                      // +0x353
    bool mFocusedHasAlpha;                  // +0x354
    bool mPressedHasAlpha;                  // +0x355
    bool mNormalHasAlpha;                   // +0x356
    bool mAlwaysDirty;                      // +0x357
    bool mBoolPlaceholder2;                 // +0x358
    bool mBoolPlaceholder3;                 // +0x359
    bool mBoolPlaceholder4;                 // +0x35a
};

std::vector<UIControlsTable*> UIControlsTable::sUIControlsArray;

// FUNCTION: LLE 0x100677db
// Creates an empty control description (a bitmap with no font, files or layout)
UIControlsTable::UIControlsTable()
    : mNormalFileName(50), mPressedFileName(50), mFocusedFileName(50)
{
    this->mBackgroundOpacity = 0.0f;
    this->mTextBoxOpacity = 0.0f;
    this->mScrollBarBgOpacity = 0.0f;
    this->mUnknown100 = 0;
    this->mID = 0;
    this->mFormID = 0;
    this->mGroup = 0;
    this->mControlType = kDbControlBitmap;
    this->mFormIndex = 0;
    this->mBorderThickness = 0;
    this->mStringTableBegin = 0;
    this->mStringTableEnd = 0;
    this->mFont = NULL;
    this->mTextAlignment = 0;
    this->mMaxChars = 0;
    this->mStartCurrentSelection = 0;
    this->mStartRange = 0;
    this->mEndRange = 0;
    this->mCurrentRange = 0;
    this->mIsVisible = false;
    this->mStartChecked = false;
    this->mListBoxAlwaysDirty = false;
    this->mSortListBox = false;
    this->mFocusedHasAlpha = false;
    this->mPressedHasAlpha = false;
    this->mNormalHasAlpha = false;
    this->mFontTableIndex = 0;
    this->mBlinkRate = 0;
}

// FUNCTION: LLE 0x1006796f
// Creates a control description from a controls database record: layout, files, sounds, colours and texture rects
UIControlsTable::UIControlsTable(const UIDbControl& inControl)
{
    this->mBackgroundOpacity = inControl.mBackgroundOpacity;
    this->mTextBoxOpacity = inControl.mTextBoxOpacity;
    this->mScrollBarBgOpacity = inControl.mScrollBarBgOpacity;
    this->mUnknown100 = inControl.mUnknown288;
    this->mID = inControl.mID;
    this->mFormID = inControl.mFormID;
    this->mGroup = inControl.mGroup;
    this->mControlType = inControl.mControlType;
    this->mFormIndex = inControl.mFormIndex;
    this->mBorderThickness = inControl.mBorderThickness;
    this->mLayout.SetTop(inControl.mTopOffsetType, inControl.mTopOffset);
    this->mLayout.SetBottom(inControl.mBottomOffsetType, inControl.mBottomOffset);
    this->mLayout.SetLeft(inControl.mLeftOffsetType, inControl.mLeftOffset);
    this->mLayout.SetRight(inControl.mRightOffsetType, inControl.mRightOffset);
    this->mStringTableBegin = inControl.mStringTableBegin;
    this->mStringTableEnd = inControl.mStringTableEnd;
    this->mFont = NULL;
    this->mTextAlignment = inControl.mTextAlignment;
    this->mMaxChars = inControl.mMaxChars;
    this->mStartCurrentSelection = inControl.mStartCurrentSelection;
    this->mStartRange = inControl.mStartRange;
    this->mEndRange = inControl.mEndRange;
    this->mCurrentRange = inControl.mCurrentRange;
    this->mIsVisible = inControl.mIsVisible;
    this->mStartChecked = inControl.mStartChecked;
    this->mListBoxAlwaysDirty = inControl.mListBoxAlwaysDirty;
    this->mSortListBox = inControl.mSortListBox;
    this->mFocusedHasAlpha = inControl.mFocusedHasAlpha;
    this->mPressedHasAlpha = inControl.mPressedHasAlpha;
    this->mNormalHasAlpha = inControl.mNormalHasAlpha;
    this->mAlwaysDirty = inControl.mAlwaysDirty;
    this->mBoolPlaceholder2 = inControl.mBoolPlaceholder2;
    this->mBoolPlaceholder3 = inControl.mBoolPlaceholder3;
    this->mFontTableIndex = inControl.mFontTableIndex;
    this->mPageSize = inControl.mPageSize;
    this->mBlinkRate = inControl.mBlinkRate;
    this->mBoolPlaceholder4 = inControl.mBoolPlaceholder4;
    if (inControl.mStringTableID > 0)
        this->mStringTableID = inControl.mStringTableID;
    else
        this->mStringTableID = 0;
    this->mNormalFileName.Copy(inControl.mNormalFileName, 0, strlen(inControl.mNormalFileName), 0, true);
    this->mPressedFileName.Copy(inControl.mPressedFileName, 0, strlen(inControl.mPressedFileName), 0, true);
    this->mFocusedFileName.Copy(inControl.mFocusedFileName, 0, strlen(inControl.mFocusedFileName), 0, true);
    this->mButtonDownSound.Copy(inControl.mButtonDownSound, 0, strlen(inControl.mButtonDownSound), 0, true);
    this->mButtonUpSound.Copy(inControl.mButtonUpSound, 0, strlen(inControl.mButtonUpSound), 0, true);
    this->CalculateColor(this->mBackground, inControl.mBackgroundRed, inControl.mBackgroundGreen, inControl.mBackgroundBlue);
    this->CalculateColor(this->mTextForeground, inControl.mTextForegroundRed, inControl.mTextForegroundGreen,
                   inControl.mTextForegroundBlue);
    this->CalculateColor(this->mSelectedListBox, inControl.mSelectedListBoxRed, inControl.mSelectedListBoxGreen,
                   inControl.mSelectedListBoxBlue);
    this->CalculateColor(this->mFocusedLineColor, inControl.mFocusedLineRed, inControl.mFocusedLineGreen,
                   inControl.mFocusedLineBlue);
    this->mNormalTextureRect = GERectangle(inControl.mNormalLeft, inControl.mNormalTop, inControl.mNormalRight,
                                     inControl.mNormalBottom);
    this->mFocusedTextureRect = GERectangle(inControl.mFocusedLeft, inControl.mFocusedTop, inControl.mFocusedRight,
                                      inControl.mFocusedBottom);
    this->mPressedTextureRect = GERectangle(inControl.mPressedLeft, inControl.mPressedTop, inControl.mPressedRight,
                                      inControl.mPressedBottom);
}

// FUNCTION: LLE 0x10067e8d
// Destroys the control description, releasing its font
UIControlsTable::~UIControlsTable()
{
    if (this->mFont)
        this->mFont->Dereference();
}

// FUNCTION: LLE 0x10067ee6
// Loads every control record from the file and registers each valid one with its form; returns the last read result
long UIControlsTable::Create(FSFileSpec& inFile, UIManager* inManager)
{
    UIControlsTable* theTable = NULL;
    unsigned long theCount = 0;
    unsigned long theRead = 0;
    UIDbControl theRecord;
    inFile.Open(kFileAccessRead);
    long theResult = inFile.Read(theCount);
    if (theResult == kErrorOK) {
        UIControlsTable::sUIControlsArray.resize(theCount, NULL);
        for (unsigned long i = 0; i < theCount; i++) {
            theTable = NULL;
            theResult = inFile.Read(reinterpret_cast<unsigned char*>(&theRecord), sizeof(UIDbControl), theRead);
            if (theRecord.mID >= 0) {
                theTable = new UIControlsTable(theRecord);
                UICDatabase theEntry;
                theEntry.mControl = NULL;
                theEntry.mTableIndex = i;
                inManager->GetFormByID(theTable->mFormID)->AddDatabaseControl(theEntry);
            }
            UIControlsTable::sUIControlsArray[i] = theTable;
        }
    }
    inFile.Close();
    return theResult;
}

// FUNCTION: LLE 0x10067fb4
// Creates a bitmap control from the description's normal image (or none)
UIControl* UIControlsTable::CreateBitmap(UIManager* inManager, const GERectangle& inRectangle)
{
    UIForm* theForm = inManager->GetFormByID(this->mFormID);
    GEBitmap* theBitmap = NULL;
    if (this->mNormalFileName.GetLength() > 0)
        theBitmap = this->CreateNormalBitmap(inManager, inRectangle);
    UIControl* theControl = new UIBitmap(this->mID, theBitmap, inRectangle, theForm, this->mFormIndex);
    if (theBitmap)
        theBitmap->Dereference();
    return theControl;
}

// FUNCTION: LLE 0x1006803a
// Loads a UI texture (clamped, point mip, given alpha) and returns a bitmap stretched over the rectangle, or NULL
GEBitmap* UIControlsTable::CreateBitmap(UIManager* inManager, const GERectangle& inRectangle,
                                        const UString& inFileName, bool inHasAlpha,
                                        const GERectangle& inTextureRectangle) const
{
    EE_LOST_DEBUG(2);  // LOST-DEBUG (PROVISIONAL, owner experiment 2026-10-24): no independent evidence; base-game LLE identical (exact window 2..18)
    GETextureDescription theDescription;
    GEBitmap* theBitmap = NULL;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mPixelDepth = 16;
    theDescription.mNumMipLevelsToDrop = 0;
    theDescription.mSurfaceVolatility = 1;
    theDescription.mFrameFormat = 0;
    theDescription.mHasAlpha = true;
    theDescription.mCreateMIPMaps = true;
    GETextureManager* theManager = inManager->GetRasterizer()->GetTextureManager(inManager->GetScreen());
    theDescription.mHasAlpha = inHasAlpha;
    theDescription.mPixelDepth = inManager->GetUITextureBitDepth();
    GETexture* theTexture = theManager->LoadTexture(inFileName, theDescription);
    if (theTexture) {
        theTexture->SetStageAlphaMode(kStageOpModulate, -1);
        theTexture->SetStageColorMode(kStageOpModulate, -1);
        theTexture->SetStageMinFilterMode(1, -1);
        theTexture->SetStageMagFilterMode(1, -1);
        theTexture->SetStageMipFilterMode(kMipFilterPoint, -1);
        theTexture->SetStageMipMapBias(-0.75f, -1);
        theTexture->SetStageTextureAddressingMode(kAddressingClamp, -1);
        theBitmap = new GEBitmap;
        theBitmap->SetTexture(theTexture, inManager->GetScreen());
        theBitmap->ComputeStretchedCoordsAndScale(inRectangle, inRectangle, inTextureRectangle,
                                                  inManager->GetScreen());
    }
    return theBitmap;
}

// FUNCTION: LLE 0x10068137
// Creates a plain, blinking, scalable or dual button with its images, text and sounds (the up sound is the down sound)
UIControl* UIControlsTable::CreateButton(UIManager* inManager, const GERectangle& inRectangle)
{
    UWideString theText;
    UIForm* theForm = inManager->GetFormByID(this->mFormID);
    GEBitmap* theNormal = this->mNormalFileName.GetLength() > 0 ? this->CreateNormalBitmap(inManager, inRectangle) : NULL;
    GEBitmap* thePressed = this->mPressedFileName.GetLength() > 0 ? this->CreatePressedBitmap(inManager, inRectangle) : NULL;
    GEBitmap* theFocused = this->mFocusedFileName.GetLength() > 0 ? this->CreateFocusedBitmap(inManager, inRectangle) : NULL;
    UIButton* theButton;
    switch (this->mControlType) {
    case kDbControlButton:
        theButton = new UIButton(this->mID, theNormal, theFocused, thePressed, inRectangle, theForm, this->mFormIndex,
                                 this->mBorderThickness);
        break;
    case kDbControlBlinkingButton:
        theButton = new UIBlinkingButton(this->mID, theNormal, theFocused, thePressed, inRectangle, theForm, this->mFormIndex,
                                         this->mBorderThickness, this->mBlinkRate);
        break;
    case kDbControlScalableButton:
        theButton = new UIScalableButton(this->mID, theNormal, theFocused, thePressed, this->mNormalTextureRect,
                                         this->mFocusedTextureRect, this->mPressedTextureRect, inRectangle, inRectangle, theForm,
                                         this->mFormIndex);
        break;
    case kDbControlDualButton:
        theButton = new UIDualButton(this->mID, theNormal, theFocused, thePressed, inRectangle, theForm, this->mFormIndex,
                                     this->mBorderThickness);
        break;
    default:
        theButton = NULL;
        break;
    }
    if (theNormal)
        theNormal->Dereference();
    if (thePressed)
        thePressed->Dereference();
    if (theFocused)
        theFocused->Dereference();
    if (this->mStringTableBegin > 0) {
        inManager->GetStringTable()->GetString(this->mStringTableBegin, theText);
        theButton->SetText(theText, this->mFont);
        theButton->SetFocusedTextColor(this->mFocusedLineColor);
        this->SetupGEText(theButton->GetGEText());
    } else if (this->mFont) {
        theButton->SetText(theText, this->mFont);
        theButton->SetFocusedTextColor(this->mFocusedLineColor);
        this->SetupGEText(theButton->GetGEText());
    }
    if (this->mButtonDownSound.GetLength() > 0)
        theButton->SetButtonDownSound(this->mButtonDownSound);
    if (this->mButtonUpSound.GetLength() > 0)
        theButton->SetButtonUpSound(this->mButtonUpSound);   // Fixed: used to pass mButtonDownSound
    return theButton;
}

// FUNCTION: LLE 0x100683ca
// Creates a check box with its text, start state and sounds (the up sound is the down sound)
UIControl* UIControlsTable::CreateCheckBox(UIManager* inManager, const GERectangle& inRectangle)
{
    UWideString theText;
    UIForm* theForm = inManager->GetFormByID(this->mFormID);
    if (this->mStringTableBegin > 0)
        inManager->GetStringTable()->GetString(this->mStringTableBegin, theText);
    else
        theText = "";
    UICheckBox* theCheckBox = new UICheckBox(this->mID, theText, this->mFont, inRectangle, theForm, this->mFormIndex,
                                             this->mBorderThickness, this->mBoolPlaceholder3);
    this->SetupGEText(theCheckBox->GetTextLabel()->GetGEText());
    if (this->mStartChecked)
        theCheckBox->SetChecked(true);
    if (this->mButtonDownSound.GetLength() > 0)
        theCheckBox->SetButtonDownSound(this->mButtonDownSound);
    if (this->mButtonUpSound.GetLength() > 0)
        theCheckBox->SetButtonUpSound(this->mButtonUpSound);   // Fixed: used to pass mButtonDownSound
    return theCheckBox;
}

// FUNCTION: LLE 0x100684c1
// Creates a combo box (editable or label) with its strings, colours, list height and start selection
UIControl* UIControlsTable::CreateComboBox(UIManager* inManager, const GERectangle& inRectangle)
{
    GERectangle theListRectangle;
    std::vector<UWideString> theStrings;
    UIForm* theForm = inManager->GetFormByID(this->mFormID);
    this->SetupStringArray(inManager, theStrings);
    theListRectangle.SetTopLeft(inRectangle.GetTopLeft());
    theListRectangle.SetRight(inRectangle.GetRight());
    if (this->mBoolPlaceholder3)
        theListRectangle.SetBottom(inRectangle.GetTop() + (this->mFont->GetHeight() + this->mBorderThickness * 2.0f));
    else
        theListRectangle.SetBottom(inRectangle.GetTop() + kScrollBarWidth);
    long theListHeight;
    if (inRectangle.GetHeight() > theListRectangle.GetHeight())
        theListHeight = UMFloor(inRectangle.GetHeight() - theListRectangle.GetHeight());
    else
        theListHeight = 1;
    UIComboBox* theComboBox;
    switch (this->mControlType) {
    case kDbControlEditableComboBox:
        theComboBox = new UIComboBox(this->mID, 0, theStrings, this->mFont, theListRectangle, theListHeight, theForm,
                                     this->mFormIndex, this->mMaxChars, this->mBorderThickness);
        break;
    case kDbControlComboBox:
        theComboBox = new UIComboBox(this->mID, 1, theStrings, this->mFont, theListRectangle, theListHeight, theForm,
                                     this->mFormIndex, this->mMaxChars, this->mBorderThickness);
        break;
    default:
        theComboBox = NULL;
        break;
    }
    this->SetupGEText(theComboBox->GetGEText());
    theComboBox->SetListBoxFocusedRectColor(this->mFocusedLineColor);
    theComboBox->SetListBoxSelectedBgColor(this->mSelectedListBox);
    if (this->mStartCurrentSelection > 0)
        theComboBox->SetCurrentSelection(this->mStartCurrentSelection);
    theComboBox->SetTextBoxBgOpacity(this->mTextBoxOpacity);
    theComboBox->SetTextBoxBgColor(this->mBackground);
    theComboBox->SetListBoxAlwaysDirty(this->mListBoxAlwaysDirty);
    return theComboBox;
}

// FUNCTION: LLE 0x100686eb
// Creates a file combo box with its strings, colours and start selection
UIControl* UIControlsTable::CreateFileComboBox(UIManager* inManager, const GERectangle& inRectangle)
{
    std::vector<UWideString> theStrings;
    UIFileComboBox* theComboBox = NULL;
    UIForm* theForm = inManager->GetFormByID(this->mFormID);
    this->SetupStringArray(inManager, theStrings);
    theComboBox = new UIFileComboBox(this->mID, 1, theStrings, this->mFont, inRectangle.GetTopLeft(),
                                     inRectangle.GetWidth() + 1.0f, UMFloor(inRectangle.GetHeight()) + 1, theForm,
                                     this->mFormIndex, this->mMaxChars, this->mBorderThickness);
    this->SetupGEText(theComboBox->GetGEText());
    theComboBox->SetListBoxFocusedRectColor(this->mFocusedLineColor);
    theComboBox->SetListBoxSelectedBgColor(this->mSelectedListBox);
    if (this->mStartCurrentSelection > 0)
        theComboBox->SetCurrentSelection(this->mStartCurrentSelection);
    theComboBox->SetTextBoxBgOpacity(this->mTextBoxOpacity);
    theComboBox->SetTextBoxBgColor(this->mBackground);
    theComboBox->SetListBoxAlwaysDirty(this->mListBoxAlwaysDirty);
    return theComboBox;
}

// FUNCTION: LLE 0x1006885c
// Lays out the control on the window and creates it by type (user types via the manager), then sets its background
UIControl* UIControlsTable::CreateControl(UIManager* inManager)
{
    GERectangle theRectangle;
    GERectangle theFontRectangle;
    GERectangle theParent;
    GEWindow* theWindow = inManager->GetRasterizer()->GetWindow(inManager->GetScreen());
    UIForm* theForm = inManager->GetFormByID(this->mFormID);
    UIControl* theControl = NULL;
    float theOffset = theForm->GetUseExclusiveCoords() ? 0.0f : 1.0f;
    theParent.SetLeft(0.0f);
    theParent.SetTop(0.0f);
    theParent.SetRight(theWindow->GetWidth() - theOffset);
    theParent.SetBottom(theWindow->GetHeight() - theOffset);
    this->mLayout.CalculateRectangle(theParent, theRectangle, theForm->GetUseExclusiveCoords());
    theFontRectangle = theRectangle;
    if (theForm->GetUseExclusiveCoords()) {
        theFontRectangle.SetRight(theFontRectangle.GetRight() + 1.0f);
        theFontRectangle.SetBottom(theFontRectangle.GetBottom() + 1.0f);
    }
    this->SetupFont(inManager, theFontRectangle, false);
    switch (this->mControlType) {
    case kDbControlBitmap:
        theControl = this->CreateBitmap(inManager, theRectangle);
        break;
    case kDbControlButton:
    case kDbControlBlinkingButton:
    case kDbControlScalableButton:
    case kDbControlDualButton:
        theControl = this->CreateButton(inManager, theRectangle);
        break;
    case kDbControlCheckBox:
        theControl = this->CreateCheckBox(inManager, theRectangle);
        break;
    case kDbControlEditableComboBox:
    case kDbControlComboBox:
        theControl = this->CreateComboBox(inManager, theRectangle);
        break;
    case kDbControlLineGraph:
        theControl = this->CreateGraph(inManager, theRectangle);
        break;
    case kDbControlNumberMultiLineEditBox:
    case kDbControlNumberEditBox:
    case kDbControlMultiLineEditBox:
    case kDbControlEditBox:
        theControl = this->CreateEditBox(inManager, theRectangle);
        break;
    case kDbControlListBox:
    case kDbControlAutoSelectListBox:
        theControl = this->CreateListBox(inManager, theRectangle);
        break;
    case kDbControlRadioButton:
    case kDbControlRadioTab:
        theControl = this->CreateRadioButton(inManager, theRectangle);
        break;
    case kDbControlStatusBar:
        theControl = this->CreateStatusBar(inManager, theRectangle);
        break;
    case kDbControlHorizontalScrollBar:
    case kDbControlVerticalScrollBar:
        theControl = this->CreateScrollBar(inManager, theRectangle);
        break;
    case kDbControlTextLabel:
    case kDbControlMultiLineTextLabel:
    case kDbControlNumberTextLabel:
    case kDbControlNumberMultiLineTextLabel:
    case kDbControlHelpTextLabel:
    case kDbControlMultiLineHelpTextLabel:
        theControl = this->CreateTextLabel(inManager, theRectangle);
        break;
    case kDbControlHorizontalSlider:
    case kDbControlVerticalSlider:
        theControl = this->CreateSlider(inManager, theRectangle);
        break;
    case kDbControlFileListBox:
        theControl = this->CreateFileListBox(inManager, theRectangle);
        break;
    case kDbControlFileComboBox:
        theControl = this->CreateFileComboBox(inManager, theRectangle);
        break;
    case kDbControlScrollingTextLabel:
        theControl = this->CreateScrollingTextLabel(inManager, theRectangle);
        break;
    case kDbControlMovie:
        theControl = this->CreateMovie(inManager, theRectangle);
        break;
    default:
        if (this->mControlType >= kDbControlStatusButton)
            theControl = inManager->CreateUserDefinedControl(*this);
        break;
    }
    if (theControl) {
        theControl->SetBgColor(this->mBackground);
        theControl->SetBgOpacity(this->mBackgroundOpacity);
        theControl->SetAlwaysDirty(this->mAlwaysDirty);
    }
    if (this->mFont) {
        this->mFont->Dereference();
        this->mFont = NULL;
    }
    return theControl;
}

// FUNCTION: LLE 0x10068b52
// Attaches the description's trigger events to the form's control, creating each event through the manager
void UIControlsTable::CreateControlEvent(UIForm* inForm)
{
    UIControl* theControl = inForm->GetControl(this->mFormIndex);
    UIManager* theManager = inForm->GetManager();
    for (unsigned long i = 0; i < this->mEventData.size(); i++) {
        UIEvent* theEvent = theManager->CreateControlEvent(inForm, this->mEventData[i].mEventID);
        if (theEvent) {
            theControl->SetTriggerEvent(this->mEventData[i].mEventType, theEvent);
            theEvent->Dereference();
        }
    }
}

// FUNCTION: LLE 0x10068bc4
// Creates a text or number edit box (single or multi-line) with its start text or number
UIControl* UIControlsTable::CreateEditBox(UIManager* inManager, const GERectangle& inRectangle)
{
    UWideString theText;
    UIForm* theForm = inManager->GetFormByID(this->mFormID);
    UIEditBox* theEditBox;
    switch (this->mControlType) {
    case kDbControlNumberMultiLineEditBox:
        theEditBox = new UIEditBox(this->mID, this->mFont, this->mMaxChars, true, inRectangle, theForm, this->mFormIndex, this->mBorderThickness,
                                   true, this->mStartChecked);
        break;
    case kDbControlNumberEditBox:
        theEditBox = new UIEditBox(this->mID, this->mFont, this->mMaxChars, true, inRectangle, theForm, this->mFormIndex, this->mBorderThickness,
                                   false, this->mStartChecked);
        break;
    case kDbControlMultiLineEditBox:
        theEditBox = new UIEditBox(this->mID, this->mFont, this->mMaxChars, false, inRectangle, theForm, this->mFormIndex,
                                   this->mBorderThickness, true, this->mStartChecked);
        break;
    case kDbControlEditBox:
        theEditBox = new UIEditBox(this->mID, this->mFont, this->mMaxChars, false, inRectangle, theForm, this->mFormIndex,
                                   this->mBorderThickness, false, this->mStartChecked);
        break;
    default:
        theEditBox = NULL;
        break;
    }
    this->SetupGEText(theEditBox->GetGEText());
    if (this->mControlType == kDbControlNumberMultiLineEditBox || this->mControlType == kDbControlNumberEditBox)
        theEditBox->SetText((unsigned long)this->mStringTableBegin);
    else if (this->mStringTableBegin > 0) {
        inManager->GetStringTable()->GetString(this->mStringTableBegin, theText);
        theEditBox->SetText(theText);
    }
    theEditBox->SetSelectedBgColor(this->mSelectedListBox);
    return theEditBox;
}

// FUNCTION: LLE 0x10068d36
// Creates a file list box accepting the description's extensions, with its colours and start selection
UIControl* UIControlsTable::CreateFileListBox(UIManager* inManager, const GERectangle& inRectangle)
{
    std::vector<UWideString> theStrings;
    UIForm* theForm = inManager->GetFormByID(this->mFormID);
    UString theExtension;
    UIFileListBox* theListBox = new UIFileListBox(this->mID, this->mFont, false, inRectangle, theForm, this->mFormIndex,
                                                  this->mBorderThickness);
    this->SetupGEText(theListBox->GetGEText());
    theListBox->SetFocusedRectColor(this->mFocusedLineColor);
    theListBox->GetSelectedBackground()->SetColor(this->mSelectedListBox);
    theListBox->SetScrollBarBgOpacity(this->mScrollBarBgOpacity);
    this->SetupStringArray(inManager, theStrings);
    for (unsigned long i = 0; i < theStrings.size(); i++) {
        theExtension = theStrings[i];
        theListBox->AddValidExtension(theExtension);
    }
    if (this->mStartCurrentSelection > 0)
        theListBox->ChangeSelectionIndex(this->mStartCurrentSelection, false, false);
    return theListBox;
}

// FUNCTION: LLE 0x10068e70
// Creates a line graph with the description's font and colour
UIControl* UIControlsTable::CreateGraph(UIManager* inManager, const GERectangle& inRectangle)
{
    UIForm* theForm = inManager->GetFormByID(this->mFormID);
    UIGraph* theGraph;
    if (this->mControlType == kDbControlLineGraph)
        theGraph = new UILineGraph(this->mID, inRectangle, theForm, this->mFormIndex, this->mBorderThickness);
    else
        theGraph = NULL;
    theGraph->SetFont(this->mFont);
    theGraph->SetFontColor(this->mTextForeground);
    return theGraph;
}

// FUNCTION: LLE 0x10068ee3
// Creates a list box (auto-select or not) with its strings, colours and start selection
UIControl* UIControlsTable::CreateListBox(UIManager* inManager, const GERectangle& inRectangle)
{
    UIForm* theForm = inManager->GetFormByID(this->mFormID);
    std::vector<UWideString> theStrings;
    this->SetupStringArray(inManager, theStrings);
    UILBString* theListBox;
    switch (this->mControlType) {
    case kDbControlListBox:
        theListBox = new UILBString(this->mID, theStrings, this->mFont, false, inRectangle, theForm, this->mFormIndex,
                                    this->mBorderThickness);
        break;
    case kDbControlAutoSelectListBox:
        theListBox = new UILBString(this->mID, theStrings, this->mFont, true, inRectangle, theForm, this->mFormIndex,
                                    this->mBorderThickness);
        break;
    default:
        theListBox = NULL;
        break;
    }
    this->SetupGEText(theListBox->GetGEText());
    theListBox->SetFocusedRectColor(this->mFocusedLineColor);
    theListBox->GetSelectedBackground()->SetColor(this->mSelectedListBox);
    theListBox->SetScrollBarBgOpacity(this->mScrollBarBgOpacity);
    if (this->mStartCurrentSelection > 0)
        theListBox->ChangeSelectionIndex(this->mStartCurrentSelection, false, false);
    return theListBox;
}

// FUNCTION: LLE 0x10069000
// Creates a movie control playing the description's file from the startup folder (looping per a record flag)
UIControl* UIControlsTable::CreateMovie(UIManager* inManager, GERectangle inRectangle) const
{
    FSFileSpec theFile;
    UString theFileName;
    UIForm* theForm = inManager->GetFormByID(this->mFormID);
    bool theLoop = this->mBoolPlaceholder4;
    theFile.SetVolume(gStartupVolume);
    theFile.SetDirectory(gStartupDirectory);
    theFileName = this->GetNormalFileName();
    theFile.SetFile(theFileName);
    GEMovie* theMovie = inManager->GetRasterizer()->CreateMovie(theFile, inManager->GetScreen(), inRectangle,
                                                                theLoop);
    UIControl* theControl = new UIMovie(this->mID, theMovie, inRectangle, theForm, this->mFormIndex);
    if (theMovie)
        theMovie->Dereference();
    return theControl;
}

// FUNCTION: LLE 0x100690f0
// Creates a radio button or radio tab with its text, sounds (up = down sound) and optional unchecked/pressed images
UIControl* UIControlsTable::CreateRadioButton(UIManager* inManager, const GERectangle& inRectangle)
{
    UWideString theText;
    UIForm* theForm = inManager->GetFormByID(this->mFormID);
    if (this->mStringTableBegin > 0)
        inManager->GetStringTable()->GetString(this->mStringTableBegin, theText);
    else
        theText = "";
    UIRadioButton* theRadioButton;
    switch (this->mControlType) {
    case kDbControlRadioButton:
        theRadioButton = new UIRadioButton(this->mID, theText, this->mFont, inRectangle, theForm, this->mFormIndex, this->mBorderThickness,
                                           false, this->mStartChecked, this->mBoolPlaceholder3);
        break;
    case kDbControlRadioTab:
        theRadioButton = new UIRadioButton(this->mID, theText, this->mFont, inRectangle, theForm, this->mFormIndex, this->mBorderThickness,
                                           true, this->mStartChecked, this->mBoolPlaceholder3);
        break;
    default:
        theRadioButton = NULL;
        break;
    }
    if (theText.GetLength() > 0)
        this->SetupGEText(theRadioButton->GetTextLabel()->GetGEText());
    if (this->mButtonDownSound.GetLength() > 0)
        theRadioButton->SetButtonDownSound(this->mButtonDownSound);
    if (this->mButtonUpSound.GetLength() > 0)
        theRadioButton->SetButtonUpSound(this->mButtonUpSound);   // Fixed: used to pass mButtonDownSound
    if (this->mNormalFileName.GetLength() > 0) {
        GEBitmap* theBitmap = this->CreateNormalBitmap(inManager, inRectangle);
        theRadioButton->SetUpUncheckedBitmap(theBitmap);
        theBitmap->Dereference();
    }
    if (!(this->mPressedFileName.GetLength() > 0))
        return theRadioButton;
    GEBitmap* theBitmap = this->CreatePressedBitmap(inManager, inRectangle);
    theRadioButton->SetDownUncheckedBitmap(theBitmap);
    theRadioButton->SetDownCheckedBitmap(theBitmap);
    theRadioButton->SetUpCheckedBitmap(theBitmap);
    theBitmap->Dereference();
    return theRadioButton;
}

// FUNCTION: LLE 0x1006929e
// Creates a horizontal or vertical scroll bar with its range and start value
UIControl* UIControlsTable::CreateScrollBar(UIManager* inManager, const GERectangle& inRectangle)
{
    UIForm* theForm = inManager->GetFormByID(this->mFormID);
    UIScrollBar* theScrollBar;
    switch (this->mControlType) {
    case kDbControlHorizontalScrollBar:
        theScrollBar = new UIScrollBar(this->mID, inRectangle.GetTopLeft(), inRectangle.GetWidth() + 1.0f, 0, theForm,
                                       this->mFormIndex, this->mBorderThickness);
        break;
    case kDbControlVerticalScrollBar:
        theScrollBar = new UIScrollBar(this->mID, inRectangle.GetTopLeft(), inRectangle.GetHeight() + 1.0f, 1, theForm,
                                       this->mFormIndex, this->mBorderThickness);
        break;
    default:
        theScrollBar = NULL;
        break;
    }
    theScrollBar->SetRange(this->mStartRange, this->mEndRange);
    theScrollBar->SetCurrentRangeValue(this->mCurrentRange);
    return theScrollBar;
}

// FUNCTION: LLE 0x100693a9
// Creates a scrolling text label with its string-table text
UIControl* UIControlsTable::CreateScrollingTextLabel(UIManager* inManager, const GERectangle& inRectangle)
{
    UWideString theText;
    UIForm* theForm = inManager->GetFormByID(this->mFormID);
    if (this->mControlType != kDbControlNumberTextLabel && this->mControlType != kDbControlNumberMultiLineTextLabel && this->mStringTableBegin > 0)
        inManager->GetStringTable()->GetString(this->mStringTableBegin, theText);
    else
        theText = "";
    UITLScrolling* theLabel = new UITLScrolling(theText, this->mFont, this->mID, inRectangle, theForm, this->mFormIndex,
                                                this->mBorderThickness);
    this->SetupGEText(theLabel->GetGEText());
    return theLabel;
}

// FUNCTION: LLE 0x10069461
// Creates a horizontal or vertical slider with its range, start value and page size (capped below the range)
UIControl* UIControlsTable::CreateSlider(UIManager* inManager, const GERectangle& inRectangle)
{
    UIForm* theForm = inManager->GetFormByID(this->mFormID);
    UISlider* theSlider;
    switch (this->mControlType) {
    case kDbControlHorizontalSlider:
        theSlider = new UISlider(this->mID, inRectangle, 0, theForm, this->mFormIndex, this->mBorderThickness);
        break;
    case kDbControlVerticalSlider:
        theSlider = new UISlider(this->mID, inRectangle, 1, theForm, this->mFormIndex, this->mBorderThickness);
        break;
    default:
        theSlider = NULL;
        break;
    }
    long theRange = this->mEndRange - this->mStartRange;
    theSlider->SetRange(this->mStartRange, this->mEndRange);
    theSlider->SetCurrentRangeValue(this->mCurrentRange);
    if (this->mPageSize >= theRange)
        theSlider->SetPageSize(theRange - 1);
    else
        theSlider->SetPageSize(this->mPageSize);
    return theSlider;
}

// FUNCTION: LLE 0x10069521
// Creates a status bar with its range, value, colour and optional text
UIControl* UIControlsTable::CreateStatusBar(UIManager* inManager, const GERectangle& inRectangle)
{
    UWideString theText;
    UIForm* theForm = inManager->GetFormByID(this->mFormID);
    UIStatusBar* theStatusBar = new UIStatusBar(this->mID, inRectangle, theForm, this->mFormIndex, this->mBorderThickness);
    theStatusBar->SetRange(this->mStartRange, this->mEndRange);
    theStatusBar->SetCurrentRangeValue(this->mCurrentRange);
    theStatusBar->SetStatusBarColor(this->mTextForeground);
    theStatusBar->SetStatusBarOpacity(this->mTextBoxOpacity);
    if (!(this->mStringTableBegin > 0))
        return theStatusBar;
    inManager->GetStringTable()->GetString(this->mStringTableBegin, theText);
    theStatusBar->SetText(theText, this->mFont);
    theStatusBar->SetFocusedTextColor(this->mFocusedLineColor);
    this->SetupGEText(theStatusBar->GetGEText());
    return theStatusBar;
}

// FUNCTION: LLE 0x10069624
// Creates a text label (single or multi-line); number and help types show the string-table index as a number
UIControl* UIControlsTable::CreateTextLabel(UIManager* inManager, const GERectangle& inRectangle)
{
    UWideString theText;
    UIForm* theForm = inManager->GetFormByID(this->mFormID);
    if (this->mControlType != kDbControlNumberTextLabel && this->mControlType != kDbControlNumberMultiLineTextLabel && this->mStringTableBegin > 0)
        inManager->GetStringTable()->GetString(this->mStringTableBegin, theText);
    else
        theText = "";
    UITextLabel* theLabel;
    switch (this->mControlType) {
    case kDbControlTextLabel:
    case kDbControlNumberTextLabel:
    case kDbControlHelpTextLabel:
        theLabel = new UITextLabel(theText, this->mFont, this->mID, inRectangle, theForm, 0, this->mFormIndex, this->mBorderThickness);
        break;
    case kDbControlMultiLineTextLabel:
    case kDbControlNumberMultiLineTextLabel:
    case kDbControlMultiLineHelpTextLabel:
        theLabel = new UITextLabel(theText, this->mFont, this->mID, inRectangle, theForm, 1, this->mFormIndex, this->mBorderThickness);
        break;
    default:
        theLabel = NULL;
        break;
    }
    this->SetupGEText(theLabel->GetGEText());
    if (this->mControlType != kDbControlTextLabel && this->mControlType != kDbControlMultiLineTextLabel)
        theLabel->SetText(this->mStringTableBegin);
    theLabel->SetAlignment(this->mTextAlignment);
    theLabel->SetScrollBarBgOpacity(this->mScrollBarBgOpacity);
    theLabel->SetFitRectToText(this->mBoolPlaceholder2);
    return theLabel;
}

// FUNCTION: LLE 0x10069766
// Deletes every control description and empties the table
void UIControlsTable::DeleteAll()
{
    for (unsigned long i = 0; i < UIControlsTable::sUIControlsArray.size(); i++) {
        if (UIControlsTable::sUIControlsArray[i])
            delete UIControlsTable::sUIControlsArray[i];
    }
    UIControlsTable::sUIControlsArray.resize(0, NULL);
}

// FUNCTION: LLE 0x100697a6
// Recomputes a created control's rectangle and font for the current window size
void UIControlsTable::RelayoutControl(UIControl* inControl, UIManager* inManager)
{
    GERectangle theParent;
    GERectangle theRectangle;
    UIForm* theForm = inManager->GetFormByID(this->mFormID);
    GEWindow* theWindow = inManager->GetRasterizer()->GetWindow(inManager->GetScreen());
    float theOffset = theForm->GetUseExclusiveCoords() ? 0.0f : 1.0f;
    if (!inControl)
        return;
    theParent.SetLeft(0.0f);
    theParent.SetTop(0.0f);
    theParent.SetRight(theWindow->GetWidth() - theOffset);
    theParent.SetBottom(theWindow->GetHeight() - theOffset);
    this->mLayout.CalculateRectangle(theParent, theRectangle, theForm->GetUseExclusiveCoords());
    inControl->SetRectangle(theRectangle);
    this->SetupFont(inManager, theRectangle, false);
    inControl->ChangeFont(this->mFont);
    if (this->mFont) {
        this->mFont->Dereference();
        this->mFont = NULL;
    }
}

// FUNCTION: LLE 0x100698ab
// Picks the description's font: scaled to the control or window height when the record asks, else the table font
void UIControlsTable::SetupFont(UIManager* inManager, const GERectangle& inRectangle, bool inScaleToWindow)
{
    UIForm* theForm = inManager->GetFormByID(this->GetFormID());
    if (this->mFont)
        this->mFont->Dereference();
    if (this->mFontTableIndex > 0) {
        if (this->mBoolPlaceholder3 == true) {
            long theSize;
            bool theScale = inScaleToWindow;
            if (!theScale) {
                switch (this->mControlType) {
                case kDbControlEditableComboBox:
                case kDbControlComboBox:
                case kDbControlNumberMultiLineEditBox:
                case kDbControlMultiLineEditBox:
                case kDbControlListBox:
                case kDbControlAutoSelectListBox:
                case kDbControlMultiLineTextLabel:
                case kDbControlNumberMultiLineTextLabel:
                case kDbControlFileListBox:
                case kDbControlMultiLineHelpTextLabel:
                case kDbControlScrollingTextLabel:
                    theScale = true;
                    break;
                }
            }
            if (!theScale)
                theSize = UMFloor(inRectangle.GetHeight() - this->GetBorderThickness() * 2);
            else {
                GEWindow* theWindow = inManager->GetRasterizer()->GetWindow(inManager->GetScreen());
                GEFont* theFont = UIFontTable::GetFont(this->GetFontTableIndex());
                theForm = inManager->GetFormByID(this->GetFormID());
                theSize = UMFloor((float)theFont->GetHeight() * theWindow->GetHeight() /
                                  theForm->GetDesignedHeight());
                theFont->Dereference();
            }
            this->mFont = UIFontTable::CreateClosestFont(this->mFontTableIndex, theSize);
        } else
            this->mFont = UIFontTable::GetFont(this->GetFontTableIndex());
    } else
        this->mFont = NULL;
    if (!theForm->GetUseGDIForText() && this->mFont && !this->mFont->GetFontBitmap())
        new GEFontBitmap(inManager->GetRasterizer(), this->mFont, inManager->GetScreen());
}

// FUNCTION: LLE 0x10069a4f
// Sets text to transparent in the description's text colour
void UIControlsTable::SetupGEText(GEText* inText) const
{
    inText->SetIsTransparent(true);
    inText->SetFgColor(this->mTextForeground);
}

// FUNCTION: LLE 0x10069a6c
// Fills a list with the description's string-table range (empty when the range is unset)
void UIControlsTable::SetupStringArray(UIManager* inManager, std::vector<UWideString>& outStrings) const
{
    outStrings.clear();
    if (!(this->mStringTableBegin > 0) || !(this->mStringTableEnd >= this->mStringTableBegin))
        return;
    outStrings.resize(this->mStringTableEnd - this->mStringTableBegin + 1);
    for (long i = this->mStringTableBegin; i <= this->mStringTableEnd; i++)
        inManager->GetStringTable()->GetString(i, outStrings[i - this->mStringTableBegin]);
}

// Header-inline members (UIControlsTable.h, declaration order)
// FUNCTION: LLE 0x10065f69
// FUNCTION: LLE 0x10065f78
// FUNCTION: LLE 0x10065f9c
// FUNCTION: LLE 0x10065fc0
// FUNCTION: LLE 0x10065fe4
// FUNCTION: LLE 0x10065feb
// FUNCTION: LLE 0x10065ff9
// FUNCTION: LLE 0x1006600c
// FUNCTION: LLE 0x1006601f
// FUNCTION: LLE 0x10066032
// FUNCTION: LLE 0x10066048
// FUNCTION: LLE 0x1006605f
// FUNCTION: LLE 0x10066076
// FUNCTION: LLE 0x1006608d
// FUNCTION: LLE 0x10066094
// FUNCTION: LLE 0x1006609b
// FUNCTION: LLE 0x100660a2
// FUNCTION: LLE 0x100660a9
// FUNCTION: LLE 0x100660b0
// FUNCTION: LLE 0x100660b7
// FUNCTION: LLE 0x100660be
// FUNCTION: LLE 0x100660c5
// FUNCTION: LLE 0x100660cc
// FUNCTION: LLE 0x100660d3
// FUNCTION: LLE 0x100660da
// FUNCTION: LLE 0x100660e1
// FUNCTION: LLE 0x100660e8
// FUNCTION: LLE 0x100660ef
// FUNCTION: LLE 0x100660f6
// FUNCTION: LLE 0x100660fd
// FUNCTION: LLE 0x10066104
// FUNCTION: LLE 0x1006610b

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// SYNTHETIC: LLE 0x10066153
// SYNTHETIC: LLE 0x100663e1
// SYNTHETIC: LLE 0x100666cb
// SYNTHETIC: LLE 0x1006672c
// SYNTHETIC: LLE 0x10066736
// SYNTHETIC: LLE 0x10066746
// SYNTHETIC: LLE 0x10066752
// SYNTHETIC: LLE 0x1006675c
// SYNTHETIC: LLE 0x10066766
// SYNTHETIC: LLE 0x10066776
// SYNTHETIC: LLE 0x10066782
// SYNTHETIC: LLE 0x1006678c
// SYNTHETIC: LLE 0x10066796
// SYNTHETIC: LLE 0x100667a6
// SYNTHETIC: LLE 0x100667b2
// SYNTHETIC: LLE 0x100667bc
// SYNTHETIC: LLE 0x100667c6
// SYNTHETIC: LLE 0x100667d6
// SYNTHETIC: LLE 0x100667e2
// SYNTHETIC: LLE 0x100667ec
// SYNTHETIC: LLE 0x100667f6
// SYNTHETIC: LLE 0x10066806
// SYNTHETIC: LLE 0x10066812
// SYNTHETIC: LLE 0x1006681c
// SYNTHETIC: LLE 0x10066826
// SYNTHETIC: LLE 0x10066836
// SYNTHETIC: LLE 0x10066842
// SYNTHETIC: LLE 0x1006684c
// SYNTHETIC: LLE 0x10066864
// SYNTHETIC: LLE 0x10066876
// FUNCTION: LLE 0x100668d7
// SYNTHETIC: LLE 0x100668e9
// SYNTHETIC: LLE 0x10066922
// SYNTHETIC: LLE 0x10066955
// FUNCTION: LLE 0x100669b6
// SYNTHETIC: LLE 0x100669c8
// SYNTHETIC: LLE 0x10066a1f
// SYNTHETIC: LLE 0x10066a6a
// FUNCTION: LLE 0x10066acb
// FUNCTION: LLE 0x10066ad2
// FUNCTION: LLE 0x10066ad9
// FUNCTION: LLE 0x10066ae0
// FUNCTION: LLE 0x10066aed
// FUNCTION: LLE 0x10066aff
// FUNCTION: LLE 0x10066b0c
// FUNCTION: LLE 0x10066b19
// FUNCTION: LLE 0x10066b26
// FUNCTION: LLE 0x10066b33
// FUNCTION: LLE 0x10066b40
// FUNCTION: LLE 0x10066b4d
// FUNCTION: LLE 0x10066b5a
// FUNCTION: LLE 0x10066b67
// SYNTHETIC: LLE 0x10066b74
// SYNTHETIC: LLE 0x10066d98
// SYNTHETIC: LLE 0x1006707c
// SYNTHETIC: LLE 0x100670dd
// SYNTHETIC: LLE 0x100670f5
// SYNTHETIC: LLE 0x10067107
// SYNTHETIC: LLE 0x10067168
// SYNTHETIC: LLE 0x1006718f
// SYNTHETIC: LLE 0x100671b0
// FUNCTION: LLE 0x10067211
// FUNCTION: LLE 0x10067218
// FUNCTION: LLE 0x1006721f
// FUNCTION: LLE 0x1006722c
// SYNTHETIC: LLE 0x10067239
// SYNTHETIC: LLE 0x1006730a
// SYNTHETIC: LLE 0x100673fa
// FUNCTION: LLE 0x1006745b
// FUNCTION: LLE 0x10067469
// FUNCTION: LLE 0x1006747f
// SYNTHETIC: LLE 0x10067491
// SYNTHETIC: LLE 0x10067549
// SYNTHETIC: LLE 0x1006764e
// FUNCTION: LLE 0x100676af
// SYNTHETIC: LLE 0x100676bd
// SYNTHETIC: LLE 0x100676f0
// SYNTHETIC: LLE 0x1006771d
// SYNTHETIC: LLE 0x10067792
// SYNTHETIC: LLE 0x1006779c
// SYNTHETIC: LLE 0x100677b4
// SYNTHETIC: LLE 0x100677c0
// TEMPLATE: LLE 0x10069ad4
// TEMPLATE: LLE 0x10069b1b
// TEMPLATE: LLE 0x10069c70
// TEMPLATE: LLE 0x10069c83
