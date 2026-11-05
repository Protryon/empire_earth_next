// Platform layer: the game's configuration file (see platform/Platform.h).
//
// One INI file, config.ini, holds every setting the game keeps between runs. Where it lives (PlatConfigGetPath):
//   1. $EE_CONFIG (a file path; tests and the harness use it) when set;
//   2. else config.ini in the data root (platform/PlatPath.h: the executable's directory, or $EE_DATA_ROOT), so a
//      play folder is self-contained;
//   3. else, when that directory cannot be written (a read-only install, a macOS app bundle), the same file in the
//      user's configuration directory: %APPDATA%\Empire Earth AoC on Windows, ~/Library/Application
//      Support/Empire Earth AoC on macOS, $XDG_CONFIG_HOME/empire-earth-aoc (~/.config/empire-earth-aoc) on Linux.
//      A file already there is read when the data root has none.
// A missing file is an empty configuration: the game writes its defaults on its first run.
//
// Format (written whole after every change, through a temporary file that replaces the old one):
//
//     [Game]
//     Player Name = Player
//     Game Window Width = 1024
//     Last Game File = "  leading spaces kept"
//
// Sections ("[Game]") and value names are matched ignoring ASCII case and keep the case they were first written
// with. A value is the text after the first '=', without the spaces around it. A value starting with '"' is a
// quoted string: \" \\ \n \r \t and \xHH are its escapes, nothing may follow the closing quote; the writer quotes a
// string only when it would not read back plain (leading or trailing blanks, a leading '"', control characters).
// Other bytes (the game's ANSI code page text) are written as they are. Integers are decimal (0x... is read too).
// Lines starting with ';' or '#' are comments; lines that are not "[section]" or "name = value" are ignored (and
// dropped when the file is rewritten).
//
// Every function -> nonzero on success, 0 on failure (a missing value, a value that does not fit or does not parse,
// or a file that cannot be written).
#ifndef PLATFORM_PLATCONFIG_H
#define PLATFORM_PLATCONFIG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// A string value into outData (inSize bytes, zero-terminated): 0 when it is missing or does not fit.
int PlatConfigGetString(const char* inSection, const char* inName, char* outData, uint32_t inSize);
// An integer value: 0 when it is missing or is not an integer.
int PlatConfigGetInt(const char* inSection, const char* inName, int32_t* outValue);
// Sets a value (the section is created when needed) and rewrites the file.
int PlatConfigSetString(const char* inSection, const char* inName, const char* inValue);
int PlatConfigSetInt(const char* inSection, const char* inName, int32_t inValue);
// Removes a value, or with inName NULL the whole section: 0 when there was none.
int PlatConfigDelete(const char* inSection, const char* inName);
// The native path of the file the configuration is read from and written to -> its length, 0 when it does not fit.
uint32_t PlatConfigGetPath(char* outPath, uint32_t inSize);

#ifdef __cplusplus
}
#endif

#endif
