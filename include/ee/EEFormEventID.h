// ee/EEFormEventID.h (RECONSTRUCTION). Named integer constants; each comment gives the evidence for its name;
// (provisional name) marks an inferred one.
#ifndef EE_EEFORMEVENTID_H
#define EE_EEFORMEVENTID_H

// UI database form-event ids (dbuiformevents): EECreateFormEvent creates the event class for each
enum EEFormEventID {
    kFormEventSSFormOpen = 9,           // db dbuiformevents id 9 'SS Form Open'; EECreateFormEvent: new EEUIESSEOpen
    kFormEventDSOpen = 15,              // db dbuiformevents id 15 'DS Open'; EECreateFormEvent: new EEUIEDSOpen
};

#endif // EE_EEFORMEVENTID_H
