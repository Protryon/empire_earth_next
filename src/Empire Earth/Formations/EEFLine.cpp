// C:\EEX\Empire Earth\Formations\EEFLine.cpp (file and folder names inferred: no __FILE__). The line formation: 0xdd8b2..0xddc20 (ctor, ??_G,
// dtor, Place, its vector<EEFormationSlot> COMDATs).
// Unit flags: the exe's module set.
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
#include "ee/EEAreaUtilities.h"
#include "ee/EEFormation.h"
#include "ee/EEFLine.h"
#include "ee/EESimCapture.h"


// NEAR: register allocation only (inCount in ebx and the new array in edi in the original)
// FUNCTION: EXE 0x4dd8b2
// Creates a line formation of inCount columns, precomputing the column offsets alternating out from the
// centre; inDepth shifts alternate rows and an empty row follows every inRows rows
EEFLine::EEFLine(long inType, float inSpacing, float inDepth, long inCount, long inRows)
    : EEFormation(inType), mSpacing(inSpacing), mDepth(inDepth), mRows(inRows + 1)
{
    this->mCount = inCount;
    this->mOffsets = new float[inCount];
    float theOffset;
    long theLeft;
    long theRight;
    long theIndex;
    if (inCount % 2 == 0) {
        theOffset = inSpacing * -0.5f;
        theLeft = inCount / 2 - 1;
        theRight = inCount / 2;
        theIndex = 0;
    } else {
        theOffset = 0.0f;
        this->mOffsets[0] = 0.0f;
        theLeft = inCount / 2 - 1;
        theRight = inCount / 2 + 1;
        theIndex = 1;
    }
    while (theLeft >= 0 && theRight < inCount) {
        theOffset += inSpacing;
        if (theLeft >= 0) {
            this->mOffsets[theIndex] = -theOffset;
            theLeft--;
            theIndex++;
        }
        this->mOffsets[theIndex] = theOffset;
        theRight++;
        theIndex++;
    }
}

// SYNTHETIC: EXE 0x4dd966
// (the scalar deleting destructor)

// FUNCTION: EXE 0x4dd982
// Destroys the line formation, freeing its column offsets
EEFLine::~EEFLine()
{
    delete[] this->mOffsets;
}

// FUNCTION: EXE 0x4dd9a3
// Places the units row by row behind the formation centre, spaced by their largest collision radius,
// all facing the formation heading
void EEFLine::Place(std::vector<EEComplexUnit*>& inUnits, std::vector<EEFormationSlot>& outSlots)
{
    GEVector theOffset;
    GE3DPoint theCenter(this->mCenter);
    GEVector theTransformedOffset;
    EEFormationSlot theSlot;
    long theRow = 1;
    long theUnitCount = inUnits.size();
    outSlots.clear();
    float theRadius = 0.0f;
    for (long i = 0; i < theUnitCount; i++)
        theRadius = max(inUnits[i]->mUnitClass->mOuterCollisionSphere, theRadius);
    long theDepth = 0;
    for (long j = 0; j < theUnitCount; j++) {
        EEComplexUnit* theUnit = inUnits[j];
        long theColumn = j % this->mCount;
        theOffset.SetX(theRadius * this->mOffsets[theColumn]);
        if (theRow % 2 != 1)
            theOffset.SetX(theOffset.GetX() + this->mDepth);
        theOffset.SetY(-((float)theDepth * this->mSpacing * theRadius));
        theOffset.SetZ(0.0f);
        this->mOrientation.Apply(theOffset, theTransformedOffset);
        theSlot.mPosition = GE3DPoint(theCenter.GetX() + theTransformedOffset.GetX(),
                                    theCenter.GetY() + theTransformedOffset.GetY(),
                                    theCenter.GetZ() + theTransformedOffset.GetZ());
        theSlot.mUnit = theUnit;
        theSlot.mHeading = this->mHeading;
        outSlots.push_back(theSlot);
        if (theColumn != this->mCount - 1)
            continue;
        theRow++;
        theDepth++;
        if (this->mRows != 0 && theRow % this->mRows == 0) {
            theRow++;
            theDepth++;
        }
    }
    if (EESimCaptureRecord::Active()) {
        EESimCaptureRecord theRecord(0x4e4c4d46);   // "FMLN"
        EEFormationCaptureBegin(theRecord, this);
        theRecord.I32(this->mCount);
        theRecord.I32(this->mRows);
        theRecord.F32(this->mSpacing);
        theRecord.F32(this->mDepth);
        theRecord.F32s(this->mOffsets, this->mCount);
        EEFormationCaptureEnd(theRecord, inUnits, outSlots, 0);
    }
}

// TEMPLATE: EXE 0x4ddb77
// (compiler-generated; name in analysis/exe/names.txt)

// Template COMDATs of this unit (vector<EEFormationSlot>; names in analysis/exe/names.txt):
// TEMPLATE: EXE 0x4ddbb2
// TEMPLATE: EXE 0x4ddbfc
