// CRC16: table-driven 16-bit CRC (polynomial 0x8005), ee_won's (rust/crates/ee_won/src/crc16.rs)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <stdint.h>
#include <stddef.h>
#include "rust/ee_rust.h"
#include "ee/wonapi/WONCRC16.h"
namespace WONAPI {
// FUNCTION: WON 0x006a0c70
// Creates a CRC with the given initial and final XOR values and resets it
CRC16::CRC16(unsigned short initial, unsigned short xorOut)
    : mInitValue(initial), mXOROutValue(xorOut) { this->Reset(); }
// FUNCTION: WON 0x006a0c90
// Feeds the given bytes into the running CRC
void CRC16::Put(const void* data, unsigned int length)
{
    this->mRegister = ee_won_crc16_put(this->mRegister, static_cast<const uint8_t*>(data), length);
}
}
