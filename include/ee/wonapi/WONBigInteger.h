// BigInteger (0x20 bytes): storage and lifetime declarations only
#ifndef EE_WONAPI_WONBIGINTEGER_H
#define EE_WONAPI_WONBIGINTEGER_H
#include "ee/wonapi/WONBlowfish.h"
namespace WONAPI {
class Random; //SDK random-bits constructor parameter, no field
class BigInteger {
public:
    typedef unsigned long word;
    typedef unsigned __int64 dword;
protected:
    mutable word stackN[4]; //04..14, inline digit array
    mutable word* n; //14, owned iff distinct from stackN
    mutable unsigned short nlen; //18 allocation capacity
    mutable unsigned short len; //1a significant length
    bool negative; //1c; natural1d..20 alignment
    void NewBuffer(int,const word* = NULL,int = 0) const; //3d3230
    void Init(); //3d32b0
    static const int BITS; //SDK constant 32, no object layout effect
public:
    void fromBinary(const RawBuffer&); //3d3530, ret 4, borrowed const byte string
    int toBinary(RawBuffer&) const; //3d3380, ret 4
    static int st_bitLength(const BigInteger&); //3d2ff0
    static RawBuffer st_randomBits(int,Random&); //3d3020, random-byte mask algorithm
    static void st_setBit(BigInteger&,int); //3d31b0, bit setter
    static void st_gcd(BigInteger&,const BigInteger&,const BigInteger&); //3d4fa0, ordered Euclid dispatch
    static int AddWords(word*,const word*,const word*,int,int); //3d3650 cdecl
    static word SubWords(word*,const word*,const word*,int,int); //3d3710 cdecl
    static void MultiplyWords(word*,const word*,const word*,int,int); //3d37c0 cdecl
    static void MultiplyWord(word*,const word*,word,int); //3d38c0 cdecl
    static int CompareWords(const word*,const word*,int,int); //3d3920 cdecl
    static void KaratsubaMultiplyWords(word*,word*,const word*,const word*,int,int,int); //3d3990 cdecl
    static int FindSize(const word*,int); //3d3c10 cdecl
    static void st_copy(BigInteger&,const BigInteger&); //3d3310
    static void st_grow(const BigInteger&,int,bool copyOld=true); //3d3340
    static bool st_bit(const BigInteger&,int); //3d3160 original external bit test
    static void st_mul(BigInteger&,const BigInteger&,const BigInteger&); //3d4120
    static void st_kmul(BigInteger&,const BigInteger&,const BigInteger&,word* = NULL); //3d4500
    static void st_mod(BigInteger&,const BigInteger&,const BigInteger&); //3d4740
    static void st_div(BigInteger*,BigInteger*,const BigInteger&,const BigInteger&); //3d4960
    static void st_modExp(BigInteger&,const BigInteger&,const BigInteger&,const BigInteger&,const BigInteger&,short); //3d4c90, 6 args
    static int st_recip(BigInteger&,const BigInteger&); //3d4dd0
    static void st_exp(BigInteger&,const BigInteger&,const BigInteger&);
    static void st_modExp(BigInteger&,const BigInteger&,const BigInteger&,const BigInteger&);
    static void st_div(BigInteger&,const BigInteger&,const BigInteger&);
    static void st_euclid(BigInteger&,const BigInteger&,const BigInteger&); //3d4e60
    static void st_modMulRecip(BigInteger&,const BigInteger&,const BigInteger&,const BigInteger&,const BigInteger&,short,word* = NULL); //3d4fe0
    static void st_inverseModN(BigInteger&,const BigInteger&,const BigInteger&); //3d5270
    static void st_CopyBuffer(word*,const word*,size_t); //3d31f0, external memcpy/memmove helper
    static void st_zero(BigInteger&); //3d3610, external field initializer
    static void st_one(BigInteger&); //3d3630, external field initializer
    static void st_add(BigInteger&,const BigInteger&,const BigInteger&); //3d3c30
    static void st_sub(BigInteger&,const BigInteger&,const BigInteger&); //3d3cc0
    static int st_cmp(const BigInteger&,const BigInteger&); //3d3d80
    static int st_ucmp(const BigInteger&,const BigInteger&); //3d3dd0, external compare forwarder
    static void st_shiftLeft(BigInteger&,const BigInteger&,int); //3d3e00
    static void st_shiftRight(BigInteger&,const BigInteger&,int); //3d3f10
    static void st_add_unsigned(BigInteger&,const BigInteger&,const BigInteger&); //3d4000
    static void st_sub_unsigned(BigInteger&,const BigInteger&,const BigInteger&,bool determine_sign=true); //3d4070
    int bitLength() const; //3d59b0, int in eax, ret 0
    int byteLength() const; //3d2fd0, SDK external byte-length helper
    bool testBit(int); //3d58e0, external bit helper
    BigInteger& setBit(int); //3d5900, external bit setter
    BigInteger& shiftRight(int); //3d5cb0, external in-place shift
    BigInteger gcd(const BigInteger&) const; //3d5920, hidden result + arg, ret 8
    BigInteger sub(const BigInteger&) const; //3d5c00, hidden result + arg, ret 8
    BigInteger add(const BigInteger&) const; //3d5b70, hidden result + arg, ret 8
    BigInteger mod(const BigInteger&) const; //3d5ae0, hidden result + arg, ret 8
    BigInteger inverseModN(const BigInteger&) const; //3d5a50, hidden result + arg, ret 8
    BigInteger modExp(const BigInteger&,const BigInteger&,const BigInteger&,short) const; //3d59c0, ret 0x14
    BigInteger modMulRecip(const BigInteger&,const BigInteger&,const BigInteger&,short) const; //3d5d80, ret 0x14
    bool equals(const BigInteger&) const; //3d5cd0, ret 4
    int cmp(const BigInteger&) const; //3d5c90, ret 4
    static const BigInteger ZERO; //defined in the original only: 566bc8, ctor 3d56d0(0)
    static const BigInteger ONE; //defined in the original only: 566ba8, ctor 3d5720(1)
    BigInteger recip(int&) const; //3d5cf0, hidden 0x20-byte result + int output ref, ret 8
    virtual void debugInitStr() {} //slot 0, 326b60 (empty)
    virtual void initStr() {} //slot 4, 326b60 (empty)
    virtual ~BigInteger(); //3d32d0, free(n) iff n!=stackN; deleting thunk 3d32f0, slot 8
    BigInteger(); //3d5760, Init3d32b0/NewBuffer3d3230 establish pointer and lengths
    BigInteger(int); //3d57b0, signed 32-bit initializer
    BigInteger(const RawBuffer&); //3d5800, ret 4
    BigInteger(int,Random&); //3d5820, random-bits constructor, ret 8
    BigInteger(const BigInteger&); //3d5780, initializes then st_copy 3d3310 (deep copy)
    const BigInteger& operator=(const BigInteger&); //3d58c0, st_copy3d3310 then returns this; no shallow pointer copy
    BigInteger(const std::string&);
    void fromString(const std::string&);
    std::string toString() const;
    BigInteger modExp(const BigInteger&,const BigInteger&) const;
    BigInteger mul(const BigInteger&) const;
    BigInteger div(const BigInteger&) const;
    BigInteger divrem(const BigInteger&,BigInteger&) const;
    BigInteger negate() const;
    BigInteger& shiftLeft(int);
    bool operator<(const BigInteger&) const;

};
}
#endif
