// C:\EEX\Empire Earth\Graphics Thread\EEE3DIcon.h (RECONSTRUCTION; file name inferred).
// An animated 3D icon over a unit or at a point: one model part of the graphic, its orbiting parts, a scale that
// pulses between the graphic effect's +0x20 and its height, a scrolling texture (vtable 0x4408d8; RTTI EEE3DIcon >
// EEEffect3D; EEE3DIcon.cpp 0xf2046..0xf2d93; EEE3DGoal derives from it). sizeof 0x118. Names are provisional.
// Requires ee/EEEffect3D.h, ee/EEGraphicObject.h.
#ifndef EE_EEE3DICON_H
#define EE_EEE3DICON_H

#include <vector>

class EEGraphicObject;
class EEOrbital;

class EEE3DIcon : public EEEffect3D {
public:
    // 0xf2046: inLocation 0: follows the unit
    EEE3DIcon(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, const GE3DPoint* inLocation, long inKind);
    EEE3DIcon(long inKind);                 // 0xf210e (the load-time ctor)
    virtual ~EEE3DIcon();                   // +0x00 0xf2177 (??_G 0xf20f2)
    virtual long DrawPass2();               // +0x08 (`return 0`: ICF-folded into 0xfca98)
    virtual long DrawPass4();               // +0x10 (likewise)
    virtual long DrawPass5();               // +0x14 (likewise)
    virtual long DrawPass6();               // +0x18 0xf2658 (the draw)
    virtual long Queue(EEEffectQueue* inQueues);       // +0x24 0xf21e5 (queue 4, by texture)
    virtual void PrepareQueue();            // +0x2c (empty: ICF-folded into 0xe123d)
    virtual long SetUp();                   // +0x30 0xf23e2 (loads the graphic, copies its orbitals)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);    // +0x38 0xf24e3
    virtual long RepairPointers();          // +0x3c 0xf2536
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // +0x40 0xf2a57
    virtual long Step();                    // +0x4c 0xf2aaa (animation, scale, scroll; 7 past the duration)
    virtual void SetLocation(const GE3DPoint& inLocation);   // +0x58 0xf2637 (unless it follows the unit)

    void Clear();                           // 0xf238a (provisional name)
    void UpdatePosition();                  // 0xf2c95 (the unit's or the point, plus the effect's offset)

    // +0x74 .. +0x115 but the orbitals, the graphic, the draw copies and the orbital count: the record's (ee_gfx
    // effect_icon.rs: the tick, the draw's opacity and fraction, load and save)
    GE3DPoint& mLocation = *reinterpret_cast<GE3DPoint*>(&this->mRecord->icon.location);   // (saved when it does not follow the unit)
    std::vector<EEOrbital*> mOrbitals;      // +0x80 (copies of the graphic's; deleted by the dtor)
    std::vector<float> mAngles;             // +0x90 (the orbitals' angles the draw uses)
    EEGraphicObject* mGraphicObject;        // +0xa0
    GETransformation mDrawTransformation;   // +0xa4 (Virtual24 copies mTransformation)
    float& mOpacity = this->mRecord->icon.opacity;
    float& mTime = this->mRecord->icon.time;                // the time into the current frame
    float& mScale = this->mRecord->icon.scale;
    float& mAge = this->mRecord->icon.age;                  // (saved)
    float& mFraction = this->mRecord->icon.fraction;        // the draw's fractional frame position
    uint32_t& mFrame = this->mRecord->icon.frame;
    unsigned long mDrawFrame;               // +0xfc
    uint32_t& mFrameCount = this->mRecord->icon.frame_count;
    uint32_t& mFrameTime = this->mRecord->icon.frame_time;
    long mOrbitalCount;                     // +0x108
    float& mScroll = this->mRecord->icon.scroll;            // the texture's u offset
    float mDrawScroll;                      // +0x110
    bool& mFollowUnit = this->mRecord->icon.follow_unit;    // (saved)
    bool& mGrowing = this->mRecord->icon.growing;
};

#endif
