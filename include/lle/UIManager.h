// C:\EEX\Low-Level Engine\User Interface\UIManager.h (RECONSTRUCTION). The UI root: the forms and the form stack,
// the visible forms, the queued form events, the shared control bitmaps, the music, and the rasterizer/sound/
// string-table handles. UIManager.cpp (0x10078c81..0x1007ba37, __FILE__ asserts) held the ctor and the out-of-line
// members; the inline members were emitted first by UIBackground.cpp (0x10059fdb..0x1005a269, declaration order).
// Vtable 0x10098b70 (13 slots, exported names). The original's sizeof was 0x12c (??_E stride, 0x1005a70d); its
// members are the Rust manager's now (see the class). Requires lle/UIForm.h (CreateForm).
#ifndef LLE_UIMANAGER_H
#define LLE_UIMANAGER_H

#include <stdint.h>

#include "lle/UIEvent.h"
#include "lle/UIForm.h"
#include "rust/ui/managerobj.h"


// The UI database file names (exported globals; defined in Utilities, 0x1012ec10..)


// The class is whole in Rust (rust/crates/ee_ui/src/managerobj.rs, R6 ui round 17): its state (the event factories,
// the stack, the forms, the visible and copied forms, the queued events, the fonts, the music, the UI texture and the
// control bitmaps, the candidate window, the locks, the handles, the counts and flags) and every method but the
// database's are the Rust object mRust; UIManager.cpp holds the forwarders, the ctor and the dtor, the database's
// LoadDatabase, CreateBackgrounds and RemoveDatabase, the mouse buttons' statics and the host (the op switch,
// HostOp: a member, so that it has the friendship UIForm gives UIManager). EEUIManager derives from it and overrides
// UpdateKeyboard and UpdateMouse; the Rust UpdateUI calls them through the C++ object.
class LLE_API UIManager {
public:
    // Creates the UI manager: its locks, the UIControls texture and the check box, radio, sort and scroll-bar
    // bitmaps cut from it; looks up the shared mouse buttons and wheel on first use; no music playing
    // (ee_ui_manager_start: Init's ops read the manager through the forwarders, so it runs after mRust is set)
    UIManager(GERasterizer* inRasterizer, SESoundManager* inSoundManager, unsigned long inNumberOfSurfaces,
              unsigned long inUITextureBitDepth, UResourceFile* inStringTable, unsigned long inScreen)
        : mRust(ee_ui_manager_new(this, ee_ui_manager_host, inRasterizer, inSoundManager, inNumberOfSurfaces,
                                  inUITextureBitDepth, inStringTable, inScreen))
    {
        ee_ui_manager_start(this->mRust);
    }
    // Destroys the UI manager: stops the music, frees the locks, texture, bitmaps, forms, hot keys, IME window and database
    virtual ~UIManager() { ee_ui_manager_destroy(this->mRust); }
    // Marks a rectangle dirty: on the top form directly, else on that form and every visible form above that overlaps it
    void AddDirtyRectangle(const GERectangle& inRectangle, UIForm* inForm) { this->CallRect(EeUiManagerCall::AddDirtyRectangle, (intptr_t)inForm, inRectangle); }
    // Passes an IME result string to the top form, under the UI lock
    void AddIMEResultString(const UWideString& inString) { this->Call(EeUiManagerCall::AddImeResult, (intptr_t)&inString); }
    // Passes the IME composition string, attributes and cursor to the top form under the UI lock; returns its result
    long ChangeIMECompositionString(const UWideString& inString, const UString& inAttributes, unsigned long inCursor)
    {
        return ee_ui_manager_change_composition(this->mRust, &(const EeUiImeComposition&)EeUiImeComposition{&inString, &inAttributes, inCursor});
    }
    void ChangeRasterizer(GERasterizer* inRasterizer) { this->Call(EeUiManagerCall::ChangeRasterizer, (intptr_t)inRasterizer); }
    // Snapshots every visible form (and the candidate window when open) for drawing; returns the last copy result
    long CopyData() { return (long)this->Call(EeUiManagerCall::CopyData); }
    virtual UIBackground* CreateUserDefinedBackground(const UIBackgroundTable& inTable) { return NULL; }
    virtual UIControl* CreateUserDefinedControl(UIControlsTable& inTable) { return NULL; }
    // inline: exported 0x1005a028 (UIBackground.cpp's inline block)
    virtual UIForm* CreateForm(const UIDbDataForms& inData) { return new UIForm(this, inData, true); }
    // Draws the snapshotted forms with alpha blending (their text too, unless drawn by GDI) and the candidate window
    long DrawPolygons() { return (long)this->Call(EeUiManagerCall::DrawPolygons); }
    // Draws the GDI text of the snapshotted forms and the candidate window, under the draw lock
    long DrawTextA() { return (long)this->Call(EeUiManagerCall::DrawText); }
    // The header's getters and setters (the original's inline members, 0x10059fdb..0x1005a269)
    void ForceUIUpdate() { this->Call(EeUiManagerCall::ForceUiUpdate); }
    GEBitmap* GetAlphaSortDownBitmap() const { return reinterpret_cast<GEBitmap*>(this->Call(EeUiManagerCall::Bitmap, 1)); }
    GEBitmap* GetAlphaSortUpBitmap() const { return reinterpret_cast<GEBitmap*>(this->Call(EeUiManagerCall::Bitmap, 0)); }
    GEBitmap* GetCheckDownOff() const { return reinterpret_cast<GEBitmap*>(this->Call(EeUiManagerCall::Bitmap, 12)); }
    GEBitmap* GetCheckDownOn() const { return reinterpret_cast<GEBitmap*>(this->Call(EeUiManagerCall::Bitmap, 14)); }
    GEBitmap* GetCheckOff() const { return reinterpret_cast<GEBitmap*>(this->Call(EeUiManagerCall::Bitmap, 11)); }
    GEBitmap* GetCheckOn() const { return reinterpret_cast<GEBitmap*>(this->Call(EeUiManagerCall::Bitmap, 13)); }
    UIForm* GetCurrentForm() const { return reinterpret_cast<UIForm*>(this->Call(EeUiManagerCall::CurrentForm)); }
    // Returns the form with the given ID (NULL: none)
    UIForm* GetFormByID(unsigned long inID) const
    {
        // an unknown ID gives NULL (the original returned the word one past the end of the form list)
        return reinterpret_cast<UIForm*>(this->Call(EeUiManagerCall::FormById, (intptr_t)inID));
    }
    unsigned long GetFormCount() const { return (unsigned long)this->Call(EeUiManagerCall::FormCount); }
    UIForm* GetStackedForm(unsigned long inIndex) const { return reinterpret_cast<UIForm*>(this->Call(EeUiManagerCall::StackedForm, (intptr_t)inIndex)); } // port (test fixture)
    GEBitmap* GetDragBarBitmap() const { return reinterpret_cast<GEBitmap*>(this->Call(EeUiManagerCall::Bitmap, 10)); }
    // the bitmap by its index (EeUiManagerCall::Bitmap: GetAlphaSortUpBitmap 0 .. GetRadioDownOn 20)
    GEBitmap* GetBitmap(unsigned long inIndex) const { return reinterpret_cast<GEBitmap*>(this->Call(EeUiManagerCall::Bitmap, (intptr_t)inIndex)); }
#if defined(LOWLEVELENGINE_EXPORTS) || defined(LLE_CURRENT_HEADERS)   // the exe reads __imp_sLMouseButton inline (0x17d6a9, 37 sites)
    static ISEntity* GetLMouseButton() { return *reinterpret_cast<ISEntity**>(ee_ui_key_slot(EeUiKey::LeftMouse)); }
#endif
    GEBitmap* GetRadioDownOff() const { return reinterpret_cast<GEBitmap*>(this->Call(EeUiManagerCall::Bitmap, 18)); }
    GEBitmap* GetRadioOff() const { return reinterpret_cast<GEBitmap*>(this->Call(EeUiManagerCall::Bitmap, 17)); }
    GEBitmap* GetRadioOn() const { return reinterpret_cast<GEBitmap*>(this->Call(EeUiManagerCall::Bitmap, 19)); }
    GERasterizer* GetRasterizer() const { return reinterpret_cast<GERasterizer*>(this->Call(EeUiManagerCall::Rasterizer)); }
#if defined(LOWLEVELENGINE_EXPORTS) || defined(LLE_CURRENT_HEADERS)   // the exe reads __imp_sRMouseButton inline (27 sites)
    static ISEntity* GetRMouseButton() { return *reinterpret_cast<ISEntity**>(ee_ui_key_slot(EeUiKey::RightMouse)); }
#endif
    GEBitmap* GetSBBitmapDownNormal() const { return reinterpret_cast<GEBitmap*>(this->Call(EeUiManagerCall::Bitmap, 2)); }
    GEBitmap* GetSBBitmapDownPressed() const { return reinterpret_cast<GEBitmap*>(this->Call(EeUiManagerCall::Bitmap, 3)); }
    GEBitmap* GetSBBitmapLeftNormal() const { return reinterpret_cast<GEBitmap*>(this->Call(EeUiManagerCall::Bitmap, 4)); }
    GEBitmap* GetSBBitmapLeftPressed() const { return reinterpret_cast<GEBitmap*>(this->Call(EeUiManagerCall::Bitmap, 5)); }
    GEBitmap* GetSBBitmapRightNormal() const { return reinterpret_cast<GEBitmap*>(this->Call(EeUiManagerCall::Bitmap, 6)); }
    GEBitmap* GetSBBitmapRightPressed() const { return reinterpret_cast<GEBitmap*>(this->Call(EeUiManagerCall::Bitmap, 7)); }
    GEBitmap* GetSBBitmapUpNormal() const { return reinterpret_cast<GEBitmap*>(this->Call(EeUiManagerCall::Bitmap, 8)); }
    GEBitmap* GetSBBitmapUpPressed() const { return reinterpret_cast<GEBitmap*>(this->Call(EeUiManagerCall::Bitmap, 9)); }
    unsigned long GetScreen() const { return (unsigned long)this->Call(EeUiManagerCall::Screen); }
    SESoundManager* GetSoundManager() const { return reinterpret_cast<SESoundManager*>(this->Call(EeUiManagerCall::SoundManager)); }
    UResourceFile* GetStringTable() const { return reinterpret_cast<UResourceFile*>(this->Call(EeUiManagerCall::StringTable)); }
    unsigned long GetUITextureBitDepth() const { return (unsigned long)this->Call(EeUiManagerCall::TextureBitDepth); }
    bool IsFormVisible(UIForm* inForm) { return this->Call(EeUiManagerCall::IsFormVisible, (intptr_t)inForm) != 0; }
    // Loads the UI database (fonts, forms, backgrounds, controls, control and form events, hot keys) from a directory
    void LoadDatabase(FSPath* inPath, const UString& inDirectory) { ee_ui_manager_load_database(this, inPath, &inDirectory); }
    void LockUIForCopyData() { this->Call(EeUiManagerCall::LockForCopy, 0); }
    // Closes the top form, re-shows the forms it covered, redraws its area and activates the form below; returns OK
    long Pop() { return (long)this->Call(EeUiManagerCall::Pop); }
    // Pops forms until the given one is on top, when it is on the stack; returns OK (each Pop does)
    long PopTo(UIForm* inForm) { return (long)this->Call(EeUiManagerCall::PopTo, (intptr_t)inForm); }
    // Opens a form on top of the stack (unless already stacked), deactivating the old top; returns OK
    long Push(UIForm* inForm) { return (long)this->Call(EeUiManagerCall::Push, (intptr_t)inForm); }
    // Marks every visible form and all its controls dirty
    void RedrawAll() { this->Call(EeUiManagerCall::RedrawAll); }
    void RegisterControlEventCallback(UIEvent* (*inCallback)(UIForm*, unsigned long)) { this->Call(EeUiManagerCall::RegisterControlCallback, (intptr_t)inCallback); }
    void RegisterFormEventCallback(UIEvent* (*inCallback)(UIManager*, unsigned long)) { this->Call(EeUiManagerCall::RegisterFormCallback, (intptr_t)inCallback); }
    // Re-lays out every stacked form to the window size and rebuilds the clip rectangles
    void RelayoutAllForms() { this->Call(EeUiManagerCall::RelayoutAllForms); }
    // Swaps the left and right mouse buttons when the left-handed setting changes
    static void SetMouseLeftHanded(bool inLeftHanded) { ee_ui_manager_set_mouse_left_handed(inLeftHanded); }
    // Replaces the top form with another (unless already stacked), re-showing what the old one covered; returns OK
    long SwapForm(UIForm* inForm) { return (long)this->Call(EeUiManagerCall::SwapForm, (intptr_t)inForm); }
    // Sets the music volume; stops the music at 0 and restarts the last track (or the intro) when raised from 0
    void SetMusicVolume(float inVolume) { this->CallPoint(EeUiManagerCall::SetMusicVolume, inVolume, 0.0f); }
    // Loads and starts a music track unless it is the current one (names compared lower-cased)
    void StartMusic(const UWideString& inFileName) { this->Call(EeUiManagerCall::StartMusic, (intptr_t)(const char*)inFileName); }
    void UnlockUIForCopyData() { this->Call(EeUiManagerCall::LockForCopy, 1); }
    // Runs one UI update: keyboard, mouse, queued events, then any pending music stop or start; returns the mouse result
    long UpdateUI(const GE2DPoint& inMouseLocation) { return (long)this->CallPoint(EeUiManagerCall::UpdateUi, inMouseLocation.GetX(), inMouseLocation.GetY()); }
    // Passes a translated character to the top form unless its repeat count is 16 or more; returns its result
    long UpdateTranslatedKey(const char* inKey, short inRepeat) { return (long)this->Call(EeUiManagerCall::UpdateTranslatedKey, (intptr_t)inKey, (intptr_t)inRepeat); }
    // Passes keyboard input to the top form when the keyboard changed, a control holds it, or an update is forced
    virtual long UpdateKeyboard() { return (long)this->Call(EeUiManagerCall::UpdateKeyboard); }
    // Passes the mouse location and buttons to the top form when the mouse changed, it is captured, or an update is forced
    virtual long UpdateMouse(const GE2DPoint& inMouseLocation) { return (long)this->CallPoint(EeUiManagerCall::UpdateMouse, inMouseLocation.GetX(), inMouseLocation.GetY()); } // slot 0x30
    // the Rust manager's calls out that are not Rust's (ee_ui lle_manager.rs: ee_ui_manager_host comes first)
    // the forwarders' one call into the Rust manager (with a rectangle, with two floats)
    intptr_t Call(EeUiManagerCall inCall, intptr_t inA = 0, intptr_t inB = 0, intptr_t inC = 0, float* ioF = NULL) const { return ee_ui_manager_call(this->mRust, inCall, inA, inB, inC, ioF); }
    intptr_t CallRect(EeUiManagerCall inCall, intptr_t inA, const GERectangle& inRectangle) const { return this->Call(inCall, inA, 0, 0, UIF4(inRectangle)); }
    intptr_t CallPoint(EeUiManagerCall inCall, float inX, float inY) const { return this->Call(inCall, 0, 0, 0, UIF4(inX, inY)); }

    URust<EeUiManager> mRust;                     // the state (managerobj.rs)
    // (the original's sMouseLeftHanded is Rust's: ee_ui lle_manager.rs, ee_ui_mouse_left_handed)
};

#endif
