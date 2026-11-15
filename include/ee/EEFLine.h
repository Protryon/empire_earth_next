// C:\EEX\Empire Earth\Formations\EEFLine.h (RECONSTRUCTION; file name inferred; provisional names). A
// line of units (RTTI EEFLine > EEFormation; vtable 0x43f868; sizeof 0x74).
// Requires ee/EEFormation.h.
#ifndef EE_EEFLINE_H
#define EE_EEFLINE_H

class EEFLine : public EEFormation {
public:
    EEFLine(long inType, float inSpacing, float inDepth, long inCount, long inRows);
    virtual ~EEFLine();
    virtual void Place(std::vector<EEComplexUnit*>& inUnits, std::vector<EEFormationSlot>& outSlots);

    float* mOffsets;                        // +0x60 (new float[mCount]: 0, then -x/+x pairs)
    float mSpacing;                         // +0x64
    float mDepth;                           // +0x68
    long mCount;                            // +0x6c
    long mRows;                             // +0x70 (inRows + 1)
};

#endif
