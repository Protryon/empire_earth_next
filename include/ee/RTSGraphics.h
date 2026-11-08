// C:\EEX\RTS Engine\Graphics\RTSGraphics.h (RECONSTRUCTION; file name inferred). The graphics
// thread's engine side (RTTI RTSGraphics, vtable 0x441c50; RTSGraphics.cpp 0x11a1cc..0x11ab55, __FILE__ 0x4be990;
// its asserts name mRenderNow, mThreadCreated, mThreadDone, mThread) and the render objects it draws (RTTI
// RTSRenderObject, vtable 0x441390: four pure slots, the destructor last). The game's EEGraphics (vtable 0x4412c0)
// derives from it. Member names are provisional unless an assert names them; slots named by offset.
// Requires lle/LowLevelEngine.h (TSThread, TSEvent), lle/GERasterizer.h, <vector>.
#ifndef EE_RTSGRAPHICS_H
#define EE_RTSGRAPHICS_H

#include <vector>

class FSFileSpec;
class GEMovie;
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
    virtual ~RTSRenderObject();             // +0x10 (??_G 0x1106e5)
};

class RTSGraphics {
public:
    // 0x11a1cc: creates the events, starts the thread (Main) at inThreadPriority, waits for it
    RTSGraphics(GERasterizer* inRasterizer, unsigned long inScreen, RTSMouse* inMouse, UIManager* inUIManager,
                bool inIsFullScreen, long inThreadPriority);
    virtual ~RTSGraphics();                 // +0x00 (??_G 0x11a34c; dtor 0x11a368)
    virtual long RestoreSurfaces();         // +0x04 0x11a703: the screen's RestoreAllSurfaces
    // +0x08 0x11a4e1: the thread's messages 0x401..0x40f (posted by the members below)
    virtual void HandleMessage(unsigned long inMessage, void* inParameter1, void* inParameter2);
    virtual void FrameStarting();           // +0x0c 0x11a3af: each render object's slot 0x00
    virtual void Render(bool inDrawMouse, bool inShowScene);  // +0x10 0x11a71c (the thread's frame; provisional name)
    virtual void SceneStarting();           // +0x14 0x11a8fb: each render object's slot 0x04
    virtual void RenderScene();             // +0x18 0x11a922: SetAlphaTestingRefValue(0), each object's slot 0x08
    virtual void FrameDone();               // +0x1c 0x11a95d: each render object's slot 0x0c

    static unsigned long Main(void* inGraphics);   // 0x11a3d5 (the thread function; named by the ctor's assert)
    void PopRenderObject();                 // 0x11a4c9 (message 0x405)
    void PushRenderObject(RTSRenderObject* inObject);   // 0x11a4d1 (message 0x404)
    void PostClearRenderObjects();          // 0x11a984: posts 0x406
    void Shutdown();                        // 0x11a999: posts 0x401 and waits for mThreadDone
    long PostPause();                       // 0x11a9bd (Post's result: EEUserInterface 0x2784d9 tests eax): waits for mIsResumePending, sets mIsPausePending, posts 0x402 (pause: mIsPaused = true) (provisional name)
    long PlayMovie(const FSFileSpec& inFile, UReferenceCounter* inMovieDoneEvent);  // 0x11a9ea (Post's result): posts 0x40c with a copy
    void PostPushRenderObject(RTSRenderObject* inObject);   // 0x11aa38: posts 0x404
    void PostSetFlag5B(bool inFlag);        // 0x11aa51: posts 0x40a (provisional name)
    void PostSetRasterizer(GERasterizer* inRasterizer, unsigned long inScreen);   // 0x11aa6c: posts 0x407
    void PostSetFlag54(bool inFlag);        // 0x11aa87: posts 0x408 (provisional name)
    void PostResume();                      // 0x11aaa2: waits for mIsPausePending, sets mIsResumePending, posts 0x403 (resume: mIsPaused = false) (provisional name)
    void PostStopMovie();                   // 0x11aacf: posts 0x40d
    void SetSurface(GESurface* inSurface);  // 0x11aae4: mU34, and mU38 the last attached surface before type 3

    TSThread* mThread;                      // +0x04 (named by the ctor's asserts)
    GERasterizer* mRasterizer;              // +0x08
    unsigned long mScreen;                  // +0x0c
    bool mShowOverdraw;                     // +0x10 (message 0x40e: the overdraw display mode)
    std::vector<RTSRenderObject*> mRenderObjects;   // +0x14
    TSEvent* mRenderNow;                    // +0x24 (named by the ctor's asserts)
    TSEvent* mThreadCreated;                // +0x28
    TSEvent* mThreadDone;                   // +0x2c
    RTSMouse* mMouse;                       // +0x30
    GESurface* mSurface;                    // +0x34 (the screen's secondary surface)
    GESurface* mU38;                        // +0x38
    UIManager* mUIManager;                  // +0x3c
    UReferenceCounter* mMovieDoneEvent;     // +0x40 (PlayMovie's second argument, referenced; class provisional name)
    GEMovie* mMovie;                        // +0x44
    float mSavedMasterVolume;               // +0x48 (the master volume, saved while a movie plays)
    float mSavedConductorVolume;            // +0x4c (the conductor's volume, saved likewise)
    long mL50;                              // +0x50
    bool mB54;                              // +0x54
    bool GetIsMoviePlaying() const { return this->mIsMoviePlaying; }  // (EEUIManager::UpdateMouse 0x6751ab: `mov al; test al, al`)
    volatile bool mIsMoviePlaying;          // +0x55 (a movie is playing; volatile: `mov byte ptr [x], 0`, 0x11a63b)
    volatile bool mIsPaused;                // +0x56 (= true; volatile: `mov byte ptr [x], 0`, 0x11a530)
    volatile bool mIsPausePending;          // +0x57 (PostResume waits for it to clear)
    volatile bool mIsResumePending;         // +0x58 (PostPause waits for it to clear)
    bool mIsQuitting;                       // +0x59 (message 0x401: the thread ends)
    bool mIsWindowed;                       // +0x5a (= !inArg5)
    bool mIsSlowPlayback;                   // +0x5b
};

class SEMixer;
extern SEMixer* gConductor;             // 0x5319c0 (the mixer; its conductor at +0 is muted for a movie) (provisional name)
bool EELobbyIsShown();                      // 0x297ed0: a global flag (the thread skips the frame while it is set) (provisional name)
void EELobbyLock();                         // 0x297f15: enters a global critical section (0x565858) (provisional name)
void EELobbyUnlock();                       // 0x297f21: leaves it (provisional name)
extern UString gGraphicsThreadName;       // 0x52a260 (the thread's name; provisional name)

#endif
