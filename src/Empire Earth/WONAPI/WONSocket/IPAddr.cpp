// WONAPI IPAddr: IPv4 address and port, with DNS lookup and local adapter listing (file grouping inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <winsock2.h>
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory>
#include "winsdk/iphlpapi.h"
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {

// FUNCTION: WON 0x006b2660
// Sets the address from a dotted IP or host name (resolved by DNS) and a port; returns false if it does not resolve
bool IPAddr::Fn2B2660(const std::string& inHost,unsigned short inPort)
{
    memset(&this->mAddr,0,sizeof(this->mAddr));
    this->mIsValid=false;
    unsigned long aHost=inet_addr(inHost.c_str());
    if (aHost==INADDR_NONE) {
        hostent* aEntry=gethostbyname(inHost.c_str());
        if (aEntry==NULL) return false;
        memcpy(&aHost,aEntry->h_addr_list[0],sizeof(aHost));
    }
    if (this->mRememberHostString) this->mHost=inHost;
    else this->mHost.erase();
    this->mAddr.sin_family=AF_INET;
    this->mAddr.sin_addr.s_addr=aHost;
    this->mAddr.sin_port=htons(inPort);
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
    return this->Fn2B2660(inHostAndPort.substr(0,aColon),
        static_cast<unsigned short>(atoi(inHostAndPort.substr(aColon+1).c_str())));
}
// FUNCTION: WON 0x006b29d0
// Sets the address from the 6-byte wire form (port then IP, network order)
void IPAddr::SetSixByte(const void* inAddress)
{
    memset(&this->mAddr,0,sizeof(this->mAddr));
    this->mAddr.sin_family=AF_INET;
    memcpy(&this->mAddr.sin_port,inAddress,6);
    this->mHost.erase();
    this->mIsValid=true;
}
// FUNCTION: WON 0x006b2b00
// Sets the address from a Winsock sockaddr_in and forgets the host name
void IPAddr::Fn2B2B00(const sockaddr_in& inAddress)
{
    this->mAddr=inAddress;
    this->mHost.erase();
    this->mIsValid=true;
}
// FUNCTION: WON 0x006b2c20
// Sets the address from a host-order IP and port and forgets the host name
void IPAddr::Fn2B2C20(long inHost,unsigned short inPort)
{
    memset(&this->mAddr,0,sizeof(this->mAddr));
    this->mAddr.sin_family=AF_INET;
    this->mAddr.sin_addr.s_addr=htonl(inHost);
    this->mAddr.sin_port=htons(inPort);
    this->mHost.erase();
    this->mIsValid=true;
}
// FUNCTION: WON 0x006b2df0
// Sets the address from a dotted "ip[:port]" string without DNS, using the default port if none; returns validity
bool IPAddr::SetWithDefaultPortNoDNS(const std::string& inHostAndPort,unsigned short inDefaultPort)
{
    this->mIsValid=false;
    std::string::size_type aColon=inHostAndPort.find(':');
    unsigned long aHost;
    unsigned short aPort;
    if (aColon!=std::string::npos) {
        aHost=inet_addr(inHostAndPort.substr(0,aColon).c_str());
        aPort=atoi(inHostAndPort.substr(aColon+1).c_str());
    } else {
        aHost=inet_addr(inHostAndPort.c_str());
        aPort=inDefaultPort;
    }
    if (aHost!=INADDR_NONE) {
        memset(&this->mAddr,0,sizeof(this->mAddr));
        this->mAddr.sin_family=AF_INET;
        this->mAddr.sin_addr.s_addr=aHost;
        this->mAddr.sin_port=htons(static_cast<unsigned short>(aPort));
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
    return std::string(inet_ntoa(this->mAddr.sin_addr), std::allocator<char>());
}
// FUNCTION: WON 0x006b2500
// Returns the chosen local interface address, else this machine's host name resolved, else an invalid address
IPAddr IPAddr::GetLocalAddr()
{
    if (IPAddr::mLocalHost!=0) return IPAddr(IPAddr::mLocalHost,0);
    char aHostName[1024];
    int aErr=gethostname(aHostName,1024);
    if (aErr==0) return IPAddr(aHostName,0);
    return IPAddr();
}

// FUNCTION: WON 0x006b37a0
// Chooses the local interface IP; with validation, refuses an IP this machine does not have
bool IPAddr::SetLocalHost(long inHost,bool inValidate)
{
    if (inValidate && inHost!=0) {
        std::list<LobbyNetAddress> aAddresses;
        LobbyGetNetAddresses(aAddresses,false);
        std::list<LobbyNetAddress>::iterator aEnd=aAddresses.end();
        std::list<LobbyNetAddress>::iterator it=aAddresses.begin();
        for (;it!=aEnd;++it)
            if (it->mHandle==inHost) break;
        if (it==aEnd) return false;
    }
    IPAddr::mLocalHost=inHost;
    return true;
}


// FUNCTION: WON 0x006b3540
// Lists each network adapter's description and first IP via Iphlpapi.dll; false when unavailable
static bool IPAddr_PlatformGetNetworkAdapterList(std::list<LobbyNetAddress>& outAddresses)
{
    typedef DWORD (WINAPI *GetAdaptersInfoFunc)(PIP_ADAPTER_INFO,PULONG);
    static GetAdaptersInfoFunc aGetAdaptersInfoFunc=NULL;
    static bool haveLoaded=false;
    struct AutoUnloadLibrary {
        HMODULE mModule;
        AutoUnloadLibrary() : mModule(NULL) {}
        ~AutoUnloadLibrary() { if(mModule!=NULL) FreeLibrary(mModule); }
    };
    static AutoUnloadLibrary anUnload;
    if(!haveLoaded) {
        HMODULE aModule=LoadLibraryA("Iphlpapi.dll");
        if(aModule!=NULL) {
            anUnload.mModule=aModule;
            aGetAdaptersInfoFunc=(GetAdaptersInfoFunc)GetProcAddress(aModule,"GetAdaptersInfo");
        }
        haveLoaded=true;
    }
    if(aGetAdaptersInfoFunc==NULL) return false;
    DWORD aBufSize=0;
    aGetAdaptersInfoFunc(NULL,&aBufSize);
    if(aBufSize==0 || aBufSize>256000) return false;
    char* aBuf=new char[aBufSize];
    std::auto_ptr<char> aDelBuf(aBuf);
    PIP_ADAPTER_INFO anAdapter=(PIP_ADAPTER_INFO)aBuf;
    aGetAdaptersInfoFunc(anAdapter,&aBufSize);
    for(;anAdapter!=NULL;anAdapter=anAdapter->Next) {
        std::string aName=anAdapter->Description;
        long aHost=ntohl(inet_addr(anAdapter->IpAddressList.IpAddress.String));
        outAddresses.push_back(LobbyNetAddress(aName,aHost));
    }
    return true;
}
// FUNCTION: WON 0x006b32b0
// Lists this machine's IP addresses: from the adapter list when details are asked for, else by host name lookup
void LobbyGetNetAddresses(std::list<LobbyNetAddress>& outAddresses,bool inGetDetails)
{
    outAddresses.clear();
    if(inGetDetails && IPAddr_PlatformGetNetworkAdapterList(outAddresses) && !outAddresses.empty()) return;
    char aHostName[1024];
    if(gethostname(aHostName,sizeof(aHostName))!=0) return;
    hostent* anEntry=gethostbyname(aHostName);
    if(anEntry==NULL) return;
    for(int i=0;anEntry->h_addr_list[i]!=NULL;++i) {
        unsigned long anAddress=*(unsigned long*)anEntry->h_addr_list[i];
        outAddresses.push_back(LobbyNetAddress("",ntohl(anAddress)));
    }
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
// Creates the address from a Winsock sockaddr_in
IPAddr::IPAddr(const sockaddr_in& inAddress)
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
    return reinterpret_cast<const char*>(&this->mAddr.sin_port);
}
// FUNCTION: WON 0x006b2f90
// Sets the port
void IPAddr::SetPort(unsigned short inPort)
{
    this->mAddr.sin_port=htons(inPort);
}
// FUNCTION: WON 0x006b2fb0
// Returns the IP in host byte order
long IPAddr::GetHost() const
{
    return ntohl(this->mAddr.sin_addr.s_addr);
}
// FUNCTION: WON 0x006b3070
// Returns "host:port", formatted into a 50-byte buffer (a long remembered host name overflows it)
std::string IPAddr::GetHostAndPortString(bool inUseOriginal) const
{
    long host=ntohl(this->GetHost());
    std::string hostText;
    unsigned char* bytes=reinterpret_cast<unsigned char*>(&host);
    char text[50];
    sprintf(text,"%s:%d",this->GetHostString(inUseOriginal).c_str(),this->GetPort());
    return text;
}
// FUNCTION: WON 0x006b31c0
// Returns the port in host byte order
unsigned short IPAddr::GetPort() const
{
    return ntohs(this->mAddr.sin_port);
}
// FUNCTION: WON 0x006b31d0
// Returns whether the address has been set successfully
bool IPAddr::IsValid() const
{
    return this->mIsValid;
}
// FUNCTION: WON 0x006b31e0
// Returns the Winsock sockaddr_in
const sockaddr_in& IPAddr::GetSockAddrIn() const
{
    return this->mAddr;
}
// FUNCTION: WON 0x006b31f0
// Orders addresses by their 6-byte wire form (port first)
bool IPAddr::operator<(const IPAddr& other) const
{
    return memcmp(&mAddr.sin_port,&other.mAddr.sin_port,6)<0;
}
// FUNCTION: WON 0x006b3220
// Returns whether two addresses have the same IP and port
bool IPAddr::operator==(const IPAddr& other) const
{
    return memcmp(&this->mAddr.sin_port,&other.mAddr.sin_port,6)==0;
}
// FUNCTION: WON 0x006b3240
// Returns whether two addresses differ in IP or port
bool IPAddr::operator!=(const IPAddr& other) const
{
    return memcmp(&this->mAddr.sin_port,&other.mAddr.sin_port,6)!=0;
}
// FUNCTION: WON 0x006b3260
// Compares two addresses by host-order IP, then by port
int IPAddr::StringCompare(const IPAddr& other) const
{
    int result=this->GetHost()-other.GetHost();
    if(result!=0) return result;
    return this->GetPort()-other.GetPort();
}
}

// SDK static: the chosen local interface in host order; in the original at 565f40
namespace WONAPI {
long IPAddr::mLocalHost = INADDR_ANY;
}

// Compiler-generated for this unit:
// SYNTHETIC: WON 0x006b3790
// (_$E72: atexit destructor of IPAddr_PlatformGetNetworkAdapterList's static anUnload)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006b8710
// (public: __thiscall std::auto_ptr<char>::~auto_ptr<char>(void))
// TEMPLATE: WON 0x006b9130
// (protected: struct std::list<struct WONAPI::LobbyNetAddress,class std::allocator<struct WONAPI::LobbyNetAddress> >::_Node * __thiscall std::list<struct)
// TEMPLATE: WON 0x006b97f0
// (protected: void __thiscall std::list<struct WONAPI::LobbyNetAddress,class std::allocator<struct WONAPI::LobbyNetAddress> >::_Incsize(unsigned int))
// TEMPLATE: WON 0x006b9e30
// (void __cdecl std::_Construct(struct WONAPI::LobbyNetAddress *,struct WONAPI::LobbyNetAddress const &))
