// C:\EEX\Empire Earth\Graphics Thread\EEROUnits.h (RECONSTRUCTION; file name inferred). The units' render object (RTTI
// EEROUnits > EERenderObject > RTSRenderObject, vtable 0x441bbc; EEROUnits.cpp 0x114296..0x118741, __FILE__ 0x4be760):
// the models, rings, bars and labels of the
// units in EERenderLists. No members of its own. Names are provisional. Requires ee/EERenderObject.h.
#ifndef EE_EEROUNITS_H
#define EE_EEROUNITS_H

#include "rust/game/gamecpp.h"
class EEROUnits : public EERenderObject {
public:
    EEROUnits() {  }
    // +0x10 dtor 0x114516 (??_G 0x1144fa)
    virtual ~EEROUnits() {  }
    // +0x00 0x114521: EERenderObject::Fn10EABE
    virtual void FrameStarting() { this->FillRenderLists(); }
    // +0x04 0xe123d trivial: ret (ICF)
    virtual void SceneStarting() {  }
    // +0x08 0x118203
    virtual void RenderScene() { ee_gamecpp_ru_draw(false); }
    // +0x0c 0x11852f
    virtual void FrameDone() { ee_gamecpp_ru_frame_done(); }

    // 0x116a97 (0x13fccb calls it; asserts) (provisional name)
    static void LoadMedia() { ee_gamecpp_ru_load_media(); }

};


// 0x118676 (cdecl): inPoint's screen position through the main viewport (EEE3DGoal) (provisional name)
void GetScreenPoint(const GE3DPoint& inPoint, GE2DPoint& outPoint);

#endif
