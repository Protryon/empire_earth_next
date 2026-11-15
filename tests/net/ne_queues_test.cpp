// NELagLossSimulator, NERecordPlayback and NETimestampedMessage as whole Rust objects (LLE Network Engine round 2):
// the original units, unmodified (orig_q/NELagLossSimulator.cpp, NERecordPlayback.cpp, NETimestampedMessage.cpp,
// compiled over the stand-in headers of orig_q/lle: the clock, events, threads, messages and files of one world),
// against ee_net's Rust objects (lagloss.rs, recplay.rs) whose host op table answers with the same world objects,
// as the client's and the server's hosts (NESynchronousCommunications.cpp) answer with the game's.
//
// The lag thread and the playback thread are real threads that run one at a time (orig_q/lle/q_world.h): the
// world hands the baton on by a fixed rule, so both sides interleave the same way. Each sequence makes random calls
// on a lag simulator and a recorder with the same random stream: messages received (lost, lagged or passed on),
// sent (lost or sent), lag on and off (flushed or not), loss on and off, messages passed, recorded to a file and
// played back from one (written by the recorder or made by the world, some records naming no message), slow and
// normal playback, the clock moving on (the threads running at their deadlines). Each side's world prints a line
// for every effect; the traces must be the same. At the end both objects are freed, every thread must have ended,
// and the messages left (the originals' leaks) are listed.
//
// The Rust side's host also records each op with its arguments and answer (messages, events, threads and files by
// world id), the API calls and the thread switches: the first sequences are rust/crates/ee_net/src/golden_queues.txt,
// which cargo replays against the Rust alone (`--print FILE` writes it).
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#include "orig_q/lle/q_world.h"
QWorld* gQ;
static bool sVerbose = false;   // --verbose: each trace line to stderr as it is made
#include "orig_q/NETimestampedMessage.cpp"
#include "orig_q/NELagLossSimulator.cpp"
#include "orig_q/NERecordPlayback.cpp"
#include "rust/ee_net.h"
#include "ne_tcp_host.h"

// URandom's out-of-line members, as GEBitmap.cpp defines them
void (*gURandomTraceHook)(URandom* inRandom) = NULL;
float URandom::Generate()
{
    this->mSeed = (this->mMultiplier * this->mSeed + this->mIncrement) % this->mModulus;
    return (float)((double)this->mSeed / (double)this->mModulus);
}
unsigned long URandom::NextInSeries()
{
    this->mSeed = (this->mMultiplier * this->mSeed + this->mIncrement) % this->mModulus;
    return this->mSeed;
}

int QWorld::IdOf(NEMessage* inMessage)
{
    return inMessage != NULL ? inMessage->mID : 0;
}

// ---- the owners' callbacks (the client's and the server's): each takes the message's reference

static void ReceiveCallback(NEMessage* inMessage, void*)
{
    gQ->Line("cb receive m%d", inMessage->mID);
    inMessage->Dereference();
}
static long SendCallback(NEMessage* inMessage, void*)
{
    long theResult = gQ->Next() % 5 == 0 ? 7 : 0;
    gQ->Line("cb send m%d = %ld", inMessage->mID, theResult);
    inMessage->Dereference();
    return theResult;
}
static void PlayCallback(NEMessage* inMessage, void*)
{
    gQ->Line("cb play m%d", inMessage->mID);
    inMessage->Dereference();
}

// ---- the Rust side's host

static std::string Hex(const unsigned char* inData, size_t inSize)
{
    std::string theHex;
    char theByte[4];
    for (size_t i = 0; i < inSize; i++) {
        snprintf(theByte, sizeof(theByte), "%02x", inData[i]);
        theHex += theByte;
    }
    return theHex.empty() ? "-" : theHex;
}
static std::string Tok(char inKind, int inID) { return std::string(1, inKind) + std::to_string(inID); }
static std::string MsgTok(size_t inMessage) { return Tok('m', gQ->IdOf((NEMessage*)inMessage)); }
static std::string EvTok(size_t inEvent) { return Tok('e', inEvent ? ((TSEvent*)inEvent)->mID : 0); }
static std::string ThTok(size_t inThread) { return Tok('t', inThread ? ((TSThread*)inThread)->mID : 0); }
static std::string FileTok(size_t inFile) { return Tok('f', inFile ? ((FSFileSpec*)inFile)->mID : 0); }

static unsigned long RustLagThread(void* inObject) { return ee_net_ll_thread((const EeNetLagLoss*)inObject); }
static unsigned long RustPlaybackThread(void* inObject) { return ee_net_rp_thread((const EeNetRecPlay*)inObject); }

static ptrdiff_t RustHost(void* inContext, EeNetQOp inOp, size_t a, size_t b, size_t c, size_t d)
{
    NEMessage* theMessage = (NEMessage*)a;
    TSEvent* theEvent = (TSEvent*)a;
    FSFileSpec* theFile = (FSFileSpec*)a;
    std::string theArgs = std::to_string((int)inOp);
    std::string theAnswer;
    intptr_t theResult = 0;
    switch (inOp) {
    case EeNetQOp::Time:
        theResult = (intptr_t)TMGetTime();
        break;
    case EeNetQOp::Verify:
        theArgs += " " + std::to_string(a) + " " + std::to_string(b);
        gQ->Line("verify %d", (int)b);
        break;
    case EeNetQOp::FpuStart:
        UMSetFloatingPointPrecision(kFPPrecision53);
        break;
    case EeNetQOp::FpuCheck:
        if (UMGetFloatingPointPrecision() != kFPPrecision53)
            UMSetFloatingPointPrecision(kFPPrecision53);
        if (UMGetFloatingPointRoundingMode() != 0)
            UMSetFloatingPointRoundingMode(0);
        break;
    case EeNetQOp::EventNew: {
        TSEvent* theNew = NULL;
        TSScheduler::CreateEvent(theNew);
        theResult = (intptr_t)theNew;
        theAnswer = EvTok(theResult);
        break;
    }
    case EeNetQOp::EventFree:
        theArgs += " " + EvTok(a);
        TSScheduler::DestroyEvent(theEvent);
        break;
    case EeNetQOp::EventSet:
        theArgs += " " + EvTok(a);
        theEvent->Set();
        break;
    case EeNetQOp::EventClear:
        theArgs += " " + EvTok(a);
        theEvent->Clear();
        break;
    case EeNetQOp::EventWait:
        theArgs += " " + EvTok(a) + " " + std::to_string(b);
        gQ->Op("op " + theArgs);
        theResult = theEvent->Wait((unsigned long)b);
        gQ->Op("= " + std::to_string(theResult));
        return theResult;
    case EeNetQOp::ThreadNew: {
        TSThread* theThread = NULL;
        theArgs += " " + std::to_string(a);
        theResult = TSScheduler::CreateThread(theThread, a == 0 ? RustLagThread : RustPlaybackThread, (void*)b, 0, false);
        *(TSThread**)c = theThread;
        theAnswer = ThTok((size_t)theThread) + " " + std::to_string(theResult);
        break;
    }
    case EeNetQOp::ThreadName:
        theArgs += " " + ThTok(a) + " " + std::string((const char*)b, c);
        ((TSThread*)a)->SetName(UString(std::string((const char*)b, c).c_str()));
        break;
    case EeNetQOp::ThreadFree:
        theArgs += " " + ThTok(a);
        TSScheduler::DestroyThread((TSThread*)a);
        break;
    case EeNetQOp::MsgReference:
        theArgs += " " + MsgTok(a);
        theMessage->Reference();
        break;
    case EeNetQOp::MsgDereference:
        theArgs += " " + MsgTok(a);
        theMessage->Dereference();
        break;
    case EeNetQOp::MsgExportSize:
        theArgs += " " + MsgTok(a);
        theResult = (intptr_t)theMessage->ExportSize();
        break;
    case EeNetQOp::MsgExport: {
        theArgs += " " + MsgTok(a) + " " + std::to_string(c);
        unsigned char* theData;
        unsigned long theSize;
        theMessage->ExportMessage(&theData, theSize);
        size_t theCopy = theSize < c ? theSize : c;
        memcpy((void*)b, theData, theCopy);
        delete[] theData;
        theResult = (intptr_t)theSize;
        theAnswer = std::to_string(theSize) + " " + Hex((const unsigned char*)b, theCopy);
        break;
    }
    case EeNetQOp::MsgFromBytes: {
        theArgs += " " + (a ? Hex((const unsigned char*)a, b) : std::string("null"));
        NEMessage* theNew = NEMessage::ConvertMemoryToMessage((unsigned char*)a);
        theResult = (intptr_t)theNew;
        theAnswer = MsgTok(theResult);
        break;
    }
    case EeNetQOp::Deliver:
        theArgs += " " + std::to_string(a) + " " + MsgTok(b);
        if (a == 0)
            ReceiveCallback((NEMessage*)b, inContext);
        else if (a == 1)
            theResult = SendCallback((NEMessage*)b, inContext);
        else
            PlayCallback((NEMessage*)b, inContext);
        break;
    case EeNetQOp::FileCreate:
        theArgs += " " + FileTok(a);
        theResult = theFile->Create();
        break;
    case EeNetQOp::FileDelete:
        theArgs += " " + FileTok(a);
        theResult = theFile->Delete();
        break;
    case EeNetQOp::FileOpen:
        theArgs += " " + FileTok(a) + " " + std::to_string(b);
        theResult = theFile->Open((unsigned long)b);
        break;
    case EeNetQOp::FileClose:
        theArgs += " " + FileTok(a);
        theFile->Close();
        break;
    case EeNetQOp::FileWrite: {
        theArgs += " " + FileTok(a) + " " + Hex((const unsigned char*)b, c);
        unsigned long theWritten;
        theResult = theFile->Write((const unsigned char*)b, (unsigned long)c, theWritten);
        break;
    }
    case EeNetQOp::FileWriteU32: {
        theArgs += " " + FileTok(a) + " " + std::to_string(b);
        unsigned long theValue = (unsigned long)b;
        theResult = theFile->Write(theValue);
        break;
    }
    case EeNetQOp::FileReadU32: {
        uint32_t* theOut = (uint32_t*)b;
        theArgs += " " + FileTok(a) + " " + std::to_string(*theOut);
        unsigned long theValue = *theOut;
        theResult = theFile->Read(theValue);
        *theOut = (uint32_t)theValue;
        theAnswer = std::to_string(theResult) + " " + std::to_string(*theOut);
        break;
    }
    case EeNetQOp::FileRead: {
        theArgs += " " + FileTok(a) + " " + std::to_string(c);
        unsigned long theRead;
        theResult = theFile->Read((unsigned char*)b, (unsigned long)c, theRead);
        theAnswer = std::to_string(theResult) + " " + Hex((const unsigned char*)b, c);
        break;
    }
    }
    gQ->Op("op " + theArgs);
    gQ->Op("= " + (theAnswer.empty() ? std::to_string((long long)theResult) : theAnswer));
    return theResult;
}

// ---- one sequence on one side

struct Rng {
    uint32_t mState;
    uint32_t Next()
    {
        this->mState = this->mState * 1664525u + 1013904223u;
        return this->mState >> 8;
    }
    uint32_t Below(uint32_t inN) { return this->Next() % inN; }
};

static const float kPercentages[] = {0.0f, 1.0f, 25.5f, 50.0f, 99.0f, 100.5f};

struct Sequence {
    bool mRust;
    Rng mRandom;
    NELagLossSimulator* mOrigLag;
    NERecordPlayback* mOrigRec;
    EeNetLagLoss* mLag;
    EeNetRecPlay* mRec;
    FSFileSpec* mFiles[3];
    bool mLagOn;
    unsigned char mMode;      // the recorder's mode as the calls set it
    bool mPlayingFile;        // played back from a file: the thread's events exist

    void Call(const std::string& inCall)
    {
        if (this->mRust)
            gQ->Op("call " + inCall);
        gQ->Line("call %s", inCall.c_str());
    }
    void Result(long inResult)
    {
        if (this->mRust)
            gQ->Op("ret " + std::to_string(inResult));
        gQ->Line("= %ld", inResult);
    }
    NEMessage* NewMessage()
    {
        unsigned char theBytes[8];
        theBytes[0] = (unsigned char)this->mRandom.Below(7);
        for (int i = 1; i <= theBytes[0]; i++)
            theBytes[i] = (unsigned char)this->mRandom.Next();
        return new NEMessage(theBytes, (size_t)theBytes[0] + 1);
    }
    FSFileSpec* File() { return this->mFiles[this->mRandom.Below(3)]; }
    // a record file made by the world: the start time, then records (some naming no message), maybe cut short
    void MakeFile()
    {
        FSFileSpec* theSpec = this->File();
        QFile& f = gQ->mFiles[theSpec->mID];
        f.mExists = true;
        f.mOpen = false;
        f.mPos = 0;
        f.mData.clear();
        uint32_t theStart = (uint32_t)(gQ->mClock - this->mRandom.Below(2000));
        auto Put = [&](uint32_t inValue) {
            for (int i = 0; i < 4; i++)
                f.mData.push_back((unsigned char)(inValue >> (8 * i)));
        };
        Put(theStart);
        // where the file may end early: at a record's start, inside its time, inside its message (not inside its
        // size: the first record's message would then be read from no bytes, which the original reads past)
        std::vector<size_t> theCuts;
        uint32_t theTime = theStart;
        int theCount = (int)this->mRandom.Below(6);
        for (int i = 0; i < theCount; i++) {
            for (size_t k = 0; k < 4; k++)
                theCuts.push_back(f.mData.size() + k);
            theTime += this->mRandom.Below(4) == 0 ? 0 : this->mRandom.Below(400);
            Put(theTime);
            bool theBad = this->mRandom.Below(8) == 0;
            unsigned char n = (unsigned char)this->mRandom.Below(5);
            Put(1u + n);
            f.mData.push_back(theBad ? 0xff : n);
            for (int j = 0; j < n; j++) {
                theCuts.push_back(f.mData.size());
                f.mData.push_back((unsigned char)this->mRandom.Next());
            }
        }
        if (this->mRandom.Below(4) == 0 && !theCuts.empty())
            f.mData.resize(theCuts[this->mRandom.Below((uint32_t)theCuts.size())]);
        gQ->Line("made f%d %s", theSpec->mID, Hex(f.mData.data(), f.mData.size()).c_str());
    }

    void Step()
    {
        uint32_t theWhat = this->mRandom.Below(100);
        char theCall[128];
        long r = 0;
        if (theWhat < 6) {   // the world changes
            gQ->mClock += this->mRandom.Below(300);
            if (this->mRandom.Below(4) == 0)
                gQ->mPrecision = this->mRandom.Below(2) == 0 ? 0x20000 : kFPPrecision53;
            if (this->mRandom.Below(4) == 0)
                gQ->mRounding = this->mRandom.Below(2) == 0 ? 0x100 : 0;
            gQ->Line("env %lu %lx %lx", gQ->mClock, gQ->mPrecision, gQ->mRounding);
            return;
        }
        if (theWhat < 18) {   // time passes, the threads running at their deadlines
            unsigned long theTime = this->mRandom.Below(3) == 0 ? this->mRandom.Below(3000) : this->mRandom.Below(300);
            snprintf(theCall, sizeof(theCall), "pass %lu", theTime);
            this->Call(theCall);
            gQ->Pass(theTime);
        } else if (theWhat < 34) {
            NEMessage* m = this->Random() ? NULL : this->NewMessage();
            snprintf(theCall, sizeof(theCall), "ll process m%d", gQ->IdOf(m));
            this->Call(theCall);
            r = this->mRust ? ee_net_ll_process(this->mLag, m) : this->mOrigLag->Process(m);
        } else if (theWhat < 42) {
            NEMessage* m = this->Random() ? NULL : this->NewMessage();
            snprintf(theCall, sizeof(theCall), "ll send m%d", gQ->IdOf(m));
            this->Call(theCall);
            r = this->mRust ? ee_net_ll_send(this->mLag, m) : this->mOrigLag->Send(m);
        } else if (theWhat < 47) {
            if (this->mLagOn)
                return;
            unsigned long theMin = this->mRandom.Below(400), theMax = theMin;
            if (this->mRandom.Below(3) != 0)
                theMax += this->mRandom.Below(600);
            snprintf(theCall, sizeof(theCall), "ll enablelag %lu %lu", theMin, theMax);
            this->Call(theCall);
            if (this->mRust)
                ee_net_ll_enable_lag(this->mLag, (uint32_t)theMin, (uint32_t)theMax);
            else
                this->mOrigLag->EnableLag(theMin, theMax);
            this->mLagOn = true;
        } else if (theWhat < 51) {
            bool theFlush = this->mRandom.Below(2) != 0;
            snprintf(theCall, sizeof(theCall), "ll disablelag %d", theFlush);
            this->Call(theCall);
            if (this->mRust)
                ee_net_ll_disable_lag(this->mLag, theFlush);
            else
                this->mOrigLag->DisableLag(theFlush);
            this->mLagOn = false;
        } else if (theWhat < 56) {
            uint32_t theKind = this->mRandom.Below(4);
            float thePercentage = kPercentages[this->mRandom.Below(6)];
            snprintf(theCall, sizeof(theCall), "ll loss %u %g", theKind, thePercentage);
            this->Call(theCall);
            if (this->mRust) {
                if (theKind == 0)
                    ee_net_ll_enable_receive_loss(this->mLag, thePercentage);
                else if (theKind == 1)
                    ee_net_ll_enable_send_loss(this->mLag, thePercentage);
                else if (theKind == 2)
                    ee_net_ll_disable_receive_loss(this->mLag);
                else
                    ee_net_ll_disable_send_loss(this->mLag);
            } else {
                if (theKind == 0)
                    this->mOrigLag->EnableReceiveLoss(thePercentage);
                else if (theKind == 1)
                    this->mOrigLag->EnableSendLoss(thePercentage);
                else if (theKind == 2)
                    this->mOrigLag->DisableReceiveLoss();
                else
                    this->mOrigLag->DisableSendLoss();
            }
        } else if (theWhat < 70) {
            NEMessage* m = this->NewMessage();
            snprintf(theCall, sizeof(theCall), "rp process m%d", m->mID);
            this->Call(theCall);
            if (this->mRust)
                ee_net_rp_process(this->mRec, m);
            else
                this->mOrigRec->Process(m);
            // while playing back, a live message is dropped without being given back: the caller lets it go here
            if (this->mMode == kRecordModePlaying)
                m->Dereference();
        } else if (theWhat < 73) {
            this->Call("rp pass");
            r = this->mRust ? ee_net_rp_pass_mode(this->mRec) : this->mOrigRec->PassMode();
            this->mMode = kRecordModePassing;
            this->mPlayingFile = false;
        } else if (theWhat < 78) {
            FSFileSpec* theSpec = this->File();
            snprintf(theCall, sizeof(theCall), "rp record f%d", theSpec->mID);
            this->Call(theCall);
            r = this->mRust ? ee_net_rp_record_mode_file(this->mRec, theSpec) : this->mOrigRec->RecordMode(theSpec);
            this->mMode = kRecordModeRecording;
            this->mPlayingFile = false;
        } else if (theWhat < 79) {
            this->Call("rp recordnet");
            UString theAddress("host");
            unsigned long thePort = 7;
            r = this->mRust ? ee_net_rp_record_mode_network(this->mRec) : this->mOrigRec->RecordMode(theAddress, thePort);
            this->mMode = kRecordModeRecording;
            this->mPlayingFile = false;
        } else if (theWhat < 84) {
            FSFileSpec* theSpec = this->File();
            snprintf(theCall, sizeof(theCall), "rp play f%d", theSpec->mID);
            this->Call(theCall);
            r = this->mRust ? ee_net_rp_playback_mode_file(this->mRec, theSpec) : this->mOrigRec->PlaybackMode(theSpec);
            this->mMode = kRecordModePlaying;
            this->mPlayingFile = true;
        } else if (theWhat < 85) {
            this->Call("rp playnet");
            UString theAddress("host");
            unsigned long thePort = 7;
            r = this->mRust ? ee_net_rp_playback_mode_network(this->mRec)
                            : this->mOrigRec->PlaybackMode(theAddress, thePort);
            this->mMode = kRecordModePlaying;
            this->mPlayingFile = false;
        } else if (theWhat < 89) {
            // (only with the playback thread's events: the original reads them)
            if (!this->mPlayingFile)
                return;
            uint32_t theKind = this->mRandom.Below(3);
            snprintf(theCall, sizeof(theCall), "rp toggle %u", theKind);
            this->Call(theCall);
            if (theKind == 0)
                r = this->mRust ? ee_net_rp_playback_complete(this->mRec) : this->mOrigRec->PlaybackComplete();
            else if (theKind == 1) {
                if (this->mRust)
                    ee_net_rp_set_slow_playback(this->mRec);
                else
                    this->mOrigRec->SetSlowPlayback();
            } else {
                if (this->mRust)
                    ee_net_rp_set_normal_playback(this->mRec);
                else
                    this->mOrigRec->SetNormalPlayback();
            }
        } else if (theWhat < 92) {
            this->Call("rp mode");
            r = this->mRust ? ee_net_rp_mode(this->mRec) : this->mOrigRec->GetMode();
        } else {
            this->MakeFile();
            return;
        }
        this->Result(r);
        this->Call("run");
        gQ->LetRun();
        this->Result(0);
    }
    bool Random() { return this->mRandom.Below(16) == 0; }

    void Run(uint32_t inSeed, int inSteps)
    {
        this->mRandom.mState = inSeed;
        for (int i = 0; i < 3; i++)
            this->mFiles[i] = new FSFileSpec(i + 1);
        this->Call("new");
        if (this->mRust) {
            this->mLag = ee_net_ll_new(NULL, RustHost);
            ee_net_ll_disable_lag(this->mLag, false);
            ee_net_ll_disable_receive_loss(this->mLag);
            ee_net_ll_disable_send_loss(this->mLag);
            this->mRec = ee_net_rp_new(NULL, RustHost);
            ee_net_rp_pass_mode(this->mRec);
        } else {
            this->mOrigLag = new NELagLossSimulator(ReceiveCallback, SendCallback, NULL);
            this->mOrigLag->DisableLag(false);
            this->mOrigLag->DisableReceiveLoss();
            this->mOrigLag->DisableSendLoss();
            this->mOrigRec = new NERecordPlayback(PlayCallback, NULL);
            this->mOrigRec->PassMode();
        }
        this->mMode = kRecordModePassing;
        this->Result(0);
        for (int i = 0; i < inSteps; i++)
            this->Step();
        this->Call("free");
        if (this->mRust) {
            ee_net_rp_free(this->mRec);
            ee_net_ll_free(this->mLag);
        } else {
            delete this->mOrigRec;
            delete this->mOrigLag;
        }
        this->Result(0);
        this->Call("run");
        gQ->LetRun();
        this->Result(0);
        for (int i = 0; i < 3; i++)
            delete this->mFiles[i];
        gQ->Line("threads %d", (int)gQ->mThreads.size());
        if (gQ->mThreads.size() != 1) {
            fprintf(stderr, "ne_queues_test: sequence %u (%s) left a thread running\n", inSeed, this->mRust ? "rust" : "orig");
            fflush(stderr);
            _exit(3);
        }
        std::string theLeft;
        for (auto& m : gQ->mMessages)
            theLeft += " m" + std::to_string(m.first) + ":" + std::to_string(m.second->mCount);
        gQ->Line("left%s", theLeft.c_str());
    }
};

static std::string RunSide(bool inRust, uint32_t inSeed, int inSteps, bool inRecord, std::string& outOps)
{
    QWorld* theWorld = new QWorld;
    theWorld->mRandom = inSeed * 2654435761u + 7;
    theWorld->mRecord = inRecord;
    theWorld->mEcho = sVerbose;
    gQ = theWorld;
    Sequence theSequence = {};
    theSequence.mRust = inRust;
    theSequence.Run(inSeed, inSteps);
    std::string theTrace = theWorld->mTrace;
    outOps = theWorld->mOps;
    // the messages the originals leak go with the world
    for (auto& m : theWorld->mMessages)
        delete m.second;
    delete theWorld;
    gQ = NULL;
    return theTrace;
}

int main(int argc, char** argv)
{
    int theCount = 1500, theSteps = 80, theRecorded = 60, theShow = 0;
    const char* thePrint = NULL;
    std::map<std::string, int> theKinds;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--count") == 0 && i + 1 < argc)
            theCount = atoi(argv[++i]);
        else if (strcmp(argv[i], "--print") == 0 && i + 1 < argc)
            thePrint = argv[++i];
        else if (strcmp(argv[i], "--show") == 0 && i + 1 < argc)
            theShow = atoi(argv[++i]);
        else if (strcmp(argv[i], "--verbose") == 0)
            sVerbose = true;
    }
    int theDiffering = 0;
    size_t theLines = 0;
    std::string theGolden;
    for (int s = 0; s < theCount; s++) {
        if (sVerbose) {
            fprintf(stderr, "sequence %d\n", s + 1);
            fflush(stderr);
        }
        std::string theOps;
        std::string theOrig = RunSide(false, (uint32_t)s + 1, theSteps, false, theOps);
        bool theRecord = thePrint != NULL && s < theRecorded;
        std::string theRust = RunSide(true, (uint32_t)s + 1, theSteps, theRecord, theOps);
        if (theRecord)
            theGolden += "seq " + std::to_string(s + 1) + "\n" + theOps;
        if (s + 1 == theShow)
            printf("%s", theOrig.c_str());
        for (size_t i = 0, theEnd; i < theOrig.size(); i = theEnd + 1) {
            theEnd = theOrig.find('\n', i);
            std::string theLine = theOrig.substr(i, theEnd - i);
            // the kind: a call's name, else the line's first two words
            size_t theFrom = theLine.find(' ') + 1;
            size_t theSecond = theLine.find(' ', theFrom);
            size_t theThird = theSecond == std::string::npos ? std::string::npos : theLine.find(' ', theSecond + 1);
            theKinds[theLine.substr(theFrom, theThird == std::string::npos ? std::string::npos : theThird - theFrom)]++;
            theLines++;
        }
        if (theOrig != theRust) {
            if (theDiffering++ < 3) {
                size_t i = 0;
                while (i < theOrig.size() && i < theRust.size() && theOrig[i] == theRust[i])
                    i++;
                size_t theStart = theOrig.rfind('\n', i == 0 ? 0 : i - 1);
                theStart = theStart == std::string::npos ? 0 : theStart + 1;
                printf("sequence %d differs:\n  orig: %.300s\n  rust: %.300s\n", s + 1, theOrig.c_str() + theStart,
                       theRust.c_str() + theStart);
            }
        }
    }
    if (thePrint != NULL) {
        FILE* theFile = fopen(thePrint, "wb");
        fwrite(theGolden.data(), 1, theGolden.size(), theFile);
        fclose(theFile);
    }
    for (auto& k : theKinds)
        if (k.second > theCount / 2)
            printf("  %-24s %d\n", k.first.c_str(), k.second);
    printf("ne_queues_test: %d sequences, %zu trace lines, %d differing\n", theCount, theLines, theDiffering);
    return theDiffering == 0 ? 0 : 1;
}
