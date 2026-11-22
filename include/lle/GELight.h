// C:\EEX\Low-Level Engine\Graphics Engine\GELight.h (RECONSTRUCTION). A light whole in Rust (LLE round 9): the class
// holds the Rust record (rust/crates/ee_render/src/ge light.rs, EeGeLight: the original's members in their order) and
// forwards; the DX7 rasterizer reads the record directly.
#ifndef LLE_GELIGHT_H
#define LLE_GELIGHT_H

#include "rust/render/ge.h"

class LLE_API GELight {                     // sizeof 0x74
public:
    GELight() { ee_ge_light_init(&this->mRust); }
    GELight& operator=(const GELight& inLight) { ee_ge_light_assign(&this->mRust, &inLight.mRust); return *this; }
    float GetAttenuation0() const { return this->mRust.attenuation0; }
    float GetAttenuation1() const { return this->mRust.attenuation1; }
    float GetAttenuation2() const { return this->mRust.attenuation2; }
    const GEColor& GetColor() const { return *reinterpret_cast<const GEColor*>(this->mRust.color); }
    const GEColor& GetComputedColor() const { return *reinterpret_cast<const GEColor*>(this->mRust.computed_color); }
    const GEVector& GetDirection() const { return *reinterpret_cast<const GEVector*>(this->mRust.direction); }
    const GE3DPoint& GetLocation() const { return *reinterpret_cast<const GE3DPoint*>(this->mRust.location); }
    float GetRange() const { return this->mRust.range; }
    unsigned long GetStyle() const { return this->mRust.style; }
    void SetAttenuation0(float inAttenuation) { this->mRust.attenuation0 = inAttenuation; }
    void SetAttenuation1(float inAttenuation) { this->mRust.attenuation1 = inAttenuation; }
    void SetAttenuation2(float inAttenuation) { this->mRust.attenuation2 = inAttenuation; }
    void SetColor(const GEColor& inColor) { ee_ge_light_set_color(&this->mRust, reinterpret_cast<const float(*)[3]>(&inColor)); }
    void SetDirection(const GEVector& inDirection)
        { ee_ge_light_set_direction(&this->mRust, reinterpret_cast<const float(*)[3]>(&inDirection)); }
    void SetIntensity(float inIntensity) { ee_ge_light_set_intensity(&this->mRust, inIntensity); }
    void SetStyle(unsigned long inStyle) { this->mRust.style = inStyle; }
private:
    EeGeLight mRust;
};

#endif
