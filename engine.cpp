#include <iostream>
#include <thread>

#include <string>
#include <unordered_map>

#include "io.hpp"
#include "engine.hpp"


void InstrumentOrderBook::tryExecuteBuy(Order& order) {
	std::unique_lock<std::mutex> lk(sell_head->m);
	Node* curr = sell_head->next;
	if (curr == nullptr) { // no matching resting order
		// add buy order to buy resting
		insertBuy(order);
		Output::OrderAdded(order.order_id, order.instrument.c_str(), order.price, order.count, false, getCurrentTimestamp());
		return;
	} 
	std::unique_lock<std::mutex> lk_2(curr->m);
	int totalCount = order.count;
	while (totalCount > 0 && curr != nullptr) {
		Order& match = *(curr->order);
		if (match.price > order.price) {
			order.count = totalCount;
			insertBuy(order);
			Output::OrderAdded(order.order_id, order.instrument.c_str(), order.price, order.count, false,
				getCurrentTimestamp());
			break;
		}
		// case 1 first sell order count >= my count
		if (match.count >= totalCount) {
			Output::OrderExecuted(match.order_id, order.order_id, curr->exec_id,
					match.price, totalCount, getCurrentTimestamp());
			curr->exec_id += 1;
			match.count -= totalCount;
			totalCount = 0;
		} else if (match.count < totalCount && match.count > 0) {
			Output::OrderExecuted(match.order_id, order.order_id, curr->exec_id,
					match.price, match.count, getCurrentTimestamp());
			curr->exec_id += 1;
			totalCount -= match.count;
			match.count = 0;
			match.isFullyFilled = true;
		}

		if (curr->next == nullptr) {
			if (totalCount > 0) {
				order.count = totalCount;
				insertBuy(order);
				Output::OrderAdded(order.order_id, order.instrument.c_str(), order.price, order.count, false,
					getCurrentTimestamp()); 
			}
			break;
		} 
		lk.swap(lk_2);
		lk_2 = std::unique_lock<std::mutex>(curr->next->m);
		curr = curr->next;
	}
	
}

void InstrumentOrderBook::tryExecuteSell(Order& order) {
	std::unique_lock<std::mutex> lk(buy_head->m);
	Node* curr = buy_head->next;
	if (curr == nullptr) { // no matching resting order
		// add buy order to buy resting
		insertSell(order);
		Output::OrderAdded(order.order_id, order.instrument.c_str(), order.price, order.count, true, getCurrentTimestamp());
		return;
	} 
	std::unique_lock<std::mutex> lk_2(curr->m);
	int totalCount = order.count;
	while (totalCount > 0 && curr != nullptr) {
		Order& match = *(curr->order);
		if (match.price < order.price) {
			order.count = totalCount;
			insertSell(order);
			Output::OrderAdded(order.order_id, order.instrument.c_str(), order.price, order.count, true,
				getCurrentTimestamp());
			break;
		}
		// case 1 first sell order count >= my count
		if (match.count >= totalCount) {
			Output::OrderExecuted(match.order_id, order.order_id, curr->exec_id,
					match.price, totalCount, getCurrentTimestamp());
			curr->exec_id += 1;
			match.count -= totalCount;
			totalCount = 0;
		} else if (match.count < totalCount && match.count > 0) {
			Output::OrderExecuted(match.order_id, order.order_id, curr->exec_id,
					match.price, match.count, getCurrentTimestamp());
			curr->exec_id += 1;
			totalCount -= match.count;
			match.count = 0;
			match.isFullyFilled = true;
		}

		if (curr->next == nullptr) {
			if (totalCount > 0) {
				order.count = totalCount;
				insertSell(order); 
				Output::OrderAdded(order.order_id, order.instrument.c_str(), order.price, order.count, true,
					getCurrentTimestamp());
			}
			break;
		} 
		lk.swap(lk_2);
		lk_2 = std::unique_lock<std::mutex>(curr->next->m);
		curr = curr->next;
	}
}

void InstrumentOrderBook::tryCancel(int target_order_id) {
	// iterate through sell list to find matching order id
	std::thread::id this_id = std::this_thread::get_id();
	{
		// block to unlock unique_lock when it goes out of scope
		std::unique_lock<std::mutex> lk(sell_head->m);
		Node* curr = sell_head->next;
		if (curr != nullptr) { // at least one order excluding dummy node
			std::unique_lock<std::mutex> lk_2(curr->m);
			// check if order matches
			while(curr != nullptr) {
				Order& order = *(curr->order);
				if(order.order_id == target_order_id) {
					break;
				}
				lk.swap(lk_2);
				lk_2 = std::unique_lock<std::mutex>(curr->next->m);
				curr = curr->next;
			}
			if(curr != nullptr) {
				// if code reaches here, target_order_id is in sell list
				if (!curr->order->isFullyFilled && !curr->order->isCancelled && this_id == curr->order->creator_thread_id) {
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
		// block to unlock unique_lock when it goes out of scope
		std::unique_lock<std::mutex> lk(sell_head->m);
		Node* curr = buy_head->next;
		if (curr != nullptr) { // at least one order excluding dummy node
			std::unique_lock<std::mutex> lk_2(curr->m);
			// check if order matches
			while(curr != nullptr) {
				Order& order = *(curr->order);
				if(order.order_id == target_order_id) {
					break;
				}
				lk.swap(lk_2);
				lk_2 = std::unique_lock<std::mutex>(curr->next->m);
				curr = curr->next;
			}
			if(curr != nullptr) {
				// if code reaches here, target_order_id is in buy list
				if (!curr->order->isFullyFilled && !curr->order->isCancelled  && this_id == curr->order->creator_thread_id) {
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
	auto output_time = getCurrentTimestamp();
	Output::OrderDeleted(target_order_id, false, output_time);
	
}

void InstrumentOrderBook::insertBuy(Order& order) {
	// dummy mutex
	std::mutex mut;
	Node* curr = nullptr;
	Node* curr_next = buy_head;
	std::unique_lock<std::mutex> curr_lk(mut);
	std::unique_lock<std::mutex> curr_next_lk (curr_next->m);
	while(curr_next != nullptr && (curr_next->order->price >= order.price ||  curr_next == buy_head)) {
		curr_lk.swap(curr_next_lk);
		curr_next_lk = std::unique_lock<std::mutex>(curr_next->next->m);
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
}

void InstrumentOrderBook::insertSell(Order& order) {
	// dummy mutex
	std::mutex mut;
	Node* curr = nullptr;
	Node* curr_next = sell_head;
	std::unique_lock<std::mutex> curr_lk(mut);
	std::unique_lock<std::mutex> curr_next_lk (curr_next->m);
	while(curr_next != nullptr && (curr_next->order->price <= order.price ||  curr_next == sell_head)) {
		curr_lk.swap(curr_next_lk);
		curr_next_lk = std::unique_lock<std::mutex>(curr_next->next->m);
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
				SyncCerr {} << "Got cancel: ID: " << input.order_id << std::endl;
				
				// Remember to take timestamp at the appropriate time, or compute
				// an appropriate timestamp!
				// auto output_time = getCurrentTimestamp();
				// Output::OrderDeleted(input.order_id, true, output_time);
				std::string instr(input.instrument); 
				order_map.getInstrument(instr).tryCancel(input.order_id);
				break;
			}

			case input_buy: {
				std::string instr(input.instrument); 
				std::thread::id currThreadId = std::this_thread::get_id();
				Order* newOrder = new Order(input.order_id, instr, input.price, input.count, "buy", currThreadId);
				order_map.getInstrument(instr).tryExecuteBuy(*newOrder);
				break;
			}

			case input_sell: {
				std::string instr(input.instrument); 
				std::thread::id currThreadId = std::this_thread::get_id();
				Order* newOrder = new Order(input.order_id, instr, input.price, input.count, "sell", currThreadId);
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
