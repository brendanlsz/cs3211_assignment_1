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
		std::chrono::microseconds::rep timestamp;        // the timestamp when the order was added to the book
};

class InstrumentOrderBook{

	class CompareBuy {
		public:
			bool operator() (Order x, Order y)
			{
				return x.price < y.price;
			}
	};

	class CompareSell {
		public:
			bool operator() (Order x, Order y)
			{
				return x.price > y.price;
			}
	};
	std::priority_queue<Order, std::vector<Order>, CompareBuy> buy;
	std::priority_queue<Order, std::vector<Order>, CompareSell> sell;

	public:
	InstrumentOrderBook() : buy{} , sell{} {}

};

class OrderMap {
	// hasmap<"str: instrument" : InstrumentOrderBook>
	std::unordered_map<std::string, InstrumentOrderBook> instrument_map;
	std::mutex mut;

	public:
	OrderMap() : instrument_map{}, mut{} {}

	InstrumentOrderBook getInstrument() {

	}

}

class InstrumentOrderBook{
	std::vector<> buy;
	std::vector<> sell;

	public:
	InstrumentOrderBook() : buy{} , sell{} {}

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
