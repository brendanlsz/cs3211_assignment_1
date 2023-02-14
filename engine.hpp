// This file contains declarations for the main Engine class. You will
// need to add declarations to this file as you develop your Engine.

#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <chrono>
#include <thread>
#include <unordered_map>

#include "io.hpp"

class Order {
public:
	int order_id;        // the order ID
	std::string instrument;       // the instrument name
	int price;            // the order's price
	int count;            // the order's size
	std::string side;             // the side (buy or sell)
	bool isFullyFilled;
	bool isCancelled;
	Order(int order_id, std::string instrument, int price, int count, std::string side) 
		: order_id(order_id), instrument(instrument), price(price), count(count), side(side), 
		isFullyFilled(false), isCancelled(false){}
};

class Node {
public:
	Node* next;
	Order* order;
	std::mutex m;
	int exec_id;

	Node(Node* next, Order* order) : next(next), order(order), m{}, exec_id(1) {}
};

class InstrumentOrderBook {
public:
	Node* const buy_head;
	Node* const sell_head;
	std::string instr;
	//InstrumentOrderBook() : buy{} , sell{} {}
	InstrumentOrderBook(std::string instr) : buy_head(new Node(nullptr, new Order{0, instr, 0, 0, "0"})), sell_head(new Node(nullptr, new Order{0, instr, 0, 0, "0"})), 
		instr(instr) {}
	void tryExecuteBuy(Order& order);
	void tryExecuteSell(Order& order);
	void tryCancel(int order_id);
	void insertBuy(Order& order);
	void insertSell(Order& order);
};

class OrderMap {
	// hasmap<"str: instrument" : InstrumentOrderBook>
	std::unordered_map<std::string, InstrumentOrderBook> instrument_map;
	std::unordered_map<int, std::string> order_instrument_map;
	std::mutex mut;

public:
	OrderMap() : instrument_map{}, order_instrument_map{}, mut{} {}

	InstrumentOrderBook& getInstrument(std::string instrument);
	std::string getOrderInstrument(int orderId);
	void addOrderInstrumentRecord(int orderId, std::string instr);
};

struct Engine
{
public:
	void accept(ClientConnection conn);

private:
	OrderMap order_map;
	void connection_thread(ClientConnection conn);
	
};



inline std::chrono::microseconds::rep getCurrentTimestamp() noexcept
{
	return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

#endif
