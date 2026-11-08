// MD5Digest: MD5 message digest (WONCrypt version, used by signatures and the random generator)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <string.h>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONMD5Digest.h"
namespace WONAPI {
// FUNCTION: WON 0x007d9350
// Creates a digest in its initial state
MD5Digest::MD5Digest()
{
	this->reset();
}

// FUNCTION: WON 0x007d93a0
// Resets the bit count and the four state words to the MD5 initial values
void MD5Digest::reset() 
{
	this->count = 0;
    this->mDigest[0] = 0x67452301;
    this->mDigest[1] = 0xEFCDAB89;
    this->mDigest[2] = 0x98BADCFE;
    this->mDigest[3] = 0x10325476;
}

// FUNCTION: WON 0x007d93d0
// Finishes the hash over the data so far, returns the 16-byte digest and resets
RawBuffer MD5Digest::digest()
{
	RawBuffer in = this->out;
	this->out.erase();

	return this->digest(in,in.length());
}

// FUNCTION: WON 0x007d94e0
// Adds the data, then finishes and returns the 16-byte digest
RawBuffer MD5Digest::digest(const RawBuffer& in)
{
	this->update(in);
	return this->digest();	
}

// FUNCTION: WON 0x007d9510
// Pads the final pos bytes with the bit count, hashes them and returns the digest; resets
RawBuffer MD5Digest::digest(const RawBuffer& in, int pos)
{
    if (pos != 0) 
		memcpy(this->tmp,in.data(),pos);

    this->tmp[pos++] = 0x80;

    if (pos > DATA_LENGTH - 8)
    {
        while (pos < DATA_LENGTH)
            this->tmp[pos++] = 0;

        MD5Digest::byte2int(this->tmp, 0, this->data, 0, DATA_LENGTH/4);
        this->transform(this->data);
        pos = 0;
    }

    while (pos < DATA_LENGTH - 8)
        this->tmp[pos++] = 0;

    MD5Digest::byte2int(this->tmp, 0, this->data, 0, (DATA_LENGTH/4)-2);

    this->data[14] = (int) this->count;
    this->data[15] = (int) (this->count>>32);

    this->transform(this->data);

    RawBuffer buf(HASH_LENGTH,0);

    // Little endian
    int off = 0;
    for (int i = 0; i < HASH_LENGTH/4; ++i) {
        int d = this->mDigest[i];
        buf[off++] = (unsigned char) d;
        buf[off++] = (unsigned char) (d>>8);
        buf[off++] = (unsigned char) (d>>16);
        buf[off++] = (unsigned char) (d>>24);
    }
	
	this->reset();
    return buf;
}

// FUNCTION: WON 0x007d9810
// Adds a buffer to the hash
void MD5Digest::update(const RawBuffer& input)
{
	this->update(input.data(),input.length());
}

// FUNCTION: WON 0x007d9830
// Adds bytes to the hash, transforming each complete 64-byte block and keeping the rest
void MD5Digest::update(const void *theData, unsigned long theLen)
{
	__int64 tmp = count & 0xFFFFFFFFL;
	int offset = 0;
	const unsigned char *data = static_cast<const unsigned char*>(theData);
	int len = theLen;
	
	count+=len*8;

	int num = (int)(tmp >> 3) & (DATA_LENGTH-1);

	if (num != 0)
	{
		if ((num+len) >= DATA_LENGTH)
		{
			out.append(data + offset,DATA_LENGTH - num);
			transform();
			offset += (DATA_LENGTH-num);
			len-=(DATA_LENGTH-num);
			num=0;
			// drop through and do the rest
		}
		else
		{
			out.append(data + offset,len);
			return;
		}
	}

	// we now can process the input data in blocks of blockSize
	// chars and save the leftovers to this->data.
	if (len >= DATA_LENGTH)
	{
		do
		{
			out.append(data+offset,DATA_LENGTH);
			transform();
			offset+=DATA_LENGTH;
			len-=DATA_LENGTH;
		} while (len >= DATA_LENGTH);
	}

	out.append(data+offset,len);
}	

// FUNCTION: WON 0x007d9a80
// Returns one MD5 step with the F function (steps 1-16)
int MD5Digest::FF(int a,int b,int c,int d,int k,int s,int t)
{
    a += k+t+MD5Digest::F(b,c,d);
    a = (a << s | (unsigned int)a >> -s);
    return a+b;
}

// FUNCTION: WON 0x007d9ac0
// Returns one MD5 step with the G function (steps 17-32)
int MD5Digest::GG(int a,int b,int c,int d,int k,int s,int t)
{
    a += k+t+MD5Digest::G(b,c,d);
    a = (a << s | (unsigned int)a >> -s);
    return a+b;
}

// FUNCTION: WON 0x007d9b00
// Returns one MD5 step with the H function (steps 33-48)
int MD5Digest::HH(int a,int b,int c,int d,int k,int s,int t)
{
    a += k+t+MD5Digest::H(b,c,d);
    a = (a << s | (unsigned int)a >> -s);
    return a+b;
}

// FUNCTION: WON 0x007d9b40
// Returns one MD5 step with the I function (steps 49-64)
int MD5Digest::II(int a,int b,int c,int d,int k,int s,int t)
{
    a += k+t+MD5Digest::I(b,c,d);
    a = (a << s | (unsigned int)a >> -s);
    return a+b;
}

// FUNCTION: WON 0x007d9b80
// Transforms the buffered 64-byte block
void MD5Digest::transform()
{
	this->ReadData();
	this->transform(this->data);
}

// FUNCTION: WON 0x007d9ba0
// Applies the 64 MD5 steps of one block to the state
void MD5Digest::transform (int M[])
{
    int a,b,c,d;

    a = this->mDigest[0];
    b = this->mDigest[1];
    c = this->mDigest[2];
    d = this->mDigest[3];

    a = MD5Digest::FF(a,b,c,d,M[ 0], 7,0xd76aa478);
    d = MD5Digest::FF(d,a,b,c,M[ 1],12,0xe8c7b756);
    c = MD5Digest::FF(c,d,a,b,M[ 2],17,0x242070db);
    b = MD5Digest::FF(b,c,d,a,M[ 3],22,0xc1bdceee);
    a = MD5Digest::FF(a,b,c,d,M[ 4], 7,0xf57c0faf);
    d = MD5Digest::FF(d,a,b,c,M[ 5],12,0x4787c62a);
    c = MD5Digest::FF(c,d,a,b,M[ 6],17,0xa8304613);
    b = MD5Digest::FF(b,c,d,a,M[ 7],22,0xfd469501);
    a = MD5Digest::FF(a,b,c,d,M[ 8], 7,0x698098d8);
    d = MD5Digest::FF(d,a,b,c,M[ 9],12,0x8b44f7af);
    c = MD5Digest::FF(c,d,a,b,M[10],17,0xffff5bb1);
    b = MD5Digest::FF(b,c,d,a,M[11],22,0x895cd7be);
    a = MD5Digest::FF(a,b,c,d,M[12], 7,0x6b901122);
    d = MD5Digest::FF(d,a,b,c,M[13],12,0xfd987193);
    c = MD5Digest::FF(c,d,a,b,M[14],17,0xa679438e);
    b = MD5Digest::FF(b,c,d,a,M[15],22,0x49b40821);

    a = MD5Digest::GG(a,b,c,d,M[ 1], 5,0xf61e2562);
    d = MD5Digest::GG(d,a,b,c,M[ 6], 9,0xc040b340);
    c = MD5Digest::GG(c,d,a,b,M[11],14,0x265e5a51);
    b = MD5Digest::GG(b,c,d,a,M[ 0],20,0xe9b6c7aa);
    a = MD5Digest::GG(a,b,c,d,M[ 5], 5,0xd62f105d);
    d = MD5Digest::GG(d,a,b,c,M[10], 9,0x02441453);
    c = MD5Digest::GG(c,d,a,b,M[15],14,0xd8a1e681);
    b = MD5Digest::GG(b,c,d,a,M[ 4],20,0xe7d3fbc8);
    a = MD5Digest::GG(a,b,c,d,M[ 9], 5,0x21e1cde6);
    d = MD5Digest::GG(d,a,b,c,M[14], 9,0xc33707d6);
    c = MD5Digest::GG(c,d,a,b,M[ 3],14,0xf4d50d87);
    b = MD5Digest::GG(b,c,d,a,M[ 8],20,0x455a14ed);
    a = MD5Digest::GG(a,b,c,d,M[13], 5,0xa9e3e905);
    d = MD5Digest::GG(d,a,b,c,M[ 2], 9,0xfcefa3f8);
    c = MD5Digest::GG(c,d,a,b,M[ 7],14,0x676f02d9);
    b = MD5Digest::GG(b,c,d,a,M[12],20,0x8d2a4c8a);

    a = MD5Digest::HH(a,b,c,d,M[ 5], 4,0xfffa3942);
    d = MD5Digest::HH(d,a,b,c,M[ 8],11,0x8771f681);
    c = MD5Digest::HH(c,d,a,b,M[11],16,0x6d9d6122);
    b = MD5Digest::HH(b,c,d,a,M[14],23,0xfde5380c);
    a = MD5Digest::HH(a,b,c,d,M[ 1], 4,0xa4beea44);
    d = MD5Digest::HH(d,a,b,c,M[ 4],11,0x4bdecfa9);
    c = MD5Digest::HH(c,d,a,b,M[ 7],16,0xf6bb4b60);
    b = MD5Digest::HH(b,c,d,a,M[10],23,0xbebfbc70);
    a = MD5Digest::HH(a,b,c,d,M[13], 4,0x289b7ec6);
    d = MD5Digest::HH(d,a,b,c,M[ 0],11,0xeaa127fa);
    c = MD5Digest::HH(c,d,a,b,M[ 3],16,0xd4ef3085);
    b = MD5Digest::HH(b,c,d,a,M[ 6],23,0x04881d05);
    a = MD5Digest::HH(a,b,c,d,M[ 9], 4,0xd9d4d039);
    d = MD5Digest::HH(d,a,b,c,M[12],11,0xe6db99e5);
    c = MD5Digest::HH(c,d,a,b,M[15],16,0x1fa27cf8);
    b = MD5Digest::HH(b,c,d,a,M[ 2],23,0xc4ac5665);

    a = MD5Digest::II(a,b,c,d,M[ 0], 6,0xf4292244);
    d = MD5Digest::II(d,a,b,c,M[ 7],10,0x432aff97);
    c = MD5Digest::II(c,d,a,b,M[14],15,0xab9423a7);
    b = MD5Digest::II(b,c,d,a,M[ 5],21,0xfc93a039);
    a = MD5Digest::II(a,b,c,d,M[12], 6,0x655b59c3);
    d = MD5Digest::II(d,a,b,c,M[ 3],10,0x8f0ccc92);
    c = MD5Digest::II(c,d,a,b,M[10],15,0xffeff47d);
    b = MD5Digest::II(b,c,d,a,M[ 1],21,0x85845dd1);
    a = MD5Digest::II(a,b,c,d,M[ 8], 6,0x6fa87e4f);
    d = MD5Digest::II(d,a,b,c,M[15],10,0xfe2ce6e0);
    c = MD5Digest::II(c,d,a,b,M[ 6],15,0xa3014314);
    b = MD5Digest::II(b,c,d,a,M[13],21,0x4e0811a1);
    a = MD5Digest::II(a,b,c,d,M[ 4], 6,0xf7537e82);
    d = MD5Digest::II(d,a,b,c,M[11],10,0xbd3af235);
    c = MD5Digest::II(c,d,a,b,M[ 2],15,0x2ad7d2bb);
    b = MD5Digest::II(b,c,d,a,M[ 9],21,0xeb86d391);

    this->mDigest[0] += a;
    this->mDigest[1] += b;
    this->mDigest[2] += c;
    this->mDigest[3] += d;
}

// FUNCTION: WON 0x007da370
// Moves the buffered 64 bytes into the block words and empties the buffer
void MD5Digest::ReadData()
{

	MD5Digest::byte2int(this->out.data(),0,this->data,0,DATA_LENGTH/4);

	this->out.erase();
}

// FUNCTION: WON 0x007da4b0
// Packs bytes into little-endian 32-bit words
void MD5Digest::byte2int(const unsigned char src[], int srcOffset,int dst[], int dstOffset, int length)
{
    while (length-- > 0)
    {
        // Little endian
        dst[dstOffset] =  (src[srcOffset++] & 0xFF);
        dst[dstOffset] |= ((int)(src[srcOffset++] & 0xFF) <<  8);
		dst[dstOffset] |= ((int)(src[srcOffset++] & 0xFF) << 16);
		dst[dstOffset] |= ((int)(src[srcOffset++] & 0xFF) << 24);
		dstOffset++;
    }
}


}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007dac40
// (public: class std::basic_string<unsigned char,struct std::char_traits<unsigned char>,class std::allocator<unsigned char> > & __thiscall std::basic_str)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007db030
// (public: static unsigned char * __cdecl std::char_traits<unsigned char>::assign(unsigned char *,unsigned int,unsigned char)    [?assign@?$char_traits@E)
