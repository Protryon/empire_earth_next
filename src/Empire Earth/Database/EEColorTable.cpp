// C:\EEX\Empire Earth\Database\EEColorTable.cpp (name from RTTI EEColorTable; no __FILE__). 0xd5d08..0xd5f7e: the
// two class statics' init chains, Load (db\dbcolortable.dat), Unload, FindIndex, the ctor, ??_G. EEButtonObject.cpp
// ends at 0xd5d08, EECPBehaviorTable.cpp's array chain starts at 0xd5f7e. Unit flags: the exe's module set.

#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"

#include "ee/EEColorTable.h"
#include "ee/EEDbRecords.h"

std::vector<EEColorTable*> EEColorTable::sTable;
GEColor EEColorTable::sColor;

// Loads the player colours from db\dbcolortable.dat (main, second with a 60% dark shade, fourth: the record and its
// copy are ee_data tables.rs); returns the last read status
long EEColorTable::Load(FSFileSpec* inFile)
{
    return EEDbReadRecords(inFile, ee_data_color_record_size(),
        [](unsigned long inCount) { EEColorTable::sTable.resize(inCount, NULL); return true; },
        [](unsigned long inIndex, const unsigned char* inRecord) {
            EEColorTable* theColor = new EEColorTable;
            EeDataPlayerColors c;
            ee_data_color_from_record(inRecord, &c);
            GEColor* theOut[4] = {&theColor->mColor, &theColor->mColor2, &theColor->mDarkColor, &theColor->mColor4};
            for (int k = 0; k < 4; k++) {
                theOut[k]->SetRed(c.colors[k][0]);
                theOut[k]->SetGreen(c.colors[k][1]);
                theOut[k]->SetBlue(c.colors[k][2]);
            }
            EEColorTable::sTable[inIndex] = theColor;
        });
}

// Destroys every player colour and empties the table
void EEColorTable::Unload()
{
    for (unsigned long i = 0; i < EEColorTable::sTable.size(); i++)
        delete EEColorTable::sTable[i];
    EEColorTable::sTable.resize(0);
}

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
