
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
SoundDescriptor::SoundDescriptor() { this->mSoundFlags=0; }
// Creates a sound descriptor for a file path with the given flags (emitted at 0x00729830)
SoundDescriptor::SoundDescriptor(const std::string& path,long flags) :mPath(path)
{
    this->mSoundFlags=flags;
}
// FUNCTION: WON 0x00729990
// Sets or clears the given sound flags
void SoundDescriptor::SetSoundFlags(int flags,bool on)
{
    if(on) this->mSoundFlags|=flags;
    else this->mSoundFlags&=~flags;
}
// Creates a sound from a copy of the descriptor, not looping (emitted at 0x007299c0)
Sound::Sound(const SoundDescriptor& descriptor)
    :mDescriptor(descriptor)
{
    this->mIsLooping=false;
}
// FUNCTION: WON 0x00729bd0
// Plays the sound unless it is muted
void Sound::Play()
{
    if(!this->GetMuted()) this->PlayHook();
}
// FUNCTION: WON 0x00729bf0
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

// FUNCTION: WON 0x00729c20
// Returns true when the sound's muted flag (4) is set
bool WONAPI::Sound::GetMuted()
{
    return (this->mDescriptor.mSoundFlags&4)?true:false;
}

namespace WONAPI {


SoundDescriptor::SoundDescriptor(const char *thePath) : mPath(thePath)
{
	this->mSoundFlags = 0;
}


void Sound::Rewind()
{
	this->RewindHook();
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00729810
// (public: __thiscall WONAPI::SoundDescriptor::SoundDescriptor(void)    [??0SoundDescriptor@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x00729b60
// (public: virtual __thiscall WONAPI::Sound::~Sound(void)    [??1Sound@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00729b40
// (public: virtual void * __thiscall WONAPI::Sound::`scalar deleting destructor'(unsigned int)    [??_GSound@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00729830
// (public: __thiscall WONAPI::SoundDescriptor::SoundDescriptor(class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > co)
// TEMPLATE: WON 0x007299c0
// (public: __thiscall WONAPI::Sound::Sound(struct WONAPI::SoundDescriptor const &)    [??0Sound@WONAPI@@QAE@ABUSoundDescriptor@1@@Z])
