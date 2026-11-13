// WON's crypto, auth and digest classes as forwarders to ee_won (rust/crates/ee_won): the C++ API round trip on every
// OS. CRC16 and MD5 against known values, Blowfish by value (copies keep the key), ElGamal keys copied deeply and
// signing and verifying, a key block (AuthBase over ee_won's peerobj) signed by its own key. Links with the won module's forwarder units and
// the Rust library. Prints one line per failed check; the exit code is the number of failed checks.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONBuffer.h"
#include "ee/wonapi/WONCRC16.h"
#include "ee/wonapi/WONMD5.h"
#include "ee/wonapi/WONMD5Digest.h"
#include "ee/wonapi/WONBlowfish.h"
#include "ee/wonapi/WONElGamal.h"
#include "ee/wonapi/WONAuthPeerData.h"

using namespace WONAPI;

static int sFailures;
#define CHECK(c) do { if (!(c)) { sFailures++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static std::string Hex(const void* inData, unsigned long inLength)
{
    std::string s;
    char b[4];
    for (unsigned long i = 0; i < inLength; i++) { sprintf(b, "%02x", static_cast<const unsigned char*>(inData)[i]); s += b; }
    return s;
}

static std::string Unhex(const char* inText)
{
    std::string s;
    for (; inText[0] && inText[1]; inText += 2) { unsigned v; sscanf(inText, "%2x", &v); s += static_cast<char>(v); }
    return s;
}

int main()
{
    // CRC-16/UMTS of "123456789"
    CRC16 crc;
    crc.Put("1234", 4);
    crc.Put("56789", 5);
    CHECK(crc.Get() == 0xfee8);

    // RFC 1321
    MD5 md5;
    md5.Update("message ", 8);
    md5.Update("digest", 6);
    unsigned char digest[16];
    md5.Digest(digest);
    CHECK(Hex(digest, 16) == "f96b697d7cb7938d525a2f31aaf161d0");
    MD5Digest md5d;
    md5d.update("abc", 3);
    RawBuffer d = md5d.digest();
    CHECK(Hex(d.data(), static_cast<unsigned long>(d.length())) == "900150983cd24fb0d6963f7d28e17f72");
    CHECK(Hex(md5d.digest(RawBuffer(reinterpret_cast<const unsigned char*>("abc"), 3)).data(), 16) ==
          "900150983cd24fb0d6963f7d28e17f72");

    // Blowfish: no key, a key, a copy, the framing
    Blowfish none;
    CHECK(none.Encrypt("x", 1).get() == NULL);
    Blowfish bf("secret", 6);
    CHECK(bf.GetKeyLen() == 6 && memcmp(bf.GetKey(), "secret", 6) == 0);
    ByteBufferPtr enc = bf.Encrypt("hello, world", 12);
    CHECK(enc.get() != NULL && enc->length() == 24);
    Blowfish copy = bf;
    ByteBufferPtr dec = copy.Decrypt(enc->data(), enc->length());
    CHECK(dec.get() != NULL && dec->length() == 12 && memcmp(dec->data(), "hello, world", 12) == 0);
    CHECK(bf.Decrypt(enc->data(), enc->length() - 1).get() == NULL);
    CHECK(!bf.SetKey("abcd", 4) && bf.Encrypt("x", 1).get() == NULL);
    CHECK(copy.Create(16) && copy.GetKeyLen() == 16);

    // ElGamal: a test key (512-bit p, 160-bit q; ee_won's golden_won.txt), signing with the private key, verifying
    // with copies
    std::string priv = Unhex(
        "3081f5024100c37d6f28fb6c0e9aa489f1d80d5a41952bb7d31ebeb9ca0031dcf9a0c134e9395d5e21986c08c9491a49c334"
        "a82d7fff2603de67ab87a6801de67f3c3dec7211021500eeac675c1162f5b42138f37567274df60cbb1485024100abbb86ba"
        "89355c5c884dfc2f9c1e6692390d87ef1220f9eb98b763cc3631c652f83640f980949efa8b80256910f55e629ce126227b13"
        "b856bad2720bd64c60360240228d68730c33fa487c1f0c0154e0dec1f064072e366b35b75439c4f08e3a0fe4fd52a3d18348"
        "0fc30601fe4ed2739518140e9196a72f1bce2e908e04c732843c02145c37feebabe4399fe2f449a19ced631c5254d013");
    std::string pub = Unhex(
        "3081df024100c37d6f28fb6c0e9aa489f1d80d5a41952bb7d31ebeb9ca0031dcf9a0c134e9395d5e21986c08c9491a49c334"
        "a82d7fff2603de67ab87a6801de67f3c3dec7211021500eeac675c1162f5b42138f37567274df60cbb1485024100abbb86ba"
        "89355c5c884dfc2f9c1e6692390d87ef1220f9eb98b763cc3631c652f83640f980949efa8b80256910f55e629ce126227b13"
        "b856bad2720bd64c60360240228d68730c33fa487c1f0c0154e0dec1f064072e366b35b75439c4f08e3a0fe4fd52a3d18348"
        "0fc30601fe4ed2739518140e9196a72f1bce2e908e04c732843c");
    ElGamal signer(priv.data(), static_cast<int>(priv.length()), true);
    CHECK(signer.IsPrivate() && signer.IsPublic());
    ByteBufferPtr raw = signer.GetRawPrivateKey();
    CHECK(raw.get() != NULL && std::string(raw->data(), raw->length()) == priv);
    ElGamal verifier;
    CHECK(!verifier.IsPublic());
    CHECK(verifier.SetPublicKey(pub.data(), static_cast<int>(pub.length())));
    ElGamal assigned;
    assigned = verifier;
    std::list<ElGamal> keys;
    keys.push_back(assigned);
    ByteBufferPtr sig = signer.Sign("message", 7);
    CHECK(sig.get() != NULL && sig->length() == 40);
    CHECK(keys.front().Verify("message", 7, sig->data(), sig->length()));
    CHECK(!verifier.Verify("massage", 7, sig->data(), sig->length()));
    CHECK(verifier.Sign("message", 7).get() == NULL);
    verifier.Invalidate();
    CHECK(!verifier.Verify("message", 7, sig->data(), sig->length()));
    CHECK(keys.front().IsPublic());

    // a public key block holding that key, signed by it
    WriteBuffer block(0, false);
    block.AppendShort(3);
    block.AppendLong(1000);
    block.AppendLong(2000000000);
    block.AppendShort(17);
    block.AppendShort(1);
    block.AppendShort(static_cast<short>(pub.length()));
    block.AppendBytes(pub.data(), static_cast<unsigned long>(pub.length()));
    ByteBufferPtr blockSig = signer.Sign(block.data(), block.length());
    block.AppendBytes(blockSig->data(), blockSig->length());
    AuthBase* keyBlock = new AuthBase(EE_WON_AUTH_PUB_KEY_BLOCK, block.data(), block.length());
    keyBlock->AddRef();
    CHECK(keyBlock->IsValid());
    CHECK(keyBlock->Num(EE_WON_AUTHOBJ_BLOCK_ID) == 17);
    CHECK(keyBlock->GetIssueTime() == 1000 && keyBlock->GetExpireTime() == 2000000000);
    CHECK(keyBlock->IsValidAndVerified(signer));
    CHECK(keyBlock->Num(EE_WON_AUTHOBJ_HAS_KEY) == 1 && keyBlock->Encrypt("x", 1).get() != NULL);
    CHECK(keyBlock->Verify(keyBlock));
    CHECK(keyBlock->Num(EE_WON_AUTHOBJ_EXPIRED, 0) == 0);
    AuthBase* truncated = new AuthBase(EE_WON_AUTH_PUB_KEY_BLOCK, block.data(), 20);
    truncated->AddRef();
    CHECK(!truncated->IsValid() && truncated->Num(EE_WON_AUTHOBJ_EXPIRED, 0) == 1);
    CHECK(!keyBlock->Verify(truncated));
    truncated->Release();
    keyBlock->Release();

    printf("won_crypto_test: %d failed\n", sFailures);
    return sFailures;
}
