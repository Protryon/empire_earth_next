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

#include "rust/data/media.h"
#include "rust/game/gamecpp.h"

// RTTI, vtable 0x43e4f4 (two slots), ctor 0xd5498. The record's members (mPosition, mHotKeyIndex, mColumn, mRow,
// mID) are EeDataButtonFields (rust/crates/ee_data tables.rs: the record and its copy), the base; the use count
// (mMediaCount, mIsLoading) is EeDataMediaUse (media.rs).
class EEButtonObject : public EeDataButtonFields, public EeDataMediaUse {
public:
    // Creates a button image entry with no texture and no references
    EEButtonObject() : EeDataButtonFields(), EeDataMediaUse(), mTexture(NULL), mCellU(0.0f), mCellV(0.0f) {  }
    // Destroys the button image entry, releasing its texture
    // +0x00 (??_G 0xd54c3)
    virtual ~EEButtonObject() { ee_gamecpp_db_button_free(this); }
    // Loads the button image for a user (media interface): adds a reference
    // +0x04 0xd5cfe (a tail call of Acquire)
    virtual long LoadMedia() { return this->Acquire(); }
    // Unloads the button image for a user (media interface): drops a reference
    // +0x08 0xd5d03 (a tail call of Release)
    virtual long UnloadMedia() { return this->Release(); }
    // Drops a reference to the button image once any load has finished, releasing its texture with the last one; returns 0
    // 0xd5bdf (EEPlayer::SwapClassButtonMedia; provisional name): dereference (returns 0)
    long Release() { return ee_gamecpp_db_button_release(this); }
    // Adds a reference to the button image, loading its texture on the first one (later callers wait for that load);
    // returns the load error
    // 0xd5690: reference, loading the texture on the first one
    long Acquire() { return ee_gamecpp_db_button_acquire(this); }
    // Loads the button image entries (file name, cell row and column, id, hot key, position) from db\dbButtons.dat;
    // stops at the first read error and returns it
    // 0xd5500 (db\dbButtons.dat)
    static long Load(FSFileSpec* inFile) { return ee_gamecpp_db_button_load(inFile); }
    // Destroys every button image entry and empties the table
    static void Unload() { ee_gamecpp_db_button_unload(); }
    // Writes each button image with a non-zero reference count to a media dump file
    static void DumpAllLoadedMedia(FSFileSpec& inFile) { ee_gamecpp_db_button_dump(&inFile); }
    // 0xd584e (ret 0xc): sets inBitmap to this button's cell for inRectangle (EEUIBSavedGroup::SetUnit; provisional
    // name)
    // Shows the button's cell, inset by one texel, on a bitmap scaled to a rectangle; a button without a texture (or
    // whose surface is not loaded: EE-SIM's textures, a scenario's information portrait) is skipped
    void SetFaceBitmap(GEBitmap* inBitmap, const GERectangle& inRectangle, unsigned long inScreen) { ee_gamecpp_db_button_bitmap(this, true, inBitmap, &inRectangle, inScreen); }
    // 0xd5a2e: the same for a pressed button (EEUIFMGStatusButton::CopyData; provisional name)
    // Shows the pressed button's cell (no texel inset) on a bitmap scaled to a rectangle (no texture test)
    void SetPressedBitmap(GEBitmap* inBitmap, const GERectangle& inRectangle, unsigned long inScreen) { ee_gamecpp_db_button_bitmap(this, false, inBitmap, &inRectangle, inScreen); }
    // 0x1cba0c (an ICF-kept copy before EETechTree.cpp): the button table by index (vector 0x4e60e8), 0 past its end
    // (provisional name)
    static EEButtonObject* Get(unsigned long inIndex);
    // 0x26918d (at the end of EEUIFMGMainGroup.cpp, called out of line: defined there): the hot key mL24, 0 out of range
    // Provisional name
    // Returns the button's hot key, or NULL (an inline COMDAT the original emitted in EEUIFMGMainGroup.cpp)
    UIHotKey* GetHotKey() { return (UIHotKey*)ee_gamecpp_db_hot_key(this->mHotKeyIndex); }
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
    inline static std::vector<EEButtonObject*> sTable;
};

#endif
