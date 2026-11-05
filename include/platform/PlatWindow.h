// Platform layer: the game window and its input (see platform/Platform.h): window creation, the event queue
// (window, keyboard, mouse, text input), keyboard and mouse state, the cursor, the clipboard, text input with an input
// method, and the native handles the renderer (GX on wgpu) builds its surface from. It replaces the Win32 window and
// message loop (Empire Earth.cpp WinMain/WndProc, TSThread's message dispatch), DirectInput (the Input System) and
// the IMM calls of the edit boxes.
//
// Implementation: src/Platform/SDL/PlatWindow.cpp (SDL3: Windows, Linux X11/Wayland, macOS Cocoa), on every system
// (the game's window since PORT.md step 4.3).
//
// Keys are the game's numbering on every system (platform/PlatKeys.h): mScanCode is a DirectInput scan code
// (DIK_*), mVirtualKey a Windows virtual-key code (VK_*). Text is Windows-1252 bytes (platform/PlatString.h);
// characters outside it arrive as '?'. Coordinates are client-area pixels of the window, times PlatTimeMilliseconds.
#ifndef PLATFORM_PLATWINDOW_H
#define PLATFORM_PLATWINDOW_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---- the window system
// The application's name and icon, before PlatWindowSystemStart: the window class name on Win32 (the test harness
// finds the game window by it: "Empire Earth Expansion"), the X11 WM_CLASS / Wayland app id elsewhere; inIconResource
// is the Win32 icon resource id of the windows (0: none; ignored elsewhere).
void PlatWindowSetAppName(const char* inName, uint32_t inIconResource);
// Starts the window system (and installs its message box as PlatMessageSetHandler's handler) -> nonzero on success.
int PlatWindowSystemStart(void);
void PlatWindowSystemStop(void);

typedef struct PlatWindow PlatWindow;
#define PLAT_WINDOW_RESIZABLE 0x1u
#define PLAT_WINDOW_FULLSCREEN 0x2u             // borderless on the whole display (no display mode change)
#define PLAT_WINDOW_HIDDEN 0x4u
#define PLAT_WINDOW_HIGH_DPI 0x8u               // client size in physical pixels on high-density displays
#define PLAT_WINDOW_BORDERLESS 0x10u            // no frame (a Win32 WS_POPUP window)
#define PLAT_WINDOW_TOPMOST 0x20u               // above the other windows (WS_EX_TOPMOST)
// -> NULL on failure (the reason in PlatWindowLastError)
PlatWindow* PlatWindowCreate(const char* inTitle, int32_t inWidth, int32_t inHeight, uint32_t inFlags);
// the same with the client area's origin at inX, inY (desktop pixels)
PlatWindow* PlatWindowCreateAt(const char* inTitle, int32_t inX, int32_t inY, int32_t inWidth, int32_t inHeight,
                               uint32_t inFlags);
void PlatWindowDestroy(PlatWindow* inWindow);
const char* PlatWindowLastError(void);
void PlatWindowSetTitle(PlatWindow* inWindow, const char* inTitle);
void PlatWindowShow(PlatWindow* inWindow, int inShow);
int PlatWindowSetFullscreen(PlatWindow* inWindow, int inFullscreen);
void PlatWindowSetSize(PlatWindow* inWindow, int32_t inWidth, int32_t inHeight);
// a window whose size the game's mode changes leave alone (a windowed game: the frames scale to the window)
void PlatWindowSetFixedSize(PlatWindow* inWindow, int inFixed);
// the client area's origin in desktop pixels (ClientToScreen of 0, 0), and moving it there
void PlatWindowGetPosition(PlatWindow* inWindow, int32_t* outX, int32_t* outY);
void PlatWindowSetPosition(PlatWindow* inWindow, int32_t inX, int32_t inY);
void PlatWindowMinimize(PlatWindow* inWindow);
// brings the window to the front with the keyboard focus (SetForegroundWindow / SetFocus)
void PlatWindowRaise(PlatWindow* inWindow);
// the drawable size in pixels (what the renderer's surface is configured with)
void PlatWindowGetClientSize(PlatWindow* inWindow, int32_t* outWidth, int32_t* outHeight);
// the display's size in pixels (the game's mode list: the resolutions that fit)
int PlatWindowGetDisplaySize(PlatWindow* inWindow, int32_t* outWidth, int32_t* outHeight);
// the display's size in its own units (macOS: points, half the pixels on a Retina display; elsewhere the pixels):
// the resolution a full screen game defaults to
int PlatWindowGetDisplayLogicalSize(PlatWindow* inWindow, int32_t* outWidth, int32_t* outHeight);

// The native handles for a wgpu surface (WGPUSurfaceSourceWindowsHWND / XlibWindow / WaylandSurface / MetalLayer)
typedef enum PlatNativeKind {
    PLAT_NATIVE_NONE = 0,
    PLAT_NATIVE_WIN32 = 1,                  // mDisplay HINSTANCE, mWindow HWND
    PLAT_NATIVE_X11 = 2,                    // mDisplay Display*, mWindow the X11 Window id (an integer)
    PLAT_NATIVE_WAYLAND = 3,                // mDisplay wl_display*, mWindow wl_surface*
    PLAT_NATIVE_METAL = 4                   // mDisplay NULL, mWindow CAMetalLayer*
} PlatNativeKind;
typedef struct PlatNativeWindow {
    PlatNativeKind mKind;
    void* mDisplay;
    void* mWindow;
} PlatNativeWindow;
int PlatWindowGetNative(PlatWindow* inWindow, PlatNativeWindow* outNative);
// Win32 only: a hook that sees every message of the event thread before it is dispatched (MM_MCINOTIFY for CD audio,
// the WON lobby's window messages); a nonzero result drops the message. NULL removes it. Ignored elsewhere.
typedef int (*PlatNativeMessageHook)(void* inWindow, uint32_t inMessage, uint64_t inWParam, int64_t inLParam);
void PlatWindowSetNativeMessageHook(PlatNativeMessageHook inHook);

// ---- events
typedef enum PlatInputEventType {
    PLAT_EVENT_NONE = 0,
    PLAT_EVENT_QUIT,                        // the application is asked to end (last window closed, Cmd-Q, signal)
    PLAT_EVENT_CLOSE,                       // the window's close button
    PLAT_EVENT_RESIZED,                     // mSize: the new client size in pixels
    PLAT_EVENT_FOCUS_GAINED,
    PLAT_EVENT_FOCUS_LOST,
    PLAT_EVENT_MINIMIZED,
    PLAT_EVENT_RESTORED,
    PLAT_EVENT_EXPOSED,                     // the window needs repainting
    PLAT_EVENT_KEY_DOWN,                    // mKey (mRepeat nonzero for auto-repeat)
    PLAT_EVENT_KEY_UP,
    PLAT_EVENT_TEXT,                        // mText: committed text (WM_CHAR), Windows-1252
    PLAT_EVENT_TEXT_EDITING,                // mText: the input method's composition, mCursor / mLength inside it
    PLAT_EVENT_MOUSE_MOVE,                  // mMouse mX, mY (client) and mDeltaX, mDeltaY (raw motion when relative)
    PLAT_EVENT_MOUSE_DOWN,                  // mMouse mButton (1 left, 2 middle, 3 right, 4, 5), mClicks (2: double)
    PLAT_EVENT_MOUSE_UP,
    PLAT_EVENT_MOUSE_WHEEL                  // mMouse mWheel: notches, positive away from the user (WHEEL_DELTA 120)
} PlatInputEventType;

#define PLAT_MOD_SHIFT 0x1u
#define PLAT_MOD_CONTROL 0x2u
#define PLAT_MOD_ALT 0x4u
#define PLAT_MOD_GUI 0x8u                   // Windows / Command key
#define PLAT_MOD_CAPS 0x10u
#define PLAT_MOD_NUM 0x20u

typedef struct PlatInputEvent {
    uint32_t mType;                         // PlatInputEventType
    uint32_t mTime;                         // PlatTimeMilliseconds
    PlatWindow* mWindow;
    union {
        struct {
            uint32_t mScanCode;             // DIK_* (0 when the key has none)
            uint32_t mVirtualKey;           // VK_* (0 when the key has none)
            uint32_t mModifiers;            // PLAT_MOD_*
            int32_t mRepeat;
        } mKey;
        struct {
            int32_t mX, mY;
            int32_t mDeltaX, mDeltaY;
            uint32_t mButton;
            uint32_t mClicks;
            float mWheel;
            uint32_t mModifiers;
        } mMouse;
        struct {
            char mText[64];                 // zero-terminated
            int32_t mCursor;
            int32_t mLength;
        } mText;
        struct {
            int32_t mWidth, mHeight;
        } mSize;
    };
} PlatInputEvent;
// The next event -> nonzero when there was one. Wait blocks up to inTimeoutMs (PLAT_WAIT_INFINITE of
// platform/PlatThread.h: forever). Both must run on the thread that started the window system (the game's main
// thread; macOS requires it).
int PlatWindowPollEvent(PlatInputEvent* outEvent);
int PlatWindowWaitEvent(PlatInputEvent* outEvent, uint32_t inTimeoutMs);

// ---- keyboard and mouse state (GetKeyState / GetAsyncKeyState, DirectInput's device state)
int PlatKeyIsDown(uint32_t inVirtualKey);          // VK_*: held now
int PlatKeyIsToggled(uint32_t inVirtualKey);       // VK_CAPITAL / VK_NUMLOCK / VK_SCROLL: the lock is on
int PlatScanCodeIsDown(uint32_t inScanCode);       // DIK_*: held now
uint32_t PlatModifiers(void);                      // PLAT_MOD_*
// the pointer in client coordinates, and the buttons held (bit 0 left, 1 middle, 2 right) -> the buttons
uint32_t PlatMouseGetState(PlatWindow* inWindow, int32_t* outX, int32_t* outY);
// relative mode: the pointer is hidden and held in the window, motion arrives as mDeltaX/mDeltaY (DirectInput's
// exclusive mouse) -> nonzero on success
int PlatMouseSetRelative(PlatWindow* inWindow, int inRelative);
void PlatMouseWarp(PlatWindow* inWindow, int32_t inX, int32_t inY);
void PlatCursorShow(int inShow);
// confines the pointer to the window (ClipCursor) or frees it
void PlatMouseConfine(PlatWindow* inWindow, int inConfine);

// ---- clipboard (text, Windows-1252)
int PlatClipboardSetText(const char* inText);
// -> a malloc'ed copy of the clipboard's text (free() it), NULL when it holds none
char* PlatClipboardGetText(void);

// ---- text input (WM_CHAR and the input method: UIEditBox, UIIMECandidate)
// Starts delivering PLAT_EVENT_TEXT / TEXT_EDITING to inWindow; inX, inY, inWidth, inHeight is the edit area the
// input method places its candidate window by.
void PlatTextInputStart(PlatWindow* inWindow, int32_t inX, int32_t inY, int32_t inWidth, int32_t inHeight);
void PlatTextInputStop(PlatWindow* inWindow);
// Cancels the input method's open composition (ImmNotifyIME(NI_COMPOSITIONSTR, CPS_CANCEL)): the edit box lost focus
void PlatTextInputCancel(PlatWindow* inWindow);

#ifdef __cplusplus
}
#endif

#endif
