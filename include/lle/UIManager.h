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
#include "rust/ui/managerobj.h"
#include "rust/ui/obj.h"

class FSPath;
class GEBitmap;
class GEFont;
class GERasterizer;
class GESurface;
class ISEntity;
class SESMInstanceNode;
class SESoundManager;
class UIBackground;
class UIBackgroundTable;
class TSCriticalSection;
class UIControl;
class UIControlsTable;
class UIForm;
class UResourceFile;
struct UIDbDataForms;
class UIEntity;
class UIEvent;

// The UI database file names (exported globals; defined in Utilities, 0x1012ec10..)
LLE_API extern UString gDbUIHotKeyFileName;
LLE_API extern UString gDbUIFontsFileName;
LLE_API extern UString gDbUIFormEventsFileName;
LLE_API extern UString gDbUIControlEventsFileName;
LLE_API extern UString gDbUIFormsFileName;
LLE_API extern UString gDbUIControlsFileName;
LLE_API extern UString gDbUIBackgroundFileName;

// winspool.h maps AddForm to AddFormA; the export is ?AddForm@UIManager@@
#ifdef AddForm
#undef AddForm
#endif

enum class EeUiOp : uint32_t;              // rust/ee_ui.h: the Rust manager's calls out
enum class EeUiManagerCall : uint32_t;     // rust/ee_ui.h: its methods
struct EeUiManager;                        // rust/ee_ui.h: the Rust manager (managerobj.rs)

// The class is whole in Rust (rust/crates/ee_ui/src/managerobj.rs, R6 ui round 17): its state (the event factories,
// the stack, the forms, the visible and copied forms, the queued events, the fonts, the music, the UI texture and the
// control bitmaps, the candidate window, the locks, the handles, the counts and flags) and every method but the
// database's are the Rust object mRust; UIManager.cpp holds the forwarders, the ctor and the dtor, the database's
// LoadDatabase, CreateBackgrounds and RemoveDatabase, the mouse buttons' statics and the host (the op switch,
// HostOp: a member, so that it has the friendship UIForm gives UIManager). EEUIManager derives from it and overrides
// UpdateKeyboard and UpdateMouse; the Rust UpdateUI calls them through the C++ object.
class LLE_API UIManager {
public:
    UIManager(GERasterizer* inRasterizer, SESoundManager* inSoundManager, unsigned long inNumberOfSurfaces,
              unsigned long inUITextureBitDepth, UResourceFile* inStringTable, unsigned long inScreen);
    UIManager(const UIManager&) = delete;   // holds a Rust object; nothing copied a manager
    UIManager& operator=(const UIManager&) = delete;
    virtual ~UIManager();
    void AddDirtyRectangle(const GERectangle& inRectangle, UIForm* inForm);
    void AddEventToTrigger(UIEvent* inEvent, UIForm* inForm, UIEntity* inEntity);
    void AddForm(UIForm* inForm);           // 0x1005a006; UIForm::Create calls it
    void AddIMEResultString(const UWideString& inString);
    virtual long ChangeIMECompositionString(const UWideString& inString, const UString& inAttributes,
                                            unsigned long inCursor);
    void ChangeRasterizer(GERasterizer* inRasterizer);
    // Tells the top form the IME composition string closed, under the UI lock; returns its result
    virtual long CloseIMECompositionString() { return (long)this->Call(EeUiManagerCall::CloseComposition); }
    // Snapshots every visible form (and the candidate window when open) for drawing; returns the last copy result
    long CopyData() { return (long)this->Call(EeUiManagerCall::CopyData); }
    void CreateBackgrounds();
    GEBitmap* CreateBitmapForScaledControl(GEBitmap* inBitmap, const GERectangle& inRectangle);
    UIEvent* CreateControlEvent(UIForm* inForm, unsigned long inEventID);
    UIEvent* CreateFormEvent(unsigned long inEventID);
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
    GEBitmap* GetAlphaSortDownBitmap() const;
    GEBitmap* GetAlphaSortUpBitmap() const;
    GEBitmap* GetCheckDownNot() const;
    GEBitmap* GetCheckDownOff() const;
    GEBitmap* GetCheckDownOn() const;
    GEBitmap* GetCheckNot() const;
    GEBitmap* GetCheckOff() const;
    GEBitmap* GetCheckOn() const;
    UIForm* GetCurrentForm() const { return reinterpret_cast<UIForm*>(this->Call(EeUiManagerCall::CurrentForm)); }
    GEFont* GetFont(unsigned long inIndex) const;
    UIForm* GetFormByID(unsigned long inID) const;
    unsigned long GetFormCount() const { return (unsigned long)this->Call(EeUiManagerCall::FormCount); }
    UIForm* GetStackedForm(unsigned long inIndex) const;   // port (test fixture)
    TSCriticalSection* GetUILock() const;                  // port (the harness's bridge: the lock UpdateUI holds)
    GEBitmap* GetDragBarBitmap() const;
    // the bitmap by its index (EeUiManagerCall::Bitmap: GetAlphaSortUpBitmap 0 .. GetRadioDownOn 20)
    GEBitmap* GetBitmap(unsigned long inIndex) const { return reinterpret_cast<GEBitmap*>(this->Call(EeUiManagerCall::Bitmap, (intptr_t)inIndex)); }
#if defined(LOWLEVELENGINE_EXPORTS) || defined(LLE_CURRENT_HEADERS)   // the exe reads __imp_sLMouseButton inline (0x17d6a9, 37 sites)
    static ISEntity* GetLMouseButton() { return UIManager::sLMouseButton; }
#endif
#ifdef LOWLEVELENGINE_EXPORTS
#if defined(LLE_UIFORM_CPP) || defined(LLE_UIMANAGER_CPP) || defined(LLE_UIBACKGROUND_CPP)
    static ISEntity* GetMouseWheel() { return UIManager::sMouseWheel; }
#endif
#endif
    static long GetMouseWheelScrollAmount();
    unsigned long GetNumberOfSurfaces() const { return (unsigned long)this->Call(EeUiManagerCall::NumberOfSurfaces); }
    GEBitmap* GetRadioDownOff() const;
    GEBitmap* GetRadioOff() const;
    GEBitmap* GetRadioDownOn() const;
    GEBitmap* GetRadioOn() const;
    GERasterizer* GetRasterizer() const;
#if defined(LOWLEVELENGINE_EXPORTS) || defined(LLE_CURRENT_HEADERS)   // the exe reads __imp_sRMouseButton inline (27 sites)
    static ISEntity* GetRMouseButton() { return UIManager::sRMouseButton; }
#endif
    GEBitmap* GetSBBitmapDownNormal() const;
    GEBitmap* GetSBBitmapDownPressed() const;
    GEBitmap* GetSBBitmapLeftNormal() const;
    GEBitmap* GetSBBitmapLeftPressed() const;
    GEBitmap* GetSBBitmapRightNormal() const;
    GEBitmap* GetSBBitmapRightPressed() const;
    GEBitmap* GetSBBitmapUpNormal() const;
    GEBitmap* GetSBBitmapUpPressed() const;
    unsigned long GetScreen() const { return (unsigned long)this->Call(EeUiManagerCall::Screen); }
    SESoundManager* GetSoundManager() const;
    UResourceFile* GetStringTable() const;
    unsigned long GetUITextureBitDepth() const { return (unsigned long)this->Call(EeUiManagerCall::TextureBitDepth); }
    bool IsFormVisible(UIForm* inForm) { return this->Call(EeUiManagerCall::IsFormVisible, (intptr_t)inForm) != 0; }
    void LoadDatabase(FSPath* inPath, const UString& inDirectory);
    void LockUIForCopyData() { this->Call(EeUiManagerCall::LockForCopy, 0); }
    // Tells the top form an IME composition string opened, under the UI lock; returns its result
    virtual long OpenIMECompositionString() { return (long)this->Call(EeUiManagerCall::OpenComposition); }
    // Closes the top form, re-shows the forms it covered, redraws its area and activates the form below; returns OK
    long Pop() { return (long)this->Call(EeUiManagerCall::Pop); }
    // Pops forms until the given one is on top, when it is on the stack; returns OK (each Pop does)
    long PopTo(UIForm* inForm) { return (long)this->Call(EeUiManagerCall::PopTo, (intptr_t)inForm); }
    void ProcessAllEventsForForm(UIForm* inForm);
    // Opens a form on top of the stack (unless already stacked), deactivating the old top; returns OK
    long Push(UIForm* inForm) { return (long)this->Call(EeUiManagerCall::Push, (intptr_t)inForm); }
    // Marks every visible form and all its controls dirty
    void RedrawAll() { this->Call(EeUiManagerCall::RedrawAll); }
    void RegisterControlEventCallback(UIEvent* (*inCallback)(UIForm*, unsigned long));
    void RegisterFormEventCallback(UIEvent* (*inCallback)(UIManager*, unsigned long));
    // Re-lays out every stacked form to the window size and rebuilds the clip rectangles
    void RelayoutAllForms() { this->Call(EeUiManagerCall::RelayoutAllForms); }
    void RemoveDatabase();
    void SetFont(unsigned long inIndex, GEFont* inFont);
    static void SetMouseLeftHanded(bool inLeftHanded);
    // Replaces the top form with another (unless already stacked), re-showing what the old one covered; returns OK
    long SwapForm(UIForm* inForm) { return (long)this->Call(EeUiManagerCall::SwapForm, (intptr_t)inForm); }
    void SetMusicVolume(float inVolume);
    void StartMusic(const UWideString& inFileName);
    // Stops the music and unloads the track
    void StopMusic() { this->Call(EeUiManagerCall::StopMusic); }
    void UnlockUIForCopyData() { this->Call(EeUiManagerCall::LockForCopy, 1); }
    virtual long UpdateUI(const GE2DPoint& inMouseLocation);   // slot 0x28
    long UpdateTranslatedKey(const char* inKey, short inRepeat);
protected:
    // Passes keyboard input to the top form when the keyboard changed, a control holds it, or an update is forced
    virtual long UpdateKeyboard() { return (long)this->Call(EeUiManagerCall::UpdateKeyboard); }
    virtual long UpdateMouse(const GE2DPoint& inMouseLocation);   // slot 0x30
public:
    // the Rust manager's calls out that are not Rust's (ee_ui lle_manager.rs: ee_ui_manager_host comes first)
    static intptr_t HostOp(void* inManager, EeUiOp inOp, intptr_t inA, intptr_t inB, intptr_t inC, intptr_t inD,
                           float* ioF);
private:
    // the forwarders' one call into the Rust manager (with a rectangle, with two floats)
    intptr_t Call(EeUiManagerCall inCall, intptr_t inA = 0, intptr_t inB = 0, intptr_t inC = 0, float* ioF = NULL) const;
    intptr_t CallRect(EeUiManagerCall inCall, intptr_t inA, const GERectangle& inRectangle) const;
    intptr_t CallPoint(EeUiManagerCall inCall, float inX, float inY) const;

    EeUiManager* mRust;                     // the state (managerobj.rs)
public:   // the mouse buttons' shims (UIManager.cpp)
#if defined(LOWLEVELENGINE_EXPORTS) || defined(LLE_CURRENT_HEADERS)
    // Inferred guard (see LowLevelEngine.h: a static data member costs every includer a $E number); the exe
    // imports both
    static ISEntity* sLMouseButton;
    static ISEntity* sRMouseButton;
#endif
#ifdef LOWLEVELENGINE_EXPORTS
#if defined(LLE_UIFORM_CPP) || defined(LLE_UIMANAGER_CPP) || defined(LLE_UIBACKGROUND_CPP)
    // Inferred: visible to its users only (shown to UIControl.cpp it renumbers that unit's $E chains)
    static ISEntity* sMouseWheel;
    static bool sMouseLeftHanded;
#endif
#endif
};

#endif
