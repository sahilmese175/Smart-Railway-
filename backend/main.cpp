// SmartRailway : backend entry point
//
//   ./railway_server                     serve on http://127.0.0.1:8080
//   ./railway_server --port 9000         other port
//   ./railway_server --host 0.0.0.0      let other computers on your network connect
//   ./railway_server --reset             throw away saved data and reload the sample trains
//   ./railway_server --web <dir>         folder with index.html (default: ./frontend or ../frontend)
//   ./railway_server --data <file>       JSON data file (default: backend/data/trains.json)
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "api/router.h"
#include "api/server.h"
#include "train/train.h"

namespace fs = std::filesystem;

static fs::path findWebDir(const char* argv0) {
    std::error_code ec;
    fs::path exeDir = fs::absolute(fs::path(argv0), ec).parent_path();
    fs::path candidates[] = {
        fs::current_path() / "frontend", fs::current_path() / ".." / "frontend",
        exeDir / "frontend", exeDir / ".." / "frontend", exeDir / ".." / ".." / "frontend",
    };
    for (const fs::path& c : candidates)
        if (fs::exists(c / "index.html", ec)) return fs::weakly_canonical(c, ec);
    return {};
}

int main(int argc, char** argv) {
    std::string host = "127.0.0.1", webArg, dataArg;
    int port = 8080;
    bool reset = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() { return (i + 1 < argc) ? std::string(argv[++i]) : std::string(); };
        if (a == "--port") port = std::atoi(next().c_str());
        else if (a == "--host") host = next();
        else if (a == "--web") webArg = next();
        else if (a == "--data") dataArg = next();
        else if (a == "--reset") reset = true;
        else if (a == "--help" || a == "-h") {
            std::cout << "Usage: railway_server [--port N] [--host H] [--web DIR] [--data FILE] [--reset]\n";
            return 0;
        } else { std::cerr << "Unknown option " << a << " (try --help)\n"; return 1; }
    }
    if (port < 1 || port > 65535) { std::cerr << "Port must be between 1 and 65535\n"; return 1; }

    fs::path web = webArg.empty() ? findWebDir(argv[0]) : fs::path(webArg);
    if (web.empty()) std::cerr << "Warning: frontend folder not found. Run from the project folder or pass --web <dir>.\n";
    fs::path dataFile = dataArg.empty()
        ? (web.empty() ? fs::path("backend/data/trains.json") : web.parent_path() / "backend" / "data" / "trains.json")
        : fs::path(dataArg);

    // ---- Member 1: Train & Route module -------------------------------------------------
    railway::TrainManager& trains = railway::trainManager();
    trains.setStorePath(dataFile.string());
    std::string loadMsg;
    if (reset || !trains.load(&loadMsg)) {
        if (!reset) std::cout << "No saved data yet (" << loadMsg << "): loading the sample trains.\n";
        trains.seedDemoData();
    } else if (!loadMsg.empty()) {
        std::cout << "Loaded data file, but skipped some entries: " << loadMsg << "\n";
    }

    // ---- register every module's endpoints here --------------------------------------------
    api::Router router;
    api::registerTrainRoutes(router);       // Member 1
    // api::registerBookingRoutes(router);  // Member 2  <- uncomment when the module exists
    // api::registerSeatRoutes(router);     // Member 3
    // api::registerWaitingRoutes(router);  // Member 4
    api::registerMetaRoutes(router);

    std::cout << "SmartRailway\n"
              << "  trains   : " << trains.trains().size() << ", stations: " << trains.stations().count() << "\n"
              << "  data     : " << dataFile.string() << "\n"
              << "  web      : " << (web.empty() ? "(none)" : web.string()) << "\n"
              << "  open     : http://" << (host == "0.0.0.0" ? "localhost" : host) << ":" << port << "\n"
              << "  stop with Ctrl+C\n";

    if (!api::runServer(router, host, port, web.string())) {
        std::cerr << "Could not listen on " << host << ":" << port << " (is another program using that port?)\n";
        return 1;
    }
    return 0;
}
