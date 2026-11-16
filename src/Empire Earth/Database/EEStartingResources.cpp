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
#include "ee/EEDbRecords.h"

std::vector<EEStartingResources*> EEStartingResources::sTable;

// Creates a starting-resources preset with every amount zero
EEStartingResources::EEStartingResources() : EeDataStartingResourcesFields()
{
}

// Destroys the starting-resources preset (nothing to release)
EEStartingResources::~EEStartingResources()
{
}

// Loads the starting-resource presets (food, wood, stone, gold, iron, name) from db\dbStartingResources.dat (the
// record and its copy: ee_data tables.rs); rows with a negative index are skipped. Returns the last read status
long EEStartingResources::Load(FSFileSpec* inFile)
{
    return EEDbReadRecords(inFile, ee_data_starting_resources_record_size(), [](unsigned long) { return true; },
        [](unsigned long, const unsigned char* inRecord) {
            EeDataStartingResourcesFields theFields;
            if (ee_data_starting_resources_from_record(inRecord, &theFields)) {
                EEStartingResources* theResources = new EEStartingResources;
                *static_cast<EeDataStartingResourcesFields*>(theResources) = theFields;
                EEStartingResources::sTable.push_back(theResources);
            }
        });
}

// Destroys every starting-resources preset and empties the table
void EEStartingResources::Unload()
{
    for (unsigned long i = 0; i < EEStartingResources::sTable.size(); i++) {
        delete EEStartingResources::sTable[i];
        EEStartingResources::sTable[i] = NULL;
    }
    EEStartingResources::sTable.clear();
}

// The tag-point helpers (EETagPointList / EETagPoint, ee/EEGraphicObject.h), defined in this unit's range: included
// after the table's code so the init chain above keeps its numbering
#include <map>
#include "lle/GEColor.h"
#include "lle/GEModel.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicObject.h"

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
