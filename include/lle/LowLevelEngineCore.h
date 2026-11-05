// The non-class part of the provisional engine API (LLE_API, the ES/TM/U/UM free functions, the startup paths),
// split from lle/LowLevelEngine.h so that a unit can see it without the per-class headers (inferred header).
// Error System.cpp includes only this and the string/file/scheduler headers: the first unit in link order emits
// the exported inline members of every class it sees, and the original's emits only UWideString, UString, FSFile,
// FSBasicFile and FSFileSpec (0x10001000..0x1000144b).
#ifndef LLE_LOWLEVELENGINECORE_H
#define LLE_LOWLEVELENGINECORE_H

// The engine's own units are built with LOWLEVELENGINE_EXPORTS (VC6 DLL-wizard convention). EE_STATIC: everything is
// linked into one exe, so the other units neither import nor export. The engine units still declare their classes
// dllexport: clang-cl emits a class's real vector deleting destructor (??_E, what `delete[]` of a polymorphic array
// calls through the vtable) only for exported classes or in a unit that holds both the vtable and a new[]/delete[];
// otherwise ??_E is a weak alias of the scalar ??_G, and a `delete[]` elsewhere frees the array past its cookie. The
// exe gets an export table; nothing is loaded dynamically. (PORT_LEARNINGS.md "One static exe")
#if defined(LOWLEVELENGINE_EXPORTS)
#define LLE_API __declspec(dllexport)
#elif defined(EE_STATIC)
#define LLE_API
#else
#define LLE_API __declspec(dllimport)
#endif

#include <math.h>     // lrintf (UMFloatToLong)
#include <stdint.h>   // uintptr_t (USavedID)
#include <vector>   // UString::Tokenize
#include "platform/Platform.h"   // the platform layer (time, threads, ...): every engine and game unit sees it

// ---- Error system (ES)
// proven: "theResult == keseOK" assert text. Other values: codes returned by the Thread/Time System units;
// names are provisional.
enum ESError {
    kErrorOK = 0,
    keseOK = kErrorOK,  // spelled this way in the original's assert strings
    kErrorNativeError = 1,                    // after ESRecordNativeError(GetLastError()) (TSEvent::Set 0x53d09)
    kErrorNotSuspended = 2,                   // TSThread::Resume: ResumeThread returned 0
    kErrorNotFound = 3,                       // TSScheduler::DestroyThread, TSThread::Receive (no message), TMTimer
    kErrorBusy = 5,                           // TMTimer::Set* while running
    kErrorAlreadyExists = 2,                  // FSFileSpec::Create (ERROR_FILE_EXISTS), CreateDirectory (ERROR_ALREADY_
                                            // EXISTS), Lock/Unlock (already in that state); same value as
                                            // kErrorNotSuspended: the real name of 2 is unknown
    kErrorNotOpen = 4,                        // FSMemoryFile::Read with no buffer; FSFileSpec I/O on a closed file
    kErrorNoMoreFiles = 7,                    // FSFileSpec::GetNextFile: ERROR_NO_MORE_FILES
    kErrorBeyondEnd = 0xa,                    // FSFileSpec::SetPosition past the end of the file
    kErrorNotWritable = 0xf,                  // FSFileSpec::Write/SetLength without write access
    kErrorNotReadable = 0x10,                 // FSFileSpec::Read without read access
    kErrorAlreadyOpen = 5,                    // FSBasicFile::Delete on an open file
    kErrorOutOfMemory = 8,                    // operator new failed (TSScheduler::Create*)
    kErrorFailed = 9,                         // TSInitialize: URegisterShutdown failed; FSMemoryFile's unsupported
                                            // operations (DeleteDirectory, Lock, GetFirstFile, ...)
    kErrorBadPath = 0xe,                      // FSBasicFile::Delete / FSFileSpec::Open: path longer than MAX_PATH
    kErrorIOFailed = 0xe,                     // FSDebugFile::ReadText/WriteText: short fread/fwrite (same value as
                                            // kErrorBadPath; real name unknown)
    kErrorNotSupported = 0xd,                 // FSDebugFile::Copy
    kErrorTimeout = 0x11,                     // TSEvent::Wait: WAIT_TIMEOUT
    kErrorEndOfFile = 0x12                    // FSMemoryFile::Read past the end
};

LLE_API void ESVerify(const char* inFile, unsigned long inLine, const char* inExpression);
LLE_API unsigned long ESRecordNativeError(unsigned long inError);
LLE_API unsigned long ESGetNativeError();
LLE_API void ESDebugOut(const char* inFormat, ...);   // proven ?ESDebugOut@@YAXPBDZZ (Default.dll)
class FSFileSpec;
LLE_API long ESBeginRecording(FSFileSpec& inSpec);    // proven ?ESBeginRecording@@YAJAAVFSFileSpec@@@Z (exe import)
LLE_API long ESEndRecording();                        // proven ?ESEndRecording@@YAJXZ (exe import)
LLE_API void ESInitialize(HINSTANCE inInstance);      // proven ?ESInitialize@@YAXPAUHINSTANCE__@@@Z (exe import)
LLE_API void ESDumpStack(unsigned long inDepth);       // exported ?ESDumpStack@@YAXK@Z (empty; provisional parameter name)
LLE_API void ESReportMessage(const char* inFormat, ...);   // exported ?ESReportMessage@@YAXPBDZZ (empty)

class UString;

// inferred: release-build verify macro; the call passes __FILE__, __LINE__ and the expression text
#define ESVERIFY(expr) if (!(expr)) ESVerify(__FILE__, __LINE__, #expr)
// ESVERIFY_AT carries the original's __LINE__ explicitly (owner 2026-11-02: line-number parity dropped,
// so sources need no blank-line padding; N is the line the original pushes)
#define ESVERIFY_AT(expr, line) if (!(expr)) ESVerify(__FILE__, line, #expr)

// ---- Timing (TM): Time Manager unit 0x53701..0x53819 (folder/file name inferred)
struct TMTime {                             // filled from the local time by TMGetTimeOfDay 0x5376c
    unsigned long mMonth;                   // +0x00 0-based (wMonth - 1)
    unsigned long mDay;                     // +0x04
    unsigned long mYear;                    // +0x08
    unsigned long mHour;                    // +0x0c
    unsigned long mMinute;                  // +0x10
    unsigned long mSecond;                  // +0x14
};
LLE_API long TMInitialize();
LLE_API unsigned long TMGetTime();
LLE_API int64_t TMGetMicroTime();
LLE_API void TMGetTimeOfDay(TMTime& outTime);
// TMPoll.h / TMTimer.h are NOT included here: every static data member of a class a unit sees consumes a VC6
// `$E` number (probe: one per member; extern objects consume none), and the plug-ins' $E numbering proves they
// saw no engine-class static data members (TMPoll::sCounterFrequency is one). Engine units include them directly.

// ---- Shutdown registry (Utilities): engine subsystems register cleanup functions
LLE_API long URegisterShutdown(void (*inFunction)(bool));
LLE_API void UShutdown(bool inForced, bool inTerminate);    // Utilities.cpp 0x83838; provisional parameter names
LLE_API bool UShutdownInProgress();
LLE_API unsigned short UGetOSLanguage();
LLE_API unsigned long UGetOSType();                         // 1: 95, 2: 98/ME, 3: NT 5+, 4: NT 4 or older (inferred meanings)

// ---- Utility math (UM)
LLE_API void UMSetFloatingPointPrecision(unsigned long inPrecision);
LLE_API unsigned long UMGetFloatingPointPrecision();
LLE_API void UMSetFloatingPointRoundingMode(unsigned long inMode);
LLE_API unsigned long UMGetFloatingPointRoundingMode();
LLE_API float UMArcTan2(float inY, float inX);  // proven ?UMArcTan2@@YAMMM@Z (Default.dll CMUSpreadTeams 0x3aa3)
LLE_API float UMCos(float inAngle);             // proven ?UMCos@@YAMM@Z (Default.dll import)
LLE_API float UMArcCos(float inValue);          // proven ?UMArcCos@@YAMM@Z (GETransformation::GetPitch 0xd7dd)
LLE_API float UMArcTan(float inValue);          // proven ?UMArcTan@@YAMM@Z (GEViewport::ComputePlanes 0x37270)
LLE_API float UMTan(float inAngle);             // proven ?UMTan@@YAMM@Z (GEViewport ctor 0x36aba)
LLE_API bool UMQuadraticEquation(float inA, float inB, float inC, float& outRoot1, float& outRoot2);   // proven ?UMQuadraticEquation@@YA_NMMMAAM0@Z (exe import)
// Provisional name: no UMSin export; GEQuaternion::Slerp (0x23526) computes sines as UMCos(pi/2 - x) inline
// pi/2 is 1.5707964f (0x3fc90fdb) in every module: the engine's word 0x10097808, the exe's 0x43c008
// (1.5707963f would be 0x3fc90fda, one ulp lower)
inline float UMSin(float inAngle) { return UMCos(1.5707964f - inAngle); }
// Saved games keep an object's 32-bit id in the pointer field the object will fill: the loader reads the id into
// the field and a fix-up pass (RepairPointers, Restore, ...) looks the object up. The id held by such a field (its
// low 32 bits: the loader writes 4 bytes, whatever the pointer size).
template <class T> inline unsigned long USavedID(T* inField)
{
    return (unsigned long)reinterpret_cast<uintptr_t>(inField);
}
// Provisional name. Rounds in the current rounding mode (to nearest), as the VC6 build's fld/fistp did.
inline long UMFloatToLong(float inValue)
{
    return lrintf(inValue);
}
// Provisional name (not exported: a free inline). Default.dll 0xc29f: `fld r; fld st0; fmul st1` (the
// argument loaded once and squared), where `r * r` would give `fld r; fmul r`.
// Provisional name (not exported). Default.dll 0x41ca: `fcom [0.0]; jae; fchs` with no fabs call, re-evaluated
// whole inside max().
inline float UMAbs(float inValue)
{
    return inValue < 0.0f ? -inValue : inValue;
}
inline float UMSquare(float inValue)
{
    return inValue * inValue;
}
// Provisional name. Default.dll (EEClumps 0x7ff1): the same fld/fistp, then `fild r; fcomp v; jbe; dec r`:
// rounds down when the rounded value is above the input (floor under round-to-nearest).
// Provisional name. Default.dll units 11/12 (0x15970, 0x16035, 0x16461, ...): the same floor, but the rounded
// value is compared through `fild qword` (an unsigned result variable inside the helper).
inline unsigned long UMFloorUnsigned(float inValue)
{
    unsigned long theResult = (unsigned long)lrintf(inValue);   // round to nearest, as fld/fistp
    if (theResult > inValue)
        theResult--;
    return theResult;
}
inline long UMFloor(float inValue)
{
    long theResult = (long)lrintf(inValue);   // round to nearest, as fld/fistp
    if (theResult > inValue)
        theResult--;
    return theResult;
}
// Provisional name (not exported). DX7HRDisplay.dll (DX7Rasterizer::Draw3DLine 0x10004213): the same fld/fistp, then
// `fild r; fcomp v; jae; inc r`: rounds up when the rounded value is below the input.
inline long UMCeil(float inValue)
{
    long theResult = (long)lrintf(inValue);   // round to nearest, as fld/fistp
    if (theResult < inValue)
        theResult++;
    return theResult;
}
#ifdef LLE_EXE_HEADERS
// exe only (engine/plug-in token streams unchanged): the same ceiling through an unsigned result (`fild qword` compare;
// RTSRegionTable's ctors 0xa5b91/0x11e05e) provisional name. Developers' asm, as UMCeil above: the original
// converts with a bare `fld [slot]; fistp [slot]` from a memory-resident float (no __ftol call, no compiler round trip),
// which only an inlined asm helper produces, then compares the unsigned result through `fild qword`.
inline unsigned long UMCeilUnsigned(float inValue)
{
    unsigned long theResult = (unsigned long)lrintf(inValue);   // round to nearest, as fld/fistp
    if (theResult < inValue)
        theResult++;
    return theResult;
}
#endif

// ---- Startup paths (mixer: volume + directory + "redist\win32" is the Miles redist directory)
// (an extern object of the forward-declared UString; defined in File System.cpp's range)
LLE_API extern UString gStartupVolume;
LLE_API extern UString gStartupDirectory;

// UMSetFloatingPointPrecision / UMGetFloatingPointPrecision: the float.h _PC_* precision-control value (_controlfp(x, _MCW_PC), UMath.cpp)
enum UMFloatingPointPrecision {
    kFPPrecision64 = 0,      // float.h _PC_64
    kFPPrecision53 = 0x10000,  // float.h _PC_53
    kFPPrecision24 = 0x20000,  // float.h _PC_24
};

// UGetOSType (Utilities.cpp 0x100837eb) results from GetVersion. Names are provisional.
enum UOSType {
    kOSWindows95 = 1,  // UGetOSType 0x100837eb returns 1 for Win32 platform, version 4.0..4.9 (95) (provisional name)
    kOSWindows98 = 2,  // UGetOSType 0x100837eb returns 2 for Win32 platform, 4.10+ (98/ME) (provisional name)
    kOSWindowsNT5 = 3,  // UGetOSType 0x100837eb returns 3 for NT platform, major >= 5 (2000/XP) (provisional name)
    kOSWindowsNT4 = 4,  // UGetOSType 0x100837eb returns 4 for NT major < 5, or Win32s/major < 4 (provisional name)
};

#endif
