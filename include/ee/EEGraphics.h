// C:\EEX\Empire Earth\Graphics Thread\EEGraphics.h (RECONSTRUCTION). The graphics thread object (RTTI EEGraphics >
// RTSGraphics, vtable 0x4412c0, 10 slots; EEGraphics.cpp 0x10972b..0x10a9b0, __FILE__ 0x10977e). The game's
// instance is gGraphics (0x5318f4).
// Names are provisional unless an assert names them. Requires ee/RTSGraphics.h's includes.
#ifndef EE_EEGRAPHICS_H
#define EE_EEGRAPHICS_H

#include "ee/RTSGraphics.h"

class GESurface;
class GEViewport;
class TSEvent;

// A screen capture request (EEUserInterface.cpp 0x27828f creates it: its assert names mCaptureComplete; the graphics
// thread fills mSurface and sets the event). Struct name and the other member names are provisional.
struct EEScreenCapture {
    long mWidth;                            // +0x00 (-1: capture the render target, else a size for Virtual24)
    GESurface* mSurface;                    // +0x04 (dereferenced by the requester)
    TSEvent* mCaptureComplete;              // +0x08 (named by EEUserInterface.cpp's assert)
    bool mIsPostGame;                       // +0x0c (a post-game capture: "EEPostGame-" and jpg)
};

class EEGraphics : public RTSGraphics {   // RTTI EEGraphics > RTSGraphics
public:
    // 0x10977e: RTSGraphics(..., the screen's GetIsFullScreen(), inThreadPriority), then mMessageQueueDone
    EEGraphics(GERasterizer* inRasterizer, unsigned long inScreen, RTSMouse* inMouse, UIManager* inUIManager,
               long inThreadPriority);
    virtual ~EEGraphics();                  // +0x00 (??_G 0x109803; dtor 0x10981f)
    virtual long RestoreSurfaces();         // +0x04 0x1099dc
    // +0x08 0x109902: the game's messages 0x501..0x50b, the rest to RTSGraphics
    virtual void HandleMessage(unsigned long inMessage, void* inParameter1, void* inParameter2);
    virtual void FrameStarting();           // +0x0c 0x109840 (locks the world around RTSGraphics::FrameStarting)
    virtual void SceneStarting();           // +0x14 0x109ada
    virtual void RenderScene();             // +0x18 0x109adf
    virtual void FrameDone();               // +0x1c 0x109ae4 (unlocks what Virtual0C locked)
    virtual void SaveScreenShot(EEScreenCapture* inCapture);   // +0x20 0x109b22 (message 0x505; provisional name)
    virtual void RenderTo(EEScreenCapture* inCapture);         // +0x24 0x109cd4

    // the message posts (thread messages 0x503..0x50b; provisional names)
    void PostInitialiseScene();             // 0x10a862
    void PostRedrawUI();                    // 0x10a877
    void PostScreenShot(EEScreenCapture* inCapture);   // 0x10a88c (0x505)
    void PostReleaseScene();                // 0x10a8a5: mThread->Post(0x504, 0, 0, true)
    void PostFadeIn(unsigned long inValue);       // 0x10a8ba
    void PostFadeOut(unsigned long inValue);      // 0x10a8d3
    void PostLetterboxIn(unsigned long inValue);  // 0x10a8ec
    void PostLetterboxOut(unsigned long inValue);  // 0x10a905
    long PostMessage50AAndWait();           // 0x10a91e: Post(0x50a, 0, 0, true), then waits on +0x6c when it worked

    // Members from the ctor 0x10977e: GERectangle's inline ctor (top, left, bottom, right =
    // +0x68, +0x60, +0x5c, +0x64), then after the vptr `mov dword ptr [+0x70], 0` (a plain long would be `and`:
    // volatile) and TSScheduler::CreateEvent(+0x6c, true). sizeof 0x74 (new site). Names are provisional
    GERectangle mRectangle;                 // +0x5c
    TSEvent* mMessageQueueDone;             // +0x6c (named by the ctor's assert; PostMessage50AAndWait waits on it)
    volatile long mLockMode;                // +0x70 (message 0x502; Virtual0C/1C lock by it)
};

extern EEGraphics* gGraphics;               // 0x5318f4

// the graphics thread's settings the messages change (EERenderObject.cpp's range, 0x10ac30..; cdecl free functions
// or statics: provisional names)
void EEInitialiseScene();                   // 0x10ae74 (message 0x503: resets ~40 render globals)
void EEReleaseScene();                      // 0x10f239 (message 0x504 and ~EEGraphics)
void EEStartLightFadeIn(unsigned long inValue);  // 0x10f37d (message 0x506: 1/inValue, or FLT_MAX for 0)
void EEStartLightFadeOut(unsigned long inValue);  // 0x10f3c6 (message 0x507)
void EEStartLetterboxIn(unsigned long inValue);  // 0x10b36a (message 0x508)
void EEStartLetterboxOut(unsigned long inValue);  // 0x10b31e (message 0x509)
extern bool gB529066;                       // 0x529066 (cleared before each frame's render objects; provisional name)
extern bool gB529068;                       // 0x529068 (likewise)

// Virtual24's globals (provisional names). Virtual24 saves and restores gOrthographicView/gB529065 and
// tiles the capture at the window size (unsigned: fild qword).
extern GEViewport gViewport;                // 0x5308f0 (Empire Earth.cpp)
extern bool& gOrthographicView;              // 0x5308e2 (= true while the tiles render)
extern bool gB529065;                       // 0x529065 (= true while Virtual24 runs)
extern bool& gAmbientEffectsEnabled;         // 0x5308d5 (EEE2DBattlefieldAmbient: the ambient effects run) (provisional name)
extern bool& gDrawBlood;                     // 0x5308df (EEE2DImpactBlood draws only when set) (provisional name)
extern unsigned long& gGameWindowWidth;      // 0x53087c (1024 with the free camera)
extern unsigned long& gGameWindowHeight;     // 0x530878 (768 with the free camera)
// 0x5309e0: the camera transformation (also EECCEarthquake.h; the 3D effects map their position through it,
// EEEffect3D::Queue 0x10520b) (provisional name)
class GETransformation;
// 0x4e8f08: the viewport the 3D effects reset the depth range of around their draws (SetDepthRange(0, 0), then
// (0, 1); EEE3DScreenFlash::DrawPass6 0xf8f1d) (provisional name)
extern GEViewport gU4E8F08;

#endif
