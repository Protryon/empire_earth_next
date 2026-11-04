// C:\EEX\Empire Earth\Campaign\EECampaign.cpp (file name inferred: no __FILE__). A campaign: the
// archive "Campaigns\<name><gCampaignExtension>" with seven tags and the scenario list, plus the scenario files and
// their media: 0x28befc..0x28d82d (the eight tag keys' init chains, then the members, alphabetical). Unit flags: the
// exe's module set.
#include <windows.h>
#include <mbstring.h>
#include <string.h>
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
#include "lle/FSRestorable.h"
#include "lle/FSPersistent.h"
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
#include "lle/UEncryption.h"

#include "ee/EmpireEarth.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEGameOptions.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEPSaveGameHeader.h"
#include "ee/EEScenarioManager.h"
#include "ee/EESMEffect.h"
#include "ee/EEPersistentTemplates.h"
#include "ee/EECampaignFormManager.h"
#include "ee/EEStringIDs.h"
#include "ee/EEScenarioIDs.h"
// the campaign archive's tag keys (unguarded file-scope globals, 0x5656a0 down to 0x5655f8)
const UWideString kEECampaignAuthorNameKey("AuthorName");
const UWideString kEECampaignAuthorEmailKey("AuthorEmail");
const UWideString kEECampaignCampaignNameKey("CampaignName");
const UWideString kEECampaignCampaignDescriptionKey("CampaignDescription");
const UWideString kEECampaignCampaignPasswordKey("CampaignPassword");
const UWideString kEECampaignRequiredCivNameKey("RequiredCivName");
const UWideString kEECampaignBackgroundImageKey("BackgroundImage");
const UWideString kEECampaignScenarioListKey("ScenarioList.txt");

// FUNCTION: EXE 0x68c08c
// Creates an empty campaign in the game's data directory, with no required civilization or background image
EECampaign::EECampaign()
{
    this->mDirectory = (const char*)(gStartupDirectory + "data\\");
    this->mRequiredCiv = "";
    this->mBackgroundImage = "";
}

// FUNCTION: EXE 0x68c122
// Copies each scenario and media file into the data directory under its campaign-prefixed name; returns whether
// every one was copied
bool EECampaign::CopyScenarioFiles(std::vector<UWideString>& inFiles, std::vector<UWideString>& outFiles)
{
    std::vector<UWideString> theUnused;
    std::vector<UWideString> theCopied;
    UWideString theError;
    UString theDestination;
    UString thePath;
    UString theLowerPath;
    UString theLowerName;
    FSFileSpec* theSpec;
    bool theResult = false;
    if (this->ListScenarioFiles(outFiles, theError, true, true)) {
        std::vector<UWideString>::iterator theSource = inFiles.begin();
        for (std::vector<UWideString>::iterator it = outFiles.begin(); it != outFiles.end(); ++it, ++theSource) {
            long theFound = gDataPath->FindFile((const char*)*theSource, theSpec);
            if (theFound != kErrorOK)
                break;
            thePath = theSpec->GetVolume() + theSpec->GetDirectory() + theSpec->GetFile();
            theLowerPath = thePath;
            theLowerPath.ToLower();
            theLowerName = (const char*)*theSource;
            theLowerName.ToLower();
            theDestination = thePath.GetSubstring(0, theLowerPath.Find(theLowerName, 0)) + (const char*)*it;
            if (theSpec->Copy(theDestination, false, false) != kErrorOK)
                break;
            theCopied.push_back(*it);
            delete theSpec;
        }
        if (theCopied.size() == outFiles.size())
            theResult = true;
    }
    return theResult;
}

// NEAR: register allocation only (this in esi/iterator in ebx in the original; ours edi/esi)
// FUNCTION: EXE 0x68c36e
// Writes the campaign's scenario list (one scenario file name per line) to a file in the campaign directory;
// returns whether it worked, with an error text otherwise
bool EECampaign::CreateScenarioList(const UWideString& inFile, UWideString& outError)
{
    FSFileSpec theFile;
    UString theName;
    UString theLine;
    bool theResult = false;
    theFile.SetVolume(gStartupVolume);
    theFile.SetDirectory(UString((const char*)this->mDirectory));
    theName = (const char*)inFile;
    theFile.SetFile(theName);
    theFile.Delete();
    long theError = theFile.Create();
    if (theError == kErrorOK)
        theError = theFile.Open(kFileAccessWrite);
    if (theError == kErrorOK) {
        std::vector<UWideString>::iterator it;
        for (it = this->mScenarios.begin(); it != this->mScenarios.end() && theError == kErrorOK; ++it) {
            theLine = (const char*)*it;
            theLine += gScenarioExtension;
            theError = theLine.Save(theFile);
        }
        if (theError != kErrorOK)
            gResource->GetString(IDS_THERE_WAS_ERROR_WRITING_CAMPAIGN, outError);
    } else
        gResource->GetString(IDS_COULDNT_OPEN_CAMPAIGN_FOR_WRITING, outError);
    if (theError == kErrorOK && theFile.Close() == kErrorOK)
        theResult = true;
    return theResult;
}

// FUNCTION: EXE 0x68c4a5
// Returns a stored campaign password decrypted
UWideString EECampaign::DecryptPassword(const UWideString& inPassword)
{
    UWideString theResult;
    UString theDecrypted;
    UString theEncrypted((const char*)inPassword);
    UEDecryptUString(theEncrypted, theDecrypted);
    theResult = (const char*)theDecrypted;
    return theResult;
}

// FUNCTION: EXE 0x68c517
// Returns a campaign password encrypted for storage
UWideString EECampaign::EncryptPassword(const UWideString& inPassword)
{
    UWideString theResult;
    UString theEncrypted;
    UString thePassword((const char*)inPassword);
    UEEncryptUString(thePassword, theEncrypted);
    theResult = (const char*)theEncrypted;
    return theResult;
}

// FUNCTION: EXE 0x68c589
// Returns the archive path of the campaign's scenario list file
UWideString EECampaign::GetScenarioListFile()
{
    UWideString theFile;
    theFile = UWideString("Campaigns\\");
    theFile += gU52A280;
    theFile += kEECampaignScenarioListKey;
    return theFile;
}

// FUNCTION: EXE 0x68c5f2
// Returns the author's e-mail address
const UWideString& EECampaign::GetAuthorEmail()
{
    return this->mAuthorEmail;
}

// FUNCTION: EXE 0x68c5f6
// Returns the author's name
const UWideString& EECampaign::GetAuthorName()
{
    return this->mAuthorName;
}

// FUNCTION: EXE 0x68c5fa
// Returns the background image name
const UWideString& EECampaign::GetBackgroundImage()
{
    return this->mBackgroundImage;
}

// FUNCTION: EXE 0x68c5fe
// Returns the campaign description
const UWideString& EECampaign::GetDescription()
{
    return this->mDescription;
}

// FUNCTION: EXE 0x68c602
// Returns the campaign name
const UWideString& EECampaign::GetCampaignName()
{
    return this->mCampaignName;
}

// ICF-FOLDED: EXE 0x52c738
// Returns the campaign's scenario names (`return this`: folded into an identical function elsewhere)
const std::vector<UWideString>& EECampaign::GetScenarios()
{
    return this->mScenarios;
}

// FUNCTION: EXE 0x68c606
// Returns the decrypted campaign password
const UWideString& EECampaign::GetPassword()
{
    return this->mPassword;
}

// FUNCTION: EXE 0x68c60d
// Returns the civilization the campaign requires ("" for any)
const UWideString& EECampaign::GetRequiredCiv()
{
    return this->mRequiredCiv;
}

// NEAR: FSArchive::FindFile(const UWideString&) is inline in our header, the original calls the import, and
// its two push_backs are expanded (insert(end(), x)); slots follow from that
// FUNCTION: EXE 0x68c614
// Reads a campaign archive: its tags, decrypted password and scenario list (optionally checking every scenario
// exists); returns whether it worked, with an error text otherwise
bool EECampaign::Load(const UWideString& inFile, UWideString& outError, bool inCheckFiles)
{
    UWideString theFormat;
    UWideString thePassword;
    UWideString theScenario;
    UWideString theListFile;
    UWideString theRequiredCiv;
    UString theLine;
    FSFileSpec* theSpec = NULL;
    FSMemoryFile* theList = NULL;
    bool theResult = false;
    long theFindResult = gDataPath->FindFile(UString((const char*)inFile), theSpec);
    if (theFindResult == kErrorOK) {
        FSArchive* theArchive = new FSArchive(*theSpec);
        if (theArchive->Open() == kErrorOK) {
            theArchive->GetTag(kEECampaignRequiredCivNameKey, theRequiredCiv);
            if (theRequiredCiv == " ")
                this->mRequiredCiv = "";
            else
                this->mRequiredCiv = theRequiredCiv;
            theArchive->GetTag(kEECampaignBackgroundImageKey, this->mBackgroundImage);
            if (theArchive->GetTag(kEECampaignCampaignNameKey, this->mCampaignName) == kErrorOK
                && theArchive->GetTag(kEECampaignCampaignDescriptionKey, this->mDescription) == kErrorOK
                && theArchive->GetTag(kEECampaignAuthorNameKey, this->mAuthorName) == kErrorOK
                && theArchive->GetTag(kEECampaignAuthorEmailKey, this->mAuthorEmail) == kErrorOK) {
                theArchive->GetTag(kEECampaignCampaignPasswordKey, thePassword);
                if (thePassword != "")
                    this->mPassword = this->DecryptPassword(thePassword);
                else
                    this->mPassword = "";
                theListFile = this->GetScenarioListFile();
                if (theArchive->FindFile(theListFile, theList) == kErrorOK) {
                    this->mScenarios.clear();
                    theResult = true;
                    while (theLine.Load(*theList) == kErrorOK) {
                        theScenario = (const char*)theLine;
                        if (inCheckFiles) {
                            bool theFound = gDataPath->FindFile(UWideString("Scenarios\\") + theScenario) == kErrorOK;
                            if (theFound) {
                                if (theScenario.GetLength() > 4)
                                    theScenario.InsertString("", theScenario.GetLength() - 4, true);
                                this->mScenarios.insert(this->mScenarios.end(), theScenario);
                            } else {
                                gResource->GetString(IDS_SCENARIO_DIDNT_EXIST_ON_DISK, theFormat);
                                sprintf(outError, (const char*)theFormat, (const char*)theScenario);
                                this->mScenarios.clear();
                                theResult = false;
                                break;
                            }
                        } else {
                            if (theScenario.GetLength() > 4)
                                theScenario.InsertString("", theScenario.GetLength() - 4, true);
                            this->mScenarios.insert(this->mScenarios.end(), theScenario);
                        }
                    }
                    delete theList;
                } else
                    gResource->GetString(IDS_THERE_WAS_ERROR_READING_TAG, outError);
            } else
                gResource->GetString(IDS_THERE_WAS_ERROR_READING_TAG, outError);
        } else
            gResource->GetString(IDS_FILE_COULDNT_BE_OPENED, outError);
        delete theArchive;
        delete theSpec;
    } else
        gResource->GetString(IDS_FILE_COULDNT_BE_OPENED, outError);
    return theResult;
}

// SYNTHETIC: EXE 0x68c9e9
// (FSArchive's scalar deleting destructor, with the local copy of its vtable 0x44c050: `new FSArchive` in Load)

// NEAR: register allocation only (this in edi, the += import in ebx in the original)
// FUNCTION: EXE 0x68ca06
// Lists every scenario file of the campaign (optionally with the campaign prefix) with its media and sounds, plus
// the background image; returns false at the first scenario whose files are missing
bool EECampaign::ListScenarioFiles(std::vector<UWideString>& outFiles, UWideString& outError, bool inAdd,
                                   bool inPrefixed)
{
    UWideString theDefault;
    EEPSaveGameHeader theHeader;
    EESMMediaStrings theMedia;
    UWideString theFile;
    UWideString theUnused1;
    UWideString theUnused2;
    UWideString thePrefixedFile;
    UWideString thePrefix;
    bool theResult = true;
    std::vector<UWideString>::iterator it;
    for (it = this->mScenarios.begin(); it != this->mScenarios.end(); ++it) {
        theFile = "";
        theFile += "Scenarios\\";
        theFile += *it;
        theFile += gScenarioExtension;
        thePrefixedFile = "";
        thePrefixedFile += "Scenarios\\";
        thePrefixedFile += gU52A280;
        thePrefixedFile += *it;
        thePrefixedFile += gScenarioExtension;
        if (inAdd)
            outFiles.push_back(inPrefixed ? thePrefixedFile : theFile);
        if (!this->ListScenarioMedia(theFile, outFiles, outError, inPrefixed)) {
            theResult = false;
            break;
        }
    }
    gResource->GetString(IDS_SE_MAP_NONE, theDefault);
    if (this->mBackgroundImage.GetLength() > 0 && !this->mBackgroundImage.Equals(theDefault)) {
        thePrefix = inPrefixed ? gU52A280 : UWideString("");
        outFiles.push_back("Scenarios\\" + thePrefix + this->mBackgroundImage);
    }
    return theResult;
}

// FUNCTION: EXE 0x68cc2e
// Adds a scenario's media files and effect sounds that are not in an archive to a file list (no duplicates);
// returns whether all were found, with an error text naming the missing ones otherwise
bool EECampaign::ListScenarioMedia(const UWideString& inFile, std::vector<UWideString>& ioFiles,
                                   UWideString& outError, bool inPrefixed)
{
    EEPSaveGameHeader theHeader;
    EESMMediaStrings theMedia;
    std::vector<UWideString> theFiles;
    UWideString theName;
    UWideString theFormat;
    UWideString theError;
    bool theResult = false;
    bool theMediaFailed = false;
    FSFileSpec* theSpec;
    std::vector<UWideString>::iterator it;
    long theFindResult = gDataPath->FindFile(UString((const char*)inFile), theSpec);
    if (theFindResult == kErrorOK) {
        if (theHeader.LoadFromFile(*theSpec) == kErrorOK) {
            theMedia = theHeader.mMediaStrings;
            if (theMedia.ListMediaFiles(inFile, theFiles, theError, inPrefixed)) {
                for (it = theFiles.begin(); it != theFiles.end(); ++it) {
                    if (!this->NotArchived(*it))
                        continue;
                    theName = *it;
                    theName.ToLower();
                    std::vector<UWideString>::iterator theFound = std::find(ioFiles.begin(), ioFiles.end(), theName);
                    if (theFound == ioFiles.end())
                        ioFiles.push_back(theName);
                }
                theResult = true;
            } else
                theMediaFailed = true;
            if (!this->ListEffectSounds(inFile, ioFiles, theError, inPrefixed)) {
                theResult = false;
                theMediaFailed = true;
            }
        } else {
            theResult = false;
            theMediaFailed = false;
        }
        delete theSpec;
        theSpec = NULL;
    }
    if (!theResult) {
        if (theMediaFailed) {
            gResource->GetString(IDS_FILES_BELOW_ARE_USED_IN_SCENARIO, theFormat);
            sprintf(outError, (const char*)theFormat, (const char*)inFile);
            outError += "\n\n" + theError;
        } else {
            gResource->GetString(IDS_CAMPAIGN_HAS_SCENARIO_WHICH_DIFFERS, theFormat);
            sprintf(outError, (const char*)theFormat, (const char*)this->mCampaignName, (const char*)inFile);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x68ceba
// Adds the sound file of a play-sound or dialogue effect (.wav or .mp3, optionally campaign-prefixed) to a file
// list unless archived or already listed; returns false with its name in the error when the sound is missing
bool EECampaign::ListEffectSound(const UWideString& inFile, EESMEffect* inEffect, std::vector<UWideString>& ioFiles,
                                 UWideString& outError, bool inPrefixed)
{
    UWideString theFile;
    UWideString theSound;
    UString thePath;
    UString theTail;
    UString theHead;
    UString theName;
    bool theResult = true;
    if (inEffect->mType == kSMEffectMedia && (inEffect->mMediaAction == kSMMediaActionPlaySound || inEffect->mMediaAction == kSMMediaActionSendDialogue)) {
        theSound = inEffect->mFileName;
        if (theSound != "") {
            theName = "Sounds\\";
            theName += (const char*)theSound;
            theFile = "";
            if (theName.GetLength() > 4
                && (theName.GetSubstring(theName.GetLength() - 4, 4).EqualsIgnoreCase(".wav", 4)
                    || theName.GetSubstring(theName.GetLength() - 4, 4).EqualsIgnoreCase(".mp3", 4))) {
                theFile = (const char*)theName;
                if (gDataPath->FindFile(theFile) != kErrorOK)
                    theFile = "";
            } else {
                theFile = (const char*)(theName + ".wav");
                if (gDataPath->FindFile(theFile) != kErrorOK) {
                    theFile = (const char*)(theName + ".mp3");
                    if (gDataPath->FindFile(theFile) != kErrorOK)
                        theFile = "";
                }
            }
            if (theFile != "") {
                if (this->NotArchived(theFile)) {
                    if (inPrefixed) {
                        thePath = (const char*)theFile;
                        unsigned long thePosition = thePath.Find((const char*)theSound, 0);
                        if (thePosition != thePath.GetLength()) {
                            theHead = thePath.GetSubstring(0, thePosition);
                            theTail = thePath.GetSubstring(thePosition, thePath.GetLength());
                            theFile = (const char*)(theHead + (const char*)gU52A280 + theTail);
                        }
                    }
                    std::vector<UWideString>::iterator theFound = std::find(ioFiles.begin(), ioFiles.end(), theFile);
                    if (theFound == ioFiles.end())
                        ioFiles.push_back(UWideString(theFile));
                }
            } else {
                sprintf(outError, "%s\n", (const char*)theName);
                theResult = false;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x68d219
// Adds the sounds of every effect in a scenario's saved scenario manager to a file list; returns whether all were
// found, collecting the missing names in the error
bool EECampaign::ListEffectSounds(const UWideString& inFile, std::vector<UWideString>& ioFiles,
                                  UWideString& outError, bool inPrefixed)
{
    UWideString theError;
    bool theResult = false;
    FSPersistent* thePersistent = NULL;
    long theLoadResult = EELoadPersistent(UString((const char*)inFile), 6, thePersistent);
    if (theLoadResult == kErrorOK) {
        EEPTPRestorable<EEScenarioManager>* theRestorable =
            dynamic_cast<EEPTPRestorable<EEScenarioManager>*>(thePersistent);
        if (theRestorable != NULL) {
            EEScenarioManager* theManager = theRestorable->mObject;
            if (theManager != NULL) {
                theResult = true;
                for (std::vector<EESMEffect*>::iterator it = theManager->mEffects.begin();
                     it != theManager->mEffects.end(); ++it) {
                    if (!this->ListEffectSound(inFile, *it, ioFiles, theError, inPrefixed)) {
                        theResult = false;
                        outError += theError;
                    }
                }
                delete theManager;
                theRestorable->mObject = NULL;
            }
        }
        // The restorable wrapper EELoadPersistent created is freed too (it leaked).
        delete thePersistent;
    }
    return theResult;
}

// FUNCTION: EXE 0x68d2e3
// Returns whether a file lies outside the game's archives
bool EECampaign::NotArchived(UWideString inFile)
{
    bool theArchived = false;
    gDataPath->GetFileArchiveStatus(inFile, theArchived);
    return theArchived == false;
}

// FUNCTION: EXE 0x68d317
// Builds the campaign archive from the scenario list file and copies of every scenario file, then deletes the
// copies; returns whether it worked
bool EECampaign::SaveArchive(FSArchive* inArchive, UWideString& outError)
{
    std::vector<UWideString> theFiles;
    std::vector<UWideString> theCopied;
    std::vector<UString> theNames;
    UWideString theListFile;
    FSFileSpec theDirectory;
    UString theName;
    std::vector<UWideString>::iterator it;
    FSFileSpec* theSpec;
    long theError;
    bool theResult = false;
    if (this->ListScenarioFiles(theFiles, outError, true, false)) {
        theListFile = this->GetScenarioListFile();
        if (this->CreateScenarioList(theListFile, outError)) {
            if (this->CopyScenarioFiles(theFiles, theCopied)) {
                for (it = theCopied.begin(); it != theCopied.end(); ++it) {
                    theName = "";
                    theName += (const char*)*it;
                    theNames.push_back(theName);
                }
                theNames.push_back((const char*)theListFile);
                theDirectory.SetDirectory((const char*)this->mDirectory);
                if (inArchive->Create(theDirectory, theNames, 0x1000) == kErrorOK)
                    theResult = true;
                else
                    gResource->GetString(IDS_THERE_WAS_ERROR_WRITING_CAMPAIGN, outError);
            }
            if (theListFile.GetLength() > 0)
                theCopied.push_back((const char*)theListFile);
            for (it = theCopied.begin(); it != theCopied.end(); ++it) {
                theError = gDataPath->FindFile((const char*)*it, theSpec);
                if (theError == kErrorOK) {
                    theSpec->Delete();
                    delete theSpec;
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x68d53b
// Saves the campaign: writes its tags (password encrypted) and then the archive; needs a campaign name; returns
// whether it worked, with an error text otherwise
bool EECampaign::Save(UWideString& outError)
{
    FSFileSpec theFile;
    UWideString theEncrypted;
    UWideString theRequiredCiv;
    UString theName;
    UString theUnused;
    bool theResult = false;
    if (this->mCampaignName != "") {
        theFile.SetVolume(gStartupVolume);
        theFile.SetDirectory(UString((const char*)this->mDirectory));
        theName += "Campaigns\\";
        theName += UString((const char*)this->GetCampaignName());
        theName += gCampaignExtension;
        theFile.SetFile(theName);
        FSArchive* theArchive = new FSArchive(theFile);
        if (this->mRequiredCiv.GetLength() > 0)
            theRequiredCiv = this->mRequiredCiv;
        else
            theRequiredCiv = " ";
        if (theArchive->SetTag(kEECampaignRequiredCivNameKey, theRequiredCiv) == kErrorOK
            && theArchive->SetTag(kEECampaignBackgroundImageKey, this->mBackgroundImage) == kErrorOK
            && theArchive->SetTag(kEECampaignCampaignNameKey, this->mCampaignName) == kErrorOK
            && theArchive->SetTag(kEECampaignCampaignDescriptionKey, this->mDescription) == kErrorOK
            && theArchive->SetTag(kEECampaignAuthorNameKey, this->mAuthorName) == kErrorOK
            && theArchive->SetTag(kEECampaignAuthorEmailKey, this->mAuthorEmail) == kErrorOK) {
            if (this->mPassword != "") {
                theEncrypted = this->EncryptPassword(this->mPassword);
                if (theArchive->SetTag(kEECampaignCampaignPasswordKey, theEncrypted) != kErrorOK)
                    goto done;
            }
            theResult = this->SaveArchive(theArchive, outError);
        }
done:
        delete theArchive;
    } else
        gResource->GetString(IDS_YOU_MUST_ENTER_NAME_FOR_YOUR_CAMPAIGN, outError);
    return theResult;
}

// FUNCTION: EXE 0x68d79b
// Sets the author's e-mail address
void EECampaign::SetAuthorEmail(const UWideString& inText)
{
    this->mAuthorEmail = inText;
}

// FUNCTION: EXE 0x68d7ab
// Sets the author's name
void EECampaign::SetAuthorName(const UWideString& inText)
{
    this->mAuthorName = inText;
}

// FUNCTION: EXE 0x68d7bb
// Sets the background image name
void EECampaign::SetBackgroundImage(const UWideString& inText)
{
    this->mBackgroundImage = inText;
}

// FUNCTION: EXE 0x68d7cb
// Sets the campaign description
void EECampaign::SetCampaignDescription(const UWideString& inText)
{
    this->mDescription = inText;
}

// FUNCTION: EXE 0x68d7db
// Sets the campaign name
void EECampaign::SetCampaignName(const UWideString& inText)
{
    this->mCampaignName = inText;
}

// FUNCTION: EXE 0x68d7eb
// Sets the campaign's scenario list
void EECampaign::SetCampaignScenarios(const std::vector<UWideString>& inScenarios)
{
    this->mScenarios = inScenarios;
}

// FUNCTION: EXE 0x68d7f7
// Sets the directory the campaign is saved into
void EECampaign::SetDirectory(const UWideString& inText)
{
    this->mDirectory = inText;
}

// FUNCTION: EXE 0x68d807
// Sets the campaign password (plain text)
void EECampaign::SetPassword(const UWideString& inText)
{
    this->mPassword = inText;
}

// FUNCTION: EXE 0x68d81a
// Sets the civilization the campaign requires
void EECampaign::SetRequiredCiv(const UWideString& inText)
{
    this->mRequiredCiv = inText;
}

// SYNTHETIC: EXE 0x68befc
// SYNTHETIC: EXE 0x68bf06
// SYNTHETIC: EXE 0x68bf17
// SYNTHETIC: EXE 0x68bf23
// SYNTHETIC: EXE 0x68bf2e
// SYNTHETIC: EXE 0x68bf38
// SYNTHETIC: EXE 0x68bf49
// SYNTHETIC: EXE 0x68bf55
// SYNTHETIC: EXE 0x68bf60
// SYNTHETIC: EXE 0x68bf6a
// SYNTHETIC: EXE 0x68bf7b
// SYNTHETIC: EXE 0x68bf87
// SYNTHETIC: EXE 0x68bf92
// SYNTHETIC: EXE 0x68bf9c
// SYNTHETIC: EXE 0x68bfad
// SYNTHETIC: EXE 0x68bfb9
// SYNTHETIC: EXE 0x68bfc4
// SYNTHETIC: EXE 0x68bfce
// SYNTHETIC: EXE 0x68bfdf
// SYNTHETIC: EXE 0x68bfeb
// SYNTHETIC: EXE 0x68bff6
// SYNTHETIC: EXE 0x68c000
// SYNTHETIC: EXE 0x68c011
// SYNTHETIC: EXE 0x68c01d
// SYNTHETIC: EXE 0x68c028
// SYNTHETIC: EXE 0x68c032
// SYNTHETIC: EXE 0x68c043
// SYNTHETIC: EXE 0x68c04f
// SYNTHETIC: EXE 0x68c05a
// SYNTHETIC: EXE 0x68c064
// SYNTHETIC: EXE 0x68c075
// SYNTHETIC: EXE 0x68c081
