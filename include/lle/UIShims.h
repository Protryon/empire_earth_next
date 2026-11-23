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

enum class EeUiOp : uint32_t;

struct EeUiShimText {
    const uint8_t* ptr;
    size_t len;
};

extern "C" void ee_ui_shim_out(void* inOut, const uint8_t* inText, size_t inLength);

// the hosts the moved cases plug into (ee_ui lle_*.rs): each answers its moved ops and passes the others to the C++
// host (the class's HostOp)
#define EE_UI_HOST_ARGS void* inObj, EeUiOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d, float* f
extern "C" intptr_t ee_ui_manager_host(EE_UI_HOST_ARGS);
extern "C" intptr_t ee_ui_event_host(EE_UI_HOST_ARGS);
extern "C" intptr_t ee_ui_form_host(EE_UI_HOST_ARGS);
extern "C" intptr_t ee_ui_control_host(EE_UI_HOST_ARGS);
extern "C" intptr_t ee_ui_table_host(EE_UI_HOST_ARGS);
extern "C" intptr_t ee_ui_combo_host(EE_UI_HOST_ARGS);
extern "C" intptr_t ee_ui_background_host(EE_UI_HOST_ARGS);
extern "C" intptr_t ee_ui_edit_host(EE_UI_HOST_ARGS);

// a host case (ee_ui lle_hosts.rs: the Rust host's table names the op's shim): one statement on inObj, the host's
// object, with the op's numbers a..d and floats f; EE_UI_CASE answers a number, EE_UI_VOID nothing (0)
#define EE_UI_CASE(name) extern "C" intptr_t ee_uicpp_##name(void* inObj, intptr_t a, intptr_t b, intptr_t c, intptr_t d, float* f)
#define EE_UI_VOID(name) extern "C" void ee_uicpp_##name(void* inObj, intptr_t a, intptr_t b, intptr_t c, intptr_t d, float* f)

// the text as a string (the bytes up to the first 0)
inline UWideString UIShimText(EeUiShimText inText) { return UWideString(std::string((const char*)inText.ptr, inText.len).c_str()); }
inline UString UIShimString(EeUiShimText inText) { return UString(std::string((const char*)inText.ptr, inText.len).c_str()); }
inline std::string UIShimStd(EeUiShimText inText) { return std::string((const char*)inText.ptr, inText.len); }
// a string's bytes appended to an out text
inline void UIShimOut(void* inOut, const UWideString& inText) { ee_ui_shim_out(inOut, (const uint8_t*)(const char*)inText, inText.GetTCHARLength()); }

#endif
