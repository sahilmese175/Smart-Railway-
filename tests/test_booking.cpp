#include <cstdio>
#include <string>

#include "api/router.h"
#include "booking/booking.h"
#include "common/json.h"

using namespace railway;

static int checks = 0;
static int failed = 0;

#define CHECK(condition) \
    do { \
        ++checks; \
        if (!(condition)) { \
            ++failed; \
            std::printf("  FAILED %s:%d  %s\n", __FILE__, __LINE__, #condition); \
        } \
    } while (0)

int main() {
    BookingManager& manager = BookingManager::instance();
    std::string error;

    const int rahul = manager.addPassenger("Rahul", 28, error);
    const int amina = manager.addPassenger("Amina", 32, error);
    CHECK(rahul == 1 && amina == 2 && error.empty());
    CHECK(manager.addPassenger("", 20, error) == 0 && !error.empty());
    CHECK(manager.addPassenger("Invalid age", 0, error) == 0);

    Passenger passenger{};
    CHECK(manager.searchPassenger(rahul, passenger));
    CHECK(std::string(passenger.name) == "Rahul" && passenger.age == 28);
    CHECK(manager.findPassengers("rah").size() == 1);
    CHECK(manager.updatePassenger(rahul, "Rahul Mehta", 29, error));
    CHECK(manager.searchPassenger(rahul, passenger) && std::string(passenger.name) == "Rahul Mehta");

    const BookingOperationResult first = manager.create(rahul, 12001, "Pune", "Mumbai");
    const BookingOperationResult second = manager.create(amina, 12001, "pune", "mumbai");
    CHECK(first.ok && first.pnr == 10001 && first.code == 201);
    CHECK(second.ok && second.pnr == 10002);

    BookingDetails details;
    CHECK(getBooking(first.pnr, details));
    CHECK(details.booking.seatNo == 1 && details.booking.status == BOOKING_CONFIRMED);
    CHECK(std::string(details.passengerName) == "Rahul Mehta");
    CHECK(std::string(details.source) == "Pune" && std::string(details.destination) == "Mumbai");
    CHECK(details.fare == 300.0);
    CHECK(details.booking.trainNo == 12001);

    Booking booking{};
    CHECK(searchPNR(first.pnr, booking) && booking.seatNo == 1);
    CHECK(!manager.create(9999, 12001, "Pune", "Mumbai").ok);
    CHECK(!manager.create(rahul, 12001, "Mumbai", "Pune").ok);
    CHECK(!manager.create(rahul, 12001, "Pune", "Nashik").ok);

    std::string cancelError;
    CHECK(cancelBooking(first.pnr, &cancelError) && cancelError == "Booking cancelled");
    CHECK(manager.create(rahul, 12001, "Pune", "Mumbai").pnr == 10003);
    CHECK(getBooking(first.pnr, details) && details.booking.status == BOOKING_CANCELLED);
    CHECK(!manager.cancel(first.pnr).ok);

    CHECK(manager.deletePassenger(amina, error));
    CHECK(!manager.searchPassenger(amina, passenger));
    CHECK(getBooking(second.pnr, details) && std::string(details.passengerName) == "Amina");

    const ds::DynamicArray<LocalTrainInfo> trains = manager.trains();
    CHECK(trains.size() == 1 && trains[0].trainNo == 12001 && trains[0].totalSeats == 100);
    CHECK(trains[0].stops.size() == 2 && trains[0].stops[0] == "Pune" && trains[0].stops[1] == "Mumbai");

    api::Router router;
    api::registerBookingRoutes(router);
    api::Request request;
    api::Response response;
    request.method = "GET";
    request.path = "/api/booking-trains";
    CHECK(router.dispatch(request, response) && response.status == 200);
    CHECK(response.body.find("\"trainNo\":12001") != std::string::npos);

    request = api::Request{};
    request.method = "POST";
    request.path = "/api/passengers";
    request.body = "{\"name\":\"API Passenger\",\"age\":20}";
    CHECK(router.dispatch(request, response) && response.status == 201);
    json::Value responseBody;
    CHECK(json::parse(response.body, responseBody, error));
    const int apiPassengerId = static_cast<int>(responseBody.get("passenger")->get("passengerId")->number);

    request = api::Request{};
    request.method = "POST";
    request.path = "/api/bookings";
    request.body = "{\"passengerId\":" + std::to_string(apiPassengerId) +
                   ",\"trainNo\":12001,\"source\":\"Pune\",\"destination\":\"Mumbai\"}";
    CHECK(router.dispatch(request, response) && response.status == 201);
    CHECK(response.body.find("\"status\":\"CONFIRMED\"") != std::string::npos);

    for (int i = 0; i < 97; ++i)
        CHECK(manager.create(rahul, 12001, "Pune", "Mumbai").ok);
    const BookingOperationResult full = manager.create(rahul, 12001, "Pune", "Mumbai");
    CHECK(!full.ok && full.code == 409);
    CHECK(manager.cancel(second.pnr).ok);
    CHECK(manager.create(rahul, 12001, "Pune", "Mumbai").ok);

    std::printf("test_booking: %d checks, %d failed\n", checks, failed);
    return failed ? 1 : 0;
}
