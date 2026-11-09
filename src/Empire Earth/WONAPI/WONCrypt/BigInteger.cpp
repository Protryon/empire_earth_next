// BigInteger: storage, growth and big-endian byte conversion (arithmetic lives in the other BigInteger files)
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
// FUNCTION: WON 0x007d2ff0
// Returns the number of significant bits in the value
int BigInteger::st_bitLength(const BigInteger& value)
{
    int length=value.len;
    if(length==0) return 0;
    int result=(length-1)*32;
    unsigned int high=value.n[length-1];
    while(high!=0) {high>>=1;++result;}
    return result;
}
// FUNCTION: WON 0x007d3230
// Replaces the word buffer (inline for up to 4 words), copying old words when given and freeing the old heap buffer
void BigInteger::NewBuffer(int size,const word* oldBuffer,int oldSize) const
{
    word* oldN=this->n;
    if(size>4) this->n=new word[size];
    else this->n=this->stackN;
    if(oldBuffer!=this->n) memset(this->n,0,size*sizeof(word));
    this->nlen=size;
    this->len=oldSize;
    if(oldBuffer!=NULL && oldBuffer!=this->n) memcpy(this->n,oldBuffer,sizeof(word)*oldSize);
    if(oldN!=this->stackN) delete[] oldN;
}
// FUNCTION: WON 0x007d32b0
// Initialises the value to zero with the inline buffer
void BigInteger::Init()
{
    this->n=NULL;
    this->NewBuffer(4);
    this->negative=false;
}
// FUNCTION: WON 0x007d3310
// Copies the value and sign of one big integer into another (no-op on self-copy)
void BigInteger::st_copy(BigInteger& target,const BigInteger& source)
{
    if(&target==&source) return;
    target.NewBuffer(source.nlen,source.n,source.len);
    target.negative=source.negative;
}
// FUNCTION: WON 0x007d3340
// Enlarges the buffer to hold at least the given words (plus 16 spare), optionally keeping the value
void BigInteger::st_grow(const BigInteger& value,int size,bool copyOld)
{
    if(size<=value.nlen) return;
    size+=16;
    if(copyOld) value.NewBuffer(size,value.n,value.len);
    else value.NewBuffer(size);
}
// FUNCTION: WON 0x007d3380
// Writes the magnitude as big-endian bytes (with a 0 sign byte when needed); returns the byte count
int BigInteger::toBinary(RawBuffer& buffer) const
{
    int byteCount=(BigInteger::st_bitLength(*this)+7)/8;
    buffer.erase();
    buffer.reserve(byteCount+1);
    int pos=0,bitPos=0;
    if(byteCount==0) buffer+=(unsigned char)0;
    for(int i=0;i<byteCount;++i) {
        int value=((unsigned int)this->n[pos]>>bitPos)&0xffL;
        bitPos+=8;
        if(bitPos>=32) {
            bitPos-=32;
            ++pos;
            if(bitPos>0) value|=(this->n[pos]<<(8-bitPos))&0xffL;
        }
        buffer+=(unsigned char)value;
    }
    if(!this->negative && (char)buffer[buffer.length()-1]<0) buffer+=(unsigned char)0;
    std::reverse(buffer.begin(),buffer.end());
    return byteCount;
}
// FUNCTION: WON 0x007d3530
// Sets the value from big-endian unsigned bytes
void BigInteger::fromBinary(const RawBuffer& buffer)
{
    this->negative=false;
    int count=(buffer.length()*8+31)/32;
    BigInteger::st_grow(*this,count,false);
    this->len=count;
    int pos=0;
    this->n[pos]=0;
    int bitPos=0;
    for(int i=buffer.length()-1;i>=0;--i) {
        word value=buffer[i]&0xff;
        this->n[pos]|=value<<bitPos;
        bitPos+=8;
        if(bitPos<32 || i<=0)
            continue;
        ++pos;
        this->n[pos]=0;
        bitPos-=32;
        if(bitPos>0) this->n[pos]=value>>(8-bitPos);
    }
    while(this->len>0 && this->n[this->len-1]==0) --this->len;
}
}

// WON SDK class constants; their dynamic initializers and atexit cleanups are compiler generated
namespace WONAPI {
const BigInteger BigInteger::ZERO = BigInteger(0);
const BigInteger BigInteger::ONE = BigInteger(1);
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007d56c0
// (_$E36    [_$E36])
// TEMPLATE: WON 0x007d56d0
// (_$E32    [_$E32])
// TEMPLATE: WON 0x007d56e0
// (_$E35    [_$E35])
// TEMPLATE: WON 0x007d56f0
// (_$E33    [_$E33])
// TEMPLATE: WON 0x007d5710
// (_$E41    [_$E41])
// TEMPLATE: WON 0x007d5720
// (_$E38    [_$E38])
// TEMPLATE: WON 0x007d5730
// (_$E40    [_$E40])
// TEMPLATE: WON 0x007d5740
// (_$E39    [_$E39])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007daf40
// (public: class std::basic_string<unsigned char,struct std::char_traits<unsigned char>,class std::allocator<unsigned char> > & __thiscall std::basic_str)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007daea0
// (public: class std::_Ptrit<unsigned char,int,unsigned char *,unsigned char &,unsigned char *,unsigned char &> __thiscall std::basic_string<unsigned cha)
// TEMPLATE: WON 0x007daee0
// (public: class std::_Ptrit<unsigned char,int,unsigned char *,unsigned char &,unsigned char *,unsigned char &> __thiscall std::basic_string<unsigned cha)
// TEMPLATE: WON 0x007db070
// (void __cdecl std::reverse(class std::_Ptrit<unsigned char,int,unsigned char *,unsigned char &,unsigned char *,unsigned char &>,class std::_Ptrit<unsig)
// TEMPLATE: WON 0x007db0a0
// (void __cdecl std::_Reverse(class std::_Ptrit<unsigned char,int,unsigned char *,unsigned char &,unsigned char *,unsigned char &>,class std::_Ptrit<unsi)
// TEMPLATE: WON 0x007db0f0
// (public: class std::_Ptrit<unsigned char,int,unsigned char *,unsigned char &,unsigned char *,unsigned char &> & __thiscall std::_Ptrit<unsigned char,in)
// TEMPLATE: WON 0x007db100
// (void __cdecl std::iter_swap(class std::_Ptrit<unsigned char,int,unsigned char *,unsigned char &,unsigned char *,unsigned char &>,class std::_Ptrit<uns)
// TEMPLATE: WON 0x007db130
// (void __cdecl std::_Iter_swap(class std::_Ptrit<unsigned char,int,unsigned char *,unsigned char &,unsigned char *,unsigned char &>,class std::_Ptrit<un)
