// ===== Member 1 module: Train & Route Management =====
// Public interface for the rest of the team:
//   searchTrain(no)                  -> Train* or nullptr
//   findStationIndex(no, station)    -> index of station on that train's route, -1 if absent
//   findTrainsBetween(src, dst, out) -> trains that run src -> dst (in that order)
// Segment i of a train = stop i -> stop i+1 (this is what Member 3's Segment Tree indexes).
#pragma once
#include <string>
#include "../ds/dynarray.h"
#include "../ds/linkedlist.h"
#include "../ds/hashtable.h"

struct Stop {
    std::string station, arrival, departure;   // "HH:MM"; first stop has no arrival, last has no departure
    int fare = 0;                              // cumulative fare from the first stop
};

struct Train {
    int trainNo = 0;
    std::string name;
    int totalSeats = 0;
    LinkedList<Stop> route;

    int stopIndex(const std::string& station) const;   // case-insensitive, O(stops)
    int totalFare() const;
    std::string departureTime() const;
};

struct Match {                                  // one search result
    const Train* train;
    int from, to, fare, duration;               // duration in minutes
    std::string departure, arrival;
};

struct StationEntry {                           // value stored in the station hash table
    std::string display;
    DynArray<int> trains;
};

std::string toLower(std::string s);
int parseMinutes(const std::string& hhmm);      // -1 if invalid

class TrainManager {
    HashTable<int, Train*> trains_;                    // trainNo -> Train          (HASH TABLE)
    HashTable<std::string, StationEntry> stations_;    // station -> trains at it   (HASH TABLE, inverted index)
    DynArray<Train*> order_;                           // insertion order           (ARRAY)

    static bool validate(int no, const std::string& name, int seats, const DynArray<Stop>& stops, std::string& err);
    void indexTrain(const Train* t);
    void unindexTrain(const Train* t);
public:
    ~TrainManager();
    bool addTrain(int no, const std::string& name, int seats, const DynArray<Stop>& stops, std::string& err);
    bool updateTrain(int no, const std::string& name, int seats, const DynArray<Stop>& stops, std::string& err);
    bool deleteTrain(int no);

    Train* searchTrain(int no) const;
    int findStationIndex(int no, const std::string& station) const;
    void findTrainsBetween(const std::string& src, const std::string& dst, DynArray<Match>& out) const;

    DynArray<Train*> sortedTrains(const std::string& key, bool asc) const;       // number|name|fare|departure|stops|seats
    static void sortMatches(DynArray<Match>& m, const std::string& key, bool asc); // departure|fare|duration|number
    DynArray<StationEntry> allStations() const;                                  // alphabetical

    const HashTable<int, Train*>& trainTable() const { return trains_; }
    const HashTable<std::string, StationEntry>& stationTable() const { return stations_; }
    size_t count() const { return order_.size(); }

    bool save(const std::string& path) const;
    bool load(const std::string& path);          // line: no|name|seats|station~arr~dep~fare;station~...
};
