// C:\EEX\Empire Earth\Database\EEColorTable.h (RECONSTRUCTION; file name inferred; provisional names).
// The player colour table of db\dbcolortable.dat (RTTI EEColorTable, no base; vtable 0x43e504, one slot: ??_G
// 0xd5f61; sizeof 0x34 from Load's new: four GEColors). EEColorTable.cpp 0xd5d08..0xd5f7e.
#ifndef EE_EECOLORTABLE_H
#define EE_EECOLORTABLE_H

#include <vector>

#include "lle/GEColor.h"

class FSFileSpec;

class EEColorTable {
public:
    EEColorTable() {}                       // 0xd5f1c (out of line: four GEColor default ctors)
    virtual ~EEColorTable() {}              // +0x00 (??_G 0xd5f61)
    static long Load(FSFileSpec* inFile);   // 0xd5d91
    static void Unload();                   // 0xd5e88
    static unsigned long FindIndex(const GEColor& inColor);   // 0xd5ec8 (0 when not found)

    GEColor mColor;                         // +0x04 (record +0x6c)
    GEColor mColor2;                        // +0x10 (record +0x78)
    GEColor mDarkColor;                     // +0x1c (record +0x78 * 0.6)
    GEColor mColor4;                        // +0x28 (record +0x84)

    // inline (out-of-line COMDATs 0x12113f / 0x121168 after their first users in EEMiniMap.cpp, cdecl): entry
    // inIndex's mDarkColor / mColor2, sColor past the table (provisional names)
    static const GEColor& GetDarkColor(unsigned long inIndex)
        { return inIndex < EEColorTable::sTable.size() ? EEColorTable::sTable[inIndex]->mDarkColor : EEColorTable::sColor; }
    static const GEColor& GetColor2(unsigned long inIndex)
        { return inIndex < EEColorTable::sTable.size() ? EEColorTable::sTable[inIndex]->mColor2 : EEColorTable::sColor; }

    // only EEColorTable.cpp sees the statics (their $E numbers would renumber the includers' chains; provisional name)
    static std::vector<EEColorTable*> sTable;   // 0x8e6108 (guard byte 0x8e6104 bit 1)
    static GEColor sColor;                      // 0x8e60f8 (guard bit 2; unread here)
};

#endif
