// C:\EEX\Empire Earth\Database\EEStartingResources.cpp (name from RTTI EEStartingResources; no __FILE__). 0xda984..
// 0xdae63: the class static table's init chain, ctor, ??_G, dtor, Load (db\dbStartingResources.dat), Unload, the
// table's push_back COMDAT, then the six tag-point helpers 0xdab52..0xdae63 (EETagPointList, EETagPoint: ee_gfx
// tags.rs since the Graphics + LLE cluster's round 1).
// EESoundObject.cpp ends at 0xda984, EETerrainColorTable.cpp's chain starts at 0xdae63. Unit flags: the exe's
// module set.

#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/FSFileSpec.h"

#include "ee/EEStartingResources.h"
#include "ee/EEDbRecords.h"

EEDbList<EEStartingResources, EeDataList::StartingResources> EEStartingResources::sTable;



// Loads the starting-resource presets (food, wood, stone, gold, iron, name) from db\dbStartingResources.dat into the
// Rust list (the record, its copy and the list: ee_data tables.rs); rows with a negative index are skipped. Returns the
// last read status
long EEStartingResources::Load(FSFileSpec* inFile)
{
    return EEDbLoadList(inFile, EeDataList::StartingResources);
}

// Empties the table
void EEStartingResources::Unload()
{
    EEStartingResources::sTable.clear();
}
