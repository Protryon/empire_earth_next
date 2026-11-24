// stand-in (tests/net/ee_stand/ee_world.h: the server's and client's golden): the real header, with the Rust calls of
// the inlines the golden traces renamed to the driver's world (ee_session_test.cpp defines them): TMGetTime,
// UResourceFile::GetString, EECPEngineChoiceCount (inline over Rust since 456ed11cb and the AI manifest)
#ifndef NET_STAND_LOWLEVELENGINECORE_H
#define NET_STAND_LOWLEVELENGINECORE_H
#define ee_time_get ee_stand_time_get
#define ee_util_resource_string_get ee_stand_resource_string_get
#define ee_cp_engine_choice_count ee_stand_engine_choice_count
#include "../../../../include/lle/LowLevelEngineCore.h"
#endif
