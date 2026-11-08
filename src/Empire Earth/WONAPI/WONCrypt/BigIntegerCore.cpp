// BigInteger core: signed add/subtract/compare, word shifts and unsigned add/subtract
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
namespace WONAPI {
const int BigInteger::BITS = 32;
// FUNCTION: WON 0x007d3c30
// Sets r to a + b, handling the signs
void BigInteger::st_add(BigInteger &r, const BigInteger &a, const BigInteger &b)
{
    if (a.len == 0)
    {
        BigInteger::st_copy(r, b);
        return;
    }
    if (b.len == 0)
    {
        BigInteger::st_copy(r, a);
        return;
    }

    if (a.negative)
    {
        if (b.negative)
        {
            BigInteger::st_add_unsigned(r, a, b);
            r.negative = true;
        }
        else
        {
            BigInteger::st_sub_unsigned(r, b, a);
        }
    }
    else
    {
        if (b.negative)
        {
            BigInteger::st_sub_unsigned(r, a, b);
        }
        else
        {
            BigInteger::st_add_unsigned(r, a, b);
        }
    }
}

// FUNCTION: WON 0x007d3cc0
// Sets r to a - b, handling the signs
void BigInteger::st_sub(BigInteger &r, const BigInteger &a, const BigInteger &b)
{
    if (a.len == 0)
    {
        BigInteger::st_copy(r, b);
        if (b.len > 0)
            r.negative = true ^ b.negative;
        return;
    }
    if (b.len == 0)
    {
        BigInteger::st_copy(r, a);
        return;
    }

    if (a.negative)
    {
        if (b.negative)
        {
            BigInteger::st_sub_unsigned(r, b, a);
        }
        else
        {
            BigInteger::st_add_unsigned(r, b, a);
            r.negative = true;
        }
    }
    else
    {
        if (b.negative)
        {
            BigInteger::st_add_unsigned(r, a, b);
        }
        else
        {
            BigInteger::st_sub_unsigned(r, a, b);
        }
    }
}

// FUNCTION: WON 0x007d3d80
// Compares two signed big integers; returns -1, 0 or 1
int BigInteger::st_cmp(const BigInteger &a, const BigInteger &b)
{
    if (a.len == 0 && b.len == 0) return 0;

    if (a.negative)
    {
        if (b.negative)
        {
            return BigInteger::st_ucmp(b, a);
        }
        else
        {
            return -1;
        }
    }
    else
    {
        if (b.negative)
        {
            return 1;
        }
        else
        {
            return BigInteger::st_ucmp(a, b);
        }
    }
}

// FUNCTION: WON 0x007d3e00
// Sets r to a shifted left by n bits
void BigInteger::st_shiftLeft(BigInteger &r, const BigInteger &a, int n)
{
    if (a.len == 0)
    {
        BigInteger::st_zero(r);
        return;
    }
    int rem = n % BigInteger::BITS;
    int blocks = n / BigInteger::BITS;
    int len = a.len;

    BigInteger::st_grow(r, len + blocks);
    r.len = len + blocks;

    word *rn = r.n;

	BigInteger::st_CopyBuffer(rn+blocks,a.n,len);

    if (blocks > 0)
        for (int i = blocks-1; i >= 0 ; --i) { rn[i] = 0; }

    if (rem == 0)
        return;
    word carry = 0;

    int rlen = r.len;
    for (int i = blocks; i < rlen; ++i)
    {
        word l = rn[i];

        rn[i] = ((l << rem) | carry);
        carry = (unsigned int)l >> (BigInteger::BITS-rem);
    }
    if (carry == 0)
        return;
    rlen += 1;
    BigInteger::st_grow(r, rlen);
    r.n[rlen-1] = carry;
    r.len = rlen;
}

// FUNCTION: WON 0x007d3f10
// Sets r to a shifted right by n bits (zero when all words shift out)
void BigInteger::st_shiftRight(BigInteger &r, const BigInteger &a, int n)
{
    int rem = n % BigInteger::BITS;
    int blocks = n / BigInteger::BITS;

    if (blocks >= a.len)
    {
        BigInteger::st_zero(r);
        return;
    }

    BigInteger::st_grow(r, a.len - blocks);
    r.len = a.len - blocks;

	BigInteger::st_CopyBuffer(r.n,a.n+blocks,r.len);

    if (rem == 0)
        return;
    word carry = 0;
    word *rn = r.n;

    int rlen = r.len;
    for (int i = rlen-1; i > 0; --i)
    {
        word l = rn[i];

        rn[i] = ((unsigned int)l >> rem) | carry;
        carry = (l << (BigInteger::BITS-rem));
    }
    int l = rn[0];

    rn[0] = ((unsigned int)l >> rem) | carry;
    if (rlen > 0 && rn[rlen-1] == 0)
        r.len--;
}

// FUNCTION: WON 0x007d4000
// Sets r to |a| + |b| (non-negative)
void BigInteger::st_add_unsigned(BigInteger &r, const BigInteger &a, const BigInteger &b)
{
    // Ensure a is the longest
    if (a.len < b.len)
    {
		BigInteger::st_add_unsigned(r,b,a);
		return;
    }

    // Needed in case the result is same object as r
    int alen = a.len;
    int blen = b.len;

    BigInteger::st_grow(r, alen + 1);
   
	r.negative = false;

	r.len = BigInteger::AddWords(r.n,a.n,b.n,alen,blen);
}

// FUNCTION: WON 0x007d4070
// Sets r to the difference of the magnitudes, setting the sign when asked
void BigInteger::st_sub_unsigned(BigInteger &r, const BigInteger &a, const BigInteger &b, bool determine_sign)
{
    switch (BigInteger::st_ucmp(a, b))
    {
    case 0:
        BigInteger::st_zero(r);
        return;
    case -1:
		if(determine_sign)
			r.negative = true;

		BigInteger::st_sub_unsigned(r,b,a,false);
		return;
    case 1:
        if(determine_sign)
			r.negative = false;
    }

    // Now a is the largest

    BigInteger::st_grow(r, a.len);

    word borrow = BigInteger::SubWords(r.n,a.n,b.n,a.len,b.len);
	r.len = BigInteger::FindSize(r.n,a.len);
}

}
