// gEmptyWideString, the shared empty wide string (L"", zero .bss; was 0x4e60bc, right after gWonClientId).
// A data-only unit so that its readers (GUIStringSingleton's empty GUIStringData, EEWonNet, LanLogicHost,
// RoutingLogic) need no game unit for it: it was EEWonNet.cpp's, and that one name tied all of WONGUI into the
// game's dependency cycle (tools/cpp_deps.py, PORT_LEARNINGS "Cycle cuts").
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
wchar_t gEmptyWideString[2];
