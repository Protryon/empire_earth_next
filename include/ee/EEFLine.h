// C:\EEX\Empire Earth\Formations\EEFLine.h (RECONSTRUCTION; file name inferred; provisional names). A
// line of units (RTTI EEFLine > EEFormation; vtable 0x43f868; sizeof 0x74).
// Requires ee/EEFormation.h.
#ifndef EE_EEFLINE_H
#define EE_EEFLINE_H

class EEFLine : public EEFormation {
public:
    EEFLine(long inType, float inSpacing, float inDepth, long inCount, long inRows);   // 0xdd8b2
    virtual ~EEFLine();                     // 0xdd982 (??_G 0xdd966)
    virtual void Place(std::vector<EEComplexUnit*>& inUnits, std::vector<EEFormationSlot>& outSlots);   // 0xdd9a3

    float* mOffsets;                        // +0x60 (new float[mCount]: 0, then -x/+x pairs)
    float mSpacing;                         // +0x64
    float mDepth;                           // +0x68
    long mCount;                            // +0x6c
    long mRows;                             // +0x70 (inRows + 1)
};

#endif
