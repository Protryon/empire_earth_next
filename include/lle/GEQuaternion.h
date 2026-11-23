// C:\EEX\Low-Level Engine\Graphics Engine\GEQuaternion.h (RECONSTRUCTION). GEQuaternion.cpp (~0x10023219) holds the
// conversions, Normalize and Slerp; the rest is inline (GEBoundingBox.cpp emits it first, 0x10011ddf..).
#ifndef LLE_GEQUATERNION_H
#define LLE_GEQUATERNION_H
#include "lle/GERust.h"

class GEQuaternion : public EeGeQuaternion {   // the Rust record (mX, mY, mZ, mW; rust/base/math.h)
public:
    GEQuaternion() : EeGeQuaternion{0.0f, 0.0f, 0.0f, 1.0f} {}
    GEQuaternion(const GETransformation& inTransformation) : EeGeQuaternion{} { this->ConvertFrom(inTransformation); }
    bool operator==(const GEQuaternion& inQuaternion) const
        { return this->mW == inQuaternion.mW && this->mX == inQuaternion.mX && this->mY == inQuaternion.mY && this->mZ == inQuaternion.mZ; }
    bool operator!=(const GEQuaternion& inQuaternion) const
        { return this->mW != inQuaternion.mW || this->mX != inQuaternion.mX || this->mY != inQuaternion.mY || this->mZ != inQuaternion.mZ; }
    GEQuaternion& operator+=(const GEQuaternion& inQuaternion)   // component by component
    {
        static_cast<EeGeQuaternion&>(*this) = EeGeQuaternion{this->mX + inQuaternion.mX, this->mY + inQuaternion.mY,
                                                             this->mZ + inQuaternion.mZ, this->mW + inQuaternion.mW};
        return *this;
    }
    // from a transformation's rotation (the vector part negated), normalised
    void ConvertFrom(const GETransformation& inTransformation) { ee_ge_quat_from(GERust(inTransformation), GERust(*this)); }
    float GetX() const { return this->mX; }
    float GetY() const { return this->mY; }
    float GetZ() const { return this->mZ; }
    void Normalize() { ee_ge_quat_normalize(GERust(*this)); }   // to unit length
};

#endif
