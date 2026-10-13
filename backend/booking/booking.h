#pragma once

#include <string>

#include "common/contract.h"
#include "ds/dynamic_array.h"

namespace railway {

struct BookingDetails {
    Booking booking{};
    char passengerName[50]{};
    char trainName[50]{};
    char source[50]{};
    char destination[50]{};
    double fare = 0;
};

struct LocalTrainInfo {
    int trainNo = 0;
    std::string name;
    int totalSeats = 0;
    ds::DynamicArray<std::string> stops;
};

struct BookingOperationResult {
    bool ok = false;
    int code = 400;
    int pnr = 0;
    std::string message;
};

class BookingManager {
public:
    static BookingManager& instance();
    ~BookingManager();
    BookingManager(const BookingManager&) = delete;
    BookingManager& operator=(const BookingManager&) = delete;

    int addPassenger(const std::string& name, int age, std::string& error);
    bool deletePassenger(int passengerId, std::string& error);
    bool updatePassenger(int passengerId, const std::string& name, int age, std::string& error);
    bool searchPassenger(int passengerId, Passenger& out) const;
    ds::DynamicArray<Passenger> findPassengers(const std::string& query) const;
    ds::DynamicArray<Passenger> passengers() const;

    BookingOperationResult create(int passengerId, int trainNo, const std::string& source,
                                  const std::string& destination);
    BookingOperationResult cancel(int pnr);
    bool searchPNR(int pnr, Booking& out) const;
    bool getBooking(int pnr, BookingDetails& out) const;
    ds::DynamicArray<BookingDetails> bookings() const;
    ds::DynamicArray<LocalTrainInfo> trains() const;

private:
    BookingManager();
    struct Impl;
    Impl* impl_;
};

// Stable module interface used by other C++ modules.
int createBooking(int passengerId, int trainNo, const char* source, const char* destination,
                  std::string* error = nullptr);
bool cancelBooking(int pnr, std::string* error = nullptr);
bool searchPNR(int pnr, Booking& out);
bool getBooking(int pnr, BookingDetails& out);

}  // namespace railway
