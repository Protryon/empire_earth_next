// ee/EESMDialogMessage.h: layout only as far as the verifier-exact
// functions that include it prove it (member names by offset are placeholders).
// EEScenarioManager's +0x144 vector holds these by value (0x40-byte elements; vector members 0x1b5652..). The vtable
// 0x446028 has two slots (+0x00 0x1af92b loads, +0x04 0x1afa14 saves: `ret 0xc`, the UWideString Load/Save of +0x04
// first) and no dtor slot; the dtor is implicit (its ??_G 0x1b6125 is called directly with 0 by vector::_Destroy, the
// out-of-line body 0x1b5cfd destroys +0x18, +0x04 without a vptr store). Copy ctor 0x1b9ee9 / operator= 0x1b9f4a are
// user-written (field-wise, in the same order) and defined in
// EEScenarioManager.cpp after the default ctor. Requires lle/UWideString.h, lle/FSFile.h.
#ifndef EE_EESMDIALOGMESSAGE_H
#define EE_EESMDIALOGMESSAGE_H

#include "rust/sim/scenario.h"

// the numbers (scroll speed, unit type, duration, unit id, cinematic flag) are the Rust record's (ee_scenario fields.rs)
class EESMDialogMessage : public EeScenarioMessageFields {
public:
    // Creates an empty dialog message: no unit or duration, no camera move (scroll speed -1), not for a cinematic
    EESMDialogMessage() : EeScenarioMessageFields(ee_scenario_message_defaults()) {  }
    // the copies are the implicit member-wise ones (the original's 0x1b9ee9 / 0x1b9f4a copied field by field)
    // a dialog message's Load and Save (ee_scenario save.rs message_io): the text, unit type, sound, duration, unit,
    // scroll speed, and (a save, or a load from version 0.200 on) the cinematic flag; the first error
    // +0x00 0x1af92b
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile) { return ee_scenario_en_message_io(this, &inFile, inMajorVersion, inMinorVersion, false); }
    // +0x04 0x1afa14
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile) { return ee_scenario_en_message_io(this, &outFile, inMajorVersion, inMinorVersion, true); }

    UWideString mText;                      // +0x04
    UWideString mSound;                     // +0x18
};

#endif
