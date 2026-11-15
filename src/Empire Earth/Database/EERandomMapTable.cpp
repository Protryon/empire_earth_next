// C:\EEX\Empire Earth\Database\EERandomMapTable.cpp (name from RTTI EERandomMapTable; no __FILE__). 0xda3be..
// 0xda56d: the class static table's init chain, ctor, Load (db\dbrandommap.dat: the records are read and dropped),
// Unload, ??_G. EEMusicObject.cpp ends at 0xda3be, EESoundObject.cpp's chains start at 0xda56d. Unit flags: the
// exe's module set.

#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/FSFileSpec.h"

#include "ee/EEDatabaseTables.h"

std::vector<EERandomMapTable*> EERandomMapTable::sTable;

// FUNCTION: EXE 0x4da407
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

// FUNCTION: EXE 0x4da47b
// Reads db\dbrandommap.dat, creating one empty entry per record (the record contents are dropped); returns the
// last read status
long EERandomMapTable::Load(FSFileSpec* inFile)
{
    unsigned long theCount = 0;
    unsigned long theRead = 0;
    EERandomMapRecord theRecord;   // read and dropped
    inFile->Open(kFileAccessRead);
    long theResult = inFile->Read(theCount);
    if (theResult == kErrorOK) {
        EERandomMapTable::sTable.resize(theCount, NULL);
        for (unsigned long i = 0; i < theCount; i++) {
            EERandomMapTable* theTable = new EERandomMapTable;
            theResult = inFile->Read(reinterpret_cast<unsigned char*>(&theRecord), sizeof(theRecord), theRead);
            EERandomMapTable::sTable[i] = theTable;
        }
    }
    inFile->Close();
    return theResult;
}

// FUNCTION: EXE 0x4da510
// Destroys every random-map table entry and empties the table
void EERandomMapTable::Unload()
{
    for (unsigned long i = 0; i < EERandomMapTable::sTable.size(); i++)
        delete EERandomMapTable::sTable[i];
    EERandomMapTable::sTable.resize(0);
}

// SYNTHETIC: EXE 0x4da550
// (the scalar deleting destructor)

// SYNTHETIC: EXE 0x4da3be
// SYNTHETIC: EXE 0x4da3c8
// SYNTHETIC: EXE 0x4da3e0
// SYNTHETIC: EXE 0x4da3ec
