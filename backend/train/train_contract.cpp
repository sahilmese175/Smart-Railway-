// train/train_contract.cpp
// Implements the Member-1 functions declared in common/contract.h so that the other
// three modules can call them without knowing anything about how trains are stored.
#include <cstring>
#include <string>

#include "common/contract.h"
#include "train/train.h"

namespace railway {

bool searchTrain(int trainNo, TrainInfo& out) {
    Train* t = trainManager().searchTrain(trainNo);
    if (!t) return false;
    out.trainNo = t->trainNo;
    std::strncpy(out.trainName, t->name.c_str(), sizeof out.trainName - 1);
    out.trainName[sizeof out.trainName - 1] = '\0';
    out.totalSeats = t->totalSeats;
    out.stationCount = t->stationCount();
    return true;
}

int findStationIndex(int trainNo, const char* stationName) {
    if (!stationName) return -1;
    return trainManager().findStationIndex(trainNo, stationName);
}

int findTrainsBetween(const char* source, const char* destination, int* outTrainNos, int maxOut) {
    if (!source || !destination) return 0;
    SearchResult r = trainManager().findTrainsBetween(source, destination, SortField::Departure, ds::SortAlgo::Merge, false);
    if (!r.ok) return 0;
    int n = 0;
    for (const Match& m : r.matches) {
        if (outTrainNos && n < maxOut) outTrainNos[n] = m.train->trainNo;
        ++n;
    }
    return outTrainNos && n > maxOut ? maxOut : n;
}

double fareBetween(int trainNo, int sourceIndex, int destinationIndex) {
    Train* t = trainManager().searchTrain(trainNo);
    if (!t || sourceIndex < 0 || destinationIndex >= t->stationCount() || sourceIndex >= destinationIndex) return -1;
    return static_cast<double>(t->fareBetween(sourceIndex, destinationIndex));
}

int segmentCount(int trainNo) {
    Train* t = trainManager().searchTrain(trainNo);
    return t ? t->stationCount() - 1 : 0;
}

}  // namespace railway
