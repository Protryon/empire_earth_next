// C:\EEX\Empire Earth\Database\EEDatabase.cpp (name from RTTI EEDatabase; no __FILE__). The database of loaded
// EEDbObject records: 0xd6035..0xd61f7 (ctor, ??_G, dtor, GetEntry, GetCount, Load, Clear, then EEDbObject's ??_G,
// emitted after its first user). EEButtonObject's neighbour unit ends at 0xd6035 (0xd5f7e..0xd6035 is a globals
// chain unit of its own); EEDbObject.cpp's key chains start at 0xd61f7. No sDatabaseLock chain: this unit does not
// include Database/EEDatabase.h. Unit flags: the exe's module set.
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/FSFileSpec.h"

#include "ee/EEDatabase.h"
#include "ee/EEDbObject.h"

// FUNCTION: EXE 0x4d6035
// Creates an empty database with no record list
EEDatabase::EEDatabase()
{
    this->mObjects = NULL;
}

// SYNTHETIC: EXE 0x4d6042
// (the scalar deleting destructor)

// FUNCTION: EXE 0x4d605e
// Destroys the database, freeing its records
EEDatabase::~EEDatabase()
{
    this->Clear();
}

// FUNCTION: EXE 0x4d6069
// Returns the record at an index, or NULL when out of range or nothing is loaded
EEDbObject* EEDatabase::GetEntry(unsigned long inIndex)
{
    if (this->mObjects != NULL && inIndex < this->mObjects->size())
        return (*this->mObjects)[inIndex];
    return NULL;
}

// FUNCTION: EXE 0x4d6094
// Returns the number of records (0 when nothing is loaded)
unsigned long EEDatabase::GetCount()
{
    if (this->mObjects != NULL)
        return this->mObjects->size();
    return 0;
}

// FUNCTION: EXE 0x4d60ae
// Loads the database's records from a file, replacing any loaded ones; stops at the first failed record and
// returns its error
long EEDatabase::Load(FSFileSpec* inFile)
{
    unsigned long theCount = 0;
    long theResult;
    if (this->mObjects != NULL)
        this->Clear();
    this->mObjects = new std::vector<EEDbObject*>;
    inFile->Open(kFileAccessRead);
    theResult = inFile->Read(theCount);
    if (theResult == kErrorOK) {
        this->mObjects->resize(theCount, NULL);
        for (unsigned long i = 0; i < theCount; i++) {
            EEDbObject* theObject = new EEDbObject;
            theResult = theObject->Load(inFile);
            if (theResult != kErrorOK) {
                delete theObject;   // the failed record is freed (it leaked)
                break;
            }
            (*this->mObjects)[i] = theObject;
        }
    }
    inFile->Close();
    return theResult;
}

// FUNCTION: EXE 0x4d6179
// Frees every record and the record list
void EEDatabase::Clear()
{
    if (this->mObjects == NULL)
        return;
    for (unsigned long i = 0; i != this->mObjects->size(); i++)
        delete (*this->mObjects)[i];
    delete this->mObjects;
    this->mObjects = NULL;
}

// SYNTHETIC: EXE 0x4d61ce
// (EEDbObject's scalar deleting destructor)
