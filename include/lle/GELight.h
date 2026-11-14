// C:\EEX\Low-Level Engine\Graphics Engine\GELight.h (RECONSTRUCTION). Everything is inline except the members in
// GELight.cpp (Attenuation0 accessors); GEBoundingBox.cpp emits the block first (0x10010858..0x10010af2).
#ifndef LLE_GELIGHT_H
#define LLE_GELIGHT_H

class LLE_API GELight {                     // sizeof 0x74 (implicit copy ctor 0x10a5d)
public:
    GELight()
    {
        this->mStyle = 0;
        this->mIntensity = 1.0f;
        this->mRange = 0.0f;
        this->mAttenuation0 = 1.0f;
        this->mAttenuation1 = 0.0f;
        this->mAttenuation2 = 0.0f;
    }
    ~GELight() {}
    // 0x108d1: copies the defining state only (the computed and model-space members are derived); the intensity is
    // copied twice
    GELight& operator=(const GELight& inLight)
    {
        this->mColor = inLight.mColor;
        this->mLocation = inLight.mLocation;
        this->mDirection = inLight.mDirection;
        this->mIntensity = inLight.mIntensity;
        this->mAttenuation0 = inLight.mAttenuation0;
        this->mAttenuation1 = inLight.mAttenuation1;
        this->mAttenuation2 = inLight.mAttenuation2;
        this->mIntensity = inLight.mIntensity;
        this->mRange = inLight.mRange;
        this->mStyle = inLight.mStyle;
        return *this;
    }
#if defined(LLE_CURRENT_HEADERS) && !defined(LOWLEVELENGINE_EXPORTS)
    // Inferred: the renderers saw a newer header with this getter inline (DX7HRDisplay.dll imports no GELight
    // member, and DX7Rasterizer::UpdateLight 0x1000cbe5 reads +0x48 directly)
    float GetAttenuation0() const { return this->mAttenuation0; }
#else
    float GetAttenuation0() const;
#endif
    float GetAttenuation1() const { return this->mAttenuation1; }
    float GetAttenuation2() const { return this->mAttenuation2; }
    const GEColor& GetColor() const { return this->mColor; }
    const GEColor& GetComputedColor() const { return this->mComputedColor; }
    const GEVector& GetDirection() const { return this->mDirection; }
    const GE3DPoint& GetLocation() const { return this->mLocation; }
    float GetRange() const { return this->mRange; }
    unsigned long GetStyle() const { return this->mStyle; }
#ifdef LLE_EXE_HEADERS
    // inline in the exe's view: the exe never imports it and stores +0x48 directly (EERenderObject 0x10ae74)
    void SetAttenuation0(float inAttenuation) { this->mAttenuation0 = inAttenuation; }
#else
    void SetAttenuation0(float inAttenuation);
#endif
    void SetAttenuation1(float inAttenuation) { this->mAttenuation1 = inAttenuation; }
    void SetAttenuation2(float inAttenuation) { this->mAttenuation2 = inAttenuation; }
    void SetColor(const GEColor& inColor)
    {
        this->mColor = inColor;
        this->mColor.Normalize();
        this->mComputedColor.SetRed(inColor.GetRed() * this->mIntensity);
        this->mComputedColor.SetGreen(inColor.GetGreen() * this->mIntensity);
        this->mComputedColor.SetBlue(inColor.GetBlue() * this->mIntensity);
    }
    void SetDirection(const GEVector& inDirection)
    {
        this->mDirection.SetX(-inDirection.GetX());
        this->mDirection.SetY(-inDirection.GetY());
        this->mDirection.SetZ(-inDirection.GetZ());
        this->mDirection.Normalize();
    }
    void SetIntensity(float inIntensity)
    {
        this->mIntensity = inIntensity;
        this->mComputedColor.SetRed(this->mColor.GetRed() * inIntensity);
        this->mComputedColor.SetGreen(this->mColor.GetGreen() * inIntensity);
        this->mComputedColor.SetBlue(this->mColor.GetBlue() * inIntensity);
    }
    void SetLocation(const GE3DPoint& inLocation) { this->mLocation = inLocation; }
    void SetRange(float inRange)
    {
        this->mRange = inRange;
        this->mRangeSquared = inRange * inRange;
        this->mRangeInverse = inRange == 0.0f ? 0.0f : 1.0f / inRange;
    }
    void SetStyle(unsigned long inStyle) { this->mStyle = inStyle; }
private:
    GEColor mColor;                         // +0x00
    GEColor mComputedColor;                 // +0x0c color * intensity
    GE3DPoint mLocation;                    // +0x18
    GE3DPoint mModelSpaceLocation;          // +0x24
    GEVector mDirection;                    // +0x30 stored negated, normalized
    GEVector mModelSpaceDirection;          // +0x3c
    float mAttenuation0;                    // +0x48
    float mAttenuation1;                    // +0x4c
    float mAttenuation2;                    // +0x50
    float mIntensity;                       // +0x54
    float mRange;                           // +0x58
    float mRangeInverse;                    // +0x5c
    float mRangeSquared;                    // +0x60
    float mModelSpaceRange;                 // +0x64
    float mModelSpaceRangeInverse;          // +0x68
    float mModelSpaceRangeSquared;          // +0x6c
    unsigned long mStyle;                   // +0x70
};

#endif
