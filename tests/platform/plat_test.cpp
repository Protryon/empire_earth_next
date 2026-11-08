// Tests of the platform layer (include/platform/), the same source on every system: built and run by
// `python -I tools/xbuild.py test` against src/Platform/Win32 (Windows) or src/Platform/Posix (Linux, macOS).
// Prints one line per failed check and a summary; the exit code is the number of failed checks (capped at 100).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <atomic>
#include <string>
#include <vector>

#include "platform/PlatFile.h"
#include "platform/PlatMessage.h"
#include "platform/PlatPath.h"
#include "platform/PlatConfig.h"
#include "platform/PlatSocket.h"
#include "platform/PlatString.h"
#include "platform/PlatThread.h"
#include "platform/PlatTime.h"

#if defined(_WIN32)
#include <process.h>
#define PLAT_TEST_GETPID _getpid
#else
#include <unistd.h>
#define PLAT_TEST_GETPID getpid
#endif

static int sChecks;
static int sFailures;
static const char* sSection = "";

#define CHECK(c) Check((c), #c, __LINE__)

static void Check(bool inOk, const char* inText, int inLine)
{
    sChecks++;
    if (!inOk) {
        sFailures++;
        printf("FAIL [%s] line %d: %s (last error %u, socket error %u)\n", sSection, inLine, inText,
               PlatGetLastError(), PlatSocketLastError());
        fflush(stdout);
    }
}

static void Section(const char* inName)
{
    sSection = inName;
    printf("-- %s\n", inName);
    fflush(stdout);
}

// ---- time

static std::atomic<int> sTicks;
static void Tick(void*)
{
    sTicks++;
}

static void TestTime()
{
    Section("time");
    CHECK(PlatCounterFrequency() > 0);
    int64_t c0 = PlatCounterTicks();
    uint32_t t0 = PlatTimeMilliseconds();
    PlatSleep(50);
    uint32_t theElapsed = PlatTimeMilliseconds() - t0;
    int64_t theCounted = (PlatCounterTicks() - c0) * 1000 / PlatCounterFrequency();
    CHECK(theElapsed >= 40 && theElapsed < 1000);
    CHECK(theCounted >= 40 && theCounted < 1000);
    PlatSleep(0);
    PlatTimeBeginPeriod(1);
    PlatTimeEndPeriod(1);
    PlatDateTime theNow;
    PlatGetLocalTime(&theNow);
    CHECK(theNow.mYear >= 2020 && theNow.mMonth >= 1 && theNow.mMonth <= 12 && theNow.mDay >= 1 && theNow.mDay <= 31);
    CHECK(theNow.mDayOfWeek <= 6 && theNow.mHour <= 23 && theNow.mMinute <= 59 && theNow.mMilliseconds <= 999);
    // the wall clock: FILETIME units after 2020-01-01 (132223104000000000), moving with the sleep (it may be set)
    uint64_t theWall0 = PlatTimeWallClock100ns();
    PlatSleep(30);
    uint64_t theWall1 = PlatTimeWallClock100ns();
    CHECK(theWall0 > 132223104000000000ULL);
    CHECK(theWall1 >= theWall0 + 200000ULL && theWall1 - theWall0 < 600000000ULL);   // 20 ms .. 60 s

    Section("timers");
    sTicks = 0;
    uint32_t thePeriodic = PlatTimerStartCallback(10, 1, Tick, NULL);
    CHECK(thePeriodic != 0);
    PlatSleep(200);
    CHECK(PlatTimerStop(thePeriodic) == 1);
    int theCount = sTicks;
    CHECK(theCount >= 5 && theCount <= 30);
    PlatSleep(50);
    CHECK(sTicks - theCount <= 1);   // stopped (one may have been in flight)
    CHECK(PlatTimerStop(thePeriodic) == 0);

    PlatEvent* theEvent = PlatEventCreate(0, 0);
    uint32_t theOneShot = PlatTimerStartEvent(30, 0, theEvent);
    CHECK(theOneShot != 0);
    CHECK(PlatEventWait(theEvent, 0) == PLAT_WAIT_TIMEOUT);
    CHECK(PlatEventWait(theEvent, 2000) == 0);
    PlatSleep(20);
    CHECK(PlatTimerStop(theOneShot) == 0 || PlatTimerStop(theOneShot) == 0);   // a fired one-shot is gone (Win32 may
                                                                                // still know it: either way not 1 twice)
    PlatEventDestroy(theEvent);
}

// ---- threads and synchronisation

struct ThreadArgs {
    PlatMutex* mMutex;
    PlatEvent* mGo;
    PlatEvent* mDone;
    volatile int32_t* mCounter;
    int mResult;
};

static uint32_t TryLockElsewhere(void* inArgs)
{
    ThreadArgs* a = static_cast<ThreadArgs*>(inArgs);
    a->mResult = PlatMutexTryLock(a->mMutex);
    if (a->mResult)
        PlatMutexUnlock(a->mMutex);
    return 7;
}

static uint32_t CountUp(void* inArgs)
{
    ThreadArgs* a = static_cast<ThreadArgs*>(inArgs);
    PlatEventWait(a->mGo, PLAT_WAIT_INFINITE);
    for (int i = 0; i < 20000; i++)
        PlatAtomicIncrement(a->mCounter);
    for (int i = 0; i < 5000; i++)
        PlatAtomicDecrement(a->mCounter);
    PlatEventSet(a->mDone);
    return 0;
}

static uint32_t SetLater(void* inEvent)
{
    PlatSleep(30);
    PlatEventSet(static_cast<PlatEvent*>(inEvent));
    return 0;
}

static std::atomic<int> sStarted;
static uint32_t MarkStarted(void*)
{
    sStarted = 1;
    return 42;
}

static void WaitExit(PlatThread* inThread, uint32_t& outCode)
{
    for (int i = 0; i < 500; i++) {
        if (PlatThreadGetExitCode(inThread, &outCode) && outCode != PLAT_THREAD_STILL_ACTIVE)
            return;
        PlatSleep(5);
    }
}

static void TestThreads()
{
    Section("mutex");
    PlatMutex* theMutex = PlatMutexCreate();
    CHECK(theMutex != NULL);
    PlatMutexLock(theMutex);
    PlatMutexLock(theMutex);   // recursive
    CHECK(PlatMutexTryLock(theMutex) == 1);
    ThreadArgs theArgs = {theMutex, NULL, NULL, NULL, -1};
    uint32_t theID = 0;
    PlatThread* theThread = PlatThreadCreate(TryLockElsewhere, &theArgs, 0, 0, &theID);
    CHECK(theThread != NULL && theID != 0 && theID != PlatCurrentThreadId());
    uint32_t theCode = 0;
    WaitExit(theThread, theCode);
    CHECK(theCode == 7);
    CHECK(theArgs.mResult == 0);   // held here
    PlatThreadClose(theThread);
    PlatMutexUnlock(theMutex);
    PlatMutexUnlock(theMutex);
    PlatMutexUnlock(theMutex);
    theArgs.mResult = -1;
    theThread = PlatThreadCreate(TryLockElsewhere, &theArgs, 64 * 1024, 0, &theID);
    WaitExit(theThread, theCode);
    CHECK(theArgs.mResult == 1);   // free now
    PlatThreadClose(theThread);
    PlatMutexDestroy(theMutex);

    Section("events");
    PlatEvent* theAuto = PlatEventCreate(0, 1);
    PlatEvent* theManual = PlatEventCreate(1, 0);
    CHECK(theAuto && theManual);
    CHECK(PlatEventWait(theAuto, 0) == 0);
    CHECK(PlatEventWait(theAuto, 0) == PLAT_WAIT_TIMEOUT);   // cleared by the wait it released
    uint32_t t0 = PlatTimeMilliseconds();
    CHECK(PlatEventWait(theAuto, 60) == PLAT_WAIT_TIMEOUT);
    CHECK(PlatTimeMilliseconds() - t0 >= 50);
    CHECK(PlatEventSet(theManual) == 1);
    CHECK(PlatEventWait(theManual, 0) == 0);
    CHECK(PlatEventWait(theManual, 0) == 0);   // stays set
    PlatEvent* theEvents[3] = {theAuto, theManual, NULL};
    theEvents[2] = PlatEventCreate(0, 1);
    CHECK(PlatEventWaitAny(theEvents, 3, 0) == 1);   // the lowest set one
    CHECK(PlatEventReset(theManual) == 1);
    CHECK(PlatEventWaitAny(theEvents, 3, 0) == 2);
    CHECK(PlatEventWaitAny(theEvents, 3, 0) == PLAT_WAIT_TIMEOUT);
    theThread = PlatThreadCreate(SetLater, theAuto, 0, 0, &theID);
    CHECK(PlatEventWaitAny(theEvents, 3, 5000) == 0);
    WaitExit(theThread, theCode);
    PlatThreadClose(theThread);
    CHECK(PlatEventWaitAny(theEvents, 0, 0) == PLAT_WAIT_FAILED);

    Section("atomics");
    volatile int32_t theCounter = 0;
    PlatEvent* theGo = PlatEventCreate(1, 0);
    PlatEvent* theDone[4];
    PlatThread* theWorkers[4];
    ThreadArgs theWorkerArgs[4];
    for (int i = 0; i < 4; i++) {
        theDone[i] = PlatEventCreate(1, 0);
        theWorkerArgs[i] = ThreadArgs{NULL, theGo, theDone[i], &theCounter, 0};
        theWorkers[i] = PlatThreadCreate(CountUp, &theWorkerArgs[i], 0, 0, &theID);
        CHECK(theWorkers[i] != NULL);
    }
    PlatEventSet(theGo);
    for (int i = 0; i < 4; i++) {
        CHECK(PlatEventWait(theDone[i], 10000) == 0);
        WaitExit(theWorkers[i], theCode);
        PlatThreadClose(theWorkers[i]);
        PlatEventDestroy(theDone[i]);
    }
    CHECK(theCounter == 4 * 15000);
    PlatEventDestroy(theGo);

    Section("threads");
    sStarted = 0;
    theThread = PlatThreadCreate(MarkStarted, NULL, 0, 1, &theID);
    CHECK(theThread != NULL);
    PlatSleep(30);
    CHECK(sStarted == 0);   // held
    CHECK(PlatThreadGetExitCode(theThread, &theCode) && theCode == PLAT_THREAD_STILL_ACTIVE);
    CHECK(PlatThreadSetPriority(theThread, PLAT_PRIORITY_BELOW_NORMAL) == 1);
    CHECK(PlatThreadGetPriority(theThread) == PLAT_PRIORITY_BELOW_NORMAL);
    CHECK(PlatThreadWait(theThread, 0) == PLAT_WAIT_TIMEOUT);   // held, so not ended
    CHECK(PlatThreadWait(theThread, 30) == PLAT_WAIT_TIMEOUT);
    CHECK(PlatThreadResume(theThread) == 1);
    CHECK(PlatThreadWait(theThread, 5000) == 0);
    CHECK(PlatThreadWait(theThread, 0) == 0);   // stays ended
    WaitExit(theThread, theCode);
    CHECK(sStarted == 1 && theCode == 42);
    CHECK(PlatThreadResume(theThread) == 0 || PlatThreadResume(theThread) == 0xFFFFFFFFu);   // not suspended
    PlatThreadClose(theThread);
    PlatThread* theSelf = PlatThreadOpenCurrent();
    CHECK(theSelf != NULL);
    CHECK(PlatThreadGetPriority(theSelf) != PLAT_PRIORITY_ERROR);
    CHECK(PlatThreadGetExitCode(theSelf, &theCode) && theCode == PLAT_THREAD_STILL_ACTIVE);
    PlatThreadClose(theSelf);

    PlatEventDestroy(theAuto);
    PlatEventDestroy(theManual);
    PlatEventDestroy(theEvents[2]);
}

// ---- strings

static void TestStrings()
{
    Section("strings");
    CHECK(PlatStrICmp("Hello", "hELLO") == 0);
    CHECK(PlatStrICmp("abc", "ABD") < 0);
    CHECK(PlatStrICmp("b", "A") > 0);
    CHECK(PlatStrICmp("_", "a") < 0);   // _stricmp lower-cases: '_' (0x5f) < 'a'
    CHECK(PlatStrNICmp("Data\\Units", "DATA\\Other", 5) == 0);
    CHECK(PlatStrNICmp("Data\\Units", "DATA\\Other", 6) > 0);
    char theBuffer[40];
    CHECK(strcmp(PlatIToA(-123, theBuffer, 10), "-123") == 0);
    CHECK(strcmp(PlatIToA(255, theBuffer, 16), "ff") == 0);
    CHECK(strcmp(PlatIToA(-1, theBuffer, 16), "ffffffff") == 0);
    CHECK(strcmp(PlatIToA(5, theBuffer, 2), "101") == 0);
    char* theCopy = PlatStrDup("copy");
    CHECK(theCopy && strcmp(theCopy, "copy") == 0);
    free(theCopy);

    const unsigned char* s = reinterpret_cast<const unsigned char*>("Empire Earth");
    CHECK(PlatMbsLen(s) == 12);
    CHECK(PlatMbsNBCnt(s, 6) == 6 && PlatMbsNBCnt(s, 100) == 12);
    CHECK(PlatMbsNCCnt(s, 3) == 3);
    CHECK(PlatMbsInc(s) == s + 1 && PlatMbsNInc(s, 7) == s + 7 && PlatMbsNInc(s, 99) == s + 12);
    CHECK(PlatMbsDec(s, s + 3) == s + 2 && PlatMbsDec(s, s) == NULL);
    const unsigned char* u = reinterpret_cast<const unsigned char*>("EMPIRE EARTH");
    CHECK(PlatMbsCmp(s, u) > 0 && PlatMbsICmp(s, u) == 0);
    CHECK(PlatMbsNCmp(s, u, 1) == 0 && PlatMbsNBCmp(s, u, 2) > 0 && PlatMbsNBICmp(s, u, 12) == 0);
    unsigned char theText[32];
    memset(theText, 'x', sizeof(theText));
    PlatMbsNCpy(theText, s, 20);
    CHECK(memcmp(theText, "Empire Earth\0\0\0\0\0\0\0\0x", 21) == 0);   // zero-padded to 20
    PlatMbsNBCat(theText, reinterpret_cast<const unsigned char*>(": AoC!"), 5);
    CHECK(strcmp(reinterpret_cast<char*>(theText), "Empire Earth: AoC") == 0);
    CHECK(PlatMbsChr(theText, 'E') == theText && PlatMbsChr(theText, ':') == theText + 12);
    CHECK(PlatMbsChr(theText, 'z') == NULL);
    PlatMbsUpr(theText);
    CHECK(strcmp(reinterpret_cast<char*>(theText), "EMPIRE EARTH: AOC") == 0);
    PlatMbsLwr(theText);
    CHECK(strcmp(reinterpret_cast<char*>(theText), "empire earth: aoc") == 0);
    CHECK(PlatIsMbcSpace(' ') && PlatIsMbcSpace('\t') && !PlatIsMbcSpace('x'));
    CHECK(PlatIsMbcDigit('7') && !PlatIsMbcDigit('a'));
    CHECK(!PlatIsMbbLead('A'));

    Section("cp1252");
    // every byte round-trips (the five undefined ones as the C1 controls)
    char theAll[256];
    for (int i = 0; i < 256; i++)
        theAll[i] = static_cast<char>(i);
    wchar_t theWide[260];
    CHECK(PlatCp1252ToWide(theAll, 256, NULL, 0) == 256);
    CHECK(PlatCp1252ToWide(theAll, 256, theWide, 260) == 256 && theWide[256] == 0);
    CHECK(theWide[0] == 0 && theWide['A'] == L'A' && theWide[0x80] == 0x20AC && theWide[0x81] == 0x81
          && theWide[0x9F] == 0x0178 && theWide[0xE9] == 0xE9 && theWide[0xFF] == 0xFF);
    char theBack[260];
    CHECK(PlatWideToCp1252(theWide, 256, theBack, 260) == 256 && memcmp(theBack, theAll, 256) == 0
          && theBack[256] == 0);
    // terminated texts, capacity, unmappable characters
    CHECK(PlatCp1252ToWide("caf\xe9 \x80", PLAT_TEXT_TERMINATED, theWide, 260) == 6 && theWide[3] == 0xE9
          && theWide[5] == 0x20AC && theWide[6] == 0);
    // no room for the terminator: none written
    CHECK(PlatCp1252ToWide("abcdef", PLAT_TEXT_TERMINATED, theWide, 3) == 3 && theWide[2] == L'c');
    const wchar_t theOdd[] = {L'x', 0x4E2D, 0x2122, 0x0153, L'y', 0};
    CHECK(PlatWideToCp1252(theOdd, PLAT_TEXT_TERMINATED, theBack, 260) == 5
          && strcmp(theBack, "x?\x99\x9cy") == 0);
    CHECK(PlatWideToCp1252(theOdd, PLAT_TEXT_TERMINATED, NULL, 0) == 5);
    CHECK(PlatWideToCp1252(theOdd, PLAT_TEXT_TERMINATED, theBack, 2) == 2 && theBack[0] == 'x' && theBack[1] == '?');
    const wchar_t thePair[] = {L'a', static_cast<wchar_t>(0xD83D), static_cast<wchar_t>(0xDE00), L'b', 0};
    CHECK(PlatWideToCp1252(thePair, PLAT_TEXT_TERMINATED, theBack, 260) == 3 && strcmp(theBack, "a?b") == 0);
}

// ---- files and paths

static std::string sRoot;   // the test's directory (game form, trailing separator)

static bool WriteAll(const std::string& inPath, const char* inText)
{
    PlatFile* f = PlatFileOpen(inPath.c_str(), PLAT_FILE_WRITE);
    if (!f)
        return false;
    uint32_t theWritten = 0;
    int ok = PlatFileWrite(f, inText, static_cast<uint32_t>(strlen(inText)), &theWritten);
    PlatFileClose(f);
    return ok && theWritten == strlen(inText);
}

static void TestFiles()
{
    Section("paths");
    char theTemp[PLAT_MAX_PATH];
    uint32_t n = PlatGetTempDirectory(theTemp, PLAT_MAX_PATH);
    CHECK(n > 3 && theTemp[1] == ':' && theTemp[n - 1] == '\\');
    char theCurrent[PLAT_MAX_PATH];
    CHECK(PlatGetCurrentDirectory(theCurrent, PLAT_MAX_PATH) > 2 && theCurrent[1] == ':');
    char theNative[1024];
    CHECK(PlatPathToNative(theCurrent, theNative, sizeof(theNative)) > 0);
    char theBack[PLAT_MAX_PATH];
    CHECK(PlatPathFromNative(theNative, theBack, PLAT_MAX_PATH) > 0 && PlatStrICmp(theBack, theCurrent) == 0);
    char theData[PLAT_MAX_PATH];
    CHECK(PlatPathGetDataRoot(theData, PLAT_MAX_PATH) > 2);
    CHECK(PlatPathToNative("", theNative, sizeof(theNative)) == 0);

    Section("directories");
    char theName[64];
    snprintf(theName, sizeof(theName), "PlatTest%d\\", static_cast<int>(PLAT_TEST_GETPID()));
    sRoot = std::string(theTemp) + theName;
    CHECK(PlatCreateDirectory(sRoot.c_str()) == PLAT_FILE_OK);
    CHECK(PlatCreateDirectory(sRoot.c_str()) == PLAT_FILE_EXISTS);
    CHECK(PlatCreateDirectory((sRoot + "Data").c_str()) == PLAT_FILE_OK);
    CHECK(PlatCreateDirectory((sRoot + "data\\Sub").c_str()) == PLAT_FILE_OK);   // another case of "Data"
    CHECK(PlatCreateDirectory((sRoot + "Missing\\Sub").c_str()) == PLAT_FILE_NATIVE_ERROR);
    CHECK(PlatGetLastError() == 3);   // ERROR_PATH_NOT_FOUND
    uint32_t theAttributes = PlatPathGetAttributes((sRoot + "DATA\\sub").c_str());
    CHECK(theAttributes != PLAT_ATTR_INVALID && (theAttributes & PLAT_ATTR_DIRECTORY));
    CHECK(PlatPathGetAttributes((sRoot + "nothing.txt").c_str()) == PLAT_ATTR_INVALID);
    CHECK(PlatGetLastError() == 2);   // ERROR_FILE_NOT_FOUND

    Section("files");
    std::string theFile = sRoot + "Data\\Sub\\File.TXT";
    CHECK(PlatFileCreateNew(theFile.c_str()) == PLAT_FILE_OK);
    CHECK(PlatFileCreateNew((sRoot + "data\\SUB\\file.txt").c_str()) == PLAT_FILE_EXISTS);   // any case
    CHECK(PlatFileCreateNew((sRoot + "Nope\\file.txt").c_str()) == PLAT_FILE_BAD_PATH);
    std::string theOther = sRoot + "DATA\\sub\\file.txt";   // the same file through another case
    CHECK(WriteAll(theOther, "Hello, Empire Earth"));
    PlatFile* f = PlatFileOpen(theFile.c_str(), PLAT_FILE_READ | PLAT_FILE_WRITE);
    CHECK(f != NULL);
    if (f) {
        CHECK(PlatFileGetSize(f) == 19);
        char theBuffer[64] = {0};
        uint32_t theRead = 0;
        CHECK(PlatFileRead(f, theBuffer, 5, &theRead) && theRead == 5 && memcmp(theBuffer, "Hello", 5) == 0);
        CHECK(PlatFileSeek(f, 7, PLAT_SEEK_BEGIN) == 7);
        CHECK(PlatFileRead(f, theBuffer, 64, &theRead) && theRead == 12 && memcmp(theBuffer, "Empire Earth", 12) == 0);
        CHECK(PlatFileRead(f, theBuffer, 64, &theRead) && theRead == 0);   // end of file
        CHECK(PlatFileSeek(f, -5, PLAT_SEEK_END) == 14);
        CHECK(PlatFileSeek(f, 2, PLAT_SEEK_CURRENT) == 16);
        CHECK(PlatFileSeek(f, -100, PLAT_SEEK_CURRENT) == 0xFFFFFFFFu);
        CHECK(PlatFileSeek(f, 5, PLAT_SEEK_BEGIN) == 5 && PlatFileTruncate(f));
        CHECK(PlatFileGetSize(f) == 5);
        uint64_t theTime = 0;
        CHECK(PlatFileGetWriteTime(f, &theTime) && theTime > 116444736000000000ULL);   // after 1970
        CHECK(PlatFileClose(f));
    }
    f = PlatFileOpen(theFile.c_str(), PLAT_FILE_READ);
    CHECK(f != NULL);
    if (f) {
        const void* theView = NULL;
        PlatFileMapping* theMapping = PlatFileMap(f, &theView);
        CHECK(theMapping != NULL && theView != NULL && memcmp(theView, "Hello", 5) == 0);
        if (theMapping)
            PlatFileUnmap(theMapping, theView);
        PlatFileClose(f);
    }
    CHECK(PlatFileOpen((sRoot + "Data\\Sub\\absent.txt").c_str(), PLAT_FILE_READ) == NULL);
    CHECK(PlatGetLastError() == 2);
    CHECK(PlatFileOpen((sRoot + "Data").c_str(), PLAT_FILE_READ) == NULL);   // a directory

    std::string theCopy = sRoot + "data\\copy.txt";
    CHECK(PlatCopyFile(theOther.c_str(), theCopy.c_str(), 1));
    CHECK(!PlatCopyFile(theOther.c_str(), theCopy.c_str(), 1));
    CHECK(PlatCopyFile(theOther.c_str(), theCopy.c_str(), 0));
    CHECK(PlatPathSetAttributes(theCopy.c_str(), PLAT_ATTR_READONLY));
    CHECK(PlatPathGetAttributes(theCopy.c_str()) & PLAT_ATTR_READONLY);
    CHECK(!PlatDeleteFile(theCopy.c_str()));   // read-only
    CHECK(PlatPathSetAttributes(theCopy.c_str(), PLAT_ATTR_NORMAL));
    CHECK(!(PlatPathGetAttributes(theCopy.c_str()) & PLAT_ATTR_READONLY));
    CHECK(PlatFileCreateNew((sRoot + "Data\\b.dat").c_str()) == PLAT_FILE_OK);
    CHECK(PlatFileCreateNew((sRoot + "Data\\A.dat").c_str()) == PLAT_FILE_OK);

    Section("find");
    PlatFindData theEntry;
    PlatFind* theFind = PlatFindFirst((sRoot + "DATA\\*.*").c_str(), &theEntry);
    CHECK(theFind != NULL);
    std::vector<std::string> theNames;
    if (theFind) {
        do {
            if (strcmp(theEntry.mName, ".") != 0 && strcmp(theEntry.mName, "..") != 0)
                theNames.push_back(theEntry.mName);
        } while (PlatFindNext(theFind, &theEntry) == PLAT_FILE_OK);
        CHECK(PlatGetLastError() == 18);   // ERROR_NO_MORE_FILES
        PlatFindClose(theFind);
    }
    CHECK(theNames.size() == 4);
    if (theNames.size() == 4)   // NTFS's order: case-insensitive
        CHECK(theNames[0] == "A.dat" && theNames[1] == "b.dat" && theNames[2] == "copy.txt" && theNames[3] == "Sub");
    theFind = PlatFindFirst((sRoot + "data\\*.DAT").c_str(), &theEntry);
    int theDat = 0;
    if (theFind) {
        do
            theDat++;
        while (PlatFindNext(theFind, &theEntry) == PLAT_FILE_OK);
        PlatFindClose(theFind);
    }
    CHECK(theDat == 2);
    theFind = PlatFindFirst((sRoot + "data\\sub").c_str(), &theEntry);   // a plain name finds the entry itself
    CHECK(theFind != NULL && (theEntry.mAttributes & PLAT_ATTR_DIRECTORY));
    if (theFind)
        PlatFindClose(theFind);
    CHECK(PlatFindFirst((sRoot + "data\\*.none").c_str(), &theEntry) == NULL);

    Section("temp files");
    char theTempFile[PLAT_MAX_PATH];
    CHECK(PlatCreateTempFile(sRoot.c_str(), "eeTest", theTempFile));
    CHECK(PlatPathGetAttributes(theTempFile) != PLAT_ATTR_INVALID);
    CHECK(strstr(theTempFile, "eeT") != NULL);
    CHECK(PlatDeleteFile(theTempFile));

    Section("cleanup");
    CHECK(!PlatRemoveDirectory((sRoot + "Data").c_str()));   // not empty
    CHECK(PlatDeleteFile(theFile.c_str()));
    CHECK(PlatDeleteFile(theCopy.c_str()));
    CHECK(PlatDeleteFile((sRoot + "data\\B.DAT").c_str()));
    CHECK(PlatDeleteFile((sRoot + "data\\a.dat").c_str()));
    CHECK(PlatRemoveDirectory((sRoot + "Data\\Sub").c_str()));
    CHECK(PlatRemoveDirectory((sRoot + "Data").c_str()));
    CHECK(PlatRemoveDirectory(sRoot.c_str()));

#if !defined(_WIN32)
    Section("volumes");
    // a volume mapped onto a native directory: Q:\ is the temporary directory
    CHECK(PlatPathToNative(theTemp, theNative, sizeof(theNative)) > 0);
    CHECK(PlatPathSetVolumeRoot('q', theNative));
    CHECK(PlatCreateDirectory("Q:\\PlatTestVolume") == PLAT_FILE_OK);
    CHECK(PlatPathGetAttributes((std::string(theTemp) + "PLATTESTVOLUME").c_str()) & PLAT_ATTR_DIRECTORY);
    char theMapped[PLAT_MAX_PATH];
    CHECK(PlatPathFromNative((std::string(theNative) + "/PlatTestVolume").c_str(), theMapped, PLAT_MAX_PATH) > 0);
    CHECK(PlatStrICmp(theMapped, "Q:\\PlatTestVolume") == 0);
    CHECK(PlatRemoveDirectory("q:\\plattestvolume"));
    CHECK(PlatPathSetVolumeRoot('Q', NULL));
    CHECK(PlatPathToNative("C:\\Windows\\..\\tmp\\.\\x.", theNative, sizeof(theNative)) > 0);
    // "." and ".." are resolved textually, trailing dots dropped; the volume is the root
    size_t theLength = strlen(theNative);
    CHECK(theLength >= 6 && strcmp(theNative + theLength - 6, "/tmp/x") == 0 && strstr(theNative, "Windows") == NULL);
#endif
}

// ---- the configuration file

static std::string ReadText(const char* inPath)
{
    std::string theText;
    FILE* f = fopen(inPath, "rb");
    if (f) {
        int c;
        while ((c = fgetc(f)) != EOF)
            theText += static_cast<char>(c);
        fclose(f);
    }
    return theText;
}

static void WriteText(const char* inPath, const char* inText)
{
    FILE* f = fopen(inPath, "wb");
    if (f) {
        fputs(inText, f);
        fclose(f);
    }
}

static void TestConfig(const std::string& inPath)
{
    Section("config");
    // a hand-written file, read on first use: comments, quotes, CR LF, blank lines, other indents
    WriteText(inPath.c_str(), "\xef\xbb\xbf# settings\r\n---\r\nhand:   # a block\r\n    a: 1  # one\r\n\r\n"
                              "    b: 'it''s # here'\r\n    inner:\r\n        c: \"x\\ty\\x41\"\r\n    d: yes\r\n"
                              "    url: http://x#y\r\n    e: ~\r\nother: plain text: no\r\n");
    char thePath[512];
    CHECK(PlatConfigGetPath(thePath, sizeof(thePath)) && inPath == thePath);   // EE_CONFIG names the file
    int32_t theValue = 0;
    char theBuffer[128];
    CHECK(PlatConfigGetInt("hand", "a", &theValue) && theValue == 1);
    CHECK(PlatConfigGetString("hand", "b", theBuffer, sizeof(theBuffer)) && strcmp(theBuffer, "it's # here") == 0);
    CHECK(PlatConfigGetString("hand.inner", "c", theBuffer, sizeof(theBuffer)) && strcmp(theBuffer, "x\tyA") == 0);
    CHECK(PlatConfigGetInt("hand", "d", &theValue) && theValue == 1);
    CHECK(PlatConfigGetString("hand", "url", theBuffer, sizeof(theBuffer)) && strcmp(theBuffer, "http://x#y") == 0);
    CHECK(PlatConfigGetString("hand", "e", theBuffer, sizeof(theBuffer)) && theBuffer[0] == 0);
    CHECK(PlatConfigGetString("", "other", theBuffer, sizeof(theBuffer)) && strcmp(theBuffer, "plain text: no") == 0);
    CHECK(PlatConfigDelete("hand", NULL));
    CHECK(PlatConfigDelete(NULL, "other"));
    CHECK(!PlatConfigGetInt("hand", "a", &theValue));
    CHECK(!PlatConfigGetInt("display", "width", &theValue));   // the rest starts empty
    CHECK(PlatConfigSetInt("display.resolution", "width", 1024));
    CHECK(PlatConfigSetInt("display", "negative", -7));
    const char theString[] = "C:\\Games\\Empire Earth \"AoC\"";
    CHECK(PlatConfigSetString("player", "path", theString));
    CHECK(PlatConfigSetString("player", "blank", " padded\t"));
    CHECK(PlatConfigSetString("player", "empty", ""));
    CHECK(PlatConfigSetString("player", "number", "12"));      // a string that looks like a number: quoted
    CHECK(PlatConfigSetString("player", "name", "Unknown"));
    CHECK(PlatConfigSetBool("audio", "music", 1));
    CHECK(PlatConfigSetInt(NULL, "top", 3));
    CHECK(PlatConfigGetInt("DISPLAY.Resolution", "WIDTH", &theValue) && theValue == 1024);   // keys ignore case
    CHECK(PlatConfigGetInt("display", "negative", &theValue) && theValue == -7);
    CHECK(PlatConfigGetInt("audio", "music", &theValue) && theValue == 1);   // true: 1
    CHECK(PlatConfigGetInt("", "top", &theValue) && theValue == 3);
    CHECK(!PlatConfigGetInt("display", "resolution", &theValue));   // a block is no value
    CHECK(PlatConfigGetString("player", "PATH", theBuffer, sizeof(theBuffer)) && strcmp(theBuffer, theString) == 0);
    CHECK(!PlatConfigGetString("player", "path", theBuffer, 4));   // too small
    CHECK(!PlatConfigGetInt("player", "path", &theValue));         // not an integer
    CHECK(PlatConfigGetString("display.resolution", "width", theBuffer, sizeof(theBuffer)) && strcmp(theBuffer, "1024") == 0);
    CHECK(PlatConfigGetString("player", "blank", theBuffer, sizeof(theBuffer)) && strcmp(theBuffer, " padded\t") == 0);
    CHECK(PlatConfigGetString("player", "empty", theBuffer, sizeof(theBuffer)) && theBuffer[0] == 0);
    CHECK(!PlatConfigGetInt("player", "missing", &theValue));
    // the file: nested blocks, a string quoted only when it needs it, blocks in their first-written order
    std::string theText = ReadText(inPath.c_str());
    const char* theHeader = "# Empire Earth AoC settings. The game rewrites this file whenever a setting changes (comments"
                            " are not kept).\n";
    CHECK(theText == std::string(theHeader) + "display:\n  resolution:\n    width: 1024\n  negative: -7\n\n"
                     "player:\n  path: C:\\Games\\Empire Earth \"AoC\"\n  blank: \" padded\\t\"\n  empty: \"\"\n"
                     "  number: \"12\"\n  name: Unknown\n\naudio:\n  music: true\ntop: 3\n");
    CHECK(PlatConfigDelete("display", "negative"));
    CHECK(!PlatConfigDelete("display", "negative"));
    CHECK(PlatConfigDelete("player", NULL));
    CHECK(PlatConfigDelete("audio", "music"));   // an empty block is not written
    CHECK(!PlatConfigGetString("player", "blank", theBuffer, sizeof(theBuffer)));
    CHECK(ReadText(inPath.c_str()) == std::string(theHeader) + "display:\n  resolution:\n    width: 1024\ntop: 3\n");
    remove(inPath.c_str());
}

// ---- sockets

static void TestSockets()
{
    Section("sockets");
    CHECK(PlatSocketStartup());
    CHECK(PlatSocketParseIPv4("127.0.0.1") == PlatHostToNet32(0x7F000001u));
    CHECK(PlatSocketParseIPv4("not an address") == PLAT_INADDR_NONE);
    char theText[16];
    PlatSocketFormatIPv4(PlatHostToNet32(0xC0A80102u), theText);
    CHECK(strcmp(theText, "192.168.1.2") == 0);
    CHECK(PlatHostToNet16(0x1234) == PlatNetToHost16(0x1234) && PlatNetToHost16(PlatHostToNet16(0x1234)) == 0x1234);
    char theHost[256];
    CHECK(PlatSocketGetHostName(theHost, sizeof(theHost)) && theHost[0] != 0);
    uint32_t theAddresses[8];
    CHECK(PlatSocketResolveIPv4("127.0.0.1", theAddresses, 8) >= 1 && theAddresses[0] == PlatHostToNet32(0x7F000001u));
    PlatNetAdapter theAdapters[10];
    uint32_t theAdapterCount = PlatSocketGetAdapters(theAdapters, 10);
    printf("   %u adapter(s)%s%s\n", theAdapterCount, theAdapterCount ? ", first: " : "",
           theAdapterCount ? theAdapters[0].mDescription : "");

    // stream: listen on a free loopback port, connect, accept, exchange, readiness event
    PlatSocket theListen = PlatSocketOpen(PLAT_SOCKET_STREAM);
    CHECK(theListen != PLAT_SOCKET_INVALID);
    PlatSockAddrIn theAddress;
    memset(&theAddress, 0, sizeof(theAddress));
    theAddress.mFamily = PLAT_AF_INET;
    theAddress.mAddress = PlatHostToNet32(0x7F000001u);
    CHECK(PlatSocketBind(theListen, &theAddress));
    CHECK(PlatSocketListen(theListen, 4));
    PlatSockAddrIn theBound;
    CHECK(PlatSocketGetName(theListen, &theBound) && theBound.mPort != 0 && theBound.mFamily == PLAT_AF_INET);
    PlatEvent* theAcceptEvent = PlatEventCreate(1, 0);
    CHECK(PlatSocketSelectEvent(theListen, theAcceptEvent, PLAT_SOCKET_EVENT_ACCEPT));
    PlatSocket theClient = PlatSocketOpen(PLAT_SOCKET_STREAM);
    CHECK(PlatSocketConnect(theClient, &theBound));
    CHECK(PlatEventWait(theAcceptEvent, 5000) == 0);
    PlatSockAddrIn thePeer;
    PlatSocket theServer = PlatSocketAccept(theListen, &thePeer);
    CHECK(theServer != PLAT_SOCKET_INVALID && thePeer.mAddress == theAddress.mAddress);
    PlatEvent* theDataEvent = PlatEventCreate(1, 0);
    CHECK(PlatSocketSelectEvent(theServer, theDataEvent, PLAT_SOCKET_EVENT_READ));
    char theBuffer[64];
    CHECK(PlatSocketRecv(theServer, theBuffer, sizeof(theBuffer)) == -1
          && PlatSocketLastError() == PLAT_SOCK_EWOULDBLOCK);   // non-blocking now
    CHECK(PlatEventWait(theDataEvent, 0) == PLAT_WAIT_TIMEOUT);
    CHECK(PlatSocketSend(theClient, "ping!", 5) == 5);
    CHECK(PlatEventWait(theDataEvent, 5000) == 0);
    PlatEventReset(theDataEvent);
    CHECK(PlatSocketRecv(theServer, theBuffer, 2) == 2 && memcmp(theBuffer, "pi", 2) == 0);
    CHECK(PlatEventWait(theDataEvent, 5000) == 0);   // signalled again: data is left
    PlatEventReset(theDataEvent);
    CHECK(PlatSocketRecv(theServer, theBuffer, sizeof(theBuffer)) == 3 && memcmp(theBuffer, "ng!", 3) == 0);
    CHECK(PlatEventWait(theDataEvent, 100) == PLAT_WAIT_TIMEOUT);   // drained
    CHECK(PlatSocketSend(theServer, "pong", 4) == 4);
    CHECK(PlatSocketRecv(theClient, theBuffer, sizeof(theBuffer)) == 4 && memcmp(theBuffer, "pong", 4) == 0);
    CHECK(PlatSocketClose(theClient));
    int32_t theEnd = -1;   // the peer's close: a receive of 0 bytes once it has arrived
    for (int i = 0; i < 200 && theEnd < 0; i++) {
        theEnd = PlatSocketRecv(theServer, theBuffer, sizeof(theBuffer));
        if (theEnd < 0)
            PlatSleep(5);
    }
    CHECK(theEnd == 0);
    CHECK(PlatSocketSelectEvent(theServer, NULL, 0));
    CHECK(PlatSocketClose(theServer));
    CHECK(PlatSocketClose(theListen));

    // datagrams
    PlatSocket theReceiver = PlatSocketOpen(PLAT_SOCKET_DGRAM);
    PlatSocket theSender = PlatSocketOpen(PLAT_SOCKET_DGRAM);
    CHECK(theReceiver != PLAT_SOCKET_INVALID && theSender != PLAT_SOCKET_INVALID);
    CHECK(PlatSocketBind(theReceiver, &theAddress));
    CHECK(PlatSocketGetName(theReceiver, &theBound));
    CHECK(PlatSocketSetNonBlocking(theReceiver, 1));
    CHECK(PlatSocketRecvFrom(theReceiver, theBuffer, sizeof(theBuffer), &thePeer) == -1
          && PlatSocketLastError() == PLAT_SOCK_EWOULDBLOCK);
    CHECK(PlatSocketSetBroadcast(theSender, 1));
    CHECK(PlatSocketSendTo(theSender, "datagram", 8, &theBound) == 8);
    int32_t theGot = -1;
    for (int i = 0; i < 200 && theGot < 0; i++) {
        theGot = PlatSocketRecvFrom(theReceiver, theBuffer, sizeof(theBuffer), &thePeer);
        if (theGot < 0)
            PlatSleep(5);
    }
    CHECK(theGot == 8 && memcmp(theBuffer, "datagram", 8) == 0 && thePeer.mAddress == theAddress.mAddress);
    PlatSocketClose(theReceiver);
    PlatSocketClose(theSender);

    // a refused connection
    PlatSocket theRefused = PlatSocketOpen(PLAT_SOCKET_STREAM);
    CHECK(!PlatSocketConnect(theRefused, &theBound) && PlatSocketLastError() == PLAT_SOCK_ECONNREFUSED);
    PlatSocketClose(theRefused);

    Section("socket wait");
    // options, then a non-blocking connect the wait completes
    theListen = PlatSocketOpen(PLAT_SOCKET_STREAM);
    CHECK(PlatSocketSetReuseAddress(theListen, 1));
    theAddress.mPort = 0;
    CHECK(PlatSocketBind(theListen, &theAddress) && PlatSocketListen(theListen, 4));
    CHECK(PlatSocketGetName(theListen, &theBound));
    CHECK(PlatSocketWait(theListen, PLAT_SOCKET_WAIT_READ, 0) == 0);   // nobody connecting yet
    theClient = PlatSocketOpen(PLAT_SOCKET_STREAM);
    CHECK(PlatSocketSetNoDelay(theClient, 1));
    CHECK(PlatSocketSetLinger(theClient, 1, 2));
    CHECK(PlatSocketSetNonBlocking(theClient, 1));
    CHECK(PlatSocketConnect(theClient, &theBound) || PlatSocketLastError() == PLAT_SOCK_EWOULDBLOCK);
    CHECK(PlatSocketWait(theClient, PLAT_SOCKET_WAIT_WRITE | PLAT_SOCKET_WAIT_EXCEPT, 5000)
          == static_cast<int32_t>(PLAT_SOCKET_WAIT_WRITE));
    CHECK(PlatSocketGetError(theClient) == 0);
    CHECK(PlatSocketWait(theListen, PLAT_SOCKET_WAIT_READ, 5000) == static_cast<int32_t>(PLAT_SOCKET_WAIT_READ));
    theServer = PlatSocketAccept(theListen, NULL);
    CHECK(theServer != PLAT_SOCKET_INVALID);
    uint32_t t0 = PlatTimeMilliseconds();
    CHECK(PlatSocketWait(theServer, PLAT_SOCKET_WAIT_READ, 60) == 0);   // no data: the timeout
    CHECK(PlatTimeMilliseconds() - t0 >= 40);
    CHECK(PlatSocketSend(theClient, "abc", 3) == 3);
    CHECK(PlatSocketWait(theServer, PLAT_SOCKET_WAIT_READ | PLAT_SOCKET_WAIT_WRITE, 5000)
          == static_cast<int32_t>(PLAT_SOCKET_WAIT_READ | PLAT_SOCKET_WAIT_WRITE));
    CHECK(PlatSocketRecv(theServer, theBuffer, sizeof(theBuffer)) == 3);
    // shutdown: the peer reads the end of the stream (readable, then 0 bytes)
    CHECK(PlatSocketShutdown(theClient, PLAT_SOCKET_SHUT_SEND));
    CHECK(PlatSocketWait(theServer, PLAT_SOCKET_WAIT_READ, 5000) & PLAT_SOCKET_WAIT_READ);
    CHECK(PlatSocketRecv(theServer, theBuffer, sizeof(theBuffer)) == 0);
    CHECK(PlatSocketSend(theServer, "back", 4) == 4);   // the other direction still open
    CHECK(PlatSocketWait(theClient, PLAT_SOCKET_WAIT_READ, 5000) & PLAT_SOCKET_WAIT_READ);
    CHECK(PlatSocketRecv(theClient, theBuffer, sizeof(theBuffer)) == 4);
    CHECK(PlatSocketShutdown(theServer, PLAT_SOCKET_SHUT_BOTH));
    PlatSocketClose(theClient);
    PlatSocketClose(theServer);
    CHECK(PlatSocketClose(theListen));
    // a refused non-blocking connect: except, with the reason pending
    theRefused = PlatSocketOpen(PLAT_SOCKET_STREAM);
    CHECK(PlatSocketSetNonBlocking(theRefused, 1));
    CHECK(!PlatSocketConnect(theRefused, &theBound));
    if (PlatSocketLastError() == PLAT_SOCK_EWOULDBLOCK) {   // else refused at once (allowed for loopback)
        int32_t theReady = PlatSocketWait(theRefused, PLAT_SOCKET_WAIT_WRITE | PLAT_SOCKET_WAIT_EXCEPT, 10000);
        CHECK(theReady == static_cast<int32_t>(PLAT_SOCKET_WAIT_EXCEPT));
        CHECK(PlatSocketGetError(theRefused) == PLAT_SOCK_ECONNREFUSED);
    } else
        CHECK(PlatSocketLastError() == PLAT_SOCK_ECONNREFUSED);
    PlatSocketClose(theRefused);
    PlatEventDestroy(theAcceptEvent);
    PlatEventDestroy(theDataEvent);
    PlatSocketCleanup();
}

// ---- messages

static std::string sShown;
static void Capture(const char* inTitle, const char* inText, uint32_t inFlags)
{
    sShown = std::string(inTitle) + "|" + inText + ((inFlags & PLAT_MESSAGE_ERROR) ? "|error" : "");
}

static void TestMessages()
{
    Section("messages");
#if !defined(_WIN32)   // Win32 shows a real (modal) message box
    PlatMessageSetHandler(Capture);
    PlatMessageShow("Title", "Text", PLAT_MESSAGE_ERROR);
    CHECK(sShown == "Title|Text|error");
    PlatMessageSetHandler(NULL);
#else
    (void)Capture;
#endif
    // the machine id: the same on every call (a machine without one, e.g. a container lacking /etc/machine-id,
    // reports that and 0)
    int theOk = -1, theOkAgain = -1;
    uint32_t theId = PlatSystemMachineId(&theOk);
    CHECK(PlatSystemMachineId(&theOkAgain) == theId && theOk == theOkAgain && (theOk == 0 || theOk == 1));
    CHECK(theOk || theId == 0);
    CHECK(PlatSystemMachineId(NULL) == theId);
    printf("   machine id %08x (%s)\n", theId, theOk ? "found" : "none");
}

int main(int argc, char** argv)
{
    // the configuration goes to a scratch file, not the program's directory
    std::string theConfig = std::string(getenv("TMPDIR") ? getenv("TMPDIR") : getenv("TEMP") ? getenv("TEMP") : "/tmp")
                            + "/plat_test_config.yaml";
    remove(theConfig.c_str());
#if defined(_WIN32)
    _putenv_s("EE_CONFIG", theConfig.c_str());
#else
    setenv("EE_CONFIG", theConfig.c_str(), 1);
#endif
    bool theNetwork = !(argc > 1 && strcmp(argv[1], "--no-network") == 0);
    TestTime();
    TestThreads();
    TestStrings();
    TestFiles();
    TestConfig(theConfig);
    if (theNetwork)
        TestSockets();
    TestMessages();
    printf("%d checks, %d failed\n", sChecks, sFailures);
    return sFailures > 100 ? 100 : sFailures;
}
