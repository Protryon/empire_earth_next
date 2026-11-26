// C:\EEX\Empire Earth\Graphics Thread\EEROUserInterface.h (RECONSTRUCTION; file name inferred).
// The render object that draws the UI (RTTI EEROUserInterface > EERenderObject > RTSRenderObject, vtable 0x441bec,
// sizeof 8; EEROUserInterface.cpp 0x118741..0x118aa2). EEUserInterface 0x27754b news it with gUIManager and pushes
// it on the graphics thread (gU56534C). Only what EEUserInterface.cpp needs; slot names are provisional. Requires
// ee/EERenderObject.h.
#ifndef EE_EEROUSERINTERFACE_H
#define EE_EEROUSERINTERFACE_H

#include "rust/game/gamecpp.h"
class UIManager;

class EEROUserInterface : public EERenderObject {
public:
    // Creates the UI's render object for a UI manager
    EEROUserInterface(UIManager* inManager) { this->mUIManager = inManager; }
    // Destroys the UI's render object
    // +0x10 (??_G 0x118788; dtor 0x1187a4)
    virtual ~EEROUserInterface() {  }
    // The frame's data and the unit panel's flag, the z-buffer clears under the panels and the UI's polygons: gamecpp
    // ro_views.rs
    // +0x00 0x1187af
    virtual void FrameStarting() { ee_gamecpp_gu_frame_starting(this); }
    // +0x04 0x118802
    virtual void SceneStarting() { ee_gamecpp_gu_scene_starting(); }
    // +0x08 0x118907
    virtual void RenderScene() { ee_gamecpp_gu_render_scene(this); }
    // Draws the UI's text
    // +0x0c 0x118a99
    virtual void FrameDone() { this->mUIManager->DrawTextA(); }

    UIManager* mUIManager;                  // +0x04 (the ctor stores its argument)
};

#endif
