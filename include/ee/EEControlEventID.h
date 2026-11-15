// ee/EEControlEventID.h (RECONSTRUCTION). Named integer constants; each comment gives the evidence for its name;
// (provisional name) marks an inferred one.
#ifndef EE_EECONTROLEVENTID_H
#define EE_EECONTROLEVENTID_H

// UI control event ids (dbuicontrolevents.dat: event id, owner control, trigger type, name); the EECreate<Form>Event
// factories in EEUIManager.cpp switch on them; names from the record names (a repeated name takes its trigger type,
// UIControlTriggerID).
enum EEControlEventID {
    kControlEventWNSCancel = 106,                                  // dbuicontrolevents.dat event 106 (owner control 564, trigger 1008) "WNS Cancel"
};

#endif // EE_EECONTROLEVENTID_H
