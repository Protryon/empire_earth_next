// AsyncOp's members only the drivers use, cut from ee/wonapi/WONRouting.h (R6 WON round 38): included in its
// class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
CompletionBase<SmartPtr<AsyncOp> >* GetCompletion() const
{ return static_cast<CompletionBase<SmartPtr<AsyncOp> >*>(ee_won_asyncop_completion(this->mOp)); }
bool IsCompletionPending() const { return ee_won_asyncop_completion_pending(this->mOp); }
bool IsRunAsyncImmediately() const { return ee_won_asyncop_run_async_immediately(this->mOp); }
// unless the op runs (29f1b0)
void SetTimeout(unsigned long inTimeout) { ee_won_asyncop_set_timeout(this->mOp,static_cast<uint32_t>(inTimeout)); }
bool RunBlock(unsigned long inTimeout) { return this->Run(2,inTimeout)==0; } // 29f6e0: whether it succeeded
void QueueInOpContainer() { ee_won_asyncop_queue_in_container(this->mOp); } // 29f200, or WONAPICore's queue
DWORD RealTimeLeft() { return ee_won_asyncop_real_time_left(this->mOp); }
bool IsAsync() const { return this->GetMode()==1; }
//: 0x291983 (an inline COMDAT of EELobbyFileTransfer.cpp): finished without error
bool Succeeded() { return this->GetStatus()==0 && !this->IsKilled(); }
