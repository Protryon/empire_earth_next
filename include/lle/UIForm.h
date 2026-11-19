// C:\EEX\Low-Level Engine\User Interface\UIForm.h (RECONSTRUCTION). A window of controls: entity groups, controls,
// backgrounds, hot keys, form events and the dirty-rectangle bookkeeping. UIForm.cpp (0x1007042f..0x10075330,
// __FILE__ asserts) held the ctors and the out-of-line members; the inline members were emitted first by
// UIBackground.cpp (0x10059559..0x10059795, declaration order). Vtable 0x10098b08 (20 slots, exported names).
// The original's sizeof was 0x1a0 (??_E stride 0x10059e1c); its members are the Rust form's now (see the class).
#ifndef LLE_UIFORM_H
#define LLE_UIFORM_H

#include <stdint.h>   // int32_t/uint32_t: the on-disk fields' fixed widths

#include <vector>
#include <deque>

#include "lle/UILayoutUtility.h"
#include "platform/PlatWindow.h"   // PlatKeyIsToggled
#include "rust/ui/formobj.h"
#include "rust/ui/obj.h"

class FSFileSpec;
class GEBitmap;
class GEText;
class ISEntity;
class TMPoll;
class TSCriticalSection;
class UIBackground;
class UIControl;
class UIEMouseWheelScroll;
class UIEntityGroup;
class UIEvent;
class UIFormManager;
class UIHotKey;
class UIManager;
class UITextLabel;

// The form record of the UI database (0x10c bytes; UIForm::Create 0x100716c0 reads it). Exported parameter type
// (??0UIForm@@QAE@PAVUIManager@@ABUUIDbDataForms@@_N@Z); field names are provisional, offsets proven by that ctor.
struct UIDbDataForms {
    char mLabel[0x64];                      // +0x000
    char mMusicFile[0x64];                  // +0x064 "none" stops the music
    int32_t mUnusedID;                      // +0x0c8 (provisional name)
    int32_t mFormID;                        // +0x0cc 0 or less: no form
    uint32_t mControlCount;                 // +0x0d0
    uint32_t mControlGroupCount;            // +0x0d4
    float mLeftOffset;                      // +0x0d8
    float mRightOffset;                     // +0x0dc
    float mBottomOffset;                    // +0x0e0
    float mTopOffset;                       // +0x0e4
    uint32_t mDesignedWidth;                // +0x0e8
    uint32_t mDesignedHeight;               // +0x0ec
    uint32_t mHelpTextTimeDelay;            // +0x0f0
    uint32_t mUnused;                       // +0x0f4 (provisional name)
    uint32_t mLeftOffsetType;               // +0x0f8
    uint32_t mRightOffsetType;              // +0x0fc
    uint32_t mTopOffsetType;                // +0x100
    uint32_t mBottomOffsetType;             // +0x104
    bool mTabbingOn;                        // +0x108
    bool mUseToolTips;                      // +0x109
    bool mUseExclusiveCoords;               // +0x10a
};
static_assert(sizeof(UIDbDataForms) == 268, "on-disk record: 268 bytes on every target");

// A control the form creates from the controls table when it opens (exported AddDatabaseControl parameter type;
// 8-byte vector element at UIForm +0x48)
struct UICDatabase {
    unsigned long mTableIndex;              // +0x00 UIControlsTable::GetUIControlsTable index
    UIControl* mControl;                    // +0x04 created by Open, deleted by Close
};

// A form event the form creates when it opens (exported AddEvent parameter type; 8-byte vector element at +0x08)
struct UIEventData {
    unsigned long mEventID;                 // +0x00 UIManager::CreateFormEvent argument
    unsigned long mEventType;               // +0x04 SetFormEvent selector (0..12)
};

enum class EeUiOp : uint32_t;              // rust/ee_ui.h: the Rust form's calls out
enum class EeUiFormCall : uint32_t;        // rust/ee_ui.h: its methods
struct EeUiForm;                           // rust/ee_ui.h: the Rust form (formobj.rs)

// The class is whole in Rust (rust/crates/ee_ui/src/formobj.rs, R6 ui round 16): its state (the controls, groups,
// backgrounds, hot keys, database records, events, the dirty-rectangle lists, the captures, the popup, the help text,
// the layout, the counts) and every method are the Rust object mRust; UIForm.cpp holds the forwarders, the ctors and
// the dtor, the database's Create and the host (the op switch, HostOp: a member, so that it has the friendship
// UIControl, UIHotKey, UIEntityGroup and UITextLabel give UIForm). The game's forms derive from it and override its
// virtuals; the Rust form calls the two it calls itself (SetHelpTextPopupLocation, DrawBackground) through the C++
// object. GetRectangle and GetMouseLocation answer the address of the Rust object's members (GERectangle's and
// GE2DPoint's layouts); GetMusicFileName answers a copy.
class LLE_API UIForm {
    friend class UIControl;                 // CaptureMouse/CaptureKeyboard call SetMouseCaptured/SetKeyboardCaptured
    friend class UIManager;                 // UIManager::HostOp: the protected Close/CopyData/DrawPolygons/Open, AddDirtyRectangle
public:
    UIForm(unsigned long inControlCount, unsigned long inControlGroupCount, UIManager* inManager,
           unsigned long inFormID, bool inUseGDIForText);
    UIForm(UIManager* inManager, const UIDbDataForms& inData, bool inUseGDIForText);
    UIForm(const UIForm&) = delete;         // holds a Rust object; nothing copied a form
    UIForm& operator=(const UIForm&) = delete;
    virtual ~UIForm();
    // Starts the form's music (or stops the music for "none"), restores the tab stop and fires the activate event
    virtual void Activate() { this->Call(EeUiFormCall::Activate); }                // slot 0x04
    virtual void AddIMEResultString(const UWideString& inString);
    virtual long ChangeIMECompositionString(const UWideString& inString, const UString& inAttributes,
                                            unsigned long inCursor);
    virtual long CloseIMECompositionString();
    // Fires the deactivate event, hides the popup and help text, and releases the captured mouse and keyboard
    virtual void Deactivate() { this->Call(EeUiFormCall::Deactivate); }
protected:
    virtual long DrawBackground(const GERectangle& inRectangle);   // slot 0x18
public:
    virtual long OpenIMECompositionString();
    virtual void RelayoutForm(const GERectangle& inRectangle);
    // Places the tool tip by the mouse inside the form: shifted left at the right edge, above the cursor near the bottom
    virtual void SetHelpTextPopupLocation() { this->Call(EeUiFormCall::SetHelpTextPopupLocation); }
    // Fires hot keys (higher groups first; one non-always-fire key per frame), then Tab/Shift+Tab, else passes the
    // keyboard to the capturing control or the tab stop
    virtual long UpdateKeyboard() { return (long)this->Call(EeUiFormCall::UpdateKeyboard); }
    // Passes the mouse to the captured or mouse-over control (wheel included), then fires the form's own mouse events
    virtual long UpdateMouse() { return (long)this->Call(EeUiFormCall::UpdateMouse); }
    virtual long UpdateTranslatedKey(const char* inKey, short inRepeat);
protected:
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
    virtual void SetRectangle(const GERectangle& inRectangle);  // slot 0x4c
public:
    // Inserts a background before the first one with an equal or higher draw priority
    void AddBackground(UIBackground* inBackground) { this->Call(EeUiFormCall::AddBackground, (intptr_t)inBackground); }
    void AddClipRectangle(const GERectangle& inRectangle);
    void AddControl(UIControl* inControl, unsigned long inGroup);
    void AddDatabaseControl(const UICDatabase& inControl);
    // port: makes room for a control at inIndex (the controls the table did not have: UIControlsTable::AddControlLike);
    // grows the control arrays (made with the form, mControlCount long)
    void ReserveControlIndex(unsigned long inIndex);
    void AddEvent(const UIEventData& inEvent);
    void AddHotKey(const UIHotKey& inHotKey, unsigned long inGroup);
    void AddHotKey(unsigned long inKey, unsigned long inModifier1, unsigned long inModifier2,
                   unsigned long inModifier3, UIEvent* inJustPressedEvent, UIEvent* inPressedEvent,
                   UIEvent* inJustUnpressedEvent, UIEvent* inDoublePressedEvent, unsigned long inGroup,
                   bool inAlwaysFire);
    void ClearAllClipRectangles() { this->Call(EeUiFormCall::ClearAllClipRectangles); }
    static long Create(FSFileSpec& inFile, UIManager* inManager);
    long DrawBackground(GEBitmap* inBitmap, const GERectangle& inRectangle, const GE2DPoint& inTextureSize);
    long DrawBackground(GEBitmap* inBitmap, const GERectangle& inRectangle, const GERectangle& inDrawRectangle,
                        const GERectangle& inTileRectangle);
    long DrawClippedText(const GERectangle& inRectangle, GEText* inText, UIControl* inControl);
    UIBackground* GetBackgroundByID(unsigned long inID) const;
    UIControl* GetControl(unsigned long inIndex) const;
    UIControl* GetControlById(unsigned long inId);
    unsigned long GetControlCount() const { return (unsigned long)this->Call(EeUiFormCall::ControlCount); }
    unsigned long GetControlGroupCount() const { return (unsigned long)this->Call(EeUiFormCall::ControlGroupCount); }
    unsigned long GetDesignedHeight() const { return (unsigned long)this->Call(EeUiFormCall::DesignedHeight); }
    unsigned long GetDesignedWidth() const { return (unsigned long)this->Call(EeUiFormCall::DesignedWidth); }
    UIEntityGroup* GetEntityGroup(unsigned long inIndex) const;
    UIEntityGroup* GetEntityGroupById(unsigned long inId) const;
    float GetFadeOpacity();
    unsigned long GetFormID() const { return (unsigned long)this->Call(EeUiFormCall::FormId); }
    UIFormManager* GetFormManager() const;
    UIEvent* GetFormUpdateEvent() const { return reinterpret_cast<UIEvent*>(this->Call(EeUiFormCall::Event, 13)); }
    UITextLabel* GetHelpTextLabel() const;
    bool GetIsKeyboardCaptured() const { return this->GetKeyboardCaptured() != NULL; }
    bool GetIsMouseCaptured() const { return this->GetMouseCaptured() != NULL; }
    bool GetUseExclusiveCoords() const { return this->Call(EeUiFormCall::UseExclusiveCoords) != 0; }
    UIManager* GetManager() const { return reinterpret_cast<UIManager*>(this->Call(EeUiFormCall::Manager)); }
    const GE2DPoint& GetMouseLocation() const;
    UIControl* GetMouseOverControl() const;
    UWideString GetMusicFileName() const;
    // Num Lock is off (GetKeyState(VK_NUMLOCK) & 1 was the original's test; 0x100596a2)
    static bool GetNumLockOff() { return PlatKeyIsToggled(VK_NUMLOCK) ? false : true; }
    const GERectangle& GetRectangle() const;
    bool GetTabbingOn() const { return this->Call(EeUiFormCall::TabbingOn) != 0; }
    unsigned long GetTabStop() const { return (unsigned long)this->Call(EeUiFormCall::TabStop); }
    bool GetUseGDIForText() const { return this->Call(EeUiFormCall::UseGdiForText) != 0; }
    void LockDrawBackground() { this->Call(EeUiFormCall::LockDraw, 1); }
    void RedrawIntersectControls(const GERectangle& inRectangle);
    void RemoveAllHotKeysForAGroup(unsigned long inGroup);
    void RemoveBackground(UIBackground* inBackground);
    void RemoveControl(UIControl* inControl, unsigned long inGroup);
    void RemoveHotKey(unsigned long inKey, unsigned long inModifier1, unsigned long inModifier2,
                      unsigned long inModifier3, unsigned long inGroup);
    void RemoveHotKey(unsigned long inIndex, unsigned long inGroup);
    // Marks every control of the form dirty
    void SetAllControlsDirty() { this->Call(EeUiFormCall::SetAllControlsDirty); }
    void SetFormActivateEvent(UIEvent* inEvent) { this->Call(EeUiFormCall::SetEvent, 14, (intptr_t)inEvent); }
    void SetFormCloseEvent(UIEvent* inEvent) { this->Call(EeUiFormCall::SetEvent, 1, (intptr_t)inEvent); }
    void SetFormDeactivateEvent(UIEvent* inEvent) { this->Call(EeUiFormCall::SetEvent, 15, (intptr_t)inEvent); }
    void SetFormManager(UIFormManager* inFormManager);
    // one (the event's index in the Rust form: formobj.rs EV_*)
    void SetFormOpenEvent(UIEvent* inEvent) { this->Call(EeUiFormCall::SetEvent, 0, (intptr_t)inEvent); }
    void SetFormUpdateEvent(UIEvent* inEvent) { this->Call(EeUiFormCall::SetEvent, 13, (intptr_t)inEvent); }
    void SetHelpTextLabel(UITextLabel* inLabel);
    void SetHelpTextTimeDelay(unsigned long inTimeDelay);
    void SetHotKeyGroupEnabled(unsigned long inGroup, bool inEnabled);
    void SetLayout(UILayoutUtility& inLayout);
    void SetMouseMiddleButtonPressedEvent(UIEvent* inEvent);
    void SetMouseMiddleButtonUnpressedEvent(UIEvent* inEvent);
    void SetMouseWheelMoveEvent(UIEvent* inEvent) { this->Call(EeUiFormCall::SetEvent, 12, (intptr_t)inEvent); }
    void SetMusicFileName(const UWideString& inName);
    // Replaces the popup control, hiding the old one and redrawing what it (and a tool-tip shadow) covered
    void SetPopup(UIControl* inControl) { this->Call(EeUiFormCall::SetPopup, (intptr_t)inControl); }
    // Turns keyboard tabbing on or off; turning it on moves the tab stop to the first tabbable control
    void SetTabbing(bool inTabbing) { this->Call(EeUiFormCall::SetTabbing, inTabbing); }
    // Moves the tab stop to a control if it exists and is tabbable; otherwise keeps the old stop and returns false
    bool SetTabStop(unsigned long inTabStop) { return this->Call(EeUiFormCall::SetTabStop, (intptr_t)inTabStop) != 0; }
    void SetUseToolTips(bool inUseToolTips) { this->Call(EeUiFormCall::SetUseToolTips, inUseToolTips); }
    // Moves the tab stop on to the next shown, enabled, tabbable control, skipping radio-button runs, wrapping round
    void Tab() { this->Call(EeUiFormCall::Tab); }
    void UnlockDrawBackground() { this->Call(EeUiFormCall::LockDraw, 0); }
    // Refreshes the help text from the control under the mouse, re-placing a tool tip that sizes itself to its text
    void UpdateHelpString() { this->Call(EeUiFormCall::UpdateHelpString); }
    void UpdateMouseLocation(const GE2DPoint& inLocation);
protected:
    bool GetDrawToolTipShadow() const { return this->Call(EeUiFormCall::DrawToolTipShadow) != 0; }
    void SetDrawToolTipShadow(bool inDraw) { this->Call(EeUiFormCall::SetDrawToolTipShadow, inDraw); }
    UIControl* GetKeyboardCaptured() const;
    UIControl* GetMouseCaptured() const;
private:
    void AddDirtyRectangle(const GERectangle& inRectangle);
    void SetKeyboardCaptured(UIControl* inControl);
    void SetMouseCaptured(UIControl* inControl) { this->Call(EeUiFormCall::SetMouseCaptured, (intptr_t)inControl); }
    // the forwarders' one call into the Rust form, and the Rust form's calls out
    intptr_t Call(EeUiFormCall inCall, intptr_t inA = 0, intptr_t inB = 0, float* ioF = NULL) const;
    static intptr_t HostOp(void* inForm, EeUiOp inOp, intptr_t inA, intptr_t inB, intptr_t inC, intptr_t inD,
                           float* ioF);

    EeUiForm* mRust;                        // the state (formobj.rs)
#ifdef LOWLEVELENGINE_EXPORTS
    // Inferred guard (see LowLevelEngine.h: a static data member costs every includer a $E number)
#if defined(LLE_UIFORM_CPP)
    // Inferred: visible to UIForm.cpp only (shown to UIControl.cpp it renumbers that unit's $E chains)
    static ISEntity* sTabKey;
#endif
    static ISEntity* sShiftKey;             // 0x1012e70c (read by UIControl::ShiftKeyPressed)
#endif
};

// UIForm::SetFormEvent selector (UIEventData::mEventType, db form-event type)
enum UIFormEventType {
    kUIEventFormOpen = 0,                      // UIForm::SetFormEvent case 0 calls SetFormOpenEvent (exported)
    kUIEventFormClose = 1,                     // UIForm::SetFormEvent case 1 calls SetFormCloseEvent (exported)
    kUIEventMouseLeftButtonPressed = 2,        // UIForm::SetFormEvent case 2 calls SetMouseLeftButtonPressedEvent (exported)
    kUIEventMouseLeftButtonUnpressed = 3,      // UIForm::SetFormEvent case 3 calls SetMouseLeftButtonUnpressedEvent (exported)
    kUIEventMouseLeftButtonDoublePressed = 4,  // UIForm::SetFormEvent case 4 calls SetMouseLeftButtonDoublePressedEvent (exported)
    kUIEventMouseMiddleButtonPressed = 5,      // UIForm::SetFormEvent case 5 calls SetMouseMiddleButtonPressedEvent (exported)
    kUIEventMouseMiddleButtonUnpressed = 6,    // UIForm::SetFormEvent case 6 calls SetMouseMiddleButtonUnpressedEvent (exported)
    kUIEventMouseMiddleButtonDoublePressed = 7,  // UIForm::SetFormEvent case 7 calls SetMouseMiddleButtonDoublePressedEvent (exported)
    kUIEventMouseRightButtonPressed = 8,       // UIForm::SetFormEvent case 8 calls SetMouseRightButtonPressedEvent (exported)
    kUIEventMouseRightButtonUnpressed = 9,     // UIForm::SetFormEvent case 9 calls SetMouseRightButtonUnpressedEvent (exported)
    kUIEventMouseRightButtonDoublePressed = 10,  // UIForm::SetFormEvent case 10 calls SetMouseRightButtonDoublePressedEvent (exported)
    kUIEventMouseMove = 11,                    // UIForm::SetFormEvent case 11 calls SetMouseMoveEvent (exported)
    kUIEventMouseWheelMove = 12,               // UIForm::SetFormEvent case 12 calls SetMouseWheelMoveEvent (exported)
};

#endif
