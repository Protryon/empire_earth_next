// The directory ops' requests and status replies through ee_won's dirops module (rust/crates/ee_won/src/dirops.rs):
// the ops hand their members over as an EeWonDirRequest (strings as UTF-16 LE bytes) and copy the results back
#pragma once
#include <string.h>
#include <string>
#include <list>
#include <vector>
#include "ee/wonapi/WONBuffer.h"
#include "ee/wonapi/WONStatus.h"
#include "rust/ee_rust.h"

namespace WONAPI {

class DirRequestFields {
public:
    DirRequestFields(int inOp, bool inIsService)
    {
        memset(&this->mReq, 0, sizeof(this->mReq));
        this->mReq.op = inOp;
        this->mReq.is_service = inIsService;
    }
    EeWonDirRequest mReq;
    EeWonDirBytes Text(const std::wstring& inText)
    {
        this->mTexts.push_back(std::string());
        WideAppendUtf16LE(this->mTexts.back(), inText);
        return Narrow(this->mTexts.back());
    }
    static EeWonDirBytes Narrow(const std::string& inText)
    {
        EeWonDirBytes b = {reinterpret_cast<const uint8_t*>(inText.data()), inText.size(), true};
        return b;
    }
    static EeWonDirBytes Buffer(const ByteBuffer* inBuffer)
    {
        EeWonDirBytes b = {NULL, 0, false};
        if (inBuffer != NULL) {
            b.data = reinterpret_cast<const uint8_t*>(inBuffer->data());
            b.len = inBuffer->length();
            b.present = true;
        }
        return b;
    }
    static EeWonDirObject Object(const DirDataObject& inObject)
    {
        EeWonDirObject o = {Narrow(inObject.mDataType), Buffer(inObject.mData.get())};
        return o;
    }
    void Objects(const DirDataObjectList& inObjects)
    {
        for (DirDataObjectList::const_iterator i = inObjects.begin(); i != inObjects.end(); ++i)
            this->mObjects.push_back(Object(*i));
    }
    void AddMod(unsigned short inOffset, bool inInsert, const DirDataObject& inObject)
    {
        EeWonDirMod m;
        m.offset = inOffset;
        m.insert = inInsert;
        m.object = Object(inObject);
        this->mMods.push_back(m);
    }
    void Types(const DirDataTypeSet& inTypes)
    {
        for (DirDataTypeSet::const_iterator i = inTypes.begin(); i != inTypes.end(); ++i)
            this->mTypes.push_back(Narrow(*i));
    }
    void AddQuery(DWORD inFlags, const std::wstring& inPath, const DirDataTypeSet& inTypes)
    {
        this->mQueryTypes.push_back(std::vector<EeWonDirBytes>());
        for (DirDataTypeSet::const_iterator i = inTypes.begin(); i != inTypes.end(); ++i)
            this->mQueryTypes.back().push_back(Narrow(*i));
        EeWonDirQuery q = {static_cast<uint32_t>(inFlags), Text(inPath), NULL, 0};
        this->mQueries.push_back(q);
    }
    // the message, behind the op's length field
    ByteBufferPtr Build(unsigned char inLengthFieldSize)
    {
        std::list<std::vector<EeWonDirBytes> >::iterator t = this->mQueryTypes.begin();
        for (size_t i = 0; i < this->mQueries.size(); ++i, ++t) {
            this->mQueries[i].types = t->empty() ? NULL : &(*t)[0];
            this->mQueries[i].type_count = t->size();
        }
        this->mReq.objects = this->mObjects.empty() ? NULL : &this->mObjects[0];
        this->mReq.object_count = this->mObjects.size();
        this->mReq.mods = this->mMods.empty() ? NULL : &this->mMods[0];
        this->mReq.mod_count = this->mMods.size();
        this->mReq.types = this->mTypes.empty() ? NULL : &this->mTypes[0];
        this->mReq.type_count = this->mTypes.size();
        this->mReq.queries = this->mQueries.empty() ? NULL : &this->mQueries[0];
        this->mReq.query_count = this->mQueries.size();
        std::string aBytes;
        ee_won_dir_request(&this->mReq, inLengthFieldSize, Append, &aBytes);
        return new ByteBuffer(aBytes.data(), (unsigned long)aBytes.length(), false);
    }
    static void Append(void* inContext, const uint8_t* inBytes, size_t inLength)
    {
        static_cast<std::string*>(inContext)->append(reinterpret_cast<const char*>(inBytes), inLength);
    }

private:
    std::list<std::string> mTexts;
    std::vector<EeWonDirObject> mObjects;
    std::vector<EeWonDirMod> mMods;
    std::vector<EeWonDirBytes> mTypes;
    std::list<std::vector<EeWonDirBytes> > mQueryTypes;
    std::vector<EeWonDirQuery> mQueries;
};

// An entity change's status reply: 100063 for another header (the op then calls InvalidReplyHeader), else the
// server's status; a returned address into *outAddr (readAddr: a service with DIR_UF_SERVRETURNADDR). A reply cut
// short throws ReadBufferException, as ReadBuffer did.
inline WONStatus DirStatusReply(const ByteBuffer* inReply, bool inReadAddr, ByteBufferPtr* outAddr)
{
    EeWonDirStatusReply r;
    ee_won_dir_status_reply(reinterpret_cast<const uint8_t*>(inReply->data()), inReply->length(), inReadAddr, &r);
    if (r.end == 1) return WS_ServerReq_InvalidReplyHeader;
    if (r.end == 2) throw ReadBufferException("Attempt to read past end of buffer.");
    if (r.has_addr && outAddr != NULL) *outAddr = new ByteBuffer(inReply->data() + r.addr_at, r.addr_len, false);
    return static_cast<WONStatus>(r.status);
}

} // namespace WONAPI
