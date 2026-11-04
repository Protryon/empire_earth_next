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
#include "ee/EESEEChangeEffectAction.h"
#include "ee/EESEEGCPlace.h"
#include "ee/EESEEPlace.h"
#include "ee/EESEEvents.h"
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
#include "ee/EESETSEffectType.h"
#include "lle/GE3DPoint.h"
#include "lle/GE3DLine.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEMap.h"
#include "ee/EEMouse.h"
#include "ee/EEGraphics.h"
#include "ee/EEAScroll.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "ee/EESMArea.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EESMObject.h"
#include "ee/EESEPlayerScreen.h"
#include "ee/EEMessage.h"
#include "ee/EESETSConditionControls.h"
#include "ee/EEALasso.h"
#include "ee/EEAMouseClick.h"
#include "ee/EESEALasso.h"
#include "ee/EESEAMouseClick.h"
#include "ee/EESETSAMouseClick.h"
#include "ee/EESETSALasso.h"
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
    return (inID >= kTechEpochPrehistoric && inID < kTechEpochAll) ? inID : inID / 1000 - 1;
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

// FUNCTION: EXE 0x59764b
// Creates a tech list box of the player tab's tech tree page, with a dark green selection highlight
EESEUITechListBox::EESEUITechListBox(unsigned long inID, std::vector<UWideString>& inStrings, GEFont* inFont,
                                     bool inAutoSelect, const GE2DPoint& inLocation, unsigned long inWidth,
                                     unsigned long inHeight, UIForm* inForm, unsigned long inIndex,
                                     unsigned long inBorderThickness)
    : UILBString(inID, inStrings, inFont, inAutoSelect, GERectangle(inLocation, (float)inWidth, (float)inHeight),
                 inForm, inIndex, inBorderThickness)
{
    this->mSelectedBackground->SetColor(GEColor(0.0f, 0.38f, 0.0f));
}

// FUNCTION: EXE 0x597709
// Appends a tech id to the list's techs (Rebuild writes the lines)
void EESEUITechListBox::AddTech(long inID)
{
    this->mTechs.push_back(inID);
}

// NEAR: inline budget: the original refuses _Ptrit::operator+ in the first two mTechs[] (the counter in memory,
// three iterator temporaries); ours expands them
// FUNCTION: EXE 0x59771c
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
        if (this->mScrollBar != NULL) {
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
                if (this->mId == kSERBUnitTypesTechList) {
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

// FUNCTION: EXE 0x59787b
// Returns the scenario editor's player tab (EEUIFScenarioEditor::GetPlayerScreen, inline in
// ee/EEUIFScenarioEditor.h; refused and emitted here, its first user)

// FUNCTION: EXE 0x597885
// Empties the list's techs and lines
void EESEUITechListBox::Clear()
{
    this->mTechs.clear();
    this->DeleteAllEntries();
}

// FUNCTION: EXE 0x59789f
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
            theRectangle.SetTop(this->GetCopyTextRectangle().GetTop() + (float)(this->mDependents[i] * this->mLineHeight));
            theRectangle.SetBottom(theRectangle.GetTop() + (float)this->mLineHeight);
            theRhombus.SetRectangle(theRectangle);
            theResult = this->GetRasterizer()->Draw(&theRhombus, this->GetScreenIndex());
        }
    }
    return theResult;
}

// NEAR: mId is read early (an accessor at the first use only?), and the SetScenEditorObsoleted loop's bound is
// recomputed from _First every pass
// FUNCTION: EXE 0x5979c1
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
        if (theTree->Get(*it)->mScenEditorObsoleted == (this->mId != kSERBUnitTypesTechList)) {
            theTech = *it;
            break;
        }
    }
    if (theTech == 100000) {
        for (it = this->mTechs.begin() + inFirst; it != this->mTechs.begin(); it--) {
            if (theTree->Get(*it)->mScenEditorObsoleted == (this->mId != kSERBUnitTypesTechList)) {
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

// FUNCTION: EXE 0x597c9f
// Sorts the techs (by epoch first when Display Epochs is checked, then by the tech's name on the Technologies page,
// else by its unit's name) and rewrites the lines, "<epoch>: <name>" when by epoch; scrolls to the selection
void EESEUITechListBox::Rebuild()
{
    UWideString theText;
    UWideString theName;
    unsigned long theSelection;
    long theCount = this->mTechs.size();
    bool theByEpoch = static_cast<UICheckBox*>(this->mForm->GetControlById(kSERBUnitTypesDisplayEpoch))->GetIsChecked();
    bool theByTech = static_cast<UIRadioButton*>(this->mForm->GetControlById(0x14d))->GetIsChecked();
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
            sprintf(theText, "%d: ", EESETLGetEpoch(theNode->mData->mID) + 1);
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

// FUNCTION: EXE 0x597f4a
// Sorts a tech id list by epoch, then unit name (std::sort over vector<long>)
// (the four sorts' helpers follow in reference order)

// FUNCTION: EXE 0x598007
// Sorts a tech id list by epoch, then tech name (std::sort over vector<long>)

// FUNCTION: EXE 0x5980c4
// Sorts a tech id list by unit name (std::sort over vector<long>)

// FUNCTION: EXE 0x598181
// Sorts a tech id list by tech name (std::sort over vector<long>)

// FUNCTION: EXE 0x59823e
// Insertion-sorts a short tech id range by epoch, then unit name (std::_Insertion_sort)

// FUNCTION: EXE 0x598267
// Quicksorts a tech id range by epoch, then unit name, heap-sorting when the recursion gets too deep (std::_Sort)

// FUNCTION: EXE 0x59833a
// Insertion-sorts the rest of a tech id range by epoch, then unit name, unguarded (std::_Unguarded_insertion_sort)

// FUNCTION: EXE 0x59836b
// Insertion-sorts a short tech id range by epoch, then tech name (std::_Insertion_sort)

// FUNCTION: EXE 0x598394
// Quicksorts a tech id range by epoch, then tech name, heap-sorting when the recursion gets too deep (std::_Sort)

// FUNCTION: EXE 0x598467
// Insertion-sorts the rest of a tech id range by epoch, then tech name, unguarded (std::_Unguarded_insertion_sort)

// FUNCTION: EXE 0x598498
// Insertion-sorts a short tech id range by unit name (std::_Insertion_sort)

// FUNCTION: EXE 0x5984c1
// Quicksorts a tech id range by unit name, heap-sorting when the recursion gets too deep (std::_Sort)

// FUNCTION: EXE 0x598594
// Insertion-sorts the rest of a tech id range by unit name, unguarded (std::_Unguarded_insertion_sort)

// FUNCTION: EXE 0x5985c5
// Insertion-sorts a short tech id range by tech name (std::_Insertion_sort)

// FUNCTION: EXE 0x5985ee
// Quicksorts a tech id range by tech name, heap-sorting when the recursion gets too deep (std::_Sort)

// FUNCTION: EXE 0x5986c1
// Insertion-sorts the rest of a tech id range by tech name, unguarded (std::_Unguarded_insertion_sort)

// FUNCTION: EXE 0x5986f2
// Insertion-sorts a tech id range by epoch, then unit name (std::_Insertion_sort_1, _Insertion_sort's worker)

// FUNCTION: EXE 0x59879d
// Returns whether a tech id sorts first, by epoch, then unit name (EESETLEpochUnitLess::operator(), refused inline)

// FUNCTION: EXE 0x5988b9
// Builds a heap of tech ids by epoch, then unit name (std::make_heap)

// FUNCTION: EXE 0x5988f3
// Sorts a heap of tech ids by epoch, then unit name (std::sort_heap)

// FUNCTION: EXE 0x598934
// Partitions tech ids around a median of three, by epoch, then unit name; returns the split (std::_Unguarded_partition)

// FUNCTION: EXE 0x598a45
// Moves one tech id back to its sorted place by epoch, then unit name (std::_Unguarded_insert)

// FUNCTION: EXE 0x598a75
// Insertion-sorts a tech id range by epoch, then tech name (std::_Insertion_sort_1, _Insertion_sort's worker)

// FUNCTION: EXE 0x598b20
// Returns whether a tech id sorts first, by epoch, then tech name (EESETLEpochTechLess::operator(), refused inline)

// FUNCTION: EXE 0x598c22
// Builds a heap of tech ids by epoch, then tech name (std::make_heap)

// FUNCTION: EXE 0x598c5c
// Sorts a heap of tech ids by epoch, then tech name (std::sort_heap)

// FUNCTION: EXE 0x598c9d
// Partitions tech ids around a median of three, by epoch, then tech name; returns the split (std::_Unguarded_partition)

// FUNCTION: EXE 0x598dae
// Moves one tech id back to its sorted place by epoch, then tech name (std::_Unguarded_insert)

// FUNCTION: EXE 0x598dde
// Insertion-sorts a tech id range by unit name (std::_Insertion_sort_1, _Insertion_sort's worker)

// FUNCTION: EXE 0x598e89
// Returns whether a tech id sorts first, by unit name (EESETLUnitLess::operator(), refused inline)

// FUNCTION: EXE 0x598f2e
// Builds a heap of tech ids by unit name (std::make_heap)

// FUNCTION: EXE 0x598f68
// Sorts a heap of tech ids by unit name (std::sort_heap)

// FUNCTION: EXE 0x598fa9
// Partitions tech ids around a median of three, by unit name; returns the split (std::_Unguarded_partition)

// FUNCTION: EXE 0x5990ba
// Moves one tech id back to its sorted place by unit name (std::_Unguarded_insert)

// FUNCTION: EXE 0x5990ea
// Insertion-sorts a tech id range by tech name (std::_Insertion_sort_1, _Insertion_sort's worker)

// FUNCTION: EXE 0x599195
// Returns whether a tech id sorts first, by tech name (EESETLTechLess::operator(), refused inline)

// FUNCTION: EXE 0x599220
// Builds a heap of tech ids by tech name (std::make_heap)

// FUNCTION: EXE 0x59925a
// Sorts a heap of tech ids by tech name (std::sort_heap)

// FUNCTION: EXE 0x59929b
// Partitions tech ids around a median of three, by tech name; returns the split (std::_Unguarded_partition)

// FUNCTION: EXE 0x5993ac
// Moves one tech id back to its sorted place by tech name (std::_Unguarded_insert)

// FUNCTION: EXE 0x5993dc
// Heapifies a tech id range by epoch, then unit name (std::_Make_heap, make_heap's worker)

// FUNCTION: EXE 0x599420
// Moves the top tech id of a heap by epoch, then unit name to its end (std::pop_heap)

// FUNCTION: EXE 0x599449
// Heapifies a tech id range by epoch, then tech name (std::_Make_heap, make_heap's worker)

// FUNCTION: EXE 0x59948d
// Moves the top tech id of a heap by epoch, then tech name to its end (std::pop_heap)

// FUNCTION: EXE 0x5994b6
// Heapifies a tech id range by unit name (std::_Make_heap, make_heap's worker)

// FUNCTION: EXE 0x5994fa
// Moves the top tech id of a heap by unit name to its end (std::pop_heap)

// FUNCTION: EXE 0x599523
// Heapifies a tech id range by tech name (std::_Make_heap, make_heap's worker)

// FUNCTION: EXE 0x599567
// Moves the top tech id of a heap by tech name to its end (std::pop_heap)

// FUNCTION: EXE 0x599590
// Sifts a tech id down a heap by epoch, then unit name (std::_Adjust_heap)

// FUNCTION: EXE 0x5995fd
// Pops the top tech id of a heap by epoch, then unit name (std::_Pop_heap_0, pop_heap's worker)

// FUNCTION: EXE 0x599650
// Sifts a tech id down a heap by epoch, then tech name (std::_Adjust_heap)

// FUNCTION: EXE 0x5996bd
// Pops the top tech id of a heap by epoch, then tech name (std::_Pop_heap_0, pop_heap's worker)

// FUNCTION: EXE 0x599710
// Sifts a tech id down a heap by unit name (std::_Adjust_heap)

// FUNCTION: EXE 0x59977d
// Pops the top tech id of a heap by unit name (std::_Pop_heap_0, pop_heap's worker)

// FUNCTION: EXE 0x5997d0
// Sifts a tech id down a heap by tech name (std::_Adjust_heap)

// FUNCTION: EXE 0x59983d
// Pops the top tech id of a heap by tech name (std::_Pop_heap_0, pop_heap's worker)

// FUNCTION: EXE 0x599890
// Sifts a tech id up a heap by epoch, then unit name (std::_Push_heap)

// FUNCTION: EXE 0x5998e1
// Moves a heap's top tech id to a slot and re-sifts the old value by epoch, then unit name (std::_Pop_heap)

// FUNCTION: EXE 0x599915
// Sifts a tech id up a heap by epoch, then tech name (std::_Push_heap)

// FUNCTION: EXE 0x599966
// Moves a heap's top tech id to a slot and re-sifts the old value by epoch, then tech name (std::_Pop_heap)

// FUNCTION: EXE 0x59999a
// Sifts a tech id up a heap by unit name (std::_Push_heap)

// FUNCTION: EXE 0x5999eb
// Moves a heap's top tech id to a slot and re-sifts the old value by unit name (std::_Pop_heap)

// FUNCTION: EXE 0x599a1f
// Sifts a tech id up a heap by tech name (std::_Push_heap)

// FUNCTION: EXE 0x599a70
// Moves a heap's top tech id to a slot and re-sifts the old value by tech name (std::_Pop_heap)

// SYNTHETIC: EXE 0x599aa4
// (EESEUITechListBox's scalar deleting destructor)

// FUNCTION: EXE 0x599ac0
// Destroys the tech list box (inline in the header; the out-of-line copy its scalar deleting destructor calls)

// FUNCTION: EXE 0x599ae9
// Returns the player chosen in the player tab's player combo box (+0xd8; its selection + 1 indexes gWorld.mPlayers)
// The original's COMDAT closes this unit (inline in the real header, emitted after its first user); defined here
// out of line at the same place, because ee/EESEPlayerScreen.h does not include EEWorld.h
EEPlayer* EESEPlayerScreen::GetCurrentPlayer()
{
    unsigned long theSelection;
    this->mPlayerSelection->GetCurrentSelection(theSelection);
    return gWorld.mPlayers[theSelection + 1];
}
