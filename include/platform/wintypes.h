// The Windows types and macros the game's declarations are written with (DWORD, BOOL, RECT, POINT, HWND, TRUE,
// MAX_PATH, MAKEWORD, ...), without any Windows function: a header can use them on every system, and the calls go
// through the platform layer (platform/Platform.h). On Windows this is <windows.h> itself. Elsewhere it defines the
// types with the sizes Windows gives them (DWORD and LONG are 32-bit everywhere: on LP64 systems they are not
// `unsigned long`/`long`, which is the LP64 work of PORT.md), handles as distinct opaque pointer types, and the
// macros. include/compat/windows.h maps `#include <windows.h>` here on POSIX so the units compile while they are
// moved off Windows names (PORT.md step 4).
#ifndef PLATFORM_WINTYPES_H
#define PLATFORM_WINTYPES_H

#if defined(_WIN32)
#include <windows.h>
#else

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// ---- calling conventions and decorations (meaningless off Windows)
#define WINAPI
#define WINAPIV
#define APIENTRY
#define CALLBACK
#define PASCAL
#define FAR
#define NEAR
#define far
#define near
#ifndef __cdecl
#define __cdecl
#endif
#ifndef __stdcall
#define __stdcall
#endif
#ifndef __fastcall
#define __fastcall
#endif
#ifndef _MSC_VER
#define __int64 long long
#define __int32 int
#define __int16 short
#define __int8 char
#endif
#define CONST const
#define VOID void
#define IN
#define OUT
#define OPTIONAL
#define UNREFERENCED_PARAMETER(p) ((void)(p))

// ---- integer types (Windows sizes)
typedef int BOOL;
typedef unsigned char BYTE;
typedef unsigned char UCHAR;
typedef char CHAR;
typedef short SHORT;
typedef unsigned short USHORT;
typedef unsigned short WORD;
typedef uint32_t DWORD;
typedef int32_t LONG;
typedef uint32_t ULONG;
typedef int INT;
typedef unsigned int UINT;
typedef float FLOAT;
typedef unsigned char BOOLEAN;
typedef int64_t LONGLONG;
typedef uint64_t ULONGLONG;
typedef uint64_t DWORDLONG;
typedef intptr_t INT_PTR;
typedef uintptr_t UINT_PTR;
typedef intptr_t LONG_PTR;
typedef uintptr_t ULONG_PTR;
typedef uintptr_t DWORD_PTR;
typedef size_t SIZE_T;
typedef intptr_t SSIZE_T;
typedef unsigned short WCHAR;   // UTF-16 code unit, as Windows' wchar_t
typedef char TCHAR;
typedef WORD ATOM;
typedef DWORD COLORREF;
typedef LONG HRESULT;
typedef UINT_PTR WPARAM;
typedef LONG_PTR LPARAM;
typedef LONG_PTR LRESULT;

typedef void* PVOID;
typedef void* LPVOID;
typedef const void* LPCVOID;
typedef char* LPSTR;
typedef char* PSTR;
typedef const char* LPCSTR;
typedef const char* PCSTR;
typedef WCHAR* LPWSTR;
typedef const WCHAR* LPCWSTR;
typedef char* LPTSTR;
typedef const char* LPCTSTR;
typedef BYTE* LPBYTE;
typedef BYTE* PBYTE;
typedef WORD* LPWORD;
typedef DWORD* LPDWORD;
typedef DWORD* PDWORD;
typedef LONG* LPLONG;
typedef LONG* PLONG;
typedef BOOL* LPBOOL;
typedef INT* LPINT;
typedef UINT* PUINT;

// ---- handles: distinct opaque pointer types, as <windows.h> declares them (STRICT)
typedef void* HANDLE;
typedef HANDLE* LPHANDLE;
#define DECLARE_HANDLE(name) \
    struct name##__ {        \
        int unused;          \
    };                       \
    typedef struct name##__* name
DECLARE_HANDLE(HWND);
DECLARE_HANDLE(HINSTANCE);
DECLARE_HANDLE(HDC);
DECLARE_HANDLE(HGLRC);
DECLARE_HANDLE(HICON);
DECLARE_HANDLE(HBRUSH);
DECLARE_HANDLE(HFONT);
DECLARE_HANDLE(HBITMAP);
DECLARE_HANDLE(HPEN);
DECLARE_HANDLE(HPALETTE);
DECLARE_HANDLE(HRGN);
DECLARE_HANDLE(HMENU);
DECLARE_HANDLE(HKEY);
DECLARE_HANDLE(HIMC);
DECLARE_HANDLE(HMONITOR);
DECLARE_HANDLE(HHOOK);
DECLARE_HANDLE(HACCEL);
typedef HINSTANCE HMODULE;
typedef HICON HCURSOR;
typedef void* HGDIOBJ;
typedef HANDLE HGLOBAL;
typedef HANDLE HLOCAL;
typedef int HFILE;
typedef HKEY* PHKEY;

// ---- structures
typedef struct tagRECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} RECT, *PRECT, *LPRECT;
typedef const RECT* LPCRECT;
typedef struct tagPOINT {
    LONG x;
    LONG y;
} POINT, *PPOINT, *LPPOINT;
typedef struct tagSIZE {
    LONG cx;
    LONG cy;
} SIZE, *PSIZE, *LPSIZE;
typedef struct tagPOINTS {
    SHORT x;
    SHORT y;
} POINTS;
typedef union _LARGE_INTEGER {
    struct {
        DWORD LowPart;
        LONG HighPart;
    };
    struct {
        DWORD LowPart;
        LONG HighPart;
    } u;
    LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;
typedef union _ULARGE_INTEGER {
    struct {
        DWORD LowPart;
        DWORD HighPart;
    };
    struct {
        DWORD LowPart;
        DWORD HighPart;
    } u;
    ULONGLONG QuadPart;
} ULARGE_INTEGER, *PULARGE_INTEGER;
typedef struct _FILETIME {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;
typedef struct _SYSTEMTIME {
    WORD wYear;
    WORD wMonth;
    WORD wDayOfWeek;
    WORD wDay;
    WORD wHour;
    WORD wMinute;
    WORD wSecond;
    WORD wMilliseconds;
} SYSTEMTIME, *PSYSTEMTIME, *LPSYSTEMTIME;
typedef struct _GUID {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t Data4[8];
} GUID;
typedef GUID IID;
typedef GUID CLSID;
typedef struct tagRGBQUAD {
    BYTE rgbBlue;
    BYTE rgbGreen;
    BYTE rgbRed;
    BYTE rgbReserved;
} RGBQUAD;
// the bitmap file's headers (GESurface writes screenshots as .bmp): Windows' layouts, packed as in <wingdi.h>
#pragma pack(push, 2)
typedef struct tagBITMAPFILEHEADER {
    WORD bfType;
    DWORD bfSize;
    WORD bfReserved1;
    WORD bfReserved2;
    DWORD bfOffBits;
} BITMAPFILEHEADER;
#pragma pack(pop)
typedef struct tagBITMAPINFOHEADER {
    DWORD biSize;
    LONG biWidth;
    LONG biHeight;
    WORD biPlanes;
    WORD biBitCount;
    DWORD biCompression;
    DWORD biSizeImage;
    LONG biXPelsPerMeter;
    LONG biYPelsPerMeter;
    DWORD biClrUsed;
    DWORD biClrImportant;
} BITMAPINFOHEADER;
#ifdef __cplusplus
static_assert(sizeof(BITMAPFILEHEADER) == 14 && sizeof(BITMAPINFOHEADER) == 40, "the .bmp headers' sizes");
#endif
#define BI_RGB 0
// the thread message the UI thread quits on (TSThread::Post 0x12; Win32's WM_QUIT)
#define WM_QUIT 0x0012
typedef struct tagPALETTEENTRY {
    BYTE peRed;
    BYTE peGreen;
    BYTE peBlue;
    BYTE peFlags;
} PALETTEENTRY, *PPALETTEENTRY, *LPPALETTEENTRY;

// ---- values and macros
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#define MAX_PATH 260
#define _MAX_PATH 260
#define _MAX_DRIVE 3
#define _MAX_DIR 256
#define _MAX_FNAME 256
#define _MAX_EXT 256
#define INFINITE 0xFFFFFFFFu
#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)
#define ERROR_SUCCESS 0L
#define NO_ERROR 0L
#define S_OK ((HRESULT)0L)
#define S_FALSE ((HRESULT)1L)
#define E_FAIL ((HRESULT)0x80004005L)
#define E_NOTIMPL ((HRESULT)0x80004001L)
#define E_OUTOFMEMORY ((HRESULT)0x8007000EL)
#define E_INVALIDARG ((HRESULT)0x80070057L)
#define E_POINTER ((HRESULT)0x80004003L)
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr) (((HRESULT)(hr)) < 0)

#define MAKEWORD(a, b) ((WORD)(((BYTE)(((DWORD_PTR)(a)) & 0xff)) | ((WORD)((BYTE)(((DWORD_PTR)(b)) & 0xff))) << 8))
#define MAKELONG(a, b) ((LONG)(((WORD)(((DWORD_PTR)(a)) & 0xffff)) | ((DWORD)((WORD)(((DWORD_PTR)(b)) & 0xffff))) << 16))
#define LOWORD(l) ((WORD)(((DWORD_PTR)(l)) & 0xffff))
#define HIWORD(l) ((WORD)((((DWORD_PTR)(l)) >> 16) & 0xffff))
#define LOBYTE(w) ((BYTE)(((DWORD_PTR)(w)) & 0xff))
#define HIBYTE(w) ((BYTE)((((DWORD_PTR)(w)) >> 8) & 0xff))
#define MAKELPARAM(l, h) ((LPARAM)(DWORD)MAKELONG(l, h))
#define MAKEWPARAM(l, h) ((WPARAM)(DWORD)MAKELONG(l, h))
#define MAKELRESULT(l, h) ((LRESULT)(DWORD)MAKELONG(l, h))
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#define RGB(r, g, b) ((COLORREF)(((BYTE)(r) | ((WORD)((BYTE)(g)) << 8)) | (((DWORD)(BYTE)(b)) << 16)))
#define GetRValue(rgb) (LOBYTE(rgb))
#define GetGValue(rgb) (LOBYTE(((WORD)(rgb)) >> 8))
#define GetBValue(rgb) (LOBYTE((rgb) >> 16))
#define ZeroMemory(d, n) memset((d), 0, (n))
#define FillMemory(d, n, f) memset((d), (f), (n))
#define CopyMemory(d, s, n) memcpy((d), (s), (n))
#define MoveMemory(d, s, n) memmove((d), (s), (n))

// <windows.h>'s min and max (the game calls them unqualified; NOMINMAX turns them off, as on Windows). Not macros:
// libstdc++'s <bits/c++config.h> #undefs min and max, so every standard header after this one would remove them.
// Function templates of two types give the macros' result (the usual arithmetic conversions of a ?: b); with both
// arguments of one type, std::min/std::max (more specialised) win where `using namespace std` brings them in.
#if defined(__cplusplus) && !defined(NOMINMAX)
#include <type_traits>
#include <utility>
template <class A, class B>
inline typename std::decay<decltype(true ? std::declval<A>() : std::declval<B>())>::type max(const A& a, const B& b)
{
    return a > b ? a : b;
}
template <class A, class B>
inline typename std::decay<decltype(true ? std::declval<A>() : std::declval<B>())>::type min(const A& a, const B& b)
{
    return a < b ? a : b;
}
#endif

// the virtual-key codes (and DirectInput's scan codes): the game's key numbering
#include "platform/PlatKeys.h"

#endif   // !_WIN32
#endif
