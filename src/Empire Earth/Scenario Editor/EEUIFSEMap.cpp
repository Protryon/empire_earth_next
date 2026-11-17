// C:\EEX\Empire Earth\Scenario Editor\EEUIFSEMap.cpp (__FILE__ 0x4c5148, the ctor's ESVERIFYs). The editor's map tab:
// 0x1a18f1..0x1a3f10 (UIBMap::ButtonUp, UIRBMap's Check/HideControls, EEUIFSEEMapType::ProcessEvent, the tab's ctor,
// the helper classes' ??_G, the tab's ??_G/dtor/members, then the vector push_back and EETerrainTable::GetNumTables
// COMDATs). After EEUIFSEMainButton.cpp (0x1a147f), before EEUIFSEPaint.cpp (0x1a3f10). Unit flags: the exe module
// set.
//
// The tab whole in Rust (ee_screens editor_map.rs): EEUIFSEMap holds its object and forwards; UIBMap and UIRBMap
// stay C++ classes (controls the form owns) whose slots call into it. This unit is the tab's host
// (ee_screenscpp_semp): the controls only the tab makes and the world it reads and posts to.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GERhombus.h"
#include "lle/GEText.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/UILBString.h"
#include "lle/UIFileComboBox.h"
#include "lle/UResourceFile.h"
#include "lle/UIRadioButton.h"
#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EERandomMapGenerators.h"
#include "ee/EEUIFileComboBox.h"
#include "ee/EESEARust.h"
#include "ee/EEUIFSEMap.h"
#include "ee/EEStringIDs.h"
#include "ee/EERules.h"
#include "ee/EEScenarioEditorIDs.h"
#include "rust/game/screens.h"

extern GEFont* gSeplScreenFont;   // the screen's font while a tab makes its controls (EESERBUnitTypes.cpp)
void SeplLinePush(const UWideString& inLine);   // a line for the next combo box made with pending lines (the same)

// Releases the Create Map button and creates the map from the tab's settings
void UIBMap::ButtonUp()
{
    UIButton::ButtonUp();
    this->mMap->CreateMap();
}

// Selects a map kind (blank, random or seeded) and shows only that kind's controls; blank re-runs the elevation
// file's selection event (editor_map.rs)
void UIRBMap::Check()
{
    UIRadioButton::Check();
    ee_screens_semp_kind(this->GetForm(), (uint32_t)this->GetId());
}

// Hides every kind's controls of the map tab (editor_map.rs)
void UIRBMap::HideControls()
{
    ee_screens_semp_kind(this->GetForm(), 0);
}

// Builds the map tab: the editor terrains, the size, terrain, style and seed controls, the kind radio buttons, the
// Create Map button and the elevation file controls (editor_map.rs)
EEUIFSEMap::EEUIFSEMap(unsigned long& ioIndex, UIForm* inForm, GEFont* inFont)
{
    gSeplScreenFont = inFont;
    uint32_t theIndex = (uint32_t)ioIndex;
    this->mRust = ee_screens_semp_new(this, inForm, &theIndex);
    ioIndex = theIndex;
}

// Destroys the map tab, deleting all of its controls (editor_map.rs)
EEUIFSEMap::~EEUIFSEMap()
{
    ee_screens_semp_free(this->mRust);
}

// Creates a blank map of the chosen size and terrain (then draws a new random seed), or a random map of the chosen
// size and style with a fresh seed (shown in the seed label) or the seed typed in the edit box (editor_map.rs)
void EEUIFSEMap::CreateMap()
{
    ee_screens_semp_call(this->mRust, EE_SCREENS_SEMP_CREATE_MAP, 0);
}

// In blank mode shows or hides the water/land elevation controls (the height box does the opposite); else hides all
// (editor_map.rs)
void EEUIFSEMap::ShowElevationControls(bool inShow)
{
    ee_screens_semp_call(this->mRust, EE_SCREENS_SEMP_SHOW_ELEVATION, inShow ? 1 : 0);
}

// Refreshes the elevation file list when the tab is entered (editor_map.rs)
void EEUIFSEMap::Enter()
{
    ee_screens_semp_call(this->mRust, EE_SCREENS_SEMP_ENTER, 0);
}

// ---- the tab's host (editor_map.rs: the ops from 500)

static_assert(kSEMapElevationFile == 0x75 && kSEMapWidth == 0x6b && kSEMapLandCutoff == 0x78 &&
                  kSEMapDefaultTerrainLabel == 0x64 && kSEMapWaterCutoff == 0x7d,
              "editor_map.rs's ids");

namespace {

void SempColor(GEText* inText, const GEColor& inColor)
{
    inText->SetIsTransparent(true);
    inText->SetFgColor(inColor);
}

}   // namespace

extern "C" int32_t ee_screenscpp_semp(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    UIForm* theForm = static_cast<UIForm*>(p[1]);
    GE2DPoint theLocation(f[0], f[1]);
    UWideString theText;
    switch (inOp) {
    case EE_SCREENS_SEMP_OP_TERRAIN_COUNT: return (int32_t)EETerrainTable::GetNumTables();
    case EE_SCREENS_SEMP_OP_TERRAIN: {
        EETerrainTable* theTable = EETerrainTable::Get((unsigned long)i[0]);
        if (theTable == NULL || !theTable->mIsInEditor)
            return 0;
        i[1] = (int32_t)theTable->mID;
        i[2] = (int32_t)theTable->mNameID;
        return 1;
    }
    case EE_SCREENS_SEMP_OP_LINES_GENERATORS:
        for (unsigned long k = 0; k < EERMGetNumberOfGenerators(); k++)
            SeplLinePush(EERMGetGeneratorDisplayName(k));
        return 0;
    case EE_SCREENS_SEMP_OP_LINES_MAP_SIZES:   // gameoptions.yaml map_sizes
        for (size_t k = 0; k < EEGetRules().mGameOptions.mMapSizes.size(); k++) {
            gResource->GetString(EEGetRules().mGameOptions.mMapSizes[k].mNameID, theText);
            SeplLinePush(theText);
        }
        return 0;
    case EE_SCREENS_SEMP_OP_RADIO_HEIGHT: {
        unsigned long theHeight = (unsigned long)theForm->GetManager()->GetRadioOn()->GetHeight();
        if (theHeight < (unsigned long)gSeplScreenFont->GetHeight())
            theHeight = gSeplScreenFont->GetHeight();
        return (int32_t)theHeight;
    }
    case EE_SCREENS_SEMP_OP_MAKE_RADIO: {
        gResource->GetString((unsigned long)i[1], theText);
        UIRBMap* t = new UIRBMap((unsigned long)i[0], theText, gSeplScreenFont, theLocation, 100.0f, (unsigned long)f[2],
                                 theForm, (unsigned long)i[2], i[3] != 0);
        t->GetTextLabel()->SetBgOpacity(0.4f);
        SempColor(t->GetTextLabel()->GetGEText(), GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
        theForm->AddControl(t, 1);
        *static_cast<UIControl**>(p[0]) = t;
        return 0;
    }
    case EE_SCREENS_SEMP_OP_MAKE_BUTTON: {
        UIBMap* t = new UIBMap(0x65, theLocation, 260.0f, gSeplScreenFont->GetHeight() + 10.0f, theForm,
                               (unsigned long)i[2], static_cast<EEUIFSEMap*>(p[2]));
        t->SetBgOpacity(1.0f);
        theForm->AddControl(t, 1);
        *static_cast<UIControl**>(p[0]) = t;
        return 0;
    }
    case EE_SCREENS_SEMP_OP_BUTTON_TEXT: {
        UIButton* t = static_cast<UIButton*>(p[0]);
        gResource->GetString(IDS_CREATE_MAP, theText);
        t->SetText(theText, gSeplScreenFont);
        SempColor(t->GetGEText(), GEColor(1.0f, 1.0f, 1.0f));
        return 0;
    }
    case EE_SCREENS_SEMP_OP_MAKE_FILE_COMBO: {
        FSFileSpec theSpec;
        theSpec.SetVolume(gStartupVolume);
        theSpec.SetDirectory(gStartupDirectory + "data\\" + "Scenarios\\" + "Elevation Maps\\");
        FSPath* thePath = new FSPath;
        thePath->AddDirectory(theSpec, true);
        EEUIFileComboBox* t = new EEUIFileComboBox(thePath, UString("*") + ".dat", kSEMapElevationFile, 1,
                                                   std::vector<UWideString>(), gSeplScreenFont, theLocation, 200, 400,
                                                   theForm, (unsigned long)i[2], 0, 2, false);
        GEText* theGEText = t->GetGEText();
        theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
        theGEText->SetIsTransparent(true);
        t->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
        t->SetTextBoxBgOpacity(0.4f);
        UIEvent* theEvent = new EESEERust(kSEEMapType, p[2]);
        t->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
        theEvent->Dereference();
        t->SetStringTableID(IDS_ELEVATION_FILE_TO_USE_WITH_BLANK);
        theForm->AddControl(t, 1);
        thePath->Dereference();
        *static_cast<UIControl**>(p[0]) = t;
        return 0;
    }
    case EE_SCREENS_SEMP_OP_LABEL_RECT: {
        UIControl* c = static_cast<UIControl*>(p[0]);
        const GERectangle& r = i[0] ? static_cast<UIRadioButton*>(c)->GetTextLabel()->GetRectangle()
                                    : static_cast<UIComboBox*>(c)->GetTextLabel()->GetRectangle();
        f[0] = r.GetLeft();
        f[1] = r.GetTop();
        f[2] = r.GetRight();
        f[3] = r.GetBottom();
        return 0;
    }
    case EE_SCREENS_SEMP_OP_COMBO_ATOI: return atoi(static_cast<UIComboBox*>(p[0])->GetTextLabel()->GetText(0));
    case EE_SCREENS_SEMP_OP_BLANK_MAP:
        (new EESEARust(kSEABlankMap, NULL, i[0], i[1], i[2]))->Dereference();
        return 0;
    case EE_SCREENS_SEMP_OP_RANDOM_MAP:
        (new EESEARust(kSEARandomMap, NULL, i[0], i[1]))->Dereference();
        return 0;
    case EE_SCREENS_SEMP_OP_SEED_DRAW: {
        URandomIntegers theRandom;
        theRandom.SetSeed(TMGetTime());
        gUserInterface->mRandomSeed = theRandom.Generate(0, 1000000);
        return (int32_t)gUserInterface->mRandomSeed;
    }
    case EE_SCREENS_SEMP_OP_SEED_SET: gUserInterface->mRandomSeed = i[0]; return 0;
    case EE_SCREENS_SEMP_OP_CONTROL:
        *static_cast<UIControl**>(p[1]) = static_cast<UIForm*>(p[0])->GetControlById((unsigned long)i[0]);
        return 0;
    case EE_SCREENS_SEMP_OP_PROCESS_SELECTION: {
        UIControl* c = static_cast<UIControl*>(p[0]);
        c->GetTriggerEvent(kControlTriggerSelectionChanged)->ProcessEvent(NULL, c);
        return 0;
    }
    case EE_SCREENS_SEMP_OP_FILE_ENTRIES: static_cast<EEUIFileComboBox*>(p[0])->CreateEntries(); return 0;
    }
    return 0;
}
