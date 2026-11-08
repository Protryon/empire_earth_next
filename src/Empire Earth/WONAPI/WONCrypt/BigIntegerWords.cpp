// BigInteger word arrays: unsigned add, subtract, multiply, compare and Karatsuba multiplication
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
#define LOW_WORD(x) (word)(x)
#define HIGH_WORD(x) ((word)(x>>32))
namespace WONAPI {
// FUNCTION: WON 0x007d3650
// Adds two word arrays into r; returns the result length including a final carry word
int BigInteger::AddWords(word *r, const word *a, const word *b, int NA, int NB)
{
	if(NA<NB) 
	{
		return BigInteger::AddWords(r,b,a,NB,NA);
	}

	word carry = 0;
	int i;

	for(i=0; i<NB; i++)
	{
		dword result = (dword)a[i] + b[i] + carry;
		r[i] = LOW_WORD(result);
		carry = HIGH_WORD(result);

//		NumAdds+=2;
	}

	for(; i<NA; i++)
	{
		dword result = (dword)a[i] + carry;
		r[i] = LOW_WORD(result);
		carry = HIGH_WORD(result);

//		NumAdds++;
	}

	if(carry)
	{
		r[i] = carry;
		return i+1;
	}

	return i;
}

// FUNCTION: WON 0x007d3710
// Subtracts the shorter word array from the longer into r; returns the final borrow
BigInteger::word BigInteger::SubWords(word *r, const word *a, const word *b, int NA, int NB)
{
	if(NA<NB) return BigInteger::SubWords(r,b,a,NB,NA);

	word borrow = 0;
	int i;
	for(i=0; i<NB; i++)
	{
		dword result = (dword)a[i] - b[i] - borrow;
		r[i] = LOW_WORD(result);
		borrow = 0 - HIGH_WORD(result);

//		NumAdds+=2;
	}

	for(; i<NA; i++)
	{
		dword result = (dword)a[i] - borrow;
		r[i] = LOW_WORD(result);
		borrow = 0 - HIGH_WORD(result);

//		NumAdds++;
	}

	return borrow;
}

// FUNCTION: WON 0x007d37c0
// Multiplies two word arrays into r (NA + NB words, schoolbook)
void BigInteger::MultiplyWords(word *r, const word *a, const word *b, int NA, int NB)
{
	memset(r,0,(NA+NB)*sizeof(word));
    for (int i = 0; i < NA; ++i)
    {
        word carry = 0;
        dword m1 = a[i];
        int ri = i;

        for (int j = 0; j < NB; ++j)
        {
            dword m2 = r[ri];
            m2 += b[j] * m1 + carry;
            carry = HIGH_WORD(m2);
            r[ri++] = LOW_WORD(m2);

//			NumMults++;
//			NumAdds++;
        }
        r[ri] = carry;
    }
}

// FUNCTION: WON 0x007d38c0
// Multiplies a word array by one word into r (NA + 1 words)
void BigInteger::MultiplyWord(word *r, const word *a, const word b, int NA)
{
	word carry = 0;
	dword result;
	int i;

	for(i=0; i < NA; ++i)
	{
		result = (dword)b*a[i] + carry;
		r[i] = LOW_WORD(result);
		carry = HIGH_WORD(result);

//		NumMults++;
//		NumAdds++;
	}

	r[i] = carry;
}

// FUNCTION: WON 0x007d3920
// Compares two word arrays by length, then from the top word; returns -1, 0 or 1
int BigInteger::CompareWords(const word *a, const word *b, int NA, int NB)
{
	if(NA!=NB)
		return NA>NB ? 1 : -1;

	for(int i=NA-1; i>=0; i--)
	{
		if(a[i]!=b[i])
			return a[i]>b[i] ? 1 : -1;
	}

	return 0;
}

// FUNCTION: WON 0x007d3990
// Multiplies two N-word arrays into r with Karatsuba recursion (schoolbook up to 16 words)
void BigInteger::KaratsubaMultiplyWords(word *r, word *workspace, const word *a, const word *b, int N, int /*aLen*/, int bLen)
{

	if(N<=16)
	{
		BigInteger::MultiplyWords(r,a,b,N,N);
		return;
	}

	int N2 = N>>1;
	int TwoN = N<<1;
	int TwoN2 = N2<<1;
	
	const word *a0 = a;
	const word *b0 = b;
	const word *a1 = a + N2;
	const word *b1 = b + N2;

	word *c0 = r;
	word *c1 = workspace;
	word *c2 = r+2*N2;
	word *t = workspace+N;

	if(bLen<=N2)
	{
		memset(r,0,sizeof(word)*TwoN);
		BigInteger::KaratsubaMultiplyWords(r+N2,t,a1,b0,N2,N2,bLen);
		BigInteger::KaratsubaMultiplyWords(c1,t,a0,b0,N2,N2,bLen);
		BigInteger::AddWords(r,r,c1,N+N2,N);
	}
	else
	{
		bool needSubtract = false;
		if(BigInteger::CompareWords(a1,a0,N2,N2)>0)
		{
			BigInteger::SubWords(c0,a1,a0,N2,N2);
		}
		else
		{
			BigInteger::SubWords(c0,a0,a1,N2,N2);
			needSubtract = !needSubtract;
		}

		if(BigInteger::CompareWords(b1,b0,N2,N2)>0)
		{
			BigInteger::SubWords(c2,b1,b0,N2,N2);
			needSubtract = !needSubtract;
		}
		else
		{
			BigInteger::SubWords(c2,b0,b1,N2,N2);
		}

		BigInteger::KaratsubaMultiplyWords(c1,t,c0,c2,N2,N2,N2);
		BigInteger::KaratsubaMultiplyWords(c0,t,a0,b0,N2,N2,N2);		
		BigInteger::KaratsubaMultiplyWords(c2,t,a1,b1,N2,N2,N2);

		int tSize = BigInteger::AddWords(t,c0,c2,TwoN2,TwoN2);

		if(needSubtract)
		{
			BigInteger::SubWords(t,t,c1,tSize,TwoN2);
		}
		else
		{
			tSize = BigInteger::AddWords(t,t,c1,tSize,TwoN2);
		}

		BigInteger::AddWords(r+N2,r+N2,t,N+N2,tSize);
	}

	if(!(N&1))
		return;
	if(b[N-1]!=0)
	{
		BigInteger::MultiplyWord(c1,a,b[N-1],N-1);
		BigInteger::AddWords(r+N-1,r+N-1,c1,N,N);
	}

	BigInteger::MultiplyWord(t,b,a[N-1],N-1);
	BigInteger::AddWords(r+N-1,r+N-1,t,N,N);

	dword result = a[N-1]*b[N-1] + r[TwoN-2];
	r[TwoN-2] = LOW_WORD(result);
	r[TwoN-1] = HIGH_WORD(result);
}

// FUNCTION: WON 0x007d3c10
// Returns the word count without leading zero words
int BigInteger::FindSize(const word *r, int size)
{
	while(size>0 && r[size-1]==0)
		size--;

	return size;
}

}
#undef LOW_WORD
#undef HIGH_WORD
