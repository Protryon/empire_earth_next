// Platform layer, POSIX: IPv4 sockets on BSD sockets (include/platform/PlatSocket.h).
//
// Errors are errno mapped to Winsock's numbering (per thread). Readiness events emulate WSAEventSelect with one
// poll thread, started by the first PlatSocketSelectEvent: each registered socket is "armed" until the poll finds
// it ready, then its event is set and it is disarmed until the next receive or accept on it re-arms it (Winsock's
// re-enabling rule: a recv that leaves data behind signals again). A self-pipe wakes the poll when the set changes.
// Sends never raise SIGPIPE (MSG_NOSIGNAL / SO_NOSIGPIPE).
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <map>
#include <mutex>
#include <vector>

#include "platform/PlatSocket.h"
#include "platform/PlatThread.h"

static_assert(sizeof(PlatSockAddrIn) == 16, "PlatSockAddrIn has Windows' sockaddr_in layout");

#if defined(MSG_NOSIGNAL)
#define PLAT_SEND_FLAGS MSG_NOSIGNAL
#else
#define PLAT_SEND_FLAGS 0
#endif

namespace {

thread_local uint32_t tSocketError;

uint32_t WinsockError(int inErrno)
{
    switch (inErrno) {
    case EINTR: return PLAT_SOCK_EINTR;
    case EBADF: return PLAT_SOCK_EBADF;
    case EACCES:
    case EPERM: return PLAT_SOCK_EACCES;
    case EFAULT: return PLAT_SOCK_EFAULT;
    case EINVAL: return PLAT_SOCK_EINVAL;
    case EMFILE:
    case ENFILE: return PLAT_SOCK_EMFILE;
#if EAGAIN != EWOULDBLOCK
    case EAGAIN:
#endif
    case EWOULDBLOCK: return PLAT_SOCK_EWOULDBLOCK;
    case EINPROGRESS: return PLAT_SOCK_EWOULDBLOCK;   // a non-blocking connect: Winsock says WSAEWOULDBLOCK
    case EALREADY: return PLAT_SOCK_EALREADY;
    case ENOTSOCK: return PLAT_SOCK_ENOTSOCK;
    case EMSGSIZE: return PLAT_SOCK_EMSGSIZE;
    case EOPNOTSUPP: return PLAT_SOCK_EOPNOTSUPP;
    case EADDRINUSE: return PLAT_SOCK_EADDRINUSE;
    case EADDRNOTAVAIL: return PLAT_SOCK_EADDRNOTAVAIL;
    case ENETDOWN: return PLAT_SOCK_ENETDOWN;
    case ENETUNREACH: return PLAT_SOCK_ENETUNREACH;
    case ECONNABORTED: return PLAT_SOCK_ECONNABORTED;
    case ECONNRESET:
    case EPIPE: return PLAT_SOCK_ECONNRESET;
    case ENOBUFS:
    case ENOMEM: return PLAT_SOCK_ENOBUFS;
    case EISCONN: return PLAT_SOCK_EISCONN;
    case ENOTCONN: return PLAT_SOCK_ENOTCONN;
    case ETIMEDOUT: return PLAT_SOCK_ETIMEDOUT;
    case ECONNREFUSED: return PLAT_SOCK_ECONNREFUSED;
    case EHOSTUNREACH: return PLAT_SOCK_EHOSTUNREACH;
    default: return PLAT_SOCK_OTHER;
    }
}

int FailErrno()
{
    tSocketError = WinsockError(errno);
    return 0;
}

int Fd(PlatSocket inSocket)
{
    return static_cast<int>(inSocket);
}

void ToNative(const PlatSockAddrIn* inAddress, sockaddr_in& outAddress)
{
    memset(&outAddress, 0, sizeof(outAddress));
#if defined(__APPLE__)
    outAddress.sin_len = sizeof(outAddress);
#endif
    outAddress.sin_family = AF_INET;
    outAddress.sin_port = inAddress->mPort;
    outAddress.sin_addr.s_addr = inAddress->mAddress;
}

void FromNative(const sockaddr_in& inAddress, PlatSockAddrIn* outAddress)
{
    if (!outAddress)
        return;
    memset(outAddress, 0, sizeof(*outAddress));
    outAddress->mFamily = PLAT_AF_INET;
    outAddress->mPort = inAddress.sin_port;
    outAddress->mAddress = inAddress.sin_addr.s_addr;
}

// ---- readiness events

struct Registration {
    PlatEvent* mEvent;
    uint32_t mMask;
    bool mArmed;
};

std::mutex sPollLock;
std::map<int, Registration> sRegistrations;
int sWakePipe[2] = {-1, -1};
bool sPollStarted;

void Wake()
{
    if (sWakePipe[1] >= 0) {
        char c = 0;
        ssize_t theWritten = write(sWakePipe[1], &c, 1);
        (void)theWritten;
    }
}

void* PollThread(void*)
{
    std::vector<pollfd> theFds;
    for (;;) {
        theFds.clear();
        pollfd theWake = {sWakePipe[0], POLLIN, 0};
        theFds.push_back(theWake);
        {
            std::lock_guard<std::mutex> theLock(sPollLock);
            for (const auto& theEntry : sRegistrations)
                if (theEntry.second.mArmed) {
                    pollfd theFd = {theEntry.first, POLLIN, 0};
                    theFds.push_back(theFd);
                }
        }
        if (poll(theFds.data(), static_cast<nfds_t>(theFds.size()), -1) < 0)
            continue;
        if (theFds[0].revents & POLLIN) {
            char theBuffer[64];
            while (read(sWakePipe[0], theBuffer, sizeof(theBuffer)) > 0) {
            }
        }
        std::lock_guard<std::mutex> theLock(sPollLock);
        for (size_t i = 1; i < theFds.size(); i++) {
            if (!(theFds[i].revents & (POLLIN | POLLHUP | POLLERR)))
                continue;
            std::map<int, Registration>::iterator it = sRegistrations.find(theFds[i].fd);
            if (it == sRegistrations.end() || !it->second.mArmed)
                continue;   // closed or re-registered meanwhile
            it->second.mArmed = false;
            PlatEventSet(it->second.mEvent);
        }
    }
    return NULL;
}

bool StartPollThread()
{
    if (sPollStarted)
        return true;
    if (pipe(sWakePipe) != 0)
        return false;
    for (int i = 0; i < 2; i++) {
        fcntl(sWakePipe[i], F_SETFL, fcntl(sWakePipe[i], F_GETFL) | O_NONBLOCK);
        fcntl(sWakePipe[i], F_SETFD, FD_CLOEXEC);
    }
    pthread_t theThread;
    pthread_attr_t theAttributes;
    pthread_attr_init(&theAttributes);
    pthread_attr_setdetachstate(&theAttributes, PTHREAD_CREATE_DETACHED);
    sPollStarted = pthread_create(&theThread, &theAttributes, PollThread, NULL) == 0;
    pthread_attr_destroy(&theAttributes);
    return sPollStarted;
}

// a receive or accept on inSocket: arm its event again
void Rearm(PlatSocket inSocket)
{
    std::lock_guard<std::mutex> theLock(sPollLock);
    std::map<int, Registration>::iterator it = sRegistrations.find(Fd(inSocket));
    if (it != sRegistrations.end() && !it->second.mArmed) {
        it->second.mArmed = true;
        Wake();
    }
}

}   // namespace

extern "C" int PlatSocketStartup(void)
{
    return 1;
}

extern "C" int PlatSocketCleanup(void)
{
    return 1;
}

extern "C" uint32_t PlatSocketLastError(void)
{
    return tSocketError;
}

extern "C" PlatSocket PlatSocketOpen(PlatSocketType inType)
{
    int theSocket = socket(AF_INET, inType == PLAT_SOCKET_STREAM ? SOCK_STREAM : SOCK_DGRAM, 0);
    if (theSocket < 0) {
        FailErrno();
        return PLAT_SOCKET_INVALID;
    }
    fcntl(theSocket, F_SETFD, FD_CLOEXEC);
#if defined(SO_NOSIGPIPE)
    int theOn = 1;
    setsockopt(theSocket, SOL_SOCKET, SO_NOSIGPIPE, &theOn, sizeof(theOn));
#endif
    return theSocket;
}

extern "C" int PlatSocketClose(PlatSocket inSocket)
{
    {
        std::lock_guard<std::mutex> theLock(sPollLock);
        if (sRegistrations.erase(Fd(inSocket)))
            Wake();
    }
    return close(Fd(inSocket)) == 0 ? 1 : FailErrno();
}

extern "C" int PlatSocketBind(PlatSocket inSocket, const PlatSockAddrIn* inAddress)
{
    sockaddr_in theAddress;
    ToNative(inAddress, theAddress);
    return bind(Fd(inSocket), reinterpret_cast<sockaddr*>(&theAddress), sizeof(theAddress)) == 0 ? 1 : FailErrno();
}

extern "C" int PlatSocketListen(PlatSocket inSocket, int inBacklog)
{
    if (inBacklog == PLAT_SOCKET_MAX_BACKLOG)
        inBacklog = SOMAXCONN;
    return listen(Fd(inSocket), inBacklog) == 0 ? 1 : FailErrno();
}

extern "C" PlatSocket PlatSocketAccept(PlatSocket inSocket, PlatSockAddrIn* outPeer)
{
    sockaddr_in theAddress;
    socklen_t theLength = sizeof(theAddress);
    int theSocket = accept(Fd(inSocket), reinterpret_cast<sockaddr*>(&theAddress), &theLength);
    int theError = errno;
    Rearm(inSocket);
    if (theSocket < 0) {
        errno = theError;
        FailErrno();
        return PLAT_SOCKET_INVALID;
    }
    fcntl(theSocket, F_SETFD, FD_CLOEXEC);
#if defined(SO_NOSIGPIPE)
    int theOn = 1;
    setsockopt(theSocket, SOL_SOCKET, SO_NOSIGPIPE, &theOn, sizeof(theOn));
#endif
    // Winsock: the accepted socket takes the listening socket's blocking mode (and its event selection, which the
    // game replaces right away; not carried over here)
    int theFlags = fcntl(Fd(inSocket), F_GETFL);
    if (theFlags >= 0 && (theFlags & O_NONBLOCK))
        fcntl(theSocket, F_SETFL, fcntl(theSocket, F_GETFL) | O_NONBLOCK);
    FromNative(theAddress, outPeer);
    return theSocket;
}

extern "C" int PlatSocketConnect(PlatSocket inSocket, const PlatSockAddrIn* inAddress)
{
    sockaddr_in theAddress;
    ToNative(inAddress, theAddress);
    int theResult;
    do
        theResult = connect(Fd(inSocket), reinterpret_cast<sockaddr*>(&theAddress), sizeof(theAddress));
    while (theResult != 0 && errno == EINTR);
    return theResult == 0 ? 1 : FailErrno();
}

extern "C" int PlatSocketGetName(PlatSocket inSocket, PlatSockAddrIn* outAddress)
{
    sockaddr_in theAddress;
    socklen_t theLength = sizeof(theAddress);
    if (getsockname(Fd(inSocket), reinterpret_cast<sockaddr*>(&theAddress), &theLength) != 0)
        return FailErrno();
    FromNative(theAddress, outAddress);
    return 1;
}

extern "C" int PlatSocketSetNonBlocking(PlatSocket inSocket, int inNonBlocking)
{
    int theFlags = fcntl(Fd(inSocket), F_GETFL);
    if (theFlags < 0)
        return FailErrno();
    theFlags = inNonBlocking ? (theFlags | O_NONBLOCK) : (theFlags & ~O_NONBLOCK);
    return fcntl(Fd(inSocket), F_SETFL, theFlags) == 0 ? 1 : FailErrno();
}

extern "C" int PlatSocketSetBroadcast(PlatSocket inSocket, int inBroadcast)
{
    int theValue = inBroadcast ? 1 : 0;
    return setsockopt(Fd(inSocket), SOL_SOCKET, SO_BROADCAST, &theValue, sizeof(theValue)) == 0 ? 1 : FailErrno();
}

extern "C" int32_t PlatSocketSend(PlatSocket inSocket, const void* inData, uint32_t inLength)
{
    ssize_t theSent;
    do
        theSent = send(Fd(inSocket), inData, inLength, PLAT_SEND_FLAGS);
    while (theSent < 0 && errno == EINTR);
    if (theSent < 0) {
        FailErrno();
        return -1;
    }
    return static_cast<int32_t>(theSent);
}

extern "C" int32_t PlatSocketRecv(PlatSocket inSocket, void* outData, uint32_t inLength)
{
    ssize_t theRead;
    do
        theRead = recv(Fd(inSocket), outData, inLength, 0);
    while (theRead < 0 && errno == EINTR);
    int theError = errno;
    Rearm(inSocket);
    if (theRead < 0) {
        errno = theError;
        FailErrno();
        return -1;
    }
    return static_cast<int32_t>(theRead);
}

extern "C" int32_t PlatSocketPeek(PlatSocket inSocket, void* outData, uint32_t inLength)
{
    ssize_t theRead;
    do
        theRead = recv(Fd(inSocket), outData, inLength, MSG_PEEK);
    while (theRead < 0 && errno == EINTR);
    int theError = errno;
    Rearm(inSocket);   // Winsock re-enables FD_READ after a peek too (the data is still there)
    if (theRead < 0) {
        errno = theError;
        FailErrno();
        return -1;
    }
    return static_cast<int32_t>(theRead);
}

extern "C" int32_t PlatSocketSendTo(PlatSocket inSocket, const void* inData, uint32_t inLength,
                                    const PlatSockAddrIn* inTo)
{
    sockaddr_in theAddress;
    ToNative(inTo, theAddress);
    ssize_t theSent;
    do
        theSent = sendto(Fd(inSocket), inData, inLength, PLAT_SEND_FLAGS, reinterpret_cast<sockaddr*>(&theAddress),
                         sizeof(theAddress));
    while (theSent < 0 && errno == EINTR);
    if (theSent < 0) {
        FailErrno();
        return -1;
    }
    return static_cast<int32_t>(theSent);
}

extern "C" int32_t PlatSocketRecvFrom(PlatSocket inSocket, void* outData, uint32_t inLength, PlatSockAddrIn* outFrom)
{
    sockaddr_in theAddress;
    socklen_t theLength = sizeof(theAddress);
    ssize_t theRead;
    do
        theRead = recvfrom(Fd(inSocket), outData, inLength, 0, reinterpret_cast<sockaddr*>(&theAddress), &theLength);
    while (theRead < 0 && errno == EINTR);
    int theError = errno;
    Rearm(inSocket);
    if (theRead < 0) {
        errno = theError;
        FailErrno();
        return -1;
    }
    FromNative(theAddress, outFrom);
    return static_cast<int32_t>(theRead);
}

extern "C" int PlatSocketSelectEvent(PlatSocket inSocket, PlatEvent* inEvent, uint32_t inMask)
{
    std::lock_guard<std::mutex> theLock(sPollLock);
    if (inMask == 0 || inEvent == NULL) {
        sRegistrations.erase(Fd(inSocket));
        Wake();
        return 1;
    }
    if (!StartPollThread()) {
        tSocketError = PLAT_SOCK_ENOBUFS;
        return 0;
    }
    // WSAEventSelect makes the socket non-blocking
    int theFlags = fcntl(Fd(inSocket), F_GETFL);
    if (theFlags < 0 || fcntl(Fd(inSocket), F_SETFL, theFlags | O_NONBLOCK) != 0)
        return FailErrno();
    Registration theRegistration = {inEvent, inMask, true};
    sRegistrations[Fd(inSocket)] = theRegistration;
    Wake();
    return 1;
}

extern "C" int PlatSocketGetHostName(char* outName, uint32_t inSize)
{
    return gethostname(outName, inSize) == 0 ? 1 : FailErrno();
}

extern "C" uint32_t PlatSocketResolveIPv4(const char* inName, uint32_t* outAddresses, uint32_t inMax)
{
    addrinfo theHints;
    memset(&theHints, 0, sizeof(theHints));
    theHints.ai_family = AF_INET;
    theHints.ai_socktype = SOCK_STREAM;   // one entry per address
    addrinfo* theList = NULL;
    if (getaddrinfo(inName, NULL, &theHints, &theList) != 0 || theList == NULL) {
        tSocketError = PLAT_SOCK_HOST_NOT_FOUND;
        return 0;
    }
    uint32_t n = 0;
    for (addrinfo* p = theList; p != NULL && n < inMax; p = p->ai_next) {
        if (p->ai_family != AF_INET)
            continue;
        uint32_t theAddress = reinterpret_cast<sockaddr_in*>(p->ai_addr)->sin_addr.s_addr;
        bool theSeen = false;
        for (uint32_t i = 0; i < n; i++)
            theSeen = theSeen || outAddresses[i] == theAddress;
        if (!theSeen)
            outAddresses[n++] = theAddress;
    }
    freeaddrinfo(theList);
    if (n == 0)
        tSocketError = PLAT_SOCK_HOST_NOT_FOUND;
    return n;
}

extern "C" int PlatSocketReverseLookup(uint32_t inAddress, char* outName, uint32_t inSize)
{
    sockaddr_in theAddress;
    memset(&theAddress, 0, sizeof(theAddress));
#if defined(__APPLE__)
    theAddress.sin_len = sizeof(theAddress);
#endif
    theAddress.sin_family = AF_INET;
    theAddress.sin_addr.s_addr = inAddress;
    if (getnameinfo(reinterpret_cast<sockaddr*>(&theAddress), sizeof(theAddress), outName, inSize, NULL, 0,
                    NI_NAMEREQD) != 0) {
        tSocketError = PLAT_SOCK_HOST_NOT_FOUND;
        return 0;
    }
    return 1;
}

extern "C" uint32_t PlatSocketParseIPv4(const char* inText)
{
    // inet_addr: also accepts the short forms ("127.1") and octal/hex parts, as Winsock does
    return inet_addr(inText);
}

extern "C" void PlatSocketFormatIPv4(uint32_t inAddress, char* outText)
{
    const unsigned char* b = reinterpret_cast<const unsigned char*>(&inAddress);
    snprintf(outText, 16, "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
}

extern "C" uint32_t PlatSocketGetAdapters(PlatNetAdapter* outAdapters, uint32_t inMax)
{
    ifaddrs* theList = NULL;
    if (getifaddrs(&theList) != 0) {
        FailErrno();
        return 0;
    }
    uint32_t n = 0;
    for (ifaddrs* p = theList; p != NULL && n < inMax; p = p->ifa_next) {
        if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET || (p->ifa_flags & IFF_LOOPBACK)
            || !(p->ifa_flags & IFF_UP))
            continue;   // GetAdaptersInfo lists no loopback adapter
        PlatNetAdapter& theOut = outAdapters[n++];
        memset(&theOut, 0, sizeof(theOut));
        snprintf(theOut.mDescription, sizeof(theOut.mDescription), "%s", p->ifa_name);
        theOut.mAddress = reinterpret_cast<sockaddr_in*>(p->ifa_addr)->sin_addr.s_addr;
        theOut.mNetmask = p->ifa_netmask ? reinterpret_cast<sockaddr_in*>(p->ifa_netmask)->sin_addr.s_addr : 0;
    }
    freeifaddrs(theList);
    return n;
}
