// GetCertOp's messages through ee_won's cert module (rust/crates/ee_won/src/cert.rs): Rust builds the requests and
// reads a reply into its entries up to the cut; this walks them and applies the certificate, key block, session
// key and private key checks in the original's order (GetCertOp.cpp's statements). A template over the op and the
// auth classes (Env: Op, Cert1, Cert2, KeyBlock, PeerData, PrivKey) so that tests/won/won_cert_test.cpp runs the
// same code over stand-ins; GetCertOp makes it a friend. WONStatus comes from WONRouting.h, included first.
#pragma once
#include <string.h>
#include <time.h>
#include <string>
#include <vector>
#include "ee/wonapi/WONBuffer.h"
#include "ee/wonapi/WONStatus.h"
#include "rust/ee_rust.h"

namespace WONAPI {

inline void CertEventSink(void* inContext, const EeWonCertEvent* inEvents, size_t inCount)
{
    static_cast<std::vector<EeWonCertEvent>*>(inContext)->assign(inEvents, inEvents + inCount);
}
inline void CertWriteSink(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    static_cast<WriteBuffer*>(inContext)->AppendBytes(inBytes, static_cast<unsigned long>(inLength));
}
inline EeWonDirBytes CertBytes(const void* inData, unsigned long inLength)
{
    EeWonDirBytes b = {static_cast<const uint8_t*>(inData), inLength, true};
    return b;
}
inline void CertThrowCut()
{
    throw ReadBufferException("Attempt to read past end of buffer.");
}

template <class Env> struct CertMsg {
    typedef typename Env::Op Op;
    enum {
        kDecryptFailure = 100074,
        kInvalidCertificate = 100072,
        kInvalidPrivateKey = 100075,
        kInvalidPubKeyBlock = 100073,
        kInvalidSecretConfirm = 100076,
        kMissingCertificate = 100077,
        kUnexpectedLoginChallenge = 100069,
        kPubKeyEncryptFailure = 100065,
        kTryNextServer = 100057,
        kSend = 100058,
        kRecv = 100059
    };

    static ByteBufferPtr Request(int inKind, short inKeyId, EeWonDirBytes inA, EeWonDirBytes inB)
    {
        ByteBufferPtr aBuffer;
        ee_won_cert_request(inKind, inKeyId, inA, inB, WonByteBufferSink, &aBuffer);
        return aBuffer;
    }

    static WONStatus SendPubKeyRequest(Op& o)
    {
        o.mState = Op::GETTING_PUB_KEYS;
        EeWonDirBytes none = {NULL, 0, false};
        o.mRequest = Request(EE_WON_CERT_REQ_PUBKEYS, 0, none, none);
        return (WONStatus)kRecv;
    }

    static WONStatus SendLoginRequest(Op& o)
    {
        short aKeyId = o.mKeyBlock->GetId();
        const typename Env::PrivKey& pubKey = o.mKeyBlock->GetCurrentKey();
        o.mSessionKey.Create(8);
        ByteBufferPtr encryptedSessionKey = pubKey.Encrypt(o.mSessionKey.GetKey(), o.mSessionKey.GetKeyLen());
        if (encryptedSessionKey.get() == NULL)
            return (WONStatus)kPubKeyEncryptFailure;

        std::string aUser, aPassword, aNewPassword;
        WideAppendUtf16LE(aUser, o.mAuthContext->GetUserName());
        WideAppendUtf16LE(aPassword, o.mAuthContext->GetPassword());
        WideAppendUtf16LE(aNewPassword, o.mNewPassword);
        WriteBuffer loginData(0, true);
        ee_won_cert_login_data(static_cast<const uint8_t*>(o.mSessionKey.GetKey()), o.mKeyBlock->GetId(),
                               o.mCreateAccount, CertBytes(aUser.data(), aUser.size()),
                               CertBytes(aPassword.data(), aPassword.size()),
                               CertBytes(aNewPassword.data(), aNewPassword.size()), CertWriteSink, &loginData);
        o.mAuthContext->AppendCommunityData(loginData);

        ByteBufferPtr encryptedLoginData = o.mSessionKey.Encrypt(loginData.data(), loginData.length());
        o.mRequest = Request(EE_WON_CERT_REQ_LOGIN, aKeyId,
                             CertBytes(encryptedSessionKey->data(), encryptedSessionKey->length()),
                             CertBytes(encryptedLoginData->data(), encryptedLoginData->length()));
        o.mState = Op::GETTING_CHALLENGE;
        return (WONStatus)kRecv;
    }

    static WONStatus SendLoginConfirm(Op& o)
    {
        WriteBuffer dataBlock(0, true);
        o.mAuthContext->AppendHashes(dataBlock, o.mChallengeSeed);
        ByteBufferPtr encrypted = o.mSessionKey.Encrypt(dataBlock.data(), dataBlock.length());
        EeWonDirBytes none = {NULL, 0, false};
        o.mRequest = Request(EE_WON_CERT_REQ_CONFIRM, 0, CertBytes(encrypted->data(), encrypted->length()), none);
        return (WONStatus)kRecv;
    }

    static WONStatus TryNext(Op& o, int inError)
    {
        o.SetLastServerError((WONStatus)inError);
        return (WONStatus)kTryNextServer;
    }

    static WONStatus HandleLoginChallenge(Op& o, const char* inData, unsigned short inLen)
    {
        ByteBufferPtr aDecrypt = o.mSessionKey.Decrypt(inData, inLen);
        if (aDecrypt.get() == NULL)
            return TryNext(o, kDecryptFailure);
        if (aDecrypt->length() < 16)
            CertThrowCut();
        o.mChallengeSeed.assign(reinterpret_cast<const unsigned char*>(aDecrypt->data()), 16);
        o.mAuthContext->AddSecret(o.mChallengeSeed.data());
        return (WONStatus)kSend;
    }

    static WONStatus HandleLoginReply(Op& o, const char* m, const std::vector<EeWonCertEvent>& ev)
    {
        ConstSmartPtr<typename Env::Cert1> aCert1;
        ConstSmartPtr<typename Env::Cert2> aCert2;
        typename Env::PrivKey aPrivKey;
        time_t anAuthDelta = 0;

        WONStatus aStatus = (WONStatus)ev[0].num;
        if (aStatus < 0) {
            if (!o.mChallengeSeed.empty())
                o.mAuthContext->RemoveSecret(o.mChallengeSeed.data());
            if (aStatus == -1)
                return TryNext(o, aStatus);
        }

        size_t i = 1;
        for (; i < ev.size() && ev[i].tag != EE_WON_CERT_REST; i++) {
            const EeWonCertEvent& e = ev[i];
            const char* data = m + e.at;
            unsigned short aLen = static_cast<unsigned short>(e.len);
            if (e.tag == EE_WON_CERT_CUT) {
                CertThrowCut();
            } else if (e.tag == EE_WON_CERT_ERROR) {
                o.mErrorList.push_back(std::string(data, e.len));
            } else if (e.tag == EE_WON_CERT_COMMUNITY) {
                o.mCommunityStatusList.push_back(typename Op::COMMUNITY_STATUS());
                typename Op::COMMUNITY_STATUS& aCommunityStatus = o.mCommunityStatusList.back();
                if (e.kind >= 1) aCommunityStatus.mStatus = static_cast<short>(e.num);
                if (e.kind >= 2) aCommunityStatus.mCommunityID = e.id;
                if (e.kind >= 3) WideAssignUtf16LE(aCommunityStatus.mCommunityName, data, e.len);
            } else if (e.kind == 1) {
                aCert1 = new typename Env::Cert1(data, aLen);
                if (!aCert1->IsValid() || !o.mKeyBlock->Verify(aCert1))
                    return TryNext(o, kInvalidCertificate);
                anAuthDelta = aCert1->ComputeAuthDelta();
            } else if (e.kind == 3) {
                ConstSmartPtr<typename Env::KeyBlock> aKeyBlock = new typename Env::KeyBlock(data, aLen);
                if (!aKeyBlock->IsValidAndVerified(o.mAuthContext->GetVerifierKey()))
                    return TryNext(o, kInvalidPubKeyBlock);
                o.mKeyBlock = aKeyBlock;
            } else if (e.kind == 6) {
                aCert2 = new typename Env::Cert2(data, aLen);
                if (!aCert2->IsValid() || !o.mKeyBlock->Verify(aCert2))
                    return TryNext(o, kInvalidCertificate);
                anAuthDelta = aCert2->ComputeAuthDelta();
            }
        }
        if (i == ev.size())
            CertThrowCut();

        if (aStatus < 0)
            return aStatus;

        // Commit secrets and clean up the secret file using the auth server's clock: the issue time and duration
        // of the certificate read (the original took the other one's, a NULL dereference unless both came;
        // with both it used the first's, as here; with none it passed an indeterminate duration)
        time_t anAuthTime = 0, aCertDuration = 0;
        if (aCert1.get() != NULL) {
            anAuthTime = aCert1->GetIssueTime();
            aCertDuration = aCert1->GetExpireTime() - aCert1->GetIssueTime();
        } else if (aCert2.get() != NULL) {
            anAuthTime = aCert2->GetIssueTime();
            aCertDuration = aCert2->GetExpireTime() - aCert2->GetIssueTime();
        }
        o.mAuthContext->CommitSecrets(anAuthTime, aCertDuration);

        const EeWonCertEvent& aRest = ev[i];
        ByteBufferPtr aDecryptBuf = o.mSessionKey.Decrypt(m + aRest.at, static_cast<int>(aRest.len));
        if (aDecryptBuf.get() == NULL)
            return TryNext(o, kDecryptFailure);

        std::vector<EeWonCertEvent> aCrypt;
        const char* d = aDecryptBuf->data();
        ee_won_cert_decrypted(reinterpret_cast<const uint8_t*>(d), aDecryptBuf->length(), CertEventSink, &aCrypt);
        for (size_t j = 0; j < aCrypt.size(); j++) {
            const EeWonCertEvent& e = aCrypt[j];
            if (e.tag == EE_WON_CERT_CUT)
                CertThrowCut();
            const char* data = d + e.at;
            unsigned short aLen = static_cast<unsigned short>(e.len);
            if (e.kind == 2) {
                if (!aPrivKey.SetPrivateKey(data, aLen))
                    return TryNext(o, kInvalidPrivateKey);
            } else if (e.kind == 4) {
                if ((aLen != o.mSessionKey.GetKeyLen() + 2) ||
                    memcmp(data + 2, o.mSessionKey.GetKey(), aLen - 2) != 0)
                    return TryNext(o, kInvalidSecretConfirm);
            }
        }

        if (aCert1.get() == NULL || aCert2.get() == NULL)
            return TryNext(o, kMissingCertificate);

        o.mPeerData = new typename Env::PeerData(aCert1, o.mKeyBlock, aPrivKey, anAuthDelta, aCert2);
        if (!o.mNewPassword.empty())
            o.mAuthContext->SetPassword(o.mNewPassword);

        o.mAuthContext->SetPeerData(o.mPeerData.get());
        o.mAuthContext->ClearOneTimeData();
        return WS_Success;
    }

    static WONStatus CheckResponse(Op& o)
    {
        std::vector<EeWonCertEvent> ev;
        const char* m = o.mResponse->data();
        ee_won_cert_reply(reinterpret_cast<const uint8_t*>(m), o.mResponse->length(), static_cast<int>(o.mState),
                          CertEventSink, &ev);
        const EeWonCertEvent& e = ev[0];
        switch (e.tag) {
        case EE_WON_CERT_INVALID_HEADER:
            return o.InvalidReplyHeader();
        case EE_WON_CERT_SERVER_ERROR:
            return TryNext(o, e.num);
        case EE_WON_CERT_KEY_BLOCK: {
            ConstSmartPtr<typename Env::KeyBlock> aKeyBlock = new typename Env::KeyBlock(m + e.at, e.len);
            if (!aKeyBlock->IsValidAndVerified(o.mAuthContext->GetVerifierKey()))
                return TryNext(o, kInvalidPubKeyBlock);
            o.mKeyBlock = aKeyBlock;
            return (WONStatus)kSend;
        }
        case EE_WON_CERT_UNEXPECTED_CHALLENGE:
            return TryNext(o, kUnexpectedLoginChallenge);
        case EE_WON_CERT_CHALLENGE:
            return HandleLoginChallenge(o, m + e.at, static_cast<unsigned short>(e.len));
        case EE_WON_CERT_LOGIN_STATUS:
            return HandleLoginReply(o, m, ev);
        default:
            CertThrowCut();
        }
        return WS_Success;
    }
};

} // namespace WONAPI
