// api/routes_meta.cpp : project information for the "Team" page of the UI.
#include "api/router.h"
#include "common/json.h"

namespace api {

void registerMetaRoutes(Router& router) {
    // Modules that are not written yet are listed as "planned" so the Team page shows the whole picture.
    // When a teammate finishes a module, they change its status to "live" in their own register function.
    router.addModule({"seats", "Segment-wise Seats", "Member 3", "planned",
                      "Seat inventory per route segment. Segment tree with lazy propagation."});
    router.addModule({"waiting", "Waiting List & History", "Member 4", "planned",
                      "Queue, priority queue, circular queue and stack for waiting list, history and undo."});

    const Router* self = &router;
    router.add("GET", "/api/health", [](const Request&) {
        return Response::json(200, "{\"ok\":true}");
    }, "core", "Server is alive");

    router.add("GET", "/api/meta", [self](const Request&) {
        json::Writer w;
        w.beginObject().field("ok", true).field("project", "SmartRailway");
        w.key("modules").beginArray();
        for (const ModuleInfo& m : self->modules()) {
            size_t routes = 0;
            for (const Route& r : self->routes()) if (r.module == m.id) ++routes;
            w.beginObject().field("id", m.id).field("title", m.title).field("owner", m.owner)
             .field("status", m.status).field("summary", m.summary).field("routeCount", routes).endObject();
        }
        w.endArray();
        w.key("routes").beginArray();
        for (const Route& r : self->routes())
            w.beginObject().field("method", r.method).field("pattern", r.pattern).field("module", r.module).field("doc", r.doc).endObject();
        w.endArray().endObject();
        return Response::json(200, w.str());
    }, "core", "Modules and endpoints (used by the Team page)");
}

}  // namespace api
