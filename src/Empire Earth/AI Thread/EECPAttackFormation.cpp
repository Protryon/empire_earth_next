// C:\EEX\Empire Earth\AI Thread\EECPAttackFormation.cpp (file name inferred: no __FILE__; RTTI classes
// EECPAttackFormation, vtable 0x43bc28, and EECPAttackFormationUnitAnalysis, vtable 0x43bc6c). An attack sub group's
// formation: 0x529e7..0x54304 (EECPAttackCommander.cpp ends at 0x529e7; EECPAttackFormationNode's ctor starts at
// 0x54304). The ctor, ??_G, dtor, the slots in vtable order with the unit analysis' inline ctor/dtor/??_G after their
// first user Virtual08, then the template COMDATs (map erase/clear, vector push_back/pop_back/insert/erase, the
// range sort with EECPAFRangeGreater)
#include <windows.h>
#include <vector>
#include <deque>
#include <map>
#include <set>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "ee/EmpireEarth.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFile.h"
#include "lle/FSRestorable.h"
#include "lle/USingleton.h"
#include "lle/U2DPoint.h"
#include "lle/U2DRectangle.h"

#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EECPILocation.h"
#include "ee/EECPAGTask.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EECPAttackSubGroupUnit.h"
#include "ee/EECPAttackFormation.h"

static const float kAFHalf = 0.5f;          // 0x43bc20
// 1.5f (0x43bc70) is the pooled __real literal, read by ~19 units (not a unit-local const)
// 1.25f (0x43bc74) is the pooled __real literal, read by ~6 units (not a unit-local const)
static const float kAFInvTwoPi = 0.15915494f;   // 0x43bc78

// the sort predicate of Virtual3C: longer range first, then fewer hit points (COMDAT 0x53f36) (provisional name)
struct EECPAFRangeGreater {
    bool operator()(EECPAttackSubGroupUnit* inA, EECPAttackSubGroupUnit* inB) const
    {
        float theRangeA = inA->mUnit->mStats->mRange;
        float theRangeB = inB->mUnit->mStats->mRange;
        bool theResult = false;
        if (theRangeA > theRangeB)
            theResult = true;
        else if (theRangeA == theRangeB && inA->mUnit->mHitPoints < inB->mUnit->mHitPoints)
            theResult = true;
        return theResult;
    }
};

typedef std::pair<long, EECPAttackSubGroupUnit*> EECPAFTypedUnit;

// FUNCTION: EXE 0x4529e7
// Creates an empty formation for a sub-group, holding a reference to it
EECPAttackFormation::EECPAttackFormation(EECPAttackSubGroup* inSubGroup)
    : mNoForcedMove(false)
{
    this->mSubGroup = inSubGroup;
    if (inSubGroup)
        inSubGroup->Reference();
}

// SYNTHETIC: EXE 0x452a1a
// (the scalar deleting destructor)

// FUNCTION: EXE 0x452a36
// Destroys the formation, deleting its rows and releasing its sub-group
EECPAttackFormation::~EECPAttackFormation()
{
    this->DeleteNodes();
    if (this->mSubGroup)
        this->mSubGroup->Dereference();
}

// FUNCTION: EXE 0x452a5b
// Sorts units into support (priests, kind 0x1b, any damaged important unit: integer hit-point ratio), fast,
// kind-6 ranged, ranged and melee sets; a lone fast unit joins the melee set, a lone kind-6 unit the ranged set
void EECPAttackFormation::Analyze(EECPAttackFormationUnitAnalysis& outAnalysis)
{
    EECPAFUnitsByType::iterator theIt1;
    EECPAFUnitsByType::iterator theIt2;
    unsigned long i = 0;
    if (this->mSubGroup->GetNumUnits() > 0) {
        do {
            EECPAttackSubGroupUnit* theUnit = i < this->mSubGroup->GetNumUnits() ? this->mSubGroup->mUnits[i] : 0;
            EEUnitClass* theClass = theUnit->mUnit->GetUnitClass();
            long theKind;
            if (theClass->IsPriest() || (theKind = theClass->mAIKind) == 0x1b ||
                theUnit->mIsImportant && (float)theUnit->mUnit->mHitPoints / theUnit->mUnit->mStats->mMaxHitPoints < 0.45f)   // a float ratio (was integer division)
                outAnalysis.mSupportUnits.insert(EECPAFTypedUnit(theClass->mUnitType, theUnit));
            else if (theClass->mIsHorse && theUnit->mUnit->mStats->mMaxVelocity > 1.0f &&
                     theUnit->mUnit->GetUnitClass()->mAIKind != kAIKindArcher)
                outAnalysis.mFastUnits.insert(EECPAFTypedUnit(theClass->mUnitType, theUnit));
            else if (theUnit->mUnit->mStats->mRange > 1.0f) {
                if (theKind == 6)
                    outAnalysis.mU04.insert(EECPAFUnitsByType::value_type(theClass->mUnitType, theUnit));
                else
                    outAnalysis.mRangedUnits.insert(std::pair<float, EECPAttackSubGroupUnit*>(
                        theUnit->mUnit->mStats->GetRange(), theUnit));
            } else
                outAnalysis.mMeleeUnits.insert(EECPAFUnitsByType::value_type(theClass->mUnitType, theUnit));
            i++;
        } while (i < this->mSubGroup->GetNumUnits());
    }
    if (outAnalysis.mFastUnits.size() > 0 && outAnalysis.mFastUnits.size() < 2) {
        for (theIt1 = outAnalysis.mFastUnits.begin(); theIt1 != outAnalysis.mFastUnits.end(); theIt1++) {
            EECPAttackSubGroupUnit* theUnit = theIt1->second;
            outAnalysis.mMeleeUnits.insert(EECPAFUnitsByType::value_type(theUnit->mUnit->GetUnitClass()->mUnitType, theUnit));
        }
        outAnalysis.mFastUnits.clear();
    }
    if (outAnalysis.mU04.size() <= 0 || outAnalysis.mU04.size() >= 2)
        return;
    for (theIt2 = outAnalysis.mU04.begin(); theIt2 != outAnalysis.mU04.end(); theIt2++) {
        EECPAttackSubGroupUnit* theUnit = theIt2->second;
        outAnalysis.mRangedUnits.insert(EECPAFUnitsByRange::value_type(theUnit->mUnit->mStats->GetRange(), theUnit));
    }
}

// FUNCTION: EXE 0x452cfa
// Returns the row width for a unit set: 5 above 11 units, 4 above 9, else 3
long EECPAttackFormation::GetColumnWidth(EECPAFUnitsByType& inUnits)
{
    long theWidth = 3;
    if (inUnits.size() <= 0)
        return theWidth;
    if (inUnits.size() > 11)
        theWidth = 5;
    else if (inUnits.size() > 9)
        theWidth = 4;
    return theWidth;
}

// FUNCTION: EXE 0x452d1c
// Rebuilds the formation: deletes the rows, sorts the units and arranges new rows
void EECPAttackFormation::Build()
{
    EECPAttackFormationUnitAnalysis theAnalysis;
    this->DeleteNodes();
    this->Analyze(theAnalysis);
    this->Arrange(theAnalysis);
}

// FUNCTION: EXE 0x452d55
// Creates an empty unit analysis (all unit sets empty)
// EECPAttackFormationUnitAnalysis::EECPAttackFormationUnitAnalysis (inline, ee/EECPAttackFormation.h)

// FUNCTION: EXE 0x452dc6
// Destroys the unit analysis and its unit sets
// EECPAttackFormationUnitAnalysis::~EECPAttackFormationUnitAnalysis (inline, ee/EECPAttackFormation.h)

// SYNTHETIC: EXE 0x452df9
// (EECPAttackFormationUnitAnalysis's scalar deleting destructor)

// FUNCTION: EXE 0x452e15
// Arranges the rows the default way
void EECPAttackFormation::Arrange(EECPAttackFormationUnitAnalysis& inAnalysis)
{
    this->ArrangeDefault(inAnalysis);
}

// FUNCTION: EXE 0x452e21
// Arranges rows: kind-6 ranged units in front, melee behind them, fast units in a random flank shape, then
// ranged units by range (wider rows above 6 of them)
void EECPAttackFormation::Virtual20(EECPAttackFormationUnitAnalysis& inAnalysis)
{
    EECPAttackFormationNode* theNode = NULL;
    if (inAnalysis.mU04.size() > 0)
        theNode = this->MakeNodes(inAnalysis.mU04, NULL, 2, 2, 0, 6, 1.0f, inAnalysis.mU04.size() >= 6, false);
    if (inAnalysis.mMeleeUnits.size() > 0)
        theNode = this->MakeNodes(inAnalysis.mMeleeUnits, theNode, 0, 2, 0, 8, 1.0f, true, true);
    if (inAnalysis.mFastUnits.size() > 0) {
        long theType = gPhysics->GetRandom(0L, 1L) ? 4 : 5;
        this->MakeNodes(inAnalysis.mFastUnits, theNode, 1, 3, theType, 7, 1.25f, false, false);
    }
    if (inAnalysis.mRangedUnits.size() <= 0)
        return;
    long theL60;
    long theType;
    if (inAnalysis.mRangedUnits.size() > 6) {
        theL60 = 2;
        theType = 0;
    } else {
        theL60 = 3;
        theType = 5;
    }
    this->MakeNodesByRange(inAnalysis.mRangedUnits, theNode, 0, theL60, theType, 6, 1.5f, true, true);
}

// FUNCTION: EXE 0x452efe
// Arranges two blocks: kind-6 units join the ranged set and fast units the melee set; melee rows first, then
// ranged rows in a random shape at double spacing
void EECPAttackFormation::Virtual24(EECPAttackFormationUnitAnalysis& inAnalysis)
{
    EECPAFUnitsByType::iterator theIt1;
    EECPAFUnitsByType::iterator theIt2;
    EECPAttackFormationNode* theNode = NULL;
    for (theIt1 = inAnalysis.mU04.begin(); theIt1 != inAnalysis.mU04.end(); theIt1++) {
        EECPAttackSubGroupUnit* theUnit = theIt1->second;
        inAnalysis.mRangedUnits.insert(EECPAFUnitsByRange::value_type(theUnit->mUnit->mStats->GetRange(), theUnit));
    }
    for (theIt2 = inAnalysis.mFastUnits.begin(); theIt2 != inAnalysis.mFastUnits.end(); theIt2++)
        inAnalysis.mMeleeUnits.insert(*theIt2);
    if (inAnalysis.mMeleeUnits.size() > 0)
        theNode = this->MakeNodes(inAnalysis.mMeleeUnits, NULL, 0, 2, 0, 0, 1.0f, false, true);
    if (inAnalysis.mRangedUnits.size() > 0) {
        long theType = gPhysics->GetRandom(0L, 1L) ? 4 : 5;
        this->MakeNodesByRange(inAnalysis.mRangedUnits, theNode, 0, 3, theType, 0, 2.0f, false, false);
    }
}

// FUNCTION: EXE 0x452fec
// Arranges rows: melee first, fast units as one flank row on the first row, then kind-6 and ranged rows in a
// random shape
void EECPAttackFormation::Virtual28(EECPAttackFormationUnitAnalysis& inAnalysis)
{
    EECPAttackFormationNode* theNode = NULL;
    if (inAnalysis.mMeleeUnits.size() > 0)
        theNode = this->MakeNodes(inAnalysis.mMeleeUnits, NULL, 0, 2, 0, 0, 1.0f, true, true);
    if (inAnalysis.mFastUnits.size() > 0) {
        this->MakeNodes(inAnalysis.mFastUnits, this->mNodes.size() > 0 ? this->mNodes.front() : theNode, 1, 1, 2, inAnalysis.mFastUnits.size(), 1.0f,
            false, false);
    }
    if (inAnalysis.mU04.size() > 0) {
        long theType = gPhysics->GetRandom(0L, 1L) ? 0 : 2;
        theNode = this->MakeNodes(inAnalysis.mU04, theNode, 2, 2, theType, 0, 1.0f, true, false);
    }
    if (inAnalysis.mRangedUnits.size() > 0) {
        long theType = gPhysics->GetRandom(0L, 1L) ? 0 : 2;
        this->MakeNodesByRange(inAnalysis.mRangedUnits, theNode, 0, 2, theType, 0, 1.0f, true, true);
    }
}

// FUNCTION: EXE 0x4530d0
// Arranges up to three single rows one behind another: support units, ranged units, then all other units, each
// deeper than the last; the same random flag goes to each row
void EECPAttackFormation::ArrangeDefault(EECPAttackFormationUnitAnalysis& inAnalysis)
{
    EECPAFUnitsByType theUnits;
    float theDepth = 0.0f;
    EECPAFUnitsByType::iterator theIt;
    EECPAttackFormationNode* theNode = NULL;
    bool theFlag = gPhysics->GetRandom(0L, 1L) != 0;
    if (inAnalysis.mSupportUnits.size() > 0) {
        theNode = this->MakeNode(inAnalysis.mSupportUnits, NULL, 0, 4, 7, 0, 0.0f, false, theFlag);
        theDepth = (theNode ? theNode->mDepth : 1.0f) + 1.5f;
    }
    if (inAnalysis.mRangedUnits.size() > 0) {
        theNode = this->MakeNodeByRange(inAnalysis.mRangedUnits, theNode, 0, 4, 7, 0, theDepth, false, theFlag);
        theDepth = (theNode ? theNode->mDepth : 1.0f) + 1.0f;
    }
    for (theIt = inAnalysis.mU04.begin(); theIt != inAnalysis.mU04.end(); theIt++)
        theUnits.insert(*theIt);
    for (theIt = inAnalysis.mMeleeUnits.begin(); theIt != inAnalysis.mMeleeUnits.end(); theIt++)
        theUnits.insert(*theIt);
    for (theIt = inAnalysis.mFastUnits.begin(); theIt != inAnalysis.mFastUnits.end(); theIt++)
        theUnits.insert(*theIt);
    if (theUnits.size() > 0)
        this->MakeNode(theUnits, theNode, 0, 4, 7, 0, theDepth, false, theFlag);
}

// FUNCTION: EXE 0x453244
// Re-keys a by-range unit set by unit type and makes one row of it
EECPAttackFormationNode* EECPAttackFormation::MakeNodeByRange(EECPAFUnitsByRange& inUnits,
    EECPAttackFormationNode* inPrevious, unsigned long inFlags, long inL60, long inType, long inWidth, float inSpacing,
    bool inArg8, bool inArg9)
{
    EECPAFUnitsByType theUnits;
    EECPAFUnitsByRange::iterator theIt;
    for (theIt = inUnits.begin(); theIt != inUnits.end(); theIt++) {
        EECPAttackSubGroupUnit* theUnit = theIt->second;
        theUnits.insert(EECPAFUnitsByType::value_type(theUnit->mUnit->mUnitClass->mUnitType, theUnit));
    }
    return this->MakeNode(theUnits, inPrevious, inFlags, inL60, inType, inWidth, inSpacing, inArg8, inArg9);
}

// FUNCTION: EXE 0x4532e9
// Makes one row of all the units (the width argument is unused), sorted longest range first; sets its depth from
// the largest collision radius and returns it (NULL for no units)
EECPAttackFormationNode* EECPAttackFormation::MakeNode(EECPAFUnitsByType& inUnits,
    EECPAttackFormationNode* inPrevious, unsigned long inFlags, long inL60, long inType, long inWidth, float inSpacing,
    bool inArg8, bool inArg9)
{
    EECPAFUnitsByType::iterator theIt;
    std::vector<EECPAttackSubGroupUnit*> theUnits;
    std::vector<EECPAFTypedUnit> theUnused;
    EECPAttackFormationNode* theNode = NULL;
    float theDepth = 1.0f;
    long theIndex = 0;
    if (inUnits.size() > 0) {
        theUnits.reserve(inUnits.size());
        for (theIt = inUnits.begin(); theIt != inUnits.end(); theIt++)
            theUnits.push_back(theIt->second);
        long theWidth = theUnits.size();
        if (theWidth != 0) {
            std::sort(theUnits.begin(), theUnits.end(), EECPAFRangeGreater());
            while (theUnits.size() != 0) {
                theNode = new EECPAttackFormationNode;
                theNode->mL60 = inL60;
                theNode->mType = inType;
                theNode->mPrevious = inPrevious;
                theNode->mFlags = inFlags;
                theNode->mSpacing = inSpacing;
                theNode->mIndex = theIndex++;
                theNode->mDepth = theDepth;
                if (inArg8)
                    theNode->mFlags |= 4;
                if (!(theNode->mFlags & 1))
                    inPrevious = theNode;
                long theCount = theWidth < (long)theUnits.size() ? theWidth : theUnits.size();
                float theRange = kAFHalf;
                for (long i = theCount - 1; i >= 0; i--) {
                    theNode->mUnits.push_back(theUnits.back());
                    if (theUnits[i]->mUnit->GetUnitClass()->mOuterCollisionSphere > theRange)
                        theRange = theUnits[i]->mUnit->GetUnitClass()->mOuterCollisionSphere;
                    theUnits.pop_back();
                }
                float theRow = theNode->mUnits.size() * theRange * kAFInvTwoPi;
                float theNext = theRange + theDepth;
                if (theNext > theRow)
                    theDepth = theNext;
                else
                    theDepth = theRow;
                if (inArg9)
                    theNode->mFlags |= 8;
                this->mNodes.push_back(theNode);
            }
        }
    }
    return theNode;
}

// FUNCTION: EXE 0x453569
// Re-keys a by-range unit set by unit type and makes rows of it
EECPAttackFormationNode* EECPAttackFormation::MakeNodesByRange(EECPAFUnitsByRange& inUnits,
    EECPAttackFormationNode* inPrevious, unsigned long inFlags, long inL60, long inType, long inWidth, float inSpacing,
    bool inArg8, bool inArg9)
{
    EECPAFUnitsByType theUnits;
    EECPAFUnitsByRange::iterator theIt;
    for (theIt = inUnits.begin(); theIt != inUnits.end(); theIt++) {
        EECPAttackSubGroupUnit* theUnit = theIt->second;
        theUnits.insert(EECPAFUnitsByType::value_type(theUnit->mUnit->mUnitClass->mUnitType, theUnit));
    }
    return this->MakeNodes(theUnits, inPrevious, inFlags, inL60, inType, inWidth, inSpacing, inArg8, inArg9);
}

// FUNCTION: EXE 0x45360e
// Splits a unit set into rows of the given width (or the column width when 0), interleaving small and large
// units when asked; chains each row to the previous one and returns the last row (NULL for no units)
EECPAttackFormationNode* EECPAttackFormation::MakeNodes(EECPAFUnitsByType& inUnits,
    EECPAttackFormationNode* inPrevious, unsigned long inFlags, long inL60, long inType, long inWidth, float inSpacing,
    bool inArg8, bool inArg9)
{
    std::vector<EECPAFTypedUnit> theNear;
    std::vector<EECPAFTypedUnit> theFar;
    EECPAFUnitsByType::iterator theIt;
    std::vector<EECPAFTypedUnit> theUnits;
    EECPAttackFormationNode* thePrevious = inPrevious;
    long theWidth = inWidth;
    EECPAttackFormationNode* theNode = NULL;
    long theIndex = 0;
    if (!inArg9 && !inArg8) {
        theIt = inUnits.begin();
        while (true) {
            if (theIt == inUnits.end())
                break;
            theUnits.push_back(*theIt);
            theIt++;
        }
    } else {
        theIt = inUnits.begin();
        while (true) {
            if (theIt == inUnits.end())
                break;
            if (theIt->second->mUnit->GetUnitClass()->mInnerCollisionSphere < 0.6f)
                theNear.push_back(*theIt);
            else
                theFar.push_back(*theIt);
            theIt++;
        }
        theUnits.resize(theNear.size() + theFar.size());
        unsigned long i = 0;
        while (i < theUnits.size()) {
            bool theAdded = false;
            if (theNear.size() > 0) {
                theUnits[i] = theNear.back();
                i++;
                theNear.pop_back();
                theAdded = true;
                if (inArg8 && theNear.size() > 0) {
                    theUnits[i] = theNear.back();
                    i++;
                    theNear.pop_back();
                }
            }
            if (theFar.size() > 0) {
                theUnits[i] = theFar.back();
                i++;
                theFar.pop_back();
                theAdded = true;
            }
            if (!theAdded)
                break;
        }
    }
    if (theUnits.size() <= 0)
        return theNode;
    if (inWidth == 0)
        theWidth = this->GetColumnWidth(inUnits);
    while (theUnits.size() > 0) {
        theNode = new EECPAttackFormationNode;
        theNode->mL60 = inL60;
        theNode->mType = inType;
        theNode->mPrevious = thePrevious;
        theNode->mFlags = inFlags;
        theNode->mSpacing = inSpacing;
        theNode->mIndex = theIndex++;
        if (inArg8)
            theNode->mFlags |= 4;
        if (!(theNode->mFlags & 1))
            thePrevious = theNode;
        long theCount = theWidth < (long)theUnits.size() ? theWidth : theUnits.size();
        std::vector<EECPAFTypedUnit>::iterator theUnit = theUnits.begin();
        while (theUnit != theUnits.end() && theCount > 0) {
            theNode->mUnits.push_back(theUnit->second);
            if (inArg8 && theUnit->second->mUnit->GetUnitClass()->mIsCollidable) {
                theUnit = theUnits.erase(theUnit);
                if (theUnit != theUnits.end() && theUnit->second->mUnit->GetUnitClass()->mIsCollidable) {
                    theNode->mUnits.push_back(theUnit->second);
                    theUnit = theUnits.erase(theUnit);
                }
            } else
                theUnit = theUnits.erase(theUnit);
            theCount--;
        }
        this->mNodes.push_back(theNode);
    }
    return theNode;
}

// FUNCTION: EXE 0x4539f7
// Places every row at the target tile's centre, lets the formation shift it, then creates it (units get their
// move orders)
void EECPAttackFormation::Place(const EECPILocation& inLocation, float inArg2)
{
    for (unsigned long i = 0; i < this->mNodes.size(); i++) {
        GE3DPoint thePosition(inLocation.mTile.mX + 0.5f, inLocation.mTile.GetY() + 0.5f, 0.0f);
        this->mNodes[i]->mPosition = thePosition;
        this->PlaceNode(this->mNodes[i], inArg2);
        this->mNodes[i]->Create(this);
        this->mNodes[i]->mIsPlaced = true;
    }
}

// FUNCTION: EXE 0x453a85
// Deletes every row and empties the row list
void EECPAttackFormation::DeleteNodes()
{
    for (long i = this->mNodes.size() - 1; i >= 0; i--)
        delete this->mNodes[i];
    this->mNodes.clear();
}

// TEMPLATE: EXE 0x453ac5
// (multimap<long, EECPAttackSubGroupUnit*>::clear)

// TEMPLATE: EXE 0x453ad9
// (multimap<float, EECPAttackSubGroupUnit*>'s ~_Tree)

// TEMPLATE: EXE 0x453b04
// (vector<EECPAttackFormationNode*>::push_back)

// (ICF, named elsewhere) 0x453b30
// (vector<EECPAFTypedUnit>::back; ICF-shared)

// TEMPLATE: EXE 0x453b37
// (vector<EECPAFTypedUnit>::push_back)

// TEMPLATE: EXE 0x453b63
// (vector<EECPAFTypedUnit>::pop_back)

// TEMPLATE: EXE 0x453b97
// (multimap<float, EECPAttackSubGroupUnit*>::erase(F, L))

// (ICF, named elsewhere) 0x453c08
// (vector<EECPAFTypedUnit>::insert(P, X); ICF-shared)

// TEMPLATE: EXE 0x453c58
// (multimap<float, EECPAttackSubGroupUnit*>::_Erase)

// TEMPLATE: EXE 0x453c8a
// (std::sort with EECPAFRangeGreater)

// (ICF, named elsewhere) 0x453d47
// (std::copy over EECPAFTypedUnit; ICF-shared)

// TEMPLATE: EXE 0x453d6c
// (std::_Insertion_sort)

// TEMPLATE: EXE 0x453d95
// (std::_Sort)

// TEMPLATE: EXE 0x453e68
// (std::_Unguarded_insertion_sort)

// TEMPLATE: EXE 0x453e99
// (std::_Insertion_sort_1)

// FUNCTION: EXE 0x453f36
// Orders units longest range first, then fewer hit points first
// EECPAFRangeGreater::operator() (inline, above)

// TEMPLATE: EXE 0x453f85
// (std::make_heap)

// TEMPLATE: EXE 0x453fbf
// (std::sort_heap)

// TEMPLATE: EXE 0x454000
// (std::_Unguarded_partition)

// TEMPLATE: EXE 0x454120
// (std::_Unguarded_insert)

// TEMPLATE: EXE 0x454153
// (std::_Make_heap)

// TEMPLATE: EXE 0x454197
// (std::pop_heap)

// TEMPLATE: EXE 0x4541c0
// (std::_Adjust_heap)

// TEMPLATE: EXE 0x45422e
// (std::_Pop_heap_0)

// TEMPLATE: EXE 0x454281
// (std::_Push_heap)

// TEMPLATE: EXE 0x4542d0
// (std::_Pop_heap)


// More COMDATs of this unit, each checked against the original
// TEMPLATE: EXE 0x453d47
// TEMPLATE: EXE 0x453b30
// TEMPLATE: EXE 0x453c08
