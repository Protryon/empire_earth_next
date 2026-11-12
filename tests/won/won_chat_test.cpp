// The lobby's chat command parser (ChatCommandParser: slash commands, emotes, broadcasts, name matching, durations,
// the help text): every input of a table through HandleInput under several settings, one line per case, compared
// with rust/crates/ee_won/src/golden_chat.txt (recorded over the C++ parser before its port; the Rust test
// `chat_golden` replays the same table). `--print` prints the lines instead. The exit code is the number of failures.
// The command names are the lobby's own (_LobbyResource.cfg's ChatCommandLogic resource); this driver stands in for
// the resource table, whose globals it defines.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyParser.h"
#include "ee/wonapi/WONLobbyChatCommands.h"

using namespace WONAPI;

#define CHAT_STRINGS(X) \
    X(Whisper, "w, whisper, t, tell") X(Block, "block") X(Emote, "e, em, me, emote") X(Ignore, "i, ignore") \
    X(Away, "away, afk") X(Invite, "invite") X(Uninvite, "uninvite") X(Reply, "r, reply") \
    X(Clear, "clr, cls, clear") X(Help, "?, h, help") X(ShowTeam, "team") X(Moderator, "mod") \
    X(ServerMute, "smute") X(ServerBan, "sban") X(Mute, "mute") X(Ban, "ban") X(Unmute, "unmute") \
    X(ServerUnmute, "sunmute") X(Unban, "unban") X(ServerUnban, "sunban") X(Alert, "") X(Warn, "warn") \
    X(HelpHeader, "Header") X(HelpHelp, "HelpHelp") X(ShowTeamHelp, "ShowTeamHelp") X(WhisperHelp, "WhisperHelp") \
    X(BlockHelp, "BlockHelp") X(IgnoreHelp, "IgnoreHelp") X(ReplyHelp, "ReplyHelp") X(EmoteHelp, "EmoteHelp") \
    X(AwayHelp, "AwayHelp") X(ClearHelp, "ClearHelp") X(InviteHelp, "InviteHelp") X(UninviteHelp, "UninviteHelp") \
    X(MuteHelp, "MuteHelp") X(UnmuteHelp, "UnmuteHelp") X(BanHelp, "BanHelp") X(UnbanHelp, "UnbanHelp") \
    X(ModeratorHelp, "ModeratorHelp") X(ServerMuteHelp, "ServerMuteHelp") X(ServerUnmuteHelp, "ServerUnmuteHelp") \
    X(ServerBanHelp, "ServerBanHelp") X(ServerUnbanHelp, "ServerUnbanHelp") X(AlertHelp, "AlertHelp") \
    X(WarnHelp, "WarnHelp")

#define DEFINE_STRING(name, text) GUIString ChatCommandLogic_##name##_String;
CHAT_STRINGS(DEFINE_STRING)
#define SET_STRING(name, text) ChatCommandLogic_##name##_String = GUIString(text);

wchar_t gEmptyWideString[2];

// LobbyModel.cpp's client and client list, the parts the parser uses (copied: that unit links the whole lobby; the
// list's clients are the Rust container since round 17)
namespace WONAPI {
LobbyClient::LobbyClient() : mName(GetEmptyGUIStringData())
{
    this->mClientId = 0xffff;
    this->mClientFlags = 0;
    this->mIsGameCaptain = false;
    this->mIsChatCaptain = false;
}
LobbyClient::~LobbyClient() {}
bool LobbyClient::IsAdmin() { return (this->mClientFlags & 0x20) ? true : false; }
bool LobbyClient::IsModerator() { return (this->mClientFlags & 1) ? true : false; }
bool LobbyClient::IsCaptain(bool inGame) { return inGame ? this->mIsGameCaptain : this->mIsChatCaptain; }
LobbyClientList::LobbyClientList() : mClients(EeWonLobbyKeyKind::Unsigned) {}
LobbyClientList::~LobbyClientList() {}
void LobbyClientList::AddClient(LobbyClient* inClient) { this->mClients.Set(inClient->mClientId, inClient); }
LobbyClient* LobbyClientList::FindClient(unsigned short inId) { return this->mClients.Get(inId); }
}

static std::vector<std::string> sLines;

static std::string Units(const GUIString& inText)
{
    std::string anOut;
    char aBuffer[12];
    for (int i = 0; i < inText.length(); i++) {
        unsigned c = (unsigned)inText.at(i);
        if (c > 0x20 && c < 0x7f && c != '%' && c != '|') {
            anOut += (char)c;
        } else {
            snprintf(aBuffer, sizeof(aBuffer), "%%%x;", c);
            anOut += aBuffer;
        }
    }
    return anOut.empty() ? "-" : anOut;
}

// Shared with chat.rs: the room's clients (id, name, flags: 1 moderator, 0x20 admin; game and chat captain)
struct ClientSpec {
    unsigned short mId;
    const wchar_t* mName;
    unsigned long mFlags;
    bool mGameCaptain, mChatCaptain;
};
static const ClientSpec kClients[] = {
    {1, L"Alice", 0, false, false},     {2, L"alice2", 0, false, false}, {3, L"Bob", 1, true, false},
    {4, L"Bobby", 0x20, false, true},  {5, L"Carl Jr", 0, false, false}, {6, L"\x00e9mile", 0x21, true, true},
    {7, L"\x263a", 0, false, false},   {8, L"Dave", 0, false, false},    {9, L"dave", 0, false, false},
};

// Shared with chat.rs: the inputs
static const wchar_t* const kInputs[] = {
    L"", L"hello", L"   ", L" x", L"/", L"/ ", L"/nosuch", L"/W Alice hi there", L"/w alice", L"/w alice ",
    L"/w al hi", L"/w ali hi", L"/w Bob hi", L"/w bo hi", L"/w \"Carl Jr\" yo", L"/w \"Carl Jr yo", L"/w Carl",
    L"/tell \x00e9mile salut", L"/w \x00c9MILE x", L"/w \x263a ok", L"/w dave x", L"/w DAVE x", L"/w Dav x",
    L"/whisper", L"/e waves", L"/me", L"/em  ", L"/r", L"/r hey you", L"/reply   ", L"/i", L"/i Bob",
    L"/i \"Bob\"", L"/ignore Bo", L"/i B", L"/block Bobby", L"/block", L"/block x", L"/away", L"/afk now",
    L"/invite Bob", L"/invite Bob come", L"/uninvite Bob", L"/cls", L"/?", L"/h extra", L"/team",
    L"/mod", L"/smute Bob", L"/smute Bob 10", L"/smute Bob 10m", L"/smute Bob 2h being rude", L"/sban Bob 1d",
    L"/sban Bob 3s", L"/sban Bob x", L"/sban Bob 0s", L"/sban Bob INFINITE reason", L"/sban Bob infinite",
    L"/mute Bob 5M", L"/ban Bob -1h", L"/ban Bob 99999999d", L"/ban Bob 10\x00e9", L"/unmute", L"/unmute Bob",
    L"/unmute Bob  later  ", L"/sunmute Bob", L"/unban Bob", L"/sunban Bob", L"/alert everyone", L"/warn",
    L"/warn Bob", L"/warn Bob stop", L"/startshutdown 5m", L"/abortshutdown", L":smile", L": ", L":",
    L"/W  \"Alice\"   two  spaces  ", L"/w \"\" x", L"/w \"Alice\"x", L"\t/w Alice x", L"/\tw Alice x",
    L"/w\tAlice x", L"/i \"x", L"/i ab", L"/i a ", L"/r \"quoted\"", L"/unmute \"Bob\" \"q\"", L"/unmute B",
};

static void Run(const char* inPass, int inThreshold, bool inColon, wchar_t inPrefix, bool inClients)
{
    ChatCommandParser aParser;
    aParser.InitCommands();
    aParser.mMatchThreshold = inThreshold;
    aParser.mAllowColonEmote = inColon;
    aParser.mPrefix = inPrefix;
    aParser.mInput = GUIString(L"Bob");
    aParser.mCommand = -1; // the constructor leaves it unset
    LobbyClientList* aList = new LobbyClientList;
    SmartPtr<LobbyClientList> aHold = aList;
    for (size_t i = 0; i < sizeof(kClients) / sizeof(kClients[0]); i++) {
        LobbyClient* aClient = new LobbyClient;
        aClient->mClientId = kClients[i].mId;
        aClient->mName = GUIString(kClients[i].mName);
        aClient->mClientFlags = kClients[i].mFlags;
        aClient->mIsGameCaptain = kClients[i].mGameCaptain;
        aClient->mIsChatCaptain = kClients[i].mChatCaptain;
        aList->AddClient(aClient);
    }
    if (inClients)
        aParser.SetClientList(aList, 0);
    for (size_t i = 0; i < sizeof(kInputs) / sizeof(kInputs[0]); i++) {
        // what a parse leaves alone shows the previous case's value: reset what ParseCommand may not set
        aParser.mText = GUIString(L"~");
        aParser.mInvitation = false;
        bool r = aParser.HandleInput(GUIString(kInputs[i]));
        LobbyClient* aTarget = aParser.GetTarget();
        char aLine[4096];
        snprintf(aLine, sizeof(aLine), "%s i%02d r=%d err=%d cmd=%d name=%s target=%d text=%s dur=%u inv=%d", inPass,
                 (int)i, r ? 1 : 0, aParser.mError, aParser.mCommand, Units(aParser.mTargetName).c_str(),
                 aTarget != NULL ? (int)aTarget->mClientId : -1, Units(aParser.mText).c_str(),
                 (unsigned)aParser.mDuration, aParser.mInvitation ? 1 : 0);
        sLines.push_back(aLine);
    }
}

static void RunTables()
{
    ChatCommandParser aParser;
    aParser.InitCommands();
    for (int id = 0; id <= 26; id++) {
        char aLine[512];
        snprintf(aLine, sizeof(aLine), "command %d name=%s string=%s", id, aParser.GetCommandName(id),
                 Units(aParser.GetCommandString(id)).c_str());
        sLines.push_back(aLine);
    }
    LobbyClientList* aList = new LobbyClientList;
    SmartPtr<LobbyClientList> aHold = aList;
    for (size_t i = 0; i < sizeof(kClients) / sizeof(kClients[0]); i++) {
        LobbyClient* aClient = new LobbyClient;
        aClient->mClientId = kClients[i].mId;
        aClient->mName = GUIString(kClients[i].mName);
        aClient->mClientFlags = kClients[i].mFlags;
        aClient->mIsGameCaptain = kClients[i].mGameCaptain;
        aClient->mIsChatCaptain = kClients[i].mChatCaptain;
        aList->AddClient(aClient);
    }
    static const int kRoomFlags[] = {0, 2, 8, 10, 0x0f};
    for (size_t f = 0; f < sizeof(kRoomFlags) / sizeof(kRoomFlags[0]); f++) {
        aParser.SetClientList(aList, kRoomFlags[f]);
        for (int id = 0; id <= 10; id++) {
            char aLine[4096];
            snprintf(aLine, sizeof(aLine), "help flags=%d client=%d %s", kRoomFlags[f], id,
                     Units(aParser.GetHelp((unsigned short)id)).c_str());
            sLines.push_back(aLine);
        }
    }
    ChatCommandParser aNoList;
    aNoList.InitCommands();
    sLines.push_back("help nolist " + Units(aNoList.GetHelp(1)));
}

static std::string GoldenPath()
{
    std::string aPath = __FILE__;
    size_t aSlash = aPath.find_last_of("/\\");
    aPath.resize(aSlash == std::string::npos ? 0 : aSlash + 1);
    return aPath + "../../rust/crates/ee_won/src/golden_chat.txt";
}

int main(int argc, char** argv)
{
    CHAT_STRINGS(SET_STRING)
    Run("t2", 2, false, '/', true);
    Run("t3", 3, true, '/', true);
    Run("t1", 1, false, '!', true);
    Run("none", 2, false, '/', false);
    RunTables();
    if (argc > 1 && strcmp(argv[1], "--print") == 0) {
        for (size_t i = 0; i < sLines.size(); i++)
            printf("%s\n", sLines[i].c_str());
        return 0;
    }
    FILE* aFile = fopen(GoldenPath().c_str(), "rb");
    if (aFile == NULL) {
        printf("FAIL: cannot open %s\n", GoldenPath().c_str());
        return 1;
    }
    std::vector<std::string> aWant;
    static char aBuffer[65536];
    while (fgets(aBuffer, sizeof(aBuffer), aFile)) {
        size_t n = strlen(aBuffer);
        while (n && (aBuffer[n - 1] == '\n' || aBuffer[n - 1] == '\r'))
            aBuffer[--n] = 0;
        aWant.push_back(aBuffer);
    }
    fclose(aFile);
    int aFailures = 0;
    size_t aCount = aWant.size() > sLines.size() ? aWant.size() : sLines.size();
    for (size_t i = 0; i < aCount; i++) {
        const char* aGot = i < sLines.size() ? sLines[i].c_str() : "(none)";
        const char* anExpect = i < aWant.size() ? aWant[i].c_str() : "(none)";
        if (strcmp(aGot, anExpect) != 0) {
            if (aFailures++ < 20)
                printf("FAIL line %d\n  got  %s\n  want %s\n", (int)i + 1, aGot, anExpect);
        }
    }
    printf("won_chat_test: %d lines, %d failures\n", (int)sLines.size(), aFailures);
    return aFailures;
}
