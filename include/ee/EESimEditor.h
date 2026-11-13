// TEST ONLY: the scripted scenario editor of the headless simulation (EESimEditor.cpp; EE-SIM.exe --editor-script).
// Port addition.
#ifndef EE_EESIMEDITOR_H
#define EE_EESIMEDITOR_H

#include <stdio.h>
#include <string>

// Runs the script inScript in the open scenario editor (the editor form on top, its map made), printing each step
// and its digests to inOut. -> 0, or nonzero with outError (an unknown command, a step that did not finish).
int EESimEditorRun(const std::string& inScript, FILE* inOut, std::string& outError);

#endif
