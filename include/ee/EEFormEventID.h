// ee/EEFormEventID.h (RECONSTRUCTION). Named integer constants; each comment gives the evidence for its name;
// (provisional name) marks an inferred one.
#ifndef EE_EEFORMEVENTID_H
#define EE_EEFORMEVENTID_H

// UI database form-event ids (dbuiformevents): EECreateFormEvent creates the event class for each
enum EEFormEventID {
    kFormEventMainScreenOpen = 1,       // db dbuiformevents id 1 'Main Screen Open'; EECreateFormEvent: the screen event EE_SCREENS_MAIN_OPEN
    kFormEventGameSettingsOpen = 2,     // db dbuiformevents id 2 'Game Settings Open'; no case in EECreateFormEvent
    kFormEventChatFormOpen = 3,         // db dbuiformevents id 3 'Chat Form Open'; EECreateFormEvent: EE_SCREENS_CH_OPEN
    kFormEventChatFormClose = 4,        // db dbuiformevents id 4 'Chat Form Close'; EECreateFormEvent: EE_SCREENS_CH_CLOSE
    kFormEventChatMLBPressed = 5,       // db dbuiformevents id 5 'Chat MLB Pressed'; EECreateFormEvent: EE_SCREENS_CH_CANCEL
    kFormEventChatMRBPressed = 6,       // db dbuiformevents id 6 'Chat MRB Pressed'; EECreateFormEvent: EE_SCREENS_CH_CANCEL
    kFormEventScenarioIntroOpen = 7,    // db dbuiformevents id 7 'Scenario Intro Open'; EECreateFormEvent: EE_SCREENS_SI_OPEN
    kFormEventWNSFormOpen = 8,          // db dbuiformevents id 8 'WNS Form Open'; EECreateFormEvent: new EEUIEWNSOpen
    kFormEventSSFormOpen = 9,           // db dbuiformevents id 9 'SS Form Open'; EECreateFormEvent: new EEUIESSEOpen
    kFormEventScenarioIntroClose = 10,  // db dbuiformevents id 10 'Scenario Intro Close'; EECreateFormEvent: EE_SCREENS_SI_CLOSE
    kFormEventSinglePlayerLoadOpen = 11,  // db dbuiformevents id 11 'Single Player Load Open'; EECreateFormEvent: EE_SCREENS_SPL_OPEN
    kFormEventScenarioLoadOpen = 12,    // db dbuiformevents id 12 'Scenario Load Open'; EECreateFormEvent: EE_SCREENS_SCL_OPEN
    kFormEventPGSOpen = 13,             // db dbuiformevents id 13 'PGS Open'; EECreateFormEvent: new EEUIEPGSEOpen
    kFormEventPGSClose = 14,            // db dbuiformevents id 14 'PGS Close'; EECreateFormEvent: new EEUIEPGSEClose
    kFormEventDSOpen = 15,              // db dbuiformevents id 15 'DS Open'; EECreateFormEvent: new EEUIEDSOpen
    kFormEventCEOpen = 16,              // db dbuiformevents id 16 'CE Open'; EECreateFormEvent: EE_SCREENS_CE_OPEN
    kFormEventCEClose = 0x11,           // db dbuiformevents id 17 'CE Close'; EECreateFormEvent: EE_SCREENS_CE_CLOSE
    kFormEventChatLogOpen = 0x12,       // db dbuiformevents id 18 'Chat Log Open'; EECreateFormEvent: EE_SCREENS_CL_OPEN
    kFormEventChatLogMLBPressed = 0x13,  // db dbuiformevents id 19 'Chat Log MLB Pressed'; EECreateFormEvent: EE_SCREENS_CL_MOUSE_PRESSED
    kFormEventChatLogMRBPressed = 0x14,  // db dbuiformevents id 20 'Chat Log MRB Pressed'; EECreateFormEvent: EE_SCREENS_CL_MOUSE_PRESSED
};

#endif // EE_EEFORMEVENTID_H
