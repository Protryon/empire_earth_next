// TitleConfig's and CommunityConfig's rules through ee_won's titlecfg module (rust/crates/ee_won/src/titlecfg.rs):
// Rust decides an instruction's child or error, the name-login mode and the lifespan map and writes both ToString
// texts; these apply them to the config's members in the original's order (TitleConfig.cpp's statements).
// Templates over the config so that tests/won/won_titlecfg_test.cpp runs the same code over a stand-in.
#pragma once
#include <stdint.h>
#include <list>
#include <map>
#include <string>
#include <vector>
#include "rust/ee_won.h"

namespace WONAPI {

inline void TitleCfgStringSink(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    static_cast<std::string*>(inContext)->assign(reinterpret_cast<const char*>(inBytes), inLength);
}

inline void TitleCfgPutLifespan(void* inContext, int32_t inKey, int32_t inValue)
{
    (*static_cast<std::map<long, long>*>(inContext))[inKey] = inValue;
}

template <class Cfg> struct TitleCfgMsg {
    static const uint8_t* B(const std::string& s) { return reinterpret_cast<const uint8_t*>(s.data()); }

    // HandleInstruction after ConfigObject's: the child config parsed (o.ParseTitleChild, o.ParseCommunityChild),
    // an error set on the parser, or false for an instruction that is not the title's
    template <class Parser>
    static bool Instruction(Cfg& o, bool inCommunity, bool inAlready, Parser* theParser, const std::string& theType)
    {
        EeWonTitleInstr aVerdict = ee_won_titlecfg_instruction(inCommunity, inAlready, B(theType), theType.size());
        switch (aVerdict) {
        case EeWonTitleInstr::NotMine:
            return false;
        case EeWonTitleInstr::ChildTitle:
            return o.ParseTitleChild(*theParser);
        case EeWonTitleInstr::ChildCommunity:
            return o.ParseCommunityChild(*theParser, inCommunity);
        default: {
            std::string anError;
            ee_won_titlecfg_instr_error(aVerdict, TitleCfgStringSink, &anError);
            theParser->SetError(anError, false);
            return false;
        }
        }
    }

    // CommunityConfig::PostParse: theParentName is the parent TitleConfig's mName, or NULL when there is none
    template <class Upper>
    static void PostParse(Cfg& o, const std::string* theParentName, Upper inToWide)
    {
        o.mNameLogin = static_cast<decltype(o.mNameLogin)>(
            ee_won_titlecfg_name_login(B(o.mNameLoginStr), o.mNameLoginStr.size()));
        const std::string& aName = theParentName != NULL ? *theParentName : o.mName;
        if (o.mImagePath.empty())
            o.mImagePath = "./" + aName;
        if (o.mHashString.empty())
            o.mHashString = aName;
        std::vector<int32_t> aFlat;
        for (std::list<std::list<int> >::const_iterator i = o.mCertificateLifeSpanList.begin();
             i != o.mCertificateLifeSpanList.end(); ++i) {
            aFlat.push_back(static_cast<int32_t>(i->size()));
            for (std::list<int>::const_iterator j = i->begin(); j != i->end(); ++j) aFlat.push_back(*j);
        }
        o.mTrustLevelLifespanMap.clear();
        ee_won_titlecfg_lifespans(aFlat.data(), aFlat.size(), TitleCfgPutLifespan, &o.mTrustLevelLifespanMap);
        if (o.mCommunityName.empty())
            o.mCommunityName = inToWide(o.mName);
    }

    static EeWonTitleText Text(const Cfg& o)
    {
        EeWonTitleText t;
        t.name = B(o.mName);
        t.name_len = o.mName.size();
        t.network = B(o.mNetwork);
        t.network_len = o.mNetwork.size();
        t.image_path = B(o.mImagePath);
        t.image_path_len = o.mImagePath.size();
        t.hash_string = B(o.mHashString);
        t.hash_string_len = o.mHashString.size();
        t.key_prefix = B(o.mKeyPrefix);
        t.key_prefix_len = o.mKeyPrefix.size();
        t.net_port = o.mNetPort;
        return t;
    }

    static EeWonTitleFlags Flags(const Cfg& o)
    {
        EeWonTitleFlags f;
        f.force_cd_key = o.mForceCDKey;
        f.require_crc = o.mRequireCrc;
        f.allow_beta_key = o.mAllowBetaKey;
        f.require_cd_key = o.mRequireCDKey;
        f.fail_to_guest = o.mFailToGuest;
        f.allow_guest = o.mAllowGuest;
        f.concurrent_user_login = o.mConcurrentUserLogin;
        f.concurrent_cd_key_login = o.mConcurrentCDKeyLogin;
        f.get_user_data = o.mGetUserData;
        f.get_forgeable_user_data = o.mGetForgeableUserData;
        f.cd_key_for_invite = o.mCDKeyForInvite;
        f.save_session_key = o.mSaveSessionKey;
        f.get_key_id = o.mGetKeyID;
        f.ascii_account_names = o.mASCIIAccountNames;
        return f;
    }

    static std::string TitleText(const Cfg& o, bool includeName)
    {
        EeWonTitleText t = Text(o);
        EeWonTitleFlags f = Flags(o);
        std::string s;
        ee_won_titlecfg_title_text(&t, &f, includeName, TitleCfgStringSink, &s);
        return s;
    }

    // CommunityConfig::ToString: theNarrowName is WStringToString(mCommunityName)
    static std::string CommunityText(const Cfg& o, const std::string& theNarrowName)
    {
        const std::string& aHeader = o.mCommunityName.empty() ? o.mName : theNarrowName;
        EeWonTitleText t = Text(o);
        EeWonTitleFlags f = Flags(o);
        std::vector<int32_t> aPairs;
        for (std::map<long, long>::const_iterator i = o.mTrustLevelLifespanMap.begin();
             i != o.mTrustLevelLifespanMap.end(); ++i) {
            aPairs.push_back(static_cast<int32_t>(i->first));
            aPairs.push_back(static_cast<int32_t>(i->second));
        }
        std::string s;
        ee_won_titlecfg_community_text(B(aHeader), aHeader.size(), static_cast<int32_t>(o.mCommunityID), &t, &f,
                                       B(o.mNameLoginStr), o.mNameLoginStr.size(), aPairs.data(), aPairs.size() / 2,
                                       TitleCfgStringSink, &s);
        return s;
    }
};

} // namespace WONAPI
