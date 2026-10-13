#include "api/router.h"

#include <cctype>
#include <cmath>
#include <limits>
#include <string>

#include "booking/booking.h"
#include "common/json.h"

namespace api {
namespace {

using railway::BookingDetails;
using railway::BookingManager;

Response failure(int code, const std::string& message) {
    json::Writer w;
    w.beginObject().field("ok", false).field("error", message).endObject();
    return Response::json(code, w.str());
}

bool parseInt(const std::string& text, int& value) {
    if (text.empty() || text.size() > 10) return false;
    for (unsigned char c : text) if (!std::isdigit(c)) return false;
    try {
        const long long parsed = std::stoll(text);
        if (parsed > std::numeric_limits<int>::max()) return false;
        value = static_cast<int>(parsed);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool readRequiredString(const json::Value& object, const char* field, std::string& value) {
    const json::Value* item = object.get(field);
    if (!item || !item->isString()) return false;
    value = item->string;
    return true;
}

bool readRequiredInt(const json::Value& object, const char* field, int& value) {
    const json::Value* item = object.get(field);
    if (!item || !item->isNumber() || std::floor(item->number) != item->number ||
        item->number < 0 || item->number > std::numeric_limits<int>::max()) return false;
    value = static_cast<int>(item->number);
    return true;
}

bool parseBody(const Request& req, json::Value& body, std::string& error) {
    if (!json::parse(req.body, body, error)) {
        error = "Request body is not valid JSON: " + error;
        return false;
    }
    if (!body.isObject()) {
        error = "Request body must be a JSON object";
        return false;
    }
    return true;
}

void writeBooking(json::Writer& w, const BookingDetails& d) {
    w.beginObject();
    w.field("pnr", d.booking.pnr);
    w.field("passengerId", d.booking.passengerId);
    w.field("passenger", d.passengerName);
    w.field("trainNo", d.booking.trainNo);
    w.field("trainName", d.trainName);
    w.field("source", d.source);
    w.field("destination", d.destination);
    w.field("sourceIndex", d.booking.sourceIndex);
    w.field("destinationIndex", d.booking.destinationIndex);
    w.field("seat", d.booking.seatNo);
    w.field("fare", d.fare);
    w.field("status", d.booking.status == railway::BOOKING_CONFIRMED ? "CONFIRMED" : "CANCELLED");
    w.endObject();
}

void writePassenger(json::Writer& w, const railway::Passenger& p) {
    w.beginObject().field("passengerId", p.passengerId)
        .field("name", p.name).field("age", p.age).endObject();
}

Response listPassengers(const Request& req) {
    const ds::DynamicArray<railway::Passenger> matches = BookingManager::instance().findPassengers(req.q("q"));
    json::Writer w;
    w.beginObject().field("ok", true).field("count", matches.size());
    w.key("passengers").beginArray();
    for (const railway::Passenger& p : matches) writePassenger(w, p);
    w.endArray().endObject();
    return Response::json(200, w.str());
}

Response savePassenger(const Request& req, bool update) {
    int passengerId = 0, age = 0;
    std::string name, error;
    json::Value body;
    if (!parseBody(req, body, error)) return failure(400, error);
    if (!readRequiredString(body, "name", name) || !readRequiredInt(body, "age", age))
        return failure(400, "name (string) and age (integer) are required");
    if (update && !parseInt(req.param("id"), passengerId))
        return failure(400, "Passenger ID must be a number");

    BookingManager& manager = BookingManager::instance();
    if (update) {
        if (!manager.updatePassenger(passengerId, name, age, error))
            return failure(error == "Passenger not found" ? 404 : 400, error);
    } else {
        passengerId = manager.addPassenger(name, age, error);
        if (!passengerId) return failure(400, error);
    }

    railway::Passenger passenger{};
    manager.searchPassenger(passengerId, passenger);
    json::Writer w;
    w.beginObject().field("ok", true).field("message", update ? "Passenger updated" : "Passenger added");
    w.key("passenger");
    writePassenger(w, passenger);
    w.endObject();
    return Response::json(update ? 200 : 201, w.str());
}

Response deletePassenger(const Request& req) {
    int id = 0;
    if (!parseInt(req.param("id"), id)) return failure(400, "Passenger ID must be a number");
    std::string error;
    if (!BookingManager::instance().deletePassenger(id, error)) return failure(404, error);
    json::Writer w;
    w.beginObject().field("ok", true).field("message", "Passenger deleted").endObject();
    return Response::json(200, w.str());
}

Response listTrains(const Request&) {
    const ds::DynamicArray<railway::LocalTrainInfo> trains = BookingManager::instance().trains();
    json::Writer w;
    w.beginObject().field("ok", true).field("count", trains.size());
    w.key("trains").beginArray();
    for (const railway::LocalTrainInfo& train : trains) {
        w.beginObject().field("trainNo", train.trainNo).field("name", train.name).field("totalSeats", train.totalSeats);
        w.key("stops").beginArray();
        for (const std::string& stop : train.stops) w.value(stop);
        w.endArray().endObject();
    }
    w.endArray().endObject();
    return Response::json(200, w.str());
}

Response listBookings(const Request&) {
    const ds::DynamicArray<BookingDetails> bookings = BookingManager::instance().bookings();
    json::Writer w;
    w.beginObject().field("ok", true).field("count", bookings.size());
    w.key("bookings").beginArray();
    for (const BookingDetails& booking : bookings) writeBooking(w, booking);
    w.endArray().endObject();
    return Response::json(200, w.str());
}

Response createBooking(const Request& req) {
    json::Value body;
    std::string error, source, destination;
    int passengerId = 0, trainNo = 0;
    if (!parseBody(req, body, error)) return failure(400, error);
    if (!readRequiredInt(body, "passengerId", passengerId) ||
        !readRequiredInt(body, "trainNo", trainNo) ||
        !readRequiredString(body, "source", source) ||
        !readRequiredString(body, "destination", destination))
        return failure(400, "passengerId, trainNo, source and destination are required");
    const railway::BookingOperationResult result =
        BookingManager::instance().create(passengerId, trainNo, source, destination);
    if (!result.ok) return failure(result.code, result.message);
    BookingDetails details;
    if (!railway::getBooking(result.pnr, details))
        return failure(500, "Booking was created but could not be loaded");
    json::Writer w;
    w.beginObject().field("ok", true).field("message", result.message).key("booking");
    writeBooking(w, details);
    w.endObject();
    return Response::json(201, w.str());
}

Response getBooking(const Request& req) {
    int pnr = 0;
    if (!parseInt(req.param("pnr"), pnr)) return failure(400, "PNR must be a number");
    BookingDetails details;
    if (!railway::getBooking(pnr, details)) return failure(404, "PNR not found");
    json::Writer w;
    w.beginObject().field("ok", true).key("booking");
    writeBooking(w, details);
    w.endObject();
    return Response::json(200, w.str());
}

Response cancelBooking(const Request& req) {
    int pnr = 0;
    if (!parseInt(req.param("pnr"), pnr)) return failure(400, "PNR must be a number");
    const railway::BookingOperationResult result = BookingManager::instance().cancel(pnr);
    if (!result.ok) return failure(result.code, result.message);
    BookingDetails details;
    if (!railway::getBooking(pnr, details)) return failure(500, "Cancelled booking could not be loaded");
    json::Writer w;
    w.beginObject().field("ok", true).field("message", result.message).key("booking");
    writeBooking(w, details);
    w.endObject();
    return Response::json(200, w.str());
}

}  // namespace

void registerBookingRoutes(Router& router) {
    const std::string module = "booking";
    router.addModule({"booking", "Passenger & Reservation", "Member 2", "live",
                      "Passenger management, PNR bookings, seat allocation, fare calculation and cancellation."});
    router.add("GET", "/api/passengers", listPassengers, module, "List or search passengers. ?q=name");
    router.add("POST", "/api/passengers", [](const Request& req) { return savePassenger(req, false); },
               module, "Add a passenger");
    router.add("PUT", "/api/passengers/:id", [](const Request& req) { return savePassenger(req, true); },
               module, "Update a passenger");
    router.add("DELETE", "/api/passengers/:id", deletePassenger, module, "Delete a passenger");
    router.add("GET", "/api/booking-trains", listTrains, module, "List the reservation module's demo trains");
    router.add("GET", "/api/bookings", listBookings, module, "List bookings");
    router.add("POST", "/api/bookings", createBooking, module, "Create a booking and allocate a seat");
    router.add("GET", "/api/bookings/:pnr", getBooking, module, "PNR enquiry");
    router.add("DELETE", "/api/bookings/:pnr", cancelBooking, module, "Cancel a ticket");
}

}  // namespace api
