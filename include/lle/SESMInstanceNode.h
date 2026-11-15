// C:\EEX\Low-Level Engine\Sound Engine\SESMInstanceNode.h (RECONSTRUCTION). One playing (or paused) instance of a
// sound-manager sound, pooled and reference counted, held by units, unit classes and effects. Whole in Rust
// (rust/crates/ee_sound objects.rs, LLE lane round 6): an SESMInstanceNode* is an EeSoundNode*; the pool, the count
// and the fields are Rust's. The members are defined in SESoundManager.cpp. Requires lle/LowLevelEngine.h.
#ifndef LLE_SESMINSTANCENODE_H
#define LLE_SESMINSTANCENODE_H

class LLE_API SESMInstanceNode {
public:
    void Reference();
    // the last reference returns the node to the pool
    void Dereference();
private:
    SESMInstanceNode() = delete;
    ~SESMInstanceNode() = delete;
};

#endif
