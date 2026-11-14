// ne_queues_test stand-in for the engine messages: a message of the world (its exported bytes: a count, then that
// many bytes; a count of 0xff converts to no message) with a count of references. Every effect is a world line.
#pragma once
#include <vector>
#include "q_world.h"
class NEMessage {
public:
    NEMessage(const unsigned char* inData, size_t inSize) : mBytes(inData, inData + inSize), mCount(1)
    {
        this->mID = gQ->mNextMessage++;
        gQ->mMessages[this->mID] = this;
        gQ->Line("msg new m%d %d", this->mID, (int)inSize);
    }
    void Reference()
    {
        this->mCount++;
        gQ->Line("msg ref m%d %d", this->mID, this->mCount);
    }
    void Dereference()
    {
        this->mCount--;
        gQ->Line("msg deref m%d %d", this->mID, this->mCount);
        if (this->mCount == 0) {
            gQ->mMessages.erase(this->mID);
            delete this;
        }
    }
    unsigned long ExportSize()
    {
        gQ->Line("msg size m%d", this->mID);
        return (unsigned long)this->mBytes.size();
    }
    void ExportMessage(unsigned char** outData, unsigned long& outSize)
    {
        gQ->Line("msg export m%d", this->mID);
        *outData = new unsigned char[this->mBytes.size()];
        memcpy(*outData, this->mBytes.data(), this->mBytes.size());
        outSize = (unsigned long)this->mBytes.size();
    }
    const char* GetMessageLabel() const { return "message"; }
    static NEMessage* ConvertMemoryToMessage(unsigned char* inData)
    {
        if (inData == NULL || inData[0] == 0xff) {
            gQ->Line("msg convert none");
            return NULL;
        }
        return new NEMessage(inData, (size_t)inData[0] + 1);
    }
    std::vector<unsigned char> mBytes;
    int mCount;
    int mID;
};
