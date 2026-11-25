// lle/UIShims.h (NEW, no original): the argument types of the toolkit's one-statement C++ shims that the moved host
// cases call (rust/crates/ee_ui/src/lle_cpp.rs; UI lane round 10), and the hosts Rust hands the C++ classes. A shim
// takes plain numbers, pointers (uintptr_t), a text (EeUiShimText: bytes handed in for the call) and out texts and
// numbers (an EeUiShimOut is a Rust Vec<u8> that UIShimOut appends to).
#ifndef LLE_UISHIMS_H
#define LLE_UISHIMS_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include "lle/UString.h"
#include "lle/UWideString.h"
#include "rust/ui/obj.h"
#include "rust/ui/lle_cpp.h"   // ee_ui_shim_out
#include "rust/ui/lle_bg_edit.h"   // the hosts (ee_ui_background_host, ee_ui_edit_host; the next four likewise)
#include "rust/ui/lle_combo.h"
#include "rust/ui/lle_control.h"
#include "rust/ui/lle_form.h"
#include "rust/ui/lle_manager.h"
#include "rust/ui/lle_table.h"


struct EeUiShimText {
    const uint8_t* ptr;
    size_t len;
};

// the UI database files' calls (UIHotKey.cpp), shared by the database loaders' shims
extern "C" void ee_uicpp_file_open(void* inFile);
extern "C" long ee_uicpp_file_read_count(void* inFile, unsigned long* outCount);
extern "C" long ee_uicpp_file_read(void* inFile, unsigned char* outData, unsigned long inLength);
extern "C" void ee_uicpp_file_close(void* inFile);

// (the hosts the moved cases plug into, ee_ui_*_host: their modules' headers, included above)

// a host case (ee_ui lle_hosts.rs: the Rust host's table names the op's shim): one statement on inObj, the host's
// object, with the op's numbers a..d and floats f; EE_UI_CASE answers a number, EE_UI_VOID nothing (0)
#define EE_UI_CASE(name) extern "C" intptr_t ee_uicpp_##name(void* inObj, intptr_t a, intptr_t b, intptr_t c, intptr_t d, float* f)
#define EE_UI_VOID(name) extern "C" void ee_uicpp_##name(void* inObj, intptr_t a, intptr_t b, intptr_t c, intptr_t d, float* f)

inline UString UIShimString(EeUiShimText inText) { return UString(std::string((const char*)inText.ptr, inText.len).c_str()); }
inline std::string UIShimStd(EeUiShimText inText) { return std::string((const char*)inText.ptr, inText.len); }

#endif
