#include "api/router.h"

#include "common/json.h"

namespace api {

Response Response::error(int status, const std::string& message) {
    json::Writer w;
    w.beginObject().field("ok", false).field("error", message).endObject();
    return json(status, w.str());
}

ds::DynamicArray<std::string> splitPath(const std::string& path) {
    ds::DynamicArray<std::string> parts;
    std::string cur;
    for (char c : path) {
        if (c == '/') { if (!cur.empty()) { parts.push_back(cur); cur.clear(); } }
        else cur += c;
    }
    if (!cur.empty()) parts.push_back(cur);
    return parts;
}

void Router::add(const std::string& method, const std::string& pattern, Handler handler,
                 const std::string& module, const std::string& doc) {
    Route r;
    r.method = method;
    r.pattern = pattern;
    r.module = module;
    r.doc = doc;
    r.handler = std::move(handler);
    r.segments = splitPath(pattern);
    routes_.push_back(r);
}

bool Router::dispatch(Request& req, Response& res) const {
    ds::DynamicArray<std::string> parts = splitPath(req.path);
    for (const Route& r : routes_) {
        if (r.method != req.method || r.segments.size() != parts.size()) continue;
        std::map<std::string, std::string> params;
        bool match = true;
        for (size_t i = 0; i < parts.size() && match; ++i) {
            const std::string& seg = r.segments[i];
            if (!seg.empty() && seg[0] == ':') params[seg.substr(1)] = parts[i];
            else match = (seg == parts[i]);
        }
        if (!match) continue;
        req.params = params;
        res = r.handler(req);
        return true;
    }
    return false;
}

}  // namespace api
