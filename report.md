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

Contains two `HeadNode` objects, one for `buy_head` and one for `sell_head`, essentially acting as the head node for the buy order book and sell order book (linked list) respectively. One `InstrumentOrderBook` is created for each instrument. We perform buy and sell matching, as well as cancelling on these linked lists.

We enable maximum concurrency by making use of `shared_mutex`s instead of normal mutexes.

#### Example: multiple buys
Multiple buys received in succession from multiple different threads can be processed concurrently, i.e. another buy order can begin processing before the preceding one has been completed. 

1. When a buy order is received, `tryExecuteBuy` is called, we lock the
