// C:\EEX\Low-Level Engine\Graphics Engine\GEColor.h (RECONSTRUCTION). Not included by the plug-in umbrella.
#ifndef LLE_GECOLOR_H
#define LLE_GECOLOR_H

#include "lle/GERust.h"

#include "lle/LLEForward.h"

class GEColor {                     // sizeof 0xc: red, green, blue (GetRed +0, GetGreen +4, GetBlue +8)
public:
    GEColor() : mRed(1.0f), mGreen(1.0f), mBlue(1.0f) {}   // 0xdeb3: three fld1
    GEColor(float inRed, float inGreen, float inBlue) : mRed(inRed), mGreen(inGreen), mBlue(inBlue) {}
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
    // red, green and blue from (to) the file (open or not); the first error
    template <class F> long Load(F& inFile) { return ee_ge_color_load(reinterpret_cast<float(*)[3]>(&mRed), GERustFile(inFile)); }
    template <class F> long Save(F& inFile) { return ee_ge_color_save(reinterpret_cast<const float(*)[3]>(&mRed), GERustFile(inFile)); }

    float mRed;                             // +0x00
    float mGreen;                           // +0x04
    float mBlue;                            // +0x08
};

#endif
