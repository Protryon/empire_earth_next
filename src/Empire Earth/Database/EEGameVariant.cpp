// C:\EEX\Empire Earth\Database\EEGameVariant.cpp (name from RTTI EEGameVariant; no __FILE__). 0xd68c7..0xd6bed: the
// variant table's init chain, ctor, ??_G, dtor, Load (db\dbGameVariant.dat), Unload, GetFactor, the table's
// push_back COMDAT. EEDbObject.cpp ends at 0xd68c7, EEGraphicEffect.cpp's chains start at 0xd6bed. Unit flags: the
// exe's module set.
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/FSFileSpec.h"

#include "ee/EEGameVariant.h"
#include "ee/EEUnitSetManager.h"

// NEAR (chain 0x4d68c7): the original's dtor registrar is guarded (a class static data member's guard byte
// 0x8e7f70); the header declares a plain global.
std::vector<EEGameVariant*> gGameVariants;

// FUNCTION: EXE 0x4d6910
// Creates an empty game variant
EEGameVariant::EEGameVariant()
{
}

// SYNTHETIC: EXE 0x4d6919
// (the scalar deleting destructor)

// FUNCTION: EXE 0x4d6935
// Destroys the game variant (nothing to release)
EEGameVariant::~EEGameVariant()
{
}

// FUNCTION: EXE 0x4d693c
// Loads the game variants (unit-set factors, cost, work-time, morale and earthquake factors) from db\dbGameVariant.dat;
// returns the last read status
long EEGameVariant::Load(FSFileSpec* inFile)
{
    unsigned long theCount = 0;
    unsigned long theRead = 0;
    EEGameVariantData theData;
    long theResult;
    inFile->Open(kFileAccessRead);
    theResult = inFile->Read(theCount);
    if (theResult == kErrorOK) {
        for (unsigned long i = 0; i < theCount; i++) {
            theResult = inFile->Read(reinterpret_cast<unsigned char*>(&theData), sizeof(theData), theRead);
            if (!(theData.mIndex >= 0))
                continue;
            EEGameVariant* theVariant = new EEGameVariant;
            theVariant->mFactors[0] = theData.mFactor0;
            theVariant->mFactors[1] = theData.mFactor1;
            theVariant->mFactors[2] = theData.mFactor2;
            theVariant->mFactors[3] = theData.mFactor3;
            theVariant->mFactors[4] = theData.mFactor4;
            theVariant->mFactors[5] = theData.mFactor5;
            theVariant->mFactors[6] = theData.mFactor6;
            theVariant->mFactors[7] = theData.mFactor7;
            theVariant->mFactors[8] = theData.mFactor8;
            theVariant->mFactors[9] = theData.mFactor9;
            theVariant->mSets[0] = theData.mSet0;
            theVariant->mSets[1] = theData.mSet1;
            theVariant->mSets[2] = theData.mSet2;
            theVariant->mSets[3] = theData.mSet3;
            theVariant->mSets[4] = theData.mSet4;
            theVariant->mSets[5] = theData.mSet5;
            theVariant->mSets[6] = theData.mSet6;
            theVariant->mSets[7] = theData.mSet7;
            theVariant->mSets[8] = theData.mSet8;
            theVariant->mSets[9] = theData.mSet9;
            theVariant->mDepositFactor = theData.mDepositFactor;
            theVariant->mEpochCostFactor = theData.mEpochCostFactor;
            theVariant->mWorkTimeFactor = theData.mWorkTimeFactor;
            theVariant->mMoraleBonus = theData.mMoraleBonus;
            theVariant->mIndex = theData.mIndex;
            theVariant->mNameID = theData.mNameID;
            theVariant->mFirstEpochCostFactor = theData.mFirstEpochCostFactor;
            theVariant->mSecondEpochCostFactor = theData.mSecondEpochCostFactor;
            theVariant->mSelectable = theData.mSelectable;
            theVariant->mCostSetFactor = theData.mCostSetFactor;
            theVariant->mCostSet = theData.mCostSet;
            theVariant->mEarthquakeDamageFactor = theData.mEarthquakeDamageFactor;
            gGameVariants.push_back(theVariant);
        }
    }
    inFile->Close();
    return theResult;
}

// FUNCTION: EXE 0x4d6b34
// Destroys every game variant and empties the list
void EEGameVariant::Unload()
{
    for (unsigned long i = 0; i < gGameVariants.size(); i++) {
        if (gGameVariants[i] != NULL)
            delete gGameVariants[i];
        gGameVariants[i] = NULL;
    }
    gGameVariants.clear();
}

// FUNCTION: EXE 0x4d6b74
// Returns the factor of the first (set, factor) pair whose unit set holds the unit type, else 0
float EEGameVariant::GetFactor(long inUnitType)
{
    float theFactor = 0.0f;
    for (unsigned long i = 0; i < 10; i++) {
        if (!(this->mSets[i] > 0))
            continue;
        EEUnitSet* theSet = gUnitSetManager.GetSet(this->mSets[i]);
        if (theSet->Contains(inUnitType)) {
            theFactor = this->mFactors[i];
            break;
        }
    }
    return theFactor;
}

// TEMPLATE: EXE 0x4d6bc1
// std::vector<EEGameVariant*>::push_back (ICF-named for vector<long>)

// The init chain (gGameVariants: its dtor is guarded in the original, a class static):
// SYNTHETIC: EXE 0x4d68c7
// SYNTHETIC: EXE 0x4d68d1
// SYNTHETIC: EXE 0x4d68e9
// SYNTHETIC: EXE 0x4d68f5
