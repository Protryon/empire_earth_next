// Low-Level Engine U2DFractal (exported class), as seen by its includers. Member names are provisional.
// Requires lle/LowLevelEngine.h. A square height map filled by diamond-square subdivision.
//
// Whole in Rust (rust/crates/ee_util fractal.rs, LLE lane round 7): the class holds the map's pointer and forwards.
// The original's virtual value generator was overridden only by Default.dll's HMUFractal, which is Rust's (ee_rmap
// fractal.rs): nothing derives from this class now, so its vtable went. Values not computed are 0 (they were
// uninitialised).
#ifndef LLE_U2DFRACTAL_H
#define LLE_U2DFRACTAL_H

#include "rust/ee_util.h"

class LLE_API U2DFractal {
public:
    // an uncomputed map
    U2DFractal(unsigned long inSquareDimension) : mFractal(ee_util_fractal_new(inSquareDimension)) {}
    // a map computed from a corner value, a random range and a seed
    U2DFractal(unsigned long inSquareDimension, float inCornerValue, float inRange, unsigned long inSeed)
        : mFractal(ee_util_fractal_new(inSquareDimension))
    {
        this->ComputeFractal(inCornerValue, inRange, inSeed);
    }
    ~U2DFractal() { ee_util_fractal_free(this->mFractal); }
    U2DFractal(const U2DFractal&) = delete;
    U2DFractal& operator=(const U2DFractal&) = delete;

    // diamond-square subdivision wrapping at the edges, the random range shrinking with the step; each value the
    // average plus a random offset within the scaled range, clamped to 0..1
    void ComputeFractal(float inCornerValue, float inRange, unsigned long inSeed)
        { ee_util_fractal_compute(this->mFractal, inCornerValue, inRange, inSeed); }
    float GetValue(unsigned long inX, unsigned long inY) { return ee_util_fractal_value(this->mFractal, inX, inY); }

private:
    EeUtilFractal* mFractal;
};

#endif
