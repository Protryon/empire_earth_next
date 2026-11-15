// EESEInstructions.h (RECONSTRUCTION; file name inferred). The scenario editor's instructions tab
// (RTTI EESEInstructions > EEUIFSEMainGroup; vtable 0x442de4; sizeof 0x4c: EEUIFScenarioEditor::CreateTabGroup news
// 0x4c bytes for group 6). Scenario Editor/EESEInstructions.cpp (__FILE__ 0x4c2e9c; ctor 0x1707ff, ??_G 0x171b9f, dtor
// 0x171bf5). Layout from the ctor (each member's `new` and control id) and the dtor (deletes +0x04..+0x48 in that order
// except +0x34 before +0x30). Member names are provisional. Requires ee/EEUIFSEMainGroup.h.
#ifndef EE_EESEINSTRUCTIONS_H
#define EE_EESEINSTRUCTIONS_H
#include "ee/EEScenarioEditorIDs.h"

class EEUIFileComboBox;
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

    UITextLabel* mMapLabel;                 // +0x04 control 0x44c (string 0x503f)
    UITextLabel* mMovieLabel;               // +0x08 control 0x44e (string 0x5040)
    UITextLabel* mSoundLabel;               // +0x0c control 0x450 (string 0x5041)
    EEUIFileComboBox* mMapComboBox;         // +0x10 control 0x44d (the scenario directory's *.jpg)
    EEUIFileComboBox* mMovieComboBox;       // +0x14 control 0x44f (the movies directory's *.bik)
    EEUIFileComboBox* mSoundComboBox;       // +0x18 control 0x451 (the sounds directory's *.mp3)
    UITextLabel* mInstructionsLabel;        // +0x1c control 0x452 (string 0x5045)
    UIEditBox* mInstructionsEditBox;        // +0x20 control 0x453 (multi-line, EESEEFindText)
    UITextLabel* mAftermathLabel;           // +0x24 control 0x454 (string 0x5046; hidden)
    UIEditBox* mAftermathEditBox;           // +0x28 control 0x455 (hidden)
    UITextLabel* mHintsLabel;               // +0x2c control 0x456 (string 0x5047)
    UIEditBox* mHintsEditBox;               // +0x30 control 0x457
    UITextLabel* mHistoryLabel;             // +0x34 control 0x456 (string 0x504b)
    UIEditBox* mHistoryEditBox;             // +0x38 control 0x457
    UIButton* mImportButton;                // +0x3c control 0x458 (string 0x7707; EESEIEImport)
    UIButton* mExportButton;                // +0x40 control 0x459 (string 0x7708; EESEIEExport)
    UITextLabel* mLocalizationLabel;        // +0x44 control 0x45a (string 0x7709)
    UIEditBox* mLocalizationEditBox;        // +0x48 control 0x45b (0x100 characters)
};

#endif
