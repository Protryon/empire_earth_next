#ifndef EE_WON_COMMON_MD5_H
#define EE_WON_COMMON_MD5_H
namespace WONAPI {
// WONCommon MD5 (0x58 bytes), distinct from WONCrypt MD5Digest (0xa8). No dynamic members or vtable.
class MD5 {
public:
    typedef unsigned long uint32;
private:
    uint32 m_buf[4]; //00..10
    uint32 m_bits[2]; //10..18
    unsigned char m_in[64]; //18..58
    void Transform(uint32 buf[4],uint32 in[16]); //2a2870
public:
    MD5(); //2a26b0
    void Reset(); //2a26c0
    void Update(const void*,unsigned long); //2a26f0
    void Digest(unsigned char[16]); //2a27d0
};
}
#endif
