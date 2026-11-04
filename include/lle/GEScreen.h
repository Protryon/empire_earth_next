// C:\EEX\Low-Level Engine\Graphics Engine\GEScreen.h (RECONSTRUCTION). Abstract (DX7HRDisplay DX7Screen 0x10470
// gives the slot order); the protected virtual dtor is the last slot (0x4c). All non-virtual members are inline.
#ifndef LLE_GESCREEN_H
#define LLE_GESCREEN_H

class UString;
class GE2DPoint;
class GERectangle;
class GESurface;
class GETextureManager;
class GEWindow;
struct GESurfaceDescription;
// A display mode (DX7HRDisplay: DX7Screen::EnumVideoModeCallback 0x1000dc67 fills it from a DDSURFACEDESC2, SetMode
// 0x1000de5d and the ctor read it back; vector element stride 0x14). Names are provisional.
struct GEVideoMode {
    unsigned long mWidth;                   // +0x00
    unsigned long mHeight;                  // +0x04
    unsigned long mRefreshRate;             // +0x08
    unsigned long mBitCount;                // +0x0c
    unsigned long mPixelFormat;             // +0x10 GE pixel format index (14 when no known format matches)
};
struct HINSTANCE__;

class LLE_API GEScreen {                    // sizeof 0xc (copy ctor 0xe331)
public:
    virtual long Capture(const GEWindow* inWindow, bool inToClipboard) = 0;
    virtual long CreateSurface(GESurfaceDescription& inDescription, GESurface** outSurface) = 0;
    virtual long CreateAWindow(const GE2DPoint& inOrigin, unsigned long inWidth, unsigned long inHeight,
                               UString& inClassName, UString& inTitle, GEWindow*& outWindow, unsigned long inStyle,
                               GEWindow* inParent) = 0;
#ifdef __clang__
    // VC6 bound temporaries to the non-const reference parameters above; clang does not. These const overloads
    // forward to them (clang build only; remove once the parameters themselves are const).
    long CreateAWindow(const GE2DPoint& inOrigin, unsigned long inWidth, unsigned long inHeight,
                       const UString& inClassName, const UString& inTitle, GEWindow*& outWindow, unsigned long inStyle,
                       GEWindow* inParent)
        { return CreateAWindow(inOrigin, inWidth, inHeight, const_cast<UString&>(inClassName),
                               const_cast<UString&>(inTitle), outWindow, inStyle, inParent); }
#endif
    virtual bool Get32BitDeviceSupport() = 0;
    virtual bool Get32BitTextureSupport() = 0;
    virtual bool GetAttachedToDesktop() const = 0;
    virtual bool GetCanMagFilterSurfaces() const = 0;
    virtual bool GetCanMinFilterSurfaces() const = 0;
    virtual const GEVideoMode& GetCurrentMode() const = 0;
    virtual unsigned long GetCurrentModeIndex() const = 0;
    virtual const GERectangle& GetDesktopRectangle() = 0;
    virtual bool GetIsFullScreen() const = 0;
    virtual const GEVideoMode& GetMode(unsigned long inIndex) const = 0;
    virtual unsigned long GetModeCount() const = 0;
    virtual const UString& GetName() const = 0;
    virtual bool GetSharesPrimary() = 0;
    virtual long Release(const GEWindow* inWindow) = 0;
    virtual long RestoreAllSurfaces() = 0;
    virtual long SetMode(unsigned long inIndex) = 0;

    GETextureManager* GetTextureManager() const { return this->mTextureManager; }
    unsigned long GetRasterizerIndex() const { return this->mRasterizerIndex; }
    void SetTextureManager(GETextureManager* inManager) { this->mTextureManager = inManager; }
    void SetRasterizerIndex(unsigned long inIndex) { this->mRasterizerIndex = inIndex; }
protected:
    GEScreen() {}                           // 0xe311
    virtual ~GEScreen() {}                  // 0xe31a, slot 0x4c
    static HINSTANCE__* GetInstance() { return GEScreen::sInstance; }
    static void SetInstance(HINSTANCE__* inInstance) { GEScreen::sInstance = inInstance; }

    GETextureManager* mTextureManager;      // +0x04
    unsigned long mRasterizerIndex;         // +0x08
private:
    friend LLE_API long GEInitialize(class FSFileSpec& inDirectory, HINSTANCE__* const inInstance);   // sets sInstance
    static HINSTANCE__* sInstance;          // exported ?sInstance@GEScreen@@0PAUHINSTANCE__@@A
};

#endif
