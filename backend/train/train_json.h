// train/train_json.h : JSON <-> Train conversion, shared by the REST API and by file persistence.
#pragma once
#include <string>

#include "common/json.h"
#include "train/train.h"

namespace railway {

// Writes one train as a JSON object (including derived fields such as totalFare).
void writeTrain(json::Writer& w, const Train& t);

// Reads { trainNo, name, totalSeats, farePerKm, stops:[{station, arrival, departure, distanceKm}] }.
// Extra fields (the derived ones written by writeTrain) are ignored.
bool parseTrainInput(const json::Value& v, TrainInput& out, std::string& err);

}  // namespace railway
