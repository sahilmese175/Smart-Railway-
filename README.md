# SmartRailway: Train & Route Management (Member 1 module)

C++ backend built on hand-written data structures, plus a plain HTML/CSS/JS web UI (no build step, no npm).

## Run it
```bash
cd backend
make run            # builds and starts http://localhost:8080
```
Windows (MinGW): `make LIBS="-lws2_32 -lwsock32" run`. Any C++17 compiler works:
`g++ -std=c++17 -O2 main.cpp train/train.cpp -o railway -pthread` then `./railway [port] [frontendDir] [dataFile]`.
Run it from the `backend/` folder so the default paths (`../frontend`, `../data/trains.txt`) resolve.

## Layout (who edits what)
```
backend/
  ds/dynarray.h      Array (dynamic)           reusable by every member
  ds/linkedlist.h    Singly linked list        route stops
  ds/hashtable.h     Hash table (chaining)     add hashKey() overloads for new key types
  ds/sorting.h       Stable merge sort         mergeSort(array, comparator)
  train/train.h/.cpp Train & Route logic       <- Member 1 interface lives here
  main.cpp           HTTP routes + JSON        add new /api/... routes here
  third_party/httplib.h  (cpp-httplib, MIT, single header)
frontend/  index.html  style.css  app.js
data/trains.txt      persistent storage, plain text
```

## Interface for other members (`train/train.h`)
```cpp
Train* searchTrain(int no);
int    findStationIndex(int no, const std::string& station);   // -1 if absent
void   findTrainsBetween(src, dst, DynArray<Match>& out);
```
A train's segment `i` is stop `i` to stop `i+1`. A journey from stop `a` to stop `b` uses segments `a .. b-1`
(Member 3's Segment Tree can index on exactly this; the UI already shows it).

## REST API
| Method | Path | Notes |
|---|---|---|
| GET | `/api/trains?sort=&order=` | sort: number, name, fare, departure, stops, seats |
| GET | `/api/trains/:no` | hash lookup |
| POST | `/api/trains` | form fields: `trainNo, name, totalSeats, stops` |
| PUT | `/api/trains/:no` | same fields except `trainNo` |
| DELETE | `/api/trains/:no` | |
| GET | `/api/stations` | station -> trains stopping there |
| GET | `/api/search?source=&destination=&sort=&order=` | sort: departure, fare, duration, number |
| GET | `/api/station-index?train=&station=` | |
| GET | `/api/inspect` | hash table bucket stats |

`stops` format: `Station~arrival~departure~cumulativeFare;Station~...` (first stop has no arrival, last has no departure).

## Common changes
- **New train field** (e.g. train type): add it to `Train` in `train.h`, `save`/`load` in `train.cpp`, `trainJson` in `main.cpp`, and the form in `app.js`.
- **New sort key**: add a case in `sortedTrains` / `sortMatches`, then an `<option>` in `app.js`.
- **New module's API** (booking, waiting list): create `booking/booking.h/.cpp`, add it to `SRC` in the Makefile, register routes in `main.cpp`.
- **Restyle**: colours are CSS variables at the top of `style.css`.
- Station and train names cannot contain `| ; ~ " \` (they are the file/API separators).

Suggested branches: `member1-train`, `member2-reservation`, `member3-segment-tree`, `member4-waiting-list`.
