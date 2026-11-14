// Low-Level Engine U2DFractal (exported class), as seen by its includers. Member names are provisional.
// Requires lle/LowLevelEngine.h. Layout proven by LLE's exported members (ctor 0x100545ff, dtor 0x100546a3,
// GetValue 0x10012519) and Default.dll's HMUFractal (vtable 0x10019284: dtor, ComputeFractal, override of slot 2).
#ifndef LLE_U2DFRACTAL_H
#define LLE_U2DFRACTAL_H

class LLE_API U2DFractal {                  // proven sizeof 0xc (HMUFractal adds +0xc)
public:
    U2DFractal(unsigned long inSquareDimension);
    U2DFractal(unsigned long inSquareDimension, float inCornerValue, float inRange, unsigned long inSeed);
    virtual ~U2DFractal();

    virtual void ComputeFractal(float inCornerValue, float inRange, unsigned long inSeed);   // slot 1 (0x546d2)
    float GetValue(unsigned long inX, unsigned long inY) { return this->mValues[inX][inY]; }

protected:
    virtual float GenerateFractalValue(URandom& inRandom, float inRange, float inScale, float inAverage,
                                       unsigned long inX, unsigned long inY);         // slot 2

    float** mValues;                        // proven +0x04 (GetValue; one row per dimension)
    unsigned long mSquareDimension;         // proven +0x08
};

#endif
