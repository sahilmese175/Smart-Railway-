// api/routes_train.cpp  --  MEMBER 1 : REST endpoints of the Train & Route module
//
// Pattern for every endpoint:   trainManager().clearTrace();  do the work;  send JSON + trace.
// The "trace" list is what the "Data structure console" in the UI prints.
#include <cctype>
#include <string>

#include "api/router.h"
#include "common/json.h"
#include "ds/benchmark.h"
#include "train/train.h"
#include "train/train_json.h"

namespace api {
namespace {

using railway::Train;
using railway::trainManager;

// ---------------------------------------------------------------- small helpers
void writeTrace(json::Writer& w) {
    w.key("trace").beginArray();
    for (const std::string& line : trainManager().trace()) w.value(line);
    w.endArray();
}

Response fail(int code, const std::string& message) {
    json::Writer w;
    w.beginObject().field("ok", false).field("error", message);
    writeTrace(w);
    w.endObject();
    return Response::json(code, w.str());
}

bool parseInt(const std::string& s, int& out) {
    if (s.empty() || s.size() > 9) return false;
    for (char c : s) if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    out = std::stoi(s);
    return true;
}

void writeStats(json::Writer& w, const char* key, const ds::SortStats& st, const char* algo) {
    w.key(key).beginObject();
    w.field("algorithm", algo);
    w.field("comparisons", st.comparisons);
    w.field("moves", st.moves);
    w.field("micros", st.micros);
    w.endObject();
}

// How the common query parameters  ?sort=fare&algo=quick&order=desc  are read.
struct SortParams {
    railway::SortField field = railway::SortField::TrainNo;
    ds::SortAlgo algo = ds::SortAlgo::Merge;
    bool desc = false;
    std::string error;
};
SortParams readSortParams(const Request& req, const char* defaultSort) {
    SortParams p;
    std::string sort = req.q("sort", defaultSort);
    std::string algo = req.q("algo", "merge");
    std::string order = req.q("order", "asc");
    if (!railway::parseSortField(sort, p.field)) p.error = "Unknown sort field '" + sort + "'. Use trainNo, name, fare, departure, arrival, duration, stops or distance.";
    else if (!ds::parseSortAlgo(algo, p.algo)) p.error = "Unknown algorithm '" + algo + "'. Use merge, quick, insertion or heap.";
    else if (order != "asc" && order != "desc") p.error = "order must be asc or desc";
    p.desc = (order == "desc");
    return p;
}

template <typename K> std::string keyText(const K& k);
template <> std::string keyText<int>(const int& k) { return std::to_string(k); }
template <> std::string keyText<std::string>(const std::string& k) { return k; }

template <typename K, typename V, typename H>
void writeHashStats(json::Writer& w, const ds::HashTable<K, V, H>& t) {
    w.beginObject();
    w.field("buckets", t.bucketCount());
    w.field("size", t.size());
    w.field("loadFactor", t.loadFactor());
    w.field("usedBuckets", t.usedBuckets());
    w.field("collisions", t.collisions());
    w.field("longestChain", t.longestChain());
    w.field("rehashes", t.rehashCount());
    w.endObject();
}

template <typename K, typename V, typename H>
void writeHashDump(json::Writer& w, const char* name, const ds::HashTable<K, V, H>& t) {
    w.beginObject();
    w.field("name", name);
    w.key("stats");
    writeHashStats(w, t);
    w.key("buckets").beginArray();
    for (size_t i = 0; i < t.bucketCount(); ++i) {
        w.beginObject().field("index", i);
        w.key("keys").beginArray();
        t.forEachInBucket(i, [&](const K& k, const V&) { w.value(keyText(k)); });
        w.endArray();
        w.endObject();
    }
    w.endArray();
    w.endObject();
}

void writeMatch(json::Writer& w, const railway::Match& m) {
    const Train& t = *m.train;
    w.beginObject();
    w.field("trainNo", t.trainNo);
    w.field("name", t.name);
    w.field("totalSeats", t.totalSeats);
    w.field("srcIdx", m.srcIdx);
    w.field("dstIdx", m.dstIdx);
    w.field("from", t.stop(m.srcIdx).station);
    w.field("to", t.stop(m.dstIdx).station);
    w.field("departure", railway::formatTime(m.departure));
    w.field("departureDay", railway::dayOf(m.departure));
    w.field("arrival", railway::formatTime(m.arrival));
    w.field("arrivalDay", railway::dayOf(m.arrival) - railway::dayOf(m.departure));
    w.field("durationMin", m.durationMin);
    w.field("distanceKm", m.distanceKm);
    w.field("fare", m.fare);
    w.key("stopsBetween").beginArray();
    for (int i = m.srcIdx; i <= m.dstIdx; ++i) w.value(t.stop(i).station);
    w.endArray();
    w.endObject();
}

// ---------------------------------------------------------------- trains
Response listTrains(const Request& req) {
    trainManager().clearTrace();
    SortParams sp = readSortParams(req, "trainNo");
    if (!sp.error.empty()) return fail(400, sp.error);
    ds::SortStats st;
    ds::DynamicArray<Train*> sorted = trainManager().sortedTrains(sp.field, sp.algo, sp.desc, &st);
    json::Writer w;
    w.beginObject().field("ok", true).field("count", sorted.size());
    writeStats(w, "sortStats", st, ds::sortAlgoName(sp.algo));
    w.key("trains").beginArray();
    for (Train* t : sorted) railway::writeTrain(w, *t);
    w.endArray();
    writeTrace(w);
    w.endObject();
    return Response::json(200, w.str());
}

Response getTrain(const Request& req) {
    trainManager().clearTrace();
    int no;
    if (!parseInt(req.param("no"), no)) return fail(400, "Train number must be a number");
    Train* t = trainManager().searchTrain(no);
    if (!t) return fail(404, "Train " + std::to_string(no) + " not found");
    json::Writer w;
    w.beginObject().field("ok", true);
    w.key("train");
    railway::writeTrain(w, *t);
    writeTrace(w);
    w.endObject();
    return Response::json(200, w.str());
}

Response saveTrain(const Request& req, bool isUpdate) {
    trainManager().clearTrace();
    json::Value doc;
    std::string err;
    if (!json::parse(req.body, doc, err)) return fail(400, "Request body is not valid JSON: " + err);
    railway::TrainInput in;
    if (!railway::parseTrainInput(doc, in, err)) return fail(400, err);
    railway::Status st;
    int no = in.trainNo;
    if (isUpdate) {
        if (!parseInt(req.param("no"), no)) return fail(400, "Train number must be a number");
        st = trainManager().updateTrain(no, in);
    } else {
        st = trainManager().addTrain(in);
    }
    if (!st.ok) return fail(st.code, st.message);
    json::Writer w;
    w.beginObject().field("ok", true).field("message", st.message);
    w.key("train");
    railway::writeTrain(w, *trainManager().searchTrain(no));
    writeTrace(w);
    w.endObject();
    return Response::json(st.code, w.str());
}

Response deleteTrain(const Request& req) {
    trainManager().clearTrace();
    int no;
    if (!parseInt(req.param("no"), no)) return fail(400, "Train number must be a number");
    railway::Status st = trainManager().deleteTrain(no);
    if (!st.ok) return fail(st.code, st.message);
    json::Writer w;
    w.beginObject().field("ok", true).field("message", st.message);
    writeTrace(w);
    w.endObject();
    return Response::json(200, w.str());
}

// ---------------------------------------------------------------- stations
Response listStations(const Request&) {
    trainManager().clearTrace();
    const railway::StationRegistry& reg = trainManager().stations();
    json::Writer w;
    w.beginObject().field("ok", true).field("count", reg.count());
    w.key("hash");
    writeHashStats(w, reg.table());
    w.key("stations").beginArray();
    for (size_t id = 0; id < reg.idSlots(); ++id) {
        if (!reg.alive(static_cast<int>(id))) continue;
        const ds::DynamicArray<int>& at = reg.trainsAt(static_cast<int>(id));
        w.beginObject().field("id", id).field("name", reg.name(static_cast<int>(id))).field("trainCount", at.size());
        w.key("trains").beginArray();
        for (int no : at) w.value(no);
        w.endArray();
        w.endObject();
    }
    w.endArray();
    w.endObject();
    return Response::json(200, w.str());
}

Response addStation(const Request& req) {
    trainManager().clearTrace();
    json::Value doc;
    std::string err;
    if (!json::parse(req.body, doc, err) || !doc.isObject()) return fail(400, "Request body must be JSON like {\"name\": \"Pune\"}");
    const json::Value* name = doc.get("name");
    if (!name || !name->isString()) return fail(400, "Missing field: name");
    railway::Status st = trainManager().addStation(name->string);
    if (!st.ok) return fail(st.code, st.message);
    json::Writer w;
    w.beginObject().field("ok", true).field("message", st.message);
    writeTrace(w);
    w.endObject();
    return Response::json(st.code, w.str());
}

Response deleteStation(const Request& req) {
    trainManager().clearTrace();
    railway::Status st = trainManager().deleteStation(req.param("name"));
    if (!st.ok) return fail(st.code, st.message);
    json::Writer w;
    w.beginObject().field("ok", true).field("message", st.message);
    writeTrace(w);
    w.endObject();
    return Response::json(200, w.str());
}

Response stationIndex(const Request& req) {
    trainManager().clearTrace();
    int no;
    if (!parseInt(req.q("train"), no)) return fail(400, "Give a train number: ?train=12001&station=Pune");
    std::string station = req.q("station");
    if (station.empty()) return fail(400, "Give a station name: ?train=12001&station=Pune");
    Train* t = trainManager().searchTrain(no);
    if (!t) return fail(404, "Train " + std::to_string(no) + " not found");
    int idx = trainManager().findStationIndex(no, station);
    json::Writer w;
    w.beginObject().field("ok", true).field("trainNo", no).field("station", station).field("found", idx >= 0).field("index", idx);
    w.field("stationCount", t->stationCount());
    writeTrace(w);
    w.endObject();
    return Response::json(200, w.str());
}

// ---------------------------------------------------------------- journeys
Response validate(const Request& req) {
    trainManager().clearTrace();
    int no;
    if (!parseInt(req.q("train"), no)) return fail(400, "Give a train number: ?train=12001&from=Pune&to=Mumbai");
    railway::Journey j = trainManager().validateJourney(no, req.q("from"), req.q("to"));
    json::Writer w;
    w.beginObject().field("ok", true).field("valid", j.ok).field("message", j.message).field("reason", j.code);
    if (j.ok) {
        w.key("journey");
        writeMatch(w, j.match);
    }
    writeTrace(w);
    w.endObject();
    return Response::json(200, w.str());
}

Response search(const Request& req) {
    trainManager().clearTrace();
    SortParams sp = readSortParams(req, "departure");
    if (!sp.error.empty()) return fail(400, sp.error);
    railway::SearchResult r = trainManager().findTrainsBetween(req.q("from"), req.q("to"), sp.field, sp.algo, sp.desc);
    if (!r.ok) return fail(r.code, r.message);
    json::Writer w;
    w.beginObject().field("ok", true).field("from", r.from).field("to", r.to);
    w.field("candidates", r.candidates).field("count", r.matches.size()).field("message", r.message);
    writeStats(w, "sortStats", r.stats, ds::sortAlgoName(sp.algo));
    w.key("results").beginArray();
    for (const railway::Match& m : r.matches) writeMatch(w, m);
    w.endArray();
    writeTrace(w);
    w.endObject();
    return Response::json(200, w.str());
}

// ---------------------------------------------------------------- dashboard + DSA lab
Response stats(const Request&) {
    trainManager().clearTrace();
    const auto& all = trainManager().trains();
    const railway::StationRegistry& reg = trainManager().stations();
    long seats = 0, stopSum = 0;
    const Train* longest = nullptr;
    const Train* farthest = nullptr;
    for (const Train* t : all) {
        seats += t->totalSeats;
        stopSum += t->stationCount();
        if (!longest || t->stationCount() > longest->stationCount()) longest = t;
        if (!farthest || t->totalDistance() > farthest->totalDistance()) farthest = t;
    }
    int busiestId = -1;
    for (size_t id = 0; id < reg.idSlots(); ++id) {
        if (!reg.alive(static_cast<int>(id))) continue;
        if (busiestId < 0 || reg.trainsAt(static_cast<int>(id)).size() > reg.trainsAt(busiestId).size()) busiestId = static_cast<int>(id);
    }
    json::Writer w;
    w.beginObject().field("ok", true);
    w.field("trains", all.size()).field("stations", reg.count()).field("totalSeats", seats);
    w.field("avgStops", all.empty() ? 0.0 : static_cast<double>(stopSum) / static_cast<double>(all.size()));
    if (longest) w.key("longestRoute").beginObject().field("trainNo", longest->trainNo).field("name", longest->name).field("stops", longest->stationCount()).endObject();
    if (farthest) w.key("farthest").beginObject().field("trainNo", farthest->trainNo).field("name", farthest->name).field("distanceKm", farthest->totalDistance()).endObject();
    if (busiestId >= 0) w.key("busiestStation").beginObject().field("name", reg.name(busiestId)).field("trains", reg.trainsAt(busiestId).size()).endObject();
    w.key("ds").beginObject();
    w.key("trainArray").beginObject().field("size", all.size()).field("capacity", all.capacity()).field("resizes", all.resizeCount()).endObject();
    w.key("trainHash");
    writeHashStats(w, trainManager().trainTable());
    w.key("stationHash");
    writeHashStats(w, reg.table());
    w.endObject();
    w.endObject();
    return Response::json(200, w.str());
}

Response dsaHash(const Request& req) {
    trainManager().clearTrace();
    std::string which = req.q("table", "trains");
    json::Writer w;
    w.beginObject().field("ok", true);
    w.key("table");
    if (which == "trains") writeHashDump(w, "HashTable<int, Train*>   (train number -> train)", trainManager().trainTable());
    else if (which == "stations") writeHashDump(w, "HashTable<string, int>   (station name -> station id)", trainManager().stations().table());
    else if (which == "stops") {
        int no;
        if (!parseInt(req.q("train"), no)) return fail(400, "Give ?table=stops&train=12001");
        Train* t = trainManager().searchTrain(no);
        if (!t) return fail(404, "Train " + std::to_string(no) + " not found");
        writeHashDump(w, "HashTable<string, int>   (station -> position in this route)", t->stopIndex);
    } else return fail(400, "table must be trains, stations or stops");
    w.endObject();
    return Response::json(200, w.str());
}

Response dsaLookup(const Request& req) {
    trainManager().clearTrace();
    int no;
    if (!parseInt(req.q("train"), no)) return fail(400, "Give a train number: ?train=12001");
    railway::LookupReport r = trainManager().lookupCompare(no);
    json::Writer w;
    w.beginObject().field("ok", true).field("trainNo", no).field("found", r.found).field("arraySize", r.arraySize);
    w.key("linear").beginObject().field("steps", r.linearSteps);
    w.key("order").beginArray();
    for (int v : r.insertionOrder) w.value(v);
    w.endArray().endObject();
    w.key("binary").beginObject().field("steps", r.binaryProbes.size());
    w.key("order").beginArray();
    for (int v : r.sortedOrder) w.value(v);
    w.endArray();
    w.key("probes").beginArray();
    for (size_t p : r.binaryProbes) w.value(p);
    w.endArray().endObject();
    w.key("hash").beginObject().field("bucket", r.hashBucket).field("probes", r.hashProbes).field("buckets", r.hashBuckets).endObject();
    w.endObject();
    return Response::json(200, w.str());
}

Response dsaSortBenchmark(const Request& req) {
    trainManager().clearTrace();
    int n = 500;
    if (!req.q("n").empty() && !parseInt(req.q("n"), n)) return fail(400, "n must be a number");
    if (n < 10) n = 10;
    if (n > 5000) n = 5000;
    ds::DynamicArray<ds::BenchmarkRow> rows = ds::benchmarkSorts(static_cast<size_t>(n));
    json::Writer w;
    w.beginObject().field("ok", true).field("n", n);
    w.key("rows").beginArray();
    for (const ds::BenchmarkRow& r : rows) {
        w.beginObject().field("algorithm", ds::sortAlgoName(r.algo));
        w.field("comparisons", r.stats.comparisons).field("moves", r.stats.moves).field("micros", r.stats.micros).field("sorted", r.sortedOk);
        w.endObject();
    }
    w.endArray().endObject();
    return Response::json(200, w.str());
}

Response resetDemo(const Request&) {
    trainManager().clearTrace();
    trainManager().seedDemoData();
    json::Writer w;
    w.beginObject().field("ok", true).field("message", "Demo data restored").field("trains", trainManager().trains().size());
    w.endObject();
    return Response::json(200, w.str());
}

}  // namespace

void registerTrainRoutes(Router& r) {
    r.addModule({"train", "Train & Route", "Member 1", "live",
                 "Trains, stations, routes, journey search and sorting. Array + linked list + hash table + sorting."});

    const std::string m = "train";
    r.add("GET", "/api/trains", listTrains, m, "List all trains. ?sort=&algo=&order=");
    r.add("GET", "/api/trains/:no", getTrain, m, "Get one train (hash table lookup)");
    r.add("POST", "/api/trains", [](const Request& q) { return saveTrain(q, false); }, m, "Add a train");
    r.add("PUT", "/api/trains/:no", [](const Request& q) { return saveTrain(q, true); }, m, "Update a train");
    r.add("DELETE", "/api/trains/:no", deleteTrain, m, "Delete a train");
    r.add("GET", "/api/stations", listStations, m, "List stations with the trains that stop there");
    r.add("POST", "/api/stations", addStation, m, "Add a station");
    r.add("DELETE", "/api/stations/:name", deleteStation, m, "Delete an unused station");
    r.add("GET", "/api/station-index", stationIndex, m, "Position of a station in a train's route. ?train=&station=");
    r.add("GET", "/api/validate", validate, m, "Validate source and destination for a train. ?train=&from=&to=");
    r.add("GET", "/api/search", search, m, "Trains between two stations. ?from=&to=&sort=&algo=&order=");
    r.add("GET", "/api/stats", stats, m, "Dashboard numbers and data structure statistics");
    r.add("GET", "/api/dsa/hash", dsaHash, m, "Hash table buckets. ?table=trains|stations|stops");
    r.add("GET", "/api/dsa/lookup", dsaLookup, m, "Linear vs binary vs hash lookup of a train. ?train=");
    r.add("GET", "/api/dsa/sort-benchmark", dsaSortBenchmark, m, "Compare the four sorts on random data. ?n=");
    r.add("POST", "/api/admin/reset", resetDemo, m, "Restore the sample trains");
}

}  // namespace api
