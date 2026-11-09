// The original's Win32 GDI backend of the WONAPI GUI (MSWindowManager, MSWindow, MSGraphics, MSNativeImage,
// MSNativeMask, MSNativeIcon, MSFont, MSCursor), split out of WONLobbyGUI.h. The port does not compile these
// (units.WON_REPLACED): the backend on every system is WONLobbyPlatform.h. The declarations stay, as the MS*.cpp
// sources do, so upstream changes to them still merge; they name DisplayContext::mDC, which the port replaced.
#ifndef EE_WONAPI_WONLOBBYGUIMS_H
#define EE_WONAPI_WONLOBBYGUIMS_H
#include <windows.h>
#include "ee/wonapi/WONLobbyGUI.h"

namespace WONAPI {

// Platform cursor (0x10 bytes); allocated at 0x3d0941/0x3d0271, vtable 0x45471c
class MSCursor : public Cursor {
protected:
    virtual ~MSCursor(); //3d0320 conditionalDestroyCursor thenbase
public:
    HCURSOR mNativeCursor; //08
    bool mDoDestroy; // at +0x0c; +0x0d..0x0f padding
    MSCursor(HCURSOR cursor,bool destroy=false) : mNativeCursor(cursor),mDoDestroy(destroy) {}
    virtual Cursor* GetNative() { return this; } // 0x3d02f0
};

// (0x14c bytes; vtable 0x85472c)
class MSWindowManager : public WindowManager {
public:
    HINSTANCE GetHinstance() { return this->mInstance; }
    MSWindowManager(HINSTANCE inInstance);  // 0x3d0900 (NULL: GetModuleHandle(NULL))
    virtual ~MSWindowManager(); // 0x3d0b60, forwards to the base destructor
    virtual void GetNextEvent(); //04 3d0c40
    virtual bool SetScreenResolutionHook(int,int); //08 3d0cf0
    virtual void GetScreenResolutionHook(int&,int&); //0c 3d0d50
    virtual void Signal(); //10 3d0cc0
    virtual void GetScreenRect(WONRectangle&); //1c 3d0c90
    virtual void GetSystemColorScheme(); //3d0b70 target noarg override

    HINSTANCE mInstance;                    // +0x148
};

// (0x10 bytes; vtable 0x84c6d4; ??_G 0x297531 is EELobbyInterface.cpp's COMDAT)
class MSNativeIcon : public NativeIcon {
public:
    MSNativeIcon(DisplayContext* inContext, HICON inIcon) : NativeIcon(inContext), mIcon(inIcon) {}
    virtual ~MSNativeIcon();                // 0x3cf9f0

    HICON mIcon;                            // +0x0c
};

// 0xc8-byte derived class; the later SDK's extra key-modifier field is absent.
class MSWindow : public Window {
public:
    HWND GetHWND() { return this->mHWND; }
    void SetMinimizeOnAltTab(bool minimize) { this->mMinimizeOnAltTab=minimize; }
    void SetForwardKeyMsgToParent(bool forward) { this->mForwardKeyMsgToParent=forward; }
    MSWindow();                            // 0x3d0d90, calls the Window constructor
    void CalcCodePage(); // 0x3d0e20; private helper
    void MyGetKeyState(); //3d15a0; writes targetcurKeyModbc
    void PostMouseEvent(WindowEventType,int,LPARAM); // 0x3d1600 (ret 0x0c)
    void DoMinimizeOnAltTab(); //3d16a0
    static LRESULT CALLBACK StaticWindowProc(HWND,UINT,WPARAM,LPARAM); // 0x3d1530, stdcall callback
    LRESULT WindowProc(HWND,UINT,WPARAM,LPARAM); // 0x3d16e0, thiscall
    virtual ~MSWindow();                   // 0x3d0de0, destroys the HWND
    virtual SmartPtr<Font> PlatformGetFont(const FontDescriptor&);
    virtual bool IdleUpdate();
    virtual void TimerEvent(DWORD);
    virtual void PlatformSetTitle();
    virtual void PlatformSetIcon();
    virtual void PlatformSetCursor(Cursor*);
    virtual void PlatformShowNativeCursor(bool);
    virtual void PlatformShow(bool);
    virtual void PlatformMinimize(bool);
    virtual void PlatformSetTopMost(bool);
    virtual void PlatformClose();
    virtual void Create(const WONRectangle&);
    virtual void SetPosSize(int, int, int, int);
    virtual void GetRect(WONRectangle&, bool);
    virtual void PlatformSetFocus();
    virtual void PlatformSetMouseCapture(bool);
    virtual bool PlatformIsVisible();
    virtual void OnPaint();                 // OnPaint3d23f0, no arguments
    virtual void OnCreate();                // OnCreate3d22c0, no arguments
    HWND mHWND;                            // +b8, target HWND
    DWORD mCurKeyMod;                      // +bc
    int mCodePage;                         // +c0, GetLocaleInfoA/atoi producer3d0e20
    bool mMinimizeOnAltTab;                // +c4
    bool mForwardKeyMsgToParent;           // +c5
};
// 0x3d08c0 (cdecl): the parent window of the MS windows created next (provisional name)
void SetMSWindowDefaultOwner(HWND inParent);

// MSGraphics (0x50 bytes): vtable 0x854688, created by the factory 0x3cfce0.
class MSGraphics : public Graphics {
public:
    MSGraphics(); //3cf0f0
    virtual ~MSGraphics(); //3cf130
    virtual void Attach(DisplayContext*); //3cf180
    virtual void Detach(); //3cf210
    virtual void SetColorHook(); //3cf290
    virtual DWORD GetPixelHook(int,int); //3cf310
    virtual void SetPixelHook(int,int,DWORD); //3cf330
    virtual void DrawLineHook(int,int,int,int); //3cf370
    virtual void DrawRectHook(int,int,int,int); //3cf3c0
    virtual void DrawDottedRectHook(int,int,int,int); //3cf410
    virtual void FillRectHook(int,int,int,int); //3cf430
    virtual bool BlendSupported(); //3cf460
    virtual void FillRectBlendHook(int,int,int,int,int); //3cf500
    virtual void DrawPolygonHook(POINT*,int); //3cf790
    virtual void FillPolygonHook(POINT*,int); //3cf7d0
    virtual void DrawEllipseHook(int,int,int,int); //3cf7f0
    virtual void FillEllipseHook(int,int,int,int); //3cf840
    virtual void DrawArcHook(int,int,int,int,int,int); //3cf870
    virtual void ClearClipRectHook(); //3cf970
    virtual void SetClipRectHook(int,int,int,int); //3cf980
    HDC mDC; //38
    HBRUSH mOriginalBrush; //3c
    HPEN mOriginalPen; //40
    HBRUSH mSolidBrush; //44
    HBRUSH mHollowBrush; //48
    HPEN mPen; //4c
};
// MSNativeMask (0x10 bytes): GetMask (0x3d06b0) allocates it and fills the base (0x0c bytes) and the handle at +0x0c.
class MSNativeMask : public NativeMask {
public:
    MSNativeMask(DisplayContext* context,HBITMAP mask) : NativeMask(context),mMask(mask) {}
    virtual ~MSNativeMask(); //3cf9b0
    HBITMAP GetMask() { return this->mMask; }
    HBITMAP mMask; //0c
};
// Bitmap holder (0x28 bytes); the later SDK's mImageData and useDibSection are absent.
class MSNativeImage : public NativeImage {
public:
    MSNativeImage(DisplayContext*,HBITMAP,MSNativeMask* = NULL); //3cfb70
    MSNativeImage(DisplayContext*,int,int); //3cfbf0
    virtual ~MSNativeImage(); //3cfc60
    void Init(DisplayContext*,HBITMAP,MSNativeMask*); //3cfa50
    void SelectBitmap(bool); //3cfa10
    virtual Graphics* PlatformGetGraphics(); //3cfce0, real PlatformGetGraphics
    virtual void Draw(DisplayContext*,int,int,int,int,int,int); //3cfd40
    virtual SmartPtr<RawImage32> GetRaw(); //3cfe50
    virtual SmartPtr<Image> Duplicate(); //3d0130
    virtual SmartPtr<NativeIcon> GetIcon(); //3d0350
    virtual SmartPtr<Cursor> GetCursor(int,int); // vtable +0x20, 0x3d0230
    HDC mDC; //18
    HBITMAP mBitmap; //1c
    HBITMAP mOldBitmap; //20
    bool mBitmapIsSelected; //24
};

// MSFont: 0x3cee60 stores HFONT at +0x20, HDC at +0x24, TEXTMETRICA at +0x28.
class MSFont : public Font {
public:
    MSFont(DisplayContext*, const FontDescriptor&); //3cee60
    virtual ~MSFont(); //3cef30
    virtual void PrepareContext(DisplayContext*); //3cef60 PrepareContext
    virtual void DrawString(DisplayContext*,const GUIString&,unsigned short,unsigned short,int,int); //3cef80
    virtual int GetStringWidth(const GUIString&,unsigned short,unsigned short); //3cf040
    virtual int GetCharacterWidth(int); //3cf090 GetCharWidth
    virtual int GetHeight(); //3cf0c0
    virtual int GetAscent(); //3cf0d0
    virtual int GetDescent(); //3cf0e0
    HFONT mFont; //20
    HDC mDC; //24
    TEXTMETRICA mTextMetrics; //28
};

} // namespace WONAPI

#endif
