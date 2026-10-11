#include "train/train_json.h"

#include <cmath>

namespace railway {

void writeTrain(json::Writer& w, const Train& t) {
    int n = t.stationCount();
    w.beginObject();
    w.field("trainNo", t.trainNo);
    w.field("name", t.name);
    w.field("totalSeats", t.totalSeats);
    w.field("farePerKm", t.farePerKm);
    w.field("stopCount", n);
    w.field("source", t.stop(0).station);
    w.field("destination", t.stop(n - 1).station);
    w.field("departure", formatTime(t.firstDeparture()));
    w.field("arrival", formatTime(t.lastArrival()));
    w.field("arrivalDay", dayOf(t.lastArrival()));
    w.field("durationMin", t.durationMin());
    w.field("totalDistanceKm", t.totalDistance());
    w.field("totalFare", t.totalFare());
    w.key("stops").beginArray();
    for (int i = 0; i < n; ++i) {
        const Stop& s = t.stop(i);
        w.beginObject();
        w.field("index", i);
        w.field("station", s.station);
        w.field("arrival", formatTime(s.arrival));
        w.field("arrivalDay", dayOf(s.arrival));
        w.field("departure", formatTime(s.departure));
        w.field("departureDay", dayOf(s.departure));
        w.field("distanceKm", s.distanceKm);
        w.field("fareFromStart", t.fareBetween(0, i));
        w.endObject();
    }
    w.endArray();
    w.endObject();
}

static bool readInt(const json::Value* v, int& out, const char* what, std::string& err, bool required) {
    if (!v || v->isNull()) {
        if (required) { err = std::string("Missing field: ") + what; return false; }
        return true;
    }
    if (!v->isNumber() || v->number != std::floor(v->number) || std::fabs(v->number) > 1e9) {
        err = std::string(what) + " must be a whole number";
        return false;
    }
    out = static_cast<int>(v->number);
    return true;
}

static bool readTime(const json::Value* v, int& out, const std::string& label, std::string& err) {
    out = -1;
    if (!v || v->isNull()) return true;
    if (!v->isString()) { err = label + " must be text like 07:15"; return false; }
    if (v->string.empty()) return true;
    if (!parseTime(v->string, out)) { err = label + " must look like 07:15 (24-hour time)"; return false; }
    return true;
}

bool parseTrainInput(const json::Value& v, TrainInput& out, std::string& err) {
    if (!v.isObject()) { err = "Request body must be a JSON object"; return false; }
    out.trainNo = 0;
    if (!readInt(v.get("trainNo"), out.trainNo, "trainNo", err, false)) return false;

    const json::Value* name = v.get("name");
    if (!name || !name->isString()) { err = "Missing field: name"; return false; }
    out.name = name->string;

    if (!readInt(v.get("totalSeats"), out.totalSeats, "totalSeats", err, true)) return false;

    const json::Value* fare = v.get("farePerKm");
    if (!fare || !fare->isNumber()) { err = "Missing field: farePerKm (a number)"; return false; }
    out.farePerKm = fare->number;

    const json::Value* stops = v.get("stops");
    if (!stops || !stops->isArray()) { err = "Missing field: stops (a list)"; return false; }
    out.stops.clear();
    for (size_t i = 0; i < stops->items.size(); ++i) {
        const json::Value& s = stops->items[i];
        std::string label = "Stop " + std::to_string(i + 1);
        if (!s.isObject()) { err = label + " must be an object"; return false; }
        StopInput si;
        const json::Value* st = s.get("station");
        if (!st || !st->isString()) { err = label + ": station name is missing"; return false; }
        si.station = st->string;
        if (!readTime(s.get("arrival"), si.arrival, label + " arrival", err)) return false;
        if (!readTime(s.get("departure"), si.departure, label + " departure", err)) return false;
        if (!readInt(s.get("distanceKm"), si.distanceKm, "distanceKm", err, false)) { err = label + ": " + err; return false; }
        out.stops.push_back(si);
    }
    return true;
}

}  // namespace railway
