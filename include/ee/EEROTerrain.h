// C:\EEX\Empire Earth\Graphics Thread\EEROTerrain.h (RECONSTRUCTION; file name inferred). The terrain's render object
// (RTTI EEROTerrain > EERenderObject > RTSRenderObject, vtable 0x441b7c, sizeof 8; EEROTerrain.cpp 0x11251f..0x114296,
// __FILE__ 0x4be5f4): the sky, cloud and star models,
// the terrain, its highlights and grid. Names are provisional. Requires ee/EERenderObject.h.
#ifndef EE_EEROTERRAIN_H
#define EE_EEROTERRAIN_H

class GEModel;
class GETexture;

class EEROTerrain : public EERenderObject {
public:
    EEROTerrain();                          // 0x1125fb
    virtual ~EEROTerrain();                 // +0x10 dtor 0x11262d (??_G 0x112611)
    virtual void FrameStarting();           // +0x00 0x112638
    virtual void SceneStarting();           // +0x04 0xe123d trivial: ret (ICF)
    virtual void RenderScene();             // +0x08 0x1131a4
    virtual void FrameDone();               // +0x0c 0xe123d trivial: ret (ICF)
    virtual void Virtual14(long inArg1);    // +0x14 0x157269 NEW trivial: ret 4 (ICF)

    void StepGridMode(bool inBackwards);    // 0x1138a6: mGridMode cycles 0..0x11 (provisional name)

    static void CreateSkyModel();           // 0x112e7f (the sky, clouds, stars and the area-effect textures)
    static void ReleaseSkyModels();         // 0x1138d2

    long mGridMode;                         // +0x04 (= 0 in the ctor; non-zero: Virtual08 draws the terrain grid) (provisional name)
};

extern GETexture* gAreaEffectTexture;       // 0x5293f4 (named by its assert)
extern GETexture* gCircleAreaEffectTexture; // 0x5293f8 (named by its assert)

#endif
