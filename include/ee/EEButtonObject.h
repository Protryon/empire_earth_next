// C:\EEX\Empire Earth\Database\EEButtonObject.h (RECONSTRUCTION; file name inferred; provisional names). A
// database button object (RTTI EEButtonObject; EEButtonObject.cpp 0xd5405..0xd5d08). Only what the decompiled units
// call.
#ifndef EE_EEBUTTONOBJECT_H
#define EE_EEBUTTONOBJECT_H

class GEBitmap;
class GERectangle;
class FSFileSpec;
class GETexture;
class UIHotKey;

#include "rust/data/tables.h"
#include "rust/data/media.h"

// RTTI, vtable 0x43e4f4 (two slots), ctor 0xd5498. The record's members (mPosition, mHotKeyIndex, mColumn, mRow,
// mID) are EeDataButtonFields (rust/crates/ee_data tables.rs: the record and its copy), the base; the use count
// (mMediaCount, mIsLoading) is EeDataMediaUse (media.rs).
class EEButtonObject : public EeDataButtonFields, public EeDataMediaUse {
public:
    EEButtonObject();
    virtual ~EEButtonObject();              // +0x00 (??_G 0xd54c3)
    virtual long LoadMedia();               // +0x04 0xd5cfe (a tail call of Acquire)
    virtual long UnloadMedia();             // +0x08 0xd5d03 (a tail call of Release)
    long Release();                         // 0xd5bdf (EEPlayer::SwapClassButtonMedia; provisional name): dereference (returns 0)
    long Acquire();                         // 0xd5690: reference, loading the texture on the first one
    long LoadTexture();
    static long Load(FSFileSpec* inFile);   // 0xd5500 (db\dbButtons.dat)
    static void Unload();
    static void DumpAllLoadedMedia(FSFileSpec& inFile);
    static long Reload();                   // 0xd5c31 (ReloadTexture of every referenced button)
    // 0xd584e (ret 0xc): sets inBitmap to this button's cell for inRectangle (EEUIBSavedGroup::SetUnit; provisional
    // name)
    void SetFaceBitmap(GEBitmap* inBitmap, const GERectangle& inRectangle, unsigned long inScreen);
    // 0xd5a2e: the same for a pressed button (EEUIFMGStatusButton::CopyData; provisional name)
    void SetPressedBitmap(GEBitmap* inBitmap, const GERectangle& inRectangle, unsigned long inScreen);
    // 0x1cba0c (an ICF-kept copy before EETechTree.cpp): the button table by index (vector 0x4e60e8), 0 past its end
    // (provisional name)
    static EEButtonObject* Get(unsigned long inIndex);
    // 0x26918d (at the end of EEUIFMGMainGroup.cpp, called out of line: defined there): the hot key mL24, 0 out of range
    // Provisional name
    UIHotKey* GetHotKey();
    // inline: EEUIBSavedGroup::SetUnit copies them through the FP stack (fld/fstp; provisional names)
    GETexture* GetTexture() const { return this->mTexture; }
    float GetCellU() const { return this->mCellU; }
    float GetCellV() const { return this->mCellV; }
    // members from the ctor 0xd5498 / dtor 0xd54df (sizeof 0x3c, new site); (provisional names)
    UString mName;                          // +0x04
    GETexture* mTexture;                    // +0x14 (referenced: GETexture::Dereference in the dtor)
    float mCellU;                           // +0x18
    float mCellV;                           // +0x1c
    // 0x8e60e8 (guard byte 0x8e60e4 bit 1). Only EEButtonObject.cpp sees it: its $E number would renumber the
    // includers' chains (provisional name, as the engine's LOWLEVELENGINE_EXPORTS statics)
    static std::vector<EEButtonObject*> sTable;
};

#endif
