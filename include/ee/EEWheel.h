// EEWheel.h (RECONSTRUCTION; file name inferred). A wheel of a unit's model (RTTI EEWheel, vtable 0x449af0, sizeof
// 0x10; EEWheel.cpp 0x23c83a..0x23ceca): a sub-model whose name starts with "wheel", turned by the unit's speed.
// The unit also holds EEWheel.cpp's cdecl helpers (declared below). Names are provisional.
// Requires lle/LowLevelEngine.h (GEModel), lle/GE2DPoint.h.
#ifndef EE_EEWHEEL_H
#define EE_EEWHEEL_H

class EEComplexUnit;
class EEPlayer;
class GEModel;

class EEWheel {
public:
    EEWheel(GEModel* inModel);              // 0x23c86c (the radius: half the model's height)
    EEWheel(const EEWheel& inWheel);        // 0x23c8b1 (out of line: EEComplexUnit's wheel vector copies through it)
    // Destroys the wheel (nothing to release)
    // +0x00 0x23c8d2 (??_G 0x23c895)
    virtual ~EEWheel() {  }
    void Update(EEComplexUnit* inUnit);     // 0x23c8ff: turns the wheel by the distance the unit moved

    GEModel* mModel;                        // +0x04
    float mRadius;                          // +0x08
    float mAngle;                           // +0x0c (wrapped into -2pi..)
};

// EEWheel.cpp's cdecl helpers (provisional names; types inferred)
bool EEIsWheelModel(GEModel* inModel);      // 0x23c8d9: the model's name starts with "wheel" (case-insensitive)
long EEGetFarmType(EEPlayer* inPlayer);     // 0x23cea6: 0x33a once tech 0x3abc is researched, else 0x2a

#endif
