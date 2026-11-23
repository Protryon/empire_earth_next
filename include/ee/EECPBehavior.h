// C:\EEX\Empire Earth\Database\EECPBehavior.h (RECONSTRUCTION; file name inferred; provisional class and member
// names). The computer player behaviour table of db\dbcpbehavior.dat: an array of 129 records of 0x38 bytes,
// indexed by the record's own index (the AI reads it at 0x18c53d..). Since the game's leaves round 2 the array and
// its load are ee_data's (tables.rs gCPBehaviors, ee_data_cp_behavior_load: the record EeDataCpBehavior).
#ifndef EE_EECPBEHAVIOR_H
#define EE_EECPBEHAVIOR_H

#include <stdint.h>   // int32_t/uint32_t: the on-disk fields' fixed widths

#include "rust/data/tables.h"

struct EECPBehavior {
    EECPBehavior() { this->mIsSettable = false; }

    // Loads the records into their slots of the table, squaring the maximum attack response range's float value;
    // returns the last read status (requires ee/EEDbRecords.h)
    template <class F> static long Load(F* inFile)
    {
        return EEDbReadRecords(inFile, ee_data_cp_behavior_record_size(), [](unsigned long) { return true; },
            [](unsigned long, const unsigned char* inRecord) { ee_data_cp_behavior_load(inRecord); });
    }

    int32_t mUnused1;                       // +0x00
    int32_t mIndex;                         // +0x04 (the slot; records with index <= 0 are skipped)
    // +0x08..+0x2c: the "set AI attribute" editor (EESETSETSetAIAttribute::ShowAttributeRanges 0x18d020)
    // reads three slider ranges (+0x08/+0x0c, +0x14/+0x18, +0x24/+0x28) and a scale (+0x2c, 0 -> 1; the third
    // slider's value is the float effect value times it); +0x10/+0x1c/+0x20 unread there. Names by offset.
    int32_t mIntValue1Min;                  // +0x08
    int32_t mIntValue1Max;                  // +0x0c
    int32_t mIntValue1;                     // +0x10
    int32_t mIntValue2Min;                  // +0x14
    int32_t mIntValue2Max;                  // +0x18
    int32_t mIntValue2;                     // +0x1c
    int32_t mUnused2;                       // +0x20
    int32_t mFloatSliderMin;                // +0x24
    int32_t mFloatSliderMax;                // +0x28
    int32_t mFloatSliderScale;              // +0x2c
    float mFloatValue;                      // +0x30 (squared after loading, for record 48 only)
    bool mIsSettable;                       // +0x34
};

static_assert(sizeof(EECPBehavior) == 56, "on-disk record: 56 bytes on every target");

extern "C" EECPBehavior gCPBehaviors[129];   // ee_data tables.rs

// Computer player behaviour record ids (the gCPBehaviors index = EECPBehavior::mIndex): the one the C++ reads; the
// readers' names of every record are tools/eedata ids.py CPBEHAVIOR_NAMES (the computer player that read them is Rust)
enum EECPBehaviorID {
    kCPBehaviorMaximumAttackResponseRange = 48,             // Language.rc 28048 "Maximum Attack Response Range"
};

#endif
