// C:\EEX\Low-Level Engine\Sound Engine\SESample.h (RECONSTRUCTION). A sound sample bound to the mixer that was
// active when it was made: the mixer's wave data and its playing state. Whole in Rust (rust/crates/ee_audio/src/sound
// objects.rs, LLE lane round 6): an SESample* is an EeSoundSample*; the members are inline forwarders.
// The sound manager makes and deletes samples; the mixer plug-in duplicates them (Duplicate, the original's copy
// constructor) and fills them. The "SetThe*" setters store without telling the mixer (the plug-in uses them).
// Requires lle/LowLevelEngine.h.
#ifndef LLE_SESAMPLE_H
#define LLE_SESAMPLE_H
#include "lle/LLEForward.h"

#include "lle/SEMixer.h"

// a none (-1) or paused (-2) id or channel keeps all bits set on LP64, as the C++ stored it (`= -1`)
inline unsigned long EeSoundWide(uint32_t inValue) { return inValue >= 0xfffffffeu ? (unsigned long)(long)(int32_t)inValue : (unsigned long)inValue; }

class LLE_API SESample : URustView {
public:
    // a copy of the sample's state without its wave data and channel
    void* GetData() const { return ee_sound_sample_data(Rec()); }
    float GetVolume() const { return ee_sound_sample_volume(Rec()); }
    long SetVolume(float inVolume) { return ee_sound_sample_set_volume(Rec(), inVolume); }

    EeSoundSample* Rec() const { return reinterpret_cast<EeSoundSample*>(const_cast<SESample*>(this)); }
};

#endif
