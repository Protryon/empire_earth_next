// New (port): the game messages' Execute decisions (docs/RUST_PLAN.md R5b.4 leaf 4; ee_netmsg execute.rs). Each is a
// function of plain values that EEMessage.cpp's Execute bodies (and EEGetTradePrice) ask; the C++ keeps the objects,
// the locks and the world calls. The capture's NMDC record (ee/EEMessageCapture.h) records every call's inputs and
// outputs, in game and over a sweep.
#ifndef EE_EEMESSAGEEXECUTE_H
#define EE_EEMESSAGEEXECUTE_H

#include <stdint.h>

// one step of EEMExchangeResources: apply it or not, the resource's amount (cost) and the gold's (amount)
struct EeNetMsgExchangeStep {
    uint8_t apply;
    int64_t cost;
    int64_t amount;
};

extern "C" {
// ExecuteSelectionMessage: whether the client marks the world as executing the local selection (the sender is local)
uint8_t ee_netmsg_selection_marks_local(uint8_t is_server, uint8_t is_local);
// ExecuteSelectionMessage, after Execute: whether the local sender's selected group copies the active group
uint8_t ee_netmsg_selection_copies(uint8_t is_local, uint8_t message_id, uint8_t current_id);
// ExecuteMessage: whether the command runs (else the sender's active group is reset to the selection)
uint8_t ee_netmsg_command_runs(uint8_t is_server, uint8_t message_type, uint8_t message_id, uint8_t last_id);
// the selection toggles' passes: twice while a selection is being replayed
uint32_t ee_netmsg_selection_passes(uint8_t executing_selection);
// EEMAddRemoveResources: 0 nothing, 1 add, 2 take
uint8_t ee_netmsg_add_remove(uint8_t add, uint32_t amount, int64_t have);
// EEGetTradePrice: a resource's market price (1..1000) from its rate and the player's trade balance in it
int64_t ee_netmsg_trade_price(float rate, int64_t trade, uint8_t buy);
// one EEMExchangeResources step from the price and what the payer has (the resource to sell, or the gold)
EeNetMsgExchangeStep ee_netmsg_exchange_step(uint8_t is_sell, int64_t price, int64_t have);
// EEMPayTribute: the resources (2..6) with an amount, in the tribute's order (text 0) or the chat line's (text 1);
// amounts by resource index, 7 of them; the count
uint32_t ee_netmsg_tribute_order(const uint32_t* amounts, uint8_t text, uint8_t* out_resources);
// EEMPayTribute: whose chat line it is (0 none, 1 the giver's, 2 the receiver's)
uint8_t ee_netmsg_tribute_viewer(uint8_t local_is_from, uint8_t local_is_to);
// EEMDisband: whether a unit is disbanded and the refund's fraction of its cost
uint8_t ee_netmsg_disband_refund(int64_t hit_points, int64_t max_hit_points, uint8_t can_disband, float* out_fraction);
}

#endif
