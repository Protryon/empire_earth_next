// C:\EEX\Low-Level Engine\Input System\ISEntity.h (RECONSTRUCTION). An input element: a keyboard key (by DIK_* scan
// code; 0xe0..0xe2 the either-side Control, Shift and Alt keys), a mouse axis or button. The entities are Rust's
// (rust/crates/ee_input devices.rs, LLE lane round 2; the abstract class with the ISKey, ISExtendedKey, ISMouseAxis
// and ISMouseButton subclasses before): the devices' GetEntity hands out pointers to Rust's records and these
// forwarders read the system keyboard's or mouse's state. The class has no data and is never constructed in C++.
// A NULL entity (a scan code without a key) reads as released.
#ifndef LLE_ISENTITY_H
#define LLE_ISENTITY_H

#include "rust/ee_input.h"

class ISEntity {
public:
    long GetValue() const { return ee_input_entity_value(Rust()); }                     // 1 held; an axis its motion
    long GetNormalizedValue() const { return ee_input_entity_normalized_value(Rust()); } // 0x7fff held; wheel notches
    bool JustPressed() const { return ee_input_entity_just_pressed(Rust()); }
    bool JustUnpressed() const { return ee_input_entity_just_unpressed(Rust()); }
    bool JustDoublePressed() const { return ee_input_entity_just_double_pressed(Rust()); }
    unsigned long GetIndex() const { return ee_input_entity_index(Rust()); }             // a key's scan code
    const char* GetName() const { return ee_input_entity_name(Rust()); }                 // UTF-8 ("A key")
private:
    ISEntity() = delete;
    ~ISEntity() = delete;
    const EeInputEntity* Rust() const { return reinterpret_cast<const EeInputEntity*>(this); }
};

#endif
