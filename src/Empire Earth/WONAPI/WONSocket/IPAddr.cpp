// WONAPI IPAddr: IPv4 address and port, with DNS lookup and local adapter listing (file grouping inferred). A Rust
// value since round 14 (rust/crates/ee_won/src/ipobj.rs EeWonIpAddr: the sockaddr, the validity, the remembered
// host and every step below; the parsing and lookups are ipaddr.rs's); the class is a handle copied with its value
// at every copy, as the original's implicit copy constructor and assignment copied it.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <string.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "platform/PlatSocket.h"
namespace WONAPI {

static const uint8_t* Bytes(const std::string& inText) { return reinterpret_cast<const uint8_t*>(inText.data()); }
static void TextSink(void* inText, const uint8_t* inBytes, size_t inLength)
{
    static_cast<std::string*>(inText)->assign(reinterpret_cast<const char*>(inBytes), inLength);
}

// the value's copy (the original's implicit copy constructor COMDAT 0x291d59, operator= 0x29181b and destructor)
IPAddr::IPAddr(const IPAddr& inOther) : mAddr(ee_won_ipaddr_clone(inOther.mAddr)) {}
IPAddr& IPAddr::operator=(const IPAddr& inOther)
{
    ee_won_ipaddr_assign(this->mAddr, inOther.mAddr);
    return *this;
}
IPAddr::~IPAddr() { ee_won_ipaddr_free(this->mAddr); }
IPAddr::IPAddr(EeWonIpAddr* inValue, AdoptTag) : mAddr(inValue) {}

// FUNCTION: WON 0x006b2660
// Sets the address from a dotted IP or host name (resolved by DNS) and a port; returns false if it does not resolve
bool IPAddr::Fn2B2660(const std::string& inHost,unsigned short inPort)
{
    return ee_won_ipaddr_set_resolve(this->mAddr,Bytes(inHost),inHost.length(),inPort);
}
// FUNCTION: WON 0x006b2890
// Sets the address from a "host:port" string; invalid and false when there is no colon
bool IPAddr::Fn2B2890(const std::string& inHostAndPort)
{
    return ee_won_ipaddr_set_host_port(this->mAddr,Bytes(inHostAndPort),inHostAndPort.length());
}
// FUNCTION: WON 0x006b29d0
// Sets the address from the 6-byte wire form (port then IP, network order)
void IPAddr::SetSixByte(const void* inAddress)
{
    ee_won_ipaddr_set_six(this->mAddr,static_cast<const uint8_t*>(inAddress));
}
// FUNCTION: WON 0x006b2b00
// Sets the address from a PlatSockAddrIn and forgets the host name
void IPAddr::Fn2B2B00(const PlatSockAddrIn& inAddress)
{
    ee_won_ipaddr_set_sockaddr(this->mAddr,&inAddress);
}
// FUNCTION: WON 0x006b2c20
// Sets the address from a host-order IP and port and forgets the host name
void IPAddr::Fn2B2C20(long inHost,unsigned short inPort)
{
    ee_won_ipaddr_set_host_order(this->mAddr,static_cast<uint32_t>(inHost),inPort);
}
// FUNCTION: WON 0x006b2df0
// Sets the address from a dotted "ip[:port]" string without DNS, using the default port if none; returns validity
bool IPAddr::SetWithDefaultPortNoDNS(const std::string& inHostAndPort,unsigned short inDefaultPort)
{
    return ee_won_ipaddr_set_no_dns(this->mAddr,Bytes(inHostAndPort),inHostAndPort.length(),inDefaultPort);
}

// FUNCTION: WON 0x006b2fc0
// Returns the remembered host name if asked and known, else the dotted IP
std::string IPAddr::GetHostString(bool inUseOriginal) const
{
    std::string aText;
    ee_won_ipaddr_host_string(this->mAddr,inUseOriginal,TextSink,&aText);
    return aText;
}
// FUNCTION: WON 0x006b2500
// Returns the chosen local interface address, else this machine's host name resolved, else an invalid address
IPAddr IPAddr::GetLocalAddr()
{
    return IPAddr(ee_won_ipaddr_local_addr(), AdoptTag());
}

// FUNCTION: WON 0x006b37a0
// Chooses the local interface IP; with validation, refuses an IP this machine does not have (the host name's
// addresses, as LobbyGetNetAddresses without details lists them)
bool IPAddr::SetLocalHost(long inHost,bool inValidate)
{
    return ee_won_ipaddr_set_local_host(inHost,inValidate);
}
// the chosen local interface in host order (the SDK's static mLocalHost, 565f40 in the original; Rust's now)
long IPAddr::GetLocalHost()
{
    return ee_won_ipaddr_local_host();
}
void IPAddr::SetRememberHostString(bool inRemember)
{
    ee_won_ipaddr_set_remember_host(this->mAddr,inRemember);
}

static void AddNetAddress(void* inList, const uint8_t* inName, size_t inLength, uint32_t inHost)
{
    static_cast<std::list<LobbyNetAddress>*>(inList)->push_back(
        LobbyNetAddress(std::string(reinterpret_cast<const char*>(inName),inLength),static_cast<long>(inHost)));
}
// FUNCTION: WON 0x006b32b0
// Lists this machine's IP addresses: from the adapter list when details are asked for, else by host name lookup
// (the adapter list was 0x6b3540, IPAddr_PlatformGetNetworkAdapterList)
void LobbyGetNetAddresses(std::list<LobbyNetAddress>& outAddresses,bool inGetDetails)
{
    outAddresses.clear();
    ee_won_ipaddr_local_addresses(inGetDetails,AddNetAddress,&outAddresses);
}
// FUNCTION: WON 0x006b2350
// Creates an empty, invalid address
IPAddr::IPAddr() : mAddr(ee_won_ipaddr_new()) {}
// FUNCTION: WON 0x006b2380
// Creates the address from a PlatSockAddrIn
IPAddr::IPAddr(const PlatSockAddrIn& inAddress) : mAddr(ee_won_ipaddr_new())
{
    this->Fn2B2B00(inAddress);
}
// FUNCTION: WON 0x006b23e0
// Creates the address from a host name or dotted IP and a port (resolving by DNS)
IPAddr::IPAddr(const std::string& inHost,unsigned short inPort) : mAddr(ee_won_ipaddr_new())
{
    this->Fn2B2660(inHost,inPort);
}
// FUNCTION: WON 0x006b2440
// Creates the address from a "host:port" string
IPAddr::IPAddr(const std::string& inHostAndPort) : mAddr(ee_won_ipaddr_new())
{
    this->Fn2B2890(inHostAndPort);
}
// FUNCTION: WON 0x006b24a0
// Creates the address from a host-order IP and port
IPAddr::IPAddr(long inHost,unsigned short inPort) : mAddr(ee_won_ipaddr_new())
{
    this->Fn2B2C20(inHost,inPort);
}
// FUNCTION: WON 0x006b2d50
// Sets the address from "host[:port]" (resolving by DNS), using the default port if none; returns validity
bool IPAddr::SetWithDefaultPort(const std::string& inHostAndPort,unsigned short inDefaultPort)
{
    return ee_won_ipaddr_set_with_default_port(this->mAddr,Bytes(inHostAndPort),inHostAndPort.length(),
                                               inDefaultPort);
}

}

namespace WONAPI {
// FUNCTION: WON 0x006b2630
// Returns the broadcast address 255.255.255.255 on the given port
IPAddr IPAddr::GetBroadcastAddr(int inPort)
{
    return IPAddr(-1,static_cast<unsigned short>(inPort));
}
// FUNCTION: WON 0x006b29c0
// Returns the 6-byte wire form of the address (port then IP, network order)
const char* IPAddr::GetSixByte() const
{
    return ee_won_ipaddr_six(this->mAddr);
}
// FUNCTION: WON 0x006b2f90
// Sets the port
void IPAddr::SetPort(unsigned short inPort)
{
    ee_won_ipaddr_set_port(this->mAddr,inPort);
}
// FUNCTION: WON 0x006b2fb0
// Returns the IP in host byte order
long IPAddr::GetHost() const
{
    return ee_won_ipaddr_host(this->mAddr);
}
// FUNCTION: WON 0x006b3070
// Returns "host:port", as formatted into a 50-byte buffer (a long remembered host name is cut short)
std::string IPAddr::GetHostAndPortString(bool inUseOriginal) const
{
    std::string aText;
    ee_won_ipaddr_host_and_port_string(this->mAddr,inUseOriginal,TextSink,&aText);
    return aText;
}
// FUNCTION: WON 0x006b31c0
// Returns the port in host byte order
unsigned short IPAddr::GetPort() const
{
    return ee_won_ipaddr_port(this->mAddr);
}
// FUNCTION: WON 0x006b31d0
// Returns whether the address has been set successfully
bool IPAddr::IsValid() const
{
    return ee_won_ipaddr_valid(this->mAddr);
}
// FUNCTION: WON 0x006b31e0
// Returns the PlatSockAddrIn
const PlatSockAddrIn& IPAddr::GetSockAddrIn() const
{
    return *static_cast<const PlatSockAddrIn*>(ee_won_ipaddr_sockaddr(this->mAddr));
}
// FUNCTION: WON 0x006b31f0
// Orders addresses by their 6-byte wire form (port first)
bool IPAddr::operator<(const IPAddr& other) const
{
    return ee_won_ipaddr_compare(this->mAddr,other.mAddr)<0;
}
// FUNCTION: WON 0x006b3220
// Returns whether two addresses have the same IP and port
bool IPAddr::operator==(const IPAddr& other) const
{
    return ee_won_ipaddr_compare(this->mAddr,other.mAddr)==0;
}
// FUNCTION: WON 0x006b3240
// Returns whether two addresses differ in IP or port
bool IPAddr::operator!=(const IPAddr& other) const
{
    return !(*this==other);
}
// FUNCTION: WON 0x006b3260
// Compares two addresses by host-order IP, then by port
int IPAddr::StringCompare(const IPAddr& other) const
{
    return ee_won_ipaddr_string_compare_values(this->mAddr,other.mAddr);
}
// ServerContext's conversions to and from servctx.rs's EeWonAddr (the host borrowed while this value lives)
EeWonAddr IPAddr::ToRaw() const
{
    return ee_won_ipaddr_to_raw(this->mAddr);
}
void IPAddr::FromRaw(const EeWonAddr& inRaw)
{
    ee_won_ipaddr_from_raw(this->mAddr,&inRaw);
}
}

// Compiler-generated for this unit:
// SYNTHETIC: WON 0x006b3790
// (_$E72: atexit destructor of IPAddr_PlatformGetNetworkAdapterList's static anUnload; the function no longer
// loads Iphlpapi.dll itself)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006b8710
// (public: __thiscall std::auto_ptr<char>::~auto_ptr<char>(void))
// TEMPLATE: WON 0x006b9130
// (protected: struct std::list<struct WONAPI::LobbyNetAddress,class std::allocator<struct WONAPI::LobbyNetAddress> >::_Node * __thiscall std::list<struct)
// TEMPLATE: WON 0x006b97f0
// (protected: void __thiscall std::list<struct WONAPI::LobbyNetAddress,class std::allocator<struct WONAPI::LobbyNetAddress> >::_Incsize(unsigned int))
// TEMPLATE: WON 0x006b9e30
// (void __cdecl std::_Construct(struct WONAPI::LobbyNetAddress *,struct WONAPI::LobbyNetAddress const &))
