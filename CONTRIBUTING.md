# Contributing to SmartRailway

Four members, four modules, one app. You mostly stay inside your own folders, so merge conflicts stay rare.

| Member | Module | Backend folder | Page (frontend/js/modules/) | Status |
|---|---|---|---|---|
| 1 | Train, Station & Route | `backend/train/` | overview, trains, search, stations, dsa-lab | done |
| 2 | Passenger & Reservation | `backend/booking/` | `booking.js` | done |
| 3 | Segment-wise seats (segment tree) | `backend/segmenttree/` | `seats.js` | to do |
| 4 | Waiting list & history | `backend/waiting/` | `waiting.js` | to do |

## Branches

```
main                      always builds, tests pass
 +- member1-train
 +- member2-reservation
 +- member3-segment-tree
 +- member4-waiting-list
```

1. `git checkout -b member2-reservation`
2. Commit small and often. Pull `main` before you open a pull request.
3. Open a pull request into `main`. CI runs `make` and `make test`; it must be green.
4. One teammate reviews, then squash-merge.

## Where things are

```
backend/
  ds/            hand-written data structures: DynamicArray, LinkedList, HashTable, sorting, searching
  common/        contract.h (shared structs, agreed functions) and json.h
  train/         Member 1 module  <- worked example
  api/           router.h/.cpp, one routes_<module>.cpp per module, server.cpp (HTTP, do not edit)
  main.cpp       starts everything; registers each module's routes
frontend/
  index.html, css/style.css       change colours and fonts in :root at the top of style.css
  js/core/                        api.js, ui.js, console.js, registry.js
  js/components/                  reusable pieces (train sheet, train form, network map)
  js/modules/                     one file per page; index.js lists them
tests/           test_ds.cpp, test_train.cpp. Add test_<module>.cpp for yours
docs/API.md      REST endpoints of the train module
```

## Adding your module

1. **Code.** Write your data structures and logic in `backend/<your folder>/`. Reuse `backend/ds/` or add your own header there (for example a `SegmentTree` in `backend/ds/segment_tree.h`). Use the structs in `backend/common/contract.h` for anything other modules need.
2. **Endpoints.** Copy the pattern in `backend/api/routes_train.cpp`:
   ```cpp
   void registerBookingRoutes(Router& r) {
       r.addModule({"booking", "Passenger & Reservation", "Member 2", "live", "Passengers, bookings, PNR."});
       r.add("GET", "/api/bookings/:pnr", getBooking, "booking", "Look up a booking by PNR");
   }
   ```
   Declare it in `backend/api/router.h` and call it in `backend/main.cpp` (the line is already there, commented out). Handlers run one at a time, so you do not need locks.
3. **Build.** Add your `.cpp` files to `Makefile` (and `CMakeLists.txt`).
4. **Page.** Create `frontend/js/modules/booking.js` with `registerPage({...})` and add its import in `frontend/js/modules/index.js`. `frontend/js/modules/team.js` shows a complete small page.
5. **Console messages.** If your handler fills `trace` (a list of strings) in its JSON, the "Data structure console" at the bottom of the UI prints it. `routes_train.cpp` shows how.
6. **Test.** Write `tests/test_<module>.cpp`, add it to the `test` target, and run `make test`.

## Connecting the modules (week 7)

Use the functions in `backend/common/contract.h`. Member 1 already provides `searchTrain`, `findStationIndex`, `findTrainsBetween`, `fareBetween` and `segmentCount`.

- `sourceIndex` / `destinationIndex` in a `Booking` are positions in the train's route (0 = first stop).
- Seat segment `i` is the hop from stop `i` to stop `i+1`, so a journey from position `a` to `b` uses segments `a .. b-1`. The Journey search page prints these numbers for every result.

## Style

- C++17, no external libraries (the web server is the single header `backend/third_party/httplib.h`).
- Do not use `std::vector`, `std::map` or `std::list` for the structure you are meant to demonstrate. Use your own, so you can explain it in the viva.
- Frontend: plain JavaScript modules, no build step. Edit a file and refresh the browser.

## Handy commands

```
make -j4        build        ->  build/railway_server
make run        build and start on http://127.0.0.1:8080
make test       run all unit tests
./build/railway_server --reset     restore the sample trains
./build/railway_server --port 9000
```
