// C:\EEX\Empire Earth\Database\EEMedia.h (RECONSTRUCTION; file name inferred). Base of the database's media
// objects (EESoundObject, EEMusicObject, EEGraphicEffect's second base). Vtable 0x43e53c. Only what the calamity
// units see.
#ifndef EE_EEMEDIA_H
#define EE_EEMEDIA_H

class EEMedia {
public:
    virtual ~EEMedia() {}                  // +0x00 (??_G 0xd7274)
    // +0x04/+0x08 (the area-effect calamities load and unload their sound through them; provisional names). Not
    // EEGraphicEffect's Load/Unload: those are called directly, with no base adjustment
    virtual long LoadMedia() = 0;           // pure in the vtable
    virtual long UnloadMedia() = 0;
    // 0xd53e2: the media table by index (vector 0x4e8448), 0 past its end (the tech tree's event kind 6; provisional
    // name)
    static EEMedia* Get(unsigned long inIndex);
    static unsigned long GetCount();        // 0xd7291 (the media table's size)
};

#endif
