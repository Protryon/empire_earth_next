// The Empire Earth lobby game as a Rust object (R6 WONLobby round 5): the original EELobbyGame.cpp and
// EELobbyPlayer.cpp (EELobbyPlayerSlot; the statements copied from the C++ before the port, every call on a buffer,
// a client, a player, the file transfer, the lobby or a global replaced by the host op that makes it:
// EELobbyGameHost.cpp runs the same calls) against ee_won's eegame.rs over one scripted host.
//
// Each of 60,000 sequences makes a game, then 1 to 12 random calls (every method and virtual hook, the slots'
// methods, the members read back; a quarter of them the game room's messages; half of the sequences host or join a
// game first, a third seat two players on a team), and frees it. The host answers
// each op from a seeded generator (the same stream for both sides). A read past the message's end throws (one read in
// forty): the original's ReadBufferException unwinds through its locals, the Rust answers INTPTR_MIN with the same
// releases made. The lobby's events, the default options, the message boxes and the dialogs re-enter the game with a
// random call a third of the time (two deep at most). The traces must be the same.
//
// The original read and wrote past its sixteen option types and six slot options for a number out of range (from a
// message): both sides here read nothing and write nothing for such a number (the Rust's documented behaviour).
// Members the original left uninitialized (the player counts) start at 0 on both sides.
//
// The first 80 sequences and every later one that makes an op or call none before made are
// rust/crates/ee_won_lobby/src/golden_eegame.txt, which the Rust tests replay; `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <set>
#include "rust/ee_won.h"

typedef EeWonEeGameOp Op;
typedef EeWonEeGameCall Call;

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned sSeed;
static unsigned Rand(unsigned& ioSeed) { ioSeed = ioSeed * 1103515245u + 12345u; return (ioSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return Rand(sSeed) % n; }
static std::vector<std::string> sTrace;
static std::set<std::string> sSeen;

static void Line(const char* inFormat, long long a = 0, long long b = 0, long long c = 0, long long d = 0,
                 long long e = 0, long long f = 0)
{
    char aBuffer[200];
    snprintf(aBuffer, sizeof(aBuffer), inFormat, a, b, c, d, e, f);
    sTrace.push_back(aBuffer);
}

static const intptr_t kGame = 0x9090, kRead = 0xB000, kWrite = 0xC000, kNewBuffer = 0xD000, kIter = 0xE000,
                      kClients = 0x3100, kClient = 0x3200, kPlayer = 0x3300, kServer = 0x3400, kName = 0x5000,
                      kNewString = 0x6000, kTransfer = 0x7000, kTransferName = 0x7800;
static const int kTypes = 1 + (int)Call::SlotPlayer;
static int sCounter;        // the new strings, buffers and transfers made in a run
static bool sRust;
static EeWonEeGame* sRustGame;
static int sDepth;
static intptr_t OrigCall(Call inCall, intptr_t a, intptr_t b, intptr_t c, intptr_t d);
struct ReadThrew {};

static void Reenter()
{
    if (sDepth >= 2 || R(3) != 0) return;
    sDepth++;
    // a call the lobby's controls or the default options make while the game is busy
    Call aCall = Call::GetOption;
    intptr_t a = (intptr_t)R(18) - 1, b = 0, c = 0, d = 0;
    switch (R(10)) {
    case 0: aCall = Call::SetOption; b = R(4) ? R(17) : R(1300); break;
    case 1: aCall = Call::ChangeOption; b = R(17); c = R(2); break;
    case 2: aCall = Call::SlotGetOption; a = (intptr_t)R(10) - 1; b = (intptr_t)R(8) - 1; break;
    case 3: aCall = Call::SlotGetName; a = (intptr_t)R(10) - 1; break;
    case 4: aCall = Call::IsColorFree; a = R(20); break;
    case 5: aCall = Call::CountPlayers; a = R(2); break;
    case 6: aCall = Call::SlotSetRawOption; a = R(8); b = R(6); c = R(6); break;
    case 7: aCall = Call::Field; a = R(5); break;
    case 8: aCall = Call::GetMySlot; break;
    default: break;
    }
    Line("reenter %lld %lld %lld %lld", (long long)aCall, a, b, c);
    intptr_t r;
    if (sRust) r = ee_won_eegame_call(sRustGame, aCall, a, b, c, d);
    else r = OrigCall(aCall, a, b, c, d);
    Line("back %lld", r);
    sDepth--;
}

// the host's answer; outThrew for a read past the end
static intptr_t Answer(Op inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d, bool& outThrew)
{
    intptr_t r = 0;
    outThrew = false;
    switch (inOp) {
    case Op::ReadByte: case Op::ReadShort: case Op::ReadBool: case Op::ReadLong: case Op::ReadWString:
        if (R(40) == 0) { outThrew = true; break; }
        switch (inOp) {
        case Op::ReadByte: r = R(2) ? R(4) : R(3) ? R(9) : R(256); break;
        case Op::ReadShort: r = R(3) ? (intptr_t)R(9) : (intptr_t)(short)R(0x8000) - (intptr_t)R(0x8000); break;
        case Op::ReadBool: r = R(2); break;
        case Op::ReadLong: r = R(4) ? (intptr_t)R(3) : (intptr_t)(int32_t)((uint32_t)R(0x8000) << 17); break;
        default: r = kNewString + sCounter++; break;
        }
        break;
    case Op::Available: r = (intptr_t)R(3) - 1; break;
    case Op::Length: r = R(100); break;
    case Op::NewBuffer: r = kNewBuffer + sCounter++; break;
    case Op::IsHost: case Op::IAmCaptain: case Op::ConnectDialogRunning: case Op::HostGame:
    case Op::TransferIsFileHere: case Op::StringsEqual: case Op::IterHasMore:
        r = R(3) != 0; break;
    case Op::ClientIsCaptain: r = R(4) != 0; break;
    case Op::MaxPlayers: r = R(9); break;
    case Op::GameType: r = R(3); break;
    case Op::Clients: r = R(5) ? kClients : 0; break;
    case Op::FindClient: case Op::ServerClient: case Op::IterNext: r = R(4) ? kClient + R(4) : 0; break;
    case Op::CurrentServer: r = R(3) ? kServer : 0; break;
    case Op::IterNew: r = kIter; break;
    case Op::ClientName: r = kName + (a & 0xff); break;
    case Op::ClientId: r = R(100); break;
    case Op::ClientPlayer: r = R(5) ? kPlayer + (a & 0xff) : 0; break;
    case Op::PlayerSlot: r = R(10); break;
    case Op::PlayerTeam: r = R(4); break;
    case Op::MyPlayer: r = R(5) ? kPlayer + R(4) : 0; break;
    case Op::FileTransferPort: r = R(70000); break;
    case Op::NumMapTypes: r = 5 + R(10); break;
    case Op::Version:
        *reinterpret_cast<uint32_t*>(a) = R(4) ? R(3) : 0x80000000u + R(3);
        r = kNewString + sCounter++;
        break;
    case Op::TransferNew: r = kTransfer + sCounter++; break;
    case Op::TransferName: r = kTransferName + (a & 0xff); break;
    case Op::TransferSize: case Op::TransferChecksum: r = R(100000); break;
    default: break;
    }
    if (outThrew) Line("op %lld %lld %lld %lld %lld = threw", (long long)inOp, a, b, c, d);
    else if (inOp == Op::Version) Line("op %lld %lld %lld = %lld", (long long)inOp, *reinterpret_cast<uint32_t*>(a), b, r);
    else Line("op %lld %lld %lld %lld %lld = %lld", (long long)inOp, a, b, c, d, r);
    // the events, the default options, the boxes and dialogs reach the lobby's controls
    switch (inOp) {
    case Op::SyncPlayer: case Op::SyncOption: case Op::NotifyStagingChange: case Op::LobbyEvent:
    case Op::TeamChanged: case Op::SetDefaultOptions: case Op::SameTeamBox: case Op::HostGame:
    case Op::CancelConnectDialog: case Op::SetSlotConnected: case Op::ResetReadyFlags:
        Reenter();
        break;
    default: break;
    }
    return r;
}

static intptr_t O(Op inOp, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0)
{
    bool aThrew;
    intptr_t r = Answer(inOp, a, b, c, d, aThrew);
    if (aThrew) throw ReadThrew();
    return r;
}

static intptr_t RustHost(void*, Op inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    bool aThrew;
    intptr_t r = Answer(inOp, a, b, c, d, aThrew);
    return aThrew ? INTPTR_MIN : r;
}

// ---- the original classes, every call on another object replaced by its op

// a SmartPtr member (or a GUIString's data: 0 the shared empty string)
struct Ptr {
    intptr_t mPtr = 0;
    void operator=(intptr_t p)
    {
        if (this->mPtr != p) {
            if (this->mPtr != 0) O(Op::Release, this->mPtr);
            this->mPtr = p;
            if (p != 0) O(Op::AddRef, p);
        }
    }
};

// a local that owns one reference (a std::wstring or GUIString temporary): released at the end of its scope
struct Held {
    intptr_t mPtr;
    explicit Held(intptr_t p) : mPtr(p) {}
    ~Held() { if (this->mPtr != 0) O(Op::Release, this->mPtr); }
};

struct OrigSlot {
    unsigned char mIndex;
    unsigned char mOptions[6];
    Ptr mClient, mPlayer, mName;

    void Reset() { mOptions[1] = 0; mOptions[2] = 0; mOptions[3] = 5; mOptions[4] = 0; mOptions[5] = 100; }
    intptr_t GetName()
    {
        if (this->mClient.mPtr == 0) {
            if (this->mOptions[0] == 4) return this->mName.mPtr;
            return 0;
        }
        return O(Op::ClientName, this->mClient.mPtr);
    }
    void SetState(int inState)
    {
        this->mOptions[0] = inState;
        if (inState != 3) this->ClearPlayer();
        if (inState != 4) this->mName = 0;
    }
    unsigned char GetOption(int inOption)
    {
        if (inOption >= 0 && inOption < 6) return this->mOptions[inOption];
        return 0;
    }
    void SetOption(int inOption, unsigned char inValue)
    {
        if (inOption == 0) {
            if (inValue < 5) this->SetState(inValue);
        } else if (inOption >= 1 && inOption < 6) // (the original: unchecked)
            this->mOptions[inOption] = inValue;
    }
    void ClearPlayer()
    {
        if (this->mPlayer.mPtr != 0) {
            O(Op::PlayerDetach, this->mPlayer.mPtr);
            this->mPlayer = 0;
        }
        this->mClient = 0;
    }
    void SetOpen() { this->SetState(0); }
    void SetClosed() { this->SetState(2); }
    void SetComputer() { this->SetState(1); }
    void SetName(intptr_t inName) { this->SetState(4); this->mName = inName; }
    void SetPlayer(intptr_t inClient, intptr_t inPlayer)
    {
        this->ClearPlayer();
        if (inClient != 0 && inPlayer != 0) {
            this->mClient = inClient;
            this->mPlayer = inPlayer;
            O(Op::PlayerAttach, this->mPlayer.mPtr, this->mIndex);
            this->SetState(3);
        }
    }
    void ReadData(intptr_t inBuffer)
    {
        for (int i = 0; i < 6; i++) this->mOptions[i] = (unsigned char)O(Op::ReadByte, inBuffer);
        if (this->mOptions[0] != 3) this->ClearPlayer();
    }
    void WriteData(intptr_t outBuffer)
    {
        for (int i = 0; i < 6; i++) O(Op::AppendByte, outBuffer, this->mOptions[i]);
    }
};

static int sOptionTypes[16];
static unsigned long sOptionLimits[16];
static bool sOptionTypesInitialized;
static int TypeOf(int inOption) { return inOption >= 0 && inOption < 16 ? sOptionTypes[inOption] : -1; }

struct OrigGame {
    std::vector<intptr_t> mFiles;
    Ptr mFileTransfer;
    bool mHasFile;
    unsigned char mNumHumanPlayers, mNumComputerSlots, mNumReservedSlots;
    long mOptions[16];
    OrigSlot mSlots[8];

    void Init()
    {
        for (int i = 0; i < 8; i++) {
            this->mSlots[i].mIndex = 0;
            this->mSlots[i].mOptions[0] = 0;
            this->mSlots[i].Reset();
        }
        // EELobbyInitOptionTypes
        if (!sOptionTypesInitialized) {
            sOptionTypesInitialized = true;
            int aTypes[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 0, 0, 0, 0, 0};
            memcpy(sOptionTypes, aTypes, sizeof(aTypes));
            sOptionLimits[0] = 3; sOptionLimits[1] = 6; sOptionLimits[2] = (unsigned long)O(Op::NumMapTypes);
            sOptionLimits[3] = 5; sOptionLimits[4] = 16; sOptionLimits[5] = 16; sOptionLimits[6] = 2;
            sOptionLimits[7] = 3; sOptionLimits[8] = 4; sOptionLimits[9] = 1201; sOptionLimits[10] = 7;
            for (int i = 11; i < 16; i++) sOptionLimits[i] = 2;
        }
        int i;
        for (i = 0; i < 16; i++) this->mOptions[i] = 0;
        for (i = 0; i < 8; i++) this->mSlots[i].mIndex = i;
        this->mHasFile = false;
    }
    void Free()
    {
        for (int i = 7; i >= 0; i--) {
            this->mSlots[i].ClearPlayer();
            this->mSlots[i].mName = 0;
        }
        this->mFileTransfer = 0;
        for (size_t i = 0; i < this->mFiles.size(); i++)
            if (this->mFiles[i] != 0) O(Op::Release, this->mFiles[i]);
        this->mFiles.clear();
    }

    int GetNumFiles() { return (int)this->mFiles.size(); }
    intptr_t GetFile(int inIndex)
    {
        if (inIndex >= 0 && inIndex < (int)this->mFiles.size()) return this->mFiles[inIndex];
        return 0;
    }
    void SetGameFile(intptr_t inName, unsigned long inSize, unsigned long inChecksum, bool inFlag)
    {
        if (this->mFileTransfer.mPtr == 0) this->mFileTransfer = O(Op::TransferNew);
        O(Op::TransferSetFileInfo, this->mFileTransfer.mPtr, inName, (intptr_t)inSize, (intptr_t)inChecksum);
        O(Op::TransferSetSavesPath, this->mFileTransfer.mPtr);
        if (O(Op::IsHost) && inFlag) {
            intptr_t aPort = O(Op::FileTransferPort);
            O(Op::TransferServe, this->mFileTransfer.mPtr, aPort);
        }
    }
    intptr_t GetGameFileName()
    {
        if (this->mFileTransfer.mPtr != 0) return O(Op::TransferName, this->mFileTransfer.mPtr);
        return 0;
    }
    void WriteGameFile(intptr_t outBuffer)
    {
        O(Op::AppendWString, outBuffer, this->GetGameFileName());
        O(Op::AppendLong, outBuffer, O(Op::TransferSize, this->mFileTransfer.mPtr));
        O(Op::AppendLong, outBuffer, O(Op::TransferChecksum, this->mFileTransfer.mPtr));
        O(Op::AppendShort, outBuffer, O(Op::FileTransferPort));
    }
    void ReadGameFile(intptr_t inBuffer)
    {
        Held theName(O(Op::ReadWString, inBuffer));
        unsigned long theSize = (uint32_t)(int32_t)O(Op::ReadLong, inBuffer);
        unsigned long theChecksum = (uint32_t)(int32_t)O(Op::ReadLong, inBuffer);
        unsigned short thePort = (unsigned short)O(Op::ReadShort, inBuffer);
        this->SetGameFile(theName.mPtr, theSize, theChecksum, false);
        O(Op::TransferSetHost, this->mFileTransfer.mPtr, thePort);
    }
    void StopTransfer()
    {
        if (this->mFileTransfer.mPtr != 0) O(Op::TransferStop, this->mFileTransfer.mPtr);
    }
    void UpdateTransferProgress(int inDone, int inTotal)
    {
        if (inDone == inTotal) {
            this->mHasFile = true;
            this->SetTransferPercent(100);
            O(Op::NotifyStagingChange);
        } else {
            double aValue = (double)inDone / inTotal * 100.0;
            // (int) of a double: x86's cvttsd2si (INT_MIN for NaN and out of range), as the original ran it
            int aInt = (aValue != aValue || aValue >= 2147483648.0 || aValue <= -2147483649.0) ? INT32_MIN : (int)aValue;
            this->SetTransferPercent((unsigned char)aInt);
        }
    }
    void SetTransferPercent(unsigned char inPercent) { this->ChangePlayerOption(this->GetMySlot(), 5, inPercent); }
    void StartTransfer()
    {
        if (O(Op::IsHost)) return;
        O(Op::TransferCloseClients, this->mFileTransfer.mPtr);
        O(Op::TransferSetSavesPath, this->mFileTransfer.mPtr);
        this->mHasFile = O(Op::TransferIsFileHere, this->mFileTransfer.mPtr) != 0;
        O(Op::NotifyStagingChange);
        if (!this->mHasFile) {
            this->SetTransferPercent(0);
            O(Op::TransferDownload, this->mFileTransfer.mPtr);
        } else
            this->SetTransferPercent(100);
    }
    bool CanReadyHook()
    {
        if (this->HasGameFile() && !this->mHasFile) return false;
        return true;
    }
    bool CanStartGameHook()
    {
        if (this->mNumHumanPlayers > 0 && this->mNumHumanPlayers + this->mNumComputerSlots > 1 && this->mNumReservedSlots == 0)
            return true;
        return false;
    }
    bool TryStartGameHook()
    {
        bool theDifferent = false;
        unsigned char theTeam = 0xff;
        for (int i = 0; i < 8 && !theDifferent; i++) {
            OrigSlot* theSlot = this->GetPlayerSlot(i);
            if (theSlot != NULL) {
                unsigned char theState = theSlot->mOptions[0];
                if ((theState == 3 && theSlot->mOptions[4] == 0) || theState == 1) {
                    if (theTeam == 0xff) theTeam = theSlot->mOptions[1];
                    else theDifferent = theTeam != theSlot->mOptions[1];
                }
            }
        }
        if (!theDifferent && theTeam != 0) {
            O(Op::SameTeamBox);
            return false;
        }
        this->StopTransfer();
        return O(Op::HostGame) != 0;
    }
    void ResetSlots()
    {
        this->mNumComputerSlots = 0;
        this->mNumReservedSlots = 0;
        O(Op::SetMaxPlayers, 8);
        this->mNumHumanPlayers = 1;
        for (int i = 0; i < 8; i++) {
            this->mSlots[i].SetOpen();
            this->mSlots[i].mOptions[1] = 0;
            this->mSlots[i].mOptions[3] = 5;
        }
    }
    void CountPlayers(bool inNotify)
    {
        if (!O(Op::IAmCaptain)) return;
        int theOpen = 8;
        this->mNumHumanPlayers = 0;
        this->mNumComputerSlots = 0;
        this->mNumReservedSlots = 0;
        for (int i = 0; i < 8; i++) {
            int theState = this->mSlots[i].mOptions[0];
            if (theState == 1 || theState == 2) theOpen--;
            if (theState == 3 && this->mSlots[i].mOptions[4] == 0) this->mNumHumanPlayers++;
            if (theState == 1) this->mNumComputerSlots++;
            if (theState == 4 && this->mSlots[i].mOptions[4] == 0) this->mNumReservedSlots++;
        }
        if (theOpen != O(Op::MaxPlayers)) {
            O(Op::SetMaxPlayers, theOpen);
            if (inNotify) O(Op::AdvertiseGame);
        }
        O(Op::NotifyStagingChange);
    }
    OrigSlot* GetPlayerSlot(int inSlot)
    {
        if (inSlot >= 0 && inSlot < 8) return &this->mSlots[inSlot];
        return NULL;
    }
    bool HasGameFile() { return this->GetOption(0) != 0; }
    long GetOption(int inOption)
    {
        if (inOption >= 0 && inOption < 16) return this->mOptions[inOption];
        return 0;
    }
    void WriteOption(intptr_t outBuffer, int inOption)
    {
        switch (TypeOf(inOption)) {
        case 2: O(Op::AppendShort, outBuffer, (short)this->mOptions[inOption]); break;
        case 1: O(Op::AppendByte, outBuffer, (unsigned char)this->mOptions[inOption]); break;
        case 0: O(Op::AppendBool, outBuffer, this->mOptions[inOption] != 0); break;
        }
    }
    void ReadOption(intptr_t inBuffer, int inOption)
    {
        long theValue;
        switch (TypeOf(inOption)) {
        case 2: theValue = (short)O(Op::ReadShort, inBuffer); break;
        case 1: theValue = (signed char)O(Op::ReadByte, inBuffer); break;
        case 0: theValue = O(Op::ReadBool, inBuffer) ? 1 : 0; break;
        default: return;
        }
        this->SetOption(inOption, (uint32_t)theValue);
    }
    void HandleHostGameHook(intptr_t inClient)
    {
        this->mFileTransfer = O(Op::TransferNew);
        this->mOptions[0] = 0; this->mOptions[1] = 1; this->mOptions[2] = 0; this->mOptions[3] = 0;
        this->mOptions[4] = 0; this->mOptions[5] = 0xe; this->mOptions[6] = 1; this->mOptions[7] = 1;
        this->mOptions[8] = 1; this->mOptions[9] = 0x1f4; this->mOptions[10] = 3; this->mOptions[11] = 0;
        this->mOptions[12] = 1; this->mOptions[13] = 0; this->mOptions[14] = 0; this->mOptions[15] = 1;
        O(Op::SetDefaultOptions);
        this->ResetSlots();
        this->mSlots[0].SetPlayer(inClient, O(Op::ClientPlayer, inClient));
    }
    void GetTakenColors(bool* outTaken, int inExcept)
    {
        int i;
        for (i = 0; i < 16; i++) outTaken[i] = false;
        for (i = 0; i < 8; i++) {
            if (i != inExcept) {
                OrigSlot& theSlot = this->mSlots[i];
                if ((theSlot.mOptions[0] == 3 || theSlot.mOptions[0] == 4) && theSlot.mOptions[4] == 0 && theSlot.mOptions[2] < 16)
                    outTaken[theSlot.mOptions[2]] = true;
            }
        }
    }
    bool IsColorFree(unsigned char inColor)
    {
        bool theTaken[16];
        this->GetTakenColors(theTaken, -1);
        if (inColor < 16) return !theTaken[inColor];
        return false;
    }
    void WriteSummaryHook(intptr_t outBuffer)
    {
        O(Op::AppendByte, outBuffer, (unsigned char)this->GetOption(4));
        O(Op::AppendByte, outBuffer, (unsigned char)this->GetOption(0));
        O(Op::AppendByte, outBuffer, (unsigned char)this->GetOption(2));
    }
    bool ReadSummaryHook(intptr_t inBuffer)
    {
        this->SetOption(4, (uint32_t)(long)(signed char)O(Op::ReadByte, inBuffer));
        this->SetOption(0, (uint32_t)(long)(signed char)O(Op::ReadByte, inBuffer));
        this->SetOption(2, (uint32_t)(long)(signed char)O(Op::ReadByte, inBuffer));
        return true;
    }
    void WriteDetailsHook(intptr_t outBuffer)
    {
        int i;
        for (i = 0; i < 16; i++) this->WriteOption(outBuffer, i);
        unsigned char theCount = 0;
        intptr_t thePos = O(Op::Length, outBuffer);
        O(Op::AppendByte, outBuffer, 0);
        for (i = 0; i < 8; i++) {
            OrigSlot* theSlot = this->GetPlayerSlot(i);
            if (theSlot != NULL && theSlot->mClient.mPtr != 0) {
                theCount++;
                if (O(Op::GameType) == 2) O(Op::AppendShort, outBuffer, O(Op::ClientId, theSlot->mClient.mPtr));
                else O(Op::AppendWString, outBuffer, O(Op::ClientName, theSlot->mClient.mPtr));
            }
        }
        O(Op::SetByte, outBuffer, thePos, theCount);
    }
    bool ReadDetailsHook(intptr_t inBuffer)
    {
        for (size_t k = 0; k < this->mFiles.size(); k++)
            if (this->mFiles[k] != 0) O(Op::Release, this->mFiles[k]);
        this->mFiles.clear();
        int i;
        for (i = 0; i < 16; i++) this->ReadOption(inBuffer, i);
        if ((int)O(Op::Available, inBuffer) > 0) {
            unsigned char theCount = (unsigned char)O(Op::ReadByte, inBuffer);
            intptr_t theClients = O(Op::CurrentServer);
            if (theClients == 0 && O(Op::GameType) == 2) return true;
            for (i = theCount; i > 0; i--) {
                if (O(Op::GameType) == 2) {
                    unsigned short theId = (unsigned short)O(Op::ReadShort, inBuffer);
                    intptr_t theClient = O(Op::ServerClient, theClients, theId);
                    if (theClient != 0) {
                        intptr_t aName = O(Op::ClientName, theClient);
                        if (aName != 0) O(Op::AddRef, aName);
                        this->mFiles.push_back(aName);
                    }
                } else {
                    intptr_t theString = O(Op::ReadWString, inBuffer); // (the GUIString made of the wide string, moved in)
                    this->mFiles.push_back(theString);
                }
            }
        }
        return true;
    }
    unsigned char GetFreeColor(int inExcept, unsigned char inColor)
    {
        bool theTaken[16];
        this->GetTakenColors(theTaken, inExcept);
        if (inColor < 16 && !theTaken[inColor]) return inColor;
        for (int i = 0; i < 16; i++)
            if (!theTaken[i]) return i;
        return 0;
    }
    void MakeColorsDistinct()
    {
        for (int i = 0; i < 8; i++) {
            OrigSlot* theSlot = this->GetPlayerSlot(i);
            if (theSlot != NULL && (theSlot->mOptions[0] == 3 || theSlot->mOptions[0] == 4) && theSlot->mOptions[4] == 0)
                theSlot->mOptions[2] = this->GetFreeColor(i, theSlot->mOptions[2]);
        }
    }
    int GetStatusStringHook(short inStatus)
    {
        switch (inStatus) {
        case 10000: return 1;
        case 10001: return 2;
        case 10002: return 3;
        }
        return 0;
    }
    void GetJoinGameRequestHook(intptr_t outBuffer)
    {
        uint32_t theChecksum = 0;
        Held theVersion(O(Op::Version, (intptr_t)&theChecksum));
        O(Op::AppendWString, outBuffer, theVersion.mPtr);
        O(Op::AppendLong, outBuffer, (int32_t)theChecksum);
    }
    short HandleJoinGameRequestHook(intptr_t inBuffer, intptr_t inClient)
    {
        uint32_t theChecksum = 0;
        Held theVersion(O(Op::Version, (intptr_t)&theChecksum));
        Held theText(O(Op::ReadWString, inBuffer));
        if (O(Op::StringsEqual, theVersion.mPtr, theText.mPtr) == 0) return 10001;
        if ((uint32_t)(int32_t)O(Op::ReadLong, inBuffer) != theChecksum) return 10002;
        bool theSaved = this->HasGameFile();
        int i;
        for (i = 0; i < 8; i++) {
            if (!theSaved) {
                if (this->mSlots[i].mOptions[0] == 0) break;
            } else if (this->mSlots[i].mOptions[0] == 4) {
                intptr_t aName = O(Op::ClientName, inClient);
                intptr_t aSlotName = this->mSlots[i].GetName();
                if (O(Op::StringsEqual, aName, aSlotName) != 0) break;
            }
        }
        if (i >= 8) return theSaved ? 10000 : 2;
        this->mSlots[i].SetPlayer(inClient, O(Op::ClientPlayer, inClient));
        if (!theSaved) this->mSlots[i].Reset();
        return 0;
    }
    void GetJoinGameReplyHook(intptr_t, intptr_t outBuffer)
    {
        int i;
        for (i = 0; i < 16; i++) this->WriteOption(outBuffer, i);
        for (i = 0; i < 8; i++) this->mSlots[i].WriteData(outBuffer);
        if (this->HasGameFile()) this->WriteGameFile(outBuffer);
    }
    int SeatClient(intptr_t inClient)
    {
        if (inClient != 0) {
            intptr_t thePlayer = O(Op::ClientPlayer, inClient);
            if (thePlayer != 0) {
                unsigned char theSlot = (unsigned char)O(Op::PlayerSlot, thePlayer);
                if (theSlot < 8) {
                    this->mSlots[theSlot].SetPlayer(inClient, thePlayer);
                    return theSlot;
                }
            }
        }
        return -1;
    }
    void HandleJoinGameReplyHook(intptr_t inBuffer)
    {
        this->mFileTransfer = O(Op::TransferNew);
        int i;
        for (i = 0; i < 16; i++) this->ReadOption(inBuffer, i);
        for (i = 0; i < 8; i++) this->mSlots[i].ReadData(inBuffer);
        if (this->HasGameFile()) this->ReadGameFile(inBuffer);
        intptr_t theIterator = O(Op::IterNew, O(Op::Clients));
        while (O(Op::IterHasMore, theIterator)) {
            intptr_t theClient = O(Op::IterNext, theIterator);
            this->SeatClient(theClient);
        }
        O(Op::IterFree, theIterator);
        OrigSlot* theSlot = this->GetPlayerSlot(this->GetMySlot());
        if (theSlot != NULL) theSlot->mOptions[2] = this->GetFreeColor(theSlot->mIndex, 0);
        if (this->HasGameFile()) this->StartTransfer();
    }
    void HandlePlayerJoinedHook(intptr_t inClient)
    {
        int theSlot = this->SeatClient(inClient);
        if (theSlot < 0) return;
        OrigSlot* thePlayerSlot = this->GetPlayerSlot(theSlot);
        if (thePlayerSlot != NULL && !this->HasGameFile()) {
            thePlayerSlot->Reset();
            thePlayerSlot->mOptions[2] = this->GetFreeColor(thePlayerSlot->mIndex, 0);
        }
        this->CountPlayers(false);
        O(Op::SyncPlayer, theSlot);
    }
    void HandlePlayerLeftHook(intptr_t inClient, intptr_t inPlayer)
    {
        unsigned char theSlot = (unsigned char)O(Op::PlayerSlot, inPlayer);
        if (theSlot >= 8 || this->mSlots[theSlot].mClient.mPtr != inClient) return;
        if (this->HasGameFile()) {
            this->mSlots[theSlot].SetName(O(Op::ClientName, inClient));
        } else {
            this->mSlots[theSlot].SetOpen();
            this->mSlots[theSlot].Reset();
        }
        this->CountPlayers(false);
        O(Op::SyncPlayer, theSlot);
        if (O(Op::IAmCaptain)) this->BroadcastConnectStatus(theSlot, false);
    }
    void HandleGameMessageHook(unsigned char inType, intptr_t inBuffer, intptr_t inFrom)
    {
        switch (inType) {
        case 0x64: this->HandleOptionChange(inBuffer, inFrom); break;
        case 0x65: this->HandlePlayerOptionChange(inBuffer, inFrom); break;
        case 0x66: this->HandlePlayerOptionRequest(inBuffer, inFrom); break;
        case 0x67: this->HandleGameState(inBuffer, inFrom); break;
        case 0x68: this->HandleConnectStatus(inBuffer, inFrom, true); break;
        case 0x69: this->HandleConnectStatus(inBuffer, inFrom, false); break;
        case 0x6a: this->HandleCancelConnect(inBuffer, inFrom); break;
        }
    }
    void HandleCancelConnect(intptr_t, intptr_t inFrom)
    {
        if (O(Op::ClientIsCaptain, inFrom)) O(Op::CancelConnectDialog);
    }
    void HandleConnectStatus(intptr_t inBuffer, intptr_t inFrom, bool inFromClient)
    {
        unsigned char theSlot = (unsigned char)O(Op::ReadByte, inBuffer);
        bool theFlag = O(Op::ReadBool, inBuffer) != 0;
        if (inFromClient) {
            OrigSlot* thePlayerSlot = this->GetPlayerSlot(theSlot);
            if (thePlayerSlot != NULL && thePlayerSlot->mClient.mPtr == inFrom)
                this->BroadcastConnectStatus(theSlot, theFlag);
        } else if (O(Op::ClientIsCaptain, inFrom)) {
            O(Op::SetSlotConnected, theSlot, theFlag);
        }
    }
    void BroadcastConnectStatus(unsigned char inSlot, bool inConnected)
    {
        if (!O(Op::ConnectDialogRunning)) return;
        intptr_t theBuffer = O(Op::NewBuffer);
        O(Op::AppendByte, theBuffer, 0x69);
        O(Op::AppendByte, theBuffer, inSlot);
        O(Op::AppendBool, theBuffer, inConnected);
        O(Op::BroadcastToGame, theBuffer);
        O(Op::FreeBuffer, theBuffer);
    }
    void SendConnectStatus(bool inConnected)
    {
        intptr_t theBuffer = O(Op::NewBuffer);
        O(Op::AppendByte, theBuffer, 0x68);
        O(Op::AppendByte, theBuffer, (unsigned char)this->GetMySlot());
        O(Op::AppendBool, theBuffer, inConnected);
        O(Op::SendToCaptain, theBuffer);
        O(Op::FreeBuffer, theBuffer);
    }
    void SendCancelConnect()
    {
        intptr_t theBuffer = O(Op::NewBuffer);
        O(Op::AppendByte, theBuffer, 0x6a);
        O(Op::BroadcastToGame, theBuffer);
        O(Op::FreeBuffer, theBuffer);
    }
    void HandlePlayerOptionRequest(intptr_t inBuffer, intptr_t inFrom)
    {
        if (!O(Op::IsHost)) return;
        intptr_t thePlayer = O(Op::ClientPlayer, inFrom);
        if (thePlayer == 0) return;
        int theIndex = (unsigned char)O(Op::PlayerSlot, thePlayer);
        OrigSlot* theSlot = this->GetPlayerSlot(theIndex);
        if (theSlot == NULL || theSlot->mClient.mPtr != inFrom) return;
        int theWhat = (unsigned char)O(Op::ReadByte, inBuffer);
        unsigned char theValue = (unsigned char)O(Op::ReadByte, inBuffer);
        if ((unsigned char)O(Op::PlayerSlot, thePlayer) != theIndex) return;
        bool theOK = false;
        switch (theWhat) {
        case 1: case 4: case 5: theOK = true; break;
        case 2: theOK = this->IsColorFree(theValue); break;
        }
        intptr_t theBuffer = O(Op::NewBuffer);
        O(Op::AppendByte, theBuffer, 0x65);
        O(Op::AppendByte, theBuffer, (unsigned char)theIndex);
        O(Op::AppendByte, theBuffer, (unsigned char)theWhat);
        O(Op::AppendByte, theBuffer, theOK ? theValue : theSlot->GetOption(theWhat));
        if (theOK) O(Op::BroadcastToGame, theBuffer);
        else O(Op::SendToClient, theBuffer, inFrom);
        O(Op::FreeBuffer, theBuffer);
    }
    void HandlePlayerOptionChange(intptr_t inBuffer, intptr_t inFrom)
    {
        if (!O(Op::ClientIsCaptain, inFrom)) return;
        int theIndex = (signed char)O(Op::ReadByte, inBuffer);
        int theWhat = (unsigned char)O(Op::ReadByte, inBuffer);
        unsigned char theValue = (unsigned char)O(Op::ReadByte, inBuffer);
        OrigSlot* theSlot = this->GetPlayerSlot(theIndex);
        if (theSlot != NULL) {
            if (theWhat == 1) {
                if (theSlot->mClient.mPtr != 0 && theIndex != this->GetMySlot()) {
                    unsigned char theOld = theSlot->mOptions[1];
                    unsigned char theTeam = (unsigned char)this->GetMyTeam();
                    if (theTeam != 0 && theOld != theValue) {
                        if (theOld == theTeam) O(Op::TeamChanged, theSlot->mClient.mPtr, 1);
                        else if (theValue == theTeam) O(Op::TeamChanged, theSlot->mClient.mPtr, 0);
                    }
                }
            }
            if (theWhat == 4 && theValue == 0) {
                unsigned char theColor = theSlot->mOptions[2];
                theSlot->mOptions[2] = this->GetFreeColor(theSlot->mIndex, theColor);
            }
            theSlot->SetOption(theWhat, theValue);
            if (theWhat == 0 || theWhat == 4) this->CountPlayers(true);
        }
        O(Op::SyncPlayer, theIndex);
        if (theWhat == 3) O(Op::ResetReadyFlags);
    }
    void HandleOptionChange(intptr_t inBuffer, intptr_t inFrom)
    {
        if (!O(Op::ClientIsCaptain, inFrom)) return;
        int theOption = (unsigned char)O(Op::ReadByte, inBuffer);
        this->ReadOption(inBuffer, theOption);
        O(Op::SyncOption, theOption);
        O(Op::ResetReadyFlags);
    }
    void SendOption(int inOption)
    {
        intptr_t theBuffer = O(Op::NewBuffer);
        O(Op::AppendByte, theBuffer, 0x64);
        O(Op::AppendByte, theBuffer, (unsigned char)inOption);
        this->WriteOption(theBuffer, inOption);
        O(Op::BroadcastToGame, theBuffer);
        O(Op::FreeBuffer, theBuffer);
    }
    void SetOption(int inOption, uint32_t inValue)
    {
        if (inOption >= 0 && inOption < 16 && inValue < sOptionLimits[inOption]) this->mOptions[inOption] = inValue;
    }
    void ChangeOption(int inOption, uint32_t inValue, bool inFlag)
    {
        if (inOption < 0 || inOption >= 16) return;
        if (inValue >= sOptionLimits[inOption]) {
            O(Op::SyncOption, inOption);
            return;
        }
        if ((uint32_t)this->mOptions[inOption] == inValue) return;
        this->mOptions[inOption] = inValue;
        if (inFlag) this->SendOption(inOption);
        if (inOption == 4) {
            if ((uint32_t)this->mOptions[5] < (uint32_t)this->mOptions[4] && this->mOptions[4] != 0xf)
                this->ChangeOption(5, this->mOptions[4], inFlag);
        } else if (inOption == 5) {
            if ((uint32_t)this->mOptions[5] < (uint32_t)this->mOptions[4] && this->mOptions[4] != 0xf)
                this->ChangeOption(4, this->mOptions[5], inFlag);
            return;
        } else if (inOption != 2) {
            return;
        }
        O(Op::AdvertiseGame);
    }
    int GetMySlot()
    {
        intptr_t thePlayer = O(Op::MyPlayer);
        if (thePlayer != 0) return (unsigned char)O(Op::PlayerSlot, thePlayer);
        return -1;
    }
    int GetMyTeam()
    {
        intptr_t thePlayer = O(Op::MyPlayer);
        if (thePlayer != 0) return (unsigned char)O(Op::PlayerTeam, thePlayer);
        return -1;
    }
    void HandleGameState(intptr_t inBuffer, intptr_t inFrom)
    {
        if (!O(Op::ClientIsCaptain, inFrom)) return;
        int i;
        for (i = 0; i < 16; i++) this->ReadOption(inBuffer, i);
        for (i = 0; i < 8; i++) {
            this->mSlots[i].ReadData(inBuffer);
            if (this->mSlots[i].mOptions[0] == 3) {
                unsigned short anId = (unsigned short)O(Op::ReadShort, inBuffer);
                intptr_t theClient = O(Op::FindClient, O(Op::Clients), anId);
                if (theClient != 0 && O(Op::ClientPlayer, theClient) != 0)
                    this->mSlots[i].SetPlayer(theClient, O(Op::ClientPlayer, theClient));
                else
                    this->mSlots[i].SetOpen();
            }
        }
        if (this->HasGameFile()) {
            this->ReadGameFile(inBuffer);
            this->StartTransfer();
            this->MakeColorsDistinct();
        }
        O(Op::LobbyEvent, 100065);
        O(Op::ResetReadyFlags);
        if (O(Op::IsHost)) {
            this->CountPlayers(false);
            O(Op::AdvertiseGame);
        }
    }
    void SendGameState()
    {
        intptr_t theBuffer = O(Op::NewBuffer);
        O(Op::AppendByte, theBuffer, 0x67);
        int i;
        for (i = 0; i < 16; i++) this->WriteOption(theBuffer, i);
        for (i = 0; i < 8; i++) {
            this->mSlots[i].WriteData(theBuffer);
            if (this->mSlots[i].mOptions[0] == 3) O(Op::AppendShort, theBuffer, O(Op::ClientId, this->mSlots[i].mClient.mPtr));
        }
        if (this->HasGameFile()) this->WriteGameFile(theBuffer);
        O(Op::BroadcastToGame, theBuffer);
        O(Op::FreeBuffer, theBuffer);
    }
    void ChangePlayerOption(int inSlot, int inWhat, unsigned char inValue)
    {
        OrigSlot* theSlot = this->GetPlayerSlot(inSlot);
        if (theSlot == NULL || theSlot->GetOption(inWhat) == inValue) return;
        bool theToHost = false;
        bool theOK = true;
        switch (inWhat) {
        case 1: case 2: case 4: case 5:
            theOK = inSlot == this->GetMySlot() || (O(Op::IsHost) && theSlot->mOptions[0] != 3);
            theToHost = true;
            break;
        case 0:
            if (O(Op::IsHost) && inSlot != 0) O(Op::KickClient, theSlot->mClient.mPtr, 0);
            else theOK = false;
        default:
            if (!O(Op::IsHost)) theOK = false;
            break;
        }
        if (inWhat == 2 && theOK) theOK = this->IsColorFree(inValue);
        if (!theOK) {
            O(Op::SyncPlayer, inSlot);
            return;
        }
        if (O(Op::IsHost)) theToHost = false;
        intptr_t theBuffer = O(Op::NewBuffer);
        if (theToHost) {
            O(Op::AppendByte, theBuffer, 0x66);
            O(Op::AppendByte, theBuffer, (unsigned char)inWhat);
            O(Op::AppendByte, theBuffer, inValue);
            O(Op::SendToCaptain, theBuffer);
        } else {
            O(Op::AppendByte, theBuffer, 0x65);
            O(Op::AppendByte, theBuffer, (unsigned char)inSlot);
            O(Op::AppendByte, theBuffer, (unsigned char)inWhat);
            O(Op::AppendByte, theBuffer, inValue);
            O(Op::BroadcastToGame, theBuffer);
        }
        O(Op::FreeBuffer, theBuffer);
    }
};

static OrigGame sO;

static intptr_t SlotCall(OrigSlot* s, Call inCall, intptr_t b, intptr_t c)
{
    switch (inCall) {
    case Call::SlotReset: s->Reset(); return 0;
    case Call::SlotGetName: return s->GetName();
    case Call::SlotSetState: s->SetState((unsigned char)b); return 0;
    case Call::SlotGetOption: return s->GetOption((int)b);
    case Call::SlotSetOption: s->SetOption((int)b, (unsigned char)c); return 0;
    case Call::SlotClearPlayer: s->ClearPlayer(); return 0;
    case Call::SlotSetOpen: s->SetOpen(); return 0;
    case Call::SlotSetClosed: s->SetClosed(); return 0;
    case Call::SlotSetComputer: s->SetComputer(); return 0;
    case Call::SlotSetName: s->SetName(b); return 0;
    case Call::SlotSetPlayer: s->SetPlayer(b, c); return 0;
    case Call::SlotRawOption: return s->GetOption((int)b);
    case Call::SlotSetRawOption: if (b >= 0 && b < 6) s->mOptions[b] = (unsigned char)c; return 0;
    case Call::SlotClient: return s->mClient.mPtr;
    case Call::SlotPlayer: return s->mPlayer.mPtr;
    default: return 0;
    }
}

static intptr_t OrigCall(Call inCall, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    OrigGame& g = sO;
    try {
        switch (inCall) {
        case Call::GetNumFiles: return g.GetNumFiles();
        case Call::GetFile: return g.GetFile((int)a);
        case Call::SetGameFile: g.SetGameFile(a, (uint32_t)b, (uint32_t)c, d != 0); return 0;
        case Call::GetGameFileName: return g.GetGameFileName();
        case Call::StopTransfer: g.StopTransfer(); return 0;
        case Call::UpdateTransferProgress: g.UpdateTransferProgress((int)a, (int)b); return 0;
        case Call::SetTransferPercent: g.SetTransferPercent((unsigned char)a); return 0;
        case Call::StartTransfer: g.StartTransfer(); return 0;
        case Call::CanReadyHook: return g.CanReadyHook();
        case Call::CanStartGameHook: return g.CanStartGameHook();
        case Call::TryStartGameHook: return g.TryStartGameHook();
        case Call::PostNetLeaveGameHook: case Call::NotifyStartGameHook: g.StopTransfer(); return 0;
        case Call::HandleHostGameHook: g.HandleHostGameHook(a); return 0;
        case Call::GetJoinGameRequestHook: g.GetJoinGameRequestHook(a); return 0;
        case Call::HandleJoinGameRequestHook: return g.HandleJoinGameRequestHook(a, b);
        case Call::GetJoinGameReplyHook: g.GetJoinGameReplyHook(a, b); return 0;
        case Call::HandleJoinGameReplyHook: g.HandleJoinGameReplyHook(a); return 0;
        case Call::HandlePlayerLeftHook: g.HandlePlayerLeftHook(a, b); return 0;
        case Call::HandlePlayerJoinedHook: g.HandlePlayerJoinedHook(a); return 0;
        case Call::WriteSummaryHook: g.WriteSummaryHook(a); return 0;
        case Call::WriteDetailsHook: g.WriteDetailsHook(a); return 0;
        case Call::ReadSummaryHook: return g.ReadSummaryHook(a);
        case Call::ReadDetailsHook: return g.ReadDetailsHook(a);
        case Call::HandleGameMessageHook: g.HandleGameMessageHook((unsigned char)a, b, c); return 0;
        case Call::GetStatusStringHook: return g.GetStatusStringHook((short)a);
        case Call::ResetSlots: g.ResetSlots(); return 0;
        case Call::CountPlayers: g.CountPlayers(a != 0); return 0;
        case Call::HasGameFile: return g.HasGameFile();
        case Call::GetOption: return g.GetOption((int)a);
        case Call::SetOption: g.SetOption((int)a, (uint32_t)b); return 0;
        case Call::ChangeOption: g.ChangeOption((int)a, (uint32_t)b, c != 0); return 0;
        case Call::SendGameState: g.SendGameState(); return 0;
        case Call::ChangePlayerOption: g.ChangePlayerOption((int)a, (int)b, (unsigned char)c); return 0;
        case Call::SendConnectStatus: g.SendConnectStatus(a != 0); return 0;
        case Call::SendCancelConnect: g.SendCancelConnect(); return 0;
        case Call::SendOption: g.SendOption((int)a); return 0;
        case Call::GetFreeColor: return g.GetFreeColor((int)a, (unsigned char)b);
        case Call::MakeColorsDistinct: g.MakeColorsDistinct(); return 0;
        case Call::GetMySlot: return g.GetMySlot();
        case Call::GetMyTeam: return g.GetMyTeam();
        case Call::GetTakenColors: g.GetTakenColors(reinterpret_cast<bool*>(a), (int)b); return 0;
        case Call::IsColorFree: return g.IsColorFree((unsigned char)a);
        case Call::Field:
            switch (a) {
            case 0: return g.mFileTransfer.mPtr;
            case 1: return g.mHasFile;
            case 2: return g.mNumHumanPlayers;
            case 3: return g.mNumComputerSlots;
            case 4: return g.mNumReservedSlots;
            }
            return 0;
        default: {
            OrigSlot* s = g.GetPlayerSlot((int)a);
            if (s == NULL) return 0;
            return SlotCall(s, inCall, b, c);
        }
        }
    } catch (ReadThrew&) {
        return INTPTR_MIN;
    }
}

// ---- the sequences

struct Step { Call mCall; intptr_t a, b, c, d; };

static intptr_t Client() { return R(6) ? kClient + R(4) : 0; }

static Step MakeStep()
{
    // a quarter of the calls are the game room's messages (their paths are the longest)
    Step s = {R(4) ? (Call)(1 + R(kTypes - 1)) : Call::HandleGameMessageHook, 0, 0, 0, 0};
    switch (s.mCall) {
    case Call::GetFile: s.a = (intptr_t)R(5) - 1; break;
    case Call::SetGameFile: s.a = R(5) ? kName + R(4) : 0; s.b = R(1000); s.c = R(4) ? R(1000) : 0x80000000u; s.d = R(2); break;
    case Call::UpdateTransferProgress: s.a = R(5); s.b = R(5); break;
    case Call::SetTransferPercent: s.a = R(120); break;
    case Call::HandleHostGameHook: case Call::HandlePlayerJoinedHook: s.a = Client(); break;
    case Call::GetJoinGameRequestHook: case Call::WriteSummaryHook: case Call::WriteDetailsHook: s.a = kWrite; break;
    case Call::HandleJoinGameRequestHook: s.a = kRead; s.b = kClient + R(4); break;
    case Call::GetJoinGameReplyHook: s.a = Client(); s.b = kWrite; break;
    case Call::HandleJoinGameReplyHook: case Call::ReadSummaryHook: case Call::ReadDetailsHook: s.a = kRead; break;
    case Call::HandlePlayerLeftHook: s.a = Client(); s.b = kPlayer + R(4); break;
    case Call::HandleGameMessageHook: s.a = R(6) ? 0x64 + R(7) : R(256); s.b = kRead; s.c = kClient + R(4); break;
    case Call::GetStatusStringHook: s.a = R(2) ? 10000 + R(4) : R(5); break;
    case Call::CountPlayers: case Call::SendConnectStatus: s.a = R(2); break;
    case Call::GetOption: s.a = (intptr_t)R(18) - 1; break;
    case Call::SetOption: s.a = (intptr_t)R(18) - 1; s.b = R(4) ? R(17) : (R(2) ? R(1300) : 0xffffffffu); break;
    case Call::ChangeOption: s.a = (intptr_t)R(18) - 1; s.b = R(4) ? R(17) : R(1300); s.c = R(2); break;
    case Call::ChangePlayerOption: s.a = (intptr_t)R(10) - 1; s.b = (intptr_t)R(8) - 1; s.c = R(4) ? R(6) : R(20); break;
    case Call::SendOption: s.a = R(16); break;
    case Call::GetFreeColor: s.a = (intptr_t)R(10) - 1; s.b = R(20); break;
    case Call::GetTakenColors: s.a = 1; s.b = (intptr_t)R(10) - 1; break;  // a: the test's array
    case Call::IsColorFree: s.a = R(20); break;
    case Call::Field: s.a = R(6); break;
    default:
        if (s.mCall >= Call::SlotReset) {
            s.a = (intptr_t)R(10) - 1;
            s.b = (intptr_t)R(8) - 1;
            s.c = R(6);
            if (s.mCall == Call::SlotSetName) s.b = R(4) ? kName + R(4) : 0;
            if (s.mCall == Call::SlotSetPlayer) { s.b = Client(); s.c = R(5) ? kPlayer + R(4) : 0; }
            if (s.mCall == Call::SlotSetState) s.b = R(6);
        }
        break;
    }
    return s;
}

static std::vector<std::string> RunSide(bool inRust, unsigned inSeed, const std::vector<Step>& inSteps)
{
    sTrace.clear();
    sSeed = inSeed;
    sRust = inRust;
    sDepth = 0;
    sCounter = 0;
    Line("new");
    if (inRust) sRustGame = ee_won_eegame_new((void*)kGame, RustHost);
    else {
        memset(&sO.mNumHumanPlayers, 0, 3);
        sO.mFiles.clear();
        sO.mFileTransfer.mPtr = 0;
        for (int i = 0; i < 8; i++) { sO.mSlots[i].mClient.mPtr = 0; sO.mSlots[i].mPlayer.mPtr = 0; sO.mSlots[i].mName.mPtr = 0; }
        sO.Init();
    }
    for (size_t i = 0; i < inSteps.size(); i++) {
        Step s = inSteps[i];
        bool aTaken[16] = {};
        if (s.mCall == Call::GetTakenColors) s.a = (intptr_t)aTaken;
        Line("call %lld %lld %lld %lld %lld", (long long)s.mCall, s.mCall == Call::GetTakenColors ? 1 : s.a, s.b, s.c, s.d);
        intptr_t r = inRust ? ee_won_eegame_call(sRustGame, s.mCall, s.a, s.b, s.c, s.d) : OrigCall(s.mCall, s.a, s.b, s.c, s.d);
        Line("ret %lld", r);
        if (s.mCall == Call::GetTakenColors) {
            int aBits = 0;
            for (int k = 0; k < 16; k++) aBits |= aTaken[k] << k;
            Line("taken %lld", aBits);
        }
    }
    Line("free");
    if (inRust) ee_won_eegame_free(sRustGame);
    else sO.Free();
    return sTrace;
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    CHECK(ee_won_eegame_is_teammate(1, 1));
    CHECK(!ee_won_eegame_is_teammate(0, 0));
    CHECK(!ee_won_eegame_is_teammate(2, 3));
    unsigned long long aLines = 0;
    int aDiffer = 0;
    unsigned aStepSeed = 4711;
    const int kSequences = 60000;
    for (int n = 0; n < kSequences; n++) {
        unsigned aSave = sSeed;
        sSeed = aStepSeed;
        std::vector<Step> aSteps;
        // half of the games are hosted or joined first (the slots filled), some of them twice
        switch (R(4)) {
        case 0: aSteps.push_back(Step{Call::HandleHostGameHook, kClient + R(4), 0, 0, 0}); break;
        case 1: aSteps.push_back(Step{Call::HandleJoinGameReplyHook, kRead, 0, 0, 0}); break;
        default: break;
        }
        if (R(4) == 0) aSteps.push_back(Step{Call::HandleGameMessageHook, 0x67, kRead, kClient + R(4), 0});
        // a third seat two players on a team (the game room's messages act on seated slots)
        if (R(3) == 0)
            for (int k = 0; k < 2; k++) {
                intptr_t aSlot = R(8);
                aSteps.push_back(Step{Call::SlotSetPlayer, aSlot, kClient + R(4), kPlayer + R(4), 0});
                aSteps.push_back(Step{Call::SlotSetRawOption, aSlot, 1, R(4), 0});
            }
        int aCount = 1 + (int)R(12);
        for (int i = 0; i < aCount; i++) aSteps.push_back(MakeStep());
        aStepSeed = sSeed;
        sSeed = aSave;
        unsigned aSeed = 101u + (unsigned)n * 7919u;
        std::vector<std::string> a = RunSide(false, aSeed, aSteps);
        std::vector<std::string> b = RunSide(true, aSeed, aSteps);
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
            // an op or call by its number (a return value, a re-entry's or the colours are not new)
            size_t aSpace = a[i].find(' ');
            std::string aHead = a[i].substr(0, aSpace);
            std::string aKey = aHead == "ret" || aHead == "back" || aHead == "taken" ? aHead
                                                                                    : a[i].substr(0, a[i].find(' ', aSpace + 1));
            if (sSeen.insert(aKey).second) aNew = true;
        }
        if (aPrint && aNew) {
            printf("seq %d\n", n);
            for (size_t i = 0; i < a.size(); i++) printf("%s\n", a[i].c_str());
        }
    }
    if (aDiffer) { sFailures++; printf("FAIL: %d of %d sequences differ\n", aDiffer, kSequences); }
    if (!aPrint)
        printf("won_eegame_test: %d sequences, %llu trace lines, %zu distinct ops and calls; %d checks, %d failures\n",
               kSequences, aLines, sSeen.size(), sChecks, sFailures);
    return sFailures ? 1 : 0;
}
