// C:\EEX\Low-Level Engine\Graphics Engine\GEOrientation.h (RECONSTRUCTION, minimal: what GETransformation's
// orientation overloads read). Yaw/pitch/roll order from GETransformation::Rotate(const GEOrientation&) 0xd828.
#ifndef LLE_GEORIENTATION_H
#define LLE_GEORIENTATION_H

#include "lle/GERust.h"

class FSFile;

class GEOrientation {                       // sizeof 0xc
public:
    GEOrientation() : mYaw(0.0f), mPitch(0.0f), mRoll(0.0f) {}
    GEOrientation(float inYaw, float inPitch, float inRoll) : mYaw(inYaw), mPitch(inPitch), mRoll(inRoll) {}
    GEOrientation(const GEOrientation& inOrientation)
        : mYaw(inOrientation.mYaw), mPitch(inOrientation.mPitch), mRoll(inOrientation.mRoll) {}
    ~GEOrientation() {}
    GEOrientation& operator=(const GEOrientation& inOrientation)
        { this->mYaw = inOrientation.mYaw; this->mPitch = inOrientation.mPitch; this->mRoll = inOrientation.mRoll; return *this; }
    float GetYaw() const { return this->mYaw; }  // exported 0x10011ec0 (ICF `fld [ecx]; ret`)
    float GetPitch() const { return this->mPitch; }
    float GetRoll() const { return this->mRoll; }
    // yaw, pitch and roll from (to) an open file; kErrorNotOpen when it is not open
    template <class F> long Load(F& inFile) { return ee_ge_orientation_load(GERust(*this), GERustFile(inFile)); }
    void Normalize() { ee_ge_orientation_normalize(GERust(*this)); }   // each angle into -pi..pi
    template <class F> long Save(F& inFile) { return ee_ge_orientation_save(GERust(*this), GERustFile(inFile)); }
    void SetPitch(float inPitch) { this->mPitch = inPitch; }
    void SetRoll(float inRoll) { this->mRoll = inRoll; }
    void SetYaw(float inYaw) { this->mYaw = inYaw; }

    float mYaw;                             // +0x00
    float mPitch;                           // +0x04
    float mRoll;                            // +0x08
};

#endif
