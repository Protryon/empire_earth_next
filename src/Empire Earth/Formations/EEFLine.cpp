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
#include "rust/ee_formations.h"
#include "ee/EEFormationRust.h"


// FUNCTION: EXE 0x4dd8b2
// Creates a line formation of inCount columns, precomputing the column offsets alternating out from the
// centre; inDepth shifts alternate rows and an empty row follows every inRows rows
EEFLine::EEFLine(long inType, float inSpacing, float inDepth, long inCount, long inRows)
    : EEFormation(inType), mSpacing(inSpacing), mDepth(inDepth), mRows(inRows + 1)
{
    this->mCount = inCount;
    this->mOffsets = new float[inCount];
    ee_formation_line_offsets(inSpacing, inCount, this->mOffsets);   // the geometry is Rust (ee_formations)
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
    outSlots.clear();
    EEFormationRadii theRadii(this, inUnits);
    EeFormationLine theLine = {this->mCount, this->mRows, this->mSpacing, this->mDepth};
    unsigned int theCount = ee_formation_line_place(&theRadii.mFrame, &theLine, this->mOffsets, theRadii.Radii(),
                                                    theRadii.Count(), theRadii.Slots());
    theRadii.Append(theCount, inUnits, outSlots);
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
