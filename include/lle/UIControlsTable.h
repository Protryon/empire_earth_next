// C:\EEX\Low-Level Engine\User Interface\UIControlsTable.h (RECONSTRUCTION). One control record of the UI database,
// loaded by Create into the static sUIControlsArray (0x1012e3c0); UIForm::Open creates the controls from them.
// UIControlsTable.cpp (0x10065f69..0x10069cc7). Vtable 0x10099430 (the dtor only). sizeof 0x158 (??_E stride
// 0x100666cb). Layout proven by the implicit copy ctor 0x10066153 (every member copied on its own: no arrays) and
// the UIDbControl ctor 0x1006796f. The inline members are all exported; the ones emitted at 0x10065f69..0x10066153
// are in declaration order, the others are ICF-folded with earlier getters (their position here is a guess).
// Requires lle/UIForm.h (UIEventData), lle/UILayoutUtility.h, lle/GEColor.h, <vector>.
#ifndef LLE_UICONTROLSTABLE_H
#define LLE_UICONTROLSTABLE_H

#include <vector>

#include "rust/ui/controls_table.h"
#include "rust/ui/tableobj.h"

class FSFileSpec;
class GEBitmap;
class GEFont;
class GEText;
class UIControl;
class UIForm;
class UIManager;

// The class is whole in Rust (rust/crates/ee_ui/src/tableobj.rs, R6 ui round 18): its record, layout, colours, names,
// events and font, the static array of tables, Create, AddControlLike, MoveControlsHorizontally, DeleteAll,
// CreateControl and its factories, CreateControlEvent, RelayoutControl and SetupFont are the Rust object mRust; the
// getters read its data in place. UIControlsTable.cpp keeps the three members that only call GE objects (CreateBitmap,
// SetupGEText, SetupStringArray; the original's CreateMovie left with the movies) and the host, UIControlsTable::HostOp.
class LLE_API UIControlsTable {
public:
    UIControlsTable();
    virtual ~UIControlsTable();
    void AddEvent(const UIEventData& inEvent);
    static long Create(FSFileSpec& inFile, UIManager* inManager);
    // port: adds a control the database does not have (after Create), described as a copy of the database control
    // inTemplateID (its type, font, colours, sounds and offset types; not its events) with its own id, form index,
    // group, rectangle (fractions of the window: offset type 4), string range and help string; registered with the
    // template's form as Create registers a record. -> kErrorOK; kErrorNotFound without the template or when the
    // id or the form index is taken
    static long AddControlLike(UIManager* inManager, long inTemplateID, long inID, long inFormIndex,
                               unsigned long inGroup, float inLeft, float inRight, float inTop, float inBottom,
                               long inStringTableBegin, long inStringTableEnd, unsigned long inHelpStringID);
    // port: moves the form inFormID's controls with form index inFirstIndex .. inLastIndex horizontally (after
    // Create, before the form makes its controls): inLeftDelta and inRightDelta are added to their left and right
    // offsets, which must be fractions of the window (offset type 4; others are left alone). -> the number moved
    static long MoveControlsHorizontally(long inFormID, long inFirstIndex, long inLastIndex, float inLeftDelta,
                                         float inRightDelta);
    GEBitmap* CreateBitmap(UIManager* inManager, const GERectangle& inRectangle, const UString& inFileName,
                           bool inHasAlpha, const GERectangle& inTextureRectangle) const;
    UIControl* CreateControl(UIManager* inManager);
    void CreateControlEvent(UIForm* inForm);
    GEBitmap* CreateFocusedBitmap(UIManager* inManager, const GERectangle& inRectangle) const
        { return this->CreateBitmap(inManager, inRectangle, this->GetFocusedFileName(), this->GetFocusedHasAlpha(), this->GetFocusedTextureRect()); }
    GEBitmap* CreateNormalBitmap(UIManager* inManager, const GERectangle& inRectangle) const
        { return this->CreateBitmap(inManager, inRectangle, this->GetNormalFileName(), this->GetNormalHasAlpha(), this->GetNormalTextureRect()); }
    GEBitmap* CreatePressedBitmap(UIManager* inManager, const GERectangle& inRectangle) const
        { return this->CreateBitmap(inManager, inRectangle, this->GetPressedFileName(), this->GetPressedHasAlpha(), this->GetPressedTextureRect()); }
    static void DeleteAll();
    static UIControlsTable* GetUIControlsTable(unsigned long inIndex);
    GEColor GetBackground() const { return reinterpret_cast<const GEColor&>(this->D().background); }
    GEColor GetTextForeground() const { return reinterpret_cast<const GEColor&>(this->D().text_foreground); }
    UString GetNormalFileName() const { return UString(this->D().names[0]); }
    UString GetPressedFileName() const { return UString(this->D().names[1]); }
    UString GetFocusedFileName() const { return UString(this->D().names[2]); }
    GEFont* GetFont() const { return static_cast<GEFont*>(this->D().font); }
    GEBitmap* GetNormal() const { return NULL; }    // never set (the original left these members uninitialized)
    float GetBackgroundOpacity() const { return this->F().background_opacity; }
    long GetID() const { return this->F().id; }
    long GetFormID() const { return this->F().form_id; }
    unsigned long GetGroup() const { return this->F().group; }
    unsigned long GetControlType() const { return this->F().control_type; }
    long GetFormIndex() const { return this->F().form_index; }
    long GetBorderThickness() const { return this->F().border_thickness; }
    unsigned long GetStringTableID() const { return this->F().string_table_id; }
    long GetStringTableBegin() const { return this->F().string_table_begin; }
    long GetStringTableEnd() const { return this->F().string_table_end; }
    const UILayoutUtility& GetLayout() const { return reinterpret_cast<const UILayoutUtility&>(this->D().layout); }
    const GERectangle& GetNormalTextureRect() const { return reinterpret_cast<const GERectangle&>(this->D().normal_rect); }
    const GERectangle& GetPressedTextureRect() const { return reinterpret_cast<const GERectangle&>(this->D().pressed_rect); }
    const GERectangle& GetFocusedTextureRect() const { return reinterpret_cast<const GERectangle&>(this->D().focused_rect); }
    bool GetIsVisible() const { return this->F().is_visible; }
    bool GetStartChecked() const { return this->F().start_checked; }
    bool GetFocusedHasAlpha() const { return this->F().focused_has_alpha; }
    bool GetPressedHasAlpha() const { return this->F().pressed_has_alpha; }
    bool GetNormalHasAlpha() const { return this->F().normal_has_alpha; }
    bool GetAlwaysDirty() const { return this->F().always_dirty; }
    bool GetLoopMovie() const { return this->F().loop_movie; }
    void RelayoutControl(UIControl* inControl, UIManager* inManager);
    void SetupFont(UIManager* inManager, const GERectangle& inRectangle, bool inScaleToWindow);
    void SetupGEText(GEText* inText) const;
    void SetupStringArray(UIManager* inManager, std::vector<UWideString>& outStrings) const;
private:
    friend struct UIControlsTableHost;      // UIControlsTable.cpp: the Rust table's calls out
    UIControlsTable(EeUiControlsTable* inRust) : mRust(inRust) {}
    UIControlsTable(const UIControlsTable&);             // not copied (the Rust table is owned)
    UIControlsTable& operator=(const UIControlsTable&);
    const EeUiControlsTableData& D() const { return *ee_ui_controls_table_data(this->mRust); }
    const EeUiControlsTableFields& F() const { return this->D().fields; }

    EeUiControlsTable* mRust;
};

// UI database control type (UIControlsTable::CreateControl; >= 1000: EEUIManager::CreateUserDefinedControl)
enum UIDbControlType {
    kDbControlBitmap = 0,                     // CreateBitmap (provisional name)
    kDbControlButton = 1,                     // CreateButton: new UIButton (provisional name)
    kDbControlCheckBox = 2,                   // CreateCheckBox (provisional name)
    kDbControlEditableComboBox = 3,           // CreateComboBox: UIComboBox text box type 0 (an edit box) (provisional name)
    kDbControlComboBox = 4,                   // CreateComboBox: UIComboBox text box type 1 (a text label; db GS Video Card Combo Box) (provisional name)
    kDbControlNumberMultiLineEditBox = 5,     // CreateEditBox: UIEditBox numbers only + multi-line; text set from the number (provisional name)
    kDbControlNumberEditBox = 6,              // CreateEditBox: UIEditBox numbers only, single line (provisional name)
    kDbControlMultiLineEditBox = 7,           // CreateEditBox: UIEditBox multi-line (db CE Campaign Descrip. Edit Box) (provisional name)
    kDbControlEditBox = 8,                    // CreateEditBox: UIEditBox single line (db Chat Text Edit Box) (provisional name)
    kDbControlListBox = 9,                    // CreateListBox: UILBString (provisional name)
    kDbControlAutoSelectListBox = 10,         // CreateListBox: UILBString with inAutoSelect (select on mouse over) (provisional name)
    kDbControlRadioButton = 11,               // CreateRadioButton (provisional name)
    kDbControlRadioTab = 12,                  // CreateRadioButton with inIsPushButton (db: GS Video Tab, MPS Join By IP Tab) (provisional name)
    kDbControlStatusBar = 13,                 // CreateStatusBar (provisional name)
    kDbControlHorizontalScrollBar = 14,       // CreateScrollBar style 0 (provisional name)
    kDbControlTextLabel = 15,                 // CreateTextLabel: label type 0 (single line) (provisional name)
    kDbControlMultiLineTextLabel = 16,        // CreateTextLabel: label type 1 (multi-line with scroll bar) (provisional name)
    kDbControlHorizontalSlider = 0x11,        // CreateSlider style 0 (provisional name)
    kDbControlVerticalSlider = 0x12,          // CreateSlider style 1 (provisional name)
    kDbControlVerticalScrollBar = 0x13,       // CreateScrollBar style 1 (provisional name)
    kDbControlNumberTextLabel = 0x14,         // CreateTextLabel single line, text = the number in the string-table field (db CB Civ Pts Left (#)) (provisional name)
    kDbControlNumberMultiLineTextLabel = 0x15,  // CreateTextLabel multi-line, number text (provisional name)
    kDbControlFileListBox = 0x16,             // CreateFileListBox (provisional name)
    kDbControlHelpTextLabel = 0x18,           // UIForm::Open takes it as mHelpTextLabel; single line (db Main Screen Tool Tips) (provisional name)
    kDbControlMultiLineHelpTextLabel = 0x19,  // UIForm::Open help text label, multi-line (db GS Help Roll Over Text Display) (provisional name)
    kDbControlScrollingTextLabel = 0x1a,      // CreateScrollingTextLabel: UITLScrolling (provisional name)
    kDbControlBlinkingButton = 0x1b,          // CreateButton: new UIBlinkingButton (provisional name)
    kDbControlScalableButton = 0x1c,          // CreateButton: new UIScalableButton (provisional name)
    kDbControlMovie = 0x1d,                   // no control (the original's CreateMovie: UIMovie; movies left the game) (provisional name)
    kDbControlLineGraph = 0x1e,               // CreateGraph: new UILineGraph (provisional name)
    kDbControlDualButton = 0x1f,              // CreateButton: new UIDualButton (provisional name)
    kDbControlFileComboBox = 0x20,            // CreateFileComboBox: UIFileComboBox (provisional name)
    kDbControlStatusButton = 0x3e8,           // CreateUserDefinedControl: new EEUIFMGStatusButton (RTTI)
    kDbControlMiniMap = 0x3e9,                // CreateUserDefinedControl: new EEUIMiniMap (RTTI)
    kDbControlErrorLog = 0x3eb,               // CreateUserDefinedControl: new EEUIFMGErrorLog (RTTI)
    kDbControlAttribute = 0x3ed,              // CreateUserDefinedControl: new EEUIFMGAttribute (RTTI)
    kDbControlMainGameStatusBar = 0x3ee,      // CreateUserDefinedControl: new EEUIFMGStatusBar (RTTI)
    kDbControlSelectUnits = 0x3ef,            // CreateUserDefinedControl: new EEUICSelectUnits (RTTI)
    kDbControlCommandUnits = 0x3f0,           // CreateUserDefinedControl: new EEUICCommandUnits (RTTI)
    kDbControlPortrait = 0x3f1,               // CreateUserDefinedControl: new EEUICPortrait (RTTI)
    kDbControlChatLine = 0x3f2,               // CreateUserDefinedControl: new EEUICChatLine (RTTI)
    kDbControlCBListBox = 0x3f3,              // CreateUserDefinedControl: new EECBUIListBox (RTTI)
    kDbControlWonNetStatBox = 0x3f5,          // CreateUserDefinedControl: new EEUICWonNetStatBox (RTTI)
    kDbControlUnitIconBar = 0x3f6,            // CreateUserDefinedControl: new EEUICUnitIconBar (RTTI)
    kDbControlSavedGroup = 0x3f7,             // CreateUserDefinedControl: new EEUIBSavedGroup (RTTI)
    kDbControlPGSLineGraph = 0x3f8,           // CreateUserDefinedControl: new EEUIPGSLineGraph (RTTI)
    kDbControlPing = 0x3f9,                   // CreateUserDefinedControl: new EEUICPing (RTTI)
    kDbControlWonNetRoomsBox = 0x3fa,         // CreateUserDefinedControl: new EEUICWonNetRoomsBox (RTTI)
    kDbControlWonNetGamesBox = 0x3fb,         // CreateUserDefinedControl: new EEUICWonNetGamesBox (RTTI)
};

#endif
