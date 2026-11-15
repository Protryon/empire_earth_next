// C:\EEX\Empire Earth\Database\EEDatabase.h (RECONSTRUCTION). The game database reached
// through the pointer at 0x52fca8 (never stored by code: set elsewhere; provisional class and global names). The real
// header has a header static (TSScheduler::CreateCriticalSection assert, constructed in each of 7 includers) that is
// not modelled here.
#ifndef EE_EEDATABASE_H
#define EE_EEDATABASE_H

#include <vector>

class EEDbObject;
class FSFileSpec;

// The RTTI class EEDatabase (vtable 0x43e50c, sizeof 8): the database's loaded objects. Its only
// instance is the global at 0x5302f0 (name unknown; built by `mov ecx, 0x9302f0; jmp 0xd6035`). ctor 0xd6035
// (vptr, +4 = 0), ??_G 0xd6042, dtor 0xd605e (vptr, then a tail jump to Clear). Member names are provisional.
class EEDatabase {
public:
    EEDatabase();                           // 0xd6035
    virtual ~EEDatabase();                  // +0x00 (??_G 0xd6042; dtor 0xd605e)
    void Clear();                           // 0xd6179: deletes every object (slot 0 ??_G(1)), then the vector
    long Load(FSFileSpec* inFile);          // 0xd60ae: a count, then one EEDbObject per record
    EEDbObject* GetEntry(unsigned long inIndex);    // 0xd6069 (NULL past the end; provisional name)
    unsigned long GetCount();               // 0xd6094 (provisional name)

    std::vector<EEDbObject*>* mObjects;     // +0x04
};

// 0x5302f0: the database objects (dbobjects.dat; Empire Earth.cpp defines it; the unit class factory 0x2358b1 reads it)
extern EEDatabase gDatabaseObjects;

// The pointer at 0x52fca8 (formerly `gDatabase`) is gWorldTable.mEntries._First: read the
// constants by id with gWorldTable.GetLongValue(kWorldUnitLimitRange) etc. (ee/EEDatabaseTables.h, ee/EEWorldConstants.h).

#endif
