// The lobby's sounds: WONAPI's Sound / SoundDecoder bases (RTTI
// WONAPI::Sound vtable 0x4510fc, WONAPI::SoundDecoder vtable 0x451a88) and the game's implementations, which play
// through the engine's sound manager (EELobbyMusic vtable 0x44c7e8, EELobbySound 0x44c800, EELobbySoundDecoder
// 0x44c6dc). Names RTTI does not give are inferred from behaviour (provisional names). Requires <string>,
// lle/LowLevelEngine.h, ee/EEWonNet.h.
#ifndef EE_WONAPI_EELOBBYSOUND_H
#define EE_WONAPI_EELOBBYSOUND_H

namespace WONAPI {

// what a sound is made from (Sound's ctor copies mSoundFlags to +0x08 and mPath to +0x0c; the decoder loads mPath)
struct SoundDescriptor {
    SoundDescriptor(); // 0x329810 (no flags, empty path)
    SoundDescriptor(const char*);
    void SetSoundFlags(int,bool); // 0x329990 (sets or clears one flag bit)
    SoundDescriptor(const std::string& inPath, long inType);    // 0x329830

    long mSoundFlags;                       // +0x00
    std::string mPath;                      // +0x04
};

class Sound : public RefCount {
public:
    Sound(const SoundDescriptor& inDescriptor);     // 0x3299c0 (WONAPI, /GX)
    // no user dtor: EELobbyMusic's ??_G 0x29ce1e destroys mPath and stores RefCount's vptr only
    virtual void PlayHook() {}              // +0x04 (empty; shared body 0x326b60)
    virtual void StopHook() {}              // +0x08 (empty; shared body 0x326b60)
    virtual void ResumeHook() {}            // +0x0c (empty; shared body 0x326b60)
    virtual void RewindHook() {}            // +0x10 (empty; shared body 0x326b60)
    void Play();                            // 0x329bd0
    void Stop();                           // 0x3a6db0
    void Resume();                         // 0x3a6dc0
    void Rewind();
    void SetLooping(bool inLooping) { this->mIsLooping = inLooping; }
    void SetMuted(bool inMuted);            // 0x329bf0: sets or clears the descriptor's flag 4 (provisional name)
    bool GetMuted();                       // 0x329c20

    SoundDescriptor mDescriptor;            // +0x08
    bool mIsLooping;                        // +0x1c (padding to 0x1f; the constructor stores a byte)
};

typedef SmartPtr<Sound> SoundPtr;

class SoundDecoder : public RefCount {
public:
    SoundDecoder();                         // 0x34a530
    virtual ~SoundDecoder();                // 0x328590 (ICF: ~LobbyPlayer; ??_GEELobbySoundDecoder 0x297dc5 calls it)
    SoundPtr DecodeDescriptor(const SoundDescriptor& inDescriptor); // 0x34a540 (forwards to Decode)
    virtual SoundPtr Decode(const SoundDescriptor& inDescriptor) = 0;   // +0x04
};

// the lobby music: "playing" it turns the game's music volume up, stopping it turns it down
class EELobbyMusic : public Sound {
public:
    EELobbyMusic(const SoundDescriptor& inDescriptor);    // 0x29ce06

    virtual void PlayHook();                // +0x04 0x29ce46
    virtual void StopHook();                // +0x08 0x29ce61
    virtual void ResumeHook();              // +0x0c 0x29ce78 (plays again)
};

// a lobby sound effect: a sample of the engine's sound manager
class EELobbySound : public Sound {
public:
    EELobbySound(const SoundDescriptor& inDescriptor, unsigned long inSampleID);    // 0x29ce7d
    virtual ~EELobbySound();                // 0x29cebc (unloads the sample)

    virtual void PlayHook();                // +0x04 0x29ceeb
    virtual void StopHook();                // +0x08 0x29cf1c

    unsigned long mSampleID;                // +0x20
    unsigned long mInstanceID;              // +0x24 (-1: not playing)
};

// the lobby's sound decoder (ctor and ??_G in the lobby init unit, 0x2972c6 / 0x297dc5)
class EELobbySoundDecoder : public SoundDecoder {
public:
    virtual SoundPtr Decode(const SoundDescriptor& inDescriptor);       // +0x04 0x29cf40
};

}

#endif
