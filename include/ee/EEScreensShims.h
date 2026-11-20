// ee/EEScreensShims.h (NEW, no original): the argument types of the one-statement C++ shims the UI lane's Rust calls
// (rust/crates/ee_game/src/screens/game_cpp.rs; UI lane round 7). A shim takes plain numbers, pointers (uintptr_t),
// a text (EeScreensShimText: bytes handed in for the call) and out texts and numbers (an EeScreensShimOut is a Rust
// Vec<u8> that EEShimOut appends to).
#ifndef EE_EESCREENSSHIMS_H
#define EE_EESCREENSSHIMS_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include "lle/UString.h"
#include "lle/UWideString.h"

struct EeScreensShimText {
    const uint8_t* ptr;
    size_t len;
};

extern "C" void ee_screens_shim_out(void* inOut, const uint8_t* inText, size_t inLength);

// the text as a string (the bytes up to the first 0, as the C++ cases' UWideString(const char*))
inline UWideString EEShimText(EeScreensShimText inText) { return UWideString(std::string((const char*)inText.ptr, inText.len).c_str()); }
inline UString EEShimString(EeScreensShimText inText) { return UString(std::string((const char*)inText.ptr, inText.len).c_str()); }
// a string's bytes appended to an out text
inline void EEShimOut(void* inOut, const UWideString& inText) { ee_screens_shim_out(inOut, (const uint8_t*)(const char*)inText, inText.GetTCHARLength()); }
inline void EEShimOutC(void* inOut, const char* inText) { ee_screens_shim_out(inOut, (const uint8_t*)inText, strlen(inText)); }
inline void EEShimOut(void* inOut, const UString& inText) { EEShimOutC(inOut, (const char*)inText); }

#endif
