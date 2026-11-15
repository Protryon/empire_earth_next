// C:\EEX\Empire Earth\Units\EESavedGroup.cpp (file name inferred: no __FILE__). A hot-key group: 0x208e6b..0x209008.
// Unit split: EESavedGroup's own vtable functions run 0x208e6b..0x208f8b and end at 0x209008;
// 0x209008 (EESGUnitSorter's operator(), called only by the sort templates at 0x209e03..0x20a12a) opens
// EESelectedGroup.cpp, whose vtable functions follow (0x209144..0x2093df, 0x209469) and whose std::sort instantiations
// over EESGUnitSorter run to 0x20a3c0 (EEUCAirAnimal's slots start there). EESGUnitSorter.cpp cannot sit between
// the two units (case-insensitive .dsp order: EESavedGroup < EESelectedGroup < EESGUnitSorter). 0x208ef3 is this
// class's Assign (it reads this+0x3c); names.txt's first spelling is EEGroup::Assign (EEMessage.cpp calls that).
#include <windows.h>
#include <stdio.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUserInterface.h"
#include "ee/EESavedGroup.h"

// Creates an empty hot-key group with its index for a player
EESavedGroup::EESavedGroup(EEPlayer* inPlayer, long inIndex) : EEGroup(inPlayer), mIndex(inIndex)
{
}

// Destroys the hot-key group, releasing its units
EESavedGroup::~EESavedGroup()
{
    this->Clear();
}

// Adds a living unit; refreshes the hot-key button when it becomes the group's first unit
void EESavedGroup::Add(EEComplexUnit* inUnit)
{
    if (!(inUnit->mHitPoints > 0))
        return;
    EEGroup::Add(inUnit);
    if (inUnit == this->GetFirstUnit() && gUserInterface != NULL)
        gUserInterface->PostRefreshSavedGroup(this->mIndex);
}

// Fills the hot-key group from the sorted selection, refreshes its button and links the selection to it
void EESavedGroup::Assign(EESelectedGroup* inGroup)
{
    inGroup->SortUnits();
    this->CopyUnits(inGroup);
    if (gUserInterface != NULL)
        gUserInterface->PostRefreshSavedGroup(this->mIndex);
    inGroup->mSavedGroup = this;
}

// Reads the group from the saved game (as a plain group)
long EESavedGroup::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return EEGroup::Load(inMajorVersion, inMinorVersion, inFile);
}

// Removes a unit, refreshing the hot-key button first when it was the group's first unit
void EESavedGroup::RemoveUnit(EEComplexUnit* inUnit)
{
    if (inUnit == this->GetFirstUnit() && gUserInterface != NULL)
        gUserInterface->PostRefreshSavedGroup(this->mIndex);
    EEGroup::RemoveUnit(inUnit);
}

// Turns the loaded unit ids back into units (as a plain group)
long EESavedGroup::RepairPointers(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    return EEGroup::RepairPointers(inMajorVersion, inMinorVersion);
}

// Writes the group to the saved game (as a plain group)
long EESavedGroup::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    return EEGroup::Save(inMajorVersion, inMinorVersion, outFile);
}

// Drops dead, invalid or converted units; refreshes the hot-key button again when the first one went
void EESavedGroup::Validate()
{
    bool theFirstRemoved = false;
    for (unsigned long i = 0; i < this->GetNumUnits(); ) {
        if (this->GetUnit(i)->mIsValid && this->GetUnit(i)->mHitPoints > 0 && this->GetUnit(i)->mPlayer == this->mPlayer)
            i++;
        else {
            this->RemoveUnit(this->GetUnit(i));
            if (i == 0)
                theFirstRemoved = true;
        }
    }
    if (theFirstRemoved == true && gUserInterface != NULL)
        gUserInterface->PostRefreshSavedGroup(this->mIndex);
}

// A saved group takes the given (selected) group's units (EEMSavedGroupCreate: the player's saved group and its
// active group)
void EEGroup::Assign(EEGroup* inGroup)
{
    static_cast<EESavedGroup*>(this)->Assign(static_cast<EESelectedGroup*>(inGroup));
}
