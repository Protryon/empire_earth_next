// WONDB's ProfileData (the account profile: newsletters, custom action and birth date), a shim over ee_won's
// EeWonProfile (rust/crates/ee_won/src/db.rs). The game's account ops take the profile as Rust members now
// (reqdir.rs, reqlast.rs); this class is the drivers' original half (won_db_test, won_adminop_test, won_reqdir_test,
// won_reqlast_test), moved here from src/Empire Earth/WONAPI/WONDB/ProfileData.cpp and WONRouting.h.
#pragma once
#include "ee/wonapi/WONRouting.h"
namespace WONAPI {
class ProfileData : public RefCount {
public:
    ProfileData(); //2cd770, 150 bytes, initializes birth8/9/a=0, mapc, id18=0, data1c=NULL
    void ClearNewsletterSubs(); //2cdc20, clears the map
    void ClearNewsletterSub(unsigned short);
    void SetCustomAction(unsigned short,const ByteBuffer*);
    void ClearCustomAction();
    unsigned long GetBirthDate();
    void AddNewsletterSub(unsigned short inId, bool inSubscribe = false); //2cd9e0
    bool GetNewsletterSub(unsigned short inId); //2cdae0
    void PackRequestData(WriteBuffer&,bool); //2cdcc0
    void UnpackGetData(ReadBuffer&); //2cde20
    void SetBirthDate(unsigned char inMonth,unsigned char inDay,unsigned short inYear); //2ce070
    // the Rust profile (ee_won db.rs), for the requests that pack it
    const EeWonProfile* GetProfile() const { return this->mProfile; }
protected:
    virtual ~ProfileData(); //2cd830, explicit own-vtable store at 2cd852
private:
    ProfileData(const ProfileData&);
    ProfileData& operator=(const ProfileData&);
    static void Append(void*,const uint8_t*,size_t);
    // the birth date, the newsletter subscriptions and the custom action: a Rust object (ee_won db.rs)
    EeWonProfile* mProfile;
};
}
