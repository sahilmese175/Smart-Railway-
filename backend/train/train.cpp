#include "train/train.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "ds/searching.h"
#include "train/train_json.h"

namespace railway {

// ============================================================================
//  helpers
// ============================================================================
static std::string squeeze(const std::string& raw) {
    std::string s;
    bool pendingSpace = false;
    for (unsigned char c : raw) {
        if (std::isspace(c)) { pendingSpace = !s.empty(); continue; }
        if (pendingSpace) { s += ' '; pendingSpace = false; }
        s += static_cast<char>(c);
    }
    return s;
}

std::string cleanStationName(const std::string& raw) {
    std::string s = squeeze(raw);
    bool hasLower = false, hasUpper = false;
    for (unsigned char c : s) { if (std::islower(c)) hasLower = true; if (std::isupper(c)) hasUpper = true; }
    if (hasLower && !hasUpper) {  // "pune junction" -> "Pune Junction"
        bool start = true;
        for (char& c : s) {
            if (start && std::isalpha(static_cast<unsigned char>(c))) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            start = (c == ' ' || c == '-');
        }
    }
    return s;
}

std::string stationKey(const std::string& raw) {
    std::string s = squeeze(raw);
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool parseTime(const std::string& text, int& minutesOfDay) {
    size_t colon = text.find(':');
    if (colon == std::string::npos || colon == 0 || colon > 2 || text.size() - colon - 1 != 2) return false;
    int h = 0, m = 0;
    for (size_t i = 0; i < colon; ++i) { if (!std::isdigit(static_cast<unsigned char>(text[i]))) return false; h = h * 10 + (text[i] - '0'); }
    for (size_t i = colon + 1; i < text.size(); ++i) { if (!std::isdigit(static_cast<unsigned char>(text[i]))) return false; m = m * 10 + (text[i] - '0'); }
    if (h > 23 || m > 59) return false;
    minutesOfDay = h * 60 + m;
    return true;
}

std::string formatTime(int absoluteMinutes) {
    int t = ((absoluteMinutes % 1440) + 1440) % 1440;
    char buf[8];
    std::snprintf(buf, sizeof buf, "%02d:%02d", t / 60, t % 60);
    return buf;
}

static std::string fmt1(double d) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.2f", d);
    return buf;
}

// ============================================================================
//  Train
// ============================================================================
void Train::rebuildViews() {
    stopArr.clear();
    stopIndex = ds::HashTable<std::string, int>();
    int i = 0;
    for (auto* n = route.headNode(); n; n = n->next, ++i) {
        stopArr.push_back(&n->value);
        stopIndex.put(stationKey(n->value.station), i);
    }
}

int Train::indexOf(const std::string& key) const {
    const int* p = stopIndex.get(key);
    return p ? *p : -1;
}

long Train::fareBetween(int i, int j) const {
    return std::lround((stop(j).distanceKm - stop(i).distanceKm) * farePerKm);
}

// ============================================================================
//  StationRegistry
// ============================================================================
int StationRegistry::find(const std::string& name) const {
    const int* p = idByKey_.get(stationKey(name));
    return p ? *p : -1;
}

int StationRegistry::ensure(const std::string& cleanName) {
    int existing = find(cleanName);
    if (existing >= 0) return existing;
    int id = static_cast<int>(names_.size());
    names_.push_back(cleanName);
    alive_.push_back(1);
    trainsAt_.push_back(ds::DynamicArray<int>());
    idByKey_.put(stationKey(cleanName), id);
    return id;
}

bool StationRegistry::remove(int id) {
    if (!alive(id)) return false;
    alive_[static_cast<size_t>(id)] = 0;
    idByKey_.remove(stationKey(names_[static_cast<size_t>(id)]));
    trainsAt_[static_cast<size_t>(id)].clear();
    return true;
}

// ============================================================================
//  sorting helpers
// ============================================================================
bool parseSortField(const std::string& s, SortField& out) {
    if (s == "trainNo") out = SortField::TrainNo;
    else if (s == "name") out = SortField::Name;
    else if (s == "fare") out = SortField::Fare;
    else if (s == "departure") out = SortField::Departure;
    else if (s == "arrival") out = SortField::Arrival;
    else if (s == "duration") out = SortField::Duration;
    else if (s == "stops") out = SortField::Stops;
    else if (s == "distance") out = SortField::Distance;
    else return false;
    return true;
}

static int cmpNoCase(const std::string& a, const std::string& b) {
    size_t n = a.size() < b.size() ? a.size() : b.size();
    for (size_t i = 0; i < n; ++i) {
        int x = std::tolower(static_cast<unsigned char>(a[i])), y = std::tolower(static_cast<unsigned char>(b[i]));
        if (x != y) return x < y ? -1 : 1;
    }
    return a.size() == b.size() ? 0 : (a.size() < b.size() ? -1 : 1);
}

static double numKey(const Train& t, SortField f) {
    switch (f) {
        case SortField::TrainNo: return t.trainNo;
        case SortField::Fare: return static_cast<double>(t.totalFare());
        case SortField::Departure: return t.firstDeparture();
        case SortField::Arrival: return t.lastArrival();
        case SortField::Duration: return t.durationMin();
        case SortField::Stops: return t.stationCount();
        case SortField::Distance: return t.totalDistance();
        default: return 0;
    }
}

static double numKey(const Match& m, SortField f) {
    switch (f) {
        case SortField::TrainNo: return m.train->trainNo;
        case SortField::Fare: return static_cast<double>(m.fare);
        case SortField::Departure: return m.departure;
        case SortField::Arrival: return m.arrival;
        case SortField::Duration: return m.durationMin;
        case SortField::Stops: return m.dstIdx - m.srcIdx;
        case SortField::Distance: return m.distanceKm;
        default: return 0;
    }
}

static Match makeMatch(Train* t, int si, int di) {
    Match m;
    m.train = t; m.srcIdx = si; m.dstIdx = di;
    m.departure = t->stop(si).departure;
    m.arrival = t->stop(di).arrival;
    m.durationMin = m.arrival - m.departure;
    m.distanceKm = t->stop(di).distanceKm - t->stop(si).distanceKm;
    m.fare = t->fareBetween(si, di);
    return m;
}

// ============================================================================
//  TrainManager : construction / validation
// ============================================================================
TrainManager::TrainManager() = default;

TrainManager::~TrainManager() {
    for (Train* t : all_) delete t;
}

void TrainManager::clearAll() {
    for (Train* t : all_) delete t;
    all_.clear();
    byNo_.clear();
    registry_ = StationRegistry();
}

bool TrainManager::build(const TrainInput& in, Train*& out, Status& st) {
    out = nullptr;
    auto fail = [&](int code, const std::string& msg) {
        st.ok = false; st.code = code; st.message = msg;
        return false;
    };
    if (in.trainNo < 1 || in.trainNo > 99999) return fail(400, "Train number must be between 1 and 99999");
    std::string name = squeeze(in.name);
    if (name.empty()) return fail(400, "Train name is required");
    if (name.size() > 49) return fail(400, "Train name can be at most 49 characters");
    if (in.totalSeats < 1 || in.totalSeats > 2000) return fail(400, "Total seats must be between 1 and 2000");
    if (!(in.farePerKm > 0 && in.farePerKm <= 100)) return fail(400, "Fare per km must be more than 0 and at most 100");
    size_t n = in.stops.size();
    if (n < 2) return fail(400, "A route needs at least 2 stops (source and destination)");
    if (n > 40) return fail(400, "A route can have at most 40 stops");

    // validate stops with a temporary hash table (duplicate detection in O(1) each)
    ds::HashTable<std::string, int> seen;
    ds::DynamicArray<Stop> stops;
    int base = in.stops[0].distanceKm;
    int prevDist = -1, day = 0, last = -1, firstTime = 0;
    for (size_t i = 0; i < n; ++i) {
        const StopInput& si = in.stops[i];
        std::string label = "Stop " + std::to_string(i + 1);
        std::string station = cleanStationName(si.station);
        if (station.empty()) return fail(400, label + ": station name is required");
        if (station.size() > 40) return fail(400, label + ": station name can be at most 40 characters");
        if (!seen.put(stationKey(station), static_cast<int>(i))) return fail(400, "Station '" + station + "' appears twice in this route");
        int known = registry_.find(station);
        if (known >= 0) station = registry_.name(known);   // reuse the canonical spelling

        int dist = si.distanceKm - base;
        if (dist < 0 || dist > 20000) return fail(400, label + " (" + station + "): distance must be between 0 and 20000 km");
        if (i > 0 && dist <= prevDist) return fail(400, label + " (" + station + "): distance must be larger than the previous stop");
        prevDist = dist;

        int a = si.arrival, d = si.departure;
        if (i == 0) {
            if (d < 0) return fail(400, label + " (" + station + "): the first stop needs a departure time");
            a = d;
        } else if (i == n - 1) {
            if (a < 0) return fail(400, label + " (" + station + "): the last stop needs an arrival time");
            d = a;
        } else {
            if (a < 0 && d < 0) return fail(400, label + " (" + station + "): needs an arrival or a departure time");
            if (a < 0) a = d;
            if (d < 0) d = a;
        }
        // unroll clock times into absolute minutes: a time earlier than the previous one means "next day"
        auto place = [&](int t) {
            int v = t + 1440 * day;
            while (v < last) { ++day; v += 1440; }
            last = v;
            return v;
        };
        Stop s;
        s.station = station;
        s.distanceKm = dist;
        s.arrival = place(a);
        s.departure = (i == 0) ? s.arrival : place(d);
        if (i == 0) firstTime = s.arrival;
        if (s.departure - s.arrival > 180) return fail(400, label + " (" + station + "): departure is more than 3 hours after arrival, please check the times");
        if (s.departure - firstTime > 5 * 1440) return fail(400, "The journey is longer than 5 days, please check the times");
        stops.push_back(s);
    }

    Train* t = new Train();
    t->trainNo = in.trainNo;
    t->name = name;
    t->totalSeats = in.totalSeats;
    t->farePerKm = in.farePerKm;
    for (const Stop& s : stops) t->route.push_back(s);
    t->rebuildViews();
    note("LinkedList<Stop>.push_back x" + std::to_string(n) + "  (route built node by node)");
    note("DynamicArray<Stop*> rebuilt  (O(1) access to any stop by index)");
    note("HashTable<string,int> stopIndex rebuilt  (" + std::to_string(n) + " keys, load " + fmt1(t->stopIndex.loadFactor()) + ")");
    out = t;
    return true;
}

long TrainManager::positionInAll(int trainNo) const {
    return ds::linearSearch(all_, [&](Train* t) { return t->trainNo == trainNo; });
}

void TrainManager::indexTrain(Train* t) {
    size_t before = registry_.idSlots();
    for (int i = 0; i < t->stationCount(); ++i) {
        int id = registry_.ensure(t->stop(i).station);
        registry_.trainsAt(id).push_back(t->trainNo);
    }
    size_t created = registry_.idSlots() - before;
    note("Inverted index: trainsAt[station].push_back(" + std::to_string(t->trainNo) + ") x" + std::to_string(t->stationCount()) +
         (created ? "  (" + std::to_string(created) + " new station(s) registered)" : ""));
}

void TrainManager::unindexTrain(Train* t) {
    for (int i = 0; i < t->stationCount(); ++i) {
        int id = registry_.find(t->stop(i).station);
        if (id >= 0) registry_.trainsAt(id).removeValue(t->trainNo);
    }
    note("Inverted index: removed train " + std::to_string(t->trainNo) + " from " + std::to_string(t->stationCount()) + " station lists");
}

void TrainManager::persist() {
    if (!autoSave_ || storePath_.empty()) return;
    std::string err;
    if (!save(&err)) note("WARNING: could not save data file: " + err);
    else note("Saved to disk");
}

// ============================================================================
//  CRUD
// ============================================================================
Train* TrainManager::searchTrain(int trainNo) {
    Train** p = byNo_.get(trainNo);
    note("HashTable<int,Train*>.get(" + std::to_string(trainNo) + ") -> bucket " + std::to_string(byNo_.lastBucket()) + "/" +
         std::to_string(byNo_.bucketCount()) + ", " + std::to_string(byNo_.lastProbes()) + " node(s) visited" + (p ? "" : ", not found"));
    return p ? *p : nullptr;
}

Status TrainManager::addTrain(const TrainInput& in) {
    Status st;
    Train* t = nullptr;
    if (!build(in, t, st)) return st;
    if (byNo_.get(t->trainNo)) {
        note("HashTable.get(" + std::to_string(t->trainNo) + ") found an existing train -> rejected");
        int no = t->trainNo;
        delete t;
        return Status{false, 409, "Train " + std::to_string(no) + " already exists"};
    }
    size_t rehashes = byNo_.rehashCount();
    byNo_.put(t->trainNo, t);
    note("HashTable<int,Train*>.put(" + std::to_string(t->trainNo) + ") -> bucket " + std::to_string(byNo_.bucketOf(t->trainNo)) +
         ", load factor " + fmt1(byNo_.loadFactor()) + (byNo_.rehashCount() > rehashes ? "  ** REHASH to " + std::to_string(byNo_.bucketCount()) + " buckets **" : ""));
    size_t oldCap = all_.capacity();
    all_.push_back(t);
    note("DynamicArray<Train*>.push_back -> size " + std::to_string(all_.size()) + ", capacity " + std::to_string(all_.capacity()) +
         (all_.capacity() != oldCap ? "  ** array doubled **" : ""));
    indexTrain(t);
    persist();
    return Status{true, 201, "Train " + std::to_string(t->trainNo) + " added"};
}

Status TrainManager::updateTrain(int trainNo, const TrainInput& in) {
    Train* old = searchTrain(trainNo);
    if (!old) return Status{false, 404, "Train " + std::to_string(trainNo) + " not found"};
    if (in.trainNo != 0 && in.trainNo != trainNo)
        return Status{false, 400, "The train number cannot be changed. Delete the train and add a new one instead."};
    TrainInput copy = in;
    copy.trainNo = trainNo;
    Status st;
    Train* fresh = nullptr;
    if (!build(copy, fresh, st)) return st;
    unindexTrain(old);
    long pos = positionInAll(trainNo);
    byNo_.put(trainNo, fresh);
    note("HashTable.put(" + std::to_string(trainNo) + ") overwrote the old pointer (key already present)");
    if (pos >= 0) all_[static_cast<size_t>(pos)] = fresh;
    delete old;
    indexTrain(fresh);
    persist();
    return Status{true, 200, "Train " + std::to_string(trainNo) + " updated"};
}

Status TrainManager::deleteTrain(int trainNo) {
    Train* t = searchTrain(trainNo);
    if (!t) return Status{false, 404, "Train " + std::to_string(trainNo) + " not found"};
    unindexTrain(t);
    byNo_.remove(trainNo);
    long pos = positionInAll(trainNo);
    note("DynamicArray<Train*> linear scan found position " + std::to_string(pos) + ", removeAt shifts " +
         std::to_string(all_.size() - static_cast<size_t>(pos) - 1) + " element(s) left");
    if (pos >= 0) all_.removeAt(static_cast<size_t>(pos));
    delete t;
    persist();
    return Status{true, 200, "Train " + std::to_string(trainNo) + " deleted"};
}

// ============================================================================
//  stations
// ============================================================================
Status TrainManager::addStation(const std::string& name) {
    std::string clean = cleanStationName(name);
    if (clean.empty()) return Status{false, 400, "Station name is required"};
    if (clean.size() > 40) return Status{false, 400, "Station name can be at most 40 characters"};
    if (registry_.find(clean) >= 0) {
        note("HashTable<string,int>.get(\"" + stationKey(clean) + "\") found an existing station");
        return Status{false, 409, "Station '" + clean + "' already exists"};
    }
    int id = registry_.ensure(clean);
    note("StationRegistry.ensure -> id " + std::to_string(id) + ", HashTable bucket " +
         std::to_string(registry_.table().bucketOf(stationKey(clean))) + ", DynamicArray names.push_back");
    persist();
    return Status{true, 201, "Station '" + clean + "' added"};
}

Status TrainManager::deleteStation(const std::string& name) {
    int id = registry_.find(name);
    if (id < 0) return Status{false, 404, "Station '" + squeeze(name) + "' not found"};
    size_t used = registry_.trainsAt(id).size();
    if (used > 0)
        return Status{false, 409, "'" + registry_.name(id) + "' is on the route of " + std::to_string(used) + " train(s). Remove it from those routes first."};
    std::string nm = registry_.name(id);
    registry_.remove(id);
    note("StationRegistry.remove(" + nm + ") -> HashTable.remove, slot marked free");
    persist();
    return Status{true, 200, "Station '" + nm + "' deleted"};
}

int TrainManager::findStationIndex(int trainNo, const std::string& station) {
    Train* t = searchTrain(trainNo);
    if (!t) return -1;
    int idx = t->indexOf(stationKey(station));
    note("Train.stopIndex (HashTable<string,int>).get(\"" + stationKey(station) + "\") -> " + (idx < 0 ? "not on route" : "index " + std::to_string(idx)));
    return idx;
}

// ============================================================================
//  journey queries
// ============================================================================
Journey TrainManager::validateJourney(int trainNo, const std::string& from, const std::string& to) {
    Journey j;
    auto fail = [&](int code, const std::string& msg) { j.ok = false; j.code = code; j.message = msg; return j; };
    Train* t = searchTrain(trainNo);
    if (!t) return fail(404, "Train " + std::to_string(trainNo) + " not found");
    std::string f = squeeze(from), d = squeeze(to);
    if (f.empty() || d.empty()) return fail(400, "Choose both a source and a destination");
    if (registry_.find(f) < 0) return fail(404, "'" + f + "' is not a station in the network");
    if (registry_.find(d) < 0) return fail(404, "'" + d + "' is not a station in the network");
    if (stationKey(f) == stationKey(d)) return fail(400, "Source and destination must be different");
    int si = t->indexOf(stationKey(f)), di = t->indexOf(stationKey(d));
    note("stopIndex.get(source) -> " + std::to_string(si) + ", stopIndex.get(destination) -> " + std::to_string(di));
    if (si < 0) return fail(422, "Train " + std::to_string(trainNo) + " does not stop at " + registry_.name(registry_.find(f)));
    if (di < 0) return fail(422, "Train " + std::to_string(trainNo) + " does not stop at " + registry_.name(registry_.find(d)));
    if (si > di) return fail(422, "Train " + std::to_string(trainNo) + " runs from " + t->stop(di).station + " to " + t->stop(si).station + ", not the other way round");
    j.ok = true;
    j.match = makeMatch(t, si, di);
    j.message = "Valid journey";
    return j;
}

SearchResult TrainManager::findTrainsBetween(const std::string& from, const std::string& to, SortField field, ds::SortAlgo algo, bool desc) {
    SearchResult r;
    auto fail = [&](int code, const std::string& msg) { r.ok = false; r.code = code; r.message = msg; return r; };
    std::string f = squeeze(from), d = squeeze(to);
    if (f.empty() || d.empty()) return fail(400, "Choose both a source and a destination");
    int sid = registry_.find(f), did = registry_.find(d);
    if (sid < 0) return fail(404, "'" + f + "' is not a station in the network");
    if (did < 0) return fail(404, "'" + d + "' is not a station in the network");
    if (sid == did) return fail(400, "Source and destination must be different");
    r.from = registry_.name(sid);
    r.to = registry_.name(did);
    note("StationRegistry HashTable: \"" + stationKey(f) + "\" -> id " + std::to_string(sid) + ",  \"" + stationKey(d) + "\" -> id " + std::to_string(did));

    const ds::DynamicArray<int>& candidates = registry_.trainsAt(sid);
    r.candidates = candidates.size();
    note("Inverted index trainsAt[" + r.from + "] has " + std::to_string(candidates.size()) + " train(s): only these are checked, not all " + std::to_string(all_.size()));
    std::string fk = stationKey(r.from), dk = stationKey(r.to);
    for (int no : candidates) {
        Train** p = byNo_.get(no);
        if (!p) continue;
        int si = (*p)->indexOf(fk), di = (*p)->indexOf(dk);
        if (si >= 0 && di > si) r.matches.push_back(makeMatch(*p, si, di));
    }
    note(std::to_string(r.matches.size()) + " train(s) stop at both stations in the right order (O(1) hash lookup per candidate)");

    auto less = [field, desc](const Match& a, const Match& b) {
        double x = numKey(a, field), y = numKey(b, field);
        if (field == SortField::Name) {
            int c = cmpNoCase(a.train->name, b.train->name);
            if (c != 0) return desc ? c > 0 : c < 0;
        } else if (x != y) {
            return desc ? x > y : x < y;
        }
        return a.train->trainNo < b.train->trainNo;   // fixed tie-break => every algorithm gives the same order
    };
    r.stats = ds::sortWith(algo, r.matches, less);
    note(std::string(ds::sortAlgoName(algo)) + "Sort on " + std::to_string(r.matches.size()) + " result(s): " + std::to_string(r.stats.comparisons) +
         " comparisons, " + std::to_string(r.stats.moves) + " moves");
    r.ok = true;
    r.code = 200;
    r.message = r.matches.empty() ? "No direct train between " + r.from + " and " + r.to : "OK";
    return r;
}

ds::DynamicArray<Train*> TrainManager::sortedTrains(SortField field, ds::SortAlgo algo, bool desc, ds::SortStats* stats) {
    ds::DynamicArray<Train*> arr(all_);
    auto less = [field, desc](Train* a, Train* b) {
        if (field == SortField::Name) {
            int c = cmpNoCase(a->name, b->name);
            if (c != 0) return desc ? c > 0 : c < 0;
        } else {
            double x = numKey(*a, field), y = numKey(*b, field);
            if (x != y) return desc ? x > y : x < y;
        }
        return a->trainNo < b->trainNo;
    };
    ds::SortStats st = ds::sortWith(algo, arr, less);
    if (stats) *stats = st;
    note(std::string(ds::sortAlgoName(algo)) + "Sort on " + std::to_string(arr.size()) + " train(s): " + std::to_string(st.comparisons) +
         " comparisons, " + std::to_string(st.moves) + " moves");
    return arr;
}

LookupReport TrainManager::lookupCompare(int trainNo) {
    LookupReport r;
    r.trainNo = trainNo;
    r.arraySize = all_.size();
    for (Train* t : all_) r.insertionOrder.push_back(t->trainNo);

    long li = ds::linearSearch(all_, [&](Train* t) { return t->trainNo == trainNo; }, &r.linearSteps);

    ds::DynamicArray<Train*> sorted(all_);
    ds::mergeSort(sorted, [](Train* a, Train* b) { return a->trainNo < b->trainNo; });
    for (Train* t : sorted) r.sortedOrder.push_back(t->trainNo);
    ds::binarySearch(sorted, trainNo, [](Train* t) { return t->trainNo; }, &r.binaryProbes);

    Train** p = byNo_.get(trainNo);
    r.hashBucket = byNo_.lastBucket();
    r.hashProbes = byNo_.lastProbes();
    r.hashBuckets = byNo_.bucketCount();
    r.found = (p != nullptr) && li >= 0;
    return r;
}

// ============================================================================
//  persistence
// ============================================================================
bool TrainManager::save(std::string* err) const {
    json::Writer w;
    w.beginObject();
    w.field("version", 1);
    w.key("stations").beginArray();
    for (size_t id = 0; id < registry_.idSlots(); ++id)
        if (registry_.alive(static_cast<int>(id))) w.value(registry_.name(static_cast<int>(id)));
    w.endArray();
    w.key("trains").beginArray();
    for (Train* t : all_) writeTrain(w, *t);
    w.endArray();
    w.endObject();

    std::error_code ec;
    std::filesystem::path p(storePath_);
    if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);
    std::string tmp = storePath_ + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) { if (err) *err = "cannot write " + tmp; return false; }
        out << w.str();
        if (!out) { if (err) *err = "write failed for " + tmp; return false; }
    }
    std::filesystem::rename(tmp, storePath_, ec);
    if (ec) { if (err) *err = ec.message(); return false; }
    return true;
}

bool TrainManager::load(std::string* err) {
    std::ifstream in(storePath_, std::ios::binary);
    if (!in) { if (err) *err = "no data file at " + storePath_; return false; }
    std::stringstream ss;
    ss << in.rdbuf();
    json::Value doc;
    std::string perr;
    if (!json::parse(ss.str(), doc, perr) || !doc.isObject()) { if (err) *err = "data file is not valid JSON: " + perr; return false; }

    bool prevAuto = autoSave_;
    autoSave_ = false;
    clearAll();
    if (const json::Value* st = doc.get("stations"))
        if (st->isArray())
            for (const json::Value& s : st->items)
                if (s.isString() && !cleanStationName(s.string).empty()) registry_.ensure(cleanStationName(s.string));
    std::string problems;
    if (const json::Value* tr = doc.get("trains")) {
        if (tr->isArray()) {
            for (const json::Value& tv : tr->items) {
                TrainInput ti;
                std::string e;
                if (!parseTrainInput(tv, ti, e)) { problems += e + "; "; continue; }
                Status s = addTrain(ti);
                if (!s.ok) problems += "train " + std::to_string(ti.trainNo) + ": " + s.message + "; ";
            }
        }
    }
    autoSave_ = prevAuto;
    clearTrace();
    if (err) *err = problems;
    return true;
}

// ============================================================================
TrainManager& trainManager() {
    static TrainManager instance;
    return instance;
}

}  // namespace railway
