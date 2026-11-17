// C:\EEX\Low-Level Engine\User Interface\UIControlsTable.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10065f69..0x10069cc7: UIControlsTable's header-inline block (0x10065f69, declaration order; the getters ICF
// folded into earlier units are missing), its implicit copy members and ??_E, the U2D key init chains (0x1006672c),
// then the header-inline/implicit copy members and ??_E of every control class this unit includes first
// (UIDualButton, UIFileComboBox, UIFileListBox, UIGraph, UILineGraph, UIMovie, UITLScrolling, UIStatusBar,
// UIFontTable), the sUIControlsArray init chain (0x10067792), the ctors, the dtor and the members in alphabetical
// order, then the vector<UString> COMDATs (0x10069ad4..). UIDualButton.cpp starts at 0x10069cc7.
//
// UIControlsTable is whole in Rust (rust/crates/ee_ui/src/tableobj.rs, R6 ui round 18): the record, the static array,
// Create, the factories and the font are the Rust object; this unit keeps the members that only call GE objects and
// the host of its calls out (UIControlsTableHost: the controls' constructors and setters, the files, fonts, strings).
#include <windows.h>
#include <stddef.h>

#include "rust/ui/obj.h"
#include "rust/ui/rect.h"
#include "rust/ui/tableobj.h"
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
#include "lle/UIMovie.h"
#include "lle/UIRadioButton.h"
#include "lle/UIScalableButton.h"
#include "lle/UITLScrolling.h"
#include "lle/UIStatusBar.h"
#include "lle/UIFontTable.h"
#include "lle/UIManager.h"

// The Rust table's calls out (EeUiOp, obj.rs; ee_ui tableobj.rs): a is the form, font, file, string, bitmap,
// control or event the op names, d the manager where the op needs one; rectangles and colours travel in f
struct UIControlsTableHost {
    static GERectangle R(const float* f) { return GERectangle(f[1], f[3], f[2], f[0]); }
    static GERectangle R(const EeUiRect& r) { return GERectangle(r.left, r.top, r.right, r.bottom); }
    static GEColor C(const float* f) { return GEColor(f[0], f[1], f[2]); }

    // A control's constructor (the factories' new: CreateBitmap .. CreateTextLabel)
    static UIControl* Make(const EeUiTableMake& m)
    {
        GERectangle r = R(m.rect);
        UIForm* theForm = reinterpret_cast<UIForm*>(m.form);
        GEFont* theFont = reinterpret_cast<GEFont*>(m.font);
        const UWideString* theText = reinterpret_cast<const UWideString*>(m.text);
        std::vector<UWideString>* theStrings = reinterpret_cast<std::vector<UWideString>*>(m.strings);
        GEBitmap* n = reinterpret_cast<GEBitmap*>(m.normal);
        GEBitmap* fo = reinterpret_cast<GEBitmap*>(m.focused);
        GEBitmap* p = reinterpret_cast<GEBitmap*>(m.pressed);
        switch (m.kind) {
        case EeUiTableKind::Bitmap: return new UIBitmap(m.id, n, r, theForm, m.index);
        case EeUiTableKind::Button: return new UIButton(m.id, n, fo, p, r, theForm, m.index, m.border);
        case EeUiTableKind::BlinkingButton:
            return new UIBlinkingButton(m.id, n, fo, p, r, theForm, m.index, m.border, m.blink_rate);
        case EeUiTableKind::ScalableButton:
            return new UIScalableButton(m.id, n, fo, p, R(m.normal_rect), R(m.focused_rect), R(m.pressed_rect), r, r,
                                        theForm, m.index);
        case EeUiTableKind::DualButton: return new UIDualButton(m.id, n, fo, p, r, theForm, m.index, m.border);
        case EeUiTableKind::CheckBox:
            return new UICheckBox(m.id, *theText, theFont, r, theForm, m.index, m.border, m.flag_a);
        case EeUiTableKind::ComboBox:
            return new UIComboBox(m.id, m.style, *theStrings, theFont, r, m.list_height, theForm, m.index, m.max_chars,
                                  m.border);
        case EeUiTableKind::FileComboBox:
            return new UIFileComboBox(m.id, 1, *theStrings, theFont, r.GetTopLeft(), m.length, m.list_height, theForm,
                                      m.index, m.max_chars, m.border);
        case EeUiTableKind::EditBox:
            return new UIEditBox(m.id, theFont, m.max_chars, m.flag_a, r, theForm, m.index, m.border, m.flag_b, m.flag_c);
        case EeUiTableKind::FileListBox: return new UIFileListBox(m.id, theFont, false, r, theForm, m.index, m.border);
        case EeUiTableKind::LineGraph: return new UIGraph(m.id, r, theForm, m.index, m.border);
        case EeUiTableKind::ListBox:
            return new UILBString(m.id, *theStrings, theFont, m.flag_a, r, theForm, m.index, m.border);
        case EeUiTableKind::RadioButton:
            return new UIRadioButton(m.id, *theText, theFont, r, theForm, m.index, m.border, m.flag_a, m.flag_b, m.flag_c);
        case EeUiTableKind::ScrollBar:
            return new UIScrollBar(m.id, r.GetTopLeft(), m.length, m.style, theForm, m.index, m.border);
        case EeUiTableKind::ScrollingLabel: return new UITLScrolling(*theText, theFont, m.id, r, theForm, m.index, m.border);
        case EeUiTableKind::Slider: return new UISlider(m.id, r, m.style, theForm, m.index, m.border);
        case EeUiTableKind::StatusBar: return new UIStatusBar(m.id, r, theForm, m.index, m.border);
        case EeUiTableKind::TextLabel:
            return new UITextLabel(*theText, theFont, m.id, r, theForm, m.style, m.index, m.border);
        }
        return NULL;
    }

    // SetupGEText of a made control's text (EeUiTableSet::SetupText's classes)
    static GEText* Text(UIControl* c, intptr_t inClass)
    {
        switch (inClass) {
        case 0: return static_cast<UIButton*>(c)->GetGEText();
        case 1: return static_cast<UICheckBox*>(c)->GetTextLabel()->GetGEText();
        case 2: return static_cast<UIComboBox*>(c)->GetGEText();
        case 3: return static_cast<UIEditBox*>(c)->GetGEText();
        case 4: return static_cast<UILBString*>(c)->GetGEText();
        case 5: return static_cast<UIRadioButton*>(c)->GetTextLabel()->GetGEText();
        case 6: return static_cast<UITextLabel*>(c)->GetGEText();
        default: return static_cast<UIStatusBar*>(c)->GetGEText();
        }
    }

    // A made control's setter
    static void Set(const UIControlsTable* t, UIControl* c, EeUiTableSet inSet, intptr_t a, intptr_t b, const float* f)
    {
        UIButton* theButton = static_cast<UIButton*>(c);
        UIComboBox* theCombo = static_cast<UIComboBox*>(c);
        UILBString* theList = static_cast<UILBString*>(c);
        UIRadioButton* theRadio = static_cast<UIRadioButton*>(c);
        UIStatusBar* theBar = static_cast<UIStatusBar*>(c);
        UITextLabel* theLabel = static_cast<UITextLabel*>(c);
        GEBitmap* theBitmap = reinterpret_cast<GEBitmap*>(a);
        const UWideString& theText = *reinterpret_cast<const UWideString*>(a);
        switch (inSet) {
        case EeUiTableSet::ButtonText: theButton->SetText(theText, reinterpret_cast<GEFont*>(b)); break;
        case EeUiTableSet::ButtonFocusedColor: theButton->SetFocusedTextColor(C(f)); break;
        case EeUiTableSet::ButtonDownSound: theButton->SetButtonDownSound(UString(reinterpret_cast<const char*>(a))); break;
        case EeUiTableSet::ButtonUpSound: theButton->SetButtonUpSound(UString(reinterpret_cast<const char*>(a))); break;
        case EeUiTableSet::SetupText: t->SetupGEText(Text(c, a)); break;
        case EeUiTableSet::CheckSetChecked: static_cast<UICheckBox*>(c)->SetChecked(a != 0); break;
        case EeUiTableSet::ComboFocusedRect: theCombo->SetListBoxFocusedRectColor(C(f)); break;
        case EeUiTableSet::ComboSelectedBg: theCombo->SetListBoxSelectedBgColor(C(f)); break;
        case EeUiTableSet::ComboSelection: theCombo->SetCurrentSelection((long)a); break;
        case EeUiTableSet::ComboTextBoxOpacity: theCombo->SetTextBoxBgOpacity(f[0]); break;
        case EeUiTableSet::ComboTextBoxColor: theCombo->SetTextBoxBgColor(C(f)); break;
        case EeUiTableSet::ComboListAlwaysDirty: theCombo->SetListBoxAlwaysDirty(a != 0); break;
        case EeUiTableSet::EditNumber: static_cast<UIEditBox*>(c)->SetText((unsigned long)a); break;
        case EeUiTableSet::EditText: static_cast<UIEditBox*>(c)->SetText(theText); break;
        case EeUiTableSet::EditSelectedBg: static_cast<UIEditBox*>(c)->SetSelectedBgColor(C(f)); break;
        case EeUiTableSet::ListFocusedRect: theList->SetFocusedRectColor(C(f)); break;
        case EeUiTableSet::ListSelectedBg: theList->GetSelectedBackground()->SetColor(C(f)); break;
        case EeUiTableSet::ListScrollOpacity: theList->SetScrollBarBgOpacity(f[0]); break;
        case EeUiTableSet::ListSelection: theList->ChangeSelectionIndex((unsigned long)a, false, false); break;
        case EeUiTableSet::FileListExtensions: {
            std::vector<UWideString>& theStrings = *reinterpret_cast<std::vector<UWideString>*>(a);
            UString theExtension;
            for (unsigned long i = 0; i < theStrings.size(); i++) {
                theExtension = theStrings[i];
                static_cast<UIFileListBox*>(c)->AddValidExtension(theExtension);
            }
            break;
        }
        case EeUiTableSet::GraphFont: static_cast<UIGraph*>(c)->SetFont(reinterpret_cast<GEFont*>(a)); break;
        case EeUiTableSet::GraphFontColor: static_cast<UIGraph*>(c)->SetFontColor(C(f)); break;
        case EeUiTableSet::RadioUpUnchecked: theRadio->SetUpUncheckedBitmap(theBitmap); break;
        case EeUiTableSet::RadioPressed:
            theRadio->SetDownUncheckedBitmap(theBitmap);
            theRadio->SetDownCheckedBitmap(theBitmap);
            theRadio->SetUpCheckedBitmap(theBitmap);
            break;
        case EeUiTableSet::ScrollRange: static_cast<UIScrollBar*>(c)->SetRange((long)a, (long)b); break;
        case EeUiTableSet::ScrollValue: static_cast<UIScrollBar*>(c)->SetCurrentRangeValue((long)a); break;
        case EeUiTableSet::SliderRange: static_cast<UISlider*>(c)->SetRange((long)a, (long)b); break;
        case EeUiTableSet::SliderValue: static_cast<UISlider*>(c)->SetCurrentRangeValue((long)a); break;
        case EeUiTableSet::SliderPage: static_cast<UISlider*>(c)->SetPageSize((long)a); break;
        case EeUiTableSet::StatusRange: theBar->SetRange((long)a, (long)b); break;
        case EeUiTableSet::StatusValue: theBar->SetCurrentRangeValue((long)a); break;
        case EeUiTableSet::StatusColor: theBar->SetStatusBarColor(C(f)); break;
        case EeUiTableSet::StatusOpacity: theBar->SetStatusBarOpacity(f[0]); break;
        case EeUiTableSet::StatusText: theBar->SetText(theText, reinterpret_cast<GEFont*>(b)); break;
        case EeUiTableSet::StatusFocusedColor: theBar->SetFocusedTextColor(C(f)); break;
        case EeUiTableSet::LabelNumber: theLabel->SetText((long)a); break;
        case EeUiTableSet::LabelAlign: theLabel->SetAlignment((long)a); break;
        case EeUiTableSet::LabelScrollOpacity: theLabel->SetScrollBarBgOpacity(f[0]); break;
        case EeUiTableSet::LabelFit: theLabel->SetFitRectToText(a != 0); break;
        case EeUiTableSet::BgColor: c->SetBgColor(C(f)); break;
        case EeUiTableSet::BgOpacity: c->SetBgOpacity(f[0]); break;
        case EeUiTableSet::AlwaysDirty: c->SetAlwaysDirty(a != 0); break;
        case EeUiTableSet::SetRectangle: c->SetRectangle(R(f)); break;
        case EeUiTableSet::ChangeFont: c->ChangeFont(reinterpret_cast<GEFont*>(a)); break;
        case EeUiTableSet::SetTrigger: c->SetTriggerEvent((unsigned long)a, reinterpret_cast<UIEvent*>(b)); break;
        }
    }

    static intptr_t Op(void* inTable, EeUiOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d, float* f)
    {
        UIControlsTable* t = static_cast<UIControlsTable*>(inTable);
        UIManager* m = reinterpret_cast<UIManager*>(d);
        UIForm* theForm = reinterpret_cast<UIForm*>(a);
        GEFont* theFont = reinterpret_cast<GEFont*>(a);
        FSFileSpec* theFile = reinterpret_cast<FSFileSpec*>(a);
        switch (inOp) {
        case EeUiOp::TblWindowWidth: return m->GetRasterizer()->GetWindow(m->GetScreen())->GetWidth();
        case EeUiOp::TblWindowHeight: return m->GetRasterizer()->GetWindow(m->GetScreen())->GetHeight();
        case EeUiOp::TblFormById: return (intptr_t)m->GetFormByID((unsigned long)a);
        case EeUiOp::TblFormExclusive: return theForm->GetUseExclusiveCoords();
        case EeUiOp::TblFormDesignedHeight: return theForm->GetDesignedHeight();
        case EeUiOp::TblFormGdi: return theForm->GetUseGDIForText();
        case EeUiOp::TblFontHeight: return theFont->GetHeight();
        case EeUiOp::TblFontHasBitmap: return theFont->GetFontBitmap() != NULL;
        case EeUiOp::TblStringLength: return reinterpret_cast<const UWideString*>(a)->GetLength();
        case EeUiOp::TblFileOpen: theFile->Open(kFileAccessRead); return 0;
        case EeUiOp::TblFileReadCount: {
            unsigned long theCount = 0;
            long theResult = theFile->Read(theCount);
            return (intptr_t)((uint64_t)theCount << 32 | (uint32_t)theResult);
        }
        case EeUiOp::TblFileRead: {
            unsigned long theRead = 0;
            return theFile->Read(reinterpret_cast<unsigned char*>(b), (unsigned long)c, theRead);
        }
        case EeUiOp::TblFileClose: theFile->Close(); return 0;
        case EeUiOp::TblWrap: return (intptr_t) new UIControlsTable(reinterpret_cast<EeUiControlsTable*>(a));
        case EeUiOp::TblDelete: delete reinterpret_cast<UIControlsTable*>(a); return 0;
        case EeUiOp::TblAddDatabaseControl: {
            UICDatabase theEntry;
            theEntry.mControl = NULL;
            theEntry.mTableIndex = (unsigned long)b;
            theForm->AddDatabaseControl(theEntry);
            return 0;
        }
        case EeUiOp::TblReserveIndex: theForm->ReserveControlIndex((unsigned long)b); return 0;
        case EeUiOp::TblStringNew: {
            UWideString* theString = new UWideString;
            if (a > 0)
                m->GetStringTable()->GetString((unsigned long)a, *theString);
            return (intptr_t)theString;
        }
        case EeUiOp::TblStringDelete: delete reinterpret_cast<UWideString*>(a); return 0;
        case EeUiOp::TblStringsNew: {
            std::vector<UWideString>* theStrings = new std::vector<UWideString>;
            t->SetupStringArray(m, *theStrings);
            return (intptr_t)theStrings;
        }
        case EeUiOp::TblStringsDelete: delete reinterpret_cast<std::vector<UWideString>*>(a); return 0;
        case EeUiOp::TblFontGet: return (intptr_t)UIFontTable::GetFont((unsigned long)a);
        case EeUiOp::TblFontClosest: return (intptr_t)UIFontTable::CreateClosestFont((unsigned long)a, (unsigned long)b);
        case EeUiOp::TblFontRelease: theFont->Dereference(); return 0;
        case EeUiOp::TblFontBitmapNew: new GEFontBitmap(m->GetRasterizer(), theFont, m->GetScreen()); return 0;
        case EeUiOp::TblBitmapNew: {
            GERectangle r = R(f);
            return (intptr_t)(a == 0 ? t->CreateNormalBitmap(m, r) : a == 1 ? t->CreatePressedBitmap(m, r) : t->CreateFocusedBitmap(m, r));
        }
        // the original's radio button released a bitmap that failed to load (a NULL read); none is released here
        case EeUiOp::TblBitmapRelease:
            if (a)
                reinterpret_cast<GEBitmap*>(a)->Dereference();
            return 0;
        case EeUiOp::TblMake: return (intptr_t)Make(*reinterpret_cast<const EeUiTableMake*>(a));
        case EeUiOp::TblSet: Set(t, reinterpret_cast<UIControl*>(a), (EeUiTableSet)b, c, d, f); return 0;
        case EeUiOp::TblUserDefined: return (intptr_t)m->CreateUserDefinedControl(*t);
        case EeUiOp::TblMovie: return (intptr_t)t->CreateMovie(m, R(f));
        case EeUiOp::TblEventNew: return (intptr_t)theForm->GetManager()->CreateControlEvent(theForm, (unsigned long)b);
        case EeUiOp::TblEventRelease: reinterpret_cast<UIEvent*>(a)->Dereference(); return 0;
        case EeUiOp::TblFormControl: return (intptr_t)theForm->GetControl((unsigned long)b);
        default: return 0;
        }
    }
};

// Creates an empty control description (a bitmap with no font, files or layout)
UIControlsTable::UIControlsTable() : mRust(ee_ui_controls_table_new(this, UIControlsTableHost::Op)) {}

// Destroys the control description, releasing its font
UIControlsTable::~UIControlsTable() { ee_ui_controls_table_free(this->mRust); }

void UIControlsTable::AddEvent(const UIEventData& inEvent)
{
    ee_ui_controls_table_call(this->mRust, EeUiTableCall::AddEvent, inEvent.mEventID, inEvent.mEventType, NULL);
}

// Loads every control record from the file and registers each valid one with its form; returns the last read result
long UIControlsTable::Create(FSFileSpec& inFile, UIManager* inManager)
{
    return (long)ee_ui_controls_tables(EeUiTablesCall::Create, (intptr_t)&inFile, (intptr_t)inManager,
                                       UIControlsTableHost::Op, NULL);
}

// port: see the header
long UIControlsTable::AddControlLike(UIManager* inManager, long inTemplateID, long inID, long inFormIndex,
                                     unsigned long inGroup, float inLeft, float inRight, float inTop, float inBottom,
                                     long inStringTableBegin, long inStringTableEnd, unsigned long inHelpStringID)
{
    EeUiTableLike theLike = {inTemplateID, inID, inFormIndex, inGroup, inLeft, inRight, inTop, inBottom,
                             inStringTableBegin, inStringTableEnd, inHelpStringID};
    return (long)ee_ui_controls_tables(EeUiTablesCall::AddControlLike, (intptr_t)inManager, (intptr_t)&theLike, NULL, NULL);
}

// port: see the header
long UIControlsTable::MoveControlsHorizontally(long inFormID, long inFirstIndex, long inLastIndex, float inLeftDelta,
                                               float inRightDelta)
{
    float f[4] = {inLeftDelta, inRightDelta, 0.0f, 0.0f};
    return (long)ee_ui_controls_tables(EeUiTablesCall::MoveControlsHorizontally, inFormID,
                                       (intptr_t)((uint64_t)(uint32_t)inFirstIndex | (uint64_t)(uint32_t)inLastIndex << 32),
                                       NULL, f);
}

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

// Lays out the control on the window and creates it by type (user types via the manager), then sets its background
UIControl* UIControlsTable::CreateControl(UIManager* inManager)
{
    return reinterpret_cast<UIControl*>(
        ee_ui_controls_table_call(this->mRust, EeUiTableCall::CreateControl, (intptr_t)inManager, 0, NULL));
}

// Attaches the description's trigger events to the form's control, creating each event through the manager
void UIControlsTable::CreateControlEvent(UIForm* inForm)
{
    ee_ui_controls_table_call(this->mRust, EeUiTableCall::CreateControlEvent, (intptr_t)inForm, 0, NULL);
}

// Creates a movie control playing the description's file from the startup folder (looping per a record flag)
UIControl* UIControlsTable::CreateMovie(UIManager* inManager, GERectangle inRectangle) const
{
    FSFileSpec theFile;
    UString theFileName;
    UIForm* theForm = inManager->GetFormByID(this->GetFormID());
    bool theLoop = this->GetLoopMovie();
    theFile.SetVolume(gStartupVolume);
    theFile.SetDirectory(gStartupDirectory);
    theFileName = this->GetNormalFileName();
    theFile.SetFile(theFileName);
    GEMovie* theMovie = inManager->GetRasterizer()->CreateMovie(theFile, inManager->GetScreen(), inRectangle,
                                                                theLoop);
    UIControl* theControl = new UIMovie(this->GetID(), theMovie, inRectangle, theForm, this->GetFormIndex());
    if (theMovie)
        theMovie->Dereference();
    return theControl;
}

// Deletes every control description and empties the table
void UIControlsTable::DeleteAll() { ee_ui_controls_tables(EeUiTablesCall::DeleteAll, 0, 0, NULL, NULL); }

UIControlsTable* UIControlsTable::GetUIControlsTable(unsigned long inIndex)
{
    return reinterpret_cast<UIControlsTable*>(ee_ui_controls_tables(EeUiTablesCall::At, inIndex, 0, NULL, NULL));
}

// Recomputes a created control's rectangle and font for the current window size
void UIControlsTable::RelayoutControl(UIControl* inControl, UIManager* inManager)
{
    ee_ui_controls_table_call(this->mRust, EeUiTableCall::RelayoutControl, (intptr_t)inControl, (intptr_t)inManager,
                              NULL);
}

// Picks the description's font: scaled to the control or window height when the record asks, else the table font
void UIControlsTable::SetupFont(UIManager* inManager, const GERectangle& inRectangle, bool inScaleToWindow)
{
    float f[4] = {inRectangle.mBottom, inRectangle.mLeft, inRectangle.mRight, inRectangle.mTop};
    ee_ui_controls_table_call(this->mRust, EeUiTableCall::SetupFont, (intptr_t)inManager, inScaleToWindow, f);
}

// Sets text to transparent in the description's text colour
void UIControlsTable::SetupGEText(GEText* inText) const
{
    inText->SetIsTransparent(true);
    inText->SetFgColor(this->GetTextForeground());
}

// Fills a list with the description's string-table range (empty when the range is unset)
void UIControlsTable::SetupStringArray(UIManager* inManager, std::vector<UWideString>& outStrings) const
{
    outStrings.clear();
    unsigned long theCount = ee_ui_controls_table_string_count(this->GetStringTableBegin(), this->GetStringTableEnd());
    if (theCount == 0)
        return;
    outStrings.resize(theCount);
    for (unsigned long i = 0; i < theCount; i++)
        inManager->GetStringTable()->GetString(this->GetStringTableBegin() + (long)i, outStrings[i]);
}
