// C:\EEX\Empire Earth\Database\EEAIUnitTargetingTable.h (RECONSTRUCTION; file name inferred; provisional names). The
// AI's unit targeting table, loaded from db\dbaiunittargeting.dat (Empire Earth.cpp: the key at 0x8e7ec0, FindFile,
// then Load). RTTI EEAIUnitTargetingTable (no base; vtable 0x4420c8, one slot: the deleting dtor 0x13f719, emitted by
// Empire Earth.cpp). The loader is EEAIUnitTargetingTable.cpp 0xd47a1..0xd490f. Its table is the
// class static sTable (0x4e60c0, no init chain).
#ifndef EE_EEAIUNITTARGETINGTABLE_H
#define EE_EEAIUNITTARGETINGTABLE_H

#include "ee/EEDbRecords.h"

// The table is Rust's (ee_data tables.rs: three rows of ten weights, read by EEFOFindNewTargetNearOldTarget's target
// search); EEAIUnitTargetingTable.cpp went in the game's leaves round 1.
class EEAIUnitTargetingTable {
public:
    virtual ~EEAIUnitTargetingTable() {}

    // 0xd47a1: the weights from the file (the record and its copy: ee_data tables.rs); the table stays empty unless
    // it has 3 records; the last read status
    template <class F> static long Load(F* inFile)
    {
        return EEDbReadRecords(inFile, ee_data_ai_targeting_record_size(),
                               [](unsigned long inCount) { return ee_data_ai_targeting_begin(inCount); },
                               [](unsigned long inIndex, const unsigned char* inRecord) {
                                   ee_data_ai_targeting_load(inIndex, inRecord);
                               });
    }
    static void Unload() { ee_data_ai_targeting_unload(); }
};

#endif
