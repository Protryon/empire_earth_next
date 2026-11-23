#ifndef EE_WONAPI_WONPING_H
#define EE_WONAPI_WONPING_H
#include <time.h>
#include "ee/wonapi/WONRouting.h"
#include "rust/won_net/hostop.h"
#include "rust/won_net/multiping.h"
namespace WONAPI {
// PingOp is a Rust op (rust/crates/ee_won_net/src/reqops.rs, EE_WON_REQOPS_PING).
//
// MultiPingOp is a Rust op on the Rust op base (rust/crates/ee_won_net/src/multiping.rs): new
// RustServerOp(ee_won_multiping_new()); the limits with ee_won_hostop_set (EE_WON_MULTIPING_BATCH_SIZE,
// _MAX_FAILURES, _MAX_PING_TIME), a server with EE_WON_MULTIPING_ADD (the address's six bytes, an entity or NULL),
// EE_WON_MULTIPING_CLEAR; once finished, the list by number (EE_WON_MULTIPING_COUNT, _ADDR, _TIME, _FAILURES,
// _ENTITY). The two helpers below are the original's AddServers and GetServerContext over it.

// AddServers: each service entity (not a directory) whose name passes the filter and whose address is valid (Rust's:
// dirlist.rs, R6 WON round 34; the elements hold no entity object)
inline void MultiPingAddServers(RustServerOp* inOp, const DirList& inDir, const wchar_t* inNameFilter = NULL)
{ ee_won_multiping_add_servers(inOp->GetRustOp(), inDir.get(), inNameFilter, inNameFilter != NULL ? wcslen(inNameFilter) : 0, sizeof(wchar_t)); }

inline void MultiPingSixSink(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    static_cast<std::string*>(inContext)->assign(reinterpret_cast<const char*>(inBytes), inLength);
}

// GetServerContext: the finished servers in order (the unanswered too unless discarded), not shuffled
inline SmartPtr<ServerContext> MultiPingServerContext(RustServerOp* inOp, bool discardNonResponsiveServers = true)
{
    SmartPtr<ServerContext> aContext = new ServerContext;
    EeWonHostOp* r = inOp->GetRustOp();
    int aKeep = discardNonResponsiveServers ? EE_WON_MULTIPING_KEEPS : EE_WON_MULTIPING_KEEPS_ALL;
    for (int64_t i = 0; i < ee_won_hostop_num(r, EE_WON_MULTIPING_COUNT, 0); i++) {
        std::string aSix;
        ee_won_hostop_bytes(r, EE_WON_MULTIPING_ADDR, i, MultiPingSixSink, &aSix);
        if (ee_won_hostop_num(r, aKeep, i) == 0 || aSix.size() != 6) continue;
        IPAddr anAddr;
        anAddr.SetSixByte(aSix.data());
        aContext->AddAddress(anAddr);
    }
    aContext->SetShuffleAddresses(false);
    return aContext;
}
}
#endif
