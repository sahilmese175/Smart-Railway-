# Passenger and Basic Reservation Management

This module is independent of Member 1's train database. It starts with a local demo train:

- Train 12001, Pune → Mumbai, 100 seats
- Fare: ₹2 per km (150 km for the demo journey)
- PNR numbers start at 10001

## C++ interface

Include `booking/booking.h`:

```cpp
int createBooking(int passengerId, int trainNo, const char* source, const char* destination,
                  std::string* error = nullptr);
bool cancelBooking(int pnr, std::string* error = nullptr);
bool searchPNR(int pnr, Booking& out);
bool getBooking(int pnr, BookingDetails& out);
```

`BookingManager::instance()` provides passenger add, delete, update, search and name-query operations, and exposes demo trains and booking details for the API.

## Data structures

- `LinkedList` stores passengers, local trains and booking records in insertion order.
- `HashTable` indexes passengers by ID, trains by number and bookings by PNR.
- `DynamicArray` stores local train stops and per-seat, per-segment occupancy.

The API also exposes passenger CRUD/search, booking creation, PNR enquiry, seat allocation and ticket cancellation. See [the API reference](../../docs/API.md#passengers-and-reservations).
