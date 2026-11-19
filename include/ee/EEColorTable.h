// C:\EEX\Empire Earth\Database\EEColorTable.h (RECONSTRUCTION; file name inferred; provisional names).
// The player colour table of db\dbcolortable (the original: RTTI EEColorTable, a vector of objects of four GEColors).
// Since the game's leaves round 2 the table is ee_data's list (EeDataList::Colors, tables.rs: the record's copy, the
// colours, FindIndex); the class is its static readers. Requires ee/EEDbRecords.h for Load.
#ifndef EE_EECOLORTABLE_H
#define EE_EECOLORTABLE_H

#include "lle/GEColor.h"
#include "rust/data/tables.h"

class EEColorTable {
public:
    // the colours of a player colour: the main, the second, its 60% dark shade, the fourth
    enum { kMain = 0, kSecond, kDark, kFourth };
    // Loads the player colours from db\dbcolortable; returns the last read status
    template <class F> static long Load(F* inFile) { return EEDbLoadList(inFile, EeDataList::Colors); }
    static void Unload() { ee_data_list_clear(EeDataList::Colors); }
    // the index of the player colour whose main colour equals inColor, else 0
    static unsigned long FindIndex(const GEColor& inColor)
        { return (unsigned long)ee_data_player_color_find(inColor.mRed, inColor.mGreen, inColor.mBlue); }
    // colour inWhich of player colour inIndex (white, GEColor(), past the table)
    static GEColor Get(unsigned long inIndex, unsigned int inWhich)
    {
        float theColor[3];
        ee_data_player_color(inIndex, inWhich, theColor);
        return GEColor(theColor[0], theColor[1], theColor[2]);
    }
    static GEColor GetColor(unsigned long inIndex) { return EEColorTable::Get(inIndex, kMain); }
    static GEColor GetDarkColor(unsigned long inIndex) { return EEColorTable::Get(inIndex, kDark); }
    static GEColor GetColor2(unsigned long inIndex) { return EEColorTable::Get(inIndex, kSecond); }
};

#endif
