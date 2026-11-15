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

#include "lle/UWideString.h"

class EESMDialogMessage {
public:
    EESMDialogMessage();                    // 0x1b9eb8
    EESMDialogMessage(const EESMDialogMessage& inMessage);   // 0x1b9ee9
    void operator=(const EESMDialogMessage& inMessage);   // 0x1b9f4a (void: its four callers discard eax, and its body returns no value)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x00 0x1af92b
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x04 0x1afa14

    UWideString mText;                      // +0x04
    UWideString mSound;                     // +0x18
    long mScrollSpeed;                      // +0x2c
    unsigned long mUnitType;                // +0x30 unsigned: Load/Save 0x1af92b/0x1afa14 use FSFile Read/Write(unsigned long&)
    unsigned long mDuration;                // +0x34 unsigned: Load/Save 0x1af92b/0x1afa14 use FSFile Read/Write(unsigned long&)
    unsigned long mUnitID;                  // +0x38 unsigned: Load/Save 0x1af92b/0x1afa14 use FSFile Read/Write(unsigned long&)
    bool mIsCinematic;                      // +0x3c
};

#endif
