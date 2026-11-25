// C:\EEX\Low-Level Engine\Graphics Engine\GETextureCoordinate.h (RECONSTRUCTION).
#ifndef LLE_GETEXTURECOORDINATE_H
#define LLE_GETEXTURECOORDINATE_H

#include "lle/GERust.h"

#include "lle/LLEForward.h"

class GETextureCoordinate {         // sizeof 8: u, v
public:
    GETextureCoordinate() : mU(0.0f), mV(0.0f) {}
    GETextureCoordinate(float inU, float inV) : mU(inU), mV(inV) {}
    bool operator==(const GETextureCoordinate& inCoordinate) const
        { return this->mU == inCoordinate.mU && this->mV == inCoordinate.mV; }
    bool operator!=(const GETextureCoordinate& inCoordinate) const
        { return this->mU != inCoordinate.mU || this->mV != inCoordinate.mV; }
    float GetU() const { return this->mU; }
    float GetV() const { return this->mV; }
    void SetU(float inU) { this->mU = inU; }
    void SetV(float inV) { this->mV = inV; }

    float mU;                               // +0x00
    float mV;                               // +0x04
};

#endif
