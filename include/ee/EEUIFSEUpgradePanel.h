// C:\EEX\Empire Earth\Scenario Editor\EEUIFSEUpgradePanel.h (RECONSTRUCTION; class and header names inferred: no RTTI,
// no vtable). The unit tab's upgrade panel (EESEUnit +0xb4: new 0x60; ctor 0x1a4e99, dtor 0x1a549d, ShowUnit 0x1a551a):
// a status box of the main game's kind over the scenario editor form, with its portrait,
// two status bars, nine attribute icons with their labels and two text labels. The ctor increments BOTH counters
// (ioID, ioIndex) after every control, so the first parameter is not const (EESEUnit.cpp's local declaration says
// `const unsigned long&`). Requires ee/EEUIFMGStatusBox.h.
#ifndef EE_EEUIFSEUPGRADEPANEL_H
#define EE_EEUIFSEUPGRADEPANEL_H

class EEUIFMGAttribute;
class EEUIFMGStatusBar;
class EEUICPortrait;
class EEUIFScenarioEditor;
class EEUnit;
class UITextLabel;

class EEUIFSEUpgradePanel {
public:
    EEUIFSEUpgradePanel(unsigned long& ioID, unsigned long& ioIndex, EEUIFScenarioEditor* inForm);   // 0x1a4e99
    ~EEUIFSEUpgradePanel();                 // 0x1a549d
    void ShowUnit(EEUnit* inUnit);          // 0x1a551a (the status box shows the unit)

    EEUIFMGAttribute* mAttributes[9];       // +0x00 (new 0x138, ctor 0x26783d; the status box's icons)
    UITextLabel* mLabels[9];                // +0x24 (the status box's labels)
    EEUICPortrait* mPortrait;               // +0x48 (the status box's +0x8c)
    EEUIFMGStatusBar* mLeftStatusBar;       // +0x4c (the status box's +0x84)
    EEUIFMGStatusBar* mMiddleStatusBar;     // +0x50 (the status box's +0x88)
    UITextLabel* mUnitNameLabel;            // +0x54 (the status box's +0x90)
    UITextLabel* mArmorTitleLabel;          // +0x58 (string 0x76d7; the status box's +0x98)
    EEUIFMGStatusBox* mStatusBox;           // +0x5c (new 0xb0, ctor 0x26ada8 on the editor form)
};

#endif
