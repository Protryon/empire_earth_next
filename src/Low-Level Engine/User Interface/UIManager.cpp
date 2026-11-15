// C:\EEX\Low-Level Engine\User Interface\UIManager.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10078c81..0x1007ba37: the U2D key init chains, the implicit copy members and ??_E of UIStretchedTiledBackground
// and UITBTiledBackground (UIManager.cpp was their first includer; both are UITiledBackground's tilings now), the
// ctor, the dtor and the out-of-line members in
// the original's order, then the vector/deque COMDATs. UIMovie.cpp starts at 0x1007ba37.
#define LLE_UIMANAGER_CPP   // UIManager::sMouseWheel/sMouseLeftHanded are visible here (see UIManager.h)
#include <windows.h>
#include <mbstring.h>
#include <string.h>

#include "rust/ee_ui.h"
#include "lle/LowLevelEngine.h"
#include "EEDataText.h"   // EEDbFindFile: the db tables' text forms
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/U2DRectangle.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIBackgroundTable.h"
#include "lle/UITextureBackground.h"
#include "lle/UIBFull.h"
#include "lle/UIBitmapBackground.h"
#include "lle/UITiledBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIControlsTable.h"
#include "lle/UIManager.h"

ISEntity* UIManager::sLMouseButton;
ISEntity* UIManager::sRMouseButton;
ISEntity* UIManager::sMouseWheel;
bool UIManager::sMouseLeftHanded;

// The Rust manager's calls out (EeUiOp, obj.rs): a is the form, event, bitmap, texture, lock, sound instance or
// other object the op names; rectangles and points travel in f
static GERectangle UIManagerRect(const float* inF) { return GERectangle(inF[1], inF[3], inF[2], inF[0]); }
static void UIManagerPutRect(float* outF, const GERectangle& inRectangle)
{
    outF[0] = inRectangle.mBottom;
    outF[1] = inRectangle.mLeft;
    outF[2] = inRectangle.mRight;
    outF[3] = inRectangle.mTop;
}
static float UIManagerBits(intptr_t inBits, int inHalf)
{
    uint32_t theBits = (uint32_t)((uint64_t)inBits >> (32 * inHalf));
    float theValue;
    memcpy(&theValue, &theBits, 4);
    return theValue;
}

// Loads a UI texture with the UI bit depth, alpha, mipmaps and clamped, modulated stages; keeps a reference
// (FUNCTION: LLE 0x1007ab03, UIManager::LoadTexture)
static GETexture* UIManagerLoadTexture(UIManager* inManager, const UString& inName)
{
    GETextureManager* theManager = inManager->GetRasterizer()->GetScreen(inManager->GetScreen())->GetTextureManager();
    GETextureDescription theDescription;
    theDescription.mNumMipLevelsToDrop = 0;
    theDescription.mFrameFormat = 0;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mSurfaceVolatility = 1;
    theDescription.mPixelDepth = inManager->GetUITextureBitDepth();
    theDescription.mHasAlpha = true;
    theDescription.mCreateMIPMaps = true;
    GETexture* theTexture = theManager->LoadTexture(inName, theDescription);
    theTexture->Reference();
    theTexture->SetStageAlphaMode(kStageOpModulate, -1);
    theTexture->SetStageColorMode(kStageOpModulate, -1);
    theTexture->SetStageMinFilterMode(1, -1);
    theTexture->SetStageMagFilterMode(1, -1);
    theTexture->SetStageMipFilterMode(kMipFilterPoint, -1);
    theTexture->SetStageMipMapBias(-0.75f, -1);
    theTexture->SetStageTextureAddressingMode(kAddressingClamp, -1);
    return theTexture;
}

// ChangeIMECompositionString's arguments, through the Rust IME call
struct UIManagerComposition {
    const UWideString* mString;
    const UString* mAttributes;
    unsigned long mCursor;
};

intptr_t UIManager::Call(EeUiManagerCall inCall, intptr_t inA, intptr_t inB, intptr_t inC, float* ioF) const
{
    return ee_ui_manager_call(this->mRust, inCall, inA, inB, inC, ioF);
}

intptr_t UIManager::HostOp(void* inManager, EeUiOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d, float* f)
{
    static UWideString sKey;   // UpdateTranslatedKey's character (the UI thread's)
    UIManager* theManager = static_cast<UIManager*>(inManager);
    UIForm* theForm = reinterpret_cast<UIForm*>(a);
    GEBitmap* theBitmap = reinterpret_cast<GEBitmap*>(a);
    UIIMECandidate* theCandidate = reinterpret_cast<UIIMECandidate*>(a);
    SESMInstanceNode* theInstance = reinterpret_cast<SESMInstanceNode*>(a);
    UIEvent* theEvent = reinterpret_cast<UIEvent*>(a);
    switch (inOp) {
    // the forms
    case EeUiOp::MgrFormRect: UIManagerPutRect(f, theForm->GetRectangle()); return 0;
    case EeUiOp::MgrFormId: return theForm->GetFormID();
    case EeUiOp::MgrFormUseGdi: return theForm->GetUseGDIForText();
    case EeUiOp::MgrFormKeyboard: return (intptr_t)theForm->GetKeyboardCaptured();
    case EeUiOp::MgrFormMouse: return (intptr_t)theForm->GetMouseCaptured();
    case EeUiOp::MgrFormCall:
        switch (b) {
        case 0: theForm->Open(); return 0;
        case 1: theForm->Close(); return 0;
        case 2: theForm->Activate(); return 0;
        case 3: theForm->Deactivate(); return 0;
        case 4: theForm->ClearAllClipRectangles(); return 0;
        case 5: theForm->SetAllControlsDirty(); return 0;
        case 6: return theForm->CopyData();
        case 7: return theForm->DrawPolygons();
        case 8: return theForm->DrawTextA();
        case 9: return theForm->UpdateKeyboard();
        case 10: return theForm->UpdateMouse();
        default: delete theForm; return 0;
        }
    case EeUiOp::MgrFormRectCall:
        switch (b) {
        case 0: theForm->AddDirtyRectangle(UIManagerRect(f)); return 0;
        case 1: theForm->RedrawIntersectControls(UIManagerRect(f)); return 0;
        case 2: theForm->AddClipRectangle(UIManagerRect(f)); return 0;
        case 3: theForm->RelayoutForm(UIManagerRect(f)); return 0;
        default: theForm->UpdateMouseLocation(GE2DPoint(f[0], f[1])); return 0;
        }
    case EeUiOp::MgrFormIme:
        switch (b) {
        case 0: theForm->AddIMEResultString(*reinterpret_cast<const UWideString*>(c)); return kErrorOK;
        case 1: {
            const UIManagerComposition* theArguments = reinterpret_cast<const UIManagerComposition*>(c);
            return theForm->ChangeIMECompositionString(*theArguments->mString, *theArguments->mAttributes,
                                                       theArguments->mCursor);
        }
        case 2: return theForm->CloseIMECompositionString();
        default: return theForm->OpenIMECompositionString();
        }
    case EeUiOp::MgrTranslatedKey:
        sKey.CopyString(reinterpret_cast<const char*>(b), 1);
        return theForm->UpdateTranslatedKey(sKey, (short)c);
    case EeUiOp::MgrCtlIme: return reinterpret_cast<UIControl*>(a)->GetIMEEnabled();
    case EeUiOp::PartRect: UIManagerPutRect(f, reinterpret_cast<UIControl*>(a)->GetRectangle()); return 0;
    // the events and their factories
    case EeUiOp::FormEventCall:
        if (b == 1)
            theEvent->Dereference();
        else
            theEvent->Reference();
        return 0;
    case EeUiOp::MgrEventProcess:
        theEvent->ProcessEvent(reinterpret_cast<UIForm*>(b), reinterpret_cast<UIEntity*>(c));
        return 0;
    case EeUiOp::MgrCallback:
        if (b == 0)
            return (intptr_t) reinterpret_cast<UIEvent* (*)(UIManager*, unsigned long)>(a)(theManager, (unsigned long)c);
        return (intptr_t) reinterpret_cast<UIEvent* (*)(UIForm*, unsigned long)>(a)(reinterpret_cast<UIForm*>(d),
                                                                                    (unsigned long)c);
    // the input
    case EeUiOp::MgrKeyboardShouldUpdate: return gISKeyboard->GetShouldUpdate();
    case EeUiOp::MgrMouseShouldUpdate: return gISMouse->GetShouldUpdate();
    case EeUiOp::MgrHotKeyPoll: UIHotKey::UpdatePressedPollAmount(); return 0;
    case EeUiOp::MgrHotKeysDelete: UIHotKey::DeleteHotKeys(); return 0;
    case EeUiOp::MgrVirtUpdateKeyboard: return theManager->UpdateKeyboard();
    case EeUiOp::MgrVirtUpdateMouse: return theManager->UpdateMouse(GE2DPoint(f[0], f[1]));
    // the window, the rasterizer, the texture and the bitmaps
    case EeUiOp::MgrWindow: {
        GEWindow* theWindow = theManager->GetRasterizer()->GetWindow(theManager->GetScreen());
        if (!theWindow)
            return 0;
        f[0] = theWindow->GetClientOrigin().GetX();
        f[1] = theWindow->GetClientOrigin().GetY();
        return 1;
    }
    case EeUiOp::MgrWindowSize: {
        GEWindow* theWindow = theManager->GetRasterizer()->GetWindow(theManager->GetScreen());
        return (intptr_t)((uint64_t)(uint32_t)theWindow->GetWidth() | (uint64_t)(uint32_t)theWindow->GetHeight() << 32);
    }
    case EeUiOp::MgrBlending: {
        unsigned long theSource;
        unsigned long theDestination;
        theManager->GetRasterizer()->GetBlending(theSource, theDestination, theManager->GetScreen());
        return (intptr_t)((uint64_t)(uint32_t)theSource | (uint64_t)(uint32_t)theDestination << 32);
    }
    case EeUiOp::MgrSetBlending:
        theManager->GetRasterizer()->SetBlending((unsigned long)a, (unsigned long)b, theManager->GetScreen());
        return 0;
    case EeUiOp::MgrTextureLoad:
        theManager->GetRasterizer()->GetScreen(theManager->GetScreen());
        return (intptr_t)UIManagerLoadTexture(theManager, UString("textures\\UIControls"));
    case EeUiOp::MgrTextureSize:
        f[0] = (float)reinterpret_cast<GETexture*>(a)->GetWidth();
        f[1] = (float)reinterpret_cast<GETexture*>(a)->GetHeight();
        return 0;
    case EeUiOp::MgrTextureRelease: reinterpret_cast<GETexture*>(a)->Dereference(); return 0;
    case EeUiOp::MgrBitmapNew: {
        GEBitmap* theNew = new GEBitmap;
        theNew->SetTexture(reinterpret_cast<GETexture*>(a), theManager->GetScreen());
        return (intptr_t)theNew;
    }
    case EeUiOp::MgrBitmapCoords:
        // CreateBitmap (0x1007a438): top-left, top-right, bottom-left, bottom-right
        theBitmap->SetOpacity(1.0f);
        theBitmap->SetTextureCoordinates(GETextureCoordinate(f[0], f[1]), GETextureCoordinate(f[2], f[1]),
                                         GETextureCoordinate(f[0], f[3]), GETextureCoordinate(f[2], f[3]));
        return 0;
    case EeUiOp::MgrScaledBitmapNew: {
        GETextureCoordinate theTopLeft;
        GETextureCoordinate theTopRight;
        GETextureCoordinate theBottomLeft;
        GETextureCoordinate theBottomRight;
        theBitmap->GetTextureCoordinatesPerVertex(theTopLeft, theTopRight, theBottomLeft, theBottomRight);
        GEBitmap* theNew = new GEBitmap;
        theNew->SetTexture(theBitmap->GetTexture(theManager->GetScreen()), theManager->GetScreen());
        theNew->SetTextureCoordinates(theTopLeft, theTopRight, theBottomLeft, theBottomRight);
        return (intptr_t)theNew;
    }
    case EeUiOp::BitmapScale:
        f[0] = theBitmap->GetXScale();
        f[1] = theBitmap->GetYScale();
        return 0;
    case EeUiOp::BitmapSetScale: theBitmap->SetXYScale(f[0], f[1]); return 0;
    case EeUiOp::BitmapRelease: theBitmap->Dereference(); return 0;
    // the locks
    case EeUiOp::MgrLockNew: {
        TSCriticalSection* theLock = NULL;
        ESVERIFY_AT(TSScheduler::CreateCriticalSection(theLock) == keseOK, (int)a);
        return (intptr_t)theLock;
    }
    case EeUiOp::MgrLockDelete: TSScheduler::DestroyCriticalSection(reinterpret_cast<TSCriticalSection*>(a)); return 0;
    case EeUiOp::FormLockEnter: reinterpret_cast<TSCriticalSection*>(a)->Enter(); return 0;
    case EeUiOp::FormLockLeave: reinterpret_cast<TSCriticalSection*>(a)->Leave(); return 0;
    // the IME candidate window
    case EeUiOp::MgrCandidateNew:
        return (intptr_t) new UIIMECandidate(theManager, 0.2f, 0.35f, reinterpret_cast<GEFont*>(a));
    case EeUiOp::MgrCandidateCall:
        switch (b) {
        case 0: return theCandidate->DrawPolygons();
        case 1: return theCandidate->DrawTextA();
        case 2: theCandidate->CloseCandidateWindow(); return 0;
        case 3: return theCandidate->CopyData();
        default: delete theCandidate; return 0;
        }
    case EeUiOp::MgrCandidateSetList:
        theCandidate->SetCandidateList(*reinterpret_cast<const UIIMECandidateList*>(b), f[0]);
        return 0;
    case EeUiOp::MgrCandidateOpen:
        theCandidate->OpenCandidateWindow(UIManagerRect(f), GERectangle(UIManagerBits(b, 1), UIManagerBits(c, 1),
                                                                        UIManagerBits(c, 0), UIManagerBits(b, 0)));
        return 0;
    // the music
    case EeUiOp::MgrSoundStop: theManager->GetSoundManager()->Stop(theInstance, false); return 0;
    case EeUiOp::MgrSoundUnload: theManager->GetSoundManager()->Unload((long)a); return 0;
    case EeUiOp::MgrSoundLoad: return theManager->GetSoundManager()->Load(reinterpret_cast<const char*>(a), 0, 1);
    case EeUiOp::MgrSoundPlay:
        return (intptr_t)theManager->GetSoundManager()->PlayAndReturnReferencedNode((long)a, f[0], 0.0f, 1.0f, true, NULL);
    case EeUiOp::MgrSoundParameters:
        theManager->GetSoundManager()->SetSoundParameters(theInstance, f[0], 0.0f, 1.0f);
        return 0;
    case EeUiOp::MgrInstanceRef:
        if (b == 0)
            theInstance->Reference();
        else
            theInstance->Dereference();
        return 0;
    default:
        break;
    }
    return 0;
}

// FUNCTION: LLE 0x10078ee4
// Creates the UI manager: its locks, the UIControls texture and the check box, radio, sort and scroll-bar
// bitmaps cut from it; looks up the shared mouse buttons and wheel on first use; no music playing
UIManager::UIManager(GERasterizer* inRasterizer, SESoundManager* inSoundManager, unsigned long inNumberOfSurfaces,
                     unsigned long inUITextureBitDepth, UResourceFile* inStringTable, unsigned long inScreen)
    : mRust(NULL)
{
    FSFileSpec theDirectory(gStartupVolume + gStartupDirectory);
    this->mRust = ee_ui_manager_new(this, UIManager::HostOp, inRasterizer, inSoundManager, inNumberOfSurfaces,
                                    inUITextureBitDepth, inStringTable, inScreen);
    this->Call(EeUiManagerCall::Init);   // its ops read the manager through the forwarders: after mRust is set
    if (UIManager::sLMouseButton == NULL) {
        gISMouse->GetEntity(kMouseLeftButton, UIManager::sLMouseButton);
        gISMouse->GetEntity(kMouseRightButton, UIManager::sRMouseButton);
        gISMouse->GetEntity(kMouseZAxis, UIManager::sMouseWheel);
    }
}

// FUNCTION: LLE 0x10079a18
// Destroys the UI manager: stops the music, frees the locks, texture, bitmaps, forms, hot keys, IME window and database
UIManager::~UIManager()
{
    ee_ui_manager_free(this->mRust);
    this->mRust = NULL;
    this->RemoveDatabase();
}

// FUNCTION: LLE 0x10079be5 (AddClippedForms), 0x10079e1f (AddToVisibleForms), 0x1007af90 (RemoveClippedForms): Rust
// (ee_ui manager.rs, inside Pop, SwapForm and Push).

// FUNCTION: LLE 0x10079f7f
// Marks a rectangle dirty: on the top form directly, else on that form and every visible form above that overlaps it
void UIManager::AddDirtyRectangle(const GERectangle& inRectangle, UIForm* inForm)
{
    float f[4];
    UIManagerPutRect(f, inRectangle);
    this->Call(EeUiManagerCall::AddDirtyRectangle, (intptr_t)inForm, 0, 0, f);
}

// FUNCTION: LLE 0x10059fdb
// Queues an event (referenced) to run on the next update for a form and entity
void UIManager::AddEventToTrigger(UIEvent* inEvent, UIForm* inForm, UIEntity* inEntity)
{
    this->Call(EeUiManagerCall::AddEventToTrigger, (intptr_t)inEvent, (intptr_t)inForm, (intptr_t)inEntity);
}

void UIManager::AddForm(UIForm* inForm) { this->Call(EeUiManagerCall::AppendForm, (intptr_t)inForm); }

// FUNCTION: LLE 0x1007a03d
// Passes an IME result string to the top form, under the UI lock
void UIManager::AddIMEResultString(const UWideString& inString)
{
    this->Call(EeUiManagerCall::AddImeResult, (intptr_t)&inString);
}

// FUNCTION: LLE 0x1007a074
// Updates the IME candidate list of the focused IME-enabled control: hides the window for a style-2 list of at
// most one candidate, else shows it at the control; returns OK
long UIManager::ChangeIMECandidateWindow(const UIIMECandidateList& inList)
{
    return (long)this->Call(EeUiManagerCall::ChangeCandidateWindow, (intptr_t)&inList, (intptr_t)inList.mStyle,
                            (intptr_t)inList.mCount);
}

// FUNCTION: LLE 0x1007a102
// Passes the IME composition string, attributes and cursor to the top form under the UI lock; returns its result
long UIManager::ChangeIMECompositionString(const UWideString& inString, const UString& inAttributes,
                                           unsigned long inCursor)
{
    UIManagerComposition theArguments = {&inString, &inAttributes, inCursor};
    return (long)this->Call(EeUiManagerCall::ChangeComposition, (intptr_t)&theArguments);
}

void UIManager::ChangeRasterizer(GERasterizer* inRasterizer)
{
    this->Call(EeUiManagerCall::ChangeRasterizer, (intptr_t)inRasterizer);
}

// FUNCTION: LLE 0x1007a149
// Closes the IME candidate window; returns OK
long UIManager::CloseIMECandidateWindow() { return (long)this->Call(EeUiManagerCall::CloseCandidateWindow); }

// FUNCTION: LLE 0x1007a162
// Tells the top form the IME composition string closed, under the UI lock; returns its result
long UIManager::CloseIMECompositionString() { return (long)this->Call(EeUiManagerCall::CloseComposition); }

// FUNCTION: LLE 0x1007a19b
// Snapshots every visible form (and the candidate window when open) for drawing; returns the last copy result
long UIManager::CopyData() { return (long)this->Call(EeUiManagerCall::CopyData); }

// FUNCTION: LLE 0x1007a216
// Creates every background in the UI background database on its form, by type (others via CreateUserDefinedBackground)
// tiled backgrounds get the row's ID where every other type gets its background ID
void UIManager::CreateBackgrounds()
{
    GETextureDescription theDescription;
    theDescription.mPixelDepth = 16;
    theDescription.mNumMipLevelsToDrop = 0;
    theDescription.mSurfaceVolatility = 1;
    theDescription.mFrameFormat = 0;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mHasAlpha = true;
    theDescription.mCreateMIPMaps = true;
    unsigned long theCount = UIBackgroundTable::GetArraySize();
    theDescription.mPixelDepth = this->GetUITextureBitDepth();
    for (unsigned long i = 0; i < theCount; i++) {
        UIBackgroundTable* theTable = UIBackgroundTable::GetUIBackgroundTable(i);
        if (!theTable)
            continue;
        theDescription.mHasAlpha = theTable->GetHasAlpha();
        UIForm* theForm = this->GetFormByID(theTable->GetFormID());
        if (!theForm)
            continue;   // no such form (GetFormByID used to return a garbage pointer here)
        switch (theTable->GetBackgroundType()) {
        case kBackgroundBitmap:
            new UIBitmapBackground(theTable->GetBackgroundID(), theTable->GetTextureFile(), theDescription,
                                   theTable->GetTextureRect(), theTable->GetLayout(), theForm,
                                   theTable->GetDrawPriority(), theTable->GetVisible());
            break;
        case kBackgroundTiled:
            // Fixed: passed the row ID (GetID) where every other case passes the background ID
            new UITiledBackground(theTable->GetBackgroundID(), theTable->GetTextureFile(), theDescription,
                                  theTable->GetTextureRect(), theTable->GetLayout(), theForm,
                                  theTable->GetDrawPriority(), theTable->GetVisible());
            break;
        case kBackgroundFull:
            new UIBFull(theTable->GetBackgroundID(), theTable->GetTextureFile(), theDescription,
                        theTable->GetTextureRect(), theTable->GetLayout(), theForm, theTable->GetDrawPriority(),
                        theTable->GetNumCols(), theTable->GetNumRows(), theTable->GetVisible());
            break;
        case kBackgroundLRTiled:
            new UITiledBackground(theTable->GetBackgroundID(), theTable->GetTextureFile(), theDescription,
                                  theTable->GetTextureRect(), theTable->GetLayout(), theForm,
                                  theTable->GetDrawPriority(), theTable->GetVisible(), kTilingLeftRight);
            break;
        case kBackgroundTBTiled:
            new UITiledBackground(theTable->GetBackgroundID(), theTable->GetTextureFile(), theDescription,
                                  theTable->GetTextureRect(), theTable->GetLayout(), theForm,
                                  theTable->GetDrawPriority(), theTable->GetVisible(), kTilingTopBottom);
            break;
        case kBackgroundStretchedTiled:
            new UITiledBackground(theTable->GetBackgroundID(), theTable->GetTextureFile(), theDescription,
                                  theTable->GetTextureRect(), theTable->GetLayout(), theForm,
                                  theTable->GetDrawPriority(), theTable->GetVisible(), kTilingStretched,
                                  theTable->GetTileWidthPercent(), theTable->GetTileHeightPercent());
            break;
        default:
            this->CreateUserDefinedBackground(*theTable);
            break;
        }
    }
}

// FUNCTION: LLE 0x1007a438 (CreateBitmap): the Rust constructor's MgrBitmapCoords and BitmapSetScale.

// FUNCTION: LLE 0x1007a48e
// Creates a copy of a bitmap's texture region scaled to fill a control rectangle (20-pixel source)
GEBitmap* UIManager::CreateBitmapForScaledControl(GEBitmap* inBitmap, const GERectangle& inRectangle)
{
    float f[4];
    UIManagerPutRect(f, inRectangle);
    return reinterpret_cast<GEBitmap*>(this->Call(EeUiManagerCall::ScaledBitmap, (intptr_t)inBitmap, 0, 0, f));
}

// FUNCTION: LLE 0x1007a559
// Asks each registered control-event factory in turn for a form's event; returns the first one made, or NULL
UIEvent* UIManager::CreateControlEvent(UIForm* inForm, unsigned long inEventID)
{
    return reinterpret_cast<UIEvent*>(
        this->Call(EeUiManagerCall::CreateControlEvent, (intptr_t)inForm, (intptr_t)inEventID));
}

// FUNCTION: LLE 0x1007a58f
// Asks each registered form-event factory in turn for an event; returns the first one made, or NULL
UIEvent* UIManager::CreateFormEvent(unsigned long inEventID)
{
    return reinterpret_cast<UIEvent*>(this->Call(EeUiManagerCall::CreateFormEvent, (intptr_t)inEventID));
}

// FUNCTION: LLE 0x1007a602
// Draws the snapshotted forms with alpha blending (their text too, unless drawn by GDI) and the candidate window
long UIManager::DrawPolygons() { return (long)this->Call(EeUiManagerCall::DrawPolygons); }

// FUNCTION: LLE 0x1007a6be
// Draws the GDI text of the snapshotted forms and the candidate window, under the draw lock
long UIManager::DrawTextA() { return (long)this->Call(EeUiManagerCall::DrawText); }

// The header's getters and setters (the original's inline members, 0x10059fdb..0x1005a269)
void UIManager::ForceUIUpdate() { this->Call(EeUiManagerCall::ForceUiUpdate); }
#define UI_MANAGER_BITMAP(Name, Index) \
    GEBitmap* UIManager::Name() const { return reinterpret_cast<GEBitmap*>(this->Call(EeUiManagerCall::Bitmap, Index)); }
UI_MANAGER_BITMAP(GetAlphaSortUpBitmap, 0)
UI_MANAGER_BITMAP(GetAlphaSortDownBitmap, 1)
UI_MANAGER_BITMAP(GetSBBitmapDownNormal, 2)
UI_MANAGER_BITMAP(GetSBBitmapDownPressed, 3)
UI_MANAGER_BITMAP(GetSBBitmapLeftNormal, 4)
UI_MANAGER_BITMAP(GetSBBitmapLeftPressed, 5)
UI_MANAGER_BITMAP(GetSBBitmapRightNormal, 6)
UI_MANAGER_BITMAP(GetSBBitmapRightPressed, 7)
UI_MANAGER_BITMAP(GetSBBitmapUpNormal, 8)
UI_MANAGER_BITMAP(GetSBBitmapUpPressed, 9)
UI_MANAGER_BITMAP(GetDragBarBitmap, 10)
UI_MANAGER_BITMAP(GetCheckOff, 11)
UI_MANAGER_BITMAP(GetCheckDownOff, 12)
UI_MANAGER_BITMAP(GetCheckOn, 13)
UI_MANAGER_BITMAP(GetCheckDownOn, 14)
UI_MANAGER_BITMAP(GetCheckNot, 15)
UI_MANAGER_BITMAP(GetCheckDownNot, 16)
UI_MANAGER_BITMAP(GetRadioOff, 17)
UI_MANAGER_BITMAP(GetRadioDownOff, 18)
UI_MANAGER_BITMAP(GetRadioOn, 19)
UI_MANAGER_BITMAP(GetRadioDownOn, 20)
#undef UI_MANAGER_BITMAP
UIIMECandidate* UIManager::GetCandidateWindow() const
{
    return reinterpret_cast<UIIMECandidate*>(this->Call(EeUiManagerCall::CandidateWindow));
}
UIForm* UIManager::GetCurrentForm() const { return reinterpret_cast<UIForm*>(this->Call(EeUiManagerCall::CurrentForm)); }
bool UIManager::GetDrawCandidateWindow() const { return this->Call(EeUiManagerCall::DrawCandidateWindow) != 0; }
GEFont* UIManager::GetFont(unsigned long inIndex) const
{
    return reinterpret_cast<GEFont*>(this->Call(EeUiManagerCall::Font, (intptr_t)inIndex));
}
unsigned long UIManager::GetFormCount() const { return (unsigned long)this->Call(EeUiManagerCall::FormCount); }
UIForm* UIManager::GetStackedForm(unsigned long inIndex) const
{
    return reinterpret_cast<UIForm*>(this->Call(EeUiManagerCall::StackedForm, (intptr_t)inIndex));
}
TSCriticalSection* UIManager::GetUILock() const { return reinterpret_cast<TSCriticalSection*>(this->Call(EeUiManagerCall::UiLock)); }
GESurface* UIManager::GetDrawingSurface() const { return this->GetRasterizer()->GetSecondarySurface(this->GetScreen()); }
unsigned long UIManager::GetNumberOfSurfaces() const
{
    return (unsigned long)this->Call(EeUiManagerCall::NumberOfSurfaces);
}
GERasterizer* UIManager::GetRasterizer() const
{
    return reinterpret_cast<GERasterizer*>(this->Call(EeUiManagerCall::Rasterizer));
}
unsigned long UIManager::GetScreen() const { return (unsigned long)this->Call(EeUiManagerCall::Screen); }
SESoundManager* UIManager::GetSoundManager() const
{
    return reinterpret_cast<SESoundManager*>(this->Call(EeUiManagerCall::SoundManager));
}
UResourceFile* UIManager::GetStringTable() const
{
    return reinterpret_cast<UResourceFile*>(this->Call(EeUiManagerCall::StringTable));
}
unsigned long UIManager::GetUITextureBitDepth() const
{
    return (unsigned long)this->Call(EeUiManagerCall::TextureBitDepth);
}
bool UIManager::IsFormOnStack(UIForm* inForm) { return this->Call(EeUiManagerCall::IsFormOnStack, (intptr_t)inForm) != 0; }
bool UIManager::IsFormVisible(UIForm* inForm) { return this->Call(EeUiManagerCall::IsFormVisible, (intptr_t)inForm) != 0; }
void UIManager::LockUIForCopyData() { this->Call(EeUiManagerCall::LockForCopy, 0); }
void UIManager::UnlockUIForCopyData() { this->Call(EeUiManagerCall::LockForCopy, 1); }
void UIManager::RegisterControlEventCallback(UIEvent* (*inCallback)(UIForm*, unsigned long))
{
    this->Call(EeUiManagerCall::RegisterControlCallback, (intptr_t)inCallback);
}
void UIManager::RegisterFormEventCallback(UIEvent* (*inCallback)(UIManager*, unsigned long))
{
    this->Call(EeUiManagerCall::RegisterFormCallback, (intptr_t)inCallback);
}
void UIManager::SetFont(unsigned long inIndex, GEFont* inFont)
{
    this->Call(EeUiManagerCall::SetFont, (intptr_t)inIndex, (intptr_t)inFont);
}
void UIManager::SetNumberOfFonts(unsigned long inCount) { this->Call(EeUiManagerCall::SetNumberOfFonts, (intptr_t)inCount); }

// FUNCTION: LLE 0x1007a72d
// Returns the form with the given ID (NULL: none)
UIForm* UIManager::GetFormByID(unsigned long inID) const
{
    // an unknown ID gives NULL (the original returned the word one past the end of the form list)
    return reinterpret_cast<UIForm*>(this->Call(EeUiManagerCall::FormById, (intptr_t)inID));
}

// FUNCTION: LLE 0x1007a766
// Returns the mouse wheel's scroll step: -1 when rolled forward, 1 when rolled back, 0 when still
long UIManager::GetMouseWheelScrollAmount()
{
    return ee_ui_manager_wheel_scroll_amount(UIManager::sMouseWheel->GetNormalizedValue());
}

// FUNCTION: LLE 0x1007a78a
// Loads the UI database (fonts, forms, backgrounds, controls, control and form events, hot keys) from a directory
void UIManager::LoadDatabase(FSPath* inPath, const UString& inDirectory)
{
    FSFileSpec* theDbFile = NULL;
    long theResult;

    theResult = EEDbFindFile(inPath, inDirectory + gDbUIFontsFileName, theDbFile);
    ESVERIFY_AT(theResult == keseOK, 1081);
    ESVERIFY_AT(theDbFile != NULL, 1082);
    theResult = UIFontTable::Create(*theDbFile, this);
    ESVERIFY_AT(theResult == keseOK, 1084);
    delete theDbFile;

    theResult = EEDbFindFile(inPath, inDirectory + gDbUIFormsFileName, theDbFile);
    ESVERIFY_AT(theResult == keseOK, 1089);
    ESVERIFY_AT(theDbFile != NULL, 1090);
    theResult = UIForm::Create(*theDbFile, this);
    ESVERIFY_AT(theResult == keseOK, 1092);
    delete theDbFile;

    theResult = EEDbFindFile(inPath, inDirectory + gDbUIBackgroundFileName, theDbFile);
    ESVERIFY_AT(theResult == keseOK, 1097);
    ESVERIFY_AT(theDbFile != NULL, 1098);
    theResult = UIBackgroundTable::Create(*theDbFile);
    ESVERIFY_AT(theResult == keseOK, 1100);
    this->CreateBackgrounds();
    delete theDbFile;

    theResult = EEDbFindFile(inPath, inDirectory + gDbUIControlsFileName, theDbFile);
    ESVERIFY_AT(theResult == keseOK, 1106);
    ESVERIFY_AT(theDbFile != NULL, 1107);
    UIControlsTable::Create(*theDbFile, this);
    delete theDbFile;

    theResult = EEDbFindFile(inPath, inDirectory + gDbUIControlEventsFileName, theDbFile);
    ESVERIFY_AT(theResult == keseOK, 1113);
    ESVERIFY_AT(theDbFile != NULL, 1114);
    UIEvent::CreateControlEvents(*theDbFile);
    delete theDbFile;

    theResult = EEDbFindFile(inPath, inDirectory + gDbUIFormEventsFileName, theDbFile);
    ESVERIFY_AT(theResult == keseOK, 1120);
    ESVERIFY_AT(theDbFile != NULL, 1121);
    UIEvent::CreateFormEvents(*theDbFile, this);
    delete theDbFile;

    theResult = EEDbFindFile(inPath, inDirectory + gDbUIHotKeyFileName, theDbFile);
    ESVERIFY_AT(theResult == keseOK, 1127);
    ESVERIFY_AT(theDbFile != NULL, 1128);
    UIHotKey::CreateHotKeys(*theDbFile);
    delete theDbFile;
}

// FUNCTION: LLE 0x1007ab03 (LoadTexture): UIManagerLoadTexture above (the Rust constructor's MgrTextureLoad).

// FUNCTION: LLE 0x1007abb9
// Opens the IME candidate window over the client area for the focused IME-enabled control; returns OK
long UIManager::OpenIMECandidateWindow() { return (long)this->Call(EeUiManagerCall::OpenCandidateWindow); }

// FUNCTION: LLE 0x1007ac9b
// Tells the top form an IME composition string opened, under the UI lock; returns its result
long UIManager::OpenIMECompositionString() { return (long)this->Call(EeUiManagerCall::OpenComposition); }

// FUNCTION: LLE 0x1007acd4
// Closes the top form, re-shows the forms it covered, redraws its area and activates the form below; returns OK
long UIManager::Pop() { return (long)this->Call(EeUiManagerCall::Pop); }

// FUNCTION: LLE 0x1007adbe
// Pops forms until the given one is on top, when it is on the stack; returns OK (each Pop does)
long UIManager::PopTo(UIForm* inForm) { return (long)this->Call(EeUiManagerCall::PopTo, (intptr_t)inForm); }

// FUNCTION: LLE 0x1007ae53
// Opens a form on top of the stack (unless already stacked), deactivating the old top; returns OK
long UIManager::Push(UIForm* inForm) { return (long)this->Call(EeUiManagerCall::Push, (intptr_t)inForm); }

// FUNCTION: LLE 0x1007af47
// Marks every visible form and all its controls dirty
void UIManager::RedrawAll() { this->Call(EeUiManagerCall::RedrawAll); }

// FUNCTION: LLE 0x1007b147
// Rebuilds each visible form's clip rectangles from the visible forms above it
void UIManager::ResetClippedForms() { this->Call(EeUiManagerCall::ResetClippedForms); }

// FUNCTION: LLE 0x1007b1a2
// Re-lays out every stacked form to the window size and rebuilds the clip rectangles
void UIManager::RelayoutAllForms() { this->Call(EeUiManagerCall::RelayoutAllForms); }

// FUNCTION: LLE 0x1007b254
// Runs and releases the queued events of one form; the other forms' events stay queued in order
void UIManager::ProcessAllEventsForForm(UIForm* inForm)
{
    this->Call(EeUiManagerCall::ProcessAllEventsForForm, (intptr_t)inForm);
}

// FUNCTION: LLE 0x1007b2ec
// Stops the playing music and releases its instance
void UIManager::RemoveAndStopSoundInstance() { this->Call(EeUiManagerCall::RemoveAndStopSoundInstance); }

// FUNCTION: LLE 0x1007b310
// Releases the music instance
void UIManager::RemoveSoundInstance() { this->Call(EeUiManagerCall::RemoveSoundInstance); }

// FUNCTION: LLE 0x1007b338
// Deletes the UI database tables (controls, backgrounds, fonts)
void UIManager::RemoveDatabase()
{
    UIControlsTable::DeleteAll();
    UIBackgroundTable::DeleteAll();
    UIFontTable::DeleteAll();
}

// FUNCTION: LLE 0x1007b347
// Replaces the music instance, moving the reference to the new one
void UIManager::SetSoundInstance(SESMInstanceNode* inInstance)
{
    this->Call(EeUiManagerCall::SetSoundInstance, (intptr_t)inInstance);
}

// FUNCTION: LLE 0x1007b36e
// Replaces the top form with another (unless already stacked), re-showing what the old one covered; returns OK
long UIManager::SwapForm(UIForm* inForm) { return (long)this->Call(EeUiManagerCall::SwapForm, (intptr_t)inForm); }

// FUNCTION: LLE 0x1007b511
// Swaps the left and right mouse buttons when the left-handed setting changes
void UIManager::SetMouseLeftHanded(bool inLeftHanded)
{
    ISEntity* theButton = UIManager::sLMouseButton;
    if (inLeftHanded == UIManager::sMouseLeftHanded)
        return;
    UIManager::sLMouseButton = UIManager::sRMouseButton;
    UIManager::sRMouseButton = theButton;
    UIManager::sMouseLeftHanded = inLeftHanded;
}

// FUNCTION: LLE 0x1007b53a
// Sets the music volume; stops the music at 0 and restarts the last track (or the intro) when raised from 0
void UIManager::SetMusicVolume(float inVolume)
{
    float f[4] = {inVolume, 0.0f, 0.0f, 0.0f};
    this->Call(EeUiManagerCall::SetMusicVolume, 0, 0, 0, f);
}

// FUNCTION: LLE 0x1007b616
// Loads and starts a music track unless it is the current one (names compared lower-cased)
void UIManager::StartMusic(const UWideString& inFileName)
{
    this->Call(EeUiManagerCall::StartMusic, (intptr_t)(const char*)inFileName);
}

// FUNCTION: LLE 0x1007b743
// Stops the music and unloads the track
void UIManager::StopMusic() { this->Call(EeUiManagerCall::StopMusic); }

// FUNCTION: LLE 0x1007b786
// Passes keyboard input to the top form when the keyboard changed, a control holds it, or an update is forced
long UIManager::UpdateKeyboard() { return (long)this->Call(EeUiManagerCall::UpdateKeyboard); }

// FUNCTION: LLE 0x1007b7f4
// Passes the mouse location and buttons to the top form when the mouse changed, it is captured, or an update is forced
long UIManager::UpdateMouse(const GE2DPoint& inMouseLocation)
{
    float f[4] = {inMouseLocation.GetX(), inMouseLocation.GetY(), 0.0f, 0.0f};
    return (long)this->Call(EeUiManagerCall::UpdateMouse, 0, 0, 0, f);
}

// FUNCTION: LLE 0x1007b864
// Passes a translated character to the top form unless its repeat count is 16 or more; returns its result
long UIManager::UpdateTranslatedKey(const char* inKey, short inRepeat)
{
    return (long)this->Call(EeUiManagerCall::UpdateTranslatedKey, (intptr_t)inKey, (intptr_t)inRepeat);
}

// FUNCTION: LLE 0x1007b8f1
// Runs one UI update: keyboard, mouse, queued events, then any pending music stop or start; returns the mouse result
long UIManager::UpdateUI(const GE2DPoint& inMouseLocation)
{
    float f[4] = {inMouseLocation.GetX(), inMouseLocation.GetY(), 0.0f, 0.0f};
    return (long)this->Call(EeUiManagerCall::UpdateUi, 0, 0, 0, f);
}

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// TEMPLATE: LLE 0x1007ba04
// TEMPLATE: LLE 0x1007ba0b
// SYNTHETIC: LLE 0x10078c81
// SYNTHETIC: LLE 0x10078c8b
// SYNTHETIC: LLE 0x10078c9b
// SYNTHETIC: LLE 0x10078ca7
// SYNTHETIC: LLE 0x10078cb1
// SYNTHETIC: LLE 0x10078cbb
// SYNTHETIC: LLE 0x10078ccb
// SYNTHETIC: LLE 0x10078cd7
// SYNTHETIC: LLE 0x10078ce1
// SYNTHETIC: LLE 0x10078ceb
// SYNTHETIC: LLE 0x10078cfb
// SYNTHETIC: LLE 0x10078d07
// SYNTHETIC: LLE 0x10078d11
// SYNTHETIC: LLE 0x10078d1b
// SYNTHETIC: LLE 0x10078d2b
// SYNTHETIC: LLE 0x10078d37
// SYNTHETIC: LLE 0x10078d41
// SYNTHETIC: LLE 0x10078d4b
// SYNTHETIC: LLE 0x10078d5b
// SYNTHETIC: LLE 0x10078d67
// SYNTHETIC: LLE 0x10078d71
// SYNTHETIC: LLE 0x10078d7b
// SYNTHETIC: LLE 0x10078d8b
// SYNTHETIC: LLE 0x10078d97
// SYNTHETIC: LLE 0x10078da1
// SYNTHETIC: LLE 0x10078dda
// SYNTHETIC: LLE 0x10078e07
// SYNTHETIC: LLE 0x10078e68
// SYNTHETIC: LLE 0x10078e89
// SYNTHETIC: LLE 0x1007b8e7

// TEST ONLY: the harness's presentation bridge (tools/harness/presentation_bridge.c) finds these by name in the map
// (the manager's and forms' state is Rust's: ee_ui managerobj.rs, formobj.rs): the UI lock it try-enters, the form
// stack, and a form's id and controls. Kept by the linker for it.
extern "C" void* EETestUILock(UIManager* inManager) { return inManager->GetUILock(); }
extern "C" unsigned long EETestUIFormCount(UIManager* inManager) { return inManager->GetFormCount(); }
extern "C" void* EETestUIForm(UIManager* inManager, unsigned long inIndex) { return inManager->GetStackedForm(inIndex); }
extern "C" unsigned long EETestUIFormID(UIForm* inForm) { return inForm->GetFormID(); }
extern "C" unsigned long EETestUIFormControlCount(UIForm* inForm) { return inForm->GetControlCount(); }
extern "C" void* EETestUIFormControl(UIForm* inForm, unsigned long inIndex) { return inForm->GetControl(inIndex); }
#if defined(_MSC_VER)
#pragma comment(linker, "/include:EETestUILock")
#pragma comment(linker, "/include:EETestUIFormCount")
#pragma comment(linker, "/include:EETestUIForm")
#pragma comment(linker, "/include:EETestUIFormID")
#pragma comment(linker, "/include:EETestUIFormControlCount")
#pragma comment(linker, "/include:EETestUIFormControl")
#endif
