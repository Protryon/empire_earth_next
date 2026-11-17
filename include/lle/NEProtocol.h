// C:\EEX\Low-Level Engine\Network Engine\NEProtocol.h (RECONSTRUCTION; NEProtocol.cpp 0x4440f..0x44c71). The network
// protocol: the game's one, TCP/IP, over its plug-in (rust/crates/ee_net/src/net_tcp), with the registry of protocols and the
// traffic counters. Whole in Rust (ee_net ffi_protocol.rs; PORT_LEARNINGS "LLE Network Engine, round 7"): a
// NEProtocol* is the Rust protocol (the class has no data); the class keeps the members the game calls, each
// forwarding. The Rust server and client call the protocol themselves.
// A connection (NEConnection*) and a published server (NEServer*) are the plug-in's own handles (round 5).
#ifndef LLE_NEPROTOCOL_H
#define LLE_NEPROTOCOL_H

#include <string.h>
#include <string>
#include <vector>
#include "rust/net/net.h"

class NEConnection;                         // the plug-in's connection handle (ee_net_tcp's Connection), never defined
class NEServer;                             // the plug-in's server handle (ee_net_tcp's Server), never defined

LLE_API long NEInitialize(FSFileSpec& inDirectory);
void NEShutdown(bool inForced);   // Network Engine.cpp's shutdown hook (frees the protocols)

class LLE_API NEProtocol {
public:
    static unsigned long GetCount() { return ee_net_pr_count(); }
    // the registered protocol at an index, or a beyond-end error
    static long GetProtocol(unsigned long inIndex, NEProtocol*& outProtocol)
    {
        return ee_net_pr_get((uint32_t)inIndex, reinterpret_cast<EeNetProtocol**>(&outProtocol));
    }
    // the network adapters' descriptions (the list cleared first) -> how many
    unsigned long EnumerateNetworkAdapters(std::vector<UWideString>& outAdapters)
    {
        outAdapters.clear();
        return ee_net_pr_enumerate_adapters(this->Rust(), NEProtocol::AddTo, &outAdapters);
    }
    // an adapter's address, in a string the protocol keeps (the next call overwrites it)
    UWideString& GetLocalAddress(unsigned long inAdapter)
    {
        static UWideString sAddress;
        ee_net_pr_local_address(this->Rust(), (uint32_t)inAdapter, NEProtocol::SetTo<UWideString>, &sAddress);
        return sAddress;
    }
    long GetStatus(const UWideString& inName, unsigned long inPort) { return ee_net_pr_status(this->Rust()); }
    // one datagram to a host and port
    long SendSingle(const UString& inHost, unsigned short inPort, const unsigned char* inData, unsigned long inLength)
    {
        return ee_net_pr_send_single(this->Rust(), reinterpret_cast<const uint8_t*>(static_cast<const char*>(inHost)),
                                     inHost.GetLength(), inPort, inData, (uint32_t)inLength);
    }
    // the plug-in's text answers into the engine's strings (EeNetPrSink)
    template <class T> static void SetTo(void* inContext, const uint8_t* inBytes, size_t inLength)
    {
        *static_cast<T*>(inContext) = std::string(reinterpret_cast<const char*>(inBytes), inLength).c_str();
    }
    static void AddTo(void* inContext, const uint8_t* inBytes, size_t inLength)
    {
        static_cast<std::vector<UWideString>*>(inContext)->push_back(
            UWideString(std::string(reinterpret_cast<const char*>(inBytes), inLength).c_str()));
    }
    EeNetProtocol* Rust() const { return reinterpret_cast<EeNetProtocol*>(const_cast<NEProtocol*>(this)); }

private:
    NEProtocol() = delete;
    ~NEProtocol() = delete;
};

#endif
