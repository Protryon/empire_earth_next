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

int64_t EECore10OpB4(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // EEAnimal.cpp

#define P64(x) ((int64_t)(intptr_t)(x))
#define BTN ((EEButtonObject*)p)
#define BTEX ((GETexture*)p)
#define BMP ((GEBitmap*)p)

// the buttons' C++ calls and members (gamecpp db_media.rs; p the button, the texture or the bitmap): one statement each
int64_t EECore10OpB3(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::DbtFields: return P64(static_cast<EeDataButtonFields*>(BTN));
    case O::DbtUse: return P64(static_cast<EeDataMediaUse*>(BTN));
    case O::DbtLock: return P64(sDatabaseLock.mLock);
    case O::DbtNew: return P64(new EEButtonObject);
    case O::DbtDelete: delete BTN;
        break;
    case O::DbtResize: EEButtonObject::sTable.resize((unsigned long)a[0], NULL);
        break;
    case O::DbtCount: return (int64_t)EEButtonObject::sTable.size();
    case O::DbtAt: return P64(EEButtonObject::sTable[a[0]]);
    case O::DbtSet: EEButtonObject::sTable[a[0]] = (EEButtonObject*)a[1];
        break;
    case O::DbtCopyName: EEDbCopyName(BTN->mName, *(const EeDataName*)a[0]);
        break;
    case O::DbtName: return P64((const char*)BTN->mName);
    case O::DbtTexture: return P64(&BTN->mTexture);
    case O::DbtCellU: return P64(&BTN->mCellU);
    case O::DbtCellV: return P64(&BTN->mCellV);
    case O::DbtLoadTexture: return P64(gTextureManager->LoadTexture(BTN->mName, *(const GETextureDescription*)a[0]));
    case O::DbtReloadTexture: return gTextureManager->ReloadTexture(BTN->mName, *(const GETextureDescription*)a[0]);
    case O::DbtPixelDepth: return (int64_t)gTexturePixelDepth;
    case O::DbtScreenWidth: return (int64_t)gRasterizer->GetPrimarySurface(gScreenIndex)->GetWidth();
    case O::DbtTexWidth: return (int64_t)BTEX->GetWidth();
    case O::DbtTexHeight: return (int64_t)BTEX->GetHeight();
    case O::DbtTexMipDrop: return (int64_t)BTEX->GetNumMipLevelsToDrop();
    case O::DbtTexSurface: return P64(BTEX->GetSurface());
    case O::DbtTexSurfaceHeight: return (int64_t)BTEX->GetSurface()->GetHeight();
    case O::DbtBitmapTexture: BMP->SetTexture((GETexture*)a[0], (unsigned long)a[1]);
        break;
    case O::DbtBitmapCoordinates: BMP->SetTextureCoordinates(GETextureCoordinate(f[0], f[1]), GETextureCoordinate(f[2], f[1]), GETextureCoordinate(f[0], f[3]), GETextureCoordinate(f[2], f[3]));
        break;
    case O::DbtBitmapScale: BMP->SetXYScale(f[0], f[1]);
        break;
    case O::DbtHotKeyCount: return (int64_t)UIHotKey::GetHotKeyCount();
    case O::DbtHotKey: return P64(UIHotKey::GetHotKey((unsigned long)a[0]));
    default: return EECore10OpB4(inOp, p, a, f);
    }
    return 0;
}
