// api/server.h : the only place that talks to the real HTTP library (cpp-httplib).
#pragma once
#include <string>

#include "api/router.h"

namespace api {

// Serves  /api/*  through `router`  and everything else as static files from `webDir`.
// Blocks until the server stops. Returns false if the port could not be opened.
bool runServer(const Router& router, const std::string& host, int port, const std::string& webDir);

}  // namespace api
