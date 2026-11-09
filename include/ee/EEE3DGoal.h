// C:\EEX\Empire Earth\Graphics Thread\EEE3DGoal.h (RECONSTRUCTION; file name inferred).
// A goal icon with a countdown label (vtable 0x44080c; RTTI EEE3DGoal > EEE3DIcon; EEE3DGoal.cpp 0xf1327..0xf1600:
// the key chain "EEE3DGoal-Data", both ctors, Load, the label draw, Save). sizeof 0x11c. Names are provisional.
// Requires ee/EEE3DIcon.h.
#ifndef EE_EEE3DGOAL_H
#define EE_EEE3DGOAL_H

class GE2DPoint;

// 0x118676 (cdecl, in the render objects' range): the screen point of the world point inPoint (the current camera
// transformation, then the viewport projection) (provisional name; types inferred)
void GetScreenPoint(const GE3DPoint& inPoint, GE2DPoint& outPoint);

class EEE3DGoal : public EEE3DIcon {
public:
    EEE3DGoal(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, const GE3DPoint* inLocation);   // 0xf136a (kind 0x3c)
    EEE3DGoal();                            // 0xf1393
    virtual long DrawPass3();               // +0x0c 0xf13db (the countdown label over the icon)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);    // +0x38 0xf13ae
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // +0x40 0xf15d0

    static const UString sData;             // "EEE3DGoal-Data" (0x4e8bb8; provisional name)

    unsigned long mTimeLeft;                // +0x118 (saved; the label shows it as minutes:seconds)
};

#endif
