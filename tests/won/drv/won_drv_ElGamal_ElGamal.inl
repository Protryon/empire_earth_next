// ElGamal's members only the drivers use, cut from ee/wonapi/WONElGamal.h (R6 WON round 38): included in its
// class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
bool IsPublic() const { return ee_won_elgamal_is_public(this->mKey); } //3d7840
ByteBufferPtr GetRawPrivateKey() const { return this->Run(EeWonElGamalOp::RawPrivateKey, NULL, 0); }
ByteBufferPtr GetRawPublicKey() const { return this->Run(EeWonElGamalOp::RawPublicKey, NULL, 0); }
