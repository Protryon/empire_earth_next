// LanLogic's flow (LanLogic.cpp): its decisions, the original's statements (copied from the C++ before the port, the
// objects replaced by the state they answered) against ee_won's lanlogic module through its C ABI, over every
// combination of their inputs: HandleRecvMsg, HandleMessage's offset, SendMessageToClient, SendMessageToCaptain,
// BroadcastMessage, HandleSocketClosed, HandleGameSummary, CanBroadcastGame, AdvertiseGame, HostGame's and
// JoinGame's peer-auth rule, JoinGame's status switch, ReadJoinReply's roles, HandleClientAction,
// HandleChatCommand, HandleChatMessage's whisper rule, RefreshGames, ChangeLanName, ChangeLanPort and
// HandleChangeDirectConnect's port, name and event. Prints one line per failed check; the exit code is the number
// of failed checks.
#include <stdio.h>
#include <string.h>
#include <vector>
#include "rust/ee_rust.h"

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

// the chat commands (WONLobbyChatCommands.h)
enum { LobbyChatCommand_Broadcast = 1, LobbyChatCommand_Whisper = 2, LobbyChatCommand_Emote = 3,
       LobbyChatCommand_Ignore = 5 };

static bool B(int inBits, int inBit) { return ((inBits >> inBit) & 1) != 0; }

static void TestRecvAndRoutes()
{
    for (int c = 0; c < 8; c++) {
        bool ok = B(c, 0), captain = B(c, 1), hasClient = B(c, 2);
        // HandleRecvMsg
        bool handle = false, stamp = false, newSocket = false;
        do {
            if (!ok) break;
            handle = true;
            if (captain) {
                stamp = true;
                if (!hasClient) { newSocket = true; break; }
            }
        } while (0);
        EeWonLanRecv r = ee_won_lanlogic_recv(ok, captain, hasClient);
        CHECK(r.handle == handle && r.stamp == stamp && r.new_socket == newSocket);
    }
    for (int c = 0; c < 4; c++) {
        size_t anOffset = 0; // HandleMessage
        if (B(c, 0) && B(c, 1)) anOffset = 2;
        CHECK(ee_won_lanlogic_message_offset(B(c, 0), B(c, 1)) == anOffset);
    }
    for (int c = 0; c < 16; c++) {
        bool captain = B(c, 0), connected = B(c, 1), toMe = B(c, 2), hasSocket = B(c, 3);
        int aWant; // SendMessageToClient
        if (!captain || !connected) aWant = 0;
        else if (toMe) aWant = 1;
        else if (!hasSocket) aWant = 0;
        else aWant = 2;
        CHECK((int)ee_won_lanlogic_send_to_client(captain, connected, toMe, hasSocket) == aWant);
    }
    for (int c = 0; c < 8; c++) {
        bool connected = B(c, 0), captain = B(c, 1), hasServer = B(c, 2);
        int aWant = 0; // SendMessageToCaptain
        if (!connected) aWant = 0;
        else if (captain) aWant = 1;
        else if (hasServer) aWant = 2;
        CHECK((int)ee_won_lanlogic_send_to_captain(connected, captain, hasServer) == aWant);
    }
    // BroadcastMessage over every socket set of up to four
    for (int n = 0; n <= 4; n++)
        for (int c = 0; c < (4 << n); c++) {
            bool captain = B(c, 0), local = B(c, 1);
            bool has[4], send[4], want[4];
            bool aLocal = false;
            for (int k = 0; k < n; k++) { has[k] = B(c, 2 + k); want[k] = false; send[k] = true; }
            if (captain) {
                if (local) aLocal = true;
                for (int k = 0; k < n; k++)
                    if (has[k]) want[k] = true;
            }
            CHECK(ee_won_lanlogic_broadcast(captain, local, n ? has : NULL, n ? send : NULL, (size_t)n) == aLocal);
            for (int k = 0; k < n; k++) CHECK(send[k] == want[k]);
        }
}

static void TestSocketsAndDiscovery()
{
    for (int c = 0; c < 16; c++) {
        bool have = B(c, 0), captain = B(c, 1), hasClient = B(c, 2), isServer = B(c, 3);
        bool announce = false, forget = false, lost = false; // HandleSocketClosed
        if (have) {
            if (captain) {
                if (hasClient) announce = true;
                forget = true;
            } else if (isServer) lost = true;
        }
        EeWonLanClosed r = ee_won_lanlogic_socket_closed(have, captain, hasClient, isServer);
        CHECK(r.announce_left == announce && r.forget_socket == forget && r.lost_host == lost);
    }
    for (int c = 0; c < 4; c++) {
        bool readOk = B(c, 0), aFlag = B(c, 1);
        bool keep; int anEvent = -1; // HandleGameSummary
        if (readOk) { keep = true; anEvent = aFlag ? 2 : 0; }
        else { keep = false; if (aFlag) anEvent = aFlag ? 2 : 1; }
        EeWonLanSummary r = ee_won_lanlogic_summary(readOk, aFlag);
        CHECK(r.keep == keep && r.event == anEvent);
    }
    for (int t = -1; t < 5; t++) {
        for (int c = 0; c < 8; c++) {
            bool captain = B(c, 0), discovery = B(c, 1), game = B(c, 2);
            bool aWant; // CanBroadcastGame
            if (!captain || !discovery || !game) aWant = false;
            else aWant = t == 1 ? true : false;
            CHECK(ee_won_lanlogic_can_broadcast(captain, discovery, game, t) == aWant);
        }
        CHECK(ee_won_lanlogic_advertises(t) == (t == 1)); // AdvertiseGame / RemoveGameAdvertisement
        for (int k = 0; k < 2; k++) CHECK(ee_won_lanlogic_needs_peer_auth(t, k != 0) == (t == 3 && k != 0));
    }
}

static void TestJoining()
{
    for (int aStatus = -3; aStatus < 10; aStatus++) {
        int aWant; // JoinGame's switch: 0 joined, 1 return false, 2 the password again, 3 the subnet, else again
        switch (aStatus) {
        case 0: aWant = 0; break;
        case 1: aWant = 1; break;
        case 2: aWant = 2; break;
        case 3: aWant = 3; break;
        default: aWant = 4;
        }
        CHECK((int)ee_won_lanlogic_join_step(aStatus) == aWant);
    }
    for (unsigned short anId = 0; anId < 5; anId++)
        for (unsigned short aMy = 0; aMy < 5; aMy++)
            for (unsigned short aCaptain = 0; aCaptain < 5; aCaptain++) {
                int aWant = 0; // ReadJoinReply
                if (anId == aMy) aWant = 1;
                else if (anId == aCaptain) aWant = 2;
                CHECK(ee_won_lanlogic_reply_role(anId, aMy, aCaptain) == aWant);
            }
}

static void TestCommands()
{
    for (int c = 0; c < 4; c++)
        for (int aSpec = 0; aSpec < 32; aSpec++)
            for (int anAction = -1; anAction < 22; anAction++) {
                bool connected = B(c, 0), hasClient = B(c, 1);
                int aWant = 0; // HandleClientAction
                do {
                    if (!connected || !((aSpec & 8) ? true : false)) break;
                    if (!hasClient) break;
                    switch (anAction) {
                    case 18: aWant = 18; break;
                    case 5: aWant = 5; break;
                    case 12: aWant = 12; break;
                    case 16: aWant = 16; break;
                    case 7: aWant = 7; break;
                    }
                } while (0);
                CHECK(ee_won_lanlogic_client_action(connected, aSpec, hasClient, anAction) == aWant);
            }
    for (int c = 0; c < 4; c++)
        for (int aFlags = 0; aFlags < 64; aFlags++)
            for (int aCommand = 0; aCommand < 27; aCommand++) {
                bool connected = B(c, 0), hasTarget = B(c, 1);
                int anAction = 0; unsigned short aSent = 0; bool aWhisper = false; // HandleChatCommand
                do {
                    if (!connected) break;
                    if (!(aFlags & 8)) break;
                    switch (aCommand) {
                    case LobbyChatCommand_Broadcast: case LobbyChatCommand_Emote: {
                        unsigned short f = aCommand == LobbyChatCommand_Emote ? 2 : 0;
                        if (aFlags & 0x10) f |= 0x100;
                        anAction = 1; aSent = f;
                        break;
                    }
                    case LobbyChatCommand_Whisper:
                        if (hasTarget) { anAction = 1; aSent = 4; aWhisper = true; }
                        break;
                    case LobbyChatCommand_Ignore:
                        if (hasTarget) anAction = 2;
                        break;
                    default: anAction = 3; break;
                    }
                } while (0);
                EeWonLanChatCommand r = ee_won_lanlogic_chat_command(connected, aFlags, aCommand, hasTarget);
                CHECK(r.action == anAction);
                if (anAction == 1) CHECK(r.flags == aSent && r.whisper == aWhisper);
            }
    for (int c = 0; c < 4; c++) {
        bool aSound = false; // HandleChatMessage: a whisper (route 1) from another sender
        if (B(c, 0)) { if (!B(c, 1)) aSound = true; }
        CHECK(ee_won_lanlogic_whisper_received(B(c, 0), B(c, 1)) == aSound);
    }
    for (int aKind = -1; aKind < 6; aKind++) {
        int aWant = 0; // RefreshGames
        if (aKind == 1) aWant = 1;
        else if (aKind == 3) aWant = 3;
        CHECK(ee_won_lanlogic_refresh(aKind) == aWant);
    }
    int aResults[] = { 0, 1, 0x186a0, 0x186a1, 0x186a2, -1 };
    for (size_t r = 0; r < sizeof(aResults) / sizeof(aResults[0]); r++) {
        for (size_t aLength = 0; aLength < 3; aLength++) // ChangeLanName
            CHECK(ee_won_lanlogic_name_accepted(aResults[r], aLength) ==
                  (aResults[r] == 0x186a1 && aLength != 0));
        for (int aPort = 0; aPort < 3; aPort++)
            for (int aNewPort = 0; aNewPort < 3; aNewPort++) {
                bool aSaved = true; // ChangeLanPort
                if (aResults[r] != 0x186a1) aSaved = false;
                else if (aNewPort == aPort) aSaved = false;
                CHECK(ee_won_lanlogic_port_accepted(aResults[r], aPort, aNewPort) == aSaved);
            }
    }
    for (int c = 0; c < 2; c++) {
        int aDefaultPort = 6100; // HandleChangeDirectConnect
        if (c) aDefaultPort = 2300;
        CHECK(ee_won_lanlogic_direct_default_port(c != 0, 2300, 6100) == aDefaultPort);
        CHECK(ee_won_lanlogic_direct_event(c != 0) == (c ? 2 : 0));
        CHECK(ee_won_lanlogic_direct_name_is_address((size_t)c) == (c == 0));
    }
}

int main()
{
    TestRecvAndRoutes();
    TestSocketsAndDiscovery();
    TestJoining();
    TestCommands();
    printf("won_lanlogic_test: %d checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
