// C:\EEX\Empire Earth\Scenario Editor\EESEEvents.h (RECONSTRUCTION; header and file names inferred). The scenario
// editor's UI events with per-instance state (class prefix EESEE); the trigger tab's other events are EEScreenEvent
// codes (ee_screens scenario_editor.rs). Their ProcessEvents are EESEEvents.cpp's. Requires lle/UIEvent.h,
// lle/UWideString.h, <vector>.
#ifndef EE_EESEEVENTS_H
#define EE_EESEEVENTS_H

class EESETSEScenarioComboBox;
class UIControl;
class UITextLabel;


// vtable 0x443b58; ??_G 0x17fe51; ProcessEvent 0x16fd6a; sizeof 0x2c; ctor 0x17fdf1
class EESEESliderChange : public UIEvent {
public:
    // inline (an out-of-line copy 0x17fdf1: body assignments; the format is taken by value)
    EESEESliderChange(UITextLabel* inLabel, UWideString inFormat, float inScale, float inOffset)
    {
        this->mLabel = inLabel;
        this->mFormat = inFormat;
        this->mScale = inScale;
        this->mOffset = inOffset;
    }
    virtual ~EESEESliderChange() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);

    UITextLabel* mLabel;                    // +0x0c (SetText target)
    UWideString mFormat;                    // +0x10 (sprintf format of the value)
    float mScale;                           // +0x24 (value = slider * mScale + mOffset)
    float mOffset;                          // +0x28
};

// vtable 0x443b44; ??_G 0x17febd; ProcessEvent 0x16fdbf; sizeof 0x1c; ctor 0x17fe85 inline; new at 0x17f810, 0x17f987
class EESEECheckDisplay : public UIEvent {
public:
    EESEECheckDisplay() {}                  // inline (an out-of-line copy 0x17fe85: the vector's ctor)
    virtual ~EESEECheckDisplay() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);

    std::vector<UIControl*> mControls;      // +0x0c (shown/hidden with the check box)
};

// vtable 0x443e00; ??_G 0x18439c; ProcessEvent 0x16fe58; sizeof 0x18; ctor inline; new at 0x183bec, 0x187f05
class EESEEUseObjectArea : public UIEvent {
public:
    // init list: the members are stored before the vptr (0x187f26, 0x183bec)
    EESEEUseObjectArea(UITextLabel* inLabel, EESETSEScenarioComboBox* inComboBox, UIEntity* inAreaControl)
        : mLabel(inLabel), mComboBox(inComboBox), mAreaControl(inAreaControl) {}
    virtual ~EESEEUseObjectArea() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);

    UITextLabel* mLabel;                    // +0x0c
    EESETSEScenarioComboBox* mComboBox;     // +0x10 (refilled with areas or objects)
    UIEntity* mAreaControl;                 // +0x14 (the control that selects areas)
};
#endif
