CS3211 Assignment 1 Report

<table border="0">
 <tr>
    <td>Yeat Nai Jie (A0219955N/E0550613)</td>
    <td>Brendan Lau Siew Zhi (A0219938M/E0550596)</td>
 </tr>
</table>

## Data Structures

### OrderMap

Instantiated within `Engine` and contains two `unordered_map`s.
- `instrument_map` - maps the instrument string to `InstrumentOrderBook`
- `order_instrument_map` - maps order id to instrument string, used when cancelling an order.

### InstrumentOrderBook

Contains two `HeadNode` objects, one for `buy_head` and one for `sell_head`, essentially acting as the head node for the buy order book and sell order book (linked list) respectively. One `InstrumentOrderBook` is created for each instrument. We perform buy and sell matching, as well as cancelling on these linked lists. In addition, both linked lists are designed to remain sorted throughout operations in order to facilitate order matching. This is done by inserting `Node` objects at the correct position when new orders are added 

### HeadNode

Contains a `Node` object, which acts as the head node for the buy/sell order book in `InstrumentOrderBook`. Two `HeadNode` objects are created per `InstrumentOrderBook`, one for the instrument's buy order book, and one for its sell order book,

### Node

Contains an `Order` object and a pointer to the next `Node` in the linked list for buy/sell order books

### Order

Contains information on a resting order in buy/sell order books.

## Synchronisation primitives

Our solution makes use of fine grained locks in each node in the linked lists. To enable maximum concurrency, we use `shared_mutex`s instead of normal mutexes.

### Example: multiple buys
Multiple buys received in succession from multiple different threads can be processed concurrently, i.e. another buy order can begin processing before the preceding one has been completed. 

1. When a buy order is received, `tryExecuteBuy` is called, and we instantiate a `unique_lock` on `sell_head` and a `shared_lock` on `buy_head`.
2. 
