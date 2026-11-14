// ne_queues_test's world: the clock, events, threads, messages and files the stand-in headers share, and the
// recorded ops of the Rust side. Every effect is a line of the side's trace, prefixed with the thread that made it.
//
// The threads are real threads that run one at a time: the running one keeps the baton until it waits on an event
// that is not set (or its body ends), and the world then hands it on, by a fixed rule: the lowest-numbered thread
// that can run (the main thread is 0; a new thread can run; a waiter can when its event was set), else the main
// thread when it only yields (between the driver's steps), else the clock moves to the earliest finite deadline and
// that waiter times out, else (every thread waits forever) the highest-numbered waiter's wait fails. Two sides that
// make the same calls therefore interleave the same way.
#pragma once
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class NEMessage;

enum { kQWaitOK = 0, kQWaitTimeout = 0x11, kQWaitFailed = 0x15 };
const unsigned long kQInfinite = 0xffffffffu;

struct QFile {
    std::vector<unsigned char> mData;
    bool mExists = false;
    bool mOpen = false;
    size_t mPos = 0;
};

struct QWorld {
    enum { kNew, kReady, kRunning, kBlocked, kYield, kDone };
    struct Thread {
        int mState = kRunning;
        int mEvent = 0;
        bool mInfinite = false;
        unsigned long mDeadline = 0;
        long mWake = 0;
        std::vector<int> mWaitSet;        // a WaitAny's events (mEvent is then 0)
        long mWakeIndex = 0;              // the index of the event whose Set woke it
        std::thread mThread;
    };

    std::string mTrace;
    uint32_t mRandom = 1;
    unsigned long mClock = 1000;
    unsigned long mPrecision = 0x10000;   // kFPPrecision53's value in the stand-in
    unsigned long mRounding = 0;
    std::map<int, NEMessage*> mMessages;
    int mNextMessage = 1;
    std::map<int, bool> mEvents;          // event -> set
    int mNextEvent = 1;
    std::map<int, QFile> mFiles;
    std::map<int, Thread*> mThreads;      // 0 is the main thread
    int mNextThread = 1;
    int mCurrent = 0;
    std::mutex mMutex;
    std::condition_variable mTurn;
    // the Rust side's op record (cargo's replay)
    bool mRecord = false;
    bool mEcho = false;
    std::string mOps;

    QWorld() { this->mThreads[0] = new Thread; }
    ~QWorld()
    {
        for (auto& t : this->mThreads) {
            if (t.second->mThread.joinable())
                t.second->mThread.join();
            delete t.second;
        }
    }

    void Line(const char* inFormat, ...)
    {
        char theLine[512];
        va_list theArgs;
        va_start(theArgs, inFormat);
        vsnprintf(theLine, sizeof(theLine), inFormat, theArgs);
        va_end(theArgs);
        this->mTrace += "t" + std::to_string(this->mCurrent) + " " + theLine + "\n";
        if (this->mEcho) {
            fprintf(stderr, "t%d %s\n", this->mCurrent, theLine);
            fflush(stderr);
        }
    }
    void Op(const std::string& inLine)
    {
        if (this->mRecord)
            this->mOps += inLine + "\n";
    }
    uint32_t Next()
    {
        this->mRandom = this->mRandom * 1103515245u + 12345u;
        return this->mRandom >> 8;
    }
    int IdOf(NEMessage* inMessage);

    // ---- events
    int NewEvent()
    {
        int theID = this->mNextEvent++;
        this->mEvents[theID] = false;
        this->Line("event new e%d", theID);
        return theID;
    }
    void FreeEvent(int inID)
    {
        this->Line("event free e%d", inID);
        this->mEvents.erase(inID);
    }
    void Set(int inID)
    {
        this->Line("event set e%d", inID);
        this->mEvents[inID] = true;
        for (auto& t : this->mThreads) {
            if (t.second->mState != kBlocked)
                continue;
            if (t.second->mEvent == inID) {
                t.second->mState = kReady;
                t.second->mWake = kQWaitOK;
            }
            for (size_t i = 0; t.second->mEvent == 0 && i < t.second->mWaitSet.size(); i++)
                if (t.second->mWaitSet[i] == inID) {
                    t.second->mState = kReady;
                    t.second->mWake = kQWaitOK;
                    t.second->mWakeIndex = (long)i;
                    break;
                }
        }
    }
    void Clear(int inID)
    {
        this->Line("event clear e%d", inID);
        this->mEvents[inID] = false;
    }

    // ---- the baton
    // the next thread to run after `inMe` stopped running (its state says why)
    int PickNext()
    {
        for (auto& t : this->mThreads)
            if (t.second->mState == kNew || t.second->mState == kReady)
                return t.first;
        if (this->mThreads[0]->mState == kYield)
            return 0;
        int theEarliest = -1;
        for (auto& t : this->mThreads)
            if (t.second->mState == kBlocked && !t.second->mInfinite &&
                (theEarliest < 0 || t.second->mDeadline < this->mThreads[theEarliest]->mDeadline))
                theEarliest = t.first;
        if (theEarliest >= 0) {
            Thread* t = this->mThreads[theEarliest];
            if (t->mDeadline > this->mClock)
                this->mClock = t->mDeadline;
            t->mState = kReady;
            t->mWake = kQWaitTimeout;
            return theEarliest;
        }
        for (auto i = this->mThreads.rbegin(); i != this->mThreads.rend(); ++i)
            if (i->second->mState == kBlocked) {
                i->second->mState = kReady;
                i->second->mWake = kQWaitFailed;
                return i->first;
            }
        fprintf(stderr, "ne_queues_test: no thread can run\n");
        abort();
    }
    // hands the baton from the running thread to the next one; returns when it is the caller's again (not for a
    // thread whose body ended)
    void Switch(bool inDone)
    {
        int theMe = this->mCurrent;
        int theNext = this->PickNext();
        if (theNext == theMe) {
            this->mThreads[theMe]->mState = kRunning;
            return;
        }
        this->Op("sw " + std::to_string(theNext));
        std::unique_lock<std::mutex> theLock(this->mMutex);
        this->mCurrent = theNext;
        this->mThreads[theNext]->mState = kRunning;
        this->mTurn.notify_all();
        if (inDone)
            return;
        this->mTurn.wait(theLock, [&] { return this->mCurrent == theMe; });
    }
    long Wait(int inEvent, unsigned long inTimeout)
    {
        long theResult;
        if (this->mEvents[inEvent])
            theResult = kQWaitOK;
        else if (inTimeout == 0)
            theResult = kQWaitTimeout;
        else {
            Thread* t = this->mThreads[this->mCurrent];
            t->mState = kBlocked;
            t->mEvent = inEvent;
            t->mInfinite = inTimeout == kQInfinite;
            t->mDeadline = this->mClock + inTimeout;
            this->Switch(false);
            theResult = t->mWake;
        }
        this->Line("event wait e%d %lu = %ld", inEvent, inTimeout, theResult);
        return theResult;
    }
    // several events (ne_client_test): the index of the lowest set one when the wait ends, -1 (timed out) or -2
    // (failed); a waiter woken by a Set whose event was cleared before it ran answers that event's index
    long WaitAny(const std::vector<int>& inEvents, unsigned long inTimeout)
    {
        long theResult = -3;
        for (size_t i = 0; theResult == -3 && i < inEvents.size(); i++)
            if (this->mEvents[inEvents[i]])
                theResult = (long)i;
        if (theResult == -3 && inTimeout == 0)
            theResult = -1;
        if (theResult == -3) {
            Thread* t = this->mThreads[this->mCurrent];
            t->mState = kBlocked;
            t->mEvent = 0;
            t->mWaitSet = inEvents;
            t->mInfinite = inTimeout == kQInfinite;
            t->mDeadline = this->mClock + inTimeout;
            this->Switch(false);
            t->mWaitSet.clear();
            if (t->mWake == kQWaitTimeout)
                theResult = -1;
            else if (t->mWake != kQWaitOK)
                theResult = -2;
            else {
                theResult = t->mWakeIndex;
                for (size_t i = 0; i < inEvents.size(); i++)
                    if (this->mEvents[inEvents[i]]) {
                        theResult = (long)i;
                        break;
                    }
            }
        }
        std::string theList;
        for (int e : inEvents)
            theList += (theList.empty() ? "e" : ",e") + std::to_string(e);
        this->Line("event waitany %s %lu = %ld", theList.c_str(), inTimeout, theResult);
        return theResult;
    }
    // a new thread: it can run from now, its body called with its argument
    int NewThread(std::function<unsigned long()> inBody)
    {
        int theID = this->mNextThread++;
        Thread* t = new Thread;
        t->mState = kNew;
        this->mThreads[theID] = t;
        t->mThread = std::thread([this, theID, inBody] {
            {
                std::unique_lock<std::mutex> theLock(this->mMutex);
                this->mTurn.wait(theLock, [&] { return this->mCurrent == theID; });
            }
            unsigned long theResult = inBody();
            this->Line("thread end = %lu", theResult);
            this->Op("end");
            this->mThreads[theID]->mState = kDone;
            this->Switch(true);
        });
        this->Line("thread new t%d", theID);
        return theID;
    }
    void FreeThread(int inID)
    {
        Thread* t = this->mThreads[inID];
        this->Line("thread free t%d %s", inID, t->mState == kDone ? "done" : "LIVE");
        if (t->mState != kDone) {
            fprintf(stderr, "ne_queues_test: a live thread freed\n");
            abort();
        }
        t->mThread.join();
        delete t;
        this->mThreads.erase(inID);
    }
    // the main thread lets the others run until each waits or ends (the clock does not move)
    void LetRun()
    {
        this->mThreads[0]->mState = kYield;
        this->Switch(false);
    }
    // the clock moves on by inTime, the waiters whose deadline comes first running at their deadline
    void Pass(unsigned long inTime)
    {
        unsigned long theEnd = this->mClock + inTime;
        Thread* theMain = this->mThreads[0];
        theMain->mState = kBlocked;
        theMain->mEvent = 0;
        theMain->mInfinite = false;
        theMain->mDeadline = theEnd;
        this->Switch(false);
        this->LetRun();
        this->Line("clock %lu", this->mClock);
    }

    // ---- files
    long FileCreate(int inID)
    {
        QFile& f = this->mFiles[inID];
        long r = f.mExists ? 2 : (this->Next() % 16 == 0 ? 9 : 0);
        if (r == 0) {
            f.mExists = true;
            f.mData.clear();
            f.mPos = 0;
        }
        this->Line("file create f%d = %ld", inID, r);
        return r;
    }
    long FileDelete(int inID)
    {
        QFile& f = this->mFiles[inID];
        long r = f.mExists && !f.mOpen ? 0 : 9;
        if (r == 0) {
            f.mExists = false;
            f.mData.clear();
        }
        this->Line("file delete f%d = %ld", inID, r);
        return r;
    }
    long FileOpen(int inID, unsigned long inAccess)
    {
        QFile& f = this->mFiles[inID];
        long r = f.mExists ? (this->Next() % 16 == 0 ? 9 : 0) : 9;
        if (r == 0) {
            f.mOpen = true;
            f.mPos = 0;
        }
        this->Line("file open f%d %lu = %ld", inID, inAccess, r);
        return r;
    }
    void FileClose(int inID)
    {
        this->mFiles[inID].mOpen = false;
        this->Line("file close f%d", inID);
    }
    long FileWrite(int inID, const unsigned char* inData, unsigned long inSize)
    {
        QFile& f = this->mFiles[inID];
        std::string theHex;
        char theByte[4];
        for (unsigned long i = 0; i < inSize; i++) {
            snprintf(theByte, sizeof(theByte), "%02x", inData[i]);
            theHex += theByte;
        }
        long r = f.mOpen ? 0 : 4;
        if (r == 0) {
            if (f.mData.size() < f.mPos + inSize)
                f.mData.resize(f.mPos + inSize);
            memcpy(f.mData.data() + f.mPos, inData, inSize);
            f.mPos += inSize;
        }
        this->Line("file write f%d %s = %ld", inID, theHex.c_str(), r);
        return r;
    }
    long FileReadU32(int inID, uint32_t& ioValue)
    {
        QFile& f = this->mFiles[inID];
        long r = !f.mOpen ? 4 : f.mPos + 4 > f.mData.size() ? 0x10 : 0;
        if (r == 0) {
            memcpy(&ioValue, f.mData.data() + f.mPos, 4);
            f.mPos += 4;
        }
        this->Line("file read32 f%d = %ld %lu", inID, r, (unsigned long)ioValue);
        return r;
    }
    // the buffer is always filled (zeros past the end): both sides then convert the same bytes
    long FileRead(int inID, unsigned char* outData, unsigned long inSize)
    {
        QFile& f = this->mFiles[inID];
        memset(outData, 0, inSize);
        long r = 4;
        unsigned long theCount = 0;
        if (f.mOpen) {
            theCount = f.mData.size() - f.mPos < inSize ? (unsigned long)(f.mData.size() - f.mPos) : inSize;
            memcpy(outData, f.mData.data() + f.mPos, theCount);
            f.mPos += theCount;
            r = theCount == inSize ? 0 : 0x10;
        }
        this->Line("file read f%d %lu = %ld %lu", inID, inSize, r, theCount);
        return r;
    }
};

extern QWorld* gQ;
