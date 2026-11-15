// C:\EEX\Empire Earth\Graphics Thread\EEROUserInterface.h (RECONSTRUCTION; file name inferred).
// The render object that draws the UI (RTTI EEROUserInterface > EERenderObject > RTSRenderObject, vtable 0x441bec,
// sizeof 8; EEROUserInterface.cpp 0x118741..0x118aa2). EEUserInterface 0x27754b news it with gUIManager and pushes
// it on the graphics thread (gU56534C). Only what EEUserInterface.cpp needs; slot names are provisional. Requires
// ee/EERenderObject.h.
#ifndef EE_EEROUSERINTERFACE_H
#define EE_EEROUSERINTERFACE_H

class UIManager;

class EEROUserInterface : public EERenderObject {
public:
    EEROUserInterface(UIManager* inManager);   // 0x11876d
    virtual ~EEROUserInterface();           // +0x10 (??_G 0x118788; dtor 0x1187a4)
    virtual void FrameStarting();           // +0x00 0x1187af
    virtual void SceneStarting();           // +0x04 0x118802
    virtual void RenderScene();             // +0x08 0x118907
    virtual void FrameDone();               // +0x0c 0x118a99

    UIManager* mUIManager;                  // +0x04 (the ctor stores its argument)
};

// the current player has a selection (EEROUserInterface::FrameStarting sets it, EEROUnits reads it; provisional name)
extern bool gUnitPanelActive;               // 0x5297a0

#endif
