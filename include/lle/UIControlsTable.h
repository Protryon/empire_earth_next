// C:\EEX\Low-Level Engine\User Interface\UIControlsTable.h (RECONSTRUCTION). One control record of the UI database,
// loaded by Create into the static sUIControlsArray (0x1012e3c0); UIForm::Open creates the controls from them.
// UIControlsTable.cpp (0x10065f69..0x10069cc7). Vtable 0x10099430 (the dtor only). sizeof 0x158 (??_E stride
// 0x100666cb). Layout proven by the implicit copy ctor 0x10066153 (every member copied on its own: no arrays) and
// the UIDbControl ctor 0x1006796f. The inline members are all exported; the ones emitted at 0x10065f69..0x10066153
// are in declaration order, the others are ICF-folded with earlier getters (their position here is a guess).
// Requires lle/UILayoutUtility.h, lle/GEColor.h, <vector>.
#ifndef LLE_UICONTROLSTABLE_H
#define LLE_UICONTROLSTABLE_H
#include "lle/LLEForward.h"

#include <vector>

#include "rust/ui/controls_table.h"
#include "rust/ui/lle_table.h"
#include "lle/UIShims.h"
#include "lle/UIObjHost.h"
#include "rust/ui/ids.h"
#include "rust/ui/tableobj.h"
#include "rust/ui/lle_hosts.h"


// The class is whole in Rust (rust/crates/ee_ui/src/tableobj.rs, R6 ui round 18): its record, layout, colours, names,
// events and font, the static array of tables, Create, AddControlLike, MoveControlsHorizontally, DeleteAll,
// CreateControl and its factories, CreateControlEvent, RelayoutControl and SetupFont are the Rust object mRust; the
// getters read its data in place. UIControlsTable.cpp holds the Rust table's calls out, one statement each (ee_ui
// lle_hosts_tables.rs table, UI round 16; the original's CreateMovie left with the movies).
class LLE_API UIControlsTable {
public:
    // Destroys the control description, releasing its font
    virtual ~UIControlsTable() { ee_ui_controls_table_free(this->mRust); }
    void AddEvent(unsigned long inEventID, unsigned long inEventType) { ee_ui_controls_table_call(this->mRust, EeUiTableCall::AddEvent, inEventID, inEventType, NULL); }
    // Loads every control record from the file and registers each valid one with its form; returns the last read result
    static long Create(FSFileSpec& inFile, UIManager* inManager)
    {
        return (long)ee_ui_controls_tables(EeUiTablesCall::Create, (intptr_t)&inFile, (intptr_t)inManager,
                                           ee_ui_table_host, NULL);
    }
    // port: adds a control the database does not have (after Create), described as a copy of the database control
    // inTemplateID (its type, font, colours, sounds and offset types; not its events) with its own id, form index,
    // group, rectangle (fractions of the window: offset type 4), string range and help string; registered with the
    // template's form as Create registers a record. -> kErrorOK; kErrorNotFound without the template or when the
    // id or the form index is taken
    // port: see the header
    static long AddControlLike(UIManager* inManager, long inTemplateID, long inID, long inFormIndex,
                               unsigned long inGroup, float inLeft, float inRight, float inTop, float inBottom,
                               long inStringTableBegin, long inStringTableEnd, unsigned long inHelpStringID) { return (long)ee_ui_controls_tables(EeUiTablesCall::AddControlLike, (intptr_t)inManager, (intptr_t)&(const EeUiTableLike&)EeUiTableLike{inTemplateID, inID, inFormIndex, inGroup, inLeft, inRight, inTop, inBottom, inStringTableBegin, inStringTableEnd, inHelpStringID}, NULL, NULL); }
    // port: moves the form inFormID's controls with form index inFirstIndex .. inLastIndex horizontally (after
    // Create, before the form makes its controls): inLeftDelta and inRightDelta are added to their left and right
    // offsets, which must be fractions of the window (offset type 4; others are left alone). -> the number moved
    static long MoveControlsHorizontally(long inFormID, long inFirstIndex, long inLastIndex, float inLeftDelta, float inRightDelta) { return (long)ee_ui_controls_tables(EeUiTablesCall::MoveControlsHorizontally, inFormID, (intptr_t)((uint64_t)(uint32_t)inFirstIndex | (uint64_t)(uint32_t)inLastIndex << 32), NULL, UIF4(inLeftDelta, inRightDelta)); }
    // Loads a UI texture (clamped, point mip, given alpha) and returns a bitmap stretched over the rectangle, or NULL
    GEBitmap* CreateBitmap(UIManager* inManager, const GERectangle& inRectangle, const UString& inFileName,
                           bool inHasAlpha, const GERectangle& inTextureRectangle) const { return (GEBitmap*)ee_ui_table_create_bitmap(inManager, &inRectangle, &inFileName, inHasAlpha, &inTextureRectangle); }
    // Lays out the control on the window and creates it by type (user types via the manager), then sets its background
    UIControl* CreateControl(UIManager* inManager)
    {
        return reinterpret_cast<UIControl*>(
            ee_ui_controls_table_call(this->mRust, EeUiTableCall::CreateControl, (intptr_t)inManager, 0, NULL));
    }
    // Attaches the description's trigger events to the form's control, creating each event through the manager
    void CreateControlEvent(UIForm* inForm) { ee_ui_controls_table_call(this->mRust, EeUiTableCall::CreateControlEvent, (intptr_t)inForm, 0, NULL); }
    GEBitmap* CreateFocusedBitmap(UIManager* inManager, const GERectangle& inRectangle) const
        { return this->CreateBitmap(inManager, inRectangle, this->GetFocusedFileName(), this->GetFocusedHasAlpha(), this->GetFocusedTextureRect()); }
    GEBitmap* CreateNormalBitmap(UIManager* inManager, const GERectangle& inRectangle) const
        { return this->CreateBitmap(inManager, inRectangle, this->GetNormalFileName(), this->GetNormalHasAlpha(), this->GetNormalTextureRect()); }
    GEBitmap* CreatePressedBitmap(UIManager* inManager, const GERectangle& inRectangle) const
        { return this->CreateBitmap(inManager, inRectangle, this->GetPressedFileName(), this->GetPressedHasAlpha(), this->GetPressedTextureRect()); }
    // Deletes every control description and empties the table
    static void DeleteAll() { ee_ui_controls_tables(EeUiTablesCall::DeleteAll, 0, 0, NULL, NULL); }
    static UIControlsTable* GetUIControlsTable(unsigned long inIndex) { return reinterpret_cast<UIControlsTable*>(ee_ui_controls_tables(EeUiTablesCall::At, inIndex, 0, NULL, NULL)); }
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
    // Recomputes a created control's rectangle and font for the current window size
    void RelayoutControl(UIControl* inControl, UIManager* inManager)
    {
        ee_ui_controls_table_call(this->mRust, EeUiTableCall::RelayoutControl, (intptr_t)inControl, (intptr_t)inManager,
                                  NULL);
    }
    // Picks the description's font: scaled to the control or window height when the record asks, else the table font
    void SetupFont(UIManager* inManager, const GERectangle& inRectangle, bool inScaleToWindow) { ee_ui_controls_table_call(this->mRust, EeUiTableCall::SetupFont, (intptr_t)inManager, inScaleToWindow, const_cast<float*>(&inRectangle.mBottom)); }
    // Sets text to transparent in the description's text colour (ee_ui lle_hosts.rs)
    void SetupGEText(GEText* inText) const { ee_ui_table_setup_text(const_cast<UIControlsTable*>(this), inText); }
    // Fills a list with the description's string-table range (empty when the range is unset)
    void SetupStringArray(UIManager* inManager, std::vector<UWideString>& outStrings) const { ee_ui_table_setup_strings(this, inManager, &outStrings); }
    UIControlsTable(EeUiControlsTable* inRust) : mRust(inRust) {}
    const EeUiControlsTableData& D() const { return *ee_ui_controls_table_data(this->mRust); }
    const EeUiControlsTableFields& F() const { return this->D().fields; }

    URust<EeUiControlsTable> mRust;
};

// the enum (UI round 13): rust/ui/ids.h (ee_ui ids.rs)
#endif
