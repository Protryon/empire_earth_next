#ifndef EE_WONAPI_WONPING_H
#define EE_WONAPI_WONPING_H
#include <time.h>
#include "ee/wonapi/WONRouting.h"
namespace WONAPI {
// PingOp is a Rust op (rust/crates/ee_won/src/reqops.rs, EE_WON_REQOPS_PING).
//
// MultiPingOp is a Rust op on the Rust op base (rust/crates/ee_won/src/multiping.rs): new
// RustServerOp(ee_won_multiping_new()); the limits with ee_won_hostop_set (EE_WON_MULTIPING_BATCH_SIZE,
// _MAX_FAILURES, _MAX_PING_TIME), a server with EE_WON_MULTIPING_ADD (the address's six bytes, an entity or NULL),
// EE_WON_MULTIPING_CLEAR; once finished, the list by number (EE_WON_MULTIPING_COUNT, _ADDR, _TIME, _FAILURES,
// _ENTITY). The two helpers below are the original's AddServers and GetServerContext over it.

// AddServers: each service entity (not a directory) whose name passes the filter and whose address is valid
inline void MultiPingAddServers(RustServerOp* inOp, const DirEntityList& inDir, const wchar_t* inNameFilter = NULL)
{
    std::wstring aFilter = inNameFilter != NULL ? inNameFilter : L"";
    for (DirEntityList::const_iterator i = inDir.begin(); i != inDir.end(); ++i) {
        const DirEntity* anEntity = *i;
        if (anEntity->mType == 'D' || (!aFilter.empty() && aFilter != anEntity->mName)) continue;
        IPAddr anAddr = anEntity->GetNetAddrAsIP();
        if (anAddr.IsValid())
            ee_won_hostop_set(inOp->GetRustOp(), EE_WON_MULTIPING_ADD, 0, NULL, 0,
                              reinterpret_cast<const uint8_t*>(anAddr.GetSixByte()),
                              static_cast<RefCount*>(const_cast<DirEntity*>(anEntity)));
    }
}

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
