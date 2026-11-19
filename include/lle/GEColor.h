// C:\EEX\Low-Level Engine\Graphics Engine\GEColor.h (RECONSTRUCTION). Not included by the plug-in umbrella.
#ifndef LLE_GECOLOR_H
#define LLE_GECOLOR_H

#include "lle/GERust.h"

class FSFile;

class GEColor {                     // sizeof 0xc: red, green, blue (GetRed +0, GetGreen +4, GetBlue +8)
public:
    GEColor() : mRed(1.0f), mGreen(1.0f), mBlue(1.0f) {}   // 0xdeb3: three fld1
    GEColor(float inRed, float inGreen, float inBlue) : mRed(inRed), mGreen(inGreen), mBlue(inBlue) {}
    ~GEColor() {}
    GEColor& operator=(const GEColor& inColor)
        { this->mRed = inColor.mRed; this->mGreen = inColor.mGreen; this->mBlue = inColor.mBlue; return *this; }
    bool operator==(const GEColor& inColor) const
        { return this->mRed == inColor.mRed && this->mGreen == inColor.mGreen && this->mBlue == inColor.mBlue; }
    bool operator!=(const GEColor& inColor) const
        { return this->mRed != inColor.mRed || this->mGreen != inColor.mGreen || this->mBlue != inColor.mBlue; }
    float GetRed() const { return this->mRed; }
    float GetGreen() const { return this->mGreen; }
    float GetBlue() const { return this->mBlue; }
    void SetRed(float inRed) { this->mRed = inRed; }
    void SetGreen(float inGreen) { this->mGreen = inGreen; }
    void SetBlue(float inBlue) { this->mBlue = inBlue; }
    // 0xdec4: clamps each channel to [0, 1]
    void Normalize()
    {
        this->mRed = this->mRed > 1.0f ? 1.0f : (this->mRed < 0.0f ? 0.0f : this->mRed);
        this->mGreen = this->mGreen > 1.0f ? 1.0f : (this->mGreen < 0.0f ? 0.0f : this->mGreen);
        this->mBlue = this->mBlue > 1.0f ? 1.0f : (this->mBlue < 0.0f ? 0.0f : this->mBlue);
    }
    // red, green and blue from (to) the file (open or not); the first error
    template <class F> long Load(F& inFile) { const float* f[] = {&mRed, &mGreen, &mBlue}; return GERustFields(inFile, f, 3, false, false); }
    template <class F> long Save(F& inFile) { const float* f[] = {&mRed, &mGreen, &mBlue}; return GERustFields(inFile, f, 3, false, true); }

    float mRed;                             // +0x00
    float mGreen;                           // +0x04
    float mBlue;                            // +0x08
};

#endif
