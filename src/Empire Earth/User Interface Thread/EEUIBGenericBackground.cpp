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


// Creates a tinted form background with optional frame pieces (flags: 1 edges, 2 frame corners, 4 sides)
EEUIBGenericBackground::EEUIBGenericBackground(unsigned long inBackgroundID, const UString& inTextureFileName,
                                               const GETextureDescription& inDescription,
                                               const UILayoutUtility& inLayout, UIForm* inForm,
                                               unsigned long inDrawPriority, const GEColor& inColor, bool inVisible,
                                               float inOpacity, float inScaleX, float inScaleY, unsigned long inFlags)
    : UIBitmapBackground(inBackgroundID, inTextureFileName, inDescription, GERectangle(), inLayout, inForm,
                         inDrawPriority, inVisible)
{
    ee_screens_gb_construct((uintptr_t)this, inOpacity, (uint32_t)inFlags, (uintptr_t)&inColor, inScaleX, inScaleY);
}

// Destroys the form background
EEUIBGenericBackground::~EEUIBGenericBackground()
{
}

// Draws the tinted background inside the side insets, then the frame pieces its flags ask for, each clipped to the
// redraw rectangle (the original's DrawCorners, DrawEdges, DrawFrameCorners, DrawSides, DrawEdge and DrawCorner:
// ee_screens frame.rs over backgrounds_calls.rs' fill and pieces)
long EEUIBGenericBackground::DrawBackground(const GERectangle& inRectangle)
{
    return ee_screens_frame_draw(this, &this->mRectangle, this->mScaleX, this->mScaleY, this->mFrameFlags, &inRectangle);
}

// Opens the background and scales its frame to the window size against the form's designed size
void EEUIBGenericBackground::Open()
{
    ee_screens_gb_open((uintptr_t)this);   // (backgrounds_calls.rs)
}

// Draws the campaign map background
long EEUIBMapBackground::DrawBackground(const GERectangle& inRectangle)
{
    return UIBFull::DrawBackground(inRectangle);
}

// Loads the campaign map image (PNG or JPG; the original's were TGA or JPG) and cuts it into 256x256 textures, one
// per background cell
// The image comes from the saved game/scenario resources, else from the startup path (backgrounds_calls.rs)
long EEUIBMapBackground::LoadBackground()
{
    return (long)ee_screens_mb_load((uintptr_t)this);   // (backgrounds_calls.rs)
}

// ---- the one-statement shims of ee_screens backgrounds_calls.rs (UI lane round 8: the frame's fill and pieces,
// Open, the map's LoadBackground): each one call, no logic
#include "ee/EEScreensShims.h"
#define GB(x) reinterpret_cast<EEUIBGenericBackground*>(x)
#define GBB(x) reinterpret_cast<GEBitmap*>(x)
#define MB(x) reinterpret_cast<EEUIBMapBackground*>(x)
static const GERectangle* const kGBCells[] = {&sEdgeCell, &sFrameCornerCell, &sCornerCell, &sSideCell};
static GETextureCoordinate sGBCoordinates[4];   // a piece's coordinates (the original's local)
// LoadBackground's locals
static FSFileSpec* sMBFile;
static FSFileSpec sMBDirectory;
static GESurface* sMBImage;
static GESurface* sMBCell;
static GESurfaceDescription sMBDescription;
static GESurfaceDescription sMBCellDescription;
static unsigned long sMBBits;
static GERectangle sMBSource, sMBDestination;   // a cell's copy
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
{ GBB(inBitmap)->GetTextureCoordinatesPerVertex(sGBCoordinates[0], sGBCoordinates[1], sGBCoordinates[2], sGBCoordinates[3]); }
extern "C" void ee_screenscpp_gb_coordinates_set(uintptr_t inBitmap, uint32_t inA, uint32_t inB, uint32_t inC, uint32_t inD)
{ GBB(inBitmap)->SetTextureCoordinates(sGBCoordinates[inA], sGBCoordinates[inB], sGBCoordinates[inC], sGBCoordinates[inD]); }
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
extern "C" void ee_screenscpp_mb_file_clear() { sMBFile = NULL; }
extern "C" void ee_screenscpp_mb_image_clear() { sMBImage = NULL; }
extern "C" uintptr_t ee_screenscpp_mb_secondary_surface()
{ return (uintptr_t)GERasterizer::GetActiveRasterizer()->GetSecondarySurface(gUIManager->GetScreen()); }
extern "C" uintptr_t ee_screenscpp_mb_screen()
{ return (uintptr_t)GERasterizer::GetActiveRasterizer()->GetScreen(gUIManager->GetScreen()); }
extern "C" void ee_screenscpp_mb_desc_reset() { sMBDescription = GESurfaceDescription(); }
extern "C" void ee_screenscpp_mb_desc_pixel_format(uintptr_t inSurface)
{ sMBDescription.mPixelFormat = reinterpret_cast<GESurface*>(inSurface)->GetPixelFormat(); }
extern "C" void ee_screenscpp_mb_desc_pixel_depth(uintptr_t inSurface)
{ sMBDescription.mPixelDepth = reinterpret_cast<GESurface*>(inSurface)->GetPixelDepth(); }
extern "C" void ee_screenscpp_mb_desc_system_memory() { sMBDescription.mType = kSurfaceSystemMemory; }
extern "C" void ee_screenscpp_mb_desc_volatility(uint32_t inValue) { sMBDescription.mSurfaceVolatility = inValue; }
extern "C" void ee_screenscpp_mb_desc_m14(uint32_t inValue) { sMBDescription.m14 = inValue; }
extern "C" void ee_screenscpp_mb_desc_mip_levels(uint32_t inValue) { sMBDescription.mMipLevelCount = inValue; }
extern "C" void ee_screenscpp_mb_desc_3d(bool inValue) { sMBDescription.mIs3DRenderable = inValue; }
extern "C" void ee_screenscpp_mb_desc_transparency(bool inValue) { sMBDescription.mIsTransparencyEnabled = inValue; }
extern "C" int64_t ee_screenscpp_mb_open_save_game_file(uintptr_t inBg)
{ return EEOpenSaveGameFile(MB(inBg)->GetTextureFileName(), UString(gEmptyString), sMBFile); }
extern "C" void ee_screenscpp_mb_directory_reset() { sMBDirectory = FSFileSpec(); }
extern "C" void ee_screenscpp_mb_directory_volume() { sMBDirectory.SetVolume(gStartupVolume); }
extern "C" void ee_screenscpp_mb_directory_directory() { sMBDirectory.SetDirectory(gStartupDirectory); }
extern "C" uintptr_t ee_screenscpp_mb_path_new() { return (uintptr_t)new FSPath; }
extern "C" void ee_screenscpp_mb_path_add(uintptr_t inPath)
{ reinterpret_cast<FSPath*>(inPath)->AddDirectory(sMBDirectory, true); }
extern "C" int64_t ee_screenscpp_mb_path_find(uintptr_t inPath, uintptr_t inBg)
{ return reinterpret_cast<FSPath*>(inPath)->FindFile(MB(inBg)->GetTextureFileName(), sMBFile); }
extern "C" void ee_screenscpp_mb_path_dereference(uintptr_t inPath)
{ reinterpret_cast<FSPath*>(inPath)->Dereference(); }
extern "C" bool ee_screenscpp_mb_is_png()
{ return PlatStrICmp(sMBFile->GetFile().GetSubstring(sMBFile->GetFile().GetLength() - 4, 4), ".png") == 0; }
extern "C" int64_t ee_screenscpp_mb_attributes_png()
{ return GESurface::GetImageFileAttributes(*sMBFile, kImageFilePNG, sMBDescription.mWidth, sMBDescription.mHeight, sMBBits); }
extern "C" int64_t ee_screenscpp_mb_attributes_jpg()
{ return GESurface::GetImageFileAttributes(*sMBFile, kImageFileJPG, sMBDescription.mWidth, sMBDescription.mHeight, sMBBits); }
extern "C" void ee_screenscpp_mb_create_image(uintptr_t inScreen)
{ reinterpret_cast<GEScreen*>(inScreen)->CreateSurface(sMBDescription, &sMBImage); }
extern "C" int64_t ee_screenscpp_mb_load_png() { return sMBImage->Load(*sMBFile, kImageFilePNG); }
extern "C" int64_t ee_screenscpp_mb_load_jpg() { return sMBImage->Load(*sMBFile, kImageFileJPG); }
extern "C" uint32_t ee_screenscpp_mb_width() { return (uint32_t)sMBDescription.mWidth; }
extern "C" uint32_t ee_screenscpp_mb_height() { return (uint32_t)sMBDescription.mHeight; }
extern "C" void ee_screenscpp_mb_cell_desc() { sMBCellDescription = sMBDescription; }
extern "C" void ee_screenscpp_mb_cell_texture() { sMBCellDescription.mType = kSurfaceTexture; }
extern "C" void ee_screenscpp_mb_cell_width(uint32_t inWidth) { sMBCellDescription.mWidth = inWidth; }
extern "C" void ee_screenscpp_mb_cell_height(uint32_t inHeight) { sMBCellDescription.mHeight = inHeight; }
extern "C" void ee_screenscpp_mb_columns_rows(uintptr_t inBg, uint32_t inColumns, uint32_t inRows)
{ MB(inBg)->SetNumberColumnsAndRows(inColumns, inRows); }
extern "C" void ee_screenscpp_mb_texture_description(uintptr_t inBg)
{ MB(inBg)->SetTextureDescription(GETextureDescription{sMBCellDescription.mSurfaceVolatility, 0, gGamePreferences.mSettings.mGameBitDepth, 0, sMBCellDescription.mIs3DRenderable, sMBCellDescription.mIsTransparencyEnabled, true, true}); }
extern "C" void ee_screenscpp_mb_texture_rectangle(uintptr_t inBg, float inRight, float inBottom)
{ MB(inBg)->SetTextureRectangle(GERectangle(0.0f, 0.0f, inRight, inBottom)); }
extern "C" int64_t ee_screenscpp_mb_create_cell(uintptr_t inScreen)
{ return reinterpret_cast<GEScreen*>(inScreen)->CreateSurface(sMBCellDescription, &sMBCell); }
extern "C" void ee_screenscpp_mb_source(float inLeft, float inTop, float inRight, float inBottom)
{ sMBSource = GERectangle(inLeft, inTop, inRight, inBottom); }
extern "C" void ee_screenscpp_mb_destination(float inWidth, float inHeight)
{ sMBDestination = GERectangle(0.0f, 0.0f, inWidth, inHeight); }
extern "C" int64_t ee_screenscpp_mb_copy() { return sMBImage->CopyTo(sMBCell, sMBSource, sMBDestination, false); }
extern "C" uintptr_t ee_screenscpp_mb_make_texture(uintptr_t inScreen)
{ return (uintptr_t)reinterpret_cast<GEScreen*>(inScreen)->GetTextureManager()->MakeTexture(sMBCell); }
extern "C" void ee_screenscpp_mb_cell_texture_set(uintptr_t inBg, uintptr_t inTexture, uint32_t inColumn, uint32_t inRow)
{ MB(inBg)->SetBitmapTexture(reinterpret_cast<GETexture*>(inTexture), inColumn, inRow); }
extern "C" void ee_screenscpp_mb_cell_dereference() { sMBCell->Dereference(); }
extern "C" bool ee_screenscpp_mb_image() { return sMBImage != NULL; }
extern "C" void ee_screenscpp_mb_image_dereference() { sMBImage->Dereference(); }
extern "C" bool ee_screenscpp_mb_file() { return sMBFile != NULL; }
extern "C" void ee_screenscpp_mb_file_delete() { delete sMBFile; }
