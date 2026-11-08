// BigInteger math: multiplication, division, modular exponentiation, gcd and modular inverse
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <algorithm>
#include <memory>
#include <string.h>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONBigInteger.h"
#include "ee/wonapi/WONArithmeticException.h"
namespace WONAPI {
// FUNCTION: WON 0x007d4120
// Sets r to a * b (schoolbook); throws when r aliases an operand
void BigInteger::st_mul(BigInteger &r, const BigInteger &a, const BigInteger &b)
{
    if (&r == &a || &r == &b)
		throw ArithmeticException("Result must not be either Parameter (a or b)");

    if (a.len == 0 || b.len == 0)
    {
        BigInteger::st_zero(r);
        return;
    }

    r.negative = a.negative ^ b.negative;


    BigInteger::st_grow(r, a.len + b.len,false);
    r.len = a.len + b.len;

	BigInteger::MultiplyWords(r.n,a.n,b.n,a.len,b.len);
	r.len = BigInteger::FindSize(r.n,r.len);
}

// FUNCTION: WON 0x007d4500
// Sets r to a * b with Karatsuba multiplication, using the workspace or a temporary one
void BigInteger::st_kmul(BigInteger &r, const BigInteger &a, const BigInteger &b, word *workspace)
{
	word *tn = workspace;

    if (&r == &a || &r == &b)
		throw ArithmeticException("Result must not be either Parameter (a or b)");

    if (a.len == 0 || b.len == 0)
    {
        BigInteger::st_mul(r,a,b);
        return;
    }

	if(a.len<b.len)
	{
		BigInteger::st_kmul(r,b,a,tn);
		return;
 	}

    r.negative = a.negative ^ b.negative;
	int aSize = a.len;

	if(tn==NULL)
		tn = new word[aSize*3];

	BigInteger::st_grow(a,aSize,true);
	BigInteger::st_grow(b,aSize,true);


	BigInteger::st_grow(r,aSize<<1,false);
	r.len = a.len+b.len;

	word *an = a.n;
	word *bn = b.n;

//	memset(an+a.len,0,(aSize-a.len)*sizeof(word));
//	memset(bn+b.len,0,(aSize-b.len)*sizeof(word));
//	memset(tn,0,aSize*3*sizeof(word));

//	memcpy(an,a.n,a.len*sizeof(word));
//	memcpy(bn,b.n,b.len*sizeof(word));


	BigInteger::KaratsubaMultiplyWords(r.n,tn,an,bn,a.len,a.len,b.len);
	r.len = BigInteger::FindSize(r.n,r.len);
		
	if(workspace==NULL)
		delete[] tn;
}

// FUNCTION: WON 0x007d4740
// Sets r to m modulo d by shift-and-subtract (adds d back for a negative m); throws when r is m
void BigInteger::st_mod(BigInteger &r, const BigInteger &m, const BigInteger &d)
{
    if (&r == &m)
		throw(ArithmeticException("Result must not be the same object as m"));

    BigInteger::st_copy(r, m);
    if (BigInteger::st_ucmp(m, d) < 0)
	{
		if(m.negative)
		{
			r.negative = true;
			BigInteger::st_add(r,d,r);
		}
        return;
	}

	r.negative = false;
	
    int i = BigInteger::st_bitLength(m) - BigInteger::st_bitLength(d);

    BigInteger ds;
    BigInteger::st_shiftLeft(ds, d, i);

    for (; i>= 0; --i)
    {
        if (BigInteger::st_cmp(r, ds) >= 0)
            BigInteger::st_sub(r, r, ds);
        BigInteger::st_shiftRight(ds, ds, (short)1);
    }
	
	if(m.negative)
	{
		r.negative = true;
		BigInteger::st_add(r,d,r);
	}
}

// FUNCTION: WON 0x007d4960
// Divides m by d into an optional quotient and remainder; throws on division by zero
void BigInteger::st_div(BigInteger *dvOrig, BigInteger *remOrig, const BigInteger &m, const BigInteger &d)
{
    if (d.len == 0)
		throw(ArithmeticException("div: Divide by zero"));

    if (BigInteger::st_cmp(m, d) < 0)
    {
        if (remOrig != NULL)
            BigInteger::st_copy(*remOrig, m);
        if (dvOrig != NULL)
            BigInteger::st_zero(*dvOrig);

        return;
    }

	BigInteger *dvPtr = dvOrig, *remPtr = remOrig;

    if (dvPtr == NULL)
        dvPtr = new BigInteger();
    if (remPtr == NULL)
        remPtr = new BigInteger();

	std::auto_ptr<BigInteger> aDelDv(dvPtr), aDelRem(remPtr);
	if(dvOrig!=NULL)
		aDelDv.release();
	if(remOrig!=NULL)
		aDelRem.release();


	BigInteger &dv = *dvPtr;
	BigInteger &rem = *remPtr;

    BigInteger ds;
    BigInteger::st_copy(rem, m);
    BigInteger::st_zero(dv);

    int i = BigInteger::st_bitLength(m) - BigInteger::st_bitLength(d);
    BigInteger::st_shiftLeft(ds, d, i);

    for (; i >= 0; --i)
    {
        if (dv.len == 0)
        {
            if (BigInteger::st_cmp(rem, ds) >= 0)
            {
                BigInteger::st_one(dv);
                BigInteger::st_sub(rem, rem, ds);
            }
        }
        else
        {
            BigInteger::st_shiftLeft(dv, dv, 1);
            if (BigInteger::st_cmp(rem, ds) >= 0)
            {
                dv.n[0] |= 1;
                BigInteger::st_sub(rem, rem, ds);
            }
        }
        BigInteger::st_shiftRight(ds, ds, 1);
    }
    dv.negative = m.negative ^ d.negative;
}

// FUNCTION: WON 0x007d4c90
// Sets r to a to the power modulo m by square-and-multiply with reciprocal reduction
void BigInteger::st_modExp(BigInteger &r, const BigInteger &a, const BigInteger &power, const BigInteger &modulo, const BigInteger &d, short nb)
{
    BigInteger v;

    BigInteger::st_mod(v, a, modulo);
    int bits = BigInteger::st_bitLength(power);

    if ((power.n[0] & 1) != 0)
        BigInteger::st_mod(r, a, modulo);
    else
        BigInteger::st_one(r);

	int aSize = modulo.len;
	word *workspace = new word[aSize*6];

/*
	BigInteger nprime, bsquared;

	int nbits = MontgomeryRepresentation(nprime,bsquared,modulo);
	BigInteger vMontgomery;
	BigInteger rMontgomery;
	BigInteger temp1, temp2;

	mul(temp1,v,bsquared);
	MontgomeryReduce(vMontgomery,temp2,temp1,modulo,nbits,nprime,workspace);

	mul(temp1,r,bsquared);
	MontgomeryReduce(rMontgomery,temp2,temp1,modulo,nbits,nprime,workspace);
*/

     for (int i = 1; i < bits; i++)
    {

        BigInteger::st_modMulRecip(v, v, v, modulo, d, nb,workspace);

//		kmul(temp1,vMontgomery,vMontgomery,workspace);
//		MontgomeryReduce(vMontgomery,temp2,temp1,modulo,nbits,nprime,workspace);

	
        if (BigInteger::st_bit(power, i))
		{
			BigInteger::st_modMulRecip(r, r, v, modulo, d, (short)nb,workspace);
			//kmul(temp1,rMontgomery,vMontgomery,workspace);
			//MontgomeryReduce(rMontgomery,temp2,temp1,modulo,nbits,nprime,workspace);

		}
    }

//	MontgomeryReduce(r,temp2,rMontgomery,modulo,nbits,nprime,workspace);
	delete[] workspace;

	if(power.negative)
		BigInteger::st_inverseModN(r,r,modulo);
}

// FUNCTION: WON 0x007d4dd0
// Sets r to the reciprocal 2^(2*bits)/m used for reduction; returns bits + 1
int BigInteger::st_recip(BigInteger &r, const BigInteger &m)
{
    BigInteger t;
    BigInteger::st_one(t);

    int mbits = BigInteger::st_bitLength(m);

    BigInteger::st_shiftLeft(t, t, 2*mbits);
    BigInteger::st_div(&r, NULL, t, m);
    return mbits+1;
}

// FUNCTION: WON 0x007d4e60
// Sets r to the greatest common divisor of x and y (binary gcd, x >= y)
void BigInteger::st_euclid(BigInteger &r, const BigInteger &x, const BigInteger &y)
{
    BigInteger aOrig;
    BigInteger bOrig;

    BigInteger::st_copy(aOrig, x);
    BigInteger::st_copy(bOrig, y);

	BigInteger *a = &aOrig;
	BigInteger *b = &bOrig;

    int shifts = 0;

    while (b->len != 0)
    {
        if ((a->n[0] & 1) != 0)    // a odd
            if ((b->n[0] & 1) != 0)    // b odd
            {
                BigInteger::st_sub(*a, *a, *b);
                BigInteger::st_shiftRight(*a, *a, 1);
                if (BigInteger::st_cmp(*a, *b) < 0)
                {
                    BigInteger *t = a;
                    a = b;
                    b = t;
                }
            }
            else
            {
                BigInteger::st_shiftRight(*b, *b, 1);
                if (BigInteger::st_cmp(*a, *b) < 0)
                {
                    BigInteger *t = a;
                    a = b;
                    b = t;
                }
            }
        else
            if ((b->n[0] & 1) != 0)    // b odd
            {
                BigInteger::st_shiftRight(*a, *a, 1);
                if (BigInteger::st_cmp(*a, *b) < 0)
                {
                    BigInteger *t = a;
                    a = b;
                    b = t;
                }
            }
            else
            {
                BigInteger::st_shiftRight(*a, *a, 1);
                BigInteger::st_shiftRight(*b, *b, 1);
                shifts++;
            }
    }
    if (shifts > 0)
        BigInteger::st_shiftLeft(r, *a, shifts);
    else
        BigInteger::st_copy(r, *a);
}

// FUNCTION: WON 0x007d4fe0
// Sets r to x * y modulo m with Barrett reduction; throws when more than three corrections are needed
void BigInteger::st_modMulRecip(BigInteger &r, const BigInteger &x, const BigInteger &y, const BigInteger &m, const BigInteger &i,short nb, word *t)
{
    BigInteger a;
    BigInteger b;
    BigInteger c;
    BigInteger d;

    BigInteger::st_kmul(a, x, y,t);			
	BigInteger::st_shiftRight(d, a, nb-1);		
    BigInteger::st_kmul(b, d, i,t);			
    BigInteger::st_shiftRight(c, b, nb-1);	
    BigInteger::st_kmul(b, m, c,t);		
    BigInteger::st_sub(r, a, b);		

    int j = 0;
    while (BigInteger::st_cmp(r, m) >= 0)
    {
        if (j++ > 2)
		{
			throw ArithmeticException("modMulRecip");
		}

        BigInteger::st_sub(r, r, m);
    }
}

// FUNCTION: WON 0x007d5270
// Sets r to the inverse of a modulo n (extended Euclid); throws on negative input
void BigInteger::st_inverseModN(BigInteger &r, const BigInteger &a, const BigInteger &n)
{
    if (a.negative || n.negative)
		throw ArithmeticException("inverseModN");

	BigInteger x1orig;
	BigInteger x2orig;
	BigInteger x3orig; BigInteger::st_copy(x3orig,a);

	BigInteger y1orig;
	BigInteger y2orig;
	BigInteger y3orig; BigInteger::st_copy(y3orig,n);

    BigInteger *x1 = &x1orig;
    BigInteger *x2 = &x2orig;
    BigInteger *x3 = &x3orig;
    BigInteger *y1 = &y1orig;
    BigInteger *y2 = &y2orig;
	BigInteger *y3 = &y3orig;

    BigInteger::st_one(*x1); BigInteger::st_one(*y2);
    BigInteger::st_zero(*x2); BigInteger::st_zero(*y1);

	BigInteger t1orig;
	BigInteger t2orig;
	BigInteger t3orig;
	BigInteger qorig; 
	BigInteger porig;

	BigInteger *t1 = &t1orig;
	BigInteger *t2 = &t2orig;
	BigInteger *t3 = &t3orig;
	BigInteger *q = &qorig;
	BigInteger *p = &porig;


    while (y3->len != 0)
    {
        BigInteger::st_div(q, t3, *x3, *y3);
        BigInteger::st_mul(*t1, *q, *y2);		
        BigInteger::st_sub(*t2, *x2, *t1);
        BigInteger::st_mul(*p, *q, *y1);
        BigInteger::st_sub(*t1, *x1, *p);

		BigInteger *tt1 = x1;
		BigInteger *tt2 = x2;
		BigInteger *tt3 = x3;

        x1 = y1; x2 = y2; x3 = y3;
        y1 = t1; y2 = t2; y3 = t3;
		t1 = tt1; t2 = tt2; t3 = tt3;
    }

    if (x1->negative)
        BigInteger::st_add(*x1, *x1, n);
	
    BigInteger::st_copy(r, *x1);
}

}

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
namespace WONAPI {
using namespace std;
// No original address identified: WON SDK body
void BigInteger::st_exp(BigInteger &r, const BigInteger &a, const BigInteger &power)
{
    BigInteger v1Orig;
	BigInteger v2Orig;

	BigInteger::st_copy(v1Orig,a);

	BigInteger *v1 = &v1Orig;
	BigInteger *v2 = &v2Orig;
	BigInteger *temp;
    
	int bits = BigInteger::st_bitLength(power);

    if ((power.n[0] & 1) != 0)
        BigInteger::st_copy(r,a);
    else
        BigInteger::st_one(r);


    for (int i = 1; i < bits; i++)
    {
        BigInteger::st_mul(*v2, *v1, *v1);
		temp = v1; v1 = v2; v2 = temp;

        if (BigInteger::st_bit(power, i))
		{
			BigInteger::st_copy(*v2,r);
			BigInteger::st_mul(r, *v1, *v2);
		}
    }
}

// No original address identified: WON SDK body
void BigInteger::st_modExp(BigInteger &r, const BigInteger &a, const BigInteger &power, const BigInteger &modulo)
{
    BigInteger d;
	int nb = BigInteger::st_recip(d, modulo);

	BigInteger::st_modExp(r,a,power,modulo,d,nb);
}

// No original address identified: WON SDK body
void BigInteger::st_div(BigInteger &dv, const BigInteger &m, const BigInteger &d)
{
	BigInteger::st_div(&dv, NULL, m, d);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007d4310
// (public: virtual __thiscall WONAPI::ArithmeticException::~ArithmeticException(void)    [??1ArithmeticException@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007d42f0
// (public: virtual void * __thiscall WONAPI::ArithmeticException::`scalar deleting destructor'(unsigned int)    [??_GArithmeticException@WONAPI@@UAEPAXI@)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007d4380
// (public: __thiscall WONAPI::ArithmeticException::ArithmeticException(class WONAPI::ArithmeticException const &)    [??0ArithmeticException@WONAPI@@QAE@)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007daf30
// (public: __thiscall std::auto_ptr<class WONAPI::BigInteger>::~auto_ptr<class WONAPI::BigInteger>(void))
