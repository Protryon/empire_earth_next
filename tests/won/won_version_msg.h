// GetVersionOp's and FindPatchOp's statements over ee_won's patch and miscops modules as their C++ applied them before
// the ops became Rust's (verop.rs; moved here from include/ee/wonapi/WONMiscOpsMsg.h and WONPatchMsg.h): the
// version filter set and test, and FindPatchOp's patch choice, for the drivers' original halves (won_misc_test,
// won_miscops_test, won_verop_test).
#pragma once
#include <set>
#include <string>
#include <vector>
#include "ee/wonapi/WONPatchMsg.h"
#include "rust/ee_won.h"

namespace WONAPI {

// VersionBaseOp::BuildVersionFilterSet
inline void PatchFilterSet(const std::string& theFilterString, std::set<std::string>& theFilterSet)
{
    theFilterSet.clear();
    std::vector<std::string> v = PatchSplit(theFilterString, true);
    for (size_t i = 0; i < v.size(); i++) theFilterSet.insert(v[i]);
}

// VersionBaseOp::IsFiltered: a "pt" line of the version's data naming a filter outside the set
inline bool PatchIsFiltered(const std::set<std::string>& inFilters, const std::string& theVersionData)
{
    std::vector<EeWonPatchBytes> f;
    for (std::set<std::string>::const_iterator i = inFilters.begin(); i != inFilters.end(); ++i)
        f.push_back(PatchBytes(*i));
    return ee_won_version_is_filtered(reinterpret_cast<const uint8_t*>(theVersionData.data()), theVersionData.size(),
                                      f.empty() ? NULL : &f[0], f.size());
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

} // namespace WONAPI
