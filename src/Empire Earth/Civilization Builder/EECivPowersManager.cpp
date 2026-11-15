// C:\EEX\Empire Earth\Civilization Builder\EECivPowersManager.cpp (file name from the class; no __FILE__). The
// civilization powers table (RTTI EECivPowersManager, vtable 0x44c004): 0x2830d0..0x283b2e, then the
// std::vector<EECivPower> COMDATs. Unit flags: the exe module set.
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
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "lle/UITextLabel.h"
#include "lle/UResourceFile.h"
#include "ee/EmpireEarth.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIFMGMainGroup.h"
#include "ee/EEUIManager.h"
#include "ee/EEButtonObject.h"
#include "ee/EEUnitBehavior.h"
#include "ee/EEFormation.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEPhysics.h"
#include "ee/EEUICUnitIconBar.h"
#include "ee/EEUIFMGStatusBox.h"
#include "ee/EEUIFMGStatusButton.h"
#include "ee/EEUIEMGHotKeyStatusButton.h"

#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EECivilizationManager.h"
#include "ee/EEStringIDs.h"
#include "ee/EERules.h"
#include "ee/EERulesWorld.h"
#include "ee/EECivPowerIDs.h"
// FUNCTION: EXE 0x6830d0
// Creates the civilization powers table, empty
EECivPowersManager::EECivPowersManager()
{
    this->mPowers.clear();
}

// SYNTHETIC: EXE 0x6830fa
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x683116
// Destroys the civilization powers table
EECivPowersManager::~EECivPowersManager()
{
}

// FUNCTION: EXE 0x683124
// Reads the civilization power records from the powers file
long EECivPowersManager::Load(FSFile* inFile)
{
    unsigned long theCount = 0;
    inFile->Open(kFileAccessRead);
    long theResult = inFile->Read(theCount);
    if (theResult == kErrorOK) {
        for (unsigned long i = 0; i < theCount; i++) {
            EECivPower thePower;
            unsigned long theRead;
            memset(&thePower, 0, sizeof(EECivPower));
            theResult = inFile->Read(reinterpret_cast<unsigned char*>(&thePower), sizeof(EECivPower), theRead);
            this->mPowers.push_back(thePower);
        }
    }
    inFile->Close();
    return theResult;
}

// FUNCTION: EXE 0x6831b3
// Reads a civilization power's name from the string table
long EECivPowersManager::GetName(long inID, UWideString& outName)
{
    return gResource->GetString(inID + IDS_CIV_POWER_BASE, outName);
}

// FUNCTION: EXE 0x6831d0
// Copies the record of a power id (powers start at id 0x26); false when the slot is empty or holds another id
bool EECivPowersManager::GetPower(long inID, EECivPower& outPower)
{
    bool theResult = false;
    unsigned long theIndex = inID - 0x26;
    memset(&outPower, 0, sizeof(EECivPower));
    if (this->mPowers.size() > theIndex && this->mPowers[theIndex].mID > 0 && this->mPowers[theIndex].mID == inID) {
        outPower = this->mPowers[theIndex];
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x683235
// Returns the record of a power id without range checks
EECivPower* EECivPowersManager::GetPowerRecord(long inID)
{
    return &this->mPowers[inID - 0x26];
}

// FUNCTION: EXE 0x683248
// Tells whether the player bought the civilization power
// (an upgrade whose attribute applies the power: civbuilder.yaml; the original compared the level with the id)
bool EECivPowersManager::HasPower(EEPlayer* inPlayer, long inID)
{
    bool theResult = false;
    for (std::vector<EECivUpgrade>::iterator i = inPlayer->mCivilization.GetPurchasedUpgrades().begin();
         i != inPlayer->mCivilization.GetPurchasedUpgrades().end() && !theResult; i++) {
        unsigned long theCategory;
        unsigned long theIndex;
        gCivilizationManager.SplitAttribute(*i, &theCategory, &theIndex);
        const EECivAttribute* theAttribute = EEGetRules().mCivBuilder.Find((long)theIndex);
        if (theAttribute == NULL)
            continue;
        for (const EECivApply& theApply : theAttribute->mApplies)
            if (theApply.mStat == kCivStatPower && theApply.mPower + kCivPowerFlamingArrows == inID)
                theResult = true;
    }
    return theResult;
}

// the defaults of EERules.cpp's civ powers name these rows (EESoundObject.h, EEGraphicEffect.h)
static_assert(kSoundSlavery == 0x422 && kSoundPriestTowerConvert == 0x41c && kSoundLightning == 0x176 &&
              kSoundThunder2 == 0x1ba && kGraphicEffectLightningUnit == 0x15 && kGraphicEffectDustEffect == 0xab &&
              kGraphicEffectDisintegrationAreaEffectIntermediate == 0xec, "EERules.cpp's civ power rows");

// FUNCTION: EXE 0x6832a3
// Applies a bought civ power to a player: researches its technology, then the power's own effects
// (flaming/crusader upgrades, media loads, exploration sight pulse, mining limit, camouflage, expansionism settlements)
// The effects are civpowers.yaml's (C++ primitives the data selects and parameterises; the original's switch on the
// power is the vanilla data).
void EECivPowersManager::ApplyPower(EEPlayer* inPlayer, unsigned long inID)
{
    EECivPower thePower;
    if (this->GetPower(inID, thePower) == false)
        return;
    inPlayer->GetTechTree()->Research(thePower.mTechnology, NULL);
    const EECivPowerRule* theRule = EEGetRules().mCivPowers.Find((long)inID - kCivPowerFlamingArrows);
    if (theRule == NULL)
        return;
    EETechTreeNode* theNode;
    EESoundObject* theSound;
    EEGraphicEffect* theEffect;
    for (const EECivPowerEffect& theDo : theRule->mEffects) {
        switch (theDo.mAction) {
        case kPowerResearchIfResearched:
            theNode = inPlayer->GetTechTree()->Get(theDo.mIfTech);
            if (theNode && theNode->mIsResearched) {
                theNode = inPlayer->GetTechTree()->Get(theDo.mTech);
                if (theNode)
                    theNode->ResearchNode(NULL);
            }
            break;
        case kPowerLoadSound:
            theSound = EESoundObject::Get(theDo.mSound);
            if (theSound) {
                theSound->LoadMedia();
                inPlayer->AddLoadedMedia(theSound);
            }
            break;
        case kPowerLoadEffect:
            theEffect = EEGraphicEffect::Get(theDo.mEffect);
            if (theEffect) {
                theEffect->LoadMedia();
                inPlayer->AddLoadedMedia(theEffect);
            }
            break;
        case kPowerPulseSight:
            inPlayer->PulseSight(false, thePower.mAmount, theDo.mUnitType);
            break;
        case kPowerSetMiningLimit:
            inPlayer->mMiningLimit = thePower.mMiningLimit;
            break;
        case kPowerEnableCamouflage:
            inPlayer->EnableCamouflagePower();
            break;
        case kPowerDisallowTech:
            theNode = inPlayer->GetTechTree()->Get(theDo.mTech);
            if (theNode) {
                theNode->mIsAllowed = false;
                theNode->SetObsolete(true, false);
            }
            break;
        case kPowerAllowTech:
            theNode = inPlayer->GetTechTree()->Get(theDo.mTech);
            if (theNode)
                theNode->mIsAllowed = true;
            break;
        }
    }
}


// FUNCTION: EXE 0x683609
// Standard vector code for the powers table (a std::vector<EECivPower> template COMDAT)

// FUNCTION: EXE 0x683635
// Standard vector code for the powers table (a std::vector<EECivPower> template COMDAT)

// FUNCTION: EXE 0x683670
// Standard vector code for the powers table (a std::vector<EECivPower> template COMDAT)

// FUNCTION: EXE 0x6836cd
// Standard vector code for the powers table (a std::vector<EECivPower> template COMDAT)

// FUNCTION: EXE 0x683711
// Standard vector code for the powers table (a std::vector<EECivPower> template COMDAT)

// FUNCTION: EXE 0x68374b
// Standard vector code for the powers table (a std::vector<EECivPower> template COMDAT)

// FUNCTION: EXE 0x683a33
// Standard vector code for the powers table (a std::vector<EECivPower> template COMDAT)

// FUNCTION: EXE 0x683a48
// Standard vector code for the powers table (a std::vector<EECivPower> template COMDAT)

// FUNCTION: EXE 0x683a5f
// Standard vector code for the powers table (a std::vector<EECivPower> template COMDAT)

// FUNCTION: EXE 0x683a8c
// Standard vector code for the powers table (a std::vector<EECivPower> template COMDAT)

// FUNCTION: EXE 0x683ac1
// Standard vector code for the powers table (a std::vector<EECivPower> template COMDAT)

// FUNCTION: EXE 0x683ae6
// Standard vector code for the powers table (a std::vector<EECivPower> template COMDAT)

// FUNCTION: EXE 0x683b18
// Standard vector code for the powers table (a std::vector<EECivPower> template COMDAT)
