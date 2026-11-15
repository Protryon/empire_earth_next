// C:\EEX\Empire Earth\Formations\EEFWedge.h (RECONSTRUCTION; file name inferred; provisional names). A
// wedge of units (RTTI EEFWedge > EEFormation; vtable 0x43f914; sizeof 0x84).
// Requires ee/EEFormation.h.
#ifndef EE_EEFWEDGE_H
#define EE_EEFWEDGE_H

class EEFWedge : public EEFormation {
public:
    EEFWedge(long inType, float inSpacing, float inAngle, unsigned long inCount);   // 0xddf3a
    // out of line (ICF-folded into ~EEFCircle 0xdd1be; ~EEFDeepWedge 0xdd8a7 / ~EEFShallowWedge 0xdde89 jump to it)
    virtual ~EEFWedge();
    virtual void Place(std::vector<EEComplexUnit*>& inUnits, std::vector<EEFormationSlot>& outSlots);   // 0xde019

    // GEVectors: Place passes &+0x60 / &+0x6c straight to GETransformation::Apply, the ctor zeroes them through the
    // exe-view GEVector() (six fldz stores)
    GEVector mLeftArm;                      // +0x60 (the left arm's direction: -x, -y)
    GEVector mRightArm;                     // +0x6c (the right arm: x, -y)
    float mSpacing;                         // +0x78
    float mRowDepth;                        // +0x7c (inCount * inSpacing / 2, at most inSpacing / tan(inAngle / 2))
    unsigned long mCount;                   // +0x80
};

#endif
