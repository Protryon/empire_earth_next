// QueueSocket's members only the drivers use, cut from ee/wonapi/WONRouting.h (R6 WON round 38): included in its
// class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
void StartCloseTimer(long inTimeout) { ee_won_queuesocket_start_close_timer(this->mQueue, inTimeout); }
void CancelCloseTimer() { ee_won_queuesocket_cancel_close_timer(this->mQueue); }
void SetQueueCompletion(CompletionBase<AsyncOpPtr>* inCompletion)
{ ee_won_queuesocket_set_queue_completion(this->mQueue, inCompletion); }
void QueueOpFast(SocketOp* inOp, DWORD inTimeout=0xffffffff)
{ ee_won_queuesocket_queue_op_fast(this->mQueue, static_cast<AsyncOp*>(inOp), static_cast<uint32_t>(inTimeout)); }
