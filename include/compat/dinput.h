// POSIX only: what the Input System's headers (lle/ISKeyboard.h, lle/ISMouse.h) declare with DirectInput 7 types,
// as data only: the scan codes (platform/PlatKeys.h), the mouse state and buffered-data records, the device as an
// opaque type. No DirectInput function: the Input System's units (Input System.cpp, ISKeyboard.cpp, ISMouse.cpp)
// are rewritten on the window layer's events (PORT.md step 4), everything that only includes their headers
// compiles.
#ifndef EE_COMPAT_DINPUT_H
#define EE_COMPAT_DINPUT_H

#include "platform/PlatKeys.h"
#include "platform/wintypes.h"

#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0700
#endif

typedef struct _DIMOUSESTATE {
    LONG lX;
    LONG lY;
    LONG lZ;
    BYTE rgbButtons[4];
} DIMOUSESTATE, *LPDIMOUSESTATE;

typedef struct DIDEVICEOBJECTDATA {
    DWORD dwOfs;
    DWORD dwData;
    DWORD dwTimeStamp;
    DWORD dwSequence;
} DIDEVICEOBJECTDATA, *LPDIDEVICEOBJECTDATA;

#define DIMOFS_X 0
#define DIMOFS_Y 4
#define DIMOFS_Z 8
#define DIMOFS_BUTTON0 12
#define DIMOFS_BUTTON1 13
#define DIMOFS_BUTTON2 14
#define DIMOFS_BUTTON3 15

#define DI_OK S_OK
#define DI_BUFFEROVERFLOW S_FALSE

struct IDirectInputDevice;
struct IDirectInput;
typedef struct IDirectInputDevice* LPDIRECTINPUTDEVICE;
typedef struct IDirectInput* LPDIRECTINPUT;

#endif
