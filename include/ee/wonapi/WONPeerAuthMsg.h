// PeerAuthClient's and PeerAuthServer's handshake through ee_won's peerauth module
// (rust/crates/ee_won/src/peerauth.rs): Rust reads and writes the messages, keeps the state and the order of every
// step; the certificates, the peer data's keys and the two secrets stay the class's, reached through its Pa*
// methods as hooks. A template over the class so that tests/won/won_peerauth_test.cpp runs the same code over a
// stand-in; the classes make it a friend. WONStatus and AuthType come from WONRouting.h, included first.
#pragma once
#include <stdint.h>
#include <string>
#include "rust/ee_won.h"

namespace WONAPI {

inline void PeerAuthStringSink(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    static_cast<std::string*>(inContext)->assign(reinterpret_cast<const char*>(inBytes), inLength);
}

// P provides (each as the original's statement it stands for):
//   bool PaPrivateDecrypt(const void*, unsigned long, std::string&)  mPeerData->GetPrivateKey().Decrypt, false for NULL
//   bool PaMakeCert(bool auth2, const void*, unsigned long)          the other side's certificate = new ...; IsValid()
//   bool PaVerifyCert()                                              mPeerData->Verify(certificate)
//   bool PaCertExpired()                                             certificate->IsExpired(mPeerData->GetAuthDelta())
//   bool PaCertEncrypt(const void*, unsigned long, std::string&)     certificate->GetPubKey().Encrypt, false for NULL
//   bool PaHasOwnCert2()                                             mPeerData->GetCertificate2()!=NULL
//   void PaOwnCertRaw(bool auth2, std::string&)                      mPeerData's certificate's raw bytes
//   void PaSecretCreate(int which, int length)                       mSecretA/B.Create
//   bool PaSecretSetKey(int which, const void*, int)                 mSecretA/B.SetKey
//   void PaSecretKey(int which, std::string&)                        mSecretA/B's key
//   void PaNewSession(int type, unsigned short id, int which, unsigned char lfs)   mSession = new AuthSession
//   void PaGetState(EeWonPeerState&), void PaSetState(const EeWonPeerState&)       the members the handshake keeps
template <class P> struct PeerAuthMsg {
    static P& Of(void* c) { return *static_cast<P*>(c); }
    static bool Decrypt(void* c, const uint8_t* d, size_t n, EeWonByteSink s, void* sc)
    {
        std::string out;
        if (!Of(c).PaPrivateDecrypt(d, static_cast<unsigned long>(n), out)) return false;
        s(sc, reinterpret_cast<const uint8_t*>(out.data()), out.size());
        return true;
    }
    static bool MakeCert(void* c, bool auth2, const uint8_t* d, size_t n)
    {
        return Of(c).PaMakeCert(auth2, d, static_cast<unsigned long>(n));
    }
    static bool Verify(void* c) { return Of(c).PaVerifyCert(); }
    static bool Expired(void* c) { return Of(c).PaCertExpired(); }
    static bool Encrypt(void* c, const uint8_t* d, size_t n, EeWonByteSink s, void* sc)
    {
        std::string out;
        if (!Of(c).PaCertEncrypt(d, static_cast<unsigned long>(n), out)) return false;
        s(sc, reinterpret_cast<const uint8_t*>(out.data()), out.size());
        return true;
    }
    static bool HasOwn2(void* c) { return Of(c).PaHasOwnCert2(); }
    static void OwnRaw(void* c, bool auth2, EeWonByteSink s, void* sc)
    {
        std::string out;
        Of(c).PaOwnCertRaw(auth2, out);
        s(sc, reinterpret_cast<const uint8_t*>(out.data()), out.size());
    }
    static void Create(void* c, uint8_t which, int32_t n) { Of(c).PaSecretCreate(which, n); }
    static bool SetKey(void* c, uint8_t which, const uint8_t* d, size_t n)
    {
        return Of(c).PaSecretSetKey(which, d, static_cast<int>(n));
    }
    static void Key(void* c, uint8_t which, EeWonByteSink s, void* sc)
    {
        std::string out;
        Of(c).PaSecretKey(which, out);
        s(sc, reinterpret_cast<const uint8_t*>(out.data()), out.size());
    }
    static void Session(void* c, int32_t type, uint16_t id, uint8_t which, uint8_t lfs)
    {
        Of(c).PaNewSession(type, id, which, lfs);
    }
    static EeWonPeerHooks Hooks(P& o)
    {
        EeWonPeerHooks h;
        h.context = &o;
        h.private_decrypt = Decrypt;
        h.make_cert = MakeCert;
        h.verify_cert = Verify;
        h.cert_expired = Expired;
        h.cert_encrypt = Encrypt;
        h.has_own_cert2 = HasOwn2;
        h.own_cert_raw = OwnRaw;
        h.secret_create = Create;
        h.secret_set_key = SetKey;
        h.secret_key = Key;
        h.new_session = Session;
        return h;
    }

    // PeerAuthClient::Start after mPeerData = theData: the request to the sink
    static void ClientStart(P& o, int theType, unsigned char theLengthFieldSize, EeWonByteSink inSink, void* inContext)
    {
        EeWonPeerHooks h = Hooks(o);
        EeWonPeerState s;
        o.PaGetState(s);
        ee_won_peer_client_start(&s, &h, theType, theLengthFieldSize, inSink, inContext);
        o.PaSetState(s);
    }
    // HandleRecvMsg (the caller set its out message to NULL): the status; the out message to the sink when one came
    static long ClientRecv(P& o, const void* inMsg, unsigned long inLen, EeWonByteSink inSink, void* inContext)
    {
        EeWonPeerHooks h = Hooks(o);
        EeWonPeerState s;
        o.PaGetState(s);
        bool aHasOut = false;
        int32_t aStatus = ee_won_peer_client_recv(&s, &h, static_cast<const uint8_t*>(inMsg), inLen, inSink,
                                                  inContext, &aHasOut);
        o.PaSetState(s);
        return aStatus;
    }
    static void ServerStart(P& o, unsigned char theLengthFieldSize)
    {
        EeWonPeerState s;
        o.PaGetState(s);
        ee_won_peer_server_start(&s, theLengthFieldSize);
        o.PaSetState(s);
    }
    static long ServerRecv(P& o, const void* inMsg, unsigned long inLen, EeWonByteSink inSink, void* inContext)
    {
        EeWonPeerHooks h = Hooks(o);
        EeWonPeerState s;
        o.PaGetState(s);
        bool aHasOut = false;
        int32_t aStatus = ee_won_peer_server_recv(&s, &h, static_cast<const uint8_t*>(inMsg), inLen, inSink,
                                                  inContext, &aHasOut);
        o.PaSetState(s);
        return aStatus;
    }
};

} // namespace WONAPI
