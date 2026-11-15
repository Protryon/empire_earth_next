// TEST ONLY: the civilization builder's steps of the scripted scenario editor gate (EE-SIM.exe --editor-script; the
// "cb" command of EESimEditor.cpp; tools/sim_editor.py --script tools/sim_editor/civbuilder.txt). Port addition, no
// original address.
//
// The builder is opened as a person opens it (the editor's civilization builder button: the creator over the
// editor's civilization; or the game's paths: the edit of the current player's, the civilization timer), then driven
// through its forms' controls (7 the builder, 15 the name dialog, 16 save, 17 load, and the message boxes), and
// digested through what a person sees and what the game keeps: every control of a form (texts, lists, selections,
// flags), the forms on the stack, the editor's and the players' civilizations, the edited civilization, and the
// civilization files' bytes.
//   cb open                         the editor's civilization builder button released (EESEEOpenCivBuilder)
//   cb edit | cb timer              CivBuilderEditCivilization | EEProcessTimerExpired (the game's paths)
//   cb blink 0|1                    EESetCivBuilderButtonBlinking
//   cb fire FORM ID                 a form's button released (ButtonUp, as by the mouse); ID 0x.. or decimal
//   cb event FORM ID TRIGGER        a control's trigger event (0x3e8.., e.g. 0x3f0 a list's release) processed
//   cb pick FORM ID LINE            a list's line selected with its event (its ChangeSelectionIndex)
//   cb pickname FORM ID NAME        a list's line NAME selected with its event
//   cb reset FORM ID                a list's ResetSelection; cb tab FORM ID: its TabbedTo
//   cb copy FORM ID                 a list's CopyData and its snapshot
//   cb text FORM ID TEXT            an edit box's text ('_' a space, '-' none)
//   cb answer ID                    the message box on top: its button ID (1 OK/Yes, 2 No) released
//   cb form FORM                    the form's controls (CivFormDigest); cb top: the form on top's
//   cb state                        the stack, the editor's and the current player's civilizations, the edited one
//   cb file NAME | cb rm NAME       Users\default\Civilizations\NAME.civ: its bytes as hex | deleted
//   cb put NAME HEX                 such a file written from hex bytes
//   cb save NAME 0|1                CivBuilderSaveCivilization (the campaign's path with 1)
//   cb load NAME                    EELoadCivilization from the user's directory: its result, name and upgrades
//   cb apply PLAYER NAME            EEApplyCivilizationFile to a player, then its civilization
//   cb flag freecam|custom|scenario 0|1   gFreeCamera, gAllowCustomCivs, gIsScenarioGame set
//   cb close                        the form on top popped
#include <string>
#include <vector>
#include <typeinfo>
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include <list>

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
#include "lle/UIListBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"

#include "lle/UIEditBox.h"
#include "lle/UIFileListBox.h"

#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EECivilization.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EECivilizationManager.h"
#include "ee/EECBUIListBox.h"
#include "ee/EECBEditableCivilization.h"
#include "ee/EECBCivilizationCreator.h"
#include "ee/EECivilizationBuilder.h"
#include "ee/EEAResetAllPlayerInfo.h"
#include "ee/EESimEditor.h"

namespace {

struct CivButtonAccess : UIButton {
    static void Up(UIButton* inButton) { (inButton->*(&CivButtonAccess::ButtonUp))(); }
};
struct CivListAccess : UILBString {
    static unsigned long Index(const UILBString* inList) { return (inList->*(&CivListAccess::GetSelectionIndex))(); }
    static bool Auto(const UILBString* inList) { return (inList->*(&CivListAccess::GetAutoSelect))(); }
    static bool Copied(const UILBString* inList) { return (inList->*(&CivListAccess::GetCopiedHasSelection))(); }
    static unsigned long Drawn(const UILBString* inList) { return (inList->*(&CivListAccess::GetDrawCount))(); }
};

std::string Hexed(const unsigned char* inData, size_t inLength)
{
    static const char kDigits[] = "0123456789abcdef";
    std::string theText;
    for (size_t i = 0; i < inLength; i++) {
        theText += kDigits[inData[i] >> 4];
        theText += kDigits[inData[i] & 15];
    }
    return theText;
}

std::string Text(const UWideString& inText)
{
    return std::string("'") + (const char*)inText + "'";
}

std::string Upgrades(const std::vector<EECivUpgrade>& inUpgrades)
{
    std::string theText = "[";
    for (size_t i = 0; i < inUpgrades.size(); i++)
        theText += (i ? "," : "") + std::to_string((unsigned long)inUpgrades[i]);
    return theText + "]";
}

std::string Name(const void* inObject, const std::type_info& inType)
{
    std::string theName = inObject == NULL ? "null" : inType.name();
    if (theName.compare(0, 6, "class ") == 0)
        theName.erase(0, 6);
    return theName;
}

std::string Colour(const GEColor& inColor)
{
    char theText[64];
    snprintf(theText, sizeof(theText), "%g,%g,%g", (double)inColor.mRed, (double)inColor.mGreen, (double)inColor.mBlue);
    return theText;
}

unsigned long Number(const std::string& inText)
{
    return strtoul(inText.c_str(), NULL, 0);
}

UIForm* Form(const std::string& inId)
{
    return gUIManager->GetFormByID(Number(inId));
}

UIControl* Control(const std::string& inForm, const std::string& inId)
{
    UIForm* theForm = Form(inForm);
    return theForm == NULL ? NULL : theForm->GetControlById(Number(inId));
}

// a form's controls (TEST ONLY): index, class, id, display mode, then a label's lines, an edit box's text, a
// list's flags, selection and every line, a button's trigger event
void CivFormDigest(UIForm* inForm, std::vector<std::string>& outLines)
{
    if (inForm == NULL) {
        outLines.push_back("  no form");
        return;
    }
    outLines.push_back("  form " + std::to_string(inForm->GetFormID()) + " " + Name(inForm, typeid(*inForm)) + " controls " +
                       std::to_string(inForm->GetControlCount()));
    for (unsigned long i = 0; i < inForm->GetControlCount(); i++) {
        UIControl* c = inForm->GetControl(i);
        if (c == NULL)
            continue;
        std::string theLine = "  c" + std::to_string(i) + " " + Name(c, typeid(*c)) + " id=" + std::to_string(c->GetId()) +
                              " m=" + std::to_string(c->GetDisplayMode());
        if (EECBUIListBox* b = dynamic_cast<EECBUIListBox*>(c)) {
            unsigned long theSelection = 0;
            bool theSelected = b->GetCurrentSelection(theSelection) == kErrorOK;
            theLine += std::string(" upgradesonly=") + (b->mSelectUpgradesOnly ? "1" : "0") + " points=" +
                       (b->mShowPoints ? "1" : "0") + " sort=" + (b->mSortLines ? "1" : "0") + " tab=" +
                       (b->GetTabable() ? "1" : "0") + " auto=" + (CivListAccess::Auto(b) ? "1" : "0") + " has=" +
                       (b->GetHasSelection() ? "1" : "0") + " ix=" + std::to_string(CivListAccess::Index(b)) + " hc=" +
                       Colour(b->mC13C) + " n=" + std::to_string(b->GetLineCount()) + " sel=" +
                       (theSelected ? std::to_string(theSelection) : std::string("none"));
            outLines.push_back(theLine);
            for (unsigned long k = 0; k < b->GetLineCount(); k++)
                outLines.push_back("    " + std::to_string(k) + " " + Text(b->GetData(k)));
            continue;
        }
        if (UILBString* b = dynamic_cast<UILBString*>(c)) {
            unsigned long theSelection = 0;
            bool theSelected = b->GetCurrentSelection(theSelection) == kErrorOK;
            theLine += " n=" + std::to_string(b->GetLineCount()) + " sel=" +
                       (theSelected ? std::to_string(theSelection) : std::string("none"));
            for (unsigned long k = 0; k < b->GetLineCount() && k < 8; k++)
                theLine += " " + Text(b->GetData(k));
        } else if (UIEditBox* b = dynamic_cast<UIEditBox*>(c)) {
            theLine += " lines=" + std::to_string(b->GetLineCount()) + " " +
                       (b->GetLineCount() > 0 ? Text(b->GetText(0)) : std::string("''"));
        } else if (UITextLabel* b = dynamic_cast<UITextLabel*>(c)) {
            theLine += " lines=" + std::to_string(b->GetLineCount());
            for (unsigned long k = 0; k < b->GetLineCount() && k < 4; k++)
                theLine += " " + Text(b->GetText(k));
        }
        for (unsigned long t = kControlTriggerRangeChanged; t <= kControlTriggerRightButtonUp; t++)
            if (UIEvent* e = c->GetTriggerEvent(t))
                theLine += " e" + std::to_string(t - kControlTriggerRangeChanged) + "=" + Name(e, typeid(*e));
        outLines.push_back(theLine);
    }
}

std::string CivDigest(const char* inLabel, EECivilization* inCiv)
{
    if (inCiv == NULL)
        return std::string("  ") + inLabel + " none";
    return std::string("  ") + inLabel + " name=" + Text(inCiv->mName) + " points=" +
           std::to_string(inCiv->mAvailablePoints) + " max=" + std::to_string(inCiv->mMaxPoints) +
           " unchanged=" + (inCiv->mUnchanged ? "1" : "0") + " nopoints=" + (inCiv->mNoPointsAdded ? "1" : "0") +
           " bought=" + Upgrades(inCiv->mPurchasedUpgrades) + " deferred=" + Upgrades(inCiv->mDeferredUpgrades);
}

// the edited civilization (gEditableCivilization) as the builder keeps it
std::string EditDigest()
{
    EECBEditableCivilization* e = gEditableCivilization;
    if (e == NULL)
        return "  edit none";
    EECBCivilizationCreator* theCreator = dynamic_cast<EECBCivilizationCreator*>(e);
    std::string theKind = theCreator == NULL ? "edit" : theCreator->mIsUnlimited ? "create-unlimited" : "create";
    return "  edit " + theKind + " name=" + Text(*e->GetName()) + " left=" + std::to_string(e->GetPointsLeft()) +
           " max=" + std::to_string(e->GetMaxPoints()) + " raw=" + std::to_string(e->mPointsLeft) + " player=" +
           std::to_string(e->mPlayer) + " choices=" + (e->HasChoices() ? "1" : "0") + " canchange=" +
           (e->CanChange() ? "1" : "0") + " upgrades=" + Upgrades(e->mUpgrades) + " ids=" + Upgrades(e->CopyUpgradeIDs());
}

UString UserFile(const std::string& inName)
{
    return CivBuilderGetUserDirectory() + UString(inName.c_str()) + ".civ";
}

FSFileSpec UserSpec(const std::string& inName)
{
    FSFileSpec theFile;
    theFile.SetVolume(gStartupVolume);
    theFile.SetDirectory(CivBuilderGetUserDirectory());
    theFile.SetFile(UString(inName.c_str()) + ".civ");
    return theFile;
}

std::string Word(const std::string& inWord)
{
    std::string theText = inWord == "-" ? std::string() : inWord;
    for (char& c : theText)
        if (c == '_')
            c = ' ';
    return theText;
}

}  // namespace

// One "cb" step (on the UI thread, in the step's action): false for an unknown sub-command
bool EESimCivBuilderStep(const std::vector<std::string>& w, std::vector<std::string>& outLines)
{
    const std::string c = w.size() > 1 ? w[1] : std::string();
    if (c == "open") {
        UIControl* theButton = gUIManager->mScenarioEditorForm->GetControlById(17);
        if (UIEvent* theEvent = theButton->GetTriggerEvent(kControlTriggerButtonUp))
            theEvent->ProcessEvent(gUIManager->mScenarioEditorForm, theButton);
    } else if (c == "edit") {
        outLines.push_back("  edit -> " + std::to_string(CivBuilderEditCivilization()));
    } else if (c == "timer") {
        outLines.push_back("  timer -> " + std::to_string(EEProcessTimerExpired()));
    } else if (c == "blink") {
        outLines.push_back("  blink -> " + std::to_string(EESetCivBuilderButtonBlinking(Number(w[2]) != 0)));
        UIForm* theMain = gUIManager->GetFormByID(kFormMainGame);
        for (unsigned long k : {(unsigned long)kMainGameCivSelectorBuilder, (unsigned long)kMainGameLargeCivBuilderAlert}) {
            UIControl* theButton = theMain == NULL ? NULL : theMain->GetControl(k);
            outLines.push_back("  main c" + std::to_string(k) + " " +
                               (theButton == NULL ? std::string("none") : "m=" + std::to_string(theButton->GetDisplayMode())));
        }
    } else if (c == "fire") {
        UIButton* theButton = dynamic_cast<UIButton*>(Control(w[2], w[3]));
        if (theButton == NULL)
            outLines.push_back("  fire: no button");
        else
            CivButtonAccess::Up(theButton);
    } else if (c == "event") {   // cb event FORM ID TRIGGER: a control's trigger event (0x3e8..) processed
        UIControl* theControl = Control(w[2], w[3]);
        UIEvent* theEvent = theControl == NULL ? NULL : theControl->GetTriggerEvent(Number(w[4]));
        if (theEvent == NULL)
            outLines.push_back("  event: none");
        else
            outLines.push_back("  event -> " + std::to_string(theEvent->ProcessEvent(Form(w[2]), theControl)));
    } else if (c == "pick") {
        UILBString* theList = dynamic_cast<UILBString*>(Control(w[2], w[3]));
        if (theList == NULL)
            outLines.push_back("  pick: no list");
        else
            theList->ChangeSelectionIndex(Number(w[4]), true, false);
    } else if (c == "pickname") {   // cb pickname FORM ID NAME: a file list's line NAME selected with its event
        UILBString* theList = dynamic_cast<UILBString*>(Control(w[2], w[3]));
        unsigned long k = 0;
        while (theList != NULL && k < theList->GetLineCount() && !(theList->GetData(k) == Word(w[4]).c_str()))
            k++;
        if (theList == NULL || k == theList->GetLineCount())
            outLines.push_back("  pickname: none");
        else {
            outLines.push_back("  pickname line " + std::to_string(k));
            theList->ChangeSelectionIndex(k, true, false);
        }
    } else if (c == "reset" || c == "tab") {
        EECBUIListBox* theList = dynamic_cast<EECBUIListBox*>(Control(w[2], w[3]));
        if (theList == NULL)
            outLines.push_back("  " + c + ": no list");
        else if (c == "reset")
            theList->ResetSelection();
        else
            theList->TabbedTo();
    } else if (c == "copy") {   // cb copy FORM ID: a list's CopyData, then its snapshot's flags
        EECBUIListBox* theList = dynamic_cast<EECBUIListBox*>(Control(w[2], w[3]));
        if (theList == NULL)
            outLines.push_back("  copy: no list");
        else {
            long theResult = theList->CopyData();
            outLines.push_back("  copy -> " + std::to_string(theResult) + " points=" +
                               (theList->mCopiedShowPoints ? "1" : "0") + " hc=" + Colour(theList->mC148) + " has=" +
                               (CivListAccess::Copied(theList) ? "1" : "0") + " drawn=" +
                               std::to_string(CivListAccess::Drawn(theList)));
        }
    } else if (c == "text") {
        UIEditBox* theEdit = dynamic_cast<UIEditBox*>(Control(w[2], w[3]));
        if (theEdit == NULL)
            outLines.push_back("  text: no edit box");
        else
            theEdit->SetText(UWideString(Word(w[4]).c_str()));
    } else if (c == "answer") {
        UIForm* theForm = gUIManager->GetCurrentForm();
        UIButton* theButton = theForm == NULL ? NULL : dynamic_cast<UIButton*>(theForm->GetControlById(Number(w[2])));
        if (theButton == NULL)
            outLines.push_back("  answer: no button");
        else
            CivButtonAccess::Up(theButton);
    } else if (c == "form") {
        CivFormDigest(Form(w[2]), outLines);
    } else if (c == "top") {
        CivFormDigest(gUIManager->GetCurrentForm(), outLines);
    } else if (c == "state") {
        std::string theStack = "  stack";
        for (unsigned long i = 0; i < gUIManager->GetFormCount(); i++) {
            UIForm* f = gUIManager->GetStackedForm(i);
            theStack += " " + (f == NULL ? std::string("null") : std::to_string(f->GetFormID()));
        }
        outLines.push_back(theStack);
        outLines.push_back(CivDigest("editor", gUIManager->mScenarioEditorForm != NULL
                                                   ? &gUIManager->mScenarioEditorForm->mC1A0 : NULL));
        EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
        outLines.push_back(CivDigest("player", thePlayer != NULL ? &thePlayer->mCivilization : NULL));
        if (thePlayer != NULL)
            outLines.push_back(std::string("  chosen=") + (thePlayer->mCivilizationChosen ? "1" : "0") + " locked=" +
                               (thePlayer->mCivilizationLocked ? "1" : "0") + " file=" + Text(UWideString((const char*)thePlayer->mCivilizationFile)));
        outLines.push_back(EditDigest());
        outLines.push_back("  last=" + Text(UWideString((const char*)gLastCivilizationFile)));
    } else if (c == "file") {
        FSFileSpec theFile = UserSpec(w[2]);
        std::string theLine = "  file " + w[2];
        unsigned long theSize = 0;
        if (theFile.Open(kFileAccessRead) != kErrorOK || theFile.GetLength(theSize) != kErrorOK) {
            theLine += " missing";
        } else {
            std::vector<unsigned char> theBytes(theSize + 1);
            unsigned long theRead = 0;
            theFile.Read(theBytes.data(), theSize, theRead);
            theLine += " " + std::to_string(theRead) + " " + Hexed(theBytes.data(), theRead);
        }
        theFile.Close();
        outLines.push_back(theLine);
    } else if (c == "rm") {
        FSFileSpec theFile = UserSpec(w[2]);
        theFile.Delete();
    } else if (c == "put") {
        FSFileSpec theFile = UserSpec(w[2]);
        std::vector<unsigned char> theBytes;
        for (size_t i = 0; i + 1 < w[3].size(); i += 2)
            theBytes.push_back((unsigned char)strtoul(w[3].substr(i, 2).c_str(), NULL, 16));
        theFile.Delete();
        theFile.Create();
        unsigned long theWritten = 0;
        if (theFile.Open(kFileAccessWrite) == kErrorOK)
            theFile.Write(theBytes.data(), (unsigned long)theBytes.size(), theWritten);
        theFile.Close();
    } else if (c == "save") {
        outLines.push_back("  save -> " + std::to_string(CivBuilderSaveCivilization(UString(Word(w[2]).c_str()),
                                                                                       Number(w[3]) != 0)));
    } else if (c == "load") {
        UWideString theName;
        std::vector<EECivUpgrade> theUpgrades;
        long theResult = EELoadCivilization(UString(Word(w[2]).c_str()), EEGetCivilizationDirectory(true), theName,
                                            theUpgrades);
        outLines.push_back("  load -> " + std::to_string(theResult) + " " + Text(theName) + " " + Upgrades(theUpgrades));
    } else if (c == "apply") {
        EEPlayer* thePlayer = gWorld.mPlayers[Number(w[2])];
        UString theName(Word(w[3]).c_str());
        outLines.push_back("  apply -> " + std::to_string(EEApplyCivilizationFile(theName, thePlayer)));
        outLines.push_back(CivDigest("applied", &thePlayer->mCivilization));
    } else if (c == "flag") {   // cb flag freecam|custom|scenario 0|1: the game's flags the builder reads
        bool& theFlag = w[2] == "freecam" ? gFreeCamera : w[2] == "custom" ? gAllowCustomCivs : gIsScenarioGame;
        theFlag = Number(w[3]) != 0;
    } else if (c == "close") {
        gUIManager->Pop();
    } else {
        return false;
    }
    return true;
}
