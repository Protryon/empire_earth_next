// C:\EEX\Empire Earth\Empire Earth\EEUnitSetManager.cpp (name from RTTI EEUnitSetManager; no __FILE__). The database's
// unit sets: 0x136f4c..0x1372a6 (dtor, ??_G, Load, then vector<EEUnitSet*>::push_back). EEUnitSet.cpp ends at
// 0x136f4c; the EEVCWonder members (0x1372a6.., alphabetical: EEVCWonder.cpp) follow. Unit flags: the exe module set.
#include <windows.h>
#include <string.h>
#include <set>
#include <vector>
#include <utility>

#include "lle/LowLevelEngine.h"
#include "lle/FSFile.h"

#include "ee/EEUnitSetManager.h"

// One record of the unit set file (sizeof 0xb8: Load's memset and Read; provisional names)
struct EEUnitSetRecord {
    char mName[0x68];                       // +0x00
    long mID;                               // +0x68 (0 or less: no set at this index)
    unsigned long mFamilies[8];             // +0x6c (unit classes; all eight are added)
    long mUnitTypes[8];                     // +0x8c (only the positive ones are added)
    long mIncludedSet;                      // +0xac (index of the set's mL30)
    long mExcludedSet;                      // +0xb0 (index of the set's mL34)
    bool mShowInEditor;                     // +0xb4 (the set's mB39)
};
static_assert(sizeof(EEUnitSetRecord) == 184, "on-disk record: 184 bytes on every target");

// FUNCTION: EXE 0x536f4c
// Destroys the unit set manager, deleting every unit set
EEUnitSetManager::~EEUnitSetManager()
{
    for (unsigned long i = 0; i < this->mSets.size(); i++)
        delete this->mSets[i];
}

// FUNCTION: EXE 0x536f8d
// Destroys the unit set manager and frees it when asked (scalar deleting destructor)

// Returns the unit set at an index, or NULL past the end (no address in this range: 0xd150 in EECPASGUProphet.cpp's
// range is the ICF representative that every caller calls; GetSize is out of line there, 0x91f11. Defined here for
// the link; source position unknown)
EEUnitSet* EEUnitSetManager::GetSet(unsigned long inSet)
{
    return inSet < this->GetSize() ? this->mSets[inSet] : 0;
}

// FUNCTION: EXE 0x536fa9
// Loads the unit sets from the database file (an empty slot for each unused record), then links each set's
// included and excluded sets; returns the last read's result
long EEUnitSetManager::Load(FSFile* inFile)
{
    std::vector<std::pair<long, long> > theLinks;
    unsigned long theCount = 0;
    EEUnitSet* theSet;
    inFile->Open(kFileAccessRead);
    long theResult = inFile->Read(theCount);
    if (theResult == kErrorOK) {
        for (unsigned long i = 0; i < theCount; i++) {
            EEUnitSetRecord theRecord;
            unsigned long theRead;
            memset(&theRecord, 0, sizeof(theRecord));
            theResult = inFile->Read(reinterpret_cast<unsigned char*>(&theRecord), sizeof(theRecord), theRead);
            if (theRecord.mID > 0) {
                theSet = new EEUnitSet;
                theSet->mName = UWideString(theRecord.mName);
                theSet->mShowInEditor = theRecord.mShowInEditor;
                theLinks.push_back(std::pair<long, long>(theRecord.mIncludedSet, theRecord.mExcludedSet));
                long j;
                for (j = 0; j < 8; j++)
                    theSet->AddFamily(theRecord.mFamilies[j]);
                for (j = 0; j < 8; j++)
                    if (theRecord.mUnitTypes[j] > 0)
                        theSet->AddUnitType(theRecord.mUnitTypes[j]);
            } else {
                theSet = NULL;
                theLinks.push_back(std::pair<long, long>(0, 0));
            }
            this->mSets.push_back(theSet);
        }
    }
    inFile->Close();
    for (unsigned long i = 0; i < theCount; i++) {
        if (theLinks[i].first > 0 && theLinks[i].first < (long)this->mSets.size())
            this->mSets[i]->mIncludedSet = this->mSets[theLinks[i].first];
        if (theLinks[i].second > 0 && theLinks[i].second < (long)this->mSets.size())
            this->mSets[i]->mExcludedSet = this->mSets[theLinks[i].second];
    }
    return theResult;
}

// TEMPLATE: EXE 0x53727a
// ?push_back@?$vector@PAVEEUnitSet@@V?$allocator@PAVEEUnitSet@@@std@@@std@@QAEXABQAVEEUnitSet@@@Z
