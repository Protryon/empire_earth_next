// The EE lobby's saved game transfer and its sounds as Rust objects (R6 WONLobby round 7): the original
// EELobbyFileTransfer.cpp (EELobbyFileTransfer and SendFileOp) and EELobbySound.cpp (EELobbyMusic, EELobbySound,
// EELobbySoundDecoder's load), their statements copied from the C++ before the port with every call on a socket,
// an op, the lobby game, a file, the clock or the game's sound and UI managers replaced by the host op that makes it
// (EELobbyGameHost.cpp and EELobbyGlueHost.cpp run the same calls), against ee_won's eexfer.rs and eesound.rs over
// one scripted host.
//
// Each of 50,000 sequences makes a transfer, a send op (of a path or of none) and the two sounds, then makes 1 to 14
// random calls (SetPath, SetFileInfo, IsFileHere, Serve, Stop, CloseClients, Download, the four completions'
// callbacks, the members; the send op's Start, Continue and Cleanup; the sounds' hooks and the decoder's load) and
// frees them. The host answers every op from a seeded generator (the same stream for both sides); killing a client's
// socket runs its close completion half of the time (the original's set was not changed by a Kill: the Rust walks a
// copy). References are not traced (the original's SmartPtr temporaries take and give back ones the Rust does not
// take); each object's count is, at the end of the sequence. The traces must be the same.
//
// The first 60 sequences and every later one that makes an op or call none before made are
// rust/crates/ee_won_lobby/src/golden_eexfer.txt, which the Rust tests replay; `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <set>
#include <map>
#include "rust/won_lobby/eesound.h"
#include "rust/won_lobby/eexfer.h"

typedef EeWonEeXferOp Op;
typedef EeWonEeXferCall Call;
typedef EeWonEeXferSendCall SCall;
typedef EeWonEeSoundOp SOp;
typedef EeWonEeSoundCall SndCall;

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned sSeed;
static unsigned Rand(unsigned& ioSeed) { ioSeed = ioSeed * 1103515245u + 12345u; return (ioSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return Rand(sSeed) % n; }
static std::vector<std::string> sTrace;
static std::set<std::string> sSeen;
static std::map<intptr_t, int> sRefs;

static void Line(const char* inFormat, long long a = 0, long long b = 0, long long c = 0, long long d = 0)
{
    char aBuffer[300];
    snprintf(aBuffer, sizeof(aBuffer), inFormat, a, b, c, d);
    sTrace.push_back(aBuffer);
}

static const intptr_t kXfer = 0x7000, kSend = 0x7100, kGame = 0x3000, kFile = 0xF11E, kMusic = 0x5100, kEffect = 0x5200;
static bool sRust;
static EeWonEeXfer* sRustXfer;
static int sDepth;
static int sOps;
static intptr_t XferCall(Call inCall, intptr_t a, intptr_t b);

static intptr_t Socket() { return 0x100 * (1 + (intptr_t)R(5)); }

static intptr_t Host(void* inObj, Op inOp, intptr_t a, intptr_t b, intptr_t c)
{
    sOps++;
    bool aLate = sOps > 80; // the send loop ends: no more data, a failed send
    intptr_t r = 0;
    switch (inOp) {
    case Op::AddRef: sRefs[a]++; return 0;
    case Op::Release: sRefs[a]--; return 0;
    case Op::SavedGameInfo: {
        uint32_t* s = reinterpret_cast<uint32_t*>(b);
        uint32_t* k = reinterpret_cast<uint32_t*>(c);
        if (R(3)) { *s = R(4) ? 1000 + R(3) : 0xFFFFFFF0u; *k = R(3); }
        sTrace.push_back("op 1 path '" + std::string(reinterpret_cast<const char*>(a)) + "' = " +
                        std::to_string((unsigned long long)*s) + " " + std::to_string((unsigned long long)*k));
        return 0;
    }
    case Op::QueueSend: case Op::QueueDownload:
        Line("op %lld %lld path '", (long long)inOp, a);
        sTrace.back() += std::string(reinterpret_cast<const char*>(b), (size_t)c) + "'";
        return 0;
    case Op::Open:
        r = R(3) ? kFile : 0;
        Line("op 40 path '");
        sTrace.back() += std::string(reinterpret_cast<const char*>(a)) + "' = " + std::to_string((long long)r);
        return r;
    case Op::Send: {
        int* aSent = reinterpret_cast<int*>(c);
        static const intptr_t kStatus[] = {0, 0, 0, 100001, 100001, -1, 100002};
        r = aLate ? -1 : kStatus[R(7)];
        *aSent = R(5) == 0 ? (int)R(3) - 1 : (int)R(b + 1);
        Line("op 44 %lld sent %lld = %lld", b, *aSent, r);
        return r;
    }
    case Op::Read: // (b: the buffer, not traced)
        r = aLate ? 0 : R(4) == 0 ? 0 : R(2) ? (intptr_t)(1 + R(0x2000)) : 0x2000;
        Line("op 41 %lld %lld = %lld", a, c, r);
        return r;
    case Op::Time: r = 0x7FFFF000 + 500 * (intptr_t)R(5); break; // differences of 0..2000 ms by 500
    case Op::MyGame: r = R(4) ? kGame : 0; break;
    case Op::Succeeded: case Op::HostValid: r = R(3) != 0; break;
    case Op::OpSocket: case Op::AcceptedSocket: r = Socket(); break;
    case Op::NewSocket: r = Socket() + 0x1000; sRefs[r]++; break;
    case Op::Bind: case Op::Listen: r = R(4) ? 0 : -(intptr_t)R(3) - 1; break;
    case Op::BytesRead: r = R(100000); break;
    case Op::TimeLeft: r = R(2) ? 0 : 500; break;
    default: break;
    }
    Line("op %lld %lld %lld %lld", (long long)inOp, a, b, c);
    sTrace.back() += " = " + std::to_string((long long)r);
    (void)inObj;
    if (inOp == Op::Kill && sDepth == 0 && R(2) == 0) {
        sDepth++;
        Line("reenter closed %lld", a);
        XferCall(Call::OnClientClosed, 0x0C00 + a, 0); // an op whose socket the host answers
        sDepth--;
    }
    return r;
}

static intptr_t SoundHost(void* inObj, SOp inOp, intptr_t a)
{
    intptr_t r = 0;
    switch (inOp) {
    case SOp::HasUiManager: case SOp::HasSoundManager: r = R(4) != 0; break;
    case SOp::Play: r = R(5) ? (intptr_t)R(50) : (intptr_t)0xFFFFFFFF; break;
    case SOp::Load:
        r = R(3) ? (intptr_t)R(90) : (intptr_t)0xFFFFFFFF;
        Line("sop 8 '");
        sTrace.back() += std::string(reinterpret_cast<const char*>(a)) + "' = " + std::to_string((long long)r);
        return r;
    default: break;
    }
    Line("sop %lld %lld %lld = %lld", (long long)inOp, (intptr_t)inObj, a, r);
    return r;
}

static intptr_t O(Op inOp, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0) { return Host((void*)kXfer, inOp, a, b, c); }
static intptr_t OS(Op inOp, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0) { return Host((void*)kSend, inOp, a, b, c); }
static intptr_t OSnd(intptr_t inObj, SOp inOp, intptr_t a = 0) { return SoundHost((void*)inObj, inOp, a); }

// ---- the original classes, every call on another object replaced by its op

// SmartPtr<AsyncSocket> / SmartPtr<QueueSocket>: AddRef on taking, Release on letting go
struct Ptr {
    intptr_t p;
    Ptr(intptr_t q = 0) : p(q) { if (p) O(Op::AddRef, p); }
    Ptr(const Ptr& o) : p(o.p) { if (p) O(Op::AddRef, p); }
    ~Ptr() { if (p) O(Op::Release, p); }
    Ptr& operator=(const Ptr& o) { intptr_t old = p; p = o.p; if (p) O(Op::AddRef, p); if (old) O(Op::Release, old); return *this; }
    bool operator<(const Ptr& o) const { return p < o.p; }
};
// `new QueueSocket(0)`'s object (the host's NewSocket answers with one reference: the new object's first holder)
static Ptr Adopt(intptr_t q) { Ptr x(q); O(Op::Release, q); return x; }

static struct OrigXfer {
    Ptr mConnection;
    std::set<Ptr> mClients;
    std::string mPath;
    unsigned long mLocalSize, mLocalChecksum, mSize, mChecksum;
    long mLastProgressTime;
} *sX;

static void OrigSetPath(const std::string& inPath)
{
    sX->mPath = inPath;
    sX->mLocalChecksum = 0;
    sX->mLocalSize = 0;
    uint32_t s = (uint32_t)sX->mLocalSize, k = (uint32_t)sX->mLocalChecksum;
    O(Op::SavedGameInfo, (intptr_t)sX->mPath.c_str(), (intptr_t)&s, (intptr_t)&k); // EELobbyGetSavedGameInfo
    sX->mLocalSize = s;
    sX->mLocalChecksum = k;
}

static bool OrigIsFileHere() { return sX->mSize == sX->mLocalSize && sX->mChecksum == sX->mLocalChecksum; }

static void OrigOnClientClosed(intptr_t inOp)
{
    Ptr theSocket = O(Op::OpSocket, inOp);
    sX->mClients.erase(theSocket);
}

static void OrigOnClientAccepted(intptr_t inOp)
{
    if (!O(Op::Succeeded, inOp))
        return;
    Ptr theSocket = O(Op::AcceptedSocket, inOp);
    sX->mClients.insert(theSocket);
    O(Op::QueueSend, theSocket.p, (intptr_t)sX->mPath.data(), (intptr_t)sX->mPath.size());
}

static void OrigOnProgress(intptr_t inOp)
{
    intptr_t theGame = O(Op::MyGame);
    if (theGame == 0)
        return;
    unsigned long theTime = (unsigned long)O(Op::Time);
    if ((uint32_t)(theTime - sX->mLastProgressTime) < 1000)
        return;
    sX->mLastProgressTime = (long)theTime;
    intptr_t aRead = O(Op::BytesRead, inOp);
    O(Op::GameProgress, theGame, (int)aRead, (int)sX->mSize);
}

static void OrigOnFileReceived(intptr_t inOp)
{
    intptr_t theGame = O(Op::MyGame);
    if (theGame != 0 && O(Op::Succeeded, inOp))
        O(Op::GameProgress, theGame, (int)sX->mSize, (int)sX->mSize);
}

static void OrigCloseClients()
{
    // (the copy: the Rust walks one; the original's Kill never changed the set before its completion was posted)
    std::set<Ptr> aCopy = sX->mClients;
    for (std::set<Ptr>::iterator it = aCopy.begin(); it != aCopy.end(); ++it)
        O(Op::Kill, it->p);
    aCopy.clear();
    sX->mClients.clear();
}

static void OrigStop()
{
    OrigCloseClients();
    if (sX->mConnection.p != 0) {
        O(Op::Kill, sX->mConnection.p);
        sX->mConnection = Ptr();
    }
}

static long OrigServe(unsigned short inPort)
{
    OrigStop();
    sX->mConnection = Adopt(O(Op::NewSocket));
    long theStatus = (long)O(Op::Bind, sX->mConnection.p, inPort);
    if (theStatus != 0)
        return theStatus;
    theStatus = (long)O(Op::Listen, sX->mConnection.p);
    if (theStatus != 0)
        return theStatus;
    O(Op::RepeatAccept, sX->mConnection.p);
    return 0;
}

static void OrigDownload()
{
    if (sX->mPath.empty() || !O(Op::HostValid))
        return;
    sX->mLastProgressTime = (long)(unsigned long)O(Op::Time);
    Ptr theSocket = Adopt(O(Op::NewSocket));
    O(Op::QueueDownload, theSocket.p, (intptr_t)sX->mPath.data(), (intptr_t)sX->mPath.size());
    sX->mClients.insert(theSocket);
}

static intptr_t OrigXferCall(Call inCall, intptr_t a, intptr_t b)
{
    switch (inCall) {
    case Call::SetPath: OrigSetPath(std::string(reinterpret_cast<const char*>(a), (size_t)b)); return 0;
    case Call::SetFileInfo: sX->mSize = (unsigned long)(uint32_t)a; sX->mChecksum = (unsigned long)(uint32_t)b; return 0;
    case Call::IsFileHere: return OrigIsFileHere();
    case Call::Serve: return OrigServe((unsigned short)a);
    case Call::Stop: OrigStop(); return 0;
    case Call::CloseClients: OrigCloseClients(); return 0;
    case Call::Download: OrigDownload(); return 0;
    case Call::OnClientClosed: OrigOnClientClosed(a); return 0;
    case Call::OnClientAccepted: OrigOnClientAccepted(a); return 0;
    case Call::OnProgress: OrigOnProgress(a); return 0;
    case Call::OnFileReceived: OrigOnFileReceived(a); return 0;
    case Call::Field:
        switch (a) {
        case 0: return (intptr_t)(uint32_t)sX->mLocalSize;
        case 1: return (intptr_t)(uint32_t)sX->mLocalChecksum;
        case 2: return (intptr_t)(uint32_t)sX->mSize;
        case 3: return (intptr_t)(uint32_t)sX->mChecksum;
        case 4: return (intptr_t)(uint32_t)sX->mLastProgressTime;
        case 5: return sX->mConnection.p;
        case 6: return (intptr_t)sX->mClients.size();
        case 7: return (intptr_t)sX->mPath.size();
        }
        return 0;
    }
    return 0;
}

static intptr_t XferCall(Call inCall, intptr_t a, intptr_t b)
{
    return sRust ? ee_won_eexfer_call(sRustXfer, inCall, a, b) : OrigXferCall(inCall, a, b);
}

// SendFileOp
static struct OrigSend {
    intptr_t mFile;
    std::string mPath;
    unsigned char mBuffer[0x2000];
    unsigned long mPos, mLength; // ReadBuffer mReadBuffer
} *sS;

static void OrigCloseFile()
{
    if (sS->mFile != 0) {
        OS(Op::Close, sS->mFile); // fclose
        sS->mFile = 0;
    }
}

static bool OrigReadChunk()
{
    int theCount = (int)OS(Op::Read, sS->mFile, (intptr_t)sS->mBuffer, 0x2000);
    if (theCount < 0)
        theCount = 0;
    sS->mPos = 0; // mReadBuffer.SetData(mBuffer, theCount)
    sS->mLength = (unsigned long)theCount;
    return theCount > 0;
}

static long OrigContinue()
{
    for (;;) {
        if ((unsigned long)sS->mPos < (unsigned long)sS->mLength) {
            OS(Op::WaitForWrite);
            int theSent = 0;
            long theStatus = (long)OS(Op::Send, (intptr_t)(sS->mBuffer + sS->mPos), (intptr_t)(sS->mLength - sS->mPos),
                                      (intptr_t)&theSent);
            if ((long)theSent > 0)
                sS->mPos += theSent; // mReadBuffer.ReadBytes(theSent)
            if (theStatus == 100001 && OS(Op::TimeLeft) == 0)
                return 100001;
            if (theStatus != 0)
                return theStatus;
        } else if (!OrigReadChunk()) {
            OrigCloseFile();
            return 0;
        }
    }
}

static long OrigStart()
{
    if (sS->mPath.size() == 0)
        return 100007;
    if ((sS->mFile = OS(Op::Open, (intptr_t)sS->mPath.c_str())) == 0)
        return 100007;
    OrigReadChunk();
    return OrigContinue();
}

static intptr_t SendCall(EeWonEeXferSend* inRust, SCall inCall)
{
    if (sRust) return ee_won_eexfer_send_call(inRust, inCall);
    switch (inCall) {
    case SCall::Start: return OrigStart();
    case SCall::Continue: return OrigContinue();
    case SCall::Cleanup: OrigCloseFile(); return 0;
    }
    return 0;
}

// EELobbyMusic and EELobbySound
static struct OrigSound { unsigned long mSampleID, mInstanceID; } sEffect;

static intptr_t SoundCall(EeWonEeSound* inRust, bool inMusic, SndCall inCall, intptr_t a)
{
    if (sRust) return ee_won_eesound_call(inRust, inCall, a);
    intptr_t o = inMusic ? kMusic : kEffect;
    switch (inCall) {
    case SndCall::Play: case SndCall::Resume:
        if (inMusic) {
            if (OSnd(o, SOp::HasUiManager)) OSnd(o, SOp::MusicVolumeOn);
        } else if (inCall == SndCall::Play) {
            if (OSnd(o, SOp::HasSoundManager)) sEffect.mInstanceID = (unsigned long)(uint32_t)OSnd(o, SOp::Play, sEffect.mSampleID);
        }
        return 0;
    case SndCall::Stop:
        if (inMusic) {
            if (OSnd(o, SOp::HasUiManager)) OSnd(o, SOp::MusicVolumeOff);
        } else {
            if (OSnd(o, SOp::HasSoundManager) && !(sEffect.mInstanceID == 0xFFFFFFFFul))
                OSnd(o, SOp::Stop, sEffect.mInstanceID);
            sEffect.mInstanceID = 0xFFFFFFFFul;
        }
        return 0;
    case SndCall::Field:
        if (inMusic) return 0;
        return a == 0 ? (intptr_t)sEffect.mSampleID : a == 1 ? (intptr_t)sEffect.mInstanceID : 0;
    }
    return 0;
}

static long long OrigLoad(const char* inPath)
{
    if (!OSnd(0, SOp::HasSoundManager)) return -1;
    return (long long)(uint32_t)OSnd(0, SOp::Load, (intptr_t)inPath);
}

// ---- the sequences

enum What { kXferStep, kSendStep, kMusicStep, kEffectStep, kLoadStep };
struct Step { What mWhat; int mCall; intptr_t a, b; std::string mText; };

static const char* const kPaths[] = {"", "save1.ees", "Saved Games\\Two.ees", "x"};

static std::vector<std::string> RunSide(bool inRust, unsigned inSeed, const std::vector<Step>& inSteps,
                                        const std::string& inSendPath, unsigned long inSample)
{
    sTrace.clear();
    sRefs.clear();
    sSeed = inSeed;
    sRust = inRust;
    sDepth = 0;
    sOps = 0;
    EeWonEeXferSend* aRustSend = NULL;
    EeWonEeSound* aMusic = NULL;
    EeWonEeSound* anEffect = NULL;
    if (inRust) {
        sRustXfer = ee_won_eexfer_new((void*)kXfer, Host);
        aRustSend = ee_won_eexfer_send_new((void*)kSend, Host, (const uint8_t*)inSendPath.data(), inSendPath.size());
        aMusic = ee_won_eesound_new((void*)kMusic, EeWonEeSoundKind::Music, SoundHost, 0);
        anEffect = ee_won_eesound_new((void*)kEffect, EeWonEeSoundKind::Effect, SoundHost, (uint32_t)inSample);
    } else {
        sX = new OrigXfer();
        sX->mLocalSize = sX->mLocalChecksum = sX->mSize = sX->mChecksum = 0;
        sX->mLastProgressTime = 0;
        sS = new OrigSend();
        sS->mFile = 0;
        sS->mPath = inSendPath;
        sS->mPos = sS->mLength = 0;
        sEffect.mSampleID = inSample;
        sEffect.mInstanceID = 0xFFFFFFFFul;
    }
    for (size_t i = 0; i < inSteps.size(); i++) {
        const Step& s = inSteps[i];
        sOps = 0;
        Line("call %lld %lld %lld %lld", s.mWhat, s.mCall, s.a, s.b);
        intptr_t r = 0;
        switch (s.mWhat) {
        case kXferStep:
            if ((Call)s.mCall == Call::SetPath) {
                sTrace.back() += " '" + s.mText + "'";
                r = XferCall(Call::SetPath, (intptr_t)s.mText.data(), (intptr_t)s.mText.size());
            } else
                r = XferCall((Call)s.mCall, s.a, s.b);
            break;
        case kSendStep: r = SendCall(aRustSend, (SCall)s.mCall); break;
        case kMusicStep: r = SoundCall(aMusic, true, (SndCall)s.mCall, s.a); break;
        case kEffectStep: r = SoundCall(anEffect, false, (SndCall)s.mCall, s.a); break;
        case kLoadStep:
            sTrace.back() += " '" + s.mText + "'";
            r = inRust ? (intptr_t)ee_won_eesound_load(SoundHost, (const uint8_t*)s.mText.c_str()) : (intptr_t)OrigLoad(s.mText.c_str());
            break;
        }
        Line("ret %lld", r);
    }
    Line("free");
    if (inRust) {
        ee_won_eexfer_free(sRustXfer);
        ee_won_eexfer_send_free(aRustSend);
        ee_won_eesound_free(aMusic);
        ee_won_eesound_free(anEffect);
    } else {
        delete sX; // the members' SmartPtrs
        sX = NULL;
        delete sS;
        sS = NULL;
        // ~EELobbySound: the sample unloaded
        if (OSnd(kEffect, SOp::HasSoundManager)) OSnd(kEffect, SOp::Unload, sEffect.mSampleID);
    }
    for (std::map<intptr_t, int>::iterator it = sRefs.begin(); it != sRefs.end(); ++it)
        if (it->second != 0) Line("refs %lld %lld", it->first, it->second);
    return sTrace;
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    unsigned long long aLines = 0;
    int aDiffer = 0;
    unsigned aStepSeed = 4711;
    const int kSequences = 50000;
    for (int n = 0; n < kSequences; n++) {
        unsigned aSave = sSeed;
        sSeed = aStepSeed;
        std::string aSendPath = kPaths[R(4)];
        unsigned long aSample = R(5) ? R(40) : 0xFFFFFFFFul;
        std::vector<Step> aSteps;
        int aCount = 1 + (int)R(14);
        for (int i = 0; i < aCount; i++) {
            Step s = {kXferStep, (int)Call::Field, (intptr_t)R(9), 0, ""};
            switch (R(20)) {
            case 0: case 1: s.mCall = (int)Call::SetPath; s.mText = kPaths[R(4)]; break;
            case 2: s.mCall = (int)Call::SetFileInfo; s.a = R(4) ? 1000 + (intptr_t)R(3) : (intptr_t)0xFFFFFFF0u; s.b = R(3); break;
            case 3: s.mCall = (int)Call::IsFileHere; break;
            case 4: s.mCall = (int)Call::Serve; s.a = R(2) ? 8889 : 70000; break;
            case 5: s.mCall = (int)Call::Stop; break;
            case 6: s.mCall = (int)Call::CloseClients; break;
            case 7: s.mCall = (int)Call::Download; break;
            case 8: s.mCall = (int)Call::OnClientClosed; s.a = 0xA00 + (intptr_t)R(4); break;
            case 9: case 10: s.mCall = (int)Call::OnClientAccepted; s.a = 0xA00 + (intptr_t)R(4); break;
            case 11: s.mCall = (int)Call::OnProgress; s.a = 0xA10; break;
            case 12: s.mCall = (int)Call::OnFileReceived; s.a = 0xA20; break;
            case 13: case 14: s.mWhat = kSendStep; s.mCall = 1 + (int)R(3); break;
            case 15: s.mWhat = kMusicStep; s.mCall = 1 + (int)R(3); break;
            case 16: case 17: s.mWhat = kEffectStep; s.mCall = 1 + (int)R(4); s.a = R(3); break;
            case 18: s.mWhat = kLoadStep; s.mText = kPaths[R(4)]; break;
            default: break;
            }
            aSteps.push_back(s);
        }
        aStepSeed = sSeed;
        sSeed = aSave;
        unsigned aSeed = 101u + (unsigned)n * 7919u;
        std::vector<std::string> a = RunSide(false, aSeed, aSteps, aSendPath, aSample);
        std::vector<std::string> b = RunSide(true, aSeed, aSteps, aSendPath, aSample);
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
        bool aNew = n < 60;
        for (size_t i = 0; i < a.size(); i++) {
            size_t aSpace = a[i].find(' ');
            std::string aHead = a[i].substr(0, aSpace);
            size_t aSecond = a[i].find(' ', aSpace + 1);
            std::string aKey = aHead == "ret" || aHead == "free" || aHead == "refs" ? aHead
                             : aHead == "call" ? a[i].substr(0, a[i].find(' ', aSecond + 1)) : a[i].substr(0, aSecond);
            if (sSeen.insert(aKey).second) aNew = true;
        }
        if (aPrint && aNew) {
            printf("seq %d %u %s|%lu\n", n, aSeed, aSendPath.c_str(), aSample);
            for (size_t i = 0; i < a.size(); i++) printf("%s\n", a[i].c_str());
        }
    }
    if (aDiffer) { sFailures++; printf("FAIL: %d of %d sequences differ\n", aDiffer, kSequences); }
    if (!aPrint)
        printf("won_eexfer_test: %d sequences, %llu trace lines, %zu distinct ops and calls; %d checks, %d failures\n",
               kSequences, aLines, sSeen.size(), sChecks, sFailures);
    return sFailures ? 1 : 0;
}
