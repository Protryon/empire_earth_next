// C:\EEX\Empire Earth\Database\EETerrainTypeTable.cpp (name from RTTI EETerrainTypeTable; no __FILE__). 0xdbcc8..
// 0xdc682: the class static table's init chain, ctor, Load (db\dbterrainType.dat), Unload, ??_G, the damage lookup
// 0xdbe44, LoadDamageTables 0xdbf11, ClearDamageTables. EETerrainTable.cpp ends at 0xdbcc8, EEUCUpgradeTable.cpp's
// chain starts at 0xdc682. Unit flags: the exe's module set.

#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/FSFileSpec.h"

#include "ee/EmpireEarth.h"
#include "ee/EEDatabaseTables.h"

// The damage tables (.bss after sTable 0x4e86f8): 0x4e8708 gDamageTable (EmpireEarth.h), 0x4e870c / 0x4e8710 its
// two twin tables, 0x4e8714 / 0x4e8718 their row and column counts (provisional names). VC6 orders a unit's .bss by
// neither definition order nor its reverse: ours places them 0x10 8710, 8714, 8708, 8718, 870c (placement open).
float** gDamageTable;
float** gDamageTableScaled1;
float** gDamageTableScaled2;
unsigned long gDamageTableRowCount;
unsigned long gDamageTableColumnCount;

// One record of db\dbterrainType.dat (0x80 bytes, read whole). Inferred layout.
struct EETerrainTypeRecord {
    char mLabel[100];                       // +0x000 the row name (the game never reads it; the text's label)
    int32_t mUnusedDatabaseID;              // +0x064 the database row id (not read)
    int32_t mID;                            // +0x68 -> +0x08
    int32_t mFirstColor;                    // +0x6c -> +0x0c
    int32_t mLastColor;                     // +0x70 -> +0x10
    int32_t mNameID;                        // +0x74 -> +0x14
    int32_t mUnusedPaletteIndex;            // +0x78 -> +0x18
    bool mIsInEditor;                       // +0x7c -> +0x1c
};
static_assert(sizeof(EETerrainTypeRecord) == 128, "on-disk record: 128 bytes on every target");

std::vector<EETerrainTypeTable*> EETerrainTypeTable::sTable;

// FUNCTION: EXE 0x4dbd11
// Creates an empty terrain type
EETerrainTypeTable::EETerrainTypeTable()
{
    this->mID = 0;
    this->mFirstColor = 0;
    this->mLastColor = 0;
    this->mNameID = 0;
    this->mL18 = 0;
    this->mIsInEditor = false;
}

// FUNCTION: EXE 0x4dbd2e
// Loads the terrain types (id, colour range, name, editor flag) from db\dbterrainType.dat; returns the last read status
long EETerrainTypeTable::Load(FSFileSpec* inFile)
{
    long theResult;
    EETerrainTypeRecord theRecord;
    inFile->Open(kFileAccessRead);
    unsigned long theCount = 0;
    unsigned long theRead = 0;
    theResult = inFile->Read(theCount);
    if (theResult == kErrorOK) {
        EETerrainTypeTable::sTable.resize(theCount, NULL);
        for (unsigned long i = 0; i < theCount; i++) {
            EETerrainTypeTable* theType = new EETerrainTypeTable;
            theResult = inFile->Read(reinterpret_cast<unsigned char*>(&theRecord), sizeof(theRecord), theRead);
            theType->mID = theRecord.mID;
            theType->mFirstColor = theRecord.mFirstColor;
            theType->mLastColor = theRecord.mLastColor;
            theType->mNameID = theRecord.mNameID;
            theType->mL18 = theRecord.mUnusedPaletteIndex;
            theType->mIsInEditor = theRecord.mIsInEditor;
            EETerrainTypeTable::sTable[i] = theType;
        }
    }
    inFile->Close();
    return theResult;
}

// FUNCTION: EXE 0x4dbde7
// Destroys every terrain type and empties the table
void EETerrainTypeTable::Unload()
{
    for (unsigned long i = 0; i < EETerrainTypeTable::sTable.size(); i++)
        delete EETerrainTypeTable::sTable[i];
    EETerrainTypeTable::sTable.resize(0);
}

// SYNTHETIC: EXE 0x4dbe27
// (the scalar deleting destructor)

// the lookup's classes (included after the table's init chain, as EETerrainTable.cpp does)
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEUnit.h"
#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEGlobalConstants.h"
#include "ee/EERules.h"
#include "ee/EERulesWorld.h"

// FUNCTION: EXE 0x4dbe44
// Returns the damage factor of an attack kind against a unit class (its family row), 0 when it cannot be attacked or a
// player is missing; the base table unless exactly one player (player 0 aside) is human, then a twin by difficulty
float EEToHitTable::GetAttackFactor(long inKind, EEUnitClass* inClass, unsigned long inPlayer, unsigned long inTargetPlayer)
{
    long theRow = inClass->mFamily;
    float theResult = 0.0f;
    if (inClass->CanBeAttacked()) {
        EEPlayer* thePlayer = gWorld.mPlayers[inPlayer];
        EEPlayer* theTarget = gWorld.mPlayers[inTargetPlayer];
        if (thePlayer == NULL || theTarget == NULL)
            return 0.0f;
        // the scale by difficulty (gameoptions.yaml difficulties: dbworld difficulty_damage_scale F / F2 on easy and
        // hard, none on medium) when exactly one side is "human" (IsHuman, not player 0; provisional name)
        bool theHuman = thePlayer->IsHuman() && inPlayer != 0;
        bool theTargetHuman = theTarget->IsHuman() && inTargetPlayer != 0;
        float theFactor = gDamageTable[theRow][inKind];
        if (theHuman == theTargetHuman)
            return theFactor;
        const std::vector<EEDifficultyRule>& theList = EEGetRules().mGameOptions.mDifficulties;
        unsigned long theDifficulty = gWorld.mGameOptions.mDifficulty;
        if (theDifficulty >= theList.size())
            return theFactor;
        const EEDifficultyRule& theRule = theList[theDifficulty];
        theResult = EERuleFloat(theTargetHuman ? theRule.mScaleHumanTarget : theRule.mScaleHumanAttacker) * theFactor;
    }
    return theResult;
}


// One record of the damage tables' file (0x17c bytes, read whole): 0x44 damage values from +0x6c. Inferred layout.
struct EEDamageTableRecord {
    char mLabel[100];                       // +0x000 the row name (the game never reads it; the text's label)
    int32_t mUnusedDatabaseID;              // +0x064 the database row id (not read)
    int32_t mUnusedIndex;                   // +0x068 the 0-based row index (not read)
    int32_t mDamage[0x44];                  // +0x6c (scaled by kHeightEpsilon into gDamageTable's row)
};
static_assert(sizeof(EEDamageTableRecord) == 380, "on-disk record: 380 bytes on every target");

// The number of unit families: the rows of dbfamily (the original refused a file of more than 60)
unsigned long EEFamilyCount()
{
    return gDamageTableRowCount;
}

// FUNCTION: EXE 0x4dbf11
// Loads every row of 0x44 damage factors (percent to fraction; the original: up to 60 rows) and builds the two twin
// tables scaled by world constant 0x59's two values; returns the last read status
// (the base row is written one statement per column)
long EETerrainTypeTable::LoadDamageTables(FSFileSpec* inFile)
{
    unsigned long theCount = 0;
    unsigned long theRead = 0;
    EEDamageTableRecord theRecord;
    if (gDamageTable != NULL || gDamageTableScaled1 != NULL || gDamageTableScaled2 != NULL)
        EETerrainTypeTable::ClearDamageTables();
    inFile->Open(kFileAccessRead);
    long theResult = inFile->Read(theCount);
    if (theResult == kErrorOK) {
        gDamageTableRowCount = theCount;
        gDamageTable = new float*[theCount];
        gDamageTableScaled1 = new float*[theCount];
        gDamageTableScaled2 = new float*[theCount];
        unsigned long i;
        for (i = 0; i < theCount; i++) {
            gDamageTable[i] = new float[0x44];
            gDamageTableScaled2[i] = new float[0x44];
            gDamageTableScaled1[i] = new float[0x44];
            theResult = inFile->Read(reinterpret_cast<unsigned char*>(&theRecord), sizeof(theRecord), theRead);
            gDamageTableColumnCount = 0x44;
            float theScale = kHeightEpsilon;
            gDamageTable[i][0] = theRecord.mDamage[0] * theScale;
            gDamageTable[i][1] = theRecord.mDamage[1] * theScale;
            gDamageTable[i][2] = theRecord.mDamage[2] * theScale;
            gDamageTable[i][3] = theRecord.mDamage[3] * theScale;
            gDamageTable[i][4] = theRecord.mDamage[4] * theScale;
            gDamageTable[i][5] = theRecord.mDamage[5] * theScale;
            gDamageTable[i][6] = theRecord.mDamage[6] * theScale;
            gDamageTable[i][7] = theRecord.mDamage[7] * theScale;
            gDamageTable[i][8] = theRecord.mDamage[8] * theScale;
            gDamageTable[i][9] = theRecord.mDamage[9] * theScale;
            gDamageTable[i][10] = theRecord.mDamage[10] * theScale;
            gDamageTable[i][11] = theRecord.mDamage[11] * theScale;
            gDamageTable[i][12] = theRecord.mDamage[12] * theScale;
            gDamageTable[i][13] = theRecord.mDamage[13] * theScale;
            gDamageTable[i][14] = theRecord.mDamage[14] * theScale;
            gDamageTable[i][15] = theRecord.mDamage[15] * theScale;
            gDamageTable[i][16] = theRecord.mDamage[16] * theScale;
            gDamageTable[i][17] = theRecord.mDamage[17] * theScale;
            gDamageTable[i][18] = theRecord.mDamage[18] * theScale;
            gDamageTable[i][19] = theRecord.mDamage[19] * theScale;
            gDamageTable[i][20] = theRecord.mDamage[20] * theScale;
            gDamageTable[i][21] = theRecord.mDamage[21] * theScale;
            gDamageTable[i][22] = theRecord.mDamage[22] * theScale;
            gDamageTable[i][23] = theRecord.mDamage[23] * theScale;
            gDamageTable[i][24] = theRecord.mDamage[24] * theScale;
            gDamageTable[i][25] = theRecord.mDamage[25] * theScale;
            gDamageTable[i][26] = theRecord.mDamage[26] * theScale;
            gDamageTable[i][27] = theRecord.mDamage[27] * theScale;
            gDamageTable[i][28] = theRecord.mDamage[28] * theScale;
            gDamageTable[i][29] = theRecord.mDamage[29] * theScale;
            gDamageTable[i][30] = theRecord.mDamage[30] * theScale;
            gDamageTable[i][31] = theRecord.mDamage[31] * theScale;
            gDamageTable[i][32] = theRecord.mDamage[32] * theScale;
            gDamageTable[i][33] = theRecord.mDamage[33] * theScale;
            gDamageTable[i][34] = theRecord.mDamage[34] * theScale;
            gDamageTable[i][35] = theRecord.mDamage[35] * theScale;
            gDamageTable[i][36] = theRecord.mDamage[36] * theScale;
            gDamageTable[i][37] = theRecord.mDamage[37] * theScale;
            gDamageTable[i][38] = theRecord.mDamage[38] * theScale;
            gDamageTable[i][39] = theRecord.mDamage[39] * theScale;
            gDamageTable[i][40] = theRecord.mDamage[40] * theScale;
            gDamageTable[i][41] = theRecord.mDamage[41] * theScale;
            gDamageTable[i][42] = theRecord.mDamage[42] * theScale;
            gDamageTable[i][43] = theRecord.mDamage[43] * theScale;
            gDamageTable[i][44] = theRecord.mDamage[44] * theScale;
            gDamageTable[i][45] = theRecord.mDamage[45] * theScale;
            gDamageTable[i][46] = theRecord.mDamage[46] * theScale;
            gDamageTable[i][47] = theRecord.mDamage[47] * theScale;
            gDamageTable[i][48] = theRecord.mDamage[48] * theScale;
            gDamageTable[i][49] = theRecord.mDamage[49] * theScale;
            gDamageTable[i][50] = theRecord.mDamage[50] * theScale;
            gDamageTable[i][51] = theRecord.mDamage[51] * theScale;
            gDamageTable[i][52] = theRecord.mDamage[52] * theScale;
            gDamageTable[i][53] = theRecord.mDamage[53] * theScale;
            gDamageTable[i][54] = theRecord.mDamage[54] * theScale;
            gDamageTable[i][55] = theRecord.mDamage[55] * theScale;
            gDamageTable[i][56] = theRecord.mDamage[56] * theScale;
            gDamageTable[i][57] = theRecord.mDamage[57] * theScale;
            gDamageTable[i][58] = theRecord.mDamage[58] * theScale;
            gDamageTable[i][59] = theRecord.mDamage[59] * theScale;
            gDamageTable[i][60] = theRecord.mDamage[60] * theScale;
            gDamageTable[i][61] = theRecord.mDamage[61] * theScale;
            gDamageTable[i][62] = theRecord.mDamage[62] * theScale;
            gDamageTable[i][63] = theRecord.mDamage[63] * theScale;
            gDamageTable[i][64] = theRecord.mDamage[64] * theScale;
            gDamageTable[i][65] = theRecord.mDamage[65] * theScale;
            gDamageTable[i][66] = theRecord.mDamage[66] * theScale;
            gDamageTable[i][67] = theRecord.mDamage[67] * theScale;
        }
        for (i = 0; i < theCount; i++) {
            for (long j = 0; j < 0x44; j++) {
                gDamageTableScaled1[i][j] = gWorldTable.GetFloatValue(kWorldDifficultyDamageScale) * gDamageTable[i][j];
                gDamageTableScaled2[i][j] = gWorldTable.GetFloatValue2(kWorldDifficultyDamageScale) * gDamageTable[i][j];
            }
        }
    }
    inFile->Close();
    return theResult;
}


// FUNCTION: EXE 0x4dc5bb
// Frees the three damage tables and zeroes their sizes
void EETerrainTypeTable::ClearDamageTables()
{
    unsigned long i;
    if (gDamageTable != NULL) {
        for (i = 0; i < gDamageTableRowCount; i++)
            if (gDamageTable[i] != NULL)
                delete[] gDamageTable[i];
        delete[] gDamageTable;
    }
    if (gDamageTableScaled1 != NULL) {
        for (i = 0; i < gDamageTableRowCount; i++)
            if (gDamageTableScaled1[i] != NULL)
                delete[] gDamageTableScaled1[i];
        delete[] gDamageTableScaled1;
    }
    if (gDamageTableScaled2 != NULL) {
        for (i = 0; i < gDamageTableRowCount; i++)
            if (gDamageTableScaled2[i] != NULL)
                delete[] gDamageTableScaled2[i];
        delete[] gDamageTableScaled2;
    }
    gDamageTable = NULL;
    gDamageTableScaled1 = NULL;
    gDamageTableScaled2 = NULL;
    gDamageTableColumnCount = 0;
    gDamageTableRowCount = 0;
}

// SYNTHETIC: EXE 0x4dbcc8
// SYNTHETIC: EXE 0x4dbcd2
// SYNTHETIC: EXE 0x4dbcea
// SYNTHETIC: EXE 0x4dbcf6
