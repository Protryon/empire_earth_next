// C:\EEX\Empire Earth\User Interface Thread\EEUICWonNetStatBox.h (RECONSTRUCTION; file name inferred). The WON
// statistics lists (RTTI EEUICWonNetStatBox > UIListBox<EEUICWNSBStat>; vtable 0x44a470: one new virtual, slot 0xbc
// 0x25eca2). Names are provisional. Requires lle/UIListBox.h.
#ifndef EE_EEUICWONNETSTATBOX_H
#define EE_EEUICWONNETSTATBOX_H

// one line (sizeof 0x34: GetNumLines 0x25f4c9 divides by 0x34). The vector's _Tidy 0x25f7de (the list box's
// +0xa0/+0xb0 and the stat box's +0x138) runs ~UWideString at +0 of each element and nothing else: a string, then
// 0x20 trivially destructible bytes (not decoded)
struct EEUICWNSBStat {
    UWideString mText;                      // +0x00
    // eight dwords (the copy assignment 0x25f501 moves them one by one; DrawTextA 0x25e413 prints +0x14..+0x2c with
    // "%ld", the percentages +0x20/+0x24/+0x28 over +0x1c and the time +0x30 / +0x1c: fild qword = unsigned)
    unsigned long mL14;                     // +0x14
    unsigned long mL18;                     // +0x18
    unsigned long mNumGames;                // +0x1c (the divisor)
    unsigned long mL20;                     // +0x20
    unsigned long mL24;                     // +0x24
    unsigned long mL28;                     // +0x28
    long mL2C;                              // +0x2c (Refresh: -6)
    unsigned long mTotalTime;               // +0x30 (seconds times mL1C)
};

class EEUICWonNetStatBox : public UIListBox<EEUICWNSBStat> {
public:
    EEUICWonNetStatBox(unsigned long inID, GEFont* inFont, const GE2DPoint& inLocation, unsigned long inWidth, unsigned long inHeight, UIForm* inForm, unsigned long inIndex, unsigned long inBorder); // original25e2f8/274b38 ret32, no body credit
    // 0x25e2f8 (ret 0x20): UIListBox's ctor 0x25ed50, mStats' allocator byte, then vptr 0x44a470, mRowHeight, mText
    virtual ~EEUICWonNetStatBox();          // 0x25e3cb (??_G 0x25e3af)
    virtual long CopyData();                // +0x14 0x25e3f9 (the base's; clears mCopiedHasSelection under two lines)
    virtual long DrawTextA();               // +0x1c 0x25e413 (twelve columns per copied line)
    virtual void Refresh();                 // +0xbc 0x25eca2 (the stats form's open event; provisional name)

    GEText* mText;                          // +0x134 (ctor: new GEText(argument 2, the font); dtor ??_G)
    std::vector<EEUICWNSBStat> mStats;      // +0x138 (dtor 0x25f7de, the list box's own vector type)
    unsigned long mRowHeight;               // +0x148 (ctor: argument 2's +0x2c)
    // sizeof 0x14c (its `new`, sizes.txt)
};

#endif
