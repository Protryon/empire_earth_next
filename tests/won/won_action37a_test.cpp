// EEAWonNet (the WON front end's UI-thread action; Internet only: EEWonNet posts it from the routing server's
// callbacks) whole in Rust, R6 WON lobby and network round 37 (ee_won_lobby wonaction.rs), against the class as it
// was (won_action37a_orig.h: the three constructors and Virtual4 with its statics).
//
// Two worlds made alike (the UI manager's forms, the lobby's chat label, the game room's player list, chat label
// and games box, the room list's box and two labels, the resource file) hold real state: a list's lines, first
// line shown and selection, a label's text and scroll bar. Each sequence runs random actions (types 2, 3, 4, 6, 7,
// 8 and an unknown one; the refill brackets "@@@@" and "@@@"; names that repeat) on both sides, the original's
// Virtual4 on one world and the Rust's over the game's host (ee/EEAWonNetHost.inl, the very statements, over the
// stand-ins) on the other, with the same random user input between actions (the form changes, a scroll, a
// selection). After every action the calls' traces must be equal; after every sequence the worlds' state and
// Virtual4's statics. Then every host op is skipped in turn (it answers 0 and does nothing): each skip must make
// some sequence differ. The exit code is the real pass's mismatches plus the skips not caught.
//
// usage: won_action37a_test [sequences=3000] [seed=1]
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "rust/won_lobby/wonaction.h"

// ---- the world

static std::vector<std::string>* sLog;
static void Log(const std::string& s) { sLog->push_back(s); }
static std::string N(long long v) { return std::to_string(v); }
static std::string T(const UWideString& s) { return std::string((const char*)s); }

enum { kFormLobby = 26, kFormAvailableRooms = 31, kFormAvailableGames = 32 };
enum { kAvailableGamesAvalGamesListBox = 13, kAvailableRoomsAvailRoomsListBox = 6, kAvailableRoomsTotalGamesNumber = 14,
       kAvailableRoomsServersNameLabel = 16 };
static const unsigned long IDS_SETUP_PLAYERS = 7206;

struct UIControl {
    std::string mName;
    virtual ~UIControl() {}
};
struct UISlider {
    unsigned long mMax = 0, mValue = 0;
    unsigned long GetMaxRange() const { return mMax; }
    unsigned long GetCurrentRangeValue() const { return mValue; }
};
struct UIScrollBar {
    UISlider mSlider;
    UISlider* GetSlider() { return &mSlider; }
};
struct UITextLabel : UIControl {
    std::string mText;
    bool mHasBar = false;
    UIScrollBar mBar;
    UIScrollBar* GetScrollBar() { Log(mName + " bar?"); return mHasBar ? &mBar : NULL; }
    void SetText(const UWideString& t) { Log(mName + " set " + T(t)); mText = T(t); }
    // a line appended: the slider's range grows by one line once the label has a scroll bar
    void AppendText(const UWideString& t)
    {
        Log(mName + " append " + T(t));
        mText += T(t) + "\n";
        if (mHasBar) mBar.mSlider.mMax++;
    }
    void SetStartingDisplayLine(unsigned long l) { Log(mName + " line " + N(l)); mBar.mSlider.mValue = l; }
};
static std::string Describe(const UWideString& s) { return T(s); }
template <class L> struct UIListBox : UIControl {
    std::vector<L> mData;
    unsigned long mDisplay = 0;
    long mSelection = -1;
    unsigned long GetDisplayLine() const { Log(mName + " display"); return mDisplay; }
    void DeleteAllEntries() { Log(mName + " delete all"); mData.clear(); mSelection = -1; mDisplay = 0; }
    unsigned long GetLineCount() const { Log(mName + " count"); return (unsigned long)mData.size(); }
    void SetStartingDisplayLine(unsigned long l) { Log(mName + " line " + N(l)); mDisplay = l; }
    void AppendLine(const L& l) { Log(mName + " append " + Describe(l)); mData.push_back(l); }
    long GetCurrentSelection(unsigned long& out) const
    {
        Log(mName + " selection");
        if (mSelection < 0 || (size_t)mSelection >= mData.size()) return kErrorFailed;
        out = (unsigned long)mSelection;
        return kErrorOK;
    }
    const L& GetData(unsigned long i) const { Log(mName + " data " + N(i)); return mData[i]; }
    void ChangeSelectionIndex(unsigned long i, bool a, bool b) { Log(mName + " select " + N(i) + " " + N(a) + N(b)); mSelection = (long)i; }
};
struct EEUICWNGBLine { UWideString mName; long mHumans; long mPlayers; bool mHasPassword; bool mIsPrivate; bool mIsInProgress; };
struct EEUICWNRBLine { UWideString mName; UWideString mDescription; long mNumPlayers; long mNumGames; };
static std::string Describe(const EEUICWNGBLine& l)
{ return T(l.mName) + " " + N(l.mHumans) + " " + N(l.mPlayers) + " " + N(l.mHasPassword) + N(l.mIsPrivate) + N(l.mIsInProgress); }
static std::string Describe(const EEUICWNRBLine& l) { return T(l.mName) + "|" + T(l.mDescription) + "|" + N(l.mNumPlayers) + "|" + N(l.mNumGames); }
struct EEUICWonNetGamesBox : UIListBox<EEUICWNGBLine> {};
struct EEUICWonNetRoomsBox : UIListBox<EEUICWNRBLine> {};
struct UIForm {
    long mId = 0;
    std::map<long, UIControl*> mControls;
    UIControl* GetControl(long i) { Log("form " + N(mId) + " control " + N(i)); return mControls.count(i) ? mControls[i] : NULL; }
};

// one side's world
struct World {
    std::vector<std::string> mLog;
    UIForm mForms[4];          // the lobby, the room list, the game room, another
    long mCurrent = 3;
    UITextLabel mLobbyChat, mRoomChat, mTotals, mServer;
    UIListBox<UWideString> mPlayers;
    EEUICWonNetGamesBox mGames;
    EEUICWonNetRoomsBox mRooms;
    int mTexts = 0;            // the UWideStrings the host made and has not deleted
    World()
    {
        static const long kIds[4] = {kFormLobby, kFormAvailableRooms, kFormAvailableGames, 5};
        for (int i = 0; i < 4; i++) mForms[i].mId = kIds[i];
        Add(0, 8, &mLobbyChat, "lobby chat");
        Add(2, 8, &mPlayers, "players");
        Add(2, kAvailableGamesAvalGamesListBox, &mGames, "games");
        Add(2, 15, &mRoomChat, "room chat");
        Add(1, kAvailableRoomsAvailRoomsListBox, &mRooms, "rooms");
        Add(1, kAvailableRoomsTotalGamesNumber, &mTotals, "totals");
        Add(1, kAvailableRoomsServersNameLabel, &mServer, "server");
    }
    void Add(int f, long id, UIControl* c, const char* n) { c->mName = n; mForms[f].mControls[id] = c; }
    template <class L> static std::string Dump(const UIListBox<L>& l)
    {
        std::string s = l.mName + "[" + N(l.mDisplay) + "," + N(l.mSelection) + "]";
        for (const L& x : l.mData) s += " {" + Describe(x) + "}";
        return s;
    }
    static std::string Dump(const UITextLabel& l)
    { return l.mName + "[" + N(l.mHasBar) + "," + N(l.mBar.mSlider.mMax) + "," + N(l.mBar.mSlider.mValue) + "] " + l.mText; }
    std::string Dump() const
    {
        return Dump(mLobbyChat) + "\n" + Dump(mRoomChat) + "\n" + Dump(mTotals) + "\n" + Dump(mServer) + "\n" + Dump(mPlayers) + "\n" +
               Dump(mGames) + "\n" + Dump(mRooms) + "\ntexts " + N(mTexts);
    }
};
static World* sW;
struct UIManagerT {
    UIForm* GetCurrentForm() { Log("current form"); return &sW->mForms[sW->mCurrent]; }
    UIForm* GetFormByID(long id)
    {
        Log("form by id " + N(id));
        for (UIForm& f : sW->mForms) if (f.mId == id) return &f;
        return NULL;
    }
};
static UIManagerT sUIManager;
static UIManagerT* gUIManager = &sUIManager;
struct ResourceT { long GetString(unsigned long id, UWideString& out) { Log("res " + N(id)); out = ("S" + N(id)).c_str(); return kErrorOK; } };
static ResourceT sResource;
static ResourceT* gResource = &sResource;
struct EEAction {
    virtual ~EEAction() {}
    virtual void Virtual4() = 0;
    virtual void Virtual8() = 0;
};

// ---- the original
static UWideString sSelectedGame("");
static unsigned long sGameCount;
static unsigned long sGameSelection;
static unsigned long sGameDisplayLine;
static unsigned long sListDisplayLine;
#include "won_action37a_orig.h"
static std::string OrigStatics(bool inReset)
{
    std::string s = "selected=" + T(sSelectedGame) + " count=" + N(sGameCount) + " selection=" + N(sGameSelection) + " games_line=" +
                    N(sGameDisplayLine) + " list_line=" + N(sListDisplayLine);
    if (inReset) {
        sSelectedGame = "";
        sGameCount = sGameSelection = sGameDisplayLine = sListDisplayLine = 0;
    }
    return s;
}

// ---- the Rust, over the game's host
namespace New {
#include "ee/EEAWonNetHost.inl"
}
static int sSkip = -1;         // the op skipped (the mutation pass)
static int sHits[64];
static const int kOps = (int)EeWonActionOp::RoomsAppend;
static intptr_t Host(EeWonActionOp inOp, intptr_t a, intptr_t b)
{
    typedef EeWonActionOp Op;
    sHits[(int)inOp]++;
    if ((int)inOp == sSkip)
        return inOp == Op::TextBytes || inOp == Op::GamesName ? (intptr_t) "" : 0;
    // a skipped op's 0 where an object is taken: nothing to call
    bool aNoObject = inOp != Op::CurrentFormIs && inOp != Op::CurrentControl && inOp != Op::FormControl && inOp != Op::TextNew &&
                     inOp != Op::ResString && inOp != Op::GamesBox && inOp != Op::RoomsBox;
    if ((aNoObject && a == 0) || (inOp == Op::ResString && b == 0)) {
        Log("no object");
        return inOp == Op::TextBytes || inOp == Op::GamesName ? (intptr_t) "" : 0;
    }
    if (inOp == Op::TextNew) sW->mTexts++;
    if (inOp == Op::TextDelete) sW->mTexts--;
    return New::WonActionHost(inOp, a, b);
}
static std::string NewStatics(bool inReset)
{
    char aText[512];
    ee_won_action_statics(aText, sizeof(aText), inReset);
    return aText;
}

// ---- the sequences

static uint32_t sSeed;
static uint32_t R(uint32_t n) { sSeed = sSeed * 1664525u + 1013904223u; return (sSeed >> 8) % n; }
static const char* kNames[] = {"alpha", "beta", "gamma", "delta", "", "@@", "@@@@@", "al pha"};
static const char* Name() { return kNames[R(8)]; }

// one action's making
struct Spec {
    long mType;
    std::string mText, mPrefix, mRoom, mServer;
    long mHumans, mPlayers, mL8C;
    bool mPassword, mPrivate, mInProgress, mFromMe;
};
static Spec MakeSpec()
{
    static const long kTypes[] = {3, 3, 3, 3, 4, 4, 4, 6, 6, 7, 7, 2, 2, 8, 5};
    Spec s;
    s.mType = kTypes[R(15)];
    uint32_t k = R(10);
    s.mText = k < 2 ? "@@@@" : k < 4 ? "@@@" : Name();
    s.mPrefix = R(2) ? "al: " : "";
    s.mRoom = R(4) ? Name() : "";
    s.mServer = Name();
    s.mHumans = (long)R(9) - 1;
    s.mPlayers = (long)R(9);
    s.mL8C = (long)R(50) - 3;
    s.mPassword = R(2); s.mPrivate = R(2); s.mInProgress = R(2); s.mFromMe = R(2);
    return s;
}
// the user between two actions: the same on both worlds
static void Perturb(World& w, uint32_t inWhat, uint32_t inValue)
{
    switch (inWhat) {
    case 0: case 1: case 2: w.mCurrent = (long)(inValue % 4); break;
    case 3: w.mGames.mSelection = w.mGames.mData.empty() ? -1 : (long)(inValue % (w.mGames.mData.size() + 1)) - 1; break;
    case 4: w.mGames.mDisplay = inValue % 6; break;
    case 5: w.mPlayers.mDisplay = inValue % 6; break;
    case 6: w.mRoomChat.mHasBar = inValue & 1; w.mLobbyChat.mHasBar = (inValue >> 1) & 1; break;
    case 7: { UISlider& s = (inValue & 1 ? w.mRoomChat : w.mLobbyChat).mBar.mSlider; s.mValue = s.mMax ? (inValue >> 1) % (s.mMax + 1) : 0; break; }
    default: break;
    }
}

static int RunPass(int inSequences, uint32_t inSeed, bool inReport)
{
    int aBad = 0;
    for (int q = 0; q < inSequences; q++) {
        sSeed = inSeed * 7919u + (uint32_t)q;
        World aOrig, aNew;
        OrigStatics(true);
        NewStatics(true);
        int aSteps = 1 + (int)R(40);
        bool aDiffers = false;
        for (int i = 0; i < aSteps && !aDiffers; i++) {
            uint32_t aWhat = R(12), aValue = R(1000);
            Perturb(aOrig, aWhat, aValue);
            Perturb(aNew, aWhat, aValue);
            Spec s = MakeSpec();
            // the original: the constructor of the type as EEWonNet called it (types 2 and 8: the members set, as
            // no constructor sets them)
            sW = &aOrig; sLog = &aOrig.mLog; aOrig.mLog.clear();
            EEAWonNet* o;
            if (s.mType == 3)
                o = new EEAWonNet(s.mType, UWideString(s.mText.c_str()), s.mHumans, s.mPlayers, s.mPassword, s.mPrivate, s.mInProgress);
            else if (s.mType == 6 || s.mType == 7)
                o = new EEAWonNet(s.mType, s.mType == 6 ? &aOrig.mRoomChat : NULL, UWideString(s.mPrefix.c_str()), UWideString(s.mText.c_str()), s.mFromMe);
            else
                o = new EEAWonNet(s.mType, NULL, UWideString(s.mText.c_str()), s.mPassword);
            if (s.mType == 2 || s.mType == 8) {
                o->mRoomName = s.mRoom.c_str(); o->mServerName = s.mServer.c_str();
                o->mL8C = s.mL8C; o->mHumans = s.mHumans; o->mPlayers = s.mPlayers;
            }
            o->Virtual4();
            delete o;
            // the Rust
            sW = &aNew; sLog = &aNew.mLog; aNew.mLog.clear();
            EeWonAction* n;
            if (s.mType == 3)
                n = ee_won_action_new_game(s.mType, s.mText.c_str(), s.mHumans, s.mPlayers, s.mPassword, s.mPrivate, s.mInProgress);
            else if (s.mType == 6 || s.mType == 7)
                n = ee_won_action_new_chat(s.mType, s.mType == 6 ? (intptr_t) static_cast<UIControl*>(&aNew.mRoomChat) : 0, s.mPrefix.c_str(), s.mText.c_str(), s.mFromMe);
            else
                n = ee_won_action_new_list(s.mType, 0, s.mText.c_str(), s.mPassword);
            if (s.mType == 2 || s.mType == 8)
                ee_won_action_set_room(n, s.mRoom.c_str(), s.mServer.c_str(), s.mL8C, s.mHumans, s.mPlayers);
            ee_won_action_run(n, Host);
            ee_won_action_free(n);
            if (aOrig.mLog != aNew.mLog) {
                aDiffers = true;
                if (inReport) {
                    printf("MISMATCH sequence %d step %d type %ld text '%s'\n", q, i, s.mType, s.mText.c_str());
                    for (size_t k = 0; k < aOrig.mLog.size() || k < aNew.mLog.size(); k++)
                        printf("  %-40s | %s\n", k < aOrig.mLog.size() ? aOrig.mLog[k].c_str() : "-", k < aNew.mLog.size() ? aNew.mLog[k].c_str() : "-");
                }
            }
        }
        std::string aOrigEnd = aOrig.Dump() + "\n" + OrigStatics(false), aNewEnd = aNew.Dump() + "\n" + NewStatics(false);
        if (!aDiffers && aOrigEnd != aNewEnd) {
            aDiffers = true;
            if (inReport) printf("MISMATCH sequence %d end\n--- original\n%s\n--- rust\n%s\n", q, aOrigEnd.c_str(), aNewEnd.c_str());
        }
        aBad += aDiffers;
    }
    return aBad;
}

int main(int argc, char** argv)
{
    int aSequences = argc > 1 ? atoi(argv[1]) : 3000;
    uint32_t aSeed = argc > 2 ? (uint32_t)atoi(argv[2]) : 1;
    int aBad = RunPass(aSequences, aSeed, true);
    int aReached = 0;
    for (int i = 1; i <= kOps; i++) aReached += sHits[i] != 0;
    printf("won_action37a_test: %d sequences, %d mismatches, %d of %d host ops reached\n", aSequences, aBad, aReached, kOps);
    int aMissed = 0;
    for (int i = 1; i <= kOps; i++) {
        sSkip = i;
        if (RunPass(aSequences, aSeed, false) == 0) {
            printf("  op %d skipped: NOT CAUGHT\n", i);
            aMissed++;
        }
    }
    sSkip = -1;
    printf("won_action37a_test: %d of %d op skips caught\n", kOps - aMissed, kOps);
    return aBad + aMissed + (aReached != kOps);
}
