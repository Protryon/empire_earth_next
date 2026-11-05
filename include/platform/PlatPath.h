// Platform layer: the game's paths on the native file system, and the data root (see platform/Platform.h).
//
// The game writes Windows paths: a volume ("C:"), backslash separators, any letter case (FSFileSpec, the data
// tables, saved settings). PlatFile.h takes them in that form everywhere. On Win32 they are the native paths. On
// POSIX each volume letter maps to a native directory (every letter to "/" by default), separators become '/', and
// every existing path component is found case-insensitively, so a copy of the Windows install works unchanged.
// Paths the platform hands back (current directory, temporary directory, the data root) are in the game's form.
#ifndef PLATFORM_PLATPATH_H
#define PLATFORM_PLATPATH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The native path of a game path (Win32: the path unchanged; POSIX: resolved as above, the missing tail kept as
// written) into outNative (inSize bytes) -> its length, 0 when it does not fit or the path is invalid.
uint32_t PlatPathToNative(const char* inPath, char* outNative, uint32_t inSize);
// The game form of a native absolute path -> its length, 0 when it does not fit or no volume contains it.
uint32_t PlatPathFromNative(const char* inNative, char* outPath, uint32_t inSize);
// Maps a volume letter ('A'..'Z', any case) to a native directory (POSIX; NULL restores the default "/").
// Win32 has real volumes: -> 0 there. -> nonzero on success.
int PlatPathSetVolumeRoot(char inVolume, const char* inNativeRoot);

// The directory holding the game's data, in the game's form with a trailing separator -> its length, 0 on
// failure. Win32: the executable's directory. POSIX: $EE_DATA_ROOT when set, else the executable's directory (a
// macOS app bundle: its Contents/Resources when that holds the data).
uint32_t PlatPathGetDataRoot(char* outPath, uint32_t inSize);
// Makes the data root the current directory (the game resolves its data relative to it) -> nonzero on success.
int PlatPathEnterDataRoot(void);
// The executable's file name (no directory) -> its length, 0 on failure.
uint32_t PlatPathGetExecutableName(char* outName, uint32_t inSize);

#ifdef __cplusplus
}
#endif

#endif
