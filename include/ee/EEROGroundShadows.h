// C:\EEX\Empire Earth\Graphics Thread\EEROGroundShadows.h (RECONSTRUCTION; file name inferred).
// The ground shadows' render object (RTTI EEROGroundShadows > EERenderObject > RTSRenderObject, vtable 0x4413f0;
// EEROGroundShadows.cpp 0x111dae..0x11251f; its draws are ee_gfx shadows.rs's). No members of its own. Requires ee/EERenderObject.h, ee/EERenderLists.h.
#ifndef EE_EEROGROUNDSHADOWS_H
#define EE_EEROGROUNDSHADOWS_H

class EEROGroundShadows : public EERenderObject {
public:
    EEROGroundShadows();
    virtual ~EEROGroundShadows();           // +0x10 (??_G 0x111dc0; dtor 0x111ddc)
    virtual void FrameStarting();           // +0x00 0x111de7: Fn10C036, then EERenderObject::Fn10EABE
    virtual void SceneStarting();           // +0x04 (empty, ICF-shared 0xe123d)
    virtual void RenderScene();             // +0x08 0x11207f: the shadows and the effects of queue [0]
    virtual void FrameDone();               // +0x0c (empty, ICF-shared 0xe123d)

    static void CreateShadowModel();        // 0x111df8: the static "StaticShadow" quad (0x529098) (provisional name)
    static void ReleaseShadowModel();
    // DrawShadows (0x1121c8), DrawUnitShadows (0x1122bb) and the space-tile test (0x11246b): ee_gfx shadows.rs
};

#endif
