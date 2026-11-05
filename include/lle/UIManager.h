// C:\EEX\Low-Level Engine\User Interface\UIManager.h (RECONSTRUCTION). The UI root: the forms and the form stack,
// the visible forms, the queued form events, the shared control bitmaps, the music, and the rasterizer/sound/
// string-table handles. UIManager.cpp (0x10078c81..0x1007ba37, __FILE__ asserts) holds the ctor and the out-of-line
// members; the inline members are emitted first by UIBackground.cpp (0x10059fdb..0x1005a269, declaration order).
// Vtable 0x10098b70 (13 slots, exported names). sizeof 0x12c (??_E stride, 0x1005a70d). Layout proven by the ctor,
// the dtor and the exported getters. Requires lle/UIForm.h (CreateForm), lle/GERasterizer.h, lle/TSCriticalSection.h.
#ifndef LLE_UIMANAGER_H
#define LLE_UIMANAGER_H

#include <vector>
#include <deque>
#include <algorithm>

#include "lle/UIEvent.h"

class FSPath;
class GEBitmap;
class GEFont;
class GERasterizer;
class GETexture;
class GETextureCoordinate;
class ISEntity;
class GESurface;
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

// One queued event trigger (provisional name; 12-byte deque element, AddEventToTrigger 0x10059fdb)
struct UIEventTrigger {
    UIEvent* mEvent;                        // +0x00 (referenced)
    UIForm* mForm;                          // +0x04
    UIEntity* mEntity;                      // +0x08
};

// winspool.h maps AddForm to AddFormA; the export is ?AddForm@UIManager@@
#ifdef AddForm
#undef AddForm
#endif

class LLE_API UIManager {
public:
    UIManager(GERasterizer* inRasterizer, SESoundManager* inSoundManager, unsigned long inNumberOfSurfaces,
              unsigned long inUITextureBitDepth, UResourceFile* inStringTable, unsigned long inScreen);
    virtual ~UIManager();
    void AddDirtyRectangle(const GERectangle& inRectangle, UIForm* inForm);
    void AddEventToTrigger(UIEvent* inEvent, UIForm* inForm, UIEntity* inEntity)
    {
        UIEventTrigger theTrigger;
        theTrigger.mEvent = inEvent;
        theTrigger.mEntity = inEntity;
        theTrigger.mForm = inForm;
        inEvent->Reference();
        this->mEventsToTrigger.push_back(theTrigger);
    }
    void AddForm(UIForm* inForm) { this->mForms.push_back(inForm); }  // 0x1005a006; UIForm::Create expands it
    void AddIMEResultString(const UWideString& inString);
    virtual long ChangeIMECandidateWindow(const UIIMECandidateList& inList);   // slot 0x04
    virtual long ChangeIMECompositionString(const UWideString& inString, const UString& inAttributes,
                                            unsigned long inCursor);
    void ChangeRasterizer(GERasterizer* inRasterizer) { this->mRasterizer = inRasterizer; }
    virtual long CloseIMECandidateWindow();
    virtual long CloseIMECompositionString();
    long CopyData();
    void CreateBackgrounds();
    GEBitmap* CreateBitmapForScaledControl(GEBitmap* inBitmap, const GERectangle& inRectangle);
    UIEvent* CreateControlEvent(UIForm* inForm, unsigned long inEventID);
    UIEvent* CreateFormEvent(unsigned long inEventID);
    void CreateIMECandidateWindow(GEFont* inFont);
    virtual UIBackground* CreateUserDefinedBackground(const UIBackgroundTable& inTable) { return NULL; }
    virtual UIControl* CreateUserDefinedControl(UIControlsTable& inTable) { return NULL; }
    // inline: exported 0x1005a028 (UIBackground.cpp's inline block)
    virtual UIForm* CreateForm(const UIDbDataForms& inData) { return new UIForm(this, inData, true); }
    long DrawPolygons();
    long DrawTextA();
    void ForceUIUpdate() { this->mForceUIUpdate = true; }
    GEBitmap* GetAlphaSortDownBitmap() const { return this->mAlphaSortDownBitmap; }  // ICF-folded (0x10045c66)
    GEBitmap* GetAlphaSortUpBitmap() const { return this->mAlphaSortUpBitmap; }
    UIIMECandidate* GetCandidateWindow() const { return this->mCandidateWindow; }  // ICF-folded (0x100660be)
    GEBitmap* GetCheckDownNot() const { return this->mCheckDownNot; }  // ICF-folded (0x1006608d)
    GEBitmap* GetCheckDownOff() const { return this->mCheckDownOff; }
    GEBitmap* GetCheckDownOn() const { return this->mCheckDownOn; }  // ICF-folded (0x1006609b)
    GEBitmap* GetCheckNot() const { return this->mCheckNot; }     // ICF-folded (0x100660a2)
    GEBitmap* GetCheckOff() const { return this->mCheckOff; }
    GEBitmap* GetCheckOn() const { return this->mCheckOn; }       // ICF-folded (0x10066094)
    UIForm* GetCurrentForm() const
    {
        if (this->mFormStack.size() > 0)
            return this->mFormStack.back();
        return NULL;
    }
    bool GetDrawCandidateWindow() const { return this->mDrawCandidateWindow; }
    GEFont* GetFont(unsigned long inIndex) const { return this->mFonts[inIndex]; }
    UIForm* GetFormByID(unsigned long inID) const;
    unsigned long GetFormCount() const { return this->mFormStack.size(); }
    UIForm* GetStackedForm(unsigned long inIndex) const { return this->mFormStack[inIndex]; }   // port (test fixture)
    GEBitmap* GetDragBarBitmap() const { return this->mDragBarBitmap; }
    GESurface* GetDrawingSurface() const { return this->mRasterizer->GetSecondarySurface(this->mScreen); }
#if defined(LOWLEVELENGINE_EXPORTS) || defined(LLE_CURRENT_HEADERS)   // the exe reads __imp_sLMouseButton inline (0x17d6a9, 37 sites)
    static ISEntity* GetLMouseButton() { return UIManager::sLMouseButton; }
#endif
#ifdef LOWLEVELENGINE_EXPORTS
#if defined(LLE_UIFORM_CPP) || defined(LLE_UIMANAGER_CPP) || defined(LLE_UIBACKGROUND_CPP)
    static bool GetMouseLeftHanded() { return UIManager::sMouseLeftHanded; }
    static ISEntity* GetMouseWheel() { return UIManager::sMouseWheel; }
#endif
#endif
    static long GetMouseWheelScrollAmount();
    unsigned long GetNumberOfSurfaces() const { return this->mNumberOfSurfaces; }
    GEBitmap* GetRadioDownOff() const { return this->mRadioDownOff; }
    GEBitmap* GetRadioOff() const { return this->mRadioOff; }
    GEBitmap* GetRadioDownOn() const { return this->mRadioDownOn; }
    GEBitmap* GetRadioOn() const { return this->mRadioOn; }
    GERasterizer* GetRasterizer() const { return this->mRasterizer; }
#if defined(LOWLEVELENGINE_EXPORTS) || defined(LLE_CURRENT_HEADERS)   // the exe reads __imp_sRMouseButton inline (27 sites)
    static ISEntity* GetRMouseButton() { return UIManager::sRMouseButton; }
#endif
    GEBitmap* GetSBBitmapDownNormal() const { return this->mSBBitmapDownNormal; }
    GEBitmap* GetSBBitmapDownPressed() const { return this->mSBBitmapDownPressed; }
    GEBitmap* GetSBBitmapLeftNormal() const { return this->mSBBitmapLeftNormal; }
    GEBitmap* GetSBBitmapLeftPressed() const { return this->mSBBitmapLeftPressed; }
    GEBitmap* GetSBBitmapRightNormal() const { return this->mSBBitmapRightNormal; }
    GEBitmap* GetSBBitmapRightPressed() const { return this->mSBBitmapRightPressed; }  // ICF-folded (0x10061152)
    GEBitmap* GetSBBitmapUpNormal() const { return this->mSBBitmapUpNormal; }
    GEBitmap* GetSBBitmapUpPressed() const { return this->mSBBitmapUpPressed; }  // ICF-folded (0x10045c5f)
    unsigned long GetScreen() const { return this->mScreen; }
    SESoundManager* GetSoundManager() const { return this->mSoundManager; }
    UResourceFile* GetStringTable() const { return this->mStringTable; }
    unsigned long GetUITextureBitDepth() const { return this->mUITextureBitDepth; }  // ICF-folded (0x10065fe4)
    unsigned long GetVisibleFormCount() const { return this->mVisibleForms.size(); }
    bool IsFormOnStack(UIForm* inForm)
    {
        return std::find(this->mFormStack.begin(), this->mFormStack.end(), inForm) != this->mFormStack.end();
    }
    bool IsFormVisible(UIForm* inForm)
    {
        return std::find(this->mVisibleForms.begin(), this->mVisibleForms.end(), inForm) != this->mVisibleForms.end();
    }
    void LoadDatabase(FSPath* inPath, const UString& inDirectory);
    void LockUIForCopyData() { this->mCriticalSection->Enter(); }
    virtual long OpenIMECandidateWindow();
    virtual long OpenIMECompositionString();
    long Pop();
    long PopTo(UIForm* inForm);
    void ProcessAllEventsForForm(UIForm* inForm);
    long Push(UIForm* inForm);
    void RedrawAll();
    void RegisterControlEventCallback(UIEvent* (*inCallback)(UIForm*, unsigned long))
    {
        UIEvent* (*theCallback)(UIForm*, unsigned long) = inCallback;
        mControlEventCallbacks.push_back(theCallback);
    }
    void RegisterFormEventCallback(UIEvent* (*inCallback)(UIManager*, unsigned long))
    {
        UIEvent* (*theCallback)(UIManager*, unsigned long) = inCallback;
        mFormEventCallbacks.push_back(theCallback);
    }
    void RelayoutAllForms();
    void RemoveDatabase();
    void SetFont(unsigned long inIndex, GEFont* inFont) { this->mFonts[inIndex] = inFont; }
    static void SetMouseLeftHanded(bool inLeftHanded);
    long SwapForm(UIForm* inForm);
    void SetMusicVolume(float inVolume);
    void SetNumberOfFonts(unsigned long inCount) { this->mFonts.resize(inCount); }
    void StartMusic(const UWideString& inFileName);
    void StopMusic();
    void UnlockUIForCopyData() { this->mCriticalSection->Leave(); }
    virtual long UpdateUI(const GE2DPoint& inMouseLocation);   // slot 0x28
    long UpdateTranslatedKey(const char* inKey, short inRepeat);
protected:
    virtual long UpdateKeyboard();
    virtual long UpdateMouse(const GE2DPoint& inMouseLocation);   // slot 0x30
    void RemoveAndStopSoundInstance();
    void RemoveSoundInstance();
    void SetSoundInstance(SESMInstanceNode* inInstance);
private:
    void AddClippedForms(unsigned long inIndex);
    unsigned long AddToVisibleForms(UIForm* inForm);
    void CreateBitmap(GEBitmap* inBitmap, GETextureCoordinate& inTopLeft, GETextureCoordinate& inTopRight,
                      GETextureCoordinate& inBottomRight, GETextureCoordinate& inBottomLeft);
#ifdef __clang__
    // VC6 bound temporaries to the non-const reference parameters above; clang does not. These const overloads
    // forward to them (clang build only; remove once the parameters themselves are const).
    void CreateBitmap(GEBitmap* inBitmap, const GETextureCoordinate& inTopLeft, const GETextureCoordinate& inTopRight,
                      const GETextureCoordinate& inBottomRight, const GETextureCoordinate& inBottomLeft)
        { CreateBitmap(inBitmap, const_cast<GETextureCoordinate&>(inTopLeft), const_cast<GETextureCoordinate&>(inTopRight), const_cast<GETextureCoordinate&>(inBottomRight), const_cast<GETextureCoordinate&>(inBottomLeft)); }
#endif
    void LoadTexture(const UString& inName, GETexture*& outTexture);
    void RemoveClippedForms(unsigned long inIndex);
    void ResetClippedForms();
protected:
    TSCriticalSection* mCriticalSection;    // +0x004 forms, stack and events (LockUIForCopyData)
    TSCriticalSection* mDrawCriticalSection;   // +0x008 the copied forms
    std::vector<UIEvent* (*)(UIManager*, unsigned long)> mFormEventCallbacks;   // +0x00c
    std::vector<UIEvent* (*)(UIForm*, unsigned long)> mControlEventCallbacks;   // +0x01c
    std::vector<UIForm*> mFormStack;        // +0x02c
    std::vector<UIForm*> mForms;            // +0x03c every form (AddForm)
    std::vector<UIForm*> mVisibleForms;     // +0x04c
    std::deque<UIEventTrigger> mEventsToTrigger;   // +0x05c
    std::vector<UIForm*> mCopiedForms;      // +0x070 the visible forms as of CopyData (drawn)
    std::vector<GEFont*> mFonts;            // +0x080
    UWideString mMusicFileName;             // +0x090
    GERasterizer* mRasterizer;              // +0x0a4
    SESoundManager* mSoundManager;          // +0x0a8
    GETexture* mUITexture;                  // +0x0ac textures\UIControls
    GEBitmap* mAlphaSortUpBitmap;           // +0x0b0
    GEBitmap* mAlphaSortDownBitmap;         // +0x0b4
    GEBitmap* mSBBitmapDownNormal;          // +0x0b8
    GEBitmap* mSBBitmapDownPressed;         // +0x0bc
    GEBitmap* mSBBitmapLeftNormal;          // +0x0c0
    GEBitmap* mSBBitmapLeftPressed;         // +0x0c4
    GEBitmap* mSBBitmapRightNormal;         // +0x0c8
    GEBitmap* mSBBitmapRightPressed;        // +0x0cc
    GEBitmap* mSBBitmapUpNormal;            // +0x0d0
    GEBitmap* mSBBitmapUpPressed;           // +0x0d4
    GEBitmap* mDragBarBitmap;               // +0x0d8
    GEBitmap* mCheckOff;                    // +0x0dc
    GEBitmap* mCheckDownOff;                // +0x0e0
    GEBitmap* mCheckOn;                     // +0x0e4
    GEBitmap* mCheckDownOn;                 // +0x0e8
    GEBitmap* mCheckNot;                    // +0x0ec
    GEBitmap* mCheckDownNot;                // +0x0f0
    GEBitmap* mRadioOff;                    // +0x0f4
    GEBitmap* mRadioDownOff;                // +0x0f8
    GEBitmap* mRadioOn;                     // +0x0fc
    GEBitmap* mRadioDownOn;                 // +0x100
    UResourceFile* mStringTable;            // +0x104
    UIIMECandidate* mCandidateWindow;       // +0x108
    long mMusicID;                          // +0x10c SESoundManager sound id (-1: none)
    SESMInstanceNode* mMusicInstance;       // +0x110
    float mMusicVolume;                     // +0x114
    unsigned long mNumberOfSurfaces;        // +0x118
    unsigned long mScreen;                  // +0x11c
    unsigned long mUITextureBitDepth;       // +0x120
    bool mCandidateWindowOpen;              // +0x124
    bool mDrawCandidateWindow;              // +0x125 mCandidateWindowOpen as of CopyData
    bool mStopMusic;                        // +0x126 (UpdateUI)
    bool mStartMusic;                       // +0x127 (UpdateUI)
    bool mForceUIUpdate;                    // +0x128
#if defined(LOWLEVELENGINE_EXPORTS) || defined(LLE_CURRENT_HEADERS)
    // Inferred guard (see LowLevelEngine.h: a static data member costs every includer a $E number); the exe
    // imports both
private:
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
