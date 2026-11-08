// C:\EEX\Empire Earth\Graphics Thread\EEE3DRain.h (RECONSTRUCTION; file name inferred).
// The rain over a storm's tiles: falling drops (a three-vertex model "EEE3DRainDropModel", textures\raindrop) spawned
// over random tiles, a splash effect where a drop hits water, the "3DRain" loop and "3D Rain Rumble" sounds
// (vtable 0x440c04; RTTI EEE3DRain > EEE3DStorm; EEE3DRain.cpp 0xf7394..0xf8ceb: the ctors .. SetCursor, then the two
// vectors' template COMDATs). sizeof 0xf0. Names are provisional. Requires ee/EEE3DStorm.h and its requirements.
#ifndef EE_EEE3DRAIN_H
#define EE_EEE3DRAIN_H

#include <vector>

class SESMInstanceNode;

// a falling drop (0x18 bytes; MarkInvalid builds one, AddedToParent moves it down until it reaches mGroundZ)
struct EE3DRainDrop {
    EE3DRainDrop(const GE3DPoint& inPosition, float inGroundZ, float inAngle, bool inWater)
        : mPosition(inPosition), mGroundZ(inGroundZ), mAngle(inAngle), mWater(inWater) {}
    GE3DPoint mPosition;                    // +0x00
    float mGroundZ;                         // +0x0c (0 over water)
    float mAngle;                           // +0x10 (a random angle, 0..2 pi)
    bool mWater;                            // +0x14 (the tile is water: a splash when the drop lands)
};

// a drop to draw this frame (0x14 bytes; Virtual24 copies the drops)
struct EE3DRainDraw {
    EE3DRainDraw(const GE3DPoint& inPosition, float inAngle, bool inWater)
        : mPosition(inPosition), mAngle(inAngle), mWater(inWater) {}
    GE3DPoint mPosition;                    // +0x00
    float mAngle;                           // +0x0c
    bool mWater;                            // +0x10
};

class EEE3DRain : public EEE3DStorm {
public:
    EEE3DRain(const std::vector<EETileCoordinate>& inTiles, EEGraphicEffect* inGraphicEffect);   // 0xf7394 (kind 0x2a)
    EEE3DRain();                            // 0xf743d
    virtual ~EEE3DRain();                   // +0x00 0xf74a0 (??_G 0xf7421)
    virtual long DrawPass6();               // +0x18 0xf7b55: draws the drops
    virtual long Queue(EEEffectQueue* inQueues);       // +0x24 0xf76da
    virtual void PrepareQueue();            // +0x2c (empty: ICF-folded into 0xe123d)
    virtual long SetUp();                   // +0x30 (ICF-folded into EEE3DSnow's 0xfa291): centre, transformation, radius
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);    // +0x38 0xf7c83
    virtual long RepairPointers();          // +0x3c 0xf7d0f: the drops saved as a count are respawned
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // +0x40 0xf7d7f
    virtual long Step();                    // +0x4c 0xf7e3f: the update (7: over)
    virtual void Virtual64();               // +0x64 0xf7792: the graphic effect's parameters, sounds, the drop model
    virtual long AddDrop();                 // +0x68 0xf75b7: a new drop over a random tile
    virtual long UpdateDrops();             // +0x6c 0xf7e9f: spawns and moves the drops
    virtual long Virtual70();               // +0x70 (`return 0`: ICF-folded into 0xfca98)
    virtual void SetSoundLevel(SESMInstanceNode* inSound, float inVolume);  // +0x74 0xf80e8: the sound at the effect
    virtual long StartSounds();             // +0x78 0xf8195: starts the loop and the rumbles

    // the sounds (defined in the .cpp after their first users, so called out of line: 0xf7519.. after the dtor,
    // 0xf8337/0xf835e after SetCursor)
    void StopSound();                       // 0xf7519
    void StopRumble();                      // 0xf753e
    void ReleaseSound();                    // 0xf7563
    void ReleaseRumble();                   // 0xf758d
    void SetSound(SESMInstanceNode* inSound);   // 0xf8337
    void SetRumble(SESMInstanceNode* inSound);  // 0xf835e

    std::vector<EE3DRainDraw> mDraws;       // +0xb0
    std::vector<EE3DRainDrop> mDrops;       // +0xc0
    SESMInstanceNode* mSound;               // +0xd0 (the loop)
    SESMInstanceNode* mRumble;              // +0xd4
    unsigned long mDropCount;               // +0xd8 (saved; drops spawned so far)
    unsigned long mLoadedDrops;             // +0xdc (Load: the drop count Virtual3C respawns)
    unsigned long mSoundID;                 // +0xe0 (the graphic effect's mL64)
    unsigned long mRumbleID1;               // +0xe4 (mL68)
    unsigned long mRumbleID2;               // +0xe8 (mL6C)
    float mRumbleDelay;                     // +0xec (saved; ms to the next rumble)
};

#endif
