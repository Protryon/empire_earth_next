// RegAuth's password cipher and port default through ee_won's miscops module (rust/crates/ee_won/src/miscops.rs);
// these apply them to the op's members in the original's order (RegAuth.cpp's statements; FindPatchOp's patch choice,
// its op now Rust's (verop.rs), is tests/won/won_version_msg.h). DetectFirewallOp is a Rust object (fwop.rs).
// Templates over the op so that tests/won/won_miscops_test.cpp runs the same code over stand-ins. WONStatus comes
// from WONRouting.h, included first.
#pragma once
#include <stdint.h>
#include <set>
#include <string>
#include <vector>
#include "ee/wonapi/WONBuffer.h"
#include "rust/ee_won.h"

namespace WONAPI {

inline void MiscOpsStringSink(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    static_cast<std::string*>(inContext)->assign(reinterpret_cast<const char*>(inBytes), inLength);
}

// RegAuth's stored password: encrypted (false when the cipher gave nothing) or decrypted
inline bool RegAuthPassword(bool inDecrypt, const void* inData, unsigned long inLength, std::string& outText)
{
    return ee_won_regauth_password(inDecrypt, static_cast<const uint8_t*>(inData), inLength, MiscOpsStringSink, &outText);
}

} // namespace WONAPI
