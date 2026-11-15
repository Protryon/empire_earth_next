// C:\EEX\Empire Earth\Campaign\EECampaignEditorBehavior.cpp (file name inferred: no __FILE__). The
// campaign editor's model (RTTI EECampaignEditorBehavior, vtable 0x44c058): 0x28d82d..0x28f0cb, the members in the
// header's order, Save's three checks, then the std::sort and vector erase COMDATs of vector<UWideString>. Unit
// flags: the exe's module set.
#include <windows.h>
#include <mbstring.h>
#include <set>
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
#include "lle/FSMemoryFile.h"
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

#include "ee/EmpireEarth.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EECampaignFormManager.h"
#include "ee/EECampaignEditorBehavior.h"
#include "ee/EEStringIDs.h"
// FUNCTION: EXE 0x68d82d
// Creates the campaign editor's model with an empty campaign, not new and not edited
EECampaignEditorBehavior::EECampaignEditorBehavior()
    : mIsNew(false), mPasswordEntered(false)
{
}

// SYNTHETIC: EXE 0x68d885
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x68d8a1
// Destroys the campaign editor's model
EECampaignEditorBehavior::~EECampaignEditorBehavior()
{
}

// FUNCTION: EXE 0x68d8d6
// Moves an available scenario to the end of the campaign's scenario list and marks the campaign edited; returns
// whether it was available
bool EECampaignEditorBehavior::AddScenario(const UWideString& inScenario)
{
    std::vector<UWideString> theScenarios;
    bool theResult = false;
    std::vector<UWideString>::iterator it = std::find(this->mAvailableScenarios.begin(), this->mAvailableScenarios.end(),
                                                      inScenario);
    if (it != this->mAvailableScenarios.end()) {
        this->mAvailableScenarios.erase(it);
        theScenarios = this->mCampaign.GetScenarios();
        theScenarios.push_back(inScenario);
        this->mCampaign.SetCampaignScenarios(theScenarios);
        theResult = true;
        this->mPasswordEntered = theResult;
    }
    return theResult;
}

// FUNCTION: EXE 0x68d98b
// Moves a campaign scenario by a number of places in the list and marks the campaign edited; returns false when it
// is not in the campaign or the new place is out of range
bool EECampaignEditorBehavior::MoveScenario(const UWideString& inScenario, long inDelta)
{
    std::vector<UWideString> theScenarios;
    UWideString theUnused;
    bool theFound = false;
    bool theResult = false;
    theScenarios = this->mCampaign.GetScenarios();
    long theIndex = 0;
    for (std::vector<UWideString>::iterator it = theScenarios.begin(); it != theScenarios.end() && !theFound; ++it) {
        if (*it == inScenario)
            theFound = true;
        theIndex++;
    }
    if (theFound) {
        long thePosition = theIndex + inDelta - 1;
        if (thePosition >= 0 && thePosition < (long)theScenarios.size()) {
            std::vector<UWideString>::iterator theFoundIt = std::find(theScenarios.begin(), theScenarios.end(),
                                                                       inScenario);
            theScenarios.erase(theFoundIt);
            if (thePosition > theIndex)
                thePosition--;
            theScenarios.insert(theScenarios.begin() + thePosition, inScenario);
            this->mCampaign.SetCampaignScenarios(theScenarios);
            theResult = true;
            this->mPasswordEntered = true;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x68dadc
// Starts a new campaign: default name, author, e-mail and description texts, no scenarios, no password; returns
// the first string-table error
long EECampaignEditorBehavior::New()
{
    UWideString theText;
    std::vector<UWideString> theScenarios;
    long theResult = gResource->GetString(IDS_EECAMPAIGN, theText);
    if (theResult == kErrorOK) {
        this->mCampaign.SetCampaignName(theText);
        theResult = gResource->GetString(IDS_SETUP_NONE_PROVIDED, theText);
        if (theResult == kErrorOK) {
            this->mCampaign.SetAuthorEmail(theText);
            theResult = gResource->GetString(IDS_SETUP_NONE_PROVIDED_1BD3, theText);
            if (theResult == kErrorOK) {
                this->mCampaign.SetAuthorName(theText);
                theResult = gResource->GetString(IDS_NO_DESCRIPTION_PROVIDED, theText);
                if (theResult == kErrorOK) {
                    this->mCampaign.SetCampaignDescription(theText);
                    theScenarios = this->mCampaign.GetScenarios();
                    for (std::vector<UWideString>::iterator it = theScenarios.begin(); it != theScenarios.end();
                         ++it)
                        this->RemoveScenario(*it);
                    this->mPassword = "";
                    this->mPasswordEntered = false;
                    this->mIsNew = true;
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x68dbfc
// Deletes a campaign file from the campaigns directory and drops it from the campaign list; returns whether it
// worked, with an error text otherwise
bool EECampaignEditorBehavior::Delete(const UWideString& inName, UWideString& outError)
{
    UWideString theName(inName);
    FSFileSpec theFile;
    UWideString theError;
    bool theResult = false;
    theFile.SetVolume(gStartupVolume);
    theFile.SetDirectory(gStartupDirectory + "data\\" + "Campaigns\\");
    theFile.SetFile(UString((const char*)inName));
    if (theFile.Delete() == kErrorOK) {
        if (theName.GetLength() > 4)
            theName.InsertString("", theName.GetLength() - 4, true);
        std::vector<UWideString>::iterator it = std::find(this->mScenarios.begin(), this->mScenarios.end(), theName);
        if (it != this->mScenarios.end())
            this->mScenarios.erase(it);
        this->mPasswordEntered = false;
        theResult = true;
    } else {
        gResource->GetString(IDS_CAMPAIGN_COULDNT_BE_DELETED, theError);
        outError = theError;
    }
    return theResult;
}

// FUNCTION: EXE 0x68dd83
// Returns the names of the campaigns on disk
void EECampaignEditorBehavior::GetScenarios(std::vector<UWideString>& outScenarios)
{
    outScenarios = this->mScenarios;
}

// FUNCTION: EXE 0x68dd96
// Returns the scenarios that are not in the campaign
void EECampaignEditorBehavior::GetAvailableScenarios(std::vector<UWideString>& outScenarios)
{
    outScenarios = this->mAvailableScenarios;
}

// FUNCTION: EXE 0x68dda9
// Returns whether the campaign may be edited: it is new, has no password, or the entered password matches
bool EECampaignEditorBehavior::GetIsUnlocked()
{
    UWideString thePassword(this->mCampaign.GetPassword());
    bool theResult = false;
    if (this->mIsNew || thePassword == "" || this->mPassword == thePassword)
        theResult = true;
    return theResult;
}

// FUNCTION: EXE 0x68de0c
// Returns the campaign's background image name
const UWideString& EECampaignEditorBehavior::GetBackgroundImage()
{
    return this->mCampaign.GetBackgroundImage();
}

// FUNCTION: EXE 0x68de14
// Returns the civilization the campaign requires
const UWideString& EECampaignEditorBehavior::GetRequiredCiv()
{
    return this->mCampaign.GetRequiredCiv();
}

// FUNCTION: EXE 0x68de1c
// Returns the campaign's scenario names
void EECampaignEditorBehavior::GetCampaignScenarios(std::vector<UWideString>& outScenarios)
{
    outScenarios = this->mCampaign.GetScenarios();
}

// FUNCTION: EXE 0x68de31
// Returns the author's e-mail address
UWideString EECampaignEditorBehavior::GetAuthorEmail()
{
    return this->mCampaign.GetAuthorEmail();
}

// FUNCTION: EXE 0x68de4b
// Returns the author's name
UWideString EECampaignEditorBehavior::GetAuthorName()
{
    return this->mCampaign.GetAuthorName();
}

// FUNCTION: EXE 0x68de65
// Returns the campaign description
UWideString EECampaignEditorBehavior::GetDescription()
{
    return this->mCampaign.GetDescription();
}

// FUNCTION: EXE 0x68de7f
// Returns the campaign name
UWideString EECampaignEditorBehavior::GetName()
{
    return this->mCampaign.GetCampaignName();
}

// FUNCTION: EXE 0x68de99
// Lists the campaign's scenario files with their media; returns whether all were found
bool EECampaignEditorBehavior::GetScenarioList(std::vector<UWideString>& outScenarios, UWideString& outError)
{
    return this->mCampaign.ListScenarioFiles(outScenarios, outError, false, false);
}

// FUNCTION: EXE 0x68deb0
// Rescans the user campaigns and the scenarios not in the campaign (sorted); warns once when a campaign scenario on
// disk differs from its archived copy; returns true
bool EECampaignEditorBehavior::Reset()
{
    std::set<UString> theFiles;
    std::set<UString>::const_iterator it;

    UWideString theName;
    UWideString theExtension;
    UWideString theMessage;
    UWideString theFormat;
    UString thePattern;
    FSFileSpec theDirectory;
    FSMemoryFile* theFile = NULL;
    FSFileSpec* theSpec = NULL;
    FSPath* theCampaignPath = new FSPath;
    FSPath* theScenarioPath = new FSPath;
    bool theWarned = false;
    this->mCampaign.SetDirectory((const char*)(gStartupDirectory + "data\\"));
    theDirectory.SetVolume(gStartupVolume);
    theDirectory.SetDirectory(gStartupDirectory + "data\\" + "Campaigns\\");
    theCampaignPath->AddDirectory(theDirectory, true);
    thePattern = UString("*") + gCampaignExtension;
    theCampaignPath->ListFiles(thePattern, theFiles);
    this->mScenarios.clear();
    for (it = theFiles.begin(); it != theFiles.end(); ++it) {
        theName = (const char*)*it;
        if (theName.GetLength() > 4) {
            theExtension = theName.GetIndexedSubstring(theName.GetLength() - 4, theName.GetLength() - 1);
            theExtension.ToLower();
            if (theExtension == (const char*)gCampaignExtension)
                theName = theName.GetIndexedSubstring(0, theName.GetLength() - 5);
        }
        if (EECampaignFormManager::GetBuiltInCampaign(theName) <= 0)
            this->mScenarios.push_back(theName);
    }
    theDirectory.SetVolume(gStartupVolume);
    theDirectory.SetDirectory(gStartupDirectory + "data\\" + "Scenarios\\");
    theScenarioPath->AddDirectory(theDirectory, true);
    thePattern = UString("*") + gScenarioExtension;
    theScenarioPath->ListFiles(thePattern, theFiles);
    this->mAvailableScenarios.clear();
    for (it = theFiles.begin(); it != theFiles.end(); ++it) {
        theName = (const char*)*it;
        if (theName.GetLength() > 4) {
            theExtension = theName.GetIndexedSubstring(theName.GetLength() - 4, theName.GetLength() - 1);
            theExtension.ToLower();
            if (theExtension == (const char*)gScenarioExtension)
                theName = theName.GetIndexedSubstring(0, theName.GetLength() - 5);
        }
        if (std::find(this->mCampaign.GetScenarios().begin(), this->mCampaign.GetScenarios().end(), theName)
            == this->mCampaign.GetScenarios().end()) {
            this->mAvailableScenarios.push_back(theName);
            std::sort(this->mAvailableScenarios.begin(), this->mAvailableScenarios.end());
        } else {
            unsigned long theSum = 0;
            long theError = theCampaignPath->FindFile(UString((const char*)(this->GetName() + (const char*)gCampaignExtension)),
                                                      theSpec);
            if (theError == kErrorOK) {
                FSArchive* theArchive = new FSArchive(*theSpec);
                if (theArchive->Open() == kErrorOK) {
                    theError = theArchive->FindFile("Scenarios\\" + gU52A280 + theName + (const char*)gScenarioExtension,
                                                    theFile);
                    if (theError == kErrorOK) {
                        if (theFile->Open(kFileAccessRead) == kErrorOK) {
                            unsigned long theLength;
                            if (theFile->GetLength(theLength) == kErrorOK) {
                                for (unsigned long i = 0; i < theLength; i++) {
                                    unsigned char theByte;
                                    theFile->Read(theByte);
                                    theSum += theByte;
                                }
                                if (!theWarned) {
                                    bool theChanged = theSum != EEGetFileChecksum(theDirectory,
                                        UString((const char*)(theName + (const char*)gScenarioExtension)), 0);
                                    if (theChanged) {
                                        gResource->GetString(IDS_CAMPAIGN_HAS_SCENARIO_WHICH_DIFFERS, theFormat);
                                        sprintf(theMessage, (const char*)theFormat, (const char*)this->GetName(),
                                                (const char*)theName);
                                        gUIManager->DisplayMessage(theMessage, true, NULL, NULL, true);
                                        theWarned = true;
                                    }
                                }
                            }
                        }
                        theFile->Close();
                        delete theFile;
                        theFile = NULL;
                    }
                }
                theArchive->Close();
                delete theArchive;
            }
        }
    }
    theCampaignPath->Dereference();
    theScenarioPath->Dereference();
    delete theSpec;
    delete theFile;
    return true;
}

// FUNCTION: EXE 0x68e655
// Moves a campaign scenario back to the sorted available list and marks the campaign edited; returns whether it
// was in the campaign
bool EECampaignEditorBehavior::RemoveScenario(const UWideString& inScenario)
{
    std::vector<UWideString> theScenarios;
    bool theResult = false;
    theScenarios = this->mCampaign.GetScenarios();
    std::vector<UWideString>::iterator it = std::find(theScenarios.begin(), theScenarios.end(), inScenario);
    if (it != theScenarios.end()) {
        theScenarios.erase(it);
        this->mCampaign.SetCampaignScenarios(theScenarios);
        this->mAvailableScenarios.push_back(inScenario);
        std::sort(this->mAvailableScenarios.begin(), this->mAvailableScenarios.end());
        theResult = true;
        this->mPasswordEntered = theResult;
    }
    return theResult;
}

// FUNCTION: EXE 0x68e736
// Validates and saves the campaign with the entered password, then rescans the lists; returns whether it worked
bool EECampaignEditorBehavior::Save(UWideString& outError)
{
    bool theResult = this->Validate(outError);
    if (!theResult)
        return theResult;
    this->mCampaign.SetPassword(this->mPassword);
    theResult = this->mCampaign.Save(outError);
    if (theResult) {
        theResult = this->Reset();
        this->mPasswordEntered = false;
    }
    return theResult;
}

// FUNCTION: EXE 0x68e77a
// Loads a campaign by name (checking its scenarios exist) and rescans the lists, locked until its password is
// entered; returns whether it worked
bool EECampaignEditorBehavior::Load(const UWideString& inName, UWideString& outError)
{
    UWideString theFile;
    theFile = "Campaigns\\";
    theFile += inName;
    bool theResult = this->mCampaign.Load(theFile, outError, true);
    if (theResult) {
        theResult = this->Reset();
        this->mPasswordEntered = false;
        this->mIsNew = false;
        this->mPassword = "";
    }
    return theResult;
}

// FUNCTION: EXE 0x68e7f7
// Sets the campaign author's e-mail address
void EECampaignEditorBehavior::SetAuthorEmail(const UWideString& inText)
{
    this->mCampaign.SetAuthorEmail(inText);
}

// FUNCTION: EXE 0x68e806
// Sets the campaign author's name
void EECampaignEditorBehavior::SetAuthorName(const UWideString& inText)
{
    this->mCampaign.SetAuthorName(inText);
}

// FUNCTION: EXE 0x68e815
// Sets the campaign's background image name
void EECampaignEditorBehavior::SetBackgroundImage(const UWideString& inText)
{
    this->mCampaign.SetBackgroundImage(inText);
}

// FUNCTION: EXE 0x68e824
// Sets the campaign description
void EECampaignEditorBehavior::SetDescription(const UWideString& inText)
{
    this->mCampaign.SetCampaignDescription(inText);
}

// FUNCTION: EXE 0x68e833
// Sets the campaign name
void EECampaignEditorBehavior::SetName(const UWideString& inText)
{
    this->mCampaign.SetCampaignName(inText);
}

// FUNCTION: EXE 0x68e842
// Sets the entered password and marks the campaign edited
void EECampaignEditorBehavior::SetPassword(const UWideString& inPassword)
{
    this->mPassword = inPassword;
    this->mPasswordEntered = true;
}

// FUNCTION: EXE 0x68e860
// Sets the civilization the campaign requires
void EECampaignEditorBehavior::SetRequiredCiv(const UWideString& inText)
{
    this->mCampaign.SetRequiredCiv(inText);
}

// FUNCTION: EXE 0x68e86f
// Checks that the campaign has a name; returns false with an error text otherwise
bool EECampaignEditorBehavior::ValidateName(UWideString& outError)
{
    UWideString theName(this->mCampaign.GetCampaignName());
    bool theResult = false;
    if (theName != "")
        theResult = true;
    else
        gResource->GetString(IDS_YOU_MUST_ENTER_NAME_FOR_YOUR_CAMPAIGN, outError);
    return theResult;
}

// FUNCTION: EXE 0x68e8c7
// Checks the campaign's name, other settings and scenario list; returns false with the first error text otherwise
bool EECampaignEditorBehavior::Validate(UWideString& outError)
{
    bool theResult = false;
    if (this->ValidateName(outError) && this->ValidateOther(outError) && this->ValidateScenarios(outError))
        theResult = true;
    return theResult;
}

// ICF-FOLDED: EXE 0x4b3257
// Accepts any other campaign setting (`return true`: folded into an identical function elsewhere)
bool EECampaignEditorBehavior::ValidateOther(UWideString& outError)
{
    return true;
}

// FUNCTION: EXE 0x68e8fe
// Checks that the campaign has at least one scenario; returns false with an error text otherwise
bool EECampaignEditorBehavior::ValidateScenarios(UWideString& outError)
{
    std::vector<UWideString> theScenarios;
    bool theResult = false;
    theScenarios = this->mCampaign.GetScenarios();
    if (theScenarios.size() > 0)
        theResult = true;
    else
        gResource->GetString(IDS_CAMPAIGN_MUST_HAVE_AT_LEAST, outError);
    return theResult;
}

// The template COMDATs of the unit, in the original's order (std::sort of vector<UWideString>, then erase):
// TEMPLATE: EXE 0x68e967
// (std::sort)
// TEMPLATE: EXE 0x68ea12
// (std::_Insertion_sort)
// TEMPLATE: EXE 0x68ea37
// (std::_Sort)
// TEMPLATE: EXE 0x68eafc
// (std::_Unguarded_insertion_sort)
// TEMPLATE: EXE 0x68eb39
// (std::_Insertion_sort_1)
// TEMPLATE: EXE 0x68ebff
// (std::make_heap)
// TEMPLATE: EXE 0x68ec39
// (std::sort_heap)
// TEMPLATE: EXE 0x68ec6f
// (std::_Unguarded_partition)
// TEMPLATE: EXE 0x68ed89
// (std::_Unguarded_insert)
// TEMPLATE: EXE 0x68edde
// (std::_Make_heap)
// TEMPLATE: EXE 0x68ee39
// (std::pop_heap)
// TEMPLATE: EXE 0x68ee5e
// (std::_Adjust_heap)
// TEMPLATE: EXE 0x68ef01
// (std::_Pop_heap_0)
// TEMPLATE: EXE 0x68ef6e
// (std::_Push_heap)
// TEMPLATE: EXE 0x68efe1
// (std::_Pop_heap)
// TEMPLATE: EXE 0x68f02d
// (vector<UWideString>::erase(iterator))
// TEMPLATE: EXE 0x68f0a1
// (std::copy)
