// C:\EEX\Empire Earth\Formations\EEFCircle.h (RECONSTRUCTION; file name inferred; provisional names). A
// ring of units (RTTI EEFCircle > EEFormation; vtable 0x43f828; sizeof 0x78).
// Requires ee/EEFormation.h.
#ifndef EE_EEFCIRCLE_H
#define EE_EEFCIRCLE_H

class EEFCircle : public EEFormation {
public:
    EEFCircle(long inMode, long inType);    // 0xdd160
    // 0xdd1be (out of line: the ??_G 0xdd1a2 and the derived dtors call it; ICF-shared with ~EEFWedge)
    virtual ~EEFCircle();
    virtual void Place(std::vector<EEComplexUnit*>& inUnits, std::vector<EEFormationSlot>& outSlots);   // 0xdd1cc

    float mHalfArc;                         // +0x60 (= pi)
    float mF64;                             // +0x64
    float mF68;                             // +0x68
    void SetMinRadius(float inRadius) { this->mMinRadius = inRadius; }   // inline: EEHerd 0xa4351/0xa1efc store it after the argument evaluation
    float mMinRadius;                       // +0x6c (Place's lower bound of the radius)
    long mMode;                             // +0x70 (0, 1 inward, 2 outward: Place's heading switch)
    bool mB74;                              // +0x74
};

#endif
