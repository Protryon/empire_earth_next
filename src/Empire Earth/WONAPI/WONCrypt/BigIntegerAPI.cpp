// BigInteger: lifetime, bit access, random bits and the member-function API over the st_ helpers

// ZERO and ONE are defined in BigInteger.cpp
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <algorithm>
#include <string.h>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONBigInteger.h"
#include "ee/wonapi/WONRandom.h"
namespace WONAPI {
// FUNCTION: WON 0x007d3020
// Returns numBits random bits as big-endian bytes, masking the excess high bits
RawBuffer BigInteger::st_randomBits(int numBits, Random &rndSrc)
{		
	int numBytes = (numBits+7)/8;
	RawBuffer randomBits(numBytes,(unsigned char)0);

	// Generate random bytes and mask out any excess bits
	if (numBytes > 0) 
	{
		rndSrc.nextBytes(randomBits);
		int excessBits = 8*numBytes - numBits;
		randomBits[0] &= (1 << (8-excessBits)) - 1;
	}
	return randomBits;
}

// FUNCTION: WON 0x007d2fd0
// Returns the number of bytes needed for the magnitude
int BigInteger::byteLength() const
{
	return ((BigInteger::st_bitLength(*this) + 7) / 8);
}

// FUNCTION: WON 0x007d3160
// Returns whether bit i of the value is set (false beyond its length)
bool BigInteger::st_bit(const BigInteger &n, int i)
{
    int bit = i % 32;
    i /= 32;

    if (i >= n.len || ((n.n[i] & (1L << bit)) == 0))
        return false;
	
    return true;
}

// FUNCTION: WON 0x007d31b0
// Sets bit i of the value; does nothing beyond its current length
void BigInteger::st_setBit(BigInteger &n, int i)
{
    int bit = i % 32;
    i /= 32;

	
    if (i >= n.len)
		return;
	
	n.n[i] |= (1L << bit);				
}

// FUNCTION: WON 0x007d31f0
// Copies count words (memmove when source and target are the same)
void BigInteger::st_CopyBuffer(word *dest, const word *src, size_t count)
{
	if(dest!=src)
		memcpy(dest,src,sizeof(word)*count);
	else
		memmove(dest,src,sizeof(word)*count);
}

// FUNCTION: WON 0x007d32d0
// Destroys the value, freeing a heap word buffer
BigInteger::~BigInteger()
{
	if(this->n!=this->stackN)
		delete[] this->n;
}

// FUNCTION: WON 0x007d3610
// Sets the value to zero
void BigInteger::st_zero(BigInteger &a)
{
    a.n[0] = 0;
    a.negative = false;
    a.len = 0;
}

// FUNCTION: WON 0x007d3630
// Sets the value to one
void BigInteger::st_one(BigInteger &a)
{
    a.n[0] = 1;
    a.negative = false;
    a.len = 1;
}

// FUNCTION: WON 0x007d3dd0
// Compares the magnitudes of two values; returns -1, 0 or 1
int BigInteger::st_ucmp(const BigInteger &a, const BigInteger &b)
{
    return BigInteger::CompareWords(a.n,b.n,a.len,b.len);
/*	int alen = a.len;
    int blen = b.len;

    if (alen < blen) return -1;
    if (alen > blen) return 1;

    word *an = a.n;
    word *bn = b.n;

    for (int i = alen-1; i >= 0; --i)
    {
        if (an[i] < bn[i]) return -1;
        if (an[i] > bn[i]) return 1;
    }
    return 0;*/
}

// FUNCTION: WON 0x007d4fa0
// Sets r to the greatest common divisor of a and b
void BigInteger::st_gcd(BigInteger &r, const BigInteger &a, const BigInteger &b)
{
    if (BigInteger::st_cmp(a, b) > 0)
        BigInteger::st_euclid(r, a, b);
    else
        BigInteger::st_euclid(r, b, a);
}

// FUNCTION: WON 0x007d5760
// Creates a zero value
BigInteger::BigInteger()
{
	this->Init();
}

// FUNCTION: WON 0x007d5780
// Creates a copy of another value
BigInteger::BigInteger(const BigInteger &theVal)
{
	this->Init();
	BigInteger::st_copy(*this,theVal);
}

// FUNCTION: WON 0x007d57b0
// Creates a value from an int
BigInteger::BigInteger(int theFrom)
{
	this->Init();

	this->negative = theFrom<0;
	if(this->negative)
		this->n[0] = theFrom*-1;
	else
		this->n[0] = theFrom;

	this->len = 1;
	if(theFrom==0)
		this->len = 0;
}

// FUNCTION: WON 0x007d5800
// Creates a value from big-endian unsigned bytes
BigInteger::BigInteger(const RawBuffer &buffer)
{
	this->Init();
    this->fromBinary(buffer);
}

// FUNCTION: WON 0x007d5820
// Creates a random value of up to numBits bits
BigInteger::BigInteger(int numBits, Random &rndSrc)
{
	this->Init();
	this->fromBinary(BigInteger::st_randomBits(numBits,rndSrc));
}

// FUNCTION: WON 0x007d58c0
// Copies another value into this one
const BigInteger& BigInteger::operator =(const BigInteger& theVal)
{
	BigInteger::st_copy(*this,theVal);
	return *this;
}

// FUNCTION: WON 0x007d58e0
// Returns whether the given bit is set
bool BigInteger::testBit(int theBit)
{
	return BigInteger::st_bit(*this,theBit);
}

// FUNCTION: WON 0x007d5900
// Sets the given bit (within the current length) and returns this value
BigInteger& BigInteger::setBit(int theBit)
{
	BigInteger::st_setBit(*this,theBit);
	return *this;
}

// FUNCTION: WON 0x007d5920
// Returns the greatest common divisor with b
BigInteger BigInteger::gcd(const BigInteger &b) const
{
	BigInteger r;
	BigInteger::st_gcd(r,*this,b);

	return r;
}

// FUNCTION: WON 0x007d59b0
// Returns the number of significant bits
int BigInteger::bitLength() const
{ 
	return BigInteger::st_bitLength(*this); 
}

// FUNCTION: WON 0x007d59c0
// Returns this value raised to a power modulo m, using a precomputed reciprocal
BigInteger BigInteger::modExp(const BigInteger &power, const BigInteger &modulo, const BigInteger &recip, short nb) const
{
	BigInteger r;
	BigInteger::st_modExp(r, *this, power, modulo,recip,nb);

    return r;
}

// FUNCTION: WON 0x007d5a50
// Returns the multiplicative inverse modulo n
BigInteger BigInteger::inverseModN(const BigInteger &n) const
{
	BigInteger r;

	BigInteger::st_inverseModN(r,*this, n);

    return r;
}

// FUNCTION: WON 0x007d5ae0
// Returns this value modulo b
BigInteger BigInteger::mod(const BigInteger &b) const
{
	BigInteger r;

	BigInteger::st_mod(r, *this, b);
   
	return r;
}

// FUNCTION: WON 0x007d5b70
// Returns this value plus b
BigInteger BigInteger::add(const BigInteger &b) const
{
	BigInteger r;
	
	BigInteger::st_add(r, *this, b);
    return r;
}

// FUNCTION: WON 0x007d5c00
// Returns this value minus b
BigInteger BigInteger::sub(const BigInteger &b) const
{
	BigInteger r;
	BigInteger::st_sub(r, *this, b);

    return r;
}

// FUNCTION: WON 0x007d5c90
// Compares with another value; returns -1, 0 or 1
int BigInteger::cmp(const BigInteger &a) const
{
    return BigInteger::st_cmp(*this, a);
}

// FUNCTION: WON 0x007d5cb0
// Shifts this value right by n bits in place
BigInteger& BigInteger::shiftRight(int n)
{
	BigInteger::st_shiftRight(*this, *this, (short) n);
    return *this;
}

// FUNCTION: WON 0x007d5cd0
// Returns whether the values are equal
bool BigInteger::equals(const BigInteger &b) const
{
    return (this->cmp(b) == 0);
}

// FUNCTION: WON 0x007d5cf0
// Returns the reciprocal used for fast modular reduction and its bit count
BigInteger BigInteger::recip(int &nb) const
{
	BigInteger r;

	nb = BigInteger::st_recip(r,*this);
	return r;
}

// FUNCTION: WON 0x007d5d80
// Returns this value times y modulo m, using a precomputed reciprocal
BigInteger BigInteger::modMulRecip(const BigInteger &y, const BigInteger &m, const BigInteger &i, short nb) const
{
	BigInteger r;

	BigInteger::st_modMulRecip(r,*this,y,m,i,nb);
	
	return r;
}

}

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
namespace WONAPI {
using namespace std;
// No original address identified: WON SDK body
void BigInteger::fromString(const std::string& theString)
{

	BigInteger::st_zero(*this);

	int aPos = 0;
	while(aPos<theString.length() && isspace(theString[aPos]))
		aPos++;

	int lastPos = aPos;
	while(lastPos<theString.length() && isdigit(theString[lastPos]))
		lastPos++;

	string aStr = theString.substr(aPos,lastPos-aPos);


	BigInteger thousand(1000);
	BigInteger r1(0), r2(0);

	if(aStr.size()%3!=0)
	{
		string aSubStr = aStr.substr(0,aStr.size()%3);
		r1 = r2 = atoi(aSubStr.c_str());
		aPos+=aStr.size()%3;
	}
	
	while(aPos<aStr.length())
	{
		string aSubStr = theString.substr(aPos,3);

		
		BigInteger anInt = atoi(aSubStr.c_str());

		BigInteger::st_mul(r1,thousand,r2);
		BigInteger::st_add(r1,r1,anInt);

		r2 = r1;
		aPos+=3;
	}

	BigInteger::st_copy(*this,r1);
}

// No original address identified: WON SDK body
std::string BigInteger::toString() const
{
	string s;

	BigInteger N(1000);
	BigInteger r;
	BigInteger tmp = *this;

	char aBuf[20];

	while(!tmp.equals(BigInteger::ZERO))
	{
		tmp = tmp.divrem(N,r);
		sprintf(aBuf,"%03d",r.n[0]);
		s=aBuf+s;
	}


/*
    RawBuffer buffer = toByteArray();

	for(int i=buffer.length()-1; i>=0; i--)
	{
		int low = buffer[i] & 0x0f;
		int high = (buffer[i] & 0xf0) >> 4;
	
		s+=IntToHex(high);
		s+=IntToHex(low);
	}*/

    // remove leading 0's.
    int pos = 0, len = s.length();
    while ((pos < len) && (s[pos] == '0'))
        pos++;

    if (pos == len) 
	{
        return "0";
    }
	else 
	{	
        return s.substr(pos);
    }		
}

// No original address identified: WON SDK body
BigInteger::BigInteger(const std::string &theString)
{
	this->Init();
	this->fromString(theString);
}

// No original address identified: WON SDK body
BigInteger BigInteger::modExp(const BigInteger &power, const BigInteger &modulo) const
{
	BigInteger r;
	BigInteger::st_modExp(r, *this, power, modulo);

    return r;
}

// No original address identified: WON SDK body
BigInteger BigInteger::mul(const BigInteger &b) const
{
	BigInteger r;
	BigInteger::st_mul(r, *this, b);

    return r;
}

// No original address identified: WON SDK body
BigInteger BigInteger::div(const BigInteger &b) const
{
	BigInteger r;
	
	BigInteger::st_div(r, *this, b);

	return r;
}

// No original address identified: WON SDK body
BigInteger BigInteger::divrem(const BigInteger &a, BigInteger &rem) const
{
	BigInteger div;
	BigInteger::st_div(&div,&rem,*this,a);

	return div;
}

// No original address identified: WON SDK body
BigInteger BigInteger::negate() const
{
	BigInteger r(*this);

	r.negative = !(this->negative);
	return r;
}

// No original address identified: WON SDK body
BigInteger& BigInteger::shiftLeft(int n)
{
	BigInteger::st_shiftLeft(*this, *this, (short) n);
    return *this;
}

// No original address identified: WON SDK body
bool BigInteger::operator<(const BigInteger &a) const
{
	return cmp(a)<0;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007d32f0
// (public: virtual void * __thiscall WONAPI::BigInteger::`scalar deleting destructor'(unsigned int)    [??_GBigInteger@WONAPI@@UAEPAXI@Z])
