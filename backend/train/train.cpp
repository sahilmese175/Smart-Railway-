#include "train.h"
#include "../ds/sorting.h"
#include <fstream>
#include <sstream>
#include <cctype>

std::string toLower(std::string s) { for (auto& c : s) c = (char)tolower((unsigned char)c); return s; }

int parseMinutes(const std::string& t) {
    if (t.size() != 5 || t[2] != ':' || !isdigit((unsigned char)t[0]) || !isdigit((unsigned char)t[1]) ||
        !isdigit((unsigned char)t[3]) || !isdigit((unsigned char)t[4])) return -1;
    int h = (t[0] - '0') * 10 + (t[1] - '0'), m = (t[3] - '0') * 10 + (t[4] - '0');
    return (h < 24 && m < 60) ? h * 60 + m : -1;
}

// ---------- Train ----------
int Train::stopIndex(const std::string& station) const {
    std::string key = toLower(station);
    int i = 0;
    for (auto* n = route.head(); n; n = n->next, i++) if (toLower(n->val.station) == key) return i;
    return -1;
}
int Train::totalFare() const { return route.last() ? route.last()->fare : 0; }
std::string Train::departureTime() const { return route.head() ? route.head()->val.departure : ""; }

// ---------- TrainManager ----------
TrainManager::~TrainManager() { for (size_t i = 0; i < order_.size(); i++) delete order_[i]; }

bool TrainManager::validate(int no, const std::string& name, int seats, const DynArray<Stop>& st, std::string& err) {
    auto bad = [](const std::string& s) { return s.find_first_of("|;~\"\\") != std::string::npos; };
    if (no <= 0) { err = "Train number must be a positive number."; return false; }
    if (name.empty() || bad(name)) { err = "Train name is required and cannot contain | ; ~ \" or \\."; return false; }
    if (seats <= 0) { err = "Total seats must be greater than 0."; return false; }
    if (st.size() < 2) { err = "A route needs at least 2 stations."; return false; }
    for (size_t i = 0; i < st.size(); i++) {
        const Stop& s = st[i];
        if (s.station.empty() || bad(s.station)) { err = "Stop " + std::to_string(i + 1) + ": station name is missing or has invalid characters."; return false; }
        if (i > 0 && parseMinutes(s.arrival) < 0) { err = s.station + ": arrival time must be HH:MM."; return false; }
        if (i + 1 < st.size() && parseMinutes(s.departure) < 0) { err = s.station + ": departure time must be HH:MM."; return false; }
        if (s.fare < 0 || (i > 0 && s.fare < st[i - 1].fare)) { err = s.station + ": fare cannot be lower than the previous station's fare."; return false; }
        for (size_t j = 0; j < i; j++)
            if (toLower(st[j].station) == toLower(s.station)) { err = "Station '" + s.station + "' appears twice on the route."; return false; }
    }
    return true;
}

void TrainManager::indexTrain(const Train* t) {
    for (auto* n = t->route.head(); n; n = n->next) {
        std::string key = toLower(n->val.station);
        if (StationEntry* e = stations_.get(key)) e->trains.push(t->trainNo);
        else { StationEntry e2; e2.display = n->val.station; e2.trains.push(t->trainNo); stations_.put(key, e2); }
    }
}
void TrainManager::unindexTrain(const Train* t) {
    for (auto* n = t->route.head(); n; n = n->next) {
        std::string key = toLower(n->val.station);
        StationEntry* e = stations_.get(key);
        if (!e) continue;
        for (size_t i = 0; i < e->trains.size(); i++) if (e->trains[i] == t->trainNo) { e->trains.removeAt(i); break; }
        if (e->trains.empty()) stations_.remove(key);
    }
}

bool TrainManager::addTrain(int no, const std::string& name, int seats, const DynArray<Stop>& stops, std::string& err) {
    if (!validate(no, name, seats, stops, err)) return false;
    if (trains_.get(no)) { err = "Train " + std::to_string(no) + " already exists."; return false; }
    Train* t = new Train;
    t->trainNo = no; t->name = name; t->totalSeats = seats;
    for (size_t i = 0; i < stops.size(); i++) t->route.push_back(stops[i]);
    trains_.put(no, t); order_.push(t); indexTrain(t);
    return true;
}

bool TrainManager::updateTrain(int no, const std::string& name, int seats, const DynArray<Stop>& stops, std::string& err) {
    Train* t = searchTrain(no);
    if (!t) { err = "Train " + std::to_string(no) + " not found."; return false; }
    if (!validate(no, name, seats, stops, err)) return false;
    unindexTrain(t);
    t->name = name; t->totalSeats = seats; t->route.clear();
    for (size_t i = 0; i < stops.size(); i++) t->route.push_back(stops[i]);
    indexTrain(t);
    return true;
}

bool TrainManager::deleteTrain(int no) {
    Train* t = searchTrain(no);
    if (!t) return false;
    unindexTrain(t);
    trains_.remove(no);
    for (size_t i = 0; i < order_.size(); i++) if (order_[i] == t) { order_.removeAt(i); break; }
    delete t;
    return true;
}

Train* TrainManager::searchTrain(int no) const { Train** p = trains_.get(no); return p ? *p : nullptr; }

int TrainManager::findStationIndex(int no, const std::string& station) const {
    const Train* t = searchTrain(no);
    return t ? t->stopIndex(station) : -1;
}

void TrainManager::findTrainsBetween(const std::string& src, const std::string& dst, DynArray<Match>& out) const {
    StationEntry* e = stations_.get(toLower(src));       // O(1): only look at trains that stop at src
    if (!e) return;
    for (size_t k = 0; k < e->trains.size(); k++) {
        const Train* t = searchTrain(e->trains[k]);
        int i = t->stopIndex(src), j = t->stopIndex(dst);
        if (i < 0 || j <= i) continue;                    // dst must come AFTER src
        const Stop* a = t->route.at(i); const Stop* b = t->route.at(j);
        int d = parseMinutes(b->arrival) - parseMinutes(a->departure);
        if (d < 0) d += 1440;                             // overnight
        out.push(Match{t, i, j, b->fare - a->fare, d, a->departure, b->arrival});
    }
}

DynArray<Train*> TrainManager::sortedTrains(const std::string& key, bool asc) const {
    DynArray<Train*> a = order_;
    auto val = [&](const Train* t) -> long {
        if (key == "fare") return t->totalFare();
        if (key == "departure") return parseMinutes(t->departureTime());
        if (key == "stops") return (long)t->route.size();
        if (key == "seats") return t->totalSeats;
        return t->trainNo;
    };
    if (key == "name") mergeSort(a, [&](Train* x, Train* y) { return asc ? toLower(x->name) < toLower(y->name) : toLower(x->name) > toLower(y->name); });
    else mergeSort(a, [&](Train* x, Train* y) { return asc ? val(x) < val(y) : val(x) > val(y); });
    return a;
}

void TrainManager::sortMatches(DynArray<Match>& m, const std::string& key, bool asc) {
    auto val = [&](const Match& x) -> long {
        if (key == "fare") return x.fare;
        if (key == "duration") return x.duration;
        if (key == "number") return x.train->trainNo;
        return parseMinutes(x.departure);
    };
    mergeSort(m, [&](const Match& x, const Match& y) { return asc ? val(x) < val(y) : val(x) > val(y); });
}

DynArray<StationEntry> TrainManager::allStations() const {
    DynArray<StationEntry> a;
    stations_.forEach([&](const std::string&, const StationEntry& e) { a.push(e); });
    mergeSort(a, [](const StationEntry& x, const StationEntry& y) { return toLower(x.display) < toLower(y.display); });
    return a;
}

bool TrainManager::save(const std::string& path) const {
    std::ofstream f(path);
    if (!f) return false;
    for (size_t i = 0; i < order_.size(); i++) {
        const Train* t = order_[i];
        f << t->trainNo << '|' << t->name << '|' << t->totalSeats << '|';
        for (auto* n = t->route.head(); n; n = n->next)
            f << n->val.station << '~' << n->val.arrival << '~' << n->val.departure << '~' << n->val.fare << (n->next ? ";" : "");
        f << '\n';
    }
    return true;
}

static void split(const std::string& s, char d, DynArray<std::string>& out) {
    std::string cur;
    for (char c : s) { if (c == d) { out.push(cur); cur.clear(); } else cur += c; }
    out.push(cur);
}

bool TrainManager::load(const std::string& path) {
    std::ifstream f(path);
    if (!f) return false;
    std::string line, err;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        DynArray<std::string> p; split(line, '|', p);
        if (p.size() != 4) continue;
        DynArray<std::string> rawStops; split(p[3], ';', rawStops);
        DynArray<Stop> stops;
        for (size_t i = 0; i < rawStops.size(); i++) {
            DynArray<std::string> q; split(rawStops[i], '~', q);
            if (q.size() != 4) continue;
            stops.push(Stop{q[0], q[1], q[2], atoi(q[3].c_str())});
        }
        addTrain(atoi(p[0].c_str()), p[1], atoi(p[2].c_str()), stops, err);
    }
    return true;
}
