// Platform layer, Win32: IPv4 sockets on Winsock 2 (include/platform/PlatSocket.h). A PlatSocket is the SOCKET, a
// PlatSockAddrIn has sockaddr_in's layout, and the readiness event is WSAEventSelect on the PlatEvent's handle (on
// Win32 a PlatEvent* is the event HANDLE, platform/PlatThread.h). The calls are the Winsock functions the TCP
// protocol made itself, so the test harness's socket guards see the same imports.
#include <winsock2.h>
#include <iphlpapi.h>
#include <windows.h>
#include <string.h>

#include "platform/PlatSocket.h"

static_assert(sizeof(PlatSockAddrIn) == sizeof(sockaddr_in), "PlatSockAddrIn is sockaddr_in");
static_assert(PLAT_SOCKET_EVENT_READ == FD_READ && PLAT_SOCKET_EVENT_ACCEPT == FD_ACCEPT, "Winsock's FD_* bits");
static_assert(PLAT_SOCKET_MAX_BACKLOG == SOMAXCONN, "Winsock 2's SOMAXCONN");
static_assert(PLAT_SOCK_EWOULDBLOCK == WSAEWOULDBLOCK && PLAT_SOCK_HOST_NOT_FOUND == WSAHOST_NOT_FOUND,
              "Winsock's error numbering");
static_assert(PLAT_SOCK_EDESTADDRREQ == WSAEDESTADDRREQ && PLAT_SOCK_ESTALE == WSAESTALE
                  && PLAT_SOCK_EREMOTE == WSAEREMOTE
                  && PLAT_SOCK_SYSNOTREADY == WSASYSNOTREADY && PLAT_SOCK_VERNOTSUPPORTED == WSAVERNOTSUPPORTED
                  && PLAT_SOCK_EDISCON == WSAEDISCON && PLAT_SOCK_ENOTEMPTY == WSAENOTEMPTY,
              "Winsock's error numbering");
static_assert(PLAT_SOCKET_SHUT_RECEIVE == SD_RECEIVE && PLAT_SOCKET_SHUT_SEND == SD_SEND
                  && PLAT_SOCKET_SHUT_BOTH == SD_BOTH, "Winsock's SD_* values");

namespace {

SOCKET Native(PlatSocket inSocket)
{
    return static_cast<SOCKET>(inSocket);
}

const sockaddr* Address(const PlatSockAddrIn* inAddress)
{
    return reinterpret_cast<const sockaddr*>(inAddress);
}

}   // namespace

extern "C" int PlatSocketStartup(void)
{
    WSADATA theData;
    return WSAStartup(MAKEWORD(2, 0), &theData) == 0 ? 1 : 0;
}

extern "C" int PlatSocketCleanup(void)
{
    return WSACleanup() == 0 ? 1 : 0;
}

extern "C" uint32_t PlatSocketLastError(void)
{
    return static_cast<uint32_t>(WSAGetLastError());
}

extern "C" PlatSocket PlatSocketOpen(PlatSocketType inType)
{
    SOCKET theSocket = socket(AF_INET, inType == PLAT_SOCKET_STREAM ? SOCK_STREAM : SOCK_DGRAM, 0);
    return theSocket == INVALID_SOCKET ? PLAT_SOCKET_INVALID : static_cast<PlatSocket>(theSocket);
}

extern "C" int PlatSocketClose(PlatSocket inSocket)
{
    return closesocket(Native(inSocket)) == 0 ? 1 : 0;
}

extern "C" int PlatSocketBind(PlatSocket inSocket, const PlatSockAddrIn* inAddress)
{
    return bind(Native(inSocket), Address(inAddress), sizeof(sockaddr_in)) == 0 ? 1 : 0;
}

extern "C" int PlatSocketListen(PlatSocket inSocket, int inBacklog)
{
    return listen(Native(inSocket), inBacklog) == 0 ? 1 : 0;
}

extern "C" PlatSocket PlatSocketAccept(PlatSocket inSocket, PlatSockAddrIn* outPeer)
{
    int theLength = sizeof(sockaddr_in);
    SOCKET theSocket = accept(Native(inSocket), reinterpret_cast<sockaddr*>(outPeer), outPeer ? &theLength : NULL);
    return theSocket == INVALID_SOCKET ? PLAT_SOCKET_INVALID : static_cast<PlatSocket>(theSocket);
}

extern "C" int PlatSocketConnect(PlatSocket inSocket, const PlatSockAddrIn* inAddress)
{
    return connect(Native(inSocket), Address(inAddress), sizeof(sockaddr_in)) == 0 ? 1 : 0;
}

extern "C" int PlatSocketGetName(PlatSocket inSocket, PlatSockAddrIn* outAddress)
{
    int theLength = sizeof(sockaddr_in);
    return getsockname(Native(inSocket), reinterpret_cast<sockaddr*>(outAddress), &theLength) == 0 ? 1 : 0;
}

extern "C" int PlatSocketSetNonBlocking(PlatSocket inSocket, int inNonBlocking)
{
    u_long theValue = inNonBlocking ? 1 : 0;
    return ioctlsocket(Native(inSocket), FIONBIO, &theValue) == 0 ? 1 : 0;
}

extern "C" int PlatSocketSetBroadcast(PlatSocket inSocket, int inBroadcast)
{
    BOOL theValue = inBroadcast ? TRUE : FALSE;
    return setsockopt(Native(inSocket), SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&theValue),
                      sizeof(theValue)) == 0 ? 1 : 0;
}

extern "C" int PlatSocketSetLinger(PlatSocket inSocket, int inOn, int inSeconds)
{
    linger theValue;
    theValue.l_onoff = static_cast<u_short>(inOn ? 1 : 0);
    theValue.l_linger = static_cast<u_short>(inSeconds);
    return setsockopt(Native(inSocket), SOL_SOCKET, SO_LINGER, reinterpret_cast<const char*>(&theValue),
                      sizeof(theValue)) == 0 ? 1 : 0;
}

extern "C" int PlatSocketSetReuseAddress(PlatSocket inSocket, int inReuse)
{
    BOOL theValue = inReuse ? TRUE : FALSE;
    return setsockopt(Native(inSocket), SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&theValue),
                      sizeof(theValue)) == 0 ? 1 : 0;
}

extern "C" int PlatSocketSetNoDelay(PlatSocket inSocket, int inNoDelay)
{
    BOOL theValue = inNoDelay ? TRUE : FALSE;
    return setsockopt(Native(inSocket), IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&theValue),
                      sizeof(theValue)) == 0 ? 1 : 0;
}

extern "C" int PlatSocketShutdown(PlatSocket inSocket, int inHow)
{
    return shutdown(Native(inSocket), inHow) == 0 ? 1 : 0;
}

extern "C" int32_t PlatSocketWait(PlatSocket inSocket, uint32_t inMask, uint32_t inTimeoutMs)
{
    fd_set theRead, theWrite, theExcept;
    FD_ZERO(&theRead);
    FD_ZERO(&theWrite);
    FD_ZERO(&theExcept);
    if (inMask & PLAT_SOCKET_WAIT_READ)
        FD_SET(Native(inSocket), &theRead);
    if (inMask & PLAT_SOCKET_WAIT_WRITE)
        FD_SET(Native(inSocket), &theWrite);
    if (inMask & PLAT_SOCKET_WAIT_EXCEPT)
        FD_SET(Native(inSocket), &theExcept);
    timeval theTime;
    theTime.tv_sec = static_cast<long>(inTimeoutMs / 1000);
    theTime.tv_usec = static_cast<long>((inTimeoutMs % 1000) * 1000);
    int theResult = select(0, &theRead, &theWrite, &theExcept, inTimeoutMs == 0xFFFFFFFFu ? NULL : &theTime);
    if (theResult == SOCKET_ERROR)
        return -1;
    int32_t theReady = 0;
    if (FD_ISSET(Native(inSocket), &theRead))
        theReady |= PLAT_SOCKET_WAIT_READ;
    if (FD_ISSET(Native(inSocket), &theWrite))
        theReady |= PLAT_SOCKET_WAIT_WRITE;
    if (FD_ISSET(Native(inSocket), &theExcept))
        theReady |= PLAT_SOCKET_WAIT_EXCEPT;
    return theReady;
}

extern "C" uint32_t PlatSocketGetError(PlatSocket inSocket)
{
    int theError = 0;
    int theLength = sizeof(theError);
    if (getsockopt(Native(inSocket), SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&theError), &theLength) != 0)
        return static_cast<uint32_t>(WSAGetLastError());
    return static_cast<uint32_t>(theError);
}

extern "C" int32_t PlatSocketSend(PlatSocket inSocket, const void* inData, uint32_t inLength)
{
    return send(Native(inSocket), static_cast<const char*>(inData), static_cast<int>(inLength), 0);
}

extern "C" int32_t PlatSocketRecv(PlatSocket inSocket, void* outData, uint32_t inLength)
{
    return recv(Native(inSocket), static_cast<char*>(outData), static_cast<int>(inLength), 0);
}

extern "C" int32_t PlatSocketPeek(PlatSocket inSocket, void* outData, uint32_t inLength)
{
    return recv(Native(inSocket), static_cast<char*>(outData), static_cast<int>(inLength), MSG_PEEK);
}

extern "C" int32_t PlatSocketSendTo(PlatSocket inSocket, const void* inData, uint32_t inLength,
                                    const PlatSockAddrIn* inTo)
{
    return sendto(Native(inSocket), static_cast<const char*>(inData), static_cast<int>(inLength), 0, Address(inTo),
                  sizeof(sockaddr_in));
}

extern "C" int32_t PlatSocketRecvFrom(PlatSocket inSocket, void* outData, uint32_t inLength, PlatSockAddrIn* outFrom)
{
    int theLength = sizeof(sockaddr_in);
    return recvfrom(Native(inSocket), static_cast<char*>(outData), static_cast<int>(inLength), 0,
                    reinterpret_cast<sockaddr*>(outFrom), outFrom ? &theLength : NULL);
}

extern "C" int PlatSocketSelectEvent(PlatSocket inSocket, PlatEvent* inEvent, uint32_t inMask)
{
    return WSAEventSelect(Native(inSocket), reinterpret_cast<WSAEVENT>(inEvent), static_cast<long>(inMask)) == 0 ? 1
                                                                                                                : 0;
}

extern "C" int PlatSocketGetHostName(char* outName, uint32_t inSize)
{
    return gethostname(outName, static_cast<int>(inSize)) == 0 ? 1 : 0;
}

extern "C" uint32_t PlatSocketResolveIPv4(const char* inName, uint32_t* outAddresses, uint32_t inMax)
{
    hostent* theHost = gethostbyname(inName);
    if (theHost == NULL || theHost->h_addrtype != AF_INET)
        return 0;
    uint32_t n = 0;
    for (; theHost->h_addr_list[n] != NULL; n++)
        if (n < inMax)
            memcpy(&outAddresses[n], theHost->h_addr_list[n], 4);
    return n < inMax ? n : inMax;
}

extern "C" int PlatSocketReverseLookup(uint32_t inAddress, char* outName, uint32_t inSize)
{
    hostent* theHost = gethostbyaddr(reinterpret_cast<const char*>(&inAddress), 4, AF_INET);
    if (theHost == NULL || inSize == 0)
        return 0;
    strncpy(outName, theHost->h_name, inSize - 1);
    outName[inSize - 1] = 0;
    return 1;
}

extern "C" uint32_t PlatSocketParseIPv4(const char* inText)
{
    return inet_addr(inText);
}

extern "C" void PlatSocketFormatIPv4(uint32_t inAddress, char* outText)
{
    in_addr theAddress;
    theAddress.s_addr = inAddress;
    strncpy(outText, inet_ntoa(theAddress), 15);
    outText[15] = 0;
}

extern "C" uint32_t PlatSocketGetAdapters(PlatNetAdapter* outAdapters, uint32_t inMax)
{
    ULONG theSize = 0x1900;
    char* theBuffer = new char[theSize];
    DWORD theError = GetAdaptersInfo(reinterpret_cast<IP_ADAPTER_INFO*>(theBuffer), &theSize);
    if (theError == ERROR_BUFFER_OVERFLOW) {
        delete[] theBuffer;
        theBuffer = new char[theSize];
        theError = GetAdaptersInfo(reinterpret_cast<IP_ADAPTER_INFO*>(theBuffer), &theSize);
    }
    uint32_t n = 0;
    if (theError == ERROR_SUCCESS) {
        for (IP_ADAPTER_INFO* theAdapter = reinterpret_cast<IP_ADAPTER_INFO*>(theBuffer); theAdapter != NULL;
             theAdapter = theAdapter->Next) {
            if (n < inMax) {
                PlatNetAdapter& theOut = outAdapters[n];
                strncpy(theOut.mDescription, theAdapter->Description, sizeof(theOut.mDescription) - 1);
                theOut.mDescription[sizeof(theOut.mDescription) - 1] = 0;
                theOut.mAddress = inet_addr(theAdapter->IpAddressList.IpAddress.String);
                theOut.mNetmask = inet_addr(theAdapter->IpAddressList.IpMask.String);
            }
            n++;
        }
    }
    delete[] theBuffer;
    return n < inMax ? n : inMax;
}
