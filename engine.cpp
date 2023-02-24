#include <iostream>
#include <thread>

#include <string>
#include <unordered_map>

#include "io.hpp"
#include "engine.hpp"


void InstrumentOrderBook::tryExecuteBuy(Order& order) {

	std::unique_lock<std::mutex> sell_head_ctrl_lk(sell_head->control_m);
	std::unique_lock<std::mutex> buy_head_ctrl_lk(buy_head->control_m);

	std::unique_lock<std::shared_mutex> sell_head_lk(sell_head->m);
	std::shared_lock<std::shared_mutex> buy_head_lk(buy_head->m);

	sell_head_ctrl_lk.unlock();
	buy_head_ctrl_lk.unlock();

	
	std::unique_lock<std::shared_mutex> lk(sell_head->head->m);
	sell_head_lk.unlock();
	Node* actual_sell_head = sell_head->head;
	Node* curr = actual_sell_head->next;
	if (curr == nullptr) { // no matching resting order
		// add buy order to buy resting
		lk.unlock();
		insertBuy(order, std::move(buy_head_lk));
		// Output::OrderAdded(order.order_id, order.instrument.c_str(), order.price, order.count, false, getCurrentTimestamp());
		return;
		
	} 
	std::unique_lock<std::shared_mutex> lk_2(curr->m);
	int totalCount = order.count;
	while (totalCount > 0 && curr != nullptr) {
		Order& match = *(curr->order);
		if (match.price > order.price) {
			order.count = totalCount;
			lk.unlock();
			lk_2.unlock();
			insertBuy(order, std::move(buy_head_lk));
			// Output::OrderAdded(order.order_id, order.instrument.c_str(), order.price, order.count, false,
			// 	getCurrentTimestamp());
			break;
		}
		// case 1 first sell order count >= my count
		if(!match.isCancelled && !match.isFullyFilled) {
			if (match.count >= totalCount) {
				Output::OrderExecuted(match.order_id, order.order_id, curr->exec_id,
						match.price, totalCount, getCurrentTimestamp());
				curr->exec_id += 1;
				match.count -= totalCount;
				if(match.count == 0) {
					match.isFullyFilled = true;
				}
				totalCount = 0;
			} else if (match.count < totalCount && match.count > 0) {
				Output::OrderExecuted(match.order_id, order.order_id, curr->exec_id,
						match.price, match.count, getCurrentTimestamp());
				curr->exec_id += 1;
				totalCount -= match.count;
				match.count = 0;
				match.isFullyFilled = true;
			}
		}
		

		if (curr->next == nullptr) {
			if (totalCount > 0) {
				order.count = totalCount;
				lk.unlock();
				lk_2.unlock();
				insertBuy(order, std::move(buy_head_lk));
				// Output::OrderAdded(order.order_id, order.instrument.c_str(), order.price, order.count, false,
				// 	getCurrentTimestamp()); 
			}
			break;
		} 
		lk.swap(lk_2);
		lk_2 = std::unique_lock<std::shared_mutex>(curr->next->m);
		curr = curr->next;
	}
	
}

void InstrumentOrderBook::tryExecuteSell(Order& order) {
	std::unique_lock<std::mutex> sell_head_ctrl_lk(sell_head->control_m);
	std::unique_lock<std::mutex> buy_head_ctrl_lk(buy_head->control_m);

	std::shared_lock<std::shared_mutex> sell_head_lk(sell_head->m);
	std::unique_lock<std::shared_mutex> buy_head_lk(buy_head->m);

	sell_head_ctrl_lk.unlock();
	buy_head_ctrl_lk.unlock();

	std::unique_lock<std::shared_mutex> lk(buy_head->head->m);
	buy_head_lk.unlock();
	Node* actual_buy_head = buy_head->head;
	Node* curr = actual_buy_head->next;
	if (curr == nullptr) { // no matching resting order
		// add buy order to buy resting
		lk.unlock();
		insertSell(order, std::move(sell_head_lk));
		// Output::OrderAdded(order.order_id, order.instrument.c_str(), order.price, order.count, true, getCurrentTimestamp());
		return;
	} 
	std::unique_lock<std::shared_mutex> lk_2(curr->m);
	int totalCount = order.count;
	while (totalCount > 0 && curr != nullptr) {
		Order& match = *(curr->order);
		if (match.price < order.price) {
			order.count = totalCount;
			lk.unlock();
			lk_2.unlock();
			insertSell(order, std::move(sell_head_lk));
			// Output::OrderAdded(order.order_id, order.instrument.c_str(), order.price, order.count, true,
			// 	getCurrentTimestamp());
			break;
		}
		// case 1 first sell order count >= my count
		if(!match.isCancelled && !match.isFullyFilled) {
			if (match.count >= totalCount) {
				Output::OrderExecuted(match.order_id, order.order_id, curr->exec_id,
						match.price, totalCount, getCurrentTimestamp());
				curr->exec_id += 1;
				match.count -= totalCount;
				if(match.count == 0) {
					match.isFullyFilled = true;
				}
				totalCount = 0;
				
			} else if (match.count < totalCount && match.count > 0) {
				Output::OrderExecuted(match.order_id, order.order_id, curr->exec_id,
						match.price, match.count, getCurrentTimestamp());
				curr->exec_id += 1;
				totalCount -= match.count;
				match.count = 0;
				match.isFullyFilled = true;
			}
		}
		

		if (curr->next == nullptr) {
			if (totalCount > 0) {
				order.count = totalCount;
				lk.unlock();
				lk_2.unlock();
				insertSell(order, std::move(sell_head_lk)); 
				// Output::OrderAdded(order.order_id, order.instrument.c_str(), order.price, order.count, true,
				// 	getCurrentTimestamp());
			}
			break;
		} 
		lk.swap(lk_2);
		lk_2 = std::unique_lock<std::shared_mutex>(curr->next->m);
		curr = curr->next;
	}
}

void InstrumentOrderBook::tryCancel(int target_order_id) {
	// iterate through sell list to find matching order id
	
	// std::unique_lock<std::shared_mutex> sell_head_lk(sell_head->m);
	// std::unique_lock<std::shared_mutex> buy_head_lk(buy_head->m);
	
	{	
		std::unique_lock<std::mutex> sell_head_ctrl_lk(sell_head->control_m);
		std::unique_lock<std::shared_mutex> sell_head_lk(sell_head->m);
		sell_head_ctrl_lk.unlock();
		std::unique_lock<std::shared_mutex> sell_lk_1(sell_head->head->m);
		
		sell_head_lk.unlock();
		
		Node* curr = sell_head->head->next;
		if (curr != nullptr) { // at least one order excluding dummy node
			std::unique_lock<std::shared_mutex> sell_lk_2(curr->m);
			// check if order matches
			while(curr != nullptr) {
				Order& order = *(curr->order);
				
				if(order.order_id == target_order_id) {
					break;
				}
				sell_lk_1.swap(sell_lk_2);
				if(curr->next != nullptr) {
					sell_lk_2 = std::unique_lock<std::shared_mutex>(curr->next->m);
				}
				curr = curr->next;
			}
			if(curr != nullptr) {
				// if code reaches here, target_order_id is in sell list
				if (!curr->order->isFullyFilled && !curr->order->isCancelled) {
					// execute cancel order
					auto output_time = getCurrentTimestamp();
					curr->order->isCancelled = true;
					Output::OrderDeleted(target_order_id, true, output_time);

				} else {
					// reject cancel order
					auto output_time = getCurrentTimestamp();
					Output::OrderDeleted(target_order_id, false, output_time);
				}
				return;
			}
		}
	}

	// iterate through buy list to find matching order id if cannot find in sell list
	{
		std::unique_lock<std::mutex> buy_head_ctrl_lk(buy_head->control_m);
		std::unique_lock<std::shared_mutex> buy_head_lk(buy_head->m);

		buy_head_ctrl_lk.unlock();
		
		std::unique_lock<std::shared_mutex> buy_lk_1(buy_head->head->m);
		buy_head_lk.unlock();
		Node* curr = buy_head->head->next;
		if (curr != nullptr) { // at least one order excluding dummy node
			std::unique_lock<std::shared_mutex> buy_lk_2(curr->m);
			// check if order matches
			while(curr != nullptr) {
				Order& order = *(curr->order);
				if(order.order_id == target_order_id) {
					break;
				}
				buy_lk_1.swap(buy_lk_2);
				if(curr->next != nullptr) {
					buy_lk_2 = std::unique_lock<std::shared_mutex>(curr->next->m);
				}
				curr = curr->next;
			}
			if(curr != nullptr) {
				// if code reaches here, target_order_id is in buy list
				if (!curr->order->isFullyFilled && !curr->order->isCancelled) {
					// execute cancel order
					auto output_time = getCurrentTimestamp();
					curr->order->isCancelled = true;
					Output::OrderDeleted(target_order_id, true, output_time);

				} else {
					// reject cancel order
					auto output_time = getCurrentTimestamp();
					Output::OrderDeleted(target_order_id, false, output_time);
				}
				return;
			}
		}
	}
	
	
	// Reject cancel order since order_id was not found in either list
	Output::OrderDeleted(target_order_id, false, getCurrentTimestamp());
	
}

void InstrumentOrderBook::insertBuy(Order& order, std::shared_lock<std::shared_mutex> buy_head_lk ) {
	
	// dummy mutex
	// std::shared_mutex mut;
	// Node* curr = nullptr;
	// Node* curr_next = buy_head;
	// std::unique_lock<std::shared_mutex> curr_lk(mut);
	// std::unique_lock<std::shared_mutex> curr_next_lk = std::move(buy_head_lk);

	// Lock buy_head->head(dummy node) since function already have shared owner_ship
	std::unique_lock<std::shared_mutex> curr_lk (buy_head->head->m);
	buy_head_lk.unlock();
	Node* actual_buy_head = buy_head->head;
	Node* curr = actual_buy_head;
	Node* curr_next = actual_buy_head->next;
	if(curr_next == nullptr) {
		Node* node = new Node(nullptr, &order);
		curr->next = node;
		Output::OrderAdded(order.order_id, order.instrument.c_str(), order.price, order.count, false, getCurrentTimestamp());
		return;
	}
	std::unique_lock<std::shared_mutex> curr_next_lk(curr_next->m);
	while(curr_next != nullptr && (curr_next->order->price >= order.price || curr_next == buy_head->head)) {
		curr_lk.swap(curr_next_lk);
		if(curr_next->next != nullptr) {
			curr_next_lk = std::unique_lock<std::shared_mutex>(curr_next->next->m);
		}
		curr = curr_next;
		curr_next = curr_next->next;
	}
	// insert node between curr and curr_next;
	Node* node = new Node(nullptr, &order);
	if(curr == nullptr) {
		// insert at head, should NEVER happen
		throw("err, insertion at head");
	} else if (curr_next == nullptr) {
		// insert at tail
		curr->next = node;
	} else {
		// insert in between
		curr->next = node;
		node->next = curr_next;
	}
	Output::OrderAdded(order.order_id, order.instrument.c_str(), order.price, order.count, false, getCurrentTimestamp());
}

void InstrumentOrderBook::insertSell(Order& order, std::shared_lock<std::shared_mutex> sell_head_lk) {
	// dummy mutex
	// std::shared_mutex mut;
	// Node* curr = nullptr;
	// Node* curr_next = sell_head;
	std::unique_lock<std::shared_mutex> curr_lk (sell_head->head->m);
	sell_head_lk.unlock();
	Node* actual_sell_head = sell_head->head;
	Node* curr = actual_sell_head;
	Node* curr_next = actual_sell_head->next;
	if(curr_next == nullptr) {
		Node* node = new Node(nullptr, &order);
		curr->next = node;
		Output::OrderAdded(order.order_id, order.instrument.c_str(), order.price, order.count, true, getCurrentTimestamp());
		return;
	}

	std::unique_lock<std::shared_mutex> curr_next_lk(curr_next->m);
	// std::unique_lock<std::shared_mutex> curr_next_lk = std::move(sell_head_lk);
	while(curr_next != nullptr && (curr_next->order->price <= order.price ||  curr_next == sell_head->head)) {
		curr_lk.swap(curr_next_lk);
		if(curr_next->next != nullptr) {
			curr_next_lk = std::unique_lock<std::shared_mutex>(curr_next->next->m);
		}
		curr = curr_next;
		curr_next = curr_next->next;
	}
	// insert node between curr and curr_next;
	Node* node = new Node(nullptr, &order);
	if(curr == nullptr) {
		// insert at head
		// should never happen
		throw("insertion at head");
	} else if (curr_next == nullptr) {
		// insert at tail
		curr->next = node;
	} else {
		// insert in between
		curr->next = node;
		node->next = curr_next;
	}
	Output::OrderAdded(order.order_id, order.instrument.c_str(), order.price, order.count, true, getCurrentTimestamp());

};

InstrumentOrderBook& OrderMap::getInstrument(std::string instrument) {
	std::unique_lock lock{mut};
	if(instrument_map.contains(instrument)) {
		return instrument_map.at(instrument);
	} else {
		InstrumentOrderBook newInstrument(instrument);
		instrument_map.insert(std::make_pair(instrument, newInstrument));
		return instrument_map.at(instrument);
	}
}

std::string OrderMap::getOrderInstrument(int orderId) {
	std::unique_lock lock{mut};
	if(order_instrument_map.contains(orderId)) {
		return order_instrument_map.at(orderId);
	} else {
		return "not found";
	}
}

void OrderMap::addOrderInstrumentRecord(int orderId, std::string instr) {
	std::unique_lock lock{mut};
	order_instrument_map.insert(std::make_pair(orderId, instr));
}

void Engine::accept(ClientConnection connection)
{
	auto thread = std::thread(&Engine::connection_thread, this, std::move(connection));
	thread.detach();
}

void Engine::connection_thread(ClientConnection connection)
{
	while(true)
	{
		ClientCommand input {};
		switch(connection.readInput(input))
		{
			case ReadResult::Error: SyncCerr {} << "Error reading input" << std::endl;
			case ReadResult::EndOfFile: return;
			case ReadResult::Success: break;
		}

		// Functions for printing output actions in the prescribed format are
		// provided in the Output class:
		switch(input.type)
		{
			case input_cancel: {
				// SyncCerr {} << "Got cancel: ID: " << input.order_id << std::endl;
				
				// Remember to take timestamp at the appropriate time, or compute
				// an appropriate timestamp!
				// auto output_time = getCurrentTimestamp();
				// Output::OrderDeleted(input.order_id, true, output_time);

				std::string instr = order_map.getOrderInstrument(input.order_id); 
				if(instr.compare("not found") == 0) {
					Output::OrderDeleted(input.order_id, false, getCurrentTimestamp());
				} else {
					order_map.getInstrument(instr).tryCancel(input.order_id);
				}
				break;
			}

			case input_buy: {
				// SyncCerr {} << "Got buy: ID: " << input.order_id << std::endl;
				std::string instr(input.instrument); 
				order_map.addOrderInstrumentRecord(input.order_id, instr);
				Order* newOrder = new Order(input.order_id, instr, input.price, input.count, "buy");
				order_map.getInstrument(instr).tryExecuteBuy(*newOrder);
				break;
			}

			case input_sell: {
				// SyncCerr {} << "Got sell: ID: " << input.order_id << std::endl;
				std::string instr(input.instrument); 
				order_map.addOrderInstrumentRecord(input.order_id, instr);
				Order* newOrder = new Order(input.order_id, instr, input.price, input.count, "sell");
				order_map.getInstrument(instr).tryExecuteSell(*newOrder);
				break;
			}
			default: {
				SyncCerr {}
				    << "Got order: " << static_cast<char>(input.type) << " " << input.instrument << " x " << input.count << " @ "
				    << input.price << " ID: " << input.order_id << std::endl;

				// Remember to take timestamp at the appropriate time, or compute
				// an appropriate timestamp!
				auto output_time = getCurrentTimestamp();

				Output::OrderAdded(input.order_id, input.instrument, input.price, input.count, input.type == input_sell,
				    output_time);
				break;
			}
		}

		// Additionally:

		// Remember to take timestamp at the appropriate time, or compute
		// an appropriate timestamp!
		//intmax_t output_time = getCurrentTimestamp();

		// Check the parameter names in `io.hpp`.
		//Output::OrderExecuted(123, 124, 1, 2000, 10, output_time);
		// intmax_t output_time = getCurrentTimestamp();

		// // Check the parameter names in `io.hpp`.
		// Output::OrderExecuted(123, 124, 1, 2000, 10, output_time);
	}
}
