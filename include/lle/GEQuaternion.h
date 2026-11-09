// C:\EEX\Low-Level Engine\Graphics Engine\GEQuaternion.h (RECONSTRUCTION). GEQuaternion.cpp (~0x10023219) holds the
// conversions, Normalize and Slerp; the rest is inline (GEBoundingBox.cpp emits it first, 0x10011ddf..).
#ifndef LLE_GEQUATERNION_H
#define LLE_GEQUATERNION_H

class LLE_API GEQuaternion {                // sizeof 0x10: x, y, z, w
public:
    GEQuaternion()
    {
        this->mW = 1.0f;
        this->mX = 0.0f;
        this->mY = 0.0f;
        this->mZ = 0.0f;
    }
    GEQuaternion(const GEQuaternion& inQuaternion) { *this = inQuaternion; }   // ICF-folded with operator=
    GEQuaternion(const GETransformation& inTransformation) { this->ConvertFrom(inTransformation); }
    ~GEQuaternion() {}
    GEQuaternion& operator=(const GEQuaternion& inQuaternion)
    {
        this->mW = inQuaternion.mW;
        this->mX = inQuaternion.mX;
        this->mY = inQuaternion.mY;
        this->mZ = inQuaternion.mZ;
        return *this;
    }
    bool operator==(const GEQuaternion& inQuaternion) const
        { return this->mW == inQuaternion.mW && this->mX == inQuaternion.mX && this->mY == inQuaternion.mY && this->mZ == inQuaternion.mZ; }
    bool operator!=(const GEQuaternion& inQuaternion) const
        { return this->mW != inQuaternion.mW || this->mX != inQuaternion.mX || this->mY != inQuaternion.mY || this->mZ != inQuaternion.mZ; }
    GEQuaternion& operator+=(const GEQuaternion& inQuaternion)
    {
        this->mW += inQuaternion.mW;
        this->mX += inQuaternion.mX;
        this->mY += inQuaternion.mY;
        this->mZ += inQuaternion.mZ;
        return *this;
    }
    void ConvertFrom(const GETransformation& inTransformation);
    void ConvertTo(GETransformation& outTransformation) const;
    float GetW() const { return this->mW; }
    float GetX() const { return this->mX; }
    float GetY() const { return this->mY; }
    float GetZ() const { return this->mZ; }
    void Normalize();
    void Slerp(const GEQuaternion& inTarget, float inT, GEQuaternion& outQuaternion) const;
private:
    friend struct GERustLayout;             // the Rust mirror's layout check (Graphics Engine/GERustMath.h)
    float mX;                               // +0x00
    float mY;                               // +0x04
    float mZ;                               // +0x08
    float mW;                               // +0x0c
};

#endif
