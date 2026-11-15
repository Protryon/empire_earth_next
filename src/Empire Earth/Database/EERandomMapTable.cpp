// C:\EEX\Empire Earth\Database\EERandomMapTable.cpp (name from RTTI EERandomMapTable; no __FILE__). 0xda3be..
// 0xda56d: the class static table's init chain, ctor, Load (removed: see below),
// Unload, ??_G. EEMusicObject.cpp ends at 0xda3be, EESoundObject.cpp's chains start at 0xda56d. Unit flags: the
// exe's module set.

#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/FSFileSpec.h"

#include "ee/EEDatabaseTables.h"

std::vector<EERandomMapTable*> EERandomMapTable::sTable;

// Creates a random-map table entry with every value zero
EERandomMapTable::EERandomMapTable()
    : mL04(0), mL08(0), mF0C(0.0f), mF10(0.0f), mF14(0.0f), mL54(0), mL58(0), mL5C(0)
{
    this->mF18 = 0.0f;
    this->mF1C = 0.0f;
    this->mF20 = 0.0f;
    this->mF24 = 0.0f;
    this->mF28 = 0.0f;
    this->mF2C = 0.0f;
    this->mF30 = 0.0f;
    this->mF34 = 0.0f;
    this->mF38 = 0.0f;
    this->mF3C = 0.0f;
    this->mF40 = 0.0f;
    this->mF44 = 0.0f;
    this->mF48 = 0.0f;
    this->mF4C = 0.0f;
    this->mF50 = 0.0f;
}

// Destroys every random-map table entry and empties the table
void EERandomMapTable::Unload()
{
    for (unsigned long i = 0; i < EERandomMapTable::sTable.size(); i++)
        delete EERandomMapTable::sTable[i];
    EERandomMapTable::sTable.resize(0);
}
