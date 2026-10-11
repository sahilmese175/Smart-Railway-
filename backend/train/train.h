// train/train.h
// -----------------------------------------------------------------------------
// MEMBER 1 : Train, Station & Route module
//
// Data structures used (all hand-written, see backend/ds/):
//   * ds::LinkedList<Stop>               the ordered route of one train
//   * ds::DynamicArray<Stop*>            random-access view of that route
//   * ds::HashTable<string,int>          station name -> index inside one train
//   * ds::HashTable<int,Train*>          train number -> train      (O(1) search)
//   * ds::DynamicArray<Train*>           every train in insertion order
//   * StationRegistry                    hash table + array + inverted index
//   * ds::mergeSort/quickSort/...        sorting trains and search results
// -----------------------------------------------------------------------------
#pragma once
#include <string>

#include "ds/dynamic_array.h"
#include "ds/hash_table.h"
#include "ds/linked_list.h"
#include "ds/sorting.h"

namespace railway {

// ---- helpers ------------------------------------------------------------------------
std::string cleanStationName(const std::string& raw);   // trim, collapse spaces, tidy case
std::string stationKey(const std::string& raw);         // lower-case lookup key
bool parseTime(const std::string& text, int& minutesOfDay);  // "07:15" -> 435
std::string formatTime(int absoluteMinutes);            // 1500 -> "01:00" (day is separate)
inline int dayOf(int absoluteMinutes) { return absoluteMinutes / 1440; }

// ---- plain records --------------------------------------------------------------------
struct Stop {
    std::string station;
    int arrival = 0;      // absolute minutes since 00:00 of the departure day (can exceed 1440)
    int departure = 0;
    int distanceKm = 0;   // cumulative distance from the first station
};

// What the API / UI sends when creating or editing a train (times are minutes of day, -1 = blank)
struct StopInput {
    std::string station;
    int arrival = -1;
    int departure = -1;
    int distanceKm = 0;
};
struct TrainInput {
    int trainNo = 0;
    std::string name;
    int totalSeats = 0;
    double farePerKm = 0;
    ds::DynamicArray<StopInput> stops;
};

// ---- Train ----------------------------------------------------------------------------
class Train {
public:
    int trainNo = 0;
    std::string name;
    int totalSeats = 0;
    double farePerKm = 0;

    ds::LinkedList<Stop> route;                     // the real route (linked list)
    ds::DynamicArray<Stop*> stopArr;                // O(1) positional access into the list
    ds::HashTable<std::string, int> stopIndex;      // station key -> position

    Train() = default;
    Train(const Train&) = delete;
    Train& operator=(const Train&) = delete;

    void rebuildViews();                            // call after the route changes
    int stationCount() const { return static_cast<int>(stopArr.size()); }
    const Stop& stop(int i) const { return *stopArr[static_cast<size_t>(i)]; }
    int indexOf(const std::string& key) const;      // -1 if the train does not stop there
    int firstDeparture() const { return stop(0).departure; }
    int lastArrival() const { return stop(stationCount() - 1).arrival; }
    int durationMin() const { return lastArrival() - firstDeparture(); }
    int totalDistance() const { return stop(stationCount() - 1).distanceKm; }
    long fareBetween(int i, int j) const;           // whole rupees
    long totalFare() const { return fareBetween(0, stationCount() - 1); }
};

// ---- StationRegistry ----------------------------------------------------------------------
class StationRegistry {
public:
    int find(const std::string& name) const;                 // id or -1
    int ensure(const std::string& cleanName);                // find or create, returns id
    bool alive(int id) const { return id >= 0 && static_cast<size_t>(id) < alive_.size() && alive_[static_cast<size_t>(id)]; }
    const std::string& name(int id) const { return names_[static_cast<size_t>(id)]; }
    size_t count() const { return idByKey_.size(); }
    size_t idSlots() const { return names_.size(); }
    ds::DynamicArray<int>& trainsAt(int id) { return trainsAt_[static_cast<size_t>(id)]; }
    const ds::DynamicArray<int>& trainsAt(int id) const { return trainsAt_[static_cast<size_t>(id)]; }
    bool remove(int id);                                     // caller checks trainsAt is empty
    const ds::HashTable<std::string, int>& table() const { return idByKey_; }

private:
    ds::HashTable<std::string, int> idByKey_;
    ds::DynamicArray<std::string> names_;
    ds::DynamicArray<char> alive_;
    ds::DynamicArray<ds::DynamicArray<int>> trainsAt_;       // inverted index: station -> train numbers
};

// ---- query results ------------------------------------------------------------------------
enum class SortField { TrainNo, Name, Fare, Departure, Arrival, Duration, Stops, Distance };
bool parseSortField(const std::string& s, SortField& out);

struct Match {                      // one train that can carry you from A to B
    Train* train = nullptr;
    int srcIdx = -1, dstIdx = -1;
    int departure = 0, arrival = 0, durationMin = 0, distanceKm = 0;
    long fare = 0;
};

struct Status { bool ok = true; int code = 200; std::string message; };

struct Journey {                    // result of validating "train N from A to B"
    bool ok = false;
    int code = 200;
    std::string message;
    Match match;
};

struct SearchResult {
    bool ok = false;
    int code = 200;
    std::string message;
    std::string from, to;           // canonical names
    size_t candidates = 0;          // trains that stop at the source
    ds::DynamicArray<Match> matches;
    ds::SortStats stats;
};

struct LookupReport {               // "how would each structure find train N?"
    int trainNo = 0;
    bool found = false;
    size_t linearSteps = 0;
    ds::DynamicArray<size_t> binaryProbes;   // positions probed in the sorted array
    ds::DynamicArray<int> insertionOrder;    // train numbers as stored (what linear search walks)
    ds::DynamicArray<int> sortedOrder;       // train numbers sorted (what binary search walks)
    size_t arraySize = 0;
    size_t hashBucket = 0, hashProbes = 0, hashBuckets = 0;
};

// ---- TrainManager -----------------------------------------------------------------------------
class TrainManager {
public:
    TrainManager();
    ~TrainManager();
    TrainManager(const TrainManager&) = delete;
    TrainManager& operator=(const TrainManager&) = delete;

    // CRUD -----------------------------------------------------------------------------------
    Status addTrain(const TrainInput& in);
    Status updateTrain(int trainNo, const TrainInput& in);
    Status deleteTrain(int trainNo);
    Train* searchTrain(int trainNo);                       // O(1) via hash table

    // Stations ----------------------------------------------------------------------------------
    Status addStation(const std::string& name);
    Status deleteStation(const std::string& name);
    int findStationIndex(int trainNo, const std::string& station);   // -1 if absent

    // Queries -----------------------------------------------------------------------------------
    Journey validateJourney(int trainNo, const std::string& from, const std::string& to);
    SearchResult findTrainsBetween(const std::string& from, const std::string& to,
                                   SortField field, ds::SortAlgo algo, bool descending);
    ds::DynamicArray<Train*> sortedTrains(SortField field, ds::SortAlgo algo, bool descending, ds::SortStats* stats);
    LookupReport lookupCompare(int trainNo);

    // Read-only views (for stats and the DSA Lab) ----------------------------------------------------
    const ds::DynamicArray<Train*>& trains() const { return all_; }
    const ds::HashTable<int, Train*>& trainTable() const { return byNo_; }
    const StationRegistry& stations() const { return registry_; }

    // Persistence --------------------------------------------------------------------------------------
    void setStorePath(const std::string& path) { storePath_ = path; }
    bool save(std::string* err = nullptr) const;
    bool load(std::string* err = nullptr);                 // false if file missing or invalid
    void clearAll();
    void seedDemoData();                                   // defined in seed_data.cpp
    void setAutoSave(bool on) { autoSave_ = on; }

    // "What did the data structures just do?" log, shown in the UI console -----------------------------
    void clearTrace() { trace_.clear(); }
    const ds::DynamicArray<std::string>& trace() const { return trace_; }

private:
    ds::DynamicArray<Train*> all_;                         // insertion order
    ds::HashTable<int, Train*> byNo_;                      // train number -> train
    StationRegistry registry_;
    std::string storePath_;
    bool autoSave_ = true;
    ds::DynamicArray<std::string> trace_;

    void note(const std::string& line) { trace_.push_back(line); }
    void persist();
    bool build(const TrainInput& in, Train*& out, Status& st);      // validate + create (not yet indexed)
    void indexTrain(Train* t);
    void unindexTrain(Train* t);
    long positionInAll(int trainNo) const;
};

// The process-wide manager used by the REST API and the contract functions.
TrainManager& trainManager();

}  // namespace railway
