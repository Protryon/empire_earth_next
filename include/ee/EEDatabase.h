// C:\EEX\Empire Earth\Database\EEDatabase.h (RECONSTRUCTION). The database of db\dbobjects.dat's records (the
// original: RTTI EEDatabase, vtable 0x43e50c, sizeof 8; its only instance gDatabaseObjects at 0x5302f0). Since the
// game's leaves round 2 the records are ee_data's (tables.rs: ee_data_objects_*); the class is their readers.
#ifndef EE_EEDATABASE_H
#define EE_EEDATABASE_H

#include "rust/data/tables.h"
class EEDbObject;   // ee/EEDbObject.h: the record's bytes

class EEDatabase {
public:
    // 0xd6179: no records
    void Clear() { ee_data_objects_clear(); }
    // 0xd60ae: a count, then one record each (the reads stop at the first error); its status (requires
    // ee/EEDbRecords.h)
    template <class F> long Load(F* inFile)
    {
        ee_data_objects_clear();
        return EEDbReadRecords(inFile, ee_data_object_record_size(),
            [](unsigned long inCount) { ee_data_objects_begin(inCount); return true; },
            [](unsigned long inIndex, const unsigned char* inRecord) { ee_data_objects_load(inIndex, inRecord); }, true);
    }
    // 0xd6069: record inIndex (NULL past the end or nothing loaded; provisional name)
    EEDbObject* GetEntry(unsigned long inIndex) { return reinterpret_cast<EEDbObject*>(ee_data_objects_get(inIndex)); }
    unsigned long GetCount() { return (unsigned long)ee_data_objects_count(); }
};

// 0x5302f0: the database objects (dbobjects.dat; Empire Earth.cpp defines it; the unit class factory 0x2358b1 reads it)
extern EEDatabase gDatabaseObjects;

// The pointer at 0x52fca8 (formerly `gDatabase`) is gWorldTable.mEntries._First: read the
// constants by id with gWorldTable.GetLongValue(kWorldUnitLimitRange) etc. (ee/EEDatabaseTables.h, ee/EEWorldConstants.h).

#endif
