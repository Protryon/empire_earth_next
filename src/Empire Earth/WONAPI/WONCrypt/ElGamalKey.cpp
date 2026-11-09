// ElGamal key loading from ASN.1 integer sequences (and the SDK key writer)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONIntegerExtractor.h"
#include "ee/wonapi/WONElGamal.h"
namespace WONAPI {
// The original's default constructor is at 0x007d7690 (with the seven BigInteger member constructors)
ElGamal::ElGamal()
{
	this->Invalidate();
}

// The original's key constructor is at 0x007d7740
ElGamal::ElGamal(const void *theKey, int theKeyLen, bool isPrivate)
{
	if(isPrivate)
		this->SetPrivateKey(theKey,theKeyLen);
	else
		this->SetPublicKey(theKey,theKeyLen);
}

// FUNCTION: WON 0x007d7850
// Loads a private key (p, q, g, y, x) and its reduction reciprocal; false when the data is malformed
bool ElGamal::SetPrivateKey(const void *theKey, int theKeyLen)
{		
	this->Invalidate();

	IntegerExtractor anExtractor(theKey,theKeyLen);
	bool aSuccess = anExtractor.Extract(this->p) && anExtractor.Extract(this->q) && anExtractor.Extract(this->g) 
			   && anExtractor.Extract(this->y) && anExtractor.Extract(this->x);	
	
	if(!aSuccess)
		return false;
	
	this->modulusLen = this->p.bitLength()/8;

	this->recip = this->p.recip(this->nb);
	

	this->mIsPrivate = true;
	this->mIsPublic = true;
	return true;
}

// FUNCTION: WON 0x007d79e0
// Loads a public key (p, q, g, y) and its reduction reciprocal; false when the data is malformed
bool ElGamal::SetPublicKey(const void *theKey, int theKeyLen)
{		
	this->Invalidate();

	IntegerExtractor anExtractor(theKey,theKeyLen);
	bool aSuccess = anExtractor.Extract(this->p) && anExtractor.Extract(this->q) && anExtractor.Extract(this->g)
		&& anExtractor.Extract(this->y);
	
	if(!aSuccess)
		return false;

	this->modulusLen = this->p.bitLength()/8;

	this->recip = this->p.recip(this->nb);

	this->mIsPublic = true;
	return true;
}

}

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
namespace WONAPI {
using namespace std;
// No original address identified: WON SDK body
ByteBufferPtr ElGamal::GetRawPrivateKey() const
{
	if(!this->IsPrivate())
		return NULL;

	IntegerInserter anInserter;
	anInserter.Insert(this->p);
	anInserter.Insert(this->q);
	anInserter.Insert(this->g);
	anInserter.Insert(this->y);
	anInserter.Insert(this->x);

	return anInserter.Get();
}

// No original address identified: WON SDK body
ByteBufferPtr ElGamal::GetRawPublicKey() const
{
	if(!this->IsPublic())
		return NULL;

	IntegerInserter anInserter;
	anInserter.Insert(this->p);
	anInserter.Insert(this->q);
	anInserter.Insert(this->g);
	anInserter.Insert(this->y);

	return anInserter.Get();
}

// No original address identified: WON SDK body
IntegerInserter::IntegerInserter()
{
}

// No original address identified: WON SDK body
void IntegerInserter::AddLength(RawBuffer &theBuf, int theLen)
{
	if(theLen>0x7f)
	{
		if(theLen>0xff)
		{
			theBuf+=0x82; // 2 length bytes

			unsigned char aLowLen = ((unsigned short)theLen)>>8;
			unsigned char aHighLen = theLen & 0xff;
			theBuf+=aHighLen;
			theBuf+=aLowLen;
		}
		else
		{
			theBuf+=0x81; // 1 length byte
			theBuf+=(unsigned char)theLen;
		}
	}
	else
	{
		theBuf+=(unsigned char)theLen;
	}

}

// No original address identified: WON SDK body
ByteBufferPtr IntegerInserter::Get()
{
	RawBuffer aHeader;
	aHeader += (unsigned char)(SEQUENCE | CONSTRUCTED);
	this->AddLength(aHeader,this->out.length());
	
	char *aBuf = new char[aHeader.length() + this->out.length()];
	memcpy(aBuf,aHeader.data(),aHeader.length());
	memcpy(aBuf+aHeader.length(),this->out.data(),this->out.length());
	return new ByteBuffer(aBuf,aHeader.length() + this->out.length(),true);
}

// No original address identified: WON SDK body
void IntegerInserter::Insert(const BigInteger &theInt)
{
	this->out+=(unsigned char)INTEGER;
	RawBuffer aBuf;
	theInt.toBinary(aBuf);
	this->AddLength(this->out,aBuf.length());
	this->out+=aBuf;	
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007d7690
// (public: __thiscall WONAPI::ElGamal::ElGamal(void)    [??0ElGamal@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x007d7740
// (public: __thiscall WONAPI::ElGamal::ElGamal(void const *,int,bool)    [??0ElGamal@WONAPI@@QAE@PBXH_N@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006e93e0
// (public: __thiscall WONAPI::IntegerInserter::IntegerInserter(void)    [??0IntegerInserter@WONAPI@@QAE@XZ])
