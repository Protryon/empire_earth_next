// C:\EEX\Empire Earth\Database\EEStartingResources.cpp (name from RTTI EEStartingResources; no __FILE__). 0xda984..
// 0xdae63: the class static table's init chain, ctor, ??_G, dtor, Load (db\dbStartingResources.dat), Unload, the
// table's push_back COMDAT, then the six tag-point helpers 0xdab52..0xdae63 (EETagPointList, EETagPoint).
// EESoundObject.cpp ends at 0xda984, EETerrainColorTable.cpp's chain starts at 0xdae63. Unit flags: the exe's
// module set.

#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/FSFileSpec.h"

#include "ee/EEStartingResources.h"

// One record of db\dbStartingResources.dat (0x84 bytes, read whole). Inferred layout.
struct EEStartingResourcesRecord {
    char mLabel[100];                       // +0x000 the row name (the game never reads it; the text's label)
    int32_t mUnusedDatabaseID;              // +0x064 the database row id (not read)
    int32_t mIndex;                         // +0x68 (negative: skipped)
    int32_t mNameID;                        // +0x6c -> +0x18
    int32_t mFood;                          // +0x70 -> +0x04
    int32_t mWood;                          // +0x74 -> +0x08
    int32_t mStone;                         // +0x78 -> +0x0c
    int32_t mGold;                          // +0x7c -> +0x10
    int32_t mIron;                          // +0x80 -> +0x14
};
static_assert(sizeof(EEStartingResourcesRecord) == 132, "on-disk record: 132 bytes on every target");

std::vector<EEStartingResources*> EEStartingResources::sTable;

// FUNCTION: EXE 0x4da9cd
// Creates a starting-resources preset with every amount zero
EEStartingResources::EEStartingResources()
{
    for (long i = 0; i < 5; i++)
        this->mAmounts[i] = 0;
}

// SYNTHETIC: EXE 0x4da9e2
// (the scalar deleting destructor)

// FUNCTION: EXE 0x4da9fe
// Destroys the starting-resources preset (nothing to release)
EEStartingResources::~EEStartingResources()
{
}

// FUNCTION: EXE 0x4daa05
// Loads the starting-resource presets (food, wood, stone, gold, iron, name) from db\dbStartingResources.dat; returns
// the last read status
long EEStartingResources::Load(FSFileSpec* inFile)
{
    unsigned long theCount = 0;
    unsigned long theRead = 0;
    EEStartingResourcesRecord theRecord;
    inFile->Open(kFileAccessRead);
    long theResult = inFile->Read(theCount);
    if (theResult == kErrorOK) {
        for (unsigned long i = 0; i < theCount; i++) {
            EEStartingResources* theResources;
            theResult = inFile->Read(reinterpret_cast<unsigned char*>(&theRecord), sizeof(theRecord), theRead);
            if (theRecord.mIndex >= 0) {
                theResources = new EEStartingResources;
                theResources->mAmounts[1] = theRecord.mWood;
                theResources->mAmounts[0] = theRecord.mFood;
                theResources->mAmounts[2] = theRecord.mStone;
                theResources->mAmounts[4] = theRecord.mIron;
                theResources->mAmounts[3] = theRecord.mGold;
                theResources->mNameID = theRecord.mNameID;
            } else {
                theResources = NULL;
            }
            if (theResources != NULL)
                EEStartingResources::sTable.push_back(theResources);
        }
    }
    inFile->Close();
    return theResult;
}

// FUNCTION: EXE 0x4daae6
// Destroys every starting-resources preset and empties the table
void EEStartingResources::Unload()
{
    for (unsigned long i = 0; i < EEStartingResources::sTable.size(); i++) {
        delete EEStartingResources::sTable[i];
        EEStartingResources::sTable[i] = NULL;
    }
    EEStartingResources::sTable.clear();
}

// TEMPLATE: EXE 0x4dab26
// std::vector<EEStartingResources*>::push_back

// The tag-point helpers (EETagPointList / EETagPoint, ee/EEGraphicObject.h), defined in this unit's range: included
// after the table's code so the init chain above keeps its numbering
#include <map>
#include "lle/GEColor.h"
#include "lle/GEModel.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicObject.h"

// FUNCTION: EXE 0x4dab52
// Releases every tag point's model reference, frees the points and empties the list
void EETagPointList::Clear()
{
    for (unsigned long i = 0; i < this->mPoints.size(); i++) {
        EETagPoint* thePoint = this->mPoints[i];
        if (thePoint != NULL) {
            thePoint->mModel->Dereference();
            delete thePoint;
        }
        this->mPoints[i] = NULL;
    }
    this->mPoints.clear();
}

// FUNCTION: EXE 0x4dab97
// Computes the tag point's position at a frame, carried through its model and every parent model's transform
void EETagPoint::GetOffset(unsigned long inFrame, GE3DPoint& outPoint)
{
    GE3DPoint thePoint = this->mModel->GetTagPoint(this->mIndex, inFrame);
    GE3DPoint thePrevious;
    for (GEModel* theModel = this->mModel; theModel != NULL; theModel = theModel->GetParentModel()) {
        thePrevious = thePoint;
        theModel->GetInternalTransform(inFrame).Apply(thePrevious, thePoint);
    }
    outPoint = thePoint;
}

// FUNCTION: EXE 0x4dac22
// Computes the tag point's position at a frame with a part transform applied first, then the model chain
void EETagPoint::GetOffset(unsigned long inFrame, const GETransformation& inPart, GE3DPoint& outPoint)
{
    GEModel* theModel = this->mModel;
    GE3DPoint theLocal = theModel->GetTagPoint(this->mIndex, inFrame);
    GE3DPoint thePoint;
    GE3DPoint thePrevious;
    inPart.Apply(theLocal, thePoint);
    for (; theModel != NULL; theModel = theModel->GetParentModel()) {
        thePrevious = thePoint;
        theModel->GetInternalTransform(inFrame).Apply(thePrevious, thePoint);
    }
    outPoint = thePoint;
}

// FUNCTION: EXE 0x4daccd
// Computes the tag point's world position at a frame: through the model chain, then the given transformation
void EETagPoint::GetPoint(unsigned long inFrame, const GETransformation& inTransformation, GE3DPoint& outPoint)
{
    GE3DPoint thePoint = this->mModel->GetTagPoint(this->mIndex, inFrame);
    GE3DPoint thePrevious;
    for (GEModel* theModel = this->mModel; theModel != NULL; theModel = theModel->GetParentModel()) {
        thePrevious = thePoint;
        theModel->GetInternalTransform(inFrame).Apply(thePrevious, thePoint);
    }
    inTransformation.Apply(thePoint, outPoint);
}

// FUNCTION: EXE 0x4dad54
// Computes the tag point's world position at a frame: a part transform, the model chain, then the given transformation
void EETagPoint::GetPoint(unsigned long inFrame, const GETransformation& inTransformation,
                          const GETransformation& inPart, GE3DPoint& outPoint)
{
    GEModel* theModel = this->mModel;
    GE3DPoint theLocal = theModel->GetTagPoint(this->mIndex, inFrame);
    GE3DPoint thePoint;
    GE3DPoint thePrevious;
    inPart.Apply(theLocal, thePoint);
    for (; theModel != NULL; theModel = theModel->GetParentModel()) {
        thePrevious = thePoint;
        theModel->GetInternalTransform(inFrame).Apply(thePrevious, thePoint);
    }
    inTransformation.Apply(thePoint, outPoint);
}

// FUNCTION: EXE 0x4dadfb
// Computes the tag point's transformation at a frame: a part transform, the model chain, then the given transformation
void EETagPoint::GetTransformation(unsigned long inFrame, const GETransformation& inTransformation,
                                   const GETransformation& inPart, GETransformation& outTransformation)
{
    GETransformation theResult(inPart);
    GETransformation thePrevious(false);
    for (GEModel* theModel = this->mModel; theModel != NULL; theModel = theModel->GetParentModel()) {
        thePrevious = theResult;
        theModel->GetInternalTransform(inFrame).Apply(thePrevious, theResult);
    }
    inTransformation.Apply(theResult, outTransformation);
}

// SYNTHETIC: EXE 0x4da984
// SYNTHETIC: EXE 0x4da98e
// SYNTHETIC: EXE 0x4da9a6
// SYNTHETIC: EXE 0x4da9b2
