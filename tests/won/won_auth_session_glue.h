// The game's AuthSession class as it was until WON Internet round 22 (include/ee/wonapi/WONRouting.h and
// WONAuth/AuthSession.cpp: a RefCount shim owning an ee_won EeWonAuthSession), kept for the drivers whose original
// statements hold one (won_auth_orig.h's PeerAuthClient / PeerAuthServer, won_peerobj_test, won_peerop_test). The
// game's session is an ee_won handle now (rust/crates/ee_won/src/hostauth.rs).
#pragma once
#include <time.h>
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONBlowfish.h"
#include "ee/wonapi/WONBuffer.h"
#include "rust/ee_won.h"

namespace WONAPI {
class AuthSession : public RefCount {
protected:
    EeWonAuthSession* mImpl;
    virtual ~AuthSession() { ee_won_auth_session_free(this->mImpl); }
public:
    AuthSession(AuthType theType, unsigned short theId, Blowfish theKey, unsigned char theLengthFieldSize)
        : mImpl(ee_won_auth_session_new(theType, theId, reinterpret_cast<const EeWonBlowfish*>(&theKey),
                                        theLengthFieldSize, static_cast<int64_t>(time(NULL))))
    {
    }
    explicit AuthSession(EeWonAuthSession* inImpl) : mImpl(inImpl) {} // adopts a session ee_won made
    WONStatus Encrypt(ByteBufferPtr& theMsg) { return this->Crypt(false, theMsg); }
    WONStatus Decrypt(ByteBufferPtr& theMsg) { return this->Crypt(true, theMsg); }
    time_t GetLastUseTime() const { return static_cast<time_t>(ee_won_auth_session_get(this->mImpl, 2)); }
    const Blowfish& GetKey() const { return *reinterpret_cast<const Blowfish*>(ee_won_auth_session_key(this->mImpl)); }
    AuthType GetAuthType() const { return static_cast<AuthType>(ee_won_auth_session_get(this->mImpl, 0)); }
    unsigned short GetSessionId() const { return static_cast<unsigned short>(ee_won_auth_session_get(this->mImpl, 1)); }
    void SetLengthFieldSize(unsigned char size) { ee_won_auth_session_set_lfs(this->mImpl, size); }
    bool IsEncrypted() { AuthType t = this->GetAuthType(); return (t == AUTH_TYPE_SESSION || t == AUTH_TYPE_PERSISTENT); }
private:
    WONStatus Crypt(bool inDecrypt, ByteBufferPtr& theMsg)
    {
        ByteBufferPtr anOut;
        bool replaced = false;
        WONStatus aStatus = ee_won_auth_session_crypt(this->mImpl, inDecrypt, reinterpret_cast<const uint8_t*>(theMsg->data()),
                                                     theMsg->length(), static_cast<int64_t>(time(NULL)), WonByteBufferSink,
                                                     &anOut, &replaced);
        if (replaced)
            theMsg = anOut;
        return aStatus;
    }
};
}
