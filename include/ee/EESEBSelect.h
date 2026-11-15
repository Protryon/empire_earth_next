// C:\EEX\Empire Earth\Scenario Editor\EESEBSelect.h (RECONSTRUCTION; provisional names). The
// player screen's helper controls in 0x172d2c..0x173218 (before EESEPlayerScreen's ctor 0x173313): the technology
// list arrow buttons (RTTI EESEBSelect > UIButton; vtable 0x442ef0; ids 0x156 ">" 0x157 "<" 0x158 ">>" 0x159 "<<"; two
// class-static repeat polls, 500 ms and 200 ms, guard 0x531f50) and the screen's tab radio buttons (RTTI EESETab >
// UIRadioButton; vtable 0x442f9c). Requires lle/UIButton.h, lle/UIRadioButton.h, lle/TMPoll.h.
#ifndef EE_EESEBSELECT_H
#define EE_EESEBSELECT_H

class EESEBSelect : public UIButton {
public:
    EESEBSelect(unsigned long inId, const GE2DPoint& inLocation, UIForm* inForm, GEFont* inFont,
                unsigned long inIndex);   // 0x172da4
    virtual ~EESEBSelect() {}               // ??_G 0x172e64
    void Select();                          // 0x172f3b (moves the technologies between the two lists)
protected:
    virtual void ButtonDown();              // slot 0x88 0x172e87 (with key repeat)
    virtual void ButtonUp();                // slot 0x8c 0x172f0e
public:
    static TMPoll sRepeatDelay;             // 0x531f58 (500 ms)
    static TMPoll sRepeatRate;              // 0x531f30 (200 ms)
};

class EESETab : public UIRadioButton {
public:
    EESETab(unsigned long inId, const GE2DPoint& inLocation, GEFont* inFont, UIForm* inForm, unsigned long inIndex,
            bool inIsChecked);   // 0x173060
    virtual ~EESETab() {}                   // ??_G 0x17318d
    virtual void Check();                   // slot 0x94 0x1731b0 (switches the player screen's page)
};

#endif
