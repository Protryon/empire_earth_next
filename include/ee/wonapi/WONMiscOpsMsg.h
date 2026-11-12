// FindPatchOp's patch choice, DetectFirewallOp's request, reply and datagram wait, and RegAuth's password cipher and
// port default through ee_won's miscops module (rust/crates/ee_won/src/miscops.rs); these apply them to the ops'
// members in the original's order (FindPatchOp.cpp's, DetectFirewallOp.cpp's and RegAuth.cpp's statements).
// Templates over the op so that tests/won/won_miscops_test.cpp runs the same code over stand-ins. WONStatus comes
// from WONRouting.h, included first.
#pragma once
#include <stdint.h>
#include <set>
#include <string>
#include <vector>
#include "ee/wonapi/WONBuffer.h"
#include "rust/ee_won.h"

namespace WONAPI {

inline void MiscOpsStringSink(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    static_cast<std::string*>(inContext)->assign(reinterpret_cast<const char*>(inBytes), inLength);
}

// Op has mPatchList (a list of records with mFromVersion and mToVersion), mVersionCompare, mValidVersionSet and
// mFilteredVersionSet
template <class Op> struct FindPatchMsg {
    enum { List_All = 0, List_Valid = 1, List_Highest = 2 };
    struct Ctx {
        Op* mOp;
        const std::set<std::string>* mSet;
    };
    static int32_t Compare(void* c, const uint8_t* a, size_t an, const uint8_t* b, size_t bn)
    {
        Ctx* x = static_cast<Ctx*>(c);
        return x->mOp->mVersionCompare(std::string(reinterpret_cast<const char*>(a), an),
                                       std::string(reinterpret_cast<const char*>(b), bn));
    }
    static bool InSet(void* c, const uint8_t* a, size_t n)
    {
        Ctx* x = static_cast<Ctx*>(c);
        return x->mSet->find(std::string(reinterpret_cast<const char*>(a), n)) != x->mSet->end();
    }
    static void Put(void* c, size_t i) { static_cast<std::vector<size_t>*>(c)->push_back(i); }

    // GetPatchList, GetValidPatchList and GetHighestPatchList(from, to): aList cleared, then the chosen patches
    template <class L>
    static void List(Op& o, int inWhich, const std::string& theFromVersion, const std::string& theToVersion, L& aList)
    {
        aList.clear();
        std::vector<const typename L::value_type*> anInfos;
        std::vector<EeWonPatchRef> aRefs;
        for (typename L::const_iterator i = o.mPatchList.begin(); i != o.mPatchList.end(); ++i) {
            anInfos.push_back(&*i);
            EeWonPatchRef r;
            r.from = reinterpret_cast<const uint8_t*>(i->mFromVersion.data());
            r.from_len = i->mFromVersion.size();
            r.to = reinterpret_cast<const uint8_t*>(i->mToVersion.data());
            r.to_len = i->mToVersion.size();
            aRefs.push_back(r);
        }
        Ctx aCtx = {&o, inWhich == List_Valid ? &o.mValidVersionSet : &o.mFilteredVersionSet};
        EeWonPatchHooks h;
        h.context = &aCtx;
        h.compare = Compare;
        h.in_set = InSet;
        std::vector<size_t> aChosen;
        ee_won_findpatch_list(inWhich, reinterpret_cast<const uint8_t*>(theFromVersion.data()), theFromVersion.size(),
                              reinterpret_cast<const uint8_t*>(theToVersion.data()), theToVersion.size(), aRefs.data(),
                              aRefs.size(), &h, Put, &aChosen);
        std::vector<typename L::value_type> aCopies;
        for (size_t i = 0; i < aChosen.size(); i++) aCopies.push_back(*anInfos[aChosen[i]]);
        for (size_t i = 0; i < aCopies.size(); i++) aList.push_back(aCopies[i]);
    }

    // DoFinish's patch id: the from and to versions of an id "from~to" (a to version alone without the '~')
    template <class Info> static void SplitId(Info& anInfo, const std::string& aPatchId)
    {
        intptr_t aPos = ee_won_findpatch_id_split(reinterpret_cast<const uint8_t*>(aPatchId.data()), aPatchId.size());
        if (aPos < 0)
            anInfo.mToVersion = aPatchId;
        else {
            anInfo.mFromVersion = aPatchId.substr(0, aPos);
            anInfo.mToVersion = aPatchId.substr(aPos + 1);
        }
    }
    // DoFinish's _cs and _ps values: assigned only from 4 bytes
    static void Le32(unsigned long& ioValue, const void* inData, unsigned long inLength)
    {
        uint32_t v = 0;
        if (ee_won_findpatch_le32(static_cast<const uint8_t*>(inData), inLength, &v)) ioValue = v;
    }
};

// Op has mLengthFieldSize, mUseUDP, mListenPort, mWaitForConnect, mMaxConnectWaitTime, mRequest, mResponse,
// mPublicAddr and InvalidReplyHeader() and RecvServerDatagram()
template <class Op> struct FirewallMsg {
    static long GetNextRequest(Op& o)
    {
        ee_won_firewall_request(o.mLengthFieldSize, o.mUseUDP, o.mListenPort, o.mWaitForConnect,
                                static_cast<uint32_t>(o.mMaxConnectWaitTime), WonByteBufferSink, &o.mRequest);
        return WS_ServerReq_Recv;
    }
    static long CheckResponse(Op& o)
    {
        int16_t aStatus = 0;
        int32_t aHost = 0;
        bool aHasHost = false;
        int aKind = ee_won_firewall_reply(reinterpret_cast<const uint8_t*>(o.mResponse->data()), o.mResponse->length(),
                                          &aStatus, &aHost, &aHasHost);
        if (aKind < 0) throw ReadBufferException("Attempt to read past end of buffer.");
        if (aKind == 0) return o.InvalidReplyHeader();
        if (aHasHost) o.mPublicAddr.Fn2B2C20(static_cast<long>(aHost), 0);
        if (o.mUseUDP) return o.RecvServerDatagram();
        return aStatus;
    }
    static unsigned long Wait(unsigned long inTimeLeft, unsigned long inMaxWait, bool inAsync)
    {
        return ee_won_firewall_wait(static_cast<uint32_t>(inTimeLeft), static_cast<uint32_t>(inMaxWait), inAsync);
    }
};

// RegAuth's stored password: encrypted (false when the cipher gave nothing) or decrypted
inline bool RegAuthPassword(bool inDecrypt, const void* inData, unsigned long inLength, std::string& outText)
{
    return ee_won_regauth_password(inDecrypt, static_cast<const uint8_t*>(inData), inLength, MiscOpsStringSink, &outText);
}

} // namespace WONAPI
