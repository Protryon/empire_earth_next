// C:\EEX\Empire Earth\Database\EEColorTable.cpp (name from RTTI EEColorTable; no __FILE__). 0xd5d08..0xd5f7e: the
// two class statics' init chains, Load (db\dbcolortable.dat), Unload, FindIndex, the ctor, ??_G. EEButtonObject.cpp
// ends at 0xd5d08, EECPBehaviorTable.cpp's array chain starts at 0xd5f7e. Unit flags: the exe's module set.

#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/FSFileSpec.h"

#include "ee/EEColorTable.h"

// One record of db\dbcolortable.dat (0xa8 bytes, read whole). Inferred layout.
struct EEColorTableRecord {
    char mLabel[100];                       // +0x000 the row name (the game never reads it; the text's label)
    long mUnusedDatabaseID;                 // +0x064 the database row id (not read)
    long mUnusedIndex;                      // +0x068 the 0-based row index (not read)
    float mColorRed;                        // +0x6c
    float mColorGreen;                      // +0x70
    float mColorBlue;                       // +0x74
    float mColor2Red;                       // +0x78
    float mColor2Green;                     // +0x7c
    float mColor2Blue;                      // +0x80
    float mColor4Red;                       // +0x84
    float mColor4Green;                     // +0x88
    float mColor4Blue;                      // +0x8c
    char _gap90[0x18];
};
static_assert(sizeof(EEColorTableRecord) == 168, "on-disk record: 168 bytes on every target");

std::vector<EEColorTable*> EEColorTable::sTable;
GEColor EEColorTable::sColor;

// FUNCTION: EXE 0x4d5d91
// Loads the player colours from db\dbcolortable.dat (main, second with a 60% dark shade, fourth); returns the last
// read status
long EEColorTable::Load(FSFileSpec* inFile)
{
    unsigned long theCount = 0;
    unsigned long theRead = 0;
    EEColorTableRecord theRecord;
    inFile->Open(kFileAccessRead);
    long theResult = inFile->Read(theCount);
    if (theResult == kErrorOK) {
        EEColorTable::sTable.resize(theCount, NULL);
        for (unsigned long i = 0; i < theCount; i++) {
            EEColorTable* theColor = new EEColorTable;
            theResult = inFile->Read(reinterpret_cast<unsigned char*>(&theRecord), sizeof(theRecord), theRead);
            theColor->mColor.SetRed(theRecord.mColorRed);
            theColor->mColor.SetGreen(theRecord.mColorGreen);
            theColor->mColor.SetBlue(theRecord.mColorBlue);
            theColor->mColor2.SetRed(theRecord.mColor2Red);
            theColor->mColor2.SetGreen(theRecord.mColor2Green);
            theColor->mColor2.SetBlue(theRecord.mColor2Blue);
            theColor->mDarkColor.SetRed(theRecord.mColor2Red * 0.6f);
            theColor->mDarkColor.SetGreen(theRecord.mColor2Green * 0.6f);
            theColor->mDarkColor.SetBlue(theRecord.mColor2Blue * 0.6f);
            theColor->mColor4.SetRed(theRecord.mColor4Red);
            theColor->mColor4.SetGreen(theRecord.mColor4Green);
            theColor->mColor4.SetBlue(theRecord.mColor4Blue);
            EEColorTable::sTable[i] = theColor;
        }
    }
    inFile->Close();
    return theResult;
}

// FUNCTION: EXE 0x4d5e88
// Destroys every player colour and empties the table
void EEColorTable::Unload()
{
    for (unsigned long i = 0; i < EEColorTable::sTable.size(); i++)
        delete EEColorTable::sTable[i];
    EEColorTable::sTable.resize(0);
}

// FUNCTION: EXE 0x4d5ec8
// Returns the index of the player colour whose main colour equals the given one, else 0
unsigned long EEColorTable::FindIndex(const GEColor& inColor)
{
    unsigned long theResult = 0;
    for (unsigned long i = 0; i < EEColorTable::sTable.size(); i++) {
        if (EEColorTable::sTable[i]->mColor == inColor) {
            theResult = i;
            break;
        }
    }
    return theResult;
}

// SYNTHETIC: EXE 0x4d5f1c
// (the ctor, emitted out of line after its first user)

// SYNTHETIC: EXE 0x4d5f61
// (the scalar deleting destructor)

// SYNTHETIC: EXE 0x4d5d08
// SYNTHETIC: EXE 0x4d5d12
// SYNTHETIC: EXE 0x4d5d2a
// SYNTHETIC: EXE 0x4d5d36
// SYNTHETIC: EXE 0x4d5d51
// SYNTHETIC: EXE 0x4d5d5b
// SYNTHETIC: EXE 0x4d5d74
// SYNTHETIC: EXE 0x4d5d80
