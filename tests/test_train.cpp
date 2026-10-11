// Unit tests for the Train & Route module (backend/train/)   (build & run: make test)
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

#include "common/contract.h"
#include "common/json.h"
#include "train/train.h"
#include "train/train_json.h"

using namespace railway;

static int g_checks = 0, g_failed = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(cond)) { ++g_failed; std::printf("  FAILED %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
    } while (0)

static StopInput stop(const char* name, const char* arr, const char* dep, int km) {
    StopInput s;
    s.station = name;
    s.distanceKm = km;
    if (arr[0]) parseTime(arr, s.arrival);
    if (dep[0]) parseTime(dep, s.departure);
    return s;
}

static TrainInput sample(int no, const char* name = "Test Express") {
    TrainInput t;
    t.trainNo = no;
    t.name = name;
    t.totalSeats = 100;
    t.farePerKm = 2.0;
    t.stops.push_back(stop("Alpha", "", "08:00", 0));
    t.stops.push_back(stop("Beta", "09:00", "09:05", 50));
    t.stops.push_back(stop("Gamma", "10:30", "", 120));
    return t;
}

static void testHelpers() {
    int m = 0;
    CHECK(parseTime("07:15", m) && m == 435);
    CHECK(parseTime("7:05", m) && m == 425);
    CHECK(!parseTime("24:00", m) && !parseTime("12:60", m) && !parseTime("1215", m) && !parseTime("ab:cd", m) && !parseTime("12:5", m));
    CHECK(formatTime(1500) == "01:00" && dayOf(1500) == 1 && formatTime(435) == "07:15");
    CHECK(cleanStationName("  nashik   road ") == "Nashik Road");
    CHECK(cleanStationName("Mumbai CSMT") == "Mumbai CSMT");
    CHECK(stationKey("  PUNE ") == "pune");
}

static void testJson() {
    json::Writer w;
    w.beginObject().field("a", 1).field("s", "he said \"hi\"\n").key("arr").beginArray().value(1).value(2.5).value(true).endArray().endObject();
    CHECK(w.str() == "{\"a\":1,\"s\":\"he said \\\"hi\\\"\\n\",\"arr\":[1,2.50,true]}");
    json::Value v;
    std::string err;
    CHECK(json::parse("{\"x\": [1, 2, {\"y\": \"z\\u0041\"}], \"t\": true, \"n\": null}", v, err));
    CHECK(v.get("x") && v.get("x")->items.size() == 3);
    CHECK(v.get("x")->items[2].get("y")->string == "zA");
    CHECK(v.get("t")->boolean && v.get("n")->isNull() && v.get("missing") == nullptr);
    CHECK(!json::parse("{\"x\": ", v, err) && !err.empty());
    CHECK(!json::parse("[1,2,]", v, err));
}

static void testAddSearchDelete() {
    TrainManager m;
    CHECK(m.addTrain(sample(100)).ok);
    CHECK(m.addTrain(sample(200, "Second")).ok);
    CHECK(m.trains().size() == 2);
    Train* t = m.searchTrain(100);
    CHECK(t && t->name == "Test Express" && t->stationCount() == 3);
    CHECK(m.searchTrain(999) == nullptr);
    CHECK(t->durationMin() == 150 && t->totalDistance() == 120 && t->totalFare() == 240);
    CHECK(t->fareBetween(1, 2) == 140);

    Status dup = m.addTrain(sample(100));
    CHECK(!dup.ok && dup.code == 409);
    CHECK(m.trains().size() == 2 && m.stations().count() == 3);

    CHECK(m.findStationIndex(100, "beta") == 1);
    CHECK(m.findStationIndex(100, "Nowhere") == -1);
    CHECK(m.findStationIndex(555, "Alpha") == -1);

    Status del = m.deleteTrain(100);
    CHECK(del.ok && m.trains().size() == 1 && m.searchTrain(100) == nullptr);
    CHECK(m.deleteTrain(100).code == 404);
    // inverted index must forget the deleted train
    int alpha = m.stations().find("Alpha");
    CHECK(alpha >= 0 && m.stations().trainsAt(alpha).size() == 1 && m.stations().trainsAt(alpha)[0] == 200);
}

static void testValidation() {
    TrainManager m;
    TrainInput t = sample(1);
    t.trainNo = 0;                    CHECK(!m.addTrain(t).ok);
    t = sample(1); t.trainNo = 100000; CHECK(!m.addTrain(t).ok);
    t = sample(1); t.name = "   ";     CHECK(!m.addTrain(t).ok);
    t = sample(1); t.totalSeats = 0;   CHECK(!m.addTrain(t).ok);
    t = sample(1); t.farePerKm = 0;    CHECK(!m.addTrain(t).ok);
    t = sample(1); t.stops.removeAt(2); t.stops.removeAt(1); CHECK(!m.addTrain(t).ok);     // only one stop
    t = sample(1); t.stops[1].station = "alpha"; CHECK(!m.addTrain(t).ok);                // duplicate station
    t = sample(1); t.stops[1].distanceKm = 0;   CHECK(!m.addTrain(t).ok);                 // distance not increasing
    t = sample(1); t.stops[0].departure = -1;   CHECK(!m.addTrain(t).ok);                 // first stop needs departure
    t = sample(1); t.stops[2].arrival = -1;     CHECK(!m.addTrain(t).ok);                 // last stop needs arrival
    t = sample(1); t.stops[1].departure = 9 * 60 + 5 + 4 * 60; CHECK(!m.addTrain(t).ok); // 4h dwell
    CHECK(m.trains().size() == 0 && m.stations().count() == 0);   // failed adds leave no trace

    // station spelling is unified with the registry
    CHECK(m.addTrain(sample(2)).ok);
    TrainInput u = sample(3);
    u.stops[0].station = "ALPHA";     // different case, same station
    CHECK(m.addTrain(u).ok);
    CHECK(m.stations().count() == 3);
}

static void testOvernight() {
    TrainManager m;
    TrainInput t;
    t.trainNo = 7; t.name = "Night"; t.totalSeats = 10; t.farePerKm = 1;
    t.stops.push_back(stop("A", "", "22:00", 0));
    t.stops.push_back(stop("B", "23:50", "00:10", 100));   // departs after midnight
    t.stops.push_back(stop("C", "06:30", "", 300));
    CHECK(m.addTrain(t).ok);
    Train* tr = m.searchTrain(7);
    CHECK(tr->stop(1).arrival == 22 * 60 + 110 && tr->stop(1).departure == 24 * 60 + 10);
    CHECK(dayOf(tr->stop(1).departure) == 1 && dayOf(tr->lastArrival()) == 1);
    CHECK(tr->durationMin() == 8 * 60 + 30);
}

static void testUpdate() {
    TrainManager m;
    m.addTrain(sample(100));
    TrainInput u = sample(100, "Renamed");
    u.stops.push_back(stop("Delta", "11:30", "", 200));
    u.stops[2].departure = 10 * 60 + 40;
    CHECK(m.updateTrain(100, u).ok);
    CHECK(m.searchTrain(100)->name == "Renamed" && m.searchTrain(100)->stationCount() == 4);
    CHECK(m.trains().size() == 1);
    CHECK(m.stations().find("Delta") >= 0 && m.stations().trainsAt(m.stations().find("Delta")).size() == 1);
    TrainInput bad = sample(101);
    CHECK(m.updateTrain(100, bad).code == 400);   // number change refused
    CHECK(m.updateTrain(555, sample(555)).code == 404);
    TrainInput shrink = sample(100);              // drop Beta from the route
    shrink.stops.removeAt(1);
    CHECK(m.updateTrain(100, shrink).ok);
    CHECK(m.stations().trainsAt(m.stations().find("Beta")).size() == 0);
    CHECK(m.deleteStation("Beta").ok);            // now unused, so it can go
    CHECK(m.deleteStation("Alpha").code == 409);  // still on a route
}

static void testJourneys() {
    TrainManager m;
    m.seedDemoData();
    CHECK(m.trains().size() == 8);

    Journey ok = m.validateJourney(12001, "pune", "MUMBAI");
    CHECK(ok.ok && ok.match.srcIdx == 0 && ok.match.dstIdx == 4 && ok.match.fare == 269);
    CHECK(!m.validateJourney(12001, "Mumbai", "Pune").ok);
    CHECK(m.validateJourney(12001, "Pune", "Delhi").code == 422);
    CHECK(m.validateJourney(12001, "Pune", "Atlantis").code == 404);
    CHECK(m.validateJourney(12001, "Pune", "pune").code == 400);
    CHECK(m.validateJourney(1, "Pune", "Mumbai").code == 404);

    SearchResult r = m.findTrainsBetween("Pune", "Mumbai", SortField::Duration, ds::SortAlgo::Merge, false);
    CHECK(r.ok && r.matches.size() == 4);
    for (size_t i = 1; i < r.matches.size(); ++i) CHECK(r.matches[i - 1].durationMin <= r.matches[i].durationMin);
    // direction matters: 12002 runs Mumbai -> Pune only
    SearchResult back = m.findTrainsBetween("Mumbai", "Pune", SortField::Departure, ds::SortAlgo::Quick, false);
    CHECK(back.ok && back.matches.size() == 2);   // 12002 and 11301
    bool has12002 = false;
    for (const Match& x : back.matches) if (x.train->trainNo == 12002) has12002 = true;
    CHECK(has12002);
    CHECK(!m.findTrainsBetween("Pune", "Nowhere", SortField::Fare, ds::SortAlgo::Heap, true).ok);
    CHECK(m.findTrainsBetween("Nagpur", "Pune", SortField::Fare, ds::SortAlgo::Heap, true).matches.size() == 0);

    // every algorithm and both orders agree, thanks to the train-number tie-break
    ds::SortAlgo algos[] = {ds::SortAlgo::Merge, ds::SortAlgo::Quick, ds::SortAlgo::Insertion, ds::SortAlgo::Heap};
    SortField fields[] = {SortField::TrainNo, SortField::Name, SortField::Fare, SortField::Departure, SortField::Duration, SortField::Stops, SortField::Distance};
    for (SortField f : fields)
        for (bool desc : {false, true}) {
            ds::DynamicArray<Train*> ref = m.sortedTrains(f, ds::SortAlgo::Merge, desc, nullptr);
            for (ds::SortAlgo a : algos) {
                ds::DynamicArray<Train*> other = m.sortedTrains(f, a, desc, nullptr);
                bool same = other.size() == ref.size();
                for (size_t i = 0; same && i < ref.size(); ++i) same = other[i] == ref[i];
                CHECK(same);
            }
        }
    ds::DynamicArray<Train*> byFare = m.sortedTrains(SortField::Fare, ds::SortAlgo::Quick, true, nullptr);
    CHECK(byFare[0]->trainNo == 12951);   // most expensive route

    LookupReport lr = m.lookupCompare(12123);
    CHECK(lr.found && lr.linearSteps == 4 && lr.hashProbes >= 1 && lr.binaryProbes.size() >= 1);
    CHECK(!m.lookupCompare(424242).found);
}

static void testPersistence() {
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / "smartrailway_test";
    fs::remove_all(dir);
    std::string file = (dir / "trains.json").string();
    {
        TrainManager a;
        a.setStorePath(file);
        a.seedDemoData();
        CHECK(a.addStation("Goa").ok);
        CHECK(a.deleteTrain(12010).ok);
    }
    TrainManager b;
    b.setStorePath(file);
    std::string err;
    CHECK(b.load(&err) && err.empty());
    CHECK(b.trains().size() == 7 && b.searchTrain(12010) == nullptr);
    CHECK(b.searchTrain(11023)->lastArrival() == 24 * 60 + 5 * 60 + 40);   // overnight times survive the round trip
    CHECK(b.stations().find("Goa") >= 0);
    CHECK(b.trains()[0]->trainNo == 12001);                                 // insertion order preserved
    TrainManager c;
    c.setStorePath((dir / "missing.json").string());
    CHECK(!c.load(&err));
    fs::remove_all(dir);
}

static void testContract() {
    trainManager().seedDemoData();
    TrainInfo info;
    CHECK(searchTrain(12001, info) && std::strcmp(info.trainName, "Deccan Express") == 0 && info.totalSeats == 120 && info.stationCount == 5);
    CHECK(!searchTrain(1, info));
    CHECK(findStationIndex(12001, "Karjat") == 2 && findStationIndex(12001, "Delhi") == -1);
    int found[10];
    int n = findTrainsBetween("Pune", "Mumbai", found, 10);
    CHECK(n == 4);
    int capped[2];
    CHECK(findTrainsBetween("Pune", "Mumbai", capped, 2) == 2);
    CHECK(fareBetween(12001, 0, 4) == 269 && fareBetween(12001, 3, 1) == -1 && fareBetween(1, 0, 1) == -1);
    CHECK(segmentCount(12001) == 4 && segmentCount(1) == 0);
}

int main() {
    testHelpers();
    testJson();
    testAddSearchDelete();
    testValidation();
    testOvernight();
    testUpdate();
    testJourneys();
    testPersistence();
    testContract();
    std::printf("test_train: %d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
