// C:\EEX\Empire Earth\Graphics Thread\EEE3DSnow.h (RECONSTRUCTION; file name inferred).
// The snow over a storm's tiles: drifting, spinning flakes (a two-triangle model "EEE3DSnowFlakeModel",
// textures\snowflake) spawned over random tiles (vtable 0x440dc8; RTTI EEE3DSnow > EEE3DStorm; EEE3DSnow.cpp
// 0xf9b6d..0xfb088: the ctors .. Virtual6C, EEE3DSnowFlake's ctor and Update, then the vectors' template COMDATs).
// sizeof 0xd8. Names are provisional. Requires ee/EEE3DStorm.h and its requirements.
// The flakes (the original's `EEE3DSnowFlake`: their drift and spin), their count, the queue, set-up, update, load,
// save and respawn are the storm's (ee_gfx effect_storm.rs); the snow keeps its flake model and its draw.
#ifndef EE_EEE3DSNOW_H
#define EE_EEE3DSNOW_H

class EEE3DSnow : public EEE3DStorm {
public:
    EEE3DSnow(const std::vector<EETileCoordinate>& inTiles, EEGraphicEffect* inGraphicEffect);   // 0xf9b6d (kind 0x3d)
    EEE3DSnow();                            // 0xf9c11
    virtual ~EEE3DSnow();                   // +0x00 0xf9c87 (??_G 0xf9bf5)
    virtual long DrawPass6();               // +0x18 0xfa404: draws the flakes (fading out near the camera)
    virtual void PrepareQueue();            // +0x2c (empty: ICF-folded into 0xe123d)
    virtual void Virtual64();               // +0x64 0xf9ef5: the graphic effect's parameters, the flake model
};

#endif
