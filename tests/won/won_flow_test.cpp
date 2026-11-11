// The lobby's game room flow and start-up checks (StagingLogic.cpp, InitLogic.cpp): their decisions, the
// original's statements (copied from the C++ before the port, the objects replaced by the state they answered)
// against ee_won's flow module through its C ABI, over every combination of their inputs: StagingLogic's
// SendToCaptain, BroadcastToGame, SendToClient, HandlePlayerJoined, SetGameInProgress, ToggleReady, HostGame's
// warnings, VerifyPingBeforeJoin, JoinGame's status message, LeaveGame's confirmation, HandleKicked,
// HandleGameAction, NotifyPingChange, KickClient and HandleClientLeft; InitLogic's SetError, Start's Internet check,
// DirectoryCompletion's targets, answer and missing lists, VersionCompletion, AcceptCompletion, HandleTOU,
// HTTPDocumentCompletion's length rule, HandleComponentEvent, and ParseCrossPromotion's strtok parse over 20,000
// generated advert documents. Prints one line per failed check; the exit code is the number of failed checks.
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include "rust/ee_rust.h"

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static bool B(int inBits, int inBit) { return ((inBits >> inBit) & 1) != 0; }

static unsigned sSeed = 777;
static unsigned Rand() { sSeed = sSeed * 1103515245u + 12345u; return (sSeed >> 16) & 0x7fff; }

static void TestStaging()
{
    for (int c = 0; c < 4; c++) {
        bool game = B(c, 0), captain = B(c, 1);
        int aWant = 0; // SendToCaptain
        if (game) { if (captain) aWant = 1; else aWant = 2; }
        CHECK((int)ee_won_stage_send_to_captain(game, captain) == aWant);
        aWant = 0; // BroadcastToGame
        if (game && captain) aWant = 3;
        CHECK((int)ee_won_stage_broadcast(game, captain) == aWant);
        aWant = 0; // SendToClient
        if (game && captain) aWant = 2;
        CHECK((int)ee_won_stage_send_to_client(game, captain) == aWant);
        // HandlePlayerJoined
        bool hasPlayer = B(c, 0), iAmCaptain = B(c, 1);
        int aStep;
        if (!hasPlayer) aStep = 0;
        else if (!iAmCaptain) aStep = 2;
        else aStep = 1;
        CHECK(ee_won_stage_player_joined(hasPlayer, iAmCaptain) == aStep);
        bool aLast = B(c, 0), aReady = B(c, 1);
        if (aLast && !aReady) aLast = false;
        CHECK(ee_won_stage_joined_keeps_all_ready(B(c, 0), aReady) == aLast);
    }
    for (int c = 0; c < 16; c++) {
        bool game = B(c, 0), cur = B(c, 1), want = B(c, 2), captain = B(c, 3);
        bool aSets = true; // SetGameInProgress
        if (!game || cur == want || !captain) aSets = false;
        CHECK(ee_won_stage_sets_in_progress(game, cur, want, captain) == aSets);
        // ToggleReady
        bool client = B(c, 0), g = B(c, 1), cap = B(c, 2), ready = B(c, 3);
        int anAction = 0; bool aWantReady = false, anAsk = false;
        do {
            if (!client || !g) break;
            if (cap) { anAction = 1; break; }
            bool aReady = !ready;
            anAction = 2; aWantReady = aReady; anAsk = aReady;
        } while (0);
        EeWonToggleReady t = ee_won_stage_toggle_ready(client, g, cap, ready);
        CHECK(t.action == anAction);
        if (anAction == 2) CHECK(t.want_ready == aWantReady && t.ask_can_ready == anAsk);
    }
    for (int aType = 0; aType < 5; aType++)
        for (int c = 0; c < 8; c++) {
            bool show = B(c, 0), done = B(c, 1), behind = B(c, 2);
            int aWant = 0; // HostGame
            do {
                if (aType == 0) { aWant = -1; break; }
                if (aType != 1 && show) {
                    if (!done) aWant = 1;
                    else if (behind) aWant = 2;
                }
            } while (0);
            CHECK(ee_won_stage_host_warning(aType, show, done, behind) == aWant);
        }
    for (int aType = 0; aType < 5; aType++)
        for (int c = 0; c < 16; c++) {
            bool replied = B(c, 0), config = B(c, 1), allow = B(c, 2), show = B(c, 3);
            int aWant = 0; // VerifyPingBeforeJoin
            do {
                if (aType != 2 || replied) break;
                if (config && !allow) aWant = 1;
                else if (show) aWant = 2;
            } while (0);
            CHECK(ee_won_stage_verify_ping(aType, replied, config, allow, show) == aWant);
        }
    for (int s = -32768; s < 32768; s++) {
        short aStatus = (short)s; // JoinGame
        bool aShown = false;
        if (aStatus != 0) { if (aStatus != 1) aShown = true; }
        CHECK(ee_won_stage_join_status_shown(aStatus) == aShown);
    }
    for (int c = 0; c < 8; c++) {
        bool confirm = B(c, 0), show = B(c, 1), captain = B(c, 2);
        int aWant = 0; // LeaveGame
        if (confirm && show) { if (captain) aWant = 1; else aWant = 2; }
        CHECK(ee_won_stage_leave_confirm(confirm, show, captain) == aWant);
    }
    for (int aType = 0; aType < 5; aType++)
        for (int c = 0; c < 4; c++) {
            bool inBan = B(c, 0), isMe = B(c, 1);
            int aKind = !inBan; // HandleKicked
            int aFlags = aType == 2 ? 6 : 10;
            int aMessage = 0;
            if (isMe) { if (inBan) aMessage = 1; else aMessage = 2; }
            EeWonKicked k = ee_won_stage_kicked(inBan, aType, isMe);
            CHECK(k.kind == aKind && k.flags == aFlags && k.message == aMessage);
        }
    for (int aType = 0; aType < 5; aType++)
        for (int anAction = -1; anAction < 10; anAction++) {
            int aWant = 0; // HandleGameAction
            switch (anAction) {
            case 7: aWant = 7; break;
            case 1: aWant = 1; break;
            case 2: aWant = 2; break;
            case 3: case 4: aWant = 3; break;
            case 5: aWant = 5; break;
            case 6: if (aType == 3) aWant = 6; break;
            }
            CHECK(ee_won_stage_game_action(anAction, aType) == aWant);
        }
    for (int c = 0; c < 8; c++) {
        bool game = B(c, 0), captain = B(c, 1), same = B(c, 2);
        bool aReports = true; // NotifyPingChange
        if (!game || captain || !same) aReports = false;
        CHECK(ee_won_stage_reports_ping(game, captain, same) == aReports);
        bool aKicks = true; // KickClient
        if (!B(c, 0) || !B(c, 1) || !B(c, 2)) aKicks = false;
        CHECK(ee_won_stage_kicks(B(c, 0), B(c, 1), B(c, 2)) == aKicks);
        bool aLeft = true; // HandleClientLeft
        if (!B(c, 0) || !B(c, 1)) aLeft = false;
        CHECK(ee_won_stage_client_left(B(c, 0), B(c, 1)) == aLeft);
    }
}

// ParseCrossPromotion's parse (the original's statements; the result kept as strings)
static void OriginalCrossPromotion(const std::string& inDocument, std::string& outLink, std::string& outImage)
{
    std::vector<char> storage(inDocument.begin(), inDocument.end());
    storage.push_back(0);
    char* token = &storage[0];
    char delimiters[] = " \t\n=";
    std::string imageURL("");
    outLink = "";
    token = strtok(token, delimiters);
    while (token != NULL) {
        if (_stricmp(token, "link") == 0) {
            token = strtok(NULL, "\n");
            if (token == NULL)
                break;
            outLink = token;
        } else if (_stricmp(token, "image") == 0) {
            token = strtok(NULL, "\n");
            if (token == NULL)
                break;
            imageURL = token;
        }
        if (token == NULL)
            break;
        token = strtok(NULL, delimiters);
    }
    outImage = imageURL;
}

static void CheckCrossPromotion(const std::string& inDocument)
{
    std::string aLink, anImage;
    OriginalCrossPromotion(inDocument, aLink, anImage);
    struct Sinks {
        std::string mLink, mImage;
        int mCalls;
        static void Link(void* c, const uint8_t* b, size_t n)
        { Sinks* s = static_cast<Sinks*>(c); s->mLink.append(reinterpret_cast<const char*>(b), n); s->mCalls++; }
        static void Image(void* c, const uint8_t* b, size_t n)
        { Sinks* s = static_cast<Sinks*>(c); s->mImage.append(reinterpret_cast<const char*>(b), n); s->mCalls++; }
    } aGot;
    aGot.mCalls = 0;
    ee_won_init_cross_promotion(reinterpret_cast<const uint8_t*>(inDocument.data()), inDocument.length(), Sinks::Link,
                                Sinks::Image, &aGot);
    CHECK(aGot.mLink == aLink && aGot.mImage == anImage);
    if (aGot.mLink != aLink || aGot.mImage != anImage)
        printf("  doc [%s] want [%s] [%s] got [%s] [%s]\n", inDocument.c_str(), aLink.c_str(), anImage.c_str(),
               aGot.mLink.c_str(), aGot.mImage.c_str());
}

static void TestInit()
{
    for (int e = 0; e < 0x80; e++)
        for (int i = 0; i < 0x80; i += 3) {
            int anErrors = e & ~i; // SetError
            bool anAllow = (anErrors & 5) == anErrors;
            EeWonInitErrors r = ee_won_init_errors(e, i);
            CHECK(r.errors == anErrors);
            if (anErrors != 0) CHECK(r.allow_ignore == anAllow);
        }
    for (int c = 0; c < 4; c++) CHECK(ee_won_init_no_internet(B(c, 0), B(c, 1)) == (B(c, 0) && !B(c, 1))); // Start
    for (int anIndex = -1; anIndex < 6; anIndex++)
        for (int c = 0; c < 2; c++) {
            bool empty = c == 0;
            int aWant = 0; // DirectoryCompletion's switch (1 auth, 2 account, 3 firewall, 4 versions)
            switch (anIndex) {
            case 0: if (empty) aWant = 1; break;
            case 1: aWant = 2; break;
            case 2: aWant = 3; break;
            case 3: aWant = 4; break;
            }
            CHECK(ee_won_init_dir_request_target(anIndex, !empty) == aWant);
        }
    int aStatuses[] = { 0, -1223, -1, 1, -2816, -2815, -2814, -2800, -2801, -2813, 12 };
    for (size_t k = 0; k < sizeof(aStatuses) / sizeof(aStatuses[0]); k++) {
        int status = aStatuses[k];
        CHECK(ee_won_init_dir_answered(status) == (status == 0 || status == -1223));
        int aWant; // VersionCompletion
        if (status != 0 && status != -2816 && status != -2814) {
            if (status == -2815 || status == -2800) aWant = 1;
            else aWant = 2;
        } else aWant = 0;
        CHECK(ee_won_init_version_outcome(status) == aWant);
    }
    for (int a = 0; a < 2; a++)
        for (int b = 0; b < 2; b++)
            for (int v = 0; v < 2; v++) {
                int errors = 0; // DirectoryCompletion's missing lists
                if (a == 0) errors = 8;
                if (b == 0) errors |= 4;
                if (v == 0) errors |= 0x10;
                CHECK(ee_won_init_missing_servers(a, b, v) == errors);
            }
    for (int c = 0; c < 4; c++) {
        int aWant; // AcceptCompletion
        if (!B(c, 0)) aWant = 0;
        else if (B(c, 1)) aWant = 1;
        else aWant = 2;
        CHECK(ee_won_init_accept_outcome(B(c, 0), B(c, 1)) == aWant);
    }
    for (int s = -1; s < 3; s++)
        for (int m = -2; m < 3; m++)
            for (int t = -2; t < 3; t++)
                CHECK(ee_won_init_tou_saved(s, m, t) == (s == 0 && m > t)); // HandleTOU
    for (size_t n = 0; n < 5; n++) {
        size_t aBytes = 0; // HandleTOU's write, HTTPDocumentCompletion's advert
        if (n > 1) aBytes = n - 1;
        CHECK(ee_won_init_document_bytes(n) == aBytes);
    }
    for (int t = 100020; t < 100050; t++) {
        int aWant = 0; // HandleComponentEvent
        if (t != 100045) {
            if (t == 100025) aWant = 2;
        } else aWant = 1;
        CHECK(ee_won_init_event(t) == aWant);
    }
    // the advert documents: words, keywords in any case, separators, empty lines, NULs, ends
    const char* aPieces[] = { "link", "LINK", "Link", "image", "IMAGE", "imagex", "lin", " ", "\t", "\n", "=", "\n\n",
                              "http://a.b/c", "pic.png", "x y", "\0", "a=b", "  =  ", "\r", "\xc3\xa9" };
    size_t aCount = sizeof(aPieces) / sizeof(aPieces[0]);
    CheckCrossPromotion("");
    CheckCrossPromotion("link http://a/b\nimage=pic.png\n");
    for (int n = 0; n < 20000; n++) {
        std::string aDocument;
        int aLength = Rand() % 12;
        for (int k = 0; k < aLength; k++) {
            size_t p = Rand() % aCount;
            if (p == 15) aDocument.push_back('\0');
            else aDocument += aPieces[p];
        }
        CheckCrossPromotion(aDocument);
    }
}

int main()
{
    TestStaging();
    TestInit();
    printf("won_flow_test: %d checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
