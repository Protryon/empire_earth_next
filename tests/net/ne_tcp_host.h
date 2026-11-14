// TEST ONLY: the engine calls of the TCP plug-in (TCP Protocol.cpp's), for the tests that link the Rust server or
// client: since LLE Network Engine round 7 they call the protocol (ee_net ffi_protocol.rs) and so link the plug-in,
// which these tests never reach (their worlds answer the protocol ops).
#ifndef NE_TCP_HOST_H
#define NE_TCP_HOST_H

#include <stdint.h>

extern "C" void ee_tcp_host_record_native_error(uint32_t) {}
extern "C" uint32_t ee_tcp_host_native_error() { return 0; }
extern "C" uint32_t ee_tcp_host_time_ms() { return 0; }
extern "C" int32_t ee_tcp_host_port_offset() { return 0; }

#endif
