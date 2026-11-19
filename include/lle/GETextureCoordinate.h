// C:\EEX\Low-Level Engine\Graphics Engine\GETextureCoordinate.h (RECONSTRUCTION).
#ifndef LLE_GETEXTURECOORDINATE_H
#define LLE_GETEXTURECOORDINATE_H

#include "lle/GERust.h"

class FSFileSpec;

class GETextureCoordinate {         // sizeof 8: u, v
public:
    GETextureCoordinate() : mU(0.0f), mV(0.0f) {}
    GETextureCoordinate(float inU, float inV) : mU(inU), mV(inV) {}
    ~GETextureCoordinate() {}
    GETextureCoordinate& operator=(const GETextureCoordinate& inCoordinate)
        { this->mU = inCoordinate.mU; this->mV = inCoordinate.mV; return *this; }
    bool operator==(const GETextureCoordinate& inCoordinate) const
        { return this->mU == inCoordinate.mU && this->mV == inCoordinate.mV; }
    bool operator!=(const GETextureCoordinate& inCoordinate) const
        { return this->mU != inCoordinate.mU || this->mV != inCoordinate.mV; }
    float GetU() const { return this->mU; }
    float GetV() const { return this->mV; }
    void SetU(float inU) { this->mU = inU; }
    void SetV(float inV) { this->mV = inV; }
    // U and V from (to) an open file (an FSFileSpec); on a failure the position goes back to where it was
    template <class F> long Load(F& inFile) { const float* f[] = {&mU, &mV}; return GERustFields(inFile, f, 2, true, false, true); }
    template <class F> long Save(F& inFile) { const float* f[] = {&mU, &mV}; return GERustFields(inFile, f, 2, true, true, true); }

    float mU;                               // +0x00
    float mV;                               // +0x04
};

#endif
