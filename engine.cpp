#include <iostream>
#include <thread>
#include <unordered_map>
#include <queue>

#include <string>
#include <unordered_map>

#include "io.hpp"
#include "engine.hpp"

class Order {
	public:
		int order_id;        // the order ID
		std::string instrument;       // the instrument name
		int price;            // the order's price
		int count;            // the order's size
		std::string side;             // the side (buy or sell)
		int match_count;  // number of times order was matched
		std::chrono::microseconds::rep timestamp;        // the timestamp when the order was added to the book

		Order(int order_id, std::string instrument, int price, int count, std::string side, std::chrono::microseconds::rep timestamp) 
			: order_id(order_id), instrument(instrument), price(price), count(count), side(side), match_count(0), timestamp(timestamp) {}
};

class Node {
	public:
		Node* next;
		Order* order;
		std::mutex m;

		Node(Node* next, Order* order) : next(next), order(order), m{} {}
};

class InstrumentOrderBook{

	// class CompareBuy {
	// 	public:
	// 		bool operator() (Order x, Order y)
	// 		{
	// 			return x.price < y.price;
	// 		}
	// };

	// class CompareSell {
	// 	public:
	// 		bool operator() (Order x, Order y)
	// 		{
	// 			return x.price > y.price;
	// 		}
	// };
	// std::priority_queue<Order, std::vector<Order>, CompareBuy> buy;
	// std::priority_queue<Order, std::vector<Order>, CompareSell> sell;	

	public:
	Node* buy_head;
	Node* sell_head;
	std::string instr;
	//InstrumentOrderBook() : buy{} , sell{} {}
	InstrumentOrderBook(std::string instr) : buy_head(new Node(nullptr, new Order{0, instr, 0, 0, "0", 0})), sell_head(new Node(nullptr, new Order{0, instr, 0, 0, "0", 0})), 
		instr(instr) {}

	void insertBuy(Order* order) {
		// dummy mutex
		std::mutex mut;
		Node* curr = nullptr;
		Node* curr_next = buy_head;
		std::unique_lock<std::mutex> curr_lk(mut);
		std::unique_lock<std::mutex> curr_next_lk (curr_next->m);
		while(curr_next != nullptr && curr_next->order->price > order->price) {
			curr_lk.swap(curr_next_lk);
			curr_next_lk = std::unique_lock<std::mutex>(curr_next->next->m);
			curr = curr_next;
			curr_next = curr_next->next;
		}
		// insert node between curr and curr_next;
		Node* node = new Node(nullptr, order);
		if(curr == nullptr) {
			// insert at head
			buy_head = node;
			buy_head->next = curr_next;
		} else if (curr_next == nullptr) {
			// insert at tail
			curr->next = node;
		} else {
			// insert in between
			curr->next = node;
			node->next = curr_next;
		}
	}

	void insertSell(Order* order) {
		// dummy mutex
		std::mutex mut;
		Node* curr = nullptr;
		Node* curr_next = buy_head;
		std::unique_lock<std::mutex> curr_lk(mut);
		std::unique_lock<std::mutex> curr_next_lk (curr_next->m);
		while(curr_next != nullptr && curr_next->order->price < order->price) {
			curr_lk.swap(curr_next_lk);
			curr_next_lk = std::unique_lock<std::mutex>(curr_next->next->m);
			curr = curr_next;
			curr_next = curr_next->next;
		}
		// insert node between curr and curr_next;
		Node* node = new Node(nullptr, order);
		if(curr == nullptr) {
			// insert at head
			buy_head = node;
			buy_head->next = curr_next;
		} else if (curr_next == nullptr) {
			// insert at tail
			curr->next = node;
		} else {
			// insert in between
			curr->next = node;
			node->next = curr_next;
		}
	}
};

class OrderMap {
	// hasmap<"str: instrument" : InstrumentOrderBook>
	std::unordered_map<std::string, InstrumentOrderBook> instrument_map;
	std::mutex mut;

	public:
	OrderMap() : instrument_map{}, mut{} {}

	InstrumentOrderBook& getInstrument(std::string instrument) {
		std::unique_lock lock{mut};
		if(instrument_map.contains(instrument)) {
			return instrument_map.at(instrument);
		} else {
			InstrumentOrderBook newInstrument(instrument);
			instrument_map.insert(std::make_pair(instrument, newInstrument));
			return instrument_map.at(instrument);
		}
	}
};

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
				auto output_time = getCurrentTimestamp();
				Output::OrderDeleted(input.order_id, true, output_time);
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
		intmax_t output_time = getCurrentTimestamp();

		// Check the parameter names in `io.hpp`.
		Output::OrderExecuted(123, 124, 1, 2000, 10, output_time);
	}
}
