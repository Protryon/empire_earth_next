// WONAPI IPAddr: IPv4 address and port, with DNS lookup and local adapter listing (file grouping inferred). The
// parsing, the lookups, the text forms, the order and the local address lists are Rust's
// (rust/crates/ee_won/src/ipaddr.rs); the class keeps its PlatSockAddrIn and the remembered host string.
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

// A parsed address into the sockaddr: zeroed, IPv4, the network-order address and port
static void SetAddr(PlatSockAddrIn& outAddr, uint32_t inAddress, uint16_t inPort)
{
    memset(&outAddr, 0, sizeof(outAddr));
    outAddr.mFamily = PLAT_AF_INET;
    outAddr.mAddress = inAddress;
    outAddr.mPort = inPort;
}

// FUNCTION: WON 0x006b2660
// Sets the address from a dotted IP or host name (resolved by DNS) and a port; returns false if it does not resolve
bool IPAddr::Fn2B2660(const std::string& inHost,unsigned short inPort)
{
    memset(&this->mAddr,0,sizeof(this->mAddr));
    this->mIsValid=false;
    uint32_t anAddress=0;
    uint16_t aPort=0;
    if (!ee_won_ipaddr_resolve(Bytes(inHost),inHost.length(),inPort,&anAddress,&aPort)) return false;
    if (this->mRememberHostString) this->mHost=inHost;
    else this->mHost.erase();
    SetAddr(this->mAddr,anAddress,aPort);
    this->mIsValid=true;
    return true;
}
// FUNCTION: WON 0x006b2890
// Sets the address from a "host:port" string; invalid and false when there is no colon
bool IPAddr::Fn2B2890(const std::string& inHostAndPort)
{
    std::string::size_type aColon=inHostAndPort.find(':');
    if (aColon==std::string::npos) {
        this->mIsValid=false;
        return false;
    }
    // the port is the text after the colon as atoi reads it
    std::string aPort=inHostAndPort.substr(aColon+1);
    return this->Fn2B2660(inHostAndPort.substr(0,aColon),ee_won_ipaddr_port_of(Bytes(aPort),aPort.length()));
}
// FUNCTION: WON 0x006b29d0
// Sets the address from the 6-byte wire form (port then IP, network order)
void IPAddr::SetSixByte(const void* inAddress)
{
    memset(&this->mAddr,0,sizeof(this->mAddr));
    this->mAddr.mFamily=PLAT_AF_INET;
    memcpy(&this->mAddr.mPort,inAddress,6);
    this->mHost.erase();
    this->mIsValid=true;
}
// FUNCTION: WON 0x006b2b00
// Sets the address from a PlatSockAddrIn and forgets the host name
void IPAddr::Fn2B2B00(const PlatSockAddrIn& inAddress)
{
    this->mAddr=inAddress;
    this->mHost.erase();
    this->mIsValid=true;
}
// FUNCTION: WON 0x006b2c20
// Sets the address from a host-order IP and port and forgets the host name
void IPAddr::Fn2B2C20(long inHost,unsigned short inPort)
{
    SetAddr(this->mAddr,PlatHostToNet32(static_cast<uint32_t>(inHost)),PlatHostToNet16(inPort));
    this->mHost.erase();
    this->mIsValid=true;
}
// FUNCTION: WON 0x006b2df0
// Sets the address from a dotted "ip[:port]" string without DNS, using the default port if none; returns validity
bool IPAddr::SetWithDefaultPortNoDNS(const std::string& inHostAndPort,unsigned short inDefaultPort)
{
    this->mIsValid=false;
    uint32_t anAddress=0;
    uint16_t aPort=0;
    if (ee_won_ipaddr_parse_no_dns(Bytes(inHostAndPort),inHostAndPort.length(),inDefaultPort,&anAddress,&aPort)) {
        SetAddr(this->mAddr,anAddress,aPort);
        this->mHost.erase();
        this->mIsValid=true;
    }
    return this->mIsValid;
}

// FUNCTION: WON 0x006b2fc0
// Returns the remembered host name if asked and known, else the dotted IP
std::string IPAddr::GetHostString(bool inUseOriginal) const
{
    if (inUseOriginal && !this->mHost.empty()) return this->mHost;
    char aText[16];
    ee_won_ipaddr_dotted(this->mAddr.mAddress,aText);
    return std::string(aText, std::allocator<char>());
}
// FUNCTION: WON 0x006b2500
// Returns the chosen local interface address, else this machine's host name resolved, else an invalid address
IPAddr IPAddr::GetLocalAddr()
{
    if (IPAddr::mLocalHost!=0) return IPAddr(IPAddr::mLocalHost,0);
    char aHostName[1024];
    if (ee_won_ipaddr_host_name(aHostName,sizeof(aHostName))) return IPAddr(aHostName,0);
    return IPAddr();
}

// FUNCTION: WON 0x006b37a0
// Chooses the local interface IP; with validation, refuses an IP this machine does not have (the host name's
// addresses, as LobbyGetNetAddresses without details lists them)
bool IPAddr::SetLocalHost(long inHost,bool inValidate)
{
    if (inValidate && inHost!=0 && !ee_won_ipaddr_is_local(static_cast<uint32_t>(inHost)))
        return false;
    IPAddr::mLocalHost=inHost;
    return true;
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
IPAddr::IPAddr()
{
    memset(&this->mAddr,0,sizeof(this->mAddr));
    this->mIsValid=false;
    this->mRememberHostString=false;
}
// FUNCTION: WON 0x006b2380
// Creates the address from a PlatSockAddrIn
IPAddr::IPAddr(const PlatSockAddrIn& inAddress)
{
    this->mRememberHostString=false;
    this->Fn2B2B00(inAddress);
}
// FUNCTION: WON 0x006b23e0
// Creates the address from a host name or dotted IP and a port (resolving by DNS)
IPAddr::IPAddr(const std::string& inHost,unsigned short inPort)
{
    this->mRememberHostString=false;
    this->Fn2B2660(inHost,inPort);
}
// FUNCTION: WON 0x006b2440
// Creates the address from a "host:port" string
IPAddr::IPAddr(const std::string& inHostAndPort)
{
    this->mRememberHostString=false;
    this->Fn2B2890(inHostAndPort);
}
// FUNCTION: WON 0x006b24a0
// Creates the address from a host-order IP and port
IPAddr::IPAddr(long inHost,unsigned short inPort)
{
    this->mRememberHostString=false;
    this->Fn2B2C20(inHost,inPort);
}
// FUNCTION: WON 0x006b2d50
// Sets the address from "host[:port]" (resolving by DNS), using the default port if none; returns validity
bool IPAddr::SetWithDefaultPort(const std::string& inHostAndPort,unsigned short inDefaultPort)
{
    if(inHostAndPort.find(':')==std::string::npos) return this->Fn2B2660(inHostAndPort,inDefaultPort);
    return this->Fn2B2890(inHostAndPort);
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
    return reinterpret_cast<const char*>(&this->mAddr.mPort);
}
// FUNCTION: WON 0x006b2f90
// Sets the port
void IPAddr::SetPort(unsigned short inPort)
{
    this->mAddr.mPort=PlatHostToNet16(inPort);
}
// FUNCTION: WON 0x006b2fb0
// Returns the IP in host byte order
long IPAddr::GetHost() const
{
    return static_cast<long>(PlatNetToHost32(this->mAddr.mAddress));
}
// FUNCTION: WON 0x006b3070
// Returns "host:port", as formatted into a 50-byte buffer (a long remembered host name is cut short)
std::string IPAddr::GetHostAndPortString(bool inUseOriginal) const
{
    std::string aHost=this->GetHostString(inUseOriginal);
    char text[50];
    ee_won_ipaddr_host_and_port(Bytes(aHost),aHost.length(),this->GetPort(),text);
    return text;
}
// FUNCTION: WON 0x006b31c0
// Returns the port in host byte order
unsigned short IPAddr::GetPort() const
{
    return PlatNetToHost16(this->mAddr.mPort);
}
// FUNCTION: WON 0x006b31d0
// Returns whether the address has been set successfully
bool IPAddr::IsValid() const
{
    return this->mIsValid;
}
// FUNCTION: WON 0x006b31e0
// Returns the PlatSockAddrIn
const PlatSockAddrIn& IPAddr::GetSockAddrIn() const
{
    return this->mAddr;
}
// FUNCTION: WON 0x006b31f0
// Orders addresses by their 6-byte wire form (port first)
bool IPAddr::operator<(const IPAddr& other) const
{
    return ee_won_ipaddr_compare_six(reinterpret_cast<const uint8_t*>(&this->mAddr.mPort),
                                     reinterpret_cast<const uint8_t*>(&other.mAddr.mPort))<0;
}
// FUNCTION: WON 0x006b3220
// Returns whether two addresses have the same IP and port
bool IPAddr::operator==(const IPAddr& other) const
{
    return ee_won_ipaddr_compare_six(reinterpret_cast<const uint8_t*>(&this->mAddr.mPort),
                                     reinterpret_cast<const uint8_t*>(&other.mAddr.mPort))==0;
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
    return ee_won_ipaddr_string_compare(this->mAddr.mAddress,this->mAddr.mPort,other.mAddr.mAddress,
                                        other.mAddr.mPort);
}
}

// SDK static: the chosen local interface in host order; in the original at 565f40
namespace WONAPI {
long IPAddr::mLocalHost = PLAT_INADDR_ANY;
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
