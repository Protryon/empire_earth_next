// The LAN lobby's messages (LanLogic.cpp): each one built the way the original LanLogic built it, with WriteBuffer
// (the statements copied from the C++ before the port), and by ee_won's lan module through its C ABI, must be the
// same bytes; and the Rust reads of the original's messages give back their fields, a short message nothing, a
// short join reply what the original had applied. Links WriteBuffer / ReadBuffer / ByteBuffer. Prints one line per
// failed check; the exit code is the number of failed checks.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONBuffer.h"
#include "rust/ee_won.h"

using namespace WONAPI;

static int sFailures;
#define CHECK(c) do { if (!(c)) { sFailures++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static void Append(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    static_cast<std::string*>(inContext)->append(reinterpret_cast<const char*>(inBytes), inLength);
}

static void AppendUnits(void* inContext, const uint16_t* inUnits, size_t inCount)
{
    std::wstring* anOut = static_cast<std::wstring*>(inContext);
    for (size_t i = 0; i < inCount; i++)
        WideAppendUtf16Unit(*anOut, inUnits[i]);
}

static std::string Bytes(WriteBuffer& inBuffer)
{
    ByteBufferPtr aBuffer = inBuffer.ToByteBuffer(true);
    return std::string(aBuffer->data(), aBuffer->length());
}

static std::string Utf16(const std::wstring& inText)
{
    std::string aBytes;
    WideAppendUtf16LE(aBytes, inText);
    return aBytes;
}

static const uint8_t* U8(const std::string& inText)
{
    return reinterpret_cast<const uint8_t*>(inText.data());
}

static bool Same(const char* inWhat, const std::string& inGot, const std::string& inWant)
{
    if (inGot == inWant)
        return true;
    printf("  %s\n  got ", inWhat);
    for (size_t i = 0; i < inGot.size(); i++) printf(" %02x", (unsigned char)inGot[i]);
    printf("\n  want");
    for (size_t i = 0; i < inWant.size(); i++) printf(" %02x", (unsigned char)inWant[i]);
    printf("\n");
    return false;
}

struct Texts {
    std::wstring mFirst, mSecond;
    static void First(void* c, const uint16_t* u, size_t n) { AppendUnits(&static_cast<Texts*>(c)->mFirst, u, n); }
    static void Second(void* c, const uint16_t* u, size_t n) { AppendUnits(&static_cast<Texts*>(c)->mSecond, u, n); }
};

static EeWonLanMsg Read(const std::string& inBytes, size_t inSkip, Texts& outTexts)
{
    EeWonLanMsg aMsg;
    ee_won_lan_read_game(U8(inBytes), inBytes.length(), inSkip, &aMsg, Texts::First, Texts::Second, &outTexts);
    return aMsg;
}

struct Players {
    std::vector<std::pair<unsigned short, std::wstring> > mList;
    static void Add(void* c, uint16_t inId, const uint16_t* inName, size_t inCount)
    {
        Players* p = static_cast<Players*>(c);
        p->mList.push_back(std::make_pair((unsigned short)inId, std::wstring()));
        AppendUnits(&p->mList.back().second, inName, inCount);
    }
};

int main()
{
    const std::wstring aName = L"Joiner \xe9\x20ac";
    const std::wstring aPassword = L"pw";
    const std::wstring aText = L"hello \U0001F600";

    // join request (JoinGame)
    WriteBuffer request(2, true);
    request.AppendByte(1);
    request.AppendWString(aName, 2);
    request.AppendWString(aPassword, 2);
    std::string aWant = Bytes(request), aGot;
    std::string aNameBytes = Utf16(aName), aPasswordBytes = Utf16(aPassword), aTextBytes = Utf16(aText);
    ee_won_lan_join_request(U8(aNameBytes), aNameBytes.length(), U8(aPasswordBytes), aPasswordBytes.length(), Append,
                            &aGot);
    CHECK(Same("join request", aGot, aWant));
    {
        Texts t;
        // the host reads RecvMsgOp's message, without the prefix
        EeWonLanMsg m = Read(aWant.substr(2), 0, t);
        CHECK(m.kind == 1 && t.mFirst == aName && t.mSecond == aPassword);
        Texts u;
        CHECK(Read(aWant.substr(2, aWant.length() - 3), 0, u).kind == 0);
    }

    // refused joins (HandleJoinRequest)
    for (short aStatus = 2; aStatus <= 3; aStatus++) {
        WriteBuffer aReply(2, true);
        aReply.AppendByte(2);
        aReply.AppendShort(aStatus);
        aWant = Bytes(aReply);
        aGot.clear();
        ee_won_lan_join_refused(aStatus, Append, &aGot);
        CHECK(Same("join refused", aGot, aWant));
    }

    // the accepted join's reply: the captain (1), the new player (7), the players in the client map's order
    {
        unsigned short anIds[3] = {1, 3, 7};
        std::wstring aNames[3] = {L"host", L"", aName};
        WriteBuffer aReply(2, true);
        aReply.AppendByte(2);
        aReply.AppendShort(0);
        aReply.AppendShort(1);
        aReply.AppendShort(7);
        aReply.AppendShort(3);
        std::vector<size_t> anOffsets(1, 0);
        std::string aAll;
        for (int i = 0; i < 3; i++) {
            aReply.AppendShort(anIds[i]);
            aReply.AppendWString(aNames[i], 2);
            aAll += Utf16(aNames[i]);
            anOffsets.push_back(aAll.length());
        }
        aWant = Bytes(aReply);
        aGot.clear();
        uint16_t anIds16[3] = {1, 3, 7};
        ee_won_lan_join_accepted(1, 7, 3, anIds16, U8(aAll), &anOffsets[0], Append, &aGot);
        CHECK(Same("join accepted", aGot, aWant));
        // the joiner reads it without the prefix (RecvMsgOp's message)
        std::string aBody = aWant.substr(2);
        Players p;
        bool aHave = false;
        uint16_t aCaptain = 0, aYou = 0;
        int aStatus = ee_won_lan_read_join_reply(U8(aBody), aBody.length(), &aHave, &aCaptain, &aYou, Players::Add, &p);
        CHECK(aStatus == 0 && aHave && aCaptain == 1 && aYou == 7 && p.mList.size() == 3);
        CHECK(p.mList.size() == 3 && p.mList[2].first == 7 && p.mList[2].second == aName && p.mList[1].second.empty());
        // short inside the last name: the captain and the first two players were applied, then bad data
        Players q;
        aStatus = ee_won_lan_read_join_reply(U8(aBody), aBody.length() - 1, &aHave, &aCaptain, &aYou, Players::Add, &q);
        CHECK(aStatus == 1 && aHave && aCaptain == 1 && q.mList.size() == 2);
        Players r;
        aStatus = ee_won_lan_read_join_reply(U8(aBody), 6, &aHave, &aCaptain, &aYou, Players::Add, &r);
        CHECK(aStatus == 1 && aHave == false && r.mList.empty());
    }

    // client joined / left (HandleJoinRequest, HandleSocketClosed)
    {
        WriteBuffer aNotify(2, true);
        aNotify.AppendByte(3);
        aNotify.AppendShort(7);
        aNotify.AppendWString(aName, 2);
        aWant = Bytes(aNotify);
        aGot.clear();
        ee_won_lan_client_joined(7, U8(aNameBytes), aNameBytes.length(), Append, &aGot);
        CHECK(Same("client joined", aGot, aWant));
        Texts t;
        EeWonLanMsg m = Read(aWant, 2, t);   // the host delivers it to itself with the prefix
        CHECK(m.kind == 3 && m.id == 7 && t.mFirst == aName);

        WriteBuffer aLeft(2, true);
        aLeft.AppendByte(4);
        aLeft.AppendShort(0xfffe);
        aWant = Bytes(aLeft);
        aGot.clear();
        ee_won_lan_client_left(0xfffe, Append, &aGot);
        CHECK(Same("client left", aGot, aWant));
        Texts u;
        m = Read(aWant.substr(2), 0, u);
        CHECK(m.kind == 4 && m.id == 0xfffe);
    }

    // chat request and line (SendChat, HandleChatRequest)
    {
        WriteBuffer aBuffer(2, true);
        aBuffer.AppendByte(5);
        aBuffer.AppendShort(0x104);
        aBuffer.AppendWString(aText, 2);
        aBuffer.AppendShort(9);
        aWant = Bytes(aBuffer);
        aGot.clear();
        ee_won_lan_chat_request(0x104, U8(aTextBytes), aTextBytes.length(), 9, Append, &aGot);
        CHECK(Same("chat request", aGot, aWant));
        Texts t;
        EeWonLanMsg m = Read(aWant.substr(2), 0, t);
        CHECK(m.kind == 5 && m.flags == 0x104 && m.target == 9 && t.mFirst == aText);

        WriteBuffer aLine(2, true);
        aLine.AppendByte(6);
        aLine.AppendShort(0x104);
        aLine.AppendShort(3);
        aLine.AppendWString(aText, 2);
        aLine.AppendShort(9);
        aWant = Bytes(aLine);
        aGot.clear();
        ee_won_lan_chat_line(0x104, 3, U8(aTextBytes), aTextBytes.length(), 9, Append, &aGot);
        CHECK(Same("chat line", aGot, aWant));
        Texts u;
        m = Read(aWant.substr(2), 0, u);
        CHECK(m.kind == 6 && m.flags == 0x104 && m.id == 3 && m.target == 9 && u.mFirst == aText);
    }

    // ping, staging (HandleNetworkTimer, SendToCaptain)
    {
        WriteBuffer aPing(2, true);
        aPing.AppendByte(7);
        aWant = Bytes(aPing);
        aGot.clear();
        ee_won_lan_ping(Append, &aGot);
        CHECK(Same("ping", aGot, aWant));

        ByteBufferPtr aStaging = new ByteBuffer("staging", 7, false);
        WriteBuffer aMessage(2, true);
        aMessage.AppendByte(8);
        aMessage.AppendBuffer(aStaging.get(), 0);
        aWant = Bytes(aMessage);
        aGot.clear();
        ee_won_lan_staging(reinterpret_cast<const uint8_t*>(aStaging->data()), aStaging->length(), Append, &aGot);
        CHECK(Same("staging", aGot, aWant));
        WriteBuffer anEmpty(2, true);
        anEmpty.AppendByte(8);
        anEmpty.AppendBuffer(NULL, 0);
        aWant = Bytes(anEmpty);
        aGot.clear();
        ee_won_lan_staging(NULL, 0, Append, &aGot);
        CHECK(Same("empty staging", aGot, aWant));
        Texts t;
        CHECK(Read(aWant.substr(2), 0, t).kind == 8);
    }

    // discovery packets (RefreshGames, BroadcastGameSummary / KeepAlive / Removed)
    for (int aKind = 1; aKind <= 4; aKind++) {
        WriteBuffer aBuffer(0, true);
        aBuffer.AppendByte((unsigned char)aKind);
        if (aKind != 4)
            aBuffer.AppendShort(33334);
        aWant = Bytes(aBuffer);
        aGot.clear();
        ee_won_lan_discovery((uint8_t)aKind, 33334, Append, &aGot);
        CHECK(Same("discovery", aGot, aWant));
        uint8_t k = 0;
        uint16_t aPort = 0;
        CHECK(ee_won_lan_read_discovery(U8(aWant), aWant.length(), &k, &aPort) && k == aKind &&
              aPort == (aKind == 4 ? 0 : 33334));
        CHECK(aKind == 4 || !ee_won_lan_read_discovery(U8(aWant), 2, &k, &aPort));
    }

    // the host's decisions
    CHECK(ee_won_lan_join_status(1, 10, 0, 10, 1, true) == 3);
    CHECK(ee_won_lan_join_status(3, 10, 0, 10, 1, true) == 0);
    CHECK(ee_won_lan_join_status(1, 10, 0, 10, 0, false) == 2);
    CHECK(ee_won_lan_chat_route(4) == 1 && ee_won_lan_chat_route(0x100) == 2 && ee_won_lan_chat_route(0) == 0);
    CHECK(ee_won_lan_chat_line_flags(0x105) == 4 && ee_won_lan_chat_line_flags(0x100) == 0x100);
    unsigned short aNext = 0;
    CHECK(ee_won_lan_next_client_id(&aNext) == 1 && aNext == 2);

    printf("won_lan_test: %d failures\n", sFailures);
    return sFailures;
}
