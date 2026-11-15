// C:\EEX\Empire Earth\Database\EEAIUnitTargetingTable.h (RECONSTRUCTION; file name inferred; provisional names). The
// AI's unit targeting table, loaded from db\dbaiunittargeting.dat (Empire Earth.cpp: the key at 0x8e7ec0, FindFile,
// then Load). RTTI EEAIUnitTargetingTable (no base; vtable 0x4420c8, one slot: the deleting dtor 0x13f719, emitted by
// Empire Earth.cpp). The loader is EEAIUnitTargetingTable.cpp 0xd47a1..0xd490f. Its table is the
// class static sTable (0x4e60c0, no init chain).
#ifndef EE_EEAIUNITTARGETINGTABLE_H
#define EE_EEAIUNITTARGETINGTABLE_H

class FSFileSpec;

class EEAIUnitTargetingTable {
public:
    virtual ~EEAIUnitTargetingTable() {}

    static long Load(FSFileSpec* inFile);   // 0xd47a1 (three records)
    static void Unload();                   // 0xd48d8

    static long** sTable;                   // 0x4e60c0: three rows of ten values (provisional name; read by
                                            // EECPAttackSubGroupUnit.cpp and two UI units, so a class static)
};

#endif
