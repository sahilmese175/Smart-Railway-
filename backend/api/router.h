// api/router.h
// -----------------------------------------------------------------------------
// A tiny HTTP-library-independent router. Every module (train, booking, seats,
// waiting list ...) registers its endpoints here, so handlers are plain C++
// functions   Response handler(const Request&)   that are easy to read and test.
// Only api/server.cpp knows about the real web server.
// -----------------------------------------------------------------------------
#pragma once
#include <functional>
#include <map>
#include <string>

#include "ds/dynamic_array.h"
#include "ds/linked_list.h"

namespace api {

struct Request {
    std::string method;                         // "GET", "POST", ...
    std::string path;                           // "/api/trains/12001"
    std::string body;
    std::map<std::string, std::string> query;   // ?a=1&b=2
    std::map<std::string, std::string> params;  // :name parts of the route pattern

    std::string q(const std::string& key, const std::string& def = "") const {
        auto it = query.find(key);
        return it == query.end() ? def : it->second;
    }
    std::string param(const std::string& key) const {
        auto it = params.find(key);
        return it == params.end() ? std::string() : it->second;
    }
};

struct Response {
    int status = 200;
    std::string body;
    std::string contentType = "application/json";

    static Response json(int status, const std::string& body) { Response r; r.status = status; r.body = body; return r; }
    static Response error(int status, const std::string& message);   // {"ok":false,"error":"..."}
};

using Handler = std::function<Response(const Request&)>;

struct ModuleInfo {
    std::string id;        // "train"
    std::string title;     // "Train & Route"
    std::string owner;     // "Member 1"
    std::string status;    // "live" | "planned"
    std::string summary;   // one line shown on the Team page
};

struct Route {
    std::string method;
    std::string pattern;   // "/api/trains/:no"
    std::string module;    // id of the module that owns it
    std::string doc;       // one-line description (shown on the Team page)
    Handler handler;
    ds::DynamicArray<std::string> segments;
};

class Router {
public:
    void add(const std::string& method, const std::string& pattern, Handler handler,
             const std::string& module = "", const std::string& doc = "");
    void addModule(const ModuleInfo& info) { modules_.push_back(info); }

    // Finds the matching route and runs it. Returns false when nothing matches.
    bool dispatch(Request& req, Response& res) const;

    const ds::LinkedList<Route>& routes() const { return routes_; }
    const ds::DynamicArray<ModuleInfo>& modules() const { return modules_; }

private:
    ds::LinkedList<Route> routes_;
    ds::DynamicArray<ModuleInfo> modules_;
};

ds::DynamicArray<std::string> splitPath(const std::string& path);

// ---- every module exposes one register function (declared here, defined in its routes file) ----
void registerTrainRoutes(Router& router);   // Member 1  (api/routes_train.cpp)
void registerMetaRoutes(Router& router);    // project info for the Team page (api/routes_meta.cpp)
void registerBookingRoutes(Router& router);  // Member 2  (api/routes_booking.cpp)
// void registerSeatRoutes(Router&);        // Member 3
// void registerWaitingRoutes(Router&);     // Member 4

}  // namespace api
