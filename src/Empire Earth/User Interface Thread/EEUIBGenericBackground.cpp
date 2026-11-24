// C:\EEX\Empire Earth\User Interface Thread\EEUIBGenericBackground.cpp (file name inferred: no __FILE__; RTTI class
// EEUIBGenericBackground). The tinted, framed form background: 0x254911..0x255c2a (the four GERectangle `$E` chains
// of the frame's texture cells, ctor, ??_G, dtor, the frame pieces, DrawBackground, Open, then 0x2556e6;
// EEUIBColor ends at 0x254911, EEUIBSavedGroup starts at 0x255c2a). Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "rust/render/ge.h"   // ee_ge_draw2d_fill (ee_ge list2d.rs)
#include "lle/GEBitmap.h"
#include "lle/FSArchive.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "ee/EmpireEarth.h"
#include "lle/UIBitmapBackground.h"
#include "ee/EEUIBGenericBackground.h"
#include "lle/FSPath.h"
#include "lle/UIBFull.h"
#include "ee/EEGameOptions.h"
#include "ee/EEGamePreferences.h"
#include "ee/EEUIManager.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EEUIBMapBackground.h"
#include "rust/game/screens.h"   // ee_screens_frame_draw (ee_screens frame.rs)

extern char gEmptyString[];                 // 0x4e5b98 (pooled "")

// the frame's cells in its 64x64 texture (provisional names)
static GERectangle sEdgeCell(57.0f, 0.0f, 64.0f, 64.0f);           // 0x564558 (the edges)
static GERectangle sFrameCornerCell(0.0f, 0.0f, 28.0f, 38.0f);     // 0x564548 (the large corners)
static GERectangle sCornerCell(49.0f, 0.0f, 56.0f, 7.0f);          // 0x564538 (the small corners)
static GERectangle sSideCell(30.0f, 0.0f, 48.0f, 20.0f);           // 0x564528 (the side ornaments)


// ---- the one-statement shims of ee_screens backgrounds_calls.rs (UI lane round 8: the frame's fill and pieces,
// Open, the map's LoadBackground): each one call, no logic
#include "ee/EEScreensShims.h"
#define GB(x) reinterpret_cast<EEUIBGenericBackground*>(x)
#define GBB(x) reinterpret_cast<GEBitmap*>(x)
#define MB(x) reinterpret_cast<EEUIBMapBackground*>(x)
static const GERectangle* const kGBCells[] = {&sEdgeCell, &sFrameCornerCell, &sCornerCell, &sSideCell};
// LoadBackground's locals
extern "C" void ee_screenscpp_gb_set_opacity(uintptr_t inBg, float inOpacity) { GB(inBg)->mOpacity = inOpacity; }
extern "C" void ee_screenscpp_gb_set_flags(uintptr_t inBg, uint32_t inFlags) { GB(inBg)->mFrameFlags = inFlags; }
extern "C" void ee_screenscpp_gb_set_color(uintptr_t inBg, uintptr_t inColor)
{ GB(inBg)->mColor = *reinterpret_cast<const GEColor*>(inColor); }
extern "C" void ee_screenscpp_gb_set_scale_x(uintptr_t inBg, float inScale) { GB(inBg)->mScaleX = inScale; }
extern "C" void ee_screenscpp_gb_set_scale_y(uintptr_t inBg, float inScale) { GB(inBg)->mScaleY = inScale; }
extern "C" uintptr_t ee_screenscpp_gb_rasterizer(uintptr_t inBg)
{ return (uintptr_t)GB(inBg)->GetForm()->GetManager()->GetRasterizer(); }
extern "C" uint32_t ee_screenscpp_gb_screen(uintptr_t inBg)
{ return (uint32_t)GB(inBg)->GetForm()->GetManager()->GetScreen(); }
extern "C" uintptr_t ee_screenscpp_gb_color(uintptr_t inBg) { return (uintptr_t)&GB(inBg)->mColor; }
extern "C" float ee_screenscpp_gb_opacity(uintptr_t inBg) { return GB(inBg)->mOpacity; }
extern "C" uintptr_t ee_screenscpp_gb_bitmap(uintptr_t inBg) { return (uintptr_t)GB(inBg)->GetBitmap(); }
extern "C" void ee_screenscpp_gb_stretch(uintptr_t inBitmap, float inT0, float inT1, float inT2, float inT3, float inD0, float inD1, float inD2, float inD3, uint32_t inCell, uint32_t inScreen)
{ GBB(inBitmap)->ComputeStretchedCoordsAndScale(GERectangle(inT0, inT1, inT2, inT3), GERectangle(inD0, inD1, inD2, inD3), *kGBCells[inCell], inScreen); }
extern "C" void ee_screenscpp_gb_coordinates_load(uintptr_t inBitmap)
{ GBB(inBitmap)->GetTextureCoordinatesPerVertex(EEScratch<GETextureCoordinate[4]>(kScratchGBCoordinates)[0], EEScratch<GETextureCoordinate[4]>(kScratchGBCoordinates)[1], EEScratch<GETextureCoordinate[4]>(kScratchGBCoordinates)[2], EEScratch<GETextureCoordinate[4]>(kScratchGBCoordinates)[3]); }
extern "C" void ee_screenscpp_gb_coordinates_set(uintptr_t inBitmap, uint32_t inA, uint32_t inB, uint32_t inC, uint32_t inD)
{ GBB(inBitmap)->SetTextureCoordinates(EEScratch<GETextureCoordinate[4]>(kScratchGBCoordinates)[inA], EEScratch<GETextureCoordinate[4]>(kScratchGBCoordinates)[inB], EEScratch<GETextureCoordinate[4]>(kScratchGBCoordinates)[inC], EEScratch<GETextureCoordinate[4]>(kScratchGBCoordinates)[inD]); }
extern "C" void ee_screenscpp_gb_swap_scale(uintptr_t inBitmap)
{ GBB(inBitmap)->SetXYScale(GBB(inBitmap)->GetYScale(), GBB(inBitmap)->GetXScale()); }
extern "C" void ee_screenscpp_gb_bitmap_at(uintptr_t inBitmap, float inX, float inY)
{ GBB(inBitmap)->SetLocation(GE2DPoint(inX, inY)); }
extern "C" int64_t ee_screenscpp_gb_draw(uintptr_t inBg, uintptr_t inBitmap, uint32_t inScreen)
{ return GB(inBg)->GetForm()->GetManager()->GetRasterizer()->Draw(GBB(inBitmap), inScreen); }
extern "C" void ee_screenscpp_gb_base_open(uintptr_t inBg) { GB(inBg)->UIBackground::Open(); }
extern "C" float ee_screenscpp_gb_window_width() { return (float)gGameWindow->GetWidth(); }
extern "C" float ee_screenscpp_gb_window_height() { return (float)gGameWindow->GetHeight(); }
extern "C" float ee_screenscpp_gb_designed_width(uintptr_t inBg)
{ return (float)GB(inBg)->GetForm()->GetDesignedWidth(); }
extern "C" float ee_screenscpp_gb_designed_height(uintptr_t inBg)
{ return (float)GB(inBg)->GetForm()->GetDesignedHeight(); }
extern "C" void ee_screenscpp_mb_file_clear() { EEScratch<FSFileSpec*>(kScratchMBFile) = NULL; }
extern "C" void ee_screenscpp_mb_image_clear() { EEScratch<GESurface*>(kScratchMBImage) = NULL; }
extern "C" uintptr_t ee_screenscpp_mb_secondary_surface()
{ return (uintptr_t)GERasterizer::GetActiveRasterizer()->GetSecondarySurface(gUIManager->GetScreen()); }
extern "C" uintptr_t ee_screenscpp_mb_screen()
{ return (uintptr_t)GERasterizer::GetActiveRasterizer()->GetScreen(gUIManager->GetScreen()); }
extern "C" void ee_screenscpp_mb_desc_reset() { EEScratch<GESurfaceDescription>(kScratchMBDescription) = GESurfaceDescription(); }
extern "C" void ee_screenscpp_mb_desc_pixel_format(uintptr_t inSurface)
{ EEScratch<GESurfaceDescription>(kScratchMBDescription).mPixelFormat = reinterpret_cast<GESurface*>(inSurface)->GetPixelFormat(); }
extern "C" void ee_screenscpp_mb_desc_pixel_depth(uintptr_t inSurface)
{ EEScratch<GESurfaceDescription>(kScratchMBDescription).mPixelDepth = reinterpret_cast<GESurface*>(inSurface)->GetPixelDepth(); }
extern "C" void ee_screenscpp_mb_desc_system_memory() { EEScratch<GESurfaceDescription>(kScratchMBDescription).mType = kSurfaceSystemMemory; }
extern "C" void ee_screenscpp_mb_desc_volatility(uint32_t inValue) { EEScratch<GESurfaceDescription>(kScratchMBDescription).mSurfaceVolatility = inValue; }
extern "C" void ee_screenscpp_mb_desc_m14(uint32_t inValue) { EEScratch<GESurfaceDescription>(kScratchMBDescription).m14 = inValue; }
extern "C" void ee_screenscpp_mb_desc_mip_levels(uint32_t inValue) { EEScratch<GESurfaceDescription>(kScratchMBDescription).mMipLevelCount = inValue; }
extern "C" void ee_screenscpp_mb_desc_3d(bool inValue) { EEScratch<GESurfaceDescription>(kScratchMBDescription).mIs3DRenderable = inValue; }
extern "C" void ee_screenscpp_mb_desc_transparency(bool inValue) { EEScratch<GESurfaceDescription>(kScratchMBDescription).mIsTransparencyEnabled = inValue; }
extern "C" int64_t ee_screenscpp_mb_open_save_game_file(uintptr_t inBg)
{ return EEOpenSaveGameFile(MB(inBg)->GetTextureFileName(), UString(gEmptyString), EEScratch<FSFileSpec*>(kScratchMBFile)); }
extern "C" void ee_screenscpp_mb_directory_reset() { EEScratch<FSFileSpec>(kScratchMBDirectory) = FSFileSpec(); }
extern "C" void ee_screenscpp_mb_directory_volume() { EEScratch<FSFileSpec>(kScratchMBDirectory).SetVolume(gStartupVolume); }
extern "C" void ee_screenscpp_mb_directory_directory() { EEScratch<FSFileSpec>(kScratchMBDirectory).SetDirectory(gStartupDirectory); }
extern "C" uintptr_t ee_screenscpp_mb_path_new() { return (uintptr_t)new FSPath; }
extern "C" void ee_screenscpp_mb_path_add(uintptr_t inPath)
{ reinterpret_cast<FSPath*>(inPath)->AddDirectory(EEScratch<FSFileSpec>(kScratchMBDirectory), true); }
extern "C" int64_t ee_screenscpp_mb_path_find(uintptr_t inPath, uintptr_t inBg)
{ return reinterpret_cast<FSPath*>(inPath)->FindFile(MB(inBg)->GetTextureFileName(), EEScratch<FSFileSpec*>(kScratchMBFile)); }
extern "C" void ee_screenscpp_mb_path_dereference(uintptr_t inPath)
{ reinterpret_cast<FSPath*>(inPath)->Dereference(); }
extern "C" bool ee_screenscpp_mb_is_png()
{ return PlatStrICmp(EEScratch<FSFileSpec*>(kScratchMBFile)->GetFile().GetSubstring(EEScratch<FSFileSpec*>(kScratchMBFile)->GetFile().GetLength() - 4, 4), ".png") == 0; }
extern "C" int64_t ee_screenscpp_mb_attributes_png()
{ return GESurface::GetImageFileAttributes(*EEScratch<FSFileSpec*>(kScratchMBFile), kImageFilePNG, EEScratch<GESurfaceDescription>(kScratchMBDescription).mWidth, EEScratch<GESurfaceDescription>(kScratchMBDescription).mHeight, EEScratch<unsigned long>(kScratchMBBits)); }
extern "C" int64_t ee_screenscpp_mb_attributes_jpg()
{ return GESurface::GetImageFileAttributes(*EEScratch<FSFileSpec*>(kScratchMBFile), kImageFileJPG, EEScratch<GESurfaceDescription>(kScratchMBDescription).mWidth, EEScratch<GESurfaceDescription>(kScratchMBDescription).mHeight, EEScratch<unsigned long>(kScratchMBBits)); }
extern "C" void ee_screenscpp_mb_create_image(uintptr_t inScreen)
{ reinterpret_cast<GEScreen*>(inScreen)->CreateSurface(EEScratch<GESurfaceDescription>(kScratchMBDescription), &EEScratch<GESurface*>(kScratchMBImage)); }
extern "C" int64_t ee_screenscpp_mb_load_png() { return EEScratch<GESurface*>(kScratchMBImage)->Load(*EEScratch<FSFileSpec*>(kScratchMBFile), kImageFilePNG); }
extern "C" int64_t ee_screenscpp_mb_load_jpg() { return EEScratch<GESurface*>(kScratchMBImage)->Load(*EEScratch<FSFileSpec*>(kScratchMBFile), kImageFileJPG); }
extern "C" uint32_t ee_screenscpp_mb_width() { return (uint32_t)EEScratch<GESurfaceDescription>(kScratchMBDescription).mWidth; }
extern "C" uint32_t ee_screenscpp_mb_height() { return (uint32_t)EEScratch<GESurfaceDescription>(kScratchMBDescription).mHeight; }
extern "C" void ee_screenscpp_mb_cell_desc() { EEScratch<GESurfaceDescription>(kScratchMBCellDescription) = EEScratch<GESurfaceDescription>(kScratchMBDescription); }
extern "C" void ee_screenscpp_mb_cell_texture() { EEScratch<GESurfaceDescription>(kScratchMBCellDescription).mType = kSurfaceTexture; }
extern "C" void ee_screenscpp_mb_cell_width(uint32_t inWidth) { EEScratch<GESurfaceDescription>(kScratchMBCellDescription).mWidth = inWidth; }
extern "C" void ee_screenscpp_mb_cell_height(uint32_t inHeight) { EEScratch<GESurfaceDescription>(kScratchMBCellDescription).mHeight = inHeight; }
extern "C" void ee_screenscpp_mb_columns_rows(uintptr_t inBg, uint32_t inColumns, uint32_t inRows)
{ MB(inBg)->SetNumberColumnsAndRows(inColumns, inRows); }
extern "C" void ee_screenscpp_mb_texture_description(uintptr_t inBg)
{ MB(inBg)->SetTextureDescription(GETextureDescription{EEScratch<GESurfaceDescription>(kScratchMBCellDescription).mSurfaceVolatility, 0, gGamePreferences.mSettings.mGameBitDepth, 0, EEScratch<GESurfaceDescription>(kScratchMBCellDescription).mIs3DRenderable, EEScratch<GESurfaceDescription>(kScratchMBCellDescription).mIsTransparencyEnabled, true, true}); }
extern "C" void ee_screenscpp_mb_texture_rectangle(uintptr_t inBg, float inRight, float inBottom)
{ MB(inBg)->SetTextureRectangle(GERectangle(0.0f, 0.0f, inRight, inBottom)); }
extern "C" int64_t ee_screenscpp_mb_create_cell(uintptr_t inScreen)
{ return reinterpret_cast<GEScreen*>(inScreen)->CreateSurface(EEScratch<GESurfaceDescription>(kScratchMBCellDescription), &EEScratch<GESurface*>(kScratchMBCell)); }
extern "C" void ee_screenscpp_mb_source(float inLeft, float inTop, float inRight, float inBottom)
{ EEScratch<GERectangle>(kScratchMBSource) = GERectangle(inLeft, inTop, inRight, inBottom); }
extern "C" void ee_screenscpp_mb_destination(float inWidth, float inHeight)
{ EEScratch<GERectangle>(kScratchMBDestination) = GERectangle(0.0f, 0.0f, inWidth, inHeight); }
extern "C" int64_t ee_screenscpp_mb_copy() { return EEScratch<GESurface*>(kScratchMBImage)->CopyTo(EEScratch<GESurface*>(kScratchMBCell), EEScratch<GERectangle>(kScratchMBSource), EEScratch<GERectangle>(kScratchMBDestination), false); }
extern "C" uintptr_t ee_screenscpp_mb_make_texture(uintptr_t inScreen)
{ return (uintptr_t)reinterpret_cast<GEScreen*>(inScreen)->GetTextureManager()->MakeTexture(EEScratch<GESurface*>(kScratchMBCell)); }
extern "C" void ee_screenscpp_mb_cell_texture_set(uintptr_t inBg, uintptr_t inTexture, uint32_t inColumn, uint32_t inRow)
{ MB(inBg)->SetBitmapTexture(reinterpret_cast<GETexture*>(inTexture), inColumn, inRow); }
extern "C" void ee_screenscpp_mb_cell_dereference() { EEScratch<GESurface*>(kScratchMBCell)->Dereference(); }
extern "C" bool ee_screenscpp_mb_image() { return EEScratch<GESurface*>(kScratchMBImage) != NULL; }
extern "C" void ee_screenscpp_mb_image_dereference() { EEScratch<GESurface*>(kScratchMBImage)->Dereference(); }
extern "C" bool ee_screenscpp_mb_file() { return EEScratch<FSFileSpec*>(kScratchMBFile) != NULL; }
extern "C" void ee_screenscpp_mb_file_delete() { delete EEScratch<FSFileSpec*>(kScratchMBFile); }
