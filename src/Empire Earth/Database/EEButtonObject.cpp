// C:\EEX\Empire Earth\Database\EEButtonObject.cpp (name from RTTI EEButtonObject; no __FILE__). 0xd5405..0xd5d08:
// the class static table's init chain, the header lock's chain (Database\EEDatabase.h), ctor, ??_G, dtor, Load
// (db\dbButtons.dat), Unload, DumpAllLoadedMedia, Acquire (reference + load on demand), LoadTexture, two drawing
// helpers, Release (dereference), Reload, the two virtual thunks. EEAnimal.cpp ends at 0xd5405, the globals-chain
// unit (EEColorTable?) starts at 0xd5d08. Unit flags: the exe's module set.
// port (game core lane, round 10): the bodies are Rust (ee_game gamecpp db_media.rs); the methods forward, their C++
// calls and members are EECore10OpB3's cases.

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/UIHotKey.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GEBitmap.h"

#include "ee/EmpireEarth.h"
#include "ee/EEButtonObject.h"
#include "ee/EEDbRecords.h"
#include "ee/EEMediaUse.h"
#include "rust/game/gamecpp.h"

std::vector<EEButtonObject*> EEButtonObject::sTable;

#include <EEDatabase.h>

// 0x5308b4: the pixel depth textures are loaded with (16 or 32; provisional name)
extern unsigned long& gTexturePixelDepth;

// Creates a button image entry with no texture and no references
EEButtonObject::EEButtonObject()
    : EeDataButtonFields(), EeDataMediaUse(), mTexture(NULL), mCellU(0.0f), mCellV(0.0f)
{
}

// Destroys the button image entry, releasing its texture
EEButtonObject::~EEButtonObject()
{
    ee_gamecpp_db_button_free(this);
}

// Loads the button image entries (file name, cell row and column, id, hot key, position) from db\dbButtons.dat;
// stops at the first read error and returns it
long EEButtonObject::Load(FSFileSpec* inFile)
{
    return ee_gamecpp_db_button_load(inFile);
}

// Destroys every button image entry and empties the table
void EEButtonObject::Unload()
{
    ee_gamecpp_db_button_unload();
}

// Writes each button image with a non-zero reference count to a media dump file
void EEButtonObject::DumpAllLoadedMedia(FSFileSpec& inFile)
{
    ee_gamecpp_db_button_dump(&inFile);
}

// Adds a reference to the button image, loading its texture on the first one (later callers wait for that load);
// returns the load error
long EEButtonObject::Acquire()
{
    return ee_gamecpp_db_button_acquire(this);
}

// Loads the button's texture (a mip level dropped at 800 pixels wide or less, alpha for ids below 50) and computes
// its cell's texture offset; returns 3 when the texture is missing
long EEButtonObject::LoadTexture()
{
    return ee_gamecpp_db_button_load_texture(this);
}

// Shows the button's cell, inset by one texel, on a bitmap scaled to a rectangle; a button without a texture (or
// whose surface is not loaded: EE-SIM's textures, a scenario's information portrait) is skipped
void EEButtonObject::SetFaceBitmap(GEBitmap* inBitmap, const GERectangle& inRectangle, unsigned long inScreen)
{
    ee_gamecpp_db_button_bitmap(this, true, inBitmap, &inRectangle, inScreen);
}

// Shows the pressed button's cell (no texel inset) on a bitmap scaled to a rectangle (no texture test)
void EEButtonObject::SetPressedBitmap(GEBitmap* inBitmap, const GERectangle& inRectangle, unsigned long inScreen)
{
    ee_gamecpp_db_button_bitmap(this, false, inBitmap, &inRectangle, inScreen);
}

// Drops a reference to the button image once any load has finished, releasing its texture with the last one; returns 0
long EEButtonObject::Release()
{
    return ee_gamecpp_db_button_release(this);
}

// Reloads the textures of every referenced button image (a mip level dropped at 800 pixels wide or less, as on the
// first load); returns the first error
long EEButtonObject::Reload()
{
    return ee_gamecpp_db_button_reload();
}

// Loads the button image for a user (media interface): adds a reference
long EEButtonObject::LoadMedia()
{
    return this->Acquire();
}

// Unloads the button image for a user (media interface): drops a reference
long EEButtonObject::UnloadMedia()
{
    return this->Release();
}

// Returns the button's hot key, or NULL (an inline COMDAT the original emitted in EEUIFMGMainGroup.cpp)
UIHotKey* EEButtonObject::GetHotKey()
{
    return (UIHotKey*)ee_gamecpp_db_hot_key(this->mHotKeyIndex);
}

// ------------------------------------------------------------------------- round 10, lane B: the buttons' C++


#define P64(x) ((int64_t)(intptr_t)(x))
#define BTN ((EEButtonObject*)p)
#define BTEX ((GETexture*)p)
#define BMP ((GEBitmap*)p)

