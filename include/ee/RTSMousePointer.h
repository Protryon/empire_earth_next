// C:\EEX\RTS Engine\Graphics\RTSMousePointer.h (RECONSTRUCTION; file name inferred). The thread
// Layout proven: RTSMousePointer sizeof 0x20 = its new-site size, ctor 0x11b54e exact in RTSMousePointer.cpp. Member
// names are provisional.
// that draws the mouse pointer (RTTI RTSMousePointer, vtable 0x441c8c: only the destructor; RTSMousePointer.cpp
// 0x11b54e..0x11b844, __FILE__ 0x4bec78; its asserts name mThreadCreated, mThreadDone, mThread). Member names
// Provisional unless an assert names them. Requires lle/LowLevelEngine.h, ee/EEMouse.h.
#ifndef EE_RTSMOUSEPOINTER_H
#define EE_RTSMOUSEPOINTER_H
#include "rust/game/rts.h"

class GESurface;
class GEWindow;
class RTSMouse;
class TSEvent;
class TSThread;

class RTSMousePointer {
public:
    // 0x11b54e: creates the events, starts the thread (Main) at inThreadPriority, waits for it
    RTSMousePointer(RTSMouse* inMouse, GEWindow* inWindow, long inThreadPriority) : mWindow(inWindow) { ee_rts_mouse_pointer_construct(this, inMouse, inThreadPriority); }
    // +0x00 (??_G 0x11b628; dtor 0x11b644)
    virtual ~RTSMousePointer() { ee_rts_mouse_pointer_destruct(this); }

    // 0x11b813: the thread's messages 0x401 (end), 0x402/0x404 (mB1C off/on), 0x405 (mU04 = the surface)
    void HandleMessage(unsigned long inMessage, void* inParameter1, void* inParameter2)
        { ee_rts_mouse_pointer_handle_message(this, inMessage, inParameter1); }

    GESurface* mSurface;                    // +0x04 (the surface the pointer is drawn on)
    TSThread* mThread;                      // +0x08
    TSEvent* mThreadCreated;                // +0x0c
    TSEvent* mThreadDone;                   // +0x10
    RTSMouse* mMouse;                       // +0x14
    GEWindow* mWindow;                      // +0x18
    volatile bool mIsShown;                 // +0x1c (volatile: `mov byte ptr [x], 0`, 0x11b837; drawing enabled)
    bool mQuit;                             // +0x1d (message 0x401: the thread ends)
};

extern UString gMousePointerThreadName;    // 0x52a240 (the thread's name; provisional name)
extern RTSMousePointer* gMousePointer;      // 0x5318fc (Empire Earth.cpp 0x140306 news it; EEGraphics::RestoreSurfaces waits on mB1C; provisional name)

#endif
