// SmartRoutingConnection's members only the drivers use, cut from ee/wonapi/WONRouting.h (R6 WON round 38): included in its
// class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
// (each call below holds the socket's lock around the model's call, as the original's AutoCrit: ee_won_smart_locked)
void StartClientItr() { ee_won_smart_locked(this->GetDataLock(), this->mModel, EeWonSmartLocked::Start, 0, 0, 0, 0); }
// 2c3970: a chat command word (case-insensitive)
void AddCommandString(const std::wstring& inText, int inCommand) // an InputCommand
{ ee_won_smart_locked(this->GetDataLock(), this->mModel, EeWonSmartLocked::AddCommand, (intptr_t)inText.data(), (intptr_t)inText.length(), inCommand, 0); }
void ClearCommandStrings() { ee_won_smart_locked(this->GetDataLock(), this->mModel, EeWonSmartLocked::ClearCommands, 0, 0, 0, 0); }
void SetAllowPartialNameMatch(bool inAllow) { this->SetSmart(EeWonSmartNumber::PartialNames, inAllow); } //2c40a0
void SetServerWideCommands(bool inOn) { this->SetSmart(EeWonSmartNumber::ServerWide, inOn); } //2c40f0
void SetDoColonEmote(bool inOn) { this->SetSmart(EeWonSmartNumber::ColonEmote, inOn); }
