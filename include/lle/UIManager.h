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
#include "rust/ee_ui.h"

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
class UIControl;
class UIControlsTable;
class UIForm;
class UResourceFile;
struct UIDbDataForms;
struct UIIMECandidateList;
class UIEntity;
class UIEvent;
class UIIMECandidate;

// The UI database file names (exported globals; defined in Utilities, 0x1012ec10..)
LLE_API extern UString gDbUIHotKeyFileName;          // 0x1012ec10
LLE_API extern UString gDbUIFontsFileName;           // 0x1012ec20
LLE_API extern UString gDbUIFormEventsFileName;      // 0x1012ec30
LLE_API extern UString gDbUIControlEventsFileName;   // 0x1012ec40
LLE_API extern UString gDbUIFormsFileName;           // 0x1012ec50
LLE_API extern UString gDbUIControlsFileName;        // 0x1012ec60
LLE_API extern UString gDbUIBackgroundFileName;      // 0x1012ec70

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
    virtual long ChangeIMECandidateWindow(const UIIMECandidateList& inList);   // slot 0x04
    virtual long ChangeIMECompositionString(const UWideString& inString, const UString& inAttributes,
                                            unsigned long inCursor);
    void ChangeRasterizer(GERasterizer* inRasterizer);
    virtual long CloseIMECandidateWindow();
    virtual long CloseIMECompositionString();
    long CopyData();
    void CreateBackgrounds();
    GEBitmap* CreateBitmapForScaledControl(GEBitmap* inBitmap, const GERectangle& inRectangle);
    UIEvent* CreateControlEvent(UIForm* inForm, unsigned long inEventID);
    UIEvent* CreateFormEvent(unsigned long inEventID);
    virtual UIBackground* CreateUserDefinedBackground(const UIBackgroundTable& inTable) { return NULL; }
    virtual UIControl* CreateUserDefinedControl(UIControlsTable& inTable) { return NULL; }
    // inline: exported 0x1005a028 (UIBackground.cpp's inline block)
    virtual UIForm* CreateForm(const UIDbDataForms& inData) { return new UIForm(this, inData, true); }
    long DrawPolygons();
    long DrawTextA();
    void ForceUIUpdate();
    GEBitmap* GetAlphaSortDownBitmap() const;
    GEBitmap* GetAlphaSortUpBitmap() const;
    UIIMECandidate* GetCandidateWindow() const;
    GEBitmap* GetCheckDownNot() const;
    GEBitmap* GetCheckDownOff() const;
    GEBitmap* GetCheckDownOn() const;
    GEBitmap* GetCheckNot() const;
    GEBitmap* GetCheckOff() const;
    GEBitmap* GetCheckOn() const;
    UIForm* GetCurrentForm() const;
    bool GetDrawCandidateWindow() const;
    GEFont* GetFont(unsigned long inIndex) const;
    UIForm* GetFormByID(unsigned long inID) const;
    unsigned long GetFormCount() const;
    UIForm* GetStackedForm(unsigned long inIndex) const;   // port (test fixture)
    GEBitmap* GetDragBarBitmap() const;
    GESurface* GetDrawingSurface() const;
#if defined(LOWLEVELENGINE_EXPORTS) || defined(LLE_CURRENT_HEADERS)   // the exe reads __imp_sLMouseButton inline (0x17d6a9, 37 sites)
    static ISEntity* GetLMouseButton() { return UIManager::sLMouseButton; }
#endif
#ifdef LOWLEVELENGINE_EXPORTS
#if defined(LLE_UIFORM_CPP) || defined(LLE_UIMANAGER_CPP) || defined(LLE_UIBACKGROUND_CPP)
    static ISEntity* GetMouseWheel() { return UIManager::sMouseWheel; }
#endif
#endif
    static long GetMouseWheelScrollAmount();
    unsigned long GetNumberOfSurfaces() const;
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
    unsigned long GetScreen() const;
    SESoundManager* GetSoundManager() const;
    UResourceFile* GetStringTable() const;
    unsigned long GetUITextureBitDepth() const;
    bool IsFormOnStack(UIForm* inForm);
    bool IsFormVisible(UIForm* inForm);
    void LoadDatabase(FSPath* inPath, const UString& inDirectory);
    void LockUIForCopyData();
    virtual long OpenIMECandidateWindow();
    virtual long OpenIMECompositionString();
    long Pop();
    long PopTo(UIForm* inForm);
    void ProcessAllEventsForForm(UIForm* inForm);
    long Push(UIForm* inForm);
    void RedrawAll();
    void RegisterControlEventCallback(UIEvent* (*inCallback)(UIForm*, unsigned long));
    void RegisterFormEventCallback(UIEvent* (*inCallback)(UIManager*, unsigned long));
    void RelayoutAllForms();
    void RemoveDatabase();
    void SetFont(unsigned long inIndex, GEFont* inFont);
    static void SetMouseLeftHanded(bool inLeftHanded);
    long SwapForm(UIForm* inForm);
    void SetMusicVolume(float inVolume);
    void SetNumberOfFonts(unsigned long inCount);
    void StartMusic(const UWideString& inFileName);
    void StopMusic();
    void UnlockUIForCopyData();
    virtual long UpdateUI(const GE2DPoint& inMouseLocation);   // slot 0x28
    long UpdateTranslatedKey(const char* inKey, short inRepeat);
protected:
    virtual long UpdateKeyboard();
    virtual long UpdateMouse(const GE2DPoint& inMouseLocation);   // slot 0x30
    void RemoveAndStopSoundInstance();
    void RemoveSoundInstance();
    void SetSoundInstance(SESMInstanceNode* inInstance);
private:
    void ResetClippedForms();
    // the forwarders' one call into the Rust manager, and the Rust manager's calls out
    intptr_t Call(EeUiManagerCall inCall, intptr_t inA = 0, intptr_t inB = 0, intptr_t inC = 0, float* ioF = NULL) const;
    static intptr_t HostOp(void* inManager, EeUiOp inOp, intptr_t inA, intptr_t inB, intptr_t inC, intptr_t inD,
                           float* ioF);

    EeUiManager* mRust;                     // the state (managerobj.rs)
#if defined(LOWLEVELENGINE_EXPORTS) || defined(LLE_CURRENT_HEADERS)
    // Inferred guard (see LowLevelEngine.h: a static data member costs every includer a $E number); the exe
    // imports both
    static ISEntity* sLMouseButton;         // 0x1012e9a8
    static ISEntity* sRMouseButton;         // 0x1012e9ac
#endif
#ifdef LOWLEVELENGINE_EXPORTS
#if defined(LLE_UIFORM_CPP) || defined(LLE_UIMANAGER_CPP) || defined(LLE_UIBACKGROUND_CPP)
    // Inferred: visible to its users only (shown to UIControl.cpp it renumbers that unit's $E chains)
    static ISEntity* sMouseWheel;           // 0x1012e9b0
    static bool sMouseLeftHanded;           // 0x1012e9b4
#endif
#endif
};

#endif
