// C:\EEX\Low-Level Engine\User Interface\UIForm.h (RECONSTRUCTION). A window of controls: entity groups, controls,
// backgrounds, hot keys, form events and the dirty-rectangle bookkeeping. UIForm.cpp (0x1007042f..0x10075330,
// __FILE__ asserts) holds the ctors and the out-of-line members; the inline members are emitted first by
// UIBackground.cpp (0x10059559..0x10059795, declaration order). Vtable 0x10098b08 (20 slots, exported names).
// sizeof 0x1a0 (??_E stride 0x10059e1c; UIManager::CreateForm news 0x1a0). Layout proven by the ctors, the dtor, Init
// and the event setters. Requires lle/UIControl.h, lle/UILayoutUtility.h, lle/TSCriticalSection.h, <vector>, <deque>.
#ifndef LLE_UIFORM_H
#define LLE_UIFORM_H

#include <stdint.h>   // int32_t/uint32_t: the on-disk fields' fixed widths

#include <vector>
#include <deque>

#include "lle/UILayoutUtility.h"
#include "platform/PlatWindow.h"   // PlatKeyIsToggled

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

// One dirty rectangle per surface still to be redrawn (0x14-byte deque element at +0x58; provisional name)
struct UIDirtyRectangle {
    UIDirtyRectangle() : mSurfaceCount(0) {}
    GERectangle mRectangle;                 // +0x00
    unsigned long mSurfaceCount;            // +0x10 UIManager::GetNumberOfSurfaces when added
};

class LLE_API UIForm {
    friend struct UIFormRust;               // UIForm.cpp: the Rust focus walk's and hit test's view (ee_ui form.rs)
    friend class UIControl;                 // CaptureMouse writes mMouseCaptured; SetKeyboardCaptured is private
    friend class UIManager;                 // calls the protected Close/CopyData/DrawPolygons/Open and AddDirtyRectangle
public:
    UIForm(unsigned long inControlCount, unsigned long inControlGroupCount, UIManager* inManager,
           unsigned long inFormID, bool inUseGDIForText);
    UIForm(UIManager* inManager, const UIDbDataForms& inData, bool inUseGDIForText);
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
    // inline: exported 0x10059766 (store order top, left, bottom, right: GERectangle::operator=)
    virtual void SetRectangle(const GERectangle& inRectangle) { this->mRectangle = inRectangle; }  // slot 0x4c
public:
    void AddBackground(UIBackground* inBackground);
    void AddClipRectangle(const GERectangle& inRectangle);
    void AddControl(UIControl* inControl, unsigned long inGroup);
    void AddDatabaseControl(const UICDatabase& inControl) { this->mDatabaseControls.push_back(inControl); }
    // port: makes room for a control at inIndex (the controls the table did not have: UIControlsTable::AddControlLike);
    // grows the control arrays (made with the form, mControlCount long)
    void ReserveControlIndex(unsigned long inIndex);
    void AddDrawControl(unsigned long inIndex);
    void AddEvent(const UIEventData& inEvent) { this->mEventData.push_back(inEvent); }
    void AddHotKey(const UIHotKey& inHotKey, unsigned long inGroup);
    void AddHotKey(unsigned long inKey, unsigned long inModifier1, unsigned long inModifier2,
                   unsigned long inModifier3, UIEvent* inJustPressedEvent, UIEvent* inPressedEvent,
                   UIEvent* inJustUnpressedEvent, UIEvent* inDoublePressedEvent, unsigned long inGroup,
                   bool inAlwaysFire);
    void ClearAllClipRectangles() { this->mClipRectangles.clear(); }
    static long Create(FSFileSpec& inFile, UIManager* inManager);
    long DrawBackground(GEBitmap* inBitmap, const GERectangle& inRectangle, const GE2DPoint& inTextureSize);
    long DrawBackground(GEBitmap* inBitmap, const GERectangle& inRectangle, const GERectangle& inDrawRectangle,
                        const GERectangle& inTileRectangle);
    long DrawClippedText(const GERectangle& inRectangle, GEText* inText, UIControl* inControl);
    bool GetAvailableIndex(unsigned long& outIndex) const;
    UIBackground* GetBackgroundByID(unsigned long inID) const;
    const std::vector<GERectangle>& GetClipRectangles() { return this->mClipRectangles; }
    UIControl* GetControl(unsigned long inIndex) const { return this->mControls[inIndex]; }
    UIControl* GetControlById(unsigned long inId);
    unsigned long GetControlCount() const { return this->mControlCount; }
    unsigned long GetControlGroupCount() const { return this->mControlGroupCount; }
    unsigned long GetDesignedHeight() const { return this->mDesignedHeight; }  // ICF-folded (0x1004040d)
    unsigned long GetDesignedWidth() const { return this->mDesignedWidth; }
    UIEntityGroup* GetEntityGroup(unsigned long inIndex) const { return this->mEntityGroups[inIndex]; }
    UIEntityGroup* GetEntityGroupById(unsigned long inId) const;
    float GetFadeOpacity() { return this->mFadeOpacity; }
    UIEvent* GetFormActivateEvent() const { return this->mFormActivateEvent; }
    UIEvent* GetFormDeactivateEvent() const { return this->mFormDeactivateEvent; }
    unsigned long GetFormID() const { return this->mFormID; }
    UIFormManager* GetFormManager() const { return this->mFormManager; }
    UIEvent* GetFormUpdateEvent() const { return this->mFormUpdateEvent; }
    UITextLabel* GetHelpTextLabel() const { return this->mHelpTextLabel; }  // ICF-folded (0x100633d4)
    UIHotKey* GetHotKey(unsigned long inIndex, unsigned long inGroup) const { return this->mHotKeys[inGroup][inIndex]; }
    unsigned long GetHotKeyCount(unsigned long inGroup) const { return this->mHotKeys[inGroup].size(); }
    unsigned long GetHowLongHasFormBeenOpen() { return TMGetTime() - this->mOpenTime; }
    bool GetIsKeyboardCaptured() const { return this->mKeyboardCaptured != NULL; }
    bool GetIsMouseCaptured() const { return this->mMouseCaptured != NULL; }
    bool GetUseExclusiveCoords() const { return this->mUseExclusiveCoords; }
    UIManager* GetManager() const { return this->mManager; }  // ICF-folded (0x100633b1)
    UIEvent* GetMouseLeftButtonPressedEvent() const { return this->mMouseLeftButtonPressedEvent; }
    UIEvent* GetMouseLeftButtonUnpressedEvent() const { return this->mMouseLeftButtonUnpressedEvent; }
    UIEvent* GetMouseLeftButtonDoublePressedEvent() const { return this->mMouseLeftButtonDoublePressedEvent; }
    const GE2DPoint& GetMouseLocation() const { return this->mMouseLocation; }
    UIEvent* GetMouseMiddleButtonPressedEvent() const { return this->mMouseMiddleButtonPressedEvent; }
    UIEvent* GetMouseMiddleButtonUnpressedEvent() const { return this->mMouseMiddleButtonUnpressedEvent; }
    UIEvent* GetMouseMiddleButtonDoublePressedEvent() const { return this->mMouseMiddleButtonDoublePressedEvent; }
    UIEvent* GetMouseMoveEvent() const { return this->mMouseMoveEvent; }  // ICF-folded (0x1001132b)
    UIControl* GetMouseOverControl() const { return this->mMouseOverControl; }  // ICF-folded (0x1005a0e8)
    UIEvent* GetMouseRightButtonPressedEvent() const { return this->mMouseRightButtonPressedEvent; }
    UIEvent* GetMouseRightButtonUnpressedEvent() const { return this->mMouseRightButtonUnpressedEvent; }
    UIEvent* GetMouseRightButtonDoublePressedEvent() const { return this->mMouseRightButtonDoublePressedEvent; }
    UIEvent* GetMouseWheelMoveEvent() const { return this->mMouseWheelMoveEvent; }
    const UWideString& GetMusicFileName() const { return this->mMusicFileName; }
    // Num Lock is off (GetKeyState(VK_NUMLOCK) & 1 was the original's test; 0x100596a2)
    static bool GetNumLockOff() { return PlatKeyIsToggled(VK_NUMLOCK) ? false : true; }
    const GERectangle& GetRectangle() const { return this->mRectangle; }
    bool GetShouldFadeIn();
    bool GetTabbingOn() const { return this->mTabbingOn; }
    unsigned long GetTabStop() const { return this->mTabStop; }
    void GetToolTipShadowRectangle(const GERectangle& inRectangle, GERectangle& outRectangle);
    bool GetUseGDIForText() const { return this->mUseGDIForText; }
    void LoadBackgrounds();
    void LockDrawBackground() { this->mDrawBackgroundLock->Enter(); }
    void RedrawIntersectControls(const GERectangle& inRectangle);
    void RemoveAllHotKeys();
    void RemoveAllHotKeysForAGroup(unsigned long inGroup);
    void RemoveBackground(UIBackground* inBackground);
    void RemoveControl(UIControl* inControl, unsigned long inGroup);   // inline (0x100596d9), defined below
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
    void SetHelpTextLabel(UITextLabel* inLabel) { this->mHelpTextLabel = inLabel; }
    // inline (exported 0x1005970f expands TMPoll::SetTimeDelay). Inferred: defined only where lle/TMPoll.h was
    // included first; including TMPoll.h here shows its static sCounterFrequency to every UI unit, which renumbers
    // their $E chains (UIControl.cpp's binds were recorded without it)
#ifdef LLE_TMPOLL_H
    void SetHelpTextTimeDelay(unsigned long inTimeDelay) { this->mHelpTextPoll->SetTimeDelay(inTimeDelay); }
#else
    void SetHelpTextTimeDelay(unsigned long inTimeDelay);
#endif
    void SetHotKeyGroupEnabled(unsigned long inGroup, bool inEnabled);
    void SetLayout(UILayoutUtility& inLayout) { this->mLayout = inLayout; }
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
    void SetMusicFileName(const UWideString& inName) { this->mMusicFileName = inName; }
    void SetPopup(UIControl* inControl);
    void SetTabbing(bool inTabbing);
    bool SetTabStop(unsigned long inTabStop);
    void SetUseToolTips(bool inUseToolTips) { this->mUseToolTips = inUseToolTips; }
    void ShiftTab();
    void Tab();
    void UnloadBackgrounds();
    void UnlockDrawBackground() { this->mDrawBackgroundLock->Leave(); }
    void UpdateHelpString();
    void UpdateMouseLocation(const GE2DPoint& inLocation);
protected:
    UIControl* GetKeyboardCaptured() const { return this->mKeyboardCaptured; }  // ICF-folded (0x100660be)
    UIControl* GetMouseCaptured() const { return this->mMouseCaptured; }   // ICF-folded (0x100633db)
private:
    void AddCopyDirtyRectangle(const GERectangle& inRectangle);
    void AddDirtyRectangle(const GERectangle& inRectangle);
    void CheckedRadioButton(long& ioIndex);
    void Init();
    void SetFormEvent(unsigned long inType, UIEvent* inEvent);
    void SetKeyboardCaptured(UIControl* inControl);
    void SetMouseCaptured(UIControl* inControl) { this->mMouseCaptured = inControl; }
    void SetMouseOverControl(UIControl* inControl);
    void UpdateHelpText(bool inForce);

protected:
    bool mDrawToolTipShadow;                // +0x004 (mUseToolTips at construction; the exe's EEUIFMainGame ctor 0x262e73 stores false)
private:
    std::vector<UIEventData> mEventData;    // +0x008
    std::vector<std::vector<UIHotKey*> > mHotKeys;   // +0x018 per group
    std::vector<UIHotKey*> mHotKeyList;     // +0x028 every hot key, for the update order
    std::vector<UIBackground*> mBackgrounds;   // +0x038 by draw priority
    std::vector<UICDatabase> mDatabaseControls;  // +0x048
    std::deque<UIDirtyRectangle> mDirtyRectangles;   // +0x058
    std::vector<GERectangle> mCopyDirtyRectangles;   // +0x06c guarded by mDrawBackgroundLock
    std::vector<GERectangle> mClipRectangles;   // +0x07c
    UIFormManager* mFormManager;            // +0x08c (owned)
    UILayoutUtility mLayout;                // +0x090
    GERectangle mCopiedPopupRectangle;      // +0x0b0
    GERectangle mRectangle;                 // +0x0c0
    UWideString mMusicFileName;             // +0x0d0
    GE2DPoint mMouseLocation;               // +0x0e4
    GE2DPoint mUnknown0EC;                  // +0x0ec (provisional name; zero at construction, unused)
    UIEntityGroup** mEntityGroups;          // +0x0f4 [mControlGroupCount]
    UIControl** mControls;                  // +0x0f8 [mControlCount]
    UIControl** mDrawControls;              // +0x0fc [mControlCount] controls to draw this frame
    GEText* mText;                          // +0x100 provisional name (deleted, never created here)
    UIManager* mManager;                    // +0x104
    UIControl* mKeyboardCaptured;           // +0x108
    UIControl* mMouseCaptured;              // +0x10c
    UIControl* mPopup;                      // +0x110
    UIControl* mCopiedPopup;                // +0x114
    UIControl* mMouseOverControl;           // +0x118
    UITextLabel* mHelpTextLabel;            // +0x11c
    UIEvent* mFormCloseEvent;               // +0x120
    UIEvent* mFormOpenEvent;                // +0x124
    UIEvent* mMouseLeftButtonPressedEvent;  // +0x128
    UIEvent* mMouseLeftButtonUnpressedEvent;    // +0x12c
    UIEvent* mMouseLeftButtonDoublePressedEvent;    // +0x130
    UIEvent* mMouseMiddleButtonPressedEvent;    // +0x134
    UIEvent* mMouseMiddleButtonUnpressedEvent;  // +0x138
    UIEvent* mMouseMiddleButtonDoublePressedEvent;  // +0x13c
    UIEvent* mMouseMoveEvent;               // +0x140
    UIEvent* mMouseRightButtonPressedEvent; // +0x144
    UIEvent* mMouseRightButtonUnpressedEvent;   // +0x148
    UIEvent* mMouseRightButtonDoublePressedEvent;   // +0x14c
    UIEvent* mMouseWheelMoveEvent;          // +0x150
    UIEMouseWheelScroll* mMouseWheelScrollEvent;    // +0x154 the default wheel event (Init)
    UIEvent* mFormUpdateEvent;              // +0x158
    UIEvent* mFormActivateEvent;            // +0x15c
    UIEvent* mFormDeactivateEvent;          // +0x160
    UIEvent* mFormKeyboardCapturedEvent;    // +0x164
    TMPoll* mHelpTextPoll;                  // +0x168
    TSCriticalSection* mDrawBackgroundLock; // +0x16c
    long mControlCount;                     // +0x170 signed (the loops compare it signed)
    long mControlGroupCount;                // +0x174
    unsigned long mTabStop;                 // +0x178
    unsigned long mPopupDrawCount;          // +0x17c surfaces still to draw mCopiedPopup on
    unsigned long mFormID;                  // +0x180
    unsigned long mDesignedWidth;           // +0x184
    unsigned long mDesignedHeight;          // +0x188
    unsigned long mHelpTextTimeDelay;       // +0x18c
    unsigned long mOpenTime;                // +0x190 TMGetTime at Open
    float mFadeOpacity;                     // +0x194
    bool mUseToolTips;                      // +0x198
    bool mTabbingOn;                        // +0x199
    bool mUnknown19A;                       // +0x19a (provisional name; false in Init, unused)
    bool mUseGDIForText;                    // +0x19b
    bool mUseExclusiveCoords;               // +0x19c
#ifdef LOWLEVELENGINE_EXPORTS
    // Inferred guard (see LowLevelEngine.h: a static data member costs every includer a $E number)
#if defined(LLE_UIFORM_CPP)
    // Inferred: visible to UIForm.cpp only (shown to UIControl.cpp it renumbers that unit's $E chains)
    static ISEntity* sTabKey;               // 0x1012e708
#endif
    static ISEntity* sShiftKey;             // 0x1012e70c (read by UIControl::ShiftKeyPressed)
#endif
};

// inline (exported 0x100596d9; UIForm::Close expands it). Inferred: defined only where UIControl.h and
// UIEntityGroup.h were included first (including them here drags GERhombus & co. into every includer)
#if defined(LLE_UICONTROL_H) && defined(LLE_UIENTITYGROUP_H)
inline void UIForm::RemoveControl(UIControl* inControl, unsigned long inGroup)
{
    this->mControls[inControl->GetIndex()] = NULL;
    this->mEntityGroups[inGroup]->RemoveEntity(inControl);
}
#endif

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
