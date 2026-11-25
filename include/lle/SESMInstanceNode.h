// C:\EEX\Low-Level Engine\Sound Engine\SESMInstanceNode.h (RECONSTRUCTION). One playing (or paused) instance of a
// sound-manager sound, pooled and reference counted, held by units, unit classes and effects. Whole in Rust
// (rust/crates/ee_audio/src/sound objects.rs, LLE lane round 6): an SESMInstanceNode* is an EeSoundNode*; the pool, the count
// and the fields are Rust's. The members are inline forwarders. Requires lle/LowLevelEngine.h.
#ifndef LLE_SESMINSTANCENODE_H
#define LLE_SESMINSTANCENODE_H
#include "lle/LLEForward.h"

#include "rust/audio/sound.h"

class LLE_API SESMInstanceNode : URustView {
public:
    void Reference() { ee_sound_node_reference(Rec()); }
    // the last reference returns the node to the pool
    void Dereference() { ee_sound_node_dereference(Rec()); }
    EeSoundNode* Rec() { return reinterpret_cast<EeSoundNode*>(this); }
};

#endif
