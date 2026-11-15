// C:\EEX\Empire Earth\Scenario Editor\EEUIFSEESElevation.cpp (file name inferred: no __FILE__). The elevation tab's
// elevation slider: 0x1a1397..0x1a147f. Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GEBitmap.h"
#include "lle/GEVertexArray.h"
#include "lle/GERectangle.h"
#include "lle/GERhombus.h"
#include "lle/ISEntity.h"
#include "lle/TSCriticalSection.h"
#include "lle/UILayoutUtility.h"
#include "lle/UIEntity.h"
#include "lle/UIControl.h"
#include "lle/UISlider.h"
#include "lle/UIForm.h"
#include "lle/UITextLabel.h"
#include "ee/EEUIFSEESElevation.h"
#include "ee/EEScenarioEditorIDs.h"

// FUNCTION: EXE 0x5a1397
// Creates the elevation tab's vertical slider, 20 pixels wide and the given height
EEUIFSEESElevation::EEUIFSEESElevation(unsigned long inId, const GE2DPoint& inPosition, unsigned long inHeight,
                                       UIForm* inForm, unsigned long inIndex)
    : UISlider(inId, GERectangle(inPosition, 20.0f, (float)inHeight), 1, inForm, inIndex, 2)
{
}

// SYNTHETIC: EXE 0x5a13f5
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x5a1411
// Destroys the elevation slider
EEUIFSEESElevation::~EEUIFSEESElevation()
{
}

// FUNCTION: EXE 0x5a1437
// Moves the slider, first showing the new elevation level (max - value - 24) in the value label when it changes
void EEUIFSEESElevation::ChangeRange(unsigned long inValue)
{
    if (this->GetCurrentRangeValue() != inValue)
    {
        long theMax = this->GetMaxRange();
        UITextLabel* theLabel = static_cast<UITextLabel*>(this->GetForm()->GetControlById(kSEElevationStrengthLabel));
        theLabel->SetText((char)(theMax - inValue - 24));
    }
    UISlider::ChangeRange(inValue);
}
