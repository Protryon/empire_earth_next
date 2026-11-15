// C:\EEX\Empire Earth\Graphics Thread\EEROEffects.h (RECONSTRUCTION; file name inferred). The
// effects' render object (RTTI EEROEffects > EERenderObject > RTSRenderObject, vtable 0x4413d0; EEROEffects.cpp
// 0x111d3d..0x111dae). No members of its own. Requires ee/EERenderObject.h.
#ifndef EE_EEROEFFECTS_H
#define EE_EEROEFFECTS_H

class EEROEffects : public EERenderObject {
public:
    EEROEffects();                          // 0x111d3d
    virtual ~EEROEffects();                 // +0x10 (??_G 0x111d4f; dtor 0x111d6b)
    virtual void FrameStarting();           // +0x00 0x111d76: EERenderObject::QueueEffects
    virtual void SceneStarting();           // +0x04 0x111d7b: clears the z buffer
    virtual void RenderScene();             // +0x08 0xe123d (empty, ICF-shared)
    virtual void FrameDone();               // +0x0c 0xe123d
};

// a zero rectangle in .data (0x4e8f18; copied by value before ClearZBuffer here and in 4 other units: 0x11673f,
// 0x118813, 0x197429, 0x259b5a). Provisional name, type and home

#endif
