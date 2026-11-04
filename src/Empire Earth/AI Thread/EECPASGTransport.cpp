// C:\EEX\Empire Earth\AI Thread\EECPASGTransport.cpp (file name inferred: no __FILE__; RTTI class
// EECPASGTransport, vtable 0x43bbdc). What a transport attack sub group carries: 0x51805..0x51952 (EECPASGSubmarine's
// code ends at 0x51805; EECPASTargetUnit's Clear starts at 0x51952). ctor, ??_G, dtor, the slots, ReleaseLand.
#include <windows.h>
#include <vector>
#include <set>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEViewport.h"
#include "lle/GETransformation.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSRestorable.h"
#include "lle/USingleton.h"

#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EECPILocation.h"
#include "ee/EECPASTargetUnit.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EECPAttackSubGroupUnit.h"
#include "ee/EECPAttackGroup.h"
#include "ee/EECPAttackCommander.h"
#include "ee/EECPASGLand.h"
#include "ee/EECPASGTransport.h"

// FUNCTION: EXE 0x451805
// Creates the transport part of a sub-group, carrying no land group
EECPASGTransport::EECPASGTransport() : mLand(NULL), mOwner(NULL), mCargoStage(0)
{
}

// SYNTHETIC: EXE 0x451819
// (the scalar deleting destructor)

// FUNCTION: EXE 0x451835
// Destroys the transport part (nothing to release)
EECPASGTransport::~EECPASGTransport()
{
}

// FUNCTION: EXE 0x45183c
// Reads the carried land group's id (resolved later by Restore) and the cargo stage; returns the first read error
long EECPASGTransport::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = 0;
    unsigned long theLandID = 0;
    theResult = inFile.Read(theLandID);
    this->mLand = reinterpret_cast<EECPASGLand*>(theLandID);
    if (theResult == 0)
        theResult = inFile.Read(this->mCargoStage);
    return theResult;
}

// FUNCTION: EXE 0x451871
// Turns the loaded land group id back into the commander's land sub-group and takes a reference; returns 0
long EECPASGTransport::Restore(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    EECPAttackSubGroup* theSubGroup = dynamic_cast<EECPAttackSubGroup*>(this);
    if (!this->mLand)
        return 0;
    EECPAttackCommander* theCommander = theSubGroup->mGroup->mCommander;
    this->mLand = dynamic_cast<EECPASGLand*>(theCommander->GetSubGroup(USavedID(this->mLand)));
    if (this->mLand)
        this->mLand->Reference();
    return 0;
}

// FUNCTION: EXE 0x4518c7
// Lets go of the carried land group: clears its transport if that is this one and drops the reference
void EECPASGTransport::ReleaseLand()
{
    EECPASGLand* theLand = this->mLand;
    if (!theLand)
        return;
    this->mLand = NULL;
    if (theLand->mTransport == this)
        theLand->ReleaseTransport();
    theLand->Dereference();
}

// FUNCTION: EXE 0x4518ec
// Writes the carried land group's id (0 if none) and the cargo stage; returns the first write error
long EECPASGTransport::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    unsigned long theLandID = this->mLand ? this->mLand->GetID() : 0;
    long theResult = outFile.Write(theLandID);
    if (theResult == 0)
        theResult = outFile.Write(this->mCargoStage);
    return theResult;
}

// FUNCTION: EXE 0x451931
// Switches to carrying another land group, releasing the old one and referencing the new one
void EECPASGTransport::SetLand(EECPASGLand* inLand)
{
    if (this->mLand == inLand)
        return;
    this->ReleaseLand();
    this->mLand = inLand;
    if (inLand != NULL)
        inLand->Reference();
}


// ICF-FOLDED: EXE 0x418aa9
// Returns true
// (no address of its own: folded into the shared `mov al, 1; ret` body the vtable slot points at)
bool EECPASGTransport::Virtual0C()
{
    return true;
}
