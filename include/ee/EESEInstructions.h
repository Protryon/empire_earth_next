// EESEInstructions.h (RECONSTRUCTION; file name inferred). The scenario editor's instructions tab
// (RTTI EESEInstructions > EEUIFSEMainGroup; vtable 0x442de4; sizeof 0x4c: EEUIFScenarioEditor::CreateTabGroup news
// 0x4c bytes for group 6). Scenario Editor/EESEInstructions.cpp (__FILE__ 0x4c2e9c; ctor 0x1707ff, ??_G 0x171b9f, dtor
// 0x171bf5). Layout from the ctor (each member's `new` and control id) and the dtor (deletes +0x04..+0x48 in that order
// except +0x34 before +0x30). Member names are provisional. The tab is whole in Rust (ee_screens
// editor_instructions.rs): the class holds its object. Requires ee/EEUIFSEMainGroup.h.
#ifndef EE_EESEINSTRUCTIONS_H
#define EE_EESEINSTRUCTIONS_H
#include "ee/EEScenarioEditorIDs.h"

struct EeScreensSeInstructions;
class GEFont;
class UIButton;
class UIEditBox;
class UIForm;
class UITextLabel;

class EESEInstructions : public EEUIFSEMainGroup {
public:
    EESEInstructions(unsigned long& ioIndex, UIForm* inForm, GEFont* inFont);   // 0x1707ff
    virtual ~EESEInstructions();                        // +0x00 0x171bf5 (??_G 0x171b9f)
    virtual void Leave() { this->Store(); }             // +0x08 (ICF-folded `jmp [vptr+0x1c]` 0x66295)
    virtual void Load();                                // +0x14 0x171ce3
    virtual void Enter();                               // +0x18 0x171f62
    virtual void Store();                               // +0x1c 0x171f8b
    virtual void MouseMove(const GE2DPoint& inLocation);   // +0x24 (ICF-folded empty `ret 4`)

    // the tab's host (ee_screenscpp_sein): a member for EESMMediaStrings' friendship (Load's and Store's transfers)
    static int32_t Host(uint32_t inOp, void* const* p, int32_t* i, float* f);

    EeScreensSeInstructions* mRust;         // the tab (editor_instructions.rs): its controls
};

#endif
