// C:\EEX\Empire Earth\Graphics Thread\EEROEffects.h (RECONSTRUCTION; file name inferred). The
// effects' render object (RTTI EEROEffects > EERenderObject > RTSRenderObject, vtable 0x4413d0; EEROEffects.cpp
// 0x111d3d..0x111dae). No members of its own. Requires ee/EERenderObject.h.
#ifndef EE_EEROEFFECTS_H
#define EE_EEROEFFECTS_H

#include "rust/game/gamecpp.h"
class EEROEffects : public EERenderObject {
public:
    // Creates the effects' render object
    EEROEffects() {  }
    // Destroys the effects' render object
    // +0x10 (??_G 0x111d4f; dtor 0x111d6b)
    virtual ~EEROEffects() {  }
    // Sorts the visible effects into the render queues for this frame
    // +0x00 0x111d76: EERenderObject::QueueEffects
    virtual void FrameStarting() { this->QueueEffects(); }
    // Clears the z buffer of the game view before the effects draw
    // +0x04 0x111d7b: clears the z buffer
    virtual void SceneStarting() { ee_gamecpp_ov_effects_scene_starting(); }
    // Virtual08 and Virtual0C: one empty body, ICF-folded into 0xe123d (no address of their own here)
    // +0x08 0xe123d (empty, ICF-shared)
    virtual void RenderScene() {  }
    // +0x0c 0xe123d
    virtual void FrameDone() {  }
};

// a zero rectangle in .data (0x4e8f18; copied by value before ClearZBuffer here and in 4 other units: 0x11673f,
// 0x118813, 0x197429, 0x259b5a). Provisional name, type and home

#endif
