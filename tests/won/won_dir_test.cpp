// The directory server's entity replies (DirEntityReplyParser.cpp): replies built with WriteBuffer (every get flag
// on its own and together, services and directories, data objects and ACLs, wrong headers, negative statuses,
// every cut of a reply), parsed, and each entity's fields printed one line per reply, compared with
// rust/crates/ee_won/src/golden_dir.txt (recorded over the C++ parser before its port). `--print` prints the lines.
// The exit code is the number of failures.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONBuffer.h"
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONDirExtended.h"

using namespace WONAPI;

static std::vector<std::string> sLines;

static std::string Hex(const void* inData, size_t inLength)
{
    std::string anOut;
    char aBuffer[4];
    for (size_t i = 0; i < inLength; i++) {
        snprintf(aBuffer, sizeof(aBuffer), "%02x", static_cast<const unsigned char*>(inData)[i]);
        anOut += aBuffer;
    }
    return anOut.empty() ? "-" : anOut;
}

static std::string Units(const std::wstring& inText)
{
    std::string anOut;
    char aBuffer[12];
    for (size_t i = 0; i < inText.size(); i++) {
        snprintf(aBuffer, sizeof(aBuffer), "%s%x", i ? "." : "", (unsigned)inText[i]);
        anOut += aBuffer;
    }
    return "|" + anOut + "|";
}

static std::string Entity(const DirEntity* inEntity)
{
    char aBuffer[512];
    snprintf(aBuffer, sizeof(aBuffer), "type %d life %ld crc %lu created %ld cip %lu cid %lu touched %ld tip %lu tid %lu uniq %d",
             (int)inEntity->mType, (long)inEntity->mLifespan, (unsigned long)inEntity->mCRC, (long)inEntity->mCreated,
             (unsigned long)inEntity->mCreateIP, (unsigned long)inEntity->mCreateId, (long)inEntity->mTouched,
             (unsigned long)inEntity->mTouchIP, (unsigned long)inEntity->mTouchId, inEntity->mReqUnique ? 1 : 0);
    std::string anOut = std::string(aBuffer) + " path " + Units(inEntity->mPath) + " name " + Units(inEntity->mName) +
                        " display " + Units(inEntity->mDisplayName) + " addr " +
                        (inEntity->mNetAddr.get() != NULL ? Hex(inEntity->mNetAddr->data(), inEntity->mNetAddr->length())
                                                          : std::string("null"));
    for (DirDataObjectList::const_iterator i = inEntity->mDataObjects.begin(); i != inEntity->mDataObjects.end(); ++i)
        anOut += " do " + Hex(i->mDataType.data(), i->mDataType.size()) + "=" +
                 (i->mData.get() != NULL ? Hex(i->mData->data(), i->mData->length()) : std::string("null"));
    for (DirACLList::const_iterator i = inEntity->mACLs.begin(); i != inEntity->mACLs.end(); ++i) {
        snprintf(aBuffer, sizeof(aBuffer), " acl %d/%lu/%lu/%u", (int)i->mType, (unsigned long)i->mUser,
                 (unsigned long)i->mCommunity, (unsigned)i->mTrust);
        anOut += aBuffer;
    }
    return anOut;
}

static void Fields(WriteBuffer& ioMessage, unsigned long inFlags, char inType, int inSeed, bool inMulti)
{
    if (inFlags & 0x10) ioMessage.AppendByte(inType);
    bool aService = inType == 'S' || (inFlags & 5) == 0 || !inMulti;
    bool aDir = inType == 'D' || (inFlags & 2) == 0;
    std::wstring aPath = L"/EE/p", aName = L"n\x263a";
    aPath += (wchar_t)('0' + inSeed);
    if (aService) {
        if (inFlags & 0x01000000) ioMessage.AppendWString(aPath, 2);
        if (inFlags & 0x02000000) ioMessage.AppendWString(aName, 2);
        if (inFlags & 0x04000000) {
            ioMessage.AppendByte(3);
            ioMessage.AppendBytes("\x01\x02\x03", 3);
        }
    } else if (aDir && inMulti) {
        if (inFlags & 0x00010000) ioMessage.AppendWString(aPath, 2);
        if (inFlags & 0x00020000) ioMessage.AppendWString(aName, 2);
        if (inFlags & 0x00040000) ioMessage.AppendByte((unsigned char)inSeed);
    }
    if (inFlags & 0x20) ioMessage.AppendWString(L"Display", 2);
    if (inFlags & 0x100) ioMessage.AppendLong(-100 - inSeed);
    if (inFlags & 0x40) ioMessage.AppendLong(1000 + inSeed);
    if (inFlags & 0x80) ioMessage.AppendLong(2000 + inSeed);
    if (inFlags & 0x2000) ioMessage.AppendLong(0x80000001);
    if (inMulti && (inFlags & 0x8000)) {
        ioMessage.AppendLong(0x0100007f);
        ioMessage.AppendLong(-2);
    }
    if (inMulti && (inFlags & 0x4000)) {
        ioMessage.AppendLong(77);
        ioMessage.AppendLong(-78);
    }
    if (inFlags & 0x800) {
        ioMessage.AppendShort(2);
        for (int i = 0; i < 2; i++) {
            if (inFlags & 0x200) ioMessage.AppendString(i ? "type2" : "t", 1);
            if (inFlags & 0x400) {
                ioMessage.AppendShort((short)(i + 1));
                ioMessage.AppendBytes("\xaa\xbb", i + 1);
            }
        }
    }
    if (inFlags & 0x1000) {
        ioMessage.AppendShort(2);
        for (int a = 0; a < 2; a++) {
            ioMessage.AppendByte((unsigned char)(a ? 0xf0 : 'u'));
            ioMessage.AppendShort((short)(a + 1));
            for (int u = 0; u <= a; u++) {
                ioMessage.AppendLong(u ? -5 : 5);
                ioMessage.AppendLong(0x7fffffff - u);
                ioMessage.AppendShort((short)(u ? -1 : 3));
            }
        }
    }
}

static std::string Multi(const std::string& inReply)
{
    DirEntityList aList;
    unsigned char aSequence = 0xee;
    std::string anOut;
    try {
        WONStatus aStatus = DirEntityReplyParser::ParseMultiEntityReply(inReply.data(), (unsigned long)inReply.size(),
                                                                        aList, &aSequence);
        char aBuffer[64];
        snprintf(aBuffer, sizeof(aBuffer), "status %d seq %02x", (int)aStatus, aSequence);
        anOut = aBuffer;
    } catch (ReadBufferException& anException) {
        char aBuffer[64];
        snprintf(aBuffer, sizeof(aBuffer), " seq %02x", aSequence);
        anOut = "throw " + anException.mMessage + aBuffer;
    }
    for (DirEntityList::iterator i = aList.begin(); i != aList.end(); ++i)
        anOut += " { " + Entity(i->get()) + " }";
    return anOut;
}

static std::string Single(const std::string& inReply)
{
    DirEntityPtr anEntity;
    std::string anOut;
    try {
        WONStatus aStatus = DirEntityReplyParser::ParseSingleEntityReply(inReply.data(), (unsigned long)inReply.size(),
                                                                         anEntity);
        char aBuffer[32];
        snprintf(aBuffer, sizeof(aBuffer), "status %d", (int)aStatus);
        anOut = aBuffer;
    } catch (ReadBufferException& anException) {
        anOut = "throw " + anException.mMessage;
    }
    if (anEntity.get() != NULL)
        anOut += " { " + Entity(anEntity.get()) + " }";
    return anOut;
}

static std::string Reply(bool inMulti, unsigned long inFlags, short inStatus, unsigned char inSequence, int inHeader)
{
    WriteBuffer aMessage(4, false);
    aMessage.AppendByte(inHeader == 1 ? 6 : 5);
    aMessage.AppendShort(inHeader == 2 ? 3 : 2);
    aMessage.AppendShort(inMulti ? (inHeader == 3 ? 2 : 3) : (inHeader == 3 ? 3 : 2));
    aMessage.AppendShort(inStatus);
    if (inMulti) {
        aMessage.AppendByte(inSequence);
        aMessage.AppendLong((int32_t)inFlags);
        aMessage.AppendShort(3);
        Fields(aMessage, inFlags, 'S', 1, true);
        Fields(aMessage, inFlags, 'D', 2, true);
        Fields(aMessage, inFlags, 'X', 3, true);
    } else {
        aMessage.AppendLong((int32_t)inFlags);
        Fields(aMessage, inFlags, 'S', 4, false);
    }
    return std::string(aMessage.data(), aMessage.length());
}

static std::string GoldenPath()
{
    std::string aPath = __FILE__;
    size_t aSlash = aPath.find_last_of("/\\");
    aPath.resize(aSlash == std::string::npos ? 0 : aSlash + 1);
    return aPath + "../../rust/crates/ee_won/src/golden_dir.txt";
}

int main(int argc, char** argv)
{
    const unsigned long kAll = 0x07077ff7;
    std::vector<unsigned long> aFlags;
    aFlags.push_back(0);
    aFlags.push_back(kAll);
    aFlags.push_back(kAll & ~0x10ul);
    aFlags.push_back(kAll & ~0x7ul);
    aFlags.push_back((kAll & ~0x7ul) | 2);
    aFlags.push_back((kAll & ~0x7ul) | 1);
    aFlags.push_back(0x07070000);
    for (int b = 0; b < 27; b++)
        aFlags.push_back((1ul << b) | 0x10 | 0x7);
    for (size_t f = 0; f < aFlags.size(); f++) {
        char aHead[64];
        snprintf(aHead, sizeof(aHead), "multi %08lx: ", aFlags[f]);
        sLines.push_back(aHead + Multi(Reply(true, aFlags[f], 0, 0x80, 0)));
        snprintf(aHead, sizeof(aHead), "single %08lx: ", aFlags[f]);
        sLines.push_back(aHead + Single(Reply(false, aFlags[f], 0, 0, 0)));
    }
    sLines.push_back("multi more: " + Multi(Reply(true, kAll, 0, 0x01, 0)));
    sLines.push_back("multi status: " + Multi(Reply(true, kAll, -12, 0x80, 0)));
    sLines.push_back("single status: " + Single(Reply(false, kAll, -3, 0, 0)));
    sLines.push_back("multi status+: " + Multi(Reply(true, kAll, 7, 0x80, 0)));
    for (int h = 1; h <= 3; h++) {
        char aHead[32];
        snprintf(aHead, sizeof(aHead), "multi header %d: ", h);
        sLines.push_back(aHead + Multi(Reply(true, kAll, 0, 0x80, h)));
        snprintf(aHead, sizeof(aHead), "single header %d: ", h);
        sLines.push_back(aHead + Single(Reply(false, kAll, 0, 0, h)));
    }
    std::string aMulti = Reply(true, kAll, 0, 0x80, 0), aSingle = Reply(false, kAll, 0, 0, 0);
    for (size_t n = 0; n < aMulti.size(); n++) {
        char aHead[32];
        snprintf(aHead, sizeof(aHead), "multi cut %d: ", (int)n);
        sLines.push_back(aHead + Multi(aMulti.substr(0, n)));
    }
    for (size_t n = 0; n < aSingle.size(); n++) {
        char aHead[32];
        snprintf(aHead, sizeof(aHead), "single cut %d: ", (int)n);
        sLines.push_back(aHead + Single(aSingle.substr(0, n)));
    }
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
    printf("won_dir_test: %d lines, %d failures\n", (int)sLines.size(), aFailures);
    return aFailures;
}
