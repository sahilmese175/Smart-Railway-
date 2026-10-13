# REST API: Train & Route module (Member 1)

All endpoints return JSON. On failure: `{"ok": false, "error": "...", "trace": [...]}` with an HTTP status 400/404/409.
Successful responses carry `"ok": true`. Most also carry `"trace"`: the data structure steps the server took (shown in the UI console).

Time is `"HH:MM"` (24 hour). A stop that happens after midnight has `arrivalDay` / `departureDay` = 1 (or more).
Fares are whole rupees: `distance km x farePerKm`, rounded.

## Trains

| Method | Path | Notes |
|---|---|---|
| GET | `/api/trains?sort=&order=&algo=` | `sort`: `trainNo name fare departure arrival duration stops distance`. `order`: `asc desc`. `algo`: `merge quick insertion heap`. Response has `sortStats` (comparisons, moves, micros). |
| GET | `/api/trains/:no` | Hash table lookup. |
| POST | `/api/trains` | Body below. 201 on success. |
| PUT | `/api/trains/:no` | Same body; the train number cannot change. |
| DELETE | `/api/trains/:no` | |

Train body:

```json
{
  "trainNo": 12001, "name": "Deccan Express", "totalSeats": 120, "farePerKm": 1.4,
  "stops": [
    {"station": "Pune",     "departure": "07:15", "distanceKm": 0},
    {"station": "Lonavala", "arrival": "08:20", "departure": "08:22", "distanceKm": 64},
    {"station": "Mumbai",   "arrival": "10:30", "distanceKm": 192}
  ]
}
```

Rules checked by the server: unique train number, name up to 49 characters, at least 2 stops, no station twice on one route,
distances increasing, times moving forward (a time earlier than the previous one means the next day). Unknown station names are created automatically.

## Stations

| Method | Path | Notes |
|---|---|---|
| GET | `/api/stations` | Each station with the train numbers that stop there (inverted index) and hash table statistics. |
| POST | `/api/stations` | `{"name": "Nagpur"}` |
| DELETE | `/api/stations/:name` | Only if no train stops there. |
| GET | `/api/station-index?train=12001&station=Karjat` | Position in that train's route: `{"found": true, "index": 2, "stationCount": 5}` |

## Journeys

| Method | Path | Notes |
|---|---|---|
| GET | `/api/validate?train=12001&from=Pune&to=Karjat` | `{"valid": true, "journey": {...}}` or `{"valid": false, "message": "..."}` |
| GET | `/api/search?from=Pune&to=Mumbai&sort=duration&algo=merge&order=asc` | Trains that reach `to` after stopping at `from`. Each result has `srcIdx`, `dstIdx` (route positions), `fare`, `durationMin`, `stopsBetween`. Seat segments used: `srcIdx .. dstIdx-1`. |

## Passengers and reservations

The reservation module keeps independent demo train data (`12001`, Pune to Mumbai, 100 seats), so its bookings do not depend on the train-management module.

| Method | Path | Notes |
|---|---|---|
| GET | `/api/passengers?q=Rahul` | List passengers; optional case-insensitive name search. |
| POST | `/api/passengers` | Add passenger: `{"name":"Rahul","age":28}`. |
| PUT | `/api/passengers/:id` | Update passenger using the same body. |
| DELETE | `/api/passengers/:id` | Delete passenger; existing booking snapshots remain available. |
| GET | `/api/booking-trains` | List reservation-module demo trains and stops. |
| GET | `/api/bookings` | List bookings, including cancelled tickets. |
| POST | `/api/bookings` | Create booking: `{"passengerId":1,"trainNo":12001,"source":"Pune","destination":"Mumbai"}`. Assigns an available seat and returns PNR and fare. |
| GET | `/api/bookings/:pnr` | PNR enquiry. |
| DELETE | `/api/bookings/:pnr` | Cancel ticket and release its seat. |

The demo train fare is ₹2 per km (Pune–Mumbai is 150 km). Seat occupancy is tracked per route segment; cancelled bookings retain PNR history and release the seat.

## Dashboard and DSA lab

| Method | Path | Notes |
|---|---|---|
| GET | `/api/stats` | Counts, busiest station, and sizes of the internal structures. |
| GET | `/api/dsa/hash?table=trains` | Bucket contents. `table`: `trains`, `stations`, or `stops&train=12001`. |
| GET | `/api/dsa/lookup?train=12001` | Steps taken by linear search, binary search and the hash table. |
| GET | `/api/dsa/sort-benchmark?n=500` | The four sorts on `n` random records. |
| POST | `/api/admin/reset` | Restore the sample trains. |

## Project

| Method | Path | Notes |
|---|---|---|
| GET | `/api/health` | `{"ok": true}` |
| GET | `/api/meta` | Modules (owner, status) and all registered endpoints. Used by the Team page. |
