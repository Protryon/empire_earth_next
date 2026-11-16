// C:\EEX\Empire Earth\Scenario Editor\EESEUITechListBox.cpp (file name inferred: no __FILE__; RTTI class
// EESEUITechListBox). The player tab's technology list boxes: 0x19764b..0x199afd (the ctor, the members, the four
// sort predicates and their std::sort COMDATs, the ??_G, the dtor and EESEPlayerScreen::GetCurrentPlayer's COMDAT).
// After EESEUICTriggerSelectUnits.cpp, before EESEUnit.cpp (0x199afd: the unit tab's helper classes follow in
// definition order, not alphabetically, so they are one unit). Unit flags: the exe module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
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
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIComboBox.h"
#include "lle/UIFileComboBox.h"
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"
#include "lle/UISlider.h"
#include "lle/UICheckBox.h"

#include "lle/UIRadioButton.h"
#include "lle/UITriStateCheckBox.h"
#include <vector>
#include <deque>

#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EESMEffect.h"
#include "ee/EESETSBaseObject.h"
#include "ee/EESETriggerScreen.h"
#include "lle/USingleton.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEScenarioManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EESETSEScenarioComboBox.h"
#include "ee/EESETSUICBPlayer.h"
#include "ee/EESETSEffectControls.h"
#include "lle/GE3DPoint.h"
#include "lle/GE3DLine.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEMap.h"
#include "ee/EEMouse.h"
#include "ee/EEGraphics.h"
#include "ee/EEARust.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "ee/EESMArea.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EESMObject.h"
#include "ee/EESEPlayerScreen.h"
#include "ee/EEMessage.h"
#include "ee/EESETSConditionControls.h"
#include "ee/EEUICSelectUnits.h"
#include "ee/EESEUICTriggerSelectUnits.h"
#include "ee/EESETSOUnitList.h"
#include "ee/EESETSAComboBox.h"
#include "ee/EESETSAEditBox.h"
#include "ee/EESETSObject.h"
#include "ee/EEPlayer.h"
#include "ee/EESEUITechListBox.h"


// a tech id's epoch: the ids below 15 are the epochs, the rest divide by 1000 (as EETechTree::Get); written in each
// predicate as an inline expression (every use recomputes it) (provisional name)
inline long EESETLGetEpoch(long inID)
{
    return EETechTreeEpochOf(inID);
}

// The four sorts of Rebuild (std::sort over mTechs; each operator() is a refused inline COMDAT after its sort's
// helpers). By epoch, then by the name of the unit class the tech builds (0x19879d) (provisional name)
struct EESETLEpochUnitLess {
    bool operator()(const long& inLeft, const long& inRight) const
    {
        UWideString theLeft;
        UWideString theRight;
        EETechTreeNode* theNode = gTechTree.Get(inLeft);
        gResource->GetString(gUnitClassTable[theNode->mData->mUnitClass]->mNameStringID, theLeft);
        theNode = gTechTree.Get(inRight);
        gResource->GetString(gUnitClassTable[theNode->mData->mUnitClass]->mNameStringID, theRight);
        return EESETLGetEpoch(inLeft) < EESETLGetEpoch(inRight) ||
               (EESETLGetEpoch(inLeft) == EESETLGetEpoch(inRight) && theLeft < theRight);
    }
};

// by epoch, then by the tech's own name (0x198b20) (provisional name)
struct EESETLEpochTechLess {
    bool operator()(const long& inLeft, const long& inRight) const
    {
        UWideString theLeft;
        UWideString theRight;
        EETechTreeNode* theNode = gTechTree.Get(inLeft);
        gResource->GetString(theNode->mData->mNameID, theLeft);
        theNode = gTechTree.Get(inRight);
        gResource->GetString(theNode->mData->mNameID, theRight);
        return EESETLGetEpoch(inLeft) < EESETLGetEpoch(inRight) ||
               (EESETLGetEpoch(inLeft) == EESETLGetEpoch(inRight) && theLeft < theRight);
    }
};

// by the unit class name (0x198e89) (provisional name)
struct EESETLUnitLess {
    bool operator()(const long& inLeft, const long& inRight) const
    {
        UWideString theLeft;
        UWideString theRight;
        EETechTreeNode* theNode = gTechTree.Get(inLeft);
        gResource->GetString(gUnitClassTable[theNode->mData->mUnitClass]->mNameStringID, theLeft);
        theNode = gTechTree.Get(inRight);
        gResource->GetString(gUnitClassTable[theNode->mData->mUnitClass]->mNameStringID, theRight);
        return theLeft < theRight;
    }
};

// by the tech name (0x199195) (provisional name)
struct EESETLTechLess {
    bool operator()(const long& inLeft, const long& inRight) const
    {
        UWideString theLeft;
        UWideString theRight;
        EETechTreeNode* theNode = gTechTree.Get(inLeft);
        gResource->GetString(theNode->mData->mNameID, theLeft);
        theNode = gTechTree.Get(inRight);
        gResource->GetString(theNode->mData->mNameID, theRight);
        return theLeft < theRight;
    }
};

// Creates a tech list box of the player tab's tech tree page, with a dark green selection highlight
EESEUITechListBox::EESEUITechListBox(unsigned long inID, std::vector<UWideString>& inStrings, GEFont* inFont,
                                     bool inAutoSelect, const GE2DPoint& inLocation, unsigned long inWidth,
                                     unsigned long inHeight, UIForm* inForm, unsigned long inIndex,
                                     unsigned long inBorderThickness)
    : UILBString(inID, inStrings, inFont, inAutoSelect, GERectangle(inLocation, (float)inWidth, (float)inHeight),
                 inForm, inIndex, inBorderThickness)
{
    this->GetSelectedBackground()->SetColor(GEColor(0.0f, 0.38f, 0.0f));
}

// Appends a tech id to the list's techs (Rebuild writes the lines)
void EESEUITechListBox::AddTech(long inID)
{
    this->mTechs.push_back(inID);
}

// NEAR: inline budget: the original refuses _Ptrit::operator+ in the first two mTechs[] (the counter in memory,
// three iterator temporaries); ours expands them
// Copies the list for drawing and marks the visible lines related to the selected tech: in the enabled list the
// techs that depend on it, in the disabled list the techs it depends on; returns the base copy result
long EESEUITechListBox::CopyData()
{
    EETechTree* theTree = gUIManager->mScenarioEditorForm->GetPlayerScreen()->GetCurrentPlayer()->GetTechTree();
    long theResult = UILBString::CopyData();
    this->mDependents.clear();
    if (theResult == kErrorOK) {
        unsigned long theFirst;
        unsigned long theLast;
        unsigned long theSelection;
        if (this->ScrollBarExists()) {
            theFirst = this->GetDisplayLine();
            theLast = this->GetNumberLinesDisplayed() + theFirst;
        } else {
            theFirst = 0;
            theLast = this->GetNumLines();
        }
        if (this->GetCurrentSelection(theSelection) == kErrorOK) {
            for (unsigned long i = theFirst; i < theLast; i++) {
                EETechTreeNode* theNode = theTree->Get(this->mTechs[i]);
                if (i == theSelection)
                    continue;
                if (this->GetId() == kSERBUnitTypesTechList) {
                    if (theNode->DependsOn(this->mTechs[theSelection]))
                        this->mDependents.push_back(i - theFirst);
                } else {
                    if (theTree->Get(this->mTechs[theSelection])->DependsOn(theNode->mData->mID))
                        this->mDependents.push_back(i - theFirst);
                }
            }
        }
    }
    return theResult;
}

// Empties the list's techs and lines
void EESEUITechListBox::Clear()
{
    this->mTechs.clear();
    this->DeleteAllEntries();
}

// Draws the list box plus a blue bar behind each line CopyData marked; returns the last draw result
long EESEUITechListBox::DrawPolygons()
{
    GERhombus theRhombus;
    GERectangle theRectangle;
    long theResult = UILBString::DrawPolygons();
    if (this->mDependents.size() > 0) {
        theRhombus.SetColor(GEColor(0.0f, 0.0f, 1.0f));
        theRectangle.SetLeft(this->GetCopyTextRectangle().GetLeft());
        theRectangle.SetRight(this->GetCopyTextRectangle().GetRight());
        for (unsigned long i = 0; i < this->mDependents.size(); i++) {
            theRectangle.SetTop(this->GetCopyTextRectangle().GetTop() + (float)(this->mDependents[i] * this->GetLineHeight()));
            theRectangle.SetBottom(theRectangle.GetTop() + (float)this->GetLineHeight());
            theRhombus.SetRectangle(theRectangle);
            theResult = this->GetRasterizer()->Draw(&theRhombus, this->GetScreenIndex());
        }
    }
    return theResult;
}

// NEAR: mId is read early (an accessor at the first use only?), and the SetScenEditorObsoleted loop's bound is
// recomputed from _First every pass
// Moves the techs of lines inFirst..inLast to the other list (disables or enables them for the current player, which
// is marked changed if one is in a reached epoch), refills both lists and reselects a nearby tech and the other's
void EESEUITechListBox::Fill(long inFirst, long inLast)
{
    long theTech = 100000;
    EESEUITechListBox* theOther =
        static_cast<EESEUITechListBox*>(gUIManager->GetScenarioEditorForm()->GetControlById(this->GetId() == kSERBUnitTypesTechList ? 0x155 : 0x154));
    EETechTree* theTree = gUIManager->mScenarioEditorForm->GetPlayerScreen()->GetCurrentPlayer()->GetTechTree();
    bool theEpochReached = false;
    unsigned long theSelection;
    long theOtherTech;
    std::vector<long>::iterator it;

    if (theOther->GetCurrentSelection(theSelection) == kErrorOK)
        theOtherTech = theOther->mTechs[theSelection];
    else
        theOtherTech = 100000;

    for (it = this->mTechs.begin() + inFirst; it != this->mTechs.begin() + (inLast + 1); it++) {
        theTree->Get(*it)->SetScenEditorObsoleted(this->GetId() == kSERBUnitTypesTechList, true);
        if (theTree->Get(*it)->mEpoch <= theTree->mCurrentEpoch)
            theEpochReached = true;
    }
    if (theEpochReached == true) {
        unsigned long thePlayer = theTree->GetPlayer()->mPlayerID;
        gUIManager->mScenarioEditorForm->GetPlayerScreen()->MarkPlayerChanged(thePlayer, false);
    }

    for (it = this->mTechs.begin() + inLast; it != this->mTechs.end(); it++) {
        if (theTree->Get(*it)->mScenEditorObsoleted == (this->GetId() != kSERBUnitTypesTechList)) {
            theTech = *it;
            break;
        }
    }
    if (theTech == 100000) {
        for (it = this->mTechs.begin() + inFirst; it != this->mTechs.begin(); it--) {
            if (theTree->Get(*it)->mScenEditorObsoleted == (this->GetId() != kSERBUnitTypesTechList)) {
                theTech = *it;
                break;
            }
        }
    }
    gUIManager->mScenarioEditorForm->GetPlayerScreen()->FillTechLists();

    if (theTech < 100000) {
        it = std::find(this->mTechs.begin(), this->mTechs.end(), theTech);
        this->ChangeSelectionIndex(it - this->mTechs.begin(), false, true);
    } else
        this->ChangeSelectionIndex(0, false, true);
    if (theOtherTech < 100000) {
        it = std::find(theOther->mTechs.begin(), theOther->mTechs.end(), theOtherTech);
        theOther->ChangeSelectionIndex(it - theOther->mTechs.begin(), false, true);
    } else
        theOther->ChangeSelectionIndex(0, false, true);
}

// Sorts the techs (by epoch first when Display Epochs is checked, then by the tech's name on the Technologies page,
// else by its unit's name) and rewrites the lines, "<epoch>: <name>" when by epoch; scrolls to the selection
void EESEUITechListBox::Rebuild()
{
    UWideString theText;
    UWideString theName;
    unsigned long theSelection;
    long theCount = this->mTechs.size();
    bool theByEpoch = static_cast<UICheckBox*>(this->GetForm()->GetControlById(kSERBUnitTypesDisplayEpoch))->GetIsChecked();
    bool theByTech = static_cast<UIRadioButton*>(this->GetForm()->GetControlById(0x14d))->GetIsChecked();
    long i;

    if (theByEpoch) {
        if (!theByTech)
            std::sort(this->mTechs.begin(), this->mTechs.end(), EESETLEpochUnitLess());
        else
            std::sort(this->mTechs.begin(), this->mTechs.end(), EESETLEpochTechLess());
    } else {
        if (!theByTech)
            std::sort(this->mTechs.begin(), this->mTechs.end(), EESETLUnitLess());
        else
            std::sort(this->mTechs.begin(), this->mTechs.end(), EESETLTechLess());
    }
    this->DeleteAllEntries();
    if (theByEpoch) {
        for (i = 0; i < theCount; i++) {
            EETechTreeNode* theNode = gTechTree.Get(this->mTechs[i]);
            sprintf(theText, "%ld: ", EESETLGetEpoch(theNode->mData->mID) + 1);
            if (!theByTech)
                gResource->GetString(gUnitClassTable[theNode->mData->mUnitClass]->mNameStringID, theName);
            else
                gResource->GetString(theNode->mData->mNameID, theName);
            theText += theName;
            this->AppendLine(theText);
        }
    } else {
        for (i = 0; i < theCount; i++) {
            EETechTreeNode* theNode = gTechTree.Get(this->mTechs[i]);
            if (!theByTech)
                gResource->GetString(gUnitClassTable[theNode->mData->mUnitClass]->mNameStringID, theText);
            else
                gResource->GetString(theNode->mData->mNameID, theText);
            this->AppendLine(theText);
        }
    }
    if (this->GetLineCount() > 0) {
        this->GetCurrentSelection(theSelection);
        this->SetStartingDisplayLine(theSelection);
    }
    // epoch loop's mTechs[i]; 11 units at the end make it exact
}

// Returns the player chosen in the player tab's player combo box (+0xd8; its selection + 1 indexes gWorld.mPlayers)
// The original's COMDAT closes this unit (inline in the real header, emitted after its first user); defined here
// out of line at the same place, because ee/EESEPlayerScreen.h does not include EEWorld.h
EEPlayer* EESEPlayerScreen::GetCurrentPlayer()
{
    unsigned long theSelection;
    this->GetPlayerSelection()->GetCurrentSelection(theSelection);
    return gWorld.mPlayers[theSelection + 1];
}
