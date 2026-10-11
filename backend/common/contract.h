// common/contract.h
// -----------------------------------------------------------------------------
// THE TEAM CONTRACT
//
// These plain structs and function signatures are what the four modules agree on
// BEFORE coding (see the project plan). Each member can change what is inside
// their module freely, but if you change something in THIS file, tell the team.
//
//   Member 1  Train & Route            -> implemented in backend/train/
//   Member 2  Passenger & Reservation  -> TODO  (backend/booking/)
//   Member 3  Segment-tree seats       -> TODO  (backend/segmenttree/)
//   Member 4  Waiting list & history   -> TODO  (backend/waiting/)
// -----------------------------------------------------------------------------
#pragma once

namespace railway {

// ---- shared data structures -----------------------------------------------------
struct Passenger {
    int passengerId;
    char name[50];
    int age;
};

enum BookingStatus { BOOKING_CONFIRMED = 1, BOOKING_WAITING = 2, BOOKING_CANCELLED = 3 };

struct Booking {
    int pnr;
    int trainNo;
    int passengerId;
    int sourceIndex;       // index of the boarding station in the train's route (0-based)
    int destinationIndex;  // index of the destination station in the route
    int seatNo;
    int status;            // BookingStatus
};

// The slim view of a train that other modules are allowed to depend on.
struct TrainInfo {
    int trainNo;
    char trainName[50];
    int totalSeats;
    int stationCount;      // segments = stationCount - 1  (segment i = stop i -> stop i+1)
};

// ---- Member 1 interface (Train & Route) -------------------------------------------
// All return false / -1 / 0 when the train or station does not exist.
bool searchTrain(int trainNo, TrainInfo& out);
int findStationIndex(int trainNo, const char* stationName);   // index in route, or -1
int findTrainsBetween(const char* source, const char* destination, int* outTrainNos, int maxOut);
double fareBetween(int trainNo, int sourceIndex, int destinationIndex);  // rupees, or -1
int segmentCount(int trainNo);                                  // stationCount - 1, or 0

// ---- Member 2 interface (Booking)  : declare here when ready ----------------------
// int  createBooking(...);   bool cancelBooking(int pnr);   bool searchPNR(int pnr, Booking& out);

// ---- Member 3 interface (Segment tree) : declare here when ready ------------------
// bool checkAvailability(int trainNo, int seatNo, int fromSeg, int toSeg);
// bool bookSegment(...);     bool cancelSegment(...);

// ---- Member 4 interface (Waiting list / history) : declare here when ready --------
// void addWaitingPassenger(...);  int getNextPassenger(...);  void showHistory();

}  // namespace railway
