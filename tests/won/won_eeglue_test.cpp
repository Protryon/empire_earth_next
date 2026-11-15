// The Empire Earth lobby's glue as Rust (R6 WONLobby round 6): the original EELobbyInit.cpp, EELobbyMisc.cpp and the
// decisions of EELobbyInterface.cpp (the statements copied from the C++ before the port, every call on a game object,
// a lobby object, a string or a global replaced by the host op that makes it: EELobbyGlueHost.cpp runs the same
// calls) against ee_won's eeglue.rs over one scripted host. The lobby game and the connect dialog are real ee_won
// objects on both sides (the original's calls on them are the C++ forwarders'; the Rust calls them directly); their
// ops are traced as "gop" and "dop".
//
// Each of 60,000 sequences sets a game's options and slots (some seated by clients), a saved game's and the
// preferences' options (the struct reads are not traced: the host answers them from these), then makes 1 to 8
// random glue calls (the set-up, the details' paint and the columns, the dialogs, the connect wait, the saved games,
// the game options and players, the adapter, the game-done event, the run loop and the return from a game) and
// forgets the glue's state. The host answers every op from a seeded generator (the same stream for both sides). The
// connect dialog's modal loop and the lobby's run re-enter the glue with a connect status, a cancel or the quit flag
// half of the time (two deep at most). The traces must be the same.
//
// The first 80 sequences and every later one that makes an op or call none before made are
// rust/crates/ee_won/src/golden_eeglue.txt, which the Rust tests replay; `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <set>
#include <map>
#include "rust/ee_won.h"

typedef EeWonEeGlueOp Op;
typedef EeWonEeGlueCall Call;
typedef EeWonEeGlueField F;
typedef EeWonEeGlueText T;
typedef EeWonEeGameCall GC;
typedef EeWonLobbyDlgCall DC;

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned sSeed;
static unsigned Rand(unsigned& ioSeed) { ioSeed = ioSeed * 1103515245u + 12345u; return (ioSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return Rand(sSeed) % n; }
static std::vector<std::string> sTrace;
static std::set<std::string> sSeen;

static void Line(const char* inFormat, long long a = 0, long long b = 0, long long c = 0, long long d = 0,
                 long long e = 0, long long f = 0, long long g = 0)
{
    char aBuffer[300];
    snprintf(aBuffer, sizeof(aBuffer), inFormat, a, b, c, d, e, f, g);
    sTrace.push_back(aBuffer);
}

static const intptr_t kGame = 0x3000, kItemA = 0x3100, kItemB = 0x3200, kGraphics = 0x6A, kList = 0x6B,
                      kConfig = 0x77, kFileDlg = 0xF11E, kConnectDlg = 0xC0DE, kSave = 0x5A7E, kSaveOptions = 0x8100,
                      kDefaultOptions = 0x8200, kAddresses = 0xADD, kForm = 0xF0, kCombo = 0xC0, kName = 0x9000,
                      kFilter = 0xF1;

// ---- the sequence's world: a saved game's and the preferences' options, read without ops
struct Player { long mType, mColor, mTeam, mCitizens; int mName; bool mNameEmpty; };
struct Options { long mFields[16]; Player mPlayers[16]; };
static Options sOptions[2]; // 0 the saved game's, 1 the preferences'
static Options& OptionsOf(intptr_t p) { return sOptions[p == kDefaultOptions ? 1 : 0]; }
// the player being made (EELobbyFillPlayers' local): its type is read without an op
static long sPlayerType;

static bool sRust;
static EeWonEeGame* sGame;
static EeWonLobbyDlg* sDialog; // the connect dialog's Rust object (both sides)
static int sDepth;
static intptr_t OrigCall(Call inCall, intptr_t a, intptr_t b, intptr_t c);

static intptr_t GlueCall(Call inCall, intptr_t a, intptr_t b, intptr_t c)
{
    return sRust ? ee_won_eeglue_call(inCall, a, b, c) : OrigCall(inCall, a, b, c);
}

// the lobby's messages during the connect wait and the run: a connect status, the captain's cancel, the quit flag
static void Reenter()
{
    if (sDepth >= 2 || R(2) != 0) return;
    sDepth++;
    Call aCall = Call::SetSlotConnected;
    intptr_t a = R(9), b = R(2);
    switch (R(5)) {
    case 0: aCall = Call::CancelConnectDialog; a = b = 0; break;
    case 1: aCall = Call::SetQuitFlag; a = b = 0; break;
    case 2: aCall = Call::IsConnectDialogRunning; a = b = 0; break;
    default: break;
    }
    Line("reenter %lld %lld %lld", (long long)aCall, a, b);
    intptr_t r = GlueCall(aCall, a, b, 0);
    Line("back %lld", r);
    sDepth--;
}

static std::string Name(intptr_t p) { return p ? std::string(1, (char)('A' + (p - kName) / 16)) : std::string(); }

static intptr_t Host(Op inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t e, intptr_t f)
{
    intptr_t r = 0;
    switch (inOp) {
    // the reads without a trace (fields, the C++ objects' Rust objects, a string's units)
    case Op::GameRust: return (intptr_t)sGame;
    case Op::DialogRust: return a == kConnectDlg ? (intptr_t)sDialog : 0;
    case Op::ItemGame: return a ? kGame : 0;
    case Op::SaveChecksum: return 0x1234;
    case Op::OptionsField: {
        Options& o = OptionsOf(a);
        if (b == (intptr_t)F::MapTypeIndex) return o.mFields[15];
        return o.mFields[b - (intptr_t)F::MapSize];
    }
    case Op::OptionsPlayer: {
        Player& p = OptionsOf(a).mPlayers[b];
        switch ((F)c) {
        case F::Type: return p.mType;
        case F::Color: return p.mColor;
        case F::Team: return p.mTeam;
        case F::StartingCitizens: return p.mCitizens;
        case F::NameEmpty: return p.mNameEmpty;
        default: return 0;
        }
    }
    case Op::PlayerType: return sPlayerType;
    case Op::StringUnits: {
        std::string n = Name(a);
        if (b != 0) for (size_t i = 0; i < n.size(); i++) reinterpret_cast<uint32_t*>(b)[i] = (uint32_t)n[i];
        return (intptr_t)n.size();
    }
    // the traced ops
    case Op::MyGame: r = R(6) ? kGame : 0; break;
    case Op::GameIsHost: case Op::IAmCaptain: case Op::IsLobbyOpen: case Op::IsShown: r = R(2); break;
    case Op::TextWidth: r = R(4) ? (intptr_t)R(90) : (intptr_t)R(5) - 2; break;
    case Op::FontHeight: case Op::ListFontHeight: r = 10 + R(5); break;
    case Op::TextFind: r = (intptr_t)R(5) - 1; break;
    case Op::TextCompareNoCase: r = (intptr_t)R(5) - 2; break;
    case Op::NumMapTypes: r = R(4) ? 3 + R(3) : 0; break;
    case Op::PlayerColor: {
        float* rgb = reinterpret_cast<float*>(b);
        static const float kV[8] = {0.0f, 1.0f, 0.5f, 0.25f, 1.5f, -0.5f, 0.999f, 0.0039f};
        rgb[0] = kV[R(8)]; rgb[1] = kV[R(8)]; rgb[2] = kV[R(8)];
        uint32_t bits[3];
        memcpy(bits, rgb, sizeof(bits)); // the floats' bits, for the replay
        Line("op %lld %lld rgb %lld %lld %lld", (long long)inOp, a, bits[0], bits[1], bits[2]);
        return 0;
    }
    case Op::FindDialog: r = R(5) ? (b ? kConnectDlg : kFileDlg) : 0; break;
    case Op::Config: r = R(3) ? 0xC0F : 0; break;
    case Op::ConfigPort: r = R(70000); break;
    case Op::LobbyWindow: r = R(3) ? 0x3D : 0; break;
    case Op::SaveNew: r = kSave; break;
    case Op::SaveOpen: case Op::SaveLoadHeader: r = R(5) != 0; break;
    case Op::SaveLength: r = R(100000); break;
    case Op::SaveOptions: r = kSaveOptions; break;
    case Op::OptionsPlayerName: { Player& p = OptionsOf(a).mPlayers[b]; r = kName + 16 * p.mName; break; }
    case Op::FileName: r = kName + 16 * 5; break;
    case Op::FileSize: r = R(50000); break;
    case Op::ClientPlayer: r = R(3) ? 0x7300 : 0; break;
    case Op::DefaultOptions: r = kDefaultOptions; break;
    case Op::PlayerNew: sPlayerType = 3; break;
    case Op::PlayerSet: if (a == (intptr_t)F::Type) sPlayerType = b; break;
    case Op::LocalHost: r = R(4) ? 0x0a000001 + R(3) : 0; break;
    case Op::AddressesGet: r = kAddresses; break;
    case Op::AddressesCount: r = R(4); break;
    case Op::AddressHandle: r = 0x0a000001 + R(3); break;
    case Op::LobbyRun: r = R(4); break;
    case Op::GameForm: r = 0xF0; break;
    case Op::CurrentForm: r = R(2) ? 0xF0 : 0xF1; break;
    default: break;
    }
    Line("op %lld %lld %lld %lld %lld %lld %lld", (long long)inOp, a, b, c, d, e, f);
    sTrace.back() += " = " + std::to_string((long long)r);
    if (inOp == Op::LobbyRun) Reenter();
    return r;
}

static intptr_t O(Op inOp, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0, intptr_t e = 0, intptr_t f = 0)
{
    return Host(inOp, a, b, c, d, e, f);
}

static intptr_t GameHost(void*, EeWonEeGameOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    if (inOp == EeWonEeGameOp::NumMapTypes) return 12; // read by a new game (untraced; the Rust tests answer 12)
    intptr_t r = inOp == EeWonEeGameOp::ClientName ? kName + 16 * R(4) : R(4);
    if (inOp == EeWonEeGameOp::MyPlayer) r = R(3) ? 0x7300 : 0;
    if (inOp == EeWonEeGameOp::PlayerSlot) r = R(9);
    Line("gop %lld %lld %lld %lld %lld", (long long)inOp, a, b, c, d);
    sTrace.back() += " = " + std::to_string((long long)r);
    return r;
}

static intptr_t DialogHost(void*, EeWonLobbyDlgOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    intptr_t r = 0;
    switch (inOp) {
    case EeWonLobbyDlgOp::GameRust: return (intptr_t)sGame;
    case EeWonLobbyDlgOp::GameIsHost: r = R(2); break;
    case EeWonLobbyDlgOp::ConnectInsert: r = R(2); break;
    case EeWonLobbyDlgOp::DoDialog: r = R(2) ? 100001 : 100002; break;
    case EeWonLobbyDlgOp::RootWindow: r = R(4) ? 0x4000 : 0; break;
    default: break;
    }
    Line("dop %lld %lld %lld %lld", (long long)inOp, a, b, c);
    sTrace.back() += " = " + std::to_string((long long)r);
    if (inOp == EeWonLobbyDlgOp::DoDialog) Reenter();
    return r;
}

static intptr_t EG(GC inCall, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0)
{
    return ee_won_eegame_call(sGame, inCall, a, b, c, d);
}
static intptr_t ED(DC inCall, intptr_t a = 0, intptr_t b = 0) { return ee_won_lobbydlg_call(sDialog, inCall, a, b, 0); }

// ---- the original statements, every call on another object replaced by its op

static struct Orig {
    intptr_t mFileDialog, mConnectDialog;   // SmartPtr<EELobbyFileDialog>, SmartPtr<EELobbyConnectDialog>
    bool mClosing, mGameDone;
    unsigned long mColors[16];               // gEELobbyColors
} sO;

static void Draw(intptr_t g, T k, intptr_t v, intptr_t x, intptr_t y) { O(Op::DrawText, g, (intptr_t)k, v, x, y); }

// EELobbyServerDetailsItem's texts (a text is its kind and value)
struct Text { T k; intptr_t v; };
static Text OptionText(int inOption, int inFirst, unsigned long inCount)
{
    unsigned long theValue = (unsigned long)(long)EG(GC::GetOption, inOption);
    if (theValue < inCount) return Text{T::Resource, (intptr_t)(theValue + inFirst)};
    return Text{T::Empty, 0};
}
static Text EpochText(bool inStart)
{
    int theOption = inStart ? 4 : 5;
    if ((long)EG(GC::GetOption, theOption) == 15) return Text{T::Resource, 15}; // gEELobbyGameOptionsCtrl_Epoch16
    return OptionText(theOption, 0, 16);
}
static Text OnOffText(int inOption)
{
    return (long)EG(GC::GetOption, inOption) != 0 ? Text{T::Resource, 39} : Text{T::Resource, 40};
}
static Text MapTypeName(int inMapType)
{
    if (inMapType >= 0 && (unsigned long)inMapType < (unsigned long)O(Op::NumMapTypes)) return Text{T::MapType, inMapType};
    return Text{T::Empty, 0};
}
static Text MapTypeText()
{
    if ((long)EG(GC::GetOption, 0) == 0) return MapTypeName((int)(long)EG(GC::GetOption, 2));
    return Text{T::Resource, 17}; // gEELobbyGameOptionsCtrl_GameType2
}

static void OrigPaintDetails(intptr_t inItem, intptr_t g)
{
    O(Op::ItemGame, inItem);
    int theCounts[4] = {8, 8, 7, 7};
    // GameType, MapSize, MapType, Resources, StartingEpoch, EndingEpoch, GameUnitLimit, GameVariant
    Text theLabels1[8] = {{T::Resource, 42}, {T::Resource, 43}, {T::Resource, 44}, {T::Resource, 45},
                          {T::Resource, 46}, {T::Resource, 47}, {T::Resource, 48}, {T::Resource, 49}};
    Text theValues1[8];
    theValues1[0] = OptionText(0, 0x10, 3);
    theValues1[1] = OptionText(1, 0x13, 6);
    theValues1[2] = MapTypeName((int)(long)EG(GC::GetOption, 2));
    theValues1[3] = OptionText(3, 0x19, 5);
    theValues1[4] = EpochText(true);
    theValues1[5] = EpochText(false);
    theValues1[6] = Text{T::Number, (intptr_t)(long)EG(GC::GetOption, 9)};
    theValues1[7] = OptionText(6, 0x1e, 2);
    // DifficultyLevel, GameSpeed, RevealMap, UseCustomCivs, LockTeams, LockSpeed, NumberOfWonders
    Text theLabels2[7] = {{T::Resource, 50}, {T::Resource, 51}, {T::Resource, 52}, {T::Resource, 53},
                          {T::Resource, 54}, {T::Resource, 55}, {T::Resource, 58}};
    Text theValues2[7];
    theValues2[0] = OptionText(7, 0x20, 3);
    theValues2[1] = OptionText(8, 0x23, 4);
    theValues2[2] = OnOffText(0xb);
    theValues2[3] = OnOffText(0xc);
    theValues2[4] = OnOffText(0xd);
    theValues2[5] = OnOffText(0xe);
    // GetWondersText
    if ((long)EG(GC::GetOption, 10) == 0) theValues2[6] = Text{T::Resource, 41};
    else theValues2[6] = Text{T::Number, (intptr_t)(long)EG(GC::GetOption, 10)};
    Text* theColumns[4] = {theLabels1, theValues1, theLabels2, theValues2};
    int theGaps[4] = {10, 50, 10, 50};
    unsigned long theColors[4] = {0x8000000f, 0x8000000e, 0x8000000f, 0x8000000e};
    int theX = 0, theY, i;
    for (i = 0; i < 4; i++) {
        theY = 0;
        int theWidth = 0;
        O(Op::SetColor, g, (intptr_t)theColors[i]);
        for (int j = 0; j < theCounts[i]; j++) {
            int theTextWidth = (int)O(Op::TextWidth, g, (intptr_t)theColumns[i][j].k, theColumns[i][j].v);
            if (theTextWidth > theWidth) theWidth = theTextWidth;
            Draw(g, theColumns[i][j].k, theColumns[i][j].v, theX, theY);
            theY += (int)O(Op::FontHeight, g);
        }
        theX += theGaps[i] + theWidth;
    }
    theY = 0;
    O(Op::SetColor, g, 0x8000000f);
    Draw(g, T::Resource, 70, theX, 0); // gEELobbyInterface_Players
    theX += (int)O(Op::TextWidth, g, (intptr_t)T::Resource, 70) + 10;
    O(Op::SetColor, g, 0x8000000e);
    int theNumFiles = (int)EG(GC::GetNumFiles);
    for (i = 0; i < theNumFiles; i++) {
        intptr_t theFile = EG(GC::GetFile, i);
        Draw(g, T::Data, theFile, theX, theY);
        theY += (int)O(Op::FontHeight, g);
    }
}

static void OrigInit()
{
    O(Op::SetColumnHandlers, 0);
    O(Op::SetColumnHandlers, 1);
    O(Op::SetFactories);
    O(Op::AddResources);
    for (int i = 0; i < 16; i++) O(Op::AddGameOption, 0, (intptr_t)T::Resource, i);
    // EELobbyAddMapTypeOptions
    int theCount = (int)O(Op::NumMapTypes);
    for (int i = 0; i < theCount; i++) O(Op::AddGameOption, 1, (intptr_t)T::MapType, i);
    // EELobbyGetPlayerColors(gEELobbyColors, 16)
    for (int i = 0; i < 16; i++) {
        float theColor[3];
        O(Op::PlayerColor, i, (intptr_t)theColor);
        int theRed = (int)(theColor[0] * 255.0f);
        int theGreen = (int)(theColor[1] * 255.0f);
        int theBlue = (int)(theColor[2] * 255.0f);
        if (theRed < 0) theRed = 0;
        if (theGreen < 0) theGreen = 0;
        if (theBlue < 0) theBlue = 0;
        if (theRed > 255) theRed = 255;
        if (theGreen > 255) theGreen = 255;
        if (theBlue > 255) theBlue = 255;
        sO.mColors[i] = (theRed << 8 | theGreen) << 8 | theBlue;
    }
    O(Op::AddGameOption, 1, (intptr_t)T::Resource, 17); // gEELobbyGameOptionsCtrl_GameType2
    O(Op::SetProductName);
}

// SafeGetComponent into a held SmartPtr: the found one comes with the look-up's reference, the old one is released
static void Assign(intptr_t& ioMember, intptr_t inNew)
{
    intptr_t theOld = ioMember;
    ioMember = inNew;
    if (theOld != 0) O(Op::Release, theOld);
}

static void OrigShutdown()
{
    O(Op::LobbyClose);
    O(Op::ResourceDestroy);
    Assign(sO.mFileDialog, 0);
    Assign(sO.mConnectDialog, 0);
}

static bool OrigRunConnectDialog()
{
    intptr_t theDialog = sO.mConnectDialog; // SmartPtr<EELobbyConnectDialog> theDialog = EELobbyGetConnectDialog()
    if (theDialog == 0) return false;
    O(Op::AddRef, theDialog);
    bool theResult = false;
    intptr_t theGame = O(Op::MyGame);
    if (theGame != 0) {
        bool theFlag = O(Op::GameIsHost, theGame) != 0;
        theResult = ED(DC::ConnectRun, theGame) != 0; // theDialog->Run(theGame)
        theGame = O(Op::MyGame);
        if (theGame == 0)
            theResult = false;
        else if (!theResult) {
            if (theFlag) {
                O(Op::SetGameInProgress, 0);
                EG(GC::SendCancelConnect);
            } else if (!ED(DC::Field, 8)) { // !theDialog->IsEnded()
                EG(GC::SendConnectStatus, 0);
                O(Op::LeaveGame, 0);
            }
        }
    }
    O(Op::Release, theDialog);
    return theResult;
}

// EELobbySetSlot(slot, inOptions.mPlayers[inPlayer])
static void OrigSetSlot(int inSlot, intptr_t inOptions, int inPlayer)
{
    Player& p = OptionsOf(inOptions).mPlayers[inPlayer];
    switch (p.mType) {
    case 0: EG(GC::SlotSetComputer, inSlot); break;
    case 1:
    case 2:
        if (EG(GC::SlotClient, inSlot) != 0 && O(Op::ClientPlayer, EG(GC::SlotClient, inSlot)) != 0) {
            EG(GC::SlotSetRawOption, inSlot, 4, p.mType == 2);
            intptr_t theClient = EG(GC::SlotClient, inSlot);
            intptr_t thePlayer = O(Op::ClientPlayer, EG(GC::SlotClient, inSlot));
            EG(GC::SlotSetPlayer, inSlot, theClient, thePlayer);
        } else if (p.mNameEmpty) {
            EG(GC::SlotSetClosed, inSlot);
        } else {
            intptr_t theName = O(Op::OptionsPlayerName, inOptions, inPlayer); // EELobbyToGUIString(mName)
            EG(GC::SlotSetName, inSlot, theName);
            O(Op::Release, theName);
            EG(GC::SlotSetRawOption, inSlot, 4, p.mType == 2);
        }
        break;
    case 3: EG(GC::SlotSetOpen, inSlot); return;
    case 4: EG(GC::SlotSetClosed, inSlot); return;
    }
    if (p.mType == 1) EG(GC::SlotSetRawOption, inSlot, 2, (unsigned char)p.mColor);
    if (p.mType != 2) {
        EG(GC::SlotSetRawOption, inSlot, 1, (unsigned char)p.mTeam);
        EG(GC::SlotSetRawOption, inSlot, 3, (unsigned char)p.mCitizens);
    }
}

static void OrigSetGameOptions(intptr_t inOptions)
{
    Options& o = OptionsOf(inOptions);
    EG(GC::SetOption, 2, (intptr_t)(uint32_t)o.mFields[15]); // EERMFindGenerator(mMapType, theMapType)
    EG(GC::SetOption, 1, (intptr_t)(uint32_t)o.mFields[0]);
    EG(GC::SetOption, 3, (intptr_t)(uint32_t)o.mFields[1]);
    EG(GC::SetOption, 4, (intptr_t)(uint32_t)o.mFields[2]);
    EG(GC::SetOption, 5, (intptr_t)(uint32_t)o.mFields[3]);
    EG(GC::SetOption, 6, (intptr_t)(uint32_t)(o.mFields[4] - 1));
    EG(GC::SetOption, 7, (intptr_t)(uint32_t)o.mFields[5]);
    switch (o.mFields[6]) {
    case 0: EG(GC::SetOption, 8, 0); break;
    case 1: EG(GC::SetOption, 8, 1); break;
    case 2: EG(GC::SetOption, 8, 2); break;
    case 3: EG(GC::SetOption, 8, 3); break;
    }
    EG(GC::SetOption, 9, (intptr_t)(uint32_t)o.mFields[7]);
    EG(GC::SetOption, 10, (intptr_t)(uint32_t)o.mFields[8]);
    EG(GC::SetOption, 11, o.mFields[9] != 0);
    EG(GC::SetOption, 12, o.mFields[10] != 0);
    EG(GC::SetOption, 13, o.mFields[11] != 0);
    EG(GC::SetOption, 14, o.mFields[12] != 0);
    EG(GC::SetOption, 15, o.mFields[13] != 0);
}

static void OrigSetupSavedGame(intptr_t inOptions, intptr_t inFile, intptr_t inChecksum)
{
    intptr_t theGame = O(Op::MyGame);
    if (theGame == 0) return;
    EG(GC::SetOption, 0, 1);
    {
        intptr_t theName = O(Op::FileName, inFile);
        EG(GC::SetGameFile, theName, O(Op::FileSize, inFile), inChecksum, 1);
        O(Op::Release, theName);
    }
    OrigSetGameOptions(inOptions);
    std::set<long> theFreeOptions, theFreeSlots;
    std::map<std::string, long> theNames;
    long i;
    for (i = 0; i < 8; i++) {
        Player& p = OptionsOf(inOptions).mPlayers[i];
        if (p.mType == 1 || p.mType == 2) {
            intptr_t theName = O(Op::OptionsPlayerName, inOptions, i);
            theNames[Name(theName)] = i;
            O(Op::Release, theName);
        }
        theFreeOptions.insert(i);
        theFreeSlots.insert(i);
    }
    for (i = 0; i < 8; i++) {
        if ((unsigned char)EG(GC::SlotRawOption, i, 0) == 3) {
            long theIndex = theNames[Name(EG(GC::SlotGetName, i))];
            OrigSetSlot((int)i, inOptions, (int)theIndex);
            theFreeOptions.erase(theIndex);
            theFreeSlots.erase(i);
        }
    }
    while (theFreeOptions.size() != 0 && theFreeSlots.size() != 0) {
        OrigSetSlot((int)*theFreeSlots.begin(), inOptions, (int)*theFreeOptions.begin());
        theFreeSlots.erase(theFreeSlots.begin());
        theFreeOptions.erase(theFreeOptions.begin());
    }
}

static bool OrigLoadSavedGame(intptr_t inFile)
{
    intptr_t theSave = O(Op::SaveNew, inFile);
    bool theResult = false;
    if (!O(Op::SaveOpen, theSave)) {
        O(Op::BoxReadError);
    } else {
        O(Op::SaveClose, theSave);
        if (!O(Op::SaveLoadHeader, theSave)) {
            O(Op::BoxReadError);
        } else {
            intptr_t theOptions = O(Op::SaveOptions, theSave); // theOptions = theHeader.mGameOptions
            intptr_t theGame = O(Op::MyGame);
            if (theGame != 0) {
                std::set<std::string> theNames;
                int i;
                for (i = 0; i < 8; i++) {
                    Player& p = OptionsOf(theOptions).mPlayers[i];
                    if (p.mType == 1 || p.mType == 2) {
                        intptr_t theName = O(Op::OptionsPlayerName, theOptions, i);
                        theNames.insert(Name(theName));
                        O(Op::Release, theName);
                    }
                }
                theResult = true;
                for (i = 0; i < 8; i++) {
                    if ((unsigned char)EG(GC::SlotRawOption, i, 0) == 3 &&
                        theNames.find(Name(EG(GC::SlotGetName, i))) == theNames.end()) {
                        O(Op::BoxNotInGame, EG(GC::SlotGetName, i));
                        theResult = false;
                        break;
                    }
                }
                if (theResult) OrigSetupSavedGame(theOptions, inFile, 0x1234); // the header's mChecksum
            }
        }
    }
    O(Op::SaveFree, theSave);
    return theResult;
}

static bool OrigSavedGameInfo(intptr_t inFile, uint32_t& outLength, uint32_t& outChecksum)
{
    intptr_t theSave = O(Op::SaveNew, inFile);
    bool theResult = false;
    if (O(Op::SaveOpen, theSave)) {
        outLength = (uint32_t)O(Op::SaveLength, theSave);
        O(Op::SaveClose, theSave);
        if (O(Op::SaveLoadHeader, theSave)) {
            outChecksum = 0x1234;
            theResult = true;
        }
    }
    O(Op::SaveFree, theSave);
    return theResult;
}

static void PlayerSet(F inField, intptr_t inValue) { O(Op::PlayerSet, (intptr_t)inField, inValue); }

static void OrigSlotToPlayer(int inSlot)
{
    if (inSlot < 0) { PlayerSet(F::Type, 4); return; } // GetPlayerSlot(i) of 8..15: NULL
    switch ((unsigned char)EG(GC::SlotRawOption, inSlot, 0)) {
    case 0: PlayerSet(F::Type, 4); return;
    case 1: PlayerSet(F::Type, 0); break;
    case 2: PlayerSet(F::Type, 4); return;
    case 3: PlayerSet(F::Type, (unsigned char)EG(GC::SlotRawOption, inSlot, 4) != 0 ? 2 : 1); break;
    }
    if ((unsigned char)EG(GC::SlotRawOption, inSlot, 0) == 3)
        O(Op::PlayerSetName, EG(GC::SlotGetName, inSlot));
    PlayerSet(F::IsReady, 0);
    PlayerSet(F::B36, 0);
    if (sPlayerType == 1) PlayerSet(F::Color, (unsigned char)EG(GC::SlotRawOption, inSlot, 2));
    if (sPlayerType != 2) {
        PlayerSet(F::Team, (unsigned char)EG(GC::SlotRawOption, inSlot, 1));
        PlayerSet(F::StartingCitizens, (unsigned char)EG(GC::SlotRawOption, inSlot, 3));
    }
}

static void OrigFillPlayers(intptr_t outMyPlayer)
{
    intptr_t theGame = O(Op::MyGame);
    if (theGame == 0) return;
    bool theRandom = EG(GC::HasGameFile) != 0;
    for (int i = 0; i < 16; i++) {
        O(Op::PlayerNew);
        OrigSlotToPlayer(i < 8 ? i : -1);
        PlayerSet(F::Slot, i);
        bool theLocal = (int)EG(GC::GetMySlot) == i;
        PlayerSet(F::IsLocal, theLocal);
        if (theLocal) O(Op::PlayerCopyTo, outMyPlayer);
        if (theRandom) PlayerSet(F::LoadedType, sPlayerType);
        O(Op::PlayerCommit, i);
    }
}

static void OrigFillGameOptions(intptr_t outMyPlayer)
{
    intptr_t theGame = O(Op::MyGame);
    if (theGame == 0) return;
    O(Op::OptionsBegin);
    switch ((long)EG(GC::GetOption, 0)) {
    case 0: O(Op::OptionSet, (intptr_t)F::LoadType, 0); break;
    case 1: O(Op::OptionSet, (intptr_t)F::LoadType, 2); break;
    case 2: O(Op::OptionSet, (intptr_t)F::LoadType, 1); break;
    }
    long theSize = (long)EG(GC::GetOption, 1);
    if (theSize >= 0 && theSize <= 5) O(Op::OptionSet, (intptr_t)F::MapSize, theSize);
    if (EG(GC::HasGameFile)) O(Op::OptionMapTypeName, EG(GC::GetGameFileName));
    else O(Op::OptionMapTypeGenerator, (long)EG(GC::GetOption, 2));
    O(Op::OptionSet, (intptr_t)F::Resources, (long)EG(GC::GetOption, 3));
    O(Op::OptionSet, (intptr_t)F::EpochStart, (long)EG(GC::GetOption, 4));
    O(Op::OptionSet, (intptr_t)F::EpochEnd, (long)EG(GC::GetOption, 5));
    O(Op::OptionSet, (intptr_t)F::Variant, (long)EG(GC::GetOption, 6) + 1);
    O(Op::OptionSet, (intptr_t)F::Difficulty, (long)EG(GC::GetOption, 7));
    long theSpeed = (long)EG(GC::GetOption, 8);
    if (theSpeed >= 0 && theSpeed <= 3) O(Op::OptionSet, (intptr_t)F::GameSpeed, theSpeed);
    O(Op::OptionSet, (intptr_t)F::UnitLimit, (long)EG(GC::GetOption, 9));
    O(Op::OptionSet, (intptr_t)F::WondersForVictory, (long)EG(GC::GetOption, 10));
    O(Op::OptionSet, (intptr_t)F::RevealMap, (long)EG(GC::GetOption, 11) != 0);
    O(Op::OptionSet, (intptr_t)F::AllowCustomCivs, (long)EG(GC::GetOption, 12) != 0);
    O(Op::OptionSet, (intptr_t)F::LockTeams, (long)EG(GC::GetOption, 13) != 0);
    O(Op::OptionSet, (intptr_t)F::LockSpeed, (long)EG(GC::GetOption, 14) != 0);
    O(Op::OptionSet, (intptr_t)F::Victory, (long)EG(GC::GetOption, 15) != 0);
    OrigFillPlayers(outMyPlayer);
    O(Op::OptionsEnd);
}

static void OrigSyncNetworkAdapter()
{
    unsigned long theIndex = 0;
    long theHandle = (long)O(Op::LocalHost);
    if (theHandle != 0) {
        unsigned long i = 1;
        intptr_t theAddresses = O(Op::AddressesGet); // LobbyGetNetAddresses(theAddresses, false)
        intptr_t theCount = O(Op::AddressesCount, theAddresses);
        for (intptr_t k = 0; k < theCount; k++, i++) {
            if ((long)O(Op::AddressHandle, theAddresses, k) == theHandle) {
                theIndex = i;
                break;
            }
        }
        O(Op::AddressesFree, theAddresses);
    }
    O(Op::SetNetworkAdapter, (intptr_t)theIndex);
}

static void OrigGameDone(intptr_t inForm, int32_t* ioCount)
{
    static const int kIds[18] = {11, 5, 3, 4, 6, 16, 23, 26, 10, 9, 13, 22, 2, 1, 20, 27, 0, 7};
    for (int i = 0; i < 18; i++) O(Op::FormHide, inForm, kIds[i]);
    (*ioCount)++;
    if (*ioCount == 2 && O(Op::IsLobbyOpen)) O(Op::PostReturnFromGame);
}

static void OrigRunLoop()
{
    if (sO.mClosing) return;
    sO.mClosing = true;
    O(Op::LocksRelease); // EELobbyActionLocksReleased
    for (;;) {
        int theResult = (int)O(Op::LobbyRun);
        if (theResult == 2) {
            if (OrigRunConnectDialog()) {
                O(Op::PostStartHostedGame);
                O(Op::ShowGameScreen);
                O(Op::LobbySuspend);
                O(Op::Hide);
                break;
            }
        } else if (theResult == 1) {
            O(Op::JoinGame);
            if (OrigRunConnectDialog()) {
                O(Op::ShowGameScreen);
                O(Op::LobbySuspend);
                O(Op::Hide);
                break;
            }
        } else {
            if (theResult == 3) {
                OrigShutdown();
                O(Op::SierraUpdate);
            } else
                OrigShutdown();
            break;
        }
        O(Op::DeleteCommunications);
    }
    sO.mGameDone = false;
    sO.mClosing = false;
    O(Op::LocksRetake);
}

static void OrigReturnFromGame()
{
    if (O(Op::IsLobbyOpen)) {
        O(Op::Show);
        intptr_t theForm = O(Op::GameForm);
        if (O(Op::CurrentForm) == theForm) O(Op::PopForm);
        O(Op::LobbyResume);
        if (sO.mGameDone) O(Op::BoxDissolved);
        OrigRunLoop();
    }
}

static intptr_t OrigCall(Call inCall, intptr_t a, intptr_t b, intptr_t c)
{
    switch (inCall) {
    case Call::Init: OrigInit(); return 0;
    case Call::PaintDetails: OrigPaintDetails(a, b); return 0;
    case Call::DetailsHeight: return (int)O(Op::ListFontHeight, a) * 9;
    case Call::EpochPaint: { O(Op::ItemGame, b); Text t = EpochText(true); Draw(a, t.k, t.v, 0, 0); return 0; }
    case Call::EpochCompare: {
        O(Op::ItemGame, a);
        long theB = (long)EG(GC::GetOption, 4);
        return (int)((long)EG(GC::GetOption, 4) - theB);
    }
    case Call::EpochKeep: {
        Text t = EpochText(true);
        return (int)O(Op::TextFind, 0, (intptr_t)t.k, t.v, b) >= 0;
    }
    case Call::MapTypePaint: { Text t = MapTypeText(); Draw(a, t.k, t.v, 0, 0); return 0; }
    case Call::MapTypeCompare: {
        Text ta = MapTypeText();
        Text tb = MapTypeText();
        return (int)O(Op::TextCompareNoCase, (intptr_t)ta.k, ta.v, (intptr_t)tb.k, tb.v);
    }
    case Call::MapTypeKeep: { Text t = MapTypeText(); return (int)O(Op::TextFind, 0, (intptr_t)t.k, t.v, b) >= 0; }
    case Call::GetComponents: {
        intptr_t theFile = O(Op::FindDialog, a, 0);
        Assign(sO.mFileDialog, theFile);
        intptr_t theConnect = O(Op::FindDialog, a, 1);
        Assign(sO.mConnectDialog, theConnect);
        return 0;
    }
    case Call::Shutdown: OrigShutdown(); return 0;
    case Call::FileTransferPort: {
        intptr_t theConfig = O(Op::Config);
        if (theConfig == 0) return 8889;
        return (unsigned short)O(Op::ConfigPort, theConfig);
    }
    case Call::FileDialog: return sO.mFileDialog;
    case Call::ConnectDialog: return sO.mConnectDialog;
    case Call::RunConnectDialog: return OrigRunConnectDialog();
    case Call::SetSlotConnected: if (sO.mConnectDialog != 0) ED(DC::ConnectSetConnected, (unsigned char)a, b != 0); return 0;
    case Call::CancelConnectDialog: if (sO.mConnectDialog != 0 && !O(Op::IAmCaptain)) ED(DC::ConnectEnd, 0); return 0;
    case Call::PostConnectStatus: {
        intptr_t theWindow = O(Op::LobbyWindow);
        if (theWindow != 0) O(Op::PostSendConnectStatus, theWindow, a != 0);
        return 0;
    }
    case Call::SendConnectStatus: if (O(Op::MyGame) != 0) EG(GC::SendConnectStatus, a != 0); return 0;
    case Call::IsConnectDialogRunning: return sO.mConnectDialog != 0 ? ED(DC::Field, 7) != 0 : 0;
    case Call::SavedGameInfo: return OrigSavedGameInfo(a, *reinterpret_cast<uint32_t*>(b), *reinterpret_cast<uint32_t*>(c));
    case Call::LoadSavedGame: return OrigLoadSavedGame(a);
    case Call::FillMapTypeCombo: {
        O(Op::ComboClear, a);
        int theCount = (int)O(Op::NumMapTypes);
        for (int i = 0; i < theCount; i++) O(Op::ComboAddMapType, a, i);
        return 0;
    }
    case Call::SetDefaultGameOptions: OrigSetGameOptions(O(Op::DefaultOptions)); return 0;
    case Call::FillGameOptions: OrigFillGameOptions(a); return 0;
    case Call::SyncNetworkAdapter: OrigSyncNetworkAdapter(); return 0;
    case Call::GameDone: OrigGameDone(a, reinterpret_cast<int32_t*>(b)); return 0;
    case Call::RunLoop: OrigRunLoop(); return 0;
    case Call::ReturnFromGame: OrigReturnFromGame(); return 0;
    case Call::SetQuitFlag: if (!O(Op::IsShown)) sO.mGameDone = true; return 0;
    case Call::Field:
        switch (a) {
        case 0: return sO.mFileDialog;
        case 1: return sO.mConnectDialog;
        case 2: return sO.mClosing;
        case 3: return sO.mGameDone;
        }
        return 0;
    case Call::Reset: return 0;
    }
    return 0;
}

// ---- the Rust side and the sequences

static intptr_t RustHost(Op inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t e, intptr_t f)
{
    return Host(inOp, a, b, c, d, e, f);
}

struct Step { Call mCall; intptr_t a, b, c; };
struct Setup { int mKind, mSlot, mOption; intptr_t mValue; }; // kind 0 a game option, 1 a slot's raw option, 2 a client

static std::vector<std::string> RunSide(bool inRust, unsigned inSeed, const std::vector<Step>& inSteps,
                                        const std::vector<Setup>& inSetup)
{
    sTrace.clear();
    sSeed = inSeed;
    sRust = inRust;
    sDepth = 0;
    sGame = ee_won_eegame_new((void*)0x6000, GameHost);
    sDialog = ee_won_lobbydlg_new((void*)kConnectDlg, EeWonLobbyDlgKind::Connect, DialogHost);
    for (size_t i = 0; i < inSetup.size(); i++) {
        const Setup& u = inSetup[i];
        if (u.mKind == 0) {
            Line("gset %lld %lld", u.mOption, u.mValue);
            EG(GC::SetOption, u.mOption, u.mValue);
        } else if (u.mKind == 1) {
            Line("gslot %lld %lld %lld", u.mSlot, u.mOption, u.mValue);
            EG(GC::SlotSetRawOption, u.mSlot, u.mOption, u.mValue);
        } else {
            Line("gplayer %lld %lld %lld", u.mSlot, u.mValue, u.mValue + 0x100);
            EG(GC::SlotSetPlayer, u.mSlot, u.mValue, u.mValue + 0x100);
        }
    }
    if (inRust) ee_won_eeglue_set_host(RustHost);
    else memset(&sO, 0, sizeof(sO));
    for (size_t i = 0; i < inSteps.size(); i++) {
        const Step& s = inSteps[i];
        Line("call %lld %lld %lld %lld", (long long)s.mCall, s.a, s.b, s.c);
        intptr_t r;
        if (s.mCall == Call::SavedGameInfo) {
            uint32_t aLength = 7, aChecksum = 9;
            r = inRust ? ee_won_eeglue_call(s.mCall, s.a, (intptr_t)&aLength, (intptr_t)&aChecksum)
                       : OrigCall(s.mCall, s.a, (intptr_t)&aLength, (intptr_t)&aChecksum);
            Line("out %lld %lld", aLength, aChecksum);
        } else if (s.mCall == Call::GameDone) {
            int32_t aCount = (int32_t)s.b;
            r = inRust ? ee_won_eeglue_call(s.mCall, s.a, (intptr_t)&aCount, 0) : OrigCall(s.mCall, s.a, (intptr_t)&aCount, 0);
            Line("out %lld", aCount);
        } else
            r = inRust ? ee_won_eeglue_call(s.mCall, s.a, s.b, s.c) : OrigCall(s.mCall, s.a, s.b, s.c);
        Line("ret %lld", r);
        if (s.mCall == Call::Init) {
            for (unsigned k = 0; k < 16; k++)
                Line("palette %lld %lld", k, inRust ? (long long)ee_won_eecolor_palette(k) : (long long)sO.mColors[k]);
        }
    }
    Line("free");
    for (intptr_t k = 0; k < 4; k++) {
        intptr_t v = inRust ? ee_won_eeglue_call(Call::Field, k, 0, 0) : OrigCall(Call::Field, k, 0, 0);
        Line("field %lld %lld", k, v);
    }
    if (inRust) ee_won_eeglue_call(Call::Reset, 0, 0, 0);
    Line("gfree");
    ee_won_lobbydlg_free(sDialog);
    ee_won_eegame_free(sGame);
    sGame = NULL;
    sDialog = NULL;
    return sTrace;
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    unsigned long long aLines = 0;
    int aDiffer = 0;
    unsigned aStepSeed = 4711;
    const int kSequences = 60000;
    static const char* const kFiles[3] = {"data\\Saved Games\\a.ees", "b.ees", ""};
    for (int n = 0; n < kSequences; n++) {
        unsigned aSave = sSeed;
        sSeed = aStepSeed;
        // the world: the game's options and slots, the two options structs
        std::vector<Setup> aSetup;
        // (the epochs, options 4 and 5, near the sixteenth now and then: its name is the details' exception)
        for (int o = 0; o < 16; o++)
            if (R(2)) {
                intptr_t v = R(4) == 0 ? (intptr_t)R(1300) : (intptr_t)R(4);
                if ((o == 4 || o == 5) && R(3) == 0) v = 13 + R(3);
                aSetup.push_back(Setup{0, 0, o, v});
            }
        for (int i = 0; i < 8; i++) {
            if (R(3) == 0) aSetup.push_back(Setup{2, i, 0, 0x7200 + (intptr_t)R(3)});
            static const unsigned kRange[6] = {6, 6, 17, 6, 2, 120};
            for (int o = 0; o < 6; o++)
                if (R(2)) aSetup.push_back(Setup{1, i, o, (intptr_t)R(kRange[o])});
        }
        for (int k = 0; k < 2; k++) {
            Options& w = sOptions[k];
            for (int f = 0; f < 16; f++) w.mFields[f] = R(4) ? (long)R(5) : (long)R(20) - 3;
            for (int i = 0; i < 16; i++) {
                Player& p = w.mPlayers[i];
                p.mType = R(6);
                p.mColor = R(4) ? R(16) : R(300);
                p.mTeam = R(6);
                p.mCitizens = R(5);
                p.mName = (int)R(4);
                p.mNameEmpty = R(4) == 0;
            }
        }
        std::vector<Step> aSteps;
        if (R(3) == 0) aSteps.push_back(Step{Call::GetComponents, kConfig, 0, 0});
        int aCount = 1 + (int)R(8);
        for (int i = 0; i < aCount; i++) {
            Step s = {Call::Init, 0, 0, 0};
            switch (R(26)) {
            case 0: break;
            case 1: case 2: s.mCall = Call::PaintDetails; s.a = kItemA; s.b = kGraphics; break;
            case 3: s.mCall = Call::DetailsHeight; s.a = kList; break;
            case 4: s.mCall = R(2) ? Call::EpochPaint : Call::MapTypePaint; s.a = kGraphics; s.b = kItemA; break;
            case 5: s.mCall = R(2) ? Call::EpochCompare : Call::MapTypeCompare; s.a = kItemA; s.b = kItemB; break;
            case 6: s.mCall = R(2) ? Call::EpochKeep : Call::MapTypeKeep; s.a = kItemA; s.b = kFilter; break;
            case 7: s.mCall = Call::GetComponents; s.a = kConfig; break;
            case 8: s.mCall = Call::Shutdown; break;
            case 9: s.mCall = R(2) ? Call::FileTransferPort : Call::Field; s.a = R(4); break;
            case 10: case 11: s.mCall = Call::RunConnectDialog; break;
            case 12: s.mCall = Call::SetSlotConnected; s.a = R(4) ? R(9) : R(300); s.b = R(2); break;
            case 13: s.mCall = R(2) ? Call::CancelConnectDialog : Call::IsConnectDialogRunning; break;
            case 14: s.mCall = R(2) ? Call::PostConnectStatus : Call::SendConnectStatus; s.a = R(3); break;
            case 15: s.mCall = Call::SavedGameInfo; s.a = (intptr_t)kFiles[R(3)]; break;
            case 16: case 17: s.mCall = Call::LoadSavedGame; s.a = (intptr_t)kFiles[R(3)]; break;
            case 18: s.mCall = Call::FillMapTypeCombo; s.a = kCombo; break;
            case 19: s.mCall = Call::SetDefaultGameOptions; s.a = kGame; break;
            case 20: case 21: s.mCall = Call::FillGameOptions; s.a = 0x3E; break;
            case 22: s.mCall = Call::SyncNetworkAdapter; break;
            case 23: s.mCall = Call::GameDone; s.a = kForm; s.b = R(4); break;
            case 24: s.mCall = R(2) ? Call::RunLoop : Call::ReturnFromGame; break;
            default: s.mCall = Call::SetQuitFlag; break;
            }
            aSteps.push_back(s);
        }
        aStepSeed = sSeed;
        sSeed = aSave;
        unsigned aSeed = 101u + (unsigned)n * 7919u;
        std::vector<std::string> a = RunSide(false, aSeed, aSteps, aSetup);
        std::vector<std::string> b = RunSide(true, aSeed, aSteps, aSetup);
        aLines += a.size();
        sChecks++;
        if (a != b) {
            if (++aDiffer <= 3) {
                printf("sequence %d differs\n", n);
                for (size_t i = 0; i < a.size() || i < b.size(); i++) {
                    const char* x = i < a.size() ? a[i].c_str() : "-";
                    const char* y = i < b.size() ? b[i].c_str() : "-";
                    if (strcmp(x, y) != 0) { printf("  line %zu: C++ '%s' Rust '%s'\n", i, x, y); break; }
                }
            }
            continue;
        }
        bool aNew = n < 80;
        for (size_t i = 0; i < a.size(); i++) {
            size_t aSpace = a[i].find(' ');
            std::string aHead = a[i].substr(0, aSpace);
            std::string aKey = aHead == "ret" || aHead == "back" || aHead == "out" || aHead == "field" || aHead == "palette"
                                   ? aHead
                                   : a[i].substr(0, a[i].find(' ', aSpace + 1));
            if (sSeen.insert(aKey).second) aNew = true;
        }
        if (aPrint && aNew) {
            printf("seq %d\n", n);
            for (size_t k = 0; k < 2; k++) {
                Options& w = sOptions[k];
                printf("options %zu", k);
                for (int f = 0; f < 16; f++) printf(" %ld", w.mFields[f]);
                for (int i = 0; i < 16; i++) {
                    Player& p = w.mPlayers[i];
                    printf(" %ld %ld %ld %ld %d %d", p.mType, p.mColor, p.mTeam, p.mCitizens, p.mName, (int)p.mNameEmpty);
                }
                printf("\n");
            }
            for (size_t i = 0; i < a.size(); i++) printf("%s\n", a[i].c_str());
        }
    }
    if (aDiffer) { sFailures++; printf("FAIL: %d of %d sequences differ\n", aDiffer, kSequences); }
    if (!aPrint)
        printf("won_eeglue_test: %d sequences, %llu trace lines, %zu distinct ops and calls; %d checks, %d failures\n",
               kSequences, aLines, sSeen.size(), sChecks, sFailures);
    return sFailures ? 1 : 0;
}
