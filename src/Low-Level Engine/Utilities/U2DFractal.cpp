// C:\EEX\Low-Level Engine\Utilities\U2DFractal.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x100545ff..0x100549b6: the two ctors, the dtor, ComputeFractal (diamond-square) and the default value generator.
// No inline block, no init chains, no COMDATs; U2DSparseArray.cpp's U2DPoint key chains start at 0x100549b6.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/U2DFractal.h"

// FUNCTION: LLE 0x100545ff
// Creates an uncomputed square fractal height map of the given size
U2DFractal::U2DFractal(unsigned long inSquareDimension)
{
    this->mSquareDimension = inSquareDimension;
    this->mValues = new float*[inSquareDimension];
    for (unsigned long i = 0; i < inSquareDimension; i++)
        this->mValues[i] = new float[inSquareDimension];
}

// FUNCTION: LLE 0x10054642
// Creates a square fractal height map and fills it from a corner value, a random range and a seed
U2DFractal::U2DFractal(unsigned long inSquareDimension, float inCornerValue, float inRange, unsigned long inSeed)
{
    this->mSquareDimension = inSquareDimension;
    this->mValues = new float*[inSquareDimension];
    for (unsigned long i = 0; i < inSquareDimension; i++)
        this->mValues[i] = new float[inSquareDimension];
    this->ComputeFractal(inCornerValue, inRange, inSeed);
}

// FUNCTION: LLE 0x100546a3
// Destroys the fractal height map
U2DFractal::~U2DFractal()
{
    for (unsigned long i = 0; i < this->mSquareDimension; i++)
        delete[] this->mValues[i];
    delete[] this->mValues;
}

// FUNCTION: LLE 0x100546d2
// Fills the map by diamond-square subdivision (wrapping at the edges), the random range shrinking with the step
void U2DFractal::ComputeFractal(float inCornerValue, float inRange, unsigned long inSeed)
{
    URandom theRandom;
    theRandom.mSeed = inSeed;
    unsigned long theStep = this->mSquareDimension;
    this->mValues[0][0] = inCornerValue;
    if (theStep <= 1)
        return;
    float theDimension = (float)theStep;
    do {
        float theScale = (float)theStep / theDimension;
        unsigned long y; for (y = 0; y < this->mSquareDimension; y += theStep) {
            for (unsigned long x = 0; x < this->mSquareDimension; x += theStep) {
                unsigned long theNextX = (x + theStep) % this->mSquareDimension;
                unsigned long theNextY = (y + theStep) % this->mSquareDimension;
                float theAverage = (this->mValues[x][y] + this->mValues[theNextX][y] + this->mValues[x][theNextY]
                                    + this->mValues[theNextX][theNextY])
                                   * 0.25f;
                this->mValues[x + theStep / 2][y + theStep / 2] = this->GenerateFractalValue(theRandom, inRange, theScale, theAverage,
                                                                         x + theStep / 2, y + theStep / 2);
            }
        }
        for (y = 0; y < this->mSquareDimension; y += theStep) {
            for (unsigned long x = 0; x < this->mSquareDimension; x += theStep) {
                unsigned long theNextX = (x + theStep) % this->mSquareDimension;
                unsigned long thePreviousX = (x - theStep / 2) % this->mSquareDimension;
                unsigned long theNextY = (y + theStep) % this->mSquareDimension;
                float theAverage = (this->mValues[thePreviousX][y + theStep / 2] + this->mValues[x][theNextY]
                                    + this->mValues[x + theStep / 2][y + theStep / 2] + this->mValues[x][y])
                                   * 0.25f;
                this->mValues[x][y + theStep / 2] = this->GenerateFractalValue(theRandom, inRange, theScale, theAverage, x,
                                                               y + theStep / 2);
                unsigned long thePreviousY = (y - theStep / 2) % this->mSquareDimension;
                theAverage = (this->mValues[x + theStep / 2][thePreviousY] + this->mValues[theNextX][y] + this->mValues[x][y]
                              + this->mValues[x + theStep / 2][y + theStep / 2])
                             * 0.25f;
                this->mValues[x + theStep / 2][y] = this->GenerateFractalValue(theRandom, inRange, theScale, theAverage,
                                                               x + theStep / 2, y);
            }
        }
        theStep /= 2;
    } while (theStep > 1);
}

// FUNCTION: LLE 0x10054966
// Returns the average plus a random offset within the scaled range, clamped to 0..1
float U2DFractal::GenerateFractalValue(URandom& inRandom, float inRange, float inScale, float inAverage,
                                       unsigned long inX, unsigned long inY)
{
    float theValue = (inRange - inRandom.Generate() * (inRange + inRange)) * inScale + inAverage;
    return min(max(theValue, 0.0f), 1.0f);
}
