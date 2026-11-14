// ne_msgmgr_test's world: the messages, files, clock and answers the stand-in headers share. Every effect a call
// has on the world is a line of the side's trace; the answers (a send's result, a file's open and read) come from the
// side's random stream, so two sides that make the same calls in the same order get the same answers.
#pragma once
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <map>
#include <string>
#include <vector>

class NEMessage;

struct NetFile {
    std::string mName;
    unsigned long mLength = 0;
    unsigned long mPos = 0;
    bool mOpen = false;
};

struct NetWorld {
    std::string mTrace;
    uint32_t mRandom = 1;
    unsigned long mClock = 1000;
    std::map<int, NEMessage*> mMessages;
    int mNextMessage = 1;
    std::map<int, NetFile> mFiles;
    int mNextFile = 1;

    void Line(const char* inFormat, ...)
    {
        char theLine[256];
        va_list theArgs;
        va_start(theArgs, inFormat);
        vsnprintf(theLine, sizeof(theLine), inFormat, theArgs);
        va_end(theArgs);
        this->mTrace += theLine;
        this->mTrace += '\n';
    }
    uint32_t Next()
    {
        this->mRandom = this->mRandom * 1103515245u + 12345u;
        return this->mRandom >> 8;
    }
    int NewMessage(NEMessage* inMessage)
    {
        int theID = this->mNextMessage++;
        this->mMessages[theID] = inMessage;
        return theID;
    }
    void FreeMessage(int inID) { this->mMessages.erase(inID); }
    int IdOf(NEMessage* inMessage);
    long Send(const char* inWhat, unsigned long inSlot, const unsigned char* inData, unsigned long inLength)
    {
        std::string theHex;
        char theByte[4];
        for (unsigned long i = 0; i < inLength; i++) {
            snprintf(theByte, sizeof(theByte), "%02x", inData[i]);
            theHex += theByte;
        }
        long theResult = this->Next() % 10 == 0 ? 5 : 0;
        this->mTrace += std::string(inWhat) + " " + std::to_string(inSlot) + " " + theHex + " = " +
                        std::to_string(theResult) + "\n";
        return theResult;
    }
    NetFile& File(int inID) { return this->mFiles[inID]; }
    int FileClone(int inSource)
    {
        int theID = this->mNextFile++;
        NetFile theFile = this->mFiles[inSource];
        theFile.mOpen = false;
        theFile.mPos = 0;
        this->mFiles[theID] = theFile;
        this->Line("fclone f%d f%d", inSource, theID);
        return theID;
    }
    void FileFree(int inID)
    {
        this->Line("ffree f%d", inID);
        this->mFiles.erase(inID);
    }
    long FileOpen(int inID)
    {
        long theResult = this->Next() % 8 == 0 ? 9 : 0;
        if (theResult == 0) {
            this->mFiles[inID].mOpen = true;
            this->mFiles[inID].mPos = 0;
        }
        this->Line("fopen f%d = %ld", inID, theResult);
        return theResult;
    }
    void FileClose(int inID)
    {
        this->mFiles[inID].mOpen = false;
        this->Line("fclose f%d", inID);
    }
    long FileRead(int inID, unsigned char* outData, unsigned long inSize, unsigned long& outRead)
    {
        NetFile& theFile = this->mFiles[inID];
        outRead = 0;
        if (!theFile.mOpen || this->Next() % 16 == 0) {
            this->Line("fread f%d = 9", inID);
            return 9;
        }
        unsigned long theCount = theFile.mLength - theFile.mPos < inSize ? theFile.mLength - theFile.mPos : inSize;
        for (unsigned long i = 0; i < theCount; i++)
            outData[i] = (unsigned char)((theFile.mPos + i) * 31 + theFile.mName.size());
        theFile.mPos += theCount;
        outRead = theCount;
        this->Line("fread f%d = 0 %lu", inID, theCount);
        return 0;
    }
};

extern NetWorld* gWorld;
