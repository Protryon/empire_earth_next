// C:\EEX\Low-Level Engine\User Interface\UIForm.h (RECONSTRUCTION). A window of controls: entity groups, controls,
// backgrounds, hot keys, form events and the dirty-rectangle bookkeeping. UIForm.cpp (0x1007042f..0x10075330,
// __FILE__ asserts) held the ctors and the out-of-line members; the inline members were emitted first by
// UIBackground.cpp (0x10059559..0x10059795, declaration order). Vtable 0x10098b08 (20 slots, exported names).
// The original's sizeof was 0x1a0 (??_E stride 0x10059e1c); its members are the Rust form's now (see the class).
#ifndef LLE_UIFORM_H
#define LLE_UIFORM_H
#include "lle/LLEForward.h"

#include <stdint.h>   // int32_t/uint32_t: the on-disk fields' fixed widths

#include <vector>
#include <deque>

#include "lle/UILayoutUtility.h"
#include "rust/ui/formobj.h"
#include "rust/ui/lle_form.h"
#include "rust/ui/ids.h"
#include "rust/ui/obj.h"
#include "lle/UIObjHost.h"
#include "lle/UIShims.h"


// The form record of the UI database (0x10c bytes; UIForm::Create 0x100716c0 reads it). Exported parameter type
// (??0UIForm@@QAE@PAVUIManager@@ABUUIDbDataForms@@_N@Z); field names are provisional, offsets proven by that ctor.
// struct UIDbDataForms: rust/ui/formobj.h (ee_ui formobj.rs DbForm, under the members' names; UI round 13)

// (the original's UICDatabase and UIEventData records, AddDatabaseControl's and AddEvent's parameters, are their two
// numbers: the table index and control; the event ID and type)


// The class is whole in Rust (rust/crates/ee_ui/src/formobj.rs, R6 ui round 16): its state (the controls, groups,
// backgrounds, hot keys, database records, events, the dirty-rectangle lists, the captures, the popup, the help text,
// the layout, the counts) and every method are the Rust object mRust; UIForm.cpp holds the forwarders, the ctors and
// the dtor, the database's Create and the host (the op switch, HostOp: a member, so that it has the friendship
// UIControl, UIHotKey, UIEntityGroup and UITextLabel give UIForm). The game's forms derive from it and override its
// virtuals; the Rust form calls the two it calls itself (SetHelpTextPopupLocation, DrawBackground) through the C++
// object. GetRectangle and GetMouseLocation answer the address of the Rust object's members (GERectangle's and
// GE2DPoint's layouts). The IME passes to the control holding the keyboard are the
// Rust form's (ee_ui lle_form.rs ee_ui_form_pass_to_keyboard), called by the Rust manager.
class LLE_API UIForm {
public:
    // Creates an empty form with the given control and group counts, tool tips and tabbing off
    UIForm(unsigned long inControlCount, unsigned long inControlGroupCount, UIManager* inManager,
           unsigned long inFormID, bool inUseGDIForText)
        : mRust(ee_ui_form_new(this, ee_ui_form_host, inControlCount, inControlGroupCount, inManager, inFormID,
                               inUseGDIForText)) {}
    // Creates a form from its UI database record: counts, layout, tool-tip settings, music file (lower-cased), tabbing
    UIForm(UIManager* inManager, const UIDbDataForms& inData, bool inUseGDIForText)
        : mRust(ee_ui_form_new_db(this, ee_ui_form_host, inManager, &inData, inUseGDIForText)) {}
    // Destroys the form, releasing its events, hot keys, groups, backgrounds, database-created controls and draw lock
    virtual ~UIForm() { ee_ui_form_free(this->mRust); }
    // Starts the form's music (or stops the music for "none"), restores the tab stop and fires the activate event
    virtual void Activate() { this->Call(EeUiFormCall::Activate); }                // slot 0x04
    // Fires the deactivate event, hides the popup and help text, and releases the captured mouse and keyboard
    virtual void Deactivate() { this->Call(EeUiFormCall::Deactivate); }
    // Redraws every visible background that overlaps the rectangle; returns the last draw result
    virtual long DrawBackground(const GERectangle& inRectangle) { return (long)this->CallRect(EeUiFormCall::DrawBackground, 0, 0, inRectangle); } // slot 0x18
    // Lays the form out again inside a new rectangle, with its backgrounds and database controls
    virtual void RelayoutForm(const GERectangle& inRectangle) { this->CallRect(EeUiFormCall::RelayoutForm, 0, 0, inRectangle); }
    // Places the tool tip by the mouse inside the form: shifted left at the right edge, above the cursor near the bottom
    virtual void SetHelpTextPopupLocation() { this->Call(EeUiFormCall::SetHelpTextPopupLocation); }
    // Fires hot keys (higher groups first; one non-always-fire key per frame), then Tab/Shift+Tab, else passes the
    // keyboard to the capturing control or the tab stop
    virtual long UpdateKeyboard() { return (long)this->Call(EeUiFormCall::UpdateKeyboard); }
    // Passes the mouse to the captured or mouse-over control (wheel included), then fires the form's own mouse events
    virtual long UpdateMouse() { return (long)this->Call(EeUiFormCall::UpdateMouse); }
    // Passes a typed character to the control holding the keyboard
    virtual long UpdateTranslatedKey(const char* inKey, short inRepeat) { return ee_ui_form_pass_to_keyboard(this, 4, inKey, NULL, (uint32_t)(int)inRepeat); }
    // Closes the form: fires the close event, clears the draw list and popup, closes the backgrounds, flushes its events
    // and deletes the controls created from the database
    virtual void Close() { this->Call(EeUiFormCall::Close); }                   // slot 0x38
    // Snapshots the form for drawing: fires the update event, copies every dirty shown control, lists the controls to draw,
    // ages the dirty rectangles and updates the help text
    virtual long CopyData() { return (long)this->Call(EeUiFormCall::CopyData); }
    // Redraws the queued background rectangles, then every listed control and the popup (with a shadow under a tool tip)
    virtual long DrawPolygons() { return (long)this->Call(EeUiFormCall::DrawPolygons); }
    // Draws the text of every listed control, then the popup's, and empties the draw list
    virtual long DrawTextA() { return (long)this->Call(EeUiFormCall::DrawText); }
    // Opens the form: lays it out on the screen, opens the backgrounds, creates the database controls and their events,
    // fires the open event and marks every control dirty
    virtual void Open() { this->Call(EeUiFormCall::Open); }
    // The header's members (inline in the original, emitted first by UIBackground.cpp 0x10059559..0x10059795)
    virtual void SetRectangle(const GERectangle& inRectangle) { this->CallRect(EeUiFormCall::SetRectangle, 0, 0, inRectangle); } // slot 0x4c
    // Puts a control into its form slot (when the slot is empty) and adds it to the given group
    void AddControl(UIControl* inControl, unsigned long inGroup) { this->Call(EeUiFormCall::AddControl, (intptr_t)inControl, (intptr_t)inGroup); }
    // Copies a hot key into the given group and the form's list, keeping both sorted by key count, always-fire keys last
    void AddHotKey(const UIHotKey& inHotKey, unsigned long inGroup) { this->Call(EeUiFormCall::AddHotKey, (intptr_t)&inHotKey, (intptr_t)inGroup); }
    void AddHotKey(unsigned long inKey, unsigned long inModifier1, unsigned long inModifier2,
                   unsigned long inModifier3, UIEvent* inJustPressedEvent, UIEvent* inPressedEvent,
                   UIEvent* inJustUnpressedEvent, UIEvent* inDoublePressedEvent, unsigned long inGroup,
                   bool inAlwaysFire)
    {
        ee_ui_form_add_hot_key(this->mRust, inKey, inModifier1, inModifier2, inModifier3, inJustPressedEvent,
                               inPressedEvent, inJustUnpressedEvent, inDoublePressedEvent, inGroup, inAlwaysFire);
    }
    // Draws the part of a bitmap's texture under a rectangle at a screen location
    long DrawBackground(GEBitmap* inBitmap, const GERectangle& inRectangle, const GE2DPoint& inTextureSize) { return (long)this->CallRect(EeUiFormCall::DrawBitmap, (intptr_t)inBitmap, ee_ui_float_pair_bits(inTextureSize.GetX(), inTextureSize.GetY()), inRectangle); }
    // Draws a bitmap tiled over a tile area, clipped to the rectangle, one piece per tile
    long DrawBackground(GEBitmap* inBitmap, const GERectangle& inRectangle, const GERectangle& inDrawRectangle,
                        const GERectangle& inTileRectangle)
    {
        return ee_ui_form_draw_tiles(this->mRust, inBitmap, UIRect(inRectangle), UIRect(inDrawRectangle), UIRect(inTileRectangle));
    }
    // Draws text clipped around the form's clip rectangles, the copied popup or the IME candidate window
    long DrawClippedText(const GERectangle& inRectangle, GEText* inText, UIControl* inControl) { return (long)this->CallRect(EeUiFormCall::DrawClippedText, (intptr_t)inText, (intptr_t)inControl, inRectangle); }
    // Returns the background with the given ID, or NULL
    UIBackground* GetBackgroundByID(unsigned long inID) const { return reinterpret_cast<UIBackground*>(this->Call(EeUiFormCall::BackgroundById, (intptr_t)inID)); }
    UIControl* GetControl(unsigned long inIndex) const { return reinterpret_cast<UIControl*>(this->Call(EeUiFormCall::Control, (intptr_t)inIndex)); }
    // Returns the control with the given ID (NULL: none)
    UIControl* GetControlById(unsigned long inId) { return reinterpret_cast<UIControl*>(this->Call(EeUiFormCall::ControlById, (intptr_t)inId)); }
    unsigned long GetControlCount() const { return (unsigned long)this->Call(EeUiFormCall::ControlCount); }
    unsigned long GetControlGroupCount() const { return (unsigned long)this->Call(EeUiFormCall::ControlGroupCount); }
    unsigned long GetDesignedHeight() const { return (unsigned long)this->Call(EeUiFormCall::DesignedHeight); }
    unsigned long GetDesignedWidth() const { return (unsigned long)this->Call(EeUiFormCall::DesignedWidth); }
    UIEntityGroup* GetEntityGroup(unsigned long inIndex) const { return reinterpret_cast<UIEntityGroup*>(this->Call(EeUiFormCall::EntityGroup, (intptr_t)inIndex)); }
    // Returns the group holding the control with the given ID; falls back to group 0 when no group holds it
    UIEntityGroup* GetEntityGroupById(unsigned long inId) const { return reinterpret_cast<UIEntityGroup*>(this->Call(EeUiFormCall::EntityGroupById, (intptr_t)inId)); }
    unsigned long GetFormID() const { return (unsigned long)this->Call(EeUiFormCall::FormId); }
    UIFormManager* GetFormManager() const { return reinterpret_cast<UIFormManager*>(this->Call(EeUiFormCall::FormManager)); }
    UIEvent* GetFormUpdateEvent() const { return reinterpret_cast<UIEvent*>(this->Call(EeUiFormCall::Event, 13)); }
    // (UITextLabel derives from UIControl alone: the control's address is the label's)
    UITextLabel* GetHelpTextLabel() const { return reinterpret_cast<UITextLabel*>(this->Call(EeUiFormCall::HelpTextLabel)); }
    bool GetIsKeyboardCaptured() const { return this->Call(EeUiFormCall::KeyboardCaptured) != 0; }
    bool GetIsMouseCaptured() const { return this->Call(EeUiFormCall::MouseCaptured) != 0; }
    bool GetUseExclusiveCoords() const { return this->Call(EeUiFormCall::UseExclusiveCoords) != 0; }
    UIManager* GetManager() const { return reinterpret_cast<UIManager*>(this->Call(EeUiFormCall::Manager)); }
    const GE2DPoint& GetMouseLocation() const { return *reinterpret_cast<const GE2DPoint*>(this->Call(EeUiFormCall::MouseLocation)); }
    UIControl* GetMouseOverControl() const { return reinterpret_cast<UIControl*>(this->Call(EeUiFormCall::MouseOverControl)); }
    const GERectangle& GetRectangle() const { return *reinterpret_cast<const GERectangle*>(this->Call(EeUiFormCall::Rectangle)); }
    bool GetUseGDIForText() const { return this->Call(EeUiFormCall::UseGdiForText) != 0; }
    // Deletes every hot key of one group
    void RemoveAllHotKeysForAGroup(unsigned long inGroup) { this->Call(EeUiFormCall::RemoveAllHotKeysForAGroup, (intptr_t)inGroup); }
    // (exported 0x100596d9) Empties the control's form slot and takes it out of the group
    void RemoveControl(UIControl* inControl, unsigned long inGroup) { this->Call(EeUiFormCall::RemoveControl, (intptr_t)inControl, (intptr_t)inGroup); }
    // Deletes the first hot key of a group that matches the key and the three modifiers
    void RemoveHotKey(unsigned long inKey, unsigned long inModifier1, unsigned long inModifier2,
                      unsigned long inModifier3, unsigned long inGroup) { this->Call(EeUiFormCall::RemoveHotKeyKeys, ee_ui_form_pack_keys(inKey, inModifier1, inModifier2, inModifier3), (intptr_t)inGroup); }
    // Deletes the hot key at an index in a group
    void RemoveHotKey(unsigned long inIndex, unsigned long inGroup) { this->Call(EeUiFormCall::RemoveHotKeyAt, (intptr_t)inIndex, (intptr_t)inGroup); }
    // Marks every control of the form dirty
    void SetAllControlsDirty() { this->Call(EeUiFormCall::SetAllControlsDirty); }
    void SetFormActivateEvent(UIEvent* inEvent) { this->Call(EeUiFormCall::SetEvent, 14, (intptr_t)inEvent); }
    void SetFormCloseEvent(UIEvent* inEvent) { this->Call(EeUiFormCall::SetEvent, 1, (intptr_t)inEvent); }
    void SetFormDeactivateEvent(UIEvent* inEvent) { this->Call(EeUiFormCall::SetEvent, 15, (intptr_t)inEvent); }
    // Replaces the form manager, deleting the old one
    void SetFormManager(UIFormManager* inFormManager) { this->Call(EeUiFormCall::SetFormManager, (intptr_t)inFormManager); }
    // one (the event's index in the Rust form: formobj.rs EV_*)
    void SetFormOpenEvent(UIEvent* inEvent) { this->Call(EeUiFormCall::SetEvent, 0, (intptr_t)inEvent); }
    void SetFormUpdateEvent(UIEvent* inEvent) { this->Call(EeUiFormCall::SetEvent, 13, (intptr_t)inEvent); }
    void SetHelpTextTimeDelay(unsigned long inTimeDelay) { this->Call(EeUiFormCall::SetHelpTextTimeDelay, (intptr_t)inTimeDelay); }
    void SetLayout(UILayoutUtility& inLayout) { this->Call(EeUiFormCall::SetLayout, ee_ui_form_pack_types(inLayout.GetLeftOffsetType(), inLayout.GetRightOffsetType(), inLayout.GetTopOffsetType(), inLayout.GetBottomOffsetType()), 0, UIF4(inLayout.GetLeftOffset(), inLayout.GetRightOffset(), inLayout.GetTopOffset(), inLayout.GetBottomOffset())); }
    void SetMouseMiddleButtonPressedEvent(UIEvent* inEvent) { this->Call(EeUiFormCall::SetEvent, 5, (intptr_t)inEvent); }
    void SetMouseMiddleButtonUnpressedEvent(UIEvent* inEvent) { this->Call(EeUiFormCall::SetEvent, 6, (intptr_t)inEvent); }
    void SetMouseWheelMoveEvent(UIEvent* inEvent) { this->Call(EeUiFormCall::SetEvent, 12, (intptr_t)inEvent); }
    void SetMusicFileName(const UWideString& inName) { this->Call(EeUiFormCall::SetMusicFileName, (intptr_t)(const char*)inName); }
    // Turns keyboard tabbing on or off; turning it on moves the tab stop to the first tabbable control
    void SetTabbing(bool inTabbing) { this->Call(EeUiFormCall::SetTabbing, inTabbing); }
    // Moves the tab stop to a control if it exists and is tabbable; otherwise keeps the old stop and returns false
    bool SetTabStop(unsigned long inTabStop) { return this->Call(EeUiFormCall::SetTabStop, (intptr_t)inTabStop) != 0; }
    void SetUseToolTips(bool inUseToolTips) { this->Call(EeUiFormCall::SetUseToolTips, inUseToolTips); }
    // Refreshes the help text from the control under the mouse, re-placing a tool tip that sizes itself to its text
    void UpdateHelpString() { this->Call(EeUiFormCall::UpdateHelpString); }
    bool GetDrawToolTipShadow() const { return this->Call(EeUiFormCall::DrawToolTipShadow) != 0; }
    void SetDrawToolTipShadow(bool inDraw) { this->Call(EeUiFormCall::SetDrawToolTipShadow, inDraw); }
    // the Rust form's calls out that are not Rust's (ee_ui lle_form.rs: ee_ui_form_host comes first)
    // Queues a dirty rectangle for every drawing surface, dropping queued ones it covers; marks an overlapped popup dirty
    void AddDirtyRectangle(const GERectangle& inRectangle) { this->CallRect(EeUiFormCall::AddDirtyRectangle, 0, 0, inRectangle); }
    // the forwarders' one call into the Rust form (with a rectangle)
    intptr_t Call(EeUiFormCall inCall, intptr_t inA = 0, intptr_t inB = 0, float* ioF = NULL) const { return ee_ui_form_call(this->mRust, inCall, inA, inB, ioF); }
    intptr_t CallRect(EeUiFormCall inCall, intptr_t inA, intptr_t inB, const GERectangle& inRectangle) const { return this->Call(inCall, inA, inB, UIF4(inRectangle)); }

    URust<EeUiForm> mRust;                        // the state (formobj.rs)
};

// the enum (UI round 13): rust/ui/ids.h (ee_ui ids.rs)
#endif
