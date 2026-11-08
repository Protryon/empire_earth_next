// WON's AsyncSocket on the platform socket layer, over 127.0.0.1: a TCP listen / non-blocking connect / accept /
// send / recv / peer close, and a UDP SendTo / RecvFrom, plus IPAddr's parsing. Links with the won module's
// AsyncSocket, AsyncSocketDefaults, AsyncSocketInternet, IPAddr and NetStats units and the platform socket layer.
// Prints one line per failed check; the exit code is the number of failed checks.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "platform/PlatSocket.h"
#include "platform/PlatTime.h"

using namespace WONAPI;

static int sFailures;
#define CHECK(c) do { if (!(c)) { sFailures++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

// Retries a non-blocking call that reports WS_TimedOut (would block) for up to two seconds, waiting on the socket
template <class F> static WONStatus Retry(AsyncSocket& inSocket, bool inRead, F inCall)
{
    uint64_t theEnd = PlatTimeMilliseconds64() + 2000;
    WONStatus theStatus;
    while ((theStatus = inCall()) == WS_TimedOut && PlatTimeMilliseconds64() < theEnd) {
        if (inRead)
            inSocket.WaitForRead(50);
        else
            inSocket.WaitForWrite(50);
    }
    return theStatus;
}

int main()
{
    CHECK(AsyncSocket::StartWinsock() == 0);

    // IPAddr parsing (no DNS)
    IPAddr anAddr;
    CHECK(anAddr.SetWithDefaultPortNoDNS("127.0.0.1:1234", 99));
    CHECK(anAddr.GetHost() == 0x7f000001 && anAddr.GetPort() == 1234);
    CHECK(anAddr.GetHostAndPortString(false) == "127.0.0.1:1234");
    CHECK(anAddr.SetWithDefaultPortNoDNS("10.1.2.3", 99) && anAddr.GetPort() == 99);
    CHECK(!anAddr.SetWithDefaultPortNoDNS("not an address", 99));
    const unsigned char theSix[6] = {0x04, 0xd2, 127, 0, 0, 1};
    CHECK(memcmp(IPAddr("127.0.0.1", 1234).GetSixByte(), theSix, 6) == 0);
    CHECK(IPAddr("localhost", 7).IsValid());   // resolves by name
    CHECK(IPAddr::GetBroadcastAddr(5).GetHostString(false) == "255.255.255.255");

    // TCP: listen on an ephemeral port, connect, accept
    AsyncSocket aListener(0);
    CHECK(aListener.Bind(IPAddr("127.0.0.1", 0), true) == 0);
    unsigned short aPort = aListener.GetLocalPort();
    CHECK(aPort != 0);
    CHECK(aListener.Listen(5) == WS_Success);

    AsyncSocket aClient(0);
    WONStatus aStatus = aClient.Fn2B0E60(IPAddr("127.0.0.1", aPort));
    CHECK(aStatus == WS_Success || aStatus == WS_TimedOut);   // non-blocking: in progress
    CHECK(aClient.WaitForConnect(2000) == WS_Success);
    CHECK(aClient.SetNagling(false) == 0);

    SmartPtr<AsyncSocket> anAccepted;
    aListener.WaitForAccept(2000);
    CHECK(aListener.Accept(anAccepted) == 0);
    CHECK(anAccepted.get() != NULL && anAccepted->IsConnected());
    if (anAccepted.get() != NULL) {
        CHECK(anAccepted->GetDestAddr().GetHost() == 0x7f000001);

        // a read with nothing queued would block -> WS_TimedOut
        char aBuffer[64];
        int aCount = -1;
        CHECK(anAccepted->Recv(aBuffer, sizeof(aBuffer), &aCount) == WS_TimedOut);

        const char aMessage[] = "hello, WON";
        int aSent = 0;
        CHECK(Retry(aClient, false, [&] { return aClient.Send(aMessage, sizeof(aMessage), &aSent); }) == WS_Success);
        CHECK(aSent == (int)sizeof(aMessage));
        aCount = 0;
        CHECK(Retry(*anAccepted.get(), true, [&] { return anAccepted->Recv(aBuffer, sizeof(aMessage), &aCount); })
              == WS_Success);
        CHECK(aCount == (int)sizeof(aMessage) && memcmp(aBuffer, aMessage, sizeof(aMessage)) == 0);

        // the peer closes -> Shutdown
        aClient.Close();
        CHECK(!aClient.IsValid());
        CHECK(Retry(*anAccepted.get(), true, [&] { return anAccepted->Recv(aBuffer, sizeof(aBuffer), &aCount); })
              == WS_AsyncSocket_Shutdown);
    }

    // connecting to a port nobody listens on fails with a Winsock error (not 0, not WS_TimedOut)
    aListener.Close();
    AsyncSocket aRefused2(0);
    aStatus = aRefused2.Fn2B0E60(IPAddr("127.0.0.1", aPort));
    if (aStatus == WS_TimedOut || aStatus == WS_Success)
        aStatus = aRefused2.WaitForConnect(3000);
    CHECK(aStatus == PLAT_SOCK_ECONNREFUSED);
    if (aStatus != PLAT_SOCK_ECONNREFUSED)
        printf("  refused connect -> %ld\n", (long)aStatus);

    // UDP: SendTo / RecvFrom over loopback
    AsyncSocket aReceiver(1);
    CHECK(aReceiver.Bind(IPAddr("127.0.0.1", 0), false) == 0);
    unsigned short aUdpPort = aReceiver.GetLocalPort();
    AsyncSocket aSender(1);
    CHECK(aSender.Bind(IPAddr("127.0.0.1", 0), false) == 0);
    const char aDatagram[] = "datagram";
    CHECK(aSender.SendTo(aDatagram, sizeof(aDatagram), IPAddr("127.0.0.1", aUdpPort)) == WS_Success);
    char aBuffer[64];
    IPAddr aFrom;
    int aCount = 0;
    CHECK(Retry(aReceiver, true, [&] { return aReceiver.RecvFrom(aBuffer, sizeof(aBuffer), &aFrom, &aCount); })
          == WS_Success);
    CHECK(aCount == (int)sizeof(aDatagram) && memcmp(aBuffer, aDatagram, sizeof(aDatagram)) == 0);
    CHECK(aFrom.GetHost() == 0x7f000001 && aFrom.GetPort() == aSender.GetLocalPort());
    CHECK(aReceiver.Send(aDatagram, 1, NULL) != WS_Success);   // a datagram socket with no peer

    // local addresses (informational: depends on the machine)
    std::list<LobbyNetAddress> anAddresses;
    LobbyGetNetAddresses(anAddresses, true);
    printf("adapters: %u, internet: %d\n", (unsigned)anAddresses.size(), (int)AsyncSocket::HasInternetConnection());

    aClient.Close();
    aReceiver.Close();
    aSender.Close();
    CHECK(AsyncSocket::StopWinsock() == 0);
    printf("%s (%d failed)\n", sFailures ? "FAIL" : "PASS", sFailures);
    return sFailures;
}
