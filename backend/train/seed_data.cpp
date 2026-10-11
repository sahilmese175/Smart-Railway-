// train/seed_data.cpp
// -----------------------------------------------------------------------------
// SAMPLE DATA ONLY. Train numbers, names, distances and timings below are made up
// for the demo (loosely inspired by the Pune-Mumbai corridor); they are not the
// real timetable. Edit freely, or reset from the UI ("Reset demo data").
// -----------------------------------------------------------------------------
#include <initializer_list>

#include "train/train.h"

namespace railway {

namespace {
struct SeedStop { const char* station; const char* arrival; const char* departure; int km; };

void seed(TrainManager& m, int no, const char* name, int seats, double farePerKm, std::initializer_list<SeedStop> stops) {
    TrainInput in;
    in.trainNo = no;
    in.name = name;
    in.totalSeats = seats;
    in.farePerKm = farePerKm;
    for (const SeedStop& s : stops) {
        StopInput si;
        si.station = s.station;
        si.distanceKm = s.km;
        if (s.arrival[0]) parseTime(s.arrival, si.arrival);
        if (s.departure[0]) parseTime(s.departure, si.departure);
        in.stops.push_back(si);
    }
    m.addTrain(in);
}
}  // namespace

void TrainManager::seedDemoData() {
    bool prev = autoSave_;
    autoSave_ = false;
    clearAll();

    seed(*this, 12001, "Deccan Express", 120, 1.4, {
        {"Pune", "", "07:15", 0}, {"Lonavala", "08:20", "08:22", 64}, {"Karjat", "08:55", "08:57", 92},
        {"Kalyan", "09:40", "09:42", 138}, {"Mumbai", "10:30", "", 192}});

    seed(*this, 12002, "Deccan Express (Return)", 120, 1.4, {
        {"Mumbai", "", "17:10", 0}, {"Kalyan", "17:55", "17:57", 54}, {"Karjat", "18:40", "18:42", 100},
        {"Lonavala", "19:15", "19:17", 128}, {"Pune", "20:30", "", 192}});

    seed(*this, 12010, "Pragati Express", 150, 1.6, {
        {"Pune", "", "07:50", 0}, {"Lonavala", "08:50", "08:52", 64}, {"Kalyan", "10:05", "10:07", 138},
        {"Mumbai", "10:55", "", 192}});

    seed(*this, 12123, "Intercity Superfast", 180, 1.8, {
        {"Pune", "", "06:00", 0}, {"Lonavala", "07:05", "07:07", 64}, {"Karjat", "07:35", "07:37", 92},
        {"Mumbai", "09:40", "", 192}});

    seed(*this, 11023, "Sahyadri Express", 200, 1.1, {
        {"Kolhapur", "", "20:30", 0}, {"Sangli", "21:25", "21:30", 48}, {"Satara", "23:20", "23:22", 150},
        {"Pune", "01:55", "02:05", 270}, {"Lonavala", "03:10", "03:12", 334}, {"Karjat", "03:50", "03:52", 362},
        {"Kalyan", "04:35", "04:37", 408}, {"Mumbai", "05:40", "", 462}});

    seed(*this, 12109, "Panchavati Express", 160, 1.2, {
        {"Mumbai", "", "06:10", 0}, {"Kalyan", "07:00", "07:02", 54}, {"Igatpuri", "08:20", "08:22", 136},
        {"Nashik Road", "09:10", "09:12", 188}, {"Manmad", "10:50", "", 258}});

    seed(*this, 11301, "Udyan Link", 220, 1.3, {
        {"Mumbai", "", "08:05", 0}, {"Pune", "11:35", "11:40", 192}, {"Solapur", "15:45", "15:50", 455},
        {"Kalaburagi", "17:30", "17:35", 590}, {"Raichur", "19:10", "19:12", 705}, {"Guntakal", "22:20", "22:25", 940},
        {"Bengaluru", "05:55", "", 1150}});

    seed(*this, 12951, "Capital Rajdhani Link", 400, 2.4, {
        {"Mumbai", "", "17:00", 0}, {"Surat", "20:15", "20:20", 263}, {"Vadodara", "22:05", "22:10", 392},
        {"Ratlam", "00:55", "01:00", 655}, {"Kota", "04:05", "04:10", 922}, {"Delhi", "08:30", "", 1384}});

    // stations that exist in the network but are not on any route yet
    registry_.ensure("Nagpur");

    autoSave_ = prev;
    clearTrace();
    persist();
    clearTrace();
}

}  // namespace railway
