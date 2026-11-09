
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONElGamal.h"
namespace WONAPI {
// FUNCTION: WON 0x007d7820
// Marks the key as holding neither a public nor a private key
void ElGamal::Invalidate()
{
    this->mIsPrivate=false;
    this->mIsPublic=false;
}
// FUNCTION: WON 0x007d7830
// Returns whether a private key is loaded
bool ElGamal::IsPrivate() const
{
    return this->mIsPrivate;
}
// FUNCTION: WON 0x007d7840
// Returns whether a public key is loaded
bool ElGamal::IsPublic() const
{
    return this->mIsPublic;
}
}
