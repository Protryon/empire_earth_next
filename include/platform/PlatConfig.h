// Platform layer: the game's configuration file (see platform/Platform.h).
//
// One YAML file, config.yaml, holds every setting the game keeps between runs. Where it lives (PlatConfigGetPath):
//   1. $EE_CONFIG (a file path; tests and the harness use it) when set;
//   2. else config.yaml in the data root (platform/PlatPath.h: the executable's directory, or $EE_DATA_ROOT), so a
//      play folder is self-contained;
//   3. else, when that directory cannot be written (a read-only install, a macOS app bundle), the same file in the
//      user's configuration directory: %APPDATA%\Empire Earth AoC on Windows, ~/Library/Application
//      Support/Empire Earth AoC on macOS, $XDG_CONFIG_HOME/empire-earth-aoc (~/.config/empire-earth-aoc) on Linux.
//      A file already there is read when the data root has none.
// A missing file is an empty configuration: the game writes its defaults on its first run.
//
// Format (written whole after every change, through a temporary file that replaces the old one): a subset of YAML,
// nested blocks of "key: value" lines indented by two spaces:
//
//     display:
//       window_mode: 0
//       resolution:
//         width: 1024
//     player:
//       name: Player
//       last_game: "  leading spaces kept"
//
// A value is addressed by its block's path (the keys above it joined by '.': "display.resolution"; "" or NULL is the
// top level) and its key. Keys are matched ignoring ASCII case and keep the case they were first written with; blocks
// and keys keep the order they were first written in. A value is the text after "key:", without the blanks around it
// and without a comment (" #..." outside quotes). A value starting with '"' is a double-quoted string: \" \\ \n \r \t
// \0 and \xHH are its escapes; one starting with '\'' is single-quoted ('' is a quote). The writer quotes a string
// only when it would not read back as the same plain string (empty, blanks around it, YAML indicator characters, a
// number or boolean look, ": " or " #", control characters). Other bytes (the game's ANSI code page text) are written
// as they are. Integers are decimal (0x... is read too); booleans are written true/false and read as 1/0 by
// PlatConfigGetInt (yes/no and on/off too). Comments, blank lines and lines that are not "key:" or "key: value" are
// ignored (and dropped when the file is rewritten). Flow collections ([...], {...}) and lists are not supported.
//
// Every function -> nonzero on success, 0 on failure (a missing value, a value that does not fit or does not parse,
// or a file that cannot be written).
#ifndef PLATFORM_PLATCONFIG_H
#define PLATFORM_PLATCONFIG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// A string value into outData (inSize bytes, zero-terminated): 0 when it is missing, is a block or does not fit.
int PlatConfigGetString(const char* inBlock, const char* inKey, char* outData, uint32_t inSize);
// An integer value (true/false: 1/0): 0 when it is missing or is not an integer.
int PlatConfigGetInt(const char* inBlock, const char* inKey, int32_t* outValue);
// Sets a value (its blocks are created when needed; a block of that name is replaced) and rewrites the file.
int PlatConfigSetString(const char* inBlock, const char* inKey, const char* inValue);
int PlatConfigSetInt(const char* inBlock, const char* inKey, int32_t inValue);
int PlatConfigSetBool(const char* inBlock, const char* inKey, int inValue);
// Removes a value, or with inKey NULL the whole block: 0 when there was none.
int PlatConfigDelete(const char* inBlock, const char* inKey);
// The native path of the file the configuration is read from and written to -> its length, 0 when it does not fit.
uint32_t PlatConfigGetPath(char* outPath, uint32_t inSize);

#ifdef __cplusplus
}
#endif

#endif
