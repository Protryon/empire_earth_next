// Sound decoder base: lifetime and descriptor decoding through the virtual Decode (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/EELobbySound.h"
namespace WONAPI {
// FUNCTION: WON 0x0074a530
// Creates a sound decoder (nothing beyond the reference-counted base)
SoundDecoder::SoundDecoder()
{
}

// Destroys the decoder; the reference-counted base does the cleanup (emitted at 0x00728590)
SoundDecoder::~SoundDecoder()
{
}

// FUNCTION: WON 0x0074a540
// Decodes the sound a descriptor names by calling the virtual Decode
SoundPtr SoundDecoder::DecodeDescriptor(const SoundDescriptor& inDescriptor)
{
	return this->Decode(inDescriptor);
}
}
