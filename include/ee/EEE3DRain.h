// C:\EEX\Empire Earth\Graphics Thread\EEE3DRain.h (RECONSTRUCTION; file name inferred).
// The rain over a storm's tiles: falling drops (a three-vertex model "EEE3DRainDropModel", textures\raindrop) spawned
// over random tiles, a splash effect where a drop hits water, the "3DRain" loop and "3D Rain Rumble" sounds
// (vtable 0x440c04; RTTI EEE3DRain > EEE3DStorm; EEE3DRain.cpp 0xf7394..0xf8ceb: the ctors .. SetCursor, then the two
// vectors' template COMDATs). sizeof 0xf0. Names are provisional. Requires ee/EEE3DStorm.h and its requirements.
// The drops, their count, the thunder delay, the queue, set-up, load, save and respawn are the storm's (ee_gfx
// effect_storm.rs); the rain keeps its sounds, its drop model and its draw.
#ifndef EE_EEE3DRAIN_H
#define EE_EEE3DRAIN_H

class SESMInstanceNode;

class EEE3DRain : public EEE3DStorm {
public:
    EEE3DRain(const std::vector<EETileCoordinate>& inTiles, EEGraphicEffect* inGraphicEffect);   // 0xf7394 (kind 0x2a)
    EEE3DRain();                            // 0xf743d
    virtual ~EEE3DRain();                   // +0x00 0xf74a0 (??_G 0xf7421)
    virtual long DrawPass6();               // +0x18 0xf7b55: draws the drops
    virtual void PrepareQueue();            // +0x2c (empty: ICF-folded into 0xe123d)
    virtual long Step();                    // +0x4c 0xf7e3f: the sounds, then the storm's update (7: over)
    virtual void Virtual64();               // +0x64 0xf7792: the graphic effect's parameters, sounds, the drop model
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

    SESMInstanceNode* mSound;               // +0xd0 (the loop)
    SESMInstanceNode* mRumble;              // +0xd4
    unsigned long mSoundID;                 // +0xe0 (the graphic effect's mL64)
    unsigned long mRumbleID1;               // +0xe4 (mL68)
    unsigned long mRumbleID2;               // +0xe8 (mL6C)
};

#endif
