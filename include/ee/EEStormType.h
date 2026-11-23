// ee/EEStormType.h (RECONSTRUCTION). Named integer constants; each comment gives the evidence for its name;
// (provisional name) marks an inferred one.
#ifndef EE_EESTORMTYPE_H
#define EE_EESTORMTYPE_H

// EEEMStorm::mType: EEEMStorm::GetTypeName names each type (names from its strings; setrilo environment_manager.h
// StormType agrees for 0..4).
enum EEStormType {
    kStormUnknown = 0,     // EEEMStorm::GetTypeName case 0: "Unknown"
    kStormRain = 1,        // EEEMStorm::GetTypeName case 1: "Rain"
    kStormSnow = 2,        // EEEMStorm::GetTypeName case 2: "Snow"
    kStormHurricane = 3,   // EEEMStorm::GetTypeName case 3: "Hurricane"
    kStormBlizzard = 4,    // EEEMStorm::GetTypeName case 4: "Blizzard"
};

#endif // EE_EESTORMTYPE_H
