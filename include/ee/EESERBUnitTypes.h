// C:\EEX\Empire Earth\Scenario Editor\EESERBUnitTypes.h (RECONSTRUCTION; file name inferred).
// The small control and action classes of the scenario editor's player tab (EESERBUnitTypes.cpp 0x173218..0x17acf6,
// EESEPlayerScreen's unit). Names are provisional; layouts only as far as the unit's code shows them. Requires
// lle/UIRadioButton.h, lle/UIComboBox.h, lle/UIButton.h, ee/EEAction.h.
#ifndef EE_EESERBUNITTYPES_H
#define EE_EESERBUNITTYPES_H

// 0x5318cc: the world's player count the editor's player tab sets (EESEAChangeWorldPlayerCount; provisional name)
extern unsigned long& gMaxPlayers;

// (EESEAChangeWorldPlayerCount, RTTI > EEAction, vtable 0x44310c, Virtual4 0x1737a3: gMaxPlayers set to its count,
// is the EESEARust kind EE_SCREENS_SEW_PLAYER_COUNT, ee_screens editor_whole.rs)

// (EESEDiplomacyShow, the diplomacy buttons' bitmaps and help text: EESERBUnitTypes.cpp, registered with the shells)

// (EESEEChangeStartEpoch, the start epoch combo box's event, is an EESEERust: ee/EESEARust.h, editor_events.rs)

#endif
