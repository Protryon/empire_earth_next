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
#include <stddef.h>

#include "rust/ee_rust.h"
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

// One record of the controls database file (Create reads them with FSFileSpec::Read, 0x35c bytes each). The
// offsets are proven by the UIDbControl ctor 0x1006796f (provisional field names).
struct UIDbControl {
    char mLabel[100];                       // +0x000
    char mNormalFileName[100];              // +0x064
    char mPressedFileName[100];             // +0x0c8
    char mFocusedFileName[100];             // +0x12c
    char mButtonDownSound[100];             // +0x190
    char mButtonUpSound[100];               // +0x1f4
    float mBackgroundOpacity;               // +0x258
    float mTextBoxOpacity;                  // +0x25c
    float mScrollBarBgOpacity;              // +0x260
    uint32_t mUnused1[5];                   // +0x264
    float mLeftOffset;                      // +0x278
    float mRightOffset;                     // +0x27c
    float mBottomOffset;                    // +0x280
    float mTopOffset;                       // +0x284
    int32_t mUnusedDbRecordId;              // +0x288
    int32_t mID;                            // +0x28c
    int32_t mFormID;                        // +0x290
    uint32_t mGroup;                        // +0x294
    uint32_t mControlType;                  // +0x298
    int32_t mFormIndex;                     // +0x29c
    int32_t mBorderThickness;               // +0x2a0
    uint32_t mLeftOffsetType;               // +0x2a4
    uint32_t mRightOffsetType;              // +0x2a8
    uint32_t mTopOffsetType;                // +0x2ac
    uint32_t mBottomOffsetType;             // +0x2b0
    uint32_t mBackgroundRed;                // +0x2b4
    uint32_t mBackgroundGreen;              // +0x2b8
    uint32_t mBackgroundBlue;               // +0x2bc
    int32_t mStringTableBegin;              // +0x2c0
    int32_t mStringTableEnd;                // +0x2c4
    uint32_t mFontTableIndex;               // +0x2c8
    uint32_t mTextForegroundRed;            // +0x2cc
    uint32_t mTextForegroundGreen;          // +0x2d0
    uint32_t mTextForegroundBlue;           // +0x2d4
    int32_t mTextAlignment;                 // +0x2d8
    int32_t mNormalLeft;                    // +0x2dc
    int32_t mNormalRight;                   // +0x2e0
    int32_t mNormalTop;                     // +0x2e4
    int32_t mNormalBottom;                  // +0x2e8
    int32_t mPressedLeft;                   // +0x2ec
    int32_t mPressedRight;                  // +0x2f0
    int32_t mPressedTop;                    // +0x2f4
    int32_t mPressedBottom;                 // +0x2f8
    int32_t mFocusedLeft;                   // +0x2fc
    int32_t mFocusedRight;                  // +0x300
    int32_t mFocusedTop;                    // +0x304
    int32_t mFocusedBottom;                 // +0x308
    int32_t mMaxChars;                      // +0x30c
    uint32_t mSelectedListBoxRed;           // +0x310
    uint32_t mSelectedListBoxGreen;         // +0x314
    uint32_t mSelectedListBoxBlue;          // +0x318
    int32_t mStartCurrentSelection;         // +0x31c
    uint32_t mFocusedLineRed;               // +0x320
    uint32_t mFocusedLineGreen;             // +0x324
    uint32_t mFocusedLineBlue;              // +0x328
    int32_t mStartRange;                    // +0x32c
    int32_t mEndRange;                      // +0x330
    int32_t mCurrentRange;                  // +0x334
    int32_t mPageSize;                      // +0x338
    int32_t mStringTableID;                 // +0x33c
    int32_t mBlinkRate;                     // +0x340
    uint32_t mUnused2[3];                   // +0x344
    bool mIsVisible;                        // +0x350
    bool mStartChecked;                     // +0x351
    bool mListBoxAlwaysDirty;               // +0x352
    bool mSortListBox;                      // +0x353
    bool mFocusedHasAlpha;                  // +0x354
    bool mPressedHasAlpha;                  // +0x355
    bool mNormalHasAlpha;                   // +0x356
    bool mAlwaysDirty;                      // +0x357
    bool mFitRectToText;                 // +0x358
    bool mScaleToControl;                 // +0x359
    bool mLoopMovie;                 // +0x35a
};
static_assert(sizeof(UIDbControl) == 860, "on-disk record: 860 bytes on every target");
static_assert(sizeof(UIDbControl) == sizeof(EeUiDbControl), "ee_ui/src/controls_table.rs DbControl");
static_assert(offsetof(UIDbControl, mBackgroundOpacity) == offsetof(EeUiDbControl, background_opacity), "ee_ui DbControl");
static_assert(offsetof(UIDbControl, mID) == offsetof(EeUiDbControl, id), "ee_ui DbControl");
static_assert(offsetof(UIDbControl, mBlinkRate) == offsetof(EeUiDbControl, blink_rate), "ee_ui DbControl");
static_assert(offsetof(UIDbControl, mLoopMovie) == offsetof(EeUiDbControl, loop_movie), "ee_ui DbControl");

std::vector<UIControlsTable*> UIControlsTable::sUIControlsArray;

// The Rust logic's view of a table (rust/crates/ee_ui/src/controls_table.rs): its plain members from
// mBackgroundOpacity on as one record, its layout, colours and texture rectangles.
struct UIControlsTableRust {
    EeUiControlsTableRefs mRefs;
    UIControlsTableRust(UIControlsTable* inTable)
    {
#define EE_UI_TABLE_FIELD(member, field)                                                                               \
    static_assert(offsetof(UIControlsTable, member) - offsetof(UIControlsTable, mBackgroundOpacity) ==                 \
                      offsetof(EeUiControlsTableFields, field),                                                        \
                  "ee_ui/src/controls_table.rs TableFields")
        EE_UI_TABLE_FIELD(mBackgroundOpacity, background_opacity);
        EE_UI_TABLE_FIELD(mTextBoxOpacity, text_box_opacity);
        EE_UI_TABLE_FIELD(mScrollBarBgOpacity, scroll_bar_bg_opacity);
        EE_UI_TABLE_FIELD(mUnknown100, unknown100);
        EE_UI_TABLE_FIELD(mID, id);
        EE_UI_TABLE_FIELD(mFormID, form_id);
        EE_UI_TABLE_FIELD(mGroup, group);
        EE_UI_TABLE_FIELD(mControlType, control_type);
        EE_UI_TABLE_FIELD(mFormIndex, form_index);
        EE_UI_TABLE_FIELD(mBorderThickness, border_thickness);
        EE_UI_TABLE_FIELD(mStringTableID, string_table_id);
        EE_UI_TABLE_FIELD(mFontTableIndex, font_table_index);
        EE_UI_TABLE_FIELD(mBlinkRate, blink_rate);
        EE_UI_TABLE_FIELD(mStringTableBegin, string_table_begin);
        EE_UI_TABLE_FIELD(mStringTableEnd, string_table_end);
        EE_UI_TABLE_FIELD(mTextAlignment, text_alignment);
        EE_UI_TABLE_FIELD(mMaxChars, max_chars);
        EE_UI_TABLE_FIELD(mStartCurrentSelection, start_current_selection);
        EE_UI_TABLE_FIELD(mStartRange, start_range);
        EE_UI_TABLE_FIELD(mEndRange, end_range);
        EE_UI_TABLE_FIELD(mCurrentRange, current_range);
        EE_UI_TABLE_FIELD(mPageSize, page_size);
        EE_UI_TABLE_FIELD(mIsVisible, is_visible);
        EE_UI_TABLE_FIELD(mStartChecked, start_checked);
        EE_UI_TABLE_FIELD(mListBoxAlwaysDirty, list_box_always_dirty);
        EE_UI_TABLE_FIELD(mSortListBox, sort_list_box);
        EE_UI_TABLE_FIELD(mFocusedHasAlpha, focused_has_alpha);
        EE_UI_TABLE_FIELD(mPressedHasAlpha, pressed_has_alpha);
        EE_UI_TABLE_FIELD(mNormalHasAlpha, normal_has_alpha);
        EE_UI_TABLE_FIELD(mAlwaysDirty, always_dirty);
        EE_UI_TABLE_FIELD(mFitRectToText, fit_rect_to_text);
        EE_UI_TABLE_FIELD(mScaleToControl, scale_to_control);
        EE_UI_TABLE_FIELD(mLoopMovie, loop_movie);
#undef EE_UI_TABLE_FIELD
        static_assert(sizeof(UIControlsTable) - offsetof(UIControlsTable, mBackgroundOpacity) >= sizeof(EeUiControlsTableFields),
                      "ee_ui/src/controls_table.rs TableFields");
        static_assert(sizeof(UILayoutUtility) == sizeof(EeUiLayout), "ee_ui/src/layout.rs");
        static_assert(sizeof(GEColor) == sizeof(EeUiColor) && offsetof(GEColor, mGreen) == offsetof(EeUiColor, green) &&
                          offsetof(GEColor, mBlue) == offsetof(EeUiColor, blue),
                      "ee_ui/src/button.rs UiColor");
        static_assert(sizeof(GERectangle) == sizeof(EeUiRect) && offsetof(GERectangle, mLeft) == offsetof(EeUiRect, left) &&
                          offsetof(GERectangle, mTop) == offsetof(EeUiRect, top),
                      "ee_ui/src/rect.rs");
        this->mRefs.fields = reinterpret_cast<EeUiControlsTableFields*>(&inTable->mBackgroundOpacity);
        this->mRefs.layout = reinterpret_cast<EeUiLayout*>(&inTable->mLayout);
        this->mRefs.background = reinterpret_cast<EeUiColor*>(&inTable->mBackground);
        this->mRefs.text_foreground = reinterpret_cast<EeUiColor*>(&inTable->mTextForeground);
        this->mRefs.selected_list_box = reinterpret_cast<EeUiColor*>(&inTable->mSelectedListBox);
        this->mRefs.focused_line = reinterpret_cast<EeUiColor*>(&inTable->mFocusedLineColor);
        this->mRefs.normal_rect = reinterpret_cast<EeUiRect*>(&inTable->mNormalTextureRect);
        this->mRefs.pressed_rect = reinterpret_cast<EeUiRect*>(&inTable->mPressedTextureRect);
        this->mRefs.focused_rect = reinterpret_cast<EeUiRect*>(&inTable->mFocusedTextureRect);
    }
    operator const EeUiControlsTableRefs*() const { return &this->mRefs; }
    static std::vector<UIControlsTable*>& Array() { return UIControlsTable::sUIControlsArray; }
    static const EeUiRect* Rect(const GERectangle& inRectangle) { return reinterpret_cast<const EeUiRect*>(&inRectangle); }
    static GERectangle Rect(const EeUiRect& inRectangle)
    {
        return GERectangle(inRectangle.left, inRectangle.top, inRectangle.right, inRectangle.bottom);
    }
};

// The static array for the Rust searches (ee_ui/src/controls_table_ffi.rs), a slot at a time
extern "C" unsigned long ee_uicpp_controls_table_count()
{
    return (unsigned long)UIControlsTableRust::Array().size();
}

extern "C" bool ee_uicpp_controls_table_key(unsigned long inIndex, EeUiControlsTableKey* outKey)
{
    UIControlsTable* theTable = UIControlsTableRust::Array()[inIndex];
    if (theTable == NULL)
        return false;
    outKey->id = theTable->GetID();
    outKey->form_id = theTable->GetFormID();
    outKey->form_index = theTable->GetFormIndex();
    return true;
}

extern "C" EeUiLayout* ee_uicpp_controls_table_layout(unsigned long inIndex)
{
    return reinterpret_cast<EeUiLayout*>(const_cast<UILayoutUtility*>(&UIControlsTableRust::Array()[inIndex]->GetLayout()));
}

// FUNCTION: LLE 0x100677db
// Creates an empty control description (a bitmap with no font, files or layout)
UIControlsTable::UIControlsTable()
    : mNormalFileName(50), mPressedFileName(50), mFocusedFileName(50)
{
    ee_ui_controls_table_init(UIControlsTableRust(this));
    this->mFont = NULL;
}

// FUNCTION: LLE 0x1006796f
// Creates a control description from a controls database record: layout, files, sounds, colours and texture rects
UIControlsTable::UIControlsTable(const UIDbControl& inControl)
{
    ee_ui_controls_table_from_record(UIControlsTableRust(this), reinterpret_cast<const EeUiDbControl*>(&inControl));
    this->mFont = NULL;
    this->mNormalFileName.Copy(inControl.mNormalFileName, 0, strlen(inControl.mNormalFileName), 0, true);
    this->mPressedFileName.Copy(inControl.mPressedFileName, 0, strlen(inControl.mPressedFileName), 0, true);
    this->mFocusedFileName.Copy(inControl.mFocusedFileName, 0, strlen(inControl.mFocusedFileName), 0, true);
    this->mButtonDownSound.Copy(inControl.mButtonDownSound, 0, strlen(inControl.mButtonDownSound), 0, true);
    this->mButtonUpSound.Copy(inControl.mButtonUpSound, 0, strlen(inControl.mButtonUpSound), 0, true);
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
            if (ee_ui_controls_table_record_is_control(theRecord.mID)) {
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

// port: see the header
long UIControlsTable::AddControlLike(UIManager* inManager, long inTemplateID, long inID, long inFormIndex,
                                     unsigned long inGroup, float inLeft, float inRight, float inTop, float inBottom,
                                     long inStringTableBegin, long inStringTableEnd, unsigned long inHelpStringID)
{
    unsigned long theTemplateIndex;
    if (!ee_ui_controls_table_find_template(inTemplateID, inID, inFormIndex, &theTemplateIndex))
        return kErrorNotFound;
    UIControlsTable* theTable = new UIControlsTable(*UIControlsTable::sUIControlsArray[theTemplateIndex]);
    theTable->mEventData.clear();
    theTable->mFont = NULL;
    ee_ui_controls_table_like(UIControlsTableRust(theTable), inID, inFormIndex, inGroup, inLeft, inRight, inTop, inBottom,
                              inStringTableBegin, inStringTableEnd, inHelpStringID);
    UICDatabase theEntry;
    theEntry.mControl = NULL;
    theEntry.mTableIndex = UIControlsTable::sUIControlsArray.size();
    UIControlsTable::sUIControlsArray.push_back(theTable);
    inManager->GetFormByID(theTable->mFormID)->ReserveControlIndex((unsigned long)inFormIndex);
    inManager->GetFormByID(theTable->mFormID)->AddDatabaseControl(theEntry);
    return kErrorOK;
}

// port: see the header
long UIControlsTable::MoveControlsHorizontally(long inFormID, long inFirstIndex, long inLastIndex, float inLeftDelta,
                                               float inRightDelta)
{
    return ee_ui_controls_table_move_horizontally(inFormID, inFirstIndex, inLastIndex, inLeftDelta, inRightDelta);
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
                                             this->mBorderThickness, this->mScaleToControl);
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
    long theListHeight;
    theListRectangle = UIControlsTableRust::Rect(ee_ui_controls_table_combo_list(
        UIControlsTableRust::Rect(inRectangle), this->mScaleToControl, this->mScaleToControl ? this->mFont->GetHeight() : 0,
        this->mBorderThickness, &theListHeight));
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
    long theHeight;
    float theWidth = ee_ui_controls_table_file_combo_size(UIControlsTableRust::Rect(inRectangle), &theHeight);
    theComboBox = new UIFileComboBox(this->mID, 1, theStrings, this->mFont, inRectangle.GetTopLeft(), theWidth, theHeight,
                                     theForm, this->mFormIndex, this->mMaxChars, this->mBorderThickness);
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
    theParent = UIControlsTableRust::Rect(
        ee_ui_controls_table_parent_rect(theWindow->GetWidth(), theWindow->GetHeight(), theForm->GetUseExclusiveCoords()));
    this->mLayout.CalculateRectangle(theParent, theRectangle, theForm->GetUseExclusiveCoords());
    theFontRectangle = UIControlsTableRust::Rect(
        ee_ui_controls_table_font_rect(UIControlsTableRust::Rect(theRectangle), theForm->GetUseExclusiveCoords()));
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
    bool theLoop = this->mLoopMovie;
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
                                           false, this->mStartChecked, this->mScaleToControl);
        break;
    case kDbControlRadioTab:
        theRadioButton = new UIRadioButton(this->mID, theText, this->mFont, inRectangle, theForm, this->mFormIndex, this->mBorderThickness,
                                           true, this->mStartChecked, this->mScaleToControl);
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
        theScrollBar = new UIScrollBar(this->mID, inRectangle.GetTopLeft(),
                                       ee_ui_controls_table_scroll_bar_length(UIControlsTableRust::Rect(inRectangle), false), 0,
                                       theForm, this->mFormIndex, this->mBorderThickness);
        break;
    case kDbControlVerticalScrollBar:
        theScrollBar = new UIScrollBar(this->mID, inRectangle.GetTopLeft(),
                                       ee_ui_controls_table_scroll_bar_length(UIControlsTableRust::Rect(inRectangle), true), 1,
                                       theForm, this->mFormIndex, this->mBorderThickness);
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
    theSlider->SetRange(this->mStartRange, this->mEndRange);
    theSlider->SetCurrentRangeValue(this->mCurrentRange);
    theSlider->SetPageSize(ee_ui_controls_table_slider_page_size(this->mStartRange, this->mEndRange, this->mPageSize));
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
    theLabel->SetFitRectToText(this->mFitRectToText);
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
    if (!inControl)
        return;
    theParent = UIControlsTableRust::Rect(
        ee_ui_controls_table_parent_rect(theWindow->GetWidth(), theWindow->GetHeight(), theForm->GetUseExclusiveCoords()));
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
        if (this->mScaleToControl == true) {
            long theSize;
            if (!ee_ui_controls_table_font_scales_to_window(this->mControlType, inScaleToWindow))
                theSize = ee_ui_controls_table_control_font_size(inRectangle.GetHeight(), this->GetBorderThickness());
            else {
                GEWindow* theWindow = inManager->GetRasterizer()->GetWindow(inManager->GetScreen());
                GEFont* theFont = UIFontTable::GetFont(this->GetFontTableIndex());
                theForm = inManager->GetFormByID(this->GetFormID());
                theSize = ee_ui_controls_table_window_font_size(theFont->GetHeight(), theWindow->GetHeight(),
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
    unsigned long theCount = ee_ui_controls_table_string_count(this->mStringTableBegin, this->mStringTableEnd);
    if (theCount == 0)
        return;
    outStrings.resize(theCount);
    for (unsigned long i = 0; i < theCount; i++)
        inManager->GetStringTable()->GetString(this->mStringTableBegin + (long)i, outStrings[i]);
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
