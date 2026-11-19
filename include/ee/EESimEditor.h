// TEST ONLY: the scripted scenario editor of the headless simulation (EESimEditor.cpp; EE-SIM.exe --editor-script).
// Port addition.
#ifndef EE_EESIMEDITOR_H
#define EE_EESIMEDITOR_H

#include <string>
#include <vector>

// One script line's command (its words) run as a step in the open scenario editor -> 0, 1 not a command, 2 not
// finished within a minute; its output lines into outLines (the script's runner: ee_game sim_driver editor.rs)
int EESimEditorStep(const std::vector<std::string>& inWords, std::vector<std::string>& outLines);

#endif
