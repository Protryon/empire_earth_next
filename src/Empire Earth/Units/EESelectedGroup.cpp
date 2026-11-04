// C:\EEX\Empire Earth\Units\EESelectedGroup.cpp (file name inferred: no __FILE__). The player's selection groups:
// 0x209008..0x20a3c0 (see EESavedGroup.cpp for the split). Order: EESGUnitSorter's comparison
// (0x209008/0x209018, used only by the sort templates), EESelectedGroup's dtor and members, the vtable slots, SortUnits
// with the sorter's inline ??_G (0x20944c) after it, Validate, ShowSelectionAreas, then the template COMDATs: vector<16-byte
// rectangle>::push_back/insert (0x2097cb..0x209bc0), std::sort over mUnits with EESGUnitSorter (0x209b15..0x20a279).
#include <windows.h>
#include <stdio.h>
#include <algorithm>
#include <map>

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
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUserInterface.h"
#include "ee/EEAIContinentManager.h"
#include "lle/U2DRectangle.h"
#include "ee/EETileCoordinate.h"
#include "ee/EEMap.h"
#include "ee/EEAEManager.h"
#include "ee/EEAreaEffectTemplate.h"
#include "ee/EEAIKindIDs.h"

// The order units are sorted in when a selection changes (RTTI EESGUnitSorter; vtable 0x446fb8: only its
// user-declared empty virtual dtor, whose ??_G 0x20944c follows SortUnits, its first user) (provisional names).
static const float kWounded = 0.1f;   // 0x446fb0 (provisional name)

class EESGUnitSorter {
public:
    virtual ~EESGUnitSorter() {}
    bool operator()(EEComplexUnit* const& inFirst, EEComplexUnit* const& inSecond) const;   // 0x209008
    bool Compare(EEComplexUnit* const& inFirst, EEComplexUnit* const& inSecond) const;      // 0x209018
};

// FUNCTION: EXE 0x609008
// Returns whether the first unit sorts before the second in a selection (see Compare)
bool EESGUnitSorter::operator()(EEComplexUnit* const& inFirst, EEComplexUnit* const& inSecond) const
{
    return this->Compare(inFirst, inSecond);
}

// FUNCTION: EXE 0x609018
// Orders selected units: same class healthy before wounded (<= 10% hit points), then by id; else shorter range first,
// then smaller scale if both are ranged, else stronger attack, then unit type
bool EESGUnitSorter::Compare(EEComplexUnit* const& inFirst, EEComplexUnit* const& inSecond) const
{
    EEUnitClass* theFirstClass = inFirst->mUnitClass;
    EEUnitClass* theSecondClass = inSecond->mUnitClass;
    EEUnitStats* theFirstStats = inFirst->mStats;
    EEUnitStats* theSecondStats = inSecond->mStats;
    float theSecondHealth = (float)inSecond->mHitPoints / theSecondStats->mMaxHitPoints;
    float theFirstHealth = (float)inFirst->mHitPoints / theFirstStats->mMaxHitPoints;
    bool theFirstWounded = theFirstHealth <= kWounded;
    bool theSecondWounded = theSecondHealth <= kWounded;
    bool theResult;
    if (theFirstClass == theSecondClass) {
        if (!theFirstWounded && theSecondWounded == true)
            theResult = true;
        else if (theFirstWounded == true && !theSecondWounded)
            theResult = false;
        else
            theResult = inFirst->mUnitID < inSecond->mUnitID;
    } else if (theFirstStats->GetRange() < theSecondStats->GetRange())
        theResult = true;
    else if (theFirstStats->GetRange() > theSecondStats->GetRange())
        theResult = false;
    else if (!theFirstClass->mIsMelee && !theSecondClass->mIsMelee) {
        if (theFirstStats->GetScale() < theSecondStats->GetScale())
            theResult = true;
        else if (theFirstStats->GetScale() > theSecondStats->GetScale())
            theResult = false;
        else
            theResult = theFirstClass->mUnitType < theSecondClass->mUnitType;
    } else if (theFirstStats->GetAttack() > theSecondStats->GetAttack())
        theResult = true;
    else if (theFirstStats->GetAttack() < theSecondStats->GetAttack())
        theResult = false;
    else
        theResult = theFirstClass->mUnitType < theSecondClass->mUnitType;
    return theResult;
}

// FUNCTION: EXE 0x609144
// Destroys the selection, clearing it first
EESelectedGroup::~EESelectedGroup()
{
    this->Clear();
}

// FUNCTION: EXE 0x60915b
// Copies another selection's units, sort flag and saved group, and stamps the change time
EESelectedGroup& EESelectedGroup::CopySelection(EESelectedGroup* inGroup)
{
    this->CopyGroup(inGroup);
    this->mNeedsSort = inGroup->mNeedsSort;
    this->mSavedGroup = inGroup->mSavedGroup;
    this->mChangeTime = TMGetTime();
    return *this;
}

// FUNCTION: EXE 0x609185
// Adds a unit, optionally marking the selection for sorting; for the current player updates the UI and area outlines
void EESelectedGroup::AddUnit(EEComplexUnit* inUnit, bool inNeedsSort)
{
    EEGroup::Add(inUnit);
    if (inNeedsSort) {
        this->mNeedsSort = true;
    }
    if (this->mPlayer == gWorld.GetCurrentPlayer()) {
        gUserInterface->PostMessage424Once();
        this->RefreshSelectionAreas(inUnit);
    }
    this->mChangeTime = TMGetTime();
}

// FUNCTION: EXE 0x6091d1
// Adds a valid unit that is not selected yet, marking the selection for sorting
void EESelectedGroup::Add(EEComplexUnit* inUnit)
{
    this->AddIfAbsent(inUnit, true);
}

// FUNCTION: EXE 0x6091df
// Adds a valid unit that is not selected yet and forgets the saved group the selection came from
void EESelectedGroup::AddIfAbsent(EEComplexUnit* inUnit, bool inNeedsSort)
{
    if (inUnit->mIsValid && !this->Contains(inUnit)) {
        this->AddUnit(inUnit, inNeedsSort);
        this->mSavedGroup = NULL;
    }
}

// FUNCTION: EXE 0x60920c
// Selects a saved group's units, remembering the group only if asked (its index is searched for but unused)
void EESelectedGroup::SelectSavedGroup(EEGroup* inGroup, bool inRememberGroup)
{
    this->CopyUnits(inGroup);
    for (unsigned long i = 0; i < 20; i++) {
        if (this->mPlayer->mGroups[i] == inGroup)
            break;
    }
    if (inRememberGroup) {
        this->mSavedGroup = inGroup;
    } else {
        this->mSavedGroup = NULL;
    }
    this->mChangeTime = TMGetTime();
}

// FUNCTION: EXE 0x60924f
// Returns the continent id most of the selected units are on
unsigned short EESelectedGroup::GetContinent()
{
    std::map<unsigned short, unsigned long> theCounts;
    for (unsigned long i = 0, theNumUnits = this->GetNumUnits(); i < theNumUnits; i++) {
        unsigned short theContinent = gContinentManager.GetContinentID(this->GetUnit(i));
        theCounts[theContinent] = theCounts[theContinent] + 1;
    }
    // the most common continent (the first, i.e. lowest id, on a tie)
    std::map<unsigned short, unsigned long>::iterator theBest = theCounts.begin();
    for (std::map<unsigned short, unsigned long>::iterator it = theCounts.begin(); it != theCounts.end(); ++it) {
        if (it->second > theBest->second)
            theBest = it;
    }
    return theBest->first;
}

// FUNCTION: EXE 0x60930f
// Reads the base group and the saved group's index from the saved game; returns the first read error
long EESelectedGroup::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = EEGroup::Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult != 0)
        return theResult;
    unsigned long theIndex;
    theResult = inFile.Read(theIndex);
    this->mSavedGroup = reinterpret_cast<EEGroup*>(theIndex);
    return theResult;
}

// FUNCTION: EXE 0x609340
// Empties the selection, clears its sort flag, refreshes the area outlines and stamps the change time
void EESelectedGroup::Clear()
{
    EEGroup::Clear();
    this->mNeedsSort = false;
    this->ShowSelectionAreas();
    this->mChangeTime = TMGetTime();
}

// FUNCTION: EXE 0x60935e
// Removes a unit, refreshing the area outlines and (for the current player) the UI, and forgets the saved group
void EESelectedGroup::RemoveUnit(EEComplexUnit* inUnit)
{
    EEGroup::RemoveUnit(inUnit);
    this->RefreshSelectionAreas(inUnit);
    if (this->mPlayer == gWorld.GetCurrentPlayer()) {
        gUserInterface->PostMessage424Once();
    }
    this->mSavedGroup = NULL;
    this->mChangeTime = TMGetTime();
}

// FUNCTION: EXE 0x6093a3
// Turns the loaded saved-group index back into the player's group (-1 for none) and refreshes the area outlines
long EESelectedGroup::RepairPointers(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    long theResult = EEGroup::RepairPointers(inMajorVersion, inMinorVersion);
    if (this->mSavedGroup == (EEGroup*)-1) {
        this->mSavedGroup = NULL;
    } else {
        this->mSavedGroup = this->mPlayer->mGroups[reinterpret_cast<long>(this->mSavedGroup)];
    }
    this->ShowSelectionAreas();
    return theResult;
}

// FUNCTION: EXE 0x6093df
// Writes the base group and the saved group's index (-1 if none) to the saved game; returns the first write error
long EESelectedGroup::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    unsigned long theIndex = -1;
    long theResult = EEGroup::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult != 0)
        return theResult;
    for (unsigned long i = 0; i < 20; i++) {
        if (this->mPlayer->mGroups[i] == this->mSavedGroup)
            theIndex = i;
    }
    theResult = outFile.Write(theIndex);
    return theResult;
}

// FUNCTION: EXE 0x60942f
// Sorts the selected units with EESGUnitSorter if a change asked for it
void EESelectedGroup::SortUnits()
{
    if (this->mNeedsSort) {
        SortBy(EESGUnitSorter());
        this->mNeedsSort = false;
    }
}

// SYNTHETIC: EXE 0x60944c
// (EESGUnitSorter's scalar deleting destructor)

// FUNCTION: EXE 0x609469
// Drops dead units without a tile (keeping aircraft and spaceships inside a carrier), moves along found paths and
// refreshes the area outlines if units remain
void EESelectedGroup::Validate()
{
    unsigned long i = 0;
    while (i < this->GetNumUnits()) {
        if (this->GetUnit(i)->mIsValid == true || this->GetUnit(i)->mTile != NULL)
            i++;
        else {
            long theCategory = this->GetUnit(i)->mUnitClass->mCategory;
            if ((theCategory == kUnitCategoryAirplane || theCategory == kUnitCategoryPlanetSpaceship) && this->GetUnit(i)->mContainer != NULL)
                i++;
            else
                this->RemoveUnit(this->GetUnit(i));
        }
    }
    if (this->mPathRequests.size() > 0)
        this->MoveAlongFoundPaths();
    if (this->GetNumUnits() > 0)
        this->ShowSelectionAreas();
}

// the area-effect ranges are shown on the map (ShowSelectionAreas; 0x5634c8) (provisional name)
static bool sAreasShown;

// FUNCTION: EXE 0x609500
// Outlines on the map the area-effect range and continent of every built building of the type of the first built
// building (not an airport) with an area effect in the current player's selection; else clears the outlines
// The range is the template's radius times the stats' area-effect scale (+0x84)
void EESelectedGroup::ShowSelectionAreas()
{
    std::vector<U2DRectangle<long> > theAreas;
    std::vector<unsigned short> theContinents;
    float theRadius = 1.0f;
    bool theFound = false;
    U2DRectangle<long> theArea;
    unsigned long theType = 0;
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    if (thePlayer == NULL || this != thePlayer->mSelectedGroup)
        return;
    unsigned long i;
    for (i = 0; i < this->GetNumUnits(); i++) {
        EEComplexUnit* theUnit = this->GetUnit(i);
        if (theUnit->mUnitClass->mCategory != kUnitCategoryBuilding || !theUnit->mIsBuilt || theUnit->mUnitClass->mUnitType == kUnitTypeAirport)
            continue;
        for (long k = 0; k < 4; k++) {
            unsigned long theTemplate = k < 4 ? theUnit->mStats->mAreaEffectTemplates[k] : 0;
            if (theTemplate <= 0)
                continue;
            theFound = true;
            theType = theUnit->mUnitClass->mUnitType;
            theRadius = theUnit->mStats->mAreaEffectScale * gAEManager.mTemplates[theTemplate]->mRadius;
            break;
        }
        if (theFound)
            break;
    }
    if (!theFound) {
        if (sAreasShown == true) {
            gWorld.mMap->ClearU1955DC();
            sAreasShown = false;
        }
        return;
    }
    for (EEUnit* theOther = gWorld.GetCurrentPlayer()->mFirstUnit; theOther != NULL;
         theOther = static_cast<EEUnit*>(theOther->mNext)) {
        if (theOther->mUnitClass->mUnitType != theType)
            continue;
        EEComplexUnit* theComplex = theOther->mComplexUnit;
        if (!theComplex->mIsBuilt)
            continue;
        float theX = theComplex->mPhysicalModel.GetLocation().GetX();
        float theY = theComplex->mPhysicalModel.GetLocation().GetY();
        theArea.mLeft = UMFloor(theX - theRadius);
        theArea.mRight = UMFloor(theX + theRadius);
        theArea.mTop = UMFloor(theY - theRadius);
        theArea.mBottom = UMFloor(theY + theRadius);
        theAreas.push_back(theArea);
        unsigned short theContinent;
        if (theComplex->mUnitClass->mUnitType == kUnitTypeSpaceDock) {
            theContinent = 0;
        } else {
            EETileCoordinate theTile;
            theTile.mX = UMFloor(theX);
            theTile.mY = UMFloor(theY);
            theContinent = gContinentManager.GetContinent(theTile, kAIPathNonSpace);
        }
        theContinents.push_back(theContinent);
    }
    gWorld.mMap->BuildAreaOutlines(theAreas, theContinents);
    sAreasShown = true;
}

// FUNCTION: EXE 0x6097b3
// Refreshes the area outlines when the unit is a building
void EESelectedGroup::RefreshSelectionAreas(EEComplexUnit* inUnit)
{
    if (inUnit->mUnitClass->mCategory == kUnitCategoryBuilding) {
        this->ShowSelectionAreas();
    }
}


// The template COMDATs of the unit, in the original's order (the vector<rectangle> push_back/insert members
// 0x2097cb..0x209bc0 and the map search 0x209ad6 come from the unwritten ShowSelectionAreas/GetContinent):
// TEMPLATE: EXE 0x609b15
// (EESelectedGroup::SortBy<EESGUnitSorter>)
// TEMPLATE: EXE 0x609bc0
// (std::sort)
// TEMPLATE: EXE 0x609ca0
// (EESGUnitSorter's implicit copy ctor)
// TEMPLATE: EXE 0x609cab
// (std::_Insertion_sort)
// TEMPLATE: EXE 0x609cdd
// (std::_Sort)
// TEMPLATE: EXE 0x609dc7
// (std::_Unguarded_insertion_sort)
// TEMPLATE: EXE 0x609e03
// (std::_Insertion_sort_1)
// TEMPLATE: EXE 0x609eb4
// (std::make_heap)
// TEMPLATE: EXE 0x609ef7
// (std::sort_heap)
// TEMPLATE: EXE 0x609f3e
// (std::_Unguarded_partition)
// TEMPLATE: EXE 0x60a04f
// (std::_Unguarded_insert)
// TEMPLATE: EXE 0x60a07f
// (std::_Make_heap)
// TEMPLATE: EXE 0x60a0d6
// (std::pop_heap)
// TEMPLATE: EXE 0x60a108
// (std::_Adjust_heap)
// TEMPLATE: EXE 0x60a17b
// (std::_Pop_heap_0)
// TEMPLATE: EXE 0x60a1ea
// (std::_Push_heap)
// TEMPLATE: EXE 0x60a23b
// (std::_Pop_heap)
// TEMPLATE: EXE 0x609ad6
// (std::max_element over the continent counts; it sits before SortBy in the original: first reference order)

// TEMPLATE: EXE 0x6097cb
// (std::vector<U2DRectangle<long> >::push_back)

// TEMPLATE: EXE 0x6097f7
// (std::vector<U2DRectangle<long> >::insert(iterator, const T&))

// TEMPLATE: EXE 0x609847
// (std::vector<U2DRectangle<long> >::insert(iterator, size_type, const T&))

// TEMPLATE: EXE 0x609b5c
// (std::fill over the 16-byte rectangle elements of ShowSelectionAreas's vector)

// TEMPLATE: EXE 0x609b8a
// (std::copy_backward over the same elements)
