// C:\EEX\Empire Earth\Scenario Editor\EESERBUnitTypes.h (RECONSTRUCTION; file name inferred).
// The small control and action classes of the scenario editor's player tab (EESERBUnitTypes.cpp 0x173218..0x17acf6,
// EESEPlayerScreen's unit). Names are provisional; layouts only as far as the unit's code shows them. Requires
// lle/UIRadioButton.h, lle/UIComboBox.h, lle/UIButton.h, ee/EEAction.h.
#ifndef EE_EESERBUNITTYPES_H
#define EE_EESERBUNITTYPES_H

class UIButton;

// 0x5318cc: the world's player count the editor's player tab sets (EESEAChangeWorldPlayerCount; provisional name)
extern unsigned long& gMaxPlayers;

// RTTI EESEAChangeWorldPlayerCount > EEAction; vtable 0x44310c (??_G EEAction's 0x1e1b69, +0x08 the shared empty
// slot); sizeof 0x10. Built inline by EESEPlayerScreen::SetPlayerCount 0x173653 and posted to the UI thread.
class EESEAChangeWorldPlayerCount : public EEAction {
public:
    EESEAChangeWorldPlayerCount(unsigned long inCount) : mCount(inCount) {}
    virtual void Virtual4();                // +0x04 0x1737a3
    virtual void Virtual8() {}              // +0x08 (0x1720c6)

    unsigned long mCount;                   // +0x0c
};

// The diplomacy buttons' bitmaps and help text for the state inState (0 allies, 1 enemies), then the form's help text
// when inHelp (EESERBUnitTypes.cpp; the buttons a kind over the button shell, ee_game shells_editor.rs)
void EESEDiplomacyShow(UIButton* inButton, unsigned long inState, bool inHelp);

// (EESEEChangeStartEpoch, the start epoch combo box's event, is an EESEERust: ee/EESEARust.h, editor_events.rs)

#endif
