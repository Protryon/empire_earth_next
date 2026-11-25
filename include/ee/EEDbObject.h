// C:\EEX\Empire Earth\Database\EEDbObject.h (RECONSTRUCTION; file name inferred; provisional names). One
// record of db\dbobjects.dat (RTTI EEDbObject, no base; vtable 0x43e514, one slot: the ??_G 0xd61ce that
// EEDatabase.cpp emits; sizeof 0x7bc from EEDatabase::Load's new). EEDbObject.cpp (0xd61f7..0xd68c7) defined the db
// file name keys (34 init chains) and Load. The ctor and dtor are inline (a UString at +0x7ac, vptr after it).
#ifndef EE_EEDBOBJECT_H
#define EE_EEDBOBJECT_H

class FSFileSpec;

// Since the game's leaves round 2 a record is ee_data's bytes (tables.rs ee_data_objects_*; ee/EEDatabase.h): the
// class is their layout (the original's vtable, its unread +0x7ac name and its Load went).
class EEDbObject {
public:
    char mRecord[0x7a8];                    // +0x004 (undecoded)
};

// The db file names are Rust's (ee_game gamecpp db_text.rs DB_TABLE_FILES and its three beside it; they were
// EEDbObject.cpp's 30 UStrings). The one a C++ loader still names (EEUnitClass.cpp's EEDbFindFile):
inline const char* const kDbObjectsFile = "db\\dbobjects.dat";

#endif
