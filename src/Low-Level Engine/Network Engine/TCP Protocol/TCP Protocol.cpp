// C:\EEX\Low-Level Engine\Network Engine\TCP Protocol\TCP Protocol.cpp
// The TCP/IP network plug-in is Rust (rust/crates/ee_net/src/net_tcp), which NEProtocol calls directly (its NPIM* entry
// points, their PIMPlugin table and the conversions went in LLE Network Engine round 2; its configuration, in round
// 1). This file keeps the engine calls the Rust makes.
#include <string.h>
#include <string>
#include <vector>

#include "platform/wintypes.h"   // the Windows types the engine headers use (on Windows <windows.h>, as before)
#include "platform/PlatSocket.h"
#include "platform/PlatSim.h"
#include "platform/PlatThread.h"

#include "lle/LowLevelEngine.h"
#include "rust/net/net_tcp.h"

// ---- the engine calls Rust needs (extern "C" so Rust can link them)

extern "C" void ee_tcp_host_record_native_error(uint32_t inError)
{
    ESRecordNativeError(inError);
}
extern "C" uint32_t ee_tcp_host_native_error()
{
    return (uint32_t)ESGetNativeError();
}
extern "C" uint32_t ee_tcp_host_time_ms()
{
    return (uint32_t)TMGetTime();
}
extern "C" int32_t ee_tcp_host_port_offset()
{
    return gPlatSimPortOffset;
}
