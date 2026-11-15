// C:\EEX\Empire Earth\User Interface Thread\EEUIFMGStatusButton.h (RECONSTRUCTION; file name inferred). The main game
// form's command buttons (controls 0x4e..0x56 and 0x6e..0xcd; RTTI EEUIFMGStatusButton > UIButton, vtable 0x44ad98;
// EEUIFMGStatusButton.cpp 0x26de97..0x271319). A button shows either a tech tree node
// (a unit to build, a technology to research: mPowerRegeneration) or an EEButtonObject command (mB148, mButton). Layout from the
// ctor 0x26de97 and the dtor 0x26e225 (sizeof 0x14c); provisional names. Requires lle/UIButton.h, lle/UIEvent.h.
#ifndef EE_EEUIFMGSTATUSBUTTON_H
#define EE_EEUIFMGSTATUSBUTTON_H

class EEButtonObject;
class GEText;
class UIControlsTable;
class UIManager;

class EEUIFMGStatusButton : public UIButton {
public:
    // 0x26de97 (ret 0xc; the user-defined control factory)
    EEUIFMGStatusButton(UIManager* inManager, const GERectangle& inRectangle, UIControlsTable& inTable);
    virtual ~EEUIFMGStatusButton();
    virtual void ButtonUp();                // +0x8c 0x26e274
    void ReleaseButton();                   // 0x26e71d (the main group hides every button, then calls it)
    virtual long CopyData();                // +0x14 0x26e734
    virtual long DrawPolygons();            // +0x18 0x26eaf4
    virtual long DrawTextA();               // +0x1c 0x26ebb5
    virtual bool GetHelpString(UWideString& outString) const;   // +0x20 0x26ebdf
    void HotKeyDown();                      // 0x26f16c (the hot-key event, pressed)
    void HotKeyUp();                        // 0x26f174 (the hot-key event, released)
    virtual long MouseRButtonDown();        // +0x44 0x26f186
    virtual long MouseRButtonUp();          // +0x48 0x26f1a4
    // for the presses' host (ee_screens status_button.rs): UIButton::ButtonUp, and the mouse in the rectangle
    void BaseButtonUp() { UIButton::ButtonUp(); }
    bool IsMouseInside() const { return this->GetRectangle().Contains(this->GetMouseLocation()); }
    void SetCommandButton(unsigned long inButton);  // 0x26fa82 (an EEButtonObject index)
    // 0x26faba (ret 0xc): a technology button (the unit class's technology, its unit type, the unit's id)
    void SetTechButton(long inTechnology, unsigned long inUnitType, unsigned long inUnitID);
    virtual void SetLocation(const GE2DPoint& inLocation);   // +0x6c 0x26fb19
    virtual void TabbedFrom();              // +0x78 0x26fb79
    virtual void TabbedTo();                // +0x74 0x26fb8d

    UWideString mU104;                      // +0x104 (constructed and destroyed only)
    unsigned long mLastErrorTime;           // +0x118 TMGetTime() of the last "cannot" chat line
    EEButtonObject* mButtonObject;          // +0x11c
    GEBitmap* mDither;                      // +0x120 textures\Dither over a disabled button
    GEBitmap* mIcon;                        // +0x124
    GEText* mBuildQueue;                    // +0x128 the queue count (ESVERIFY(this->mBuildQueue), line 195)
    long mTechnology;                       // +0x12c the technology (the main group matches the queued items' key)
    unsigned long mUnitType;                // +0x130
    float mF134;                            // +0x134 (= 40)
    float mF138;                            // +0x138 (= 40)
    unsigned long mUnitID;                  // +0x13c
    long mCount;                            // +0x140 (the main group's queue counts)
    unsigned long mButton;                  // +0x144 the EEButtonObject index
    bool mIsCommand;                        // +0x148 an EEButtonObject command (else a tech tree node)
    bool mShowCount;                        // +0x149 mAreaEffectStrength != 0 as of the last CopyData
    bool mAvailable;                        // +0x14a
    bool mDrawIcon;                         // +0x14b
};

// RTTI EEUIEndCinematicModeEvent (vtable 0x44ae68; ??_G 0x271265 after its first user EEModeKeyboardCinematic; ProcessEvent
// 0x26fb93 is defined in EEUIFMGStatusButton.cpp after TabbedTo). sizeof 0xc.
class EEUIEndCinematicModeEvent : public UIEvent {
public:
    virtual ~EEUIEndCinematicModeEvent() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

#endif
