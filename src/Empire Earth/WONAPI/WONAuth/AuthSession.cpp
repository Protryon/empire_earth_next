// AuthSession: Blowfish session encryption of server messages with sequence numbers
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <string.h>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include <time.h>
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONBlowfish.h"
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {
// FUNCTION: WON 0x006e27b0
// Creates a session with its type, id, Blowfish key and length-field size; sequences start at 0
AuthSession::AuthSession(AuthType theType, unsigned short theId, Blowfish theKey, 
						 unsigned char theLengthFieldSize) : mAuthType(theType), mId(theId), 
						 mKey(theKey), mLengthFieldSize(theLengthFieldSize), mInSeq(0), mOutSeq(0) 
{
	this->mLastUseTime = time(NULL);
}

// FUNCTION: WON 0x006e2a80
// Destroys the session (members only)
AuthSession::~AuthSession()
{
}

// FUNCTION: WON 0x006e2b40
// Replaces the message with its encrypted form (session id and sequence number when sessioned)
WONStatus AuthSession::Encrypt(ByteBufferPtr &theMsg)
{
	this->mLastUseTime = time(NULL);

	if(this->mAuthType==AUTH_TYPE_NONE || this->mAuthType==AUTH_TYPE_PERSISTENT_NOCRYPT)
		return WS_Success;

	WriteBuffer aMsg(this->mLengthFieldSize, true);
	aMsg.AppendByte(12); // encrypted message

	int anOffset = this->mLengthFieldSize;
	if(anOffset==2 && theMsg->length()>=0xffff) // extended 2-byte length
		anOffset = 6;

	WriteBuffer aSeqBuf(0, true);
	const char *aBuf = theMsg->data() + anOffset;
	unsigned long aLen = theMsg->length() - anOffset;

	if(this->mAuthType==AUTH_TYPE_SESSION)
	{
		aMsg.AppendShort(this->mId);
		aSeqBuf.AppendShort(++this->mOutSeq);
		aSeqBuf.AppendBytes(aBuf,aLen);
		aBuf = aSeqBuf.mData;
		aLen = aSeqBuf.mLength;
	}

	ByteBufferPtr anEncrypt = this->mKey.Encrypt(aBuf,aLen);
	if(anEncrypt.get()==NULL)
		return WS_AuthSession_EncryptFailure;
	
	aMsg.AppendBytes(anEncrypt->data(),anEncrypt->length());
	theMsg = aMsg.ToByteBuffer(true);
	return WS_Success;
}

// FUNCTION: WON 0x006e2d60
// Decrypts an encrypted service message in place, checking session id and sequence; returns the status
WONStatus AuthSession::Decrypt(ByteBufferPtr &theMsg)
{
	this->mLastUseTime = time(NULL);

	try
	{
		if(this->mAuthType==AUTH_TYPE_NONE || this->mAuthType==AUTH_TYPE_PERSISTENT_NOCRYPT)
			return WS_Success;

		ReadBuffer anIn(theMsg->data(),theMsg->length());
		WriteBuffer anOut(0, true);
		unsigned char headerType = anIn.ReadByte();
		switch (headerType)
		{
			case 2:							break;	//WONMsg::EncryptedService:
			case 4:	anOut.AppendByte(3);	break;	//WONMsg::MiniEncryptedService:
			case 6:	anOut.AppendByte(5);	break;	//WONMsg::SmallEncryptedService:
			case 8:	anOut.AppendByte(7);	break;	//WONMsg::LargeEncryptedService:
			case 12:						break;	//WONMsg::HeaderEncryptedService:

			default:
				return WS_Success;
		}

		bool sessioned = this->mAuthType==AUTH_TYPE_SESSION;

		if(sessioned)
		{
			unsigned short aSessionId = anIn.ReadShort();
			if(aSessionId!=this->mId)
				return WS_AuthSession_DecryptSessionIdMismatch;
		}

		ByteBufferPtr aDecrypt = this->mKey.Decrypt(anIn.mData + anIn.mPos, anIn.mLength - anIn.mPos);
		if(aDecrypt.get()==NULL)
			return WS_AuthSession_DecryptFailure;

		if(sessioned)
		{
			if(aDecrypt->length()<2)
				return WS_AuthSession_DecryptBadLen;

			if(++this->mInSeq!=*(const unsigned short*)aDecrypt->data()) // sequence mismatch
				return WS_AuthSession_DecryptInvalidSequenceNum;

			anOut.AppendBytes(aDecrypt->data()+2,aDecrypt->length()-2);
		}
		else
			anOut.AppendBytes(aDecrypt->data(),aDecrypt->length());

		theMsg = anOut.ToByteBuffer(true);
		return WS_Success;
	}
	catch(ReadBufferException&)
	{
		return WS_AuthSession_DecryptUnpackFailure;
	}
}

}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006e2a60
// (protected: virtual void * __thiscall WONAPI::AuthSession::`scalar deleting destructor'(unsigned int)    [??_GAuthSession@WONAPI@@MAEPAXI@Z])
