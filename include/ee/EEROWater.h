// C:\EEX\Empire Earth\Graphics Thread\EEROWater.h (RECONSTRUCTION; file name inferred). The
// water's render object (RTTI EEROWater > EERenderObject > RTSRenderObject, vtable 0x441c10; its members open
// EEUnitPicker.cpp's range 0x118aa2..0x118c40: no __FILE__, so it may be a unit of its own). No members of its own.
// Requires ee/EERenderObject.h.
#ifndef EE_EEROWATER_H
#define EE_EEROWATER_H

class EEROWater : public EERenderObject {
public:
    EEROWater();                            // 0x118aa2
    virtual ~EEROWater();                   // +0x10 (??_G 0x118ab4; dtor 0x118ad0)
    virtual void FrameStarting();           // +0x00 0x118adb: the terrain mesh's water animation
    virtual void SceneStarting();           // +0x04 (empty, ICF-shared 0xe123d)
    virtual void RenderScene();             // +0x08 0x118ae9: draws the water and the water effects (queue [4])
    virtual void FrameDone();               // +0x0c (empty, ICF-shared 0xe123d)
};

#endif
