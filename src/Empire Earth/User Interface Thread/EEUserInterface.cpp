// C:\EEX\Empire Earth\User Interface Thread: the UI thread's units in one (LLE/UI round 2).
// Unit flags: the exe module set.
//
// port: the original's User Interface Thread units were one compiled file each (link/exe/units.txt's ranges
// 0x254911..0x27aa0b; EEFOFindNewTargetNearOldTarget.cpp, 0x23dd54..0x2544e9, stays its own unit), and the port added
// two hosts without an address (EEUIRustControl.cpp, EEUIShell.cpp). Each is a section here, headed by its path and
// keeping its comment (its address range, what is whole in Rust); a reference elsewhere to one of them
// (EEUIFMainGame.cpp's shims, EEUIShell.cpp's shells, ...) names its section. The includes are one list.
#include <windows.h>
#include <locale.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEScreen.h"
#include "lle/GERasterizer.h"
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
#include "ee/EEScreensShims.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUIFMainGame.h"
#include "lle/SESoundManager.h"
#include "lle/UIButton.h"
#include "lle/UResourceFile.h"
#include "lle/UITextLabel.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIBUpgrade.h"
#include "lle/UIBitmap.h"
#include "ee/EEUIFMGStatusBar.h"
#include "lle/GETexture.h"
#include "lle/GEViewport.h"
#include "lle/GEModel.h"
#include "lle/GEModelManager.h"
#include "lle/GEMaterial.h"
#include "ee/EEUICCommandUnits.h"
#include "lle/UIFontTable.h"
#include "ee/EEButtonObject.h"
#include "ee/EEUICPortrait.h"
#include "ee/EEStringIDs.h"
#include "ee/EEMouse.h"
#include "ee/EEUnitPicker.h"
#include "ee/EEUICSelectUnits.h"
#include "ee/EEUIShell.h"
#include "lle/Input System.h"
#include "lle/UIEntityGroup.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "ee/EEUIFMGMainGroup.h"
#include "lle/UITLScrolling.h"
#include "ee/EEMap.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEGhostUnit.h"
#include "lle/NESynchronousServer.h"
#include "ee/EEServer.h"
#include "ee/EEPhysics.h"
#include "lle/USingleton.h"
#include "ee/EECalamityManager.h"
#include "ee/EECalamityClass.h"
#include "ee/EEScreenEvent.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEUIFMGUnitHack.h"
#include "ee/EEScenarioManager.h"
#include "ee/EECombatGoals.h"
#include "ee/EEUIFMGAttribute.h"
#include "lle/UIHotKey.h"
#include "ee/EEUnitBehavior.h"
#include "ee/EEFormation.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEUIFMGStatusBox.h"
#include "ee/EEUIFMGStatusButton.h"
#include "lle/UIStatusBar.h"
#include "lle/UIControlsTable.h"
#include "ee/EECPEngine.h"
#include "ee/EECBUIListBox.h"
#include "ee/EEUIEWonNetEvent.h"
#include "lle/UIBackgroundTable.h"
#include "ee/EEUIMiniMap.h"
#include "ee/EEUICPing.h"
#include "ee/EEUICWonNetRoomsBox.h"
#include "ee/EEUICWonNetGamesBox.h"
#include "ee/EEUICWonNetStatBox.h"
#include "lle/UIGraph.h"
#include "ee/EEGameState.h"   // gGameState
#include "platform/PlatSim.h"
#include "lle/NESynchronousClient.h"
#include "ee/EEClient.h"
#include "ee/EERenderObject.h"
#include "ee/EEROUserInterface.h"
#include "ee/EEDataPreLoading.h"
#include "ee/EEMusicManager.h"
#include "ee/EEAmbientSoundManager.h"
#include "ee/EEPathManager.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEArtificialIntelligence.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEMiniMap.h"
#include "ee/EEPSaveGameHeader.h"
#include "ee/EESaveLoad.h"
#include "ee/RTSMousePointer.h"
#include "lle/UIFormManager.h"
#include "ee/EEUISCIFormManager.h"
#include "ee/EEWonNetAvailableGameFormManager.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "ee/EESEARust.h"

// ==== EEUIBGenericBackground.cpp ====================================================================================
// C:\EEX\Empire Earth\User Interface Thread\EEUIBGenericBackground.cpp (file name inferred: no __FILE__; RTTI class
// EEUIBGenericBackground). The tinted, framed form background: 0x254911..0x255c2a (the four GERectangle `$E` chains
// of the frame's texture cells, ctor, ??_G, dtor, the frame pieces, DrawBackground, Open, then 0x2556e6;
// EEUIBColor ends at 0x254911, EEUIBSavedGroup starts at 0x255c2a). Unit flags: the exe module set.
// the frame's cells in its 64x64 texture (provisional names)
static GERectangle sEdgeCell(57.0f, 0.0f, 64.0f, 64.0f);           // 0x564558 (the edges)
static GERectangle sFrameCornerCell(0.0f, 0.0f, 28.0f, 38.0f);     // 0x564548 (the large corners)
static GERectangle sCornerCell(49.0f, 0.0f, 56.0f, 7.0f);          // 0x564538 (the small corners)
static GERectangle sSideCell(30.0f, 0.0f, 48.0f, 20.0f);           // 0x564528 (the side ornaments)

// ---- the one-statement shims of ee_screens backgrounds_calls.rs (UI lane round 8: the frame's fill and pieces,
// Open, the map's LoadBackground): each one call, no logic
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
#undef GB
#undef GBB
#undef MB

// ==== EEUIBuildGate.cpp =============================================================================================
// C:\EEX\Empire Earth\User Interface Thread\EEUIBuildGate.cpp (file name inferred: no __FILE__; RTTI class
// EEUIBuildGate). The gate placement mode: 0x25619b..0x25643d (the members in alphabetical order; ctor and ??_G are
// inline in the header, emitted by the scenario editor form's unit; EEUIBuildWall follows at 0x25643d). Unit flags:
// the exe module set.
// cdecl helpers (the units range 0x21048f..0x216d57; types inferred) (provisional names)
void EEClearGateHighlights(EEPlayer* inPlayer, long inUnitType);  // 0x21176e: restores the colour of inPlayer's walls
void EEHighlightGateWalls(EEPlayer* inPlayer, long inUnitType);  // 0x211957: highlights them

// the gate placement's world calls (placement_ffi.rs: Initialize, Terminate, ClearGhostUnits)
extern "C" void ee_screenscpp_pl_gate_highlight(int32_t inPlayer, int32_t inWallType)
{ EEHighlightGateWalls(gWorld.GetPlayer(inPlayer), inWallType); }
extern "C" void ee_screenscpp_pl_gate_unhighlight(int32_t inPlayer, int32_t inWallType)
{ EEClearGateHighlights(gWorld.GetPlayer(inPlayer), inWallType); }
extern "C" void ee_screenscpp_pl_ghosts_enter() { gWorld.mGhostUnitsLock->Enter(); }
extern "C" void ee_screenscpp_pl_ghosts_leave() { gWorld.mGhostUnitsLock->Leave(); }
extern "C" void ee_screenscpp_pl_ghosts_clear() { gWorld.ClearGhostUnits(); }
extern "C" void ee_screenscpp_pl_gate_load_media(int32_t inPlayer, EEUnitClass* inClass)
{ gWorld.GetPlayer(inPlayer)->SetUnitClassMedia(inClass->mUnitType, 0x7f); }
extern "C" void ee_screenscpp_pl_gate_unload_media(int32_t inPlayer, EEUnitClass* inClass)
{ gWorld.GetPlayer(inPlayer)->UnloadUnitClassMedia(inClass->mUnitType); }

// the main game form's Build (0x25619b; its Cancel: EEUIBuildWall.cpp) (placement_ffi.rs)
extern "C" void ee_screenscpp_pl_main_build_gate(float inX, float inY, int32_t inDirection)
{ static_cast<EEABuild*>(gUIManager->GetMainGameForm()->mBuild)->BuildGate(GE2DPoint(inX, inY), (long)inDirection); }

// ==== EEUIBuildWall.cpp =============================================================================================
// C:\EEX\Empire Earth\User Interface Thread\EEUIBuildWall.cpp (file name inferred: no __FILE__; RTTI class
// EEUIBuildWall). The wall placement mode: 0x25643d..0x256845 (the members in alphabetical order; Cancel is ICF-folded
// into EEUIBuildGate's 0x2561b6; ctor and ??_G are inline in the header, emitted by the scenario editor form's unit;
// EEUIBUpgrade follows at 0x256845). Unit flags: the exe module set.
// the placements' world calls (placement_ffi.rs: Initialize, Terminate, the gate's ClearGhostUnits)
extern "C" void ee_screenscpp_pl_wall_load_media(int32_t inPlayer, int32_t inTechnology)
{ gWorld.GetPlayer(inPlayer)->SetUnitClassMedia(gWorld.GetPlayer(inPlayer)->mTechnologies->Get(inTechnology)->mData->mUnitClass, 0x7f); }
extern "C" void ee_screenscpp_pl_wall_unload_media(int32_t inPlayer, int32_t inTechnology)
{ gWorld.GetPlayer(inPlayer)->UnloadUnitClassMedia(gWorld.GetPlayer(inPlayer)->GetTechTree()->Get(inTechnology)->mData->mUnitClass); }
extern "C" void ee_screenscpp_pl_unlock_cursor() { gUIManager->GetMainGameForm()->mIsCursorLocked = false; }

// the main game form's Cancel (0x2561b6, ICF with EEUIBuildGate's) and Build (0x25643d) (placement_ffi.rs)
extern "C" void ee_screenscpp_pl_main_cancel() { gUIManager->GetMainGameForm()->SetUIState(0); }
extern "C" void ee_screenscpp_pl_main_build_wall(float inStartX, float inStartY, float inEndX, float inEndY)
{ static_cast<EEABuild*>(gUIManager->GetMainGameForm()->mBuild)->BuildWall(GE2DPoint(inStartX, inStartY), GE2DPoint(inEndX, inEndY)); }

// ==== EEUIBUpgrade.cpp ==============================================================================================
// C:\EEX\Empire Earth\User Interface Thread\EEUIBUpgrade.cpp (file name inferred: no __FILE__; RTTI class EEUIBUpgrade).
// The in-game improvement button: 0x256845..0x257223 (ctor, the in-class mouse forwarders, ??_G, then the out-of-line
// members in alphabetical order; EEUICChatLine's ctor at 0x257223). Unit flags: the exe module set.
// The button's host (upgrade.rs EE_SCREENS_UP_OP_*): units (as EEUnit*) and nodes as handles (the selection's reads
// are hud_views.rs')
// the host's one-statement ops (upgrade_ops.rs; EEUIBUpgrade::HostOp's switch until UI lane round 16), on the call
// record's button, the unit (a) and the button's improvement
#define UPX_CALL (*static_cast<EEUIBUpgradeCall*>(reinterpret_cast<void*>(inCall)))
#define UPX_BUTTON static_cast<EEUIBUpgrade*>(UPX_CALL.mButton)
#define UPX_UNIT reinterpret_cast<EEUnit*>(inA)
extern "C" int64_t ee_screenscpp_upx_allowed(uintptr_t inCall, int64_t inA, int64_t inB) { return (int64_t)(gWorld.mUnitImprovementsAllowed == true); }
extern "C" int64_t ee_screenscpp_upx_can_buy(uintptr_t inCall, int64_t inA, int64_t inB) { return (int64_t)(UPX_UNIT->mPlayer->CanBuy(UPX_UNIT->mUnitClass->GetUnitType(), UPX_BUTTON->mAttribute, static_cast<UWideString*>(UPX_CALL.mOut))); }
extern "C" int64_t ee_screenscpp_upx_editor_unit(uintptr_t inCall, int64_t inA, int64_t inB) { return (int64_t)((int64_t) static_cast<EEUIFScenarioEditor*>(UPX_BUTTON->GetForm())->GetFirstSelectedUnit()); }
extern "C" int64_t ee_screenscpp_upx_can_raise(uintptr_t inCall, int64_t inA, int64_t inB) { return (int64_t)(UPX_UNIT->mStats->mImprovement.CanUpgrade(UPX_UNIT->mStats->mUnitClass, UPX_BUTTON->mAttribute) == true); }
extern "C" int64_t ee_screenscpp_upx_maxed(uintptr_t inCall, int64_t inA, int64_t inB) { return (int64_t)(UPX_UNIT->mPlayer->IsImprovementMaxed(UPX_UNIT->mUnitClass->GetUnitType(), UPX_BUTTON->mAttribute)); }
extern "C" int64_t ee_screenscpp_upx_level(uintptr_t inCall, int64_t inA, int64_t inB) { return (int64_t)(UPX_UNIT->GetPlayer()->GetImprovementLevel(UPX_UNIT->mUnitClass->GetUnitType(), UPX_BUTTON->mAttribute)); }
extern "C" void ee_screenscpp_upx_text_name(uintptr_t inCall, int64_t inA, int64_t inB) { UPX_UNIT->GetStats()->GetImprovementName(UPX_CALL.mText, UPX_BUTTON->mAttribute); }
extern "C" void ee_screenscpp_upx_text_gain(uintptr_t inCall, int64_t inA, int64_t inB) { UPX_UNIT->GetStats()->GetImprovementGainText(UPX_CALL.mText, UPX_BUTTON->mAttribute, (long)inB); }
extern "C" void ee_screenscpp_upx_text_string(uintptr_t inCall, int64_t inA, int64_t inB) { gResource->GetString((long)inA, UPX_CALL.mText); }
extern "C" void ee_screenscpp_upx_out_append(uintptr_t inCall, int64_t inA, int64_t inB) { *static_cast<UWideString*>(UPX_CALL.mOut) += UPX_CALL.mText; }
extern "C" void ee_screenscpp_upx_out_set(uintptr_t inCall, int64_t inA, int64_t inB) { *static_cast<UWideString*>(UPX_CALL.mOut) = UPX_CALL.mText; }
extern "C" void ee_screenscpp_upx_out_bytes(uintptr_t inCall, int64_t inA, int64_t inB) { *static_cast<UWideString*>(UPX_CALL.mOut) += std::string(reinterpret_cast<const char*>(inA), (size_t)inB).c_str(); }
extern "C" void ee_screenscpp_upx_base_help(uintptr_t inCall, int64_t inA, int64_t inB) { UPX_BUTTON->UIControl::GetHelpString(inA == 0 ? *static_cast<UWideString*>(UPX_CALL.mOut) : UPX_CALL.mText); }
extern "C" void ee_screenscpp_upx_upgrade(uintptr_t inCall, int64_t inA, int64_t inB) { (new EEAUpgradeImprovement(UPX_UNIT, UPX_BUTTON->mAttribute, inB != 0, !UPX_BUTTON->mScenarioEditor))->Dereference(); }
extern "C" int64_t ee_screenscpp_upx_out_length(uintptr_t inCall, int64_t inA, int64_t inB) { return (int64_t)(static_cast<UWideString*>(UPX_CALL.mOut)->GetLength()); }
extern "C" int64_t ee_screenscpp_upx_refusal_interval(uintptr_t inCall, int64_t inA, int64_t inB) { return (int64_t)(gWorldTable.GetLongValue(kWorldRefusalMessageInterval)); }
extern "C" int64_t ee_screenscpp_upx_time(uintptr_t inCall, int64_t inA, int64_t inB) { return (int64_t)(TMGetTime()); }
extern "C" void ee_screenscpp_upx_chat(uintptr_t inCall, int64_t inA, int64_t inB) { gUserInterface->AddChatLogEntry(*static_cast<UWideString*>(UPX_CALL.mOut), 0); }
extern "C" void ee_screenscpp_upx_update_help(uintptr_t inCall, int64_t inA, int64_t inB) { UPX_BUTTON->GetForm()->UpdateHelpString(); }
extern "C" int64_t ee_screenscpp_upx_pressed(uintptr_t inCall, int64_t inA, int64_t inB) { return (int64_t)(UPX_BUTTON->GetState() == kButtonStatePressed); }
extern "C" void ee_screenscpp_upx_base_button_up(uintptr_t inCall, int64_t inA, int64_t inB) { UPX_BUTTON->UIButton::ButtonUp(); }
extern "C" int64_t ee_screenscpp_upx_base_move(uintptr_t inCall, int64_t inA, int64_t inB) { return (int64_t)(UPX_BUTTON->UIButton::MouseMove(*static_cast<const GE2DPoint*>(UPX_CALL.mLocation))); }
extern "C" void ee_screenscpp_upx_help_text(uintptr_t inCall, int64_t inA, int64_t inB) { UPX_BUTTON->GetHelpString(UPX_CALL.mText); }
extern "C" int64_t ee_screenscpp_upx_contains(uintptr_t inCall, int64_t inA, int64_t inB) { return (int64_t)(UPX_BUTTON->GetRectangle().Contains(UPX_BUTTON->GetMouseLocation())); }
extern "C" void ee_screenscpp_upx_release_mouse(uintptr_t inCall, int64_t inA, int64_t inB) { UPX_BUTTON->ReleaseCapturedMouse(); }
extern "C" int64_t ee_screenscpp_upx_cost_differs(uintptr_t inCall, int64_t inA, int64_t inB) { return (int64_t)(static_cast<UITextLabel*>(UPX_BUTTON->mCostLabel) != NULL && *static_cast<UWideString*>(UPX_CALL.mOut) != static_cast<UITextLabel*>(UPX_BUTTON->mCostLabel)->GetText(0)); }
extern "C" void ee_screenscpp_upx_cost_set(uintptr_t inCall, int64_t inA, int64_t inB) { static_cast<UITextLabel*>(UPX_BUTTON->mCostLabel)->SetText(*static_cast<UWideString*>(UPX_CALL.mOut)); }
extern "C" void ee_screenscpp_upx_dirty(uintptr_t inCall, int64_t inA, int64_t inB) { UPX_BUTTON->SetDirty(true); }
extern "C" void ee_screenscpp_upx_border_thickness(uintptr_t inCall, int64_t inA, int64_t inB) { UPX_BUTTON->SetBorderThickness((unsigned long)inA); }
extern "C" void ee_screenscpp_upx_bar_set_pressed(uintptr_t inCall, int64_t inA, int64_t inB) { static_cast<EEUIFMGStatusBar*>(UPX_BUTTON)->mPressed = inA != 0; }
extern "C" int64_t ee_screenscpp_upx_bar_base_l_down(uintptr_t inCall) { return UPX_BUTTON->UIButton::MouseLButtonDown(); }
extern "C" int64_t ee_screenscpp_upx_bar_base_l_up(uintptr_t inCall) { return UPX_BUTTON->UIButton::MouseLButtonUp(); }
extern "C" int64_t ee_screenscpp_upx_bar_base_r_down(uintptr_t inCall) { return UPX_BUTTON->EEUIBUpgrade::MouseRButtonDown(); }
#undef UPX_CALL
#undef UPX_BUTTON
#undef UPX_UNIT

// ---- the host's multi-statement cases (UI lane round 9: ee_game screens upgrade_calls.rs), each one statement on
// the call record's button
#define UPC(inCall) (*reinterpret_cast<EEUIBUpgradeCall*>(inCall))
#define UPB(inCall) static_cast<EEUIBUpgrade*>(UPC(inCall).mButton)
static const unsigned long kUpSounds[] = {kSoundButton2, kSoundButton1};
extern "C" int64_t ee_screenscpp_up_attribute(uintptr_t inCall) { return (int64_t)UPB(inCall)->mAttribute; }
extern "C" void ee_screenscpp_up_set_attribute(uintptr_t inCall, int64_t inValue)
{ UPB(inCall)->mAttribute = (long)inValue; }
extern "C" int64_t ee_screenscpp_up_level(uintptr_t inCall) { return (int64_t)UPB(inCall)->mLevel; }
extern "C" void ee_screenscpp_up_set_level(uintptr_t inCall, int64_t inValue) { UPB(inCall)->mLevel = (long)inValue; }
extern "C" int64_t ee_screenscpp_up_last_message(uintptr_t inCall)
{ return (int64_t)(long)UPB(inCall)->mLastMessageTime; }
extern "C" void ee_screenscpp_up_set_last_message(uintptr_t inCall, int64_t inValue)
{ UPB(inCall)->mLastMessageTime = (unsigned long)(long)inValue; }
extern "C" bool ee_screenscpp_up_editor(uintptr_t inCall) { return UPB(inCall)->mScenarioEditor; }
extern "C" bool ee_screenscpp_up_disabled(uintptr_t inCall) { return UPB(inCall)->mDisabled; }
extern "C" void ee_screenscpp_up_set_disabled(uintptr_t inCall, bool inValue) { UPB(inCall)->mDisabled = inValue; }
extern "C" uintptr_t ee_screenscpp_up_editor_complex(uintptr_t inCall)
{ return (uintptr_t)static_cast<EEUnit*>(static_cast<EEUIFScenarioEditor*>(UPB(inCall)->GetForm())->GetFirstSelectedUnit()->GetComplexUnit()); }
extern "C" float ee_screenscpp_up_step(uintptr_t inCall, uintptr_t inUnit)
{ return reinterpret_cast<EEUnit*>(inUnit)->mUnitClass->mImprovements->mFactors[UPB(inCall)->mAttribute].mOffset; }
extern "C" float ee_screenscpp_up_cost_factor(uintptr_t inCall, uintptr_t inUnit)
{ return reinterpret_cast<EEUnit*>(inUnit)->mUnitClass->mImprovements->mFactors[UPB(inCall)->mAttribute].mScale; }
extern "C" int64_t ee_screenscpp_up_unit_type(uintptr_t inUnit)
{ return (int64_t)reinterpret_cast<EEUnit*>(inUnit)->mUnitClass->GetUnitType(); }
extern "C" bool ee_screenscpp_up_class_exists(int64_t inUnitType) { return gUnitClassTable[(long)inUnitType] != NULL; }
extern "C" int64_t ee_screenscpp_up_class_technology(int64_t inUnitType)
{ return (int64_t)gUnitClassTable[(long)inUnitType]->GetTechnology(); }
extern "C" uintptr_t ee_screenscpp_up_node(int64_t inTechnology)
{ return (uintptr_t)gWorld.GetCurrentPlayer()->GetTechTree()->Get((long)inTechnology); }
extern "C" void ee_screenscpp_up_text_cost(uintptr_t inCall, uintptr_t inNode, uintptr_t inUnit, float inFactor)
{ reinterpret_cast<EETechTreeNode*>(inNode)->GetCostString(UPC(inCall).mText, reinterpret_cast<EEUnit*>(inUnit)->mUnitClass->GetUnitType(), inFactor); }
extern "C" void ee_screenscpp_up_sound(uint32_t inWhich)
{ gSoundManager->Play(EESoundObject::Get(kUpSounds[inWhich])->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL); }
extern "C" uintptr_t ee_screenscpp_up_help_label(uintptr_t inCall)
{ return (uintptr_t)UPB(inCall)->GetForm()->GetHelpTextLabel(); }
extern "C" void ee_screenscpp_up_help_label_set(uintptr_t inLabel, uintptr_t inCall)
{ reinterpret_cast<UITextLabel*>(inLabel)->SetText(UPC(inCall).mText); }
extern "C" void ee_screenscpp_up_bg_opacity(uintptr_t inCall, float inOpacity) { UPB(inCall)->SetBgOpacity(inOpacity); }
extern "C" void ee_screenscpp_up_border_opacity(uintptr_t inCall, float inOpacity)
{ UPB(inCall)->SetBorderOpacity(inOpacity); }
extern "C" bool ee_screenscpp_up_bar_enabled(uintptr_t inCall)
{ return static_cast<EEUIFMGStatusBar*>(UPB(inCall))->mEnabled; }
extern "C" bool ee_screenscpp_up_bar_pressed(uintptr_t inCall)
{ return static_cast<EEUIFMGStatusBar*>(UPB(inCall))->mPressed; }
extern "C" bool ee_screenscpp_up_has_capture(uintptr_t inCall) { return UPB(inCall)->GetHasMouseCapture(); }
extern "C" bool ee_screenscpp_up_inside_form_mouse(uintptr_t inCall)
{ return UPB(inCall)->GetRectangle().Contains(UPB(inCall)->GetForm()->GetMouseLocation()); }
extern "C" bool ee_screenscpp_up_inside_location(uintptr_t inCall)
{ return UPB(inCall)->GetRectangle().Contains(*static_cast<const GE2DPoint*>(UPC(inCall).mLocation)); }
extern "C" float ee_screenscpp_up_icon_left(uintptr_t inCall)
{ return static_cast<UIBitmap*>(static_cast<EEUIFMGStatusBar*>(UPB(inCall))->mIcon)->GetRectangle().GetTopLeft().GetX(); }
extern "C" float ee_screenscpp_up_icon_top(uintptr_t inCall)
{ return static_cast<UIBitmap*>(static_cast<EEUIFMGStatusBar*>(UPB(inCall))->mIcon)->GetRectangle().GetTopLeft().GetY(); }
extern "C" void ee_screenscpp_up_icon_set_location(uintptr_t inCall, float inX, float inY)
{ static_cast<UIBitmap*>(static_cast<EEUIFMGStatusBar*>(UPB(inCall))->mIcon)->SetLocation(GE2DPoint(inX, inY)); }
// the constructor's body and the draw, dirty, mouse and press overrides: hud_bodies.rs over these (UI lane round 12)
#define UPBB(b) ((EEUIBUpgrade*)(b))
extern "C" void ee_screenscpp_upb_set_num_queued(uintptr_t inButton, int64_t inValue)
{ UPBB(inButton)->mNumQueued = (long)inValue; }
extern "C" void ee_screenscpp_upb_set_level(uintptr_t inButton, int64_t inValue)
{ UPBB(inButton)->mLevel = (long)inValue; }
extern "C" void ee_screenscpp_upb_set_last_message(uintptr_t inButton, uint64_t inValue)
{ UPBB(inButton)->mLastMessageTime = (unsigned long)inValue; }
extern "C" void ee_screenscpp_upb_set_cost_label(uintptr_t inButton, uintptr_t inLabel)
{ UPBB(inButton)->mCostLabel = (UITextLabel*)inLabel; }
extern "C" void ee_screenscpp_upb_bg_opacity(uintptr_t inButton, float inOpacity)
{ UPBB(inButton)->SetBgOpacity(inOpacity); }
extern "C" void ee_screenscpp_upb_border_opacity(uintptr_t inButton, float inOpacity)
{ UPBB(inButton)->SetBorderOpacity(inOpacity); }
extern "C" void ee_screenscpp_upb_border_thickness(uintptr_t inButton, uint32_t inThickness)
{ UPBB(inButton)->SetBorderThickness(inThickness); }
extern "C" void ee_screenscpp_upb_set_disabled(uintptr_t inButton, bool inValue)
{ UPBB(inButton)->mDisabled = inValue; }
extern "C" bool ee_screenscpp_upb_disabled(uintptr_t inButton) { return UPBB(inButton)->GetDisabled(); }
extern "C" void ee_screenscpp_upb_base_button_down(uintptr_t inButton) { EEUIBUpgrade::BaseButtonDown(UPBB(inButton)); }
extern "C" void ee_screenscpp_upb_update_help(uintptr_t inButton) { UPBB(inButton)->GetForm()->UpdateHelpString(); }
extern "C" int64_t ee_screenscpp_upb_base_copy_data(uintptr_t inButton)
{ return (int64_t)UPBB(inButton)->UIButton::CopyData(); }
extern "C" uintptr_t ee_screenscpp_upb_cost_label(uintptr_t inButton) { return (uintptr_t)static_cast<UITextLabel*>(UPBB(inButton)->mCostLabel); }
extern "C" void ee_screenscpp_upb_label_dirty_rect(uintptr_t inButton, uintptr_t inLabel)
{ UPBB(inButton)->GetForm()->GetManager()->AddDirtyRectangle(((UITextLabel*)inLabel)->GetRectangle(), UPBB(inButton)->GetForm()); }
extern "C" int64_t ee_screenscpp_upb_label_copy_data(uintptr_t inLabel)
{ return (int64_t)((UITextLabel*)inLabel)->CopyData(); }
extern "C" int64_t ee_screenscpp_upb_base_draw_text(uintptr_t inButton)
{ return (int64_t)UPBB(inButton)->UIButton::DrawTextA(); }
extern "C" int64_t ee_screenscpp_upb_label_draw_text(uintptr_t inLabel)
{ return (int64_t)((UITextLabel*)inLabel)->DrawTextA(); }
extern "C" bool ee_screenscpp_upb_can_upgrade(uintptr_t inButton, uintptr_t* inUnit)
{ return UPBB(inButton)->GetCanUpgrade(*reinterpret_cast<EEUnit**>(inUnit), NULL); }
extern "C" void ee_screenscpp_upb_base_mouse_over(uintptr_t inButton) { EEUIBUpgrade::BaseMouseOver(UPBB(inButton)); }
extern "C" void ee_screenscpp_upb_capture(uintptr_t inButton) { EEUIBUpgrade::CaptureOf(UPBB(inButton)); }
extern "C" void ee_screenscpp_upb_button_down(uintptr_t inButton) { EEUIBUpgrade::ButtonDownOf(UPBB(inButton)); }
extern "C" void ee_screenscpp_upb_label_set_dirty(uintptr_t inLabel) { ((UITextLabel*)inLabel)->SetDirty(true); }
extern "C" void ee_screenscpp_upb_base_set_dirty(uintptr_t inButton, bool inDirty)
{ UPBB(inButton)->UIControl::SetDirty(inDirty); }
#undef UPC
#undef UPB
#undef UPBB

// ==== EEUICCommandUnits.cpp =========================================================================================
// C:\EEX\Empire Earth\User Interface Thread\EEUICCommandUnits.cpp (file name inferred: no __FILE__; RTTI class
// EEUICCommandUnits). The main game form's right-drag command control: 0x257881..0x258464 (ctor, ??_G, dtor, then
// the members in alphabetical order; EEUICPing's ctor at 0x258464). Unit flags: the exe module set.

// Creates the main game view's right-drag command control, hidden, with its facing arrow: a textured quad model
// (CreateModel) and the "Unit Facing Arrow" texture, clamped and alpha-blended (LoadTexture; command_units_calls.rs)
EEUICCommandUnits::EEUICCommandUnits(UIForm* inForm, unsigned long inId, unsigned long inIndex,
                                     EEAMouseClick* inListener)
    : EEUIRustControl(inId, GERectangle(gViewport.GetViewRectangle().GetTopLeft(), 2.0f, 2.0f), inForm, inIndex, 0),
      EeScreensCommandUnitsFields(), EeScreensCommandUnitsObjects()
{
    this->Rust(EE_SCREENS_CU_DO_INIT);
    this->StoreDisplayMode(kDisplayModeHidden);
    ee_screens_cu_construct((uintptr_t)this, (uintptr_t)inListener);   // (command_units_calls.rs)
}

// The class's own ops (the arrow's draw, the ground point, the pick, the group's centre, the click: the old RustOp)
// are command_units_ops.rs' over the one-statement shims below (UI lane round 11)
#define CU(c) static_cast<EEUICCommandUnits*>((EEUIRustControl*)(c))

extern "C" uint32_t ee_screenscpp_cu_screen() { return (uint32_t)gScreenIndex; }
extern "C" void ee_screenscpp_cu_save_ambient_color() { EEScratch<GEColor>(kScratchCUAmbientColor) = gRasterizer->GetAmbientColor(); }
extern "C" void ee_screenscpp_cu_save_viewport() { EEScratch<GEViewport>(kScratchCUViewport) = gRasterizer->GetViewport(gScreenIndex); }
extern "C" float ee_screenscpp_cu_ambient_intensity() { return gRasterizer->GetAmbientIntensity(); }
extern "C" uint64_t ee_screenscpp_cu_lighting(uint32_t inScreen)
{ return (uint64_t)gRasterizer->GetLighting(inScreen); }
extern "C" uint64_t ee_screenscpp_cu_zbuffer(uint32_t inScreen) { return (uint64_t)gRasterizer->GetZBuffer(inScreen); }
extern "C" uint64_t ee_screenscpp_cu_zcompare(uint32_t inScreen)
{ return (uint64_t)gRasterizer->GetZCompare(inScreen); }
extern "C" void ee_screenscpp_cu_depth_range(float inNear, float inFar) { gU4E8F08.SetDepthRange(inNear, inFar); }
extern "C" void ee_screenscpp_cu_set_viewport_view(uint32_t inScreen) { gRasterizer->SetViewport(gU4E8F08, inScreen); }
extern "C" void ee_screenscpp_cu_set_zbuffer_test_only(uint32_t inScreen)
{ gRasterizer->SetZBuffer(kZBufferTestOnly, inScreen); }
extern "C" void ee_screenscpp_cu_set_zcompare_less(uint32_t inScreen)
{ gRasterizer->SetZCompare(kCompareLess, inScreen); }
extern "C" void ee_screenscpp_cu_set_ambient_white(uint32_t inScreen)
{ gRasterizer->SetAmbientColor(GEColor(1.0f, 1.0f, 1.0f), inScreen); }
extern "C" void ee_screenscpp_cu_set_ambient_intensity(float inV, uint32_t inScreen)
{ gRasterizer->SetAmbientIntensity(inV, inScreen); }
extern "C" void ee_screenscpp_cu_set_lighting(uint64_t inV, uint32_t inScreen)
{ gRasterizer->SetLighting((unsigned long)inV, inScreen); }
extern "C" void ee_screenscpp_cu_set_zbuffer(uint64_t inV, uint32_t inScreen)
{ gRasterizer->SetZBuffer((unsigned long)inV, inScreen); }
extern "C" void ee_screenscpp_cu_set_zcompare(uint64_t inV, uint32_t inScreen)
{ gRasterizer->SetZCompare((unsigned long)inV, inScreen); }
extern "C" void ee_screenscpp_cu_restore_viewport(uint32_t inScreen)
{ gRasterizer->SetViewport(EEScratch<GEViewport>(kScratchCUViewport), inScreen); }
extern "C" void ee_screenscpp_cu_restore_ambient_color(uint32_t inScreen)
{ gRasterizer->SetAmbientColor(EEScratch<GEColor>(kScratchCUAmbientColor), inScreen); }
extern "C" void ee_screenscpp_cu_transform_reset() { EEScratch<GETransformation>(kScratchCUTransformation) = GETransformation(); }
extern "C" void ee_screenscpp_cu_transform_yaw(float inYaw) { EEScratch<GETransformation>(kScratchCUTransformation).SetOrientationYPR(inYaw, 0.0f, 0.0f); }
extern "C" void ee_screenscpp_cu_transform_position(float inX, float inY, float inZ)
{ EEScratch<GETransformation>(kScratchCUTransformation).SetPosition(inX, inY, inZ); }
extern "C" void ee_screenscpp_cu_model_frame(uintptr_t inCtl) { static_cast<GEModel*>(CU(inCtl)->mModel)->SetGraphicsCurrentFrame(0); }
extern "C" float ee_screenscpp_cu_camera_zoom() { return gWorld.mCameraControl.mZoom; }
extern "C" void ee_screenscpp_cu_model_scale(uintptr_t inCtl, float inV) { static_cast<GEModel*>(CU(inCtl)->mModel)->SetGraphicsScale(inV); }
extern "C" void ee_screenscpp_cu_model_transform(uintptr_t inCtl)
{ static_cast<GEModel*>(CU(inCtl)->mModel)->SetGraphicsPostModelTransform(EEScratch<GETransformation>(kScratchCUTransformation)); }
extern "C" void ee_screenscpp_cu_model_prepare(uintptr_t inCtl) { static_cast<GEModel*>(CU(inCtl)->mModel)->PrepareTransforms(); }
extern "C" void ee_screenscpp_cu_model_draw(uintptr_t inCtl, uint32_t inScreen)
{ gRasterizer->Draw(static_cast<GEModel*>(CU(inCtl)->mModel), inScreen, 1.0f, gOrthographicView); }
extern "C" float ee_screenscpp_cu_half_width() { return gU4E8F08.GetHalfWidth(); }
extern "C" float ee_screenscpp_cu_x_center() { return gU4E8F08.GetXCenter(); }
extern "C" float ee_screenscpp_cu_y_center() { return gU4E8F08.GetYCenter(); }
extern "C" bool ee_screenscpp_cu_ortho() { return gOrthographicView; }
extern "C" void ee_screenscpp_cu_near(float inX, float inY, float inZ) { EEScratch<GE3DPoint>(kScratchCUNear) = GE3DPoint(inX, inY, inZ); }
extern "C" void ee_screenscpp_cu_far(float inX, float inY, float inZ) { EEScratch<GE3DPoint>(kScratchCUFar) = GE3DPoint(inX, inY, inZ); }
extern "C" void ee_screenscpp_cu_denorm_persp_near()
{ gU4E8F08.DenormalizePerspectivePoint(EEScratch<GE3DPoint>(kScratchCUNear), gU4E8F08.GetFrontClippingDistance(), EEScratch<GE3DPoint>(kScratchCUNearWorld)); }
extern "C" void ee_screenscpp_cu_denorm_persp_far()
{ gU4E8F08.DenormalizePerspectivePoint(EEScratch<GE3DPoint>(kScratchCUFar), gU4E8F08.GetRearClippingDistance(), EEScratch<GE3DPoint>(kScratchCUFarWorld)); }
extern "C" void ee_screenscpp_cu_denorm_ortho_near() { gU4E8F08.DenormalizeOrthographicPoint(EEScratch<GE3DPoint>(kScratchCUNear), EEScratch<GE3DPoint>(kScratchCUNearWorld)); }
extern "C" void ee_screenscpp_cu_denorm_ortho_far() { gU4E8F08.DenormalizeOrthographicPoint(EEScratch<GE3DPoint>(kScratchCUFar), EEScratch<GE3DPoint>(kScratchCUFarWorld)); }
extern "C" void ee_screenscpp_cu_line_define() { EEScratch<GE3DLine>(kScratchCULine).Define(EEScratch<GE3DPoint>(kScratchCUNearWorld), EEScratch<GE3DPoint>(kScratchCUFarWorld)); }
extern "C" void ee_screenscpp_cu_line_solve(float inZ) { EEScratch<GE3DLine>(kScratchCULine).SolveForZ(inZ, EEScratch<float>(kScratchCUT), EEScratch<GE3DPoint>(kScratchCUTarget)); }
extern "C" float ee_screenscpp_cu_target_x() { return EEScratch<GE3DPoint>(kScratchCUTarget).GetX(); }
extern "C" float ee_screenscpp_cu_target_y() { return EEScratch<GE3DPoint>(kScratchCUTarget).GetY(); }
extern "C" float ee_screenscpp_cu_target_z() { return EEScratch<GE3DPoint>(kScratchCUTarget).GetZ(); }
extern "C" void ee_screenscpp_cu_point_set(float inX, float inY, float inZ) { EEScratch<GE3DPoint>(kScratchCUTarget) = GE3DPoint(inX, inY, inZ); }
extern "C" bool ee_screenscpp_cu_point_at(float inX, float inY)
{ return EEPointUnitOrTile(GE2DPoint(inX, inY), EEScratch<EETile*>(kScratchCUTile), EEScratch<GE3DPoint>(kScratchCUTarget), true); }
extern "C" void ee_screenscpp_cu_bounds_reset() { EEScratch<GERectangle>(kScratchCUBounds) = GERectangle(); }
extern "C" void ee_screenscpp_cu_group_bounds() { gWorld.GetCurrentPlayer()->mSelectedGroup->GetBounds(EEScratch<GERectangle>(kScratchCUBounds)); }
extern "C" void ee_screenscpp_cu_bounds_center() { EEScratch<GERectangle>(kScratchCUBounds).GetCenter(EEScratch<GE2DPoint>(kScratchCUCenter)); }
extern "C" float ee_screenscpp_cu_center_x() { return EEScratch<GE2DPoint>(kScratchCUCenter).GetX(); }
extern "C" float ee_screenscpp_cu_center_y() { return EEScratch<GE2DPoint>(kScratchCUCenter).GetY(); }
extern "C" void ee_screenscpp_cu_execute(uintptr_t inCtl, float inX, float inY, bool inHit, uint64_t inModifiers, bool inFacing, float inAngle, bool inDragged)
{ static_cast<EEAMouseClick*>(CU(inCtl)->mListener)->Execute(inX, inY, inHit, (unsigned long)inModifiers, inFacing, inAngle, inDragged); }

// ---- the one-statement shims of ee_screens command_units_calls.rs (UI lane round 8: the constructor's arrow, the
// destructor): each one call, no logic
#define CU(x) reinterpret_cast<EEUICCommandUnits*>(x)
extern "C" void ee_screenscpp_cu_set_listener(uintptr_t inCu, uintptr_t inListener)
{ CU(inCu)->mListener = reinterpret_cast<EEAMouseClick*>(inListener); }
extern "C" void ee_screenscpp_cu_listener_reference(uintptr_t inCu) { static_cast<EEAMouseClick*>(CU(inCu)->mListener)->Reference(); }
extern "C" void ee_screenscpp_cu_always_dirty(uintptr_t inCu) { CU(inCu)->SetAlwaysDirty(true); }
extern "C" void ee_screenscpp_cu_model_new(uintptr_t inCu)
{ CU(inCu)->mModel = gModelManager->CreateModel(2, 4, 1, 0, 1, 0, "Unit Direction Model", 1); }
extern "C" void ee_screenscpp_cu_model_reference(uintptr_t inCu) { static_cast<GEModel*>(CU(inCu)->mModel)->Reference(); }
extern "C" void ee_screenscpp_cu_point(uintptr_t inCu, uint32_t inIndex, float inX, float inY, float inZ)
{ static_cast<GEModel*>(CU(inCu)->mModel)->GetVertexArray(0)->SetPoint(GE3DPoint(inX, inY, inZ), inIndex); }
extern "C" void ee_screenscpp_cu_normal(uintptr_t inCu, uint32_t inIndex)
{ static_cast<GEModel*>(CU(inCu)->mModel)->GetVertexArray(0)->SetNormal(GEVector(0.0f, 0.0f, 1.0f), inIndex); }
extern "C" void ee_screenscpp_cu_coordinate(uintptr_t inCu, uint32_t inIndex, float inU, float inV)
{ static_cast<GEModel*>(CU(inCu)->mModel)->GetVertexArray(0)->SetTextureCoordinate(GETextureCoordinate(inU, inV), inIndex); }
extern "C" void ee_screenscpp_cu_vertex1(uintptr_t inCu, uint32_t inPolygon, uint32_t inVertex)
{ static_cast<GEModel*>(CU(inCu)->mModel)->GetPolygonArray(0)->SetVertexIndex1(inPolygon, inVertex); }
extern "C" void ee_screenscpp_cu_vertex2(uintptr_t inCu, uint32_t inPolygon, uint32_t inVertex)
{ static_cast<GEModel*>(CU(inCu)->mModel)->GetPolygonArray(0)->SetVertexIndex2(inPolygon, inVertex); }
extern "C" void ee_screenscpp_cu_vertex3(uintptr_t inCu, uint32_t inPolygon, uint32_t inVertex)
{ static_cast<GEModel*>(CU(inCu)->mModel)->GetPolygonArray(0)->SetVertexIndex3(inPolygon, inVertex); }
extern "C" void ee_screenscpp_cu_polygon_start(uintptr_t inCu)
{ static_cast<GEModel*>(CU(inCu)->mModel)->GetMaterial(0)->SetPolygonStartingIndex(0, 0); }
extern "C" void ee_screenscpp_cu_polygon_length(uintptr_t inCu)
{ static_cast<GEModel*>(CU(inCu)->mModel)->GetMaterial(0)->SetPolygonRangeLength(2, 0); }
extern "C" void ee_screenscpp_cu_vertex_start(uintptr_t inCu)
{ static_cast<GEModel*>(CU(inCu)->mModel)->GetMaterial(0)->SetVertexStartingIndex(0); }
extern "C" void ee_screenscpp_cu_vertex_length(uintptr_t inCu)
{ static_cast<GEModel*>(CU(inCu)->mModel)->GetMaterial(0)->SetVertexRangeLength(4); }
extern "C" void ee_screenscpp_cu_bounds(uintptr_t inCu) { static_cast<GEModel*>(CU(inCu)->mModel)->ComputeBoundingVolumes(); }
extern "C" void ee_screenscpp_cu_set_description()
{ EEScratch<GETextureDescription>(kScratchCUDescription) = GETextureDescription{1, 0, gTexturePixelDepth, 0, false, false, true, true}; }
extern "C" uintptr_t ee_screenscpp_cu_description() { return (uintptr_t)&EEScratch<GETextureDescription>(kScratchCUDescription); }
extern "C" void ee_screenscpp_cu_set_texture(uintptr_t inCu, uintptr_t inTexture)
{ CU(inCu)->mTexture = reinterpret_cast<GETexture*>(inTexture); }
extern "C" void ee_screenscpp_texture_alpha_texture(uintptr_t inTexture)
{ reinterpret_cast<GETexture*>(inTexture)->SetStageAlphaMode(kStageOpTexture, -1); }
extern "C" void ee_screenscpp_texture_color_texture(uintptr_t inTexture)
{ reinterpret_cast<GETexture*>(inTexture)->SetStageColorMode(kStageOpTexture, -1); }
extern "C" void ee_screenscpp_cu_material_texture(uintptr_t inCu)
{ static_cast<GEModel*>(CU(inCu)->mModel)->GetMaterial(0)->SetTexture(static_cast<GETexture*>(CU(inCu)->mTexture), gScreenIndex); }
extern "C" void ee_screenscpp_cu_listener_dereference(uintptr_t inCu) { static_cast<EEAMouseClick*>(CU(inCu)->mListener)->Dereference(); }
extern "C" void ee_screenscpp_cu_model_dereference(uintptr_t inCu) { static_cast<GEModel*>(CU(inCu)->mModel)->Dereference(); }
extern "C" void ee_screenscpp_cu_texture_dereference(uintptr_t inCu) { static_cast<GETexture*>(CU(inCu)->mTexture)->Dereference(); }
#undef CU

// ==== EEUICPortrait.cpp =============================================================================================
// C:\EEX\Empire Earth\User Interface Thread\EEUICPortrait.cpp (file name inferred: no __FILE__; RTTI class
// EEUICPortrait). The main game form's selected-unit portrait: 0x25861c..0x25952f (ctor, ??_G, dtor, then the members
// in alphabetical order with EEPlayer's two inline lookups after their first users; EEUICSelectUnits' init chains at
// 0x25952f). Unit flags: the exe module set.
// Creates the selected-unit portrait with its research (green) and improvement (yellow) progress bars over a
// red background, and a level text

// the tooltip being built (the UI thread's)

extern "C" void ee_screenscpp_pt_help_target(UWideString* inOut) { EEScratch<UWideString*>(kScratchPortraitHelp) = inOut; }
extern "C" void ee_screenscpp_pt_help_clear() { *EEScratch<UWideString*>(kScratchPortraitHelp) = UWideString(); }

// The portrait's own ops (portrait.rs EE_SCREENS_PT_OP_*: units and nodes as handles), its constructor's,
// destructor's, DrawTextA's and MouseMove's bodies: portrait_calls.rs over the one-statement shims below (UI lane
// round 11)
#define PT(p) ((EEUICPortrait*)(p))
#define PT_CU(u) ((EEComplexUnit*)(u))
#define PT_N(n) ((EETechTreeNode*)(n))

extern "C" bool ee_screenscpp_pt_has_unit(uintptr_t inP) { return PT(inP)->mUnit != NULL; }
extern "C" void ee_screenscpp_pt_button_release(uintptr_t inP) { static_cast<EEButtonObject*>(PT(inP)->mButton)->Release(); }
extern "C" void ee_screenscpp_pt_unit_release(uintptr_t inP) { PT(inP)->mUnit->Dereference(); }
extern "C" void ee_screenscpp_pt_set_unit(uintptr_t inP, uintptr_t inUnit)
{ PT(inP)->mUnit = reinterpret_cast<EEUnit*>(inUnit); }
extern "C" void ee_screenscpp_pt_unit_acquire(uintptr_t inP) { PT(inP)->mUnit->Reference(); }
extern "C" void ee_screenscpp_pt_button_take(uintptr_t inP) { PT(inP)->mButton = static_cast<EEButtonObject*>(PT(inP)->mUnit->mStats->mButton); }
extern "C" void ee_screenscpp_pt_button_acquire(uintptr_t inP) { static_cast<EEButtonObject*>(PT(inP)->mButton)->Acquire(); }
extern "C" void ee_screenscpp_pt_face_texture(uintptr_t inP)
{ static_cast<GEBitmap*>(PT(inP)->mPortrait)->SetTexture(static_cast<EEButtonObject*>(PT(inP)->mButton)->GetTexture(), gScreenIndex); }
extern "C" void ee_screenscpp_pt_face_bitmap(uintptr_t inP)
{ static_cast<EEButtonObject*>(PT(inP)->mButton)->SetFaceBitmap(static_cast<GEBitmap*>(PT(inP)->mPortrait), PT(inP)->GetRectangle(), gScreenIndex); }
extern "C" uint64_t ee_screenscpp_pt_level(uintptr_t inP) { return (uint64_t)PT(inP)->mLevel; }
extern "C" bool ee_screenscpp_pt_first_bar(uintptr_t inP) { return PT(inP)->mShowFirstBar; }
extern "C" bool ee_screenscpp_pt_second_bar(uintptr_t inP) { return PT(inP)->mShowSecondBar; }
extern "C" void ee_screenscpp_pt_set_level(uintptr_t inP, uint64_t inV) { PT(inP)->mLevel = (unsigned long)inV; }
extern "C" void ee_screenscpp_pt_set_first_bar(uintptr_t inP, bool inV) { PT(inP)->mShowFirstBar = inV; }
extern "C" void ee_screenscpp_pt_set_second_bar(uintptr_t inP, bool inV) { PT(inP)->mShowSecondBar = inV; }
extern "C" void ee_screenscpp_pt_set_show_level(uintptr_t inP, bool inV) { PT(inP)->mShowLevel = inV; }
extern "C" uintptr_t ee_screenscpp_pt_unit_complex(uintptr_t inP)
{ return (uintptr_t)PT(inP)->mUnit->GetComplexUnit(); }
extern "C" int64_t ee_screenscpp_pt_researched(uintptr_t inUnit)
{ return PT_CU(inUnit)->GetPlayer()->GetResearchedImprovement(PT_CU(inUnit)->GetUnitClass()->GetUnitType()); }
extern "C" uintptr_t ee_screenscpp_pt_node(int64_t inId)
{ return (uintptr_t)gWorld.GetCurrentPlayer()->GetTechTree()->Get((long)inId); }
extern "C" bool ee_screenscpp_pt_node_makes_unit(uintptr_t inNode) { return PT_N(inNode)->mData->mMakesUnit; }
extern "C" uintptr_t ee_screenscpp_pt_node_upgrade(uintptr_t inP, uintptr_t inNode)
{ return (uintptr_t)PT(inP)->mUnit->GetPlayer()->mUnitClassUpgrades[PT_N(inNode)->mData->mUnitClass]; }
extern "C" float ee_screenscpp_pt_upgrade_build_scale(uintptr_t inUp)
{ return ((EEUnitClassUpgrade*)inUp)->mBuildTimeScale; }
extern "C" int64_t ee_screenscpp_pt_node_build_time(uintptr_t inNode) { return PT_N(inNode)->GetBuildTime(); }
extern "C" int64_t ee_screenscpp_pt_improvement_level(uintptr_t inUnit, int64_t inKind)
{ return PT_CU(inUnit)->GetStats()->mImprovement.GetLevel((long)inKind); }
extern "C" int64_t ee_screenscpp_pt_improvement_time(int64_t inKind)
{ return gWorldTable.GetLongValue(EEWorldImprovementTime((long)inKind)); }
extern "C" int64_t ee_screenscpp_pt_improvement_time2(int64_t inKind)
{ return gWorldTable.GetLongValue2(EEWorldImprovementTime((long)inKind)); }
extern "C" float ee_screenscpp_pt_progress(uintptr_t inUnit)
{ return PT_CU(inUnit)->GetPlayer()->GetImprovementProgress(PT_CU(inUnit)->GetUnitClass()->GetUnitType()); }
extern "C" int64_t ee_screenscpp_pt_total_cost(uintptr_t inUnit)
{ return PT_CU(inUnit)->GetStats()->mImprovement.mTotalCost; }
extern "C" int64_t ee_screenscpp_pt_points_max() { return gWorldTable.GetLongValue(kWorldUnitImprovementPointsMax); }
extern "C" void ee_screenscpp_pt_text(uintptr_t inP, EeScreensShimText inText)
{ static_cast<GEText*>(PT(inP)->mText)->SetText(EEShimText(inText)); }
extern "C" void ee_screenscpp_pt_update_help(uintptr_t inP) { PT(inP)->GetForm()->UpdateHelpString(); }
extern "C" bool ee_screenscpp_pt_contains(uintptr_t inP, float inLeft, float inTop, float inRight, float inBottom)
{ return GERectangle(inLeft, inTop, inRight, inBottom).Contains(PT(inP)->GetMouseLocation()); }
extern "C" bool ee_screenscpp_pt_unit_building(uintptr_t inP) { return PT(inP)->mUnit->GetUnitClass()->IsBuilding(); }
extern "C" void ee_screenscpp_pt_strings_clear() { EEScratch<UWideString>(kScratchPTFormat) = UWideString(); }
extern "C" void ee_screenscpp_pt_name_clear() { EEScratch<UWideString>(kScratchPTName) = UWideString(); }
extern "C" void ee_screenscpp_pt_format_research()
{ gResource->GetString(IDS_LEFT_RIGHT_CLICK_TO_CANCEL_RESEARCH, EEScratch<UWideString>(kScratchPTFormat)); }
extern "C" void ee_screenscpp_pt_format_improvement()
{ gResource->GetString(IDS_LEFT_RIGHT_CLICK_TO_CANCEL_IMPROVEMENT, EEScratch<UWideString>(kScratchPTFormat)); }
extern "C" void ee_screenscpp_pt_name_unit_class(uintptr_t inNode)
{ gResource->GetString(gUnitClassTable[PT_N(inNode)->mData->mUnitClass]->mNameStringID, EEScratch<UWideString>(kScratchPTName)); }
extern "C" void ee_screenscpp_pt_name_node(uintptr_t inNode)
{ gResource->GetString(PT_N(inNode)->mData->mNameID, EEScratch<UWideString>(kScratchPTName)); }
extern "C" void ee_screenscpp_pt_name_improvement(uintptr_t inP)
{ PT(inP)->mUnit->mStats->GetImprovementName(EEScratch<UWideString>(kScratchPTName), PT(inP)->mUnit->GetPlayer()->GetResearchedImprovement(PT(inP)->mUnit->GetUnitClass()->GetUnitType())); }
extern "C" void ee_screenscpp_pt_help_print() { sprintf(*EEScratch<UWideString*>(kScratchPortraitHelp), EEScratch<UWideString>(kScratchPTFormat), (const char*)EEScratch<UWideString>(kScratchPTName)); }
extern "C" bool ee_screenscpp_pt_unit_hp_positive(uintptr_t inP) { return PT(inP)->mUnit->mHitPoints > 0; }
extern "C" bool ee_screenscpp_pt_unit_on_tile(uintptr_t inP) { return PT(inP)->mUnit->mTileNode != NULL; }
extern "C" uintptr_t ee_screenscpp_pt_unit_complex_field(uintptr_t inP)
{ return (uintptr_t)(EEComplexUnit*)PT(inP)->mUnit->mComplexUnit; }
extern "C" void ee_screenscpp_pt_sound()
{ gSoundManager->Play(EESoundObject::Get(kSoundButton1)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL); }
extern "C" void ee_screenscpp_pt_new_bitmap(uintptr_t inP) { PT(inP)->mPortrait = new GEBitmap; }
extern "C" void ee_screenscpp_pt_bitmap_location(uintptr_t inP, float inX, float inY)
{ static_cast<GEBitmap*>(PT(inP)->mPortrait)->SetLocation(GE2DPoint(inX, inY)); }
extern "C" void ee_screenscpp_pt_draw_init(uintptr_t inP, float* inRect)
{ ee_screens_portrait_draw_init(PT(inP), static_cast<GEBitmap*>(PT(inP)->mPortrait), inRect); }
extern "C" void ee_screenscpp_pt_new_text(uintptr_t inP, float inHeight)
{ PT(inP)->mText = new GEText(UIFontTable::CreateClosestFont(0x36, inHeight)); }
extern "C" void ee_screenscpp_pt_text_align_right(uintptr_t inP) { static_cast<GEText*>(PT(inP)->mText)->SetAlignment(kTextAlignRight); }
extern "C" void ee_screenscpp_pt_text_transparent(uintptr_t inP) { static_cast<GEText*>(PT(inP)->mText)->SetIsTransparent(true); }
extern "C" void ee_screenscpp_pt_text_color(uintptr_t inP) { static_cast<GEText*>(PT(inP)->mText)->SetFgColor(GEColor()); }
extern "C" void ee_screenscpp_pt_text_location(uintptr_t inP, float inX, float inY)
{ static_cast<GEText*>(PT(inP)->mText)->SetLocation(GE2DPoint(inX, inY)); }
extern "C" void ee_screenscpp_pt_delete_text(uintptr_t inP) { delete static_cast<GEText*>(PT(inP)->mText); }
extern "C" void ee_screenscpp_pt_bitmap_release(uintptr_t inP) { static_cast<GEBitmap*>(PT(inP)->mPortrait)->Dereference(); }
extern "C" void ee_screenscpp_pt_draw_text_base(uintptr_t inP) { PT(inP)->UIControl::DrawTextA(); }
extern "C" int32_t ee_screenscpp_pt_draw_level(uintptr_t inP)
{ return PT(inP)->GetForm()->DrawClippedText(PT(inP)->GetCopiedDrawRectangle(), static_cast<GEText*>(PT(inP)->mText), PT(inP)); }
#undef PT
#undef PT_CU
#undef PT_N

// ==== EEUICSelectUnits.cpp ==========================================================================================
// C:\EEX\Empire Earth\User Interface Thread\EEUICSelectUnits.cpp (file name inferred: no __FILE__; RTTI class
// EEUICSelectUnits). The main game form's left-drag selection control: 0x25952f..0x259f7f (the two GEColor `$E`
// chains, ctor, ??_G, dtor, then the members in alphabetical order; EEUICUnitIconBar's ctor at 0x259f7f). Unit flags:
// the exe module set.
// Creates the main game view's drag-selection box (its eight lines: ee_screens hud_draw.rs), hidden
EEUICSelectUnits::EEUICSelectUnits(UIForm* inForm, unsigned long inId, unsigned long inIndex, EEAMouseClick* inClick,
                                   EEALasso* inLasso)
    : EEUIRustControl(inId, GERectangle(GE2DPoint(gViewport.GetViewRectangle().GetTopLeft()), 2.0f, 2.0f), inForm, inIndex, 0),
      EeScreensSelectUnitsFields(), EeScreensSelectUnitsObjects{inClick, inLasso}
{
    ee_screens_su_construct(this);   // (hud_ctl_ffi.rs: hidden, the handlers referenced, always dirty)
}

// the constructor's and destructor's calls (hud_ctl_ffi.rs)
extern "C" void ee_screenscpp_su_hide(EEUICSelectUnits* inBox) { EEUICSelectUnits::Hide(inBox); }
extern "C" void ee_screenscpp_su_click_reference(EEUICSelectUnits* inBox) { static_cast<EEAMouseClick*>(inBox->mClick)->Reference(); }
extern "C" void ee_screenscpp_su_lasso_reference(EEUICSelectUnits* inBox) { static_cast<EEALasso*>(inBox->mLasso)->Reference(); }
extern "C" void ee_screenscpp_su_always_dirty(EEUICSelectUnits* inBox) { inBox->SetAlwaysDirty(true); }
extern "C" void ee_screenscpp_su_click_release(EEUICSelectUnits* inBox) { static_cast<EEAMouseClick*>(inBox->mClick)->Dereference(); }
extern "C" void ee_screenscpp_su_lasso_release(EEUICSelectUnits* inBox) { static_cast<EEALasso*>(inBox->mLasso)->Dereference(); }

// the class's ops (select_units_ops.rs; its RustOp until UI lane round 16)
#define SUC(x) reinterpret_cast<EEUICSelectUnits*>(x)
extern "C" uintptr_t ee_screenscpp_su_pick() { return (uintptr_t)gUnitPicker.PickUnit(gMouse->mPosition, 0); }
extern "C" void ee_screenscpp_su_lasso_type(uintptr_t inBox, uintptr_t inUnit, int64_t inMode)
{ static_cast<EEALasso*>(SUC(inBox)->mLasso)->SelectUnits(gU4E8F08.GetViewRectangle(), (long)inMode, reinterpret_cast<EEUnit*>(inUnit)->GetUnitClass()->mUnitType, reinterpret_cast<EEUnit*>(inUnit)->mUnitID); }
extern "C" void ee_screenscpp_su_unit_release(uintptr_t inUnit) { reinterpret_cast<EEUnit*>(inUnit)->Dereference(); }
extern "C" void ee_screenscpp_su_lasso_box(uintptr_t inBox, float inA, float inB, float inC, float inD, int64_t inMode)
{ static_cast<EEALasso*>(SUC(inBox)->mLasso)->SelectUnits(GERectangle(inA, inB, inC, inD), (long)inMode, kAnyUnitType, 0); }
extern "C" void ee_screenscpp_su_click(uintptr_t inBox, uint32_t inMode) { static_cast<EEAMouseClick*>(SUC(inBox)->mClick)->SelectAtMouse((unsigned long)inMode); }
// the trigger tab's (EESEUICTriggerSelectUnits): the point under the mouse into the unit hack's scratch point
extern "C" void ee_screenscpp_su_point_at(uintptr_t inBox)
{ EEPointUnitOrTile(SUC(inBox)->GetMouseLocation(), EEScratch<EETile*>(kScratchShimTile), EEScratch<GE3DPoint>(kScratchUHPoint), false); }
extern "C" void ee_screenscpp_su_lasso_in_rect(uintptr_t inBox, float inA, float inB, float inC, float inD, int64_t inMode)
{ static_cast<EEALasso*>(SUC(inBox)->mLasso)->SelectInRectangle(GERectangle(inA, inB, inC, inD), (long)inMode); }
extern "C" void ee_screenscpp_su_click_execute(uintptr_t inBox, float inX, float inY)
{ static_cast<EEAMouseClick*>(SUC(inBox)->mClick)->Execute(inX, inY, true, 1, false, 0.0f, false); }
extern "C" int64_t ee_screenscpp_su_left_down(uintptr_t inBox) { return static_cast<UIControl*>(SUC(inBox))->MouseLButtonDown(); }
#undef SUC

// ==== EEUIFError.cpp ================================================================================================
// C:\EEX\Empire Earth\User Interface Thread\EEUIFError.cpp (__FILE__ 0x4cd490). The game's message box:
// 0x260c27..0x2624a6 (EEUIFEBOK, EEUIFEBCancel, EEUIFError; EEUIFileComboBox's ctor at 0x2624a6). The form and its
// buttons are kinds over the shells (ee_game shells_dialog.rs); this unit keeps the box's host. Unit flags: the exe
// module set.
// the layout's registers (the UI thread's; the box's Display inputs are dialog_inputs_ffi.rs')

// A new box (7 controls, 1 group, no id; its constructor ee_screens dialog.rs construct) shown as inKind
// (EEUIFErrorDisplay, ee/EEUIManager.h, through the game's UI table)
static void EEUIFErrorShow(UIManager* inManager, unsigned long inKind, const UWideString& inMessage,
                           unsigned long inAlignment, void* inData, EEUIFErrorCallback inFirst,
                           EEUIFErrorCallback inSecond, UIForm* inCloseForm, bool inFlag)
{
    ee_screens_dialog_show(static_cast<UIForm*>(new EEUIShellForm(EE_SCREENS_SK_DIALOG, {}, 7, 1, inManager, (unsigned long)-1, true)),
                           (uint32_t)inKind, &inMessage, (uint32_t)inAlignment, inData, reinterpret_cast<const void*>(inFirst),
                           reinterpret_cast<const void*>(inSecond), inCloseForm, inFlag);
}
static const bool sErrorDisplayRegistered = (gEEGameUI.mErrorDisplay = EEUIFErrorShow, true);

static const GEColor kMessageTextColor(0.0f, 0.0f, 0.0f);
static float* const kDialogSides[4] = {&EEScratch<GERectangle>(kScratchDialogRectangle).mLeft, &EEScratch<GERectangle>(kScratchDialogRectangle).mTop, &EEScratch<GERectangle>(kScratchDialogRectangle).mRight, &EEScratch<GERectangle>(kScratchDialogRectangle).mBottom};

// the moved cases' one-statement shims (ee_screens dialog_calls.rs asks them first)
#define DLG_CTL(x) reinterpret_cast<UIControl*>(x)
#define DLG_BUTTON(x) static_cast<UIButton*>(reinterpret_cast<UIControl*>(x))
#define DLG_BOX(x) reinterpret_cast<UIForm*>(x)
extern "C" void ee_screenscpp_dlg_text_load(uint32_t inIds) { gResource->GetString(inIds, EEScratch<UWideString>(kScratchDialogText)); }
extern "C" void ee_screenscpp_dlg_button_set_text(uintptr_t inButton)
{ DLG_BUTTON(inButton)->SetText(EEScratch<UWideString>(kScratchDialogText), gFontTahoma12); }
extern "C" uintptr_t ee_screenscpp_dlg_edit_text(uintptr_t inEdit)
{ return (uintptr_t)static_cast<UIEditBox*>(DLG_CTL(inEdit))->GetText(0); }
extern "C" void ee_screenscpp_dlg_message_align(uintptr_t inLabel, uint32_t inAlignment)
{ static_cast<UITextLabel*>(DLG_CTL(inLabel))->SetAlignment(inAlignment); }
extern "C" void ee_screenscpp_dlg_message_text(uintptr_t inLabel)
{ static_cast<UITextLabel*>(DLG_CTL(inLabel))->SetText(*static_cast<const UWideString*>(ee_screens_dlg_message())); }
extern "C" float ee_screenscpp_dlg_rect_side(uint32_t inSide) { return *kDialogSides[inSide]; }
extern "C" void ee_screenscpp_dlg_set_callback(uintptr_t inButton, uintptr_t inCallback, uintptr_t inData)
{ EEUIShell::Op(DLG_CTL(inButton), EE_SCREENS_SKOP_DIALOG_SET_CALLBACK, (int64_t)inCallback, (int64_t)inData); }
extern "C" void ee_screenscpp_dlg_show_button(uintptr_t inButton)
{ DLG_CTL(inButton)->SetDisplayMode(kDisplayModeShown); }
extern "C" void ee_screenscpp_dlg_set_close(uintptr_t inButton)
{ EEUIShell::Op(DLG_CTL(inButton), EE_SCREENS_SKOP_DIALOG_SET_CLOSE, (int64_t)ee_screens_dlg_close_form()); }
extern "C" void ee_screenscpp_dlg_button_text_color(uintptr_t inButton)
{ DLG_BUTTON(inButton)->GetGEText()->SetFgColor(kMessageTextColor); }
extern "C" void ee_screenscpp_dlg_button_text_transparent(uintptr_t inButton)
{ DLG_BUTTON(inButton)->GetGEText()->SetIsTransparent(true); }
extern "C" float ee_screenscpp_dlg_window_width() { return (float)gGameWindow->GetWidth(); }
extern "C" float ee_screenscpp_dlg_window_height() { return (float)gGameWindow->GetHeight(); }
extern "C" void ee_screenscpp_dlg_layout_reset() { EEScratch<UILayoutUtility>(kScratchDialogLayout) = UILayoutUtility(); }
extern "C" void ee_screenscpp_dlg_layout_left(float inV) { EEScratch<UILayoutUtility>(kScratchDialogLayout).SetLeft(4, inV); }
extern "C" void ee_screenscpp_dlg_layout_right(float inV) { EEScratch<UILayoutUtility>(kScratchDialogLayout).SetRight(4, inV); }
extern "C" void ee_screenscpp_dlg_layout_top(float inV) { EEScratch<UILayoutUtility>(kScratchDialogLayout).SetTop(4, inV); }
extern "C" void ee_screenscpp_dlg_layout_bottom(float inV) { EEScratch<UILayoutUtility>(kScratchDialogLayout).SetBottom(4, inV); }
extern "C" void ee_screenscpp_dlg_layout_calculate(float inWidth, float inHeight)
{ EEScratch<UILayoutUtility>(kScratchDialogLayout).CalculateRectangle(GERectangle(0.0f, 0.0f, inWidth, inHeight), EEScratch<GERectangle>(kScratchDialogRectangle), true); }
extern "C" float ee_screenscpp_dlg_font16_height() { return (float)gFontTahoma16->GetHeight(); }
extern "C" void ee_screenscpp_dlg_font_create(int32_t inHeight)
{ EEScratch<GEFont*>(kScratchDialogFont) = UIFontTable::CreateClosestFont(0x1c, (long)inHeight); }
extern "C" void ee_screenscpp_dlg_font_release() { EEScratch<GEFont*>(kScratchDialogFont)->Dereference(); }
extern "C" void ee_screenscpp_dlg_button_font(uintptr_t inButton)
{ DLG_BUTTON(inButton)->GetGEText()->SetFont(EEScratch<GEFont*>(kScratchDialogFont)); }
extern "C" uintptr_t ee_screenscpp_dlg_new_label(uintptr_t inDialog)
{ return (uintptr_t) new UITextLabel(UWideString(""), EEScratch<GEFont*>(kScratchDialogFont), 0, GERectangle(GE2DPoint(0.0f, 0.0f), EEScratch<float[9]>(kScratchDialogFloats)[0], EEScratch<float[9]>(kScratchDialogFloats)[1]), DLG_BOX(inDialog), 1, 0, 0); }
extern "C" uintptr_t ee_screenscpp_dlg_new_edit(uintptr_t inDialog)
{ return (uintptr_t) new UIEditBox(4, EEScratch<GEFont*>(kScratchDialogFont), 0x32, false, GERectangle(GE2DPoint(0.0f, 0.0f), EEScratch<float[9]>(kScratchDialogFloats)[0], EEScratch<float[9]>(kScratchDialogFloats)[1]), DLG_BOX(inDialog), 4, 2, false, false); }
extern "C" uintptr_t ee_screenscpp_dlg_new_button(uintptr_t inDialog, uint32_t inKind, uint32_t inIndex)
{ return (uintptr_t) new EEUIShellButton(inKind, {}, inIndex, NULL, NULL, NULL, GERectangle(GE2DPoint(0.0f, 0.0f), 50.0f, 20.0f), DLG_BOX(inDialog), inIndex, 1); }
extern "C" void ee_screenscpp_dlg_edit_selected_color(uintptr_t inEdit)
{ static_cast<UIEditBox*>(DLG_CTL(inEdit))->SetSelectedBgColor(GEColor(EEScratch<float[9]>(kScratchDialogFloats)[0], EEScratch<float[9]>(kScratchDialogFloats)[1], EEScratch<float[9]>(kScratchDialogFloats)[2])); }
extern "C" void ee_screenscpp_dlg_edit_bg_color(uintptr_t inEdit)
{ static_cast<UIEditBox*>(DLG_CTL(inEdit))->SetBgColor(GEColor(EEScratch<float[9]>(kScratchDialogFloats)[3], EEScratch<float[9]>(kScratchDialogFloats)[4], EEScratch<float[9]>(kScratchDialogFloats)[5])); }
extern "C" void ee_screenscpp_dlg_edit_cursor_color(uintptr_t inEdit)
{ static_cast<UIEditBox*>(DLG_CTL(inEdit))->SetCursorColor(GEColor(EEScratch<float[9]>(kScratchDialogFloats)[6], EEScratch<float[9]>(kScratchDialogFloats)[7], EEScratch<float[9]>(kScratchDialogFloats)[8])); }
extern "C" float ee_screenscpp_dlg_float(uint32_t inIndex) { return EEScratch<float[9]>(kScratchDialogFloats)[inIndex]; }
extern "C" void ee_screenscpp_dlg_box_set_layout(uintptr_t inDialog) { DLG_BOX(inDialog)->SetLayout(EEScratch<UILayoutUtility>(kScratchDialogLayout)); }
extern "C" uintptr_t ee_screenscpp_dlg_new_background(uintptr_t inDialog)
{ return (uintptr_t) new EEUIBGenericBackground(0, "textures\\EEGenericBack_Gold", GETextureDescription{1, 0, DLG_BOX(inDialog)->GetManager()->GetUITextureBitDepth(), 0, false, false, true, true}, EEScratch<UILayoutUtility>(kScratchDialogLayout), DLG_BOX(inDialog), 0, GEColor(EEScratch<float[9]>(kScratchDialogFloats)[4], EEScratch<float[9]>(kScratchDialogFloats)[5], EEScratch<float[9]>(kScratchDialogFloats)[6]), true, 1.0f, 1.0f, 1.0f, 3); }
extern "C" void ee_screenscpp_dlg_delete(uintptr_t inControl) { delete DLG_CTL(inControl); }
extern "C" void ee_screenscpp_dlg_scale_background(uintptr_t inBackground)
{ reinterpret_cast<EEUIBGenericBackground*>(inBackground)->SetScale((float)gGameWindow->GetWidth() * (1.0f / 1024.0f), (float)gGameWindow->GetHeight() * (1.0f / 768.0f)); }
extern "C" bool ee_screenscpp_dlg_pressed(uintptr_t inButton)
{ return EEUIShell::Op(DLG_CTL(inButton), EE_SCREENS_SKOP_DIALOG_PRESSED) != 0; }
extern "C" uintptr_t ee_screenscpp_dlg_pressed_callback(uintptr_t inButton)
{ return (uintptr_t)EEUIShell::Op(DLG_CTL(inButton), EE_SCREENS_SKOP_DIALOG_CALLBACK); }
extern "C" uintptr_t ee_screenscpp_dlg_pressed_data(uintptr_t inButton)
{ return (uintptr_t)EEUIShell::Op(DLG_CTL(inButton), EE_SCREENS_SKOP_DIALOG_DATA); }
extern "C" void ee_screenscpp_dlg_post_closed(uintptr_t inDialog, uintptr_t inCallback, uintptr_t inData)
{ gUserInterface->PostErrorClosed(DLG_BOX(inDialog), new EEUIFErrorCallbackData{(EEUIFErrorCallback)inCallback, (void*)inData}); }

// The box's host (dialog.rs: EE_SCREENS_DLG_*; shells_dialog.rs the box's own): each op one toolkit call on its
// controls, the kind's slots (0 the message, 1 OK/Yes, 2 No, 3 Cancel, 4 the edit box, 5 the background); the plain
// toolkit calls (show, tabbing, password, lines, scroll bar, alignment, text colours, add, enable) are
// shells_dialog.rs' on the toolkit's Rust objects
extern "C" uint32_t ee_screenscpp_dialog(void* inDialog, void** ioSlots, uint32_t inOp, uint32_t inA, uint32_t inB)
{
#define theBox static_cast<UIForm*>(inDialog)
#define theControls reinterpret_cast<UIControl**>(ioSlots)
#define theError static_cast<UITextLabel*>(theControls[0])
    switch (inOp) {
    case EE_SCREENS_DLG_MESSAGE_SIZE: theError->SetRectangle(GERectangle(GE2DPoint(0.0f, 0.0f), (float)inA, (float)inB)); break;
    case EE_SCREENS_DLG_SWITCH: gUserInterface->SwitchToForm(theBox); break;
    case EE_SCREENS_DLG_PLACE: theControls[inA]->SetRectangle(EEScratch<GERectangle>(kScratchDialogRectangle)); break;
    case EE_SCREENS_DLG_SET_RECT: memcpy(kDialogSides[inA], &inB, 4); break;
    case EE_SCREENS_DLG_LINE_HEIGHT: return (uint32_t)gFontTahoma12->GetHeight();
    case EE_SCREENS_DLG_FLOAT: memcpy(&EEScratch<float[9]>(kScratchDialogFloats)[inA], &inB, 4); break;
    }
    return 0;
}
#undef theBox
#undef theControls
#undef theError
#undef DLG_CTL
#undef DLG_BUTTON
#undef DLG_BOX

// ==== EEUIFMainGame.cpp =============================================================================================
// C:\EEX\Empire Earth\User Interface Thread\EEUIFMainGame.cpp (__FILE__ 0x262e73). The in-game form: 0x26272b..0x26783d
// (the default player names' init chain first). Unit flags: the exe's module set.
// the command buttons' part of the host (EEUIFMGStatusButton.cpp)
bool EEFindGateWall(const EETileCoordinate& inTile, long& ioDirection);  // 0x2116a2 (EEUIBuildGate.cpp's)
void EEPreviewWall(std::vector<EETileCoordinate>& inTiles, long inTechnology, long inPlayer);
void EEPreviewPlacement(const GE2DPoint& inLocation);  // 0x204aba (the calamity's ghost at the mouse) (provisional name)
// EESetCivBuilderButtonBlinking 0x27fcf0: ee/EmpireEarth.h (Open passes mIsButtonBlinking)
void EEPostGraphicsAll();                   // 0x10fc0c (Open) (provisional name)
extern float& gMasterVolume;                 // 0x530830 (the master volume Open restores)

// the information panel's message is the scenario's dialog message (EEUserInterface's message 0x422 hands over an
// EESMDialogMessage; ShowInformation 0x26533e read it through the 32-bit offsets of a stand-in class, whose fields
// lined up there and not on x64: the portrait's class was read from the duration, the scroll time from the unit id)

// The unit hack whole in Rust (ee_screens unit_hack.rs): the original's members 0x2628e5 (ctor: the sorted
// class names), 0x262a91 (dtor), 0x262aa5 PlaceUnit, 0x262c6f FillComboBox, 0x262cdc LoadUnitData, 0x262d66
// Update, 0x262e29 GetSelection, and the two message ctors' COMDATs 0x262c24 (EEMHackANewUnit) and 0x262d32
// (EEMLoadUnitData). The host answers its ops (EE_SCREENS_UH_OP_*): the class table, the object and player lists,
// the keys, the mouse, the messages.
// the moved cases' shims (ee_screens unit_hack_calls.rs asks them first)
#define UH_OBJECTS(x) static_cast<UIComboBox*>(static_cast<EEUIFMainGame*>(reinterpret_cast<void*>(x))->GetControl(kMainGameTempObjectList))
static ISEntity* EEKey(unsigned long inKey);
extern "C" bool ee_screenscpp_uh_class_exists(int32_t inIndex) { return gUnitClassTable[inIndex] != NULL; }
extern "C" bool ee_screenscpp_uh_class_enabled(int32_t inIndex) { return gUnitClassTable[inIndex]->mIsEnabled; }
extern "C" bool ee_screenscpp_uh_class_placeable(int32_t inIndex)
{ return gUnitClassTable[inIndex]->mIsPlayerPlaceable; }
extern "C" void ee_screenscpp_uh_class_name(int32_t inIndex, void* inName)
{ EEShimOut(inName, UWideString((const char*)gUnitClassTable[inIndex]->mName)); }
extern "C" void ee_screenscpp_uh_combo_append(uintptr_t inForm, EeScreensShimText inLine)
{ UH_OBJECTS(inForm)->AppendLine(EEShimText(inLine)); }
extern "C" uintptr_t ee_screenscpp_uh_combo_trigger(uintptr_t inForm)
{ return (uintptr_t)UH_OBJECTS(inForm)->GetTriggerEvent(kControlTriggerSelectionChanged); }
extern "C" void ee_screenscpp_uh_combo_set_trigger(uintptr_t inForm, uintptr_t inEvent)
{ UH_OBJECTS(inForm)->SetTriggerEvent(kControlTriggerSelectionChanged, reinterpret_cast<UIEvent*>(inEvent)); }
extern "C" void ee_screenscpp_uh_combo_select_first(uintptr_t inForm) { UH_OBJECTS(inForm)->SetCurrentSelection(0); }
extern "C" void ee_screenscpp_event_reference(uintptr_t inEvent) { reinterpret_cast<UIEvent*>(inEvent)->Reference(); }
extern "C" void ee_screenscpp_uh_combo_selection(uintptr_t inForm, unsigned long* inSelection)
{ UH_OBJECTS(inForm)->GetCurrentSelection(*inSelection); }
extern "C" int32_t ee_screenscpp_uh_object_list_selection(uintptr_t inForm, unsigned long* inSelection)
{ return (int32_t)UH_OBJECTS(inForm)->GetListBox()->GetCurrentSelection(*inSelection); }
extern "C" int32_t ee_screenscpp_uh_player_list_selection(uintptr_t inForm, unsigned long* inSelection)
{ return (int32_t)static_cast<UIComboBox*>(static_cast<EEUIFMainGame*>(reinterpret_cast<void*>(inForm))->GetControl(kMainGameTempPlayerList))->GetListBox()->GetCurrentSelection(*inSelection); }
extern "C" int32_t ee_screenscpp_uh_control_value() { return (int32_t)EEKey(kKeyControl)->GetValue(); }
extern "C" void ee_screenscpp_uh_send_place(int32_t inUnitType, float inX, float inY, int32_t inFlag)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageHackANewUnit, {(long)inUnitType, GE2DPoint(inX, inY), (unsigned char)inFlag}), kSlotNone); }
extern "C" bool ee_screenscpp_uh_point(float inX, float inY)
{ return EEPointUnitOrTile(GE2DPoint(inX, inY), EEScratch<EETile*>(kScratchShimTile), EEScratch<GE3DPoint>(kScratchUHPoint), true); }
extern "C" float ee_screenscpp_uh_point_x() { return EEScratch<GE3DPoint>(kScratchUHPoint).GetX(); }
extern "C" float ee_screenscpp_uh_point_y() { return EEScratch<GE3DPoint>(kScratchUHPoint).GetY(); }
extern "C" void ee_screenscpp_uh_point_reset() { EEScratch<GE3DPoint>(kScratchUHPoint) = GE3DPoint(); }
// the host's one-call ops (unit_hack_calls.rs; the host's switch until UI lane round 16)
extern "C" int32_t ee_screenscpp_uh_class_count() { return (int32_t)EEUnitClassCount(); }
extern "C" int32_t ee_screenscpp_uh_class_category(int32_t inIndex) { return gUnitClassTable[inIndex]->mCategory; }
extern "C" int32_t ee_screenscpp_uh_class_type(int32_t inIndex) { return gUnitClassTable[inIndex]->mUnitType; }
extern "C" void ee_screenscpp_uh_combo_clear(uintptr_t inForm) { UH_OBJECTS(inForm)->DeleteAllEntries(); }
extern "C" bool ee_screenscpp_uh_tile(float inX, float inY) { return gWorld.mMap->GetTile((long)inX, (long)inY) != NULL; }
extern "C" bool ee_screenscpp_uh_player_skipped(int32_t inPlayer)
{ return gWorld.mPlayers[inPlayer] == NULL || gWorld.mPlayers[inPlayer] == gWorld.GetCurrentPlayer(); }
extern "C" int32_t ee_screenscpp_uh_local_player() { return (int32_t)gWorld.GetCurrentPlayer()->mPlayerID; }
extern "C" void ee_screenscpp_uh_send_load(int32_t inUnitType)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageLoadUnitData, {inUnitType, true}), kSlotNone); }
extern "C" bool ee_screenscpp_uh_in_view(float inX, float inY) { return gViewport.GetViewRectangle().Contains(GE2DPoint(inX, inY)); }

EEUIFMGUnitHack::EEUIFMGUnitHack(EEUIFMainGame* inForm)
    : EeScreensUnitHackFields{inForm, ee_screens_unit_hack_new(inForm)} {}

// (EEUIFMGFormCall, one call of the form's work in Rust: ee/EEUIFMainGame.h; its strings follow the Rust record as
// hud_direct.rs FormCallHead has them)
static_assert(offsetof(EEUIFMGFormCall, mText) == sizeof(EeScreensFormCallObjects), "hud_direct.rs FormCallHead");

// (BuildGate and BuildWall: the placements call the build action, ee/EEARust.h)

// (BuildUnit and the start modes StartGate, StartWall, StartCalamity, StartFlare, StartPatrol: inline in
// ee/EEUIFMainGame.h)

// Switches the interface mode, clearing ghosts and ending a wall or gate placement

// (main_form.rs' sounds and strings and mouse_modes.rs' farms and keys equal the headers': hud_cpp_consts_tests.rs)

// ---- the mouse modes' ops (ee_screens mouse_modes.rs, EE_SCREENS_MF_OP_MD_*): the picked unit, the first selected
// unit and the pointed tile held in the call; io.f[0..2] the mouse location where an op takes one
// ---- (the mouse modes' ops end)

// (the cursor's reads for one UpdateCursorKind: the Rust record EeScreensCursorRead, hud_direct.rs, untyped)

// The world's answers to the cursor's ops (EE_SCREENS_CURSOR_OP_*): the target, the selection by index, an air
// base's cargo by index; the positions' floats into outF
#define CR(x) static_cast<EeScreensCursorRead*>(reinterpret_cast<void*>(x))
extern "C" uintptr_t ee_screenscpp_cur_mouse_over(uintptr_t inCtx)
{ return (uintptr_t)static_cast<EEUIFMainGame*>(CR(inCtx)->mForm)->GetMouseOverControl(); }
extern "C" uintptr_t ee_screenscpp_cur_command_units(uintptr_t inCtx)
{ return (uintptr_t)static_cast<EEUIFMainGame*>(CR(inCtx)->mForm)->GetControl(kMainGameCommandUnits); }
extern "C" void ee_screenscpp_cur_pick(uintptr_t inCtx, uint32_t inFlags)
{ CR(inCtx)->mTarget = gUnitPicker.PickUnit(gMouse->mPosition, inFlags); }
extern "C" bool ee_screenscpp_cur_has_target(uintptr_t inCtx) { return static_cast<EEUnit*>(CR(inCtx)->mTarget) != NULL; }
extern "C" bool ee_screenscpp_cur_t_on_tile(uintptr_t inCtx) { return static_cast<EEUnit*>(CR(inCtx)->mTarget)->mTileNode != NULL; }
extern "C" bool ee_screenscpp_cur_t_disabled(uintptr_t inCtx) { return static_cast<EEUnit*>(CR(inCtx)->mTarget)->mComplexUnit->mIsDisabled; }
extern "C" bool ee_screenscpp_cur_t_protected(uintptr_t inCtx)
{ return static_cast<EEUnit*>(CR(inCtx)->mTarget)->mComplexUnit->IsProtected(); }
extern "C" bool ee_screenscpp_cur_t_has_complex(uintptr_t inCtx)
{ return static_cast<EEUnit*>(CR(inCtx)->mTarget)->GetComplexUnit() != NULL; }
extern "C" bool ee_screenscpp_cur_t_has_model(uintptr_t inCtx)
{ return static_cast<EEUnit*>(CR(inCtx)->mTarget)->GetModel()->mModel != NULL; }
extern "C" float ee_screenscpp_cur_t_scale(uintptr_t inCtx) { return static_cast<EEUnit*>(CR(inCtx)->mTarget)->mStats->GetScale(); }
extern "C" void ee_screenscpp_cur_t_default_point(uintptr_t inCtx, float* inPoint)
{ *reinterpret_cast<GE3DPoint*>(inPoint) = static_cast<EEUnit*>(CR(inCtx)->mTarget)->GetModel()->mDefaultPoint; }
extern "C" void ee_screenscpp_cur_t_place(uintptr_t inCtx, float inX, float inY, float inZ, float* inAim)
{ static_cast<EEUnit*>(CR(inCtx)->mTarget)->GetComplexUnit()->mPhysicalModel.GetLocation().Apply(GE3DPoint(inX, inY, inZ), *reinterpret_cast<GE3DPoint*>(inAim)); }
extern "C" int32_t ee_screenscpp_cur_t_tile_x(uintptr_t inCtx) { return (int32_t)static_cast<EEUnit*>(CR(inCtx)->mTarget)->GetTileX(); }
extern "C" int32_t ee_screenscpp_cur_t_tile_y(uintptr_t inCtx) { return (int32_t)static_cast<EEUnit*>(CR(inCtx)->mTarget)->GetTileY(); }
extern "C" float ee_screenscpp_cur_t_tile_height(uintptr_t inCtx) { return static_cast<EEUnit*>(CR(inCtx)->mTarget)->mTile->GetHeight(); }
extern "C" bool ee_screenscpp_cur_can_gather(uintptr_t inCtx, uint32_t inFlags)
{ return static_cast<EESelectedGroup*>(CR(inCtx)->mGroup)->FindUnitWithFlags(inFlags)->CanGather(static_cast<EEUnit*>(CR(inCtx)->mTarget), static_cast<EEUnit*>(CR(inCtx)->mTarget)->GetTileX(), static_cast<EEUnit*>(CR(inCtx)->mTarget)->GetTileY(), false, false); }
extern "C" void ee_screenscpp_cur_set_shielded(bool inShielded) { EEScratch<bool>(kScratchCurShielded) = inShielded; }
extern "C" bool ee_screenscpp_cur_convert(uintptr_t inCtx, uint32_t inIndex)
{ return EECanConvert(static_cast<EESelectedGroup*>(CR(inCtx)->mGroup)->GetUnit(inIndex), static_cast<EEUnit*>(CR(inCtx)->mTarget)->mComplexUnit, EEScratch<bool>(kScratchCurShielded)); }
extern "C" bool ee_screenscpp_cur_shielded() { return EEScratch<bool>(kScratchCurShielded); }
// UpdateCursorKind's reads (cursor_ffi.rs ee_screens_update_cursor_kind) and its result
extern "C" EEPlayer* ee_screenscpp_cur_local_player() { return gWorld.GetCurrentPlayer(); }
extern "C" EESelectedGroup* ee_screenscpp_cur_selected_group(EEPlayer* inPlayer) { return inPlayer->mSelectedGroup; }
extern "C" EEComplexUnit* ee_screenscpp_cur_group_first(EESelectedGroup* inGroup) { return inGroup->GetFirstUnit(); }
extern "C" void ee_screenscpp_cur_set_pointer(int32_t inKind) { gMouse->mPointer = inKind; }
// the host's one-call ops (cursor_calls.rs; the host's switch until UI lane round 16)
#define CRU(x, i) static_cast<EESelectedGroup*>(CR(x)->mGroup)->GetUnit(i)
#define CRT(x) static_cast<EEUnit*>(CR(x)->mTarget)
extern "C" int32_t ee_screenscpp_cur_flags(uintptr_t inCtx) { return (int32_t)static_cast<EESelectedGroup*>(CR(inCtx)->mGroup)->mKindFlags; }
extern "C" bool ee_screenscpp_cur_in_view() { return gRasterizer->GetViewport(gScreenIndex).GetViewRectangle().Contains(gMouse->mPosition); }
extern "C" void ee_screenscpp_cur_release(uintptr_t inCtx) { CRT(inCtx)->Dereference(); }
extern "C" bool ee_screenscpp_cur_seen(uintptr_t inCtx) { return EECombatIsSeenBy(CRT(inCtx), static_cast<EEPlayer*>(CR(inCtx)->mPlayer), true); }
extern "C" bool ee_screenscpp_cur_t_targetable(uintptr_t inCtx) { return ee_units_uu_targetable(0, (uint64_t)(uintptr_t)CRT(inCtx)->mUnitClass, 0); }
extern "C" bool ee_screenscpp_cur_can_heal(uintptr_t inCtx) { return EECanHeal(static_cast<EEComplexUnit*>(CR(inCtx)->mFirst), CRT(inCtx)->mComplexUnit); }
extern "C" bool ee_screenscpp_cur_sel_air_base(uintptr_t inCtx, uint32_t inA) { return CRU(inCtx, inA)->GetUnitClass()->IsAirBase(); }
extern "C" bool ee_screenscpp_cur_sel_crusader(uintptr_t inCtx, uint32_t inA) { return CRU(inCtx, inA)->mUnitClass->IsCrusader(); }
extern "C" bool ee_screenscpp_cur_sel_priest(uintptr_t inCtx, uint32_t inA) { return CRU(inCtx, inA)->mUnitClass->IsPriest(); }
extern "C" bool ee_screenscpp_cur_sel_can_attack(uintptr_t inCtx, uint32_t inA)
{ return CRU(inCtx, inA)->GetComplexUnit()->CanAttackClass(CRT(inCtx)->GetUnitClass()); }
extern "C" bool ee_screenscpp_cur_cargo_can_attack(uintptr_t inCtx, uint32_t inA, uint32_t inB)
{ return CRU(inCtx, inA)->GetComplexUnit()->mCargo[inB]->CanAttackClass(CRT(inCtx)->GetUnitClass()); }

// Tracks the mouse in wall placement (ee_screens mouse_modes.rs wall_update): a left click sets the start, then
// builds each segment and chains from its end; a right click cancels; moving previews the wall ghosts
void EEUIBuildWallUpdate(EEUIBuildWall* inWall, const GE2DPoint& inLocation)
{
    ee_screens_wall_update(EEUIFMGFormCall(gUIManager->GetMainGameForm(), inWall).Ptr(), inWall, inLocation.GetX(), inLocation.GetY());
}

// Tracks the mouse in gate placement (ee_screens mouse_modes.rs gate_update): a gate ghost on the wall under the
// mouse, built on a left click, cancelled on a right click
void EEUIBuildGateUpdate(EEUIBuildGate* inGate, const GE2DPoint& inLocation)
{
    ee_screens_gate_update(EEUIFMGFormCall(gUIManager->GetMainGameForm(), inGate).Ptr(), inGate, inLocation.GetX(), inLocation.GetY());
}

// ---- the shims of the moved cases (UI lane round 7: ee_game screens main_form_calls.rs, game_cpp.rs), each one
// C++ statement on the host's call record or the world
static EEUIFMGFormCall* R(uintptr_t inRec) { return reinterpret_cast<EEUIFMGFormCall*>(inRec); }
static UIControl* C(uintptr_t inControl) { return reinterpret_cast<UIControl*>(inControl); }
// the keyboard's key (NULL when the keyboard has none)
static ISEntity* EEKey(unsigned long inKey)
{
    ISEntity* theKey = NULL;
    return gISKeyboard->GetEntity(inKey, theKey) != 0 ? NULL : theKey;
}
// shims5
extern "C" void ee_screenscpp_mf_set_ui_state(uintptr_t inRec, int32_t inState)
{ static_cast<EEUIFMainGame*>(R(inRec)->mForm)->SetUIState(inState); }
extern "C" bool ee_screenscpp_mf_has_wall(uintptr_t inRec) { return static_cast<EEUIBuildWall*>(R(inRec)->mWall) != NULL; }
extern "C" void ee_screenscpp_mf_wall_cancel(uintptr_t inRec) { ee_screens_wall_cancel(static_cast<EEUIBuildWall*>(R(inRec)->mWall)); }
extern "C" void ee_screenscpp_mf_gate_cancel(uintptr_t inRec) { ee_screens_gate_cancel(static_cast<EEUIBuildGate*>(R(inRec)->mGate)); }
extern "C" bool ee_screenscpp_mouse_left_just_pressed() { return UIManager::GetLMouseButton()->JustPressed() != 0; }
extern "C" bool ee_screenscpp_mouse_left_down() { return UIManager::GetLMouseButton()->GetValue() != 0; }
extern "C" bool ee_screenscpp_mouse_left_just_unpressed() { return UIManager::GetLMouseButton()->JustUnpressed() != 0; }
extern "C" bool ee_screenscpp_mouse_right_just_pressed() { return UIManager::GetRMouseButton()->JustPressed() != 0; }
extern "C" bool ee_screenscpp_mouse_right_down() { return UIManager::GetRMouseButton()->GetValue() != 0; }
extern "C" bool ee_screenscpp_mouse_right_just_unpressed()
{ return UIManager::GetRMouseButton()->JustUnpressed() != 0; }
extern "C" int32_t ee_screenscpp_key_shift_value() { return (int32_t)EEKey(kKeyShift)->GetValue(); }
extern "C" bool ee_screenscpp_key_exists(uint32_t inKey) { return EEKey(inKey) != NULL; }
extern "C" bool ee_screenscpp_key_value(uint32_t inKey) { return EEKey(inKey)->GetValue() != 0; }
extern "C" bool ee_screenscpp_key_just_pressed(uint32_t inKey) { return EEKey(inKey)->JustPressed() != 0; }
extern "C" bool ee_screenscpp_rasterizer_view_contains(float inX, float inY)
{ return gRasterizer->GetViewport(gScreenIndex).GetViewRectangle().Contains(GE2DPoint(inX, inY)); }
extern "C" bool ee_screenscpp_view_contains(float inX, float inY)
{ return gViewport.GetViewRectangle().Contains(GE2DPoint(inX, inY)); }
extern "C" bool ee_screenscpp_picked_owned_by_current(uintptr_t inRec)
{ return gWorld.GetCurrentPlayer() == static_cast<EEUnit*>(R(inRec)->mPicked)->mPlayer; }
extern "C" bool ee_screenscpp_picked_owner_hostile(uintptr_t inRec)
{ return static_cast<EEUnit*>(R(inRec)->mPicked)->mPlayer->mEnemies[gWorld.GetCurrentPlayer()->mPlayerID] != 0; }
extern "C" bool ee_screenscpp_picked_seen_by_current(uintptr_t inRec)
{ return EECombatIsSeenBy(static_cast<EEComplexUnit*>(static_cast<EEUnit*>(R(inRec)->mPicked)), gWorld.GetCurrentPlayer(), true); }
extern "C" bool ee_screenscpp_mf_first_from_group(uintptr_t inRec)
{ return (R(inRec)->mFirst = gWorld.GetCurrentPlayer()->mSelectedGroup->GetFirstUnit()) != NULL; }
extern "C" void ee_screenscpp_mf_first_clear(uintptr_t inRec) { R(inRec)->mFirst = NULL; }
extern "C" bool ee_screenscpp_mf_first_selected(uintptr_t inRec)
{ return EEGetFirstSelectedUnit(reinterpret_cast<EEComplexUnit*&>(R(inRec)->mFirst)) == true; }
extern "C" bool ee_screenscpp_mf_has_first(uintptr_t inRec) { return static_cast<EEComplexUnit*>(R(inRec)->mFirst) != NULL; }
extern "C" float ee_screenscpp_first_distance_squared(uintptr_t inRec, float inX, float inY, float inZ)
{ return static_cast<EEComplexUnit*>(R(inRec)->mFirst)->mComplexUnit->DistanceSquared3D(GE3DPoint(inX, inY, inZ)); }
extern "C" float ee_screenscpp_first_min_range(uintptr_t inRec) { return static_cast<EEComplexUnit*>(R(inRec)->mFirst)->GetUnitClass()->mMinRange; }
extern "C" void ee_screenscpp_player_add_event(int64_t inX, int64_t inY, int32_t inKind)
{ gWorld.GetCurrentPlayer()->mEvents.AddEvent(U2DPoint<long>((long)inX, (long)inY), 2, gWorld.GetCurrentPlayer(), inKind, false); }
extern "C" uint32_t ee_screenscpp_ghost_count() { return (uint32_t)gWorld.GetNumGhostUnits(); }
extern "C" bool ee_screenscpp_ghost_can_place(uint32_t inGhost)
{ return static_cast<EEGhostUnit*>(gWorld.GetGhostUnit(inGhost))->mCanPlace; }
extern "C" uint32_t ee_screenscpp_ghost_type(uint32_t inGhost)
{ return (uint32_t)static_cast<EEGhostUnit*>(gWorld.GetGhostUnit(inGhost))->mUnitClass->mUnitType; }
extern "C" int64_t ee_screenscpp_ghost_tile_x(uint32_t inGhost)
{ return (int64_t)static_cast<EEGhostUnit*>(gWorld.GetGhostUnit(inGhost))->mTileX; }
extern "C" int64_t ee_screenscpp_ghost_tile_y(uint32_t inGhost)
{ return (int64_t)static_cast<EEGhostUnit*>(gWorld.GetGhostUnit(inGhost))->mTileY; }
extern "C" void ee_screenscpp_build_set_queued(uintptr_t inRec, bool inQueued)
{ static_cast<EEABuild*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mBuild)->mIsQueued = inQueued; }
extern "C" void ee_screenscpp_build_at(uintptr_t inRec, float inX, float inY)
{ static_cast<EEABuild*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mBuild)->Build(GE2DPoint(inX, inY)); }
extern "C" void ee_screenscpp_hack_update(uintptr_t inRec, float inX, float inY)
{ static_cast<EEUIFMGUnitHack*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mUnitHack)->Update(GE2DPoint(inX, inY)); }
extern "C" void ee_screenscpp_hack_point(uintptr_t inRec, float inX, float inY)
{ static_cast<EEUIFMGUnitHack*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mUnitHack)->Fn157269(GE2DPoint(inX, inY)); }
extern "C" void ee_screenscpp_hack_place(uintptr_t inRec, float inX, float inY)
{ static_cast<EEUIFMGUnitHack*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mUnitHack)->PlaceUnit(GE2DPoint(inX, inY)); }
extern "C" float ee_screenscpp_mf_mouse_x(uintptr_t inRec) { return static_cast<EEUIFMainGame*>(R(inRec)->mForm)->GetMouseLocation().GetX(); }
extern "C" float ee_screenscpp_mf_mouse_y(uintptr_t inRec) { return static_cast<EEUIFMainGame*>(R(inRec)->mForm)->GetMouseLocation().GetY(); }
extern "C" int64_t ee_screenscpp_mf_mouse_mode(uintptr_t inRec)
{ return (int64_t)static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mMouseModes[static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mUIState]; }
extern "C" bool ee_screenscpp_cinematic_ending() { return gWorld.GetScenarioManager()->mIsEndingCinematic; }
extern "C" void ee_screenscpp_post_cinematic_screen_shot() { gUserInterface->PostScreenShot(-1, true, false); }
extern "C" uintptr_t ee_screenscpp_end_cinematic_event_new()
{ return (uintptr_t)static_cast<UIEvent*>(new EEScreenEvent(EE_SCREENS_EV_END_CINEMATIC)); }
extern "C" void ee_screenscpp_mf_start_selection(uintptr_t inRec)
{ static_cast<EEUICSelectUnits*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->GetControl(kMainGameSelectUnits))->StartSelection(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->GetMouseLocation()); }
extern "C" void ee_screenscpp_mf_start_command(uintptr_t inRec)
{ static_cast<EEUICCommandUnits*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->GetControl(kMainGameCommandUnits))->StartCommand(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->GetMouseLocation()); }
extern "C" void ee_screenscpp_mf_wall_build(uintptr_t inRec) { ee_screens_wall_build(static_cast<EEUIBuildWall*>(R(inRec)->mWall)); }
extern "C" void ee_screenscpp_mf_gate_build(uintptr_t inRec, float inX, float inY, int32_t inDirection)
{ ee_screens_gate_build(static_cast<EEUIBuildGate*>(R(inRec)->mGate), inX, inY, (int32_t)inDirection); }
extern "C" void ee_screenscpp_mf_gate_create_ghost(uintptr_t inRec)
{ EEGhostUnit::CreateGate(0.0f, static_cast<EEUnitClass*>(static_cast<EEUIBuildGate*>(R(inRec)->mGate)->mUnitClass), gWorld.GetPlayer(static_cast<EEUIBuildGate*>(R(inRec)->mGate)->mPlayer)); }
extern "C" void ee_screenscpp_gate_preview(float inX, float inY, long* inDirection)
{ EEGhostUnit::PreviewGate(GE2DPoint(inX, inY), *inDirection); }
extern "C" void ee_screenscpp_mf_wall_update(uintptr_t inRec)
{ EEUIBuildWallUpdate(&static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mBuildWall, static_cast<EEUIFMainGame*>(R(inRec)->mForm)->GetMouseLocation()); }
extern "C" void ee_screenscpp_mf_gate_update(uintptr_t inRec)
{ EEUIBuildGateUpdate(&static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mBuildGate, static_cast<EEUIFMainGame*>(R(inRec)->mForm)->GetMouseLocation()); }
extern "C" void ee_screenscpp_mf_error_lock_enter(uintptr_t inRec) { static_cast<TSCriticalSection*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mErrorCriticalSection)->Enter(); }
extern "C" void ee_screenscpp_mf_error_lock_leave(uintptr_t inRec) { static_cast<TSCriticalSection*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mErrorCriticalSection)->Leave(); }
extern "C" uint32_t ee_screenscpp_mf_current_group(uintptr_t inRec)
{ return (uint32_t)static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mCurrentGroup; }
extern "C" void ee_screenscpp_mf_group_reset(uintptr_t inRec) { static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mMainGroup->Reset(); }
extern "C" void ee_screenscpp_mf_group_refresh(uintptr_t inRec) { static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mMainGroup->Refresh(); }
extern "C" bool ee_screenscpp_mf_group_wants_refresh(uintptr_t inRec)
{ return static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mMainGroup->WantsRefresh(); }
extern "C" uintptr_t ee_screenscpp_mf_pick(uintptr_t inRec)
{ return (uintptr_t)(R(inRec)->mPicked = gUnitPicker.PickUnit(gMouse->mPosition, 0)); }
extern "C" bool ee_screenscpp_mf_picked_is_wall(uintptr_t inRec) { return static_cast<EEUnit*>(R(inRec)->mPicked)->mUnitClass->mIsWall; }
extern "C" int64_t ee_screenscpp_mf_picked_tile_x(uintptr_t inRec) { return (int64_t)static_cast<EEUnit*>(R(inRec)->mPicked)->mTileX; }
extern "C" int64_t ee_screenscpp_mf_picked_tile_y(uintptr_t inRec) { return (int64_t)static_cast<EEUnit*>(R(inRec)->mPicked)->mTileY; }
extern "C" void ee_screenscpp_mouse_tile(long* inX, long* inY)
{ EEPointUnitOrTileXY(gMouse->mPosition, EEScratch<EETile*>(kScratchShimTile), *inX, *inY, true); }
extern "C" void ee_screenscpp_mf_picked_dereference(uintptr_t inRec) { static_cast<EEUnit*>(R(inRec)->mPicked)->Dereference(); }
extern "C" void ee_screenscpp_mf_picked_clear(uintptr_t inRec) { R(inRec)->mPicked = NULL; }
extern "C" void ee_screenscpp_mf_form_gate_create_ghost(uintptr_t inRec)
{ EEGhostUnit::CreateGate(0.0f, static_cast<EEUnitClass*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mBuildGate.mUnitClass), gWorld.mPlayers[static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mBuildGate.mPlayer]); }
extern "C" void ee_screenscpp_gate_preview_at_mouse(long* inDirection)
{ EEGhostUnit::PreviewGate(gMouse->mPosition, *inDirection); }
extern "C" float ee_screenscpp_mf_wall_start_x(uintptr_t inRec) { return static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mBuildWall.mStart.x; }
extern "C" float ee_screenscpp_mf_wall_start_y(uintptr_t inRec) { return static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mBuildWall.mStart.y; }
extern "C" void ee_screenscpp_mf_info_set_text(uintptr_t inRec)
{ static_cast<UITLScrolling*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->GetControl(kMainGameDialogMessage))->SetText(static_cast<const EESMDialogMessage*>(R(inRec)->mMessage)->mText); }
extern "C" uintptr_t ee_screenscpp_mf_control(uintptr_t inRec, uint32_t inControl)
{ return (uintptr_t)static_cast<EEUIFMainGame*>(R(inRec)->mForm)->GetControl(inControl); }
extern "C" uint32_t ee_screenscpp_mf_info_unit_type(uintptr_t inRec) { return (uint32_t)static_cast<const EESMDialogMessage*>(R(inRec)->mMessage)->mUnitType; }
extern "C" uint32_t ee_screenscpp_unit_class_count() { return (uint32_t)EEUnitClassCount(); }
extern "C" bool ee_screenscpp_unit_class_exists(uint32_t inKind) { return gUnitClassTable[inKind] != NULL; }
extern "C" bool ee_screenscpp_player_class_has_button(uint32_t inKind)
{ return gWorld.GetCurrentPlayer()->mUnitClassUpgrades[inKind]->mButton != NULL; }
extern "C" void ee_screenscpp_mf_info_set_face(uintptr_t inRec, uintptr_t inBitmap, uintptr_t inPortrait)
{ gWorld.GetCurrentPlayer()->mUnitClassUpgrades[static_cast<const EESMDialogMessage*>(R(inRec)->mMessage)->mUnitType]->mButton->SetFaceBitmap(reinterpret_cast<GEBitmap*>(inBitmap), C(inPortrait)->GetRectangle(), C(inPortrait)->GetScreenIndex()); }
extern "C" void ee_screenscpp_ui_bitmap_set_bitmap(uintptr_t inControl, uintptr_t inBitmap)
{ static_cast<UIBitmap*>(C(inControl))->SetBitmap(reinterpret_cast<GEBitmap*>(inBitmap)); }
extern "C" void ee_screenscpp_control_refresh_opacity(uintptr_t inControl)
{ C(inControl)->SetBgOpacity(C(inControl)->GetBgOpacity()); }
extern "C" void ee_screenscpp_mf_info_load_sound(uintptr_t inRec, unsigned long* inSample, unsigned long* inLength)
{ EELoadSound("Sounds\\" + UString((const char*)static_cast<const EESMDialogMessage*>(R(inRec)->mMessage)->mSound), 0x31, *inSample, *inLength, 1); }
extern "C" void ee_screenscpp_mf_poll_new(uintptr_t inRec, uint32_t inMilliseconds)
{ static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mPoll = new TMPoll((unsigned long)inMilliseconds, 0, false); }
extern "C" void ee_screenscpp_mf_poll_start(uintptr_t inRec) { static_cast<TMPoll*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mPoll)->Start(); }
extern "C" uint32_t ee_screenscpp_game_size_width() { return (uint32_t)gGameWindowWidth; }
extern "C" uint32_t ee_screenscpp_game_size_height() { return (uint32_t)gGameWindowHeight; }
extern "C" void ee_screenscpp_mf_scroll_new(uintptr_t inRec) { static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mScroll = new EEAScroll; }
extern "C" void ee_screenscpp_mf_scroll_set_edges(uintptr_t inRec, float inLeft, float inTop, float inRight, float inBottom)
{ static_cast<EEAScroll*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mScroll)->mEdges = GERectangle(inLeft, inTop, inRight, inBottom); }
extern "C" void ee_screenscpp_mf_mouse_click_new(uintptr_t inRec) { static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mMouseClick = new EEAMouseClick; }
extern "C" void ee_screenscpp_mf_lasso_new(uintptr_t inRec) { static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mLasso = new EEALasso; }
extern "C" void ee_screenscpp_mf_build_new(uintptr_t inRec) { static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mBuild = new EEABuild; }
extern "C" void ee_screenscpp_mf_calamity_new(uintptr_t inRec) { static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mCalamity = new EEACalamity; }
extern "C" void ee_screenscpp_mf_wheel_zoom_new(uintptr_t inRec, uint32_t inCode)
{ static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mWheelZoom = EECreateScreenEvent((unsigned long)inCode); }
extern "C" void ee_screenscpp_mf_set_wheel_event(uintptr_t inRec)
{ static_cast<EEUIFMainGame*>(R(inRec)->mForm)->SetMouseWheelMoveEvent(static_cast<UIEvent*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mWheelZoom)); }
extern "C" void ee_screenscpp_mf_grab_scroll_new(uintptr_t inRec, uint32_t inCode)
{ static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mGrabScroll = EECreateGrabScroll((unsigned long)inCode); }
extern "C" void ee_screenscpp_mf_set_middle_pressed_event(uintptr_t inRec)
{ static_cast<EEUIFMainGame*>(R(inRec)->mForm)->SetMouseMiddleButtonPressedEvent(static_cast<UIEvent*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mGrabScroll)); }
extern "C" void ee_screenscpp_mf_set_middle_unpressed_event(uintptr_t inRec)
{ static_cast<EEUIFMainGame*>(R(inRec)->mForm)->SetMouseMiddleButtonUnpressedEvent(static_cast<UIEvent*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mGrabScroll)); }
extern "C" void ee_screenscpp_mf_add_hot_key(uintptr_t inRec, uint32_t inKey, uint32_t inModifier1, uint32_t inModifier2, uint32_t inModifier3, uintptr_t inEvent1, uintptr_t inEvent2, uintptr_t inEvent3, uintptr_t inEvent4, uint32_t inId, bool inFlag)
{ static_cast<EEUIFMainGame*>(R(inRec)->mForm)->AddHotKey(inKey, inModifier1, inModifier2, inModifier3, reinterpret_cast<UIEvent*>(inEvent1), reinterpret_cast<UIEvent*>(inEvent2), reinterpret_cast<UIEvent*>(inEvent3), reinterpret_cast<UIEvent*>(inEvent4), (unsigned long)inId, inFlag); }
extern "C" void ee_screenscpp_mf_unit_load(uintptr_t inRec, uint32_t inUnit)
{ R(inRec)->mUnit = static_cast<EEUnit*>(gWorld.GetCurrentPlayer()->GetUnit((unsigned long)inUnit)); }
extern "C" uintptr_t ee_screenscpp_mf_unit(uintptr_t inRec) { return (uintptr_t)static_cast<EEUnit*>(R(inRec)->mUnit); }
extern "C" void ee_screenscpp_mf_ghost_of_node(uintptr_t inRec)
{ EEGhostUnit::Create(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->GetMouseLocation(), static_cast<EETechTreeNode*>(R(inRec)->mNode)->mData->mUnitClass, gWorld.GetCurrentPlayer(), 0.0f, false); }
extern "C" void ee_screenscpp_mf_ghost_of_calamity(uintptr_t inRec)
{ EEGhostUnit::Create(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->GetMouseLocation(), static_cast<EECalamityClass*>(R(inRec)->mCalamity)->mUnitClassIndex, gWorld.GetCurrentPlayer(), 0.0f, false); }
extern "C" void ee_screenscpp_mf_gate_set_technology(uintptr_t inRec, int32_t inTechnology)
{ static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mBuildGate.mTechnology = (long)inTechnology; }
extern "C" void ee_screenscpp_mf_gate_wall_type_from_node(uintptr_t inRec)
{ static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mBuildGate.mWallType = static_cast<EETechTreeNode*>(R(inRec)->mNode)->GetUnitClass(0); }
extern "C" void ee_screenscpp_mf_gate_class_from_node(uintptr_t inRec)
{ static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mBuildGate.mUnitClass = (gUnitClassTable[static_cast<EETechTreeNode*>(R(inRec)->mNode)->mData->mUnitClass]); }
extern "C" void ee_screenscpp_mf_gate_initialize(uintptr_t inRec)
{ ee_screens_gate_initialize(&static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mBuildGate, (int32_t)gWorld.GetCurrentPlayer()->mPlayerID); }
extern "C" void ee_screenscpp_mf_wall_set_technology(uintptr_t inRec, int32_t inTechnology)
{ static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mBuildWall.mTechnology = (long)inTechnology; }
extern "C" void ee_screenscpp_mf_wall_initialize(uintptr_t inRec)
{ ee_screens_wall_initialize(&static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mBuildWall, (int32_t)gWorld.GetCurrentPlayer()->mPlayerID); }
extern "C" uint32_t ee_screenscpp_selected_count()
{ return (uint32_t)gWorld.GetCurrentPlayer()->mSelectedGroup->GetNumUnits(); }
extern "C" void ee_screenscpp_mf_caster_first(uintptr_t inRec)
{ R(inRec)->mCaster = gWorld.GetCurrentPlayer()->mSelectedGroup->GetUnit(0); }
extern "C" void ee_screenscpp_mf_caster_clear(uintptr_t inRec) { R(inRec)->mCaster = NULL; }
extern "C" bool ee_screenscpp_mf_has_caster(uintptr_t inRec) { return static_cast<EEUnit*>(R(inRec)->mCaster) != NULL; }
extern "C" void ee_screenscpp_mf_calamity_set(uintptr_t inRec, int32_t inCalamity)
{ static_cast<EEACalamity*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mCalamity)->SetCalamity((long)inCalamity, static_cast<EECalamityClass*>(R(inRec)->mCalamity)); }
extern "C" void ee_screenscpp_mf_calamity_set_caster(uintptr_t inRec)
{ static_cast<EEACalamity*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mCalamity)->SetCasterID(static_cast<EEUnit*>(R(inRec)->mCaster)->mUnitID); }
extern "C" int64_t ee_screenscpp_mf_calamity_target_kind(uintptr_t inRec)
{ return (int64_t)static_cast<EECalamityClass*>(R(inRec)->mCalamity)->GetTargetKind(); }
extern "C" void ee_screenscpp_mouse_shift_off() { gMouse->mShiftMode = false; }
extern "C" void ee_screenscpp_mouse_set_pointer(int32_t inPointer) { gMouse->mPointer = (long)inPointer; }
extern "C" void ee_screenscpp_mouse_set_bounds(float inLeft, float inTop, float inRight, float inBottom)
{ gMouse->SetBounds(GERectangle(inLeft, inTop, inRight, inBottom)); }
extern "C" void ee_screenscpp_server_unpause(uintptr_t inServer)
{ reinterpret_cast<EEServer*>(inServer)->UnpauseGame(); }
extern "C" uintptr_t ee_screenscpp_graphics() { return (uintptr_t)gGraphics; }
extern "C" void ee_screenscpp_gfx_clear_render_objects() { gGraphics->PostClearRenderObjects(); }
extern "C" void ee_screenscpp_mf_cancel_button_up(uintptr_t inRec, uintptr_t inEvent)
{ static_cast<EEUIFMainGame*>(R(inRec)->mForm)->GetControl(kMainGameCancelUIMode)->SetTriggerEvent(kControlTriggerButtonUp, reinterpret_cast<UIEvent*>(inEvent)); }
extern "C" void ee_screenscpp_mf_cancel_right_button_up(uintptr_t inRec, uintptr_t inEvent)
{ static_cast<EEUIFMainGame*>(R(inRec)->mForm)->GetControl(kMainGameCancelUIMode)->SetTriggerEvent(kControlTriggerRightButtonUp, reinterpret_cast<UIEvent*>(inEvent)); }
extern "C" void ee_screenscpp_mf_main_group_open(uintptr_t inRec)
{ static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mMainGroup->Open(static_cast<EEUIFMainGame*>(R(inRec)->mForm)); }
extern "C" void ee_screenscpp_mf_main_group_clear_l20(uintptr_t inRec)
{ static_cast<EEUIFMainGame*>(R(inRec)->mForm)->mMainGroup->mL20 = 0; }
extern "C" void ee_screenscpp_post_graphics_all() { EEPostGraphicsAll(); }
extern "C" void ee_screenscpp_gfx_fade_in(uint32_t inValue) { gGraphics->PostFadeIn(inValue); }
extern "C" void ee_screenscpp_mf_media_load(uintptr_t inRec) { static_cast<EEMedia*>(R(inRec)->mMedia)->LoadMedia(); }
extern "C" void ee_screenscpp_mf_old_media_unload(uintptr_t inRec) { static_cast<EEMedia*>(R(inRec)->mOldMedia)->UnloadMedia(); }
extern "C" void ee_screenscpp_shell_op(uintptr_t inControl, uint32_t inOp) { EEUIShell::Op(C(inControl), inOp); }
extern "C" uintptr_t ee_screenscpp_mf_background(uintptr_t inRec, uint32_t inId)
{ return (uintptr_t)static_cast<EEUIFMainGame*>(R(inRec)->mForm)->GetBackgroundByID((unsigned long)inId); }
extern "C" void ee_screenscpp_background_draw(uintptr_t inBackground)
{ reinterpret_cast<UIBackground*>(inBackground)->DrawBackground(reinterpret_cast<UIBackground*>(inBackground)->GetRectangle()); }
extern "C" float ee_screenscpp_mf_fps(uintptr_t inRec)
{ return gRasterizer->GetFramesPerSecond(static_cast<EEUIFMainGame*>(R(inRec)->mForm)->GetManager()->GetScreen()); }
// end shims5

// the saved flags' and the error strings' shims (ee_screens hud_more.rs)
extern "C" int64_t ee_screenscpp_mff_read_bool(uintptr_t inFile, uintptr_t inField) { return reinterpret_cast<FSFile*>(inFile)->Read(*reinterpret_cast<bool*>(inField)); }
extern "C" int64_t ee_screenscpp_mff_write_bool(uintptr_t inFile, bool inValue) { return reinterpret_cast<FSFile*>(inFile)->Write(inValue); }
extern "C" void ee_screenscpp_mff_error_enter(uintptr_t inForm) { static_cast<TSCriticalSection*>(reinterpret_cast<EEUIFMainGame*>(inForm)->mErrorCriticalSection)->Enter(); }
extern "C" void ee_screenscpp_mff_error_set(uintptr_t inForm, uint32_t inIndex, uintptr_t inText) { reinterpret_cast<EEUIFMainGame*>(inForm)->mStrings[inIndex] = *reinterpret_cast<const UWideString*>(inText); }
extern "C" void ee_screenscpp_mff_strings_changed(uintptr_t inForm) { reinterpret_cast<EEUIFMainGame*>(inForm)->mStringsChanged = true; }
extern "C" void ee_screenscpp_mff_error_leave(uintptr_t inForm) { static_cast<TSCriticalSection*>(reinterpret_cast<EEUIFMainGame*>(inForm)->mErrorCriticalSection)->Leave(); }

// ---- the rest of the original host (EEUIFMainGame::HostOp, the mouse modes' EEMouseModeOp; UI lane round 9:
// main_form_calls.rs), each one statement on the call record, the form or the world, and the cases' locals
#define MF(inRec) static_cast<EEUIFMainGame*>(static_cast<EEUIFMainGame*>(R(inRec)->mForm))
extern "C" int64_t ee_screenscpp_mf_gate_direction(uintptr_t inRec)
{ return (int64_t)MF(inRec)->mBuildGate.mDirection; }
extern "C" void ee_screenscpp_mf_set_gate_direction(uintptr_t inRec, int64_t inValue)
{ MF(inRec)->mBuildGate.mDirection = (decltype(MF(inRec)->mBuildGate.mDirection))inValue; }
extern "C" int64_t ee_screenscpp_mf_wall_state(uintptr_t inRec) { return (int64_t)MF(inRec)->mBuildWall.mState; }
extern "C" void ee_screenscpp_mf_set_wall_state(uintptr_t inRec, int64_t inValue)
{ MF(inRec)->mBuildWall.mState = (decltype(MF(inRec)->mBuildWall.mState))inValue; }
extern "C" bool ee_screenscpp_mf_tooltip_shadow(uintptr_t inRec) { return MF(inRec)->GetDrawToolTipShadow(); }
extern "C" void ee_screenscpp_mf_set_tooltip_shadow(uintptr_t inRec, bool inValue)
{ MF(inRec)->SetDrawToolTipShadow(inValue); }
extern "C" int64_t ee_screenscpp_mf_build_tech(uintptr_t inRec) { return (int64_t)static_cast<EEABuild*>(MF(inRec)->mBuild)->mTechnology; }
extern "C" void ee_screenscpp_mf_set_build_tech(uintptr_t inRec, int64_t inValue)
{ static_cast<EEABuild*>(MF(inRec)->mBuild)->mTechnology = (decltype(static_cast<EEABuild*>(MF(inRec)->mBuild)->mTechnology))inValue; }
extern "C" void ee_screenscpp_mf_log_line(uintptr_t inRec, int64_t inIndex)
{ static_cast<EEUIShellList*>(MF(inRec)->GetControl(kMainGameErrorLog))->SetLine((unsigned long)inIndex, MF(inRec)->mStrings[inIndex]); }
extern "C" int64_t ee_screenscpp_mf_poll(uintptr_t inRec) { return (int64_t)static_cast<TMPoll*>(MF(inRec)->mPoll)->Poll(); }
extern "C" void ee_screenscpp_mf_scroll(uintptr_t inRec) { static_cast<EEAScroll*>(MF(inRec)->mScroll)->Update(gMouse->mPosition); }
extern "C" void ee_screenscpp_mf_tile_x(int64_t inX) { EEScratch<EETileCoordinate>(kScratchMFTile).mX = (long)inX; }
extern "C" void ee_screenscpp_mf_tile_y(int64_t inY) { EEScratch<EETileCoordinate>(kScratchMFTile).mY = (long)inY; }
extern "C" bool ee_screenscpp_mf_find_gate_wall(long* inDirection) { return EEFindGateWall(EEScratch<EETileCoordinate>(kScratchMFTile), *inDirection); }
extern "C" void ee_screenscpp_mf_clear_ghosts() { gWorld.ClearGhostUnits(); }
extern "C" void ee_screenscpp_mf_tiles_clear() { EEScratch<std::vector<EETileCoordinate>>(kScratchMFTiles).clear(); }
extern "C" void ee_screenscpp_mf_tiles_add() { EEScratch<std::vector<EETileCoordinate>>(kScratchMFTiles).push_back(EEScratch<EETileCoordinate>(kScratchMFTile)); }
extern "C" void ee_screenscpp_mf_preview_wall_call(uintptr_t inRec)
{ EEPreviewWall(EEScratch<std::vector<EETileCoordinate>>(kScratchMFTiles), static_cast<EEUIBuildWall*>(R(inRec)->mWall)->mTechnology, static_cast<EEUIBuildWall*>(R(inRec)->mWall)->mPlayer); }
extern "C" void ee_screenscpp_mf_preview_wall_form(uintptr_t inRec)
{ EEPreviewWall(EEScratch<std::vector<EETileCoordinate>>(kScratchMFTiles), MF(inRec)->mBuildWall.mTechnology, MF(inRec)->mBuildWall.mPlayer); }
extern "C" void ee_screenscpp_mf_preview_placement_mouse() { EEPreviewPlacement(gMouse->mPosition); }
extern "C" int64_t ee_screenscpp_mf_population()
{ return (int64_t)gWorld.GetCurrentPlayer()->GetTotalPopulation(false, false); }
extern "C" int64_t ee_screenscpp_mf_population_limit()
{ return (int64_t)gWorld.GetCurrentPlayer()->GetPopulationLimit(false); }
extern "C" void ee_screenscpp_mf_label_player_color(uintptr_t inRec, int64_t inControl, int64_t inPlayer)
{ static_cast<UITextLabel*>(MF(inRec)->GetControl((unsigned long)inControl))->GetGEText()->SetFgColor(reinterpret_cast<EEPlayer*>(inPlayer)->mColor); }
extern "C" void ee_screenscpp_mf_wonder_format()
{ gResource->GetString(IDS_TIME_LEFT_UNTIL_WINS_WONDER_VICTORY, EEScratch<UWideString>(kScratchMFFormat)); }
extern "C" void ee_screenscpp_mf_wonder_print(uintptr_t inRec, int64_t inPlayer, int64_t inTime)
{ sprintf(R(inRec)->mText, EEScratch<UWideString>(kScratchMFFormat), (const char*)reinterpret_cast<EEPlayer*>(inPlayer)->mName, (unsigned long)inTime); }
extern "C" void ee_screenscpp_mf_text_time(uintptr_t inRec) { R(inRec)->mText = gPhysics->GetTimeString(); }
extern "C" bool ee_screenscpp_mf_comms_paused() { return gCommunications->GetIsGamePaused(); }
extern "C" void ee_screenscpp_mf_pause_text(uintptr_t inRec, int64_t inValue)
{ MF(inRec)->Rust(EE_SCREENS_MF_DO_PAUSE_TEXT, inValue); }
extern "C" int64_t ee_screenscpp_mf_lag_status() { return (int64_t)gWorld.mLagStatus; }
extern "C" void ee_screenscpp_mf_lag_color(uintptr_t inRec, int64_t inControl, int64_t inPlayer)
{ static_cast<UIBitmap*>(MF(inRec)->GetControl((unsigned long)inControl))->GetBitmap()->SetColor(gWorld.mPlayers[inPlayer]->mColor); }
extern "C" bool ee_screenscpp_mf_has_instructions()
{ return gWorld.mScenarioManager->mMediaStrings.GetSection(3).GetLength() > 0; }
extern "C" void ee_screenscpp_mf_update_cursor(uintptr_t inRec) { MF(inRec)->UpdateCursorKind(); }
extern "C" int64_t ee_screenscpp_mf_info_duration(uintptr_t inRec) { return (int64_t)static_cast<const EESMDialogMessage*>(R(inRec)->mMessage)->mDuration; }
extern "C" void ee_screenscpp_mf_chat_top(uintptr_t inRec, int64_t inValue)
{ MF(inRec)->Rust(EE_SCREENS_MF_DO_CHAT_TOP, inValue); }
extern "C" void ee_screenscpp_mf_hide_messages(uintptr_t inRec) { MF(inRec)->Rust(EE_SCREENS_MF_DO_HIDE_MESSAGES); }
extern "C" void ee_screenscpp_mf_create_error_lock(uintptr_t inRec)
{ ESVERIFY_AT(TSScheduler::CreateCriticalSection(reinterpret_cast<TSCriticalSection*&>(MF(inRec)->mErrorCriticalSection)) == keseOK, 633); }
extern "C" void ee_screenscpp_mf_create_group(uintptr_t inRec, int64_t inIndex)
{ MF(inRec)->CreateGroup((unsigned long)inIndex); }
extern "C" void ee_screenscpp_mf_new_unit_hack(uintptr_t inRec)
{ MF(inRec)->mUnitHack = new EEUIFMGUnitHack(MF(inRec)); }
extern "C" void ee_screenscpp_mf_wall_terminate(uintptr_t inRec)
{ ee_screens_wall_terminate(&MF(inRec)->mBuildWall); }
extern "C" void ee_screenscpp_mf_gate_terminate(uintptr_t inRec)
{ ee_screens_gate_terminate(&MF(inRec)->mBuildGate); }
extern "C" void ee_screenscpp_mf_node_load(uintptr_t inRec, int64_t inTechnology)
{ R(inRec)->mNode = gWorld.GetCurrentPlayer()->mTechnologies->Get((long)inTechnology); }
extern "C" bool ee_screenscpp_mf_has_node(uintptr_t inRec) { return static_cast<EETechTreeNode*>(R(inRec)->mNode) != NULL; }
extern "C" bool ee_screenscpp_mf_node_class_exists(uintptr_t inRec)
{ return gUnitClassTable[static_cast<EETechTreeNode*>(R(inRec)->mNode)->mData->mUnitClass] != NULL; }
extern "C" int64_t ee_screenscpp_mf_can_produce(uintptr_t inRec, int64_t inTechnology)
{ return (int64_t)gWorld.GetCurrentPlayer()->CanProduce((long)inTechnology, static_cast<EEUnit*>(R(inRec)->mUnit)->mComplexUnit->mUnitClass->GetUnitType(), 1, true, 3, &R(inRec)->mText); }
extern "C" void ee_screenscpp_mf_play_sound(int64_t inSound)
{ gSoundManager->Play(EESoundObject::Get((unsigned long)inSound)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL); }
extern "C" void ee_screenscpp_mf_chat_log_error(uintptr_t inRec)
{ gUserInterface->AddChatLogEntry(R(inRec)->mText, 0); }
extern "C" int64_t ee_screenscpp_mf_selected_count()
{ return (int64_t)gWorld.GetCurrentPlayer()->mSelectedGroup->GetNumUnits(); }
extern "C" int64_t ee_screenscpp_mf_node_calamity(uintptr_t inRec)
{ return (int64_t)static_cast<EETechTreeNode*>(R(inRec)->mNode)->mData->mCalamity; }
extern "C" void ee_screenscpp_mf_calamity_load(uintptr_t inRec, int64_t inCalamity)
{ R(inRec)->mCalamity = USingleton<EECalamityManager>::GetInstance()->GetCalamityClass((EECalamityEnum)inCalamity); }
extern "C" bool ee_screenscpp_mf_has_calamity(uintptr_t inRec) { return static_cast<EECalamityClass*>(R(inRec)->mCalamity) != NULL; }
extern "C" void ee_screenscpp_mf_calamity_cast(uintptr_t inRec)
{ static_cast<EEACalamity*>(MF(inRec)->mCalamity)->Cast(MF(inRec)->GetMouseLocation()); }
extern "C" bool ee_screenscpp_mf_user_paused() { return gWorld.mUserPaused; }
extern "C" void ee_screenscpp_mf_master_volume() { gSoundManager->SetMasterVolume(gMasterVolume); }
extern "C" void ee_screenscpp_mf_mouse_position(float inX, float inY) { gMouse->SetPosition(GE2DPoint(inX, inY)); }
extern "C" void ee_screenscpp_mf_fill_hack(uintptr_t inRec) { static_cast<EEUIFMGUnitHack*>(MF(inRec)->mUnitHack)->FillComboBox(); }
extern "C" bool ee_screenscpp_mf_has_techs() { return gWorld.GetCurrentPlayer()->mTechnologies != NULL; }
extern "C" void ee_screenscpp_mf_text_epoch_title(uintptr_t inRec)
{ gResource->GetString(EEEpochTitleID(gWorld.GetCurrentPlayer()->mTechnologies->mCurrentEpoch), R(inRec)->mText); }
extern "C" void ee_screenscpp_mf_unit_info_background(uintptr_t inRec)
{ MF(inRec)->mUnitInfoBackground = MF(inRec)->GetBackgroundByID(0x64); }
extern "C" void ee_screenscpp_mf_media_old_load(uintptr_t inRec)
{ R(inRec)->mOldMedia = EEMedia::Get(MF(inRec)->mMediaID); }
extern "C" bool ee_screenscpp_mf_has_media_old(uintptr_t inRec) { return static_cast<EEMedia*>(R(inRec)->mOldMedia) != NULL; }
extern "C" int64_t ee_screenscpp_mf_player_media_id()
{ return (int64_t)gWorld.GetCurrentPlayer()->mTechnologies->mMediaID; }
extern "C" bool ee_screenscpp_mf_using_carbon() { return gWorld.mUsingCarbon; }
extern "C" void ee_screenscpp_mf_media_get(uintptr_t inRec, int64_t inMedia)
{ R(inRec)->mMedia = EEMedia::Get((long)inMedia); }
extern "C" bool ee_screenscpp_mf_has_media(uintptr_t inRec) { return static_cast<EEMedia*>(R(inRec)->mMedia) != NULL; }
extern "C" void ee_screenscpp_mf_bg_texture(uintptr_t inRec, int64_t inBackground, int64_t inTexture)
{ static_cast<UIBitmapBackground*>(MF(inRec)->GetBackgroundByID((unsigned long)inBackground))->SetBitmapTexture(static_cast<EEGraphicObject*>(static_cast<EEMedia*>(R(inRec)->mMedia))->mTextures[inTexture]); }
extern "C" bool ee_screenscpp_md_captured(uintptr_t inRec) { return MF(inRec)->GetIsMouseCaptured(); }
extern "C" void ee_screenscpp_md_point_reset() { EEScratch<GE3DPoint>(kScratchMFPoint) = GE3DPoint(0.0f, 0.0f, 0.0f); }
extern "C" bool ee_screenscpp_md_point(uintptr_t inRec, float inX, float inY)
{ return EEPointUnitOrTile(GE2DPoint(inX, inY), reinterpret_cast<EETile*&>(R(inRec)->mTile), EEScratch<GE3DPoint>(kScratchMFPoint), true); }
extern "C" float ee_screenscpp_md_point_x() { return EEScratch<GE3DPoint>(kScratchMFPoint).GetX(); }
extern "C" float ee_screenscpp_md_point_y() { return EEScratch<GE3DPoint>(kScratchMFPoint).GetY(); }
extern "C" float ee_screenscpp_md_point_z() { return EEScratch<GE3DPoint>(kScratchMFPoint).GetZ(); }
extern "C" bool ee_screenscpp_md_point_xy(uintptr_t inRec, float inX, float inY, long* inTileX, long* inTileY)
{ return EEPointUnitOrTileXY(GE2DPoint(inX, inY), reinterpret_cast<EETile*&>(R(inRec)->mTile), *inTileX, *inTileY, true); }
extern "C" bool ee_screenscpp_md_tile_seen(uintptr_t inRec)
{ return static_cast<EETile*>(R(inRec)->mTile)->IsSeenBy(gWorld.GetCurrentPlayer()); }
extern "C" bool ee_screenscpp_md_pick(uintptr_t inRec, float inX, float inY)
{ return (R(inRec)->mPicked = gUnitPicker.PickUnit(GE2DPoint(inX, inY), 0)) != NULL; }
extern "C" bool ee_screenscpp_md_can_be_attacked(uintptr_t inRec)
{ return static_cast<EEUnit*>(R(inRec)->mPicked)->GetUnitClass()->CanBeAttacked(); }
extern "C" bool ee_screenscpp_md_has_picked(uintptr_t inRec) { return static_cast<EEUnit*>(R(inRec)->mPicked) != NULL; }
extern "C" bool ee_screenscpp_md_in_wonder_set(uintptr_t inRec)
{ return gUnitSetManager.Contains(static_cast<EEUnit*>(R(inRec)->mPicked)->mComplexUnit->GetUnitClass()->GetUnitType(), static_cast<EEComplexUnit*>(R(inRec)->mFirst)->mStats->mWonderUnitSet); }
extern "C" float ee_screenscpp_md_damage(int64_t inA, int64_t inB) { return EEDamageFactor((long)inA, (long)inB); }
extern "C" bool ee_screenscpp_md_is_caster(uintptr_t inRec)
{ return static_cast<EEComplexUnit*>(R(inRec)->mFirst)->mUnitID == static_cast<EEACalamity*>(MF(inRec)->mCalamity)->mCasterID; }
extern "C" void ee_screenscpp_md_in_range_set(bool inValue) { EEScratch<bool>(kScratchMFInRange) = inValue; }
extern "C" bool ee_screenscpp_md_can_convert(uintptr_t inRec, int64_t inUnit)
{ return EECanConvert(gWorld.GetCurrentPlayer()->mSelectedGroup->GetUnit((unsigned long)inUnit), static_cast<EEUnit*>(R(inRec)->mPicked)->GetComplexUnit(), EEScratch<bool>(kScratchMFInRange)); }
extern "C" bool ee_screenscpp_md_in_range() { return EEScratch<bool>(kScratchMFInRange); }
extern "C" void ee_screenscpp_md_send_load_garrison(uintptr_t inRec)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageLoadGarrison, {static_cast<EEUnit*>(R(inRec)->mPicked)->GetUnitID(), static_cast<EEComplexUnit*>(R(inRec)->mFirst)->mComplexUnit->mUnitID, 0}), kSlotNone); }
extern "C" void ee_screenscpp_md_send_unload(uintptr_t inRec, int64_t inX, int64_t inY)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageUnloadTransport, {static_cast<EEComplexUnit*>(R(inRec)->mFirst)->GetComplexUnit()->GetUnitID(), (long)inX, (long)inY}), kSlotNone); }
extern "C" void ee_screenscpp_md_send_heal(uintptr_t inRec)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageHealUnit, {static_cast<EEUnit*>(R(inRec)->mPicked)->mComplexUnit->mUnitID}), kSlotNone); }
extern "C" void ee_screenscpp_md_send_convert(uintptr_t inRec, bool inShielded)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageAttackUnit, {static_cast<EEUnit*>(R(inRec)->mPicked)->mComplexUnit->mUnitID, inShielded, true}), kSlotNone); }
extern "C" void ee_screenscpp_md_send_rally(uintptr_t inRec, float inX, float inY, int64_t inKind, int64_t inFlag)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageSetRallyGoal, {GE2DPoint(inX, inY), (long)inKind, static_cast<EEComplexUnit*>(R(inRec)->mFirst)->mComplexUnit->GetUnitID(), (unsigned char)inFlag}), kSlotNone); }
extern "C" void ee_screenscpp_md_order_at_none(float inX, float inY) { EEOrderAtCursor(GE2DPoint(inX, inY), NULL); }
extern "C" void ee_screenscpp_md_order_at_picked(uintptr_t inRec, float inX, float inY)
{ EEOrderAtCursor(GE2DPoint(inX, inY), static_cast<EEUnit*>(R(inRec)->mPicked)); }
extern "C" void ee_screenscpp_md_order_at_complex(uintptr_t inRec, float inX, float inY)
{ EEOrderAtCursor(GE2DPoint(inX, inY), static_cast<EEUnit*>(static_cast<EEUnit*>(R(inRec)->mPicked)->mComplexUnit)); }
extern "C" void ee_screenscpp_md_calamity_effect(uintptr_t inRec, float inX, float inY, float inZ)
{ EEMakeCalamityEffect(GE3DPoint(inX, inY, inZ), static_cast<EEUnit*>(R(inRec)->mPicked)); }
extern "C" void ee_screenscpp_md_preview(uintptr_t inRec) { EEPreviewPlacement(MF(inRec)->GetMouseLocation()); }
extern "C" int64_t ee_screenscpp_md_calamity_check(uintptr_t inRec, float inX, float inY)
{ return (int64_t)static_cast<EEACalamity*>(MF(inRec)->mCalamity)->Check(GE2DPoint(inX, inY)); }
extern "C" bool ee_screenscpp_md_in_game_view(uintptr_t inRec, float inX, float inY)
{ return MF(inRec)->IsInGameView(GE2DPoint(inX, inY)); }
extern "C" bool ee_screenscpp_md_entity_enabled(uintptr_t inRec, int64_t inGroup)
{ return MF(inRec)->GetEntityGroup((unsigned long)inGroup)->GetIsEnabled(); }
extern "C" void ee_screenscpp_md_set_entity_group(uintptr_t inRec, int64_t inGroup)
{ MF(inRec)->SetEntityGroup((long)inGroup); }
extern "C" void ee_screenscpp_md_gate_clear(uintptr_t inRec) { ee_screens_gate_clear_ghosts(static_cast<EEUIBuildGate*>(R(inRec)->mGate)); }

// the destructor's, CreateGroup's and IsInGameView's bodies: hud_bodies.rs over these (UI lane round 12)
#define MFB(f) ((EEUIFMainGame*)(f))
extern "C" void ee_screenscpp_mfb_group_main(uintptr_t inForm)
{ MFB(inForm)->mMainGroup = new EEUIFMGMainGroup(MFB(inForm)); }
extern "C" uintptr_t ee_screenscpp_mfb_unit_info(uintptr_t inForm)
{ return (uintptr_t)static_cast<UIBackground*>(MFB(inForm)->mUnitInfoBackground); }
extern "C" bool ee_screenscpp_mfb_background_visible(uintptr_t inBackground)
{ return ((UIBackground*)inBackground)->GetIsVisible(); }
extern "C" bool ee_screenscpp_mfb_background_contains(uintptr_t inBackground, float inX, float inY)
{ return ((UIBackground*)inBackground)->GetRectangle().Contains(GE2DPoint(inX, inY)); }
extern "C" bool ee_screenscpp_mfb_view_contains(float inX, float inY)
{ return gViewport.GetViewRectangle().Contains(GE2DPoint(inX, inY)); }
extern "C" void ee_screenscpp_mfb_grab_scroll_dereference(uintptr_t inForm) { static_cast<UIEvent*>(MFB(inForm)->mGrabScroll)->Dereference(); }
extern "C" void ee_screenscpp_mfb_release_grab_scroll() { EEReleaseGrabScroll(0); }
extern "C" void ee_screenscpp_mfb_wheel_zoom_dereference(uintptr_t inForm) { static_cast<UIEvent*>(MFB(inForm)->mWheelZoom)->Dereference(); }
extern "C" void ee_screenscpp_mfb_scroll_dereference(uintptr_t inForm) { static_cast<EEAScroll*>(MFB(inForm)->mScroll)->Dereference(); }
extern "C" void ee_screenscpp_mfb_mouse_click_dereference(uintptr_t inForm) { static_cast<EEAMouseClick*>(MFB(inForm)->mMouseClick)->Dereference(); }
extern "C" void ee_screenscpp_mfb_lasso_dereference(uintptr_t inForm) { static_cast<EEALasso*>(MFB(inForm)->mLasso)->Dereference(); }
extern "C" void ee_screenscpp_mfb_destroy_error_section(uintptr_t inForm)
{ TSScheduler::DestroyCriticalSection(static_cast<TSCriticalSection*>(MFB(inForm)->mErrorCriticalSection)); }
extern "C" void ee_screenscpp_mfb_group_delete(uintptr_t inForm) { delete MFB(inForm)->mMainGroup; }
extern "C" void ee_screenscpp_mfb_poll_delete(uintptr_t inForm) { delete static_cast<TMPoll*>(MFB(inForm)->mPoll); }
extern "C" void ee_screenscpp_mfb_build_dereference(uintptr_t inForm) { static_cast<EEABuild*>(MFB(inForm)->mBuild)->Dereference(); }
extern "C" void ee_screenscpp_mfb_calamity_dereference(uintptr_t inForm) { static_cast<EEACalamity*>(MFB(inForm)->mCalamity)->Dereference(); }
extern "C" void ee_screenscpp_mfb_unit_hack_delete(uintptr_t inForm) { delete static_cast<EEUIFMGUnitHack*>(MFB(inForm)->mUnitHack); }
#undef UH_OBJECTS
#undef CR
#undef CRU
#undef CRT
#undef MF
#undef MFB

// ==== EEUIFMGAttribute.cpp ==========================================================================================
// C:\EEX\Empire Earth\User Interface Thread\EEUIFMGAttribute.cpp (file name inferred: no __FILE__; RTTI class
// EEUIFMGAttribute). The main game form's attribute button: 0x26783d..0x267ad9 (ctor, ??_G, dtor, CopyData,
// DrawTextA, GetIconRectangle; EEUIFMGErrorLog's ctor at 0x267ad9). Unit flags: the exe module set.
// Destroys the attribute button and its level text
EEUIFMGAttribute::~EEUIFMGAttribute() { delete static_cast<GEText*>(this->mLevelText); }

// the Rust bodies' shims (ee_screens hud_more.rs: the constructor's level text, CopyData, DrawTextA)
#define ATT(a) reinterpret_cast<EEUIFMGAttribute*>(a)
extern "C" uintptr_t ee_screenscpp_att_new_text(uintptr_t inAttr, uintptr_t inFont) { return (uintptr_t)(ATT(inAttr)->mLevelText = new GEText(reinterpret_cast<GEFont*>(inFont))); }
extern "C" void ee_screenscpp_att_text_transparent(uintptr_t inAttr) { static_cast<GEText*>(ATT(inAttr)->mLevelText)->SetIsTransparent(true); }
extern "C" void ee_screenscpp_att_text_fg(uintptr_t inAttr, uintptr_t inColor) { static_cast<GEText*>(ATT(inAttr)->mLevelText)->SetFgColor(*reinterpret_cast<const GEColor*>(inColor)); }
extern "C" float ee_screenscpp_att_left(uintptr_t inAttr) { return ATT(inAttr)->GetRectangle().GetLeft(); }
extern "C" float ee_screenscpp_att_top(uintptr_t inAttr) { return ATT(inAttr)->GetRectangle().GetTop(); }
extern "C" uint32_t ee_screenscpp_att_border(uintptr_t inAttr) { return (uint32_t)ATT(inAttr)->GetBorderThickness(); }
extern "C" void ee_screenscpp_att_text_set_location(uintptr_t inAttr, float inX, float inY) { static_cast<GEText*>(ATT(inAttr)->mLevelText)->SetLocation(inX, inY); }
extern "C" int64_t ee_screenscpp_att_base_copy_data(uintptr_t inAttr) { return ATT(inAttr)->EEUIBUpgrade::CopyData(); }
extern "C" int64_t ee_screenscpp_att_queued(uintptr_t inAttr) { return ATT(inAttr)->mNumQueued; }
extern "C" void ee_screenscpp_att_level_text(uintptr_t inAttr, EeScreensShimText inText) { static_cast<GEText*>(ATT(inAttr)->mLevelText)->SetText(EEShimText(inText)); }
extern "C" void ee_screenscpp_att_set_show(uintptr_t inAttr, bool inShow) { ATT(inAttr)->mShowLevel = inShow; }
extern "C" bool ee_screenscpp_att_show(uintptr_t inAttr) { return ATT(inAttr)->mShowLevel; }
extern "C" float ee_screenscpp_att_text_x(uintptr_t inAttr) { return static_cast<GEText*>(ATT(inAttr)->mLevelText)->GetLocation().GetX(); }
extern "C" float ee_screenscpp_att_text_y(uintptr_t inAttr) { return static_cast<GEText*>(ATT(inAttr)->mLevelText)->GetLocation().GetY(); }
extern "C" uint32_t ee_screenscpp_att_height(uintptr_t inAttr) { return (uint32_t)ATT(inAttr)->GetHeight(); }
extern "C" uint32_t ee_screenscpp_att_width(uintptr_t inAttr) { return (uint32_t)ATT(inAttr)->GetWidth(); }
extern "C" int64_t ee_screenscpp_att_base_draw_text(uintptr_t inAttr) { return ATT(inAttr)->EEUIBUpgrade::DrawTextA(); }
extern "C" int64_t ee_screenscpp_att_draw_level(uintptr_t inAttr, float inLeft, float inTop, float inRight, float inBottom)
{
    return ATT(inAttr)->GetForm()->DrawClippedText(GERectangle(GE2DPoint(inLeft, inTop), GE2DPoint(inRight, inBottom)), static_cast<GEText*>(ATT(inAttr)->mLevelText), ATT(inAttr));
}

// Returns the icon's rectangle: the whole button, or a 17-pixel strip at its left in free camera mode
GERectangle EEUIFMGAttribute::GetIconRectangle() const
{
    return !gFreeCamera ? this->GetRectangle()
                        : GERectangle(GE2DPoint(this->GetRectangle().GetLeft(), this->GetRectangle().GetTop()), 17.0f,
                                      this->GetRectangle().GetHeight());
}
#undef ATT

// ==== EEUIFMGMainGroup.cpp ==========================================================================================
// C:\EEX\Empire Earth\User Interface Thread\EEUIFMGMainGroup.cpp (__FILE__ 0x4cdc20, ESVERIFY lines 415/462). The
// main game form's main group (RTTI EEUIFMGMainGroup > EEUIFMGGroup; vtable 0x44ac84): 0x267ec8..0x2691b2 (ctor,
// Virtual10 (inline), ??_G, dtor, Virtual04, the selection members, EEUIEMGHotKeyStatusButton's ??_G after its first
// user, Virtual14, then the inline COMDATs EETechTreeNode::GetHotKey, EEComplexUnit::GetCitizenBuildTarget and
// EEButtonObject::GetHotKey; the two GetHotKeys are their classes' units' now: the command buttons named this unit
// for them). Unit flags: the exe module set.
// Creates the main game form's main panel group with its status box, no unit shown
EEUIFMGMainGroup::EEUIFMGMainGroup(EEUIFMainGame* inOwner)
    : EeScreensMainGroupFields(), mOwner(inOwner), mStatusBox(new EEUIFMGStatusBox(inOwner))
{
    ee_screens_main_group_fields(this, static_cast<EeScreensMainGroupFields*>(this));   // (the record zeroed)
}

// (EETechTreeNode::GetHotKey and EEButtonObject::GetHotKey, the inline COMDATs the original emitted here: their
// classes' units, Tech Tree/EETechTreeNode.cpp and Database/EEButtonObject.cpp)

// The panel groups' values (ee_screens panel_groups_ffi.rs, which prints their texts): each one call, no logic
extern "C" int32_t ee_screenscpp_group_conversions() { return gWorld.GetCurrentPlayer()->mSavedGroups.GetLastValue(5); }
extern "C" int32_t ee_screenscpp_group_conversions_max() { return gWorldTable.GetLongValue(kWorldConversionsStatisticMax); }
extern "C" int32_t ee_screenscpp_group_wonders_to_win() { return gWorld.mNumberOfWondersToWin; }
extern "C" bool ee_screenscpp_group_has_player() { return gWorld.GetCurrentPlayer() != NULL; }
extern "C" int32_t ee_screenscpp_group_start_epoch() { return gWorld.GetCurrentPlayer()->GetTechTree()->mStartEpoch; }
extern "C" int32_t ee_screenscpp_group_end_epoch() { return gWorld.GetCurrentPlayer()->GetTechTree()->mEndEpoch; }
extern "C" int32_t ee_screenscpp_group_epoch_title(int32_t inEpoch) { return EEEpochTitleID(inEpoch); }

// ---- the one-statement shims of ee_screens main_group_calls.rs (UI lane round 8: the group's host, ShowCommands,
// the status buttons' hot keys): each one call, no logic
#define MG(x) reinterpret_cast<EEUIFMGMainGroup*>(x)
#define MGU(x) reinterpret_cast<EEUnit*>(x)
#define MGC(x) reinterpret_cast<EEComplexUnit*>(x)
#define MGP(x) reinterpret_cast<EEPlayer*>(x)
#define MGK(x) reinterpret_cast<UIHotKey*>(x)
#define SBN(x) reinterpret_cast<EETechTreeNode*>(x)
#define SBB(x) reinterpret_cast<EEUIFMGStatusButton*>(x)
// the status box's control slots EE_SCREENS_MGR_OP_BOX_BIND binds (main_group.rs' order)
static void* EEUIFMGStatusBox::* const kMGBoxSlots[8] = {
    &EEUIFMGStatusBox::mC8C, &EEUIFMGStatusBox::mC94, &EEUIFMGStatusBox::mC90, &EEUIFMGStatusBox::mC84,
    &EEUIFMGStatusBox::mC88, &EEUIFMGStatusBox::mC98, &EEUIFMGStatusBox::mCAC, &EEUIFMGStatusBox::mCA8};
extern "C" uintptr_t ee_screenscpp_mg_owner(uintptr_t inGroup) { return (uintptr_t)MG(inGroup)->mOwner; }
extern "C" uintptr_t ee_screenscpp_mg_main_form() { return (uintptr_t)gUIManager->GetMainGameForm(); }
extern "C" void ee_screenscpp_mg_remove_hot_keys(uintptr_t inGroup)
{ MG(inGroup)->mOwner->RemoveAllHotKeysForAGroup(2); }
extern "C" void ee_screenscpp_mg_hide(uintptr_t inGroup, uint32_t inControl)
{ MG(inGroup)->mOwner->GetControl(inControl)->SetDisplayMode(kDisplayModeHidden); }
extern "C" void ee_screenscpp_mg_show(uintptr_t inGroup, uint32_t inControl)
{ MG(inGroup)->mOwner->GetControl(inControl)->SetDisplayMode(kDisplayModeShown); }
extern "C" uintptr_t ee_screenscpp_mg_button(uintptr_t inGroup, uint32_t inControl)
{ return (uintptr_t)static_cast<EEUIFMGStatusButton*>(MG(inGroup)->mOwner->GetControl(inControl)); }
extern "C" uintptr_t ee_screenscpp_mg_control(uintptr_t inGroup, uint32_t inControl)
{ return (uintptr_t)MG(inGroup)->mOwner->GetControl(inControl); }
extern "C" bool ee_screenscpp_mg_cancel_hidden(uintptr_t inGroup)
{ return MG(inGroup)->mOwner->GetControl(kMainGameCancelUIMode)->GetDisplayMode() == kDisplayModeHidden; }
extern "C" int64_t ee_screenscpp_mg_mode(uintptr_t inGroup) { return MG(inGroup)->mMode; }
extern "C" uintptr_t ee_screenscpp_mg_unit_complex(uintptr_t inUnit) { return (uintptr_t)MGU(inUnit)->mComplexUnit; }
extern "C" bool ee_screenscpp_mg_unit_alive(uintptr_t inUnit) { return MGU(inUnit)->mHitPoints != 0; }
extern "C" uintptr_t ee_screenscpp_mg_unit_player(uintptr_t inUnit) { return (uintptr_t)MGU(inUnit)->mPlayer; }
extern "C" bool ee_screenscpp_mg_unit_in_fanatic_set(uintptr_t inUnit)
{ return gUnitSetManager.GetSet(0x2f)->Contains(MGU(inUnit)->mUnitClass->mUnitType); }
extern "C" uint32_t ee_screenscpp_mg_unit_class_type(uintptr_t inUnit)
{ return (uint32_t)MGU(inUnit)->mUnitClass->mUnitType; }
extern "C" uint32_t ee_screenscpp_mg_unit_id(uintptr_t inUnit) { return (uint32_t)MGU(inUnit)->mUnitID; }
extern "C" uint64_t ee_screenscpp_mg_class_command(uintptr_t inUnit, uint32_t inIndex)
{ return MGU(inUnit)->mUnitClass->mCommands[inIndex]; }
extern "C" int64_t ee_screenscpp_mg_research_count(uintptr_t inUnit)
{ return MGU(inUnit)->mUnitClass->GetNumResearchTechnologies(); }
extern "C" int64_t ee_screenscpp_mg_research_tech(uintptr_t inUnit, int64_t inIndex)
{ return MGU(inUnit)->mUnitClass->mResearchTechnologies[inIndex]; }
extern "C" bool ee_screenscpp_mg_complex_built(uintptr_t inComplex) { return MGC(inComplex)->mIsBuilt; }
extern "C" bool ee_screenscpp_mg_complex_commandable(uintptr_t inComplex) { return MGC(inComplex)->mIsCommandable; }
extern "C" bool ee_screenscpp_mg_complex_rally(uintptr_t inComplex)
{ return MGC(inComplex)->mUnitClass->HasRallyPoint(); }
extern "C" bool ee_screenscpp_mg_complex_airport(uintptr_t inComplex)
{ return MGC(inComplex)->mUnitClass->mUnitType == kUnitTypeAirport; }
extern "C" bool ee_screenscpp_mg_research_queued(uintptr_t inComplex, uintptr_t inNode)
{ return MGC(inComplex)->IsResearchQueued(SBN(inNode)->mData->mID); }
extern "C" uintptr_t ee_screenscpp_mg_current_player() { return (uintptr_t)gWorld.GetCurrentPlayer(); }
extern "C" bool ee_screenscpp_mg_player_observer(uintptr_t inPlayer) { return MGP(inPlayer)->mObserverMode; }
extern "C" bool ee_screenscpp_mg_player_human(uintptr_t inPlayer) { return MGP(inPlayer)->IsHuman() == true; }
extern "C" bool ee_screenscpp_mg_player_fanaticism(uintptr_t inPlayer)
{ return gCivPowersManager.HasPower(MGP(inPlayer), kCivPowerFanaticism); }
extern "C" bool ee_screenscpp_mg_player_jit(uintptr_t inPlayer)
{ return gCivPowersManager.HasPower(MGP(inPlayer), kCivPowerJustInTimeManufacturing); }
extern "C" bool ee_screenscpp_mg_player_jit_on(uintptr_t inPlayer) { return MGP(inPlayer)->mJITManufacturing; }
extern "C" uintptr_t ee_screenscpp_mg_player_node(uintptr_t inPlayer, int64_t inTechnology)
{ return (uintptr_t)MGP(inPlayer)->GetTechTree()->Get(inTechnology); }
extern "C" uintptr_t ee_screenscpp_mg_button_object(uint32_t inIndex)
{ return (uintptr_t)EEButtonObject::Get(inIndex); }
extern "C" int64_t ee_screenscpp_mg_object_position(uintptr_t inObject)
{ return reinterpret_cast<EEButtonObject*>(inObject)->mPosition; }
extern "C" int64_t ee_screenscpp_mg_object_id(uintptr_t inObject)
{ return reinterpret_cast<EEButtonObject*>(inObject)->mID; }
extern "C" uintptr_t ee_screenscpp_mg_node_object(uintptr_t inNode) { return (uintptr_t)SBN(inNode)->mButton; }
extern "C" bool ee_screenscpp_mg_node_available(uintptr_t inNode) { return SBN(inNode)->GetIsAvailable(); }
extern "C" bool ee_screenscpp_mg_node_allowed(uintptr_t inNode) { return SBN(inNode)->mIsAllowed; }
extern "C" void ee_screenscpp_mg_hot_key_always_fire(uintptr_t inKey) { MGK(inKey)->SetAlwaysFire(true); }
extern "C" void ee_screenscpp_mg_add_hot_key(uintptr_t inGroup, uintptr_t inKey)
{ MG(inGroup)->mOwner->AddHotKey(*MGK(inKey), 2); }
extern "C" uintptr_t ee_screenscpp_mg_hot_key_copy(uintptr_t inGroup, uintptr_t inKey)
{ return (uintptr_t)new UIHotKey(MG(inGroup)->mOwner, *MGK(inKey)); }
extern "C" void ee_screenscpp_mg_hot_key_shift(uintptr_t inKey) { MGK(inKey)->SetModifier(kKeyShift, 0); }
extern "C" void ee_screenscpp_mg_hot_key_delete(uintptr_t inKey) { delete MGK(inKey); }
extern "C" uintptr_t ee_screenscpp_mg_hot_key_event_new(uintptr_t inButton, bool inPressed)
{ return (uintptr_t)static_cast<UIEvent*>(new EEScreenEvent(EE_SCREENS_EV_HOT_KEY, inButton, inPressed)); }
extern "C" void ee_screenscpp_mg_hot_key_pressed_event(uintptr_t inKey, uintptr_t inEvent)
{ MGK(inKey)->SetJustPressedEvent(reinterpret_cast<UIEvent*>(inEvent)); }
extern "C" void ee_screenscpp_mg_hot_key_unpressed_event(uintptr_t inKey, uintptr_t inEvent)
{ MGK(inKey)->SetJustUnpressedEvent(reinterpret_cast<UIEvent*>(inEvent)); }
extern "C" void ee_screenscpp_mg_button_hot_key_down(uintptr_t inButton) { SBB(inButton)->HotKeyDown(); }
extern "C" void ee_screenscpp_mg_saved_groups_reset() { gWorld.GetCurrentPlayer()->mSavedGroups.Reset(0x14, 1); }
extern "C" void ee_screenscpp_mg_remove_cloak(uintptr_t inUnit)
{ EERemoveCloakEffect(static_cast<EEComplexUnit*>(reinterpret_cast<RTSUnit*>(inUnit))); }
extern "C" void ee_screenscpp_mg_add_cloak(uintptr_t inUnit) { EEAddCloakEffect(MGU(inUnit)); }
extern "C" void ee_screenscpp_mg_box_fill(uintptr_t inGroup, uintptr_t inUnit)
{ MG(inGroup)->mStatusBox->Fill(MGU(inUnit)); }
extern "C" void ee_screenscpp_mg_box_refresh(uintptr_t inGroup, uintptr_t inUnit)
{ MG(inGroup)->mStatusBox->Refresh(MGU(inUnit)); }
extern "C" void ee_screenscpp_mg_box_hide(uintptr_t inGroup) { MG(inGroup)->mStatusBox->HideBackgrounds(); }
extern "C" void ee_screenscpp_mg_box_queued(uintptr_t inGroup, uintptr_t inUnit)
{ MG(inGroup)->mStatusBox->UpdateQueuedCounts(MGU(inUnit)->GetComplexUnit(), MGU(inUnit)->GetComplexUnit()->GetPlayer()->GetResearchedImprovement(MGU(inUnit)->GetComplexUnit()->GetClass()->GetUnitType())); }
extern "C" void ee_screenscpp_mg_box_bind(uintptr_t inGroup, uint32_t inSlot, uint32_t inControl)
{ MG(inGroup)->mStatusBox->*kMGBoxSlots[inSlot] = MG(inGroup)->mOwner->GetControl(inControl); }
extern "C" void ee_screenscpp_mg_box_pair(uintptr_t inGroup, uint32_t inIndex)
{ MG(inGroup)->mStatusBox->SetControls((long)inIndex, MG(inGroup)->mOwner->GetControl(inIndex + 0x45), MG(inGroup)->mOwner->GetControl(inIndex + 0x20)); }
extern "C" void ee_screenscpp_mg_icon_bar_update(uintptr_t inControl)
{ EEUIShell::Op(reinterpret_cast<UIControl*>(inControl), EE_SCREENS_SKOP_IB_UPDATE); }
extern "C" bool ee_screenscpp_mg_unit_behavior_is(uintptr_t inUnit, int64_t inBehavior)
{ return MGU(inUnit)->GetComplexUnit()->GetBehavior() == EEUnitBehavior::Get(inBehavior); }
extern "C" bool ee_screenscpp_mg_object_has_hot_key(uint32_t inIndex)
{ return EEButtonObject::Get(inIndex)->GetHotKey() != NULL; }
extern "C" uintptr_t ee_screenscpp_mg_object_hot_key(uint32_t inIndex)
{ return (uintptr_t)EEButtonObject::Get(inIndex)->GetHotKey(); }
extern "C" void ee_screenscpp_mg_minutes_seconds(uint32_t inTime, unsigned long* inMinutes, unsigned long* inSeconds)
{ gPhysics->GetMinutesAndSeconds(inMinutes, inSeconds, inTime); }
extern "C" void ee_screenscpp_mg_timer_text(uintptr_t inGroup, uint32_t inControl, EeScreensShimText inText)
{ static_cast<UIButton*>(MG(inGroup)->mOwner->GetControl(inControl))->SetText(EEShimText(inText), gU531930); }
extern "C" uintptr_t ee_screenscpp_mg_world_unit(uint32_t inId)
{ return (uintptr_t)gWorld.GetUnit((unsigned long)inId); }
extern "C" uintptr_t ee_screenscpp_mg_player_unit(uint32_t inId)
{ return (uintptr_t)gWorld.GetCurrentPlayer()->GetUnit((unsigned long)inId); }
extern "C" int64_t ee_screenscpp_mg_formation_type()
{ return gWorld.GetCurrentPlayer()->GetSelectedGroup()->GetFormation()->GetType(); }
extern "C" void ee_screenscpp_mg_verify(int64_t inLine) { ESVERIFY_AT(false, (long)inLine); }
extern "C" int64_t ee_screenscpp_mg_time_left(uintptr_t inUnit, int64_t inQueue)
{ return MGU(inUnit)->mComplexUnit->mGoalQueues[inQueue].GetTimeLeft(); }
extern "C" int64_t ee_screenscpp_mg_queue_size(uintptr_t inUnit)
{ return (int64_t)MGU(inUnit)->GetComplexUnit()->mCitizenBuildQueue.size(); }
extern "C" int64_t ee_screenscpp_mg_queue_key(uintptr_t inUnit, uint32_t inIndex)
{ return MGU(inUnit)->GetComplexUnit()->GetCitizenBuildTarget(inIndex)->GetClass()->GetTechnology(); }

// the command buttons as the rest of the game shows them (the saved group's selection: ee/EEGameUI.h)
static const bool sShowCommandsRegistered =
    (gEEGameUI.mShowCommands = [](EEUIFMGMainGroup* inGroup, EEUnit* inUnit) { inGroup->ShowCommands(inUnit); }, true);
#undef MG
#undef MGU
#undef MGC
#undef MGP
#undef MGK
#undef SBN
#undef SBB

// ==== EEUIFMGStatusBar.cpp ==========================================================================================
// C:\EEX\Empire Earth\User Interface Thread\EEUIFMGStatusBar.cpp (__FILE__ 0x4cdcb0, ESVERIFY lines 125/148). An
// improvement button drawn as an icon, a status bar and two value labels (RTTI EEUIFMGStatusBar > EEUIBUpgrade;
// vtable 0x44acc4): 0x2691b2..0x269ca8 (ctor, ??_G, GEFontBitmap's local ??_G, dtor, the overrides, MoveIcon).
// Unit flags: the exe module set.
// ---- the one-statement shims of ee_screens status_bar_calls.rs (UI lane round 8: the constructor's layout, the
// destructor, CopyData, DrawPolygons, DrawTextA): each one call, no logic
#define SBA(x) reinterpret_cast<EEUIFMGStatusBar*>(x)
#define SBAR(x) (*reinterpret_cast<const GERectangle*>(x))
#define SBAF(x) reinterpret_cast<UIForm*>(x)
static void* EEUIFMGStatusBar::* const kSBAValues[2] = {&EEUIFMGStatusBar::mValue, &EEUIFMGStatusBar::mValue2};
extern "C" void ee_screenscpp_sba_set_pressed(uintptr_t inBar, bool inPressed) { SBA(inBar)->mPressed = inPressed; }
extern "C" float ee_screenscpp_sba_border(uintptr_t inBar) { return (float)SBA(inBar)->GetBorderThickness(); }
extern "C" float ee_screenscpp_sba_rect_left(uintptr_t inRect) { return SBAR(inRect).GetLeft(); }
extern "C" float ee_screenscpp_sba_rect_top(uintptr_t inRect) { return SBAR(inRect).GetTop(); }
extern "C" float ee_screenscpp_sba_rect_right(uintptr_t inRect) { return SBAR(inRect).GetRight(); }
extern "C" float ee_screenscpp_sba_rect_bottom(uintptr_t inRect) { return SBAR(inRect).GetBottom(); }
extern "C" float ee_screenscpp_sba_rect_width(uintptr_t inRect) { return SBAR(inRect).GetWidth(); }
extern "C" float ee_screenscpp_sba_rect_height(uintptr_t inRect) { return SBAR(inRect).GetHeight(); }
extern "C" void ee_screenscpp_sba_icon_new(uintptr_t inBar, uintptr_t inForm, float inLeft, float inTop, float inRight, float inBottom)
{ SBA(inBar)->mIcon = new UIBitmap(SBA(inBar)->GetId(), NULL, GERectangle(inLeft, inTop, inRight, inBottom), SBAF(inForm), SBA(inBar)->GetIndex()); }
extern "C" void ee_screenscpp_sba_bar_new(uintptr_t inBar, uintptr_t inForm, float inLeft, float inTop, float inRight, float inBottom)
{ SBA(inBar)->mBar = new UIStatusBar(SBA(inBar)->GetId(), GERectangle(inLeft, inTop, inRight, inBottom), SBAF(inForm), SBA(inBar)->GetIndex(), 0); }
extern "C" void ee_screenscpp_sba_bar_verify(uintptr_t inBar) { ESVERIFY_AT(static_cast<UIStatusBar*>(SBA(inBar)->mBar), 125); }
extern "C" void ee_screenscpp_sba_bar_green(uintptr_t inBar)
{ static_cast<UIStatusBar*>(SBA(inBar)->mBar)->SetStatusBarColor(GEColor(0.0f, 1.0f, 0.0f)); }
extern "C" void ee_screenscpp_sba_bar_grey(uintptr_t inBar) { static_cast<UIStatusBar*>(SBA(inBar)->mBar)->SetBgColor(GEColor(0.5f, 0.5f, 0.5f)); }
extern "C" void ee_screenscpp_sba_bar_opaque(uintptr_t inBar) { static_cast<UIStatusBar*>(SBA(inBar)->mBar)->SetBgOpacity(1.0f); }
extern "C" float ee_screenscpp_sba_bar_top(uintptr_t inBar) { return static_cast<UIStatusBar*>(SBA(inBar)->mBar)->GetRectangle().GetTop(); }
extern "C" float ee_screenscpp_sba_bar_bottom(uintptr_t inBar) { return static_cast<UIStatusBar*>(SBA(inBar)->mBar)->GetRectangle().GetBottom(); }
extern "C" uintptr_t ee_screenscpp_sba_closest_font(uint32_t inHeight)
{ return (uintptr_t)UIFontTable::CreateClosestFont(1, (unsigned long)inHeight); }
extern "C" bool ee_screenscpp_sba_font_has_bitmap(uintptr_t inFont)
{ return reinterpret_cast<GEFont*>(inFont)->GetFontBitmap() != NULL; }
extern "C" bool ee_screenscpp_sba_form_gdi(uintptr_t inForm) { return SBAF(inForm)->GetUseGDIForText(); }
extern "C" void ee_screenscpp_sba_font_bitmap_new(uintptr_t inFont, uintptr_t inForm)
{ (new GEFontBitmap(SBAF(inForm)->GetManager()->GetRasterizer(), reinterpret_cast<GEFont*>(inFont), SBAF(inForm)->GetManager()->GetScreen()))->Dereference(); }
extern "C" void ee_screenscpp_sba_font_dereference(uintptr_t inFont)
{ reinterpret_cast<GEFont*>(inFont)->Dereference(); }
extern "C" void ee_screenscpp_sba_value_new(uintptr_t inBar, uint32_t inWhich, uintptr_t inFont, uintptr_t inForm, float inLeft, float inTop, float inRight, float inBottom)
{ SBA(inBar)->*kSBAValues[inWhich] = new UITextLabel(UWideString("888"), reinterpret_cast<GEFont*>(inFont), SBA(inBar)->GetId(), GERectangle(inLeft, inTop, inRight, inBottom), SBAF(inForm), 0, SBA(inBar)->GetIndex(), 0); }
extern "C" void ee_screenscpp_sba_value_verify(uintptr_t inBar) { ESVERIFY_AT(static_cast<UITextLabel*>(SBA(inBar)->mValue), 148); }
extern "C" void ee_screenscpp_sba_value_right(uintptr_t inBar, uint32_t inWhich)
{ static_cast<UITextLabel*>(SBA(inBar)->*kSBAValues[inWhich])->SetAlignment(kTextAlignRight); }
extern "C" void ee_screenscpp_sba_value_white(uintptr_t inBar, uint32_t inWhich)
{ static_cast<UITextLabel*>(SBA(inBar)->*kSBAValues[inWhich])->GetGEText()->SetFgColor(GEColor(1.0f, 1.0f, 1.0f)); }
extern "C" void ee_screenscpp_sba_value_transparent(uintptr_t inBar, uint32_t inWhich)
{ static_cast<UITextLabel*>(SBA(inBar)->*kSBAValues[inWhich])->GetGEText()->SetIsTransparent(true); }
extern "C" void ee_screenscpp_sba_bitmap140_new(uintptr_t inBar) { SBA(inBar)->mBitmap140 = new GEBitmap(); }
extern "C" void ee_screenscpp_sba_bitmap144_new(uintptr_t inBar) { SBA(inBar)->mBitmap144 = new GEBitmap(); }
extern "C" void ee_screenscpp_sba_count_new(uintptr_t inBar, uintptr_t inFont)
{ SBA(inBar)->mCount = new GEText(reinterpret_cast<GEFont*>(inFont)); }
extern "C" float ee_screenscpp_sba_left(uintptr_t inBar) { return SBA(inBar)->GetRectangle().GetLeft(); }
extern "C" float ee_screenscpp_sba_top(uintptr_t inBar) { return SBA(inBar)->GetRectangle().GetTop(); }
extern "C" void ee_screenscpp_sba_count_at(uintptr_t inBar, float inX, float inY)
{ static_cast<GEText*>(SBA(inBar)->mCount)->SetLocation(inX, inY); }
extern "C" void ee_screenscpp_sba_count_white(uintptr_t inBar)
{ static_cast<GEText*>(SBA(inBar)->mCount)->SetFgColor(GEColor(1.0f, 1.0f, 1.0f)); }
extern "C" void ee_screenscpp_sba_count_transparent(uintptr_t inBar) { static_cast<GEText*>(SBA(inBar)->mCount)->SetIsTransparent(true); }
extern "C" void ee_screenscpp_sba_set_show_count(uintptr_t inBar, bool inShow) { SBA(inBar)->mShowCount = inShow; }
extern "C" void ee_screenscpp_sba_black(uintptr_t inBar) { SBA(inBar)->SetBgColor(GEColor(0.0f, 0.0f, 0.0f)); }
extern "C" void ee_screenscpp_sba_bg_opaque(uintptr_t inBar) { SBA(inBar)->SetBgOpacity(1.0f); }
extern "C" void ee_screenscpp_sba_bar_delete(uintptr_t inBar) { delete static_cast<UIStatusBar*>(SBA(inBar)->mBar); }
extern "C" void ee_screenscpp_sba_icon_delete(uintptr_t inBar) { delete static_cast<UIBitmap*>(SBA(inBar)->mIcon); }
extern "C" void ee_screenscpp_sba_value_delete(uintptr_t inBar, uint32_t inWhich)
{ delete static_cast<UITextLabel*>(SBA(inBar)->*kSBAValues[inWhich]); }
extern "C" void ee_screenscpp_sba_bitmap140_dereference(uintptr_t inBar) { static_cast<GEBitmap*>(SBA(inBar)->mBitmap140)->Dereference(); }
extern "C" void ee_screenscpp_sba_bitmap144_dereference(uintptr_t inBar) { static_cast<GEBitmap*>(SBA(inBar)->mBitmap144)->Dereference(); }
extern "C" void ee_screenscpp_sba_count_delete(uintptr_t inBar) { delete static_cast<GEText*>(SBA(inBar)->mCount); }
extern "C" int64_t ee_screenscpp_sba_base_copy_data(uintptr_t inBar) { return SBA(inBar)->EEUIBUpgrade::CopyData(); }
extern "C" int64_t ee_screenscpp_sba_bar_copy_data(uintptr_t inBar) { return static_cast<UIStatusBar*>(SBA(inBar)->mBar)->CopyData(); }
extern "C" int64_t ee_screenscpp_sba_icon_copy_data(uintptr_t inBar) { return static_cast<UIBitmap*>(SBA(inBar)->mIcon)->CopyData(); }
extern "C" int64_t ee_screenscpp_sba_value_copy_data(uintptr_t inBar, uint32_t inWhich)
{ return static_cast<UITextLabel*>(SBA(inBar)->*kSBAValues[inWhich])->CopyData(); }
extern "C" int64_t ee_screenscpp_sba_queued(uintptr_t inBar) { return SBA(inBar)->mNumQueued; }
extern "C" void ee_screenscpp_sba_count_text(uintptr_t inBar, EeScreensShimText inText)
{ static_cast<GEText*>(SBA(inBar)->mCount)->SetText(EEShimText(inText)); }
extern "C" void ee_screenscpp_sba_copy_enabled(uintptr_t inBar) { SBA(inBar)->mCopiedEnabled = SBA(inBar)->mEnabled; }
extern "C" bool ee_screenscpp_sba_copied_enabled(uintptr_t inBar) { return SBA(inBar)->mCopiedEnabled; }
extern "C" int64_t ee_screenscpp_sba_button_draw_polygons(uintptr_t inBar)
{ return SBA(inBar)->UIButton::DrawPolygons(); }
extern "C" int64_t ee_screenscpp_sba_icon_draw_polygons(uintptr_t inBar) { return static_cast<UIBitmap*>(SBA(inBar)->mIcon)->DrawPolygons(); }
extern "C" int64_t ee_screenscpp_sba_bar_draw_polygons(uintptr_t inBar) { return static_cast<UIStatusBar*>(SBA(inBar)->mBar)->DrawPolygons(); }
extern "C" int64_t ee_screenscpp_sba_value_draw_polygons(uintptr_t inBar, uint32_t inWhich)
{ return static_cast<UITextLabel*>(SBA(inBar)->*kSBAValues[inWhich])->DrawPolygons(); }
extern "C" float ee_screenscpp_sba_count_x(uintptr_t inBar) { return static_cast<GEText*>(SBA(inBar)->mCount)->GetLocation().mX; }
extern "C" float ee_screenscpp_sba_count_y(uintptr_t inBar) { return static_cast<GEText*>(SBA(inBar)->mCount)->GetLocation().mY; }
extern "C" float ee_screenscpp_sba_width(uintptr_t inBar) { return SBA(inBar)->GetWidth(); }
extern "C" float ee_screenscpp_sba_height(uintptr_t inBar) { return SBA(inBar)->GetHeight(); }
extern "C" int64_t ee_screenscpp_sba_control_draw_text(uintptr_t inBar) { return SBA(inBar)->UIControl::DrawTextA(); }
extern "C" int64_t ee_screenscpp_sba_bar_draw_text(uintptr_t inBar) { return static_cast<UIStatusBar*>(SBA(inBar)->mBar)->DrawTextA(); }
extern "C" int64_t ee_screenscpp_sba_icon_draw_text(uintptr_t inBar) { return static_cast<UIBitmap*>(SBA(inBar)->mIcon)->DrawTextA(); }
extern "C" int64_t ee_screenscpp_sba_value_draw_text(uintptr_t inBar, uint32_t inWhich)
{ return static_cast<UITextLabel*>(SBA(inBar)->*kSBAValues[inWhich])->DrawTextA(); }
extern "C" bool ee_screenscpp_sba_value2_has_text(uintptr_t inBar)
{ return static_cast<UITextLabel*>(SBA(inBar)->mValue2)->GetText(0).GetLength() > 0; }
extern "C" bool ee_screenscpp_sba_show_count(uintptr_t inBar) { return SBA(inBar)->mShowCount; }
extern "C" int64_t ee_screenscpp_sba_draw_count(uintptr_t inBar, float inLeft, float inTop, float inRight, float inBottom)
{ return SBA(inBar)->GetForm()->DrawClippedText(GERectangle(inLeft, inTop, inRight, inBottom), static_cast<GEText*>(SBA(inBar)->mCount), SBA(inBar)); }
#undef SBA
#undef SBAR
#undef SBAF

// ==== EEUIFMGStatusBox.cpp ==========================================================================================
// C:\EEX\Empire Earth\User Interface Thread\EEUIFMGStatusBox.cpp (__FILE__ 0x4cdcfc). The main group's status box
// (no vtable; ctor 0x26ada8): 0x269ca8..0x26de97 (the init chains of three GEColors and the two icon tables, the
// icon cell ctor, the ctor, the selection members, the icon textures, EEUIFMGStatusBar's SetIconBitmap after its first
// user, the exchange-button helpers 0x26ddb7/0x26de3b). Unit flags: the exe module set.
static GEColor kStatusColor1(0.0f, 0.0f, 1.0f);
static GEColor kStatusColor2(1.0f, 1.0f, 0.0f);
static GEColor kStatusColor3(0.0f, 1.0f, 0.5f);
// (the icon cells' table, 0x564890, is Rust's: ee_screens status_box.rs CELLS)
extern bool sIconsLoaded;                           // 0x5652ec; cross-TU flag (stored in EEGlobals.cpp)

// Creates the main panel's status box with no controls bound and no attributes shown

// (status_box.rs' constants equal the headers': hud_cpp_consts_tests.rs)

// The call's unit (an EEUnit*) for the status box's view of it (hud_views.rs status_box_view, the original's
// EEStatusBoxView), and whether the box has its third and fourth bars
extern "C" int64_t ee_screenscpp_status_box_unit(void* inCall) { return (int64_t)static_cast<EEUnit*>(static_cast<EEUIFMGStatusBoxCall*>(inCall)->mUnit); }
extern "C" bool ee_screenscpp_status_box_bar3(void* inCall) { return static_cast<EEUIFMGStatusBox*>(static_cast<EEUIFMGStatusBoxCall*>(inCall)->mBox)->mCA8 != NULL; }
extern "C" bool ee_screenscpp_status_box_bar4(void* inCall) { return static_cast<EEUIFMGStatusBox*>(static_cast<EEUIFMGStatusBoxCall*>(inCall)->mBox)->mCAC != NULL; }

// ---- the one-statement shims of ee_screens status_box_calls.rs (UI lane round 8; once HostOp's cases and
// EEUIFMGStatusBar::SetIconBitmap): each one call, no logic
#define SBC(x) (*reinterpret_cast<EEUIFMGStatusBoxCall*>(x))
#define SBX(x, i) static_cast<EEUIFMGStatusBox*>(SBC(x).mBox)->GetBoxControl(i)
#define SBBAR(x, i) static_cast<EEUIFMGStatusBar*>(SBX(x, i))
static const GEColor* const kSBBarColors[3] = {&kStatusColor3, &kStatusColor1, &kStatusColor2};
static const char* const kSBTextureNames[4] = {"", "textures\\unit improvements_01", "textures\\unit improvements_02",
                                               "textures\\unit upgrades_03"};
extern "C" int64_t ee_screenscpp_sb_upgrade_queued(uintptr_t inCall, int64_t inCtl)
{ return static_cast<EEUIBUpgrade*>(SBX(inCall, inCtl))->mNumQueued; }
extern "C" void ee_screenscpp_sb_control_set_dirty(uintptr_t inCall, int64_t inCtl)
{ SBX(inCall, inCtl)->SetDirty(true); }
extern "C" void ee_screenscpp_sb_upgrade_set_queued(uintptr_t inCall, int64_t inCtl, int64_t inCount)
{ static_cast<EEUIBUpgrade*>(SBX(inCall, inCtl))->mNumQueued = (long)inCount; }
extern "C" int64_t ee_screenscpp_sb_upgrade_attribute(uintptr_t inCall, int64_t inCtl)
{ return static_cast<EEUIBUpgrade*>(SBX(inCall, inCtl))->mAttribute; }
extern "C" void ee_screenscpp_sb_upgrade_set_attribute(uintptr_t inCall, int64_t inCtl, int64_t inValue)
{ static_cast<EEUIBUpgrade*>(SBX(inCall, inCtl))->mAttribute = (long)inValue; }
extern "C" void ee_screenscpp_sb_upgrade_cost_label_clear(uintptr_t inCall, int64_t inCtl)
{ static_cast<EEUIBUpgrade*>(SBX(inCall, inCtl))->mCostLabel = NULL; }
extern "C" void ee_screenscpp_sb_upgrade_cost_label(uintptr_t inCall, int64_t inCtl, int64_t inLabel)
{ static_cast<EEUIBUpgrade*>(SBX(inCall, inCtl))->mCostLabel = static_cast<UITextLabel*>(SBX(inCall, inLabel)); }
extern "C" void ee_screenscpp_sb_upgrade_update_cost(uintptr_t inCall, int64_t inCtl)
{ static_cast<EEUIBUpgrade*>(SBX(inCall, inCtl))->UpdateCost(SBC(inCall).mText); }
extern "C" bool ee_screenscpp_sb_name_empty_fill(uintptr_t inCall)
{ return static_cast<EEUnit*>(SBC(inCall).mUnit)->mStats->mName == gEmptyString; }
extern "C" bool ee_screenscpp_sb_name_empty(uintptr_t inCall) { return static_cast<EEUnit*>(SBC(inCall).mUnit)->GetStats()->mName == ""; }
extern "C" void ee_screenscpp_sb_text_class_name(uintptr_t inCall)
{ gResource->GetString(static_cast<EEUnit*>(SBC(inCall).mUnit)->GetUnitClass()->mNameStringID, SBC(inCall).mText); }
extern "C" void ee_screenscpp_sb_text_stats_name(uintptr_t inCall)
{ SBC(inCall).mText = static_cast<EEUnit*>(SBC(inCall).mUnit)->GetStats()->mName; }
extern "C" bool ee_screenscpp_sb_is_gate_fill(uintptr_t inCall)
{ return static_cast<EEUnit*>(SBC(inCall).mUnit)->mUnitClass->mCategory == kUnitCategoryGate; }
extern "C" bool ee_screenscpp_sb_is_gate(uintptr_t inCall)
{ return static_cast<EEUnit*>(SBC(inCall).mUnit)->GetUnitClass()->mCategory == kUnitCategoryGate; }
extern "C" bool ee_screenscpp_sb_gate_locked(uintptr_t inCall) { return static_cast<EEUnit*>(SBC(inCall).mUnit)->mComplexUnit->mIsLocked; }
extern "C" void ee_screenscpp_sb_suffix_clear() { EEScratch<UWideString>(kScratchSBSuffix) = UWideString(); }
extern "C" void ee_screenscpp_sb_suffix_locked() { gResource->GetString(IDS_MSG_LOCKED, EEScratch<UWideString>(kScratchSBSuffix)); }
extern "C" void ee_screenscpp_sb_suffix_unlocked() { gResource->GetString(IDS_MSG_UNLOCKED, EEScratch<UWideString>(kScratchSBSuffix)); }
extern "C" void ee_screenscpp_sb_text_append_suffix(uintptr_t inCall) { SBC(inCall).mText += EEScratch<UWideString>(kScratchSBSuffix); }
extern "C" void ee_screenscpp_sb_text_civ(uintptr_t inCall)
{ SBC(inCall).mText = static_cast<EEUnit*>(SBC(inCall).mUnit)->GetPlayer()->mCivilization.mName; }
extern "C" uintptr_t ee_screenscpp_sb_bar_control(uintptr_t inCall, int64_t inCtl)
{ return (uintptr_t)SBBAR(inCall, inCtl); }
extern "C" uintptr_t ee_screenscpp_sb_icon_rect(int64_t inIcon)
{ return (uintptr_t)&ee_screens_icon_bitmap(inIcon)->mRectangle; }
extern "C" void ee_screenscpp_sb_bar_range(uintptr_t inCall, int64_t inCtl, int64_t inValue)
{ static_cast<UIStatusBar*>(SBBAR(inCall, inCtl)->mBar)->SetRange(0, (unsigned long)(long)inValue); }
extern "C" void ee_screenscpp_sb_bar_value(uintptr_t inCall, int64_t inCtl, int64_t inValue)
{ static_cast<UIStatusBar*>(SBBAR(inCall, inCtl)->mBar)->SetCurrentRangeValue((long)inValue); }
extern "C" void ee_screenscpp_sb_bar_color(uintptr_t inCall, int64_t inCtl, uint32_t inColor)
{ static_cast<UIStatusBar*>(SBBAR(inCall, inCtl)->mBar)->SetStatusBarColor(*kSBBarColors[inColor]); }
extern "C" void ee_screenscpp_sb_bar_enabled(uintptr_t inCall, int64_t inCtl, bool inEnabled)
{ SBBAR(inCall, inCtl)->mEnabled = inEnabled; }
extern "C" void ee_screenscpp_sb_stretch(uintptr_t inCall, int64_t inIcon, int64_t inRectangle, int64_t inCtl)
{ static_cast<GEBitmap*>(ee_screens_icon_bitmap(inIcon)->mBitmap)->ComputeStretchedCoordsAndScale(static_cast<EEUIFMGAttribute*>(SBX(inCall, inRectangle))->GetIconRectangle(), static_cast<EEUIFMGAttribute*>(SBX(inCall, inRectangle))->GetIconRectangle(), static_cast<const GERectangle&>(ee_screens_icon_bitmap(inIcon)->mRectangle), SBX(inCall, inCtl)->GetScreenIndex()); }
extern "C" void ee_screenscpp_sb_button_set_normal(uintptr_t inCall, int64_t inCtl, int64_t inIcon)
{ static_cast<UIButton*>(SBX(inCall, inCtl))->SetNormalBitmap(static_cast<GEBitmap*>(ee_screens_icon_bitmap(inIcon)->mBitmap)); }
extern "C" void ee_screenscpp_sb_button_set_pressed(uintptr_t inCall, int64_t inCtl, int64_t inIcon)
{ static_cast<UIButton*>(SBX(inCall, inCtl))->SetPressedBitmap(static_cast<GEBitmap*>(ee_screens_icon_bitmap(inIcon)->mBitmap)); }
extern "C" bool ee_screenscpp_sb_button_normal_is(uintptr_t inCall, int64_t inCtl, int64_t inIcon)
{ return static_cast<UIButton*>(SBX(inCall, inCtl))->GetNormalBitmap() == static_cast<GEBitmap*>(ee_screens_icon_bitmap(inIcon)->mBitmap); }
extern "C" bool ee_screenscpp_sb_button_pressed_is(uintptr_t inCall, int64_t inCtl, int64_t inIcon)
{ return static_cast<UIButton*>(SBX(inCall, inCtl))->GetPressedBitmap() == static_cast<GEBitmap*>(ee_screens_icon_bitmap(inIcon)->mBitmap); }
extern "C" void ee_screenscpp_sb_portrait_set_unit(uintptr_t inCall)
{ static_cast<EEUICPortrait*>(static_cast<EEUIFMGStatusBox*>(SBC(inCall).mBox)->mC8C)->SetUnit(static_cast<EEUnit*>(SBC(inCall).mUnit)); }
extern "C" void ee_screenscpp_sb_portrait_clear(uintptr_t inCall)
{ static_cast<EEUICPortrait*>(static_cast<EEUIFMGStatusBox*>(SBC(inCall).mBox)->mC8C)->SetUnit(NULL); }
extern "C" void ee_screenscpp_sb_portrait_update(uintptr_t inCall)
{ static_cast<EEUICPortrait*>(static_cast<EEUIFMGStatusBox*>(SBC(inCall).mBox)->mC8C)->Update(); }
extern "C" void ee_screenscpp_sb_verify_armor_count()
{ ESVerify(__FILE__, 367, "this->mArmorAttributeCount <= keeuifmgsbMaxAttributes"); }
extern "C" void ee_screenscpp_sb_verify_count()
{ ESVerify(__FILE__, 708, "this->mAttributeCount <= keeuifmgsbMaxAttributes"); }
extern "C" uintptr_t ee_screenscpp_sb_main_background(uint32_t inId)
{ return (uintptr_t)gUIManager->GetMainGameForm()->GetBackgroundByID(inId); }
extern "C" void ee_screenscpp_sb_background_set_visible(uintptr_t inBackground, bool inVisible)
{ reinterpret_cast<UIBackground*>(inBackground)->SetVisibility(inVisible); }
extern "C" bool ee_screenscpp_sb_value_differs(uintptr_t inCall, int64_t inCtl)
{ return SBC(inCall).mText != SBBAR(inCall, inCtl)->GetValueText(); }
extern "C" bool ee_screenscpp_sb_value2_differs(uintptr_t inCall, int64_t inCtl)
{ return SBC(inCall).mText != SBBAR(inCall, inCtl)->GetValue2Text(); }
extern "C" void ee_screenscpp_sb_set_value(uintptr_t inCall, int64_t inCtl)
{ static_cast<UITextLabel*>(SBBAR(inCall, inCtl)->mValue)->SetText(SBC(inCall).mText); }
extern "C" void ee_screenscpp_sb_set_value2(uintptr_t inCall, int64_t inCtl)
{ static_cast<UITextLabel*>(SBBAR(inCall, inCtl)->mValue2)->SetText(SBC(inCall).mText); }
extern "C" int64_t ee_screenscpp_sb_class_armor(uintptr_t inCall, int64_t inIndex)
{ return static_cast<EEUnit*>(SBC(inCall).mUnit)->GetUnitClass()->mArmor[inIndex]; }
extern "C" int64_t ee_screenscpp_sb_stats_class_armor(uintptr_t inCall, int64_t inIndex)
{ return static_cast<EEUnit*>(SBC(inCall).mUnit)->mStats->mUnitClass->mArmor[inIndex]; }
extern "C" float ee_screenscpp_sb_stats_armor(uintptr_t inCall, int64_t inIndex)
{ return static_cast<EEUnit*>(SBC(inCall).mUnit)->mStats->mArmor[inIndex]; }
extern "C" int64_t ee_screenscpp_sb_complex_unit_type(uintptr_t inCall)
{ return static_cast<EEComplexUnit*>(SBC(inCall).mComplex)->GetUnitClass()->mUnitType; }
extern "C" int64_t ee_screenscpp_sb_count_queued(uintptr_t inCall, int64_t inImprovement, int64_t inKind)
{ return static_cast<EEComplexUnit*>(SBC(inCall).mComplex)->GetPlayer()->CountQueuedImprovements((unsigned long)inImprovement, (long)inKind); }
extern "C" void ee_screenscpp_sb_new_bitmap(int64_t inIcon)
{ ee_screens_icon_bitmap(inIcon)->mBitmap = new GEBitmap; }
extern "C" uint32_t ee_screenscpp_sb_primary_width()
{ return (uint32_t)gRasterizer->GetPrimarySurface(gScreenIndex)->GetWidth(); }
extern "C" void ee_screenscpp_sb_set_description(uintptr_t inCall, int32_t inDrop)
{ SBC(inCall).mDescription = GETextureDescription{1, 0, gTexturePixelDepth, inDrop, false, false, true, false}; }
extern "C" uintptr_t ee_screenscpp_sb_description(uintptr_t inCall) { return (uintptr_t)&SBC(inCall).mDescription; }
extern "C" void ee_screenscpp_sb_set_texture(uintptr_t inCall, int64_t inIndex, uintptr_t inTexture)
{ SBC(inCall).mTextures[inIndex] = reinterpret_cast<GETexture*>(inTexture); }
extern "C" void ee_screenscpp_sb_set_rect(int64_t inIcon, float inLeft, float inTop, float inRight, float inBottom)
{ ee_screens_icon_bitmap(inIcon)->mRectangle = GERectangle(inLeft, inTop, inRight, inBottom); }
extern "C" uint32_t ee_screenscpp_sb_texture_width(uintptr_t inCall, int64_t inIndex)
{ return (uint32_t)static_cast<GETexture*>(SBC(inCall).mTextures[inIndex])->GetSurface()->GetWidth(); }
extern "C" uint32_t ee_screenscpp_sb_texture_height(uintptr_t inCall, int64_t inIndex)
{ return (uint32_t)static_cast<GETexture*>(SBC(inCall).mTextures[inIndex])->GetSurface()->GetHeight(); }
extern "C" void ee_screenscpp_sb_bitmap_set_texture(int64_t inIcon, uintptr_t inCall, int64_t inIndex)
{ static_cast<GEBitmap*>(ee_screens_icon_bitmap(inIcon)->mBitmap)->SetTexture(static_cast<GETexture*>(SBC(inCall).mTextures[inIndex]), gScreenIndex); }
extern "C" void ee_screenscpp_sb_bitmap_set_coordinates(int64_t inIcon, float inU0, float inV0, float inU1, float inV1)
{ static_cast<GEBitmap*>(ee_screens_icon_bitmap(inIcon)->mBitmap)->SetTextureCoordinates(GETextureCoordinate(inU0, inV0), GETextureCoordinate(inU1, inV0), GETextureCoordinate(inU1, inV1), GETextureCoordinate(inU0, inV1)); }
extern "C" void ee_screenscpp_sb_bitmap_scale(int64_t inIcon, float inX, float inY)
{ static_cast<GEBitmap*>(ee_screens_icon_bitmap(inIcon)->mBitmap)->SetXYScale(inX, inY); }
extern "C" void ee_screenscpp_sb_icons_set_loaded(bool inLoaded) { sIconsLoaded = inLoaded; }
extern "C" uintptr_t ee_screenscpp_sb_bitmap(int64_t inIcon)
{ return (uintptr_t)static_cast<GEBitmap*>(ee_screens_icon_bitmap(inIcon)->mBitmap); }
extern "C" void ee_screenscpp_sb_bitmap_dereference(int64_t inIcon)
{ static_cast<GEBitmap*>(ee_screens_icon_bitmap(inIcon)->mBitmap)->Dereference(); }
extern "C" void ee_screenscpp_sb_bitmap_clear(int64_t inIcon) { ee_screens_icon_bitmap(inIcon)->mBitmap = NULL; }
extern "C" void ee_screenscpp_sb_set_reload_description(int32_t inDrop)
{ EEScratch<GETextureDescription>(kScratchSBReload) = GETextureDescription{1, 0, gTexturePixelDepth, inDrop, false, false, true, false}; }
extern "C" int64_t ee_screenscpp_sb_reload(int64_t inIndex)
{ return gTextureManager->ReloadTexture(UString(kSBTextureNames[inIndex]), EEScratch<GETextureDescription>(kScratchSBReload)); }
extern "C" void ee_screenscpp_sb_bar_bitmap(uintptr_t inBar, uintptr_t inBitmap)
{ static_cast<UIBitmap*>(reinterpret_cast<EEUIFMGStatusBar*>(inBar)->mIcon)->SetBitmap(reinterpret_cast<GEBitmap*>(inBitmap)); }
extern "C" void ee_screenscpp_sb_bar_stretch(uintptr_t inBar, uintptr_t inBitmap, uintptr_t inRectangle)
{ reinterpret_cast<GEBitmap*>(inBitmap)->ComputeStretchedCoordsAndScale(static_cast<UIBitmap*>(reinterpret_cast<EEUIFMGStatusBar*>(inBar)->mIcon)->GetRectangle(), static_cast<UIBitmap*>(reinterpret_cast<EEUIFMGStatusBar*>(inBar)->mIcon)->GetRectangle(), *reinterpret_cast<const GERectangle*>(inRectangle), reinterpret_cast<EEUIFMGStatusBar*>(inBar)->GetScreenIndex()); }
extern "C" void ee_screenscpp_sb_bar_dirty(uintptr_t inBar)
{ reinterpret_cast<EEUIFMGStatusBar*>(inBar)->SetDirty(true); }

// (FillArmorAttributes 0x66ae16, UpdateArmorTexts 0x66c91d and GetAttributeText 0x66cb84 are Rust's: status_box.rs
// fill_armor_attributes, update_armor_texts; hud.rs attribute_text)

// the icons' load, unload and reload as the game's media calls them (Empire Earth.cpp: ee/EEGameUI.h)
static const bool sIconsRegistered = (gEEGameUI.mLoadIcons = EEUIFMGStatusBox::LoadIcons,
                                      gEEGameUI.mUnloadIcons = EEUIFMGStatusBox::UnloadIcons,
                                      gEEGameUI.mReloadIcons = EEUIFMGStatusBox::ReloadIcons, true);
#undef SBC
#undef SBX
#undef SBBAR

// ==== EEUIFMGStatusButton.cpp =======================================================================================
// C:\EEX\Empire Earth\User Interface Thread\EEUIFMGStatusButton.cpp (__FILE__ 0x4cde4c, ESVERIFY line 195). The
// main game form's command buttons (RTTI EEUIFMGStatusButton > UIButton; vtable 0x44ad98): 0x26de97..0x271319 (ctor,
// ??_G, dtor, the overrides and members, the EEM* inline ctors after their first users, EEUIManager's two main-group
// setters, EEUIEndCinematicModeEvent::ProcessEvent, the main game form's button tests 0x26fc22..0x27115c and its
// UpdateKeyboard/UpdateMouse). Unit flags: the exe module set.
extern unsigned short gFortressUnloadCost;   // 0x4c8fa4 (= 100, a word: EEUCFortress.cpp defines it)
// 0x20d571 / 0x20da86: EEGetConversionCost / EECanConvert (ee/EEAreaUtilities.h; EEUCBasicPriest.cpp)

long EEGetTradePrice(EEPlayer* inPlayer, long inResource, unsigned char inBuy);  // 0x23cc42 (EEMessage.cpp's)
// Creates a main game command button with its icon, dither and queue count text
EEUIFMGStatusButton::EEUIFMGStatusButton(UIManager* inManager, const GERectangle& inRectangle, UIControlsTable& inTable)
    : UIButton(inTable.GetID(), NULL, NULL, NULL, inRectangle, inManager->GetFormByID(inTable.GetFormID()),
               inTable.GetFormIndex(), 0),
      EeScreensStatusButtonFields(), EeScreensStatusButtonObjects()
{
    ee_screens_status_button_fields(this, static_cast<EeScreensStatusButtonFields*>(this));
    ee_screens_sbt_construct((uintptr_t)this, (uintptr_t)inManager, (uintptr_t)&inRectangle, (uintptr_t)&inTable);   // (status_button_calls.rs)
}

// (the command buttons' part of the main game form's host, the original's EEStatusButtonOp, is Rust's over the
// shims below: ee_screens status_button_calls.rs)
// (status_button.rs' constants equal the headers': hud_cpp_consts_tests.rs)

// Builds the button's help text: market trade prices, command help (with conversion cost for priests) and
// hot key, or a tech's name, cost and description (ee_screens status_button.rs help_string)
bool EEUIFMGStatusButton::GetHelpString(UWideString& outString) const
{
    return static_cast<EEUIFMainGame*>(this->GetForm())->ButtonCall(const_cast<EEUIFMGStatusButton*>(this), EE_SCREENS_SB_DO_HELP, &outString) != 0;
}

// Runs a command button on a right release, or cancels one queued tech (Shift: five, for units only)
// (ee_screens status_button.rs)
long EEUIFMGStatusButton::MouseRButtonUp() { return (long)static_cast<EEUIFMainGame*>(this->GetForm())->ButtonCall(this, EE_SCREENS_SB_DO_RIGHT_UP); }  // (0)

// the right press's and tab's shims (ee_screens hud_more.rs)
#define SBM(b) reinterpret_cast<EEUIFMGStatusButton*>(b)
extern "C" bool ee_screenscpp_sbm_is_command(uintptr_t inButton) { return SBM(inButton)->mIsCommand; }
extern "C" void ee_screenscpp_sbm_capture_mouse(uintptr_t inButton) { SBM(inButton)->BaseCaptureMouse(); }
extern "C" void ee_screenscpp_sbm_base_button_down(uintptr_t inButton) { SBM(inButton)->BaseButtonDown(); }
extern "C" uint32_t ee_screenscpp_sbm_state(uintptr_t inButton) { return (uint32_t)SBM(inButton)->GetState(); }
extern "C" void ee_screenscpp_sbm_hot_key_up(uintptr_t inButton) { SBM(inButton)->HotKeyUp(); }
extern "C" void ee_screenscpp_sbm_base_tabbed_from(uintptr_t inButton) { SBM(inButton)->BaseTabbedFrom(); }

// EEUIEndCinematicModeEvent's body (cpp_events.rs): ends the cinematic at its saved end time (EEMEndCinematic takes the
// end time as a float)
extern "C" void ee_screenscpp_ev_end_cinematic()
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageEndCinematic, {gWorld.mSavedGameSpeed}), kSlotNone); }

// ---- the one-statement shims of ee_screens status_button_calls.rs (UI lane round 8): each one call, no logic
#define SBR(x) reinterpret_cast<EEUIFMGFormCall*>(x)
#define SBRB(x) static_cast<EEUIFMGStatusButton*>(SBR(x)->mButton)
#define SBRF(x) static_cast<EEUIFMainGame*>(static_cast<EEUIFMGStatusButton*>(SBR(x)->mButton)->GetForm())
#define SBB(x) reinterpret_cast<EEUIFMGStatusButton*>(x)
#define SBU(x) reinterpret_cast<EEComplexUnit*>(x)
#define SBN(x) reinterpret_cast<EETechTreeNode*>(x)
#define SBK(x) reinterpret_cast<EEUnitClass*>(x)
#define SBM(x) reinterpret_cast<UIManager*>(x)
#define SBT(x) (*reinterpret_cast<UIControlsTable*>(x))
extern "C" uintptr_t ee_screenscpp_sbt_unit(int64_t inIndex)
{ return (uintptr_t)gWorld.GetCurrentPlayer()->mSelectedGroup->GetUnit(inIndex); }
extern "C" int64_t ee_screenscpp_sbt_unit_get_power(uintptr_t inUnit) { return SBU(inUnit)->GetPower(); }
extern "C" int64_t ee_screenscpp_sbt_unit_power(uintptr_t inUnit) { return (long)SBU(inUnit)->mPower; }
extern "C" bool ee_screenscpp_sbt_unit_cloaked(uintptr_t inUnit) { return SBU(inUnit)->IsCloaked(); }
extern "C" bool ee_screenscpp_sbt_unit_is_priest(uintptr_t inUnit) { return SBU(inUnit)->GetUnitClass()->IsPriest(); }
extern "C" int64_t ee_screenscpp_sbt_conversion_cost(uintptr_t inUnit)
{ return EEGetConversionCost(SBU(inUnit)->mComplexUnit); }
extern "C" int64_t ee_screenscpp_sbt_unload_cost() { return gFortressUnloadCost; }
extern "C" int64_t ee_screenscpp_sbt_first_category(uintptr_t inRec)
{ return static_cast<EEComplexUnit*>(SBR(inRec)->mFirst)->GetUnitClass()->mCategory; }
extern "C" int64_t ee_screenscpp_sbt_first_type(uintptr_t inRec)
{ return static_cast<EEComplexUnit*>(SBR(inRec)->mFirst)->GetUnitClass()->mUnitType; }
extern "C" void ee_screenscpp_sbt_send_unload(uintptr_t inRec)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageUnloadTransport, {static_cast<EEComplexUnit*>(SBR(inRec)->mFirst)->mUnitID, static_cast<EEComplexUnit*>(SBR(inRec)->mFirst)->GetTileX(), static_cast<EEComplexUnit*>(SBR(inRec)->mFirst)->GetTileY()}), kSlotNone); }
extern "C" void ee_screenscpp_sbt_send_research(uintptr_t inRec, bool inShift)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageResearchTechnology, {SBRB(inRec)->mUnitID, SBRB(inRec)->mTechnology, inShift}), kSlotNone); }
extern "C" void ee_screenscpp_sbt_send_remove(uintptr_t inRec, bool inShift)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageRemoveTechnology, {SBRB(inRec)->mUnitID, SBRB(inRec)->mTechnology, false, inShift}), kSlotNone); }
extern "C" void ee_screenscpp_sbt_start_patrol(uintptr_t inRec) { SBRF(inRec)->StartPatrol(); }
extern "C" void ee_screenscpp_sbt_start_flare(uintptr_t inRec) { SBRF(inRec)->StartFlare(); }
extern "C" void ee_screenscpp_sbt_build_unit(uintptr_t inRec)
{ SBRF(inRec)->BuildUnit(SBRB(inRec)->mUnitID, SBRB(inRec)->mTechnology); }
extern "C" void ee_screenscpp_sbt_start_wall(uintptr_t inRec) { SBRF(inRec)->StartWall(SBRB(inRec)->mTechnology); }
extern "C" void ee_screenscpp_sbt_start_gate(uintptr_t inRec) { SBRF(inRec)->StartGate(SBRB(inRec)->mTechnology); }
extern "C" void ee_screenscpp_sbt_start_calamity(uintptr_t inRec)
{ SBRF(inRec)->StartCalamity(SBRB(inRec)->mTechnology); }
extern "C" int64_t ee_screenscpp_sbt_game_resources() { return gWorld.mGameOptions.mResources; }
extern "C" float ee_screenscpp_sbt_game_clock() { return gPhysics->GetGameClock(); }
extern "C" int64_t ee_screenscpp_sbt_world_long(int64_t inConstant)
{ return gWorldTable.GetLongValue((EEWorldConstant)inConstant); }
extern "C" void ee_screenscpp_sbt_string_clear() { EEScratch<UWideString>(kScratchSBTString) = UWideString(); }
extern "C" void ee_screenscpp_sbt_string_load(int64_t inId) { gResource->GetString(inId, EEScratch<UWideString>(kScratchSBTString)); }
extern "C" void ee_screenscpp_sbt_chat_add_string() { gUserInterface->AddChatLogEntry(EEScratch<UWideString>(kScratchSBTString), 0); }
extern "C" bool ee_screenscpp_sbt_shift() { return UIControl::ShiftKeyPressed(); }
extern "C" bool ee_screenscpp_sbt_is_command(uintptr_t inRec) { return SBRB(inRec)->mIsCommand; }
extern "C" uintptr_t ee_screenscpp_sbt_node(uintptr_t inRec)
{ return (uintptr_t)gWorld.GetCurrentPlayer()->GetTechTree()->Get(SBRB(inRec)->mTechnology); }
extern "C" int64_t ee_screenscpp_sbt_node_kind(uintptr_t inNode) { return SBN(inNode)->mData->mKind; }
extern "C" int64_t ee_screenscpp_sbt_node_class(uintptr_t inNode) { return SBN(inNode)->mData->mUnitClass; }
extern "C" int64_t ee_screenscpp_sbt_node_makes_unit(uintptr_t inNode) { return SBN(inNode)->mData->mMakesUnit; }
extern "C" int64_t ee_screenscpp_sbt_node_calamity(uintptr_t inNode) { return SBN(inNode)->mData->GetCalamity(); }
extern "C" int64_t ee_screenscpp_sbt_node_name_id(uintptr_t inNode) { return SBN(inNode)->mData->mNameID; }
extern "C" bool ee_screenscpp_sbt_node_contains(uintptr_t inNode, int64_t inUnit)
{ return SBN(inNode)->Contains(inUnit); }
extern "C" uintptr_t ee_screenscpp_sbt_node_calamity_class(uintptr_t inNode)
{ return (uintptr_t)USingleton<EECalamityManager>::GetInstance()->GetCalamityClass((EECalamityEnum)SBN(inNode)->mData->mCalamity); }
extern "C" uintptr_t ee_screenscpp_sbt_calamity_class(int64_t inCalamity)
{ return (uintptr_t)USingleton<EECalamityManager>::GetInstance()->GetCalamityClass((EECalamityEnum)inCalamity); }
extern "C" int64_t ee_screenscpp_sbt_calamity_cost(uintptr_t inCalamity)
{ return reinterpret_cast<EECalamityClass*>(inCalamity)->GetCost(); }
extern "C" uintptr_t ee_screenscpp_sbt_class(int64_t inIndex) { return (uintptr_t)gUnitClassTable[inIndex]; }
extern "C" bool ee_screenscpp_sbt_class_is_land_building(uintptr_t inClass) { return SBK(inClass)->IsLandBuilding(); }
extern "C" bool ee_screenscpp_sbt_class_naval_building(uintptr_t inClass)
{ return SBK(inClass)->mCategory == kUnitCategoryNavalBuilding; }
extern "C" bool ee_screenscpp_sbt_class_upgrade_named(uintptr_t inClass)
{ return gWorld.GetCurrentPlayer()->mUnitClassUpgrades[SBK(inClass)->mUnitType]->mName.GetLength() != 0; }
extern "C" int64_t ee_screenscpp_sbt_class_population(uintptr_t inClass) { return SBK(inClass)->GetPopulationCost(); }
extern "C" bool ee_screenscpp_sbt_can_produce(uintptr_t inRec, int64_t inCount)
{ return gWorld.GetCurrentPlayer()->CanProduce(SBRB(inRec)->mTechnology, SBRB(inRec)->mUnitType, inCount, true, 3, &SBR(inRec)->mText) == true; }
extern "C" bool ee_screenscpp_sbt_can_produce_quiet(uintptr_t inRec)
{ return gWorld.GetCurrentPlayer()->CanProduce(SBRB(inRec)->mTechnology, SBRB(inRec)->mUnitType, 1, true, 3, NULL) == true; }
extern "C" void ee_screenscpp_sbt_button_up(uintptr_t inRec) { SBRB(inRec)->ButtonUp(); }
extern "C" bool ee_screenscpp_sbt_mouse_inside(uintptr_t inRec) { return SBRB(inRec)->IsMouseInside(); }
extern "C" void ee_screenscpp_sbt_count_text(uintptr_t inRec, EeScreensShimText inText)
{ static_cast<GEText*>(SBRB(inRec)->mBuildQueue)->SetText(EEShimText(inText)); }
extern "C" bool ee_screenscpp_sbt_in_fanatic_set(int64_t inUnit)
{ return gUnitSetManager.GetSet(0x2f)->Contains((unsigned long)inUnit); }
extern "C" void ee_screenscpp_sbt_text_string(uintptr_t inRec, int64_t inId)
{ gResource->GetString((unsigned long)inId, SBR(inRec)->mText); }
extern "C" uintptr_t ee_screenscpp_sbt_button_object_of(uintptr_t inRec)
{ return (uintptr_t)EEButtonObject::Get(SBRB(inRec)->mButton); }
extern "C" uintptr_t ee_screenscpp_sbt_object_hot_key(uintptr_t inObject)
{ return (uintptr_t)reinterpret_cast<EEButtonObject*>(inObject)->GetHotKey(); }
extern "C" uintptr_t ee_screenscpp_sbt_node_hot_key(uintptr_t inNode) { return (uintptr_t)SBN(inNode)->GetHotKey(); }
extern "C" void ee_screenscpp_sbt_hot_key_name(uintptr_t inRec, uintptr_t inHotKey)
{ reinterpret_cast<UIHotKey*>(inHotKey)->GetHotKeyName(SBR(inRec)->mText); }
extern "C" void ee_screenscpp_sbt_text_upgrade_name(uintptr_t inRec, int64_t inClass)
{ SBR(inRec)->mText = gWorld.GetCurrentPlayer()->mUnitClassUpgrades[gUnitClassTable[inClass]->mUnitType]->mName; }
extern "C" void ee_screenscpp_sbt_text_node_cost(uintptr_t inRec, uintptr_t inNode)
{ SBN(inNode)->GetCostString(SBR(inRec)->mText, SBRB(inRec)->mUnitType, 1.0f); }
extern "C" int64_t ee_screenscpp_sbt_text_length(uintptr_t inRec) { return (int64_t)strlen(SBR(inRec)->mText); }
extern "C" int64_t ee_screenscpp_sbt_trade_price(int64_t inResource, bool inBuy)
{ return EEGetTradePrice(gWorld.GetCurrentPlayer(), (long)inResource, inBuy); }
extern "C" bool ee_screenscpp_sbt_has_object(uintptr_t inRec) { return static_cast<EEButtonObject*>(SBRB(inRec)->mButtonObject) != NULL; }
extern "C" void ee_screenscpp_sbt_set_draw_icon(uintptr_t inRec, bool inDraw) { SBRB(inRec)->mDrawIcon = inDraw; }
extern "C" void ee_screenscpp_sbt_object_pressed_icon(uintptr_t inRec)
{ static_cast<EEButtonObject*>(SBRB(inRec)->mButtonObject)->SetPressedBitmap(static_cast<GEBitmap*>(SBRB(inRec)->mIcon), SBRB(inRec)->GetRectangle(), SBRB(inRec)->GetScreenIndex()); }
extern "C" void ee_screenscpp_sbt_object_face_icon(uintptr_t inRec)
{ static_cast<EEButtonObject*>(SBRB(inRec)->mButtonObject)->SetFaceBitmap(static_cast<GEBitmap*>(SBRB(inRec)->mIcon), SBRB(inRec)->GetRectangle(), SBRB(inRec)->GetScreenIndex()); }
extern "C" void ee_screenscpp_sbt_main_group_l1c(int64_t inValue)
{ gUIManager->GetMainGameForm()->mMainGroup->mL1C = (long)inValue; }
extern "C" void ee_screenscpp_sbt_main_group_reset()
{ gUIManager->GetMainGameForm()->mMainGroup->mReset = true; }
extern "C" uintptr_t ee_screenscpp_sbt_object(uintptr_t inButton) { return (uintptr_t)static_cast<EEButtonObject*>(SBB(inButton)->mButtonObject); }
extern "C" void ee_screenscpp_sbt_object_release(uintptr_t inButton) { static_cast<EEButtonObject*>(SBB(inButton)->mButtonObject)->Release(); }
extern "C" void ee_screenscpp_sbt_object_clear(uintptr_t inButton) { SBB(inButton)->mButtonObject = NULL; }
extern "C" void ee_screenscpp_sbt_object_acquire(uintptr_t inButton) { static_cast<EEButtonObject*>(SBB(inButton)->mButtonObject)->Acquire(); }
extern "C" void ee_screenscpp_sbt_set_object(uintptr_t inButton, uint32_t inIndex)
{ SBB(inButton)->mButtonObject = EEButtonObject::Get(inIndex); }
extern "C" void ee_screenscpp_sbt_set_node_object(uintptr_t inButton, int64_t inTechnology)
{ SBB(inButton)->mButtonObject = gWorld.GetCurrentPlayer()->GetTechTree()->Get((long)inTechnology)->mButton; }
extern "C" void ee_screenscpp_sbt_set_count(uintptr_t inButton, int64_t inCount)
{ SBB(inButton)->mCount = (long)inCount; }
extern "C" void ee_screenscpp_sbt_set_command(uintptr_t inButton, bool inCommand)
{ SBB(inButton)->mIsCommand = inCommand; }
extern "C" void ee_screenscpp_sbt_set_button(uintptr_t inButton, uint32_t inIndex) { SBB(inButton)->mButton = inIndex; }
extern "C" void ee_screenscpp_sbt_set_technology(uintptr_t inButton, int64_t inTechnology)
{ SBB(inButton)->mTechnology = (long)inTechnology; }
extern "C" void ee_screenscpp_sbt_set_unit_type(uintptr_t inButton, uint32_t inKind)
{ SBB(inButton)->mUnitType = inKind; }
extern "C" void ee_screenscpp_sbt_set_unit_id(uintptr_t inButton, uint32_t inId) { SBB(inButton)->mUnitID = inId; }
extern "C" void ee_screenscpp_sbt_icon_dereference(uintptr_t inButton) { static_cast<GEBitmap*>(SBB(inButton)->mIcon)->Dereference(); }
extern "C" void ee_screenscpp_sbt_queue_delete(uintptr_t inButton) { delete static_cast<GEText*>(SBB(inButton)->mBuildQueue); }
extern "C" void ee_screenscpp_sbt_dither_dereference(uintptr_t inButton) { static_cast<GEBitmap*>(SBB(inButton)->mDither)->Dereference(); }
extern "C" bool ee_screenscpp_sbt_draw_icon(uintptr_t inButton) { return SBB(inButton)->mDrawIcon; }
extern "C" void ee_screenscpp_sbt_icon_white(uintptr_t inButton)
{ static_cast<GEBitmap*>(SBB(inButton)->mIcon)->SetColor(GEColor(1.0f, 1.0f, 1.0f)); }
extern "C" void ee_screenscpp_sbt_draw_icon_bitmap(uintptr_t inButton)
{ SBB(inButton)->GetRasterizer()->Draw(static_cast<GEBitmap*>(SBB(inButton)->mIcon), SBB(inButton)->GetScreenIndex()); }
extern "C" bool ee_screenscpp_sbt_available(uintptr_t inButton) { return SBB(inButton)->mAvailable; }
extern "C" void ee_screenscpp_sbt_dither_at_drawn(uintptr_t inButton)
{ static_cast<GEBitmap*>(SBB(inButton)->mDither)->SetLocation(SBB(inButton)->GetCopiedDrawRectangle().GetTopLeft()); }
extern "C" void ee_screenscpp_sbt_draw_dither(uintptr_t inButton)
{ SBB(inButton)->GetRasterizer()->Draw(static_cast<GEBitmap*>(SBB(inButton)->mDither), SBB(inButton)->GetScreenIndex()); }
extern "C" int64_t ee_screenscpp_sbt_base_draw_polygons(uintptr_t inButton)
{ return SBB(inButton)->UIButton::DrawPolygons(); }
extern "C" bool ee_screenscpp_sbt_show_count(uintptr_t inButton) { return SBB(inButton)->mShowCount; }
extern "C" void ee_screenscpp_sbt_draw_count(uintptr_t inButton)
{ SBB(inButton)->GetForm()->DrawClippedText(SBB(inButton)->GetRectangle(), static_cast<GEText*>(SBB(inButton)->mBuildQueue), SBB(inButton)); }
extern "C" int64_t ee_screenscpp_sbt_base_draw_text(uintptr_t inButton) { return SBB(inButton)->UIButton::DrawTextA(); }
extern "C" bool ee_screenscpp_sbt_pressed(uintptr_t inButton)
{ return SBB(inButton)->GetState() == kButtonStatePressed; }
extern "C" void ee_screenscpp_sbt_up(uintptr_t inButton) { SBB(inButton)->ButtonUp(); }
extern "C" void ee_screenscpp_sbt_icon_at(uintptr_t inButton, float inX, float inY)
{ static_cast<GEBitmap*>(SBB(inButton)->mIcon)->SetLocation(GE2DPoint(inX, inY)); }
extern "C" void ee_screenscpp_sbt_queue_at(uintptr_t inButton, float inX, float inY)
{ static_cast<GEText*>(SBB(inButton)->mBuildQueue)->SetLocation(GE2DPoint(inX, inY)); }
extern "C" void ee_screenscpp_sbt_base_set_location(uintptr_t inButton, float inX, float inY)
{ SBB(inButton)->UIButton::SetLocation(GE2DPoint(inX, inY)); }
extern "C" uintptr_t ee_screenscpp_sbt_normal_bitmap(uintptr_t inButton, uintptr_t inManager, uintptr_t inTable)
{ return (uintptr_t)SBT(inTable).CreateNormalBitmap(SBM(inManager), SBB(inButton)->GetRectangle()); }
extern "C" uintptr_t ee_screenscpp_sbt_pressed_bitmap(uintptr_t inButton, uintptr_t inManager, uintptr_t inTable)
{ return (uintptr_t)SBT(inTable).CreatePressedBitmap(SBM(inManager), SBB(inButton)->GetRectangle()); }
extern "C" void ee_screenscpp_sbt_set_pressed_bitmap(uintptr_t inButton, uintptr_t inBitmap)
{ SBB(inButton)->SetPressedBitmap(reinterpret_cast<GEBitmap*>(inBitmap)); }
extern "C" void ee_screenscpp_sbt_set_normal_bitmap(uintptr_t inButton, uintptr_t inBitmap)
{ SBB(inButton)->SetNormalBitmap(reinterpret_cast<GEBitmap*>(inBitmap)); }
extern "C" void ee_screenscpp_sbt_set_f13x(uintptr_t inButton, float inValue)
{ SBB(inButton)->mF138 = SBB(inButton)->mF134 = inValue; }
extern "C" void ee_screenscpp_sbt_icon_new(uintptr_t inButton) { SBB(inButton)->mIcon = new GEBitmap; }
extern "C" float ee_screenscpp_sbt_left(uintptr_t inButton) { return SBB(inButton)->GetRectangle().GetLeft(); }
extern "C" float ee_screenscpp_sbt_top(uintptr_t inButton) { return SBB(inButton)->GetRectangle().GetTop(); }
extern "C" void ee_screenscpp_sbt_queue_new(uintptr_t inButton, uintptr_t inTable)
{ SBB(inButton)->mBuildQueue = new GEText(SBT(inTable).GetFont()); }
extern "C" void ee_screenscpp_sbt_queue_verify(uintptr_t inButton) { ESVERIFY_AT(static_cast<GEText*>(SBB(inButton)->mBuildQueue), 195); }
extern "C" void ee_screenscpp_sbt_queue_setup(uintptr_t inButton, uintptr_t inTable)
{ SBT(inTable).SetupGEText(static_cast<GEText*>(SBB(inButton)->mBuildQueue)); }
extern "C" uintptr_t ee_screenscpp_sbt_load_dither(uintptr_t inManager)
{ return (uintptr_t)SBM(inManager)->GetRasterizer()->GetTextureManager(SBM(inManager)->GetScreen())->LoadTexture("textures\\Dither", GETextureDescription{1, 0, SBM(inManager)->GetUITextureBitDepth(), 0, false, false, true, true}); }
extern "C" void ee_screenscpp_sbt_dither_new(uintptr_t inButton) { SBB(inButton)->mDither = new GEBitmap; }
extern "C" void ee_screenscpp_sbt_dither_texture(uintptr_t inButton, uintptr_t inTexture, uintptr_t inManager)
{ static_cast<GEBitmap*>(SBB(inButton)->mDither)->SetTexture(reinterpret_cast<GETexture*>(inTexture), SBM(inManager)->GetScreen()); }
extern "C" float ee_screenscpp_sbt_texture_width(uintptr_t inTexture)
{ return (float)reinterpret_cast<GETexture*>(inTexture)->GetSurface()->GetWidth(); }
extern "C" float ee_screenscpp_sbt_texture_height(uintptr_t inTexture)
{ return (float)reinterpret_cast<GETexture*>(inTexture)->GetSurface()->GetHeight(); }
extern "C" void ee_screenscpp_sbt_dither_stretch(uintptr_t inButton, uintptr_t inRectangle, float inWidth, float inHeight, uintptr_t inManager)
{ static_cast<GEBitmap*>(SBB(inButton)->mDither)->ComputeStretchedCoordsAndScale(*reinterpret_cast<const GERectangle*>(inRectangle), *reinterpret_cast<const GERectangle*>(inRectangle), GERectangle(0.0f, 0.0f, inWidth, inHeight), SBM(inManager)->GetScreen()); }

void EEUIFMGStatusButton::ButtonUp() { static_cast<EEUIFMainGame*>(this->GetForm())->ButtonCall(this, EE_SCREENS_SB_DO_UP); }

long EEUIFMGStatusButton::CopyData() { return (long)static_cast<EEUIFMainGame*>(this->GetForm())->ButtonCall(this, EE_SCREENS_SB_DO_COPY_DATA); }
#undef SBM
#undef SBR
#undef SBRB
#undef SBRF
#undef SBB
#undef SBU
#undef SBN
#undef SBK
#undef SBT

// ==== EEUIManager.cpp ===============================================================================================
// C:\EEX\Empire Earth\User Interface Thread\EEUIManager.cpp (file name inferred: no __FILE__; RTTI class
// EEUIManager, vtable 0x44ae90). The game's UI manager: 0x271319..0x2753b1 (EEUIMiniMap.cpp's chains follow). The
// ctor (registers the event factories below and the form-event factory), ??_G, dtor, then the factories in address
// order, each followed by the ??_G bodies of the events it creates first, then the overrides, the message boxes,
// StartScenarioEditor and the callback vectors' COMDATs. The factories have provisional names (by the forms they
// serve); their case ids and classes come from emulating the original dispatch. Unit flags: the exe module set.
// Creates the game's UI manager: registers the control and form event factories (the game settings one
// twice), loads the UI database and finds the main game form
EEUIManager::EEUIManager(GERasterizer* inRasterizer, unsigned long inNumberOfSurfaces, unsigned long inScreen,
                         const UString& inUnused)
    : UIManager(inRasterizer, gSoundManager, inNumberOfSurfaces, gTexturePixelDepth, gResource, inScreen)
{
    // (the original's one factory per form, registered campaign editor, chat log, civilization builder, game settings,
    // save/load, main game, main screen, post-game summary, scenario intro, scenario load, single player load, game
    // settings again, WON server select, debug sound: one Rust table in that order but WON's, whose ids are its own;
    // the port's controls, the forms, the music and the save form's event: ee_screens ui_manager.rs)
    ee_screens_ui_manager_new(this, EEUIManagerCall(this).Ptr());
}

// Destroys the UI manager and its scenario editor form
EEUIManager::~EEUIManager()
{
    delete this->GetScenarioEditorForm();
    ee_screens_ui_manager_fields_drop(this);
}

// Creates a game form's event for a control id; NULL for an id no factory knows. The original's factories per form
// (EECreateGameSettingsEvent, EECreateSaveLoadEvent, EECreateDebugSoundEvent, EECreateCampaignEditorEvent,
// EECreateChatLogEvent, EECreateCivilizationBuilderEvent, EECreateMainGameEvent, EECreateMainScreenEvent with the
// single player and play campaign factories 0x674200 and 0x673836 folded in, EECreatePostGameSummaryEvent,
// EECreateScenarioIntroEvent, EECreateScenarioLoadEvent, EECreateSinglePlayerLoadEvent) are one Rust table in their
// registration order (ee_screens factories.rs). The save/load screens' kind comes from the screen and the editor;
// their Cancel opens the form the load was made for; the main game form's unit reload is C++.
UIEvent* EECreateGameEvent(UIForm* inForm, unsigned long inID)
{
    return reinterpret_cast<UIEvent*>(ee_screens_um_game_event((uintptr_t)inForm, (uint32_t)inID));   // (ui_manager_calls.rs)
}

// Creates the event a form fires when it opens, closes or activates; NULL for other ids (ee_screens factories.rs;
// WON's server select open is C++)
UIEvent* EECreateFormEvent(UIManager* inManager, unsigned long inID) { return reinterpret_cast<UIEvent*>(ee_screens_um_form_event((uint32_t)inID)); }

// Creates the WON server select form's cancel event; NULL for other ids
UIEvent* EECreateWonNetServerSelectEvent(UIForm* inForm, unsigned long inID) { return reinterpret_cast<UIEvent*>(ee_screens_um_wns_event((uint32_t)inID)); }

// (the ids' agreement with ee_screens ui_manager.rs' is ui_ids.rs' test)

// The Rust's calls on the manager, the toolkit, the world and the threads (EE_SCREENS_UM_OP_*, ui_manager_ffi.rs
// picks), each the original's
extern "C" int64_t ee_screenscpp_um_engine_count() { return EECPEngineChoiceCount(); }
extern "C" void ee_screenscpp_um_music(void* inCall)
{ static_cast<EEUIManager*>(static_cast<EEUIManagerCall*>(inCall)->mManager)->SetMusicVolume(!gGamePreferences.mSettings.mMusicEnabled ? 0.0f : gGamePreferences.mSettings.mMusicVolume); }
extern "C" int64_t ee_screenscpp_um_citizens() { return gWorldTable.GetLongValue(kWorldStartingCitizensDefault); }
extern "C" int64_t ee_screenscpp_um_base_keyboard(void* inCall)
{ return EEUIManager::BaseKeyboard(static_cast<EEUIManager*>(static_cast<EEUIManagerCall*>(inCall)->mManager)); }
extern "C" int64_t ee_screenscpp_um_base_mouse(void* inCall)
{ return EEUIManager::BaseMouse(static_cast<EEUIManager*>(static_cast<EEUIManagerCall*>(inCall)->mManager), *static_cast<const GE2DPoint*>(static_cast<EEUIManagerCall*>(inCall)->mMouse)); }

// ---- the one-statement shims of ee_screens ui_manager_calls.rs (UI lane round 8): each one call, no logic
#define UMCALL(x) (*reinterpret_cast<EEUIManagerCall*>(x))
#define UMG(x) reinterpret_cast<EEUIManager*>(x)
#define UMF(x) reinterpret_cast<UIForm*>(x)
#define UMCT(x) (*reinterpret_cast<UIControlsTable*>(x))
#define UMBT(x) (*reinterpret_cast<const UIBackgroundTable*>(x))
#define UMCTL(x) (*reinterpret_cast<const EeScreensManagerControl*>(x))
#define UMMOVE(x) (*reinterpret_cast<const EeScreensManagerMove*>(x))
static const unsigned long kUMKeys[] = {kKeyEscape, kKeyReturn, kKeySpace, kKeyF9};
// (ui_manager_calls.rs' ids equal the headers': hud_cpp_consts_tests.rs)
extern "C" uintptr_t ee_screenscpp_um_call_manager(uintptr_t inCall) { return (uintptr_t)static_cast<EEUIManager*>(UMCALL(inCall).mManager); }
extern "C" void ee_screenscpp_um_register_game_event(uintptr_t inManager)
{ UMG(inManager)->RegisterControlEventCallback(EECreateGameEvent); }
extern "C" void ee_screenscpp_um_register_wns_event(uintptr_t inManager)
{ UMG(inManager)->RegisterControlEventCallback(EECreateWonNetServerSelectEvent); }
extern "C" void ee_screenscpp_um_register_form_event(uintptr_t inManager)
{ UMG(inManager)->RegisterFormEventCallback(EECreateFormEvent); }
extern "C" void ee_screenscpp_um_load_database(uintptr_t inManager)
{ UMG(inManager)->LoadDatabase(gDataPath, UString()); }
extern "C" int64_t ee_screenscpp_um_add_like(uintptr_t inManager, uintptr_t inControl)
{ return UIControlsTable::AddControlLike(UMG(inManager), UMCTL(inControl).template_id, UMCTL(inControl).id, UMCTL(inControl).form_index, UMCTL(inControl).group, UMCTL(inControl).left, UMCTL(inControl).right, UMCTL(inControl).top, UMCTL(inControl).bottom, UMCTL(inControl).string_begin, UMCTL(inControl).string_end, UMCTL(inControl).help); }
extern "C" int64_t ee_screenscpp_um_move(uintptr_t inMoves)
{ return UIControlsTable::MoveControlsHorizontally(UMMOVE(inMoves).form, UMMOVE(inMoves).first, UMMOVE(inMoves).last, UMMOVE(inMoves).left_delta, UMMOVE(inMoves).right_delta); }
extern "C" void ee_screenscpp_um_clear_u138(uintptr_t inManager) { UMG(inManager)->mU138 = 0; }
extern "C" void ee_screenscpp_um_save_form_set_activate(uintptr_t inManager, uintptr_t inEvent)
{ UMG(inManager)->GetFormByID(kFormSaveGame)->SetFormActivateEvent(reinterpret_cast<UIEvent*>(inEvent)); }
extern "C" void ee_screenscpp_um_set_in_editor(uintptr_t inManager) { UMG(inManager)->mInScenarioEditor = true; }
extern "C" void ee_screenscpp_um_clear_multiplayer() { gGameState.mGameOptions.mIsMultiplayer = false; }   // (the original's gGameStateIsMultiplayer)
extern "C" bool ee_screenscpp_comms_present() { return gCommunications != NULL; }
extern "C" void ee_screenscpp_comms_delete() { delete gCommunications; }
extern "C" void ee_screenscpp_um_new_server()
{ gCommunications = new EEServer(UWideString("EEName"), gGamePreferences.mSettings.GetNetworkAdapter(), 0, 0, true, true); }
extern "C" void ee_screenscpp_server_clear_load_responses()
{ static_cast<EEServer*>(gCommunications)->mNumLoadResponses = 0; }
extern "C" void ee_screenscpp_lobby_set_map_size(uint32_t inValue) { gWorld.GetLobbyOptions().mMapSize = inValue; }
extern "C" void ee_screenscpp_lobby_set_load_type(uint32_t inValue) { gWorld.GetLobbyOptions().mLoadType = inValue; }
extern "C" void ee_screenscpp_lobby_set_epoch_prehistoric()
{ gWorld.GetLobbyOptions().mEpochStart = kTechEpochPrehistoric; }
extern "C" void ee_screenscpp_lobby_set_variant(uint32_t inValue) { gWorld.GetLobbyOptions().mVariant = inValue; }
extern "C" void ee_screenscpp_um_name_human(uintptr_t inCall)
{ gResource->GetString(IDS_SE_HUMAN, UMCALL(inCall).mNames[0]); }
extern "C" void ee_screenscpp_um_name_computer(uintptr_t inCall)
{ gResource->GetString(IDS_COMPUTER, UMCALL(inCall).mNames[1]); }
extern "C" void ee_screenscpp_lobby_player_reset(uint32_t inSlot)
{ gWorld.GetLobbyOptions().mPlayers[inSlot] = EEGameOptionsPlayer(); }
extern "C" void ee_screenscpp_lobby_player_set_type(uint32_t inSlot, uint32_t inValue)
{ gWorld.GetLobbyOptions().mPlayers[inSlot].mType = inValue; }
extern "C" void ee_screenscpp_um_lobby_player_set_name(uintptr_t inCall, uint32_t inSlot, uint32_t inName)
{ gWorld.GetLobbyOptions().mPlayers[inSlot].mName = UMCALL(inCall).mNames[inName]; }
extern "C" void ee_screenscpp_lobby_player_set_slot(uint32_t inSlot, uint32_t inValue)
{ gWorld.GetLobbyOptions().mPlayers[inSlot].mSlot = inValue; }
extern "C" void ee_screenscpp_lobby_player_set_color(uint32_t inSlot, uint32_t inValue)
{ gWorld.GetLobbyOptions().mPlayers[inSlot].mColor = inValue; }
extern "C" void ee_screenscpp_lobby_player_set_citizens(uint32_t inSlot, uint32_t inValue)
{ gWorld.GetLobbyOptions().mPlayers[inSlot].mStartingCitizens = inValue; }
extern "C" void ee_screenscpp_lobby_player_set_local(uint32_t inSlot, bool inValue)
{ gWorld.GetLobbyOptions().mPlayers[inSlot].mIsLocal = inValue; }
extern "C" void ee_screenscpp_lobby_player_set_ready(uint32_t inSlot, bool inValue)
{ gWorld.GetLobbyOptions().mPlayers[inSlot].mIsReady = inValue; }
extern "C" void ee_screenscpp_comms_start_loading()
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageStartLoading, {1, TMGetTime()}), kSlotNone); }
extern "C" void ee_screenscpp_um_key_load(uint32_t inKey) { gISKeyboard->GetEntity(kUMKeys[inKey], EEScratch<ISEntity*>(kScratchUMKey)); }
extern "C" bool ee_screenscpp_um_key_just_pressed() { return EEScratch<ISEntity*>(kScratchUMKey)->JustPressed(); }
extern "C" uintptr_t ee_screenscpp_um_current_form(uintptr_t inManager)
{ return (uintptr_t)UMG(inManager)->GetCurrentForm(); }
extern "C" bool ee_screenscpp_form_keyboard_captured(uintptr_t inForm)
{ return reinterpret_cast<UIForm*>(inForm)->GetIsKeyboardCaptured(); }
extern "C" bool ee_screenscpp_loading_saved_game() { return gWorld.mLoadingSavedGame; }
extern "C" uintptr_t ee_screenscpp_form_manager(uintptr_t inForm)
{ return (uintptr_t)reinterpret_cast<UIForm*>(inForm)->GetManager(); }
extern "C" bool ee_screenscpp_um_in_editor(uintptr_t inManager) { return UMG(inManager)->mInScenarioEditor; }
extern "C" bool ee_screenscpp_game_screen_active() { return gWorld.mGameScreenActive; }
extern "C" uintptr_t ee_screenscpp_um_editor_form(uintptr_t inManager)
{ return (uintptr_t)static_cast<UIForm*>(UMG(inManager)->GetScenarioEditorForm()); }
extern "C" uintptr_t ee_screenscpp_um_main_game_form(uintptr_t inManager)
{ return (uintptr_t)static_cast<UIForm*>(UMG(inManager)->GetMainGameForm()); }
extern "C" uintptr_t ee_screenscpp_um_new_cancel(uintptr_t inForm)
{ return (uintptr_t)static_cast<UIEvent*>(new EEScreenEvent(EE_SCREENS_EV_CANCEL, inForm)); }
extern "C" uintptr_t ee_screenscpp_um_new_load_unit() { return (uintptr_t)static_cast<UIEvent*>(new EEScreenEvent(EE_SCREENS_EV_LOAD_UNIT)); }
// the events' bodies (cpp_events.rs)
extern "C" bool ee_screenscpp_uh_in_game_form()
{ return dynamic_cast<EEUIFMainGame*>(gUIManager->GetCurrentForm()) != NULL || dynamic_cast<EEUIFScenarioEditor*>(gUIManager->GetCurrentForm()) != NULL; }
extern "C" long ee_screenscpp_ev_open_form(uintptr_t inForm) { return gUserInterface->OpenForm(reinterpret_cast<UIForm*>(inForm)); }
extern "C" void ee_screenscpp_ev_load_unit(uintptr_t inForm)
{ static_cast<EEUIFMGUnitHack*>(reinterpret_cast<EEUIFMainGame*>(inForm)->mUnitHack)->LoadUnitData(); }
extern "C" uintptr_t ee_screenscpp_um_new_wns_open() { return (uintptr_t)static_cast<UIEvent*>(new EEUIEWonNetEvent(EeWonFrontCall::StatsOpen)); }
extern "C" uintptr_t ee_screenscpp_um_new_wns_cancel() { return (uintptr_t)static_cast<UIEvent*>(new EEUIEWonNetEvent(EeWonFrontCall::StatsCancel)); }
extern "C" void ee_screenscpp_um_new_editor(uintptr_t inManager)
{ UMG(inManager)->mScenarioEditorForm = new EEUIFScenarioEditor(UMG(inManager)); }
extern "C" void ee_screenscpp_um_delete_editor(uintptr_t inManager) { delete UMG(inManager)->GetScenarioEditorForm(); }
extern "C" void ee_screenscpp_um_clear_editor(uintptr_t inManager) { UMG(inManager)->mScenarioEditorForm = NULL; }
extern "C" uint32_t ee_screenscpp_um_form_data_id(uintptr_t inData)
{ return (uint32_t)reinterpret_cast<const UIDbDataForms*>(inData)->mFormID; }
extern "C" uintptr_t ee_screenscpp_um_base_create_form(uintptr_t inManager, uintptr_t inData)
{ return (uintptr_t)static_cast<UIForm*>(UMG(inManager)->UIManager::CreateForm(*reinterpret_cast<const UIDbDataForms*>(inData))); }
extern "C" uintptr_t ee_screenscpp_um_new_main_game(uintptr_t inManager, uintptr_t inData)
{ return (uintptr_t)static_cast<UIForm*>(new EEUIFMainGame(UMG(inManager), *reinterpret_cast<const UIDbDataForms*>(inData))); }
extern "C" uint32_t ee_screenscpp_bt_type(uintptr_t inTable) { return (uint32_t)UMBT(inTable).GetBackgroundType(); }
extern "C" uint32_t ee_screenscpp_bt_form_id(uintptr_t inTable) { return (uint32_t)UMBT(inTable).GetFormID(); }
extern "C" bool ee_screenscpp_bt_attribute1(uintptr_t inTable) { return UMBT(inTable).GetAttribute1() == true; }
extern "C" bool ee_screenscpp_bt_attribute2(uintptr_t inTable) { return UMBT(inTable).GetAttribute2() == true; }
extern "C" bool ee_screenscpp_bt_attribute3(uintptr_t inTable) { return UMBT(inTable).GetAttribute3() == true; }
extern "C" uintptr_t ee_screenscpp_um_form_by_id(uintptr_t inManager, uint32_t inId)
{ return (uintptr_t)UMG(inManager)->GetFormByID(inId); }
extern "C" uintptr_t ee_screenscpp_um_new_generic_background(uintptr_t inManager, uintptr_t inTable, uintptr_t inForm, uint32_t inFlags)
{ return (uintptr_t)static_cast<UIBackground*>(new EEUIBGenericBackground(UMBT(inTable).GetBackgroundID(), UMBT(inTable).GetTextureFile(), GETextureDescription{1, 0, UMG(inManager)->GetUITextureBitDepth(), 0, false, false, UMBT(inTable).GetHasAlpha(), true}, UMBT(inTable).GetLayout(), UMF(inForm), UMBT(inTable).GetDrawPriority(), UMBT(inTable).GetColor(), UMBT(inTable).GetVisible(), UMBT(inTable).GetOpacity(), 1.0f, 1.0f, inFlags)); }
extern "C" uint32_t ee_screenscpp_ct_form_id(uintptr_t inTable) { return (uint32_t)UMCT(inTable).GetFormID(); }
extern "C" uint32_t ee_screenscpp_ct_type(uintptr_t inTable) { return (uint32_t)UMCT(inTable).GetControlType(); }
extern "C" uint32_t ee_screenscpp_um_window_width(uintptr_t inManager)
{ return (uint32_t)UMG(inManager)->GetRasterizer()->GetWindow(UMG(inManager)->GetScreen())->GetWidth(); }
extern "C" uint32_t ee_screenscpp_um_window_height(uintptr_t inManager)
{ return (uint32_t)UMG(inManager)->GetRasterizer()->GetWindow(UMG(inManager)->GetScreen())->GetHeight(); }
extern "C" bool ee_screenscpp_form_exclusive_coords(uintptr_t inForm) { return UMF(inForm)->GetUseExclusiveCoords(); }
extern "C" void ee_screenscpp_um_parent_set(float inLeft, float inTop, float inRight, float inBottom)
{ EEScratch<GERectangle>(kScratchUMParent) = GERectangle(inLeft, inTop, inRight, inBottom); }
extern "C" void ee_screenscpp_ct_layout_rectangle(uintptr_t inTable, uintptr_t inForm)
{ UMCT(inTable).GetLayout().CalculateRectangle(EEScratch<GERectangle>(kScratchUMParent), EEScratch<GERectangle>(kScratchUMRect), UMF(inForm)->GetUseExclusiveCoords()); }
extern "C" void ee_screenscpp_ct_strings(uintptr_t inManager, uintptr_t inTable)
{ UMCT(inTable).SetupStringArray(UMG(inManager), EEScratch<std::vector<UWideString>>(kScratchUMStrings)); }
extern "C" void ee_screenscpp_um_strings_clear() { EEScratch<std::vector<UWideString>>(kScratchUMStrings).clear(); }
extern "C" uintptr_t ee_screenscpp_ct_new_mini_map(uintptr_t inTable, uintptr_t inForm)
{ return (uintptr_t)static_cast<UIControl*>(new EEUIMiniMap(EEScratch<GERectangle>(kScratchUMRect), UMF(inForm), UMCT(inTable).GetFormIndex(), UMCT(inTable).GetID(), 0)); }
extern "C" uintptr_t ee_screenscpp_ct_new_status_button(uintptr_t inManager, uintptr_t inTable)
{ return (uintptr_t)static_cast<UIControl*>(new EEUIFMGStatusButton(UMG(inManager), EEScratch<GERectangle>(kScratchUMRect), UMCT(inTable))); }
extern "C" uintptr_t ee_screenscpp_ct_new_error_log(uintptr_t inTable, uintptr_t inForm)
{ return (uintptr_t)static_cast<UIControl*>(new EEUIShellList(EE_SCREENS_SK_ERROR_LOG, {}, UMCT(inTable).GetID(), std::vector<UWideString>(), UMCT(inTable).GetFont(), false, EEScratch<GERectangle>(kScratchUMRect), UMF(inForm), UMCT(inTable).GetFormIndex(), 0)); }
extern "C" uintptr_t ee_screenscpp_ct_new_attribute(uintptr_t inTable, uintptr_t inForm)
{ return (uintptr_t)static_cast<UIControl*>(new EEUIFMGAttribute(UMCT(inTable).GetFont(), UMF(inForm), EEScratch<GERectangle>(kScratchUMRect), UMCT(inTable).GetFormIndex(), UMCT(inTable).GetID(), UMCT(inTable).GetTextForeground(), false, UMCT(inTable).GetBorderThickness())); }
extern "C" uintptr_t ee_screenscpp_ct_new_status_bar(uintptr_t inTable, uintptr_t inForm)
{ return (uintptr_t)static_cast<UIControl*>(new EEUIFMGStatusBar(UMCT(inTable).GetFormID(), UMCT(inTable).GetFormIndex(), (UILayoutUtility*)&UMCT(inTable).GetLayout(), 800, 600, EEScratch<GERectangle>(kScratchUMRect), UMF(inForm), false, true, UMCT(inTable).GetLoopMovie())); }
extern "C" uintptr_t ee_screenscpp_ct_new_select_units(uintptr_t inManager, uintptr_t inTable, uintptr_t inForm)
{ return (uintptr_t)static_cast<UIControl*>(new EEUICSelectUnits(UMF(inForm), UMCT(inTable).GetID(), UMCT(inTable).GetFormIndex(), static_cast<EEAMouseClick*>(UMG(inManager)->GetMainGameForm()->mMouseClick), static_cast<EEALasso*>(UMG(inManager)->GetMainGameForm()->mLasso))); }
extern "C" uintptr_t ee_screenscpp_ct_new_command_units(uintptr_t inManager, uintptr_t inTable, uintptr_t inForm)
{ return (uintptr_t)static_cast<UIControl*>(new EEUICCommandUnits(UMF(inForm), UMCT(inTable).GetID(), UMCT(inTable).GetFormIndex(), static_cast<EEAMouseClick*>(UMG(inManager)->GetMainGameForm()->mMouseClick))); }
extern "C" uintptr_t ee_screenscpp_ct_new_portrait(uintptr_t inTable, uintptr_t inForm)
{ return (uintptr_t)static_cast<UIControl*>(new EEUICPortrait(UMF(inForm), EEScratch<GERectangle>(kScratchUMRect), UMCT(inTable).GetID(), UMCT(inTable).GetFormIndex(), false)); }
extern "C" void ee_screenscpp_ct_setup_font(uintptr_t inManager, uintptr_t inTable)
{ UMCT(inTable).SetupFont(UMG(inManager), EEScratch<GERectangle>(kScratchUMRect), true); }
extern "C" uintptr_t ee_screenscpp_ct_new_cb_list_box(uintptr_t inTable, uintptr_t inForm)
{ return (uintptr_t)static_cast<UIControl*>(new EECBUIListBox(UMCT(inTable), UMCT(inTable).GetID(), EEScratch<std::vector<UWideString>>(kScratchUMStrings), UMCT(inTable).GetFont(), EEScratch<GERectangle>(kScratchUMRect), UMF(inForm), UMCT(inTable).GetFormIndex(), UMCT(inTable).GetBorderThickness(), UMCT(inTable).GetStartChecked())); }
extern "C" uintptr_t ee_screenscpp_ct_new_rooms_box(uintptr_t inTable, uintptr_t inForm)
{ return (uintptr_t)static_cast<UIControl*>(new EEUICWonNetRoomsBox(UMCT(inTable).GetID(), UMCT(inTable).GetFont(), EEScratch<GERectangle>(kScratchUMRect).GetTopLeft(), (unsigned long)EEScratch<GERectangle>(kScratchUMRect).GetWidth(), (unsigned long)EEScratch<GERectangle>(kScratchUMRect).GetHeight(), UMF(inForm), UMCT(inTable).GetFormIndex(), UMCT(inTable).GetBorderThickness())); }
extern "C" uintptr_t ee_screenscpp_ct_new_games_box(uintptr_t inTable, uintptr_t inForm)
{ return (uintptr_t)static_cast<UIControl*>(new EEUICWonNetGamesBox(UMCT(inTable).GetID(), UMCT(inTable).GetFont(), EEScratch<GERectangle>(kScratchUMRect).GetTopLeft(), (unsigned long)EEScratch<GERectangle>(kScratchUMRect).GetWidth(), (unsigned long)EEScratch<GERectangle>(kScratchUMRect).GetHeight(), UMF(inForm), UMCT(inTable).GetFormIndex(), UMCT(inTable).GetBorderThickness())); }
extern "C" uintptr_t ee_screenscpp_ct_new_stat_box(uintptr_t inTable, uintptr_t inForm)
{ return (uintptr_t)static_cast<UIControl*>(new EEUICWonNetStatBox(UMCT(inTable).GetID(), UMCT(inTable).GetFont(), EEScratch<GERectangle>(kScratchUMRect).GetTopLeft(), (unsigned long)EEScratch<GERectangle>(kScratchUMRect).GetWidth(), (unsigned long)EEScratch<GERectangle>(kScratchUMRect).GetHeight(), UMF(inForm), UMCT(inTable).GetFormIndex(), UMCT(inTable).GetBorderThickness())); }
extern "C" uintptr_t ee_screenscpp_ct_new_saved_group(uintptr_t inTable, uintptr_t inForm)
{ return (uintptr_t)static_cast<UIButton*>(new EEUIShellButton(EE_SCREENS_SK_SAVED_GROUP, {}, UMCT(inTable).GetID(), NULL, NULL, NULL, EEScratch<GERectangle>(kScratchUMRect), UMF(inForm), UMCT(inTable).GetFormIndex(), 0)); }
extern "C" uintptr_t ee_screenscpp_ct_normal_bitmap(uintptr_t inTable, uintptr_t inForm)
{ return (uintptr_t)UMCT(inTable).CreateNormalBitmap(UMF(inForm)->GetManager(), EEScratch<GERectangle>(kScratchUMRect)); }
extern "C" uintptr_t ee_screenscpp_ct_pressed_bitmap(uintptr_t inTable, uintptr_t inForm)
{ return (uintptr_t)UMCT(inTable).CreatePressedBitmap(UMF(inForm)->GetManager(), EEScratch<GERectangle>(kScratchUMRect)); }
extern "C" void ee_screenscpp_um_button_set_normal(uintptr_t inButton, uintptr_t inBitmap)
{ reinterpret_cast<UIButton*>(inButton)->SetNormalBitmap(reinterpret_cast<GEBitmap*>(inBitmap)); }
extern "C" void ee_screenscpp_um_button_set_pressed(uintptr_t inButton, uintptr_t inBitmap)
{ reinterpret_cast<UIButton*>(inButton)->SetPressedBitmap(reinterpret_cast<GEBitmap*>(inBitmap)); }
extern "C" void ee_screenscpp_um_bitmap_dereference(uintptr_t inBitmap)
{ reinterpret_cast<GEBitmap*>(inBitmap)->Dereference(); }
extern "C" uintptr_t ee_screenscpp_ct_new_pgs_graph(uintptr_t inTable, uintptr_t inForm)
{ return (uintptr_t)static_cast<UIControl*>(new UIGraph(UMCT(inTable).GetID(), EEScratch<GERectangle>(kScratchUMRect), UMF(inForm), UMCT(inTable).GetFormIndex(), UMCT(inTable).GetBorderThickness(), EeUiGraphKind::PostGame, ee_screens_pgs_format)); }
extern "C" void ee_screenscpp_ct_graph_font(uintptr_t inGraph, uintptr_t inTable)
{ reinterpret_cast<UIGraph*>(inGraph)->SetFont(UMCT(inTable).GetFont()); }
extern "C" void ee_screenscpp_ct_graph_font_color(uintptr_t inGraph, uintptr_t inTable)
{ reinterpret_cast<UIGraph*>(inGraph)->SetFontColor(UMCT(inTable).GetTextForeground()); }
extern "C" uintptr_t ee_screenscpp_ct_new_ping(uintptr_t inTable, uintptr_t inForm)
{ return (uintptr_t)static_cast<UIControl*>(new EEUICPing(UMCT(inTable).GetID(), EEScratch<GERectangle>(kScratchUMRect).GetTopLeft(), UMF(inForm), UMCT(inTable).GetFormIndex())); }
extern "C" uintptr_t ee_screenscpp_um_button_control(uintptr_t inButton)
{ return (uintptr_t)static_cast<UIControl*>(reinterpret_cast<UIButton*>(inButton)); }

// the manager's construction as the game's start makes it (Empire Earth.cpp: ee/EEGameUI.h)
static const bool sManagerRegistered =
    (gEEGameUI.mNewManager = [](GERasterizer* inRasterizer, unsigned long inNumberOfSurfaces, unsigned long inScreen,
                                const UString& inUnused) { return new EEUIManager(inRasterizer, inNumberOfSurfaces,
                                                                                  inScreen, inUnused); },
     true);
// the icon bar's and chat line's shells over the factory's rectangle (shell_factories.rs, UI lane round 12)
extern "C" uintptr_t ee_screenscpp_sf_icon_bar_new(uintptr_t inTable, uintptr_t inForm)
{ return (uintptr_t)static_cast<UIControl*>(new EEUIShellControl(EE_SCREENS_SK_ICON_BAR, std::vector<int64_t>(), UMCT(inTable).GetID(), EEScratch<GERectangle>(kScratchUMRect), UMF(inForm), UMCT(inTable).GetFormIndex(), 0)); }
extern "C" uintptr_t ee_screenscpp_sf_chat_line_new(uintptr_t inTable, uintptr_t inForm, int64_t inOpacityBits)
{ return (uintptr_t)static_cast<UIControl*>(new EEUIShellControl(EE_SCREENS_SK_CHAT_LINE, std::vector<int64_t>(1, inOpacityBits), UMCT(inTable).GetID(), EEScratch<GERectangle>(kScratchUMRect), UMF(inForm), UMCT(inTable).GetFormIndex(), UMCT(inTable).GetBorderThickness())); }
#undef UMCALL
#undef UMG
#undef UMF
#undef UMCT
#undef UMBT
#undef UMCTL
#undef UMMOVE

// ==== EEUIMiniMap.cpp ===============================================================================================
// C:\EEX\Empire Earth\User Interface Thread\EEUIMiniMap.cpp (file name inferred: no __FILE__; RTTI class
// EEUIMiniMap). The mini-map control: 0x2753b1..0x276442 (the `$E` chains of sBounds and the two flare colours, ctor,
// ??_G, dtor, then the members in address order, EEAMiniMap's inline ctor after its first user). Unit flags: the exe
// module set.
// (EEMSendFlare's inline ctor: ee/EEMessage.h)

GERectangle EEUIMiniMap::sBounds;
static GEColor sFlareColor(1.0f, 0.0f, 0.0f);

// Creates the mini map control with its click action, the map drawn inside its border

// Snapshots the mini map for drawing: refreshes the units texture, scales the map to the window, places the
// camera outline and the blinking flare (redrawing the screen when the flare leaves the map's inner area); the
// numbers are ee_screens mini_map.rs', the C++ reads and applies them in the original's order

// NEAR: the original's frame has a zeroed dword at -4 (not identified) and builds the message's tile in the
// tile's slot after operator new

// (the Rust's calls on the control, the mouse and the world, EE_SCREENS_MM_OP_*, are mini_map_calls.rs' over the
// shims below)

// ---- the one-statement shims of ee_screens mini_map_calls.rs (UI lane round 8): each one call, no logic
#define MM(x) reinterpret_cast<EEUIMiniMap*>(x)
static GEBitmap** const kMMBitmaps[3] = {&EEScratch<GEBitmap*>(kScratchMapBitmap), &EEScratch<GEBitmap*>(kScratchWorldBitmap), &EEScratch<GEBitmap*>(kScratchFlareBitmap)};
static const GEColor* const kMMFlareColors[2] = {&sFlareColor, &EEScratch<GEColor>(kScratchFlareBlinkColor)};
static bool EEUIMiniMap::* const kMMLines[4] = {&EEUIMiniMap::mDrawLine0, &EEUIMiniMap::mDrawLine1,
                                                &EEUIMiniMap::mDrawLine2, &EEUIMiniMap::mDrawLine3};
static GE2DPoint EEMiniMap::* const kMMCorners[4] = {&EEMiniMap::mCorner0, &EEMiniMap::mCorner1, &EEMiniMap::mCorner2,
                                                     &EEMiniMap::mCorner3};
extern "C" float ee_screenscpp_mm_rect_left(uintptr_t inRect)
{ return reinterpret_cast<const GERectangle*>(inRect)->GetLeft(); }
extern "C" float ee_screenscpp_mm_rect_top(uintptr_t inRect)
{ return reinterpret_cast<const GERectangle*>(inRect)->GetTop(); }
extern "C" void ee_screenscpp_mm_action_new(uintptr_t inMap) { MM(inMap)->mAction = new EEAMiniMap; }
extern "C" void ee_screenscpp_mm_set_origin(uintptr_t inMap, float inX, float inY)
{ MM(inMap)->mOrigin = GE2DPoint(inX, inY); }
extern "C" void ee_screenscpp_mm_set_lines(uintptr_t inMap, bool inDraw)
{ MM(inMap)->mDrawLine1 = MM(inMap)->mDrawLine3 = MM(inMap)->mDrawLine0 = MM(inMap)->mDrawLine2 = inDraw; }
extern "C" bool ee_screenscpp_mm_line_drawn(uintptr_t inMap, uint32_t inLine) { return MM(inMap)->*kMMLines[inLine]; }
extern "C" void ee_screenscpp_mm_set_show_flare(uintptr_t inMap, bool inShow) { MM(inMap)->mShowFlare = inShow; }
extern "C" void ee_screenscpp_mm_set_flare_blink(uintptr_t inMap, bool inBlink) { MM(inMap)->mFlareBlink = inBlink; }
extern "C" bool ee_screenscpp_mm_show_flare(uintptr_t inMap) { return MM(inMap)->mShowFlare; }
extern "C" bool ee_screenscpp_mm_flare_blink(uintptr_t inMap) { return MM(inMap)->mFlareBlink; }
extern "C" uintptr_t ee_screenscpp_mm_action(uintptr_t inMap) { return (uintptr_t)static_cast<EEAMiniMap*>(MM(inMap)->mAction); }
extern "C" void ee_screenscpp_mm_action_dereference(uintptr_t inMap) { static_cast<EEAMiniMap*>(MM(inMap)->mAction)->Dereference(); }
extern "C" float ee_screenscpp_mm_origin_x(uintptr_t inMap) { return MM(inMap)->mOrigin.GetX(); }
extern "C" float ee_screenscpp_mm_origin_y(uintptr_t inMap) { return MM(inMap)->mOrigin.GetY(); }
extern "C" uintptr_t ee_screenscpp_mm_map_rect(uintptr_t inMap) { return (uintptr_t)&MM(inMap)->GetRectangle(); }
extern "C" uintptr_t ee_screenscpp_mm_drawn_rect(uintptr_t inMap)
{ return (uintptr_t)&MM(inMap)->GetCopiedDrawRectangle(); }
extern "C" float ee_screenscpp_mm_rect_right(uintptr_t inRect)
{ return reinterpret_cast<const GERectangle*>(inRect)->GetRight(); }
extern "C" float ee_screenscpp_mm_rect_bottom(uintptr_t inRect)
{ return reinterpret_cast<const GERectangle*>(inRect)->GetBottom(); }
extern "C" bool ee_screenscpp_mm_flare_on() { return gMiniMap->mB12185; }
extern "C" void ee_screenscpp_mm_update_world_texture() { gMiniMap->UpdateWorldTexture(); }
extern "C" float ee_screenscpp_mm_corner_x(uint32_t inCorner) { return (gMiniMap->*kMMCorners[inCorner]).GetX(); }
extern "C" float ee_screenscpp_mm_corner_y(uint32_t inCorner) { return (gMiniMap->*kMMCorners[inCorner]).GetY(); }
extern "C" uint32_t ee_screenscpp_mm_window_width() { return (uint32_t)gGameWindow->GetWidth(); }
extern "C" uint32_t ee_screenscpp_mm_window_height() { return (uint32_t)gGameWindow->GetHeight(); }
extern "C" float ee_screenscpp_mm_flare_point_x() { return gMiniMap->mU1216C.GetX(); }
extern "C" float ee_screenscpp_mm_flare_point_y() { return gMiniMap->mU1216C.GetY(); }
extern "C" float ee_screenscpp_mm_map_to_pixel(uint32_t inRow, uint32_t inColumn)
{ return gMiniMap->mMapToPixel.mMatrix[inRow][inColumn]; }
extern "C" float ee_screenscpp_mm_pixel_to_map(uint32_t inRow, uint32_t inColumn)
{ return gMiniMap->mPixelToMap.mMatrix[inRow][inColumn]; }
extern "C" uintptr_t ee_screenscpp_mm_flare_texture() { return (uintptr_t)EEScratch<GEBitmap*>(kScratchFlareBitmap)->GetTexture(gScreenIndex); }
extern "C" uint32_t ee_screenscpp_mm_texture_height(uintptr_t inTexture)
{ return (uint32_t)reinterpret_cast<GETexture*>(inTexture)->GetHeight(); }
extern "C" uint32_t ee_screenscpp_mm_texture_width(uintptr_t inTexture)
{ return (uint32_t)reinterpret_cast<GETexture*>(inTexture)->GetWidth(); }
extern "C" float ee_screenscpp_mm_flare_location_x() { return EEScratch<GEBitmap*>(kScratchFlareBitmap)->GetLocation().GetX(); }
extern "C" float ee_screenscpp_mm_flare_location_y() { return EEScratch<GEBitmap*>(kScratchFlareBitmap)->GetLocation().GetY(); }
extern "C" float ee_screenscpp_mm_flare_width() { return EEScratch<GEBitmap*>(kScratchFlareBitmap)->GetWidth(); }
extern "C" bool ee_screenscpp_mm_free_camera() { return gFreeCamera; }
extern "C" void ee_screenscpp_mm_bitmap_at_origin(uint32_t inBitmap, uintptr_t inMap)
{ (*kMMBitmaps[inBitmap])->SetLocation(MM(inMap)->mOrigin); }
extern "C" void ee_screenscpp_mm_bitmap_scale(uint32_t inBitmap, float inX, float inY)
{ (*kMMBitmaps[inBitmap])->SetXYScale(inX, inY); }
extern "C" void ee_screenscpp_mm_flare_color(bool inBlink) { EEScratch<GEBitmap*>(kScratchFlareBitmap)->SetColor(*kMMFlareColors[inBlink]); }
extern "C" void ee_screenscpp_mm_redraw_all() { gUIManager->RedrawAll(); }
extern "C" void ee_screenscpp_mm_flare_at(float inX, float inY) { EEScratch<GEBitmap*>(kScratchFlareBitmap)->SetLocation(GE2DPoint(inX, inY)); }
extern "C" void ee_screenscpp_mm_line_start(uintptr_t inMap, uint32_t inLine, float inX, float inY)
{ MM(inMap)->mLines[inLine].SetStartPoint(GE2DPoint(inX, inY)); }
extern "C" void ee_screenscpp_mm_line_end(uintptr_t inMap, uint32_t inLine, float inX, float inY)
{ MM(inMap)->mLines[inLine].SetEndPoint(GE2DPoint(inX, inY)); }
extern "C" int64_t ee_screenscpp_mm_base_copy_data(uintptr_t inMap) { return MM(inMap)->UIControl::CopyData(); }
extern "C" uintptr_t ee_screenscpp_mm_description() { return (uintptr_t)&EEScratch<GETextureDescription>(kScratchMMDescription); }
extern "C" void ee_screenscpp_mm_set_description()
{ EEScratch<GETextureDescription>(kScratchMMDescription) = GETextureDescription{1, 0, gTexturePixelDepth, 0, false, false, true, true}; }
extern "C" void ee_screenscpp_mm_bitmap_new(uint32_t inBitmap) { *kMMBitmaps[inBitmap] = new GEBitmap; }
extern "C" uintptr_t ee_screenscpp_mm_bitmap(uint32_t inBitmap) { return (uintptr_t)*kMMBitmaps[inBitmap]; }
extern "C" void ee_screenscpp_mm_bitmap_dereference(uint32_t inBitmap) { (*kMMBitmaps[inBitmap])->Dereference(); }
extern "C" void ee_screenscpp_mm_bitmap_clear(uint32_t inBitmap) { *kMMBitmaps[inBitmap] = NULL; }
extern "C" void ee_screenscpp_texture_wrap(uintptr_t inTexture)
{ reinterpret_cast<GETexture*>(inTexture)->SetStageTextureAddressingMode(kAddressingWrap, -1); }
extern "C" void ee_screenscpp_mm_flare_texture_set(uintptr_t inTexture)
{ EEScratch<GEBitmap*>(kScratchFlareBitmap)->SetTexture(reinterpret_cast<GETexture*>(inTexture), gScreenIndex); }
extern "C" void ee_screenscpp_mm_terrain_enter() { gMiniMap->mTerrainCriticalSection->Enter(); }
extern "C" void ee_screenscpp_mm_terrain_leave() { gMiniMap->mTerrainCriticalSection->Leave(); }
extern "C" int64_t ee_screenscpp_mm_base_draw(uintptr_t inMap) { return MM(inMap)->UIControl::DrawPolygons(); }
extern "C" int64_t ee_screenscpp_mm_draw_bitmap(uintptr_t inMap, uint32_t inBitmap)
{ return MM(inMap)->GetRasterizer()->Draw(*kMMBitmaps[inBitmap], gScreenIndex); }
extern "C" int64_t ee_screenscpp_mm_draw_line(uintptr_t inMap, uint32_t inLine)
{ return MM(inMap)->GetRasterizer()->Draw(&MM(inMap)->mLines[inLine], 1, gScreenIndex); }
extern "C" void ee_screenscpp_mm_save_view_rect(uintptr_t inMap)
{ EEScratch<GERectangle>(kScratchMMViewRect) = MM(inMap)->GetRasterizer()->GetViewport(gScreenIndex).GetViewRectangle(); }
extern "C" void ee_screenscpp_mm_viewport_load(uintptr_t inMap)
{ EEScratch<GEViewport>(kScratchMMViewport) = MM(inMap)->GetRasterizer()->GetViewport(gScreenIndex); }
extern "C" void ee_screenscpp_mm_viewport_clip(float inLeft, float inTop, float inRight, float inBottom)
{ EEScratch<GEViewport>(kScratchMMViewport).SetViewRectangle(GERectangle(inLeft, inTop, inRight, inBottom)); }
extern "C" void ee_screenscpp_mm_viewport_saved() { EEScratch<GEViewport>(kScratchMMViewport).SetViewRectangle(EEScratch<GERectangle>(kScratchMMViewRect)); }
extern "C" void ee_screenscpp_mm_viewport_apply(uintptr_t inMap)
{ MM(inMap)->GetRasterizer()->SetViewport(EEScratch<GEViewport>(kScratchMMViewport), gScreenIndex); }
extern "C" float ee_screenscpp_mm_mouse_x(uintptr_t inMap) { return MM(inMap)->GetMouseLocation().GetX(); }
extern "C" float ee_screenscpp_mm_mouse_y(uintptr_t inMap) { return MM(inMap)->GetMouseLocation().GetY(); }
extern "C" float ee_screenscpp_mm_form_mouse_x(uintptr_t inMap)
{ return MM(inMap)->GetForm()->GetMouseLocation().GetX(); }
extern "C" float ee_screenscpp_mm_form_mouse_y(uintptr_t inMap)
{ return MM(inMap)->GetForm()->GetMouseLocation().GetY(); }
extern "C" uintptr_t ee_screenscpp_mm_bounds() { return (uintptr_t)&EEUIMiniMap::sBounds; }
extern "C" uint32_t ee_screenscpp_mm_modifiers() { return (uint32_t)gISKeyboard->GetModifierState(); }
extern "C" void ee_screenscpp_mm_action_mode(uintptr_t inMap, int64_t inMode)
{ static_cast<EEAMiniMap*>(MM(inMap)->mAction)->mMode = (long)inMode; }
extern "C" void ee_screenscpp_mm_action_point(uintptr_t inMap, float inX, float inY)
{ static_cast<EEAMiniMap*>(MM(inMap)->mAction)->mPoint = GE2DPoint(inX, inY); }
extern "C" void ee_screenscpp_mm_action_modifiers(uintptr_t inMap, uint32_t inModifiers)
{ static_cast<EEAMiniMap*>(MM(inMap)->mAction)->mModifiers = inModifiers; }
extern "C" void ee_screenscpp_mm_post_action(uintptr_t inMap) { gUserInterface->PostAction(static_cast<EEAMiniMap*>(MM(inMap)->mAction)); }
extern "C" void ee_screenscpp_mm_capture(uintptr_t inMap) { MM(inMap)->CaptureMouse(); }
extern "C" void ee_screenscpp_mm_base_set_location(uintptr_t inMap, float inX, float inY)
{ MM(inMap)->UIControl::SetLocation(GE2DPoint(inX, inY)); }
extern "C" uint32_t ee_screenscpp_mm_border(uintptr_t inMap) { return (uint32_t)MM(inMap)->GetBorderThickness(); }
extern "C" bool ee_screenscpp_mm_tile_exists(int64_t inX, int64_t inY)
{ return ::gWorld.mMap->GetTileAtLocation(U2DPoint<long>((long)inX, (long)inY)) != NULL; }
extern "C" void ee_screenscpp_mm_send_flare(int64_t inX, int64_t inY)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageSendFlare, {(long)inX, (long)inY}), kSlotNone); }
extern "C" void ee_screenscpp_mm_pointer_clear() { gMouse->mPointer = 0; }
extern "C" void ee_screenscpp_mm_release(EEUIMiniMap* inMap) { inMap->ReleaseCapturedMouse(); }
extern "C" bool ee_screenscpp_mm_has_capture(uintptr_t inMap) { return MM(inMap)->GetHasMouseCapture(); }
extern "C" bool ee_screenscpp_mm_left_down(uintptr_t inMap) { return MM(inMap)->GetLeftMouseButtonDown(); }
extern "C" int64_t ee_screenscpp_mm_pointer() { return gMouse->mPointer; }
extern "C" void ee_screenscpp_mm_form_ui_state_clear(uintptr_t inMap)
{ static_cast<EEUIFMainGame*>(MM(inMap)->GetForm())->SetUIState(0); }
extern "C" int32_t ee_screenscpp_mm_map_width() { return (int32_t)::gWorld.mMap->mL195614; }
extern "C" int32_t ee_screenscpp_mm_map_height() { return (int32_t)::gWorld.mMap->mL195610; }
extern "C" int32_t ee_screenscpp_mm_map_size() { return (int32_t)::gWorld.mMap->mSize; }
extern "C" float ee_screenscpp_mm_update_map_to_pixel(uintptr_t inMinimap, uint32_t inRow, uint32_t inColumn)
{ return reinterpret_cast<EEMiniMap*>(inMinimap)->mMapToPixel.mMatrix[inRow][inColumn]; }
extern "C" void ee_screenscpp_mm_world_coordinates(float inExtent)
{ EEScratch<GEBitmap*>(kScratchWorldBitmap)->SetTextureCoordinates(GETextureCoordinate(), GETextureCoordinate(inExtent, 0.0f), GETextureCoordinate(0.0f, inExtent), GETextureCoordinate(inExtent, inExtent)); }
extern "C" void ee_screenscpp_mm_set_bounds(float inLeft, float inTop, float inRight, float inBottom)
{ EEUIMiniMap::sBounds = GERectangle(inLeft, inTop, inRight, inBottom); }
extern "C" void ee_screenscpp_mm_set_transformation(uintptr_t inMinimap)
{ gMiniMapTransformation = reinterpret_cast<EEMiniMap*>(inMinimap)->mPixelToMap; }
extern "C" void ee_screenscpp_mm_terrain_texture_set(uintptr_t inMinimap)
{ EEScratch<GEBitmap*>(kScratchMapBitmap)->SetTexture(reinterpret_cast<EEMiniMap*>(inMinimap)->mTerrainTexture, gScreenIndex); }
extern "C" void ee_screenscpp_mm_world_texture_set(uintptr_t inMinimap)
{ EEScratch<GEBitmap*>(kScratchWorldBitmap)->SetTexture(reinterpret_cast<EEMiniMap*>(inMinimap)->mWorldTexture, gScreenIndex); }

// the media and the update as the game calls them (Empire Earth.cpp, the world, the mini map: ee/EEGameUI.h)
static const bool sMiniMapRegistered = (gEEGameUI.mMiniMapLoadMedia = EEUIMiniMap::LoadMedia,
                                        gEEGameUI.mMiniMapUnloadMedia = EEUIMiniMap::UnloadMedia,
                                        gEEGameUI.mMiniMapUpdate = EEUIMiniMap::Update, true);
#undef MM

// ==== EEUserInterface.cpp ===========================================================================================
// C:\EEX\Empire Earth\User Interface Thread\EEUserInterface.cpp (__FILE__ 0x4cf450). The UI thread: 0x276631..0x27a37e
// (EEUIPGSLineGraph.cpp ends at 0x276631). The ctor, ??_G, dtor, the thread's handlers (in address order), Run,
// HandleMessage (the 0x401..0x443 switch), the Post* members, the window procedure, then the template COMDATs of
// the message vector. Unit flags: the exe module set. Names are provisional unless the header says otherwise.
// free functions of other units (declared where they are used, as their own units do) (provisional names)
bool EEIsLobbyOpen();                       // 0x297eca (the external lobby is running)
void EESetQuitFlag();
// CivBuilderEditCivilization 0x280faf, CivBuilderCreateCivilization 0x280ffd: ee/EECivilizationBuilder.h; EESetCivBuilderButtonBlinking 0x27fcf0: ee/EmpireEarth.h
void EELobbyShutdown();
extern bool gSightUpdatesEnabled;           // 0x4c76ec (set before the tested scenario starts) (provisional name)

static const float kGameViewLeft = 0.0f, kGameViewRight = 1600.0f;  // the game view below the top bar (.rdata
static const float kGameViewTop = 35.0f, kGameViewBottom = 1085.0f; // 0x44ba2c..0x44ba38: separate words)

// LeaveMap, CancelGame, CloseScenarioEditor, EnterScenarioEditor, StartLoadingFromEditor, ShowCivilizationBuilder and
// RestartGame are Rust's whole (ee_screens ui_game.rs; the thread's messages run them by ee_screens_ui_member).
// The UI thread's game starts' host (EE_SCREENS_UG_OP_*): Rust's (ui_game_calls.rs and the modules before it)
// over the one-statement shims below (UI lane round 11)
#define UG_UI(u) static_cast<EEUserInterface*>((void*)(u))
extern "C" void ee_screenscpp_ug_screen_active(bool inActive) { gWorld.mGameScreenActive = inActive; }
extern "C" void ee_screenscpp_ug_path_reset() { gPathManager.Reset(); }
extern "C" void ee_screenscpp_ug_sight_on() { gSightUpdatesEnabled = true; }
extern "C" void ee_screenscpp_ug_restart_units() { gWorld.RestartUnits(); }
extern "C" void ee_screenscpp_ug_resize_game(uintptr_t inUi, bool inA, bool inB)
{ UG_UI(inUi)->ResizeGameScreen(gGameWindowWidth, gGameWindowHeight, inA, inB); }
extern "C" void ee_screenscpp_ug_resize_menu(uintptr_t inUi, bool inA, bool inB)
{ UG_UI(inUi)->ResizeGameScreen(EEMenuWidth(), EEMenuHeight(), inA, inB); }
extern "C" void ee_screenscpp_ug_physics_post(uint64_t inMessage)
{ gPhysics->mThread->Post((unsigned long)inMessage, NULL, NULL, true); }
extern "C" void ee_screenscpp_ug_music() { gMusicManager->Start(); }
extern "C" void ee_screenscpp_ug_prepare_save() { gWorld.mMap->PrepareSave(); }
extern "C" int64_t ee_screenscpp_ug_save_scenario()
{ return SaveGame(UString("ScenDefault") + gScenarioExtension, TMGetTime(), true, UString("data\\") + UString("Scenarios\\")); }
extern "C" void ee_screenscpp_ug_save_error()
{ gUIManager->DisplayMessage(gLoadSaveErrorMessage, true, NULL, NULL, true); }
extern "C" void ee_screenscpp_ug_max_players() { gWorld.SetNumPlayers(gWorld.mMaxPlayers); }
extern "C" void ee_screenscpp_ug_collect_computers() { gArtificialIntelligence->CollectComputerPlayers(); }
extern "C" bool ee_screenscpp_ug_timer_running() { return gPhysics->mTimerRunning; }
extern "C" void ee_screenscpp_ug_state_clear() { gGameState.Clear(); }
extern "C" void ee_screenscpp_ug_options_defaults() { gWorld.mLobbyOptions.SetDefaults(); }
extern "C" uint64_t ee_screenscpp_ug_file_name_length(uintptr_t inFile)
{ return (uint64_t)reinterpret_cast<FSFile*>(inFile)->GetFile().GetLength(); }
extern "C" uint64_t ee_screenscpp_ug_file_scenario_at(uintptr_t inFile)
{ return (uint64_t)reinterpret_cast<FSFile*>(inFile)->GetFile().Find(gScenarioExtension, 0); }
extern "C" void ee_screenscpp_ug_post_load(bool inA, bool inB) { gDataPreLoading->PostLoadGame(inA, inB); }
extern "C" void ee_screenscpp_ug_delete_file(uintptr_t inFile) { delete reinterpret_cast<FSFile*>(inFile); }
extern "C" void ee_screenscpp_ug_update_interface(uintptr_t inUi) { UG_UI(inUi)->UpdateInterface(); }
extern "C" bool ee_screenscpp_ug_free_camera() { return gFreeCamera; }
extern "C" bool ee_screenscpp_ug_sim_run() { return gPlatSimRun; }
extern "C" void ee_screenscpp_ug_wait_preload() { gDataPreLoading->WaitForMessageQueue(); }
extern "C" void ee_screenscpp_ug_world_clear() { gWorld.Clear(); }
extern "C" bool ee_screenscpp_ug_has_communications() { return gCommunications != NULL; }
extern "C" bool ee_screenscpp_ug_is_client() { return gCommunications->IsClient(); }
extern "C" bool ee_screenscpp_ug_server_lost_after_loading()
{ return static_cast<EEClient*>(gCommunications)->mServerLostAfterLoading; }
extern "C" bool ee_screenscpp_ug_lobby_open() { return EEIsLobbyOpen(); }
extern "C" void ee_screenscpp_ug_create_editor() { gUIManager->CreateScenarioEditor(); }
extern "C" int64_t ee_screenscpp_ug_load_scendefault()
{ return EELoadSavedGameFile(UString("ScenDefault") + gScenarioExtension, true, true, UString("data\\") + UString("Scenarios\\"), 0); }
extern "C" void ee_screenscpp_ug_stop_one_shots() { gSoundManager->StopAllOneShotSounds(); }
extern "C" bool ee_screenscpp_ug_graphics_paused() { return gGraphics->mIsPaused; }
extern "C" bool ee_screenscpp_ug_can_buy_civ() { return gWorld.mGameOptions.CanBuyCivilization(); }
extern "C" bool ee_screenscpp_ug_custom_civs() { return gWorld.mGameOptions.mAllowCustomCivs; }

// The action queues' host (actions_ffi.rs picks): an action's reference, release, run, undo and undoable flag
extern "C" void ee_screenscpp_action_reference(EEAction* inAction) { inAction->Reference(); }
extern "C" void ee_screenscpp_action_release(EEAction* inAction) { inAction->Dereference(); }
extern "C" void ee_screenscpp_action_run(EEAction* inAction) { inAction->Virtual4(); }
extern "C" void ee_screenscpp_action_undo(EEAction* inAction) { inAction->Virtual8(); }
extern "C" bool ee_screenscpp_action_undoable(EEAction* inAction) { return inAction->mIsUndoable; }

// The UI thread's message host (EE_SCREENS_UI_OP_*): Rust's (ui_message_calls.rs and the modules before it) over
// the one-statement shims below (UI lane round 11)
#define UIM(u) static_cast<EEUserInterface*>((void*)(u))
#define UIM_CAP(c) reinterpret_cast<EEScreenCapture*>(c)
#define UIM_CHAT(e) reinterpret_cast<EeScreensChatEntry*>(e)
extern "C" void ee_screenscpp_uim_pause(uintptr_t inUi) { UIM(inUi)->PauseGraphics(); }
extern "C" void ee_screenscpp_uim_resume(uintptr_t inUi) { UIM(inUi)->ResumeGraphics(); }
extern "C" void ee_screenscpp_uim_status_value(uintptr_t inForm, uint64_t inControl, int64_t inValue)
{ static_cast<UIStatusBar*>(((UIForm*)inForm)->GetControl((unsigned long)inControl))->SetCurrentRangeValue(inValue); }
extern "C" void ee_screenscpp_uim_resize_game(uintptr_t inUi)
{ UIM(inUi)->ResizeGameScreen(gGameWindowWidth, gGameWindowHeight, true, true); }
extern "C" void ee_screenscpp_uim_resize_menu(uintptr_t inUi)
{ UIM(inUi)->ResizeGameScreen(EEMenuWidth(), EEMenuHeight(), true, false); }
extern "C" void ee_screenscpp_uim_screen_active() { gWorld.mGameScreenActive = true; }
extern "C" void ee_screenscpp_uim_post_load_game() { gDataPreLoading->PostLoadGame(false, false); }
extern "C" bool ee_screenscpp_uim_has_manager() { return gUIManager != NULL; }
extern "C" uintptr_t ee_screenscpp_uim_main_form() { return (uintptr_t)gUIManager->GetMainGameForm(); }
extern "C" void ee_screenscpp_uim_main_rust(uintptr_t inMain, uint32_t inWhich, int64_t inA)
{ ((EEUIFMainGame*)inMain)->Rust(inWhich, inA); }
extern "C" bool ee_screenscpp_uim_gfx_post_pause() { return gGraphics->PostPause() == kErrorOK; }
extern "C" bool ee_screenscpp_uim_gfx_paused() { return gGraphics->mIsPaused; }
extern "C" void ee_screenscpp_uim_gfx_post_resume() { gGraphics->PostResume(); }
extern "C" bool ee_screenscpp_uim_capture_event(uintptr_t inCapture)
{ return TSScheduler::CreateEvent(UIM_CAP(inCapture)->mCaptureComplete) == keseOK; }
extern "C" void ee_screenscpp_uim_capture_post(uintptr_t inCapture) { gGraphics->PostScreenShot(UIM_CAP(inCapture)); }
extern "C" void ee_screenscpp_uim_capture_wait(uintptr_t inCapture)
{ UIM_CAP(inCapture)->mCaptureComplete->Wait(kTSWaitInfinite); }
extern "C" void ee_screenscpp_uim_capture_event_free(uintptr_t inCapture)
{ TSScheduler::DestroyEvent(UIM_CAP(inCapture)->mCaptureComplete); }
extern "C" int64_t ee_screenscpp_uim_capture_width(uintptr_t inCapture) { return (int64_t)UIM_CAP(inCapture)->mWidth; }
extern "C" void ee_screenscpp_uim_capture_surface_release(uintptr_t inCapture)
{ UIM_CAP(inCapture)->mSurface->Dereference(); }
extern "C" void ee_screenscpp_uim_capture_delete(uintptr_t inCapture) { delete UIM_CAP(inCapture); }
extern "C" void ee_screenscpp_uim_verify(uint32_t inLine)
{ ESVerify(__FILE__, inLine, "TSScheduler::CreateEvent(theCapture->mCaptureComplete) == keseOK"); }
extern "C" void ee_screenscpp_uim_post_gfx_all() { EEPostGraphicsAll(); }
extern "C" bool ee_screenscpp_uim_error_has_callback(uintptr_t inData)
{ return reinterpret_cast<EEUIFErrorCallbackData*>(inData)->mCallback != NULL; }
extern "C" void ee_screenscpp_uim_error_callback(uintptr_t inData)
{ reinterpret_cast<EEUIFErrorCallbackData*>(inData)->mCallback(static_cast<UIForm*>(reinterpret_cast<EEUIFErrorCallbackData*>(inData)->mData)); }
extern "C" void ee_screenscpp_uim_delete_form(uintptr_t inForm) { delete (UIForm*)inForm; }
extern "C" void ee_screenscpp_uim_delete_error_data(uintptr_t inData)
{ delete reinterpret_cast<EEUIFErrorCallbackData*>(inData); }
extern "C" void ee_screenscpp_uim_chat_show(uintptr_t inMain, uintptr_t inEntry)
{ ((EEUIFMainGame*)inMain)->DisplayChat(GEColor(UIM_CHAT(inEntry)->head.colour[0], UIM_CHAT(inEntry)->head.colour[1], UIM_CHAT(inEntry)->head.colour[2]), UWideString(UIM_CHAT(inEntry)->name), UWideString(UIM_CHAT(inEntry)->text), UIM_CHAT(inEntry)->head.time, UIM_CHAT(inEntry)->head.kind); }
extern "C" void ee_screenscpp_uim_show_information(uintptr_t inMain, uintptr_t inInfo)
{ ((EEUIFMainGame*)inMain)->ShowInformation(reinterpret_cast<EEUIFMGInformation*>(inInfo)); }
extern "C" void ee_screenscpp_uim_delete_dialog(uintptr_t inDialog)
{ delete reinterpret_cast<EESMDialogMessage*>(inDialog); }
extern "C" void ee_screenscpp_uim_icon_bar_posted_clear(uintptr_t inUi) { UIM(inUi)->mIconBarUpdatePosted = false; }
extern "C" void ee_screenscpp_uim_change_rasterizer(uintptr_t inUi, int64_t inA, int64_t inB)
{ UIM(inUi)->ChangeRasterizer(inA, inB); }
extern "C" void ee_screenscpp_uim_post_event(uintptr_t inEvent)
{ gUserInterface->PostEvent(reinterpret_cast<UIEvent*>(inEvent), NULL); }
extern "C" void ee_screenscpp_uim_select_tab(uint8_t inTab, int64_t inB)
{ gUIManager->GetScenarioEditorForm()->SelectTab(inTab, inB); }
extern "C" void ee_screenscpp_uim_saved_group(uintptr_t inMain, uint64_t inControl)
{ EEUIShell::Op(((EEUIFMainGame*)inMain)->GetControl((unsigned long)inControl), EE_SCREENS_SKOP_SG_REFRESH); }
extern "C" void ee_screenscpp_uim_select_server(uintptr_t inUi) { UIM(inUi)->OpenSelectServer(); }
extern "C" void ee_screenscpp_uim_motd(uintptr_t inUi, uintptr_t inA, uintptr_t inB)
{ UIM(inUi)->ShowMessageOfTheDay(*reinterpret_cast<UWideString*>(inA), *reinterpret_cast<UWideString*>(inB)); }
extern "C" void ee_screenscpp_uim_delete_wstring(uintptr_t inText) { delete reinterpret_cast<UWideString*>(inText); }

// (the posts the game makes, ResumeGraphics and HandleEvent are inline: ee/EEUserInterface.h)

// The window events' one-call ops (EE_SCREENS_UI_EV_*; ui_calls.rs ui_event): the globals' NULL tests, the lobby, the
// volumes, the input devices, the mouse pointer's thread, the input method
extern "C" bool ee_screenscpp_ui_has_keyboard() { return gISKeyboard != NULL; }
extern "C" bool ee_screenscpp_ui_has_mouse() { return gISMouse != NULL; }
extern "C" bool ee_screenscpp_ui_has_window_and_mouse() { return gGameWindow != NULL && gMouse != NULL; }
extern "C" bool ee_screenscpp_ui_has_pointer() { return gMousePointer != NULL; }
extern "C" bool ee_screenscpp_ui_has_window() { return gGameWindow != NULL; }
extern "C" bool ee_screenscpp_ui_lobby_open() { return EEIsLobbyOpen(); }
extern "C" void ee_screenscpp_ui_lobby_shutdown() { EELobbyShutdown(); }
extern "C" void ee_screenscpp_ui_set_music_volume(float inVolume) { gConductor->GetConductor().SetVolume(inVolume); }
extern "C" void ee_screenscpp_ui_set_master_volume(float inVolume) { gSoundManager->SetMasterVolume(inVolume); }
extern "C" void ee_screenscpp_ui_mute(bool inMuted) { SPIMSetMuted(inMuted); }   // port: "Mute in Background"
extern "C" void ee_screenscpp_ui_center_mouse()
{ gMouse->SetPosition(GE2DPoint((float)(gGameWindow->GetWidth() >> 1), (float)(gGameWindow->GetHeight() >> 1))); }
extern "C" void ee_screenscpp_ui_pointer_post(int64_t inMessage) { gMousePointer->mThread->Post(inMessage, NULL, NULL, true); }
extern "C" bool ee_screenscpp_ui_pointer_shown() { return gMousePointer->mIsShown; }
extern "C" bool ee_screenscpp_ui_mute_in_background() { return gGamePreferences.mSettings.mMuteInBackground; }   // port
extern "C" bool ee_screenscpp_ui_full_screen() { return gGamePreferences.mSettings.GetFullScreen(); }
extern "C" void ee_screenscpp_ui_minimize() { PlatWindowMinimize(gGameWindow->GetOSRepresentation()); }   // CloseWindow
// the committed composition
extern "C" void ee_screenscpp_ui_ime_result(uintptr_t inText) { gUIManager->AddIMEResultString(reinterpret_cast<const char*>(inText)); }
extern "C" void ee_screenscpp_ui_translated_key(uintptr_t inText) { gUIManager->UpdateTranslatedKey(reinterpret_cast<const char*>(inText), 1); }

// ---- the shims of the moved code (UI lane round 7: ee_game screens ui_calls.rs, game_cpp.rs), each one C++
// statement
// (C: the main game form section's)
// shims4
extern "C" void ee_screenscpp_gfx_post_pause() { gGraphics->PostPause(); }
extern "C" bool ee_screenscpp_gfx_paused() { return gGraphics->mIsPaused; }
extern "C" void ee_screenscpp_sleep(uint32_t inMilliseconds) { PlatSleep(inMilliseconds); }
extern "C" bool ee_screenscpp_pointer_shown() { return gMousePointer->mIsShown; }
extern "C" uint32_t ee_screenscpp_game_window_width() { return (uint32_t)gGameWindow->GetWidth(); }
extern "C" uint32_t ee_screenscpp_game_window_height() { return (uint32_t)gGameWindow->GetHeight(); }
extern "C" void ee_screenscpp_pointer_post(uint32_t inMessage)
{ gMousePointer->mThread->Post(inMessage, NULL, NULL, true); }
extern "C" void ee_screenscpp_pointer_post_surface(uint32_t inMessage)
{ gMousePointer->mThread->Post(inMessage, gRasterizer->GetPrimarySurface(gScreenIndex), NULL, true); }
extern "C" void ee_screenscpp_manager_relayout_all() { gUIManager->RelayoutAllForms(); }
extern "C" void ee_screenscpp_world_write_lock() { gWorld.mLock->WriteLock(); }
extern "C" void ee_screenscpp_world_write_unlock() { gWorld.mLock->WriteUnlock(); }
extern "C" void ee_screenscpp_world_read_lock() { gWorld.mLock->ReadLock(); }
extern "C" void ee_screenscpp_world_read_unlock() { gWorld.mLock->ReadUnlock(); }
extern "C" void ee_screenscpp_game_window_client_origin() { gGameWindow->GetClientOrigin(); }
extern "C" uint32_t ee_screenscpp_rasterizer_screen_count() { return (uint32_t)gRasterizer->GetScreenCount(); }
extern "C" void ee_screenscpp_rasterizer_release_screen(uint32_t inIndex)
{ gRasterizer->GetScreen(inIndex)->Release(gGameWindow); }
extern "C" int32_t ee_screenscpp_rasterizer_deactivate() { return (int32_t)gRasterizer->Deactivate(); }
extern "C" void ee_screenscpp_verify_rasterizer_deactivated(int32_t inResult) { ESVERIFY_AT(inResult == keseOK, 252); }
extern "C" void ee_screenscpp_rasterizer_select(int32_t inIndex) { gRasterizer = GERasterizer::GetRasterizer(inIndex); }
extern "C" void ee_screenscpp_gfx_post_set_rasterizer(int32_t inScreen)
{ gGraphics->PostSetRasterizer(gRasterizer, inScreen); }
extern "C" void ee_screenscpp_manager_change_rasterizer() { gUIManager->ChangeRasterizer(gRasterizer); }
extern "C" uint32_t ee_screenscpp_menu_width() { return (uint32_t)EEMenuWidth(); }
extern "C" uint32_t ee_screenscpp_menu_height() { return (uint32_t)EEMenuHeight(); }
extern "C" void ee_screenscpp_manager_lock_for_copy() { gUIManager->LockUIForCopyData(); }
extern "C" void ee_screenscpp_manager_unlock_for_copy() { gUIManager->UnlockUIForCopyData(); }
extern "C" bool ee_screenscpp_manager_form_visible(uint32_t inForm)
{ return gUIManager->IsFormVisible(gUIManager->GetFormByID(inForm)) == true; }
extern "C" void ee_screenscpp_control_set_display_mode(uintptr_t inControl, uint32_t inMode)
{ C(inControl)->SetDisplayMode(inMode); }
extern "C" void ee_screenscpp_label_append_wide(uintptr_t inLabel, uintptr_t inText)
{ static_cast<UITextLabel*>(C(inLabel))->AppendText(*reinterpret_cast<const UWideString*>(inText)); }
extern "C" uintptr_t ee_screenscpp_select_server_event_new()
{ return (uintptr_t)static_cast<UIEvent*>(new EEUIEWonNetEvent(EeWonFrontCall::SsOpenScreen)); }
extern "C" void ee_screenscpp_manager_form_set_open_event(uint32_t inForm, uintptr_t inEvent)
{ gUIManager->GetFormByID(inForm)->SetFormOpenEvent(reinterpret_cast<UIEvent*>(inEvent)); }
extern "C" void ee_screenscpp_manager_form_set_close_event(uint32_t inForm, uintptr_t inEvent)
{ gUIManager->GetFormByID(inForm)->SetFormCloseEvent(reinterpret_cast<UIEvent*>(inEvent)); }
extern "C" bool ee_screenscpp_check_box_checked(uintptr_t inCheck)
{ return static_cast<UICheckBox*>(C(inCheck))->GetIsChecked() == true; }
extern "C" void ee_screenscpp_won_password_from_edit(uintptr_t inEdit)
{ gGamePreferences.mSettings.mWonNetPassword = UString(static_cast<UIEditBox*>(C(inEdit))->GetText(0)); }
extern "C" void ee_screenscpp_won_password_clear() { gGamePreferences.mSettings.mWonNetPassword = UString(""); }
extern "C" void ee_screenscpp_multiplayer_string_from_edit(uintptr_t inEdit)
{ gMultiplayerString = UString(static_cast<UIEditBox*>(C(inEdit))->GetText(0)); }
extern "C" void ee_screenscpp_manager_pop_to(uint32_t inForm) { gUIManager->PopTo(gUIManager->GetFormByID(inForm)); }
extern "C" void ee_screenscpp_manager_push(uint32_t inForm) { gUIManager->Push(gUIManager->GetFormByID(inForm)); }
extern "C" uintptr_t ee_screenscpp_manager_form(uint32_t inForm) { return (uintptr_t)gUIManager->GetFormByID(inForm); }
extern "C" void ee_screenscpp_keyboard_refresh() { gISKeyboard->Refresh(); }
extern "C" void ee_screenscpp_mouse_input_lock_enter() { gMouse->mInputLock->Enter(); }
extern "C" void ee_screenscpp_mouse_refresh() { gISMouse->Refresh(); }
extern "C" void ee_screenscpp_mouse_update() { gMouse->Update(); }
extern "C" void ee_screenscpp_mouse_input_lock_leave() { gMouse->mInputLock->Leave(); }
extern "C" bool ee_screenscpp_lobby_shown() { return EELobbyIsShown(); }
extern "C" void ee_screenscpp_manager_update_ui() { gUIManager->UpdateUI(gMouse->mPosition); }
extern "C" void ee_screenscpp_actions_process() { ee_screens_actions_process(); }
extern "C" void ee_screenscpp_ui_resume_graphics(uintptr_t inUi)
{ reinterpret_cast<EEUserInterface*>(inUi)->ResumeGraphics(); }
extern "C" void ee_screenscpp_continents_clear() { gContinentManager.Clear(gWorld.mMap); }
extern "C" void ee_screenscpp_continents_build_regions() { gContinentManager.BuildRegions(gWorld.mMap); }
extern "C" void ee_screenscpp_player_register_herds(uint32_t inPlayer) { gWorld.mPlayers[inPlayer]->RegisterHerds(); }
extern "C" void ee_screenscpp_player_assign_herds(uint32_t inPlayer) { gWorld.mPlayers[inPlayer]->AssignHerds(); }
extern "C" void ee_screenscpp_player_register_towns(uint32_t inPlayer) { gWorld.mPlayers[inPlayer]->RegisterTowns(); }
extern "C" void ee_screenscpp_player_assign_towns(uint32_t inPlayer) { gWorld.mPlayers[inPlayer]->AssignTowns(); }
extern "C" void ee_screenscpp_player_refresh_resource_centres(uint32_t inPlayer)
{ gWorld.mPlayers[inPlayer]->mComputerPlayer->RefreshResourceCenters(); }
extern "C" void ee_screenscpp_player_refresh_town_groups(uint32_t inPlayer)
{ gWorld.mPlayers[inPlayer]->RefreshTownGroups(); }
extern "C" void ee_screenscpp_ambient_enable_from_settings()
{ gAmbientSoundManager.SetEnabled(gGamePreferences.mSettings.mAmbientSounds); }
extern "C" void ee_screenscpp_ambient_restart() { gAmbientSoundManager.Restart(); }
extern "C" void ee_screenscpp_world_options_set_load_type(int32_t inLoadType)
{ gWorld.mGameOptions.mLoadType = inLoadType; }
extern "C" void ee_screenscpp_world_options_set_map_type(EeScreensShimText inName)
{ gWorld.mGameOptions.mMapType = EEShimText(inName); }
extern "C" int32_t ee_screenscpp_header_load_file(uintptr_t inHeader, uintptr_t inFile)
{ return (int32_t)reinterpret_cast<EEPSaveGameHeader*>(inHeader)->LoadFromFile(*reinterpret_cast<FSFile*>(inFile)); }
extern "C" void ee_screenscpp_server_set_humans(uint32_t inHumans)
{ static_cast<EEServer*>(gCommunications)->SetNumHumanPlayers(inHumans); }
extern "C" void ee_screenscpp_server_set_multiplayer(bool inMultiplayer)
{ static_cast<EEServer*>(gCommunications)->SetIsMultiplayerSession(inMultiplayer); }
extern "C" void ee_screenscpp_options_map_type_from_file(uintptr_t inFile)
{ gWorld.mLobbyOptions.mMapType = UWideString(reinterpret_cast<FSFile*>(inFile)->GetFile()); }
extern "C" void ee_screenscpp_mouse_resume() { gISMouse->Resume(); }
extern "C" void ee_screenscpp_keyboard_resume() { gISKeyboard->Resume(); }
extern "C" void ee_screenscpp_mouse_suspend() { gISMouse->Suspend(); }
extern "C" void ee_screenscpp_keyboard_suspend() { gISKeyboard->Suspend(); }
extern "C" void ee_screenscpp_set_locale_all(EeScreensShimText inLocale) { setlocale(LC_ALL, EEShimString(inLocale)); }
extern "C" void ee_screenscpp_set_locale_numeric_english() { setlocale(LC_NUMERIC, "English"); }
extern "C" void ee_screenscpp_set_locale_ctype_c() { setlocale(LC_CTYPE, "C"); }
extern "C" void ee_screenscpp_query_locale() { setlocale(LC_ALL, NULL); }
extern "C" void ee_screenscpp_ui_render_object_new() { gU56534C = new EEROUserInterface(gUIManager); }
extern "C" void ee_screenscpp_gfx_push_ui_render_object() { gGraphics->PostPushRenderObject(gU56534C); }
extern "C" void ee_screenscpp_gfx_set_flag54(bool inFlag) { gGraphics->PostSetFlag54(inFlag); }
extern "C" void ee_screenscpp_gfx_post_resume() { gGraphics->PostResume(); }
extern "C" void ee_screenscpp_fpu_set_precision_53() { UMSetFloatingPointPrecision(kFPPrecision53); }
extern "C" bool ee_screenscpp_fpu_precision_is_53() { return UMGetFloatingPointPrecision() == kFPPrecision53; }
extern "C" uint32_t ee_screenscpp_fpu_rounding_mode() { return (uint32_t)UMGetFloatingPointRoundingMode(); }
extern "C" void ee_screenscpp_fpu_set_rounding_mode(uint32_t inMode) { UMSetFloatingPointRoundingMode(inMode); }
extern "C" void ee_screenscpp_gfx_release_scene() { gGraphics->PostReleaseScene(); }
extern "C" void ee_screenscpp_gfx_wait_50a() { gGraphics->PostMessage50AAndWait(); }
extern "C" void ee_screenscpp_physics_post_init() { gPhysics->PostInit(); }
extern "C" void ee_screenscpp_physics_run_owed_ticks() { gPhysics->RunOwedTicks(); }
extern "C" void ee_screenscpp_physics_init() { gPhysics->Init(); }
extern "C" uintptr_t ee_screenscpp_communications() { return (uintptr_t)gCommunications; }
extern "C" void ee_screenscpp_new_client() { gCommunications = new EEClient; }
extern "C" void ee_screenscpp_client_start_threads() { static_cast<EEClient*>(gCommunications)->StartThreads(); }
extern "C" uintptr_t ee_screenscpp_boot_client_new(int32_t inId)
{ return (uintptr_t)static_cast<EEAction*>(new EEABootClientFromCanceledGame((long)inId, true)); }
extern "C" void ee_screenscpp_music_pause() { gMusicManager->Pause(); }
extern "C" void ee_screenscpp_ambient_stop_sounds() { gAmbientSoundManager.StopSounds(); }
extern "C" void ee_screenscpp_ambient_pause() { gAmbientSoundManager.Pause(); }
extern "C" void ee_screenscpp_chat_log_clear() { ee_screens_chat_log_clear(); }
extern "C" void ee_screenscpp_civ_builder_button_blinking(bool inBlinking)
{ EESetCivBuilderButtonBlinking(inBlinking); }
extern "C" void ee_screenscpp_player_civilization_chosen()
{ gWorld.mPlayers[gWorld.mCurrentPlayer]->mCivilizationChosen = true; }
extern "C" void ee_screenscpp_player_civilization_locked()
{ gWorld.mPlayers[gWorld.mCurrentPlayer]->mCivilizationLocked = true; }
extern "C" void ee_screenscpp_event_process(uintptr_t inEvent, uintptr_t inEntity)
{ reinterpret_cast<UIEvent*>(inEvent)->ProcessEvent(NULL, reinterpret_cast<UIEntity*>(inEntity)); }
extern "C" void ee_screenscpp_quick_save_set_load(uintptr_t inAction)
{ reinterpret_cast<EEAQuickSaveLoad*>(inAction)->mIsLoad = true; }
extern "C" void ee_screenscpp_chat_entry_text_from_dialog(uintptr_t inEntry, uintptr_t inDialog)
{ ee_screens_chat_entry_set_text(inEntry, (const char*)reinterpret_cast<EESMDialogMessage*>(inDialog)->mText); }
extern "C" void ee_screenscpp_set_quit_flag() { EESetQuitFlag(); }
extern "C" uintptr_t ee_screenscpp_boot_client_host_gone_new()
{ return (uintptr_t)static_cast<EEAction*>(new EEABootClientFromCanceledGame(IDS_HOST_OF_THIS_GAME_HAS_GONE)); }
extern "C" void ee_screenscpp_client_disconnect() { static_cast<NESynchronousClient*>(gCommunications)->Disconnect(); }
extern "C" uintptr_t ee_screenscpp_conductor() { return (uintptr_t)gConductor; }
extern "C" float ee_screenscpp_conductor_volume() { return gConductor->GetConductor().GetVolume(); }
extern "C" uintptr_t ee_screenscpp_sound_manager() { return (uintptr_t)gSoundManager; }
extern "C" float ee_screenscpp_master_volume() { return gSoundManager->GetMasterVolume(); }
extern "C" void ee_screenscpp_ime_composition(EeScreensShimText inText, EeScreensShimText inAttributes, uint32_t inCursor)
{ gUIManager->ChangeIMECompositionString(EEShimText(inText), EEShimString(inAttributes), (unsigned long)inCursor); }
// end shims4

// (the window's event pump and native message hook: Empire Earth.cpp, which installs them)

// the number of humans in the session (SetLoadProgress's test: ee_screens ui_objects_ffi.rs)
extern "C" int64_t ee_screenscpp_ui_humans()
{
    return gCommunications != NULL ? gCommunications->GetNumHumanPlayers() : 0;
}

// the posts' references and the graphics thread's resume (ee_screens ui_objects_ffi.rs)
extern "C" void ee_screenscpp_ui_event_reference(void* inEvent) { static_cast<UIEvent*>(inEvent)->Reference(); }
extern "C" void ee_screenscpp_ui_object_reference(void* inObject) { static_cast<UReferenceCounter*>(inObject)->Reference(); }
extern "C" void ee_screenscpp_ui_graphics_post_resume() { gGraphics->PostResume(); }
extern "C" bool ee_screenscpp_ui_graphics_paused() { return gGraphics->mIsPaused; }
#undef UG_UI
#undef UIM
#undef UIM_CAP
#undef UIM_CHAT

// ==== EEWonNetAvailableGameFormManager.cpp ==========================================================================
// C:\EEX\Empire Earth\User Interface Thread\EEWonNetAvailableGameFormManager.cpp (file name inferred: no __FILE__;
// RTTI class EEWonNetAvailableGameFormManager). 0x27a37e..0x27aa0b: the WON game room form's manager (ctor, ??_G,
// dtor, Get), then the game's free UI helpers in address order (EETranslateString, EEDisplayScenarioIntro, message
// 0x41b, the resource reference parser; the flare effect, the selection and tile lookups and EEExitGame, which the
// rest of the game calls, are Empire Earth/EEGameUtilities.cpp's). The helpers' names are provisional. Unit flags: the exe module set.
// Creates the WON available-games form manager with a 3-second refresh poll
EEWonNetAvailableGameFormManager::EEWonNetAvailableGameFormManager()
{
    this->mPoll = new TMPoll(3000, 0, false);
}

// Destroys the form manager and its poll
EEWonNetAvailableGameFormManager::~EEWonNetAvailableGameFormManager()
{
    delete this->mPoll;
}

// Returns the available-games form's manager, creating and attaching it on first use (Rust: wonfront.rs)
EEWonNetAvailableGameFormManager* EEWonNetAvailableGameFormManager::Get() { return (EEWonNetAvailableGameFormManager*)ee_won_front_call(EeWonFrontCall::FormManagerGet, NULL, 0); }

// the scenario intro's media from the running scenario (ui_helpers.rs display_scenario_intro)
extern "C" void ee_screenscpp_uh_intro_media_copy() { EEUISCIFormManager::Get()->mMediaStrings = ::gWorld.mScenarioManager->mMediaStrings; }

// (EETranslateString, EEDisplayScenarioIntro, EEPostF9Key and the resource reference parser: Rust, ee_game screens
// ui_helpers.rs)

// (EEExitGame: Empire Earth/EEGameUtilities.cpp)

// ==== EEUIRustControl.cpp ===========================================================================================
// New (port): the host of the game's controls whose logic is Rust (ee/EEUIRustControl.h; ee_screens hud_ctl.rs):
// any control's ops here, the class's own in its RustOp.
extern "C" int64_t ee_screenscpp_hud_ctl(void* inControl, uint32_t inOp, int64_t inA, int64_t inB, float* ioF)
{
    return EEUIRustControl::HostOp(static_cast<EEUIRustControl*>(inControl), inOp, inA, inB, ioF);
}

// a rectangle from the op's floats (left, top, right, bottom)
static GERectangle EEHudRect(const float* inF) { return GERectangle(inF[0], inF[1], inF[2], inF[3]); }

int64_t EEUIRustControl::HostOp(EEUIRustControl* inControl, uint32_t inOp, int64_t inA, int64_t inB, float* ioF)
{
    switch (inOp) {
    case EE_SCREENS_HC_SET_RECTANGLE: inControl->SetRectangle(EEHudRect(ioF)); break;
    case EE_SCREENS_HC_SET_BG_RECTANGLE: inControl->SetBgRectangle(EEHudRect(ioF)); break;
    case EE_SCREENS_HC_DRAW_BASE: return inControl->UIControl::DrawPolygons();
    case EE_SCREENS_HC_DOUBLE_BASE: return inControl->UIControl::MouseLButtonDoublePressed();
    case EE_SCREENS_HC_UPDATE_MOUSE: return inControl->UpdateMouse(GE2DPoint(ioF[0], ioF[1]));
    case EE_SCREENS_HC_MOUSE_BOUNDS: gMouse->SetBounds(EEHudRect(ioF)); break;
    case EE_SCREENS_HC_MOUSE_BOUNDS_WINDOW: gMouse->SetBounds(GERectangle(0.0f, 0.0f, (float)gGameWindow->GetWidth(), (float)gGameWindow->GetHeight())); break;
    case EE_SCREENS_HC_VIEW_HAS_MOUSE: return gViewport.GetViewRectangle().Contains(inControl->GetMouseLocation());
    case EE_SCREENS_HC_WINDOW_WIDTH: return gGameWindow->GetWidth();
    default: return inControl->RustOp(inOp, inA, inB, ioF);
    }
    return 0;
}

// ==== EEUIShell.cpp =================================================================================================
// New (port; UI cluster round 3): the generic C++ shells' base calls and their host (ee/EEUIShell.h; ee_game screens
// shells.rs): the game's C++ each kind's Rust calls, one case per step of the deleted classes' code.
// The icon bar kind's own ops (the old RustOp: a unit's class flags and goals, the selection message, the sounds,
// the keys, the mouse, the icons and the frame) are icon_bar_ops.rs' over the one-statement shims below (UI lane
// round 11)
#define IB(p) static_cast<EEUIShellControl*>((EEUIRustControl*)(p))
#define IB_CU(u) ((EEComplexUnit*)(u))
extern "C" bool ee_screenscpp_ib_class_transport(uintptr_t inUnit)
{ return IB_CU(inUnit)->GetUnitClass()->IsTransport(); }
extern "C" bool ee_screenscpp_ib_class_air_base(uintptr_t inUnit) { return IB_CU(inUnit)->GetUnitClass()->IsAirBase(); }
extern "C" bool ee_screenscpp_ib_class_paratrooper_bomber(uintptr_t inUnit)
{ return IB_CU(inUnit)->GetUnitClass()->mIsParatrooperBomber; }
extern "C" int64_t ee_screenscpp_ib_class_family(uintptr_t inUnit)
{ return (int64_t)IB_CU(inUnit)->GetUnitClass()->mFamily; }
extern "C" bool ee_screenscpp_ib_queued_goal(uintptr_t inUnit, uint64_t inGoal)
{ return IB_CU(inUnit)->GetQueuedGoal((unsigned long)inGoal) != NULL; }
extern "C" bool ee_screenscpp_ib_goals_empty(uintptr_t inUnit) { return IB_CU(inUnit)->GetQueueGoals(1).size() == 0; }
extern "C" bool ee_screenscpp_ib_goal_front_none(uintptr_t inUnit)
{ return IB_CU(inUnit)->mGoalQueues[1].GetFront() == NULL; }
extern "C" uintptr_t ee_screenscpp_ib_select_these(uintptr_t inIds, uint64_t inCount)
{ return (uintptr_t) new EEMRecord(kMessageSelectTheseUnits, {std::vector<long>((const int32_t*)inIds, (const int32_t*)inIds + inCount)}); }
extern "C" void ee_screenscpp_ib_send(uintptr_t inMessage)
{ gCommunications->ProcessLocalUserInput((NEMessage*)inMessage, kSlotNone); }
extern "C" void ee_screenscpp_ib_sound(uintptr_t inP, uint32_t inSlot)
{ gSoundManager->Play((unsigned long)(uintptr_t)IB(inP)->mSlots[inSlot], 1.0f, 0.0f, 1.0f, false, NULL); }
extern "C" bool ee_screenscpp_ib_ctrl() { return UIControl::ControlKeyPressed(); }
extern "C" bool ee_screenscpp_ib_shift() { return UIControl::ShiftKeyPressed(); }
extern "C" float ee_screenscpp_ib_mouse_x(uintptr_t inP) { return IB(inP)->GetMouseLocation().GetX(); }
extern "C" bool ee_screenscpp_ib_contains(uintptr_t inP)
{ return IB(inP)->GetRectangle().Contains(IB(inP)->GetMouseLocation()); }
extern "C" void ee_screenscpp_ib_button_acquire(uintptr_t inButton) { ((EEButtonObject*)inButton)->Acquire(); }
extern "C" void ee_screenscpp_ib_button_release(uintptr_t inButton) { ((EEButtonObject*)inButton)->Release(); }
extern "C" void ee_screenscpp_ib_icon_texture(uintptr_t inP, uintptr_t inButton)
{ static_cast<GEBitmap*>(IB(inP)->mSlots[0])->SetTexture(((EEButtonObject*)inButton)->GetTexture(), IB(inP)->GetScreenIndex()); }
extern "C" void ee_screenscpp_ib_icon_face(uintptr_t inP, uintptr_t inButton, float inLeft, float inTop, float inRight, float inBottom)
{ ((EEButtonObject*)inButton)->SetFaceBitmap(static_cast<GEBitmap*>(IB(inP)->mSlots[0]), GERectangle(inLeft, inTop, inRight, inBottom), gScreenIndex); }
extern "C" void ee_screenscpp_ib_slot_location(uintptr_t inP, uint32_t inSlot, float inX, float inY)
{ static_cast<GEBitmap*>(IB(inP)->mSlots[inSlot])->SetLocation(GE2DPoint(inX, inY)); }
extern "C" int64_t ee_screenscpp_ib_slot_draw(uintptr_t inP, uint32_t inSlot)
{ return IB(inP)->GetRasterizer()->Draw(static_cast<GEBitmap*>(IB(inP)->mSlots[inSlot]), IB(inP)->GetScreenIndex()); }

// A new file combo box (EEUIFileComboBox: the editor's map, sound and elevation lists): the path referenced, the
// list filled by the kind (shells_files.rs)
UIComboBox* EEUIShellNewFileCombo(FSPath* inPath, const UString& inPattern, unsigned long inId, GEFont* inFont,
                                  const GE2DPoint& inLocation, unsigned long inWidth, UIForm* inForm, unsigned long inIndex)
{
    inPath->Reference();
    return new EEUIShellCombo(EE_SCREENS_SK_FILE_COMBO, {(int64_t)inPath, (int64_t)(const char*)inPattern}, inId, 1, {},
                              inFont, GERectangle(inLocation, (float)inWidth, 20.0f), 400, inForm, inIndex, 0, 2);
}

static EEUIFScenarioEditor* EEShellTriggers() { return gUIManager->GetScenarioEditorForm(); }

// a shell's base class virtual (a form shell's: its host object is the UIForm*) and a form shell's own virtual
extern "C" int64_t ee_screenscpp_shell_base(void* inControl, uint32_t inVirtual, int64_t a, int64_t b, float* f)
{ return EEUIShell::Of(static_cast<UIControl*>(inControl))->BaseCall(inVirtual, a, b, f); }
extern "C" int64_t ee_screenscpp_shell_form_base(void* inForm, uint32_t inVirtual, int64_t a, int64_t b, float* f)
{ return static_cast<EEUIShellForm*>(static_cast<UIForm*>(inForm))->BaseCall(inVirtual, a, b, f); }
extern "C" int64_t ee_screenscpp_shell_form_self(void* inForm, uint32_t inVirtual)
{ return static_cast<EEUIShellForm*>(static_cast<UIForm*>(inForm))->SelfCall(inVirtual); }

extern "C" int64_t ee_screenscpp_shell(void* inControl, uint32_t inOp, int64_t a, int64_t b, float* f)
{
    UIControl* c = static_cast<UIControl*>(inControl);
    switch (inOp) {
    case EE_SCREENS_SH_BG_OPACITY: c->SetBgOpacity(f[0]); break;
    case EE_SCREENS_SH_BG_COLOR: c->SetBgColor(GEColor(f[0], f[1], f[2])); break;
    case EE_SCREENS_SH_BG_PLAYER_COLOR: c->SetBgColor(EEGetPlayerColor((unsigned long)a)); break;
    case EE_SCREENS_SH_SELECTED_COLOR: static_cast<UILBString*>(c)->GetSelectedBackground()->SetColor(GEColor(f[0], f[1], f[2])); break;
    case EE_SCREENS_SH_SELECTED_OPACITY: static_cast<UILBString*>(c)->GetSelectedBackground()->SetOpacity(f[0]); break;
    case EE_SCREENS_SH_SELECTION_INDEX: return (int64_t)(uint32_t)static_cast<EEUIShellCombo*>(c)->Selection();
    case EE_SCREENS_SH_SET_SELECTION: static_cast<UIComboBox*>(c)->SetCurrentSelection((unsigned long)a); break;
    case EE_SCREENS_SH_FILE_PATH_RELEASE: reinterpret_cast<FSPath*>(a)->Dereference(); break;
    case EE_SCREENS_SH_DELETE_ALL: static_cast<UIComboBox*>(c)->DeleteAllEntries(); break;
    case EE_SCREENS_SH_SCROLL_CAPTURED: return static_cast<UILBString*>(c)->GetScrollBarCaptured();
    case EE_SCREENS_SH_RELEASE_MOUSE: c->ReleaseCapturedMouse(); break;
    case EE_SCREENS_SH_FIRE: c->FireTriggerEvent((unsigned long)a); break;
    case EE_SCREENS_SH_SLIDER_VALUE: return (int64_t)(uint32_t)static_cast<UISlider*>(c)->GetCurrentRangeValue();
    case EE_SCREENS_SH_SLIDER_MAX: return static_cast<UISlider*>(c)->GetMaxRange();
    case EE_SCREENS_SH_LABEL_NUMBER: static_cast<UITextLabel*>(c->GetForm()->GetControlById((unsigned long)a))->SetText((long)b); break;
    case EE_SCREENS_SH_BUTTON_TEXT_BYTES:
        static_cast<UIButton*>(c)->SetText(UWideString(reinterpret_cast<const char*>(a)), reinterpret_cast<GEFont*>(b));
        break;
    case EE_SCREENS_SH_LIST_COUNT: return static_cast<UIListBox<UWideString>*>(c->GetForm()->GetControlById((unsigned long)a))->GetLineCount();
    case EE_SCREENS_SH_PLAYER_PAGE: static_cast<EEUIFScenarioEditor*>(c->GetForm())->PlayerCall(EE_SCREENS_SEPL_SELECT_PAGE, (uint32_t)a); break;
    case EE_SCREENS_SH_FILL_TECH_LISTS: gUIManager->GetScenarioEditorForm()->PlayerCall(EE_SCREENS_SEPL_FILL_TECH_LISTS); break;
    case EE_SCREENS_SH_SET_PLAYER_COUNT: static_cast<EEUIFScenarioEditor*>(c->GetForm())->PlayerCall(EE_SCREENS_SEPL_SET_PLAYER_COUNT, (uint32_t)a); break;
    case EE_SCREENS_SH_BITMAP_SCALE: reinterpret_cast<GEBitmap*>(a)->SetXYScale(f[0], f[1]); break;
    case EE_SCREENS_SH_BITMAP_RELEASE: reinterpret_cast<GEBitmap*>(a)->Dereference(); break;
    case EE_SCREENS_SH_SET_NORMAL: static_cast<UIButton*>(c)->SetNormalBitmap(reinterpret_cast<GEBitmap*>(a)); break;
    case EE_SCREENS_SH_SET_PRESSED: static_cast<UIButton*>(c)->SetPressedBitmap(reinterpret_cast<GEBitmap*>(a)); break;
    case EE_SCREENS_SH_BITMAP_OFFSET: static_cast<UIButton*>(c)->SetBitmapDrawOffset(GE2DPoint(f[0], f[1])); break;
    // the tech lists' (EESEUITechListBox)
    case EE_SCREENS_SH_LIST_VIEW: return static_cast<EEUIShellList*>(c)->View();
    case EE_SCREENS_SH_LIST_DELETE_ALL: static_cast<UILBString*>(c)->DeleteAllEntries(); break;
    case EE_SCREENS_SH_LIST_APPEND_BYTES: static_cast<UILBString*>(c)->AppendLine(UWideString(reinterpret_cast<const char*>(a))); break;
    case EE_SCREENS_SH_LIST_LINE_COUNT: return static_cast<UILBString*>(c)->GetLineCount();
    case EE_SCREENS_SH_LIST_START_LINE: static_cast<UILBString*>(c)->SetStartingDisplayLine((unsigned long)a); break;
    case EE_SCREENS_SH_LIST_SELECT_SCROLL: static_cast<UILBString*>(c)->ChangeSelectionIndex((unsigned long)a, false, true); break;
    case EE_SCREENS_SH_FORM_CONTROL: return (int64_t)c->GetForm()->GetControlById((unsigned long)a);
    case EE_SCREENS_SH_TECH_EPOCH: return EETechTreeEpochOf((long)a);
    case EE_SCREENS_SH_TECH_DATA_ID: return gTechTree.Get((long)a)->mData->mID;
    // the unit tab's (EEUIFSELBUnitList, EEUIFSELBCargoList, EEUIFSEBRotation)
    case EE_SCREENS_SH_UNIT_SELECT_TYPE: static_cast<EEUIFScenarioEditor*>(c->GetForm())->UnitCall(EE_SCREENS_SEUN_SELECT); break;
    case EE_SCREENS_SH_ENTER_KEY: return EEUIShellList::EnterKey();
    case EE_SCREENS_SH_DELETE_UNIT: (new EESEARust(EE_SCREENS_SEW_DELETE_UNIT, NULL, (int32_t)a))->Dereference(); break;
    case EE_SCREENS_SH_BITMAP_ROTATION: reinterpret_cast<GEBitmap*>(a)->SetRotation(f[0]); break;
    case EE_SCREENS_SH_SELF: return EEUIShell::Of(c)->SelfCall((uint32_t)a);
    case EE_SCREENS_SH_KEYBOARD: EEUIShell::Of(c)->Keyboard(a != 0); break;
    case EE_SCREENS_SH_ALWAYS_DIRTY: c->SetAlwaysDirty(a != 0); break;
    case EE_SCREENS_SH_EDITOR_TEXTURE: return (int64_t) static_cast<EEUIFScenarioEditor*>(c->GetForm())->GetButtonTexture();
    // the message box's buttons' (shells_dialog.rs)
    case EE_SCREENS_SH_FOCUSED_TEXT_COLOR: static_cast<UIButton*>(c)->SetFocusedTextColor(GEColor(f[0], f[1], f[2])); break;
    case EE_SCREENS_SH_CLOSE_FORM: gUserInterface->CloseForm(reinterpret_cast<UIForm*>(a)); break;
    // the saved-group buttons' (shells_saved.rs)
    case EE_SCREENS_SH_SG_NEW_PORTRAIT: return (int64_t) new GEBitmap;
    case EE_SCREENS_SH_SG_PLAY: gSoundManager->Play((unsigned long)a, 1.0f, 0.0f, 1.0f, false, NULL); break;
    case EE_SCREENS_SH_SG_PLAYER: return (int64_t)gWorld.GetCurrentPlayer();
    case EE_SCREENS_SH_SG_SHIFT: return UIControl::ShiftKeyPressed();
    case EE_SCREENS_SH_SG_INDEX: return (int64_t)c->GetIndex();
    case EE_SCREENS_SH_SG_SET_TEXTURE: reinterpret_cast<GEBitmap*>(a)->SetTexture(reinterpret_cast<GETexture*>(b), c->GetScreenIndex()); break;
    // the icon bar's (shells_icon_bar.rs): the slots let go as its destructor did
    // the chat line's (shells_chat.rs): its texts deleted, as its destructor did
    // the paint areas' (shells_paint.rs)
    case EE_SCREENS_SH_MAP_HALF: f[0] = gWorld.GetMap()->GetF1955F4(); break;
    case EE_SCREENS_SH_WINDOW_SIZE: return (int64_t)gGameWindow->GetWidth() | (int64_t)gGameWindow->GetHeight() << 32;
    case EE_SCREENS_SH_MOUSE_BOUNDS: gMouse->SetBounds(GERectangle(f[0], f[1], f[2], f[3])); break;
    case EE_SCREENS_SH_SCROLL_EDGES: reinterpret_cast<EEAScroll*>(a)->mEdges = GERectangle(f[0], f[1], f[2], f[3]); break;
    case EE_SCREENS_SH_CAPTURE_MOUSE: static_cast<EEUIShellControl*>(c)->Capture(); break;
    case EE_SCREENS_SH_PAINT_NEW: return (int64_t) static_cast<EESEAPaint*>(new EESEAPaintRust((uint32_t)a, (char)b));
    case EE_SCREENS_SH_ACTION_POST: gUserInterface->PostAction(reinterpret_cast<EESEAPaint*>(a)); break;
    case EE_SCREENS_SH_ACTION_RELEASE: reinterpret_cast<EESEAPaint*>(a)->Dereference(); break;
    // the scenario editor lane's (0xa0..): the diplomacy buttons' state, an action's post and release
    case EE_SCREENS_SH_SE_DIPLOMACY: gEEUIShellDiplomacyShow(static_cast<UIButton*>(c), (unsigned long)a, b != 0); break;
    case EE_SCREENS_SH_SE_ACTION_POST: gUserInterface->PostAction(reinterpret_cast<EEAction*>(a)); break;
    case EE_SCREENS_SH_SE_ACTION_RELEASE: reinterpret_cast<EEAction*>(a)->Dereference(); break;
    case EE_SCREENS_SH_BRUSH_HIGHLIGHT: gUIManager->GetScenarioEditorForm()->GetBrushGroup()->SetHighlight(f[0], f[1], a != 0); break;
    case EE_SCREENS_SH_BRUSH_GROUP_SIZE: return reinterpret_cast<EEUIFSEBrushGroup*>(a)->mBrushSize;
    case EE_SCREENS_SH_TERRAIN_RUST: return (int64_t)ee_screens_segr_tab(reinterpret_cast<EeScreensSeGroup*>(a));
    // the game's
    case EE_SCREENS_SH_TRIGGER_REFRESH: EEShellTriggers()->TriggerSubSlot((long)a, EE_SCREENS_SETS_SCREEN_REFRESH); break;
    case EE_SCREENS_SH_TRIGGER_REFRESH_SCREEN: EEShellTriggers()->TriggerCall(EE_SCREENS_SETS_REFRESH_SCREEN); break;
    case EE_SCREENS_SH_TRIGGER_JUMP: EESETriggerJump((unsigned long)a, (unsigned long)b); break;
    case EE_SCREENS_SH_OBJECT_UPDATE_SELECTION: EEShellTriggers()->TriggerSubOp(EE_SCREENS_SE_TRIGGER_SCREEN_OBJECT, EE_SCREENS_SEOB_UPDATE_SELECTION); break;
    case EE_SCREENS_SH_OBJECT_STORE: EEShellTriggers()->TriggerSubSlot(EE_SCREENS_SE_TRIGGER_SCREEN_OBJECT, EE_SCREENS_SETS_SCREEN_STORE); break;
    case EE_SCREENS_SH_COPY_FIRST_TO_ALL: static_cast<EEUIFScenarioEditor*>(c->GetForm())->PlayerCall(EE_SCREENS_SEPL_COPY_FIRST_TO_ALL); break;
    case EE_SCREENS_SH_BRUSH_SIZE: reinterpret_cast<EEUIFSEBrushGroup*>(a)->mBrushSize = (long)b; break;
    }
    return 0;
}

// ---- the shims of the moved cases (UI lane round 7: ee_game screens shell_calls.rs, game_cpp.rs), each one C++
// statement on the shell's control or the world
// (C: the main game form section's)
// shims6
extern "C" void ee_screenscpp_shell_text_fg(uintptr_t inControl, uint32_t inWhich, float inRed, float inGreen, float inBlue)
{ EEUIShell::Of(C(inControl))->Text(inWhich)->SetFgColor(GEColor(inRed, inGreen, inBlue)); }
extern "C" void ee_screenscpp_shell_text_transparent(uintptr_t inControl, uint32_t inWhich)
{ EEUIShell::Of(C(inControl))->Text(inWhich)->SetIsTransparent(true); }
extern "C" void ee_screenscpp_shell_button_set_text(uintptr_t inControl, EeScreensShimText inText, uintptr_t inFont)
{ static_cast<UIButton*>(C(inControl))->SetText(EEShimText(inText), reinterpret_cast<GEFont*>(inFont)); }
extern "C" void ee_screenscpp_shell_combo_append(uintptr_t inControl, EeScreensShimText inText)
{ static_cast<UIComboBox*>(C(inControl))->AppendLine(EEShimText(inText)); }
extern "C" uint32_t ee_screenscpp_shell_combo_line_count(uintptr_t inControl)
{ return (uint32_t)static_cast<UIComboBox*>(C(inControl))->GetListBox()->GetLineCount(); }
extern "C" bool ee_screenscpp_shell_combo_line_is(uintptr_t inControl, uint32_t inLine, uintptr_t inText)
{ return static_cast<UIComboBox*>(C(inControl))->GetListBox()->GetData(inLine) == *reinterpret_cast<const UWideString*>(inText); }
extern "C" void ee_screenscpp_sound_unload_media(uintptr_t inSound)
{ reinterpret_cast<EESoundObject*>(inSound)->UnloadMedia(); }
extern "C" uint32_t ee_screenscpp_sound_sample_id(uintptr_t inSound)
{ return (uint32_t)reinterpret_cast<EESoundObject*>(inSound)->GetSampleID(); }
extern "C" uintptr_t ee_screenscpp_selected_first_unit()
{ return (uintptr_t)gWorld.GetCurrentPlayer()->mSelectedGroup->GetFirstUnit(); }
extern "C" uintptr_t ee_screenscpp_player_group(uint32_t inGroup)
{ return (uintptr_t)gWorld.GetCurrentPlayer()->mGroups[inGroup]; }
extern "C" uint32_t ee_screenscpp_group_count(uintptr_t inGroup)
{ return (uint32_t)reinterpret_cast<EEGroup*>(inGroup)->GetNumUnits(); }
extern "C" uintptr_t ee_screenscpp_group_first_unit(uintptr_t inGroup)
{ return (uintptr_t)reinterpret_cast<EEGroup*>(inGroup)->GetFirstUnit(); }
extern "C" void ee_screenscpp_send_saved_group_select(uint32_t inGroup, bool inAdd)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageSavedGroupSelect, {(unsigned long)inGroup, inAdd}), kSlotNone); }
extern "C" void ee_screenscpp_send_saved_group_add_selection(uint32_t inGroup)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageSavedGroupAddSelection, {(unsigned long)inGroup}), kSlotNone); }
extern "C" void ee_screenscpp_send_saved_group_create(uint32_t inGroup)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageSavedGroupCreate, {(unsigned long)inGroup}), kSlotNone); }
extern "C" void ee_screenscpp_send_saved_group_clear(uint32_t inGroup)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageSavedGroupClear, {(unsigned long)inGroup}), kSlotNone); }
extern "C" uintptr_t ee_screenscpp_unit_stats_button(uintptr_t inUnit)
{ return (uintptr_t)reinterpret_cast<EEComplexUnit*>(inUnit)->mStats->mButton; }
extern "C" void ee_screenscpp_button_set_face(uintptr_t inButton, uintptr_t inBitmap, uintptr_t inControl)
{ reinterpret_cast<EEButtonObject*>(inButton)->SetFaceBitmap(reinterpret_cast<GEBitmap*>(inBitmap), C(inControl)->GetRectangle(), gScreenIndex); }
extern "C" float ee_screenscpp_button_cell_u(uintptr_t inButton)
{ return reinterpret_cast<EEButtonObject*>(inButton)->GetCellU(); }
extern "C" float ee_screenscpp_button_cell_v(uintptr_t inButton)
{ return reinterpret_cast<EEButtonObject*>(inButton)->GetCellV(); }
extern "C" uintptr_t ee_screenscpp_button_texture(uintptr_t inButton)
{ return (uintptr_t)reinterpret_cast<EEButtonObject*>(inButton)->GetTexture(); }
extern "C" void ee_screenscpp_texture_reference(uintptr_t inTexture)
{ reinterpret_cast<GETexture*>(inTexture)->Reference(); }
extern "C" void ee_screenscpp_texture_dereference(uintptr_t inTexture)
{ reinterpret_cast<GETexture*>(inTexture)->Dereference(); }
extern "C" void ee_screenscpp_bitmap_at_button(uintptr_t inBitmap, uintptr_t inControl)
{ reinterpret_cast<GEBitmap*>(inBitmap)->SetLocation(static_cast<UIButton*>(C(inControl))->GetBitmapLocation()); }
extern "C" void ee_screenscpp_shell_draw_bitmap(uintptr_t inControl, uintptr_t inBitmap)
{ C(inControl)->GetRasterizer()->Draw(reinterpret_cast<GEBitmap*>(inBitmap), C(inControl)->GetScreenIndex()); }
extern "C" uintptr_t ee_screenscpp_shell_slot(uintptr_t inControl, uint32_t inSlot)
{ return (uintptr_t)static_cast<EEUIShellControl*>(C(inControl))->mSlots[inSlot]; }
extern "C" void ee_screenscpp_shell_clear_slot(uintptr_t inControl, uint32_t inSlot)
{ static_cast<EEUIShellControl*>(C(inControl))->mSlots[inSlot] = NULL; }
extern "C" void ee_screenscpp_text_delete(uintptr_t inText) { delete reinterpret_cast<GEText*>(inText); }
extern "C" float ee_screenscpp_control_width(uintptr_t inControl) { return C(inControl)->GetRectangle().GetWidth(); }
extern "C" float ee_screenscpp_control_height(uintptr_t inControl) { return C(inControl)->GetRectangle().GetHeight(); }
extern "C" void ee_screenscpp_shim_tile_clear() { EEScratch<EETile*>(kScratchUIShellShimTile) = NULL; }
extern "C" void ee_screenscpp_shim_point_clear() { EEScratch<GE3DPoint>(kScratchShimPoint) = GE3DPoint(); }
extern "C" bool ee_screenscpp_pick_tile_xy(float inX, float inY, long* inTileX, long* inTileY)
{ return EEPointUnitOrTileXY(GE2DPoint(inX, inY), EEScratch<EETile*>(kScratchUIShellShimTile), *inTileX, *inTileY, false); }
extern "C" bool ee_screenscpp_shim_tile_exists() { return EEScratch<EETile*>(kScratchUIShellShimTile) != NULL; }
extern "C" float ee_screenscpp_shim_tile_height() { return EEScratch<EETile*>(kScratchUIShellShimTile)->GetHeight(); }
extern "C" void ee_screenscpp_pick_map_location(float inX, float inY)
{ EEScratch<EETile*>(kScratchUIShellShimTile) = gWorld.GetMap()->GetLocationAt(GE2DPoint(inX, inY), EEScratch<GE3DPoint>(kScratchShimPoint), false); }
extern "C" void ee_screenscpp_pick_unit_or_tile(float inX, float inY)
{ EEPointUnitOrTile(GE2DPoint(inX, inY), EEScratch<EETile*>(kScratchUIShellShimTile), EEScratch<GE3DPoint>(kScratchShimPoint), false); }
extern "C" float ee_screenscpp_shim_point_x() { return EEScratch<GE3DPoint>(kScratchShimPoint).GetX(); }
extern "C" float ee_screenscpp_shim_point_y() { return EEScratch<GE3DPoint>(kScratchShimPoint).GetY(); }
extern "C" float ee_screenscpp_shim_point_z() { return EEScratch<GE3DPoint>(kScratchShimPoint).GetZ(); }
extern "C" void ee_screenscpp_map_tile_load(uint32_t inX, uint32_t inY)
{ EEScratch<EEMapTile*>(kScratchShimMapTile) = gWorld.mMap->GetTile((unsigned long)inX, (unsigned long)inY); }
extern "C" bool ee_screenscpp_shim_map_tile_exists() { return EEScratch<EEMapTile*>(kScratchShimMapTile) != NULL; }
extern "C" float ee_screenscpp_shim_map_tile_height() { return EEScratch<EEMapTile*>(kScratchShimMapTile)->GetHeight(); }
extern "C" float ee_screenscpp_mouse_x() { return gMouse->mPosition.GetX(); }
extern "C" float ee_screenscpp_mouse_y() { return gMouse->mPosition.GetY(); }
extern "C" float ee_screenscpp_shell_mouse_x(uintptr_t inControl)
{ return static_cast<EEUIShellControl*>(C(inControl))->MouseLocation().GetX(); }
extern "C" float ee_screenscpp_shell_mouse_y(uintptr_t inControl)
{ return static_cast<EEUIShellControl*>(C(inControl))->MouseLocation().GetY(); }
extern "C" float ee_screenscpp_view_left() { return gViewport.GetViewRectangle().GetLeft(); }
extern "C" float ee_screenscpp_view_top() { return gViewport.GetViewRectangle().GetTop(); }
extern "C" float ee_screenscpp_view_right() { return gViewport.GetViewRectangle().GetRight(); }
extern "C" float ee_screenscpp_view_bottom() { return gViewport.GetViewRectangle().GetBottom(); }
// end shims6

// ---- the rest of the multi-statement cases (UI lane round 9: shell_calls.rs), each one statement, and the
// cases' locals (EEShellTree and EEShellRotate are Rust's)
#define TREE(inTree) reinterpret_cast<EETechTree*>(inTree)
static const unsigned long kShKeys[] = {kKeyComma, kKeyPeriod, kKeySpace};
extern "C" void ee_screenscpp_sh_files_clear() { EEScratch<std::set<UString>>(kScratchShFiles).clear(); }
extern "C" void ee_screenscpp_sh_files_list(uintptr_t inPath, uintptr_t inPattern)
{ reinterpret_cast<FSPath*>(inPath)->ListFiles(UString(reinterpret_cast<const char*>(inPattern)), EEScratch<std::set<UString>>(kScratchShFiles)); }
extern "C" void ee_screenscpp_sh_files_snapshot() { EEScratch<std::vector<UString>>(kScratchShLines).assign(EEScratch<std::set<UString>>(kScratchShFiles).begin(), EEScratch<std::set<UString>>(kScratchShFiles).end()); }
extern "C" uint32_t ee_screenscpp_sh_files_count() { return (uint32_t)EEScratch<std::vector<UString>>(kScratchShLines).size(); }
extern "C" void ee_screenscpp_sh_text_from_file(uint32_t inIndex) { EEScratch<UWideString>(kScratchShText) = EEScratch<std::vector<UString>>(kScratchShLines)[inIndex]; }
extern "C" void ee_screenscpp_sh_text_trim4() { EEScratch<UWideString>(kScratchShText).InsertString("", EEScratch<UWideString>(kScratchShText).GetLength() - 4, true); }
extern "C" void ee_screenscpp_sh_combo_append_text(uintptr_t inControl)
{ static_cast<UIComboBox*>(C(inControl))->AppendLine(EEScratch<UWideString>(kScratchShText)); }
extern "C" void ee_screenscpp_sh_numbered_format(int64_t inId) { gResource->GetString((unsigned long)inId, EEScratch<UWideString>(kScratchShFormat)); }
extern "C" void ee_screenscpp_sh_numbered_print(uint32_t inNumber)
{ sprintf(EEScratch<UWideString>(kScratchShText), EEScratch<UWideString>(kScratchShFormat), (unsigned long)inNumber); }
extern "C" void ee_screenscpp_sh_radio_clear_down_checked(uintptr_t inControl)
{ static_cast<UIRadioButton*>(C(inControl))->SetDownCheckedBitmap(NULL); }
extern "C" void ee_screenscpp_sh_radio_clear_up_checked(uintptr_t inControl)
{ static_cast<UIRadioButton*>(C(inControl))->SetUpCheckedBitmap(NULL); }
extern "C" void ee_screenscpp_sh_radio_clear_down_unchecked(uintptr_t inControl)
{ static_cast<UIRadioButton*>(C(inControl))->SetDownUncheckedBitmap(NULL); }
extern "C" void ee_screenscpp_sh_radio_clear_up_unchecked(uintptr_t inControl)
{ static_cast<UIRadioButton*>(C(inControl))->SetUpUncheckedBitmap(NULL); }
extern "C" int64_t ee_screenscpp_sh_form_list_selection(uintptr_t inControl, int64_t inId, unsigned long* inSelection)
{ return (int64_t)static_cast<UIListBox<UWideString>*>(C(inControl)->GetForm()->GetControlById((unsigned long)inId))->GetCurrentSelection(*inSelection); }
extern "C" void ee_screenscpp_sh_editor_player(unsigned long* inSelection)
{ gUIManager->GetScenarioEditorForm()->GetPlayerComboBox()->GetCurrentSelection(*inSelection); }
extern "C" void ee_screenscpp_sh_tech_tree_move(int32_t inA, int32_t inB, int32_t inC, bool inFlag)
{ (new EESEARust(EE_SCREENS_SEW_TECH_TREE, NULL, inA, inB, inC, inFlag))->Dereference(); }
extern "C" void ee_screenscpp_sh_combo_list_selection(uintptr_t inControl, unsigned long* inSelection)
{ const_cast<UILBString*>(static_cast<UIComboBox*>(C(inControl))->GetListBox())->GetCurrentSelection(*inSelection); }
extern "C" uintptr_t ee_screenscpp_sh_bitmap_new() { return (uintptr_t)new GEBitmap; }
extern "C" void ee_screenscpp_sh_bitmap_texture_form(uintptr_t inBitmap, uintptr_t inTexture, uintptr_t inControl)
{ reinterpret_cast<GEBitmap*>(inBitmap)->SetTexture(reinterpret_cast<GETexture*>(inTexture), C(inControl)->GetForm()->GetManager()->GetScreen()); }
extern "C" void ee_screenscpp_sh_bitmap_texture_global(uintptr_t inBitmap, uintptr_t inTexture)
{ reinterpret_cast<GEBitmap*>(inBitmap)->SetTexture(reinterpret_cast<GETexture*>(inTexture), gScreenIndex); }
extern "C" void ee_screenscpp_sh_bitmap_coords(uintptr_t inBitmap, float inLeft, float inTop, float inRight, float inBottom)
{ reinterpret_cast<GEBitmap*>(inBitmap)->SetTextureCoordinates(GETextureCoordinate(inLeft, inTop), GETextureCoordinate(inRight, inTop), GETextureCoordinate(inLeft, inBottom), GETextureCoordinate(inRight, inBottom)); }
extern "C" float ee_screenscpp_sh_texture_width(uintptr_t inTexture)
{ return (float)reinterpret_cast<GETexture*>(inTexture)->GetWidth(); }
extern "C" float ee_screenscpp_sh_texture_height(uintptr_t inTexture)
{ return (float)reinterpret_cast<GETexture*>(inTexture)->GetHeight(); }
extern "C" bool ee_screenscpp_sh_has_user_interface() { return gUserInterface != NULL; }
extern "C" void ee_screenscpp_sh_post_select_tab(uint8_t inTab, int32_t inPage)
{ gUserInterface->PostSelectEditorTab((unsigned char)inTab, (long)inPage); }
extern "C" void ee_screenscpp_sh_select_tab(uintptr_t inControl, uint8_t inTab, int32_t inPage)
{ static_cast<EEUIFScenarioEditor*>(C(inControl)->GetForm())->SelectTab((unsigned char)inTab, (long)inPage); }
extern "C" int64_t ee_screenscpp_sh_list_current(uintptr_t inControl, unsigned long* inSelection)
{ return (int64_t)static_cast<UILBString*>(C(inControl))->GetCurrentSelection(*inSelection); }
extern "C" uintptr_t ee_screenscpp_sh_form_control(uintptr_t inControl, int64_t inId)
{ return (uintptr_t)C(inControl)->GetForm()->GetControlById((unsigned long)inId); }
extern "C" bool ee_screenscpp_sh_is_radio(uintptr_t inControl)
{ return dynamic_cast<UIRadioButton*>(C(inControl)) != NULL; }
extern "C" bool ee_screenscpp_sh_radio_checked(uintptr_t inControl)
{ return dynamic_cast<UIRadioButton*>(C(inControl))->GetIsChecked(); }
extern "C" bool ee_screenscpp_sh_check_checked(uintptr_t inControl)
{ return static_cast<UICheckBox*>(C(inControl))->GetIsChecked(); }
extern "C" int64_t ee_screenscpp_sh_tech_name_id(int64_t inTechnology)
{ return (int64_t)gTechTree.Get((long)inTechnology)->mData->mNameID; }
extern "C" int64_t ee_screenscpp_sh_tech_class_name_id(int64_t inTechnology)
{ return (int64_t)gUnitClassTable[gTechTree.Get((long)inTechnology)->mData->mUnitClass]->mNameStringID; }
extern "C" uintptr_t ee_screenscpp_sh_player_tree(uint32_t inSelection)
{ return (uintptr_t)gWorld.mPlayers[inSelection + 1]->GetTechTree(); }
extern "C" int64_t ee_screenscpp_sh_tree_data_id(uintptr_t inTree, int64_t inTechnology)
{ return (int64_t)TREE(inTree)->Get((long)inTechnology)->mData->mID; }
extern "C" int64_t ee_screenscpp_sh_tree_depends(uintptr_t inTree, int64_t inTechnology, int64_t inOn)
{ return (int64_t)TREE(inTree)->Get((long)inTechnology)->DependsOn((long)inOn); }
extern "C" void ee_screenscpp_sh_tree_set_obsoleted(uintptr_t inTree, int64_t inTechnology, bool inObsoleted)
{ TREE(inTree)->Get((long)inTechnology)->SetScenEditorObsoleted(inObsoleted, true); }
extern "C" bool ee_screenscpp_sh_tree_epoch_reached(uintptr_t inTree, int64_t inTechnology)
{ return TREE(inTree)->Get((long)inTechnology)->mEpoch <= TREE(inTree)->mCurrentEpoch; }
extern "C" int64_t ee_screenscpp_sh_tree_obsoleted(uintptr_t inTree, int64_t inTechnology)
{ return (int64_t)TREE(inTree)->Get((long)inTechnology)->mScenEditorObsoleted; }
extern "C" void ee_screenscpp_sh_tree_mark_player(uintptr_t inTree)
{ gUIManager->GetScenarioEditorForm()->PlayerCall(EE_SCREENS_SEPL_MARK_CHANGED, TREE(inTree)->GetPlayer()->mPlayerID, false); }
extern "C" int64_t ee_screenscpp_sh_class_name_id(int64_t inUnitClass)
{ return (int64_t)gUnitClassTable[(long)inUnitClass]->mNameStringID; }
extern "C" int64_t ee_screenscpp_sh_class_epoch(int64_t inUnitClass)
{ return (int64_t)gUnitClassTable[(long)inUnitClass]->mEpoch; }
extern "C" int64_t ee_screenscpp_sh_class_family(int64_t inUnitClass)
{ return (int64_t)gUnitClassTable[(long)inUnitClass]->mFamily; }
extern "C" void ee_screenscpp_sh_list_insert(uintptr_t inControl, uintptr_t inText, int64_t inIndex)
{ static_cast<UILBString*>(C(inControl))->InsertLine(UWideString(reinterpret_cast<const char*>(inText)), (unsigned long)inIndex, EEScratch<UILBNoSort<UWideString>>(kScratchShNoSort)); }
extern "C" void ee_screenscpp_sh_list_data(uintptr_t inControl, int64_t inIndex, void* inOut)
{ EEShimOutC(inOut, (const char*)static_cast<UILBString*>(C(inControl))->GetData((unsigned long)inIndex)); }
extern "C" int64_t ee_screenscpp_sh_unit_name_id(uintptr_t inUnit)
{ return (int64_t)reinterpret_cast<EEUnit*>(inUnit)->mUnitClass->mNameStringID; }
extern "C" uint32_t ee_screenscpp_sh_unit_id(uintptr_t inUnit)
{ return (uint32_t)reinterpret_cast<EEUnit*>(inUnit)->GetUnitID(); }
extern "C" uint32_t ee_screenscpp_sh_ghost_count() { return (uint32_t)gWorld.GetNumGhostUnits(); }
extern "C" void ee_screenscpp_sh_ghosts_enter() { gWorld.mGhostUnitsLock->Enter(); }
extern "C" void ee_screenscpp_sh_ghosts_leave() { gWorld.mGhostUnitsLock->Leave(); }
extern "C" bool ee_screenscpp_sh_ghost_turns(uint32_t inIndex)
{ return gUnitClassTable[static_cast<EEGhostUnit*>(gWorld.GetGhostUnit(inIndex))->mUnitClass->mUnitType]->mAngularAcceleration > 0.0f; }
extern "C" void ee_screenscpp_sh_tr_from_ghost(uint32_t inIndex)
{ EEScratch<GETransformation>(kScratchShTransformation) = static_cast<EEGhostUnit*>(gWorld.GetGhostUnit(inIndex))->mTransformation; }
extern "C" void ee_screenscpp_sh_tr_orient(float inAngle) { EEScratch<GETransformation>(kScratchShTransformation).SetOrientationYPR(inAngle, 0.0f, 0.0f); }
extern "C" void ee_screenscpp_sh_tr_to_ghost(uint32_t inIndex)
{ static_cast<EEGhostUnit*>(gWorld.GetGhostUnit(inIndex))->mTransformation = EEScratch<GETransformation>(kScratchShTransformation); }
extern "C" uint32_t ee_screenscpp_sh_editor_selected_count(uintptr_t inControl)
{ return (uint32_t)static_cast<EEUIFScenarioEditor*>(C(inControl)->GetForm())->GetSelectedUnitCount(); }
extern "C" uintptr_t ee_screenscpp_sh_editor_selected(uintptr_t inControl, uint32_t inIndex)
{ return (uintptr_t)static_cast<EEUIFScenarioEditor*>(C(inControl)->GetForm())->GetSelectedUnit(inIndex); }
extern "C" bool ee_screenscpp_sh_unit_has_complex(uintptr_t inUnit)
{ return reinterpret_cast<EEUnit*>(inUnit)->mComplexUnit != NULL; }
extern "C" bool ee_screenscpp_sh_unit_turns(uintptr_t inUnit)
{ return reinterpret_cast<EEUnit*>(inUnit)->mUnitClass->mAngularAcceleration > 0.0f; }
extern "C" void ee_screenscpp_sh_tr_from_unit(uintptr_t inUnit)
{ EEScratch<GETransformation>(kScratchShTransformation) = reinterpret_cast<EEUnit*>(inUnit)->mComplexUnit->mPhysicalModel.GetLocation(); }
extern "C" void ee_screenscpp_sh_tr_to_unit(uintptr_t inUnit)
{ reinterpret_cast<EEUnit*>(inUnit)->mComplexUnit->SetTransformation(EEScratch<GETransformation>(kScratchShTransformation), true); }
extern "C" int64_t ee_screenscpp_sh_key_load(uint32_t inWhich)
{ return (int64_t)gISKeyboard->GetEntity(kShKeys[inWhich], EEScratch<ISEntity*>(kScratchShKey)); }
extern "C" bool ee_screenscpp_sh_key_just_pressed() { return EEScratch<ISEntity*>(kScratchShKey)->JustPressed() != 0; }
extern "C" bool ee_screenscpp_sh_key_just_unpressed() { return EEScratch<ISEntity*>(kScratchShKey)->JustUnpressed() != 0; }
extern "C" int64_t ee_screenscpp_sh_key_value() { return (int64_t)EEScratch<ISEntity*>(kScratchShKey)->GetValue(); }
extern "C" void ee_screenscpp_sh_screen_line(float inX, float inY)
{ gWorld.mMap->GetScreenLine(GE2DPoint(inX, inY), EEScratch<GE3DLine>(kScratchShLine)); }
extern "C" float ee_screenscpp_sh_line_start_x() { return EEScratch<GE3DLine>(kScratchShLine).GetStart().GetX(); }
extern "C" float ee_screenscpp_sh_line_start_y() { return EEScratch<GE3DLine>(kScratchShLine).GetStart().GetY(); }
extern "C" float ee_screenscpp_sh_line_start_z() { return EEScratch<GE3DLine>(kScratchShLine).GetStart().GetZ(); }
extern "C" float ee_screenscpp_sh_line_direction_x() { return EEScratch<GE3DLine>(kScratchShLine).GetDirection().GetX(); }
extern "C" float ee_screenscpp_sh_line_direction_y() { return EEScratch<GE3DLine>(kScratchShLine).GetDirection().GetY(); }
extern "C" float ee_screenscpp_sh_line_direction_z() { return EEScratch<GE3DLine>(kScratchShLine).GetDirection().GetZ(); }
extern "C" bool ee_screenscpp_sh_right_down(uintptr_t inControl) { return C(inControl)->GetRightMouseButtonDown(); }
extern "C" bool ee_screenscpp_sh_left_down(uintptr_t inControl) { return C(inControl)->GetLeftMouseButtonDown(); }
extern "C" bool ee_screenscpp_sh_has_capture(uintptr_t inControl) { return C(inControl)->GetHasMouseCapture(); }
extern "C" int64_t ee_screenscpp_sh_paint_area(uintptr_t inPaint, int32_t inLeft, int32_t inTop, int32_t inRight, int32_t inBottom)
{ return (int64_t)reinterpret_cast<EESEAPaint*>(inPaint)->SetArea(inLeft, inTop, inRight, inBottom); }
extern "C" int64_t ee_screenscpp_sh_slider_max(uintptr_t inControl, int64_t inId)
{ return (int64_t)static_cast<UISlider*>(C(inControl)->GetForm()->GetControlById((unsigned long)inId))->GetMaxRange(); }
extern "C" uint32_t ee_screenscpp_sh_slider_current(uintptr_t inControl, int64_t inId)
{ return (uint32_t)static_cast<UISlider*>(C(inControl)->GetForm()->GetControlById((unsigned long)inId))->GetCurrentRangeValue(); }
extern "C" uintptr_t ee_screenscpp_sh_rasterizer(uintptr_t inControl)
{ return (uintptr_t)C(inControl)->GetRasterizer(); }
extern "C" uint32_t ee_screenscpp_sh_screen_index(uintptr_t inControl)
{ return (uint32_t)C(inControl)->GetScreenIndex(); }
extern "C" uintptr_t ee_screenscpp_sh_copied_draw_rectangle(uintptr_t inControl)
{ return (uintptr_t)&C(inControl)->GetCopiedDrawRectangle(); }

// the icon bar's and chat line's factories and the tech list's bars: shell_factories.rs over these (UI lane round 12)
#define SF(c) static_cast<EEUIShellControl*>(reinterpret_cast<UIControl*>(c))
#define SFT(t) reinterpret_cast<UIControlsTable*>(t)
extern "C" int64_t ee_screenscpp_sf_shell_op(uintptr_t inControl, uint32_t inOp, int64_t inA, int64_t inB, float* inF)
{ return EEUIShell::Op(reinterpret_cast<UIControl*>(inControl), inOp, inA, inB, inF); }
extern "C" void ee_screenscpp_sf_slot_set(uintptr_t inControl, uint32_t inSlot, uintptr_t inValue)
{ SF(inControl)->mSlots[inSlot] = (void*)inValue; }
extern "C" uintptr_t ee_screenscpp_sf_slots(uintptr_t inControl) { return (uintptr_t)SF(inControl)->mSlots; }
extern "C" uintptr_t ee_screenscpp_sf_bitmap_new() { return (uintptr_t)new GEBitmap; }
extern "C" uintptr_t ee_screenscpp_sf_normal_bitmap(uintptr_t inTable, uintptr_t inForm, float inLeft, float inTop, float inRight, float inBottom)
{ return (uintptr_t)SFT(inTable)->CreateNormalBitmap(((UIForm*)inForm)->GetManager(), GERectangle(inLeft, inTop, inRight, inBottom)); }
extern "C" uintptr_t ee_screenscpp_sf_sound(uint32_t inId) { return (uintptr_t)EESoundObject::Get(inId); }
extern "C" void ee_screenscpp_sf_sound_load(uintptr_t inSound) { ((EESoundObject*)inSound)->LoadMedia(); }
extern "C" uintptr_t ee_screenscpp_sf_sound_sample(uintptr_t inSound)
{ return (uintptr_t)((EESoundObject*)inSound)->GetSampleID(); }
extern "C" float ee_screenscpp_sf_table_bg_opacity(uintptr_t inTable) { return SFT(inTable)->GetBackgroundOpacity(); }
extern "C" bool ee_screenscpp_sf_table_always_dirty(uintptr_t inTable) { return SFT(inTable)->GetAlwaysDirty(); }
extern "C" uintptr_t ee_screenscpp_sf_table_font(uintptr_t inTable) { return (uintptr_t)SFT(inTable)->GetFont(); }
extern "C" void ee_screenscpp_sf_set_always_dirty(uintptr_t inControl, bool inOn)
{ SF(inControl)->SetAlwaysDirty(inOn); }
extern "C" void ee_screenscpp_sf_bg_color_of_table(uintptr_t inControl, uintptr_t inTable)
{ SF(inControl)->SetBgColor(SFT(inTable)->GetBackground()); }
extern "C" void ee_screenscpp_sf_bg_opacity(uintptr_t inControl, float inOpacity)
{ SF(inControl)->SetBgOpacity(inOpacity); }
extern "C" uintptr_t ee_screenscpp_sf_bold_font(uintptr_t inFont)
{ return (uintptr_t)UIFontTable::FindOrCreateFont(((GEFont*)inFont)->GetFontName(), ((GEFont*)inFont)->GetFontStyle() | kFontStyleBold, ((GEFont*)inFont)->GetFontQuality(), ((GEFont*)inFont)->GetFontSize()); }
extern "C" uintptr_t ee_screenscpp_sf_text_new(uintptr_t inFont) { return (uintptr_t)new GEText((GEFont*)inFont); }
extern "C" void ee_screenscpp_sf_table_setup_text(uintptr_t inTable, uintptr_t inText)
{ SFT(inTable)->SetupGEText((GEText*)inText); }
extern "C" void ee_screenscpp_sf_text_at_top_left(uintptr_t inText, uintptr_t inControl)
{ ((GEText*)inText)->SetLocation(SF(inControl)->GetRectangle().GetTopLeft()); }
extern "C" uintptr_t ee_screenscpp_bars_rhombus_new() { return (uintptr_t)new GERhombus; }
extern "C" void ee_screenscpp_bars_rhombus_free(uintptr_t inRhombus) { delete (GERhombus*)inRhombus; }
extern "C" void ee_screenscpp_bars_rhombus_color(uintptr_t inRhombus, float inRed, float inGreen, float inBlue)
{ ((GERhombus*)inRhombus)->SetColor(GEColor(inRed, inGreen, inBlue)); }
extern "C" void ee_screenscpp_bars_copy_text_rect(uintptr_t inControl, float* inOut)
{ UIObjHost::PutRect(inOut, static_cast<EEUIShellList*>(reinterpret_cast<UIControl*>(inControl))->GetCopyTextRectangle()); }
extern "C" uint64_t ee_screenscpp_bars_line_height(uintptr_t inControl)
{ return (uint64_t)static_cast<EEUIShellList*>(reinterpret_cast<UIControl*>(inControl))->GetLineHeight(); }
extern "C" void ee_screenscpp_bars_rhombus_rect(uintptr_t inRhombus, float inLeft, float inTop, float inRight, float inBottom)
{ ((GERhombus*)inRhombus)->SetRectangle(GERectangle(inLeft, inTop, inRight, inBottom)); }
extern "C" int64_t ee_screenscpp_bars_draw(uintptr_t inControl, uintptr_t inRhombus)
{ return (int64_t)reinterpret_cast<UIControl*>(inControl)->GetRasterizer()->Draw((GERhombus*)inRhombus, reinterpret_cast<UIControl*>(inControl)->GetScreenIndex()); }
