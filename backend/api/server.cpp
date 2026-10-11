#include "api/server.h"

#include <exception>
#include <iostream>
#include <mutex>

#include "third_party/httplib.h"

namespace api {

bool runServer(const Router& router, const std::string& host, int port, const std::string& webDir) {
    httplib::Server svr;
    std::mutex gate;   // handlers run one at a time, so modules never need their own locks

    if (!webDir.empty() && !svr.set_mount_point("/", webDir))
        std::cerr << "Warning: could not serve web files from " << webDir << "\n";

    svr.set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type"},
    });
    svr.set_post_routing_handler([](const httplib::Request&, httplib::Response& res) {
        res.set_header("Cache-Control", "no-cache");   // always show the latest files while developing
    });

    auto handle = [&](const httplib::Request& req, httplib::Response& res) {
        Request r;
        r.method = req.method;
        r.path = req.path;
        r.body = req.body;
        for (const auto& p : req.params) r.query[p.first] = p.second;
        Response out;
        try {
            std::lock_guard<std::mutex> lock(gate);
            if (!router.dispatch(r, out))
                out = Response::error(404, "No such endpoint: " + req.method + " " + req.path);
        } catch (const std::exception& e) {
            out = Response::error(500, std::string("Server error: ") + e.what());
        }
        res.status = out.status;
        res.set_content(out.body, out.contentType);
    };

    svr.Get("/api/.*", handle);
    svr.Post("/api/.*", handle);
    svr.Put("/api/.*", handle);
    svr.Delete("/api/.*", handle);
    svr.Options("/api/.*", [](const httplib::Request&, httplib::Response& res) { res.status = 204; });

    return svr.listen(host, port);
}

}  // namespace api
