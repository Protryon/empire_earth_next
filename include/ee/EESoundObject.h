// C:\EEX\Empire Earth\Database\EESoundObject.h (RECONSTRUCTION; file name inferred). One sound of
// db\dbsounds.dat (vtable 0x43f7c0; EESoundObject.cpp around 0xda600). Field names are provisional; offsets proven by
// the
// calamity units (sound playback). Requires ee/EEMedia.h.
#ifndef EE_EESOUNDOBJECT_H
#define EE_EESOUNDOBJECT_H

class EESoundObject;
class FSFile;
class FSFileSpec;
extern std::vector<EESoundObject*> gSoundObjects;   // 0x4e867c (the sound table; provisional name; needs <vector>)

class EESoundObject : public EEMedia {
public:
    // the table of every sound by index (inline; its out-of-line COMDAT 0x525d sits in EEAUpgradeImprovement.cpp's
    // range after its first user; 0 past the end) (provisional name)
    static EESoundObject* Get(unsigned long inIndex)
    {
        if (inIndex < gSoundObjects.size())
            return gSoundObjects[inIndex];
        return NULL;
    }
    // inline accessors: the calamity classes pass their values from registers and FP temporaries
    // the unit EESoundObject.cpp (0xda56d..0xda984)
    EESoundObject();                        // 0xda600
    virtual ~EESoundObject();               // ??_G 0xda638, dtor 0xda654 (unloads when still counted)
    virtual long LoadMedia();               // EEMedia +0x04 0xda7e6 (counts a use; 9: no such sample)
    virtual long UnloadMedia();             // EEMedia +0x08 0xda92e
    static long LoadTable(FSFile* inFile);  // 0xda680 (db\dbsounds.dat)
    static void UnloadTable();              // 0xda747
    static void Dump(FSFileSpec& inFile);       // 0xda787 (fprintf to the spec's stream; EEWorld::DumpMedia)
    bool GetLooping() const { return this->mLooping; }
    unsigned long GetSampleID() const { return this->mSampleID; }
    float GetVolume() const { return this->mVolume; }
    void SetVolume(float inVolume);         // 0xb62d3 (ICF-folded with EEContagionHelper::SetFunctor: the store to +0x14; the debug sound panel 0x2545c0) (provisional name)
    void Play();                            // 0xda891: plays it at (1, 0, 1) through 0xda8a9 (provisional name)
    long Play(float inVolume, float inPan, float inPitch);   // 0xda8a9 (EEUGUnloadTransport::Unload) (provisional name)

    UString mName;                          // +0x04 (the ctor's member ctor)
    float mVolume;                          // +0x14
    unsigned long mSampleID;                // +0x18 SESoundManager sample id
    long mMaxInstances;                     // +0x1c (+0x1c..+0x28: the ctor's stores)
    long mCategory;                         // +0x20
    long mL24;                              // +0x24
    unsigned long mMediaCount;              // +0x28 (the use count)
    bool mLooping;                          // +0x2c
    volatile bool mIsLoading;                      // +0x2d (sizeof 0x30)
};

// dbsounds.dat sounds by table index (EESoundObject::Get; LoadTable 0x4da680 fills gSoundObjects in file order)
enum EESoundID {
    kSoundFlare = 0x8,                // dbsounds.dat record 8 (file order = gSoundObjects index; +0x68 = 8) name 'Flare' (sounds\flare-8.wav); its sites call EESoundObject::Get
    kSoundCashRegister = 0xe,         // dbsounds.dat record 14 (file order = gSoundObjects index; +0x68 = 14) name 'Cash Register' (sounds\register-8.wav); its sites call EESoundObject::Get
    kSoundGarrison = 0x86,            // dbsounds.dat record 134 (file order = gSoundObjects index; +0x68 = 134) name 'Garrison' (sounds\garrison2.wav); its sites call EESoundObject::Get
    kSoundLightning = 0x176,          // dbsounds.dat record 374 (file order = gSoundObjects index; +0x68 = 374) name 'Lightning' (sounds\Slightning2-8.wav); its sites call EESoundObject::Get
    kSoundTransportLoad = 0x183,      // dbsounds.dat record 387 (file order = gSoundObjects index; +0x68 = 387) name 'Transport Load' (sounds\Transport-8.wav); its sites call EESoundObject::Get
    kSoundUnderAttack = 0x18d,        // dbsounds.dat record 397 (file order = gSoundObjects index; +0x68 = 397) name 'Under Attack' (sounds\dunderattack1.wav); its sites call EESoundObject::Get
    kSoundTrumpets = 0x1b7,           // dbsounds.dat record 439 (file order = gSoundObjects index; +0x68 = 439) name 'Trumpets' (sounds\fanfare-8.wav); its sites call EESoundObject::Get
    kSoundThunder2 = 0x1ba,           // dbsounds.dat record 442 (file order = gSoundObjects index; +0x68 = 442) name 'Thunder 2' (sounds\thunder-8.wav); its sites call EESoundObject::Get
    kSoundGate = 0x1e1,               // dbsounds.dat record 481 (file order = gSoundObjects index; +0x68 = 481) name 'Gate' (sounds\gate-8.wav); its sites call EESoundObject::Get
    kSoundButton1 = 0x207,            // dbsounds.dat record 519 (file order = gSoundObjects index; +0x68 = 519) name 'Button 1' (sounds\button1.wav); its sites call EESoundObject::Get
    kSoundButton2 = 0x208,            // dbsounds.dat record 520 (file order = gSoundObjects index; +0x68 = 520) name 'Button 2' (sounds\button2.wav); its sites call EESoundObject::Get
    kSoundClockTick = 0x3a8,          // dbsounds.dat record 936 (file order = gSoundObjects index; +0x68 = 936) name 'Clock Tick' (sounds\clock-8.wav); its sites call EESoundObject::Get
    kSoundIncomingChatMessage = 0x3ae,  // dbsounds.dat record 942 (file order = gSoundObjects index; +0x68 = 942) name 'Incoming Chat Message' (sounds\message-8.wav); its sites call EESoundObject::Get
    kSoundComputerAttack = 0x3bd,     // dbsounds.dat record 957 (file order = gSoundObjects index; +0x68 = 957) name 'Computer Attack' (sounds\compattack.wav); its sites call EESoundObject::Get
    kSoundWonderClock = 0x3be,        // dbsounds.dat record 958 (file order = gSoundObjects index; +0x68 = 958) name 'Wonder Clock' (sounds\wonderclock.wav); its sites call EESoundObject::Get
    kSoundHotKeyAlarm = 0x3c2,        // dbsounds.dat record 962 (file order = gSoundObjects index; +0x68 = 962) name 'Hot Key Alarm' (sounds\hotkeyalarm.wav); its sites call EESoundObject::Get
    kSoundInsuranceCollected = 0x419,  // dbsounds.dat record 1049 (file order = gSoundObjects index; +0x68 = 1049) name 'Insurance collected' (sounds\insurance-8.wav); its sites call EESoundObject::Get
    kSoundPriestTowerConvert = 0x41c,  // dbsounds.dat record 1052 (file order = gSoundObjects index; +0x68 = 1052) name 'Priest Tower Convert' (sounds\ptower_16.wav); its sites call EESoundObject::Get
    kSoundSlavery = 0x422,            // dbsounds.dat record 1058 (file order = gSoundObjects index; +0x68 = 1058) name 'Slavery' (sounds\slavery.wav); its sites call EESoundObject::Get
};

#endif
