// HTTP layer for the Train & Route module. Add new routes below (or in new files) as the team grows.
#include "third_party/httplib.h"
#include "train/train.h"
#include <iostream>
#include <mutex>
#include <sstream>
using namespace std;

static TrainManager mgr;
static mutex mtx;
static string dataFile = "../data/trains.txt";

// ---------- tiny JSON helpers ----------
static string esc(const string& s) {
    string o;
    for (unsigned char c : s) {
        if (c == '"') o += "\\\""; else if (c == '\\') o += "\\\\"; else if (c >= 0x20) o += (char)c;
    }
    return o;
}
static string trainJson(const Train& t) {
    ostringstream o;
    o << "{\"trainNo\":" << t.trainNo << ",\"name\":\"" << esc(t.name) << "\",\"totalSeats\":" << t.totalSeats
      << ",\"totalFare\":" << t.totalFare() << ",\"stops\":[";
    for (auto* n = t.route.head(); n; n = n->next) {
        const Stop& s = n->val;
        o << "{\"station\":\"" << esc(s.station) << "\",\"arrival\":\"" << s.arrival << "\",\"departure\":\""
          << s.departure << "\",\"fare\":" << s.fare << "}" << (n->next ? "," : "");
    }
    o << "]}";
    return o.str();
}
static string matchJson(const Match& m) {
    ostringstream o;
    o << "{\"train\":" << trainJson(*m.train) << ",\"from\":" << m.from << ",\"to\":" << m.to << ",\"fare\":" << m.fare
      << ",\"duration\":" << m.duration << ",\"departure\":\"" << m.departure << "\",\"arrival\":\"" << m.arrival << "\"}";
    return o.str();
}
static void reply(httplib::Response& r, const string& body, int status = 200) { r.status = status; r.set_content(body, "application/json"); }
static void fail(httplib::Response& r, const string& msg, int status = 400) { reply(r, "{\"error\":\"" + esc(msg) + "\"}", status); }
static void persist() { if (!mgr.save(dataFile)) cerr << "Warning: could not write " << dataFile << "\n"; }

// "Pune~~06:00~0;Lonavala~07:08~07:10~80;..."  ->  DynArray<Stop>
static bool parseStops(const string& raw, DynArray<Stop>& out, string& err) {
    string cur; DynArray<string> rows;
    for (char c : raw) { if (c == ';') { rows.push(cur); cur.clear(); } else cur += c; }
    if (!cur.empty()) rows.push(cur);
    for (size_t i = 0; i < rows.size(); i++) {
        DynArray<string> f; string x;
        for (char c : rows[i]) { if (c == '~') { f.push(x); x.clear(); } else x += c; }
        f.push(x);
        if (f.size() != 4) { err = "Each stop must be station~arrival~departure~fare."; return false; }
        char* end = nullptr;
        long fare = strtol(f[3].c_str(), &end, 10);
        if (f[3].empty() || *end) { err = f[0] + ": fare must be a number."; return false; }
        out.push(Stop{f[0], f[1], f[2], (int)fare});
    }
    return true;
}
static bool parseInt(const string& s, int& v) {
    if (s.empty()) return false;
    char* end = nullptr; long x = strtol(s.c_str(), &end, 10);
    if (*end) return false;
    v = (int)x; return true;
}

// Shared by POST (create) and PUT (update)
static void saveTrain(const httplib::Request& req, httplib::Response& res, bool create, int fixedNo = 0) {
    int no = fixedNo, seats = 0; string err;
    if (create && !parseInt(req.get_param_value("trainNo"), no)) return fail(res, "Train number must be a number.");
    if (!parseInt(req.get_param_value("totalSeats"), seats)) return fail(res, "Total seats must be a number.");
    DynArray<Stop> stops;
    if (!parseStops(req.get_param_value("stops"), stops, err)) return fail(res, err);
    lock_guard<mutex> g(mtx);
    bool ok = create ? mgr.addTrain(no, req.get_param_value("name"), seats, stops, err)
                     : mgr.updateTrain(no, req.get_param_value("name"), seats, stops, err);
    if (!ok) return fail(res, err, err.find("not found") != string::npos ? 404 : 400);
    persist();
    reply(res, trainJson(*mgr.searchTrain(no)), create ? 201 : 200);
}

static string bucketJson(const DynArray<int>& b) {
    ostringstream o; o << "[";
    for (size_t i = 0; i < b.size(); i++) o << b[i] << (i + 1 < b.size() ? "," : "");
    o << "]"; return o.str();
}

int main(int argc, char** argv) {
    int port = argc > 1 ? atoi(argv[1]) : 8080;
    string webDir = argc > 2 ? argv[2] : "../frontend";
    if (argc > 3) dataFile = argv[3];
    mgr.load(dataFile);

    httplib::Server svr;
    svr.set_default_headers({{"Access-Control-Allow-Origin", "*"}, {"Access-Control-Allow-Methods", "GET,POST,PUT,DELETE,OPTIONS"},
                             {"Access-Control-Allow-Headers", "Content-Type"}});
    svr.Options(R"(.*)", [](const httplib::Request&, httplib::Response& r) { r.status = 204; });

    // GET /api/trains?sort=number|name|fare|departure|stops|seats&order=asc|desc
    svr.Get("/api/trains", [](const httplib::Request& req, httplib::Response& res) {
        lock_guard<mutex> g(mtx);
        string key = req.has_param("sort") ? req.get_param_value("sort") : "number";
        DynArray<Train*> a = mgr.sortedTrains(key, req.get_param_value("order") != "desc");
        ostringstream o; o << "[";
        for (size_t i = 0; i < a.size(); i++) o << trainJson(*a[i]) << (i + 1 < a.size() ? "," : "");
        o << "]"; reply(res, o.str());
    });
    // GET /api/trains/12001      (hash table lookup, O(1))
    svr.Get(R"(/api/trains/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        lock_guard<mutex> g(mtx);
        Train* t = mgr.searchTrain(atoi(req.matches[1].str().c_str()));
        t ? reply(res, trainJson(*t)) : fail(res, "Train not found.", 404);
    });
    svr.Post("/api/trains", [](const httplib::Request& req, httplib::Response& res) { saveTrain(req, res, true); });
    svr.Put(R"(/api/trains/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        saveTrain(req, res, false, atoi(req.matches[1].str().c_str()));
    });
    svr.Delete(R"(/api/trains/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        lock_guard<mutex> g(mtx);
        if (!mgr.deleteTrain(atoi(req.matches[1].str().c_str()))) return fail(res, "Train not found.", 404);
        persist(); reply(res, "{\"deleted\":true}");
    });

    // GET /api/stations
    svr.Get("/api/stations", [](const httplib::Request&, httplib::Response& res) {
        lock_guard<mutex> g(mtx);
        DynArray<StationEntry> a = mgr.allStations();
        ostringstream o; o << "[";
        for (size_t i = 0; i < a.size(); i++) {
            o << "{\"name\":\"" << esc(a[i].display) << "\",\"trains\":[";
            for (size_t k = 0; k < a[i].trains.size(); k++) o << a[i].trains[k] << (k + 1 < a[i].trains.size() ? "," : "");
            o << "]}" << (i + 1 < a.size() ? "," : "");
        }
        o << "]"; reply(res, o.str());
    });

    // GET /api/search?source=Pune&destination=Mumbai&sort=departure|fare|duration|number&order=asc|desc
    svr.Get("/api/search", [](const httplib::Request& req, httplib::Response& res) {
        lock_guard<mutex> g(mtx);
        string s = req.get_param_value("source"), d = req.get_param_value("destination");
        if (s.empty() || d.empty()) return fail(res, "Enter both a source and a destination.");
        if (toLower(s) == toLower(d)) return fail(res, "Source and destination must be different.");
        if (!mgr.stationTable().get(toLower(s))) return fail(res, "No train stops at '" + s + "'.", 404);
        if (!mgr.stationTable().get(toLower(d))) return fail(res, "No train stops at '" + d + "'.", 404);
        DynArray<Match> m; mgr.findTrainsBetween(s, d, m);
        TrainManager::sortMatches(m, req.has_param("sort") ? req.get_param_value("sort") : "departure", req.get_param_value("order") != "desc");
        ostringstream o; o << "[";
        for (size_t i = 0; i < m.size(); i++) o << matchJson(m[i]) << (i + 1 < m.size() ? "," : "");
        o << "]"; reply(res, o.str());
    });

    // GET /api/station-index?train=12001&station=Karjat
    svr.Get("/api/station-index", [](const httplib::Request& req, httplib::Response& res) {
        lock_guard<mutex> g(mtx);
        int no = 0;
        if (!parseInt(req.get_param_value("train"), no)) return fail(res, "Train must be a number.");
        if (!mgr.searchTrain(no)) return fail(res, "Train not found.", 404);
        int idx = mgr.findStationIndex(no, req.get_param_value("station"));
        ostringstream o; o << "{\"index\":" << idx << ",\"stops\":" << mgr.searchTrain(no)->route.size() << "}";
        reply(res, o.str());
    });

    // GET /api/inspect  (hash table internals for the DSA Inspector page)
    svr.Get("/api/inspect", [](const httplib::Request&, httplib::Response& res) {
        lock_guard<mutex> g(mtx);
        ostringstream o;
        o << "{\"trains\":{\"size\":" << mgr.trainTable().size() << ",\"buckets\":" << mgr.trainTable().buckets()
          << ",\"lengths\":" << bucketJson(mgr.trainTable().bucketLengths()) << "},"
          << "\"stations\":{\"size\":" << mgr.stationTable().size() << ",\"buckets\":" << mgr.stationTable().buckets()
          << ",\"lengths\":" << bucketJson(mgr.stationTable().bucketLengths()) << "}}";
        reply(res, o.str());
    });

    if (!svr.set_mount_point("/", webDir)) { cerr << "Cannot find frontend folder '" << webDir << "'. Run from the backend/ folder.\n"; return 1; }
    cout << "SmartRailway (Train & Route) running -> http://localhost:" << port << "  [" << mgr.count() << " trains loaded]\n";
    svr.listen("0.0.0.0", port);
}
