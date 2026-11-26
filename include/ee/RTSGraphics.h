// C:\EEX\RTS Engine\Graphics\RTSGraphics.h (RECONSTRUCTION; file name inferred). The graphics
// thread's engine side (RTTI RTSGraphics, vtable 0x441c50; RTSGraphics.cpp 0x11a1cc..0x11ab55, __FILE__ 0x4be990;
// its asserts name mRenderNow, mThreadCreated, mThreadDone, mThread) and the render objects it draws (RTTI
// RTSRenderObject, vtable 0x441390: four pure slots, the destructor last). The game's EEGraphics (vtable 0x4412c0)
// derives from it. Member names are provisional unless an assert names them; slots named by offset.
// Requires lle/LowLevelEngine.h (TSThread, TSEvent), lle/GERasterizer.h, <vector>.
#ifndef EE_RTSGRAPHICS_H
#define EE_RTSGRAPHICS_H

#include <stdint.h>
#include <vector>
#include "rust/game/rts.h"            // EeRtsGraphicsFields (RTSGraphics' plain members)

class FSFileSpec;
class GERasterizer;
class GESurface;
class RTSMouse;
class TSEvent;
class TSThread;
class UIManager;
class UReferenceCounter;

class RTSRenderObject {
public:
    virtual void FrameStarting() = 0;       // +0x00 (RTSGraphics::FrameStarting calls it for each object)
    virtual void SceneStarting() = 0;       // +0x04 (RTSGraphics::SceneStarting)
    virtual void RenderScene() = 0;         // +0x08 (RTSGraphics::RenderScene)
    virtual void FrameDone() = 0;           // +0x0c (RTSGraphics::FrameDone)
    // +0x10 (??_G 0x1106e5)
    virtual ~RTSRenderObject() {  }
};

// LLE round 8: the bodies are Rust (ee_game rts graphics.rs); the plain members are its record EeRtsGraphicsFields,
// a base (mScreen, mShowOverdraw, mL50, mB54, mIsPaused, mIsPausePending, mIsResumePending, mIsQuitting, mIsWindowed,
// mIsSlowPlayback and RenderLobby's mLobbySerial, mLobbySecondaryWidth/Height, mLobbyShownTime).
class RTSGraphics : public EeRtsGraphicsFields {
public:
    // 0x11a1cc: creates the events, starts the thread (Main) at inThreadPriority, waits for it
    RTSGraphics(GERasterizer* inRasterizer, unsigned long inScreen, RTSMouse* inMouse, UIManager* inUIManager, bool inIsFullScreen, long inThreadPriority) { ee_rts_graphics_construct(this, inRasterizer, inScreen, inMouse, inUIManager, inIsFullScreen, inThreadPriority); }
    // +0x00 (??_G 0x11a34c; dtor 0x11a368)
    virtual ~RTSGraphics() { ee_rts_graphics_destruct(this); }
    // +0x04 0x11a703: the screen's RestoreAllSurfaces
    virtual long RestoreSurfaces();
    // +0x08 0x11a4e1: the thread's messages 0x401..0x40f (posted by the members below)
    virtual void HandleMessage(unsigned long inMessage, void* inParameter1, void* inParameter2)
        { ee_rts_graphics_handle_message(this, inMessage, inParameter1, inParameter2); }
    // +0x0c 0x11a3af: each render object's slot 0x00
    virtual void FrameStarting() { ee_rts_graphics_objects_pass(this, 0); }
    // +0x10 0x11a71c (the thread's frame; provisional name)
    virtual void Render(bool inDrawMouse, bool inShowScene) { ee_rts_graphics_render(this, inDrawMouse, inShowScene); }
    // +0x14 0x11a8fb: each render object's slot 0x04
    virtual void SceneStarting() { ee_rts_graphics_objects_pass(this, 1); }
    // +0x18 0x11a922: SetAlphaTestingRefValue(0), each object's slot 0x08
    virtual void RenderScene() { ee_rts_graphics_objects_pass(this, 2); }
    // +0x1c 0x11a95d: each render object's slot 0x0c
    virtual void FrameDone() { ee_rts_graphics_objects_pass(this, 3); }

    static unsigned long Main(void* inGraphics);   // 0x11a3d5 (the thread function; named by the ctor's assert)
    // 0x11a4c9 (message 0x405)
    void PopRenderObject() { this->mRenderObjects.pop_back(); }
    // 0x11a4d1 (message 0x404)
    void PushRenderObject(RTSRenderObject* inObject) { this->mRenderObjects.push_back(inObject); }
    // 0x11a984: posts 0x406
    void PostClearRenderObjects() { this->mThread->Post(0x406, NULL, NULL, true); }
    // 0x11a999: posts 0x401 and waits for mThreadDone
    void Shutdown() { ee_rts_graphics_shutdown(this); }
    // 0x11a9bd (Post's result: EEUserInterface 0x2784d9 tests eax): waits for mIsResumePending, sets mIsPausePending, posts 0x402 (pause: mIsPaused = true) (provisional name)
    long PostPause() { return ee_rts_graphics_post_pause(this); }
    // 0x11aa38: posts 0x404
    void PostPushRenderObject(RTSRenderObject* inObject) { this->mThread->Post(0x404, inObject, NULL, true); }
    // 0x11aa51: posts 0x40a (provisional name)
    void PostSetFlag5B(bool inFlag) { this->mThread->Post(0x40a, (void*)inFlag, NULL, true); }
    // 0x11aa6c: posts 0x407
    void PostSetRasterizer(GERasterizer* inRasterizer, unsigned long inScreen)
        { this->mThread->Post(0x407, inRasterizer, reinterpret_cast<void*>(inScreen), true); }
    // 0x11aa87: posts 0x408 (provisional name)
    void PostSetFlag54(bool inFlag) { this->mThread->Post(0x408, (void*)inFlag, NULL, true); }
    // 0x11aaa2: waits for mIsPausePending, sets mIsResumePending, posts 0x403 (resume: mIsPaused = false) (provisional name)
    void PostResume() { ee_rts_graphics_post_resume(this); }
    // 0x11aae4: mU34, and mU38 the last attached surface before type 3
    void SetSurface(GESurface* inSurface) { ee_rts_graphics_set_surface(this, inSurface); }
    // port: the frame while the WON lobby is shown (Main, instead of Render): the lobby's composited frame copied to
    // the back buffer and shown (the original showed the lobby in its own GDI window)
    void RenderLobby() { ee_rts_graphics_render_lobby(this); }

    TSThread* mThread;                      // +0x04 (named by the ctor's asserts)
    GERasterizer* mRasterizer;              // +0x08
    std::vector<RTSRenderObject*> mRenderObjects;   // +0x14
    TSEvent* mRenderNow;                    // +0x24 (named by the ctor's asserts)
    TSEvent* mThreadCreated;                // +0x28
    TSEvent* mThreadDone;                   // +0x2c
    RTSMouse* mMouse;                       // +0x30
    GESurface* mSurface;                    // +0x34 (the screen's secondary surface)
    GESurface* mU38;                        // +0x38
    UIManager* mUIManager;                  // +0x3c
    // (+0x40..+0x4c: the original's movie, its done event and the volumes saved while it played; no movie plays)
    // port: RenderLobby's state (graphics thread only)
    GESurface* mLobbyFrame = NULL;          // system memory, the lobby frame's size in the back buffer's format
};

class SEMixer;
extern SEMixer* gConductor;             // 0x5319c0 (the mixer; its conductor at +0) (provisional name)
bool EELobbyIsShown();                      // 0x297ed0: a global flag (the thread skips the frame while it is set) (provisional name)
void EELobbyLock();                         // 0x297f15: enters a global critical section (0x565858) (provisional name)
void EELobbyUnlock();                       // 0x297f21: leaves it (provisional name)
// port: the WON lobby's frame for RenderLobby (EELobbyInterface.cpp; the caller holds EELobbyLock): the composited
// frame, 32-bit pixels 0xFFRRGGBB, outWidth x outHeight pixels, outPitch bytes a row, showing the lobby's
// outLogicalWidth x outLogicalHeight (1024x768) surface, a serial that changes with each frame -> false when the lobby
// is not shown or has no frame yet (nothing to unlock then); EELobbyUnlockFrame after copying it.
bool EELobbyLockFrame(const uint32_t*& outPixels, int& outWidth, int& outHeight, int& outPitch, int& outLogicalWidth,
                      int& outLogicalHeight, uint32_t& outSerial);
void EELobbyUnlockFrame();
// port: the lobby's pixels per logical pixel and where it is shown, as fractions of the back buffer
// (PlatWindowManager::SetPixelScale, SetPresentRect; the caller holds EELobbyLock), and the logical size it is laid
// out for
void EELobbySetPresentation(float inScale, float inLeft, float inTop, float inWidth, float inHeight);
void EELobbyGetSurfaceSize(int& outWidth, int& outHeight);
extern UString gGraphicsThreadName;       // 0x52a260 (the thread's name; provisional name)

#endif
