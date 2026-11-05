// C:\EEX\RTS Engine\Graphics\RTSMousePointer.cpp (__FILE__ 0x4bec78). The mouse-pointer thread (RTTI
// RTSMousePointer, vtable 0x441c8c): 0x11b54e..0x11b844 (the ctor to HandleMessage; EEMap's ctor 0x11b844 opens the
// next unit). Unit flags: the exe's module set.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/Input System.h"
#include "lle/ISMouse.h"

#include "ee/EEMouse.h"
#include "ee/RTSMousePointer.h"

// FUNCTION: EXE 0x51b54e
// Creates the mouse-pointer thread for a window, hidden and with no surface, and waits until it runs
RTSMousePointer::RTSMousePointer(RTSMouse* inMouse, GEWindow* inWindow, long inThreadPriority)
    : mWindow(inWindow)
{
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mThreadCreated) == keseOK, 44);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mThreadDone) == keseOK, 45);
    this->mSurface = NULL;
    this->mMouse = inMouse;
    this->mIsShown = false;
    this->mQuit = false;

    ESVERIFY_AT(TSScheduler::CreateThread(this->mThread, RTSMousePointer::Main, (void*)this) == keseOK, 54);
    ESVERIFY_AT(this->mThread->SetPriority(inThreadPriority) == keseOK, 55);
    this->mThreadCreated->Wait(kTSWaitInfinite);
    this->mThread->SetName(gMousePointerThreadName);
}

// SYNTHETIC: EXE 0x51b628
// (the scalar deleting destructor)

// FUNCTION: EXE 0x51b644
// Destroys the mouse-pointer thread and its events
RTSMousePointer::~RTSMousePointer()
{
    TSScheduler::DestroyEvent(this->mThreadCreated);
    TSScheduler::DestroyEvent(this->mThreadDone);
    TSScheduler::DestroyThread(this->mThread);
}

// FUNCTION: EXE 0x51b66d
// Runs the pointer thread: handles messages and, while shown with a surface, redraws the pointer when it has moved
// (restore, save under, draw; at the window's client origin unless full screen), every 25 ms
unsigned long RTSMousePointer::Main(void* inPointer)
{
    RTSMousePointer* thePointer = static_cast<RTSMousePointer*>(inPointer);
    GE2DPoint theLastPosition(-1.0f, -1.0f);
    GE2DPoint theOrigin(0.0f, 0.0f);
    GE2DPoint thePosition;
    unsigned long theMessage;
    void* theParameter1;
    void* theParameter2;
    thePointer->mThreadCreated->Set();
    UMSetFloatingPointPrecision(kFPPrecision53);
    while (!thePointer->mQuit) {
        long theResult = thePointer->mThread->Receive(theMessage, theParameter1, theParameter2);
        if (UMGetFloatingPointPrecision() != kFPPrecision53)
            UMSetFloatingPointPrecision(kFPPrecision53);
        if (UMGetFloatingPointRoundingMode() != 0)
            UMSetFloatingPointRoundingMode(0);
        if (theResult == kErrorOK)
            thePointer->HandleMessage(theMessage, theParameter1, theParameter2);
        if (thePointer->mIsShown && !thePointer->mQuit && thePointer->mSurface != NULL) {
            gMouse->mInputLock->Enter();
            gISMouse->RefreshXY();
            gMouse->Update();
            gMouse->mInputLock->Leave();
            thePosition = thePointer->mMouse->mPosition;
            if (thePosition != theLastPosition) {
                thePointer->mMouse->BeginFrame();
                theLastPosition = thePosition;
                if (thePointer->mSurface->GetParentScreen()->GetIsFullScreen()) {
                    theOrigin = GE2DPoint(0.0f, 0.0f);
                } else {
                    theOrigin = thePointer->mWindow->GetClientOrigin();
                }
                thePointer->mMouse->RestoreUnder(thePointer->mSurface, theOrigin, false);
                thePointer->mMouse->SaveUnder(thePointer->mSurface, theOrigin);
                thePointer->mMouse->Draw(thePointer->mSurface, theOrigin);
                thePointer->mMouse->EndFrame();
            }
        }
        PlatSleep(0x19);
    }
    thePointer->mThreadDone->Set();
    return 0;
}

// FUNCTION: EXE 0x51b813
// Handles a pointer thread message: quit, hide (0x402), show (0x404) or set the surface to draw on (0x405)
void RTSMousePointer::HandleMessage(unsigned long inMessage, void* inParameter1, void* inParameter2)
{
    switch (inMessage) {
    case kThreadMessageQuit:
        this->mQuit = true;
        break;
    case 0x402:
        this->mIsShown = false;
        break;
    case 0x404:
        this->mIsShown = true;
        break;
    case 0x405:
        this->mSurface = static_cast<GESurface*>(inParameter1);
        break;
    }
}
