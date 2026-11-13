// EEE3DParticle.h reconstruction: original key/pool chains f4bf3..f5d94; size0xb4 proved by pool stride.
#ifndef EE_EEE3DPARTICLE_H
#define EE_EEE3DPARTICLE_H
#include "ee/EEEffect3D.h"
template <class T> class UMemoryPool;
class GE3DLine;
class EEGraphicObject;
class EEE3DParticle : public EEEffect3D {
public:
    static const UString sData;               // 4e8c50, data chain f4bf3
    static UMemoryPool<EEE3DParticle> sPool;   // 4e8c10, pool chain f4c36
    EEE3DParticle(long inKind = 0x39);         // f4c7b; pool default construction uses kind39
    virtual ~EEE3DParticle();                 // f4cc8; vector deleting dtor f5cc0
    static bool SetPoolSize(unsigned long inChunkSize, unsigned long inMaxChunks); // f4cfa
    static EEE3DParticle* Create(GE3DPoint& inLocation, EEGraphicEffect* inGraphicEffect); // f4d77
    static EEE3DParticle* Create(GE3DLine& inLine, EEGraphicEffect* inGraphicEffect); // f4e27
    static EEE3DParticle* Create();            // f4eec
    static void ReleasePool();                // f4f18
    virtual long DrawPass2();                 // fca98: xor eax,eax;ret
    virtual long DrawPass4();                 // fca98
    virtual long DrawPass5();                 // fca98
    virtual long DrawPass6();                 // f556b: draw
    virtual long Queue(EEEffectQueue* inQueues);     // f4d0f: queue4 (+0x30)
    virtual void ReleaseResources();          // f4f30: unload graphic, base cleanup
    virtual void PrepareQueue();              // e123d: ret
    virtual long SetUp();                     // f4f56: initialize effect fields/media
    virtual void LastDereference();           // f53a3: base then pool.Delete(this)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile); // f53bc
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile); // f57fc
    virtual long Step();                      // f588d: duration and pending transform
    void Clear();                            // f503c
    long Update();                           // f50b2
    // the record's (ee_gfx effect_state.rs `EeGfxEffectParticle`); the set-up, move, tick, load and save are Rust's
    GEVector& mVelocity = *reinterpret_cast<GEVector*>(&this->mRecord->particle.velocity);   // +74 (saved)
    GE3DPoint& mPosition = *reinterpret_cast<GE3DPoint*>(&this->mRecord->particle.draw_position);   // +80
    EEGraphicObject* mGraphicObject;          // +8c
    float& mLifeFraction = this->mRecord->particle.draw_age;   // +90
    float& mAge = this->mRecord->particle.age;   // +94, elapsed time (saved)
    float& mYaw = this->mRecord->particle.yaw;   // +98 (saved)
    float& mPitch = this->mRecord->particle.pitch;   // +9c (saved)
    float& mRoll = this->mRecord->particle.roll;   // +a0 (saved)
    float& mBrightness = this->mRecord->particle.brightness;   // +a4 (saved)
    float& mLastTime = this->mRecord->particle.last_time;   // +a8 (the game clock at the last move; saved)
    float& mDuration = this->mRecord->particle.duration;   // +ac, the graphic effect's duration
    bool& mMoved = this->mRecord->particle.moved;   // +b0
};                                          // pool chunk stride0xb4 includes alignment padding
#endif
