
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
// Creates an empty sound descriptor with no flags (emitted at 0x00729810)
SoundDescriptor::SoundDescriptor() : mSoundFlags(0) {}
// Creates a sound descriptor for a file path with the given flags (emitted at 0x00729830)
SoundDescriptor::SoundDescriptor(const std::string& path,long flags) : mSoundFlags(flags), mPath(path) {}
// Sets or clears the given sound flags
void SoundDescriptor::SetSoundFlags(int flags,bool on)
{
    if(on) this->mSoundFlags|=flags;
    else this->mSoundFlags&=~flags;
}
// Creates a sound from a copy of the descriptor, not looping (emitted at 0x007299c0)
Sound::Sound(const SoundDescriptor& descriptor) : mDescriptor(descriptor), mIsLooping(false) {}
// Plays the sound unless it is muted
void Sound::Play()
{
    if(!this->GetMuted()) this->PlayHook();
}
// Mutes or unmutes the sound (flag 4); muting stops it
void Sound::SetMuted(bool muted)
{
    if(muted==this->GetMuted()) return;
    this->mDescriptor.SetSoundFlags(4,muted);
    if(muted) this->Stop();
}
}

// Stop and Resume forward to the playback hooks
void WONAPI::Sound::Stop()
{
    this->StopHook();
}
void WONAPI::Sound::Resume()
{
    this->ResumeHook();
}

// Returns true when the sound's muted flag (4) is set
bool WONAPI::Sound::GetMuted()
{
    return (this->mDescriptor.mSoundFlags&4)?true:false;
}

namespace WONAPI {


SoundDescriptor::SoundDescriptor(const char *thePath) : mSoundFlags(0), mPath(thePath) {}


void Sound::Rewind()
{
	this->RewindHook();
}
}

// the sound decoder base (SoundDecoder.cpp until R6 WON GUI round 37): decodes a descriptor through the virtual Decode
namespace WONAPI {
SoundDecoder::SoundDecoder() {}
SoundDecoder::~SoundDecoder() {} // (emitted at 0x00728590)
SoundPtr SoundDecoder::DecodeDescriptor(const SoundDescriptor& inDescriptor) { return this->Decode(inDescriptor); }
}
