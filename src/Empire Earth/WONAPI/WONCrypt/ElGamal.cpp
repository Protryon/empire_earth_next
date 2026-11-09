// ElGamal: block encryption/decryption envelopes and MD5 signature checks
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <algorithm>
#include <string.h>
#include <list>
#include <map>
#include <vector>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONElGamal.h"
#include "ee/wonapi/WONBuffer.h"
#include "ee/wonapi/WONIntegerExtractor.h"
#include "ee/wonapi/WONArithmeticException.h"
namespace WONAPI {
// FUNCTION: WON 0x007d7b50
// Returns the data encrypted in modulus-sized blocks behind a block count; NULL without a public key
ByteBufferPtr ElGamal::Encrypt(const void *thePlainText, int thePlainTextLen) const
{
	if(!this->IsPublic())
		return NULL;

	const unsigned char *aPlainText = static_cast<const unsigned char*>(thePlainText);

	int aBlockLen = this->modulusLen - 3;
	int aNumBlock = thePlainTextLen / aBlockLen;
	if ((thePlainTextLen % aBlockLen) != 0) aNumBlock++;

	WriteBuffer anEncrypt(0,true);
	anEncrypt.Reserve(4+this->modulusLen*2*aNumBlock);

	int anOffset = 0;

	anEncrypt.AppendLong(aNumBlock);

	while(anOffset < thePlainTextLen)
	{	
		int thisBlockLen = aBlockLen;
		
		if(thePlainTextLen - anOffset < aBlockLen)
			thisBlockLen = thePlainTextLen - anOffset;
		
		RawBuffer anEncryptBlock(this->modulusLen-1,(unsigned char)0);

		for(int k=0,j=this->modulusLen-2-thisBlockLen; j<this->modulusLen-2; j++,k++)
			anEncryptBlock[j] = aPlainText[anOffset+k];

		anEncryptBlock[this->modulusLen - 2] = (unsigned char)thisBlockLen;


		BigInteger ab[2];
		
		if(!this->encrypt(BigInteger(anEncryptBlock),ab))
			return NULL;


		RawBuffer aa,bb;
		ab[0].toBinary(aa);
		ab[1].toBinary(bb);
		
		if(aa.length()==this->modulusLen)
			anEncrypt.AppendBytes(aa.data(),aa.length());
		else if(aa.length()>this->modulusLen)
			anEncrypt.AppendBytes(aa.data()+aa.length()-this->modulusLen,this->modulusLen);
		else
		{
			for(int i=aa.length(); i<this->modulusLen; i++)
				anEncrypt.AppendByte(0);
			
			anEncrypt.AppendBytes(aa.data(),aa.length());
		}
		

		if(bb.length()==this->modulusLen)
			anEncrypt.AppendBytes(bb.data(),bb.length());
		else if(bb.length()>this->modulusLen)
			anEncrypt.AppendBytes(bb.data()+bb.length()-this->modulusLen,this->modulusLen);
		else
		{
			for(int i=bb.length(); i<this->modulusLen; i++)
				anEncrypt.AppendByte(0);
			
			anEncrypt.AppendBytes(bb.data(),bb.length());
		}						
		anOffset+=thisBlockLen;		
	}		

	return anEncrypt.ToByteBuffer(true);
}
// FUNCTION: WON 0x007d7fc0
// Returns the decrypted data of a block envelope; NULL without a private key or on bad input
ByteBufferPtr ElGamal::Decrypt(const void *theCipherText, int theCipherTextLen) const
{
	if(!this->IsPrivate())
		return NULL;

	const unsigned char *in = static_cast<const unsigned char*>(theCipherText);
	int inOffset = 0;

	if(theCipherTextLen-inOffset<4) return NULL;
	int aNumBlocks = *reinterpret_cast<const int*>(in); inOffset+=4;

	if(theCipherTextLen-inOffset < aNumBlocks*this->modulusLen*2-inOffset)
		return NULL;

	RawBuffer aBuf(this->modulusLen,(unsigned char)0);
	RawBuffer bBuf(this->modulusLen,(unsigned char)0);

	WriteBuffer aDecrypt(0,true);

	BigInteger a;;
	BigInteger b;
		
	BigInteger aPlainText;

	for(int i=0; i<aNumBlocks; i++)
	{
		aBuf.assign(in+inOffset,this->modulusLen); inOffset+=this->modulusLen;
		bBuf.assign(in+inOffset,this->modulusLen); inOffset+=this->modulusLen;
		
		a.fromBinary(aBuf);
		b.fromBinary(bBuf);

		if(!this->decrypt(a,b,aPlainText))
			return NULL;
		
		RawBuffer aBigIntArray;
		aPlainText.toBinary(aBigIntArray);
		

		if(aBigIntArray.length()==0) return NULL;
		int aPlainLen = aBigIntArray[aBigIntArray.length() - 1];
		
		if(aPlainLen>this->modulusLen - 3)
			return NULL;
		
		aDecrypt.AppendBytes(aBigIntArray.data()+aBigIntArray.length()-1-aPlainLen,aPlainLen);
	}

	return aDecrypt.ToByteBuffer(true);
}
// FUNCTION: WON 0x007d84a0
// Converts a digest to the integer signed with q, truncated to q's bit length
bool ElGamal::EncodeDigest(const RawBuffer& digest, BigInteger &h) const
{
	IntegerExtractor aDecoder(digest.data(),digest.length(),false);
	if(digest.length()*8 < this->q.bitLength())
	{
		if(!aDecoder.Decode(digest.length(),h))
			return false;
	}
	else
	{
		if(!aDecoder.Decode(this->q.byteLength(),h))
			return false;

		h = h.shiftRight(this->q.byteLength()*8 - this->q.bitLength() + 1);
	}

	return true;
}
// FUNCTION: WON 0x007d8620
// Checks an ElGamal signature over the message's MD5 digest with the public key
bool ElGamal::Verify(const void *theMessage, int theMessageLen, const void *theSignature, int theSignatureLen) const
{
	if(!this->IsPublic())
		return false;

	if(theSignatureLen!=this->q.byteLength()*2)
		return false;
	
	
	MD5Digest anMD5;
	anMD5.update(theMessage,theMessageLen);
	RawBuffer aDigest = anMD5.digest();
			

	BigInteger M;
	if(!this->EncodeDigest(aDigest,M))
		return false;

	const unsigned char *in = static_cast<const unsigned char*>(theSignature);

	RawBuffer aBuf(this->q.byteLength(),(unsigned char)0);
	RawBuffer bBuf(this->q.byteLength(),(unsigned char)0);

	aBuf.assign(in,aBuf.length());
	bBuf.assign(in+aBuf.length(),bBuf.length());
					
	BigInteger a(aBuf);
	BigInteger b(bBuf);
	
	return this->BogusVerify(M,a,b);	
}


using namespace std;
// No original address identified: WON SDK body
ByteBufferPtr ElGamal::Sign(const void* theMessage, int theMessageLen) const
{		
	if(!this->IsPrivate())
		return NULL;

	MD5Digest anMD5;
	anMD5.update(theMessage, theMessageLen);
	RawBuffer aDigest = anMD5.digest();
	
	BigInteger M;
	if(!this->EncodeDigest(aDigest,M))
		return NULL;
	
	BigInteger ab[2];
	
	if(!this->BogusSign(M,ab))
		return NULL;

	WriteBuffer aSignature(0,true);
		
	int qLen = this->q.byteLength();
	RawBuffer a,b;
	ab[0].toBinary(a);
	ab[1].toBinary(b);

	if(a.length()==qLen)
		aSignature.AppendBytes(a.data(),a.length());
	else if(a.length()>qLen)
		aSignature.AppendBytes(a.data()+a.length()-qLen,qLen);
	else
	{
		for(int i=a.length(); i<qLen; i++)
			aSignature.AppendByte(0);
		
		aSignature.AppendBytes(a.data(),a.length());
	}
	

	if(b.length()==qLen)
		aSignature.AppendBytes(b.data(),b.length());
	else if(b.length()>qLen)
		aSignature.AppendBytes(b.data()+b.length()-qLen,qLen);
	else
	{
		for(int i=b.length(); i<qLen; i++)
			aSignature.AppendByte(0);
		
		aSignature.AppendBytes(b.data(),b.length());
	}						
	
	return aSignature.ToByteBuffer(true);
}
// FUNCTION: WON 0x007d8990
// Encrypts one block value with a random odd k coprime to p-1; false on an arithmetic error
bool ElGamal::encrypt(const BigInteger &M, BigInteger ab[]) const
{
	try
	{
		BigInteger p_minus_1 = this->p.sub(BigInteger::ONE);
		// choose a random k, relatively prime to p-1.
		BigInteger k;
		do {
			// values of k with the same number of bits as p won't be chosen, but
			// that shouldn't be a problem.
			k = BigInteger(this->p.bitLength()-1, this->mRandom);
			if (!(k.testBit(0)))
				k = k.setBit(0); // make sure k is odd
		} while (!(k.gcd(p_minus_1).equals(BigInteger::ONE)));

		ab[0] /* a */ = this->g.modExp(k, this->p, this->recip, this->nb);
		ab[1] /* b */ = this->y.modExp(k, this->p, this->recip, this->nb).modMulRecip(M,this->p,this->recip,this->nb);

		return true;
	}
	catch(ArithmeticException&)
	{
	}

	return false;
}
// FUNCTION: WON 0x007d8b70
// Decrypts one (a, b) block pair with the private key; false on an arithmetic error
bool ElGamal::decrypt(const BigInteger &a, const BigInteger &b, BigInteger &out) const
{
    try 
	{
		out = b.modMulRecip(a.modExp(this->x, this->p, this->recip, this->nb).inverseModN(this->p), this->p, this->recip, this->nb);
		return true;
    } 
	catch (ArithmeticException &) 
	{
    }
	
	return false;
}
// FUNCTION: WON 0x007d8c50
// Checks the signature equation a == (g^b * y^a + M) mod q with a non-zero; false otherwise
bool ElGamal::BogusVerify(const BigInteger &M, const BigInteger &a, const BigInteger &b) const 
{

// check a != 0 && a == (g^b * y^a + m) mod q
	
	try
	{
		if(a.cmp(BigInteger::ZERO)==0)
			return false;

//		BigInteger a1 = a.mod(q);
//		BigInteger b1 = b.mod(q);
		
		BigInteger gbq = this->g.modExp(b,this->p,this->recip,this->nb);
		BigInteger yaq = this->y.modExp(a,this->p,this->recip,this->nb);

	/*
		cout << "p = " << p << endl;
		cout << "g = mod(" << g << ",p)" << endl;
		cout << "a = " << a << endl;
		cout << "b = " << b << endl;
		cout << "y = " << y << endl;

		cout << "gbq = " << gbq << endl;
		cout << "yaq = " << yaq << endl;*/
		
		BigInteger result;

		result = gbq.modMulRecip(yaq,this->p,this->recip,this->nb);
//		result = gbq.mul(yaq);
		result = result.add(M);
//		result = result.mod(p);
		result = result.mod(this->q);
		
		if(a.cmp(result)==0)
			return true;
		else
			return false;
	}
	catch(ArithmeticException &)
	{
	}

	return false;
}


using namespace std;
// No original address identified: WON SDK body
BigInteger ElGamal::GetRandBetween(Random &rng, const BigInteger &min, const BigInteger &max)
{
	BigInteger range = max.sub(min);
	int nBits = range.bitLength();

	BigInteger anInt;
	do
	{
		anInt = BigInteger(nBits,rng);
	}
	while (anInt.cmp(range)>0);

	anInt = anInt.add(min);
	return anInt;
}

// No original address identified: WON SDK body
bool ElGamal::BogusSign(const BigInteger &M, BigInteger ab[]) const
{
	try 
	{
		BigInteger TWO(2);		
		BigInteger k;
		
		do
		{
			k = ElGamal::GetRandBetween(this->mRandom, TWO, this->q.sub(TWO));			
			ab[0] = this->g.modExp(k,this->p);
			
			
			ab[0] = ab[0].add(M);
			
			ab[0] = ab[0].mod(this->p);
			
			ab[0] = ab[0].mod(this->q);
			
			ab[1] = k.sub(this->x.mul(ab[0]));

			
			ab[1] = ab[1].mod(this->q);
			
			
			//r = (gpc.Exponentiate(k) + m) % q;
			//s = (k - x*r) % q;
		} while (ab[0].cmp(BigInteger::ZERO)==0);			// make sure r != 0

		return true;
	}
	catch(ArithmeticException &)
	{
	}

	return false;
}
}
