// The routing server's protocol through ee_won's routemsg module (rust/crates/ee_won/src/routemsg.rs) for the one
// routing op that stays C++, RoutingGetUserStatusOp (a UDP datagram op): its request's fields handed over
// (RouteRequest) and its reply's assignments applied (RouteReply). The routing ops proper are Rust objects since net
// round 18 (routeop.rs); their templates live on in tests/won/won_routemsg_glue.h, the test glue of
// won_routemsg_test and won_reqlast_test.
#pragma once
#include <string.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "ee/wonapi/WONBuffer.h"
#include "ee/wonapi/WONStatus.h"
#include "rust/ee_won.h"

namespace WONAPI {

class RouteRequest {
public:
    RouteRequest(int inKind, bool inDerived)
    {
        memset(&this->mReq, 0, sizeof(this->mReq));
        this->mReq.kind = inKind;
        this->mReq.derived = inDerived;
    }
    EeWonRouteRequest mReq;
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
    void Ids(const std::list<unsigned short>& inIds)
    {
        this->mIds.assign(inIds.begin(), inIds.end());
        this->mReq.ids = this->mIds.empty() ? NULL : &this->mIds[0];
        this->mReq.id_count = this->mIds.size();
    }
    template <class Map> void UserIds(const Map& inMap)
    {
        for (typename Map::const_iterator i = inMap.begin(); i != inMap.end(); ++i)
            this->mUserIds.push_back(static_cast<uint32_t>(i->first));
        this->mReq.user_ids = this->mUserIds.empty() ? NULL : &this->mUserIds[0];
        this->mReq.user_id_count = this->mUserIds.size();
    }
    // the message replaces the buffer's contents (InitSendMsg's Reset, then the appends)
    void Into(WriteBuffer& outMsg)
    {
        std::string aBytes;
        ee_won_route_request(&this->mReq, Append, &aBytes);
        outMsg.Reset();
        if (!aBytes.empty()) outMsg.AppendBytes(aBytes.data(), (unsigned long)aBytes.size());
    }
    static void Append(void* inContext, const uint8_t* inBytes, size_t inLength)
    {
        static_cast<std::string*>(inContext)->append(reinterpret_cast<const char*>(inBytes), inLength);
    }

private:
    std::list<std::string> mTexts;
    std::vector<uint16_t> mIds;
    std::vector<uint32_t> mUserIds;
};

// RoutingGetUserStatusOp's datagram (always the routing service's header)
template <class Map> ByteBufferPtr RouteUserStatusRequest(const Map& inStatusMap)
{
    RouteRequest q(EE_WON_RT_REQ_USER_STATUS, false);
    q.UserIds(inStatusMap);
    WriteBuffer aMessage(0, true);
    q.Into(aMessage);
    return aMessage.ToByteBuffer(true);
}
// A reply read by Rust: its assignments, applied by the op through Each; End returns the status or throws
// ReadBufferException where the original's read failed
class RouteReply {
public:
    RouteReply(int inKind, unsigned char inType, ReadBuffer& inMsg, const EeWonRouteState* inState = NULL)
        : mData(inMsg.data())
    {
        EeWonRouteState aState;
        memset(&aState, 0, sizeof(aState));
        ee_won_route_reply(inKind, inType, reinterpret_cast<const uint8_t*>(inMsg.data()), inMsg.length(),
                           (size_t)inMsg.pos(), inState != NULL ? inState : &aState, Collect, &this->mEvents,
                           &this->mResult);
    }
    // a whole message (CheckResponse, ParseGroupListObj)
    RouteReply(int inKind, const void* inData, unsigned long inLength, const EeWonRouteState* inState = NULL)
        : mData(static_cast<const char*>(inData))
    {
        EeWonRouteState aState;
        memset(&aState, 0, sizeof(aState));
        ee_won_route_reply(inKind, 0, reinterpret_cast<const uint8_t*>(inData), inLength, 0,
                           inState != NULL ? inState : &aState, Collect, &this->mEvents, &this->mResult);
    }
    std::vector<EeWonRouteEvent> mEvents;
    EeWonRouteResult mResult;
    const char* mData;

    void Text(const EeWonRouteEvent& e, std::wstring& outText) const { WideAssignUtf16LE(outText, this->mData + e.at, e.len); }
    void Narrow(const EeWonRouteEvent& e, std::string& outText) const { outText.assign(this->mData + e.at, e.len); }
    ByteBufferPtr Buf(const EeWonRouteEvent& e) const { return new ByteBuffer(this->mData + e.at, e.len, false); }
    template <class P> void Buf(const EeWonRouteEvent& e, P& outBuffer) const { outBuffer = this->Buf(e); }
    WONStatus End(ReadBuffer& inMsg) const
    {
        inMsg.SetPos((int)this->mResult.pos);
        return this->End();
    }
    WONStatus End() const
    {
        if (this->mResult.cut) throw ReadBufferException("Attempt to read past end of buffer.");
        return (WONStatus)this->mResult.status;
    }
    static void Collect(void* inContext, const EeWonRouteEvent* inEvents, size_t inCount)
    {
        std::vector<EeWonRouteEvent>& v = *static_cast<std::vector<EeWonRouteEvent>*>(inContext);
        v.assign(inEvents, inEvents + inCount);
    }
};

// RoutingGetUserStatusOp::CheckResponse: each queried user's status, in the map's order
template <class Map> WONStatus RouteUserStatusReply(Map& ioStatusMap, ReadBuffer& inMessage)
{
    EeWonRouteState s;
    memset(&s, 0, sizeof(s));
    s.user_count = (uint32_t)ioStatusMap.size();
    RouteReply r(EE_WON_RT_USER_STATUS, 0, inMessage, &s);
    typename Map::iterator j = ioStatusMap.begin();
    for (size_t i = 0; i < r.mEvents.size(); ++i, ++j) j->second = (unsigned char)r.mEvents[i].num;
    inMessage.SetPos((int)r.mResult.pos);
    return (WONStatus)r.mResult.status;
}

} // namespace WONAPI
