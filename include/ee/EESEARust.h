// The scenario editor's actions whole in Rust (ee_screens editor_whole.rs; port addition, no original address): the
// save, test, exit and quick save actions, the tech lists' move, the rotation, the trigger area's rectangle, the
// blank and random maps, the wall and gate placements (the original's EESEASaveScenario, EESEATestScenario,
// EESEAExitScenario, EESEAQuickSaveScenario, EESEATechTree, EESEARotateUnits, EESEAPaintArea, EESEACreateBlankMap,
// EESEACreateRandomMap, EESEAPlaceWall, EESEAPlaceGate). One EEAction holds the Rust action; the kinds that posted
// themselves when made post when made. Requires ee/EEAction.h.
#ifndef EE_EESEARUST_H
#define EE_EESEARUST_H

#include <stdint.h>

// (rust/ee_screens.h declares these too; here so the actions' users need no Rust header)
struct EeScreensSewAction;
extern "C" EeScreensSewAction* ee_screens_sew_new(void* owner, uint32_t kind, void* object, int32_t a, int32_t b,
                                                  int32_t c, int32_t d);
extern "C" void ee_screens_sew_begin(EeScreensSewAction* action);
extern "C" void ee_screens_sew_run(EeScreensSewAction* action);
extern "C" void ee_screens_sew_free(EeScreensSewAction* action);
extern "C" void ee_screens_sew_rotate(EeScreensSewAction* action, float yaw);
extern "C" void ee_screens_sew_area_corner(EeScreensSewAction* action, int32_t x, int32_t y);
extern "C" void ee_screens_sew_area(const EeScreensSewAction* action, int32_t* out);
extern "C" void ee_screens_sew_wall(EeScreensSewAction* action, float x0, float y0, float x1, float y1, int32_t player,
                                    int32_t technology);
extern "C" uint8_t ee_screens_sew_gate(EeScreensSewAction* action, int32_t x, int32_t y, int32_t player,
                                       int32_t technology, int32_t direction);

// the kinds (EE_SCREENS_SEW_*)
enum {
    kSEASave = 1,
    kSEATest = 2,
    kSEAExit = 3,
    kSEAQuickSave = 4,      // object: the editor's EEAQuickSaveLoad
    kSEATechTree = 5,       // first, last, player, the left list
    kSEARotate = 6,
    kSEAPaintArea = 7,      // the first corner
    kSEABlankMap = 8,       // width, height, terrain
    kSEARandomMap = 9,      // size, style
    kSEAPlaceWall = 10,
    kSEAPlaceGate = 11,
};

class EESEARust : public EEAction {
public:
    EESEARust(uint32_t inKind, void* inObject = 0, int32_t inA = 0, int32_t inB = 0, int32_t inC = 0, int32_t inD = 0)
        : EEAction(false)
    {
        this->mAction = ee_screens_sew_new(this, inKind, inObject, inA, inB, inC, inD);
        ee_screens_sew_begin(this->mAction);
    }
    virtual ~EESEARust() { ee_screens_sew_free(this->mAction); }
    virtual void Virtual4() { ee_screens_sew_run(this->mAction); }
    virtual void Virtual8() {}
    EeScreensSewAction* mAction;
};

#endif
