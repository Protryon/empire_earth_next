// C:\EEX\Empire Earth\Database\EECPBehavior.h (RECONSTRUCTION; file name inferred; provisional class and member
// names). The computer player behaviour table of db\dbcpbehavior.dat: an array of 129 records of 0x38 bytes,
// indexed by the record's own index (the AI reads it at 0x18c53d..). Since the game's leaves round 2 the array and
// its load are ee_data's (tables.rs gCPBehaviors, ee_data_cp_behavior_load: the record EeDataCpBehavior).
#ifndef EE_EECPBEHAVIOR_H
#define EE_EECPBEHAVIOR_H

#include <stdint.h>   // int32_t/uint32_t: the on-disk fields' fixed widths

#include "rust/data/tables.h"

// The fields are Rust's (ee_data tables.rs EeDataCpBehavior, with the size assert): +0x08..+0x2c are the "set AI
// attribute" editor's three slider ranges and a scale (EESETSETSetAIAttribute::ShowAttributeRanges 0x18d020).
struct EECPBehavior : EeDataCpBehavior {
    EECPBehavior() { this->mIsSettable = false; }

    // Loads the records into their slots of the table, squaring the maximum attack response range's float value;
    // returns the last read status (requires ee/EEDbRecords.h)
    template <class F> static long Load(F* inFile)
    {
        return EEDbReadRecords(inFile, ee_data_cp_behavior_record_size(), [](unsigned long) { return true; },
            [](unsigned long, const unsigned char* inRecord) { ee_data_cp_behavior_load(inRecord); });
    }
};

extern "C" EECPBehavior gCPBehaviors[129];   // ee_data tables.rs

// Computer player behaviour record ids (the gCPBehaviors index = EECPBehavior::mIndex): the one the C++ reads; the
// readers' names of every record are tools/eedata ids.py CPBEHAVIOR_NAMES (the computer player that read them is Rust)
enum EECPBehaviorID {
    kCPBehaviorMaximumAttackResponseRange = 48,             // Language.rc 28048 "Maximum Attack Response Range"
};

#endif
