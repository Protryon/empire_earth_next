// WONDB's requests and replies through ee_won's db module (rust/crates/ee_won/src/db.rs), as the ops' C++ read
// them before they became Rust ops (moved here from include/ee/wonapi/WONDBMsg.h): the ops hand their members over
// (DbRequest) and apply a reply's read; the helpers take the members by reference so that the drivers
// (won_db_test, won_adminop_test, won_reqops_test, won_reqdir_test, won_reqlast_test) run the same code over
// stand-ins.
#pragma once
#include <string.h>
#include <string>
#include <list>
#include <vector>
#include "ee/wonapi/WONBuffer.h"
#include "ee/wonapi/WONStatus.h"
#include "rust/ee_won.h"

namespace WONAPI {

class DbRequest {
public:
    DbRequest(int inKind, unsigned char inLengthFieldSize)
    {
        memset(&this->mReq, 0, sizeof(this->mReq));
        this->mReq.kind = inKind;
        this->mReq.field_size = inLengthFieldSize;
    }
    EeWonDbRequest mReq;
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
    static EeWonDirBytes Bytes(const void* inData, unsigned long inLength)
    {
        EeWonDirBytes b = {static_cast<const uint8_t*>(inData), inLength, true};
        return b;
    }
    ByteBufferPtr Build()
    {
        ByteBufferPtr aBuffer;
        ee_won_db_request(&this->mReq, WonByteBufferSink, &aBuffer);
        return aBuffer;
    }

private:
    std::list<std::string> mTexts;
};

// the error string the header read assigned (erased when its length was read but not its bytes)
inline void DbApplyError(const EeWonDbReply& r, const char* inData, std::string& outError)
{
    if (r.read >= 6) outError.assign(inData + r.error_at, r.error_len);
    else if (r.error_erased) outError.erase();
}
inline void DbThrowIfCut(const EeWonDbReply& r)
{
    if (r.cut) throw ReadBufferException("Attempt to read past end of buffer.");
}

// DBProxyOp::CheckResponse; *outInvalid: the op returns InvalidReplyHeader()
inline WONStatus DbProxyReply(const ByteBuffer* inResponse, short& outReplyType, short& outSubReplyType,
                              std::string& outError, WriteBuffer& ioReceived, ByteBufferPtr& outReplyData,
                              bool* outInvalid)
{
    EeWonDbReply r;
    ee_won_db_reply(EE_WON_DB_PROXY, reinterpret_cast<const uint8_t*>(inResponse->data()), inResponse->length(), &r);
    *outInvalid = false;
    if (r.read >= 3) outReplyType = r.message;
    if (r.read >= 4) outSubReplyType = r.sub;
    DbApplyError(r, inResponse->data(), outError);
    DbThrowIfCut(r);
    if (r.verdict == EE_WON_DB_INVALID_HEADER) {
        *outInvalid = true;
        return WS_Success;
    }
    if (r.rest_len > 0) ioReceived.AppendBytes(inResponse->data() + r.rest_at, r.rest_len);
    if (r.verdict == EE_WON_DB_DONE) {
        outReplyData = ioReceived.ToByteBuffer(true);
        return (WONStatus)r.result;
    }
    return WS_ServerReq_Recv;
}

// The account ops' replies (create, get, update): the read with the error string (and the email) assigned; a
// reply cut short throws
inline EeWonDbReply DbAccountReply(int inKind, const ByteBuffer* inResponse, std::string& outError,
                                   std::string* outEmail = NULL)
{
    EeWonDbReply r;
    ee_won_db_reply(inKind, reinterpret_cast<const uint8_t*>(inResponse->data()), inResponse->length(), &r);
    DbApplyError(r, inResponse->data(), outError);
    if (outEmail != NULL) {
        if (r.email == 2) outEmail->assign(inResponse->data() + r.email_at, r.email_len);
        else if (r.email == 1) outEmail->erase();
    }
    DbThrowIfCut(r);
    return r;
}

// ContestCallOp::CheckResponse (the rest is the reply data) and EventReportOp::CheckResponse
inline WONStatus DbContestReply(const ByteBuffer* inResponse, ByteBufferPtr* outReplyData, bool* outInvalid)
{
    EeWonDbReply r;
    ee_won_db_reply(outReplyData != NULL ? EE_WON_DB_CONTEST : EE_WON_DB_EVENT,
                    reinterpret_cast<const uint8_t*>(inResponse->data()), inResponse->length(), &r);
    DbThrowIfCut(r);
    *outInvalid = r.verdict == EE_WON_DB_INVALID_HEADER;
    if (*outInvalid) return WS_Success;
    if (outReplyData != NULL) *outReplyData = new ByteBuffer(inResponse->data() + r.rest_at, r.rest_len, false);
    return (WONStatus)r.result;
}

// GetUserIdFromNameOp's id from the reply data
inline unsigned long DbUserId(const ByteBuffer* inReplyData)
{
    int32_t anId = 0;
    if (!ee_won_db_user_id(reinterpret_cast<const uint8_t*>(inReplyData->data()), inReplyData->length(), &anId))
        throw ReadBufferException("Attempt to read past end of buffer.");
    return (unsigned long)anId;
}

} // namespace WONAPI
