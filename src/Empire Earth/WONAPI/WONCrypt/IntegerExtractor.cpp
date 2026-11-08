// IntegerExtractor: ASN.1 integer reader used by ElGamal key loading
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONIntegerExtractor.h"
namespace WONAPI {
// FUNCTION: WON 0x007d8e10
// Wraps key bytes for reading, skipping the sequence header and its length when buffered
IntegerExtractor::IntegerExtractor(const void* data,unsigned long size,bool buffered)
    : input(static_cast<const unsigned char*>(data),size),offset(0),mIsValid(true)
{
    if (buffered) {
        if (this->GetByte()!=(SEQUENCE|CONSTRUCTED)) return;
        if (!this->LengthDecode()) return;
    }
}
// FUNCTION: WON 0x007d8eb0
// Returns the next byte, or 0 and marks the data invalid at the end
unsigned char IntegerExtractor::GetByte()
{
    if (this->offset<this->input.length()) return this->input[this->offset++];
    this->mIsValid=false;
    return 0;
}
// FUNCTION: WON 0x007d8fb0
// Returns the next byte without consuming it, or 0 and marks the data invalid at the end
unsigned char IntegerExtractor::PeekByte()
{
    if (this->offset<this->input.length()) return this->input[this->offset];
    this->mIsValid=false;
    return 0;
}
// FUNCTION: WON 0x007d90a0
// Skips an ASN.1 length field (short form or up to two significant bytes); returns validity
bool IntegerExtractor::LengthDecode()
{
    unsigned char b=this->GetByte();
    if (b&0x80) {
        int count=b&0x7f;
        if (this->input.length()-this->offset>=count) {
            b=this->GetByte();
            while (b==0 && count>1) {
                b=this->GetByte();
                --count;
            }
            switch (count) {
            case 0: return false;
            case 1: return this->mIsValid;
            case 2: this->GetByte(); return this->mIsValid;
            }
        }
        return false;
    }
    return this->mIsValid;
}
// FUNCTION: WON 0x007d9100
// Reads length bytes (dropping leading zeros) as an unsigned big integer; returns validity
bool IntegerExtractor::Decode(int length,BigInteger& value)
{
    if (!this->mIsValid) return false;
    if (this->input.length()<=this->offset) return false;
    // The original peeks at the leading byte here; the signed two's-complement conversion is disabled
    this->PeekByte();
    while (length>0 && this->PeekByte()==0) {
        ++this->offset;
        --length;
    }
    RawBuffer bytes(length,(unsigned char)0);
    for (int i=0;i<length;++i) bytes[i]=this->GetByte();
    value.fromBinary(bytes);
    return this->mIsValid;
}
// FUNCTION: WON 0x007d92a0
// Reads the next ASN.1 INTEGER into value; false on a wrong tag, a long length or bad data
bool IntegerExtractor::Extract(BigInteger& value)
{
    if (!this->mIsValid) return false;
    if (this->GetByte()!=INTEGER) return false;
    int count;
    if ((this->PeekByte()&0x80)==0) count=this->GetByte();
    else {
        int width=this->GetByte()&0x7f;
        if (width>2) return false;
        count=this->GetByte();
        if (width>1) count=(count<<8)|this->GetByte();
    }
    return this->Decode(count,value);
}
}
