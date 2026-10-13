#include "booking/booking.h"

#include <cctype>
#include <cstring>
#include <limits>

#include "ds/hash_table.h"
#include "ds/linked_list.h"

namespace railway {
namespace {

std::string normalized(const std::string& text) {
    std::string out;
    bool pendingSpace = false;
    for (unsigned char c : text) {
        if (std::isspace(c)) {
            pendingSpace = !out.empty();
            continue;
        }
        if (pendingSpace) out += ' ';
        pendingSpace = false;
        out += static_cast<char>(std::tolower(c));
    }
    return out;
}

bool validPassenger(const std::string& name, int age, std::string& error) {
    if (normalized(name).empty() || name.size() >= sizeof(Passenger::name)) {
        error = "Passenger name must contain 1 to 49 characters";
        return false;
    }
    for (unsigned char c : name) {
        if (c < 0x20) {
            error = "Passenger name cannot contain control characters";
            return false;
        }
    }
    if (age < 1 || age > 120) {
        error = "Passenger age must be between 1 and 120";
        return false;
    }
    return true;
}

void copyText(char (&target)[50], const std::string& value) {
    std::strncpy(target, value.c_str(), sizeof(target) - 1);
    target[sizeof(target) - 1] = '\0';
}

}  // namespace

struct BookingManager::Impl {
    struct Train {
        int trainNo = 0;
        std::string name;
        int totalSeats = 0;
        double farePerKm = 0;
        ds::DynamicArray<std::string> stops;
        ds::DynamicArray<int> cumulativeKm;
        ds::DynamicArray<char> occupiedSegments;
    };

    struct Record {
        Booking booking{};
        std::string passengerName;
        std::string trainName;
        std::string source;
        std::string destination;
        double fare = 0;
    };

    ds::LinkedList<Passenger> passengerList;
    ds::HashTable<int, ds::LinkedList<Passenger>::Node*> passengersById;
    ds::LinkedList<Train> trainList;
    ds::HashTable<int, ds::LinkedList<Train>::Node*> trainsByNo;
    ds::LinkedList<Record> bookingList;
    ds::HashTable<int, ds::LinkedList<Record>::Node*> bookingsByPnr;
    int nextPassengerId = 1;
    int nextPnr = 10001;

    Impl() {
        Train demo;
        demo.trainNo = 12001;
        demo.name = "Pune-Mumbai Express";
        demo.totalSeats = 100;
        demo.farePerKm = 2.0;
        demo.stops.push_back("Pune");
        demo.stops.push_back("Mumbai");
        demo.cumulativeKm.push_back(0);
        demo.cumulativeKm.push_back(150);
        for (int seat = 0; seat < demo.totalSeats; ++seat) demo.occupiedSegments.push_back(0);
        trainsByNo.put(demo.trainNo, trainList.push_back(demo));
    }

    bool seatIsFree(const Train& train, int seat, int from, int to) const {
        for (int segment = from; segment < to; ++segment) {
            const size_t index = static_cast<size_t>(seat) * (train.stops.size() - 1) +
                                 static_cast<size_t>(segment);
            if (train.occupiedSegments[index]) return false;
        }
        return true;
    }

    void setSeat(Train& train, int seat, int from, int to, char occupied) {
        for (int segment = from; segment < to; ++segment) {
            const size_t index = static_cast<size_t>(seat) * (train.stops.size() - 1) +
                                 static_cast<size_t>(segment);
            train.occupiedSegments[index] = occupied;
        }
    }
};

BookingManager& BookingManager::instance() {
    static BookingManager manager;
    return manager;
}

BookingManager::BookingManager() : impl_(new Impl()) {}
BookingManager::~BookingManager() { delete impl_; }

int BookingManager::addPassenger(const std::string& name, int age, std::string& error) {
    if (!validPassenger(name, age, error)) return 0;
    if (impl_->nextPassengerId == std::numeric_limits<int>::max()) {
        error = "Passenger ID space is exhausted";
        return 0;
    }
    Passenger passenger{};
    passenger.passengerId = impl_->nextPassengerId++;
    copyText(passenger.name, name);
    passenger.age = age;
    impl_->passengersById.put(passenger.passengerId, impl_->passengerList.push_back(passenger));
    error.clear();
    return passenger.passengerId;
}

bool BookingManager::deletePassenger(int passengerId, std::string& error) {
    auto* node = impl_->passengersById.get(passengerId);
    if (!node) {
        error = "Passenger not found";
        return false;
    }
    impl_->passengerList.removeNode(*node);
    impl_->passengersById.remove(passengerId);
    error.clear();
    return true;
}

bool BookingManager::updatePassenger(int passengerId, const std::string& name, int age, std::string& error) {
    if (!validPassenger(name, age, error)) return false;
    auto* node = impl_->passengersById.get(passengerId);
    if (!node) {
        error = "Passenger not found";
        return false;
    }
    copyText((*node)->value.name, name);
    (*node)->value.age = age;
    error.clear();
    return true;
}

bool BookingManager::searchPassenger(int passengerId, Passenger& out) const {
    auto* node = impl_->passengersById.get(passengerId);
    if (!node) return false;
    out = (*node)->value;
    return true;
}

ds::DynamicArray<Passenger> BookingManager::findPassengers(const std::string& query) const {
    ds::DynamicArray<Passenger> found;
    const std::string key = normalized(query);
    for (const Passenger& passenger : impl_->passengerList) {
        if (key.empty() || normalized(passenger.name).find(key) != std::string::npos)
            found.push_back(passenger);
    }
    return found;
}

ds::DynamicArray<Passenger> BookingManager::passengers() const {
    return findPassengers("");
}

BookingOperationResult BookingManager::create(int passengerId, int trainNo, const std::string& source,
                                               const std::string& destination) {
    BookingOperationResult result;
    auto* passengerNode = impl_->passengersById.get(passengerId);
    if (!passengerNode) {
        result.code = 404;
        result.message = "Passenger not found";
        return result;
    }
    auto* trainNode = impl_->trainsByNo.get(trainNo);
    if (!trainNode) {
        result.code = 404;
        result.message = "Train not found in the reservation demo data";
        return result;
    }

    Impl::Train& train = (*trainNode)->value;
    const std::string fromKey = normalized(source);
    const std::string toKey = normalized(destination);
    int from = -1, to = -1;
    for (size_t i = 0; i < train.stops.size(); ++i) {
        const std::string key = normalized(train.stops[i]);
        if (key == fromKey) from = static_cast<int>(i);
        if (key == toKey) to = static_cast<int>(i);
    }
    if (from < 0 || to < 0) {
        result.code = 400;
        result.message = "Source and destination must be stations on the selected train";
        return result;
    }
    if (from >= to) {
        result.code = 400;
        result.message = "Destination must come after the source";
        return result;
    }
    if (impl_->nextPnr == std::numeric_limits<int>::max()) {
        result.code = 507;
        result.message = "PNR number space is exhausted";
        return result;
    }

    int seat = 0;
    while (seat < train.totalSeats && !impl_->seatIsFree(train, seat, from, to)) ++seat;
    if (seat == train.totalSeats) {
        result.code = 409;
        result.message = "No seats are available for this journey";
        return result;
    }

    Impl::Record record;
    record.booking.pnr = impl_->nextPnr++;
    record.booking.trainNo = trainNo;
    record.booking.passengerId = passengerId;
    record.booking.sourceIndex = from;
    record.booking.destinationIndex = to;
    record.booking.seatNo = seat + 1;
    record.booking.status = BOOKING_CONFIRMED;
    record.passengerName = (*passengerNode)->value.name;
    record.trainName = train.name;
    record.source = train.stops[static_cast<size_t>(from)];
    record.destination = train.stops[static_cast<size_t>(to)];
    const int distance = train.cumulativeKm[static_cast<size_t>(to)] -
                         train.cumulativeKm[static_cast<size_t>(from)];
    record.fare = distance * train.farePerKm;
    impl_->setSeat(train, seat, from, to, 1);
    impl_->bookingsByPnr.put(record.booking.pnr, impl_->bookingList.push_back(record));

    result.ok = true;
    result.code = 201;
    result.pnr = record.booking.pnr;
    result.message = "Booking confirmed";
    return result;
}

BookingOperationResult BookingManager::cancel(int pnr) {
    BookingOperationResult result;
    auto* recordNode = impl_->bookingsByPnr.get(pnr);
    if (!recordNode) {
        result.code = 404;
        result.message = "PNR not found";
        return result;
    }
    Impl::Record& record = (*recordNode)->value;
    if (record.booking.status == BOOKING_CANCELLED) {
        result.code = 409;
        result.message = "Booking is already cancelled";
        return result;
    }
    auto* trainNode = impl_->trainsByNo.get(record.booking.trainNo);
    if (!trainNode) {
        result.code = 500;
        result.message = "Booking train data is unavailable";
        return result;
    }
    impl_->setSeat((*trainNode)->value, record.booking.seatNo - 1,
                   record.booking.sourceIndex, record.booking.destinationIndex, 0);
    record.booking.status = BOOKING_CANCELLED;
    result.ok = true;
    result.code = 200;
    result.pnr = pnr;
    result.message = "Booking cancelled";
    return result;
}

bool BookingManager::searchPNR(int pnr, Booking& out) const {
    auto* node = impl_->bookingsByPnr.get(pnr);
    if (!node) return false;
    out = (*node)->value.booking;
    return true;
}

bool BookingManager::getBooking(int pnr, BookingDetails& out) const {
    auto* node = impl_->bookingsByPnr.get(pnr);
    if (!node) return false;
    const Impl::Record& record = (*node)->value;
    out.booking = record.booking;
    copyText(out.passengerName, record.passengerName);
    copyText(out.trainName, record.trainName);
    copyText(out.source, record.source);
    copyText(out.destination, record.destination);
    out.fare = record.fare;
    return true;
}

ds::DynamicArray<BookingDetails> BookingManager::bookings() const {
    ds::DynamicArray<BookingDetails> result;
    for (const Impl::Record& record : impl_->bookingList) {
        BookingDetails details;
        details.booking = record.booking;
        copyText(details.passengerName, record.passengerName);
        copyText(details.trainName, record.trainName);
        copyText(details.source, record.source);
        copyText(details.destination, record.destination);
        details.fare = record.fare;
        result.push_back(details);
    }
    return result;
}

ds::DynamicArray<LocalTrainInfo> BookingManager::trains() const {
    ds::DynamicArray<LocalTrainInfo> result;
    for (const Impl::Train& train : impl_->trainList) {
        LocalTrainInfo info;
        info.trainNo = train.trainNo;
        info.name = train.name;
        info.totalSeats = train.totalSeats;
        info.stops = train.stops;
        result.push_back(info);
    }
    return result;
}

int createBooking(int passengerId, int trainNo, const char* source, const char* destination,
                  std::string* error) {
    if (!source || !destination) {
        if (error) *error = "Source and destination are required";
        return -1;
    }
    BookingOperationResult result = BookingManager::instance().create(passengerId, trainNo, source, destination);
    if (error) *error = result.message;
    return result.ok ? result.pnr : -1;
}

bool cancelBooking(int pnr, std::string* error) {
    BookingOperationResult result = BookingManager::instance().cancel(pnr);
    if (error) *error = result.message;
    return result.ok;
}

bool searchPNR(int pnr, Booking& out) {
    return BookingManager::instance().searchPNR(pnr, out);
}

bool getBooking(int pnr, BookingDetails& out) {
    return BookingManager::instance().getBooking(pnr, out);
}

}  // namespace railway
