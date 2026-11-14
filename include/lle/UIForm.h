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
#include "rust/ee_ui.h"

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
    friend class UIManager;                 // calls the protected Close/CopyData/DrawPolygons/Open and AddDirtyRectangle
    friend struct UIManagerRust;            // UIManager.cpp: the Rust form stack's calls on a form (ee_ui manager.rs)
public:
    UIForm(unsigned long inControlCount, unsigned long inControlGroupCount, UIManager* inManager,
           unsigned long inFormID, bool inUseGDIForText);
    UIForm(UIManager* inManager, const UIDbDataForms& inData, bool inUseGDIForText);
    UIForm(const UIForm&) = delete;         // holds a Rust object; nothing copied a form
    UIForm& operator=(const UIForm&) = delete;
    virtual ~UIForm();
    virtual void Activate();                // slot 0x04
    virtual void AddIMEResultString(const UWideString& inString);
    virtual long ChangeIMECompositionString(const UWideString& inString, const UString& inAttributes,
                                            unsigned long inCursor);
    virtual long CloseIMECompositionString();
    virtual void Deactivate();
protected:
    virtual long DrawBackground(const GERectangle& inRectangle);   // slot 0x18
public:
    virtual long OpenIMECompositionString();
    virtual void RelayoutForm(const GERectangle& inRectangle);
    virtual void SetHelpTextPopupLocation();
    virtual void UpdateInformation() {}
    virtual long UpdateKeyboard();
    virtual long UpdateMouse();
    virtual long UpdateTranslatedKey(const char* inKey, short inRepeat);
protected:
    virtual void Close();                   // slot 0x38
    virtual long CopyData();
    virtual long DrawPolygons();
    virtual long DrawTextA();
    virtual void Open();
    virtual void SetRectangle(const GERectangle& inRectangle);  // slot 0x4c
public:
    void AddBackground(UIBackground* inBackground);
    void AddClipRectangle(const GERectangle& inRectangle);
    void AddControl(UIControl* inControl, unsigned long inGroup);
    void AddDatabaseControl(const UICDatabase& inControl);
    // port: makes room for a control at inIndex (the controls the table did not have: UIControlsTable::AddControlLike);
    // grows the control arrays (made with the form, mControlCount long)
    void ReserveControlIndex(unsigned long inIndex);
    void AddDrawControl(unsigned long inIndex);
    void AddEvent(const UIEventData& inEvent);
    void AddHotKey(const UIHotKey& inHotKey, unsigned long inGroup);
    void AddHotKey(unsigned long inKey, unsigned long inModifier1, unsigned long inModifier2,
                   unsigned long inModifier3, UIEvent* inJustPressedEvent, UIEvent* inPressedEvent,
                   UIEvent* inJustUnpressedEvent, UIEvent* inDoublePressedEvent, unsigned long inGroup,
                   bool inAlwaysFire);
    void ClearAllClipRectangles();
    static long Create(FSFileSpec& inFile, UIManager* inManager);
    long DrawBackground(GEBitmap* inBitmap, const GERectangle& inRectangle, const GE2DPoint& inTextureSize);
    long DrawBackground(GEBitmap* inBitmap, const GERectangle& inRectangle, const GERectangle& inDrawRectangle,
                        const GERectangle& inTileRectangle);
    long DrawClippedText(const GERectangle& inRectangle, GEText* inText, UIControl* inControl);
    bool GetAvailableIndex(unsigned long& outIndex) const;
    UIBackground* GetBackgroundByID(unsigned long inID) const;
    UIControl* GetControl(unsigned long inIndex) const;
    UIControl* GetControlById(unsigned long inId);
    unsigned long GetControlCount() const;
    unsigned long GetControlGroupCount() const;
    unsigned long GetDesignedHeight() const;
    unsigned long GetDesignedWidth() const;
    UIEntityGroup* GetEntityGroup(unsigned long inIndex) const;
    UIEntityGroup* GetEntityGroupById(unsigned long inId) const;
    float GetFadeOpacity();
    UIEvent* GetFormActivateEvent() const;
    UIEvent* GetFormDeactivateEvent() const;
    unsigned long GetFormID() const;
    UIFormManager* GetFormManager() const;
    UIEvent* GetFormUpdateEvent() const;
    UITextLabel* GetHelpTextLabel() const;
    unsigned long GetHowLongHasFormBeenOpen();
    bool GetIsKeyboardCaptured() const { return this->GetKeyboardCaptured() != NULL; }
    bool GetIsMouseCaptured() const { return this->GetMouseCaptured() != NULL; }
    bool GetUseExclusiveCoords() const;
    UIManager* GetManager() const;
    UIEvent* GetMouseLeftButtonPressedEvent() const;
    UIEvent* GetMouseLeftButtonUnpressedEvent() const;
    UIEvent* GetMouseLeftButtonDoublePressedEvent() const;
    const GE2DPoint& GetMouseLocation() const;
    UIEvent* GetMouseMiddleButtonPressedEvent() const;
    UIEvent* GetMouseMiddleButtonUnpressedEvent() const;
    UIEvent* GetMouseMiddleButtonDoublePressedEvent() const;
    UIEvent* GetMouseMoveEvent() const;
    UIControl* GetMouseOverControl() const;
    UIEvent* GetMouseRightButtonPressedEvent() const;
    UIEvent* GetMouseRightButtonUnpressedEvent() const;
    UIEvent* GetMouseRightButtonDoublePressedEvent() const;
    UIEvent* GetMouseWheelMoveEvent() const;
    UWideString GetMusicFileName() const;
    // Num Lock is off (GetKeyState(VK_NUMLOCK) & 1 was the original's test; 0x100596a2)
    static bool GetNumLockOff() { return PlatKeyIsToggled(VK_NUMLOCK) ? false : true; }
    const GERectangle& GetRectangle() const;
    bool GetShouldFadeIn();
    bool GetTabbingOn() const;
    unsigned long GetTabStop() const;
    void GetToolTipShadowRectangle(const GERectangle& inRectangle, GERectangle& outRectangle);
    bool GetUseGDIForText() const;
    void LoadBackgrounds();
    void LockDrawBackground();
    void RedrawIntersectControls(const GERectangle& inRectangle);
    void RemoveAllHotKeys();
    void RemoveAllHotKeysForAGroup(unsigned long inGroup);
    void RemoveBackground(UIBackground* inBackground);
    void RemoveControl(UIControl* inControl, unsigned long inGroup);
    void RemoveHotKey(unsigned long inKey, unsigned long inModifier1, unsigned long inModifier2,
                      unsigned long inModifier3, unsigned long inGroup);
    void RemoveHotKey(unsigned long inIndex, unsigned long inGroup);
    void SetAllControlsDirty();
    void SetFormActivateEvent(UIEvent* inEvent);
    void SetFormCloseEvent(UIEvent* inEvent);
    void SetFormDeactivateEvent(UIEvent* inEvent);
    void SetFormKeyboardCapturedEvent(UIEvent* inEvent);
    void SetFormManager(UIFormManager* inFormManager);
    void SetFormOpenEvent(UIEvent* inEvent);
    void SetFormUpdateEvent(UIEvent* inEvent);
    void SetHelpTextLabel(UITextLabel* inLabel);
    void SetHelpTextTimeDelay(unsigned long inTimeDelay);
    void SetHotKeyGroupEnabled(unsigned long inGroup, bool inEnabled);
    void SetLayout(UILayoutUtility& inLayout);
    void SetMouseLeftButtonPressedEvent(UIEvent* inEvent);
    void SetMouseLeftButtonUnpressedEvent(UIEvent* inEvent);
    void SetMouseLeftButtonDoublePressedEvent(UIEvent* inEvent);
    void SetMouseMiddleButtonPressedEvent(UIEvent* inEvent);
    void SetMouseMiddleButtonUnpressedEvent(UIEvent* inEvent);
    void SetMouseMiddleButtonDoublePressedEvent(UIEvent* inEvent);
    void SetMouseMoveEvent(UIEvent* inEvent);
    void SetMouseRightButtonPressedEvent(UIEvent* inEvent);
    void SetMouseRightButtonUnpressedEvent(UIEvent* inEvent);
    void SetMouseRightButtonDoublePressedEvent(UIEvent* inEvent);
    void SetMouseWheelMoveEvent(UIEvent* inEvent);
    void SetMusicFileName(const UWideString& inName);
    void SetPopup(UIControl* inControl);
    void SetTabbing(bool inTabbing);
    bool SetTabStop(unsigned long inTabStop);
    void SetUseToolTips(bool inUseToolTips);
    void ShiftTab();
    void Tab();
    void UnloadBackgrounds();
    void UnlockDrawBackground();
    void UpdateHelpString();
    void UpdateMouseLocation(const GE2DPoint& inLocation);
protected:
    bool GetDrawToolTipShadow() const;
    void SetDrawToolTipShadow(bool inDraw);
    UIControl* GetKeyboardCaptured() const;
    UIControl* GetMouseCaptured() const;
private:
    void AddCopyDirtyRectangle(const GERectangle& inRectangle);
    void AddDirtyRectangle(const GERectangle& inRectangle);
    void CheckedRadioButton(long& ioIndex);
    void SetFormEvent(unsigned long inType, UIEvent* inEvent);
    void SetKeyboardCaptured(UIControl* inControl);
    void SetMouseCaptured(UIControl* inControl);
    void SetMouseOverControl(UIControl* inControl);
    void UpdateHelpText(bool inForce);
    // the forwarders' one call into the Rust form, and the Rust form's calls out
    intptr_t Call(EeUiFormCall inCall, intptr_t inA = 0, intptr_t inB = 0, float* ioF = NULL) const;
    static intptr_t HostOp(void* inForm, EeUiOp inOp, intptr_t inA, intptr_t inB, intptr_t inC, intptr_t inD,
                           float* ioF);

    EeUiForm* mRust;                        // the state (formobj.rs)
#ifdef LOWLEVELENGINE_EXPORTS
    // Inferred guard (see LowLevelEngine.h: a static data member costs every includer a $E number)
#if defined(LLE_UIFORM_CPP)
    // Inferred: visible to UIForm.cpp only (shown to UIControl.cpp it renumbers that unit's $E chains)
    static ISEntity* sTabKey;               // 0x1012e708
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
