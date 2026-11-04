// DX7Screen (RECONSTRUCTION): a GEScreen over one DirectDraw device (one per monitor). Layout from the ctor 0x1000d271
// and the inline getters (emitted by DX7HRDisplay.cpp, 0x10001322..): sizeof 0x44 (copy ctor 0x100013a3). The ctor and
// dtor are private (??0DX7Screen@@AAE.., ??1DX7Screen@@EAE..): the rasterizer creates the screens while it enumerates
// the DirectDraw devices. Member names provisional where no getter names them.
#ifndef DX7_DX7SCREEN_H
#define DX7_DX7SCREEN_H

#include <vector>

#include "dx7/DX7Common.h"
#include "lle/GERectangle.h"
#include "lle/GEScreen.h"

class DX7_API DX7Screen : public GEScreen {
    friend class DX7Rasterizer;
    friend class DX7TnLRasterizer;
    // the rasterizer's DirectDraw enumeration callbacks create and delete the screens (DX7Rasterizer.cpp)
    friend BOOL WINAPI DX7DirectDrawEnumerateCallback(GUID* inGUID, LPSTR inDescription, LPSTR inName,
                                                      LPVOID inContext);
    friend BOOL WINAPI DX7DirectDrawEnumerateExCallback(GUID* inGUID, LPSTR inDescription, LPSTR inName,
                                                        LPVOID inContext, HMONITOR inMonitor);
public:
    operator IDirectDraw7*() { return this->mDirectDraw; }

    virtual long Capture(const GEWindow* inWindow, bool inToClipboard);
    virtual long CreateSurface(GESurfaceDescription& inDescription, GESurface** outSurface);
    virtual long CreateAWindow(const GE2DPoint& inOrigin, unsigned long inWidth, unsigned long inHeight,
                               UString& inClassName, UString& inTitle, GEWindow*& outWindow, unsigned long inStyle,
                               GEWindow* inParent);
    virtual bool Get32BitDeviceSupport() { return this->m32BitDeviceSupport; }
    virtual bool Get32BitTextureSupport() { return this->m32BitTextureSupport; }
    virtual bool GetAttachedToDesktop() const { return this->mAttachedToDesktop; }
    virtual bool GetCanMagFilterSurfaces() const { return this->mCanMagFilterSurfaces; }
    virtual bool GetCanMinFilterSurfaces() const { return this->mCanMinFilterSurfaces; }
    virtual const GEVideoMode& GetCurrentMode() const { return this->mModes[this->mCurrentModeIndex]; }
    virtual unsigned long GetCurrentModeIndex() const { return this->mCurrentModeIndex; }
    virtual const GERectangle& GetDesktopRectangle();
    virtual bool GetIsFullScreen() const { return this->mIsFullScreen; }
    virtual const GEVideoMode& GetMode(unsigned long inIndex) const { return this->mModes[inIndex]; }
    virtual unsigned long GetModeCount() const { return this->mModes.size(); }
    virtual const UString& GetName() const { return *this->mName; }
    static DX7Screen* GetScreen(unsigned long inIndex) { return DX7Screen::sScreens[inIndex]; }
    static unsigned long GetScreenCount() { return DX7Screen::sScreens.size(); }
    virtual bool GetSharesPrimary() { return this->mSharesPrimary; }
    virtual long Release(const GEWindow* inWindow);
    virtual long RestoreAllSurfaces();
    virtual long SetMode(unsigned long inIndex);

private:
    DX7Screen(HMONITOR__* inMonitor, IDirectDraw7* inDirectDraw, const UString& inName, bool inAttachedToDesktop,
              bool inSharesPrimary);
    virtual ~DX7Screen();
    static long __stdcall EnumTextureFormatCallback(_DDPIXELFORMAT* inFormat, void* inContext);
    static int __stdcall EnumVideoModeCallback(_DDSURFACEDESC2* inDescription, void* inContext);
    void Test32BitSupport();
    static LRESULT CALLBACK TestWindowProc(HWND inWindow, UINT inMessage, WPARAM inWParam, LPARAM inLParam);

    std::vector<GEVideoMode> mModes;        // +0x0c
    GERectangle mDesktopRectangle;          // +0x1c
    UString* mName;                         // +0x2c
    IDirectDraw7* mDirectDraw;              // +0x30
    HMONITOR__* mMonitor;                   // +0x34
    unsigned long mCurrentModeIndex;        // +0x38
    bool mAttachedToDesktop;                // +0x3c
    bool mIsFullScreen;                     // +0x3d
    bool mSharesPrimary;                    // +0x3e
    bool mCanMagFilterSurfaces;             // +0x3f
    bool mCanMinFilterSurfaces;             // +0x40
    bool m32BitDeviceSupport;               // +0x41
    bool m32BitTextureSupport;              // +0x42

    static std::vector<DX7Screen*> sScreens;
    static unsigned long sScreensCaptured;
    static bool sFirstCaptureWasAttachedToDesktop;
};

#endif
