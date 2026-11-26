// C:\EEX\Low-Level Engine\Graphics Engine\GEOrientation.h (RECONSTRUCTION, minimal: what GETransformation's
// orientation overloads read). Yaw/pitch/roll order from GETransformation::Rotate(const GEOrientation&) 0xd828.
#ifndef LLE_GEORIENTATION_H
#define LLE_GEORIENTATION_H

#include "lle/GERust.h"

#include "lle/LLEForward.h"

class GEOrientation : public EeGeOrientation {   // the Rust record (mYaw, mPitch, mRoll; rust/base/math.h)
public:
    GEOrientation() : EeGeOrientation{0.0f, 0.0f, 0.0f} {}
    GEOrientation(float inYaw, float inPitch, float inRoll) : EeGeOrientation{inYaw, inPitch, inRoll} {}
    float GetYaw() const { return this->mYaw; }  // exported 0x10011ec0 (ICF `fld [ecx]; ret`)
    float GetPitch() const { return this->mPitch; }
    float GetRoll() const { return this->mRoll; }
    // yaw, pitch and roll from (to) an open file; kErrorNotOpen when it is not open
    template <class F> long Load(F& inFile) { return ee_ge_orientation_load(GERust(*this), GERustFile(inFile)); }
    template <class F> long Save(F& inFile) { return ee_ge_orientation_save(GERust(*this), GERustFile(inFile)); }
    void SetPitch(float inPitch) { this->mPitch = inPitch; }
    void SetRoll(float inRoll) { this->mRoll = inRoll; }
    void SetYaw(float inYaw) { this->mYaw = inYaw; }
};

#endif
